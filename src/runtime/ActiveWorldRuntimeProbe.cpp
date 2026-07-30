#include "ActiveWorldRuntimeProbe.h"

#include "ActiveWorldSave.h"

#include "kernel/h/context.h"
#include "obase/comander/CommanderState.h"
#include "obase/group/TankGroupState.h"
#include "obase/people/PeopleActiveWorldState.h"
#include "obase/tank/TankActiveWorldState.h"
#include "obase/vehicle/VehicleActiveWorldState.h"

#include <utility>

namespace {

void SetFailure(std::string* failure, const std::string& message) {
  if (failure != nullptr) *failure = message;
}

bool CaptureOwnerSections(SimulationContext* context,
                          std::vector<SActiveWorldSection>* sections) {
  if (context == nullptr || sections == nullptr) return false;
  SActiveWorldSection commanders = {};
  commanders.kind = EActiveWorldSectionKind::Commander;
  commanders.schemaVersion = 1;
  commanders.owner = "Commander";
  if (!CommanderState_CaptureStable(context, &commanders.payload))
    return false;

  SActiveWorldSection groups = {};
  groups.kind = EActiveWorldSectionKind::TankGroup;
  groups.schemaVersion = 1;
  groups.owner = "TankGroup";
  if (!TankGroupState_CaptureStable(context, &groups.payload)) return false;

  sections->clear();
  sections->push_back(std::move(commanders));
  sections->push_back(std::move(groups));

  SActiveWorldSection people = {};
  people.kind = EActiveWorldSectionKind::People;
  people.schemaVersion = 1;
  people.owner = "People";
  if (!PeopleActiveWorldState_CaptureStable(context, &people.payload))
    return false;
  sections->push_back(std::move(people));

  SActiveWorldSection tanks = {};
  tanks.kind = EActiveWorldSectionKind::Tank;
  tanks.schemaVersion = 1;
  tanks.owner = "Tank";
  if (!TankActiveWorldState_CaptureStable(context, &tanks.payload))
    return false;
  sections->push_back(std::move(tanks));

  SActiveWorldSection vehicle = {};
  vehicle.kind = EActiveWorldSectionKind::Vehicle;
  vehicle.schemaVersion = 1;
  vehicle.owner = "Vehicle";
  if (!VehicleActiveWorldState_CaptureStable(context, &vehicle.payload))
    return false;
  sections->push_back(std::move(vehicle));
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
    case EActiveWorldSectionKind::People:
      return section.owner == "People" &&
             PeopleActiveWorldState_ValidateStable(section.payload);
    case EActiveWorldSectionKind::Tank:
      return section.owner == "Tank" &&
             TankActiveWorldState_ValidateStable(section.payload);
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
    case EActiveWorldSectionKind::People:
      return PeopleActiveWorldState_MatchesStable(context, section.payload);
    case EActiveWorldSectionKind::Tank:
      return TankActiveWorldState_MatchesStable(context, section.payload);
    default:
      return false;
  }
}

class RuntimeRestoreTarget final : public IActiveWorldRestoreTarget {
 public:
  RuntimeRestoreTarget(SimulationContext* context, bool rejectValidation)
      : context_(context), snapshot_(nullptr), rejectValidation_(rejectValidation),
        began_(false), committed_(false), rolledBack_(false), ownerPhases_(0),
        referencePhases_(0), eventPhases_(0), rollbackClean_(false) {}

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
    peopleBackup_.clear();
    tankBackup_.clear();
    createdCommanders_.clear();
    createdTankGroups_.clear();
    createdVehicles_.clear();
    createdPeople_.clear();
    createdTanks_.clear();
    if (!CommanderState_CaptureStable(context_, &commanderBackup_) ||
        !TankGroupState_CaptureStable(context_, &tankGroupBackup_) ||
        !VehicleActiveWorldState_CaptureStable(context_, &vehicleBackup_) ||
        !PeopleActiveWorldState_CaptureStable(context_, &peopleBackup_) ||
        !TankActiveWorldState_CaptureStable(context_, &tankBackup_)) {
      SetFailure(failure, "active-world live owner backup failed");
      began_ = false;
      return false;
    }
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
      case EActiveWorldSectionKind::People:
        created = PeopleActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdPeople_);
        break;
      case EActiveWorldSectionKind::Tank:
        created = TankActiveWorldState_CreateStableOwners(
            context_, section.payload, &createdTanks_);
        break;
      default:
        break;
    }
    if (!created) {
      if (section.kind == EActiveWorldSectionKind::Tank &&
          TankActiveWorldState_LastFailure()[0] != '\0')
        SetFailure(failure, std::string("active-world Tank allocation failed: ") +
                                TankActiveWorldState_LastFailure());
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
        case EActiveWorldSectionKind::People:
          resolved = PeopleActiveWorldState_ApplyStableReferences(
              context_, section.payload);
          break;
        case EActiveWorldSectionKind::Tank:
          resolved = TankActiveWorldState_ApplyStableReferences(
              context_, section.payload);
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
      else
        SetFailure(failure, std::string("symbolic owner references do not ") +
                                "match the live graph: " + section.owner);
      return false;
    }
    ++referencePhases_;
    return true;
  }

  bool RestoreEvent(const SActiveWorldEvent&, std::string* failure) override {
    SetFailure(failure,
               "semantic event import is not connected to SimulationContext");
    ++eventPhases_;
    return false;
  }

  bool Validate(std::uint64_t expectedWorldFingerprint,
                std::string* failure) override {
    if (!began_ || snapshot_ == nullptr || staged_.size() != 5 ||
        ActiveWorldSave_ComputeWorldFingerprint(*snapshot_) !=
            expectedWorldFingerprint) {
      SetFailure(failure, "active-world staged fingerprint is invalid");
      return false;
    }
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
      PeopleActiveWorldState_RemoveStableOwners(context_, &createdPeople_);
      VehicleActiveWorldState_RemoveStableOwners(context_, &createdVehicles_);
      TankActiveWorldState_RemoveStableOwners(context_, &createdTanks_);
      TankGroupState_RemoveStableOwners(context_, &createdTankGroups_);
      CommanderState_RemoveStableOwners(context_, &createdCommanders_);
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
      clean = CommanderState_MatchesStable(context_, commanderBackup_) &&
              TankGroupState_MatchesStable(context_, tankGroupBackup_) &&
              VehicleActiveWorldState_MatchesStable(context_, vehicleBackup_) &&
              PeopleActiveWorldState_MatchesStable(context_, peopleBackup_) &&
              TankActiveWorldState_MatchesStable(context_, tankBackup_) &&
              clean;
    }
    staged_.clear();
    rolledBack_ = true;
    rollbackClean_ = clean;
  }

  bool Successful() const {
    return began_ && committed_ && !rolledBack_ && staged_.size() == 5;
  }
  bool RolledBackCleanly() const {
    return began_ && !committed_ && rolledBack_ && staged_.empty() &&
           rollbackClean_ && createdCommanders_.empty() &&
           createdTankGroups_.empty() && createdVehicles_.empty() &&
           createdPeople_.empty() && createdTanks_.empty();
  }
  int ownerPhases() const { return ownerPhases_; }
  int referencePhases() const { return referencePhases_; }
  int eventPhases() const { return eventPhases_; }
  int createdOwners() const {
    return static_cast<int>(createdCommanders_.size() +
                            createdTankGroups_.size() +
                            createdVehicles_.size() +
                            createdPeople_.size() + createdTanks_.size());
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
  std::vector<SActiveWorldSection> staged_;
  std::vector<std::uint8_t> commanderBackup_;
  std::vector<std::uint8_t> tankGroupBackup_;
  std::vector<std::uint8_t> vehicleBackup_;
  std::vector<std::uint8_t> peopleBackup_;
  std::vector<std::uint8_t> tankBackup_;
  std::vector<KR_ObjectID> createdCommanders_;
  std::vector<KR_ObjectID> createdTankGroups_;
  std::vector<KR_ObjectID> createdVehicles_;
  std::vector<KR_ObjectID> createdPeople_;
  std::vector<KR_ObjectID> createdTanks_;
};

}  // namespace

SActiveWorldRuntimeProbeSummary::SActiveWorldRuntimeProbeSummary()
    : ready(false), sections(0), events(0), ownerPhases(0),
      referencePhases(0), eventPhases(0), createdOwners(0),
      corruptionRejects(0), rollbacks(0), containerBytes(0),
      worldFingerprint(0) {}

bool ActiveWorldRuntime_CaptureProbe(
    SimulationContext* context, std::uint64_t contentFingerprint,
    std::uint64_t simulationTick, double simulationTime,
    const std::string& level, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure) {
  if (bytes == nullptr || summary == nullptr) {
    SetFailure(failure, "active-world capture arguments are invalid");
    return false;
  }
  *summary = SActiveWorldRuntimeProbeSummary();
  SActiveWorldSnapshot snapshot;
  snapshot.engineCompatibility = ActiveWorldSave_EngineCompatibilityVersion();
  snapshot.contentFingerprint = contentFingerprint;
  snapshot.simulationTick = simulationTick;
  snapshot.simulationTime = simulationTime;
  snapshot.level = level;
  if (!CaptureOwnerSections(context, &snapshot.sections)) {
    SetFailure(failure,
               "Commander/TankGroup/People/Tank/Vehicle stable capture failed");
    return false;
  }

  SActiveWorldSaveStatus status;
  if (!ActiveWorldSave_Encode(snapshot, bytes, &status)) {
    SetFailure(failure, status.detail);
    return false;
  }
  SActiveWorldSnapshot decoded;
  if (!ActiveWorldSave_Decode(*bytes, &decoded, &status)) {
    SetFailure(failure, status.detail);
    return false;
  }
  std::vector<std::uint8_t> corrupt = *bytes;
  corrupt[corrupt.size() / 2] ^= 0x40;
  if (ActiveWorldSave_Decode(corrupt, &decoded, &status) ||
      status.error != EActiveWorldSaveError::IntegrityMismatch) {
    SetFailure(failure, "active-world corruption probe was accepted");
    return false;
  }
  if (!ActiveWorldSave_Decode(*bytes, &decoded, &status)) {
    SetFailure(failure, status.detail);
    return false;
  }
  summary->sections = static_cast<int>(decoded.sections.size());
  summary->events = static_cast<int>(decoded.events.size());
  summary->corruptionRejects = 1;
  summary->containerBytes = bytes->size();
  summary->worldFingerprint = decoded.worldFingerprint;
  return true;
}

bool ActiveWorldRuntime_RestoreProbe(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure) {
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
    SetFailure(failure, status.detail);
    return false;
  }
  RuntimeRestoreTarget rollback(context, true);
  if (ActiveWorldSave_RestoreTransactional(snapshot, &rollback, &status) ||
      status.error != EActiveWorldSaveError::RestoreValidationFailed ||
      !rollback.RolledBackCleanly()) {
    SetFailure(failure, "active-world rollback probe did not unwind staging");
    return false;
  }
  for (const SActiveWorldSection& section : snapshot.sections) {
    if (!OwnerMatchesWorld(context, section)) {
      SetFailure(failure, "active-world rollback changed the live graph");
      return false;
    }
  }

  summary->ownerPhases = success.ownerPhases();
  summary->referencePhases = success.referencePhases();
  summary->eventPhases = success.eventPhases();
  summary->createdOwners = success.createdOwners();
  summary->rollbacks = 1;
  summary->ready = true;
  return true;
}
