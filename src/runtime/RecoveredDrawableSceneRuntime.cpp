#include "RecoveredDrawableSceneRuntime.h"

#include <new>
#include <stdexcept>

#define LAST_H__SCENE
#include "game.h"
#include "filesys.h"

#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ViewFigureLibraryState.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

extern int __loadTextures;
extern int __randomPos;
extern void __AdjustPlane(CViewOrdered* order);

namespace {

CViewScene* g_scene = nullptr;
SRecoveredDrawableSceneSummary g_summary = {};
unsigned int g_issues = 0;
int g_failurePoint = RECOVERED_DRAWABLE_SCENE_FAIL_NONE;

void ReleasePublishedScene(CViewScene* scene) {
  if (g_scene == scene) g_scene = nullptr;
  delete scene;
  g_summary = SRecoveredDrawableSceneSummary{};
}

void ConfigureLevelShutdown() {
  ZAV_ConfigureLevelShutdown(ReleasePublishedScene,
                             ViewFigureLibrary_Release);
}

void RestoreAssetOnlyShutdown() {
  ZAV_ConfigureLevelShutdown(nullptr, ViewFigureLibrary_Release);
}

bool ConfigureScene(CViewScene& scene) {
  const SRecoveredLevelSettings* settings =
      RecoveredLevelRuntime_Settings();
  CConfigFile* config = RecoveredLevelRuntime_Config();
  if (settings == nullptr || config == nullptr ||
      RecoveredSoftwareGraph_Width() <= 0 ||
      RecoveredSoftwareGraph_Height() <= 0) {
    return false;
  }

  __loadTextures = settings->loadTextures;
  __randomPos = settings->randomPosition;
  __AdjustPlane(scene.Order());
  scene.GetTerrain()->SetReductionStart(settings->terrainReduction);

  _CViewTerrain::CWaterParams water = {};
  water.m_transWater =
      static_cast<int>(config->GetDouble("Water", "TransWater"));
  water.m_transBump =
      static_cast<int>(config->GetDouble("Water", "TransBump"));
  water.m_xwTimeP = config->GetDouble("Water", "xwTimeP");
  water.m_ywTimeP = config->GetDouble("Water", "ywTimeP");
  water.m_xwTimeLandPx = config->GetDouble("Water", "xwTimeLandPx");
  water.m_xwTimeLandPy = config->GetDouble("Water", "xwTimeLandPy");
  water.m_ywTimeLandPx = config->GetDouble("Water", "ywTimeLandPx");
  water.m_ywTimeLandPy = config->GetDouble("Water", "ywTimeLandPy");
  water.m_xwStaticA =
      static_cast<int>(config->GetDouble("Water", "xwStaticA"));
  water.m_ywStaticA =
      static_cast<int>(config->GetDouble("Water", "ywStaticA"));
  water.m_xwDynamicA =
      static_cast<int>(config->GetDouble("Water", "xwDynamicA"));
  water.m_ywDynamicA =
      static_cast<int>(config->GetDouble("Water", "ywDynamicA"));
  water.m_xbTimeP = config->GetDouble("Water", "xbTimeP");
  water.m_ybTimeP = config->GetDouble("Water", "ybTimeP");
  water.m_xbTimeLandPx = config->GetDouble("Water", "xbTimeLandPx");
  water.m_xbTimeLandPy = config->GetDouble("Water", "xbTimeLandPy");
  water.m_ybTimeLandPx = config->GetDouble("Water", "ybTimeLandPx");
  water.m_ybTimeLandPy = config->GetDouble("Water", "ybTimeLandPy");
  water.m_xbStaticA =
      static_cast<int>(config->GetDouble("Water", "xbStaticA"));
  water.m_ybStaticA =
      static_cast<int>(config->GetDouble("Water", "ybStaticA"));
  water.m_xbDynamicA =
      static_cast<int>(config->GetDouble("Water", "xbDynamicA"));
  water.m_ybDynamicA =
      static_cast<int>(config->GetDouble("Water", "ybDynamicA"));
  water.m_maxPhaseVal =
      static_cast<int>(config->GetDouble("Water", "MaxPhaseVal"));
  scene.GetTerrain()->InitWaterParams(water);
  scene.GetTerrain()->InitWaterTabulation();

  const double aspect = RecoveredSoftwareGraph_Height() * 4.0 / 3.0 /
                        RecoveredSoftwareGraph_Width();
  scene.SetScale(settings->focus, settings->focus * aspect);
  scene.CheckDynamicMap();
  return true;
}

void ResetSummary() {
  g_summary = SRecoveredDrawableSceneSummary{};
}

int Fail(unsigned int issue, CViewScene* staged = nullptr) {
  if (staged != nullptr) delete staged;
  g_issues = issue;
  ResetSummary();
  RestoreAssetOnlyShutdown();
  return FALSE;
}

}  // namespace

int RecoveredDrawableScene_Initialize() {
  RecoveredDrawableScene_Release();
  g_issues = 0;

  if (!RecoveredLevelRuntime_IsPrepared()) {
    return Fail(RECOVERED_DRAWABLE_SCENE_MISSING_LEVEL);
  }
  if (!RecoveredLevelAssets_IsReady()) {
    return Fail(RECOVERED_DRAWABLE_SCENE_MISSING_ASSETS);
  }
  const char* scenePath = RecoveredLevelRuntime_SceneFile();
  const SRecoveredSceneHeader* header =
      RecoveredLevelAssets_SceneHeader();
  if (scenePath == nullptr || header == nullptr) {
    return Fail(RECOVERED_DRAWABLE_SCENE_MISSING_SCENE);
  }

  CViewScene* staged = nullptr;
  try {
    CTaggedFile file(FALSE);
    if (!file.Open(scenePath, FALSE)) {
      return Fail(RECOVERED_DRAWABLE_SCENE_MISSING_SCENE);
    }
    staged = new CViewScene(file, TRUE);
    if (!file.Close(FALSE)) {
      return Fail(RECOVERED_DRAWABLE_SCENE_DECODE_FAILURE, staged);
    }
    if (g_failurePoint == RECOVERED_DRAWABLE_SCENE_FAIL_AFTER_DECODE) {
      return Fail(RECOVERED_DRAWABLE_SCENE_FORCED_ROLLBACK, staged);
    }
    if (!ConfigureScene(*staged)) {
      return Fail(RECOVERED_DRAWABLE_SCENE_CONFIGURATION_FAILURE, staged);
    }
    if (g_failurePoint ==
        RECOVERED_DRAWABLE_SCENE_FAIL_BEFORE_PUBLICATION) {
      return Fail(RECOVERED_DRAWABLE_SCENE_FORCED_ROLLBACK, staged);
    }
    if (staged->ReferenceCount() != header->totalReferences ||
        staged->ResolvedReferenceCount() != header->totalReferences ||
        staged->LandPieceCount() <= 0) {
      return Fail(RECOVERED_DRAWABLE_SCENE_DECODE_FAILURE, staged);
    }
    if (!staged->Commit()) {
      return Fail(RECOVERED_DRAWABLE_SCENE_PUBLICATION_FAILURE, staged);
    }
  } catch (const std::bad_alloc&) {
    return Fail(RECOVERED_DRAWABLE_SCENE_ALLOCATION_FAILURE, staged);
  } catch (...) {
    return Fail(RECOVERED_DRAWABLE_SCENE_DECODE_FAILURE, staged);
  }

  g_scene = staged;
  pScene = staged;
  g_summary.bases = staged->BaseCount();
  g_summary.namedDeclarations = staged->ObjRefNames().Count();
  g_summary.expectedReferences = header->totalReferences;
  g_summary.resolvedReferences = staged->ResolvedReferenceCount();
  g_summary.landPieces = staged->LandPieceCount();
  g_summary.terrainHeightMapReady =
      staged->GetTerrain()->GetHeightMap() != nullptr ? 1 : 0;
  g_summary.bushRendererReady = bush_IsInitialized() ? 1 : 0;
  ConfigureLevelShutdown();
  return TRUE;
}

void RecoveredDrawableScene_Release() {
  CViewScene* const scene = g_scene;
  g_scene = nullptr;
  if (pScene == scene) pScene = nullptr;
  delete scene;
  ResetSummary();
  RestoreAssetOnlyShutdown();
}

bool RecoveredDrawableScene_IsReady() {
  return g_scene != nullptr && pScene == g_scene &&
         CViewScene::Current() == g_scene;
}

unsigned int RecoveredDrawableScene_Issues() { return g_issues; }

CViewScene* RecoveredDrawableScene_Get() { return g_scene; }

const SRecoveredDrawableSceneSummary* RecoveredDrawableScene_Summary() {
  return RecoveredDrawableScene_IsReady() ? &g_summary : nullptr;
}

void RecoveredDrawableScene_SetFailurePointForTesting(int failurePoint) {
  g_failurePoint = failurePoint;
}
