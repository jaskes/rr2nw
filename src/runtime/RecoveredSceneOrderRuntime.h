#pragma once

struct SRecoveredSceneHeader;

struct SRecoveredSceneOrderSummary {
  int namedDeclarations;
  int namedSlots;
  int nodes;
  int branchOrders;
  int objectReferences;
  int landPieces;
  int shelterOrders;
  int emptyOrders;
  int mapPrimaries;
  int mapEntries;
  int mapCopySentinels;
  int maximumDepth;
};

enum ERecoveredSceneOrderIssue {
  RECOVERED_SCENE_ORDER_MISSING_DEPENDENCY = 1u << 0,
  RECOVERED_SCENE_ORDER_MISSING_SCENE = 1u << 1,
  RECOVERED_SCENE_ORDER_INVALID_SCENE = 1u << 2,
  RECOVERED_SCENE_ORDER_ALLOCATION_FAILURE = 1u << 3,
  RECOVERED_SCENE_ORDER_DECODE_FAILURE = 1u << 4
};

unsigned int RecoveredSceneOrder_ValidateFile(
    const char* path, const SRecoveredSceneHeader* expectedHeader,
    SRecoveredSceneOrderSummary* summary);
int RecoveredSceneOrder_Initialize();
void RecoveredSceneOrder_Release();
bool RecoveredSceneOrder_IsReady();
unsigned int RecoveredSceneOrder_Issues();
const SRecoveredSceneOrderSummary* RecoveredSceneOrder_Summary();
const char* RecoveredSceneOrder_LastValidationStage();
