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
  SRecoveredWindowPresentation validPresentation;
  validPresentation.mode = RECOVERED_WINDOW_MODE_WINDOWED;
  validPresentation.clientWidth = 1280;
  validPresentation.clientHeight = 960;
  SRecoveredWindowPresentation invalidPresentation = validPresentation;
  invalidPresentation.clientWidth = 1280;
  invalidPresentation.clientHeight = 720;
  SRecoveredWindowPresentation validExclusive = validPresentation;
  validExclusive.mode = RECOVERED_WINDOW_MODE_EXCLUSIVE;
  validExclusive.bitsPerPixel = 32;
  validExclusive.displayFrequency = 60;
  validExclusive.displayDevice = L"\\\\.\\DISPLAY1";
  SRecoveredWindowPresentation invalidExclusive = validExclusive;
  invalidExclusive.displayDevice.clear();
  if (!RecoveredSoftwareGraph_ValidatePresentation(validPresentation) ||
      !RecoveredSoftwareGraph_ValidatePresentation(validExclusive) ||
      RecoveredSoftwareGraph_ValidatePresentation(invalidPresentation) ||
      RecoveredSoftwareGraph_ValidatePresentation(invalidExclusive) ||
      RecoveredSoftwareGraph_ApplyPresentation(validPresentation, nullptr)) {
    return Fail("presentation validation did not remain fail-closed");
  }
  GameEntry_UseRecoveredRuntime();
  const unsigned int missing = GameEntry_RuntimeMissingHooks();
  const unsigned int expectedMissing = GAME_ENTRY_MISSING_LEVEL_INIT |
                                       GAME_ENTRY_MISSING_BEGIN_LOOP |
                                       GAME_ENTRY_MISSING_PIN_INIT |
                                       GAME_ENTRY_MISSING_SUA_INIT |
                                       GAME_ENTRY_MISSING_DEBUG_MAP_DRAW |
                                       GAME_ENTRY_MISSING_LEVEL_EVENT;
  if (missing != expectedMissing) {
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
  wchar_t temporaryDirectory[MAX_PATH] = {};
  if (GetTempPathW(MAX_PATH, temporaryDirectory) == 0) {
    return Fail("temporary directory lookup failed");
  }
  const std::wstring recoveryPath =
      std::wstring(temporaryDirectory) + L"rr2nw-graph-smoke.display-recovery";
  DeleteFileW(recoveryPath.c_str());
  if (!RecoveredSoftwareGraph_ConfigureDisplayRecovery(recoveryPath) ||
      RecoveredSoftwareGraph_DisplayModeCount() != 0u ||
      !RecoveredSoftwareGraph_WindowsPresentationState().recoveryConfigured ||
      RecoveredSoftwareGraph_WindowsPresentationState().staleModeRecovered) {
    return Fail("headless display recovery did not remain bounded");
  }
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

  SGRSoftwarePresentLayout exactLayout = {};
  SGRSoftwarePresentLayout wideLayout = {};
  SGRSoftwarePresentLayout tallLayout = {};
  if (!GRSoftwareComputePresentLayout(640, 480, &exactLayout) ||
      exactLayout.targetX != 0 || exactLayout.targetY != 0 ||
      exactLayout.targetWidth != 640 || exactLayout.targetHeight != 480 ||
      exactLayout.letterboxBars != 0 ||
      !GRSoftwareComputePresentLayout(1920, 1080, &wideLayout) ||
      wideLayout.targetX != 240 || wideLayout.targetY != 0 ||
      wideLayout.targetWidth != 1440 || wideLayout.targetHeight != 1080 ||
      wideLayout.letterboxBars != 2 ||
      !GRSoftwareComputePresentLayout(1080, 1920, &tallLayout) ||
      tallLayout.targetX != 0 || tallLayout.targetY != 555 ||
      tallLayout.targetWidth != 1080 || tallLayout.targetHeight != 810 ||
      tallLayout.letterboxBars != 2 ||
      GRSoftwareComputePresentLayout(0, 480, &exactLayout) ||
      GRSoftwareComputePresentLayout(640, 0, &exactLayout) ||
      GRSoftwareComputePresentLayout(640, 480, nullptr)) {
    return Fail("software present layout is not bounded and aspect-safe");
  }

  if (!GRClearScreen(TRUE, 37) || firstScreen[0] != 37 ||
      firstScreen[640 * 480 - 1] != 37 || !GRDumpScreen()) {
    return Fail("headless software framebuffer is not operational");
  }
  SGRSoftwarePresentStats presentStats = {};
  GRSoftwareGetPresentStats(&presentStats);
  if (presentStats.fullClientBlackErases != 0u) {
    return Fail("software presenter retained a full-client black erase");
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
