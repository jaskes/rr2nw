#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

struct SRecoveredModRuntimeSummary {
  int schemaVersion = 0;
  int engineApi = 0;
  char id[65] = {};
  char version[33] = {};
  unsigned int fileCount = 0;
  unsigned int levelCount = 0;
  std::uint64_t totalBytes = 0;
  std::uint64_t modFingerprint = 0;
  unsigned int resolveCount = 0;
  unsigned int overrideHitCount = 0;
};

struct SRecoveredModLevel {
  char id[65] = {};
  char base[65] = {};
};

enum ERecoveredModRuntimeIssue {
  RECOVERED_MOD_INVALID_ARGUMENT = 1u << 0,
  RECOVERED_MOD_INVALID_BASE_ROOT = 1u << 1,
  RECOVERED_MOD_INVALID_DIRECTORY = 1u << 2,
  RECOVERED_MOD_MISSING_MANIFEST = 1u << 3,
  RECOVERED_MOD_MANIFEST_TOO_LARGE = 1u << 4,
  RECOVERED_MOD_MANIFEST_MALFORMED = 1u << 5,
  RECOVERED_MOD_UNSUPPORTED_SCHEMA = 1u << 6,
  RECOVERED_MOD_UNSUPPORTED_ENGINE = 1u << 7,
  RECOVERED_MOD_INVALID_ID = 1u << 8,
  RECOVERED_MOD_INVALID_VERSION = 1u << 9,
  RECOVERED_MOD_INVALID_FILE_ENTRY = 1u << 10,
  RECOVERED_MOD_PATH_OUTSIDE_ROOT = 1u << 11,
  RECOVERED_MOD_MISSING_SOURCE = 1u << 12,
  RECOVERED_MOD_DUPLICATE_TARGET = 1u << 13,
  RECOVERED_MOD_FILE_TOO_LARGE = 1u << 14,
  RECOVERED_MOD_TOTAL_SIZE_LIMIT = 1u << 15,
  RECOVERED_MOD_ALLOCATION_FAILURE = 1u << 16,
  RECOVERED_MOD_PATH_FAILURE = 1u << 17,
  RECOVERED_MOD_INVALID_LEVEL_ENTRY = 1u << 18,
  RECOVERED_MOD_DUPLICATE_LEVEL = 1u << 19,
  RECOVERED_MOD_MISSING_LEVEL_BASE = 1u << 20,
  RECOVERED_MOD_LEVEL_COLLISION = 1u << 21
};

// Configures one explicit, read-only data-pack overlay. The base-only form is
// still useful because it installs the common resolver without activating a
// mod. Failed configuration never replaces the previously admitted state.
bool RecoveredModRuntime_Configure(const char* baseRoot,
                                   const char* modDirectory);
void RecoveredModRuntime_Release();
bool RecoveredModRuntime_IsConfigured();
bool RecoveredModRuntime_IsActive();
unsigned int RecoveredModRuntime_Issues();
const char* RecoveredModRuntime_LastError();
const SRecoveredModRuntimeSummary* RecoveredModRuntime_Summary();

// Schema-1 derived Levels add catalog identities without modifying game.cfg.
// Each declaration inherits one physical retail Level and may replace files
// through targets rooted at the new identity. Selection is transactional and
// returns the read-only physical base directory used by legacy chdir code.
unsigned int RecoveredModRuntime_LevelCount();
bool RecoveredModRuntime_Level(unsigned int index, SRecoveredModLevel* level);
bool RecoveredModRuntime_ActivateLevel(const char* identity,
                                       char* physicalDirectory,
                                       std::size_t physicalDirectorySize);
const char* RecoveredModRuntime_ActiveLevelIdentity();
const char* RecoveredModRuntime_ActiveLevelBase();
bool RecoveredModRuntime_ActiveLevelIsDerived();

// Reserved engine-owned data contracts may inspect an exact manifest target
// without exposing the physical mod directory. These calls never fall back to
// the base tree: an undeclared target is absent, and a declared target remains
// covered by the admitted mod fingerprint.
bool RecoveredModRuntime_HasOverlayTarget(const char* target);
FILE* RecoveredModRuntime_OpenOverlayTarget(const char* target, long* length);

// Resolves an existing legacy read request. Exact case-insensitive virtual
// targets win over base data; an unmatched path is returned unchanged.
bool RecoveredModRuntime_ResolveReadPath(const char* requested,
                                         char* resolved,
                                         std::size_t resolvedSize);

// CFileResource-compatible read hook. It never opens files for writing.
FILE* RecoveredModRuntime_OpenRead(const char* requested, long* length);

// Active mod identity is folded into save/replay content identity. Base-only
// sessions retain their historical fingerprint exactly.
std::uint64_t RecoveredModRuntime_CombineContentFingerprint(
    std::uint64_t baseFingerprint);
