#include <cstdio>
#include <cstdlib>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/fountmsg.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "storage/h/subject.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "smoke-subject-state-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool RejectedStart(SimulationContext* context, KR_ObjectID attribute,
                   const char* objectName) {
  const ct_ClassTableID table = g_arena.searchSeanceClassTable("Smoke");
  KR_ObjectID object = g_arena.newObject(table, objectName);
  if (object.isNUL()) return false;
  KR_Event event;
  event.label = fou_EVCMD_START;
  event.source = g_arena.getObjectID();
  event.destination = object;
  event.timeStamp = 0.1;
  event.data.open(EDO_WRITE)
      .putObjectID(attribute)
      .putDouble(1.0)
      .putDouble(2.0)
      .putDouble(3.0)
      .close();
  context->sendEventNow(event);
  return !context->isExist(objectName) &&
         SmokeSubjectState_LiveCount() == 0 &&
         context->removeEvent(fou_EVC_MOVING, object) == 0;
}

bool RunCycle(unsigned long long* expectedFingerprint) {
  Session::m_moment = 0.0;
  SimulationContext context(64, 128);
  g_arena.openSeance(&context, 8192.0, 8192.0);
  const ct_ClassTableID attributeTable =
      g_arena.addClassTable("SmokeAttr", 1);
  KR_ObjectID attributeID =
      g_arena.newObject(attributeTable, "Smoke.Attr.Probe");
  AttributeSmoke* attribute = attributeID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoke*>(
            __attrSmokeTable.searchAttribute(attributeID));
  if (attribute != nullptr) {
    attribute->m_onLand = 0;
    attribute->m_maxBlob = 2;
    attribute->m_timeIncrement = 0.05;
    attribute->m_maxTimeLife = 2.0;
  }
  const ct_ClassTableID table = g_arena.addClassTable("Smoke", 300);
  const bool constructed =
      attributeTable != ct_NULLID && !attributeID.isNUL() &&
      attribute != nullptr && table != ct_NULLID &&
      SmokeSubjectState_TableReady(&context, 300) &&
      !SmokeSubjectState_TableReady(&context, 299) &&
      SmokeSubjectState_Capacity() == 300 &&
      SmokeSubjectState_LiveCount() == 0;
  const bool lifecycle = SmokeSubjectState_ProbeLifecycle(&context) &&
                         SmokeSubjectState_ProbeLifecycle(&context) &&
                         SmokeSubjectState_LiveCount() == 0;
  const bool simulation =
      SmokeSubjectState_ProbeSimulationLifecycle(
          &context, "Smoke.Attr.Probe", 0.1) &&
      SmokeSubjectState_ProbeSimulationLifecycle(
          &context, "Smoke.Attr.Probe", 1.0) &&
      SmokeSubjectState_LiveCount() == 0;
  const bool missingRejected =
      !SmokeSubjectState_ProbeSimulationLifecycle(
          &context, "Smoke.Attr.Missing", 0.1) &&
      RejectedStart(&context, KR_ObjectID::NUL(),
                    "Smoke.MissingAttr.Probe");
  attribute->m_onLand = 1;
  const bool missingSceneRejected =
      !SmokeSubjectState_ProbeSimulationLifecycle(
          &context, "Smoke.Attr.Probe", 0.1) &&
      RejectedStart(&context, attributeID, "Smoke.Land.Probe");
  attribute->m_onLand = 0;
  attribute->m_maxBlob = 5;
  const bool overflowRejected =
      !SmokeSubjectState_ProbeSimulationLifecycle(
          &context, "Smoke.Attr.Probe", 0.1) &&
      RejectedStart(&context, attributeID, "Smoke.Overflow.Probe");
  attribute->m_maxBlob = 2;
  attribute->m_timeIncrement = 0.002;
  const bool timeStepRejected =
      !SmokeSubjectState_ProbeSimulationLifecycle(
          &context, "Smoke.Attr.Probe", 0.1) &&
      RejectedStart(&context, attributeID, "Smoke.TimeStep.Probe");
  attribute->m_timeIncrement = 0.05;
  const unsigned long long fingerprint =
      SmokeSubjectState_Fingerprint(&context);
  const bool stable =
      fingerprint != 0 &&
      (*expectedFingerprint == 0 || *expectedFingerprint == fingerprint);
  *expectedFingerprint = fingerprint;
  g_arena.closeSeance();
  return constructed && lifecycle && simulation && missingRejected &&
         missingSceneRejected && overflowRejected && timeStepRejected &&
         stable &&
         SmokeSubjectState_Capacity() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         SmokeSubjectState_Fingerprint(&context) == 0 &&
         !context.isExist("Storage");
}

}  // namespace

int main() {
  SmokeAttributeState_Link();
  SmokeSubjectState_Link();
  unsigned long long fingerprint = 0;
  if (!RunCycle(&fingerprint) || !RunCycle(&fingerprint)) {
    return Fail("Smoke START/MOVE/remove reconstruction failed");
  }
  std::printf("smoke subject table=Smoke capacity=300 "
              "lifecycle=START-MOVE-hide-remove-twice "
              "rollback=missing-scene-overflow-timestep fingerprint=%llu\n",
              fingerprint);
  return EXIT_SUCCESS;
}
