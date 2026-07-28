#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
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
  const ct_ClassTableID smokeAttributeTable =
      g_arena.addClassTable("SmokeAttr", 1);
  KR_ObjectID smokeAttribute =
      g_arena.newObject(smokeAttributeTable, "Smoke.Attr.Probe");
  AttributeSmoke* smoke = smokeAttribute.isNUL()
      ? nullptr
      : static_cast<AttributeSmoke*>(
            __attrSmokeTable.searchAttribute(smokeAttribute));
  if (smoke != nullptr) {
    smoke->m_onLand = 0;
    smoke->m_maxBlob = 2;
    smoke->m_timeIncrement = 0.05;
    smoke->m_maxTimeLife = 2.0;
  }
  const ct_ClassTableID attributeTable =
      g_arena.addClassTable("SmokerAttr", 1);
  KR_ObjectID attribute =
      g_arena.newObject(attributeTable, "Smoker.Attr.Corpse");
  AttributeSmoker* smoker = attribute.isNUL()
      ? nullptr
      : static_cast<AttributeSmoker*>(
            __attrSmokerTable.searchAttribute(attribute));
  if (smoker != nullptr) {
    std::strncpy(smoker->m_smokeAttrName, "Smoke.Attr.Probe",
                 sizeof(smoker->m_smokeAttrName) - 1);
    smoker->m_smokeAttrName[sizeof(smoker->m_smokeAttrName) - 1] = 0;
    smoker->m_onLand = 0;
    smoker->m_useLight = 1;
    smoker->m_lightColor = 7;
    smoker->m_minLightBright = 100.0;
    smoker->m_maxLightBright = 140.0;
    smoker->m_lightRadius = 3.0;
    smoker->m_lightBrightStep = 120.0;
    smoker->m_lightOffset = 1.5;
    smoker->m_useCorona = 1;
    smoker->m_coronaR = 0.003;
    smoker->m_coronaAlpha = 140;
    smoker->m_maxCoronaR = 40.0;
    smoker->m_coronaHText = reinterpret_cast<GR_HTEXTURE>(1);
    smoker->m_coronaColor = 1;
    smoker->m_maxTimeLife = -1;
  }
  const ct_ClassTableID smokeTable =
      g_arena.addClassTable("Smoke", 2);
  const ct_ClassTableID subjectTable =
      g_arena.addClassTable("DynSmoker", 2);
  const bool constructed = attributeTable != ct_NULLID &&
                           smokeAttributeTable != ct_NULLID &&
                           smokeTable != ct_NULLID &&
                           subjectTable != ct_NULLID &&
                           !smokeAttribute.isNUL() && smoke != nullptr &&
                           !attribute.isNUL() &&
                           smoker != nullptr &&
                           SmokeSubjectState_TableReady(&context, 2) &&
                           SmokerAttributeState_ResolveReferences(&context) &&
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
  const bool emission =
      SmokerSubjectState_EmissionSupported(
          &context, "Smoker.Attr.Corpse") &&
      SmokerSubjectState_ProbeEmissionLifecycle(
          &context, "Smoker.Attr.Corpse", 0.1) &&
      SmokerSubjectState_ProbeEmissionLifecycle(
          &context, "Smoker.Attr.Corpse", 1.0) &&
      SmokerSubjectState_DynLiveCount() == 0 &&
      SmokeSubjectState_LiveCount() == 0;
  const bool lightCorona =
      SmokerSubjectState_LightCoronaSupported(
          &context, "Smoker.Attr.Corpse") &&
      SmokerSubjectState_ProbeLightCoronaLifecycle(
          &context, "Smoker.Attr.Corpse", 0.1) &&
      SmokerSubjectState_ProbeLightCoronaLifecycle(
          &context, "Smoker.Attr.Corpse", 1.0) &&
      SmokerSubjectState_DynLiveCount() == 0 &&
      SmokeSubjectState_LiveCount() == 0;
  const unsigned long long fingerprint =
      SmokerSubjectState_DynFingerprint(&context);
  const bool stable =
      fingerprint != 0 &&
      (*expectedFingerprint == 0 || *expectedFingerprint == fingerprint);
  *expectedFingerprint = fingerprint;
  g_arena.closeSeance();
  return constructed && missingRejected && lifecycle && emission &&
         lightCorona && stable &&
         SmokerSubjectState_DynCapacity() == 0 &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         SmokeSubjectState_Capacity() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         SmokerSubjectState_DynFingerprint(&context) == 0 &&
         !context.isExist("Storage");
}

}  // namespace

int main() {
  SmokeAttributeState_Link();
  SmokeSubjectState_Link();
  SmokerAttributeState_Link();
  SmokerSubjectState_Link();
  unsigned long long fingerprint = 0;
  if (!RunCycle(&fingerprint) || !RunCycle(&fingerprint)) {
    return Fail("DynSmoker create/start/remove reconstruction failed");
  }
  std::printf("smoker subject table=DynSmoker capacity=2 "
              "lifecycle=create-start-visible-MOVE-emit-Smoke-light-corona-"
              "remove-twice rollback=events-child-pools-lights fingerprint=%llu\n",
              fingerprint);
  return EXIT_SUCCESS;
}
