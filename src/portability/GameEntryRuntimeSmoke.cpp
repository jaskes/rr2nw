#include <cstdlib>
#include <cstdio>
#include <cstring>

#define LAST_H__VIEW
#include "game.h"
#include "dmap.h"
#include "filesys.h"
#include "h/super.h"

#include "GameEntryRuntimeState.h"

namespace {

enum Step {
  kInitGraph,
  kInitLevel,
  kDeinitLevel,
  kBeginLoop,
  kNextFrame,
  kPin,
  kSua,
  kPreload,
  kRestore,
  kDebugDraw,
  kLevelEvent
};

Step g_steps[16] = {};
int g_stepCount = 0;
CConfigFile* g_config = nullptr;

void Record(Step step) { g_steps[g_stepCount++] = step; }

int InitGraph(HINSTANCE) {
  Record(kInitGraph);
  return TRUE;
}

int InitLevel(const char* directory) {
  if (std::strcmp(directory, "Level.03N") != 0) return FALSE;
  Record(kInitLevel);
  return TRUE;
}

void DeinitLevel() { Record(kDeinitLevel); }
void BeginLoop() { Record(kBeginLoop); }
void NextFrame() { Record(kNextFrame); }
void InitPin() { Record(kPin); }
void InitSua() { Record(kSua); }
void Preload() { Record(kPreload); }
void Restore() { Record(kRestore); }
void DebugDraw() { Record(kDebugDraw); }
int LevelEvent(KR_Event&) {
  Record(kLevelEvent);
  return 1;
}
CConfigFile* Config() { return g_config; }

int Fail(const char* message) {
  std::fprintf(stderr, "game-entry-runtime-smoke: %s\n", message);
  return EXIT_FAILURE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return Fail("expected config fixture path");

  FILE* fixture = std::fopen(argv[1], "wb");
  if (fixture == nullptr) return Fail("cannot create config fixture");
  const char fixtureText[] = "[Smoke]\nValue=ready\n";
  if (std::fwrite(fixtureText, sizeof(fixtureText) - 1, 1, fixture) != 1 ||
      std::fclose(fixture) != 0) {
    return Fail("cannot write config fixture");
  }

  const SGameEntryRuntimeHooks empty = {};
  GameEntry_ConfigureRuntime(empty);
  if (GameEntry_RuntimeReady()) return Fail("empty runtime reported ready");
  if (ZAV_InitGraph(nullptr) != FALSE ||
      (GameEntry_RuntimeIssues() & GAME_ENTRY_MISSING_GRAPH_INIT) == 0 ||
      (GameEntry_RuntimeIssues() & GAME_ENTRY_MISSING_CONFIG) == 0) {
    return Fail("incomplete runtime was not diagnosed");
  }
  GameEntry_ClearRuntimeIssues();
  if (ZAV_InitLevel(nullptr) != FALSE ||
      GameEntry_RuntimeIssues() != GAME_ENTRY_INVALID_LEVEL_DIRECTORY) {
    return Fail("invalid level directory was not rejected");
  }

  CConfigFile config(argv[1]);
  g_config = &config;
  const SGameEntryRuntimeHooks hooks = {
      InitGraph, InitLevel, DeinitLevel, BeginLoop, NextFrame, Config,
      InitPin,   InitSua,   Preload,     Restore,   DebugDraw, LevelEvent};
  GameEntry_ConfigureRuntime(hooks);
  if (!GameEntry_RuntimeReady()) return Fail("complete runtime is not ready");

  if (!ZAV_InitGraph(nullptr) || !ZAV_InitLevel("Level.03N")) {
    return Fail("configured initializer failed");
  }
  PIN_InitEverything();
  SUA_InitEverything();
  GRPreLoadTextures();
  GRRestoreSurfaces();
  ZAV_BeginLoop();
  ZAV_NextFrame();
  g_debugMap.Draw();
  KR_Event event;
  if (g_super.m_level.receiveEvent(event) != 1) {
    return Fail("level event was not dispatched");
  }
  ZAV_DeInitLevel();
  if (&ZAV_Config() != &config) return Fail("config owner was not returned");

  const Step expected[] = {kInitGraph, kInitLevel, kPin,       kSua,
                           kPreload,   kRestore,   kBeginLoop, kNextFrame,
                           kDebugDraw, kLevelEvent, kDeinitLevel};
  if (g_stepCount != static_cast<int>(sizeof(expected) / sizeof(expected[0]))) {
    return Fail("unexpected dispatch count");
  }
  for (int index = 0; index < g_stepCount; ++index) {
    if (g_steps[index] != expected[index]) return Fail("dispatch order changed");
  }
  if (GameEntry_RuntimeIssues() != 0) {
    return Fail("configured dispatch reported an issue");
  }
  return EXIT_SUCCESS;
}
