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
  unsigned int admittedStreams = 0;
  unsigned int duplicateStreamAdmissions = 0;
  unsigned int rejectedStreams = 0;
  unsigned int streamRequests = 0;
  unsigned int streamRegistrations = 0;
  unsigned int deferredStreamRegistrations = 0;
  unsigned int streamStarts = 0;
  unsigned int streamStops = 0;
  unsigned int streamRestarts = 0;
  unsigned int streamCompletions = 0;
  unsigned int streamBufferSubmissions = 0;
  unsigned int streamUnderruns = 0;
  unsigned int streamRecoveryFailures = 0;
  unsigned int activeStreamVoices = 0;
  unsigned int activeStreamRegistrations = 0;
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
  unsigned int latePositionPromotions = 0;
  unsigned int emitterMoveUpdates = 0;
  unsigned int emitterMoveFailures = 0;
  unsigned int listenerUpdates = 0;
  unsigned int listenerFailures = 0;
  unsigned int spatialApplications = 0;
  unsigned int spatialSilentApplications = 0;
  unsigned int unpositionedEffectSuppressions = 0;
  unsigned int asymmetricModelFallbacks = 0;
  unsigned int nonMonoSpatialFallbacks = 0;
  unsigned int vehicleLoopRegistrations = 0;
  unsigned int vehicleLoopStops = 0;
  unsigned int vehiclePitchUpdates = 0;
  unsigned int vehiclePitchFailures = 0;
  unsigned int voiceStealsPrevented = 0;
  unsigned int completedVoices = 0;
  unsigned int stoppedVoices = 0;
  unsigned int focusSuspends = 0;
  unsigned int focusResumes = 0;
  unsigned int maintenanceCalls = 0;
  bool presentationActive = false;
  unsigned int presentationRequests = 0;
  unsigned int presentationBegins = 0;
  unsigned int presentationEnds = 0;
  unsigned int presentationFailures = 0;
  unsigned int presentationMutedVoiceObservations = 0;
  std::size_t cachedSampleBytes = 0;
  std::size_t streamedSampleBytes = 0;
  float effectsVolume = 1.0f;
  float vehicleVolume = 1.0f;
  float cinematicVolume = 1.0f;
  // Canonical retail basenames only; no physical path or payload is exposed.
  char streamStartSequence[512] = {};
  char activeVoiceSummary[1024] = {};
  char lastError[256] = {};
};

// Installs the one process-wide XAudio2 2.9 owner. Device creation failure is
// a clean audio-disabled state rather than a game-startup failure; WAV
// admission and telemetry remain available for diagnostics.
bool WindowsAudioRuntime_Configure(float effectsVolume, float vehicleVolume,
                                   float cinematicVolume,
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
bool WindowsAudioRuntime_StartVehicleEngineProbe(unsigned int milliseconds);
bool WindowsAudioRuntime_SetVehicleEngineProbePitch(float ratio);
bool WindowsAudioRuntime_StopListeningProbe();
bool WindowsAudioRuntime_ListeningProbeActive();
bool WindowsAudioRuntime_TestOnlySimulateDeviceLoss();

}  // namespace rr2nw
