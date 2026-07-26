#include <cstdlib>
#include <cstdio>

#define LAST_H__VIEW
#include "game.h"
#include "graph.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

extern SDeviceList _dL;

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "recovered-software-graph-smoke: %s\n", message);
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  GameEntry_UseRecoveredRuntime();
  const unsigned int missing = GameEntry_RuntimeMissingHooks();
  const unsigned int connected = GAME_ENTRY_MISSING_GRAPH_INIT |
                                 GAME_ENTRY_MISSING_NEXT_FRAME |
                                 GAME_ENTRY_MISSING_TEXTURE_PRELOAD |
                                 GAME_ENTRY_MISSING_SURFACE_RESTORE;
  if ((missing & connected) != 0 ||
      (missing & GAME_ENTRY_MISSING_LEVEL_INIT) == 0 ||
      (missing & GAME_ENTRY_MISSING_CONFIG) == 0) {
    return Fail("default recovered hook inventory changed");
  }

  if (ZAV_InitGraph(nullptr) != FALSE ||
      RecoveredSoftwareGraph_IsReady() ||
      (GameEntry_RuntimeIssues() & GAME_ENTRY_MISSING_LEVEL_INIT) == 0) {
    return Fail("partial runtime did not remain fail-closed");
  }

  if (!RecoveredSoftwareGraph_Initialize(nullptr)) {
    return Fail("headless software graph initialization failed");
  }
  unsigned char* const firstScreen = _gr_pScreen;
  if (!RecoveredSoftwareGraph_IsReady() || firstScreen == nullptr ||
      _gr_hWnd != nullptr || _gr_hDC != nullptr ||
      _gr_nScreenWidth != 640 || _gr_nScreenHeight != 480 ||
      _dL.currDevice == nullptr ||
      _dL.currDevice->swHw != GR_SOFTWARE || ppViewports == nullptr ||
      ppViewports[0] == nullptr || GRGetViewport() != ppViewports[0] ||
      _gr_DIBInfo.bmiHeader.biBitCount != 8 ||
      _gr_DIBInfo.bmiHeader.biHeight != -480) {
    return Fail("software graph state was not published");
  }

  if (!GRClearScreen(TRUE, 37) || firstScreen[0] != 37 ||
      firstScreen[640 * 480 - 1] != 37 || !GRDumpScreen()) {
    return Fail("headless software framebuffer is not operational");
  }

  const SGameEntryRuntimeHooks hooks = GameEntry_RecoveredRuntimeHooks();
  hooks.preloadTextures();
  hooks.restoreSurfaces();
  dwFrames = 41;
  hooks.nextFrame();
  if (dwFrames != 42) return Fail("recovered frame counter did not advance");

  if (!RecoveredSoftwareGraph_Initialize(nullptr) ||
      _gr_pScreen != firstScreen) {
    return Fail("repeated graph initialization was not idempotent");
  }

  ZAV_Deinit();
  if (RecoveredSoftwareGraph_IsReady() || _gr_pScreen != nullptr ||
      _dL.currDevice != nullptr || ppViewports != nullptr ||
      GRGetViewport() != nullptr || _gr_nScreenOriginX != 0 ||
      _gr_nScreenOriginY != 0 || ZAV_IsGraphShutdownArmed()) {
    return Fail("software graph shutdown left live state");
  }
  ZAV_Deinit();
  return EXIT_SUCCESS;
}
