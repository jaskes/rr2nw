#pragma once

#include <cstdint>
#include <vector>

// Deterministic simulation cadence independent from renderer and host timer
// ownership.  A caller supplies elapsed presentation time; the cadence emits
// bounded authoritative tick boundaries.  The configured step is an explicit
// compatibility policy, not evidence of the original retail frame rate.
struct SSimulationCadenceConfig {
  double fixedStepSeconds = 0.0;
  unsigned int maximumCatchUpTicks = 0;
  double maximumFrameDeltaSeconds = 0.0;
  double hardDeltaLimitSeconds = 0.0;
};

struct SSimulationCadenceTelemetry {
  std::uint64_t presentationSamples = 0;
  std::uint64_t simulationTicks = 0;
  std::uint64_t zeroTickSamples = 0;
  std::uint64_t catchUpSamples = 0;
  std::uint64_t catchUpLimitSamples = 0;
  std::uint64_t cappedSamples = 0;
  std::uint64_t invalidSamples = 0;
  std::uint64_t focusResets = 0;
  unsigned int maximumTicksPerSample = 0;
  double accumulatorSeconds = 0.0;
  double droppedSeconds = 0.0;
};

class SimulationCadence {
 public:
  bool Configure(const SSimulationCadenceConfig& config,
                 double simulationOriginSeconds);
  bool Reset(double simulationOriginSeconds);

  // On success tickTimes is replaced by zero or more monotonically increasing
  // fixed-step boundaries.  Invalid samples return false without changing the
  // authoritative accumulator/tick state or the caller's output; only the
  // diagnostic invalidSamples counter advances.
  bool Submit(double elapsedSeconds, bool applicationFocused,
              std::vector<double>* tickTimes);

  bool IsConfigured() const;
  const SSimulationCadenceConfig& Config() const;
  SSimulationCadenceTelemetry Telemetry() const;

 private:
  SSimulationCadenceConfig config_ = {};
  SSimulationCadenceTelemetry telemetry_ = {};
  std::uint64_t fixedStepNanoseconds_ = 0;
  std::uint64_t maximumFrameDeltaNanoseconds_ = 0;
  std::uint64_t hardDeltaLimitNanoseconds_ = 0;
  std::uint64_t accumulatorNanoseconds_ = 0;
  std::uint64_t simulationTicks_ = 0;
  double simulationOriginSeconds_ = 0.0;
  bool configured_ = false;
};
