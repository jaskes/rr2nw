#include "RecoveredAudioSpatial.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

bool Near(float left, float right, float tolerance = 1.0e-4f) {
  return std::fabs(left - right) <= tolerance;
}

int Fail(const char* message) {
  std::fprintf(stderr, "audio-spatial-smoke: %s\n", message);
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  const rr2nw::SRecoveredAudioListenerPose listener = {
      {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}};
  const rr2nw::SRecoveredAudioEmitterModel model = {
      10.0f, 10.0f, 100.0f, 100.0f, 0.75f};
  rr2nw::SRecoveredAudioSpatialResult center;
  rr2nw::SRecoveredAudioSpatialResult right;
  rr2nw::SRecoveredAudioSpatialResult fade;
  rr2nw::SRecoveredAudioSpatialResult far;
  if (!rr2nw::RecoveredAudioSpatial_Evaluate(
          listener, {0.0f, 0.0f, 5.0f}, model, &center) ||
      !center.audible || !Near(center.attenuation, 1.0f) ||
      !Near(center.pan, 0.0f) || !Near(center.left, center.right))
    return Fail("inner ambient region was not centered and full-scale");
  if (!rr2nw::RecoveredAudioSpatial_Evaluate(
          listener, {5.0f, 0.0f, 0.0f}, model, &right) ||
      !right.audible || !Near(right.pan, 1.0f) ||
      !Near(right.left, 0.0f) || !Near(right.right, 1.0f))
    return Fail("listener right-axis panning was not deterministic");
  if (!rr2nw::RecoveredAudioSpatial_Evaluate(
          listener, {0.0f, 0.0f, 55.0f}, model, &fade) ||
      !fade.audible || !Near(fade.attenuation, 0.5f))
    return Fail("inner-to-outer compatibility fade was not linear");
  if (!rr2nw::RecoveredAudioSpatial_Evaluate(
          listener, {0.0f, 0.0f, 100.0f}, model, &far) || far.audible ||
      !Near(far.attenuation, 0.0f))
    return Fail("outer radius did not form a hard inaudible boundary");

  rr2nw::SRecoveredAudioEmitterModel asymmetric = model;
  asymmetric.maxBack = 80.0f;
  rr2nw::SRecoveredAudioListenerPose invalid = listener;
  invalid.front.x = std::numeric_limits<float>::infinity();
  if (rr2nw::RecoveredAudioSpatial_Evaluate(
          listener, {1.0f, 2.0f, 3.0f}, asymmetric, &far) ||
      rr2nw::RecoveredAudioSpatial_Evaluate(
          invalid, {1.0f, 2.0f, 3.0f}, model, &far))
    return Fail("unsupported or invalid geometry was not fail-closed");

  std::printf("audio spatial model=symmetric-linear-compatibility "
              "listener=position/front/up pan=equal-power "
              "inner/mid/outer=1/0.5/0 asymmetric=deferred\n");
  return EXIT_SUCCESS;
}
