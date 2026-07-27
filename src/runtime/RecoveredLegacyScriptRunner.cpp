#include "RecoveredLegacyScriptRunner.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "RecoveredLegacyScriptHost.h"
#include "kernel/h/context.h"
#include "sc.h"

namespace {

struct ScriptAttempt {
  TSuaCript compiler;
  TSetOfProcess processes;
  char* source;
  bool compilerInitialized;
  bool processInitialized;
  int stage;
};

enum EScriptStage {
  SCRIPT_STAGE_ALLOCATION = 0,
  SCRIPT_STAGE_INITIALIZATION,
  SCRIPT_STAGE_COMPILE,
  SCRIPT_STAGE_PROCESS,
  SCRIPT_STAGE_EXECUTION
};

void SetResult(SRecoveredLegacyScriptRunResult* result,
               ERecoveredLegacyScriptRunStatus status, const char* error,
               unsigned int hostIssues = 0) {
  if (result == nullptr) return;
  result->status = status;
  result->hostIssues = hostIssues;
  std::snprintf(result->error, sizeof(result->error), "%s",
                error == nullptr ? "legacy script runner failed" : error);
}

void ResetResult(SRecoveredLegacyScriptRunResult* result) {
  if (result == nullptr) return;
  result->status = RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS;
  result->hostIssues = 0;
  result->error[0] = 0;
}

bool IsValidProfile(const SRecoveredLegacyScriptProfile& profile) {
  return profile.compilerWordBufferSize > 0 &&
         profile.compilerStringBufferSize > 0 &&
         profile.compilerNameCount > 0 &&
         profile.compilerTreeBufferSize > 0 &&
         profile.compilerCodeStreamSize > 0 &&
         profile.compilerLinkInfoSize > 0 &&
         profile.compilerProgramNameBufferSize > 0 &&
         profile.compilerMaximumProgramCount > 0 &&
         profile.processStorageStackSize > 0 && profile.processCount == 1 &&
         profile.processStackSize > 0 && profile.processQuants > 0 &&
         profile.maximumVmSlices > 0;
}

char* NormalizeCrlf(const char* source) {
  const std::size_t sourceLength = std::strlen(source);
  std::size_t normalizedLength = sourceLength;
  for (std::size_t index = 0; index < sourceLength; ++index) {
    if (source[index] == '\n' &&
        (index == 0 || source[index - 1] != '\r')) {
      ++normalizedLength;
    }
  }

  char* normalized =
      static_cast<char*>(std::malloc(normalizedLength + 1));
  if (normalized == nullptr) return nullptr;

  std::size_t output = 0;
  for (std::size_t index = 0; index < sourceLength; ++index) {
    if (source[index] == '\n' &&
        (index == 0 || source[index - 1] != '\r')) {
      normalized[output++] = '\r';
    }
    normalized[output++] = source[index];
  }
  normalized[output] = 0;
  return normalized;
}

void CleanupScriptAttempt(ScriptAttempt* attempt) {
  if (attempt == nullptr) return;
  if (attempt->processInitialized) {
    std::free(attempt->processes.m_stacks);
    std::free(attempt->processes.m_process);
  }
  if (attempt->compilerInitialized) {
    sc_DeleteTSuaCript(&attempt->compiler);
  }
  std::free(attempt->source);
  std::free(attempt);
}

ERecoveredLegacyScriptRunStatus StatusForLongJump(int stage) {
  if (stage <= SCRIPT_STAGE_INITIALIZATION) {
    return RECOVERED_LEGACY_SCRIPT_RUN_INITIALIZATION_FAILURE;
  }
  if (stage == SCRIPT_STAGE_COMPILE) {
    return RECOVERED_LEGACY_SCRIPT_RUN_COMPILE_FAILURE;
  }
  return RECOVERED_LEGACY_SCRIPT_RUN_PROCESS_FAILURE;
}

#ifdef _MSC_VER
// The recovered compiler reports errors through setjmp/longjmp. Everything
// mutable below the registration point lives in this POD heap allocation and
// is released explicitly on both the direct and longjmp paths.
#pragma warning(push)
#pragma warning(disable : 4611)
#endif
bool RunMemory(const char* source, const char* programName,
               const SRecoveredLegacyScriptProfile& profile,
               SimulationContext* context, double startTime,
               RecoveredLegacyScriptHost* host,
               SRecoveredLegacyScriptRunResult* result) {
  ScriptAttempt* attempt =
      static_cast<ScriptAttempt*>(std::calloc(1, sizeof(ScriptAttempt)));
  if (attempt == nullptr) {
    SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_ALLOCATION_FAILURE,
              "could not allocate legacy script runner state");
    return false;
  }

  try {
    attempt->stage = SCRIPT_STAGE_ALLOCATION;
    attempt->source = NormalizeCrlf(source);
    if (attempt->source == nullptr) {
      CleanupScriptAttempt(attempt);
      SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_ALLOCATION_FAILURE,
                "could not allocate normalized legacy script source");
      return false;
    }

    attempt->stage = SCRIPT_STAGE_INITIALIZATION;
    if (!sc_InitTSuaCript(
            &attempt->compiler, profile.compilerWordBufferSize,
            profile.compilerStringBufferSize, profile.compilerNameCount,
            profile.compilerTreeBufferSize, profile.compilerCodeStreamSize,
            profile.compilerLinkInfoSize,
            profile.compilerProgramNameBufferSize,
            profile.compilerMaximumProgramCount)) {
      CleanupScriptAttempt(attempt);
      SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_INITIALIZATION_FAILURE,
                "could not initialize legacy script compiler buffers");
      return false;
    }
    attempt->compilerInitialized = true;

    if (SUACRIPT_REGISTER_ERROR_HANDLE(attempt->compiler)) {
      const int failedStage = attempt->stage;
      const ERecoveredLegacyScriptRunStatus status =
          StatusForLongJump(failedStage);
      char error[256] = {};
      std::snprintf(error, sizeof(error), "%s",
                    attempt->compiler.m_heap.m_error.m_msg);
      CleanupScriptAttempt(attempt);
      SetResult(result, status, error);
      return false;
    }

    attempt->stage = SCRIPT_STAGE_COMPILE;
    sc_InitScannerFromMem(&attempt->compiler, attempt->source);
    sc_Compile(&attempt->compiler, programName,
               RecoveredLegacyScriptHost::Bindings(),
               RecoveredLegacyScriptHost::Constants());
    const int program = sc_ProgrammId(&attempt->compiler, programName);
    if (program < 0 || program >= attempt->compiler.m_programmCnt) {
      CleanupScriptAttempt(attempt);
      SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_COMPILE_FAILURE,
                "legacy script compiler did not publish the program");
      return false;
    }

    attempt->stage = SCRIPT_STAGE_PROCESS;
    if (!sc_InitTSetOfProcess(&attempt->processes,
                              profile.processStorageStackSize,
                              profile.processCount)) {
      CleanupScriptAttempt(attempt);
      SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_PROCESS_FAILURE,
                "could not initialize legacy script process storage");
      return false;
    }
    attempt->processInitialized = true;
    if (!sc_CreateProcess(&attempt->processes, &attempt->compiler, program,
                          profile.processStackSize, profile.processQuants,
                          RecoveredLegacyScriptHost::Constants())) {
      CleanupScriptAttempt(attempt);
      SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_PROCESS_FAILURE,
                "could not create legacy script process");
      return false;
    }

    attempt->stage = SCRIPT_STAGE_EXECUTION;
    context->start(startTime);
    bool completed = false;
    for (int slice = 0; slice < profile.maximumVmSlices; ++slice) {
      if (sc_RunProcess(&attempt->processes, 1, host) != 0) {
        completed = true;
      }
      if (!host->IsHealthy()) {
        const unsigned int hostIssues = host->Issues();
        char error[256] = {};
        std::snprintf(error, sizeof(error), "%s", host->LastError());
        CleanupScriptAttempt(attempt);
        SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE, error,
                  hostIssues);
        return false;
      }
      if (completed) break;
    }

    CleanupScriptAttempt(attempt);
    if (!completed) {
      SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_TIMEOUT,
                "legacy script exceeded its bounded VM slice budget");
      return false;
    }
  } catch (const std::bad_alloc&) {
    CleanupScriptAttempt(attempt);
    SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_ALLOCATION_FAILURE,
              "legacy script execution allocation failed");
    return false;
  } catch (...) {
    CleanupScriptAttempt(attempt);
    SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_EXCEPTION,
              "legacy script execution raised an exception");
    return false;
  }

  SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS, "");
  return true;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace

SRecoveredLegacyScriptProfile RecoveredLegacyScript_BootstrapProfile() {
  const SRecoveredLegacyScriptProfile profile = {
      8 * 1024,   // compilerWordBufferSize
      8 * 1024,   // compilerStringBufferSize
      512,        // compilerNameCount
      32 * 1024,  // compilerTreeBufferSize
      32 * 1024,  // compilerCodeStreamSize
      8 * 1024,   // compilerLinkInfoSize
      256,        // compilerProgramNameBufferSize
      1,          // compilerMaximumProgramCount
      512,        // processStorageStackSize
      1,          // processCount
      512,        // processStackSize
      4096,       // processQuants
      1024        // maximumVmSlices
  };
  return profile;
}

bool RecoveredLegacyScript_RunMemory(
    const char* source, const char* programName,
    const SRecoveredLegacyScriptProfile& profile, SimulationContext* context,
    double startTime, RecoveredLegacyScriptHost* host,
    SRecoveredLegacyScriptRunResult* result) {
  ResetResult(result);
  if (source == nullptr || source[0] == 0 || programName == nullptr ||
      programName[0] == 0 || context == nullptr || host == nullptr ||
      result == nullptr || !IsValidProfile(profile)) {
    SetResult(result, RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT,
              "legacy script runner received invalid input");
    return false;
  }

  host->Reset();
  return RunMemory(source, programName, profile, context, startTime, host,
                   result);
}
