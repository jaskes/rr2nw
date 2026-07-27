#include "RecoveredGameServicesRuntime.h"

#include <new>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__SCENE
#include "game.h"
#include "dmap.h"
#include "graph.h"
#include "h/super.h"
#include "kernel/h/session.h"
#include "suavik.h"

#include "FrameRuntimeState.h"
#include "GameEntryRuntimeState.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredSoftwareFrame.h"
#include "RecoveredSoftwareGraph.h"
#include "SupervisorShutdownState.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"

namespace {

unsigned int g_issues = 0;
bool g_platformReady = false;
bool g_comOwned = false;
bool g_sessionReady = false;
bool g_sessionAttached = false;
bool g_loopReady = false;

void Report(unsigned int issue) { g_issues |= issue; }

void EndBoundedSession() {
  SUA_BindSession(nullptr);

  if (g_super.m_context != nullptr) {
    g_super.m_context->clearObjects();
    if (g_sessionAttached) {
      g_super.m_session.Remove(g_super.m_context);
    }
    delete g_super.m_context;
    g_super.m_context = nullptr;
  }
  g_sessionAttached = false;

  delete g_super.m_publisher;
  g_super.m_publisher = nullptr;
  g_super.m_level.closeLevel();

  Session::m_realTimer = nullptr;
  Session::m_hardware = nullptr;

  const SFrameRuntimeHooks emptyFrameHooks = {};
  Frame_ConfigureRuntime(emptyFrameHooks);
  g_sessionReady = false;
  g_loopReady = false;
}

void InitializePlatform() {
  if (g_platformReady) return;

  // The bounded software path needs COM ownership but deliberately does not
  // activate the recovered RSX/audio backend.
  const HRESULT result =
      CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(result)) {
    g_comOwned = true;
    g_platformReady = true;
    return;
  }
  if (result == RPC_E_CHANGED_MODE) {
    // COM is already available on this thread in a different apartment.
    g_comOwned = false;
    g_platformReady = true;
    return;
  }
  Report(RECOVERED_GAME_SERVICES_COM_FAILURE);
}

void InitializeSession() {
  if (g_sessionReady) return;
  if (!g_platformReady) {
    Report(RECOVERED_GAME_SERVICES_MISSING_PLATFORM);
    return;
  }
  if (!RecoveredGameLevel_IsReady()) {
    Report(RECOVERED_GAME_SERVICES_MISSING_LEVEL);
    return;
  }

  EndBoundedSession();
  try {
    g_timer.Start();
    Session::m_realTimer = &g_timer;
    // Hardware/Vehicle registration belongs to the interactive-seance
    // frontier. DebugMap therefore stays inactive in this bounded context.
    Session::m_hardware = nullptr;

    g_super.m_context = new SimulationContext(64, 128);
    g_super.m_publisher = new Publisher();
    g_super.m_context->addObject("Publisher", g_super.m_publisher);
    g_super.m_context->addObject("DebugMap", &g_debugMap);
    g_super.m_context->addObject("LEVEL", &g_super.m_level);
    g_super.m_session.Add(g_super.m_context);
    g_sessionAttached = true;
    SUA_BindSession(&g_super.m_session);

    Frame_BindRecoveredSoftware();
    const SSuaShutdownHooks shutdownHooks = {nullptr,
                                              EndBoundedSession};
    SUA_ConfigureShutdown(shutdownHooks);
    SUA_ArmShutdown();
    g_sessionReady = true;
  } catch (const std::bad_alloc&) {
    EndBoundedSession();
    Report(RECOVERED_GAME_SERVICES_SESSION_FAILURE);
  } catch (...) {
    EndBoundedSession();
    Report(RECOVERED_GAME_SERVICES_SESSION_FAILURE);
  }
}

void BeginLoop() {
  g_loopReady = false;
  if (!g_sessionReady || !RecoveredGameLevel_IsReady() ||
      !RecoveredSoftwareGraph_IsReady() || pScene == nullptr ||
      ppViewports == nullptr || ppViewports[0] == nullptr ||
      !Frame_RuntimeReady(false)) {
    Report(RECOVERED_GAME_SERVICES_BEGIN_LOOP_FAILURE);
    return;
  }

  dwFrames = 0;
  dwTime0 = GetTickCount();
  pScene->CheckDynamicMap();
  GRSetViewport(ppViewports[0]);
  g_loopReady = true;
}

void DrawDebugMap() {
  if (g_debugMap.IsActive()) {
    Report(RECOVERED_GAME_SERVICES_ACTIVE_DEBUG_MAP_UNAVAILABLE);
  }
}

int ReceiveLevelEvent(KR_Event& event) {
  if (event.label == KR_WAKE_UP) return 1;
  Report(RECOVERED_GAME_SERVICES_UNSUPPORTED_LEVEL_EVENT);
  return 0;
}

int InitializeComposedLevel(const char* directory) {
  RecoveredGameServices_Release();
  g_issues = 0;
  return RecoveredGameLevel_Initialize(directory);
}

void ReleaseComposedLevel() {
  RecoveredGameServices_Release();
  RecoveredGameLevel_Release();
}

bool PumpMessages() {
  MSG message = {};
  while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
    if (message.message == WM_QUIT) {
      Report(RECOVERED_GAME_SERVICES_QUIT_REQUESTED);
      return false;
    }
    TranslateMessage(&message);
    DispatchMessageA(&message);
  }
  return true;
}

}  // namespace

void RecoveredGameServices_UseRuntime() {
  SGameEntryRuntimeHooks hooks = GameEntry_RecoveredRuntimeHooks();
  hooks.initLevel = InitializeComposedLevel;
  hooks.deinitLevel = ReleaseComposedLevel;
  hooks.beginLoop = BeginLoop;
  hooks.initPin = InitializePlatform;
  hooks.initSua = InitializeSession;
  hooks.drawDebugMap = DrawDebugMap;
  hooks.levelEvent = ReceiveLevelEvent;
  GameEntry_ConfigureRuntime(hooks);
}

void RecoveredGameServices_Release() {
  if (SUA_IsShutdownArmed()) {
    SUA_DeinitEverything();
  } else {
    EndBoundedSession();
  }

  if (g_comOwned) CoUninitialize();
  g_comOwned = false;
  g_platformReady = false;
  g_loopReady = false;
}

bool RecoveredGameServices_PlatformReady() { return g_platformReady; }

bool RecoveredGameServices_SessionReady() { return g_sessionReady; }

bool RecoveredGameServices_LoopReady() { return g_loopReady; }

bool RecoveredGameServices_IsReady() {
  return g_platformReady && g_sessionReady && g_loopReady &&
         RecoveredGameLevel_IsReady() && Frame_RuntimeReady(false);
}

unsigned int RecoveredGameServices_Issues() { return g_issues; }

int RecoveredGameServices_RunFrame() {
  if (!RecoveredGameServices_IsReady()) {
    Report(RECOVERED_GAME_SERVICES_BEGIN_LOOP_FAILURE);
    return FALSE;
  }
  if (!PumpMessages()) return FALSE;

  Frame_ClearRuntimeIssues();
  if (!GRStartScene()) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }

  CViewDynamicList dynamics;
  CFMatrix3x4 direction;
  direction.LoadIdentity();
  SUA_BeginRender(ZAV_Scene(), dynamics);
  g_debugMap.Draw();
  ZAV_RenderFrame(&direction, dynamics);
  ZAV_PrintFrameInfo();
  SUA_EndRender(ZAV_Scene());
  ZAV_EndRenderFrame();
  SUA_ProcessEvents();
  ZAV_NextFrame();

  if (Frame_RuntimeIssues() != 0 || !GRDumpScreen()) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }
  return TRUE;
}
