#pragma once

class CConfigFile;

struct SRecoveredLevelSettings {
  int hazeMin;
  int hazeDistance;
  int hazeMinWater;
  int hazeDistanceWater;
  int frameInfo;
  int loadTextures;
  int randomPosition;
  int fogMode;
  int bspCheck;
  int debugLog;
  double nearClip;
  double waterline;
  double focus;
  double terrainReduction;
};

enum ERecoveredLevelRuntimeIssue {
  RECOVERED_LEVEL_MISSING_GRAPH = 1u << 0,
  RECOVERED_LEVEL_INVALID_DIRECTORY = 1u << 1,
  RECOVERED_LEVEL_PATH_FAILURE = 1u << 2,
  RECOVERED_LEVEL_MISSING_CONFIG = 1u << 3,
  RECOVERED_LEVEL_INVALID_CONFIG = 1u << 4,
  RECOVERED_LEVEL_MISSING_SCENE = 1u << 5
};

int RecoveredLevelRuntime_Prepare(const char* directory);
void RecoveredLevelRuntime_Release();
bool RecoveredLevelRuntime_IsPrepared();
unsigned int RecoveredLevelRuntime_Issues();
CConfigFile* RecoveredLevelRuntime_Config();
const SRecoveredLevelSettings* RecoveredLevelRuntime_Settings();
const char* RecoveredLevelRuntime_Directory();
const char* RecoveredLevelRuntime_SceneFile();
