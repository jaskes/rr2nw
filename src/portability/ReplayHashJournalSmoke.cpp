#include "ReplayHashJournal.h"

#include "SimulationRandom.h"
#include "TimeRuntimeState.h"
#include "hardware.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
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

  // A process may remain alive beyond GetTickCount's 49.7-day wrap and then
  // continue for months. At that magnitude a double's ULP is wider than the
  // original absolute 1 ns validation tolerance even though the exact 25 ms
  // cadence and authoritative hashes are unchanged.
  const double hostWrapSeconds =
      static_cast<double>((std::numeric_limits<std::uint32_t>::max)()) /
      1000.0;
  const double longOrigin =
      hostWrapSeconds + 365.0 * 24.0 * 60.0 * 60.0;
  SSimulationClockState longCheckpoint = checkpoint;
  longCheckpoint.tick = 0x100000000ull + 1532u;
  longCheckpoint.eventMoment = longOrigin;
  longCheckpoint.viewTime = longOrigin;
  if (!SUA_ApplySimulationClock(longCheckpoint))
    return Fail("could not install the long-session clock checkpoint");
  SVehicleControlJournal longControls;
  if (!VehicleControlJournal_Begin(
          "Vehicle.Default", true, held, &longControls) ||
      !VehicleControlJournal_Seal(
          &longControls, longCheckpoint.tick + 12u,
          longOrigin + 12.0 * kStep))
    return Fail("could not build the long-session CTJ1 fixture");
  std::vector<SReplayHashSample> longSamples;
  for (std::uint64_t index = 1u; index <= 12u; ++index) {
    SReplayHashSample sample;
    sample.tick = longCheckpoint.tick + index;
    sample.simulationTime = longOrigin +
        static_cast<double>(index) * kStep;
    sample.stateHash = 0x6eed0e9da4d94a4full ^
                       (sample.tick * 0x100000001b3ull);
    longSamples.push_back(sample);
  }
  SReplayHashJournal longJournalA;
  SReplayHashJournal longJournalB;
  std::vector<std::uint8_t> longEncoded;
  SReplayHashJournal longDecoded;
  if (!ReplayHashJournal_Create(
          kContentFingerprint, kStep, longControls, longSamples,
          &longJournalA) ||
      !ReplayHashJournal_Create(
          kContentFingerprint, kStep, longControls, longSamples,
          &longJournalB) ||
      ReplayHashJournal_Fingerprint(longJournalA) == 0u ||
      ReplayHashJournal_Fingerprint(longJournalA) !=
          ReplayHashJournal_Fingerprint(longJournalB) ||
      !ReplayHashJournal_Encode(longJournalA, &longEncoded) ||
      !ReplayHashJournal_Decode(longEncoded, &longDecoded) ||
      ReplayHashJournal_Fingerprint(longJournalA) !=
          ReplayHashJournal_Fingerprint(longDecoded))
    return Fail("long-session RPH1 identity or round trip diverged");
  SReplayHashJournal longInvalid = longJournalA;
  longInvalid.samples[5].simulationTime += kStep;
  if (ReplayHashJournal_Validate(longInvalid))
    return Fail("long-session RPH1 admitted a whole-tick time drift");

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
