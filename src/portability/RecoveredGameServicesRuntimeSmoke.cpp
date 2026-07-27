#include <cstdio>
#include <cstdlib>

#define LAST_H__SCENE
#include "game.h"
#include "dmap.h"
#include "hardware.h"
#include "h/super.h"
#include "h/vehicle.h"
#include "kernel/h/session.h"
#include "message/hardmsg.h"
#include "message/levelmsg.h"
#include "obase/corpse/CorpseAttributeState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/lamp/LampAttributeState.h"

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

int Fail(const char* message) {
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  std::fprintf(
      stderr,
      "game-services-runtime-smoke: %s (services=%u entry=%u missing=%u "
      "platform=%d session=%d loop=%d hardware=%d seance=%d bird=%d "
      "portal=%d orphan=%d artefact=%d smoke=%d explosion=%d farter=%d "
      "lamp=%d corpse=%d skin=%d spark=%d "
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
      RecoveredGameServices_FarterAttributesReady() ? 1 : 0,
      RecoveredGameServices_LampAttributesReady() ? 1 : 0,
      RecoveredGameServices_CorpseAttributesReady() ? 1 : 0,
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
         !RecoveredGameServices_ExplosionAttributesReady() &&
         !RecoveredGameServices_FarterAttributesReady() &&
         !RecoveredGameServices_LampAttributesReady() &&
         !RecoveredGameServices_CorpseAttributesReady() &&
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
      !RecoveredGameServices_ExplosionAttributesReady() ||
      !RecoveredGameServices_FarterAttributesReady() ||
      !RecoveredGameServices_LampAttributesReady() ||
      !RecoveredGameServices_CorpseAttributesReady() ||
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
  const unsigned long long explosionFingerprint =
      ExplosionAttributeState_Fingerprint(g_super.m_context);
  const int explosionRosterSize =
      ExplosionAttributeState_RosterSize(g_super.m_context);
  const unsigned long long farterFingerprint =
      FarterAttributeState_Fingerprint(g_super.m_context);
  const int farterRosterSize =
      FarterAttributeState_RosterSize(g_super.m_context);
  const int farterCapacity = FarterAttributeState_Capacity();
  const unsigned long long lampFingerprint =
      LampAttributeState_Fingerprint(g_super.m_context);
  const int lampRosterSize = LampAttributeState_RosterSize(g_super.m_context);
  const int lampCapacity = LampAttributeState_Capacity();
  const unsigned long long corpseFingerprint =
      CorpseAttributeState_Fingerprint(g_super.m_context);
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
      explosionRosterSize > 14 || skinModelCount < 26 ||
      skinModelCount > 52 || skinSpriteCount != 1 ||
      skinCatalogFingerprint == 0 || skinResourceFingerprint == 0 ||
      farterFingerprint == 0 || farterRosterSize < 0 ||
      farterRosterSize > 4 || farterCapacity != 10 ||
      lampFingerprint == 0 || lampRosterSize != 12 || lampCapacity != 12 ||
      corpseFingerprint == 0 || corpseRosterSize < 3 ||
      corpseRosterSize > 7 || corpseCapacity < corpseRosterSize ||
      corpseCapacity > 7) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("level-aware Explosion attribute roster is invalid");
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
      FarterAttributeState_Fingerprint(g_super.m_context) !=
          farterFingerprint ||
      FarterAttributeState_RosterSize(g_super.m_context) !=
          farterRosterSize ||
      FarterAttributeState_Capacity() != farterCapacity ||
      LampAttributeState_Fingerprint(g_super.m_context) != lampFingerprint ||
      LampAttributeState_RosterSize(g_super.m_context) != lampRosterSize ||
      LampAttributeState_Capacity() != lampCapacity ||
      CorpseAttributeState_Fingerprint(g_super.m_context) !=
          corpseFingerprint ||
      CorpseAttributeState_RosterSize(g_super.m_context) !=
          corpseRosterSize ||
      CorpseAttributeState_Capacity() != corpseCapacity ||
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

  std::printf("bounded services frames=3 hooks=12 hardware=legacy "
              "arena=1 script=bounded common_attrs=3 smoke_attrs=18 "
              "explosion_attrs=%d explosion_fingerprint=%llu "
              "farter_attrs=%d/%d farter_fingerprint=%llu "
              "lamp_attrs=%d/%d lamp_fingerprint=%llu "
              "corpse_attrs=%d/%d corpse_fingerprint=%llu portal=table "
              "skin_models=%d skin_sprites=%d skin_catalog=%llu "
              "skin_resources=%llu "
              "route=table vehicle=real observer=1\n",
              explosionRosterSize, explosionFingerprint,
              farterRosterSize, farterCapacity, farterFingerprint,
              lampRosterSize, lampCapacity, lampFingerprint,
              corpseRosterSize, corpseCapacity, corpseFingerprint,
              skinModelCount,
              skinSpriteCount, skinCatalogFingerprint,
              skinResourceFingerprint);
  return EXIT_SUCCESS;
}
