#include "ActiveWorldSave.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << "active-world-save-smoke: " << message << '\n';
  return false;
}

SActiveWorldSection Section(EActiveWorldSectionKind kind, const char* owner,
                            std::initializer_list<std::uint8_t> payload) {
  SActiveWorldSection section = {};
  section.kind = kind;
  section.schemaVersion = 1;
  section.owner = owner;
  section.payload.assign(payload.begin(), payload.end());
  return section;
}

SActiveWorldEvent Event(std::uint32_t sequence, std::uint64_t tick,
                        double timeStamp, std::int32_t label,
                        const char* source, const char* destination,
                        std::initializer_list<std::uint8_t> payload) {
  SActiveWorldEvent event = {};
  event.sequence = sequence;
  event.tick = tick;
  event.timeStamp = timeStamp;
  event.label = label;
  event.source = source;
  event.destination = destination;
  event.payloadVersion = 1;
  event.payload.assign(payload.begin(), payload.end());
  return event;
}

class RestoreProbe final : public IActiveWorldRestoreTarget {
 public:
  enum class Failure { None, Begin, Owner, Reference, Event, Validate, Commit };

  explicit RestoreProbe(Failure failure)
      : failure_(failure), begins_(0), owners_(0), references_(0), events_(0),
        validations_(0), commits_(0), rollbacks_(0), baseline_("live"),
        working_(baseline_) {}

  bool Begin(const SActiveWorldSnapshot&, std::string* failure) override {
    ++begins_;
    if (failure_ == Failure::Begin) {
      *failure = "begin probe failure";
      return false;
    }
    working_ = baseline_ + ":temporary";
    return true;
  }

  bool RestoreOwner(const SActiveWorldSection& section,
                    std::string* failure) override {
    ++owners_;
    working_ += ":owner-" + section.owner;
    if (failure_ != Failure::Owner) return true;
    *failure = "owner probe failure";
    return false;
  }

  bool ResolveReferences(const SActiveWorldSection& section,
                         std::string* failure) override {
    ++references_;
    working_ += ":refs-" + section.owner;
    if (failure_ != Failure::Reference) return true;
    *failure = "reference probe failure";
    return false;
  }

  bool RestoreEvent(const SActiveWorldEvent&, std::string* failure) override {
    ++events_;
    working_ += ":event";
    if (failure_ != Failure::Event) return true;
    *failure = "event probe failure";
    return false;
  }

  bool Validate(std::uint64_t expected, std::string* failure) override {
    ++validations_;
    if (failure_ != Failure::Validate && expected != 0) return true;
    *failure = "validation probe failure";
    return false;
  }

  bool Commit(std::string* failure) override {
    ++commits_;
    if (failure_ == Failure::Commit) {
      *failure = "commit probe failure";
      return false;
    }
    baseline_ = working_;
    return true;
  }

  void Rollback() override {
    ++rollbacks_;
    working_ = baseline_;
  }

  Failure failure_;
  int begins_;
  int owners_;
  int references_;
  int events_;
  int validations_;
  int commits_;
  int rollbacks_;
  std::string baseline_;
  std::string working_;
};

void PutU64At(std::vector<std::uint8_t>* bytes, std::size_t offset,
              std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8)
    (*bytes)[offset + shift / 8] =
        static_cast<std::uint8_t>(value >> shift);
}

std::uint64_t HashPrefix(const std::vector<std::uint8_t>& bytes,
                         std::size_t size) {
  std::uint64_t hash = UINT64_C(14695981039346656037);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (!Expect(argc == 2, "expected one output path")) return EXIT_FAILURE;

  SActiveWorldSnapshot source;
  source.engineCompatibility = ActiveWorldSave_EngineCompatibilityVersion();
  source.contentFingerprint = UINT64_C(0x1122334455667788);
  source.simulationTick = 1532;
  source.simulationTime = 51.25;
  source.rngAlgorithm = 1;
  source.rngState = {0x19, 0x99, 0x05, 0x31};
  source.level = "Level.04D";
  source.mods.push_back("base-retail@may-1999");
  source.sections.push_back(Section(
      EActiveWorldSectionKind::Commander, "Commander", {1, 2, 3, 4}));
  source.sections.push_back(Section(
      EActiveWorldSectionKind::TankGroup, "TankGroup", {5, 6, 7}));
  source.sections.push_back(
      Section(EActiveWorldSectionKind::Tank, "Tank", {8, 9}));
  source.events.push_back(Event(0, 1533, 51.35, 101,
                                "C.Group.aer00.00", "C.Group.aer00.00", {}));
  source.events.push_back(Event(1, 1534, 51.45, 102,
                                "C.Group.aer00.00", "C.Unit.aer00.00",
                                {10, 11, 12}));

  SActiveWorldSaveStatus status;
  std::vector<std::uint8_t> encoded;
  SActiveWorldSnapshot decoded;
  if (!Expect(ActiveWorldSave_Encode(source, &encoded, &status),
              "canonical snapshot did not encode") ||
      !Expect(ActiveWorldSave_Decode(encoded, &decoded, &status),
              "canonical snapshot did not decode") ||
      !Expect(decoded.worldFingerprint != 0 &&
                  decoded.worldFingerprint ==
                      ActiveWorldSave_ComputeWorldFingerprint(decoded),
              "world fingerprint did not round-trip") ||
      !Expect(decoded.level == source.level &&
                  decoded.sections.size() == 3 && decoded.events.size() == 2,
              "decoded owner/event roster changed")) {
    return EXIT_FAILURE;
  }

  std::vector<std::uint8_t> corrupt = encoded;
  corrupt[corrupt.size() / 2] ^= 0x40;
  if (!Expect(!ActiveWorldSave_Decode(corrupt, &decoded, &status) &&
                  status.error == EActiveWorldSaveError::IntegrityMismatch,
              "container corruption was not rejected")) {
    return EXIT_FAILURE;
  }
  std::vector<std::uint8_t> truncated(encoded.begin(), encoded.end() - 5);
  if (!Expect(!ActiveWorldSave_Decode(truncated, &decoded, &status),
              "truncated container was accepted")) {
    return EXIT_FAILURE;
  }
  std::vector<std::uint8_t> future = encoded;
  future[8] = 2;
  const std::size_t bodySize = future.size() - 8;
  PutU64At(&future, bodySize, HashPrefix(future, bodySize));
  if (!Expect(!ActiveWorldSave_Decode(future, &decoded, &status) &&
                  status.error == EActiveWorldSaveError::UnsupportedFormat,
              "future container version was not distinguished")) {
    return EXIT_FAILURE;
  }
  std::vector<std::uint8_t> futureEngine = encoded;
  futureEngine[12] = static_cast<std::uint8_t>(
      ActiveWorldSave_EngineCompatibilityVersion() + 1);
  PutU64At(&futureEngine, bodySize, HashPrefix(futureEngine, bodySize));
  if (!Expect(!ActiveWorldSave_Decode(futureEngine, &decoded, &status) &&
                  status.error ==
                      EActiveWorldSaveError::UnsupportedEngineCompatibility,
              "future engine compatibility was not distinguished")) {
    return EXIT_FAILURE;
  }

  SActiveWorldSnapshot duplicate = source;
  duplicate.sections.insert(duplicate.sections.begin() + 1,
                            duplicate.sections.front());
  if (!Expect(!ActiveWorldSave_Encode(duplicate, &encoded, &status) &&
                  status.error == EActiveWorldSaveError::DuplicateSection,
              "duplicate owner section was accepted")) {
    return EXIT_FAILURE;
  }

  const std::wstring path = argv[1];
  DeleteFileW(path.c_str());
  if (!Expect(ActiveWorldSave_WriteAtomic(path, source, &status),
              "atomic save write failed") ||
      !Expect(ActiveWorldSave_Read(path, &decoded, &status),
              "atomic save read failed") ||
      !Expect(decoded.worldFingerprint ==
                  ActiveWorldSave_ComputeWorldFingerprint(decoded),
              "file round-trip fingerprint changed")) {
    DeleteFileW(path.c_str());
    return EXIT_FAILURE;
  }
  const std::uint64_t committedFingerprint = decoded.worldFingerprint;
  SActiveWorldSnapshot invalidFile = source;
  invalidFile.contentFingerprint = 0;
  if (!Expect(!ActiveWorldSave_WriteAtomic(path, invalidFile, &status),
              "invalid save replaced the committed file") ||
      !Expect(ActiveWorldSave_Read(path, &decoded, &status) &&
                  decoded.worldFingerprint == committedFingerprint,
              "failed atomic write changed the committed file")) {
    DeleteFileW(path.c_str());
    return EXIT_FAILURE;
  }
  DeleteFileW(path.c_str());

  RestoreProbe success(RestoreProbe::Failure::None);
  if (!Expect(ActiveWorldSave_RestoreTransactional(decoded, &success, &status),
              "transactional restore did not commit") ||
      !Expect(success.begins_ == 1 && success.owners_ == 3 &&
                  success.references_ == 3 && success.events_ == 2 &&
                  success.validations_ == 1 && success.commits_ == 1 &&
                  success.rollbacks_ == 0,
              "transactional phase ordering changed")) {
    return EXIT_FAILURE;
  }

  struct FailureCase {
    RestoreProbe::Failure failure;
    EActiveWorldSaveError error;
  };
  const FailureCase failureCases[] = {
      {RestoreProbe::Failure::Owner,
       EActiveWorldSaveError::RestoreOwnerFailed},
      {RestoreProbe::Failure::Reference,
       EActiveWorldSaveError::RestoreReferenceFailed},
      {RestoreProbe::Failure::Event,
       EActiveWorldSaveError::RestoreEventFailed},
      {RestoreProbe::Failure::Validate,
       EActiveWorldSaveError::RestoreValidationFailed},
      {RestoreProbe::Failure::Commit,
       EActiveWorldSaveError::RestoreCommitFailed}};
  for (const FailureCase& failureCase : failureCases) {
    RestoreProbe failure(failureCase.failure);
    const std::string before = failure.baseline_;
    if (!Expect(!ActiveWorldSave_RestoreTransactional(decoded, &failure,
                                                       &status) &&
                    status.error == failureCase.error,
                "restore phase failure was accepted") ||
        !Expect(failure.commits_ <= 1 && failure.rollbacks_ == 1 &&
                    failure.working_ == before && failure.baseline_ == before,
                "failed restore did not roll back atomically")) {
      return EXIT_FAILURE;
    }
  }
  RestoreProbe beginFailure(RestoreProbe::Failure::Begin);
  if (!Expect(!ActiveWorldSave_RestoreTransactional(decoded, &beginFailure,
                                                     &status) &&
                  status.error == EActiveWorldSaveError::RestoreBeginFailed &&
                  beginFailure.begins_ == 1 &&
                  beginFailure.rollbacks_ == 0,
              "failed Begin incorrectly entered rollback")) {
    return EXIT_FAILURE;
  }

  decoded.sections.front().payload.push_back(99);
  RestoreProbe stale(RestoreProbe::Failure::None);
  if (!Expect(!ActiveWorldSave_RestoreTransactional(decoded, &stale, &status) &&
                  status.error == EActiveWorldSaveError::IntegrityMismatch &&
                  stale.begins_ == 0,
              "stale in-memory snapshot reached the restore target")) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
