#include "WindowsAudioRuntime.h"

#include "RecoveredModRuntime.h"
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
#include <map>
#include <new>
#include <string>
#include <vector>

namespace rr2nw {
namespace {

constexpr unsigned int kBackendAbiVersion = 1u;
// The installed corpus contains 86 WAVs / about 38 MiB. Keep the process-wide
// cache bounded while allowing every retail Level transition to retain the
// union without turning late-campaign sounds into deterministic rejections.
constexpr std::size_t kMaximumCachedClips = 128u;
constexpr std::size_t kMaximumCachedSampleBytes = 64u * 1024u * 1024u;
constexpr std::size_t kMaximumVoices = 32u;
constexpr char kSyntheticProbeKey[] = "__rr2nw_listening_probe__";

struct CachedClip {
  SRecoveredPcmWav wav;
};

struct LoopRegistration {
  std::string clipKey;
  float intensity = 1.0f;
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
  bool looping = false;
  std::atomic<bool> finished{false};
  std::atomic<bool> failed{false};
  VoiceCallback callback;
};

void VoiceCallback::OnStreamEnd() { slot_->finished.store(true); }
void VoiceCallback::OnBufferEnd(void*) { slot_->finished.store(true); }
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
  EngineCallback engineCallback;
  std::array<VoiceSlot, kMaximumVoices> voices;
  std::map<std::string, CachedClip> clips;
  std::map<SoundStatePlaybackToken, LoopRegistration> loops;
  SoundStatePlaybackToken nextToken = 1;
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

void DestroyVoice(VoiceSlot* slot, bool completed, bool countStop) {
  if (slot == nullptr || slot->voice == nullptr) return;
  slot->voice->Stop(0u, XAUDIO2_COMMIT_NOW);
  slot->voice->FlushSourceBuffers();
  slot->voice->DestroyVoice();
  slot->voice = nullptr;
  slot->token = 0;
  slot->clipKey.clear();
  slot->intensity = 1.0f;
  slot->looping = false;
  slot->finished.store(false);
  slot->failed.store(false);
  if (completed)
    ++g_audio.telemetry.completedVoices;
  else if (countStop)
    ++g_audio.telemetry.stoppedVoices;
  UpdateActiveLoopTelemetry();
}

void DestroyAllVoices(bool countStops) {
  for (VoiceSlot& slot : g_audio.voices)
    DestroyVoice(&slot, false, countStops);
}

void DestroyDevice(bool countStops) {
  DestroyAllVoices(countStops);
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
  g_audio.engine = engine;
  g_audio.masteringVoice = mastering;
  g_audio.effectsVoice = effects;
  g_audio.telemetry.deviceReady = true;
  ++g_audio.telemetry.deviceInitializations;
  g_audio.telemetry.lastError[0] = 0;
  if (!g_audio.telemetry.applicationActive) engine->StopEngine();
  return true;
}

void ReapVoices() {
  for (VoiceSlot& slot : g_audio.voices) {
    if (slot.voice != nullptr && slot.finished.load()) {
      if (slot.failed.load()) ++g_audio.telemetry.playbackFailures;
      DestroyVoice(&slot, true, false);
    }
  }
}

bool AdmitClip(const char* fileName, int flags) {
  if (flags == 1) {
    ++g_audio.telemetry.deferredStreams;
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

bool StartCachedClip(const std::string& key, float intensity, bool looping,
                     SoundStatePlaybackToken* token,
                     SoundStatePlaybackToken recoveredToken = 0,
                     bool deviceRecovery = false) {
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
    LoopRegistration registration = {key, boundedIntensity, assignedToken};
    try {
      g_audio.loops.emplace(assignedToken, std::move(registration));
    } catch (...) {
      ++g_audio.telemetry.playbackFailures;
      SetError("effect loop registration allocation failed");
      return false;
    }
    registeredHere = true;
    ++g_audio.telemetry.loopRegistrations;
    *token = assignedToken;
    UpdateActiveLoopTelemetry();
    if (!g_audio.telemetry.deviceReady) {
      ++g_audio.telemetry.deferredLoopRegistrations;
      return true;
    }
  } else if (looping) {
    const auto registration = g_audio.loops.find(recoveredToken);
    if (registration == g_audio.loops.end() ||
        registration->second.clipKey != key) {
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
  XAUDIO2_SEND_DESCRIPTOR send = {0u, g_audio.effectsVoice};
  XAUDIO2_VOICE_SENDS sends = {1u, &send};
  HRESULT result = g_audio.engine->CreateSourceVoice(
      &slot->voice, &format, 0u, XAUDIO2_DEFAULT_FREQ_RATIO,
      &slot->callback, &sends, nullptr);
  if (FAILED(result) || slot->voice == nullptr) {
    slot->voice = nullptr;
    ++g_audio.telemetry.playbackFailures;
    SetError("XAudio2 source voice could not be created", result);
    rollbackRegistration();
    return false;
  }
  result = slot->voice->SetVolume(boundedIntensity, XAUDIO2_COMMIT_NOW);
  if (FAILED(result)) {
    SetError("XAudio2 source intensity could not be applied", result);
    ++g_audio.telemetry.playbackFailures;
    DestroyVoice(slot, false, false);
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
  slot->looping = looping;
  slot->token = assignedToken;
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
  return StartCachedClip(key, request->intensity,
                         request->playCount == 0, token);
}

void BackendStop(void*, SoundStatePlaybackToken token) {
  if (token == 0) return;
  const bool registeredLoop = g_audio.loops.find(token) != g_audio.loops.end();
  for (VoiceSlot& slot : g_audio.voices) {
    if (slot.voice != nullptr && slot.token == token) {
      DestroyVoice(&slot, false, true);
      break;
    }
  }
  if (registeredLoop) {
    g_audio.loops.erase(token);
    ++g_audio.telemetry.loopStops;
    UpdateActiveLoopTelemetry();
  }
}

void BackendSetVolume(void*, ESoundStateCategory category, float volume) {
  if (category != SOUND_STATE_CATEGORY_EFFECTS) return;
  WindowsAudioRuntime_SetEffectsVolume(volume);
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
    return false;
  }
  ++g_audio.telemetry.deviceRecoveries;
  bool exact = true;
  for (const auto& entry : g_audio.loops) {
    const LoopRegistration& loop = entry.second;
    SoundStatePlaybackToken restored = 0;
    if (!StartCachedClip(loop.clipKey, loop.intensity, true, &restored,
                         loop.token, true) || restored != loop.token) {
      ++g_audio.telemetry.loopRecoveryFailures;
      exact = false;
    }
  }
  return exact;
}

}  // namespace

bool WindowsAudioRuntime_Configure(float effectsVolume,
                                   bool enablePhysicalOutput) {
  if (g_audio.telemetry.configured || !std::isfinite(effectsVolume) ||
      effectsVolume < 0.0f || effectsVolume > 1.0f)
    return false;
  g_audio.telemetry = {};
  g_audio.telemetry.configured = true;
  g_audio.telemetry.physicalOutputEnabled = false;
  g_audio.telemetry.applicationActive = true;
  g_audio.telemetry.effectsVolume = effectsVolume;
  g_audio.nextToken = 1;
  SSoundStateBackend backend = {};
  backend.abiVersion = kBackendAbiVersion;
  backend.owner = &g_audio;
  backend.admit = BackendAdmit;
  backend.start = BackendStart;
  backend.stop = BackendStop;
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
                         loop.token, false) || materialized != loop.token) {
      ++g_audio.telemetry.loopRecoveryFailures;
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
  g_audio.loops.clear();
  g_audio.telemetry.cachedSampleBytes = 0;
  g_audio.telemetry.activeLoopRegistrations = 0;
  g_audio.telemetry.configured = false;
}

void WindowsAudioRuntime_Maintain() {
  if (!g_audio.telemetry.configured) return;
  ++g_audio.telemetry.maintenanceCalls;
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

bool WindowsAudioRuntime_StopListeningProbe() {
  bool stopped = false;
  std::vector<SoundStatePlaybackToken> tokens;
  for (const VoiceSlot& slot : g_audio.voices)
    if (slot.voice != nullptr && slot.clipKey == kSyntheticProbeKey)
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
