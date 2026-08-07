#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstring>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/skinmsg.h"
#include "message/sndmsg.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "storage/h/subject.h"
#include "sound.h"

namespace {

struct FakeAudioBackend {
  unsigned int admissions = 0;
  unsigned int starts = 0;
  unsigned int oneShots = 0;
  unsigned int loops = 0;
  unsigned int stops = 0;
  unsigned int volumes = 0;
  unsigned int focusChanges = 0;
  unsigned int maintains = 0;
  float volume = 0.0f;
  bool active = true;
};

bool FakeAdmit(void* owner, const char* fileName, int flags) {
  FakeAudioBackend* backend = static_cast<FakeAudioBackend*>(owner);
  if (fileName == nullptr || flags != 0 ||
      std::strcmp(fileName, "..\\SOUND\\soundobj-probe.wav") != 0)
    return false;
  ++backend->admissions;
  return true;
}

bool FakeStart(void* owner, const SSoundStatePlaybackRequest* request,
               SoundStatePlaybackToken* token) {
  FakeAudioBackend* backend = static_cast<FakeAudioBackend*>(owner);
  if (request == nullptr || token == nullptr || request->flags != 0 ||
      (request->playCount != 0 && request->playCount != 1) ||
      std::strcmp(request->fileName,
                  "..\\SOUND\\soundobj-probe.wav") != 0)
    return false;
  ++backend->starts;
  if (request->playCount == 0)
    ++backend->loops;
  else
    ++backend->oneShots;
  *token = 0x5252324e57000000ull + backend->starts;
  return true;
}

void FakeStop(void* owner, SoundStatePlaybackToken token) {
  if (token == 0) return;
  ++static_cast<FakeAudioBackend*>(owner)->stops;
}

void FakeVolume(void* owner, ESoundStateCategory category, float volume) {
  if (category != SOUND_STATE_CATEGORY_EFFECTS) return;
  FakeAudioBackend* backend = static_cast<FakeAudioBackend*>(owner);
  backend->volume = volume;
  ++backend->volumes;
}

void FakeFocus(void* owner, bool active) {
  FakeAudioBackend* backend = static_cast<FakeAudioBackend*>(owner);
  backend->active = active;
  ++backend->focusChanges;
}

void FakeMaintain(void* owner) {
  ++static_cast<FakeAudioBackend*>(owner)->maintains;
}

int Fail(const char* message) {
  std::fprintf(stderr, "sound-object-state-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool RunCycle(unsigned long long* expectedFingerprint,
              FakeAudioBackend* backend) {
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
  WAVObj* loadedWav = nullptr;
  KR_ObjectID oneShot;
  const unsigned int startsBefore = backend->starts;
  const unsigned int stopsBefore = backend->stops;
  const bool maintainedOneShot =
      WAVResourceState_ResolveLoaded(
          &context, "wav.SoundObj.Probe", &loadedWav) &&
      SoundObjectState_StartOneShot(
          &context, g_arena.getObjectID(), soundTable, loadedWav,
          CFVector3(1.0, 2.0, 3.0), 0.1, &oneShot) &&
      backend->starts == startsBefore + 1u &&
      SoundObjectState_RollbackOwned(&context, &oneShot) &&
      backend->stops == stopsBefore + 1u && oneShot.isNUL();
  KR_ObjectID loopRollback =
      g_arena.newObject(soundTable, "SoundObj.Loop.Rollback");
  KR_Event loopEvent;
  loopEvent.label = snd_EV_SET_WAV;
  loopEvent.source = g_arena.getObjectID();
  loopEvent.destination = loopRollback;
  loopEvent.timeStamp = 0.1;
  loopEvent.data.open(EDO_WRITE)
      .put(&loadedWav, sizeof(loadedWav))
      .close();
  context.sendEventNow(loopEvent);
  loopEvent.label = snd_EV_MOVE_TO;
  loopEvent.data.open(EDO_WRITE)
      .putDouble(4.0)
      .putDouble(5.0)
      .putDouble(6.0)
      .close();
  context.sendEventNow(loopEvent);
  loopEvent.label = snd_EV_START;
  loopEvent.data.open(EDO_WRITE).putInt(0).close();
  context.sendEventNow(loopEvent);
  const bool maintainedLoopRollback =
      SoundObjectState_Matches(loopRollback, loadedWav,
                               4.0, 5.0, 6.0, true, true, 0) &&
      SoundObjectState_RollbackOwned(&context, &loopRollback) &&
      loopRollback.isNUL() && backend->starts == startsBefore + 2u &&
      backend->stops == stopsBefore + 2u;
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
  if (!constructed || !missingRejected || !lifecycle ||
      !maintainedOneShot || !maintainedLoopRollback || !stable ||
      !released) {
    std::fprintf(stderr,
                 "sound-object-state-smoke: stage "
                 "constructed=%d missing=%d lifecycle=%d stable=%d "
                 "oneshot=%d loop_rollback=%d released=%d fingerprint=%llu "
                 "wav_table=%d wav_null=%d wav_loaded=%d device_free=%d "
                 "sound_table=%d capacity=%d live=%d\n",
                 constructed ? 1 : 0, missingRejected ? 1 : 0,
                 lifecycle ? 1 : 0, stable ? 1 : 0,
                 maintainedOneShot ? 1 : 0,
                 maintainedLoopRollback ? 1 : 0,
                 released ? 1 : 0, fingerprint,
                 wavTable, wav.isNUL() ? 1 : 0,
                 wavLoaded ? 1 : 0, deviceFree ? 1 : 0,
                 soundTable, capacityBefore, liveBefore);
  }
  return constructed && missingRejected && lifecycle && maintainedOneShot &&
         maintainedLoopRollback && stable && released;
}

}  // namespace

int main() {
  const double originalDistance = snd_distMax;
  const double originalDistanceSquared = snd_distMax2;
  const bool invalidDistanceRejected =
      !SoundState_SetMaximumDistance(0.0) &&
      !SoundState_SetMaximumDistance(-1.0) &&
      !SoundState_SetMaximumDistance(
          std::numeric_limits<double>::infinity()) &&
      snd_distMax == originalDistance &&
      snd_distMax2 == originalDistanceSquared;
  const bool configuredDistance =
      SoundState_SetMaximumDistance(300.0) &&
      snd_distMax == 300.0 && snd_distMax2 == 90000.0;
  snd_distMax = originalDistance;
  snd_distMax2 = originalDistanceSquared;
  if (!invalidDistanceRejected || !configuredDistance) {
    return Fail("device-free maximum distance was not transactional");
  }
  WAVResourceState_Link();
  SoundObjectState_Link();
  FakeAudioBackend backend;
  SSoundStateBackend bridge = {};
  bridge.abiVersion = 1u;
  bridge.owner = &backend;
  bridge.admit = FakeAdmit;
  bridge.start = FakeStart;
  bridge.stop = FakeStop;
  bridge.setCategoryVolume = FakeVolume;
  bridge.setApplicationActive = FakeFocus;
  bridge.maintain = FakeMaintain;
  SoundState_ResetTelemetryForTesting();
  if (!SoundState_ConfigureBackend(&bridge) ||
      !SoundState_SetCategoryVolume(SOUND_STATE_CATEGORY_EFFECTS, 0.4f))
    return Fail("maintained backend bridge configuration failed");
  SoundState_SetApplicationActive(false);
  SoundState_SetApplicationActive(true);
  SoundState_Maintain();
  unsigned long long fingerprint = 0;
  if (!RunCycle(&fingerprint, &backend) ||
      !RunCycle(&fingerprint, &backend)) {
    return Fail("device-free SoundObj lifecycle reconstruction failed");
  }
  SSoundStatePlaybackRequest stream = {
      "..\\SOUND\\stream.wav", 1, 1, 1.0f};
  SSoundStatePlaybackRequest loop = {
      "..\\SOUND\\soundobj-probe.wav", 0, 0, 0.75f};
  SSoundStatePlaybackRequest unsupportedRepeat = {
      "..\\SOUND\\soundobj-probe.wav", 0, 2, 0.75f};
  SoundStatePlaybackToken unsupported = 0;
  SoundStatePlaybackToken loopToken = 0;
  const SSoundStateTelemetry* telemetry = SoundState_Telemetry();
  const bool streamRejected =
      !SoundState_StartPlayback(&stream, &unsupported) && unsupported == 0;
  const bool repeatRejected =
      !SoundState_StartPlayback(&unsupportedRepeat, &unsupported) &&
      unsupported == 0;
  const bool explicitLoop =
      SoundState_StartPlayback(&loop, &loopToken) && loopToken != 0;
  SoundState_StopPlayback(&loopToken);
  const bool bridgeExact = backend.admissions == 2u && backend.starts == 9u &&
      backend.oneShots == 2u && backend.loops == 7u &&
      backend.stops == 9u && backend.volumes == 2u &&
      backend.focusChanges == 2u && backend.maintains == 1u &&
      backend.active && backend.volume == 0.4f && telemetry != nullptr &&
      streamRejected && repeatRejected && explicitLoop && loopToken == 0 &&
      telemetry->oneShotStarts == 2u &&
      telemetry->loopRequests == 7u && telemetry->loopStarts == 7u &&
      telemetry->loopFailures == 0u &&
      telemetry->unsupportedStreamStarts == 1u &&
      telemetry->unsupportedRepeatStarts == 1u;
  SoundState_ClearBackend(&backend);
  if (!bridgeExact || SoundState_BackendConfigured())
    return Fail("maintained one-shot/loop bridge was not exact or fail-closed");
  std::printf("sound distance=300/90000 invalid=transactional "
              "sound object table=SoundObj capacity=3 backend=callback-v1 "
              "lifecycle=invalid-bind-updateSound-move-start-end-reuse-one-shot-loop "
              "rollback=pool-name fingerprint=%llu\n",
              fingerprint);
  return EXIT_SUCCESS;
}
