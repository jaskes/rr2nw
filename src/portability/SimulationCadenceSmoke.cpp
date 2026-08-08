#include "SimulationCadence.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {

int Fail(const char* message) {
  std::cerr << "simulation-cadence-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool SameTicks(const std::vector<double>& left,
               const std::vector<double>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index)
    if (left[index] != right[index]) return false;
  return true;
}

bool AppendSchedule(SimulationCadence* cadence, double delta,
                    int samples, bool focused,
                    std::vector<double>* emitted) {
  if (cadence == nullptr || emitted == nullptr) return false;
  for (int index = 0; index < samples; ++index) {
    std::vector<double> batch;
    if (!cadence->Submit(delta, focused, &batch)) return false;
    emitted->insert(emitted->end(), batch.begin(), batch.end());
  }
  return true;
}

SSimulationCadenceConfig ProbeConfig() {
  SSimulationCadenceConfig config;
  config.fixedStepSeconds = 0.025;
  config.maximumCatchUpTicks = 4u;
  config.maximumFrameDeltaSeconds = 0.1;
  config.hardDeltaLimitSeconds = 2.0;
  return config;
}

}  // namespace

int main() {
  const SSimulationCadenceConfig config = ProbeConfig();

  SimulationCadence dense;
  SimulationCadence sparse;
  std::vector<double> denseTicks;
  std::vector<double> sparseTicks;
  if (!dense.Configure(config, 51.0) ||
      !sparse.Configure(config, 51.0) ||
      !AppendSchedule(&dense, 0.025, 28, true, &denseTicks) ||
      !AppendSchedule(&sparse, 0.1, 7, true, &sparseTicks) ||
      denseTicks.size() != 28u || !SameTicks(denseTicks, sparseTicks))
    return Fail("equal-time dense and sparse schedules diverged");

  const SSimulationCadenceTelemetry denseTelemetry = dense.Telemetry();
  const SSimulationCadenceTelemetry sparseTelemetry = sparse.Telemetry();
  if (denseTelemetry.presentationSamples != 28u ||
      sparseTelemetry.presentationSamples != 7u ||
      denseTelemetry.simulationTicks != 28u ||
      sparseTelemetry.simulationTicks != 28u ||
      denseTelemetry.maximumTicksPerSample != 1u ||
      sparseTelemetry.maximumTicksPerSample != 4u ||
      sparseTelemetry.catchUpSamples != 7u ||
      denseTelemetry.droppedSeconds != 0.0 ||
      sparseTelemetry.droppedSeconds != 0.0)
    return Fail("dense/sparse cadence telemetry diverged");

  SimulationCadence invalid;
  if (!invalid.Configure(config, 7.0))
    return Fail("invalid-sample owner did not configure");
  std::vector<double> sentinel(1u, 123.0);
  const double invalidDeltas[] = {
      0.0,
      -0.025,
      (std::numeric_limits<double>::quiet_NaN)(),
      (std::numeric_limits<double>::infinity)(),
      2.001,
  };
  for (double delta : invalidDeltas) {
    if (invalid.Submit(delta, true, &sentinel) || sentinel.size() != 1u ||
        sentinel[0] != 123.0)
      return Fail("invalid sample mutated output or was admitted");
  }
  const SSimulationCadenceTelemetry invalidTelemetry = invalid.Telemetry();
  if (invalidTelemetry.invalidSamples != 5u ||
      invalidTelemetry.presentationSamples != 0u ||
      invalidTelemetry.simulationTicks != 0u ||
      invalidTelemetry.accumulatorSeconds != 0.0)
    return Fail("invalid sample mutated authoritative cadence state");

  SimulationCadence stalled;
  std::vector<double> stalledTicks;
  if (!stalled.Configure(config, 11.0) ||
      !stalled.Submit(0.25, true, &stalledTicks) ||
      stalledTicks.size() != 4u)
    return Fail("bounded stall did not emit the configured maximum");
  const SSimulationCadenceTelemetry stalledTelemetry = stalled.Telemetry();
  if (stalledTelemetry.cappedSamples != 1u ||
      stalledTelemetry.maximumTicksPerSample != 4u ||
      std::fabs(stalledTelemetry.droppedSeconds - 0.15) > 1.0e-9)
    return Fail("bounded stall telemetry did not expose dropped time");

  SimulationCadence focus;
  std::vector<double> batch;
  if (!focus.Configure(config, 19.0) ||
      !focus.Submit(0.0125, true, &batch) || !batch.empty() ||
      !focus.Submit(0.1, false, &batch) || !batch.empty() ||
      !focus.Submit(0.025, true, &batch) || batch.size() != 1u)
    return Fail("focus reset produced a catch-up storm");
  const SSimulationCadenceTelemetry focusTelemetry = focus.Telemetry();
  if (focusTelemetry.focusResets != 1u ||
      focusTelemetry.simulationTicks != 1u ||
      focusTelemetry.maximumTicksPerSample != 1u ||
      std::fabs(focusTelemetry.droppedSeconds - 0.1125) > 1.0e-9)
    return Fail("focus reset telemetry diverged");

  std::cout << "simulation-cadence-smoke: OK dense="
            << denseTelemetry.presentationSamples << "/"
            << denseTelemetry.simulationTicks << " sparse="
            << sparseTelemetry.presentationSamples << "/"
            << sparseTelemetry.simulationTicks << " max="
            << sparseTelemetry.maximumTicksPerSample << " stall_drop="
            << stalledTelemetry.droppedSeconds << " focus_resets="
            << focusTelemetry.focusResets << '\n';
  return EXIT_SUCCESS;
}
