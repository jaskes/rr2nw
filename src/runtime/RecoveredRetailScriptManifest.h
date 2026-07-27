#pragma once

struct SRecoveredRetailScriptManifestSummary {
  int includeDirectives;
  int fileVisits;
  int uniqueFiles;
  int rootFiles;
  int levelFiles;
  unsigned long long totalBytes;
  unsigned long long contentFingerprint;
};

enum ERecoveredRetailScriptManifestIssue {
  RECOVERED_RETAIL_SCRIPT_INVALID_ARGUMENT = 1u << 0,
  RECOVERED_RETAIL_SCRIPT_INVALID_LEVEL_DIRECTORY = 1u << 1,
  RECOVERED_RETAIL_SCRIPT_PATH_FAILURE = 1u << 2,
  RECOVERED_RETAIL_SCRIPT_MISSING_ENTRY = 1u << 3,
  RECOVERED_RETAIL_SCRIPT_MALFORMED_INCLUDE = 1u << 4,
  RECOVERED_RETAIL_SCRIPT_INCLUDE_PATH_TOO_LONG = 1u << 5,
  RECOVERED_RETAIL_SCRIPT_INCLUDE_OUTSIDE_ROOT = 1u << 6,
  RECOVERED_RETAIL_SCRIPT_MISSING_INCLUDE = 1u << 7,
  RECOVERED_RETAIL_SCRIPT_FILE_TOO_LARGE = 1u << 8,
  RECOVERED_RETAIL_SCRIPT_INCLUDE_CYCLE = 1u << 9,
  RECOVERED_RETAIL_SCRIPT_FILE_LIMIT = 1u << 10,
  RECOVERED_RETAIL_SCRIPT_TOTAL_SIZE_LIMIT = 1u << 11,
  RECOVERED_RETAIL_SCRIPT_READ_FAILURE = 1u << 12,
  RECOVERED_RETAIL_SCRIPT_ALLOCATION_FAILURE = 1u << 13
};

// Builds a read-only manifest using the original compiler's include rule:
// every include is resolved against the selected Level directory. The entry
// point is the sibling LEVEL0.SC in that directory's retail root.
int RecoveredRetailScriptManifest_Preflight(const char* levelDirectory);
void RecoveredRetailScriptManifest_Release();
bool RecoveredRetailScriptManifest_IsReady();
unsigned int RecoveredRetailScriptManifest_Issues();
const char* RecoveredRetailScriptManifest_LastError();
const SRecoveredRetailScriptManifestSummary*
RecoveredRetailScriptManifest_Summary();
const char* RecoveredRetailScriptManifest_File(int index);
