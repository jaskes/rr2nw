#include "SimulationRandom.h"
#include "TimeRuntimeState.h"
#include "VehicleControlJournal.h"
#include "hardware.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "vehicle-control-journal-smoke: %s\n", message);
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
  checkpoint.frameSeconds = 0.025;
  if (!SUA_ApplySimulationClock(checkpoint))
    return Fail("could not install the deterministic clock checkpoint");
  SimulationRandom_Reset(0x525232u);

  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  SVehicleControlJournal journal;
  if (!VehicleControlJournal_Begin(
          "Vehicle.Default", true, held, &journal) ||
      !VehicleControlJournal_AppendAction(
          &journal, 1532u, 51.0, MOVE_FORWARD, 1.0) ||
      !VehicleControlJournal_AppendAction(
          &journal, 1534u, 51.05, TURN_RIGHT, 1.0) ||
      !VehicleControlJournal_AppendFocus(
          &journal, 1536u, 51.1, false) ||
      !VehicleControlJournal_AppendFocus(
          &journal, 1537u, 51.125, true) ||
      !VehicleControlJournal_Seal(&journal, 1538u, 51.15))
    return Fail("could not build the canonical CTJ1 fixture");

  SVehicleControlJournalStatistics statistics = {};
  std::vector<std::uint8_t> encoded;
  SVehicleControlJournal decoded;
  if (!VehicleControlJournal_Statistics(journal, &statistics) ||
      statistics.actionRecords != 2u || statistics.focusRecords != 2u ||
      statistics.firstTick != 1532u || statistics.lastTick != 1537u ||
      !VehicleControlJournal_Encode(journal, &encoded) ||
      encoded.empty() || !VehicleControlJournal_Decode(encoded, &decoded) ||
      VehicleControlJournal_Fingerprint(journal) == 0 ||
      VehicleControlJournal_Fingerprint(journal) !=
          VehicleControlJournal_Fingerprint(decoded))
    return Fail("the CTJ1 codec or statistics diverged");

  // Version 2 adds FIRE_SECONDARY to the held-action checkpoint. Keep old
  // CTJ1/version-1 files readable by proving the omitted slot is initialized
  // to neutral instead of shifting the remainder of the stream.
  std::vector<std::uint8_t> legacy = encoded;
  const std::size_t heldStart = 32u + journal.target.size();
  const std::size_t legacyOmittedSlot = heldStart + 11u * sizeof(double);
  legacy.erase(legacy.begin() + legacyOmittedSlot,
               legacy.begin() + legacyOmittedSlot + sizeof(double));
  legacy[4] = 1u;
  legacy[5] = legacy[6] = legacy[7] = 0u;
  SVehicleControlJournal legacyDecoded;
  if (!VehicleControlJournal_Decode(legacy, &legacyDecoded) ||
      legacyDecoded.initialHeldActions[11] != 0.0 ||
      legacyDecoded.records.size() != journal.records.size())
    return Fail("CTJ1/version-1 backward decoding diverged");

  SVehicleControlJournal destination = decoded;
  const std::uint64_t destinationFingerprint =
      VehicleControlJournal_Fingerprint(destination);
  std::vector<std::uint8_t> malformed = encoded;
  malformed.pop_back();
  if (VehicleControlJournal_Decode(malformed, &destination) ||
      VehicleControlJournal_Fingerprint(destination) !=
          destinationFingerprint)
    return Fail("truncated input mutated the decode destination");
  malformed = encoded;
  malformed[0] ^= 0xffu;
  if (VehicleControlJournal_Decode(malformed, &destination) ||
      VehicleControlJournal_Fingerprint(destination) !=
          destinationFingerprint)
    return Fail("bad CTJ1 magic mutated the decode destination");

  SVehicleControlJournal invalid = decoded;
  invalid.records[1].sequence = 9u;
  const std::size_t sealedRecordCount = journal.records.size();
  if (VehicleControlJournal_Validate(invalid) ||
      VehicleControlJournal_AppendAction(
          &journal, 1539u, 51.175, MOVE_FORWARD, 0.0) ||
      journal.records.size() != sealedRecordCount)
    return Fail("canonical ordering or sealed-journal rejection failed");

  SSimulationClockState mutated = checkpoint;
  mutated.tick = 9999u;
  mutated.eventMoment = 99.0;
  mutated.viewTime = 99.0;
  if (!SUA_ApplySimulationClock(mutated))
    return Fail("could not stage a mutated clock");
  SimulationRandom_Reset(7u);
  SimulationRandom_Next();
  if (!VehicleControlJournal_ApplyCheckpoint(decoded) ||
      !SUA_SimulationClockMatches(checkpoint) ||
      !SimulationRandom_Matches(decoded.randomAlgorithm,
                                decoded.randomCheckpoint))
    return Fail("the journal did not restore its clock/RNG checkpoint");

  if (!SUA_ApplySimulationClock(clockBefore) ||
      !SimulationRandom_Apply(SimulationRandom_Algorithm(), randomBefore) ||
      !SUA_SimulationClockMatches(clockBefore) ||
      !SimulationRandom_Matches(SimulationRandom_Algorithm(), randomBefore))
    return Fail("could not restore process-global continuation state");

  std::puts("vehicle-control-journal-smoke: ok");
  return EXIT_SUCCESS;
}
