#include <cstdio>
#include <cstdlib>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "storage/h/subject.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "smoker-subject-state-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool RunCycle(unsigned long long* expectedFingerprint) {
  Session::m_moment = 0.0;
  SimulationContext context(64, 128);
  g_arena.openSeance(&context, 8192.0, 8192.0);
  const ct_ClassTableID attributeTable =
      g_arena.addClassTable("SmokerAttr", 1);
  KR_ObjectID attribute =
      g_arena.newObject(attributeTable, "Smoker.Attr.Corpse");
  const ct_ClassTableID subjectTable =
      g_arena.addClassTable("DynSmoker", 2);
  const bool constructed = attributeTable != ct_NULLID &&
                           subjectTable != ct_NULLID &&
                           !attribute.isNUL() &&
                           SmokerSubjectState_DynTableReady(&context, 2) &&
                           !SmokerSubjectState_DynTableReady(&context, 62) &&
                           SmokerSubjectState_DynCapacity() == 2 &&
                           SmokerSubjectState_DynLiveCount() == 0;
  const bool missingRejected =
      !SmokerSubjectState_ProbeDynLifecycle(
          &context, "Smoker.Attr.Missing", 0.0) &&
      SmokerSubjectState_DynLiveCount() == 0;
  const bool lifecycle =
      SmokerSubjectState_ProbeDynLifecycle(
          &context, "Smoker.Attr.Corpse", 0.0) &&
      SmokerSubjectState_ProbeDynLifecycle(
          &context, "Smoker.Attr.Corpse", 1.0) &&
      SmokerSubjectState_DynLiveCount() == 0;
  const unsigned long long fingerprint =
      SmokerSubjectState_DynFingerprint(&context);
  const bool stable =
      fingerprint != 0 &&
      (*expectedFingerprint == 0 || *expectedFingerprint == fingerprint);
  *expectedFingerprint = fingerprint;
  g_arena.closeSeance();
  return constructed && missingRejected && lifecycle && stable &&
         SmokerSubjectState_DynCapacity() == 0 &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         SmokerSubjectState_DynFingerprint(&context) == 0 &&
         !context.isExist("Storage");
}

}  // namespace

int main() {
  SmokerAttributeState_Link();
  SmokerSubjectState_Link();
  unsigned long long fingerprint = 0;
  if (!RunCycle(&fingerprint) || !RunCycle(&fingerprint)) {
    return Fail("DynSmoker create/start/remove reconstruction failed");
  }
  std::printf("smoker subject table=DynSmoker capacity=2 "
              "lifecycle=create-start-remove-twice fingerprint=%llu\n",
              fingerprint);
  return EXIT_SUCCESS;
}
