#include "GameEntryRuntimeState.h"

#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavOverallInfoState.h"

namespace {

void PreloadSoftwareTextures() {
  // Software textures already reside in process memory.
}

void RestoreSoftwareSurfaces() {
  // A DIB-backed framebuffer cannot become a lost DirectDraw surface.
}

void AdvanceFrameCounter() { ++dwFrames; }

}  // namespace

SGameEntryRuntimeHooks GameEntry_RecoveredRuntimeHooks() {
  SGameEntryRuntimeHooks hooks = {};
  hooks.initGraph = RecoveredSoftwareGraph_Initialize;
  hooks.deinitLevel = RecoveredLevelRuntime_Release;
  hooks.nextFrame = AdvanceFrameCounter;
  hooks.config = RecoveredLevelRuntime_Config;
  hooks.preloadTextures = PreloadSoftwareTextures;
  hooks.restoreSurfaces = RestoreSoftwareSurfaces;
  return hooks;
}
