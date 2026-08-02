#include <cstdio>
#include <cstdlib>

#define LAST_H__SCENE
#include "game.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

namespace {

int Fail(const char* message) {
  std::fprintf(
      stderr,
      "drawable-scene-smoke: %s (issues=%u runtime=%p zav=%p current=%p "
      "building=%d top=%p bush=%d assets=%d level=%d)\n",
      message, RecoveredDrawableScene_Issues(),
      static_cast<void*>(RecoveredDrawableScene_Get()),
      static_cast<void*>(pScene), static_cast<void*>(CViewScene::Current()),
      CViewScene::IsBuilding() ? 1 : 0,
      static_cast<void*>(CViewOrdered::GetCurrentTop()),
      bush_IsInitialized() ? 1 : 0,
      RecoveredLevelAssets_IsReady() ? 1 : 0,
      RecoveredLevelRuntime_IsPrepared() ? 1 : 0);
  return EXIT_FAILURE;
}

bool IsRolledBack() {
  return !RecoveredDrawableScene_IsReady() &&
         RecoveredDrawableScene_Get() == nullptr && pScene == nullptr &&
         CViewScene::Current() == nullptr && !CViewScene::IsBuilding() &&
         CViewOrdered::GetCurrentTop() == nullptr && !bush_IsInitialized();
}

bool ExpectForcedRollback(int failurePoint) {
  RecoveredDrawableScene_SetFailurePointForTesting(failurePoint);
  return RecoveredDrawableScene_Initialize() == FALSE && IsRolledBack() &&
         RecoveredDrawableScene_Issues() ==
             RECOVERED_DRAWABLE_SCENE_FORCED_ROLLBACK &&
         RecoveredLevelAssets_IsReady() &&
         RecoveredLevelRuntime_IsPrepared();
}

bool RenderOneFrame(CViewScene& scene) {
  CFMatrix3x4 direction;
  direction.LoadIdentity();
  CViewDynamicList dynamics;
  scene.Draw(direction, dynamics);
  scene.CheckDynamicMap();
  return true;
}

void DumpSceneReferences(CViewScene& scene) {
  if (std::getenv("RR2NW_DUMP_SCENE_REFS") == nullptr) return;
  CNameDecls& declarations = scene.ObjRefNames();
  for (int declarationIndex = 0;
       declarationIndex < declarations.Count(); ++declarationIndex) {
    CNameDecl& declaration = declarations[declarationIndex];
    for (int referenceIndex = 0;
         referenceIndex < declaration.Count(); ++referenceIndex) {
      CViewObjectRef* reference =
          static_cast<CViewObjectRef*>(declaration[referenceIndex]);
      if (reference == nullptr) continue;
      const CFVector3& center = reference->Center();
      std::printf("scene-ref name=%s ordinal=%d center=%.6f,%.6f,%.6f\n",
                  declaration.Name(), referenceIndex,
                  center.x, center.y, center.z);
    }
  }
}

class SmokeLandDynamic : public CViewStickLandDynamic {
 public:
  void Draw() override {}
};

bool ExerciseLandDynamics(CViewScene& scene) {
  const CVector2 viewpoint = scene.GetTerrain()->ViewpointL();
  CVector2 cell;
  bool found = false;
  for (int y = -50; y <= 50 && !found; ++y) {
    for (int x = -50; x <= 50; ++x) {
      cell = CVector2(viewpoint.x + x, viewpoint.y + y);
      CVector2 fitted = cell;
      if (scene.GetTerrain()->FitInTrapezioid(fitted)) {
        cell = fitted;
        found = true;
        break;
      }
    }
  }
  if (!found) return false;

  SmokeLandDynamic first;
  SmokeLandDynamic second;
  first.Cell() = cell;
  second.Cell() = cell;
  CViewDynamicList dynamics;
  dynamics.Load(&first);
  dynamics.Load(&second);
  scene.PromoteDynamic(dynamics, TRUE);
  const bool attached =
      dynamics.First() == nullptr && first.Next() == &second &&
      second.Next() == &first;
  if (attached) {
    scene.RemoveLandDynamic(&first);
    scene.RemoveLandDynamic(&first);
    const bool siblingPreserved = second.Next() == &second;
    scene.RemoveLandDynamic(&second);
    scene.CheckDynamicMap();
    return siblingPreserved;
  } else {
    CViewDynamicList::WasteBox().Clear(FALSE);
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }

  RecoveredDrawableScene_Release();
  RecoveredDrawableScene_Release();
  if (RecoveredDrawableScene_Initialize() != FALSE || !IsRolledBack() ||
      RecoveredDrawableScene_Issues() !=
          RECOVERED_DRAWABLE_SCENE_MISSING_LEVEL) {
    return Fail("dependency contract failed");
  }
  if (argc == 1) return EXIT_SUCCESS;

  if (!RecoveredSoftwareGraph_Initialize(nullptr)) {
    return Fail("software graph initialization failed");
  }
  GameEntry_UseRecoveredRuntime();
  if (!RecoveredLevelRuntime_Prepare(argv[1]) ||
      !RecoveredLevelAssets_Initialize()) {
    ZAV_Deinit();
    return Fail("retail level assets failed");
  }

  if (!ExpectForcedRollback(RECOVERED_DRAWABLE_SCENE_FAIL_AFTER_DECODE) ||
      !ExpectForcedRollback(
          RECOVERED_DRAWABLE_SCENE_FAIL_BEFORE_PUBLICATION)) {
    ZAV_Deinit();
    return Fail("forced rollback contract failed");
  }

  RecoveredDrawableScene_SetFailurePointForTesting(
      RECOVERED_DRAWABLE_SCENE_FAIL_NONE);
  if (!RecoveredDrawableScene_Initialize()) {
    ZAV_Deinit();
    return Fail("real scene initialization failed");
  }

  CViewScene* scene = RecoveredDrawableScene_Get();
  DumpSceneReferences(*scene);
  const SRecoveredDrawableSceneSummary* summary =
      RecoveredDrawableScene_Summary();
  const SRecoveredSceneHeader* header =
      RecoveredLevelAssets_SceneHeader();
  if (scene == nullptr || summary == nullptr || header == nullptr ||
      ZAV_Scene() != scene || CViewScene::Current() != scene ||
      summary->bases != header->namedBases + header->directBases ||
      summary->expectedReferences != header->totalReferences ||
      summary->resolvedReferences != header->totalReferences ||
      summary->landPieces <= 0 ||
      summary->terrainHeightMapReady == 0 ||
      summary->bushRendererReady == 0 || scene->Order() == nullptr ||
      !RenderOneFrame(*scene) || !ExerciseLandDynamics(*scene)) {
    ZAV_Deinit();
    return Fail("real drawable scene contract failed");
  }

  const SRecoveredDrawableSceneSummary result = *summary;
  RecoveredDrawableScene_Release();
  if (!IsRolledBack() || !RecoveredLevelAssets_IsReady() ||
      !RecoveredLevelRuntime_IsPrepared()) {
    ZAV_Deinit();
    return Fail("explicit release did not preserve prepared assets");
  }

  if (!RecoveredDrawableScene_Initialize()) {
    ZAV_Deinit();
    return Fail("second initialization failed");
  }
  ZAV_Deinit();
  if (!IsRolledBack() || RecoveredSoftwareGraph_IsReady() ||
      RecoveredLevelAssets_IsReady() ||
      RecoveredLevelRuntime_IsPrepared()) {
    return Fail("shutdown did not release the complete scene stack");
  }

  std::printf("drawable scene bases=%d names=%d refs=%d land=%d "
              "terrain=%d bush=%d\n",
              result.bases, result.namedDeclarations,
              result.resolvedReferences, result.landPieces,
              result.terrainHeightMapReady,
              result.bushRendererReady);
  return EXIT_SUCCESS;
}
