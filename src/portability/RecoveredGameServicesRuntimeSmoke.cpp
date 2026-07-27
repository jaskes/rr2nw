#include <cstdio>
#include <cstdlib>

#define LAST_H__SCENE
#include "game.h"
#include "dmap.h"
#include "h/super.h"
#include "kernel/h/session.h"
#include "message/levelmsg.h"

#include "FrameRuntimeState.h"
#include "GameEntryRuntimeState.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredGameServicesRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

namespace {

int Fail(const char* message) {
  std::fprintf(
      stderr,
      "game-services-runtime-smoke: %s (services=%u entry=%u missing=%u "
      "platform=%d session=%d loop=%d level=%d graph=%d frame=%u "
      "context=%p publisher=%p timer=%p scene=%p current=%p bush=%d)\n",
      message, RecoveredGameServices_Issues(), GameEntry_RuntimeIssues(),
      GameEntry_RuntimeMissingHooks(),
      RecoveredGameServices_PlatformReady() ? 1 : 0,
      RecoveredGameServices_SessionReady() ? 1 : 0,
      RecoveredGameServices_LoopReady() ? 1 : 0,
      RecoveredGameLevel_IsReady() ? 1 : 0,
      RecoveredSoftwareGraph_IsReady() ? 1 : 0, Frame_RuntimeIssues(),
      static_cast<void*>(g_super.m_context),
      static_cast<void*>(g_super.m_publisher),
      static_cast<void*>(Session::m_realTimer), static_cast<void*>(pScene),
      static_cast<void*>(CViewScene::Current()),
      bush_IsInitialized() ? 1 : 0);
  return EXIT_FAILURE;
}

bool IsServiceReleased() {
  return !RecoveredGameServices_PlatformReady() &&
         !RecoveredGameServices_SessionReady() &&
         !RecoveredGameServices_LoopReady() && g_super.m_context == nullptr &&
         g_super.m_publisher == nullptr && Session::m_realTimer == nullptr;
}

bool IsLevelRolledBack() {
  return !RecoveredGameLevel_IsReady() &&
         !RecoveredLevelRuntime_IsPrepared() &&
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
  if (RecoveredGameServices_Issues() != 0 ||
      !RecoveredGameServices_RunFrame() ||
      !RecoveredGameServices_RunFrame() || dwFrames != 2) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("bounded software frames failed");
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

  std::printf("bounded services frames=3 hooks=12\n");
  return EXIT_SUCCESS;
}
