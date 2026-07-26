#include "ZavShutdownState.h"

namespace {

SZavShutdownHooks g_zavShutdownHooks = {};
int g_zavViewportCount = 0;
bool g_zavGraphShutdownArmed = false;
bool g_zavLevelShutdownArmed = false;

}  // namespace

unsigned char* pVirtualScreen = 0;

void ZAV_ConfigureShutdown(const SZavShutdownHooks& hooks) {
  g_zavShutdownHooks = hooks;
}

void ZAV_ArmGraphShutdown(int viewportCount) {
  g_zavViewportCount = viewportCount > 0 ? viewportCount : 0;
  g_zavGraphShutdownArmed = true;
}

void ZAV_ArmLevelShutdown() {
  g_zavLevelShutdownArmed = true;
}

bool ZAV_IsGraphShutdownArmed() {
  return g_zavGraphShutdownArmed;
}

bool ZAV_IsLevelShutdownArmed() {
  return g_zavLevelShutdownArmed;
}

void ZAV_DeinitLevelResources() {
  if (!g_zavLevelShutdownArmed) return;

  g_zavLevelShutdownArmed = false;
  CViewScene* const scene = pScene;
  pScene = 0;
  if (scene != 0 && g_zavShutdownHooks.releaseScene != 0) {
    g_zavShutdownHooks.releaseScene(scene);
  }
  if (g_zavShutdownHooks.clearFigureLibrary != 0) {
    g_zavShutdownHooks.clearFigureLibrary();
  }
}

void ZAV_Deinit() {
  ZAV_DeinitLevelResources();
  if (!g_zavGraphShutdownArmed) return;

  g_zavGraphShutdownArmed = false;
  SGRViewport** const viewports = ppViewports;
  const int viewportCount = g_zavViewportCount;
  ppViewports = 0;
  g_zavViewportCount = 0;

  if (viewports != 0) {
    if (g_zavShutdownHooks.releaseViewport != 0) {
      for (int i = 0; i < viewportCount; ++i) {
        if (viewports[i] != 0) {
          g_zavShutdownHooks.releaseViewport(viewports[i]);
        }
      }
    }
    delete[] viewports;
  }

  if (g_zavShutdownHooks.endProfile != 0) {
    g_zavShutdownHooks.endProfile();
  }
  if (g_zavShutdownHooks.finishGraph != 0) {
    g_zavShutdownHooks.finishGraph();
  }

  delete[] pVirtualScreen;
  pVirtualScreen = 0;
}
