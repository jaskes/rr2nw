#include <cstdio>
#include <cstdlib>
#include <cstring>

#define LAST_H__SCENE
#include "game.h"
#include "dmap.h"
#include "graph.h"
#include "hardware.h"
#include "h/super.h"
#include "h/vehicle.h"
#include "kernel/h/session.h"
#include "message/fountmsg.h"
#include "message/hardmsg.h"
#include "message/levelmsg.h"
#include "message/smokermsg.h"
#include "obase/corpse/CorpseAttributeState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/lamp/LampAttributeState.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/sound/WAVResourceState.h"

#include "FrameRuntimeState.h"
#include "GameEntryRuntimeState.h"
#include "RecoveredArenaSeanceRuntime.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredGameServicesRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredRetailScriptManifest.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

namespace {

void (*g_originalAlphaSprite)(SGRAlphaSprite*) = nullptr;
GR_HTEXTURE g_expectedSmokeTexture = nullptr;
int g_smokeSpriteDraws = 0;
bool g_smokeSpriteDrawValid = true;

void CaptureSmokeAlphaSprite(SGRAlphaSprite* sprite) {
  if (sprite != nullptr && sprite->hTexture == g_expectedSmokeTexture) {
    ++g_smokeSpriteDraws;
    g_smokeSpriteDrawValid =
        g_smokeSpriteDrawValid && sprite->x1 > sprite->x0 &&
        sprite->y1 > sprite->y0 && sprite->opacity > 0 &&
        sprite->opacity <= 255 && sprite->iz > 0;
  }
  if (g_originalAlphaSprite != nullptr) g_originalAlphaSprite(sprite);
}

class ScopedSmokeAlphaSpriteCapture {
 public:
  explicit ScopedSmokeAlphaSpriteCapture(GR_HTEXTURE expected) {
    g_expectedSmokeTexture = expected;
    g_smokeSpriteDraws = 0;
    g_smokeSpriteDrawValid = true;
    g_originalAlphaSprite = _pGRDrawAlphaSprite;
    _pGRDrawAlphaSprite = CaptureSmokeAlphaSprite;
  }

  ~ScopedSmokeAlphaSpriteCapture() {
    _pGRDrawAlphaSprite = g_originalAlphaSprite;
    g_originalAlphaSprite = nullptr;
    g_expectedSmokeTexture = nullptr;
  }
};

int Fail(const char* message) {
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  std::fprintf(
      stderr,
      "game-services-runtime-smoke: %s (services=%u entry=%u missing=%u "
      "platform=%d session=%d loop=%d hardware=%d seance=%d bird=%d "
      "portal=%d orphan=%d artefact=%d smoke=%d explosion=%d smoker=%d "
      "dyn_smoker=%d smoker_emission=%d "
      "farter=%d lamp=%d corpse=%d wav=%d skin=%d spark=%d "
      "route=%d vehicle=%d "
      "arena_issues=%llu arena_error=%s level=%d graph=%d "
      "frame=%u context=%p publisher=%p timer=%p scene=%p current=%p "
      "bush=%d observer_events=%u observer_z=%lg)\n",
      message, RecoveredGameServices_Issues(), GameEntry_RuntimeIssues(),
      GameEntry_RuntimeMissingHooks(),
      RecoveredGameServices_PlatformReady() ? 1 : 0,
      RecoveredGameServices_SessionReady() ? 1 : 0,
      RecoveredGameServices_LoopReady() ? 1 : 0,
      RecoveredGameServices_HardwareReady() ? 1 : 0,
      RecoveredGameServices_SeanceReady() ? 1 : 0,
      RecoveredGameServices_BirdAttributesReady() ? 1 : 0,
      RecoveredGameServices_PortalReady() ? 1 : 0,
      RecoveredGameServices_OrphanAttributesReady() ? 1 : 0,
      RecoveredGameServices_ArtefactAttributesReady() ? 1 : 0,
      RecoveredGameServices_SmokeAttributesReady() ? 1 : 0,
      RecoveredGameServices_ExplosionAttributesReady() ? 1 : 0,
      RecoveredGameServices_SmokerAttributesReady() ? 1 : 0,
      RecoveredGameServices_DynSmokerReady() ? 1 : 0,
      RecoveredGameServices_SmokerEmissionReady() ? 1 : 0,
      RecoveredGameServices_FarterAttributesReady() ? 1 : 0,
      RecoveredGameServices_LampAttributesReady() ? 1 : 0,
      RecoveredGameServices_CorpseAttributesReady() ? 1 : 0,
      RecoveredGameServices_WavMetadataReady() ? 1 : 0,
      RecoveredGameServices_SkinResourcesReady() ? 1 : 0,
      RecoveredGameServices_SparkAttributesReady() ? 1 : 0,
      RecoveredGameServices_RouteReady() ? 1 : 0,
      RecoveredGameServices_VehicleReady() ? 1 : 0,
      RecoveredArenaSeance_Issues(), RecoveredArenaSeance_LastError(),
      RecoveredGameLevel_IsReady() ? 1 : 0,
      RecoveredSoftwareGraph_IsReady() ? 1 : 0, Frame_RuntimeIssues(),
      static_cast<void*>(g_super.m_context),
      static_cast<void*>(g_super.m_publisher),
      static_cast<void*>(Session::m_realTimer), static_cast<void*>(pScene),
      static_cast<void*>(CViewScene::Current()),
      bush_IsInitialized() ? 1 : 0,
      observer != nullptr ? observer->inputEvents : 0,
      observer != nullptr ? observer->z : 0.0);
  return EXIT_FAILURE;
}

bool IsServiceReleased() {
  return !RecoveredGameServices_PlatformReady() &&
         !RecoveredGameServices_SessionReady() &&
         !RecoveredGameServices_LoopReady() &&
         !RecoveredGameServices_HardwareReady() &&
         !RecoveredGameServices_SeanceReady() &&
         !RecoveredGameServices_BirdAttributesReady() &&
         !RecoveredGameServices_PortalReady() &&
         !RecoveredGameServices_OrphanAttributesReady() &&
         !RecoveredGameServices_ArtefactAttributesReady() &&
         !RecoveredGameServices_SmokeAttributesReady() &&
         !RecoveredGameServices_SmokeSubjectReady() &&
         !RecoveredGameServices_SmokeTerrainReady() &&
         !RecoveredGameServices_SmokeRenderingReady() &&
         !RecoveredGameServices_SmokeVisualResourcesReady() &&
         RecoveredArenaSeance_SmokeSubjectCapacity() == 0 &&
         RecoveredArenaSeance_SmokeSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_SmokeVisualResourceFingerprint() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         !RecoveredGameServices_ExplosionAttributesReady() &&
         !RecoveredGameServices_SmokerAttributesReady() &&
         !RecoveredGameServices_SmokerReferencesReady() &&
         !RecoveredGameServices_SmokerRuntimeReady() &&
         !RecoveredGameServices_SmokerEmissionReady() &&
         RecoveredArenaSeance_SmokerReferenceFingerprint() == 0 &&
         !RecoveredGameServices_FarterAttributesReady() &&
         !RecoveredGameServices_FarterReferencesReady() &&
         !RecoveredGameServices_LampAttributesReady() &&
         !RecoveredGameServices_CorpseAttributesReady() &&
         !RecoveredGameServices_CorpseReferencesReady() &&
         !RecoveredGameServices_CorpseRuntimeReady() &&
         !RecoveredGameServices_DynSmokerReady() &&
         RecoveredArenaSeance_DynSmokerCapacity() == 0 &&
         RecoveredArenaSeance_DynSmokerFingerprint() == 0 &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         !RecoveredGameServices_WavMetadataReady() &&
         !RecoveredGameServices_SkinResourcesReady() &&
         !RecoveredGameServices_SparkAttributesReady() &&
         !RecoveredGameServices_RouteReady() &&
         !RecoveredGameServices_VehicleReady() &&
         !RecoveredArenaSeance_IsOpen() && g_vehicle == nullptr &&
         RecoveredGameServices_ObserverState() == nullptr &&
         g_super.m_context == nullptr && g_super.m_publisher == nullptr &&
         Session::m_realTimer == nullptr && Session::m_hardware == nullptr &&
         g_hardware.getContext() == nullptr;
}

bool IsLevelRolledBack() {
  return !RecoveredGameLevel_IsReady() &&
         !RecoveredLevelRuntime_IsPrepared() &&
         !RecoveredRetailScriptManifest_IsReady() &&
         !RecoveredLevelAssets_IsReady() &&
         !RecoveredDrawableScene_IsReady() && pScene == nullptr &&
         CViewScene::Current() == nullptr && !CViewScene::IsBuilding() &&
         CViewOrdered::GetCurrentTop() == nullptr && !bush_IsInitialized();
}

bool StartServices(const char* directory) {
  if (!ZAV_InitLevel(directory)) return false;
  PIN_InitEverything();
  SUA_InitEverything();
  ZAV_BeginLoop();
  return RecoveredGameServices_IsReady();
}

bool SendHardwareButton(const char* keyName, int buttonDown) {
  if (g_super.m_context == nullptr || g_hardware.getContext() == nullptr) {
    return false;
  }
  const int code = g_hardware.SearchCode(keyName);
  if (code < 0) return false;

  KR_Event event;
  event.source = g_hardware.getObjectID();
  event.destination = g_hardware.getObjectID();
  event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
  event.label = CTRL_HARDWARE_EVENT;
  event.data.open(EDO_WRITE)
      .putInt(CTRL_BUTTONS_MSG)
      .putInt(code)
      .putInt(buttonDown)
      .putInt(FALSE)
      .close();
  g_super.m_context->sendEventNow(event);
  return true;
}

bool ExerciseVisibleSmoke() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_SmokeRenderingReady() ||
      SmokeSubjectState_LiveCount() != 0 ||
      SmokerSubjectState_DynLiveCount() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* attributeName = "Smoke.Attr.Trace";
  KR_ObjectID attributeID = context->searchObject(attributeName);
  AttributeSmoke* attribute = attributeID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoke*>(
            __attrSmokeTable.searchAttribute(attributeID));
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("Smoke");
  static const char kProbeName[] = "Smoke.Rendering.Probe";
  if (attribute == nullptr || attribute->m_cacheImage == nullptr ||
      attribute->m_maxBlob <= 0 || observer == nullptr ||
      table == ct_NULLID || context->isExist(kProbeName)) {
    return false;
  }

  KR_ObjectID object = g_arena.newObject(table, kProbeName);
  if (object.isNUL()) return false;
  KR_Event event;
  event.label = fou_EVCMD_START;
  event.source = g_arena.getObjectID();
  event.destination = object;
  event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
  event.data.open(EDO_WRITE)
      .putObjectID(attributeID)
      .putDouble(observer->x)
      .putDouble(observer->y)
      .putDouble(observer->z - 64.0)
      .close();
  context->sendEventNow(event);
  const bool started = context->isExist(kProbeName) &&
                       SmokeSubjectState_LiveCount() == 1 &&
                       context->removeEvent(fou_EVC_MOVING, object) != 0;
  if (!started) {
    context->removeEvent(fou_EVC_MOVING, object);
    if (context->isExist(kProbeName)) context->removeObject(object);
    return false;
  }

  const dword framesBefore = dwFrames;
  bool firstFrame = false;
  bool secondFrame = false;
  int drawsAfterFirstFrame = 0;
  {
    ScopedSmokeAlphaSpriteCapture capture(attribute->m_cacheImage);
    firstFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterFirstFrame = g_smokeSpriteDraws;
    context->removeObject(object);
    secondFrame = RecoveredGameServices_RunFrame() != FALSE;
  }
  return firstFrame && secondFrame && g_smokeSpriteDrawValid &&
         drawsAfterFirstFrame == attribute->m_maxBlob &&
         g_smokeSpriteDraws == drawsAfterFirstFrame &&
         dwFrames == framesBefore + 2 &&
         !context->isExist(kProbeName) &&
         SmokeSubjectState_LiveCount() == 0 &&
         context->removeEvent(fou_EVC_MOVING, object) == 0;
}

bool ExerciseVisibleSmokerEmission() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_SmokerEmissionReady() ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* smokerAttributeName = "Smoker.Attr.Corpse";
  KR_ObjectID smokerAttributeID =
      context->searchObject(smokerAttributeName);
  AttributeSmoker* smokerAttribute = smokerAttributeID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoker*>(
            __attrSmokerTable.searchAttribute(smokerAttributeID));
  AttributeSmoke* smokeAttribute =
      smokerAttribute == nullptr || smokerAttribute->m_smokeAttrID.isNUL()
          ? nullptr
          : static_cast<AttributeSmoke*>(
                __attrSmokeTable.searchAttribute(
                    smokerAttribute->m_smokeAttrID));
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("DynSmoker");
  static const char kProbeName[] = "DynSmoker.Rendering.Probe";
  static const char kSmokeName[] = "Smok.Static";
  if (smokerAttribute == nullptr || smokeAttribute == nullptr ||
      smokeAttribute->m_cacheImage == nullptr ||
      smokeAttribute->m_maxBlob <= 0 || observer == nullptr ||
      table == ct_NULLID || context->isExist(kProbeName) ||
      context->isExist(kSmokeName)) {
    return false;
  }

  KR_ObjectID smoker = g_arena.newObject(table, kProbeName);
  if (smoker.isNUL()) return false;
  KR_Event event;
  event.label = fou_EVCMD_START;
  event.source = g_arena.getObjectID();
  event.destination = smoker;
  event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
  event.data.open(EDO_WRITE)
      .putObjectID(smokerAttributeID)
      .putDouble(observer->x)
      .putDouble(observer->y)
      .putDouble(observer->z - 64.0)
      .close();
  context->sendEventNow(event);

  const dword framesBefore = dwFrames;
  const bool enteredView = RecoveredGameServices_RunFrame() != FALSE &&
      context->removeEvent(sm_EV_MOVE, smoker) != 0;
  if (enteredView) {
    event.label = sm_EV_MOVE;
    event.source = smoker;
    event.destination = smoker;
    context->sendEventNow(event);
  }
  KR_ObjectID smoke = context->searchObject(kSmokeName);
  const bool emitted = enteredView && !smoke.isNUL() &&
      SmokerSubjectState_DynLiveCount() == 1 &&
      SmokeSubjectState_LiveCount() == 1 &&
      context->removeEvent(sm_EV_MOVE, smoker) != 0 &&
      context->removeEvent(fou_EVC_MOVING, smoke) != 0;
  if (!emitted) {
    context->removeEvent(sm_EV_MOVE, smoker);
    if (context->isExist(smoker)) context->removeObject(smoker);
    if (!smoke.isNUL()) {
      context->removeEvent(fou_EVC_MOVING, smoke);
      if (context->isExist(smoke)) context->removeObject(smoke);
    }
    return false;
  }
  KR_Event pending;
  pending.label = sm_EV_MOVE;
  pending.source = smoker;
  pending.destination = smoker;
  pending.timeStamp = Session::m_moment + 1000.0;
  context->addEvent(pending);
  pending.label = fou_EVC_MOVING;
  pending.source = smoke;
  pending.destination = smoke;
  context->addEvent(pending);

  bool visibleFrame = false;
  bool detachedFrame = false;
  int drawsAfterVisibleFrame = 0;
  {
    ScopedSmokeAlphaSpriteCapture capture(smokeAttribute->m_cacheImage);
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterVisibleFrame = g_smokeSpriteDraws;
    context->removeObject(smoker);
    context->removeObject(smoke);
    detachedFrame = RecoveredGameServices_RunFrame() != FALSE;
  }
  return visibleFrame && detachedFrame && g_smokeSpriteDrawValid &&
         drawsAfterVisibleFrame == smokeAttribute->m_maxBlob &&
         g_smokeSpriteDraws == drawsAfterVisibleFrame &&
         dwFrames == framesBefore + 3 &&
         !context->isExist(kProbeName) &&
         !context->isExist(kSmokeName) &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         context->removeEvent(sm_EV_MOVE, smoker) == 0 &&
         context->removeEvent(sm_EV_REMOVE, smoker) == 0 &&
         context->removeEvent(fou_EVC_MOVING, smoke) == 0;
}

bool ValidateReferenceTransaction(
    unsigned long long smokerReferenceFingerprint,
    unsigned long long farterReferenceFingerprint,
    unsigned long long corpseReferenceFingerprint) {
  KR_ObjectID smokerObjectID =
      g_super.m_context->searchObject("Smoker.Attr.Corpse");
  AttributeSmoker* smoker =
      smokerObjectID.isNUL()
          ? nullptr
          : static_cast<AttributeSmoker*>(
                __attrSmokerTable.searchAttribute(smokerObjectID));
  if (smoker == nullptr || smoker->m_smokeAttrID.isNUL()) return false;
  char smokeName[sizeof(smoker->m_smokeAttrName)] = {};
  std::memcpy(smokeName, smoker->m_smokeAttrName, sizeof(smokeName));
  const KR_ObjectID smokeAttrID = smoker->m_smokeAttrID;
  const ct_ClassTableID smokeTableID = smoker->m_smokeTableID;
  const GR_HTEXTURE coronaHText = smoker->m_coronaHText;
  const unsigned long coronaColor = smoker->m_coronaColor;
  std::strncpy(smoker->m_smokeAttrName, "Smoke.Attr.Missing",
               sizeof(smoker->m_smokeAttrName) - 1);
  smoker->m_smokeAttrName[sizeof(smoker->m_smokeAttrName) - 1] = 0;
  const bool smokerRejected =
      !SmokerAttributeState_ResolveReferences(g_super.m_context) &&
      smoker->m_smokeAttrID == smokeAttrID &&
      smoker->m_smokeTableID == smokeTableID &&
      smoker->m_coronaHText == coronaHText &&
      smoker->m_coronaColor == coronaColor;
  std::memcpy(smoker->m_smokeAttrName, smokeName, sizeof(smokeName));
  if (!smokerRejected ||
      !SmokerAttributeState_ResolveReferences(g_super.m_context) ||
      SmokerAttributeState_ReferenceFingerprint(g_super.m_context) !=
          smokerReferenceFingerprint)
    return false;

  KR_ObjectID corpseID =
      g_super.m_context->searchObject("Corpse.Attr.Default");
  AttributeCorpse* corpse =
      corpseID.isNUL()
          ? nullptr
          : static_cast<AttributeCorpse*>(
                __attrCorpseTable.searchAttribute(corpseID));
  if (corpse == nullptr || !corpse->m_isSmoking) return false;
  char smokerName[sizeof(corpse->m_smokerAttr)] = {};
  std::memcpy(smokerName, corpse->m_smokerAttr, sizeof(smokerName));
  CViewObjectModel* skin = corpse->m_cacheSkin;
  const KR_ObjectID skinID = corpse->m_skinID;
  const KR_ObjectID smokerID = corpse->m_smokerAttrID;
  const KR_ObjectID fireID = corpse->m_fireAttrID;
  const ct_ClassTableID smokerTable = corpse->m_smokerTableID;
  std::strncpy(corpse->m_smokerAttr, "Smoker.Attr.Missing",
               sizeof(corpse->m_smokerAttr) - 1);
  corpse->m_smokerAttr[sizeof(corpse->m_smokerAttr) - 1] = 0;
  const bool corpseRejected =
      !CorpseAttributeState_ResolveReferences(g_super.m_context) &&
      corpse->m_cacheSkin == skin && corpse->m_skinID == skinID &&
      corpse->m_smokerAttrID == smokerID && corpse->m_fireAttrID == fireID &&
      corpse->m_smokerTableID == smokerTable;
  std::memcpy(corpse->m_smokerAttr, smokerName, sizeof(smokerName));
  if (!corpseRejected ||
      !CorpseAttributeState_ResolveReferences(g_super.m_context) ||
      CorpseAttributeState_ReferenceFingerprint(g_super.m_context) !=
          corpseReferenceFingerprint)
    return false;

  if (FarterAttributeState_RosterSize(g_super.m_context) == 0) return true;
  KR_ObjectID farterID =
      g_super.m_context->searchObject("Farter.Attr.Factory");
  AttributeFarter* farter =
      farterID.isNUL()
          ? nullptr
          : static_cast<AttributeFarter*>(
                __attrFarterTable.searchAttribute(farterID));
  if (farter == nullptr || farter->m_wav == nullptr) return false;
  char soundName[sizeof(farter->m_soundName)] = {};
  std::memcpy(soundName, farter->m_soundName, sizeof(soundName));
  WAVObj* wav = farter->m_wav;
  const ct_ClassTableID soundTable = farter->m_ctsndID;
  std::strncpy(farter->m_soundName, "wav.Missing",
               sizeof(farter->m_soundName) - 1);
  farter->m_soundName[sizeof(farter->m_soundName) - 1] = 0;
  const bool farterRejected =
      !FarterAttributeState_ResolveReferences(g_super.m_context) &&
      farter->m_wav == wav && farter->m_ctsndID == soundTable;
  std::memcpy(farter->m_soundName, soundName, sizeof(soundName));
  return farterRejected &&
         FarterAttributeState_ResolveReferences(g_super.m_context) &&
         FarterAttributeState_ReferenceFingerprint(g_super.m_context) ==
             farterReferenceFingerprint;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }

  RecoveredGameServices_UseRuntime();
  if (!GameEntry_RuntimeReady() || GameEntry_RuntimeMissingHooks() != 0 ||
      GameEntry_BoundedStartupEnabled()) {
    return Fail("complete service hook inventory is incorrect");
  }
  if (!ZAV_InitGraph(nullptr)) {
    return Fail("public graph initialization failed");
  }

  if (argc == 1) {
    if (ZAV_InitLevel("rr2nw-missing-service-level") != FALSE ||
        !IsServiceReleased() || !IsLevelRolledBack()) {
      ZAV_Deinit();
      return Fail("missing-level service rollback failed");
    }
    ZAV_Deinit();
    return RecoveredSoftwareGraph_IsReady()
               ? Fail("graph survived complete shutdown")
               : EXIT_SUCCESS;
  }

  if (!StartServices(argv[1])) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("service initialization failed");
  }

  KR_Event wake;
  wake.label = KR_WAKE_UP;
  if (g_super.m_level.receiveEvent(wake) != 1) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("bounded Level event was not dispatched");
  }
  g_debugMap.Draw();
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  KR_ObjectID vehicleID =
      g_super.m_context->searchObject("Vehicle.Default");
  KR_ObjectID birdID = g_super.m_context->searchObject("Bird.Attr.0");
  KR_ObjectID orphanID =
      g_super.m_context->searchObject("Orphan.Attr.Default");
  KR_ObjectID artefactID =
      g_super.m_context->searchObject("Artefact.Attr.0");
  KR_ObjectID sparkID = g_super.m_context->searchObject("Spark.Flash");
  KR_ObjectID smokeID =
      g_super.m_context->searchObject("Smoke.Attr.Small");
  if (!RecoveredGameServices_HardwareReady() ||
      !RecoveredGameServices_SeanceReady() ||
      !RecoveredGameServices_BirdAttributesReady() ||
      !RecoveredGameServices_PortalReady() ||
      !RecoveredGameServices_OrphanAttributesReady() ||
      !RecoveredGameServices_ArtefactAttributesReady() ||
      !RecoveredGameServices_SmokeAttributesReady() ||
      !RecoveredGameServices_SmokeSubjectReady() ||
      !RecoveredGameServices_SmokeTerrainReady() ||
      !RecoveredGameServices_SmokeRenderingReady() ||
      !RecoveredGameServices_SmokeVisualResourcesReady() ||
      !RecoveredGameServices_ExplosionAttributesReady() ||
      !RecoveredGameServices_SmokerAttributesReady() ||
      !RecoveredGameServices_SmokerReferencesReady() ||
      !RecoveredGameServices_SmokerRuntimeReady() ||
      !RecoveredGameServices_SmokerEmissionReady() ||
      !RecoveredGameServices_DynSmokerReady() ||
      !RecoveredGameServices_FarterAttributesReady() ||
      !RecoveredGameServices_LampAttributesReady() ||
      !RecoveredGameServices_CorpseAttributesReady() ||
      !RecoveredGameServices_WavMetadataReady() ||
      !RecoveredGameServices_SkinResourcesReady() ||
      !RecoveredGameServices_SparkAttributesReady() ||
      !RecoveredGameServices_RouteReady() ||
      !RecoveredGameServices_VehicleReady() ||
      RecoveredArenaSeance_Issues() != 0 || birdID.isNUL() ||
      orphanID.isNUL() || artefactID.isNUL() || smokeID.isNUL() ||
      sparkID.isNUL() ||
      vehicleID.isNUL() ||
      g_vehicle == nullptr ||
      g_super.m_context->queryInterface(vehicleID, IVehicleIID) != g_vehicle ||
      observer == nullptr) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail(
        "recovered Hardware, Arena, common attributes, Portal, Spark, Route, "
        "Vehicle or observer was not published");
  }
  if (!SmokeSubjectState_SimulationSupported(
          g_super.m_context, "Smoke.Attr.Trace") ||
      !SmokeSubjectState_ProbeSimulationLifecycle(
          g_super.m_context, "Smoke.Attr.Trace", Session::m_moment) ||
      !SmokeSubjectState_SimulationSupported(
          g_super.m_context, "Smoke.Attr.FireArea") ||
      !SmokeSubjectState_ProbeSimulationLifecycle(
          g_super.m_context, "Smoke.Attr.FireArea", Session::m_moment) ||
      !SmokerSubjectState_EmissionSupported(
          g_super.m_context, "Smoker.Attr.Corpse") ||
      !SmokerSubjectState_ProbeEmissionLifecycle(
          g_super.m_context, "Smoker.Attr.Corpse", Session::m_moment) ||
      !SmokerSubjectState_EmissionSupported(
          g_super.m_context, "Smoker.Attr.FireArea") ||
      !SmokerSubjectState_ProbeEmissionLifecycle(
          g_super.m_context, "Smoker.Attr.FireArea", Session::m_moment) ||
      SmokeSubjectState_LiveCount() != 0 ||
      SmokerSubjectState_DynLiveCount() != 0) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail(
        "retail Smoke and Smoker free/terrain MOVE/emission lifecycle failed");
  }
  const unsigned long long explosionFingerprint =
      ExplosionAttributeState_Fingerprint(g_super.m_context);
  const int explosionRosterSize =
      ExplosionAttributeState_RosterSize(g_super.m_context);
  const unsigned long long smokerFingerprint =
      SmokerAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long smokerReferenceFingerprint =
      SmokerAttributeState_ReferenceFingerprint(g_super.m_context);
  const bool smokerRuntimeReady =
      RecoveredArenaSeance_SmokerRuntimeReady();
  const int smokeSubjectCapacity =
      RecoveredArenaSeance_SmokeSubjectCapacity();
  const unsigned long long smokeSubjectFingerprint =
      RecoveredArenaSeance_SmokeSubjectFingerprint();
  const unsigned long long smokeVisualResourceFingerprint =
      RecoveredArenaSeance_SmokeVisualResourceFingerprint();
  const int smokerRosterSize =
      SmokerAttributeState_RosterSize(g_super.m_context);
  const int smokerCapacity = SmokerAttributeState_Capacity();
  const int dynSmokerCapacity =
      RecoveredArenaSeance_DynSmokerCapacity();
  const unsigned long long dynSmokerFingerprint =
      RecoveredArenaSeance_DynSmokerFingerprint();
  const unsigned long long wavFingerprint =
      WAVResourceState_Fingerprint(g_super.m_context);
  const int wavRosterSize =
      WAVResourceState_RosterSize(g_super.m_context);
  const int wavCapacity = WAVResourceState_Capacity();
  const unsigned long long farterFingerprint =
      FarterAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long farterReferenceFingerprint =
      FarterAttributeState_ReferenceFingerprint(g_super.m_context);
  const bool farterRuntimeReady =
      RecoveredArenaSeance_FarterRuntimeReady();
  const int farterRosterSize =
      FarterAttributeState_RosterSize(g_super.m_context);
  const int farterCapacity = FarterAttributeState_Capacity();
  const unsigned long long lampFingerprint =
      LampAttributeState_Fingerprint(g_super.m_context);
  const int lampRosterSize = LampAttributeState_RosterSize(g_super.m_context);
  const int lampCapacity = LampAttributeState_Capacity();
  const unsigned long long corpseFingerprint =
      CorpseAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long corpseReferenceFingerprint =
      CorpseAttributeState_ReferenceFingerprint(g_super.m_context);
  const bool corpseRuntimeReady =
      RecoveredArenaSeance_CorpseRuntimeReady();
  const int corpseRosterSize =
      CorpseAttributeState_RosterSize(g_super.m_context);
  const int corpseCapacity = CorpseAttributeState_Capacity();
  const int skinModelCount = RecoveredArenaSeance_SkinModelCount();
  const int skinSpriteCount = RecoveredArenaSeance_SkinSpriteCount();
  const unsigned long long skinCatalogFingerprint =
      RecoveredArenaSeance_SkinCatalogFingerprint();
  const unsigned long long skinResourceFingerprint =
      RecoveredArenaSeance_SkinResourceFingerprint();
  if (explosionFingerprint == 0 || explosionRosterSize < 10 ||
      explosionRosterSize > 14 || smokerFingerprint == 0 ||
      smokerRosterSize != 12 || smokerCapacity != 12 ||
      smokerReferenceFingerprint == 0 || !smokerRuntimeReady ||
      smokeSubjectCapacity != 300 || smokeSubjectFingerprint == 0 ||
      smokeVisualResourceFingerprint == 0 ||
      SmokeSubjectState_LiveCount() != 0 ||
      dynSmokerCapacity != 62 || dynSmokerFingerprint == 0 ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      wavFingerprint == 0 || wavRosterSize < 22 || wavRosterSize > 33 ||
      wavCapacity < 30 || wavCapacity > 35 ||
      RecoveredArenaSeance_WavCatalogFingerprint() != wavFingerprint ||
      skinModelCount < 26 ||
      skinModelCount > 52 || skinSpriteCount != 1 ||
      skinCatalogFingerprint == 0 || skinResourceFingerprint == 0 ||
      farterFingerprint == 0 || farterRosterSize < 0 ||
      farterRosterSize > 4 || farterCapacity != 10 ||
      !RecoveredGameServices_FarterReferencesReady() ||
      farterReferenceFingerprint == 0 ||
      lampFingerprint == 0 || lampRosterSize != 12 || lampCapacity != 12 ||
      corpseFingerprint == 0 || corpseRosterSize < 3 ||
      corpseRosterSize > 7 || corpseCapacity < corpseRosterSize ||
      corpseCapacity > 7 ||
      !RecoveredGameServices_CorpseReferencesReady() ||
      corpseReferenceFingerprint == 0 ||
      !corpseRuntimeReady) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("level-aware Arena subject/attribute roster is invalid");
  }
  if (!ValidateReferenceTransaction(smokerReferenceFingerprint,
                                    farterReferenceFingerprint,
                                    corpseReferenceFingerprint)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Smoker/Farter/Corpse reference transaction was not atomic");
  }
  const SRecoveredObserverState observerBefore = *observer;
  if (!SendHardwareButton("W", TRUE)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("synthetic Hardware press failed");
  }
  Sleep(20);
  if (RecoveredGameServices_Issues() != 0 ||
      !RecoveredGameServices_RunFrame() ||
      !SendHardwareButton("W", FALSE) ||
      !RecoveredGameServices_RunFrame() || dwFrames != 2) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("bounded software frames failed");
  }
  observer = RecoveredGameServices_ObserverState();
  if (observer == nullptr || observer->inputEvents < 2 ||
      observer->z >= observerBefore.z) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Hardware actions did not advance the observer camera");
  }
  if (!ExerciseVisibleSmoke()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible Smoke scene/draw/detach rollback failed");
  }
  if (!ExerciseVisibleSmokerEmission()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible DynSmoker MOVE/emission/detach rollback failed");
  }

  KR_Event unsupported;
  unsupported.label = lev_SAVE;
  if (g_super.m_level.receiveEvent(unsupported) != 0 ||
      RecoveredGameServices_Issues() !=
          RECOVERED_GAME_SERVICES_UNSUPPORTED_LEVEL_EVENT) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("unsupported Level event was not diagnosed");
  }

  ZAV_DeInitLevel();
  ZAV_DeInitLevel();
  if (!IsServiceReleased() || !IsLevelRolledBack() ||
      !RecoveredSoftwareGraph_IsReady()) {
    ZAV_Deinit();
    return Fail("public service shutdown was not idempotent");
  }

  if (!StartServices(argv[1]) || RecoveredGameServices_Issues() != 0 ||
      ExplosionAttributeState_Fingerprint(g_super.m_context) !=
          explosionFingerprint ||
      SmokerAttributeState_Fingerprint(g_super.m_context) !=
          smokerFingerprint ||
      SmokerAttributeState_RosterSize(g_super.m_context) !=
          smokerRosterSize ||
      SmokerAttributeState_Capacity() != smokerCapacity ||
      SmokerAttributeState_ReferenceFingerprint(g_super.m_context) !=
          smokerReferenceFingerprint ||
      RecoveredArenaSeance_SmokerRuntimeReady() != smokerRuntimeReady ||
      RecoveredArenaSeance_SmokeSubjectCapacity() != smokeSubjectCapacity ||
      RecoveredArenaSeance_SmokeSubjectFingerprint() !=
          smokeSubjectFingerprint ||
      RecoveredArenaSeance_SmokeVisualResourceFingerprint() !=
          smokeVisualResourceFingerprint ||
      SmokeSubjectState_LiveCount() != 0 ||
      RecoveredArenaSeance_DynSmokerCapacity() != dynSmokerCapacity ||
      RecoveredArenaSeance_DynSmokerFingerprint() !=
          dynSmokerFingerprint ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      WAVResourceState_Fingerprint(g_super.m_context) != wavFingerprint ||
      WAVResourceState_RosterSize(g_super.m_context) != wavRosterSize ||
      WAVResourceState_Capacity() != wavCapacity ||
      FarterAttributeState_Fingerprint(g_super.m_context) !=
          farterFingerprint ||
      FarterAttributeState_RosterSize(g_super.m_context) !=
          farterRosterSize ||
      FarterAttributeState_Capacity() != farterCapacity ||
      FarterAttributeState_ReferenceFingerprint(g_super.m_context) !=
          farterReferenceFingerprint ||
      RecoveredArenaSeance_FarterRuntimeReady() != farterRuntimeReady ||
      LampAttributeState_Fingerprint(g_super.m_context) != lampFingerprint ||
      LampAttributeState_RosterSize(g_super.m_context) != lampRosterSize ||
      LampAttributeState_Capacity() != lampCapacity ||
      CorpseAttributeState_Fingerprint(g_super.m_context) !=
          corpseFingerprint ||
      CorpseAttributeState_RosterSize(g_super.m_context) !=
          corpseRosterSize ||
      CorpseAttributeState_Capacity() != corpseCapacity ||
      CorpseAttributeState_ReferenceFingerprint(g_super.m_context) !=
          corpseReferenceFingerprint ||
      RecoveredArenaSeance_CorpseRuntimeReady() != corpseRuntimeReady ||
      RecoveredArenaSeance_SkinCatalogFingerprint() !=
          skinCatalogFingerprint ||
      RecoveredArenaSeance_SkinResourceFingerprint() !=
          skinResourceFingerprint ||
      !RecoveredGameServices_RunFrame() || dwFrames != 1) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("service reconstruction failed");
  }
  ZAV_DeInitLevel();
  ZAV_Deinit();
  if (!IsServiceReleased() || !IsLevelRolledBack() ||
      RecoveredSoftwareGraph_IsReady()) {
    return Fail("complete service shutdown failed");
  }

  std::printf("bounded services frames=8 hooks=12 hardware=legacy "
               "arena=1 script=bounded common_attrs=3 smoke_attrs=18 "
               "smoke_subject=%d fingerprint=%llu "
               "smoke_simulation=START-MOVE-remove "
               "smoke_terrain=FireArea-directed-snap "
               "smoke_render=scene-alpha-sprite-detach smoke_visual=%llu "
              "explosion_attrs=%d explosion_fingerprint=%llu "
              "smoker_attrs=%d/%d smoker_fingerprint=%llu "
              "smoker_refs=%llu smoker_runtime=%d "
              "dyn_smoker=%d fingerprint=%llu "
              "smoker_emission=visible-MOVE-Smoke-draw-detach "
              "wav_metadata=%d/%d wav_fingerprint=%llu "
              "farter_attrs=%d/%d farter_fingerprint=%llu "
              "farter_refs=%llu farter_runtime=%d "
              "lamp_attrs=%d/%d lamp_fingerprint=%llu "
              "corpse_attrs=%d/%d corpse_fingerprint=%llu portal=table "
              "corpse_refs=%llu corpse_runtime=%d "
              "skin_models=%d skin_sprites=%d skin_catalog=%llu "
              "skin_resources=%llu "
              "route=table vehicle=real observer=1\n",
               smokeSubjectCapacity, smokeSubjectFingerprint,
               smokeVisualResourceFingerprint,
               explosionRosterSize, explosionFingerprint,
              smokerRosterSize, smokerCapacity, smokerFingerprint,
              smokerReferenceFingerprint, smokerRuntimeReady ? 1 : 0,
              dynSmokerCapacity, dynSmokerFingerprint,
              wavRosterSize, wavCapacity, wavFingerprint,
              farterRosterSize, farterCapacity, farterFingerprint,
              farterReferenceFingerprint, farterRuntimeReady ? 1 : 0,
              lampRosterSize, lampCapacity, lampFingerprint,
              corpseRosterSize, corpseCapacity, corpseFingerprint,
              corpseReferenceFingerprint, corpseRuntimeReady ? 1 : 0,
              skinModelCount,
              skinSpriteCount, skinCatalogFingerprint,
              skinResourceFingerprint);
  return EXIT_SUCCESS;
}
