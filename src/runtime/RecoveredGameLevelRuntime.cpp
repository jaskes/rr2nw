#include "RecoveredGameLevelRuntime.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredRetailScriptManifest.h"

namespace {

unsigned int g_issues = 0;
bool g_ready = false;

int Fail(unsigned int issue) {
  g_issues = issue;
  RecoveredDrawableScene_Release();
  RecoveredRetailScriptManifest_Release();
  RecoveredLevelRuntime_Release();
  g_ready = false;
  return FALSE;
}

}  // namespace

int RecoveredGameLevel_Initialize(const char* directory) {
  RecoveredGameLevel_Release();
  g_issues = 0;

  if (!RecoveredLevelRuntime_Prepare(directory)) {
    return Fail(RECOVERED_GAME_LEVEL_PREPARE_FAILURE);
  }
  if (!RecoveredRetailScriptManifest_Preflight(
          RecoveredLevelRuntime_Directory())) {
    return Fail(RECOVERED_GAME_LEVEL_SCRIPT_MANIFEST_FAILURE);
  }
  if (!RecoveredLevelAssets_Initialize()) {
    return Fail(RECOVERED_GAME_LEVEL_ASSET_FAILURE);
  }
  if (!RecoveredDrawableScene_Initialize()) {
    return Fail(RECOVERED_GAME_LEVEL_SCENE_FAILURE);
  }

  g_ready = true;
  return TRUE;
}

void RecoveredGameLevel_Release() {
  RecoveredDrawableScene_Release();
  RecoveredRetailScriptManifest_Release();
  RecoveredLevelRuntime_Release();
  g_ready = false;
}

bool RecoveredGameLevel_IsReady() {
  return g_ready && RecoveredLevelRuntime_IsPrepared() &&
         RecoveredRetailScriptManifest_IsReady() &&
         RecoveredLevelAssets_IsReady() &&
         RecoveredDrawableScene_IsReady();
}

unsigned int RecoveredGameLevel_Issues() { return g_issues; }

void RecoveredGameLevel_UseRuntime() {
  SGameEntryRuntimeHooks hooks = GameEntry_RecoveredRuntimeHooks();
  hooks.initLevel = RecoveredGameLevel_Initialize;
  hooks.deinitLevel = RecoveredGameLevel_Release;
  GameEntry_ConfigureRuntime(hooks);
  GameEntry_EnableBoundedStartup();
}
