#pragma once

#include <cstddef>

namespace rr2nw {

struct SWindowsAudioRuntimeTelemetry {
  bool configured = false;
  bool physicalOutputEnabled = false;
  bool deviceReady = false;
  bool applicationActive = true;
  unsigned int deviceInitializations = 0;
  unsigned int deviceFailures = 0;
  unsigned int deviceLosses = 0;
  unsigned int deviceRecoveries = 0;
  unsigned int admittedClips = 0;
  unsigned int duplicateAdmissions = 0;
  unsigned int rejectedClips = 0;
  unsigned int deferredStreams = 0;
  unsigned int playbackRequests = 0;
  unsigned int playbackStarts = 0;
  unsigned int playbackFailures = 0;
  unsigned int loopRequests = 0;
  unsigned int loopRegistrations = 0;
  unsigned int deferredLoopRegistrations = 0;
  unsigned int loopStarts = 0;
  unsigned int loopStops = 0;
  unsigned int loopRestarts = 0;
  unsigned int loopRecoveryFailures = 0;
  unsigned int activeLoopVoices = 0;
  unsigned int activeLoopRegistrations = 0;
  unsigned int positionedRegistrations = 0;
  unsigned int emitterMoveUpdates = 0;
  unsigned int emitterMoveFailures = 0;
  unsigned int listenerUpdates = 0;
  unsigned int listenerFailures = 0;
  unsigned int spatialApplications = 0;
  unsigned int spatialSilentApplications = 0;
  unsigned int asymmetricModelFallbacks = 0;
  unsigned int nonMonoSpatialFallbacks = 0;
  unsigned int voiceStealsPrevented = 0;
  unsigned int completedVoices = 0;
  unsigned int stoppedVoices = 0;
  unsigned int focusSuspends = 0;
  unsigned int focusResumes = 0;
  unsigned int maintenanceCalls = 0;
  std::size_t cachedSampleBytes = 0;
  float effectsVolume = 1.0f;
  char lastError[256] = {};
};

// Installs the one process-wide XAudio2 2.9 owner. Device creation failure is
// a clean audio-disabled state rather than a game-startup failure; WAV
// admission and telemetry remain available for diagnostics.
bool WindowsAudioRuntime_Configure(float effectsVolume,
                                   bool enablePhysicalOutput);
// Defers physical device creation until all startup-only gameplay probes have
// completed. This prevents verification events from becoming audible while
// still allowing their WAV resources to populate the process cache.
bool WindowsAudioRuntime_EnablePhysicalOutput();
void WindowsAudioRuntime_Shutdown();
void WindowsAudioRuntime_Maintain();
void WindowsAudioRuntime_SetApplicationActive(bool active);
bool WindowsAudioRuntime_SetEffectsVolume(float volume);
const SWindowsAudioRuntimeTelemetry* WindowsAudioRuntime_Telemetry();

// Explicit physical acceptance hook. It never runs in CTest or ordinary game
// startup and uses a generated PCM tone rather than retail media.
bool WindowsAudioRuntime_StartListeningProbe(unsigned int milliseconds);
bool WindowsAudioRuntime_StartLoopingProbe(unsigned int milliseconds);
bool WindowsAudioRuntime_StartMovingLoopProbe(unsigned int milliseconds);
bool WindowsAudioRuntime_MoveListeningProbe(float x, float y, float z);
bool WindowsAudioRuntime_StopListeningProbe();
bool WindowsAudioRuntime_ListeningProbeActive();
bool WindowsAudioRuntime_TestOnlySimulateDeviceLoss();

}  // namespace rr2nw
