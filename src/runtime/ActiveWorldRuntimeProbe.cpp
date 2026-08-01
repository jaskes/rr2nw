#include "ActiveWorldRuntimeProbe.h"

#include "ActiveWorldSave.h"
#include "ActiveWorldSemanticEvents.h"
#include "ClockActiveWorldState.h"
#include "MissionActiveWorldState.h"
#include "RecoveredModRuntime.h"
#include "SimulationRandom.h"
#include "TimeRuntimeState.h"

#include "kernel/h/context.h"
#include "obase/comander/CommanderState.h"
#include "obase/bullet/BulletActiveWorldState.h"
#include "obase/corpse/CorpseActiveWorldState.h"
#include "obase/explosion/ExplosionActiveWorldState.h"
#include "obase/group/TankGroupState.h"
#include "obase/people/PeopleActiveWorldState.h"
#include "obase/spark/SparkActiveWorldState.h"
#include "obase/smoke/SmokeActiveWorldState.h"
#include "obase/tank/TankActiveWorldState.h"
#include "obase/vehicle/VehicleActiveWorldState.h"
#include "message/recrcenmsg.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace {

void SetFailure(std::string* failure, const std::string& message) {
  if (failure != nullptr) *failure = message;
}

bool CaptureOwnerSections(SimulationContext* context,
                          std::vector<SActiveWorldSection>* sections,
                          std::string* failure) {
  if (context == nullptr || sections == nullptr) return false;
  SActiveWorldSection commanders = {};
  commanders.kind = EActiveWorldSectionKind::Commander;
  commanders.schemaVersion = 1;
  commanders.owner = "Commander";
  if (!CommanderState_CaptureStable(context, &commanders.payload)) {
    SetFailure(failure, "Commander stable capture failed");
    return false;
  }

  SActiveWorldSection groups = {};
  groups.kind = EActiveWorldSectionKind::TankGroup;
  groups.schemaVersion = 1;
  groups.owner = "TankGroup";
  if (!TankGroupState_CaptureStable(context, &groups.payload)) {
    SetFailure(failure, "TankGroup stable capture failed");
    return false;
  }

  sections->clear();
  sections->push_back(std::move(commanders));
  sections->push_back(std::move(groups));

  SActiveWorldSection people = {};
  people.kind = EActiveWorldSectionKind::People;
  people.schemaVersion = 1;
  people.owner = "People";
  if (!PeopleActiveWorldState_CaptureStable(context, &people.payload)) {
    const char* detail = PeopleActiveWorldState_LastFailure();
    SetFailure(failure,
               detail != nullptr && detail[0] != '\0'
                   ? std::string("People stable capture failed: ") + detail
                   : "People stable capture failed");
    return false;
  }
  sections->push_back(std::move(people));

  SActiveWorldSection tanks = {};
  tanks.kind = EActiveWorldSectionKind::Tank;
  tanks.schemaVersion = 1;
  tanks.owner = "Tank";
  if (!TankActiveWorldState_CaptureStable(context, &tanks.payload)) {
    SetFailure(failure, TankActiveWorldState_LastFailure());
    return false;
  }
  sections->push_back(std::move(tanks));

  SActiveWorldSection vehicle = {};
  vehicle.kind = EActiveWorldSectionKind::Vehicle;
  vehicle.schemaVersion = 1;
  vehicle.owner = "Vehicle";
  if (!VehicleActiveWorldState_CaptureStable(context, &vehicle.payload)) {
    SetFailure(failure, "Vehicle stable capture failed");
    return false;
  }
  sections->push_back(std::move(vehicle));

  SActiveWorldSection mission = {};
  mission.kind = EActiveWorldSectionKind::Mission;
  mission.schemaVersion = 1;
  mission.owner = "Mission";
  if (!MissionActiveWorldState_CaptureStable(context, &mission.payload)) {
    SetFailure(failure, MissionActiveWorldState_LastFailure());
    return false;
  }
  sections->push_back(std::move(mission));

  SActiveWorldSection bullets = {};
  bullets.kind = EActiveWorldSectionKind::Bullet;
  bullets.schemaVersion = 1;
  bullets.owner = "Bullet";
  if (!BulletActiveWorldState_CaptureStable(context, &bullets.payload)) {
    SetFailure(failure, BulletActiveWorldState_LastFailure());
    return false;
  }
  sections->push_back(std::move(bullets));

  SActiveWorldSection explosions = {};
  explosions.kind = EActiveWorldSectionKind::Explosion;
  explosions.schemaVersion = 1;
  explosions.owner = "Explosion";
  if (!ExplosionActiveWorldState_CaptureStable(context,
                                                &explosions.payload)) {
    SetFailure(failure, ExplosionActiveWorldState_LastFailure());
    return false;
  }
  sections->push_back(std::move(explosions));

  SActiveWorldSection sparks = {};
  sparks.kind = EActiveWorldSectionKind::Spark;
  sparks.schemaVersion = 1;
  sparks.owner = "Spark";
  if (!SparkActiveWorldState_CaptureStable(context, &sparks.payload)) {
    SetFailure(failure, SparkActiveWorldState_LastFailure());
    return false;
  }
  sections->push_back(std::move(sparks));

  SActiveWorldSection smokes = {};
  smokes.kind = EActiveWorldSectionKind::Smoke;
  smokes.schemaVersion = 1;
  smokes.owner = "Smoke";
  if (!SmokeActiveWorldState_CaptureStable(context, &smokes.payload)) {
    SetFailure(failure, SmokeActiveWorldState_LastFailure());
    return false;
  }
  sections->push_back(std::move(smokes));

  SActiveWorldSection corpses = {};
  corpses.kind = EActiveWorldSectionKind::Corpse;
  corpses.schemaVersion = 1;
  corpses.owner = "Corpse";
  if (!CorpseActiveWorldState_CaptureStable(context, &corpses.payload)) {
    SetFailure(failure, CorpseActiveWorldState_LastFailure());
    return false;
  }
  sections->push_back(std::move(corpses));

  SActiveWorldSection clock = {};
  clock.kind = EActiveWorldSectionKind::Clock;
  clock.schemaVersion = 1;
  clock.owner = "Clock";
  if (!ClockActiveWorldState_CaptureStable(&clock.payload)) {
    SetFailure(failure, "Clock stable capture failed");
    return false;
  }
  sections->push_back(std::move(clock));

  return true;
}

bool ValidateOwnerCodec(const SActiveWorldSection& section) {
  if (section.schemaVersion != 1) return false;
  switch (section.kind) {
    case EActiveWorldSectionKind::Commander:
      return section.owner == "Commander" &&
             CommanderState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::TankGroup:
      return section.owner == "TankGroup" &&
             TankGroupState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Vehicle:
      return section.owner == "Vehicle" &&
             VehicleActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Mission:
      return section.owner == "Mission" &&
             MissionActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::People:
      return section.owner == "People" &&
             PeopleActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Tank:
      return section.owner == "Tank" &&
             TankActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Bullet:
      return section.owner == "Bullet" &&
             BulletActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Explosion:
      return section.owner == "Explosion" &&
             ExplosionActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Spark:
      return section.owner == "Spark" &&
             SparkActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Smoke:
      return section.owner == "Smoke" &&
             SmokeActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Corpse:
      return section.owner == "Corpse" &&
             CorpseActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Clock:
      return section.owner == "Clock" &&
             ClockActiveWorldState_ValidateStable(section.payload);
    default:
      return false;
  }
}

bool OwnerMatchesWorld(SimulationContext* context,
                       const SActiveWorldSection& section) {
  switch (section.kind) {
    case EActiveWorldSectionKind::Commander:
      return CommanderState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::TankGroup:
      return TankGroupState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::Vehicle:
      return VehicleActiveWorldState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::Mission:
      return MissionActiveWorldState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::People:
      return PeopleActiveWorldState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::Tank:
      return TankActiveWorldState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::Bullet:
      return BulletActiveWorldState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::Explosion:
      return ExplosionActiveWorldState_MatchesStable(context,
                                                       section.payload);
    case EActiveWorldSectionKind::Spark:
      return SparkActiveWorldState_MatchesStable(context,
                                                  section.payload);
    case EActiveWorldSectionKind::Smoke:
      return SmokeActiveWorldState_MatchesStable(context,
                                                   section.payload);
    case EActiveWorldSectionKind::Corpse:
      return CorpseActiveWorldState_MatchesStable(context,
                                                   section.payload);
    case EActiveWorldSectionKind::Clock:
      return ClockActiveWorldState_MatchesStable(section.payload);
    default:
      return false;
  }
}

class RuntimeRestoreTarget final : public IActiveWorldRestoreTarget {
 public:
  RuntimeRestoreTarget(SimulationContext* context, bool rejectValidation)
      : context_(context), snapshot_(nullptr), rejectValidation_(rejectValidation),
        began_(false), committed_(false), rolledBack_(false), ownerPhases_(0),
        referencePhases_(0), eventPhases_(0), rollbackClean_(false),
        replacedTransientOwners_(false) {}

  bool Begin(const SActiveWorldSnapshot& snapshot,
             std::string* failure) override {
    if (context_ == nullptr || began_) {
      SetFailure(failure, "active-world transaction cannot begin");
      return false;
    }
    snapshot_ = &snapshot;
    began_ = true;
    staged_.clear();
    commanderBackup_.clear();
    tankGroupBackup_.clear();
    vehicleBackup_.clear();
    missionBackup_.clear();
    peopleBackup_.clear();
    tankBackup_.clear();
    bulletBackup_.clear();
    explosionBackup_.clear();
    sparkBackup_.clear();
    smokeBackup_.clear();
    corpseBackup_.clear();
    clockBackup_.clear();
    rngBackup_.clear();
    semanticBackup_.clear();
    stagedEvents_.clear();
    createdCommanders_.clear();
    createdTankGroups_.clear();
    createdVehicles_.clear();
    createdMissionRoutes_.clear();
    createdPeople_.clear();
    createdTanks_.clear();
    createdBullets_.clear();
    createdExplosions_.clear();
    createdSparks_.clear();
    createdSmokes_.clear();
    createdCorpses_.clear();
    createdClocks_.clear();
    createdSemanticOwners_.clear();
    replacedTransientOwners_ = false;
    for (const SActiveWorldSection& section : snapshot.sections)
      if (!ValidateOwnerCodec(section)) {
        SetFailure(failure,
                   "active-world target owner codec rejected a section");
        began_ = false;
        return false;
      }
    if (!CommanderState_CaptureStable(context_, &commanderBackup_) ||
        !TankGroupState_CaptureStable(context_, &tankGroupBackup_) ||
        !VehicleActiveWorldState_CaptureStable(context_, &vehicleBackup_) ||
        !MissionActiveWorldState_CaptureStable(context_, &missionBackup_) ||
        !PeopleActiveWorldState_CaptureStable(context_, &peopleBackup_) ||
        !TankActiveWorldState_CaptureStable(context_, &tankBackup_) ||
        !BulletActiveWorldState_CaptureStable(context_, &bulletBackup_) ||
        !ExplosionActiveWorldState_CaptureStable(context_,
                                                  &explosionBackup_) ||
        !SparkActiveWorldState_CaptureStable(context_, &sparkBackup_) ||
        !SmokeActiveWorldState_CaptureStable(context_, &smokeBackup_) ||
        !CorpseActiveWorldState_CaptureStable(context_, &corpseBackup_) ||
        !ClockActiveWorldState_CaptureStable(&clockBackup_) ||
        !SimulationRandom_Capture(&rngBackup_) ||
        !ActiveWorldSemanticEvents_Capture(
            context_, &semanticBackup_, failure)) {
      SetFailure(failure, "active-world live owner backup failed");
      began_ = false;
      return false;
    }
    std::vector<KR_ObjectID> liveBullets;
    std::vector<KR_ObjectID> liveExplosions;
    std::vector<KR_ObjectID> liveSparks;
    std::vector<KR_ObjectID> liveSmokes;
    std::vector<KR_ObjectID> liveCorpses;
    if (!BulletActiveWorldState_CollectStableOwners(
            context_, bulletBackup_, &liveBullets) ||
        !ExplosionActiveWorldState_CollectStableOwners(
            context_, explosionBackup_, &liveExplosions) ||
        !SparkActiveWorldState_CollectStableOwners(
            context_, sparkBackup_, &liveSparks) ||
        !SmokeActiveWorldState_CollectStableOwners(
            context_, smokeBackup_, &liveSmokes) ||
        !CorpseActiveWorldState_CollectStableOwners(
            context_, corpseBackup_, &liveCorpses)) {
      SetFailure(failure,
                 "active-world transient owner teardown preflight failed");
      began_ = false;
      return false;
    }
    const SActiveWorldSection* targetClock = nullptr;
    for (const SActiveWorldSection& section : snapshot.sections)
      if (section.kind == EActiveWorldSectionKind::Clock) {
        targetClock = &section;
        break;
      }
    if (targetClock == nullptr || !ValidateOwnerCodec(*targetClock) ||
        !ClockActiveWorldState_MetadataMatches(
            targetClock->payload, snapshot.simulationTick,
            snapshot.simulationTime)) {
      SetFailure(failure, "active-world target clock is invalid");
      began_ = false;
      return false;
    }
    // Owner reconstruction may temporarily invalidate a queued mission index.
    // Remove semantic events while their original owner graph is still intact;
    // Commit rebuilds the target queue, rollback rebuilds this exact backup.
    if (!ActiveWorldSemanticEvents_Clear(context_, failure)) {
      SetFailure(failure, "active-world live event detachment failed");
      began_ = false;
      return false;
    }
    // Owner state contains absolute scheduler timestamps. Put the transaction
    // on the saved continuation boundary before any owner is reconstructed;
    // the canonical Clock section later proves and reapplies the same state.
    if (!ClockActiveWorldState_ApplyStableReferences(targetClock->payload) ||
        !ClockActiveWorldState_MatchesStable(targetClock->payload)) {
      std::vector<KR_ObjectID> restoredSemanticOwners;
      const bool clockRestored = ClockActiveWorldState_ApplyStableReferences(
          clockBackup_);
      const bool eventsRestored = ActiveWorldSemanticEvents_Replace(
          context_, semanticBackup_, &restoredSemanticOwners, nullptr);
      restoredSemanticOwners.clear();
      SetFailure(failure,
                 clockRestored && eventsRestored
                     ? "active-world target clock preapply failed"
                     : "active-world target clock preapply rollback failed");
      began_ = false;
      return false;
    }
    // Effect rosters are expected to differ between the save point and the
    // load point. Their codecs already own complete reconstruction and private
    // event teardown, so replace them after the live backup and clock preapply
    // instead of requiring coincidentally identical object names. Rollback
    // reconstructs these exact backup payloads before applying references.
    replacedTransientOwners_ = true;
    CorpseActiveWorldState_RemoveStableOwners(context_, &liveCorpses);
    SmokeActiveWorldState_RemoveStableOwners(context_, &liveSmokes);
    SparkActiveWorldState_RemoveStableOwners(context_, &liveSparks);
    ExplosionActiveWorldState_RemoveStableOwners(
        context_, &liveExplosions);
    BulletActiveWorldState_RemoveStableOwners(context_, &liveBullets);
    return true;
  }

  bool RestoreOwner(const SActiveWorldSection& section,
                    std::string* failure) override {
    if (!began_ || !ValidateOwnerCodec(section)) {
      SetFailure(failure, "active-world owner codec rejected a section");
      return false;
    }
    bool created = false;
    switch (section.kind) {
      case EActiveWorldSectionKind::Commander:
        created = CommanderState_CreateStableOwners(
            context_, section.payload, &createdCommanders_);
        break;
      case EActiveWorldSectionKind::TankGroup:
        created = TankGroupState_CreateStableOwners(
            context_, section.payload, &createdTankGroups_);
        break;
      case EActiveWorldSectionKind::Vehicle:
        created = VehicleActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdVehicles_);
        break;
      case EActiveWorldSectionKind::Mission:
        created = MissionActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdMissionRoutes_);
        break;
      case EActiveWorldSectionKind::People:
        created = PeopleActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdPeople_);
        break;
      case EActiveWorldSectionKind::Tank:
        created = TankActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdTanks_);
        break;
      case EActiveWorldSectionKind::Bullet:
        created = BulletActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdBullets_);
        break;
      case EActiveWorldSectionKind::Explosion:
        created = ExplosionActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdExplosions_);
        break;
      case EActiveWorldSectionKind::Spark:
        created = SparkActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdSparks_);
        break;
      case EActiveWorldSectionKind::Smoke:
        created = SmokeActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdSmokes_);
        break;
      case EActiveWorldSectionKind::Corpse:
        created = CorpseActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdCorpses_);
        break;
      case EActiveWorldSectionKind::Clock:
        created = ClockActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdClocks_);
        break;
      default:
        break;
    }
    if (!created) {
      if (section.kind == EActiveWorldSectionKind::Tank &&
          TankActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure, std::string("active-world Tank allocation failed: ") +
                                TankActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Bullet &&
               BulletActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("active-world Bullet allocation failed: ") +
                       BulletActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Explosion &&
               ExplosionActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("active-world Explosion allocation failed: ") +
                       ExplosionActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Spark &&
               SparkActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("active-world Spark allocation failed: ") +
                       SparkActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Smoke &&
               SmokeActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("active-world Smoke allocation failed: ") +
                       SmokeActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Corpse &&
               CorpseActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("active-world Corpse allocation failed: ") +
                       CorpseActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Mission &&
               MissionActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("active-world Mission allocation failed: ") +
                       MissionActiveWorldState_LastFailure());
      else
        SetFailure(failure, "active-world owner allocation failed");
      return false;
    }
    staged_.push_back(section);
    ++ownerPhases_;
    return true;
  }

  bool ResolveReferences(const SActiveWorldSection& section,
                         std::string* failure) override {
    bool resolved = false;
    if (began_)
      switch (section.kind) {
        case EActiveWorldSectionKind::Commander:
          resolved = CommanderState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::TankGroup:
          resolved = TankGroupState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Vehicle:
          resolved = VehicleActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Mission:
          resolved = MissionActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::People:
          resolved = PeopleActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Tank:
          resolved = TankActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Bullet:
          resolved = BulletActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Explosion:
          resolved = ExplosionActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Spark:
          resolved = SparkActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Smoke:
          resolved = SmokeActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Corpse:
          resolved = CorpseActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Clock:
          resolved = ClockActiveWorldState_ApplyStableReferences(
              section.payload);
          break;
        default:
          break;
      }
    if (!resolved || !OwnerMatchesWorld(context_, section)) {
      if (section.kind == EActiveWorldSectionKind::Tank &&
          TankActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("Tank symbolic reconstruction failed: ") +
                       TankActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Bullet &&
               BulletActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("Bullet symbolic reconstruction failed: ") +
                       BulletActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Explosion &&
               ExplosionActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("Explosion symbolic reconstruction failed: ") +
                       ExplosionActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Spark &&
               SparkActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("Spark symbolic reconstruction failed: ") +
                       SparkActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Smoke &&
               SmokeActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("Smoke symbolic reconstruction failed: ") +
                       SmokeActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Corpse &&
               CorpseActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("Corpse symbolic reconstruction failed: ") +
                       CorpseActiveWorldState_LastFailure());
      else if (section.kind == EActiveWorldSectionKind::Mission &&
               MissionActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure,
                   std::string("Mission symbolic reconstruction failed: ") +
                       MissionActiveWorldState_LastFailure());
      else
        SetFailure(failure, std::string("symbolic owner references do not ") +
                                "match the live graph: " + section.owner);
      return false;
    }
    ++referencePhases_;
    return true;
  }

  bool RestoreEvent(const SActiveWorldEvent& event,
                    std::string* failure) override {
    if (!began_ || event.sequence != stagedEvents_.size()) {
      SetFailure(failure, "EVT1 restore order is invalid");
      return false;
    }
    stagedEvents_.push_back(event);
    ++eventPhases_;
    return true;
  }

  bool Validate(std::uint64_t expectedWorldFingerprint,
                std::string* failure) override {
    bool clockMetadataMatches = false;
    if (snapshot_ != nullptr)
      for (const SActiveWorldSection& section : staged_)
        if (section.kind == EActiveWorldSectionKind::Clock)
          clockMetadataMatches = ClockActiveWorldState_MetadataMatches(
              section.payload, snapshot_->simulationTick,
              snapshot_->simulationTime);
    if (!began_ || snapshot_ == nullptr || staged_.size() != 12 ||
        ActiveWorldSave_ComputeWorldFingerprint(*snapshot_) !=
            expectedWorldFingerprint ||
        !clockMetadataMatches ||
        !SimulationRandom_Validate(snapshot_->rngAlgorithm,
                                   snapshot_->rngState)) {
      SetFailure(failure, "active-world staged fingerprint is invalid");
      return false;
    }
    if (stagedEvents_.size() != snapshot_->events.size() ||
        !ActiveWorldSemanticEvents_Validate(stagedEvents_, failure) ||
        !ActiveWorldSemanticEvents_Replace(
            context_, stagedEvents_, &createdSemanticOwners_, failure) ||
        !ActiveWorldSemanticEvents_Matches(context_, stagedEvents_) ||
        !SimulationRandom_Apply(snapshot_->rngAlgorithm,
                                snapshot_->rngState) ||
        !SimulationRandom_Matches(snapshot_->rngAlgorithm,
                                  snapshot_->rngState))
      return false;
    if (rejectValidation_) {
      SetFailure(failure, "intentional rollback probe");
      return false;
    }
    return true;
  }

  bool Commit(std::string* failure) override {
    if (!began_ || committed_ || rolledBack_) {
      SetFailure(failure, "active-world transaction cannot commit");
      return false;
    }
    committed_ = true;
    return true;
  }

  void Rollback() override {
    bool clean = began_;
    if (began_) {
      // Clear the staged queue while MSH1 still describes its mission indices.
      // Reverting Player first would make a staged rc_CHECK_MISSION impossible
      // to decode and would prevent the queue from rolling back atomically.
      clean = ActiveWorldSemanticEvents_Clear(context_, nullptr) && clean;
      createdSemanticOwners_.clear();
      CorpseActiveWorldState_RemoveStableOwners(context_, &createdCorpses_);
      ClockActiveWorldState_RemoveStableOwners(context_, &createdClocks_);
      MissionActiveWorldState_RemoveStableOwners(
          context_, &createdMissionRoutes_);
      SmokeActiveWorldState_RemoveStableOwners(context_, &createdSmokes_);
      SparkActiveWorldState_RemoveStableOwners(context_, &createdSparks_);
      ExplosionActiveWorldState_RemoveStableOwners(
          context_, &createdExplosions_);
      BulletActiveWorldState_RemoveStableOwners(context_, &createdBullets_);
      PeopleActiveWorldState_RemoveStableOwners(context_, &createdPeople_);
      VehicleActiveWorldState_RemoveStableOwners(context_, &createdVehicles_);
      TankActiveWorldState_RemoveStableOwners(context_, &createdTanks_);
      TankGroupState_RemoveStableOwners(context_, &createdTankGroups_);
      CommanderState_RemoveStableOwners(context_, &createdCommanders_);
      // Re-establish the original scheduler boundary before rebuilding live
      // owner timestamps. The final Clock pass below remains the proof that
      // reconstruction itself did not drift the boundary.
      clean = ClockActiveWorldState_ApplyStableReferences(
                  clockBackup_) && clean;
      if (replacedTransientOwners_) {
        std::vector<KR_ObjectID> restoredBullets;
        std::vector<KR_ObjectID> restoredExplosions;
        std::vector<KR_ObjectID> restoredSparks;
        std::vector<KR_ObjectID> restoredSmokes;
        std::vector<KR_ObjectID> restoredCorpses;
        clean = BulletActiveWorldState_CreateStableOwners(
                    context_, bulletBackup_, &restoredBullets) &&
                clean;
        clean = ExplosionActiveWorldState_CreateStableOwners(
                    context_, explosionBackup_, &restoredExplosions) &&
                clean;
        clean = SparkActiveWorldState_CreateStableOwners(
                    context_, sparkBackup_, &restoredSparks) &&
                clean;
        clean = SmokeActiveWorldState_CreateStableOwners(
                    context_, smokeBackup_, &restoredSmokes) &&
                clean;
        clean = CorpseActiveWorldState_CreateStableOwners(
                    context_, corpseBackup_, &restoredCorpses) &&
                clean;
      }
      clean = CommanderState_ApplyStableReferences(
                  context_, commanderBackup_) && clean;
      clean = TankGroupState_ApplyStableReferences(
                  context_, tankGroupBackup_) && clean;
      clean = VehicleActiveWorldState_ApplyStableReferences(
                  context_, vehicleBackup_) && clean;
      clean = PeopleActiveWorldState_ApplyStableReferences(
                  context_, peopleBackup_) && clean;
      clean = TankActiveWorldState_ApplyStableReferences(
                  context_, tankBackup_) && clean;
      clean = BulletActiveWorldState_ApplyStableReferences(
                  context_, bulletBackup_) && clean;
      clean = ExplosionActiveWorldState_ApplyStableReferences(
                  context_, explosionBackup_) && clean;
      clean = SparkActiveWorldState_ApplyStableReferences(
                  context_, sparkBackup_) && clean;
      clean = SmokeActiveWorldState_ApplyStableReferences(
                  context_, smokeBackup_) && clean;
      clean = CorpseActiveWorldState_ApplyStableReferences(
                  context_, corpseBackup_) && clean;
      // Mission conditions can target any restored owner kind, so rebuild the
      // Player mission graph only after every ordinary owner is stable.
      clean = MissionActiveWorldState_ApplyStableReferences(
                  context_, missionBackup_) && clean;
      std::vector<KR_ObjectID> restoredSemanticOwners;
      clean = ActiveWorldSemanticEvents_Replace(
                  context_, semanticBackup_, &restoredSemanticOwners,
                  nullptr) && clean;
      restoredSemanticOwners.clear();
      // Owner allocation and event reconstruction may sample RNG or observe
      // time. Restore the continuation boundary only after that work is done.
      clean = ClockActiveWorldState_ApplyStableReferences(
                  clockBackup_) && clean;
      clean = SimulationRandom_Apply(
                  SimulationRandom_Algorithm(), rngBackup_) && clean;
      clean = CommanderState_MatchesStable(context_, commanderBackup_) &&
              TankGroupState_MatchesStable(context_, tankGroupBackup_) &&
              VehicleActiveWorldState_MatchesStable(context_, vehicleBackup_) &&
              MissionActiveWorldState_MatchesStable(
                  context_, missionBackup_) &&
              PeopleActiveWorldState_MatchesStable(context_, peopleBackup_) &&
              TankActiveWorldState_MatchesStable(context_, tankBackup_) &&
              BulletActiveWorldState_MatchesStable(context_, bulletBackup_) &&
              ExplosionActiveWorldState_MatchesStable(
                  context_, explosionBackup_) &&
              SparkActiveWorldState_MatchesStable(
                  context_, sparkBackup_) &&
              SmokeActiveWorldState_MatchesStable(
                  context_, smokeBackup_) &&
              CorpseActiveWorldState_MatchesStable(
                  context_, corpseBackup_) &&
              ClockActiveWorldState_MatchesStable(clockBackup_) &&
              SimulationRandom_Matches(
                  SimulationRandom_Algorithm(), rngBackup_) &&
              ActiveWorldSemanticEvents_Matches(
                  context_, semanticBackup_) &&
              clean;
    }
    staged_.clear();
    stagedEvents_.clear();
    rolledBack_ = true;
    rollbackClean_ = clean;
  }

  bool Successful() const {
    return began_ && committed_ && !rolledBack_ && staged_.size() == 12 &&
           snapshot_ != nullptr && stagedEvents_.size() ==
               snapshot_->events.size() &&
           ActiveWorldSemanticEvents_Matches(context_, stagedEvents_) &&
           SimulationRandom_Matches(snapshot_->rngAlgorithm,
                                    snapshot_->rngState);
  }
  bool RolledBackCleanly() const {
    return began_ && !committed_ && rolledBack_ && staged_.empty() &&
           rollbackClean_ && createdCommanders_.empty() &&
           createdTankGroups_.empty() && createdVehicles_.empty() &&
           createdMissionRoutes_.empty() &&
           createdPeople_.empty() && createdTanks_.empty() &&
           createdBullets_.empty() && createdExplosions_.empty() &&
           createdSparks_.empty() && createdSmokes_.empty() &&
           createdCorpses_.empty() && createdClocks_.empty() &&
           createdSemanticOwners_.empty() &&
           stagedEvents_.empty();
  }
  int ownerPhases() const { return ownerPhases_; }
  int referencePhases() const { return referencePhases_; }
  int eventPhases() const { return eventPhases_; }
  int createdOwners() const {
    return static_cast<int>(createdCommanders_.size() +
                            createdTankGroups_.size() +
                            createdVehicles_.size() +
                            createdMissionRoutes_.size() +
                            createdPeople_.size() + createdTanks_.size() +
                            createdBullets_.size() +
                            createdExplosions_.size() +
                            createdSparks_.size() +
                            createdSmokes_.size() +
                            createdCorpses_.size() +
                            createdClocks_.size());
  }

 private:
  SimulationContext* context_;
  const SActiveWorldSnapshot* snapshot_;
  bool rejectValidation_;
  bool began_;
  bool committed_;
  bool rolledBack_;
  int ownerPhases_;
  int referencePhases_;
  int eventPhases_;
  bool rollbackClean_;
  bool replacedTransientOwners_;
  std::vector<SActiveWorldSection> staged_;
  std::vector<std::uint8_t> commanderBackup_;
  std::vector<std::uint8_t> tankGroupBackup_;
  std::vector<std::uint8_t> vehicleBackup_;
  std::vector<std::uint8_t> missionBackup_;
  std::vector<std::uint8_t> peopleBackup_;
  std::vector<std::uint8_t> tankBackup_;
  std::vector<std::uint8_t> bulletBackup_;
  std::vector<std::uint8_t> explosionBackup_;
  std::vector<std::uint8_t> sparkBackup_;
  std::vector<std::uint8_t> smokeBackup_;
  std::vector<std::uint8_t> corpseBackup_;
  std::vector<std::uint8_t> clockBackup_;
  std::vector<std::uint8_t> rngBackup_;
  std::vector<SActiveWorldEvent> semanticBackup_;
  std::vector<SActiveWorldEvent> stagedEvents_;
  std::vector<KR_ObjectID> createdCommanders_;
  std::vector<KR_ObjectID> createdTankGroups_;
  std::vector<KR_ObjectID> createdVehicles_;
  std::vector<KR_ObjectID> createdMissionRoutes_;
  std::vector<KR_ObjectID> createdPeople_;
  std::vector<KR_ObjectID> createdTanks_;
  std::vector<KR_ObjectID> createdBullets_;
  std::vector<KR_ObjectID> createdExplosions_;
  std::vector<KR_ObjectID> createdSparks_;
  std::vector<KR_ObjectID> createdSmokes_;
  std::vector<KR_ObjectID> createdCorpses_;
  std::vector<KR_ObjectID> createdClocks_;
  std::vector<KR_ObjectID> createdSemanticOwners_;
};

}  // namespace

SActiveWorldRuntimeProbeSummary::SActiveWorldRuntimeProbeSummary()
    : ready(false), sections(0), events(0), ownerPhases(0),
      referencePhases(0), eventPhases(0), createdOwners(0),
      missionRecords(0), missionConditionReferences(0),
      missionRouteReferences(0), missionCheckEvents(0),
      clockRecords(0), rngAlgorithm(0), rngStateBytes(0), rngDrawCount(0),
      corruptionRejects(0), rollbacks(0), containerBytes(0),
      worldFingerprint(0) {}

namespace {

bool CaptureRuntime(
    SimulationContext* context, std::uint64_t contentFingerprint,
    const std::string& level, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure,
    bool stageFixtures, bool testCorruption) {
  if (bytes == nullptr || summary == nullptr) {
    SetFailure(failure, "active-world capture arguments are invalid");
    return false;
  }
  *summary = SActiveWorldRuntimeProbeSummary();
  SActiveWorldSnapshot snapshot;
  snapshot.engineCompatibility = ActiveWorldSave_EngineCompatibilityVersion();
  snapshot.contentFingerprint = contentFingerprint;
  snapshot.level = level;
  if (RecoveredModRuntime_IsActive()) {
    const unsigned int count = RecoveredModRuntime_ModCount();
    snapshot.mods.reserve(count);
    for (unsigned int index = 0; index < count; ++index) {
      SRecoveredModPackage package;
      if (!RecoveredModRuntime_Mod(index, &package)) {
        SetFailure(failure, "active mod stack enumeration failed");
        return false;
      }
      snapshot.mods.push_back(std::string(package.id) + "@" +
                              package.version);
    }
    // AWS1 keeps this metadata as a canonical set. The order-sensitive mount
    // identity is already bound by contentFingerprint.
    std::sort(snapshot.mods.begin(), snapshot.mods.end());
  }
  SSimulationClockState clockState;
  if (!SUA_CaptureSimulationClock(&clockState)) {
    char detail[320] = {};
    std::snprintf(
        detail, sizeof(detail),
        "authoritative simulation clock is invalid "
        "(tick=%llu event=%.17g view=%.17g frame=%.17g "
        "aspect=%.17g clamped=%u/%.17g)",
        static_cast<unsigned long long>(clockState.tick),
        clockState.eventMoment, clockState.viewTime,
        clockState.frameSeconds, clockState.timerAspect,
        clockState.clampedSamples, clockState.clampedSeconds);
    SetFailure(failure, detail);
    return false;
  }
  const double probeTime =
      (std::max)(clockState.eventMoment, clockState.viewTime);
  bool stagedMission = false;
  if (stageFixtures && !MissionActiveWorldState_StageProbe(
          context, probeTime + 30.0, &stagedMission)) {
    SetFailure(failure, MissionActiveWorldState_LastFailure());
    return false;
  }
  bool stagedProbeEvents = false;
  if (stageFixtures && !ActiveWorldSemanticEvents_StageProbe(
          context, probeTime + 30.0, &stagedProbeEvents, failure)) {
    if (stagedMission)
      MissionActiveWorldState_ClearProbe(context);
    return false;
  }
  const auto cleanupProbeEvents = [&]() {
    const bool effectsClean = !stagedProbeEvents ||
        ActiveWorldSemanticEvents_ClearProbe(context, failure);
    const bool missionClean = !stagedMission ||
        MissionActiveWorldState_ClearProbe(context);
    return effectsClean && missionClean;
  };
  if (!SUA_CaptureSimulationClock(&clockState) ||
      !SimulationRandom_Capture(&snapshot.rngState)) {
    cleanupProbeEvents();
    SetFailure(failure, "authoritative clock/RNG capture failed");
    return false;
  }
  snapshot.simulationTick = clockState.tick;
  snapshot.simulationTime =
      (std::max)(clockState.eventMoment, clockState.viewTime);
  snapshot.rngAlgorithm = SimulationRandom_Algorithm();
  if (!CaptureOwnerSections(context, &snapshot.sections, failure)) {
    cleanupProbeEvents();
    return false;
  }
  if (!ActiveWorldSemanticEvents_Capture(
          context, &snapshot.events, failure)) {
    cleanupProbeEvents();
    return false;
  }

  SActiveWorldSaveStatus status;
  if (!ActiveWorldSave_Encode(snapshot, bytes, &status)) {
    cleanupProbeEvents();
    SetFailure(failure, status.detail);
    return false;
  }
  SActiveWorldSnapshot decoded;
  if (!ActiveWorldSave_Decode(*bytes, &decoded, &status)) {
    cleanupProbeEvents();
    SetFailure(failure, status.detail);
    return false;
  }
  if (testCorruption) {
    std::vector<std::uint8_t> corrupt = *bytes;
    corrupt[corrupt.size() / 2] ^= 0x40;
    if (ActiveWorldSave_Decode(corrupt, &decoded, &status) ||
        status.error != EActiveWorldSaveError::IntegrityMismatch) {
      cleanupProbeEvents();
      SetFailure(failure, "active-world corruption probe was accepted");
      return false;
    }
    if (!ActiveWorldSave_Decode(*bytes, &decoded, &status)) {
      cleanupProbeEvents();
      SetFailure(failure, status.detail);
      return false;
    }
  }
  if (!cleanupProbeEvents())
    return false;
  summary->sections = static_cast<int>(decoded.sections.size());
  summary->events = static_cast<int>(decoded.events.size());
  for (const SActiveWorldSection& section : decoded.sections) {
    if (section.kind == EActiveWorldSectionKind::Mission) {
      if (!MissionActiveWorldState_ProbeCounts(
              section.payload, &summary->missionRecords,
              &summary->missionConditionReferences,
              &summary->missionRouteReferences)) {
        SetFailure(failure, "MSH1 decoded mission telemetry is invalid");
        return false;
      }
    } else if (section.kind == EActiveWorldSectionKind::Clock) {
      if (!ClockActiveWorldState_MetadataMatches(
              section.payload, decoded.simulationTick,
              decoded.simulationTime)) {
        SetFailure(failure, "CLK1 metadata diverges from the envelope");
        return false;
      }
      ++summary->clockRecords;
    }
  }
  for (const SActiveWorldEvent& event : decoded.events)
    if (event.label == rc_CHECK_MISSION)
      ++summary->missionCheckEvents;
  summary->rngAlgorithm = decoded.rngAlgorithm;
  summary->rngStateBytes = static_cast<int>(decoded.rngState.size());
  std::uint32_t rngState = 0;
  if (!SimulationRandom_Decode(
          decoded.rngState, &rngState, &summary->rngDrawCount)) {
    SetFailure(failure, "simulation RNG state is invalid");
    return false;
  }
  summary->corruptionRejects = testCorruption ? 1 : 0;
  summary->containerBytes = bytes->size();
  summary->worldFingerprint = decoded.worldFingerprint;
  return true;
}

}  // namespace

bool ActiveWorldRuntime_CaptureProbe(
    SimulationContext* context, std::uint64_t contentFingerprint,
    const std::string& level, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure) {
  return CaptureRuntime(context, contentFingerprint, level, bytes, summary,
                        failure, true, true);
}

bool ActiveWorldRuntime_Capture(
    SimulationContext* context, std::uint64_t contentFingerprint,
    const std::string& level, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure) {
  return CaptureRuntime(context, contentFingerprint, level, bytes, summary,
                        failure, false, false);
}

namespace {

bool RestoreRuntime(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure,
    bool runRollbackProbe, bool clearProbeFixtures) {
  if (context == nullptr || summary == nullptr) {
    SetFailure(failure, "active-world restore arguments are invalid");
    return false;
  }
  SActiveWorldSaveStatus status;
  SActiveWorldSnapshot snapshot;
  if (!ActiveWorldSave_Decode(bytes, &snapshot, &status)) {
    SetFailure(failure, status.detail);
    return false;
  }

  RuntimeRestoreTarget success(context, false);
  if (!ActiveWorldSave_RestoreTransactional(snapshot, &success, &status) ||
      !success.Successful()) {
    if (clearProbeFixtures) {
      ActiveWorldSemanticEvents_ClearProbe(context, nullptr);
      MissionActiveWorldState_ClearProbe(context);
    }
    SetFailure(failure, status.detail);
    return false;
  }
  if (runRollbackProbe) {
    RuntimeRestoreTarget rollback(context, true);
    if (ActiveWorldSave_RestoreTransactional(snapshot, &rollback, &status) ||
        status.error != EActiveWorldSaveError::RestoreValidationFailed ||
        !rollback.RolledBackCleanly()) {
      if (clearProbeFixtures) {
        ActiveWorldSemanticEvents_ClearProbe(context, nullptr);
        MissionActiveWorldState_ClearProbe(context);
      }
      SetFailure(failure,
                 "active-world rollback probe did not unwind staging");
      return false;
    }
  }
  for (const SActiveWorldSection& section : snapshot.sections) {
    if (!OwnerMatchesWorld(context, section)) {
      SetFailure(failure, "active-world rollback changed the live graph");
      if (clearProbeFixtures) {
        ActiveWorldSemanticEvents_ClearProbe(context, nullptr);
        MissionActiveWorldState_ClearProbe(context);
      }
      return false;
    }
  }
  if (!ActiveWorldSemanticEvents_Matches(context, snapshot.events))
    return false;
  if (clearProbeFixtures &&
      (!ActiveWorldSemanticEvents_ClearProbe(context, failure) ||
       !MissionActiveWorldState_ClearProbe(context)))
    return false;

  bool clockMatchesAfterCleanup = false;
  for (const SActiveWorldSection& section : snapshot.sections)
    if (section.kind == EActiveWorldSectionKind::Clock)
      clockMatchesAfterCleanup = ClockActiveWorldState_MatchesStable(
          section.payload);
  if (!clockMatchesAfterCleanup ||
      !SimulationRandom_Matches(snapshot.rngAlgorithm, snapshot.rngState)) {
    SetFailure(failure,
               "authoritative clock/RNG changed after restore cleanup");
    return false;
  }

  summary->ownerPhases = success.ownerPhases();
  summary->referencePhases = success.referencePhases();
  summary->eventPhases = success.eventPhases();
  summary->createdOwners = success.createdOwners();
  summary->rollbacks = runRollbackProbe ? 1 : 0;
  summary->ready = true;
  return true;
}

}  // namespace

bool ActiveWorldRuntime_RestoreProbe(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure) {
  return RestoreRuntime(context, bytes, summary, failure, true, true);
}

bool ActiveWorldRuntime_Restore(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure) {
  return RestoreRuntime(context, bytes, summary, failure, false, false);
}
