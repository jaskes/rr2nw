#include <cstdio>
#include <cstdlib>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "storage/h/subject.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "smoke-subject-state-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool RunCycle(unsigned long long* expectedFingerprint) {
  Session::m_moment = 0.0;
  SimulationContext context(64, 128);
  g_arena.openSeance(&context, 8192.0, 8192.0);
  const ct_ClassTableID table = g_arena.addClassTable("Smoke", 300);
  const bool constructed =
      table != ct_NULLID && SmokeSubjectState_TableReady(&context, 300) &&
      !SmokeSubjectState_TableReady(&context, 299) &&
      SmokeSubjectState_Capacity() == 300 &&
      SmokeSubjectState_LiveCount() == 0;
  const bool lifecycle = SmokeSubjectState_ProbeLifecycle(&context) &&
                         SmokeSubjectState_ProbeLifecycle(&context) &&
                         SmokeSubjectState_LiveCount() == 0;
  const unsigned long long fingerprint =
      SmokeSubjectState_Fingerprint(&context);
  const bool stable =
      fingerprint != 0 &&
      (*expectedFingerprint == 0 || *expectedFingerprint == fingerprint);
  *expectedFingerprint = fingerprint;
  g_arena.closeSeance();
  return constructed && lifecycle && stable &&
         SmokeSubjectState_Capacity() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         SmokeSubjectState_Fingerprint(&context) == 0 &&
         !context.isExist("Storage");
}

}  // namespace

int main() {
  SmokeSubjectState_Link();
  unsigned long long fingerprint = 0;
  if (!RunCycle(&fingerprint) || !RunCycle(&fingerprint)) {
    return Fail("Smoke create/remove reconstruction failed");
  }
  std::printf("smoke subject table=Smoke capacity=300 "
              "lifecycle=create-remove-twice fingerprint=%llu\n",
              fingerprint);
  return EXIT_SUCCESS;
}
