#include "SimulationCadence.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

const long double kNanosecondsPerSecond = 1000000000.0L;
const unsigned int kMaximumSupportedCatchUpTicks = 64u;
const double kMaximumSupportedHardDeltaSeconds = 60.0;

bool SecondsToNanoseconds(double seconds, std::uint64_t* nanoseconds) {
  if (nanoseconds == nullptr || !std::isfinite(seconds) || seconds <= 0.0 ||
      seconds > kMaximumSupportedHardDeltaSeconds)
    return false;
  const long double scaled =
      static_cast<long double>(seconds) * kNanosecondsPerSecond;
  if (scaled < 1.0L ||
      scaled > static_cast<long double>(
          (std::numeric_limits<std::uint64_t>::max)()))
    return false;
  const std::uint64_t rounded =
      static_cast<std::uint64_t>(scaled + 0.5L);
  if (rounded == 0u) return false;
  *nanoseconds = rounded;
  return true;
}

double NanosecondsToSeconds(std::uint64_t nanoseconds) {
  return static_cast<double>(nanoseconds) / 1000000000.0;
}

}  // namespace

bool SimulationCadence::Configure(
    const SSimulationCadenceConfig& config,
    double simulationOriginSeconds) {
  std::uint64_t step = 0;
  std::uint64_t maximumFrame = 0;
  std::uint64_t hardLimit = 0;
  if (!std::isfinite(simulationOriginSeconds) ||
      simulationOriginSeconds < 0.0 ||
      config.maximumCatchUpTicks == 0u ||
      config.maximumCatchUpTicks > kMaximumSupportedCatchUpTicks ||
      !SecondsToNanoseconds(config.fixedStepSeconds, &step) ||
      !SecondsToNanoseconds(config.maximumFrameDeltaSeconds, &maximumFrame) ||
      !SecondsToNanoseconds(config.hardDeltaLimitSeconds, &hardLimit) ||
      maximumFrame < step || hardLimit < maximumFrame)
    return false;

  config_ = config;
  fixedStepNanoseconds_ = step;
  maximumFrameDeltaNanoseconds_ = maximumFrame;
  hardDeltaLimitNanoseconds_ = hardLimit;
  configured_ = true;
  return Reset(simulationOriginSeconds);
}

bool SimulationCadence::Reset(double simulationOriginSeconds) {
  if (!configured_ || !std::isfinite(simulationOriginSeconds) ||
      simulationOriginSeconds < 0.0)
    return false;
  simulationOriginSeconds_ = simulationOriginSeconds;
  accumulatorNanoseconds_ = 0u;
  simulationTicks_ = 0u;
  telemetry_ = {};
  return true;
}

bool SimulationCadence::Submit(double elapsedSeconds,
                               bool applicationFocused,
                               std::vector<double>* tickTimes) {
  if (!configured_ || tickTimes == nullptr) return false;

  std::uint64_t elapsedNanoseconds = 0u;
  if (!SecondsToNanoseconds(elapsedSeconds, &elapsedNanoseconds) ||
      elapsedNanoseconds > hardDeltaLimitNanoseconds_) {
    ++telemetry_.invalidSamples;
    return false;
  }

  std::vector<double> emitted;
  emitted.reserve(config_.maximumCatchUpTicks);
  SSimulationCadenceTelemetry nextTelemetry = telemetry_;
  std::uint64_t nextAccumulator = accumulatorNanoseconds_;
  std::uint64_t nextSimulationTicks = simulationTicks_;
  ++nextTelemetry.presentationSamples;

  if (!applicationFocused) {
    nextTelemetry.droppedSeconds += NanosecondsToSeconds(
        nextAccumulator + elapsedNanoseconds);
    nextAccumulator = 0u;
    ++nextTelemetry.focusResets;
    ++nextTelemetry.zeroTickSamples;
    nextTelemetry.accumulatorSeconds = 0.0;
    accumulatorNanoseconds_ = nextAccumulator;
    telemetry_ = nextTelemetry;
    tickTimes->swap(emitted);
    return true;
  }

  std::uint64_t admittedNanoseconds = elapsedNanoseconds;
  if (admittedNanoseconds > maximumFrameDeltaNanoseconds_) {
    nextTelemetry.droppedSeconds += NanosecondsToSeconds(
        admittedNanoseconds - maximumFrameDeltaNanoseconds_);
    admittedNanoseconds = maximumFrameDeltaNanoseconds_;
    ++nextTelemetry.cappedSamples;
  }
  nextAccumulator += admittedNanoseconds;

  const std::uint64_t availableTicks =
      nextAccumulator / fixedStepNanoseconds_;
  const std::uint64_t emittedTicks = (std::min)(
      availableTicks,
      static_cast<std::uint64_t>(config_.maximumCatchUpTicks));
  const std::uint64_t discardedTicks = availableTicks - emittedTicks;
  if (discardedTicks != 0u) {
    nextAccumulator -= discardedTicks * fixedStepNanoseconds_;
    nextTelemetry.droppedSeconds += NanosecondsToSeconds(
        discardedTicks * fixedStepNanoseconds_);
    ++nextTelemetry.catchUpLimitSamples;
  }

  if (emittedTicks >
          (std::numeric_limits<std::uint64_t>::max)() -
              nextSimulationTicks ||
      nextSimulationTicks + emittedTicks >
          (std::numeric_limits<std::uint64_t>::max)() /
              fixedStepNanoseconds_) {
    ++telemetry_.invalidSamples;
    return false;
  }

  nextAccumulator -= emittedTicks * fixedStepNanoseconds_;
  for (std::uint64_t index = 0; index < emittedTicks; ++index) {
    ++nextSimulationTicks;
    const double target = simulationOriginSeconds_ +
        NanosecondsToSeconds(nextSimulationTicks * fixedStepNanoseconds_);
    if (!std::isfinite(target)) {
      ++telemetry_.invalidSamples;
      return false;
    }
    emitted.push_back(target);
  }

  nextTelemetry.simulationTicks = nextSimulationTicks;
  if (emittedTicks == 0u) ++nextTelemetry.zeroTickSamples;
  if (emittedTicks > 1u) ++nextTelemetry.catchUpSamples;
  nextTelemetry.maximumTicksPerSample = (std::max)(
      nextTelemetry.maximumTicksPerSample,
      static_cast<unsigned int>(emittedTicks));
  nextTelemetry.accumulatorSeconds = NanosecondsToSeconds(nextAccumulator);
  accumulatorNanoseconds_ = nextAccumulator;
  simulationTicks_ = nextSimulationTicks;
  telemetry_ = nextTelemetry;
  tickTimes->swap(emitted);
  return true;
}

bool SimulationCadence::IsConfigured() const {
  return configured_;
}

const SSimulationCadenceConfig& SimulationCadence::Config() const {
  return config_;
}

SSimulationCadenceTelemetry SimulationCadence::Telemetry() const {
  return telemetry_;
}
