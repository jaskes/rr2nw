#ifndef RR2NW_ACTIVE_WORLD_SAVE_H
#define RR2NW_ACTIVE_WORLD_SAVE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class EActiveWorldSectionKind : std::uint32_t {
  Commander = 1,
  TankGroup = 2,
  People = 3,
  Tank = 4,
  Cannon = 5,
  Vehicle = 6,
  Mission = 7,
  Bullet = 8,
  Explosion = 9,
  Spark = 10,
  Smoke = 11,
  Corpse = 12,
  Clock = 13,
  Taxi = 14,
  Orphan = 15
};

enum class EActiveWorldSaveError : std::uint32_t {
  None = 0,
  InvalidArgument,
  InvalidMetadata,
  InvalidOrdering,
  DuplicateSection,
  LimitExceeded,
  Truncated,
  InvalidMagic,
  UnsupportedFormat,
  UnsupportedEngineCompatibility,
  IntegrityMismatch,
  InvalidSection,
  InvalidEvent,
  IoOpenFailed,
  IoWriteFailed,
  IoFlushFailed,
  IoReadFailed,
  AtomicCommitFailed,
  RestoreBeginFailed,
  RestoreOwnerFailed,
  RestoreReferenceFailed,
  RestoreEventFailed,
  RestoreValidationFailed,
  RestoreCommitFailed
};

struct SActiveWorldSaveStatus {
  EActiveWorldSaveError error;
  std::size_t offset;
  std::string detail;

  SActiveWorldSaveStatus();
};

struct SActiveWorldSection {
  EActiveWorldSectionKind kind;
  std::uint32_t schemaVersion;
  std::string owner;
  std::vector<std::uint8_t> payload;
};

struct SActiveWorldEvent {
  std::uint32_t sequence;
  std::uint64_t tick;
  double timeStamp;
  std::int32_t label;
  std::string source;
  std::string destination;
  std::uint32_t payloadVersion;
  std::vector<std::uint8_t> payload;
};

struct SActiveWorldSnapshot {
  std::uint32_t engineCompatibility;
  std::uint64_t contentFingerprint;
  std::uint64_t worldFingerprint;
  std::uint64_t simulationTick;
  double simulationTime;
  std::uint32_t rngAlgorithm;
  std::vector<std::uint8_t> rngState;
  std::string level;
  std::vector<std::string> mods;
  std::vector<SActiveWorldSection> sections;
  std::vector<SActiveWorldEvent> events;

  SActiveWorldSnapshot();
};

std::uint32_t ActiveWorldSave_FormatVersion();
std::uint32_t ActiveWorldSave_EngineCompatibilityVersion();

std::uint64_t ActiveWorldSave_ComputeWorldFingerprint(
    const SActiveWorldSnapshot& snapshot);

bool ActiveWorldSave_Encode(const SActiveWorldSnapshot& snapshot,
                            std::vector<std::uint8_t>* bytes,
                            SActiveWorldSaveStatus* status);
bool ActiveWorldSave_Decode(const std::vector<std::uint8_t>& bytes,
                            SActiveWorldSnapshot* snapshot,
                            SActiveWorldSaveStatus* status);

bool ActiveWorldSave_WriteAtomic(const std::wstring& path,
                                 const SActiveWorldSnapshot& snapshot,
                                 SActiveWorldSaveStatus* status);
bool ActiveWorldSave_Read(const std::wstring& path,
                          SActiveWorldSnapshot* snapshot,
                          SActiveWorldSaveStatus* status);

class IActiveWorldRestoreTarget {
 public:
  virtual ~IActiveWorldRestoreTarget() {}

  virtual bool Begin(const SActiveWorldSnapshot& snapshot,
                     std::string* failure) = 0;
  virtual bool RestoreOwner(const SActiveWorldSection& section,
                            std::string* failure) = 0;
  virtual bool ResolveReferences(const SActiveWorldSection& section,
                                 std::string* failure) = 0;
  virtual bool RestoreEvent(const SActiveWorldEvent& event,
                            std::string* failure) = 0;
  virtual bool Validate(std::uint64_t expectedWorldFingerprint,
                        std::string* failure) = 0;
  virtual bool Commit(std::string* failure) = 0;
  virtual void Rollback() = 0;
};

bool ActiveWorldSave_RestoreTransactional(
    const SActiveWorldSnapshot& snapshot, IActiveWorldRestoreTarget* target,
    SActiveWorldSaveStatus* status);

#endif
