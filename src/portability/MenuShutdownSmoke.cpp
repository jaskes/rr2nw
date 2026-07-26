#include <cstdlib>
#include <cstring>
#include <iostream>

#include "SupervisorShutdownState.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

namespace {

enum Step {
  kReleaseScene = 1,
  kClearFigureLibrary,
  kReleaseFirstViewport,
  kReleaseSecondViewport,
  kEndProfile,
  kFinishGraph,
  kCloseVehiclePanel,
  kEndSupervisorSeance,
};

int g_steps[16] = {};
int g_stepCount = 0;
SGRViewport* g_firstViewport = nullptr;

void Record(Step step) {
  if (g_stepCount < static_cast<int>(sizeof(g_steps) / sizeof(g_steps[0]))) {
    g_steps[g_stepCount++] = step;
  }
}

void ReleaseScene(CViewScene*) {
  Record(kReleaseScene);
}

void ClearFigureLibrary() {
  Record(kClearFigureLibrary);
}

void ReleaseViewport(SGRViewport* viewport) {
  Record(viewport == g_firstViewport ? kReleaseFirstViewport
                                     : kReleaseSecondViewport);
}

void EndProfile() {
  Record(kEndProfile);
}

void FinishGraph() {
  Record(kFinishGraph);
}

void CloseVehiclePanel() {
  Record(kCloseVehiclePanel);
}

void EndSupervisorSeance() {
  Record(kEndSupervisorSeance);
}

int Fail(const char* message) {
  std::cerr << "legacy-menu-shutdown-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool StepsEqual(const Step* expected, int count) {
  if (g_stepCount != count) return false;
  for (int i = 0; i < count; ++i) {
    if (g_steps[i] != expected[i]) return false;
  }
  return true;
}

}  // namespace

int main() {
  ZAV_Deinit();
  SUA_DeinitEverything();

  const SZavShutdownHooks zavHooks = {
      ReleaseScene,
      ClearFigureLibrary,
      ReleaseViewport,
      EndProfile,
      FinishGraph,
  };
  ZAV_ConfigureShutdown(zavHooks);

  pScene = reinterpret_cast<CViewScene*>(0x1234);
  g_firstViewport = reinterpret_cast<SGRViewport*>(0x5678);
  ppViewports = new SGRViewport*[2];
  ppViewports[0] = g_firstViewport;
  ppViewports[1] = reinterpret_cast<SGRViewport*>(0x9ABC);
  pVirtualScreen = new unsigned char[32];
  ZAV_ArmLevelShutdown();
  ZAV_ArmGraphShutdown(2);

  if (ZAV_Viewport() != g_firstViewport || !ZAV_IsLevelShutdownArmed() ||
      !ZAV_IsGraphShutdownArmed()) {
    return Fail("ZAV lifecycle did not arm its owned resources");
  }

  ZAV_Deinit();
  const Step zavExpected[] = {
      kReleaseScene,
      kClearFigureLibrary,
      kReleaseFirstViewport,
      kReleaseSecondViewport,
      kEndProfile,
      kFinishGraph,
  };
  if (!StepsEqual(zavExpected,
                  static_cast<int>(sizeof(zavExpected) / sizeof(zavExpected[0]))) ||
      pScene != nullptr || ppViewports != nullptr || pVirtualScreen != nullptr ||
      ZAV_Viewport() != nullptr || ZAV_IsLevelShutdownArmed() ||
      ZAV_IsGraphShutdownArmed()) {
    return Fail("ZAV resources were not released in recovered order");
  }
  ZAV_Deinit();
  if (!StepsEqual(zavExpected,
                  static_cast<int>(sizeof(zavExpected) / sizeof(zavExpected[0])))) {
    return Fail("repeated ZAV shutdown released resources twice");
  }

  pScene = reinterpret_cast<CViewScene*>(0x2468);
  ZAV_ArmLevelShutdown();
  ZAV_DeinitLevelResources();
  if (pScene != nullptr || g_stepCount != 8 ||
      g_steps[6] != kReleaseScene || g_steps[7] != kClearFigureLibrary) {
    return Fail("level-only shutdown did not release its scene exactly once");
  }
  ZAV_DeinitLevelResources();
  if (g_stepCount != 8) {
    return Fail("repeated level shutdown released resources twice");
  }

  char overallInfo[40] = {};
  dwTime0 = 1000;
  m_dwPrevTime = 5000;
  dwFrames = 120;
  dwMem0 = 4096;
  dwMem1 = 1024;
  if (!ZAV_FormatOverallInfo(overallInfo, sizeof(overallInfo)) ||
      std::strcmp(overallInfo, "30 Mem=3072") != 0) {
    return Fail("overall diagnostics formatting diverged");
  }
  char truncated[4] = {};
  if (ZAV_FormatOverallInfo(truncated, sizeof(truncated)) ||
      truncated[sizeof(truncated) - 1] != '\0') {
    return Fail("overall diagnostics did not reject truncation");
  }
  dwTime0 = m_dwPrevTime;
  ZAV_PrintOverallInfo();

  const SSuaShutdownHooks suaHooks = {
      CloseVehiclePanel,
      EndSupervisorSeance,
  };
  SUA_ConfigureShutdown(suaHooks);
  SUA_ArmShutdown();
  if (!SUA_IsShutdownArmed()) {
    return Fail("Supervisor shutdown did not arm");
  }
  SUA_DeinitEverything();
  if (SUA_IsShutdownArmed() || g_stepCount != 10 ||
      g_steps[8] != kCloseVehiclePanel ||
      g_steps[9] != kEndSupervisorSeance) {
    return Fail("Supervisor resources were not released in recovered order");
  }
  SUA_DeinitEverything();
  if (g_stepCount != 10) {
    return Fail("repeated Supervisor shutdown released resources twice");
  }

  std::cout << "legacy-menu-shutdown-smoke: OK\n";
  return EXIT_SUCCESS;
}
