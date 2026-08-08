#include "WindowsAudioRuntime.h"

#include "RecoveredModRuntime.h"
#include "RecoveredAudioSpatial.h"
#include "RecoveredPcmWav.h"
#include "sound.h"

#include <windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <vector>

namespace rr2nw {
namespace {

constexpr unsigned int kBackendAbiVersion = 4u;
// The installed corpus contains 86 WAVs / about 38 MiB. Keep the process-wide
// cache bounded while allowing every retail Level transition to retain the
// union without turning late-campaign sounds into deterministic rejections.
constexpr std::size_t kMaximumCachedClips = 128u;
constexpr std::size_t kMaximumCachedSampleBytes = 64u * 1024u * 1024u;
constexpr std::size_t kMaximumVoices = 32u;
constexpr std::size_t kMaximumStreamClips = 32u;
constexpr std::size_t kMaximumActiveStreams = 8u;
constexpr std::size_t kStreamBufferCount = 4u;
constexpr std::size_t kStreamQueuedTarget = 3u;
constexpr std::size_t kStreamChunkBytes = 64u * 1024u;
constexpr char kSyntheticProbeKey[] = "__rr2nw_listening_probe__";

struct CachedClip {
  SRecoveredPcmWav wav;
};

struct StreamClip {
  std::string sourcePath;
  SRecoveredPcmWavStreamInfo wav;
};

struct SpatialSourceState {
  bool positionValid = false;
  SRecoveredAudioVector3 position;
  SRecoveredAudioEmitterModel model;
};

struct LoopRegistration {
  std::string clipKey;
  float intensity = 1.0f;
  float pitch = 1.0f;
  ESoundStateCategory category = SOUND_STATE_CATEGORY_EFFECTS;
  SoundStatePlaybackToken token = 0;
  SpatialSourceState spatial;
};

struct StreamRegistration {
  std::string clipKey;
  float intensity = 1.0f;
  int playCount = 1;
  ESoundStateCategory category = SOUND_STATE_CATEGORY_CINEMATIC;
  SoundStatePlaybackToken token = 0;
};

struct VoiceSlot;

class VoiceCallback final : public IXAudio2VoiceCallback {
 public:
  explicit VoiceCallback(VoiceSlot* slot) : slot_(slot) {}
  void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
  void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
  void STDMETHODCALLTYPE OnStreamEnd() override;
  void STDMETHODCALLTYPE OnBufferStart(void*) override {}
  void STDMETHODCALLTYPE OnBufferEnd(void*) override;
  void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
  void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override;

 private:
  VoiceSlot* slot_;
};

struct VoiceSlot {
  VoiceSlot() : callback(this) {}
  IXAudio2SourceVoice* voice = nullptr;
  SoundStatePlaybackToken token = 0;
  std::string clipKey;
  float intensity = 1.0f;
  float pitch = 1.0f;
  ESoundStateCategory category = SOUND_STATE_CATEGORY_EFFECTS;
  unsigned int sourceChannels = 0;
  SpatialSourceState spatial;
  bool looping = false;
  bool streaming = false;
  std::FILE* streamFile = nullptr;
  SRecoveredPcmWavStreamInfo streamInfo;
  std::size_t streamRemaining = 0;
  std::size_t streamNextBuffer = 0;
  bool streamFinalSubmitted = false;
  std::array<std::vector<std::uint8_t>, kStreamBufferCount> streamBuffers;
  std::atomic<bool> finished{false};
  std::atomic<bool> failed{false};
  VoiceCallback callback;
};

void VoiceCallback::OnStreamEnd() { slot_->finished.store(true); }
void VoiceCallback::OnBufferEnd(void*) {
  if (!slot_->streaming) slot_->finished.store(true);
}
void VoiceCallback::OnVoiceError(void*, HRESULT) {
  slot_->failed.store(true);
  slot_->finished.store(true);
}

class EngineCallback final : public IXAudio2EngineCallback {
 public:
  void STDMETHODCALLTYPE OnProcessingPassStart() override {}
  void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
  void STDMETHODCALLTYPE OnCriticalError(HRESULT error) override {
    error_.store(error);
    critical_.store(true);
  }
  bool TakeCritical(HRESULT* error) {
    if (!critical_.exchange(false)) return false;
    if (error != nullptr) *error = error_.load();
    return true;
  }

 private:
  std::atomic<bool> critical_{false};
  std::atomic<HRESULT> error_{S_OK};
};

struct AudioRuntimeState {
  SWindowsAudioRuntimeTelemetry telemetry;
  bool comOwned = false;
  IXAudio2* engine = nullptr;
  IXAudio2MasteringVoice* masteringVoice = nullptr;
  IXAudio2SubmixVoice* effectsVoice = nullptr;
  IXAudio2SubmixVoice* vehicleVoice = nullptr;
  IXAudio2SubmixVoice* cinematicVoice = nullptr;
  EngineCallback engineCallback;
  std::array<VoiceSlot, kMaximumVoices> voices;
  std::map<std::string, CachedClip> clips;
  std::map<std::string, StreamClip> streamClips;
  std::map<SoundStatePlaybackToken, LoopRegistration> loops;
  std::map<SoundStatePlaybackToken, StreamRegistration> streams;
  SoundStatePlaybackToken nextToken = 1;
  bool listenerValid = false;
  SRecoveredAudioListenerPose listener;
};

AudioRuntimeState g_audio;

void SetError(const char* message, HRESULT error = S_OK) {
  if (error == S_OK) {
    std::snprintf(g_audio.telemetry.lastError,
                  sizeof(g_audio.telemetry.lastError), "%s", message);
  } else {
    std::snprintf(g_audio.telemetry.lastError,
                  sizeof(g_audio.telemetry.lastError), "%s (0x%08lx)",
                  message, static_cast<unsigned long>(error));
  }
}

std::string FoldPath(const char* path) {
  std::string folded(path == nullptr ? "" : path);
  for (char& value : folded) {
    if (value == '/') value = '\\';
    if (value >= 'A' && value <= 'Z')
      value = static_cast<char>(value - 'A' + 'a');
  }
  return folded;
}

bool ValidAuthoredSoundPath(const char* path, std::string* folded) {
  if (path == nullptr || folded == nullptr) return false;
  *folded = FoldPath(path);
  const std::string prefix = "..\\sound\\";
  if (folded->compare(0, prefix.size(), prefix) != 0) return false;
  const std::string name = folded->substr(prefix.size());
  if (name.empty() || name.size() > 96u ||
      name.find_first_of("\\/:") != std::string::npos ||
      name.find("..") != std::string::npos || name.size() < 5u ||
      name.compare(name.size() - 4u, 4u, ".wav") != 0)
    return false;
  return true;
}

void UpdateActiveLoopTelemetry() {
  unsigned int active = 0;
  for (const VoiceSlot& slot : g_audio.voices)
    if (slot.voice != nullptr && slot.looping) ++active;
  g_audio.telemetry.activeLoopVoices = active;
  g_audio.telemetry.activeLoopRegistrations =
      static_cast<unsigned int>(g_audio.loops.size());
}

void UpdateActiveStreamTelemetry() {
  unsigned int active = 0;
  for (const VoiceSlot& slot : g_audio.voices)
    if (slot.voice != nullptr && slot.streaming) ++active;
  g_audio.telemetry.activeStreamVoices = active;
  g_audio.telemetry.activeStreamRegistrations =
      static_cast<unsigned int>(g_audio.streams.size());
}

IXAudio2SubmixVoice* CategoryVoice(ESoundStateCategory category) {
  if (category == SOUND_STATE_CATEGORY_VEHICLE)
    return g_audio.vehicleVoice;
  if (category == SOUND_STATE_CATEGORY_CINEMATIC)
    return g_audio.cinematicVoice;
  return g_audio.effectsVoice;
}

bool ApplyVoiceSpatial(VoiceSlot* slot) {
  if (slot == nullptr || slot->voice == nullptr) return false;
  float gain = slot->intensity;
  if (slot->category == SOUND_STATE_CATEGORY_VEHICLE ||
      slot->category == SOUND_STATE_CATEGORY_CINEMATIC)
    return SUCCEEDED(slot->voice->SetVolume(gain, XAUDIO2_COMMIT_NOW));
  if (!slot->spatial.positionValid || !g_audio.listenerValid) {
    return SUCCEEDED(slot->voice->SetVolume(gain, XAUDIO2_COMMIT_NOW));
  }
  SRecoveredAudioSpatialResult spatial;
  if (!RecoveredAudioSpatial_Evaluate(
          g_audio.listener, slot->spatial.position, slot->spatial.model,
          &spatial)) {
    return SUCCEEDED(slot->voice->SetVolume(gain, XAUDIO2_COMMIT_NOW));
  }
  gain *= spatial.attenuation;
  HRESULT result = slot->voice->SetVolume(gain, XAUDIO2_COMMIT_NOW);
  if (FAILED(result)) return false;
  if (slot->sourceChannels != 1u) return true;
  const float matrix[2] = {spatial.left, spatial.right};
  result = slot->voice->SetOutputMatrix(g_audio.effectsVoice, 1u, 2u, matrix,
                                        XAUDIO2_COMMIT_NOW);
  if (FAILED(result)) return false;
  ++g_audio.telemetry.spatialApplications;
  if (!spatial.audible) ++g_audio.telemetry.spatialSilentApplications;
  return true;
}

void DestroyVoice(VoiceSlot* slot, bool completed, bool countStop) {
  if (slot == nullptr || slot->voice == nullptr) return;
  slot->voice->Stop(0u, XAUDIO2_COMMIT_NOW);
  slot->voice->FlushSourceBuffers();
  slot->voice->DestroyVoice();
  slot->voice = nullptr;
  if (slot->streamFile != nullptr) {
    std::fclose(slot->streamFile);
    slot->streamFile = nullptr;
  }
  for (auto& buffer : slot->streamBuffers) buffer.clear();
  slot->token = 0;
  slot->clipKey.clear();
  slot->intensity = 1.0f;
  slot->pitch = 1.0f;
  slot->category = SOUND_STATE_CATEGORY_EFFECTS;
  slot->sourceChannels = 0;
  slot->spatial = {};
  slot->looping = false;
  slot->streaming = false;
  slot->streamInfo = {};
  slot->streamRemaining = 0;
  slot->streamNextBuffer = 0;
  slot->streamFinalSubmitted = false;
  slot->finished.store(false);
  slot->failed.store(false);
  if (completed)
    ++g_audio.telemetry.completedVoices;
  else if (countStop)
    ++g_audio.telemetry.stoppedVoices;
  UpdateActiveLoopTelemetry();
  UpdateActiveStreamTelemetry();
}

void DestroyAllVoices(bool countStops) {
  for (VoiceSlot& slot : g_audio.voices)
    DestroyVoice(&slot, false, countStops);
}

void DestroyDevice(bool countStops) {
  DestroyAllVoices(countStops);
  if (g_audio.cinematicVoice != nullptr) {
    g_audio.cinematicVoice->DestroyVoice();
    g_audio.cinematicVoice = nullptr;
  }
  if (g_audio.vehicleVoice != nullptr) {
    g_audio.vehicleVoice->DestroyVoice();
    g_audio.vehicleVoice = nullptr;
  }
  if (g_audio.effectsVoice != nullptr) {
    g_audio.effectsVoice->DestroyVoice();
    g_audio.effectsVoice = nullptr;
  }
  if (g_audio.masteringVoice != nullptr) {
    g_audio.masteringVoice->DestroyVoice();
    g_audio.masteringVoice = nullptr;
  }
  if (g_audio.engine != nullptr) {
    g_audio.engine->UnregisterForCallbacks(&g_audio.engineCallback);
    g_audio.engine->Release();
    g_audio.engine = nullptr;
  }
  g_audio.telemetry.deviceReady = false;
}

bool AcquireDeviceCom() {
  const HRESULT result =
      CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(result)) {
    // S_FALSE still increments the COM reference count. Retaining our own
    // reference keeps XAudio2 valid while Level teardown releases PIN's COM
    // ownership before this process-wide backend scope is destroyed.
    g_audio.comOwned = true;
    return true;
  }
  if (result == RPC_E_CHANGED_MODE) return true;
  ++g_audio.telemetry.deviceFailures;
  SetError("audio device COM boundary could not be initialized", result);
  return false;
}

bool InitializeDevice() {
  IXAudio2* engine = nullptr;
  HRESULT result = XAudio2Create(&engine, 0u, XAUDIO2_DEFAULT_PROCESSOR);
  if (FAILED(result) || engine == nullptr) {
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 2.9 device owner could not be created", result);
    return false;
  }
  engine->RegisterForCallbacks(&g_audio.engineCallback);
  IXAudio2MasteringVoice* mastering = nullptr;
  result = engine->CreateMasteringVoice(&mastering);
  if (FAILED(result) || mastering == nullptr) {
    engine->UnregisterForCallbacks(&g_audio.engineCallback);
    engine->Release();
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 mastering voice could not be created", result);
    return false;
  }
  IXAudio2SubmixVoice* effects = nullptr;
  result = engine->CreateSubmixVoice(&effects, 2u, 44100u);
  if (FAILED(result) || effects == nullptr) {
    mastering->DestroyVoice();
    engine->UnregisterForCallbacks(&g_audio.engineCallback);
    engine->Release();
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 effects category voice could not be created", result);
    return false;
  }
  result = effects->SetVolume(g_audio.telemetry.effectsVolume,
                              XAUDIO2_COMMIT_NOW);
  if (FAILED(result)) {
    effects->DestroyVoice();
    mastering->DestroyVoice();
    engine->UnregisterForCallbacks(&g_audio.engineCallback);
    engine->Release();
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 effects volume could not be applied", result);
    return false;
  }
  IXAudio2SubmixVoice* vehicle = nullptr;
  result = engine->CreateSubmixVoice(&vehicle, 2u, 44100u);
  if (FAILED(result) || vehicle == nullptr) {
    effects->DestroyVoice();
    mastering->DestroyVoice();
    engine->UnregisterForCallbacks(&g_audio.engineCallback);
    engine->Release();
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 vehicle category voice could not be created", result);
    return false;
  }
  result = vehicle->SetVolume(g_audio.telemetry.vehicleVolume,
                              XAUDIO2_COMMIT_NOW);
  if (FAILED(result)) {
    vehicle->DestroyVoice();
    effects->DestroyVoice();
    mastering->DestroyVoice();
    engine->UnregisterForCallbacks(&g_audio.engineCallback);
    engine->Release();
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 vehicle volume could not be applied", result);
    return false;
  }
  IXAudio2SubmixVoice* cinematic = nullptr;
  result = engine->CreateSubmixVoice(&cinematic, 2u, 44100u);
  if (FAILED(result) || cinematic == nullptr) {
    vehicle->DestroyVoice();
    effects->DestroyVoice();
    mastering->DestroyVoice();
    engine->UnregisterForCallbacks(&g_audio.engineCallback);
    engine->Release();
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 cinematic category voice could not be created", result);
    return false;
  }
  result = cinematic->SetVolume(g_audio.telemetry.cinematicVolume,
                                XAUDIO2_COMMIT_NOW);
  if (FAILED(result)) {
    cinematic->DestroyVoice();
    vehicle->DestroyVoice();
    effects->DestroyVoice();
    mastering->DestroyVoice();
    engine->UnregisterForCallbacks(&g_audio.engineCallback);
    engine->Release();
    ++g_audio.telemetry.deviceFailures;
    SetError("XAudio2 cinematic volume could not be applied", result);
    return false;
  }
  g_audio.engine = engine;
  g_audio.masteringVoice = mastering;
  g_audio.effectsVoice = effects;
  g_audio.vehicleVoice = vehicle;
  g_audio.cinematicVoice = cinematic;
  g_audio.telemetry.deviceReady = true;
  ++g_audio.telemetry.deviceInitializations;
  g_audio.telemetry.lastError[0] = 0;
  if (!g_audio.telemetry.applicationActive) engine->StopEngine();
  return true;
}

void ReapVoices() {
  for (VoiceSlot& slot : g_audio.voices) {
    if (slot.voice != nullptr && slot.finished.load()) {
      const bool failed = slot.failed.load();
      if (failed) ++g_audio.telemetry.playbackFailures;
      if (slot.streaming) {
        g_audio.streams.erase(slot.token);
        if (!failed) ++g_audio.telemetry.streamCompletions;
      }
      DestroyVoice(&slot, true, false);
    }
  }
  UpdateActiveStreamTelemetry();
}

bool AdmitClip(const char* fileName, int flags) {
  if (flags == 1) {
    ++g_audio.telemetry.deferredStreams;
    std::string key;
    if (!ValidAuthoredSoundPath(fileName, &key)) {
      ++g_audio.telemetry.rejectedStreams;
      SetError("authored stream path was rejected");
      return false;
    }
    if (g_audio.streamClips.find(key) != g_audio.streamClips.end()) {
      ++g_audio.telemetry.duplicateStreamAdmissions;
      return true;
    }
    if (g_audio.streamClips.size() >= kMaximumStreamClips) {
      ++g_audio.telemetry.rejectedStreams;
      SetError("authored stream entry limit was reached");
      return false;
    }
    long length = 0;
    std::FILE* file = RecoveredModRuntime_OpenRead(fileName, &length);
    if (file == nullptr || length < 12 ||
        static_cast<std::size_t>(length) >
            RecoveredPcmWav_MaximumSourceBytes()) {
      if (file != nullptr) std::fclose(file);
      ++g_audio.telemetry.rejectedStreams;
      SetError("authored stream WAV is unavailable or exceeds its limit");
      return false;
    }
    StreamClip clip;
    SRecoveredPcmWavResult inspected;
    const bool valid = RecoveredPcmWav_InspectStream(
        file, static_cast<std::size_t>(length), &clip.wav, &inspected);
    std::fclose(file);
    if (!valid) {
      ++g_audio.telemetry.rejectedStreams;
      SetError(inspected.error[0] == 0
                   ? "authored stream WAV inspection failed"
                   : inspected.error);
      return false;
    }
    clip.sourcePath = fileName;
    try {
      g_audio.streamClips.emplace(key, std::move(clip));
    } catch (...) {
      ++g_audio.telemetry.rejectedStreams;
      SetError("authored stream catalog allocation failed");
      return false;
    }
    ++g_audio.telemetry.admittedStreams;
    return true;
  }
  std::string key;
  if (flags != 0 || !ValidAuthoredSoundPath(fileName, &key)) {
    ++g_audio.telemetry.rejectedClips;
    SetError("authored effect path was rejected");
    return false;
  }
  if (g_audio.clips.find(key) != g_audio.clips.end()) {
    ++g_audio.telemetry.duplicateAdmissions;
    return true;
  }
  if (g_audio.clips.size() >= kMaximumCachedClips) {
    ++g_audio.telemetry.rejectedClips;
    SetError("effect cache entry limit was reached");
    return false;
  }
  long length = 0;
  FILE* file = RecoveredModRuntime_OpenRead(fileName, &length);
  if (file == nullptr || length < 12 ||
      static_cast<std::size_t>(length) >
          RecoveredPcmWav_MaximumSourceBytes()) {
    if (file != nullptr) std::fclose(file);
    ++g_audio.telemetry.rejectedClips;
    SetError("authored effect WAV is unavailable or exceeds its limit");
    return false;
  }
  std::vector<std::uint8_t> source;
  try {
    source.resize(static_cast<std::size_t>(length));
  } catch (...) {
    std::fclose(file);
    ++g_audio.telemetry.rejectedClips;
    SetError("effect WAV source allocation failed");
    return false;
  }
  const bool read = std::fread(source.data(), 1u, source.size(), file) ==
                    source.size();
  std::fclose(file);
  if (!read) {
    ++g_audio.telemetry.rejectedClips;
    SetError("effect WAV could not be read completely");
    return false;
  }
  CachedClip clip;
  SRecoveredPcmWavResult decoded;
  if (!RecoveredPcmWav_Decode(source.data(), source.size(), &clip.wav,
                              &decoded) ||
      clip.wav.samples.size() >
          kMaximumCachedSampleBytes -
              (std::min)(kMaximumCachedSampleBytes,
                         g_audio.telemetry.cachedSampleBytes)) {
    ++g_audio.telemetry.rejectedClips;
    SetError(decoded.error[0] == 0
                 ? "effect PCM cache byte limit was reached"
                 : decoded.error);
    return false;
  }
  const std::size_t sampleBytes = clip.wav.samples.size();
  try {
    g_audio.clips.emplace(key, std::move(clip));
  } catch (...) {
    ++g_audio.telemetry.rejectedClips;
    SetError("effect cache allocation failed");
    return false;
  }
  g_audio.telemetry.cachedSampleBytes += sampleBytes;
  ++g_audio.telemetry.admittedClips;
  return true;
}

bool SameStreamInfo(const SRecoveredPcmWavStreamInfo& left,
                    const SRecoveredPcmWavStreamInfo& right) {
  return left.channels == right.channels &&
         left.bitsPerSample == right.bitsPerSample &&
         left.sampleRate == right.sampleRate &&
         left.blockAlign == right.blockAlign &&
         left.averageBytesPerSecond == right.averageBytesPerSecond &&
         left.dataOffset == right.dataOffset &&
         left.dataBytes == right.dataBytes &&
         left.sourceBytes == right.sourceBytes;
}

bool PumpStreamBuffer(VoiceSlot* slot) {
  if (slot == nullptr || slot->voice == nullptr || !slot->streaming ||
      slot->streamFile == nullptr || slot->streamInfo.blockAlign == 0u ||
      slot->streamFinalSubmitted)
    return false;
  if (slot->streamRemaining == 0u) {
    if (!slot->looping) return false;
    if (slot->streamInfo.dataOffset >
            static_cast<std::size_t>((std::numeric_limits<long>::max)()) ||
        std::fseek(slot->streamFile,
                   static_cast<long>(slot->streamInfo.dataOffset),
                   SEEK_SET) != 0) {
      slot->failed.store(true);
      slot->finished.store(true);
      SetError("authored stream loop seek failed");
      return false;
    }
    slot->streamRemaining = slot->streamInfo.dataBytes;
  }
  std::size_t bytes = (std::min)(kStreamChunkBytes, slot->streamRemaining);
  bytes -= bytes % slot->streamInfo.blockAlign;
  if (bytes == 0u) {
    slot->failed.store(true);
    slot->finished.store(true);
    SetError("authored stream produced an unaligned terminal buffer");
    return false;
  }
  std::vector<std::uint8_t>& samples =
      slot->streamBuffers[slot->streamNextBuffer];
  if (samples.size() != kStreamChunkBytes) {
    try {
      samples.resize(kStreamChunkBytes);
    } catch (...) {
      slot->failed.store(true);
      slot->finished.store(true);
      SetError("authored stream buffer allocation failed");
      return false;
    }
  }
  if (std::fread(samples.data(), 1u, bytes, slot->streamFile) != bytes) {
    slot->failed.store(true);
    slot->finished.store(true);
    SetError("authored stream buffer could not be read completely");
    return false;
  }
  slot->streamRemaining -= bytes;
  XAUDIO2_BUFFER buffer = {};
  buffer.AudioBytes = static_cast<UINT32>(bytes);
  buffer.pAudioData = samples.data();
  buffer.pContext = slot;
  if (!slot->looping && slot->streamRemaining == 0u) {
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    slot->streamFinalSubmitted = true;
  }
  const HRESULT result = slot->voice->SubmitSourceBuffer(&buffer);
  if (FAILED(result)) {
    slot->failed.store(true);
    slot->finished.store(true);
    SetError("XAudio2 stream buffer submission failed", result);
    return false;
  }
  slot->streamNextBuffer =
      (slot->streamNextBuffer + 1u) % kStreamBufferCount;
  ++g_audio.telemetry.streamBufferSubmissions;
  g_audio.telemetry.streamedSampleBytes += bytes;
  return true;
}

bool PumpStreamVoice(VoiceSlot* slot) {
  if (slot == nullptr || slot->voice == nullptr || !slot->streaming)
    return false;
  XAUDIO2_VOICE_STATE state = {};
  slot->voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
  if (state.BuffersQueued == 0u && !slot->streamFinalSubmitted &&
      slot->streamRemaining != slot->streamInfo.dataBytes) {
    ++g_audio.telemetry.streamUnderruns;
  }
  while (state.BuffersQueued < kStreamQueuedTarget &&
         !slot->streamFinalSubmitted) {
    if (!PumpStreamBuffer(slot)) return slot->streamFinalSubmitted;
    ++state.BuffersQueued;
  }
  return true;
}

bool StartStreamClip(const std::string& key, float intensity, int playCount,
                     SoundStatePlaybackToken* token,
                     SoundStatePlaybackToken recoveredToken = 0,
                     bool deviceRecovery = false) {
  if (token == nullptr) return false;
  *token = 0;
  const bool newRequest = recoveredToken == 0;
  if (newRequest) {
    ++g_audio.telemetry.playbackRequests;
    ++g_audio.telemetry.streamRequests;
  }
  ReapVoices();
  const auto found = g_audio.streamClips.find(key);
  if (found == g_audio.streamClips.end() ||
      (playCount != 0 && playCount != 1)) {
    ++g_audio.telemetry.playbackFailures;
    SetError("stream playback requested before admission or with invalid cycle");
    return false;
  }
  const float boundedIntensity = (std::max)(0.0f, (std::min)(1.0f, intensity));
  SoundStatePlaybackToken assignedToken = recoveredToken;
  bool registeredHere = false;
  if (newRequest) {
    if (g_audio.streams.size() >= kMaximumActiveStreams) {
      ++g_audio.telemetry.voiceStealsPrevented;
      ++g_audio.telemetry.playbackFailures;
      SetError("stream registration limit reached; an active stream was not stolen");
      return false;
    }
    assignedToken = g_audio.nextToken++;
    if (assignedToken == 0) assignedToken = g_audio.nextToken++;
    StreamRegistration registration;
    registration.clipKey = key;
    registration.intensity = boundedIntensity;
    registration.playCount = playCount;
    registration.token = assignedToken;
    try {
      g_audio.streams.emplace(assignedToken, std::move(registration));
    } catch (...) {
      ++g_audio.telemetry.playbackFailures;
      SetError("stream registration allocation failed");
      return false;
    }
    registeredHere = true;
    ++g_audio.telemetry.streamRegistrations;
    *token = assignedToken;
    UpdateActiveStreamTelemetry();
    if (!g_audio.telemetry.deviceReady) {
      ++g_audio.telemetry.deferredStreamRegistrations;
      return true;
    }
  } else {
    const auto registration = g_audio.streams.find(recoveredToken);
    if (registration == g_audio.streams.end() ||
        registration->second.clipKey != key ||
        registration->second.playCount != playCount) {
      ++g_audio.telemetry.playbackFailures;
      SetError("stream recovery token was not registered");
      return false;
    }
  }

  const auto rollbackRegistration = [&]() {
    if (registeredHere) {
      g_audio.streams.erase(assignedToken);
      *token = 0;
      UpdateActiveStreamTelemetry();
    }
  };
  VoiceSlot* slot = nullptr;
  for (VoiceSlot& candidate : g_audio.voices) {
    if (candidate.voice == nullptr) {
      slot = &candidate;
      break;
    }
  }
  if (slot == nullptr) {
    ++g_audio.telemetry.voiceStealsPrevented;
    ++g_audio.telemetry.playbackFailures;
    SetError("stream voice limit reached; an active voice was not stolen");
    rollbackRegistration();
    return false;
  }
  const StreamClip& clip = found->second;
  long length = 0;
  slot->streamFile =
      RecoveredModRuntime_OpenRead(clip.sourcePath.c_str(), &length);
  SRecoveredPcmWavResult inspected;
  if (slot->streamFile == nullptr || length < 12 ||
      !RecoveredPcmWav_InspectStream(slot->streamFile,
                                    static_cast<std::size_t>(length),
                                    &slot->streamInfo, &inspected) ||
      !SameStreamInfo(slot->streamInfo, clip.wav)) {
    if (slot->streamFile != nullptr) {
      std::fclose(slot->streamFile);
      slot->streamFile = nullptr;
    }
    ++g_audio.telemetry.playbackFailures;
    SetError(inspected.error[0] == 0
                 ? "authored stream changed after admission"
                 : inspected.error);
    rollbackRegistration();
    return false;
  }
  WAVEFORMATEX format = {};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = slot->streamInfo.channels;
  format.nSamplesPerSec = slot->streamInfo.sampleRate;
  format.nAvgBytesPerSec = slot->streamInfo.averageBytesPerSecond;
  format.nBlockAlign = slot->streamInfo.blockAlign;
  format.wBitsPerSample = slot->streamInfo.bitsPerSample;
  XAUDIO2_SEND_DESCRIPTOR send = {0u, g_audio.cinematicVoice};
  XAUDIO2_VOICE_SENDS sends = {1u, &send};
  HRESULT result = g_audio.engine->CreateSourceVoice(
      &slot->voice, &format, 0u, 1.0f, &slot->callback, &sends, nullptr);
  if (FAILED(result) || slot->voice == nullptr) {
    slot->voice = nullptr;
    std::fclose(slot->streamFile);
    slot->streamFile = nullptr;
    ++g_audio.telemetry.playbackFailures;
    SetError("XAudio2 stream voice could not be created", result);
    rollbackRegistration();
    return false;
  }
  slot->finished.store(false);
  slot->failed.store(false);
  slot->token = assignedToken;
  slot->clipKey = key;
  slot->intensity = boundedIntensity;
  slot->category = SOUND_STATE_CATEGORY_CINEMATIC;
  slot->sourceChannels = slot->streamInfo.channels;
  slot->looping = playCount == 0;
  slot->streaming = true;
  slot->streamRemaining = slot->streamInfo.dataBytes;
  slot->streamNextBuffer = 0u;
  slot->streamFinalSubmitted = false;
  if (!ApplyVoiceSpatial(slot) || !PumpStreamVoice(slot)) {
    ++g_audio.telemetry.playbackFailures;
    if (g_audio.telemetry.lastError[0] == 0)
      SetError("XAudio2 stream voice could not be initialized");
    DestroyVoice(slot, false, false);
    rollbackRegistration();
    return false;
  }
  result = slot->voice->Start();
  if (FAILED(result)) {
    ++g_audio.telemetry.playbackFailures;
    SetError("XAudio2 stream voice could not be started", result);
    DestroyVoice(slot, false, false);
    rollbackRegistration();
    return false;
  }
  *token = assignedToken;
  ++g_audio.telemetry.playbackStarts;
  if (deviceRecovery)
    ++g_audio.telemetry.streamRestarts;
  else
    ++g_audio.telemetry.streamStarts;
  UpdateActiveStreamTelemetry();
  return true;
}

bool StartCachedClip(const std::string& key, float intensity, bool looping,
                     SoundStatePlaybackToken* token,
                     SoundStatePlaybackToken recoveredToken = 0,
                     bool deviceRecovery = false,
                     const SpatialSourceState& spatial = {},
                     ESoundStateCategory category = SOUND_STATE_CATEGORY_EFFECTS,
                     float pitch = 1.0f) {
  if (token == nullptr) return false;
  *token = 0;
  const bool newRequest = recoveredToken == 0;
  if (recoveredToken == 0) {
    ++g_audio.telemetry.playbackRequests;
    if (looping) ++g_audio.telemetry.loopRequests;
  }
  ReapVoices();
  const auto found = g_audio.clips.find(key);
  if (found == g_audio.clips.end()) {
    ++g_audio.telemetry.playbackFailures;
    SetError("effect playback requested before WAV admission");
    return false;
  }
  const float boundedIntensity = (std::max)(0.0f, (std::min)(1.0f, intensity));
  if (category < SOUND_STATE_CATEGORY_EFFECTS ||
      category >= SOUND_STATE_CATEGORY_COUNT || !std::isfinite(pitch) ||
      pitch < 0.15f || pitch > 4.0f) {
    ++g_audio.telemetry.playbackFailures;
    SetError("effect playback category or pitch was invalid");
    return false;
  }
  SoundStatePlaybackToken assignedToken = recoveredToken;
  bool registeredHere = false;
  if (looping && newRequest) {
    if (g_audio.loops.size() >= kMaximumVoices) {
      ++g_audio.telemetry.voiceStealsPrevented;
      ++g_audio.telemetry.playbackFailures;
      SetError("effect loop registration limit reached; an active loop was not stolen");
      return false;
    }
    assignedToken = g_audio.nextToken++;
    if (assignedToken == 0) assignedToken = g_audio.nextToken++;
    LoopRegistration registration;
    registration.clipKey = key;
    registration.intensity = boundedIntensity;
    registration.pitch = pitch;
    registration.category = category;
    registration.token = assignedToken;
    registration.spatial = spatial;
    try {
      g_audio.loops.emplace(assignedToken, std::move(registration));
    } catch (...) {
      ++g_audio.telemetry.playbackFailures;
      SetError("effect loop registration allocation failed");
      return false;
    }
    registeredHere = true;
    ++g_audio.telemetry.loopRegistrations;
    if (category == SOUND_STATE_CATEGORY_VEHICLE)
      ++g_audio.telemetry.vehicleLoopRegistrations;
    *token = assignedToken;
    UpdateActiveLoopTelemetry();
    if (!g_audio.telemetry.deviceReady) {
      ++g_audio.telemetry.deferredLoopRegistrations;
      return true;
    }
  } else if (looping) {
    const auto registration = g_audio.loops.find(recoveredToken);
    if (registration == g_audio.loops.end() ||
        registration->second.clipKey != key ||
        registration->second.category != category) {
      ++g_audio.telemetry.playbackFailures;
      SetError("effect loop recovery token was not registered");
      return false;
    }
  } else if (!g_audio.telemetry.deviceReady) {
    ++g_audio.telemetry.playbackFailures;
    SetError("one-shot playback requested without an audio device");
    return false;
  } else {
    assignedToken = g_audio.nextToken++;
    if (assignedToken == 0) assignedToken = g_audio.nextToken++;
  }

  const auto rollbackRegistration = [&]() {
    if (registeredHere) {
      g_audio.loops.erase(assignedToken);
      *token = 0;
      UpdateActiveLoopTelemetry();
    }
  };
  VoiceSlot* slot = nullptr;
  for (VoiceSlot& candidate : g_audio.voices) {
    if (candidate.voice == nullptr) {
      slot = &candidate;
      break;
    }
  }
  if (slot == nullptr) {
    ++g_audio.telemetry.voiceStealsPrevented;
    ++g_audio.telemetry.playbackFailures;
    SetError("effect voice limit reached; an active voice was not stolen");
    rollbackRegistration();
    return false;
  }

  const CachedClip& clip = found->second;
  WAVEFORMATEX format = {};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = clip.wav.channels;
  format.nSamplesPerSec = clip.wav.sampleRate;
  format.nAvgBytesPerSec = clip.wav.averageBytesPerSecond;
  format.nBlockAlign = clip.wav.blockAlign;
  format.wBitsPerSample = clip.wav.bitsPerSample;
  IXAudio2SubmixVoice* categoryVoice = CategoryVoice(category);
  XAUDIO2_SEND_DESCRIPTOR send = {0u, categoryVoice};
  XAUDIO2_VOICE_SENDS sends = {1u, &send};
  HRESULT result = g_audio.engine->CreateSourceVoice(
      &slot->voice, &format, 0u, 4.0f,
      &slot->callback, &sends, nullptr);
  if (FAILED(result) || slot->voice == nullptr) {
    slot->voice = nullptr;
    ++g_audio.telemetry.playbackFailures;
    SetError("XAudio2 source voice could not be created", result);
    rollbackRegistration();
    return false;
  }
  XAUDIO2_BUFFER buffer = {};
  buffer.Flags = XAUDIO2_END_OF_STREAM;
  buffer.AudioBytes = static_cast<UINT32>(clip.wav.samples.size());
  buffer.pAudioData = clip.wav.samples.data();
  buffer.pContext = slot;
  if (looping) buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
  slot->finished.store(false);
  slot->failed.store(false);
  slot->clipKey = key;
  slot->intensity = boundedIntensity;
  slot->pitch = pitch;
  slot->category = category;
  slot->sourceChannels = clip.wav.channels;
  slot->spatial = spatial;
  slot->looping = looping;
  slot->token = assignedToken;
  if (spatial.positionValid) {
    if (RecoveredAudioSpatial_ValidateSymmetricModel(spatial.model)) {
      if (clip.wav.channels != 1u)
        ++g_audio.telemetry.nonMonoSpatialFallbacks;
    } else {
      ++g_audio.telemetry.asymmetricModelFallbacks;
    }
  }
  if (!ApplyVoiceSpatial(slot)) {
    SetError("XAudio2 source spatial state could not be applied");
    ++g_audio.telemetry.playbackFailures;
    DestroyVoice(slot, false, false);
    rollbackRegistration();
    return false;
  }
  result = slot->voice->SetFrequencyRatio(pitch, XAUDIO2_COMMIT_NOW);
  if (FAILED(result)) {
    SetError("XAudio2 source pitch could not be applied", result);
    ++g_audio.telemetry.playbackFailures;
    DestroyVoice(slot, false, false);
    rollbackRegistration();
    return false;
  }
  result = slot->voice->SubmitSourceBuffer(&buffer);
  if (SUCCEEDED(result)) result = slot->voice->Start();
  if (FAILED(result)) {
    SetError("XAudio2 effect buffer could not be started", result);
    ++g_audio.telemetry.playbackFailures;
    DestroyVoice(slot, false, false);
    rollbackRegistration();
    return false;
  }
  *token = slot->token;
  ++g_audio.telemetry.playbackStarts;
  if (looping) {
    if (deviceRecovery)
      ++g_audio.telemetry.loopRestarts;
    else
      ++g_audio.telemetry.loopStarts;
  }
  UpdateActiveLoopTelemetry();
  return true;
}

bool BackendAdmit(void*, const char* fileName, int flags) {
  return AdmitClip(fileName, flags);
}

bool BackendStart(void*, const SSoundStatePlaybackRequest* request,
                  SoundStatePlaybackToken* token) {
  std::string key;
  if (request == nullptr ||
      !ValidAuthoredSoundPath(request->fileName, &key))
    return false;
  if (request->flags == 1) {
    return StartStreamClip(key, request->intensity, request->playCount,
                           token);
  }
  SpatialSourceState spatial;
  spatial.positionValid = request->positionValid != 0;
  spatial.position = {request->positionX, request->positionY,
                      request->positionZ};
  spatial.model = {request->minFront, request->minBack,
                   request->maxFront, request->maxBack,
                   request->intensity};
  const bool started = StartCachedClip(key, request->intensity,
                                       request->playCount == 0, token,
                                       0, false, spatial,
                                       request->category, 1.0f);
  if (started && spatial.positionValid && request->playCount == 0)
    ++g_audio.telemetry.positionedRegistrations;
  return started;
}

void BackendStop(void*, SoundStatePlaybackToken token) {
  if (token == 0) return;
  const auto loop = g_audio.loops.find(token);
  const bool registeredLoop = loop != g_audio.loops.end();
  const bool vehicleLoop = registeredLoop &&
      loop->second.category == SOUND_STATE_CATEGORY_VEHICLE;
  const bool registeredStream = g_audio.streams.find(token) !=
                                g_audio.streams.end();
  for (VoiceSlot& slot : g_audio.voices) {
    if (slot.voice != nullptr && slot.token == token) {
      DestroyVoice(&slot, false, true);
      break;
    }
  }
  if (registeredLoop) {
    g_audio.loops.erase(token);
    ++g_audio.telemetry.loopStops;
    if (vehicleLoop) ++g_audio.telemetry.vehicleLoopStops;
    UpdateActiveLoopTelemetry();
  }
  if (registeredStream) {
    g_audio.streams.erase(token);
    ++g_audio.telemetry.streamStops;
    UpdateActiveStreamTelemetry();
  }
}

bool BackendSetPitch(void*, SoundStatePlaybackToken token, float ratio) {
  if (token == 0 || !std::isfinite(ratio) || ratio < 0.15f || ratio > 4.0f) {
    ++g_audio.telemetry.vehiclePitchFailures;
    return false;
  }
  auto loop = g_audio.loops.find(token);
  if (loop == g_audio.loops.end()) {
    ++g_audio.telemetry.vehiclePitchFailures;
    return false;
  }
  loop->second.pitch = ratio;
  bool applied = true;
  for (VoiceSlot& slot : g_audio.voices) {
    if (slot.voice == nullptr || slot.token != token) continue;
    slot.pitch = ratio;
    applied = SUCCEEDED(
        slot.voice->SetFrequencyRatio(ratio, XAUDIO2_COMMIT_NOW));
    break;
  }
  if (!applied) {
    ++g_audio.telemetry.vehiclePitchFailures;
    return false;
  }
  if (loop->second.category == SOUND_STATE_CATEGORY_VEHICLE)
    ++g_audio.telemetry.vehiclePitchUpdates;
  return true;
}

bool BackendMove(void*, SoundStatePlaybackToken token,
                 float x, float y, float z) {
  if (token == 0 || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(z)) {
    ++g_audio.telemetry.emitterMoveFailures;
    return false;
  }
  const SRecoveredAudioVector3 position = {x, y, z};
  bool found = false;
  const auto loop = g_audio.loops.find(token);
  if (loop != g_audio.loops.end()) {
    loop->second.spatial.position = position;
    loop->second.spatial.positionValid = true;
    found = true;
  }
  for (VoiceSlot& slot : g_audio.voices) {
    if (slot.voice == nullptr || slot.token != token) continue;
    slot.spatial.position = position;
    slot.spatial.positionValid = true;
    found = ApplyVoiceSpatial(&slot);
    break;
  }
  if (!found) {
    ++g_audio.telemetry.emitterMoveFailures;
    return false;
  }
  ++g_audio.telemetry.emitterMoveUpdates;
  return true;
}

bool BackendSetListener(void*, const SSoundStateListenerPose* listener) {
  if (listener == nullptr) {
    ++g_audio.telemetry.listenerFailures;
    return false;
  }
  const SRecoveredAudioListenerPose pose = {
      {listener->positionX, listener->positionY, listener->positionZ},
      {listener->frontX, listener->frontY, listener->frontZ},
      {listener->upX, listener->upY, listener->upZ}};
  if (!RecoveredAudioSpatial_ValidateListener(pose)) {
    ++g_audio.telemetry.listenerFailures;
    return false;
  }
  g_audio.listener = pose;
  g_audio.listenerValid = true;
  bool applied = true;
  for (VoiceSlot& slot : g_audio.voices)
    if (slot.voice != nullptr && !ApplyVoiceSpatial(&slot)) applied = false;
  if (!applied) {
    ++g_audio.telemetry.listenerFailures;
    return false;
  }
  ++g_audio.telemetry.listenerUpdates;
  return true;
}

void BackendSetVolume(void*, ESoundStateCategory category, float volume) {
  if (category == SOUND_STATE_CATEGORY_EFFECTS) {
    (void)WindowsAudioRuntime_SetEffectsVolume(volume);
  } else if (category == SOUND_STATE_CATEGORY_VEHICLE) {
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) return;
    g_audio.telemetry.vehicleVolume = volume;
    if (g_audio.vehicleVoice != nullptr)
      (void)g_audio.vehicleVoice->SetVolume(volume, XAUDIO2_COMMIT_NOW);
  } else if (category == SOUND_STATE_CATEGORY_CINEMATIC) {
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) return;
    g_audio.telemetry.cinematicVolume = volume;
    if (g_audio.cinematicVoice != nullptr)
      (void)g_audio.cinematicVoice->SetVolume(volume, XAUDIO2_COMMIT_NOW);
  }
}

void BackendSetActive(void*, bool active) {
  WindowsAudioRuntime_SetApplicationActive(active);
}

void BackendMaintain(void*) { WindowsAudioRuntime_Maintain(); }

bool InstallSyntheticProbeClip(unsigned int milliseconds) {
  if (!g_audio.telemetry.configured || milliseconds < 100u ||
      milliseconds > 2000u)
    return false;
  const std::uint32_t sampleRate = 22050u;
  const std::size_t sampleCount =
      static_cast<std::size_t>(sampleRate) * milliseconds / 1000u;
  CachedClip clip;
  clip.wav.channels = 1u;
  clip.wav.bitsPerSample = 16u;
  clip.wav.sampleRate = sampleRate;
  clip.wav.blockAlign = 2u;
  clip.wav.averageBytesPerSecond = sampleRate * 2u;
  try {
    clip.wav.samples.resize(sampleCount * 2u);
  } catch (...) {
    return false;
  }
  for (std::size_t index = 0; index < sampleCount; ++index) {
    const double phase = 2.0 * 3.14159265358979323846 * 440.0 *
                         static_cast<double>(index) /
                         static_cast<double>(sampleRate);
    const short sample = static_cast<short>(std::sin(phase) * 8192.0);
    clip.wav.samples[index * 2u] =
        static_cast<std::uint8_t>(sample & 0xff);
    clip.wav.samples[index * 2u + 1u] =
        static_cast<std::uint8_t>((sample >> 8) & 0xff);
  }
  std::vector<SoundStatePlaybackToken> loopTokens;
  for (const auto& entry : g_audio.loops)
    loopTokens.push_back(entry.first);
  for (SoundStatePlaybackToken loopToken : loopTokens)
    BackendStop(nullptr, loopToken);
  std::vector<SoundStatePlaybackToken> streamTokens;
  for (const auto& entry : g_audio.streams)
    streamTokens.push_back(entry.first);
  for (SoundStatePlaybackToken streamToken : streamTokens)
    BackendStop(nullptr, streamToken);
  DestroyAllVoices(true);
  const auto existing = g_audio.clips.find(kSyntheticProbeKey);
  if (existing != g_audio.clips.end()) g_audio.clips.erase(existing);
  g_audio.clips.emplace(kSyntheticProbeKey, std::move(clip));
  return true;
}

bool RecoverDeviceAndLoops(HRESULT deviceError) {
  if (!g_audio.telemetry.configured || !g_audio.telemetry.deviceReady)
    return false;
  ++g_audio.telemetry.deviceLosses;
  SetError("XAudio2 reported a critical device error", deviceError);
  DestroyDevice(false);
  if (!g_audio.telemetry.physicalOutputEnabled || !InitializeDevice()) {
    g_audio.telemetry.loopRecoveryFailures +=
        static_cast<unsigned int>(g_audio.loops.size());
    g_audio.telemetry.streamRecoveryFailures +=
        static_cast<unsigned int>(g_audio.streams.size());
    return false;
  }
  ++g_audio.telemetry.deviceRecoveries;
  bool exact = true;
  for (const auto& entry : g_audio.loops) {
    const LoopRegistration& loop = entry.second;
    SoundStatePlaybackToken restored = 0;
    if (!StartCachedClip(loop.clipKey, loop.intensity, true, &restored,
                         loop.token, true, loop.spatial, loop.category,
                         loop.pitch) ||
        restored != loop.token) {
      ++g_audio.telemetry.loopRecoveryFailures;
      exact = false;
    }
  }
  for (const auto& entry : g_audio.streams) {
    const StreamRegistration& stream = entry.second;
    SoundStatePlaybackToken restored = 0;
    if (!StartStreamClip(stream.clipKey, stream.intensity, stream.playCount,
                         &restored, stream.token, true) ||
        restored != stream.token) {
      ++g_audio.telemetry.streamRecoveryFailures;
      exact = false;
    }
  }
  return exact;
}

}  // namespace

bool WindowsAudioRuntime_Configure(float effectsVolume, float vehicleVolume,
                                   float cinematicVolume,
                                   bool enablePhysicalOutput) {
  if (g_audio.telemetry.configured || SoundState_BackendConfigured() ||
      !std::isfinite(effectsVolume) ||
      effectsVolume < 0.0f || effectsVolume > 1.0f ||
      !std::isfinite(vehicleVolume) || vehicleVolume < 0.0f ||
      vehicleVolume > 1.0f || !std::isfinite(cinematicVolume) ||
      cinematicVolume < 0.0f || cinematicVolume > 1.0f)
    return false;
  g_audio.telemetry = {};
  g_audio.telemetry.configured = true;
  g_audio.telemetry.physicalOutputEnabled = false;
  g_audio.telemetry.applicationActive = true;
  g_audio.telemetry.effectsVolume = effectsVolume;
  g_audio.telemetry.vehicleVolume = vehicleVolume;
  g_audio.telemetry.cinematicVolume = cinematicVolume;
  g_audio.nextToken = 1;
  g_audio.listenerValid = false;
  g_audio.listener = {};
  // Publish the process configuration through the neutral owner before it
  // installs the callback table; ConfigureBackend then applies these exact
  // category values instead of the neutral defaults.
  if (!SoundState_SetCategoryVolume(SOUND_STATE_CATEGORY_EFFECTS,
                                    effectsVolume) ||
      !SoundState_SetCategoryVolume(SOUND_STATE_CATEGORY_VEHICLE,
                                    vehicleVolume) ||
      !SoundState_SetCategoryVolume(SOUND_STATE_CATEGORY_CINEMATIC,
                                    cinematicVolume)) {
    g_audio.telemetry.configured = false;
    return false;
  }
  SSoundStateBackend backend = {};
  backend.abiVersion = kBackendAbiVersion;
  backend.owner = &g_audio;
  backend.admit = BackendAdmit;
  backend.start = BackendStart;
  backend.stop = BackendStop;
  backend.move = BackendMove;
  backend.setListener = BackendSetListener;
  backend.setPitch = BackendSetPitch;
  backend.setCategoryVolume = BackendSetVolume;
  backend.setApplicationActive = BackendSetActive;
  backend.maintain = BackendMaintain;
  if (!SoundState_ConfigureBackend(&backend)) {
    g_audio.telemetry.configured = false;
    SetError("maintained sound-state backend boundary was unavailable");
    return false;
  }
  if (enablePhysicalOutput) (void)WindowsAudioRuntime_EnablePhysicalOutput();
  return true;
}

bool WindowsAudioRuntime_EnablePhysicalOutput() {
  if (!g_audio.telemetry.configured) return false;
  if (g_audio.telemetry.physicalOutputEnabled)
    return g_audio.telemetry.deviceReady;
  g_audio.telemetry.physicalOutputEnabled = true;
  if (!AcquireDeviceCom() || !InitializeDevice()) return false;
  bool exact = true;
  for (const auto& entry : g_audio.loops) {
    const LoopRegistration& loop = entry.second;
    SoundStatePlaybackToken materialized = 0;
    if (!StartCachedClip(loop.clipKey, loop.intensity, true, &materialized,
                         loop.token, false, loop.spatial, loop.category,
                         loop.pitch) ||
        materialized != loop.token) {
      ++g_audio.telemetry.loopRecoveryFailures;
      exact = false;
    }
  }
  for (const auto& entry : g_audio.streams) {
    const StreamRegistration& stream = entry.second;
    SoundStatePlaybackToken materialized = 0;
    if (!StartStreamClip(stream.clipKey, stream.intensity, stream.playCount,
                         &materialized, stream.token, false) ||
        materialized != stream.token) {
      ++g_audio.telemetry.streamRecoveryFailures;
      exact = false;
    }
  }
  return exact;
}

void WindowsAudioRuntime_Shutdown() {
  if (!g_audio.telemetry.configured) return;
  SoundState_ClearBackend(&g_audio);
  DestroyDevice(true);
  if (g_audio.comOwned) CoUninitialize();
  g_audio.comOwned = false;
  g_audio.clips.clear();
  g_audio.streamClips.clear();
  g_audio.loops.clear();
  g_audio.streams.clear();
  g_audio.listenerValid = false;
  g_audio.listener = {};
  g_audio.telemetry.cachedSampleBytes = 0;
  g_audio.telemetry.activeLoopRegistrations = 0;
  g_audio.telemetry.activeStreamRegistrations = 0;
  g_audio.telemetry.configured = false;
}

void WindowsAudioRuntime_Maintain() {
  if (!g_audio.telemetry.configured) return;
  ++g_audio.telemetry.maintenanceCalls;
  ReapVoices();
  for (VoiceSlot& slot : g_audio.voices)
    if (slot.voice != nullptr && slot.streaming)
      (void)PumpStreamVoice(&slot);
  ReapVoices();
  HRESULT deviceError = S_OK;
  if (!g_audio.engineCallback.TakeCritical(&deviceError)) return;
  (void)RecoverDeviceAndLoops(deviceError);
}

void WindowsAudioRuntime_SetApplicationActive(bool active) {
  if (!g_audio.telemetry.configured ||
      g_audio.telemetry.applicationActive == active)
    return;
  g_audio.telemetry.applicationActive = active;
  if (g_audio.engine == nullptr) return;
  if (active) {
    if (SUCCEEDED(g_audio.engine->StartEngine()))
      ++g_audio.telemetry.focusResumes;
  } else {
    g_audio.engine->StopEngine();
    ++g_audio.telemetry.focusSuspends;
  }
}

bool WindowsAudioRuntime_SetEffectsVolume(float volume) {
  if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f)
    return false;
  g_audio.telemetry.effectsVolume = volume;
  return g_audio.effectsVoice == nullptr ||
      SUCCEEDED(g_audio.effectsVoice->SetVolume(volume,
                                                XAUDIO2_COMMIT_NOW));
}

const SWindowsAudioRuntimeTelemetry* WindowsAudioRuntime_Telemetry() {
  return &g_audio.telemetry;
}

bool WindowsAudioRuntime_StartListeningProbe(unsigned int milliseconds) {
  if (!g_audio.telemetry.deviceReady ||
      !InstallSyntheticProbeClip(milliseconds))
    return false;
  SoundStatePlaybackToken token = 0;
  return StartCachedClip(kSyntheticProbeKey, 1.0f, false, &token);
}

bool WindowsAudioRuntime_StartLoopingProbe(unsigned int milliseconds) {
  if (!InstallSyntheticProbeClip(milliseconds)) return false;
  SoundStatePlaybackToken token = 0;
  return StartCachedClip(kSyntheticProbeKey, 1.0f, true, &token);
}

bool WindowsAudioRuntime_StartMovingLoopProbe(unsigned int milliseconds) {
  if (!InstallSyntheticProbeClip(milliseconds)) return false;
  const SSoundStateListenerPose listener = {
      0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f,
      0.0f, 1.0f, 0.0f};
  if (!BackendSetListener(nullptr, &listener)) return false;
  SpatialSourceState spatial;
  spatial.positionValid = true;
  spatial.position = {-25.0f, 0.0f, 0.0f};
  spatial.model = {5.0f, 5.0f, 100.0f, 100.0f, 1.0f};
  SoundStatePlaybackToken token = 0;
  const bool started = StartCachedClip(kSyntheticProbeKey, 1.0f, true,
                                       &token, 0, false, spatial);
  if (started) ++g_audio.telemetry.positionedRegistrations;
  return started;
}

bool WindowsAudioRuntime_StartVehicleEngineProbe(unsigned int milliseconds) {
  if (!InstallSyntheticProbeClip(milliseconds)) return false;
  SoundStatePlaybackToken token = 0;
  return StartCachedClip(kSyntheticProbeKey, 1.0f, true, &token, 0, false,
                         {}, SOUND_STATE_CATEGORY_VEHICLE, 1.0f);
}

bool WindowsAudioRuntime_SetVehicleEngineProbePitch(float ratio) {
  for (const auto& entry : g_audio.loops) {
    if (entry.second.clipKey == kSyntheticProbeKey &&
        entry.second.category == SOUND_STATE_CATEGORY_VEHICLE)
      return BackendSetPitch(nullptr, entry.first, ratio);
  }
  ++g_audio.telemetry.vehiclePitchFailures;
  return false;
}

bool WindowsAudioRuntime_MoveListeningProbe(float x, float y, float z) {
  std::vector<SoundStatePlaybackToken> tokens;
  for (const auto& entry : g_audio.loops)
    if (entry.second.clipKey == kSyntheticProbeKey)
      tokens.push_back(entry.first);
  bool moved = !tokens.empty();
  for (SoundStatePlaybackToken token : tokens)
    moved = BackendMove(nullptr, token, x, y, z) && moved;
  return moved;
}

bool WindowsAudioRuntime_StopListeningProbe() {
  bool stopped = false;
  std::vector<SoundStatePlaybackToken> tokens;
  for (const auto& entry : g_audio.loops)
    if (entry.second.clipKey == kSyntheticProbeKey)
      tokens.push_back(entry.first);
  for (const VoiceSlot& slot : g_audio.voices)
    if (slot.voice != nullptr && slot.clipKey == kSyntheticProbeKey &&
        g_audio.loops.find(slot.token) == g_audio.loops.end())
      tokens.push_back(slot.token);
  for (SoundStatePlaybackToken token : tokens) {
    if (g_audio.loops.find(token) != g_audio.loops.end())
      BackendStop(nullptr, token);
    else {
      for (VoiceSlot& slot : g_audio.voices)
        if (slot.voice != nullptr && slot.token == token)
          DestroyVoice(&slot, false, true);
    }
    stopped = true;
  }
  return stopped;
}

bool WindowsAudioRuntime_ListeningProbeActive() {
  ReapVoices();
  for (const VoiceSlot& slot : g_audio.voices)
    if (slot.voice != nullptr) return true;
  return false;
}

bool WindowsAudioRuntime_TestOnlySimulateDeviceLoss() {
  return RecoverDeviceAndLoops(E_FAIL);
}

}  // namespace rr2nw
