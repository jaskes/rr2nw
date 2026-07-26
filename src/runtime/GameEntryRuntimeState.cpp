#include "GameEntryRuntimeState.h"

#include <cstdlib>

#define LAST_H__VIEW
#include "game.h"
#include "dmap.h"
#include "filesys.h"
#include "h/olevel.h"

namespace {

SGameEntryRuntimeHooks g_hooks = GameEntry_RecoveredRuntimeHooks();
unsigned int g_issues = 0;

void Report(unsigned int issue) { g_issues |= issue; }

void Dispatch(TGameEntryStep step, unsigned int issue) {
  if (step == nullptr) {
    Report(issue);
    return;
  }
  step();
}

unsigned int MissingHooks() {
  unsigned int missing = 0;
  if (g_hooks.initGraph == nullptr) missing |= GAME_ENTRY_MISSING_GRAPH_INIT;
  if (g_hooks.initLevel == nullptr) missing |= GAME_ENTRY_MISSING_LEVEL_INIT;
  if (g_hooks.deinitLevel == nullptr) missing |= GAME_ENTRY_MISSING_LEVEL_DEINIT;
  if (g_hooks.beginLoop == nullptr) missing |= GAME_ENTRY_MISSING_BEGIN_LOOP;
  if (g_hooks.nextFrame == nullptr) missing |= GAME_ENTRY_MISSING_NEXT_FRAME;
  if (g_hooks.config == nullptr) missing |= GAME_ENTRY_MISSING_CONFIG;
  if (g_hooks.initPin == nullptr) missing |= GAME_ENTRY_MISSING_PIN_INIT;
  if (g_hooks.initSua == nullptr) missing |= GAME_ENTRY_MISSING_SUA_INIT;
  if (g_hooks.preloadTextures == nullptr) {
    missing |= GAME_ENTRY_MISSING_TEXTURE_PRELOAD;
  }
  if (g_hooks.restoreSurfaces == nullptr) {
    missing |= GAME_ENTRY_MISSING_SURFACE_RESTORE;
  }
  if (g_hooks.drawDebugMap == nullptr) {
    missing |= GAME_ENTRY_MISSING_DEBUG_MAP_DRAW;
  }
  if (g_hooks.levelEvent == nullptr) missing |= GAME_ENTRY_MISSING_LEVEL_EVENT;
  return missing;
}

}  // namespace

void GameEntry_ConfigureRuntime(const SGameEntryRuntimeHooks& hooks) {
  g_hooks = hooks;
  GameEntry_ClearRuntimeIssues();
}

void GameEntry_UseRecoveredRuntime() {
  GameEntry_ConfigureRuntime(GameEntry_RecoveredRuntimeHooks());
}

bool GameEntry_RuntimeReady() { return MissingHooks() == 0; }

unsigned int GameEntry_RuntimeMissingHooks() { return MissingHooks(); }

unsigned int GameEntry_RuntimeIssues() { return g_issues; }

void GameEntry_ClearRuntimeIssues() { g_issues = 0; }

int ZAV_InitGraph(HINSTANCE instance) {
  const unsigned int missing = MissingHooks();
  if (missing != 0) {
    Report(missing);
    return FALSE;
  }
  return g_hooks.initGraph(instance);
}

int ZAV_InitLevel(const char* directory) {
  if (directory == nullptr || directory[0] == 0) {
    Report(GAME_ENTRY_INVALID_LEVEL_DIRECTORY);
    return FALSE;
  }
  if (g_hooks.initLevel == nullptr) {
    Report(GAME_ENTRY_MISSING_LEVEL_INIT);
    return FALSE;
  }
  return g_hooks.initLevel(directory);
}

void ZAV_DeInitLevel() {
  Dispatch(g_hooks.deinitLevel, GAME_ENTRY_MISSING_LEVEL_DEINIT);
}

void ZAV_BeginLoop() {
  Dispatch(g_hooks.beginLoop, GAME_ENTRY_MISSING_BEGIN_LOOP);
}

void ZAV_NextFrame() {
  Dispatch(g_hooks.nextFrame, GAME_ENTRY_MISSING_NEXT_FRAME);
}

CConfigFile& ZAV_Config() {
  if (g_hooks.config == nullptr) {
    Report(GAME_ENTRY_MISSING_CONFIG);
    std::abort();
  }
  CConfigFile* config = g_hooks.config();
  if (config == nullptr) {
    Report(GAME_ENTRY_MISSING_CONFIG);
    std::abort();
  }
  return *config;
}

void PIN_InitEverything() {
  Dispatch(g_hooks.initPin, GAME_ENTRY_MISSING_PIN_INIT);
}

void SUA_InitEverything() {
  Dispatch(g_hooks.initSua, GAME_ENTRY_MISSING_SUA_INIT);
}

void GRPreLoadTextures() {
  Dispatch(g_hooks.preloadTextures, GAME_ENTRY_MISSING_TEXTURE_PRELOAD);
}

void GRRestoreSurfaces() {
  Dispatch(g_hooks.restoreSurfaces, GAME_ENTRY_MISSING_SURFACE_RESTORE);
}

void DebugMap::Draw() {
  Dispatch(g_hooks.drawDebugMap, GAME_ENTRY_MISSING_DEBUG_MAP_DRAW);
}

int ol_Level::receiveEvent(KR_Event& event) {
  if (g_hooks.levelEvent == nullptr) {
    Report(GAME_ENTRY_MISSING_LEVEL_EVENT);
    return 0;
  }
  return g_hooks.levelEvent(event);
}

void ol_Level::addNotify() { KR_Object::addNotify(); }

void ol_Level::removeNotify() { KR_Object::removeNotify(); }

void ol_Level::closeLevel() { m_curLevel = -1; }
