#include <cstdio>
#include <cstdlib>

#define LAST_H__SCENE
#include "game.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

namespace {

const unsigned int kExpectedMissingHooks =
    GAME_ENTRY_MISSING_BEGIN_LOOP | GAME_ENTRY_MISSING_PIN_INIT |
    GAME_ENTRY_MISSING_SUA_INIT | GAME_ENTRY_MISSING_DEBUG_MAP_DRAW |
    GAME_ENTRY_MISSING_LEVEL_EVENT;

int Fail(const char* message) {
  std::fprintf(
      stderr,
      "game-level-runtime-smoke: %s (composition=%u level=%u assets=%u "
      "scene=%u entry=%u missing=%u ready=%d graph=%d levelReady=%d "
      "assetsReady=%d sceneReady=%d zav=%p current=%p building=%d "
      "bush=%d)\n",
      message, RecoveredGameLevel_Issues(),
      RecoveredLevelRuntime_Issues(), RecoveredLevelAssets_Issues(),
      RecoveredDrawableScene_Issues(), GameEntry_RuntimeIssues(),
      GameEntry_RuntimeMissingHooks(), RecoveredGameLevel_IsReady() ? 1 : 0,
      RecoveredSoftwareGraph_IsReady() ? 1 : 0,
      RecoveredLevelRuntime_IsPrepared() ? 1 : 0,
      RecoveredLevelAssets_IsReady() ? 1 : 0,
      RecoveredDrawableScene_IsReady() ? 1 : 0,
      static_cast<void*>(pScene), static_cast<void*>(CViewScene::Current()),
      CViewScene::IsBuilding() ? 1 : 0, bush_IsInitialized() ? 1 : 0);
  return EXIT_FAILURE;
}

bool IsLevelRolledBack() {
  return !RecoveredGameLevel_IsReady() &&
         !RecoveredLevelRuntime_IsPrepared() &&
         !RecoveredLevelAssets_IsReady() &&
         !RecoveredDrawableScene_IsReady() && pScene == nullptr &&
         CViewScene::Current() == nullptr && !CViewScene::IsBuilding() &&
         CViewOrdered::GetCurrentTop() == nullptr && !bush_IsInitialized();
}

bool ExpectSceneRollback(const char* directory, int failurePoint) {
  RecoveredDrawableScene_SetFailurePointForTesting(failurePoint);
  return ZAV_InitLevel(directory) == FALSE && IsLevelRolledBack() &&
         RecoveredGameLevel_Issues() ==
             RECOVERED_GAME_LEVEL_SCENE_FAILURE &&
         RecoveredDrawableScene_Issues() ==
             RECOVERED_DRAWABLE_SCENE_FORCED_ROLLBACK;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }

  RecoveredGameLevel_UseRuntime();
  if (GameEntry_RuntimeMissingHooks() != kExpectedMissingHooks ||
      !GameEntry_BoundedStartupEnabled()) {
    return Fail("public hook inventory is incorrect");
  }
  if (!ZAV_InitGraph(nullptr)) {
    return Fail("public graph initialization failed");
  }
  if (GameEntry_RuntimeIssues() != 0) {
    ZAV_Deinit();
    return Fail("graph initialization reported unrelated missing hooks");
  }

  if (argc == 1) {
    if (ZAV_InitLevel("rr2nw-missing-level-fixture") != FALSE ||
        !IsLevelRolledBack() ||
        RecoveredGameLevel_Issues() !=
            RECOVERED_GAME_LEVEL_PREPARE_FAILURE) {
      ZAV_Deinit();
      return Fail("missing-level rollback contract failed");
    }
    ZAV_Deinit();
    if (RecoveredSoftwareGraph_IsReady()) {
      return Fail("graph survived complete shutdown");
    }
    return EXIT_SUCCESS;
  }

  if (!ExpectSceneRollback(
          argv[1], RECOVERED_DRAWABLE_SCENE_FAIL_AFTER_DECODE) ||
      !ExpectSceneRollback(
          argv[1], RECOVERED_DRAWABLE_SCENE_FAIL_BEFORE_PUBLICATION)) {
    ZAV_Deinit();
    return Fail("public scene rollback contract failed");
  }

  RecoveredDrawableScene_SetFailurePointForTesting(
      RECOVERED_DRAWABLE_SCENE_FAIL_NONE);
  if (!ZAV_InitLevel(argv[1]) || !RecoveredGameLevel_IsReady() ||
      ZAV_Scene() == nullptr ||
      RecoveredDrawableScene_Summary() == nullptr) {
    ZAV_Deinit();
    return Fail("public level initialization failed");
  }

  const SRecoveredDrawableSceneSummary result =
      *RecoveredDrawableScene_Summary();
  ZAV_DeInitLevel();
  ZAV_DeInitLevel();
  if (!IsLevelRolledBack() || !RecoveredSoftwareGraph_IsReady()) {
    ZAV_Deinit();
    return Fail("public level deinitialization was not idempotent");
  }

  if (!ZAV_InitLevel(argv[1]) || !RecoveredGameLevel_IsReady()) {
    ZAV_Deinit();
    return Fail("public level reconstruction failed");
  }
  ZAV_Deinit();
  if (!IsLevelRolledBack() || RecoveredSoftwareGraph_IsReady()) {
    return Fail("complete shutdown did not release the runtime");
  }

  std::printf("public level bases=%d names=%d refs=%d land=%d terrain=%d "
              "bush=%d\n",
              result.bases, result.namedDeclarations,
              result.resolvedReferences, result.landPieces,
              result.terrainHeightMapReady, result.bushRendererReady);
  return EXIT_SUCCESS;
}
