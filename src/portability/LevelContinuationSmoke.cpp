#include "ActiveWorldSave.h"
#include "LevelContinuation.h"
#include "SimulationRandom.h"
#include "TimeRuntimeState.h"
#include "hardware.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "level-continuation-smoke: %s\n", message);
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  SSimulationClockState before;
  std::vector<std::uint8_t> randomBefore;
  if (!SUA_CaptureSimulationClock(&before) ||
      !SimulationRandom_Capture(&randomBefore))
    return Fail("could not capture the process continuation state");

  SSimulationClockState boundary = before;
  boundary.tick = 4096u;
  boundary.eventMoment = 73.5;
  boundary.viewTime = 73.5;
  boundary.frameSeconds = 0.025;
  if (!SUA_ApplySimulationClock(boundary))
    return Fail("could not install the LCN1 boundary");
  SimulationRandom_Reset(0x4c434e31u);

  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  SVehicleControlJournal journal;
  if (!VehicleControlJournal_Begin(
          "Vehicle.Default", true, held, &journal) ||
      !VehicleControlJournal_AppendAction(
          &journal, boundary.tick, boundary.eventMoment,
          MOVE_FORWARD, 1.0) ||
      !VehicleControlJournal_AppendAction(
          &journal, boundary.tick, boundary.eventMoment,
          MOVE_FORWARD, 0.0) ||
      !VehicleControlJournal_Seal(
          &journal, boundary.tick, boundary.eventMoment))
    return Fail("could not build the sealed CTJ1 fixture");

  SActiveWorldSnapshot world;
  world.engineCompatibility = ActiveWorldSave_EngineCompatibilityVersion();
  world.contentFingerprint = 0x4c434e3146495854ull;
  world.simulationTick = boundary.tick;
  world.simulationTime = boundary.eventMoment;
  world.rngAlgorithm = SimulationRandom_Algorithm();
  if (!SimulationRandom_Capture(&world.rngState))
    return Fail("could not capture the simulation RNG");
  world.level = "Level.Fixture";
  SActiveWorldSection section = {};
  section.kind = EActiveWorldSectionKind::Commander;
  section.schemaVersion = 1u;
  section.owner = "Commander";
  section.payload.push_back(0x31u);
  world.sections.push_back(section);

  SActiveWorldSaveStatus status;
  SLevelContinuation continuation;
  if (!ActiveWorldSave_Encode(
          world, &continuation.activeWorld, &status))
    return Fail("could not encode the active-world fixture");
  continuation.controlJournal = journal;

  std::vector<std::uint8_t> encoded;
  SLevelContinuation decoded;
  if (!LevelContinuation_Encode(continuation, &encoded) || encoded.empty() ||
      !LevelContinuation_Decode(encoded, &decoded) ||
      decoded.fingerprint == 0 ||
      decoded.controlJournal.finalTick != boundary.tick ||
      decoded.controlJournal.finalTime != boundary.eventMoment ||
      decoded.activeWorld != continuation.activeWorld)
    return Fail("LCN1 canonical round trip diverged");

  const std::uint64_t decodedFingerprint = decoded.fingerprint;
  std::vector<std::uint8_t> corrupt = encoded;
  corrupt[corrupt.size() / 2u] ^= 0x40u;
  if (LevelContinuation_Decode(corrupt, &decoded) ||
      decoded.fingerprint != decodedFingerprint)
    return Fail("LCN1 corruption mutated the decode destination");
  corrupt = encoded;
  corrupt.pop_back();
  if (LevelContinuation_Decode(corrupt, &decoded) ||
      decoded.fingerprint != decodedFingerprint)
    return Fail("truncated LCN1 mutated the decode destination");

  bool active = false;
  double finalHeld[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  if (!VehicleControlJournal_DeriveLifecycle(
          decoded.controlJournal, &active, finalHeld) || !active ||
      finalHeld[VehicleControlJournal_HeldActionIndex(MOVE_FORWARD)] != 0.0 ||
      !VehicleControlJournal_Resume(&decoded.controlJournal) ||
      decoded.controlJournal.sealed ||
      !VehicleControlJournal_AppendAction(
          &decoded.controlJournal, boundary.tick + 1u,
          boundary.eventMoment + 0.025, TURN_RIGHT, 1.0))
    return Fail("sealed CTJ1 did not resume canonically");

  if (!SUA_ApplySimulationClock(before) ||
      !SimulationRandom_Apply(SimulationRandom_Algorithm(), randomBefore) ||
      !SUA_SimulationClockMatches(before) ||
      !SimulationRandom_Matches(SimulationRandom_Algorithm(), randomBefore))
    return Fail("could not restore the process continuation state");

  std::printf("level continuation codec=LCN1 world=AWV1 journal=CTJ1 "
              "boundary=%llu/%.3f bytes=%zu fingerprint=%llu\n",
              static_cast<unsigned long long>(boundary.tick),
              boundary.eventMoment, encoded.size(),
              static_cast<unsigned long long>(decodedFingerprint));
  return EXIT_SUCCESS;
}
