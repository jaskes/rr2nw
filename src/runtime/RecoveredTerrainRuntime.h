#pragma once

class _CViewTerrain;

enum ERecoveredTerrainIssue {
  RECOVERED_TERRAIN_MISSING_LEVEL = 1u << 0,
  RECOVERED_TERRAIN_MISSING_LEVEL_ASSETS = 1u << 1,
  RECOVERED_TERRAIN_MISSING_RESOURCE = 1u << 2,
  RECOVERED_TERRAIN_INVALID_RESOURCE = 1u << 3,
  RECOVERED_TERRAIN_ALLOCATION_FAILURE = 1u << 4,
  RECOVERED_TERRAIN_CONSTRUCTION_FAILURE = 1u << 5
};

unsigned int RecoveredTerrain_ValidateDirectory(const char* directory);
int RecoveredTerrain_Initialize();
void RecoveredTerrain_Release();
bool RecoveredTerrain_IsReady();
unsigned int RecoveredTerrain_Issues();
_CViewTerrain* RecoveredTerrain_Get();
