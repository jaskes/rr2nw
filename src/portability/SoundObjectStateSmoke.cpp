#include <cstdio>
#include <cstdlib>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/skinmsg.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "storage/h/subject.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "sound-object-state-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool RunCycle(unsigned long long* expectedFingerprint) {
  Session::m_moment = 0.0;
  SimulationContext context(32, 64);
  g_arena.openSeance(&context, 1024.0, 1024.0);

  const ct_ClassTableID wavTable = g_arena.addClassTable("WAVObj", 2);
  KR_ObjectID wav = g_arena.newObject(wavTable, "wav.SoundObj.Probe");
  KR_Event load;
  load.label = sk_EV_LOAD;
  load.source = g_arena.getObjectID();
  load.destination = wav;
  load.timeStamp = 0.1;
  load.data.open(EDO_WRITE)
      .putStr("soundobj-probe.wav")
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
  const bool wavLoaded = WAVResourceState_AllLoaded(&context);
  const bool deviceFree = SoundObjectState_DeviceFree();
  const bool tableReady = SoundObjectState_TableReady(&context, 3);
  const bool wrongCapacityRejected =
      !SoundObjectState_TableReady(&context, 250);
  const int capacityBefore = SoundObjectState_Capacity();
  const int liveBefore = SoundObjectState_LiveCount();
  const bool constructed = wavTable != ct_NULLID && !wav.isNUL() &&
      soundTable != ct_NULLID && wavLoaded && deviceFree && tableReady &&
      wrongCapacityRejected && capacityBefore == 3 && liveBefore == 0;
  const bool missingRejected =
      !SoundObjectState_ProbeLifecycle(
          &context, "", 0.1) &&
      SoundObjectState_LiveCount() == 0;
  const bool lifecycle =
      SoundObjectState_ProbeLifecycle(
          &context, "wav.SoundObj.Probe", 0.1) &&
      SoundObjectState_ProbeLifecycle(
          &context, "wav.SoundObj.Probe", 1.0) &&
      SoundObjectState_LiveCount() == 0;
  const unsigned long long fingerprint =
      SoundObjectState_Fingerprint(&context);
  const bool stable = fingerprint != 0 &&
      (*expectedFingerprint == 0 || *expectedFingerprint == fingerprint);
  *expectedFingerprint = fingerprint;

  g_arena.closeSeance();
  const bool released =
      SoundObjectState_Capacity() == 0 &&
      SoundObjectState_LiveCount() == 0 &&
      SoundObjectState_Fingerprint(&context) == 0 &&
      WAVResourceState_Capacity() == 0 &&
      !context.isExist("Storage") &&
      !context.isExist("wav.SoundObj.Probe") &&
      !context.isExist("snd.snd");
  if (!constructed || !missingRejected || !lifecycle || !stable ||
      !released) {
    std::fprintf(stderr,
                 "sound-object-state-smoke: stage "
                 "constructed=%d missing=%d lifecycle=%d stable=%d "
                 "released=%d fingerprint=%llu "
                 "wav_table=%d wav_null=%d wav_loaded=%d device_free=%d "
                 "sound_table=%d capacity=%d live=%d\n",
                 constructed ? 1 : 0, missingRejected ? 1 : 0,
                 lifecycle ? 1 : 0, stable ? 1 : 0,
                 released ? 1 : 0, fingerprint,
                 wavTable, wav.isNUL() ? 1 : 0,
                 wavLoaded ? 1 : 0, deviceFree ? 1 : 0,
                 soundTable, capacityBefore, liveBefore);
  }
  return constructed && missingRejected && lifecycle && stable && released;
}

}  // namespace

int main() {
  WAVResourceState_Link();
  SoundObjectState_Link();
  unsigned long long fingerprint = 0;
  if (!RunCycle(&fingerprint) || !RunCycle(&fingerprint)) {
    return Fail("device-free SoundObj lifecycle reconstruction failed");
  }
  std::printf("sound object table=SoundObj capacity=3 backend=device-free "
              "lifecycle=invalid-bind-updateSound-move-start-end-reuse "
              "rollback=pool-name fingerprint=%llu\n",
              fingerprint);
  return EXIT_SUCCESS;
}
