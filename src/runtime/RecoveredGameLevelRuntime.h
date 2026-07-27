#pragma once

enum ERecoveredGameLevelIssue {
  RECOVERED_GAME_LEVEL_PREPARE_FAILURE = 1u << 0,
  RECOVERED_GAME_LEVEL_ASSET_FAILURE = 1u << 1,
  RECOVERED_GAME_LEVEL_SCENE_FAILURE = 1u << 2,
  RECOVERED_GAME_LEVEL_SCRIPT_MANIFEST_FAILURE = 1u << 3
};

int RecoveredGameLevel_Initialize(const char* directory);
void RecoveredGameLevel_Release();
bool RecoveredGameLevel_IsReady();
unsigned int RecoveredGameLevel_Issues();

// Installs the complete recovered Level transaction behind the public
// ZAV_InitLevel/ZAV_DeInitLevel dispatch boundary.
void RecoveredGameLevel_UseRuntime();
