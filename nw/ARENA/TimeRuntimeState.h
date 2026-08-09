#ifndef RR2NW_TIME_RUNTIME_STATE_H
#define RR2NW_TIME_RUNTIME_STATE_H

#include <cstdint>

struct SSimulationClockState {
  std::uint64_t tick;
  double eventMoment;
  double viewTime;
  double frameSeconds;
  double timerAspect;
  unsigned int clampedSamples;
  double clampedSeconds;

  SSimulationClockState();
};

bool SUA_CaptureSimulationClock(SSimulationClockState* state);
bool SUA_ValidateSimulationClock(const SSimulationClockState& state);
bool SUA_ApplySimulationClock(const SSimulationClockState& state);
bool SUA_SimulationClockMatches(const SSimulationClockState& state);
bool SUA_ProcessEventsAt(double viewTime);

// GetTickCount is a modulo-2^32 millisecond source. Keep its subtraction
// unsigned and expose the exact production sampler so wrap behavior can be
// proved without waiting 49.7 days. Neither value is serialized.
std::uint32_t SUA_HostTickDelta(std::uint32_t currentTick,
                                std::uint32_t previousTick);
bool SUA_SampleLegacyTimerAtHostTick(std::uint32_t hostTick,
                                     double* timeSeconds);

// All admitted Windows input is stamped at the current authoritative event
// boundary. This keeps the retained Hardware mouse path on the same clock as
// the recovered keyboard/button adapter and makes host tick wrap irrelevant
// to gameplay ordering.
double SUA_AuthoritativeInputTime();

#endif
