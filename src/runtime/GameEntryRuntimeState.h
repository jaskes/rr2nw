#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

class CConfigFile;
class KR_Event;

typedef int (*TGameEntryInitGraph)(HINSTANCE instance);
typedef int (*TGameEntryInitLevel)(const char* directory);
typedef void (*TGameEntryStep)();
typedef CConfigFile* (*TGameEntryConfig)();
typedef int (*TGameEntryLevelEvent)(KR_Event& event);

struct SGameEntryRuntimeHooks {
  TGameEntryInitGraph initGraph;
  TGameEntryInitLevel initLevel;
  TGameEntryStep deinitLevel;
  TGameEntryStep beginLoop;
  TGameEntryStep nextFrame;
  TGameEntryConfig config;
  TGameEntryStep initPin;
  TGameEntryStep initSua;
  TGameEntryStep preloadTextures;
  TGameEntryStep restoreSurfaces;
  TGameEntryStep drawDebugMap;
  TGameEntryLevelEvent levelEvent;
};

enum EGameEntryRuntimeIssue {
  GAME_ENTRY_MISSING_GRAPH_INIT = 1u << 0,
  GAME_ENTRY_MISSING_LEVEL_INIT = 1u << 1,
  GAME_ENTRY_MISSING_LEVEL_DEINIT = 1u << 2,
  GAME_ENTRY_MISSING_BEGIN_LOOP = 1u << 3,
  GAME_ENTRY_MISSING_NEXT_FRAME = 1u << 4,
  GAME_ENTRY_MISSING_CONFIG = 1u << 5,
  GAME_ENTRY_MISSING_PIN_INIT = 1u << 6,
  GAME_ENTRY_MISSING_SUA_INIT = 1u << 7,
  GAME_ENTRY_MISSING_TEXTURE_PRELOAD = 1u << 8,
  GAME_ENTRY_MISSING_SURFACE_RESTORE = 1u << 9,
  GAME_ENTRY_MISSING_DEBUG_MAP_DRAW = 1u << 10,
  GAME_ENTRY_INVALID_LEVEL_DIRECTORY = 1u << 11,
  GAME_ENTRY_MISSING_LEVEL_EVENT = 1u << 12
};

void GameEntry_ConfigureRuntime(const SGameEntryRuntimeHooks& hooks);
SGameEntryRuntimeHooks GameEntry_RecoveredRuntimeHooks();
void GameEntry_UseRecoveredRuntime();
bool GameEntry_RuntimeReady();
unsigned int GameEntry_RuntimeMissingHooks();
unsigned int GameEntry_RuntimeIssues();
void GameEntry_ClearRuntimeIssues();

int ZAV_InitGraph(HINSTANCE instance);
int ZAV_InitLevel(const char* directory);
void ZAV_DeInitLevel();
void ZAV_BeginLoop();
void ZAV_NextFrame();
CConfigFile& ZAV_Config();
void PIN_InitEverything();
void SUA_InitEverything();
