#include "VehicleControlReplayProbe.h"

#include "ActiveWorldReplayHash.h"
#include "ReplayHashJournal.h"
#include "SimulationCadence.h"
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
#include <limits>
#include <vector>

namespace {

const double kStep = 0.025;
const double kTolerance = 1.0e-7;
const std::uint64_t kHashOffset = 14695981039346656037ull;
const std::uint64_t kHashPrime = 1099511628211ull;

SSimulationCadenceConfig ReplayCadenceConfig() {
  SSimulationCadenceConfig config;
  config.fixedStepSeconds = kStep;
  config.maximumCatchUpTicks = 4u;
  config.maximumFrameDeltaSeconds = 0.1;
  config.hardDeltaLimitSeconds = 2.0;
  return config;
}

bool ProbeCadenceBoundaries(
    SRecoveredVehicleControlReplayProbeSummary* summary) {
  if (summary == nullptr) return false;
  const SSimulationCadenceConfig config = ReplayCadenceConfig();
  int checks = 0;

  SimulationCadence invalid;
  std::vector<double> sentinel(1u, 123.0);
  if (!invalid.Configure(config, 3.0) ||
      invalid.Submit(0.0, true, &sentinel) ||
      invalid.Submit((std::numeric_limits<double>::quiet_NaN)(), true,
                     &sentinel) ||
      sentinel.size() != 1u || sentinel[0] != 123.0 ||
      invalid.Telemetry().invalidSamples != 2u)
    return false;
  ++checks;

  SimulationCadence stalled;
  std::vector<double> ticks;
  if (!stalled.Configure(config, 7.0) ||
      !stalled.Submit(0.25, true, &ticks) || ticks.size() != 4u)
    return false;
  const SSimulationCadenceTelemetry stalledTelemetry = stalled.Telemetry();
  if (stalledTelemetry.cappedSamples != 1u ||
      stalledTelemetry.maximumTicksPerSample != 4u ||
      std::fabs(stalledTelemetry.droppedSeconds - 0.15) > kTolerance)
    return false;
  ++checks;

  SimulationCadence focus;
  if (!focus.Configure(config, 11.0) ||
      !focus.Submit(0.0125, true, &ticks) || !ticks.empty() ||
      !focus.Submit(0.1, false, &ticks) || !ticks.empty() ||
      !focus.Submit(kStep, true, &ticks) || ticks.size() != 1u)
    return false;
  const SSimulationCadenceTelemetry focusTelemetry = focus.Telemetry();
  if (focusTelemetry.focusResets != 1u ||
      focusTelemetry.simulationTicks != 1u ||
      focusTelemetry.maximumTicksPerSample != 1u ||
      std::fabs(focusTelemetry.droppedSeconds - 0.1125) > kTolerance)
    return false;
  ++checks;

  summary->cadenceBoundaryChecks = checks;
  summary->cadenceFocusResets =
      static_cast<int>(focusTelemetry.focusResets);
  summary->cadenceCappedSamples =
      static_cast<int>(stalledTelemetry.cappedSamples);
  summary->cadenceDroppedSeconds = stalledTelemetry.droppedSeconds;
  return true;
}

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
       left.dynamicCollisionFrameCount == right.dynamicCollisionFrameCount &&
       left.stabilityRecoveryCount == right.stabilityRecoveryCount &&
       left.lastStabilityReason == right.lastStabilityReason));
}

void Hash(std::uint64_t* hash, const void* value, std::size_t size) {
  const unsigned char* bytes = static_cast<const unsigned char*>(value);
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

void HashU32(std::uint64_t* hash, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    const unsigned char byte =
        static_cast<unsigned char>(value >> shift);
    Hash(hash, &byte, 1u);
  }
}

void HashU64(std::uint64_t* hash, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    const unsigned char byte =
        static_cast<unsigned char>(value >> shift);
    Hash(hash, &byte, 1u);
  }
}

void HashDouble(std::uint64_t* hash, double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  HashU64(hash, bits);
}

void HashVector(std::uint64_t* hash, const CFVector3& value) {
  HashDouble(hash, value.x);
  HashDouble(hash, value.y);
  HashDouble(hash, value.z);
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
  HashDouble(&hash, state.mass);
  HashDouble(&hash, state.damage);
  HashDouble(&hash, state.lastTime);
  HashU32(&hash, static_cast<std::uint32_t>(state.secondaryBulletCount));
  HashU32(&hash, static_cast<std::uint32_t>(state.vesselKind));
  HashU32(&hash, static_cast<std::uint32_t>(state.active));
  HashU32(&hash, static_cast<std::uint32_t>(state.frameBegun));
  HashU32(&hash, static_cast<std::uint32_t>(state.advanceCount));
  HashU32(&hash, static_cast<std::uint32_t>(state.controlEventCount));
  HashU32(&hash, static_cast<std::uint32_t>(state.lastBumpFlags));
  HashU32(&hash, static_cast<std::uint32_t>(state.touchingGround));
  HashU32(&hash, static_cast<std::uint32_t>(
                     state.groundContactFrameCount));
  HashU32(&hash, static_cast<std::uint32_t>(
                     state.staticCollisionFrameCount));
  HashU32(&hash, static_cast<std::uint32_t>(
                     state.landCollisionFrameCount));
  HashU32(&hash, static_cast<std::uint32_t>(
                     state.dynamicCollisionFrameCount));
  HashU32(&hash, static_cast<std::uint32_t>(
                     state.stabilityRecoveryCount));
  HashU32(&hash, static_cast<std::uint32_t>(state.lastStabilityReason));
  HashU32(&hash, static_cast<std::uint32_t>(state.dead));
  HashU32(&hash, static_cast<std::uint32_t>(state.takingTaxi));
  return hash;
}

bool AuthoritativeStateSnapshot(
    SimulationContext* context, const KR_ObjectID& vehicle,
    std::uint64_t contentFingerprint,
    SActiveWorldReplayHashSnapshot* snapshot) {
  KR_ObjectID vehicleCopy = vehicle;
  SRecoveredVehicleRuntimeState state = {};
  if (context == nullptr || vehicleCopy.isNUL() ||
      !VehicleRuntimeState_Inspect(context, vehicle, &state))
    return false;
  std::string failure;
  return ActiveWorldReplayHash_Capture(
      context, contentFingerprint, StateFingerprint(state), snapshot,
      &failure);
}

bool SamplesMatch(const std::vector<SReplayHashSample>& left,
                  const std::vector<SReplayHashSample>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index)
    if (left[index].tick != right[index].tick ||
        left[index].simulationTime != right[index].simulationTime ||
        left[index].stateHash != right[index].stateHash)
      return false;
  return true;
}

bool ActiveWorldSnapshotsMatch(
    const std::vector<SActiveWorldReplayHashSnapshot>& left,
    const std::vector<SActiveWorldReplayHashSnapshot>& right,
    std::uint32_t* mismatchComponent) {
  if (mismatchComponent == nullptr || left.size() != right.size())
  {
    if (mismatchComponent != nullptr)
      *mismatchComponent = ACTIVE_WORLD_HASH_PROFILE_OR_ROSTER;
    return false;
  }
  *mismatchComponent = 0u;
  for (std::size_t index = 0; index < left.size(); ++index) {
    std::uint32_t mismatch = 0u;
    if (!ActiveWorldReplayHash_FirstMismatch(
            left[index], right[index], &mismatch)) {
      *mismatchComponent = mismatch == 0u
          ? ACTIVE_WORLD_HASH_PROFILE_OR_ROSTER : mismatch;
      return false;
    }
    if (mismatch != 0u) {
      *mismatchComponent = mismatch;
      return false;
    }
  }
  return true;
}

std::uint64_t SampleStreamFingerprint(
    const std::vector<SReplayHashSample>& samples) {
  if (samples.empty()) return 0u;
  std::uint64_t hash = kHashOffset;
  HashU32(&hash, static_cast<std::uint32_t>(samples.size()));
  for (const SReplayHashSample& sample : samples) {
    HashU64(&hash, sample.tick);
    HashDouble(&hash, sample.simulationTime);
    HashU64(&hash, sample.stateHash);
  }
  return hash;
}

bool ClocksMatch(const SSimulationClockState& left,
                 const SSimulationClockState& right) {
  return SUA_ValidateSimulationClock(left) &&
         SUA_ValidateSimulationClock(right) && left.tick == right.tick &&
         left.eventMoment == right.eventMoment &&
         left.viewTime == right.viewTime &&
         left.frameSeconds == right.frameSeconds &&
         left.timerAspect == right.timerAspect &&
         left.clampedSamples == right.clampedSamples &&
         left.clampedSeconds == right.clampedSeconds;
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
               double targetTime, const KR_ObjectID& vehicle,
               std::uint64_t* tick, double* time, int* frames,
               std::vector<SReplayHashSample>* samples,
               std::vector<SActiveWorldReplayHashSnapshot>* worldHashes,
               std::uint64_t contentFingerprint,
               SimulationCadence* cadence, int presentationStride,
               int* presentationSamples) {
  KR_ObjectID vehicleCopy = vehicle;
  if (context == nullptr || tick == nullptr || time == nullptr ||
      frames == nullptr || samples == nullptr || worldHashes == nullptr ||
      contentFingerprint == 0u || vehicleCopy.isNUL() ||
      cadence == nullptr || !cadence->IsConfigured() ||
      presentationStride <= 0 || presentationSamples == nullptr ||
      targetTick < *tick ||
      targetTime + kTolerance < *time)
    return false;
  while (*tick < targetTick) {
    const std::uint64_t ticksLeft = targetTick - *tick;
    const std::uint64_t presentationTicks = (std::min)(
        ticksLeft, static_cast<std::uint64_t>(presentationStride));
    std::vector<double> tickTimes;
    if (presentationTicks == 0u ||
        !cadence->Submit(
            static_cast<double>(presentationTicks) * kStep, true,
            &tickTimes) || tickTimes.size() != presentationTicks)
      return false;
    ++*presentationSamples;
    for (double tickTime : tickTimes) {
      if (!NearlyEqual(tickTime, *time + kStep) ||
          tickTime > targetTime + kTolerance)
        return false;
      ++*tick;
      *time = tickTime;
      if (!ApplyClock(*tick, *time) ||
          !VehicleRuntimeState_Advance(context, *time))
        return false;
      ++*frames;
      SActiveWorldReplayHashSnapshot state;
      if (!AuthoritativeStateSnapshot(
              context, vehicle, contentFingerprint, &state))
        return false;
      SReplayHashSample sample;
      sample.tick = *tick;
      sample.simulationTime = *time;
      sample.stateHash = state.stateHash;
      samples->push_back(sample);
      worldHashes->push_back(state);
    }
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
                   std::uint64_t contentFingerprint,
                   int presentationStride,
                   std::vector<SReplayHashSample>* samples,
                   std::vector<SActiveWorldReplayHashSnapshot>* worldHashes,
                   int* frames, int* presentationSamples,
                   int* syntheticReleases,
                   SSimulationCadenceTelemetry* cadenceTelemetry) {
  if (context == nullptr || frames == nullptr ||
      samples == nullptr || worldHashes == nullptr ||
      contentFingerprint == 0u || presentationSamples == nullptr ||
      syntheticReleases == nullptr || cadenceTelemetry == nullptr ||
      presentationStride <= 0 ||
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
  SimulationCadence cadence;
  if (!cadence.Configure(ReplayCadenceConfig(), time)) return false;
  for (const SVehicleControlJournalRecord& record : journal.records) {
    if (!AdvanceTo(context, record.tick, record.eventTime,
                   target, &tick, &time, frames, samples,
                   worldHashes, contentFingerprint,
                   &cadence, presentationStride, presentationSamples))
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
  if (!AdvanceTo(context, journal.finalTick, journal.finalTime,
                 target, &tick, &time, frames, samples,
                 worldHashes, contentFingerprint,
                 &cadence, presentationStride, presentationSamples))
    return false;
  *cadenceTelemetry = cadence.Telemetry();
  return cadenceTelemetry->simulationTicks ==
             journal.finalTick - journal.checkpointTick &&
         cadenceTelemetry->presentationSamples ==
             static_cast<std::uint64_t>(*presentationSamples) &&
         cadenceTelemetry->accumulatorSeconds == 0.0 &&
         cadenceTelemetry->droppedSeconds == 0.0;
}

}  // namespace

bool VehicleControlReplayProbe_Run(
    SimulationContext* context, const KR_ObjectID& vehicle,
    const CFVector3& position, double startTime,
    unsigned long long contentFingerprint,
    SRecoveredVehicleControlReplayProbeSummary* summary) {
  if (summary == nullptr) return false;
  *summary = {};
  KR_ObjectID vehicleCopy = vehicle;
  const bool vehicleExists = context != nullptr &&
      !vehicleCopy.isNUL() && context->isExist(vehicle);
  const bool cleanRuntime = context != nullptr &&
      VehicleRuntimeState_IsClean(context);
  if (context == nullptr || vehicleCopy.isNUL() || !vehicleExists ||
      !std::isfinite(startTime) || startTime < 0.1 ||
      contentFingerprint == 0u || !cleanRuntime) {
    std::fprintf(stderr,
        "vehicle-control-replay-probe: precondition failed "
        "context=%d vehicle=%d exists=%d time=%g content=%llu clean=%d\n",
        context != nullptr ? 1 : 0, vehicleCopy.isNUL() ? 0 : 1,
        vehicleExists ? 1 : 0, startTime, contentFingerprint,
        cleanRuntime ? 1 : 0);
    return false;
  }

  SRecoveredVehicleRuntimeState worldBefore = {};
  SSimulationClockState clockBefore;
  std::vector<std::uint8_t> randomBefore;
  if (!VehicleRuntimeState_Inspect(context, vehicle, &worldBefore) ||
      !SUA_CaptureSimulationClock(&clockBefore) ||
      !SimulationRandom_Capture(&randomBefore)) {
    std::fprintf(stderr,
        "vehicle-control-replay-probe: checkpoint capture failed\n");
    return false;
  }

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
  int recordedPresentationSamples = 0;
  int recordedSyntheticReleases = 0;
  std::vector<SReplayHashSample> recordedSamples;
  std::vector<SActiveWorldReplayHashSnapshot> recordedWorldHashes;
  std::uint64_t tick = checkpointClock.tick;
  double time = startTime;
  SimulationCadence recordingCadence;
  SSimulationCadenceTelemetry recordingCadenceTelemetry = {};
  SRecoveredVehicleRuntimeState recorded = {};
  SSimulationClockState recordedClock;
  std::vector<std::uint8_t> recordedRandom;

  if (succeeded) {
    succeeded = recordingCadence.Configure(
        ReplayCadenceConfig(), checkpointClock.viewTime);
  }
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
                          vehicle, &tick, &time, &recordedFrames,
                          &recordedSamples, &recordedWorldHashes,
                          contentFingerprint, &recordingCadence, 1,
                          &recordedPresentationSamples);
  if (succeeded)
    succeeded = RecordAction(context, &journal, tick, time,
                             TURN_RIGHT, 1.0, held);
  if (succeeded)
    succeeded = AdvanceTo(context, tick + 8u, time + 8.0 * kStep,
                          vehicle, &tick, &time, &recordedFrames,
                          &recordedSamples, &recordedWorldHashes,
                          contentFingerprint, &recordingCadence, 1,
                          &recordedPresentationSamples);
  if (succeeded)
    succeeded = RecordAction(context, &journal, tick, time,
                             TURN_RIGHT, 0.0, held);
  if (succeeded)
    succeeded = AdvanceTo(context, tick + 8u, time + 8.0 * kStep,
                          vehicle, &tick, &time, &recordedFrames,
                          &recordedSamples, &recordedWorldHashes,
                          contentFingerprint, &recordingCadence, 1,
                          &recordedPresentationSamples);
  if (succeeded)
    succeeded = RecordFocus(context, &journal, tick, time, false,
                            &applicationActive, held,
                            &recordedSyntheticReleases);
  if (succeeded)
    succeeded = AdvanceTo(context, tick + 4u, time + 4.0 * kStep,
                          vehicle, &tick, &time, &recordedFrames,
                          &recordedSamples, &recordedWorldHashes,
                          contentFingerprint, &recordingCadence, 1,
                          &recordedPresentationSamples);
  if (succeeded)
    succeeded = RecordFocus(context, &journal, tick, time, true,
                            &applicationActive, held,
                            &recordedSyntheticReleases);
  if (succeeded) {
    recordingCadenceTelemetry = recordingCadence.Telemetry();
    succeeded = recordingCadenceTelemetry.simulationTicks == 28u &&
                recordingCadenceTelemetry.presentationSamples == 28u &&
                recordingCadenceTelemetry.maximumTicksPerSample == 1u &&
                recordingCadenceTelemetry.droppedSeconds == 0.0 &&
                VehicleControlJournal_Seal(&journal, tick, time) &&
                VehicleRuntimeState_Inspect(context, vehicle, &recorded) &&
                SUA_CaptureSimulationClock(&recordedClock) &&
                SimulationRandom_Capture(&recordedRandom);
  }
  if (succeeded) summary->recordings = 1;

  const bool firstRollback = firstActivated &&
      VehicleRuntimeState_Rollback(context);
  if (firstRollback) ++summary->rollbacks;
  succeeded = succeeded && firstRollback &&
              VehicleRuntimeState_IsClean(context);

  std::vector<std::uint8_t> encoded;
  SVehicleControlJournal decoded;
  SReplayHashJournal replayJournal;
  SReplayHashJournal decodedReplayJournal;
  std::vector<std::uint8_t> replayEncoded;
  if (succeeded)
    succeeded = VehicleControlJournal_Encode(journal, &encoded) &&
                VehicleControlJournal_Decode(encoded, &decoded) &&
                VehicleControlJournal_Fingerprint(journal) != 0 &&
                VehicleControlJournal_Fingerprint(journal) ==
                    VehicleControlJournal_Fingerprint(decoded) &&
                ReplayHashJournal_CreateWithAlgorithm(
                    contentFingerprint, kStep,
                    RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64,
                    decoded, recordedSamples, &replayJournal) &&
                ReplayHashJournal_Encode(replayJournal, &replayEncoded) &&
                ReplayHashJournal_Decode(replayEncoded,
                                         &decodedReplayJournal) &&
                ReplayHashJournal_MatchesIdentity(
                    decodedReplayJournal, contentFingerprint,
                    VehicleControlJournal_Fingerprint(decoded)) &&
                ReplayHashJournal_Fingerprint(replayJournal) != 0u &&
                ReplayHashJournal_Fingerprint(replayJournal) ==
                    ReplayHashJournal_Fingerprint(decodedReplayJournal);
  if (succeeded) summary->codecRoundTrips = 1;

  bool denseActivated = false;
  int denseFrames = 0;
  int densePresentationSamples = 0;
  int denseSyntheticReleases = 0;
  std::vector<SReplayHashSample> denseSamples;
  std::vector<SActiveWorldReplayHashSnapshot> denseWorldHashes;
  SRecoveredVehicleRuntimeState denseState = {};
  SSimulationClockState denseClock;
  std::vector<std::uint8_t> denseRandom;
  SSimulationCadenceTelemetry denseCadenceTelemetry = {};
  if (succeeded)
    succeeded = VehicleControlJournal_ApplyCheckpoint(
        decodedReplayJournal.controls);
  if (succeeded) {
    denseActivated = VehicleRuntimeState_Activate(
        context, vehicle, position,
        decodedReplayJournal.controls.checkpointTime);
    succeeded = denseActivated;
  }
  if (succeeded)
    succeeded = ReplayJournal(
                    context, decodedReplayJournal.controls,
                    contentFingerprint, 1, &denseSamples,
                    &denseWorldHashes, &denseFrames,
                    &densePresentationSamples,
                    &denseSyntheticReleases,
                    &denseCadenceTelemetry) &&
                VehicleRuntimeState_Inspect(
                    context, vehicle, &denseState) &&
                SUA_CaptureSimulationClock(&denseClock) &&
                SimulationRandom_Capture(&denseRandom);
  std::uint32_t denseMismatchComponent = 0u;
  const bool denseWorldHashMatch = succeeded &&
      ActiveWorldSnapshotsMatch(recordedWorldHashes, denseWorldHashes,
                                &denseMismatchComponent);
  const bool denseHashMatch = denseWorldHashMatch &&
      SamplesMatch(decodedReplayJournal.samples, denseSamples);
  const bool denseRollback = denseActivated &&
      VehicleRuntimeState_Rollback(context);
  if (denseRollback) ++summary->rollbacks;
  succeeded = succeeded && denseHashMatch && denseRollback &&
              VehicleRuntimeState_IsClean(context);

  bool sparseActivated = false;
  int sparseFrames = 0;
  int sparsePresentationSamples = 0;
  int sparseSyntheticReleases = 0;
  std::vector<SReplayHashSample> sparseSamples;
  std::vector<SActiveWorldReplayHashSnapshot> sparseWorldHashes;
  SRecoveredVehicleRuntimeState sparseState = {};
  SSimulationClockState sparseClock;
  std::vector<std::uint8_t> sparseRandom;
  SSimulationCadenceTelemetry sparseCadenceTelemetry = {};
  if (succeeded)
    succeeded = VehicleControlJournal_ApplyCheckpoint(
        decodedReplayJournal.controls);
  if (succeeded) {
    sparseActivated = VehicleRuntimeState_Activate(
        context, vehicle, position,
        decodedReplayJournal.controls.checkpointTime);
    succeeded = sparseActivated;
  }
  if (succeeded)
    succeeded = ReplayJournal(
                    context, decodedReplayJournal.controls,
                    contentFingerprint, 4, &sparseSamples,
                    &sparseWorldHashes, &sparseFrames,
                    &sparsePresentationSamples,
                    &sparseSyntheticReleases,
                    &sparseCadenceTelemetry) &&
                VehicleRuntimeState_Inspect(
                    context, vehicle, &sparseState) &&
                SUA_CaptureSimulationClock(&sparseClock) &&
                SimulationRandom_Capture(&sparseRandom);
  std::uint32_t sparseMismatchComponent = 0u;
  const bool sparseWorldHashMatch = succeeded &&
      ActiveWorldSnapshotsMatch(recordedWorldHashes, sparseWorldHashes,
                                &sparseMismatchComponent);
  const bool sparseHashMatch = sparseWorldHashMatch &&
      SamplesMatch(decodedReplayJournal.samples, sparseSamples) &&
      SamplesMatch(denseSamples, sparseSamples);
  const bool sparseRollback = sparseActivated &&
      VehicleRuntimeState_Rollback(context);
  if (sparseRollback) ++summary->rollbacks;
  if (succeeded) summary->replays = 2;

  const bool stateMatch = succeeded &&
      StatesMatch(recorded, denseState, true) &&
      StatesMatch(recorded, sparseState, true);
  const bool clockMatch = succeeded &&
      ClocksMatch(recordedClock, denseClock) &&
      ClocksMatch(recordedClock, sparseClock);
  const bool randomMatch = succeeded &&
      recordedRandom == denseRandom && recordedRandom == sparseRandom;
  if (stateMatch) summary->stateMatches = 1;
  if (clockMatch) summary->clockMatches = 1;
  if (randomMatch) summary->randomMatches = 1;
  summary->hashMatches = (denseHashMatch ? 1 : 0) +
                         (sparseHashMatch ? 1 : 0);
  summary->activeWorldHashMatches =
      (denseWorldHashMatch ? 1 : 0) + (sparseWorldHashMatch ? 1 : 0);
  summary->activeWorldComponents =
      static_cast<int>(kActiveWorldReplayHashComponentCount);
  summary->activeWorldOwnerComponents =
      static_cast<int>(kActiveWorldReplayHashOwnerComponentCount);
  summary->activeWorldEventCount = recordedWorldHashes.empty()
      ? 0 : static_cast<int>(recordedWorldHashes.back().eventCount);
  summary->presentationNormalizedComponents = 4;
  summary->stateHashAlgorithm =
      RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64;
  summary->mismatchComponent = denseMismatchComponent != 0u
      ? denseMismatchComponent : sparseMismatchComponent;

  SRecoveredVehicleRuntimeState worldAfter = {};
  const bool worldRestored = sparseRollback &&
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
  summary->hashSamples = static_cast<int>(recordedSamples.size());
  summary->densePresentationSamples = densePresentationSamples;
  summary->sparsePresentationSamples = sparsePresentationSamples;
  summary->denseSimulationTicks = denseFrames;
  summary->sparseSimulationTicks = sparseFrames;
  summary->denseMaximumTicksPerPresentation =
      static_cast<int>(denseCadenceTelemetry.maximumTicksPerSample);
  summary->sparseMaximumTicksPerPresentation =
      static_cast<int>(sparseCadenceTelemetry.maximumTicksPerSample);
  summary->sparseCatchUpSamples =
      static_cast<int>(sparseCadenceTelemetry.catchUpSamples);
  summary->encodedBytes = static_cast<unsigned int>(encoded.size());
  summary->replayEncodedBytes =
      static_cast<unsigned int>(replayEncoded.size());
  summary->contentFingerprint = contentFingerprint;
  summary->journalFingerprint = VehicleControlJournal_Fingerprint(journal);
  summary->replayFingerprint =
      ReplayHashJournal_Fingerprint(decodedReplayJournal);
  summary->hashStreamFingerprint =
      SampleStreamFingerprint(recordedSamples);
  summary->recordedStateFingerprint = StateFingerprint(recorded);
  summary->replayedStateFingerprint = StateFingerprint(sparseState);

  const bool cadenceBoundaries = ProbeCadenceBoundaries(summary);
  const bool result = succeeded && cadenceBoundaries && stateMatch &&
         clockMatch && randomMatch &&
         worldRestored && globalsRestored && statsReady &&
         recordedFrames == denseFrames && recordedFrames == sparseFrames &&
         recordedPresentationSamples == recordedFrames &&
         densePresentationSamples == denseFrames &&
         sparsePresentationSamples == sparseFrames / 4 &&
         denseCadenceTelemetry.maximumTicksPerSample == 1u &&
         denseCadenceTelemetry.catchUpSamples == 0u &&
         sparseCadenceTelemetry.maximumTicksPerSample == 4u &&
         sparseCadenceTelemetry.catchUpSamples == 7u &&
         summary->cadenceBoundaryChecks == 3 &&
         summary->cadenceFocusResets == 1 &&
         summary->cadenceCappedSamples == 1 &&
         std::fabs(summary->cadenceDroppedSeconds - 0.15) <= kTolerance &&
         recordedSyntheticReleases == denseSyntheticReleases &&
         recordedSyntheticReleases == sparseSyntheticReleases &&
         statistics.actionRecords == 3u && statistics.focusRecords == 2u &&
         recordedSyntheticReleases == 1 && recordedFrames == 28 &&
         summary->hashMatches == 2 && summary->hashSamples == 28 &&
         summary->activeWorldHashMatches == 2 &&
         summary->activeWorldComponents == 12 &&
         summary->activeWorldOwnerComponents == 7 &&
         summary->presentationNormalizedComponents == 4 &&
         summary->stateHashAlgorithm ==
             RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64 &&
         summary->mismatchComponent == 0u &&
         summary->replayEncodedBytes > summary->encodedBytes &&
         summary->contentFingerprint != 0u &&
         summary->replayFingerprint != 0u &&
         summary->hashStreamFingerprint != 0u &&
         summary->recordedStateFingerprint != 0 &&
         summary->recordedStateFingerprint ==
             summary->replayedStateFingerprint &&
         summary->rollbacks == 3;
  if (!result) {
    std::fprintf(stderr,
        "vehicle-control-replay-probe: failed succeeded=%d state=%d "
        "clock=%d rng=%d world=%d globals=%d stats=%d "
        "frames=%d/%d/%d present=%d/%d/%d scheduler=%d/%d/%d/%d "
        "hashes=%d/%d active=%d/%u/%s "
        "releases=%d/%d/%d records=%u/%u rollbacks=%d "
        "control_failure=%d frame_failure=%d\n",
        succeeded ? 1 : 0, stateMatch ? 1 : 0, clockMatch ? 1 : 0,
        randomMatch ? 1 : 0, worldRestored ? 1 : 0,
        globalsRestored ? 1 : 0, statsReady ? 1 : 0,
        recordedFrames, denseFrames, sparseFrames,
        recordedPresentationSamples, densePresentationSamples,
        sparsePresentationSamples,
        summary->denseMaximumTicksPerPresentation,
        summary->sparseMaximumTicksPerPresentation,
        summary->sparseCatchUpSamples, summary->cadenceBoundaryChecks,
        summary->hashMatches, summary->hashSamples,
        summary->activeWorldHashMatches, summary->mismatchComponent,
        ActiveWorldReplayHash_ComponentName(summary->mismatchComponent),
        recordedSyntheticReleases,
        denseSyntheticReleases, sparseSyntheticReleases,
        statistics.actionRecords, statistics.focusRecords,
        summary->rollbacks,
        VehicleRuntimeState_LastControlFailure(),
        VehicleRuntimeState_LastFrameFailure());
  }
  return result;
}
