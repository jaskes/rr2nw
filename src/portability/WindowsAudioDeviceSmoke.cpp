#include "WindowsAudioRuntime.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
  const bool listen = argc == 2 && std::strcmp(argv[1], "--listen") == 0;
  const bool deviceCheck =
      argc == 2 && std::strcmp(argv[1], "--device-check") == 0;
  if (!listen && !deviceCheck) {
    std::fprintf(stderr,
                 "usage: rr2nw_audio_device_smoke "
                 "--device-check | --listen\n"
                 "Only --listen plays a short synthetic tone.\n");
    return EXIT_FAILURE;
  }
  if (!rr2nw::WindowsAudioRuntime_Configure(0.35f, true)) {
    std::fprintf(stderr, "audio-device-smoke: backend boundary failed\n");
    return EXIT_FAILURE;
  }
  const rr2nw::SWindowsAudioRuntimeTelemetry* telemetry =
      rr2nw::WindowsAudioRuntime_Telemetry();
  if (telemetry == nullptr || !telemetry->deviceReady) {
    std::fprintf(stderr, "audio-device-smoke: %s\n",
                 telemetry == nullptr ? "no telemetry" : telemetry->lastError);
    rr2nw::WindowsAudioRuntime_Shutdown();
    return EXIT_FAILURE;
  }
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
  if (deviceCheck) {
    std::printf("audio device=XAudio2-2.9 ready=1 output=silent focus=1/1\n");
    rr2nw::WindowsAudioRuntime_Shutdown();
    return EXIT_SUCCESS;
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
