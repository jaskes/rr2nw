#ifndef RR2NW_RECOVERED_LEGACY_SCRIPT_RUNNER_H
#define RR2NW_RECOVERED_LEGACY_SCRIPT_RUNNER_H

class RecoveredLegacyScriptHost;
class SimulationContext;

enum ERecoveredLegacyScriptRunStatus {
  RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS = 0,
  RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT,
  RECOVERED_LEGACY_SCRIPT_RUN_ALLOCATION_FAILURE,
  RECOVERED_LEGACY_SCRIPT_RUN_INITIALIZATION_FAILURE,
  RECOVERED_LEGACY_SCRIPT_RUN_COMPILE_FAILURE,
  RECOVERED_LEGACY_SCRIPT_RUN_PROCESS_FAILURE,
  RECOVERED_LEGACY_SCRIPT_RUN_TIMEOUT,
  RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
  RECOVERED_LEGACY_SCRIPT_RUN_EXCEPTION
};

struct SRecoveredLegacyScriptProfile {
  int compilerWordBufferSize;
  int compilerStringBufferSize;
  int compilerNameCount;
  int compilerTreeBufferSize;
  int compilerCodeStreamSize;
  int compilerLinkInfoSize;
  int compilerProgramNameBufferSize;
  int compilerMaximumProgramCount;
  int processStorageStackSize;
  int processCount;
  int processStackSize;
  int processQuants;
  int maximumVmSlices;
};

struct SRecoveredLegacyScriptRunResult {
  ERecoveredLegacyScriptRunStatus status;
  unsigned int hostIssues;
  char error[256];
};

SRecoveredLegacyScriptProfile RecoveredLegacyScript_BootstrapProfile();

bool RecoveredLegacyScript_RunMemory(
    const char* source, const char* programName,
    const SRecoveredLegacyScriptProfile& profile, SimulationContext* context,
    double startTime, RecoveredLegacyScriptHost* host,
    SRecoveredLegacyScriptRunResult* result);

#endif
