#include <cstdlib>
#include <iostream>

#define LAST_H__VIEW
#include "game.h"

#include "FrameRuntimeState.h"
#include "RecoveredSoftwareFrame.h"
#include "ZavSceneState.h"
#include "graph.h"
#include "h/super.h"
#include "kernel/h/session.h"

extern SDeviceList _dL;

namespace {

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << "recovered-software-frame-smoke: " << message << '\n';
  return false;
}

}  // namespace

int main() {
  SDeviceDescr device = {};
  device.swHw = GR_SOFTWARE;
  _dL.currDevice = &device;

  pScene = 0;
  g_timer.Start();
  Session::m_realTimer = &g_timer;
  Frame_BindRecoveredSoftware();

  if (!Expect(Frame_RuntimeReady(false),
              "recovered software hooks are not ready") ||
      !Expect(!Frame_RuntimeReady(true),
              "software binding unexpectedly claims a D3D flush") ||
      !Expect(GRStartScene() == TRUE,
              "software graphics scene did not start")) {
    return EXIT_FAILURE;
  }

  CViewDynamicList list;
  CFMatrix3x4 direction;
  direction.LoadIdentity();
  SUA_BeginRender(0, list);
  ZAV_RenderFrame(&direction, list);
  D3D_DrawZList();
  ZAV_PrintFrameInfo();
  SUA_EndRender(0);
  ZAV_EndRenderFrame();

  if (!Expect(Frame_RuntimeIssues() == 0,
              "empty recovered software frame reported an issue") ||
      !Expect(list.First() == 0,
              "empty arena unexpectedly published a dynamic object")) {
    return EXIT_FAILURE;
  }

  pScene = reinterpret_cast<CViewScene*>(1);
  Frame_ClearRuntimeIssues();
  ZAV_RenderFrame(&direction, list);
  if (!Expect(Frame_RuntimeIssues() ==
                  FRAME_RUNTIME_SCENE_DRAW_UNAVAILABLE,
              "unbound recovered scene draw was not diagnosed")) {
    return EXIT_FAILURE;
  }
  pScene = 0;

  Session::m_realTimer = 0;
  Frame_ClearRuntimeIssues();
  SUA_BeginRender(0, list);
  if (!Expect(Frame_RuntimeIssues() == FRAME_RUNTIME_MISSING_REAL_TIMER,
              "missing recovered timer was not diagnosed")) {
    return EXIT_FAILURE;
  }

  Frame_ClearRuntimeIssues();
  _dL.currDevice = 0;
  return EXIT_SUCCESS;
}
