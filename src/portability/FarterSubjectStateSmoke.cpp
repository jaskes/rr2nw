#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/skinmsg.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/farter/FarterSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "storage/h/subject.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "farter-subject-state-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool RunCycle(unsigned long long* expectedFingerprint) {
  Session::m_moment = 0.0;
  SimulationContext context(64, 128);
  g_arena.openSeance(&context, 1024.0, 1024.0);

  const ct_ClassTableID wavTable = g_arena.addClassTable("WAVObj", 2);
  KR_ObjectID wav = g_arena.newObject(wavTable, "wav.Farter.Probe");
  KR_Event load;
  load.label = sk_EV_LOAD;
  load.source = g_arena.getObjectID();
  load.destination = wav;
  load.timeStamp = 0.1;
  load.data.open(EDO_WRITE)
      .putStr("farter-probe.wav")
      .putDouble(1.0)
      .putDouble(2.0)
      .putDouble(20.0)
      .putDouble(30.0)
      .putDouble(0.75)
      .putInt(0)
      .close();
  context.sendEventNow(load);

  const ct_ClassTableID soundTable =
      g_arena.addClassTable("SoundObj", 3);
  const ct_ClassTableID attributeTable =
      g_arena.addClassTable("FarterAttr", 1);
  KR_ObjectID attributeID =
      g_arena.newObject(attributeTable, "Farter.Attr.Probe");
  AttributeFarter* attribute = static_cast<AttributeFarter*>(
      __attrFarterTable.searchAttribute(attributeID));
  if (attribute != nullptr) {
    std::strncpy(attribute->m_soundName, "wav.Farter.Probe",
                 sizeof(attribute->m_soundName) - 1);
    attribute->m_soundName[sizeof(attribute->m_soundName) - 1] = 0;
  }
  const bool references = attribute != nullptr &&
      FarterAttributeState_ResolveReferences(&context) &&
      FarterAttributeState_RuntimeReady(&context);

  const ct_ClassTableID farterTable =
      g_arena.addClassTable("Farter", 3);
  const bool constructed = wavTable != ct_NULLID && !wav.isNUL() &&
      soundTable != ct_NULLID && attributeTable != ct_NULLID &&
      !attributeID.isNUL() && references && farterTable != ct_NULLID &&
      FarterSubjectState_TableReady(&context, 3) &&
      !FarterSubjectState_TableReady(&context, 25) &&
      FarterSubjectState_Capacity() == 3 &&
      FarterSubjectState_LiveCount() == 0 &&
      SoundObjectState_LiveCount() == 0;
  const bool missingRejected =
      !FarterSubjectState_ProbeLifecycle(
          &context, "", 0.1) &&
      FarterSubjectState_LiveCount() == 0 &&
      SoundObjectState_LiveCount() == 0;
  const bool lifecycle =
      FarterSubjectState_ProbeLifecycle(
          &context, "Farter.Attr.Probe", 0.1) &&
      FarterSubjectState_ProbeLifecycle(
          &context, "Farter.Attr.Probe", 1.0) &&
      FarterSubjectState_LiveCount() == 0 &&
      SoundObjectState_LiveCount() == 0;
  const unsigned long long fingerprint =
      FarterSubjectState_Fingerprint(&context);
  const bool stable = fingerprint != 0 &&
      (*expectedFingerprint == 0 || *expectedFingerprint == fingerprint);
  *expectedFingerprint = fingerprint;

  g_arena.closeSeance();
  const bool released = FarterSubjectState_Capacity() == 0 &&
      FarterSubjectState_LiveCount() == 0 &&
      FarterSubjectState_Fingerprint(&context) == 0 &&
      SoundObjectState_Capacity() == 0 &&
      SoundObjectState_LiveCount() == 0 &&
      WAVResourceState_Capacity() == 0 &&
      !context.isExist("Farter.Subject.Probe") &&
      !context.isExist("snd.snd");
  return constructed && missingRejected && lifecycle && stable && released;
}

}  // namespace

int main() {
  WAVResourceState_Link();
  SoundObjectState_Link();
  FarterAttributeState_Link();
  FarterSubjectState_Link();
  unsigned long long fingerprint = 0;
  if (!RunCycle(&fingerprint) || !RunCycle(&fingerprint)) {
    return Fail("Farter audible SoundObj lifecycle reconstruction failed");
  }
  std::printf("farter subject table=Farter capacity=3 audible=1 "
              "lifecycle=START_FARTING-enter-start-exit-end-remove-reuse "
              "child=SoundObj rollback=pool-name fingerprint=%llu\n",
              fingerprint);
  return EXIT_SUCCESS;
}
