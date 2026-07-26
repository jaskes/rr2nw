#include <cstdlib>
#include <iostream>

#define LAST_H__VIEW
#include "game.h"

#include "FrameRuntimeState.h"
#include "graph.h"

extern SDeviceList _dL;

namespace {

enum Step {
  kBeginArena = 1,
  kRenderScene,
  kFlushHardware,
  kFinishGraphics,
  kEndArena,
  kReleaseFrameScene
};

int g_steps[16] = {};
int g_stepCount = 0;
int g_finishResult = FALSE;

void Record(Step step) {
  if (g_stepCount < static_cast<int>(sizeof(g_steps) / sizeof(g_steps[0]))) {
    g_steps[g_stepCount++] = step;
  }
}

void BeginArena(CViewScene*, CViewDynamicList&) {
  Record(kBeginArena);
}

void RenderScene(TCSFMatrix3x4*, CViewDynamicList&) {
  Record(kRenderScene);
}

void FlushHardware() {
  Record(kFlushHardware);
}

void FinishGraphics() {
  g_finishResult = GREndScene();
  Record(kFinishGraphics);
}

void EndArena(CViewScene*) {
  Record(kEndArena);
}

void ReleaseFrameScene() {
  Record(kReleaseFrameScene);
}

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << "legacy-frame-runtime-smoke: " << message << '\n';
  return false;
}

bool StepsEqual(const Step* expected, int count) {
  if (g_stepCount != count) return false;
  for (int index = 0; index < count; ++index) {
    if (g_steps[index] != expected[index]) return false;
  }
  return true;
}

}  // namespace

int main() {
  CViewDynamicList list;
  CFMatrix3x4 direction;
  direction.LoadIdentity();

  SDeviceDescr device = {};
  device.swHw = GR_SOFTWARE;
  _dL.currDevice = &device;

  SFrameRuntimeHooks emptyHooks = {};
  Frame_ConfigureRuntime(emptyHooks);
  if (!Expect(!Frame_RuntimeReady(false),
              "empty software frame was reported ready")) {
    return EXIT_FAILURE;
  }

  SUA_BeginRender(NULL, list);
  ZAV_RenderFrame(&direction, list);
  ZAV_PrintFrameInfo();
  SUA_EndRender(NULL);
  ZAV_EndRenderFrame();
  D3D_DrawZList();
  const unsigned int missingSoftware =
      FRAME_RUNTIME_MISSING_SUA_BEGIN |
      FRAME_RUNTIME_MISSING_ZAV_RENDER |
      FRAME_RUNTIME_MISSING_GRAPHICS_FINISH |
      FRAME_RUNTIME_MISSING_SUA_END |
      FRAME_RUNTIME_MISSING_ZAV_END;
  if (!Expect(Frame_RuntimeIssues() == missingSoftware,
              "missing software stages were not diagnosed exactly") ||
      !Expect(g_stepCount == 0,
              "an unconfigured stage was executed")) {
    return EXIT_FAILURE;
  }

  SFrameRuntimeHooks hooks = {
      BeginArena,
      RenderScene,
      FinishGraphics,
      EndArena,
      ReleaseFrameScene,
      FlushHardware
  };
  Frame_ConfigureRuntime(hooks);
  if (!Expect(Frame_RuntimeReady(false) && Frame_RuntimeReady(true),
              "complete frame hooks were not reported ready")) {
    return EXIT_FAILURE;
  }

  if (!Expect(GRStartScene() == TRUE,
              "software graphics scene did not start")) {
    return EXIT_FAILURE;
  }

  SUA_BeginRender(NULL, list);
  ZAV_RenderFrame(&direction, list);
  D3D_DrawZList();
  ZAV_PrintFrameInfo();
  SUA_EndRender(NULL);
  ZAV_EndRenderFrame();
  const Step softwareSteps[] = {
      kBeginArena,
      kRenderScene,
      kFinishGraphics,
      kEndArena,
      kReleaseFrameScene
  };
  if (!Expect(StepsEqual(softwareSteps, 5),
              "software frame stage order changed") ||
      !Expect(Frame_RuntimeIssues() == 0,
              "complete software frame reported an issue") ||
      !Expect(g_finishResult == TRUE,
              "software graphics scene did not finish")) {
    return EXIT_FAILURE;
  }

  device.swHw = GR_HARDWARE;
  D3D_DrawZList();
  const Step hardwareSteps[] = {
      kBeginArena,
      kRenderScene,
      kFinishGraphics,
      kEndArena,
      kReleaseFrameScene,
      kFlushHardware
  };
  if (!Expect(StepsEqual(hardwareSteps, 6),
              "hardware z-list flush was not dispatched exactly once")) {
    return EXIT_FAILURE;
  }

  ZAV_RenderFrame(NULL, list);
  if (!Expect((Frame_RuntimeIssues() &
               FRAME_RUNTIME_NULL_VIEW_DIRECTION) != 0,
              "null view direction was not rejected")) {
    return EXIT_FAILURE;
  }

  Frame_ClearRuntimeIssues();
  _dL.currDevice = NULL;
  return EXIT_SUCCESS;
}
