#pragma once

class CFixedColorFont;

struct SRecoveredSceneHeader {
  int namedBases;
  int directBases;
  int totalReferences;
  int reductions;
  double cellSize;
  double heightRatio;
};

enum ERecoveredLevelAssetIssue {
  RECOVERED_LEVEL_ASSET_MISSING_LEVEL = 1u << 0,
  RECOVERED_LEVEL_ASSET_BSP_CHECK_REQUESTED = 1u << 1,
  RECOVERED_LEVEL_ASSET_MISSING_SCENE = 1u << 2,
  RECOVERED_LEVEL_ASSET_INVALID_SCENE = 1u << 3,
  RECOVERED_LEVEL_ASSET_MISSING_PALETTE = 1u << 4,
  RECOVERED_LEVEL_ASSET_INVALID_PALETTE = 1u << 5,
  RECOVERED_LEVEL_ASSET_MISSING_FONT = 1u << 6,
  RECOVERED_LEVEL_ASSET_INVALID_FONT = 1u << 7,
  RECOVERED_LEVEL_ASSET_GRAPH_FAILURE = 1u << 8,
  RECOVERED_LEVEL_ASSET_FIGURE_LIBRARY_FAILURE = 1u << 9,
  RECOVERED_LEVEL_ASSET_ALLOCATION_FAILURE = 1u << 10
};

extern CFixedColorFont font5;

int RecoveredLevelAssets_Initialize();
void RecoveredLevelAssets_Release();
bool RecoveredLevelAssets_IsReady();
unsigned int RecoveredLevelAssets_Issues();
const SRecoveredSceneHeader* RecoveredLevelAssets_SceneHeader();
