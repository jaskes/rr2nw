#include "ReplayHashJournal.h"

#include "SimulationRandom.h"
#include "TimeRuntimeState.h"
#include "hardware.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

const std::uint64_t kContentFingerprint = 0x5252324e574d3401ull;
const double kStep = 0.025;

int Fail(const char* message) {
  std::fprintf(stderr, "replay-hash-journal-smoke: %s\n", message);
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  SSimulationClockState clockBefore;
  std::vector<std::uint8_t> randomBefore;
  if (!SUA_CaptureSimulationClock(&clockBefore) ||
      !SimulationRandom_Capture(&randomBefore))
    return Fail("could not capture process-global continuation state");

  SSimulationClockState checkpoint = clockBefore;
  checkpoint.tick = 1532u;
  checkpoint.eventMoment = 51.0;
  checkpoint.viewTime = 51.0;
  checkpoint.frameSeconds = kStep;
  if (!SUA_ApplySimulationClock(checkpoint))
    return Fail("could not install the deterministic clock checkpoint");
  SimulationRandom_Reset(0x525232u);

  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  SVehicleControlJournal controls;
  if (!VehicleControlJournal_Begin(
          "Vehicle.Default", true, held, &controls) ||
      !VehicleControlJournal_AppendAction(
          &controls, 1532u, 51.0, MOVE_FORWARD, 1.0) ||
      !VehicleControlJournal_AppendAction(
          &controls, 1536u, 51.1, TURN_RIGHT, 1.0) ||
      !VehicleControlJournal_AppendFocus(
          &controls, 1540u, 51.2, false) ||
      !VehicleControlJournal_Seal(&controls, 1544u, 51.3))
    return Fail("could not build the canonical CTJ1 fixture");

  std::vector<SReplayHashSample> samples;
  for (std::uint64_t tick = 1533u; tick <= 1544u; ++tick) {
    SReplayHashSample sample;
    sample.tick = tick;
    sample.simulationTime =
        51.0 + static_cast<double>(tick - 1532u) * kStep;
    sample.stateHash = 0x9e3779b97f4a7c15ull ^
                       (tick * 0x100000001b3ull);
    samples.push_back(sample);
  }

  SReplayHashJournal journal;
  std::vector<std::uint8_t> encoded;
  SReplayHashJournal decoded;
  const std::uint64_t controlFingerprint =
      VehicleControlJournal_Fingerprint(controls);
  if (!ReplayHashJournal_Create(
          kContentFingerprint, kStep, controls, samples, &journal) ||
      !ReplayHashJournal_Encode(journal, &encoded) || encoded.empty() ||
      !ReplayHashJournal_Decode(encoded, &decoded) ||
      ReplayHashJournal_Fingerprint(journal) == 0u ||
      ReplayHashJournal_Fingerprint(journal) !=
          ReplayHashJournal_Fingerprint(decoded) ||
      !ReplayHashJournal_MatchesIdentity(
          decoded, kContentFingerprint, controlFingerprint) ||
      ReplayHashJournal_MatchesIdentity(
          decoded, kContentFingerprint ^ 1u, controlFingerprint) ||
      ReplayHashJournal_MatchesIdentity(
          decoded, kContentFingerprint, controlFingerprint ^ 1u))
    return Fail("RPH1 round trip or identity validation diverged");

  SReplayHashJournal destination = decoded;
  const std::uint64_t destinationFingerprint =
      ReplayHashJournal_Fingerprint(destination);
  std::vector<std::uint8_t> malformed = encoded;
  malformed.pop_back();
  if (ReplayHashJournal_Decode(malformed, &destination) ||
      ReplayHashJournal_Fingerprint(destination) != destinationFingerprint)
    return Fail("truncated input mutated the decode destination");
  malformed = encoded;
  malformed[0] ^= 0xffu;
  if (ReplayHashJournal_Decode(malformed, &destination) ||
      ReplayHashJournal_Fingerprint(destination) != destinationFingerprint)
    return Fail("bad RPH1 magic mutated the decode destination");

  SReplayHashJournal invalid = decoded;
  invalid.samples[3].tick += 1u;
  if (ReplayHashJournal_Validate(invalid) ||
      ReplayHashJournal_Create(
          0u, kStep, controls, samples, &invalid))
    return Fail("non-contiguous ticks or empty content identity were admitted");
  invalid = decoded;
  invalid.samples[5].stateHash = 0u;
  if (ReplayHashJournal_Validate(invalid))
    return Fail("an empty authoritative state hash was admitted");
  invalid = decoded;
  invalid.samples[7].simulationTime += kStep;
  if (ReplayHashJournal_Validate(invalid))
    return Fail("a non-uniform simulation step was admitted");

  if (!SUA_ApplySimulationClock(clockBefore) ||
      !SimulationRandom_Apply(SimulationRandom_Algorithm(), randomBefore) ||
      !SUA_SimulationClockMatches(clockBefore) ||
      !SimulationRandom_Matches(SimulationRandom_Algorithm(), randomBefore))
    return Fail("could not restore process-global continuation state");

  std::printf("replay-hash-journal-smoke: ok bytes=%zu samples=%zu "
              "fingerprint=%llu\n",
              encoded.size(), samples.size(),
              static_cast<unsigned long long>(
                  ReplayHashJournal_Fingerprint(decoded)));
  return EXIT_SUCCESS;
}
