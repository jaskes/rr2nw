#include "WindowsAudioRuntime.h"
#include "RecoveredModRuntime.h"
#include "sound.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void Put16(std::vector<std::uint8_t>* bytes, std::uint16_t value) {
  bytes->push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
}

void Put32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (unsigned int shift = 0; shift < 32u; shift += 8u)
    bytes->push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

bool WriteGeneratedStream(const std::string& path) {
  constexpr std::uint32_t sampleRate = 22050u;
  constexpr std::uint32_t seconds = 3u;
  constexpr std::uint32_t sampleBytes = sampleRate * seconds * 2u;
  std::vector<std::uint8_t> wav;
  try {
    wav.reserve(44u + sampleBytes);
  } catch (...) {
    return false;
  }
  wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
  Put32(&wav, 36u + sampleBytes);
  wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  Put32(&wav, 16u);
  Put16(&wav, 1u);
  Put16(&wav, 1u);
  Put32(&wav, sampleRate);
  Put32(&wav, sampleRate * 2u);
  Put16(&wav, 2u);
  Put16(&wav, 16u);
  wav.insert(wav.end(), {'d', 'a', 't', 'a'});
  Put32(&wav, sampleBytes);
  for (std::uint32_t index = 0; index < sampleRate * seconds; ++index) {
    const std::uint32_t segment = index / (sampleRate / 4u);
    const std::uint32_t period = segment % 2u == 0u ? 50u : 67u;
    const std::int16_t sample =
        index % period < period / 2u ? 4096 : -4096;
    Put16(&wav, static_cast<std::uint16_t>(sample));
  }
  std::FILE* file = nullptr;
  if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
    return false;
  const bool written = std::fwrite(wav.data(), 1u, wav.size(), file) ==
                       wav.size();
  return std::fclose(file) == 0 && written;
}

struct GeneratedStreamFixture {
  std::string originalDirectory;
  std::string root;
  std::string soundDirectory;
  std::string levelDirectory;
  std::string wavPath;
  bool configured = false;

  bool Open() {
    char original[MAX_PATH] = {};
    char temporary[MAX_PATH] = {};
    if (GetCurrentDirectoryA(MAX_PATH, original) == 0u ||
        GetTempPathA(MAX_PATH, temporary) == 0u)
      return false;
    originalDirectory = original;
    root = std::string(temporary) + "rr2nw-audio-stream-" +
           std::to_string(GetCurrentProcessId());
    soundDirectory = root + "\\SOUND";
    levelDirectory = root + "\\Level.Probe";
    wavPath = soundDirectory + "\\stream-probe.wav";
    if ((!CreateDirectoryA(root.c_str(), nullptr) &&
         GetLastError() != ERROR_ALREADY_EXISTS) ||
        (!CreateDirectoryA(soundDirectory.c_str(), nullptr) &&
         GetLastError() != ERROR_ALREADY_EXISTS) ||
        (!CreateDirectoryA(levelDirectory.c_str(), nullptr) &&
         GetLastError() != ERROR_ALREADY_EXISTS) ||
        !WriteGeneratedStream(wavPath) ||
        !RecoveredModRuntime_Configure(root.c_str(), nullptr))
      return false;
    configured = true;
    if (!SetCurrentDirectoryA(levelDirectory.c_str())) return false;
    return true;
  }

  ~GeneratedStreamFixture() {
    if (configured) RecoveredModRuntime_Release();
    if (!originalDirectory.empty())
      SetCurrentDirectoryA(originalDirectory.c_str());
    if (!wavPath.empty()) DeleteFileA(wavPath.c_str());
    if (!levelDirectory.empty()) RemoveDirectoryA(levelDirectory.c_str());
    if (!soundDirectory.empty()) RemoveDirectoryA(soundDirectory.c_str());
    if (!root.empty()) RemoveDirectoryA(root.c_str());
  }
};

}  // namespace

int main(int argc, char** argv) {
  const bool listen = argc == 2 && std::strcmp(argv[1], "--listen") == 0;
  const bool listenLoop =
      argc == 2 && std::strcmp(argv[1], "--listen-loop") == 0;
  const bool listenMovingLoop =
      argc == 2 && std::strcmp(argv[1], "--listen-moving-loop") == 0;
  const bool listenVehiclePitch =
      argc == 2 && std::strcmp(argv[1], "--listen-vehicle-pitch") == 0;
  const bool vehiclePitchHeadless =
      argc == 2 && std::strcmp(argv[1], "--vehicle-pitch-headless") == 0;
  const bool streamHeadless =
      argc == 2 && std::strcmp(argv[1], "--stream-headless") == 0;
  const bool listenStream =
      argc == 2 && std::strcmp(argv[1], "--listen-stream") == 0;
  const bool deviceCheck =
      argc == 2 && std::strcmp(argv[1], "--device-check") == 0;
  if (!listen && !listenLoop && !listenMovingLoop &&
      !listenVehiclePitch && !vehiclePitchHeadless && !streamHeadless &&
      !listenStream && !deviceCheck) {
    std::fprintf(stderr,
                 "usage: rr2nw_audio_device_smoke "
                 "--device-check | --listen | --listen-loop | "
                 "--listen-moving-loop | --listen-vehicle-pitch\n"
                 "       rr2nw_audio_device_smoke --vehicle-pitch-headless | "
                 "--stream-headless | --listen-stream\n"
                 "Only --listen modes play a short synthetic tone.\n");
    return EXIT_FAILURE;
  }
  GeneratedStreamFixture streamFixture;
  if ((streamHeadless || listenStream) && !streamFixture.Open()) {
    std::fprintf(stderr, "audio-device-smoke: stream fixture failed\n");
    return EXIT_FAILURE;
  }
  if (!rr2nw::WindowsAudioRuntime_Configure(
          0.35f, 0.45f, 0.55f,
          !listenLoop && !listenMovingLoop && !listenVehiclePitch &&
              !vehiclePitchHeadless && !streamHeadless)) {
    std::fprintf(stderr, "audio-device-smoke: backend boundary failed\n");
    return EXIT_FAILURE;
  }
  const rr2nw::SWindowsAudioRuntimeTelemetry* telemetry =
      rr2nw::WindowsAudioRuntime_Telemetry();
  if (telemetry == nullptr ||
      (!listenLoop && !listenMovingLoop && !listenVehiclePitch &&
       !vehiclePitchHeadless && !streamHeadless &&
       !telemetry->deviceReady)) {
    std::fprintf(stderr, "audio-device-smoke: %s\n",
                 telemetry == nullptr ? "no telemetry" : telemetry->lastError);
    rr2nw::WindowsAudioRuntime_Shutdown();
    return EXIT_FAILURE;
  }
  if (streamHeadless || listenStream) {
    const char* path = "..\\SOUND\\stream-probe.wav";
    SSoundStatePlaybackRequest request = {
        path, 1, 1, 1.0f, 0, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, SOUND_STATE_CATEGORY_CINEMATIC};
    SoundStatePlaybackToken token = 0;
    const bool started = SoundState_AdmitWave(path, 1) &&
        SoundState_StartPlayback(&request, &token) && token != 0;
    if (!started) {
      telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
      std::fprintf(stderr, "audio-device-smoke: stream start failed: %s\n",
                   telemetry->lastError);
      rr2nw::WindowsAudioRuntime_Shutdown();
      return EXIT_FAILURE;
    }
    bool completed = false;
    if (listenStream) {
      Sleep(250u);
      rr2nw::WindowsAudioRuntime_SetApplicationActive(false);
      Sleep(50u);
      rr2nw::WindowsAudioRuntime_SetApplicationActive(true);
      const bool recovered =
          rr2nw::WindowsAudioRuntime_TestOnlySimulateDeviceLoss();
      const ULONGLONG deadline = GetTickCount64() + 6000u;
      do {
        rr2nw::WindowsAudioRuntime_Maintain();
        Sleep(10u);
        telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
      } while (telemetry->activeStreamRegistrations != 0u &&
               GetTickCount64() < deadline);
      completed = recovered && telemetry->activeStreamRegistrations == 0u &&
                  telemetry->streamCompletions == 1u &&
                  telemetry->focusSuspends == 1u &&
                  telemetry->focusResumes == 1u &&
                  telemetry->deviceLosses == 1u &&
                  telemetry->deviceRecoveries == 1u &&
                  telemetry->streamRestarts == 1u &&
                  telemetry->streamRecoveryFailures == 0u;
    } else {
      telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
      completed = !telemetry->deviceReady &&
          telemetry->activeStreamRegistrations == 1u &&
          telemetry->deferredStreamRegistrations == 1u;
      SoundState_StopPlayback(&token);
    }
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    const bool exact = completed && telemetry->admittedStreams == 1u &&
        telemetry->streamRequests == 1u &&
        telemetry->streamRegistrations == 1u &&
        telemetry->rejectedStreams == 0u &&
        telemetry->cinematicVolume == 0.55f &&
        (streamHeadless
             ? telemetry->streamStops == 1u &&
                   telemetry->activeStreamRegistrations == 0u &&
                   telemetry->streamBufferSubmissions == 0u
             : telemetry->streamStarts == 1u &&
                   telemetry->streamBufferSubmissions >= 6u &&
                   telemetry->streamedSampleBytes == 264600u);
    std::printf("audio stream mode=%s exact=%u lifecycle=%u/%u/%u/%u "
                "buffers=%u/%zu volume=%.2f\n",
                streamHeadless ? "headless" : "listening",
                exact ? 1u : 0u, telemetry->streamRegistrations,
                telemetry->deferredStreamRegistrations,
                telemetry->streamStarts, telemetry->streamStops,
                telemetry->streamBufferSubmissions,
                telemetry->streamedSampleBytes, telemetry->cinematicVolume);
    rr2nw::WindowsAudioRuntime_Shutdown();
    return exact ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (!listenLoop && !listenMovingLoop && !listenVehiclePitch &&
      !vehiclePitchHeadless) {
    rr2nw::WindowsAudioRuntime_SetApplicationActive(false);
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    const bool suspended = telemetry != nullptr &&
        !telemetry->applicationActive && telemetry->focusSuspends == 1u;
    rr2nw::WindowsAudioRuntime_SetApplicationActive(true);
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    const bool resumed = telemetry != nullptr && telemetry->deviceReady &&
        telemetry->applicationActive && telemetry->focusResumes == 1u;
    if (!suspended || !resumed) {
      std::fprintf(stderr,
                   "audio-device-smoke: focus suspend/resume failed\n");
      rr2nw::WindowsAudioRuntime_Shutdown();
      return EXIT_FAILURE;
    }
  }
  if (deviceCheck) {
    std::printf("audio device=XAudio2-2.9 ready=1 output=silent focus=1/1\n");
    rr2nw::WindowsAudioRuntime_Shutdown();
    return EXIT_SUCCESS;
  }
  if (vehiclePitchHeadless) {
    const bool registered =
        rr2nw::WindowsAudioRuntime_StartVehicleEngineProbe(150u);
    const bool lower =
        rr2nw::WindowsAudioRuntime_SetVehicleEngineProbePitch(0.15f);
    const bool upper =
        rr2nw::WindowsAudioRuntime_SetVehicleEngineProbePitch(4.0f);
    const bool rejectedLow =
        !rr2nw::WindowsAudioRuntime_SetVehicleEngineProbePitch(0.149f);
    const bool rejectedHigh =
        !rr2nw::WindowsAudioRuntime_SetVehicleEngineProbePitch(4.001f);
    const bool stopped = rr2nw::WindowsAudioRuntime_StopListeningProbe();
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    const bool exact = registered && lower && upper && rejectedLow &&
        rejectedHigh && stopped && telemetry != nullptr &&
        !telemetry->deviceReady && telemetry->vehicleLoopRegistrations == 1u &&
        telemetry->vehicleLoopStops == 1u &&
        telemetry->vehiclePitchUpdates == 2u &&
        telemetry->vehiclePitchFailures == 2u &&
        telemetry->deferredLoopRegistrations == 1u &&
        telemetry->activeLoopRegistrations == 0u &&
        telemetry->vehicleVolume == 0.45f;
    std::printf("audio vehicle headless=%u lifecycle=%u/%u pitch=%u/%u "
                "volume=%.2f\n",
                exact ? 1u : 0u, telemetry->vehicleLoopRegistrations,
                telemetry->vehicleLoopStops,
                telemetry->vehiclePitchUpdates,
                telemetry->vehiclePitchFailures,
                telemetry->vehicleVolume);
    rr2nw::WindowsAudioRuntime_Shutdown();
    return exact ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (listenLoop || listenMovingLoop || listenVehiclePitch) {
    const bool registered = listenVehiclePitch
        ? rr2nw::WindowsAudioRuntime_StartVehicleEngineProbe(150u)
        : listenMovingLoop
        ? rr2nw::WindowsAudioRuntime_StartMovingLoopProbe(150u)
        : rr2nw::WindowsAudioRuntime_StartLoopingProbe(150u);
    if (!registered) {
      telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
      std::fprintf(stderr,
                   "audio-device-smoke: loop registration failed "
                   "configured=%u device=%u requests=%u registrations=%u "
                   "deferred=%u error=%s\n",
                   telemetry->configured ? 1u : 0u,
                   telemetry->deviceReady ? 1u : 0u,
                   telemetry->loopRequests,
                   telemetry->loopRegistrations,
                   telemetry->deferredLoopRegistrations,
                   telemetry->lastError);
      rr2nw::WindowsAudioRuntime_Shutdown();
      return EXIT_FAILURE;
    }
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    const bool deferred = !telemetry->deviceReady &&
        telemetry->deferredLoopRegistrations == 1u &&
        telemetry->activeLoopRegistrations == 1u &&
        telemetry->activeLoopVoices == 0u;
    const bool materialized =
        rr2nw::WindowsAudioRuntime_EnablePhysicalOutput();
    bool movedSpatially = true;
    if (listenMovingLoop) {
      Sleep(300u);
      movedSpatially =
          rr2nw::WindowsAudioRuntime_MoveListeningProbe(25.0f, 0.0f, 0.0f);
      Sleep(300u);
      movedSpatially =
          rr2nw::WindowsAudioRuntime_MoveListeningProbe(100.0f, 0.0f, 0.0f) &&
          movedSpatially;
      Sleep(300u);
    }
    bool pitched = true;
    if (listenVehiclePitch) {
      pitched = rr2nw::WindowsAudioRuntime_SetVehicleEngineProbePitch(0.6f);
      Sleep(300u);
      pitched = rr2nw::WindowsAudioRuntime_SetVehicleEngineProbePitch(1.8f) &&
                pitched;
      Sleep(300u);
    }
    rr2nw::WindowsAudioRuntime_SetApplicationActive(false);
    rr2nw::WindowsAudioRuntime_SetApplicationActive(true);
    Sleep(250u);
    rr2nw::WindowsAudioRuntime_Maintain();
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    const bool activeBeforeLoss = deferred && materialized &&
        rr2nw::WindowsAudioRuntime_ListeningProbeActive() &&
        telemetry->activeLoopVoices == 1u && telemetry->loopStarts == 1u &&
        telemetry->focusSuspends == 1u && telemetry->focusResumes == 1u;
    const bool recovered =
        rr2nw::WindowsAudioRuntime_TestOnlySimulateDeviceLoss();
    Sleep(250u);
    rr2nw::WindowsAudioRuntime_Maintain();
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    const bool remainedActive = activeBeforeLoss && recovered &&
        rr2nw::WindowsAudioRuntime_ListeningProbeActive() &&
        telemetry->activeLoopVoices == 1u &&
        telemetry->deviceLosses == 1u &&
        telemetry->deviceRecoveries == 1u &&
        telemetry->loopRestarts == 1u &&
        telemetry->loopRecoveryFailures == 0u;
    const bool spatialExact = !listenMovingLoop ||
        (movedSpatially && telemetry->positionedRegistrations == 1u &&
         telemetry->emitterMoveUpdates == 2u &&
         telemetry->emitterMoveFailures == 0u &&
         telemetry->listenerUpdates == 1u &&
         telemetry->listenerFailures == 0u &&
         telemetry->spatialApplications >= 4u &&
         telemetry->spatialSilentApplications >= 1u &&
         telemetry->asymmetricModelFallbacks == 0u &&
         telemetry->nonMonoSpatialFallbacks == 0u);
    const bool vehicleExact = !listenVehiclePitch ||
        (pitched && telemetry->vehicleLoopRegistrations == 1u &&
         telemetry->vehiclePitchUpdates == 2u &&
         telemetry->vehiclePitchFailures == 0u &&
         telemetry->vehicleVolume == 0.45f);
    const bool stopped = rr2nw::WindowsAudioRuntime_StopListeningProbe() &&
        !rr2nw::WindowsAudioRuntime_ListeningProbeActive();
    telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
    std::printf("audio loop device=%s deferred=%u active=%u stopped=%u "
                "lifecycle=%u/%u/%u recovery=%u/%u/%u "
                "spatial=%u/%u/%u/%u/%u vehicle=%u/%u/%u/%u\n",
                telemetry->deviceReady ? "XAudio2-2.9" : "unavailable",
                deferred ? 1u : 0u, remainedActive ? 1u : 0u,
                stopped ? 1u : 0u,
                telemetry->loopRequests, telemetry->loopStarts,
                telemetry->loopStops, telemetry->deviceLosses,
                telemetry->deviceRecoveries,
                telemetry->loopRecoveryFailures,
                telemetry->positionedRegistrations,
                telemetry->emitterMoveUpdates,
                telemetry->listenerUpdates,
                telemetry->spatialApplications,
                telemetry->spatialSilentApplications,
                telemetry->vehicleLoopRegistrations,
                telemetry->vehiclePitchUpdates,
                telemetry->vehiclePitchFailures,
                telemetry->vehicleLoopStops);
    rr2nw::WindowsAudioRuntime_Shutdown();
    return remainedActive && spatialExact && vehicleExact && stopped
        ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (!rr2nw::WindowsAudioRuntime_StartListeningProbe(500u)) {
    std::fprintf(stderr, "audio-device-smoke: %s\n", telemetry->lastError);
    rr2nw::WindowsAudioRuntime_Shutdown();
    return EXIT_FAILURE;
  }
  const ULONGLONG deadline = GetTickCount64() + 2500u;
  while (rr2nw::WindowsAudioRuntime_ListeningProbeActive() &&
         GetTickCount64() < deadline) {
    rr2nw::WindowsAudioRuntime_Maintain();
    Sleep(10u);
  }
  const bool completed =
      !rr2nw::WindowsAudioRuntime_ListeningProbeActive();
  telemetry = rr2nw::WindowsAudioRuntime_Telemetry();
  std::printf("audio device=%s starts=%u completed=%u focus=%s\n",
              telemetry->deviceReady ? "XAudio2-2.9" : "unavailable",
              telemetry->playbackStarts, telemetry->completedVoices,
              telemetry->applicationActive ? "active" : "suspended");
  rr2nw::WindowsAudioRuntime_Shutdown();
  return completed ? EXIT_SUCCESS : EXIT_FAILURE;
}
