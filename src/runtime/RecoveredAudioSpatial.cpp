#include "RecoveredAudioSpatial.h"

#include <algorithm>
#include <cmath>

namespace rr2nw {
namespace {

bool Finite(const SRecoveredAudioVector3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

float Length(const SRecoveredAudioVector3& value) {
  return std::sqrt(value.x * value.x + value.y * value.y +
                   value.z * value.z);
}

SRecoveredAudioVector3 Scale(const SRecoveredAudioVector3& value,
                             float scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const SRecoveredAudioVector3& left,
          const SRecoveredAudioVector3& right) {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

SRecoveredAudioVector3 Cross(const SRecoveredAudioVector3& left,
                             const SRecoveredAudioVector3& right) {
  return {left.y * right.z - left.z * right.y,
          left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

bool NearlyEqual(float left, float right) {
  const float scale = (std::max)(1.0f,
                                 (std::max)(std::fabs(left),
                                            std::fabs(right)));
  return std::fabs(left - right) <= scale * 1.0e-5f;
}

}  // namespace

bool RecoveredAudioSpatial_ValidateListener(
    const SRecoveredAudioListenerPose& listener) {
  if (!Finite(listener.position) || !Finite(listener.front) ||
      !Finite(listener.up))
    return false;
  const float frontLength = Length(listener.front);
  const float upLength = Length(listener.up);
  if (frontLength < 1.0e-4f || upLength < 1.0e-4f) return false;
  const SRecoveredAudioVector3 front =
      Scale(listener.front, 1.0f / frontLength);
  const SRecoveredAudioVector3 up = Scale(listener.up, 1.0f / upLength);
  return Length(Cross(up, front)) >= 1.0e-4f;
}

bool RecoveredAudioSpatial_ValidateSymmetricModel(
    const SRecoveredAudioEmitterModel& model) {
  return std::isfinite(model.minFront) && std::isfinite(model.minBack) &&
         std::isfinite(model.maxFront) && std::isfinite(model.maxBack) &&
         std::isfinite(model.intensity) && model.minFront >= 0.0f &&
         model.maxFront > model.minFront && model.intensity >= 0.0f &&
         NearlyEqual(model.minFront, model.minBack) &&
         NearlyEqual(model.maxFront, model.maxBack);
}

bool RecoveredAudioSpatial_Evaluate(
    const SRecoveredAudioListenerPose& listener,
    const SRecoveredAudioVector3& emitter,
    const SRecoveredAudioEmitterModel& model,
    SRecoveredAudioSpatialResult* result) {
  if (result == nullptr) return false;
  *result = {};
  if (!RecoveredAudioSpatial_ValidateListener(listener) || !Finite(emitter) ||
      !RecoveredAudioSpatial_ValidateSymmetricModel(model))
    return false;

  const SRecoveredAudioVector3 relative = {
      emitter.x - listener.position.x,
      emitter.y - listener.position.y,
      emitter.z - listener.position.z};
  const float distance = Length(relative);
  float attenuation = 1.0f;
  if (distance >= model.maxFront) {
    attenuation = 0.0f;
  } else if (distance > model.minFront) {
    attenuation = (model.maxFront - distance) /
                  (model.maxFront - model.minFront);
  }

  float pan = 0.0f;
  if (distance > 1.0e-5f) {
    const float frontLength = Length(listener.front);
    const float upLength = Length(listener.up);
    const SRecoveredAudioVector3 front =
        Scale(listener.front, 1.0f / frontLength);
    const SRecoveredAudioVector3 up =
        Scale(listener.up, 1.0f / upLength);
    SRecoveredAudioVector3 right = Cross(up, front);
    right = Scale(right, 1.0f / Length(right));
    pan = (std::max)(-1.0f,
                     (std::min)(1.0f,
                                Dot(Scale(relative, 1.0f / distance), right)));
  }
  result->distance = distance;
  result->attenuation = attenuation;
  result->pan = pan;
  result->left = std::sqrt((1.0f - pan) * 0.5f);
  result->right = std::sqrt((1.0f + pan) * 0.5f);
  result->audible = attenuation > 0.0f && model.intensity > 0.0f;
  return true;
}

}  // namespace rr2nw
