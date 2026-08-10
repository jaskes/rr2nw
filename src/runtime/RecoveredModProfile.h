#pragma once

#include "RecoveredModRuntime.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ERecoveredModProfileError : std::uint32_t {
  None = 0,
  InvalidArgument,
  FileUnavailable,
  EmptyInput,
  InputTooLarge,
  EmbeddedNul,
  InvalidHeader,
  UnsupportedVersion,
  Malformed,
  InvalidName,
  InvalidPackageId,
  DuplicateProfile,
  DuplicatePackage,
  MissingActiveProfile,
  AtomicCommitFailed
};

struct SRecoveredModProfileStatus {
  ERecoveredModProfileError error = ERecoveredModProfileError::None;
  std::size_t line = 0;
  std::string detail;
};

struct SRecoveredModProfileEntry {
  std::string name;
  std::vector<std::string> selectedIds;
};

struct SRecoveredModProfileCatalog {
  std::uint32_t version = 1;
  std::string activeProfile;
  std::vector<SRecoveredModProfileEntry> profiles;
  std::uint64_t fingerprint = 0;
};

enum ERecoveredModSelectorCandidateState {
  RECOVERED_MOD_SELECTOR_DISABLED = 0,
  RECOVERED_MOD_SELECTOR_SELECTED = 1,
  RECOVERED_MOD_SELECTOR_DEPENDENCY = 2,
  RECOVERED_MOD_SELECTOR_BLOCKED = 3,
  RECOVERED_MOD_SELECTOR_INVALID = 4
};

struct SRecoveredModSelectorCandidate {
  std::string id;
  std::string version;
  ERecoveredModSelectorCandidateState state =
      RECOVERED_MOD_SELECTOR_DISABLED;
  bool explicitlySelected = false;
  bool active = false;
  unsigned int mountIndex = 0;
  unsigned int issue = 0;
  std::string reason;
};

struct SRecoveredModSelectorSnapshot {
  bool configured = false;
  bool safeMode = false;
  bool cliOverride = false;
  bool profileMissing = false;
  bool corruptProfileRecovered = false;
  bool planReady = false;
  bool dirty = false;
  bool restartRequired = false;
  std::size_t activeProfileIndex = 0;
  std::size_t stagedProfileIndex = 0;
  unsigned int candidateCount = 0;
  unsigned int activePackageCount = 0;
  unsigned int invalidCandidateCount = 0;
  unsigned int writes = 0;
  unsigned int blockedCommits = 0;
  std::uint64_t startupFingerprint = 0;
  std::uint64_t stagedFingerprint = 0;
  std::uint64_t catalogFingerprint = 0;
  std::string source;
  std::string activeProfile;
  std::string stagedProfile;
  std::string reason;
  std::string status;
  std::vector<std::string> profileNames;
  std::vector<SRecoveredModSelectorCandidate> candidates;
  std::vector<std::string> mountOrder;
};

constexpr std::size_t kRecoveredModProfileMaximumBytes = 64u * 1024u;
constexpr std::size_t kRecoveredModProfileMaximumProfiles = 16u;
constexpr std::size_t kRecoveredModProfileMaximumSelected = 64u;

bool RecoveredModProfile_Decode(
    const std::vector<std::uint8_t>& bytes,
    SRecoveredModProfileCatalog* catalog,
    SRecoveredModProfileStatus* status);
bool RecoveredModProfile_Encode(
    const SRecoveredModProfileCatalog& catalog,
    std::vector<std::uint8_t>* bytes,
    SRecoveredModProfileStatus* status);
bool RecoveredModProfile_Read(
    const std::wstring& path, SRecoveredModProfileCatalog* catalog,
    SRecoveredModProfileStatus* status);
bool RecoveredModProfile_WriteAtomic(
    const std::wstring& path, const SRecoveredModProfileCatalog& catalog,
    SRecoveredModProfileStatus* status);

// Configures the process-owned selector. Profile mode selects the persisted
// active profile; CLI mode mirrors the already-supported explicit selection
// and is intentionally read-only in the shell. Safe mode admits no user mods.
bool RecoveredModProfile_Configure(
    const std::wstring& profilePath, const char* baseRoot,
    const char* const* candidateDirectories, std::size_t candidateCount,
    std::size_t explicitDirectoryCount, const char* const* requestedIds,
    std::size_t requestedIdCount, bool activateAllCandidates,
    bool profileMode, bool safeMode);
void RecoveredModProfile_Release();
const SRecoveredModSelectorSnapshot* RecoveredModProfile_Snapshot();
bool RecoveredModProfile_StartupSelection(
    std::vector<std::string>* requestedIds, bool* activateAllCandidates);
bool RecoveredModProfile_SelectRelative(int direction);
bool RecoveredModProfile_ToggleCandidate(std::size_t index);
bool RecoveredModProfile_ResetStaged();
bool RecoveredModProfile_CommitStaged();

// Deterministic test-only failure after the complete temporary file is
// flushed but before it can replace the committed profile.
void RecoveredModProfile_FailNextAtomicCommitForTesting();
const char* RecoveredModProfile_ErrorName(ERecoveredModProfileError error);
