#include "ActiveWorldRuntimeProbe.h"

#include "ActiveWorldSave.h"

#include "kernel/h/context.h"
#include "obase/comander/CommanderState.h"
#include "obase/group/TankGroupState.h"

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
    default:
      return false;
  }
}

class RuntimeRestoreTarget final : public IActiveWorldRestoreTarget {
 public:
  RuntimeRestoreTarget(SimulationContext* context, bool rejectValidation)
      : context_(context), snapshot_(nullptr), rejectValidation_(rejectValidation),
        began_(false), committed_(false), rolledBack_(false), ownerPhases_(0),
        referencePhases_(0), eventPhases_(0) {}

  bool Begin(const SActiveWorldSnapshot& snapshot,
             std::string* failure) override {
    if (context_ == nullptr || began_) {
      SetFailure(failure, "active-world transaction cannot begin");
      return false;
    }
    snapshot_ = &snapshot;
    began_ = true;
    staged_.clear();
    return true;
  }

  bool RestoreOwner(const SActiveWorldSection& section,
                    std::string* failure) override {
    if (!began_ || !ValidateOwnerCodec(section)) {
      SetFailure(failure, "active-world owner codec rejected a section");
      return false;
    }
    staged_.push_back(section);
    ++ownerPhases_;
    return true;
  }

  bool ResolveReferences(const SActiveWorldSection& section,
                         std::string* failure) override {
    if (!began_ || !OwnerMatchesWorld(context_, section)) {
      SetFailure(failure,
                 "symbolic owner references do not match the live graph");
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
    if (!began_ || snapshot_ == nullptr || staged_.size() != 2 ||
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
    staged_.clear();
    rolledBack_ = true;
  }

  bool Successful() const {
    return began_ && committed_ && !rolledBack_ && staged_.size() == 2;
  }
  bool RolledBackCleanly() const {
    return began_ && !committed_ && rolledBack_ && staged_.empty();
  }
  int ownerPhases() const { return ownerPhases_; }
  int referencePhases() const { return referencePhases_; }
  int eventPhases() const { return eventPhases_; }

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
  std::vector<SActiveWorldSection> staged_;
};

}  // namespace

SActiveWorldRuntimeProbeSummary::SActiveWorldRuntimeProbeSummary()
    : ready(false), sections(0), events(0), ownerPhases(0),
      referencePhases(0), eventPhases(0), corruptionRejects(0), rollbacks(0),
      containerBytes(0), worldFingerprint(0) {}

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
    SetFailure(failure, "Commander/TankGroup stable capture failed");
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
  summary->rollbacks = 1;
  summary->ready = true;
  return true;
}
