#pragma once

class CViewScene;

struct SRecoveredDrawableSceneSummary {
  int bases;
  int namedDeclarations;
  int expectedReferences;
  int resolvedReferences;
  int landPieces;
  int terrainHeightMapReady;
  int bushRendererReady;
};

enum ERecoveredDrawableSceneIssue {
  RECOVERED_DRAWABLE_SCENE_MISSING_LEVEL = 1u << 0,
  RECOVERED_DRAWABLE_SCENE_MISSING_ASSETS = 1u << 1,
  RECOVERED_DRAWABLE_SCENE_MISSING_SCENE = 1u << 2,
  RECOVERED_DRAWABLE_SCENE_DECODE_FAILURE = 1u << 3,
  RECOVERED_DRAWABLE_SCENE_CONFIGURATION_FAILURE = 1u << 4,
  RECOVERED_DRAWABLE_SCENE_PUBLICATION_FAILURE = 1u << 5,
  RECOVERED_DRAWABLE_SCENE_ALLOCATION_FAILURE = 1u << 6,
  RECOVERED_DRAWABLE_SCENE_FORCED_ROLLBACK = 1u << 7
};

enum ERecoveredDrawableSceneFailurePoint {
  RECOVERED_DRAWABLE_SCENE_FAIL_NONE = 0,
  RECOVERED_DRAWABLE_SCENE_FAIL_AFTER_DECODE = 1,
  RECOVERED_DRAWABLE_SCENE_FAIL_BEFORE_PUBLICATION = 2
};

int RecoveredDrawableScene_Initialize();
void RecoveredDrawableScene_Release();
bool RecoveredDrawableScene_IsReady();
unsigned int RecoveredDrawableScene_Issues();
CViewScene* RecoveredDrawableScene_Get();
const SRecoveredDrawableSceneSummary* RecoveredDrawableScene_Summary();
void RecoveredDrawableScene_SetFailurePointForTesting(int failurePoint);
