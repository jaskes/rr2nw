#pragma once

namespace rr2nw {

struct SRecoveredAudioVector3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct SRecoveredAudioListenerPose {
  SRecoveredAudioVector3 position;
  SRecoveredAudioVector3 front;
  SRecoveredAudioVector3 up;
};

struct SRecoveredAudioEmitterModel {
  float minFront = 0.0f;
  float minBack = 0.0f;
  float maxFront = 0.0f;
  float maxBack = 0.0f;
  float intensity = 1.0f;
};

struct SRecoveredAudioSpatialResult {
  float distance = 0.0f;
  float attenuation = 0.0f;
  float pan = 0.0f;
  float left = 0.0f;
  float right = 0.0f;
  bool audible = false;
};

// RSX documented the inner ellipsoid as ambient and the outer ellipsoid as
// inaudible, but its exact interpolation curve did not survive. This bounded
// compatibility evaluator therefore admits only symmetric authored models,
// uses a linear inner-to-outer fade, and derives equal-power stereo pan from
// the source-proven listener position/front/up coordinate convention.
bool RecoveredAudioSpatial_ValidateListener(
    const SRecoveredAudioListenerPose& listener);
bool RecoveredAudioSpatial_ValidateSymmetricModel(
    const SRecoveredAudioEmitterModel& model);
bool RecoveredAudioSpatial_Evaluate(
    const SRecoveredAudioListenerPose& listener,
    const SRecoveredAudioVector3& emitter,
    const SRecoveredAudioEmitterModel& model,
    SRecoveredAudioSpatialResult* result);

}  // namespace rr2nw
