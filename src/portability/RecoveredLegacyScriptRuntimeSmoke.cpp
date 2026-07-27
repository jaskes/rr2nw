#include <cstdio>
#include <cstdlib>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "storage/h/subject.h"

#include "RecoveredLegacyScriptHost.h"
#include "RecoveredLegacyScriptRunner.h"

namespace {

int Fail(const char* message,
         const SRecoveredLegacyScriptRunResult* result = nullptr) {
  std::fprintf(stderr, "recovered-legacy-script-runtime-smoke: %s", message);
  if (result != nullptr) {
    std::fprintf(stderr, " (status=%d host=%u error=%s)",
                 static_cast<int>(result->status), result->hostIssues,
                 result->error);
  }
  std::fputc('\n', stderr);
  return EXIT_FAILURE;
}

bool RunCase(const char* source, const char* name,
             ERecoveredLegacyScriptRunStatus expectedStatus,
             unsigned int expectedHostIssues,
             SRecoveredLegacyScriptRunResult* observed) {
  SimulationContext context(16, 32);
  g_arena.openSeance(&context, 64.0, 64.0);
  RecoveredLegacyScriptHost host(&g_arena);
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_BootstrapProfile();
  const bool succeeded = RecoveredLegacyScript_RunMemory(
      source, name, profile, &context, 0.0, &host, observed);
  const bool matched =
      succeeded == (expectedStatus == RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS) &&
      observed->status == expectedStatus &&
      (observed->hostIssues & expectedHostIssues) == expectedHostIssues;
  g_arena.closeSeance();
  return matched && !context.isExist("Storage");
}

}  // namespace

int main() {
  static_assert(sizeof(void*) == 4,
                "recovered script host requires a Win32 process");
  Session::m_moment = 0.0;

  RecoveredLegacyScriptHost detachedHost(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_BootstrapProfile();
  if (RecoveredLegacyScript_RunMemory(nullptr, "invalid", profile, nullptr,
                                      0.0, &detachedHost, &result) ||
      result.status != RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT) {
    return Fail("invalid runner input did not fail closed", &result);
  }

  const char validLfSource[] =
      "func void main()\n"
      "{\n"
      "}\n";
  SimulationContext invalidProfileContext(16, 32);
  SRecoveredLegacyScriptProfile invalidProfile = profile;
  invalidProfile.processCount = 2;
  if (RecoveredLegacyScript_RunMemory(
          validLfSource, "invalid_profile", invalidProfile,
          &invalidProfileContext, 0.0, &detachedHost, &result) ||
      result.status != RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT ||
      invalidProfileContext.isExist("Storage")) {
    return Fail("invalid runner profile did not fail closed", &result);
  }

  if (!RunCase(validLfSource, "valid_lf",
               RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS, 0, &result)) {
    return Fail("LF source was not normalized and executed", &result);
  }

  const char invalidEventSource[] =
      "func void s_CloseEventData(int event) extern;\n"
      "func void main()\n"
      "{\n"
      "  s_CloseEventData(99);\n"
      "}\n";
  if (!RunCase(invalidEventSource, "invalid_event",
               RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
               RECOVERED_LEGACY_SCRIPT_HOST_INVALID_EVENT, &result)) {
    return Fail("invalid event handle was not diagnosed", &result);
  }

  const char exhaustedEventsSource[] =
      "func int s_OpenEventData(int style) extern;\n"
      "func void main()\n"
      "var int e0,e1,e2,e3,e4,e5,e6,e7,e8;\n"
      "{\n"
      "  e0 := s_OpenEventData(1);\n"
      "  e1 := s_OpenEventData(1);\n"
      "  e2 := s_OpenEventData(1);\n"
      "  e3 := s_OpenEventData(1);\n"
      "  e4 := s_OpenEventData(1);\n"
      "  e5 := s_OpenEventData(1);\n"
      "  e6 := s_OpenEventData(1);\n"
      "  e7 := s_OpenEventData(1);\n"
      "  e8 := s_OpenEventData(1);\n"
      "}\n";
  if (!RunCase(exhaustedEventsSource, "exhausted_events",
               RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
               RECOVERED_LEGACY_SCRIPT_HOST_EVENT_POOL_EXHAUSTED, &result)) {
    return Fail("event-pool exhaustion was not diagnosed", &result);
  }

  const char invalidSource[] = "func void main( { }";
  if (!RunCase(invalidSource, "invalid_source",
               RECOVERED_LEGACY_SCRIPT_RUN_COMPILE_FAILURE, 0, &result)) {
    return Fail("compiler longjmp was not contained", &result);
  }

  std::printf("legacy script host bindings=8 lf=normalized "
              "errors=fail-closed rollback=clean\n");
  return EXIT_SUCCESS;
}
