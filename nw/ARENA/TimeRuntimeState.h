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

#endif
