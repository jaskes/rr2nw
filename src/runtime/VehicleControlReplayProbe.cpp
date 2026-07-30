#include "VehicleControlReplayProbe.h"

#include "SimulationRandom.h"
#include "TimeRuntimeState.h"
#include "VehicleControlJournal.h"
#include "hardware.h"
#include "obase/vehicle/VehicleRuntimeState.h"

#include "kernel/h/context.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

const double kStep = 0.025;
const double kTolerance = 1.0e-7;
const std::uint64_t kHashOffset = 14695981039346656037ull;
const std::uint64_t kHashPrime = 1099511628211ull;

bool NearlyEqual(double left, double right) {
  return std::fabs(left - right) <= kTolerance;
}

bool NearlyEqual(const CFVector3& left, const CFVector3& right) {
  return NearlyEqual(left.x, right.x) &&
         NearlyEqual(left.y, right.y) &&
         NearlyEqual(left.z, right.z);
}

bool NearlyEqual(const CFMatrix3x4& left, const CFMatrix3x4& right) {
  return NearlyEqual(left.Row(0), right.Row(0)) &&
         NearlyEqual(left.Row(1), right.Row(1)) &&
         NearlyEqual(left.Row(2), right.Row(2)) &&
         NearlyEqual(left.Offset(), right.Offset());
}

bool StatesMatch(const SRecoveredVehicleRuntimeState& left,
                 const SRecoveredVehicleRuntimeState& right,
                 bool includeRuntimeCounters) {
  const bool publicState = left.object == right.object &&
      left.attribute == right.attribute &&
      NearlyEqual(left.position, right.position) &&
      NearlyEqual(left.subjectPosition, right.subjectPosition) &&
      NearlyEqual(left.speed, right.speed) &&
      NearlyEqual(left.direction, right.direction) &&
      NearlyEqual(left.mass, right.mass) &&
      NearlyEqual(left.lastTime, right.lastTime) &&
      left.vesselKind == right.vesselKind && left.active == right.active &&
      left.frameBegun == right.frameBegun;
  return publicState && (!includeRuntimeCounters ||
      (left.advanceCount == right.advanceCount &&
       left.controlEventCount == right.controlEventCount &&
       left.lastBumpFlags == right.lastBumpFlags &&
       left.touchingGround == right.touchingGround &&
       left.groundContactFrameCount == right.groundContactFrameCount &&
       left.staticCollisionFrameCount == right.staticCollisionFrameCount &&
       left.landCollisionFrameCount == right.landCollisionFrameCount &&
       left.dynamicCollisionFrameCount == right.dynamicCollisionFrameCount));
}

void Hash(std::uint64_t* hash, const void* value, std::size_t size) {
  const unsigned char* bytes = static_cast<const unsigned char*>(value);
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

void HashVector(std::uint64_t* hash, const CFVector3& value) {
  Hash(hash, &value.x, sizeof(value.x));
  Hash(hash, &value.y, sizeof(value.y));
  Hash(hash, &value.z, sizeof(value.z));
}

std::uint64_t StateFingerprint(
    const SRecoveredVehicleRuntimeState& state) {
  std::uint64_t hash = kHashOffset;
  HashVector(&hash, state.position);
  HashVector(&hash, state.subjectPosition);
  HashVector(&hash, state.speed);
  HashVector(&hash, state.direction.Row(0));
  HashVector(&hash, state.direction.Row(1));
  HashVector(&hash, state.direction.Row(2));
  HashVector(&hash, state.direction.Offset());
  Hash(&hash, &state.mass, sizeof(state.mass));
  Hash(&hash, &state.lastTime, sizeof(state.lastTime));
  Hash(&hash, &state.vesselKind, sizeof(state.vesselKind));
  Hash(&hash, &state.advanceCount, sizeof(state.advanceCount));
  Hash(&hash, &state.controlEventCount, sizeof(state.controlEventCount));
  Hash(&hash, &state.lastBumpFlags, sizeof(state.lastBumpFlags));
  Hash(&hash, &state.touchingGround, sizeof(state.touchingGround));
  Hash(&hash, &state.groundContactFrameCount,
       sizeof(state.groundContactFrameCount));
  Hash(&hash, &state.staticCollisionFrameCount,
       sizeof(state.staticCollisionFrameCount));
  Hash(&hash, &state.landCollisionFrameCount,
       sizeof(state.landCollisionFrameCount));
  Hash(&hash, &state.dynamicCollisionFrameCount,
       sizeof(state.dynamicCollisionFrameCount));
  return hash;
}

bool ApplyClock(std::uint64_t tick, double time) {
  SSimulationClockState clock;
  if (!SUA_CaptureSimulationClock(&clock)) return false;
  clock.tick = tick;
  clock.eventMoment = time;
  clock.viewTime = time;
  clock.frameSeconds = kStep;
  return SUA_ApplySimulationClock(clock);
}

bool AdvanceTo(SimulationContext* context, std::uint64_t targetTick,
               double targetTime, std::uint64_t* tick, double* time,
               int* frames) {
  if (context == nullptr || tick == nullptr || time == nullptr ||
      frames == nullptr || targetTick < *tick ||
      targetTime + kTolerance < *time)
    return false;
  while (*tick < targetTick) {
    const std::uint64_t ticksLeft = targetTick - *tick;
    const double timeLeft = targetTime - *time;
    if (ticksLeft == 0u || timeLeft <= 0.0) return false;
    const double step = timeLeft / static_cast<double>(ticksLeft);
    if (!std::isfinite(step) || step <= 0.0 ||
        step > 0.05 + kTolerance)
      return false;
    ++*tick;
    *time += step;
    if (!ApplyClock(*tick, *time) ||
        !VehicleRuntimeState_Advance(context, *time))
      return false;
    ++*frames;
  }
  return NearlyEqual(*time, targetTime) && ApplyClock(*tick, targetTime);
}

bool RecordAction(SimulationContext* context,
                  SVehicleControlJournal* journal,
                  std::uint64_t tick, double time, int action, double value,
                  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT]) {
  if (!VehicleRuntimeState_ApplyControlAt(
          context, action, value, time) ||
      !VehicleControlJournal_AppendAction(
          journal, tick, time, action, value))
    return false;
  const int heldIndex = VehicleControlJournal_HeldActionIndex(action);
  if (heldIndex >= 0) held[heldIndex] = value;
  return true;
}

bool RecordFocus(SimulationContext* context,
                 SVehicleControlJournal* journal,
                 std::uint64_t tick, double time, bool active,
                 bool* currentActive,
                 double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT],
                 int* syntheticReleases) {
  if (currentActive == nullptr || syntheticReleases == nullptr ||
      *currentActive == active)
    return false;
  if (!active) {
    for (std::size_t index = 0;
         index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index) {
      if (held[index] == 0.0) continue;
      if (!VehicleRuntimeState_ApplyControlAt(
              context, VehicleControlJournal_HeldAction(index), 0.0, time))
        return false;
      held[index] = 0.0;
      ++*syntheticReleases;
    }
  }
  *currentActive = active;
  return VehicleControlJournal_AppendFocus(
      journal, tick, time, active);
}

bool ReplayJournal(SimulationContext* context,
                   const SVehicleControlJournal& journal,
                   int* frames, int* syntheticReleases) {
  if (context == nullptr || frames == nullptr ||
      syntheticReleases == nullptr ||
      !VehicleControlJournal_Validate(journal) || !journal.sealed)
    return false;
  KR_ObjectID target = context->searchObject(journal.target.c_str());
  SRecoveredVehicleRuntimeState targetState = {};
  if (target.isNUL() || !VehicleRuntimeState_Inspect(
          context, target, &targetState) || !targetState.active)
    return false;
  bool active = journal.initialApplicationActive;
  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  for (std::size_t index = 0;
       index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index) {
    held[index] = journal.initialHeldActions[index];
    if (held[index] != 0.0 &&
        !VehicleRuntimeState_ApplyControlAt(
            context, VehicleControlJournal_HeldAction(index), held[index],
            journal.checkpointTime))
      return false;
  }
  std::uint64_t tick = journal.checkpointTick;
  double time = journal.checkpointTime;
  for (const SVehicleControlJournalRecord& record : journal.records) {
    if (!AdvanceTo(context, record.tick, record.eventTime,
                   &tick, &time, frames))
      return false;
    if (record.kind == VEHICLE_CONTROL_JOURNAL_ACTION) {
      if (!active || !VehicleRuntimeState_ApplyControlAt(
              context, record.action, record.value, record.eventTime))
        return false;
      const int heldIndex =
          VehicleControlJournal_HeldActionIndex(record.action);
      if (heldIndex >= 0) held[heldIndex] = record.value;
    } else {
      const bool nextActive = record.value != 0.0;
      if (active == nextActive) return false;
      if (!nextActive) {
        for (std::size_t index = 0;
             index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index) {
          if (held[index] == 0.0) continue;
          if (!VehicleRuntimeState_ApplyControlAt(
                  context, VehicleControlJournal_HeldAction(index), 0.0,
                  record.eventTime))
            return false;
          held[index] = 0.0;
          ++*syntheticReleases;
        }
      }
      active = nextActive;
    }
  }
  return AdvanceTo(context, journal.finalTick, journal.finalTime,
                   &tick, &time, frames);
}

}  // namespace

bool VehicleControlReplayProbe_Run(
    SimulationContext* context, const KR_ObjectID& vehicle,
    const CFVector3& position, double startTime,
    SRecoveredVehicleControlReplayProbeSummary* summary) {
  if (summary == nullptr) return false;
  *summary = {};
  KR_ObjectID vehicleCopy = vehicle;
  if (context == nullptr || vehicleCopy.isNUL() ||
      !context->isExist(vehicle) || !std::isfinite(startTime) ||
      startTime < 0.1 || !VehicleRuntimeState_IsClean(context))
    return false;

  SRecoveredVehicleRuntimeState worldBefore = {};
  SSimulationClockState clockBefore;
  std::vector<std::uint8_t> randomBefore;
  if (!VehicleRuntimeState_Inspect(context, vehicle, &worldBefore) ||
      !SUA_CaptureSimulationClock(&clockBefore) ||
      !SimulationRandom_Capture(&randomBefore))
    return false;

  SSimulationClockState checkpointClock = clockBefore;
  checkpointClock.tick = 1532u;
  checkpointClock.eventMoment = startTime;
  checkpointClock.viewTime = startTime;
  checkpointClock.frameSeconds = kStep;
  bool succeeded = SUA_ApplySimulationClock(checkpointClock);
  SimulationRandom_Reset(0x525232u);

  SVehicleControlJournal journal;
  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  bool applicationActive = true;
  bool firstActivated = false;
  int recordedFrames = 0;
  int recordedSyntheticReleases = 0;
  std::uint64_t tick = checkpointClock.tick;
  double time = startTime;
  SRecoveredVehicleRuntimeState recorded = {};
  SSimulationClockState recordedClock;
  std::vector<std::uint8_t> recordedRandom;

  if (succeeded) {
    firstActivated = VehicleRuntimeState_Activate(
        context, vehicle, position, startTime);
    succeeded = firstActivated;
  }
  if (succeeded)
    succeeded = VehicleControlJournal_Begin(
        "Vehicle.Default", applicationActive, held, &journal);
  if (succeeded)
    succeeded = RecordAction(context, &journal, tick, time,
                             MOVE_FORWARD, 1.0, held);
  if (succeeded)
    succeeded = AdvanceTo(context, tick + 8u, time + 8.0 * kStep,
                          &tick, &time, &recordedFrames);
  if (succeeded)
    succeeded = RecordAction(context, &journal, tick, time,
                             TURN_RIGHT, 1.0, held);
  if (succeeded)
    succeeded = AdvanceTo(context, tick + 8u, time + 8.0 * kStep,
                          &tick, &time, &recordedFrames);
  if (succeeded)
    succeeded = RecordAction(context, &journal, tick, time,
                             TURN_RIGHT, 0.0, held);
  if (succeeded)
    succeeded = AdvanceTo(context, tick + 8u, time + 8.0 * kStep,
                          &tick, &time, &recordedFrames);
  if (succeeded)
    succeeded = RecordFocus(context, &journal, tick, time, false,
                            &applicationActive, held,
                            &recordedSyntheticReleases);
  if (succeeded)
    succeeded = AdvanceTo(context, tick + 4u, time + 4.0 * kStep,
                          &tick, &time, &recordedFrames);
  if (succeeded)
    succeeded = RecordFocus(context, &journal, tick, time, true,
                            &applicationActive, held,
                            &recordedSyntheticReleases);
  if (succeeded)
    succeeded = VehicleControlJournal_Seal(&journal, tick, time) &&
                VehicleRuntimeState_Inspect(context, vehicle, &recorded) &&
                SUA_CaptureSimulationClock(&recordedClock) &&
                SimulationRandom_Capture(&recordedRandom);
  if (succeeded) summary->recordings = 1;

  const bool firstRollback = firstActivated &&
      VehicleRuntimeState_Rollback(context);
  if (firstRollback) ++summary->rollbacks;
  succeeded = succeeded && firstRollback &&
              VehicleRuntimeState_IsClean(context);

  std::vector<std::uint8_t> encoded;
  SVehicleControlJournal decoded;
  if (succeeded)
    succeeded = VehicleControlJournal_Encode(journal, &encoded) &&
                VehicleControlJournal_Decode(encoded, &decoded) &&
                VehicleControlJournal_Fingerprint(journal) != 0 &&
                VehicleControlJournal_Fingerprint(journal) ==
                    VehicleControlJournal_Fingerprint(decoded);
  if (succeeded) summary->codecRoundTrips = 1;

  bool replayActivated = false;
  int replayedFrames = 0;
  int replayedSyntheticReleases = 0;
  SRecoveredVehicleRuntimeState replayed = {};
  SSimulationClockState replayedClock;
  std::vector<std::uint8_t> replayedRandom;
  if (succeeded)
    succeeded = VehicleControlJournal_ApplyCheckpoint(decoded);
  if (succeeded) {
    replayActivated = VehicleRuntimeState_Activate(
        context, vehicle, position, decoded.checkpointTime);
    succeeded = replayActivated;
  }
  if (succeeded)
    succeeded = ReplayJournal(context, decoded, &replayedFrames,
                              &replayedSyntheticReleases) &&
                VehicleRuntimeState_Inspect(context, vehicle, &replayed) &&
                SUA_CaptureSimulationClock(&replayedClock) &&
                SimulationRandom_Capture(&replayedRandom);
  if (succeeded) summary->replays = 1;

  const bool stateMatch = succeeded && StatesMatch(recorded, replayed, true);
  const bool clockMatch = succeeded &&
      SUA_ValidateSimulationClock(recordedClock) &&
      recordedClock.tick == replayedClock.tick &&
      recordedClock.eventMoment == replayedClock.eventMoment &&
      recordedClock.viewTime == replayedClock.viewTime &&
      recordedClock.frameSeconds == replayedClock.frameSeconds &&
      recordedClock.timerAspect == replayedClock.timerAspect &&
      recordedClock.clampedSamples == replayedClock.clampedSamples &&
      recordedClock.clampedSeconds == replayedClock.clampedSeconds;
  const bool randomMatch = succeeded && recordedRandom == replayedRandom;
  if (stateMatch) summary->stateMatches = 1;
  if (clockMatch) summary->clockMatches = 1;
  if (randomMatch) summary->randomMatches = 1;

  const bool secondRollback = replayActivated &&
      VehicleRuntimeState_Rollback(context);
  if (secondRollback) ++summary->rollbacks;
  SRecoveredVehicleRuntimeState worldAfter = {};
  const bool worldRestored = secondRollback &&
      VehicleRuntimeState_IsClean(context) &&
      VehicleRuntimeState_Inspect(context, vehicle, &worldAfter) &&
      StatesMatch(worldBefore, worldAfter, false);
  const bool globalsRestored = SUA_ApplySimulationClock(clockBefore) &&
      SimulationRandom_Apply(SimulationRandom_Algorithm(), randomBefore) &&
      SUA_SimulationClockMatches(clockBefore) &&
      SimulationRandom_Matches(SimulationRandom_Algorithm(), randomBefore);

  SVehicleControlJournalStatistics statistics = {};
  const bool statsReady = VehicleControlJournal_Statistics(
      journal, &statistics);
  summary->actionRecords = static_cast<int>(statistics.actionRecords);
  summary->focusRecords = static_cast<int>(statistics.focusRecords);
  summary->syntheticReleases = recordedSyntheticReleases;
  summary->simulationFrames = recordedFrames;
  summary->encodedBytes = static_cast<unsigned int>(encoded.size());
  summary->journalFingerprint = VehicleControlJournal_Fingerprint(journal);
  summary->recordedStateFingerprint = StateFingerprint(recorded);
  summary->replayedStateFingerprint = StateFingerprint(replayed);

  const bool result = succeeded && stateMatch && clockMatch && randomMatch &&
         worldRestored && globalsRestored && statsReady &&
         recordedFrames == replayedFrames &&
         recordedSyntheticReleases == replayedSyntheticReleases &&
         statistics.actionRecords == 3u && statistics.focusRecords == 2u &&
         recordedSyntheticReleases == 1 && recordedFrames == 28 &&
         summary->recordedStateFingerprint != 0 &&
         summary->recordedStateFingerprint ==
             summary->replayedStateFingerprint &&
         summary->rollbacks == 2;
  if (!result) {
    std::fprintf(stderr,
        "vehicle-control-replay-probe: failed succeeded=%d state=%d "
        "clock=%d rng=%d world=%d globals=%d stats=%d "
        "frames=%d/%d releases=%d/%d records=%u/%u rollbacks=%d "
        "control_failure=%d frame_failure=%d\n",
        succeeded ? 1 : 0, stateMatch ? 1 : 0, clockMatch ? 1 : 0,
        randomMatch ? 1 : 0, worldRestored ? 1 : 0,
        globalsRestored ? 1 : 0, statsReady ? 1 : 0,
        recordedFrames, replayedFrames, recordedSyntheticReleases,
        replayedSyntheticReleases, statistics.actionRecords,
        statistics.focusRecords, summary->rollbacks,
        VehicleRuntimeState_LastControlFailure(),
        VehicleRuntimeState_LastFrameFailure());
  }
  return result;
}
