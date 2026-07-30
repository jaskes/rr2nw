#include "SimulationRandom.h"
#include "TimeRuntimeState.h"

#include "kernel/h/context.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "simulation-continuation-smoke: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  std::vector<std::uint8_t> originalRandom;
  if (!SimulationRandom_Capture(&originalRandom))
    return Fail("could not capture the initial simulation RNG");

  SimulationRandom_Reset(1u);
  if (SimulationRandom_Next() != 41 ||
      SimulationRandom_Next() != 18467 ||
      SimulationRandom_Next() != 6334 ||
      SimulationRandom_DrawCount() != 3u)
    return Fail("the explicit MSVC LCG sequence diverged");

  std::vector<std::uint8_t> checkpoint;
  if (!SimulationRandom_Capture(&checkpoint) || checkpoint.size() != 12u)
    return Fail("the simulation RNG checkpoint is not canonical");
  const int continued = SimulationRandom_Next();
  if (!SimulationRandom_Apply(SimulationRandom_Algorithm(), checkpoint) ||
      !SimulationRandom_Matches(SimulationRandom_Algorithm(), checkpoint) ||
      SimulationRandom_Next() != continued ||
      SimulationRandom_DrawCount() != 4u)
    return Fail("the simulation RNG did not continue from its checkpoint");

  std::vector<std::uint8_t> malformed = checkpoint;
  malformed.pop_back();
  std::vector<std::uint8_t> beforeRejectedApply;
  if (!SimulationRandom_Capture(&beforeRejectedApply) ||
      SimulationRandom_Apply(SimulationRandom_Algorithm(), malformed) ||
      SimulationRandom_Apply(0xffffffffu, checkpoint) ||
      !SimulationRandom_Matches(SimulationRandom_Algorithm(),
                                beforeRejectedApply))
    return Fail("a malformed RNG checkpoint mutated live state");

  SimulationRandom_Reset(1u);
  SimulationContext context(1, 1);
  if (context.rnd_i() != 41 || context.rnd_i(100) != 67 ||
      std::fabs(context.rnd_f() - 6334.0 / 32767.0) > 1.0e-12 ||
      SimulationRandom_DrawCount() != 3u)
    return Fail("SimulationContext did not consume the authoritative stream");

  SSimulationClockState originalClock;
  if (!SUA_CaptureSimulationClock(&originalClock))
    return Fail("could not capture the initial simulation clock");

  SSimulationClockState checkpointClock;
  checkpointClock.tick = 1532u;
  checkpointClock.eventMoment = 51.0;
  checkpointClock.viewTime = 51.025;
  checkpointClock.frameSeconds = 0.025;
  checkpointClock.timerAspect = 1.0;
  checkpointClock.clampedSamples = 2u;
  checkpointClock.clampedSeconds = 0.5;
  if (!SUA_ApplySimulationClock(checkpointClock) ||
      !SUA_SimulationClockMatches(checkpointClock))
    return Fail("the authoritative simulation clock did not round-trip");

  SSimulationClockState invalidClock = checkpointClock;
  invalidClock.frameSeconds = 0.2;
  if (SUA_ApplySimulationClock(invalidClock) ||
      !SUA_SimulationClockMatches(checkpointClock))
    return Fail("an invalid clock checkpoint mutated live state");

  if (!SUA_ApplySimulationClock(originalClock) ||
      !SimulationRandom_Apply(SimulationRandom_Algorithm(), originalRandom))
    return Fail("could not restore process-global continuation state");

  std::puts("simulation-continuation-smoke: ok");
  return 0;
}
