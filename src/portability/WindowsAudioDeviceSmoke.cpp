#include "WindowsAudioRuntime.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
  const bool deviceCheck =
      argc == 2 && std::strcmp(argv[1], "--device-check") == 0;
  if (!listen && !listenLoop && !listenMovingLoop &&
      !listenVehiclePitch && !vehiclePitchHeadless && !deviceCheck) {
    std::fprintf(stderr,
                 "usage: rr2nw_audio_device_smoke "
                 "--device-check | --listen | --listen-loop | "
                 "--listen-moving-loop | --listen-vehicle-pitch\n"
                 "       rr2nw_audio_device_smoke --vehicle-pitch-headless\n"
                 "Only --listen modes play a short synthetic tone.\n");
    return EXIT_FAILURE;
  }
  if (!rr2nw::WindowsAudioRuntime_Configure(
          0.35f, 0.45f,
          !listenLoop && !listenMovingLoop && !listenVehiclePitch &&
              !vehiclePitchHeadless)) {
    std::fprintf(stderr, "audio-device-smoke: backend boundary failed\n");
    return EXIT_FAILURE;
  }
  const rr2nw::SWindowsAudioRuntimeTelemetry* telemetry =
      rr2nw::WindowsAudioRuntime_Telemetry();
  if (telemetry == nullptr ||
      (!listenLoop && !listenMovingLoop && !listenVehiclePitch &&
       !vehiclePitchHeadless && !telemetry->deviceReady)) {
    std::fprintf(stderr, "audio-device-smoke: %s\n",
                 telemetry == nullptr ? "no telemetry" : telemetry->lastError);
    rr2nw::WindowsAudioRuntime_Shutdown();
    return EXIT_FAILURE;
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
