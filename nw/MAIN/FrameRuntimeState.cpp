#include "FrameRuntimeState.h"

#include "graph.h"

namespace {

SFrameRuntimeHooks g_frameHooks = {};
unsigned int g_frameIssues = 0;

void MarkMissing(unsigned int issue) {
  g_frameIssues |= issue;
}

}  // namespace

void Frame_ConfigureRuntime(const SFrameRuntimeHooks& hooks) {
  g_frameHooks = hooks;
  Frame_ClearRuntimeIssues();
}

bool Frame_RuntimeReady(bool hardware) {
  const bool softwareReady =
      g_frameHooks.beginArenaRender != 0 &&
      g_frameHooks.renderScene != 0 &&
      g_frameHooks.finishGraphicsScene != 0 &&
      g_frameHooks.endArenaRender != 0 &&
      g_frameHooks.releaseFrameScene != 0;
  return softwareReady &&
         (!hardware || g_frameHooks.flushHardwareZList != 0);
}

unsigned int Frame_RuntimeIssues() {
  return g_frameIssues;
}

void Frame_ClearRuntimeIssues() {
  g_frameIssues = 0;
}

void SUA_BeginRender(CViewScene* scene, CViewDynamicList& list) {
  if (g_frameHooks.beginArenaRender != 0) {
    g_frameHooks.beginArenaRender(scene, list);
  } else {
    MarkMissing(FRAME_RUNTIME_MISSING_SUA_BEGIN);
  }
}

void ZAV_RenderFrame(TCSFMatrix3x4* direction, CViewDynamicList& list) {
  if (direction == 0) {
    MarkMissing(FRAME_RUNTIME_NULL_VIEW_DIRECTION);
    return;
  }
  if (g_frameHooks.renderScene != 0) {
    g_frameHooks.renderScene(direction, list);
  } else {
    MarkMissing(FRAME_RUNTIME_MISSING_ZAV_RENDER);
  }
}

void ZAV_PrintFrameInfo() {
  if (g_frameHooks.finishGraphicsScene != 0) {
    g_frameHooks.finishGraphicsScene();
  } else {
    MarkMissing(FRAME_RUNTIME_MISSING_GRAPHICS_FINISH);
  }
}

void SUA_EndRender(CViewScene* scene) {
  if (g_frameHooks.endArenaRender != 0) {
    g_frameHooks.endArenaRender(scene);
  } else {
    MarkMissing(FRAME_RUNTIME_MISSING_SUA_END);
  }
}

void ZAV_EndRenderFrame() {
  if (g_frameHooks.releaseFrameScene != 0) {
    g_frameHooks.releaseFrameScene();
  } else {
    MarkMissing(FRAME_RUNTIME_MISSING_ZAV_END);
  }
}

void D3D_DrawZList() {
  if (!GRIsHardware()) return;

  if (g_frameHooks.flushHardwareZList != 0) {
    g_frameHooks.flushHardwareZList();
  } else {
    MarkMissing(FRAME_RUNTIME_MISSING_D3D_FLUSH);
  }
}
