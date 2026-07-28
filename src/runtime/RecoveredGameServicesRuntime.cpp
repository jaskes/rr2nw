#include "RecoveredGameServicesRuntime.h"

#include <algorithm>
#include <cmath>
#include <new>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__SCENE
#include "game.h"
#include "dmap.h"
#include "graph.h"
#include "hardware.h"
#include "h/super.h"
#include "kernel/h/session.h"
#include "message/hardmsg.h"
#include "suavik.h"
#include "zav.h"

#include "FrameRuntimeState.h"
#include "GameEntryRuntimeState.h"
#include "RecoveredArenaSeanceRuntime.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredSoftwareFrame.h"
#include "RecoveredSoftwareGraph.h"
#include "SupervisorShutdownState.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokerSubjectState.h"

namespace {

class RecoveredObserverInput final : public KR_Object {
 public:
  RecoveredObserverInput() { Reset(CFVector3(0.0, 0.0, 0.0)); }

  void Reset(const CFVector3& position) {
    m_state = {position.x, position.y, position.z, 0.0, 0.0, 0};
    m_forward = 0.0;
    m_backward = 0.0;
    m_left = 0.0;
    m_right = 0.0;
    m_up = 0.0;
    m_down = 0.0;
    m_turnLeft = 0.0;
    m_turnRight = 0.0;
    m_lookUp = 0.0;
    m_lookDown = 0.0;
    m_quitRequested = false;
  }

  void addNotify() override {
    KR_Object::addNotify();
    Subscribe(CTRL_SUBSCRIBE);
  }

  void removeNotify() override {
    Subscribe(CTRL_UNSUBSCRIBE);
    KR_Object::removeNotify();
  }

  int receiveEvent(KR_Event& event) override {
    if (event.label == KR_WAKE_UP || event.label == CTRL_CHAR ||
        event.label == CTRL_MOUSE_MOVE_MSG ||
        event.label == CTRL_JOYSTICK_MOVE_MSG) {
      return 1;
    }
    if (event.label != CTRL_BUTTONS_MSG) return 0;

    int action = -1;
    double down = 0.0;
    int code = 0;
    int repeat = 0;
    event.data.open(EDO_READ)
        .getInt(action)
        .getDouble(down)
        .getInt(code)
        .getInt(repeat)
        .close();
    (void)code;
    (void)repeat;
    ++m_state.inputEvents;

    switch (action) {
      case MOVE_FORWARD:
        m_forward = down;
        break;
      case MOVE_BACKWARD:
        m_backward = down;
        break;
      case STRAFE_LEFT:
        m_left = down;
        break;
      case STRAFE_RIGHT:
        m_right = down;
        break;
      case STRAFE_UP:
        m_up = down;
        break;
      case STRAFE_DOWN:
        m_down = down;
        break;
      case TURN_LEFT:
        m_turnLeft = down;
        break;
      case TURN_RIGHT:
        m_turnRight = down;
        break;
      case LOOK_UP:
        m_lookUp = down;
        break;
      case LOOK_DOWN:
        m_lookDown = down;
        break;
      case EXIT:
        if (down > 0.0) {
          m_quitRequested = true;
          if (_gr_hWnd != nullptr) PostMessageA(_gr_hWnd, WM_CLOSE, 0, 0);
        }
        break;
      default:
        break;
    }
    return 1;
  }

  bool shouldDump() override { return false; }

  void Advance(double deltaTime) {
    const double boundedDelta =
        (std::max)(0.0, (std::min)(deltaTime, 0.1));
    const double turnSpeed = 1.5;
    const double moveSpeed = 320.0;
    m_state.yaw +=
        (m_turnRight - m_turnLeft) * turnSpeed * boundedDelta;
    m_state.pitch +=
        (m_lookUp - m_lookDown) * turnSpeed * boundedDelta;
    m_state.pitch = (std::max)(-1.4, (std::min)(m_state.pitch, 1.4));

    const double forward = m_forward - m_backward;
    const double strafe = m_right - m_left;
    const double vertical = m_up - m_down;
    const double distance = moveSpeed * boundedDelta;
    const double sinYaw = std::sin(m_state.yaw);
    const double cosYaw = std::cos(m_state.yaw);
    m_state.x += (sinYaw * forward + cosYaw * strafe) * distance;
    m_state.y += vertical * distance;
    m_state.z += (-cosYaw * forward + sinYaw * strafe) * distance;
  }

  void BuildCamera(CFMatrix3x4* direction) const {
    direction->LoadIdentity();
    direction->RotateOxL(-m_state.pitch);
    direction->RotateOyL(-m_state.yaw);
    const CFVector3 inversePosition(-m_state.x, -m_state.y, -m_state.z);
    direction->TranslateR(inversePosition);
  }

  bool QuitRequested() const { return m_quitRequested; }
  const SRecoveredObserverState& state() const { return m_state; }

 private:
  void Subscribe(int label) {
    if (getContext() == nullptr) return;
    KR_ObjectID hardware = getContext()->searchObject("Hardware");
    if (hardware.isNUL()) return;

    KR_Event event;
    event.source = getObjectID();
    event.destination = hardware;
    event.label = label;
    event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
    event.data.open(EDO_WRITE).putObjectID(getObjectID());
    if (label == CTRL_SUBSCRIBE) event.data.putInt(EXCLUSIVE);
    event.data.close();
    getContext()->sendEventNow(event);
  }

  SRecoveredObserverState m_state = {};
  double m_forward = 0.0;
  double m_backward = 0.0;
  double m_left = 0.0;
  double m_right = 0.0;
  double m_up = 0.0;
  double m_down = 0.0;
  double m_turnLeft = 0.0;
  double m_turnRight = 0.0;
  double m_lookUp = 0.0;
  double m_lookDown = 0.0;
  bool m_quitRequested = false;
};

unsigned int g_issues = 0;
bool g_platformReady = false;
bool g_comOwned = false;
bool g_sessionReady = false;
bool g_sessionAttached = false;
bool g_loopReady = false;
bool g_hardwareReady = false;
bool g_windowQuitRequested = false;
RecoveredObserverInput g_observerInput;

void Report(unsigned int issue) { g_issues |= issue; }

bool BindHardwareControl(int action, const char* keyName) {
  const int code = g_hardware.SearchCode(keyName);
  if (code < 0) return false;
  g_hardware.SetControl(action, code);
  return true;
}

bool ConfigureHardwareControls() {
  g_hardware.ClearCache();
  if (!BindHardwareControl(MOVE_FORWARD, "W") ||
      !BindHardwareControl(MOVE_BACKWARD, "S") ||
      !BindHardwareControl(STRAFE_LEFT, "A") ||
      !BindHardwareControl(STRAFE_RIGHT, "D") ||
      !BindHardwareControl(STRAFE_UP, "Space") ||
      !BindHardwareControl(STRAFE_DOWN, "LCtrl") ||
      !BindHardwareControl(TURN_LEFT, "Left") ||
      !BindHardwareControl(TURN_RIGHT, "Right") ||
      !BindHardwareControl(LOOK_UP, "Up") ||
      !BindHardwareControl(LOOK_DOWN, "Down") ||
      !BindHardwareControl(EXIT, "Esc")) {
    return false;
  }
  g_hardware.Link();
  return true;
}

LRESULT ForwardWindowMessageToHardware(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
  if (!g_hardwareReady || g_hardware.getContext() == nullptr) {
    return DefWindowProcA(window, message, wParam, lParam);
  }
  return g_hardware.WndProc(window, message, wParam, lParam);
}

void RemoveAttachedObject(SimulationContext* context, KR_Object* object) {
  if (context == nullptr || object == nullptr ||
      object->getContext() != context) {
    return;
  }
  const KR_ObjectID id = object->getObjectID();
  context->removeObject(id);
}

void EndBoundedSession() {
  RecoveredSoftwareGraph_ConfigureWindowMessageHook(nullptr);
  SUA_BindSession(nullptr);

  if (g_super.m_context != nullptr) {
    // Arena owns all script-created class-table objects. Release that graph
    // while its context and the legacy services it may notify still exist.
    RecoveredArenaSeance_Release();
    // Subscribers must leave while Hardware can still accept unsubscribe
    // events. The legacy clearObjects() walks allocation order instead.
    RemoveAttachedObject(g_super.m_context, &g_super.m_level);
    RemoveAttachedObject(g_super.m_context, &g_debugMap);
    RemoveAttachedObject(g_super.m_context, &g_observerInput);
    RemoveAttachedObject(g_super.m_context, &g_hardware);
    RemoveAttachedObject(g_super.m_context, g_super.m_publisher);
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
  g_hardwareReady = false;
  g_windowQuitRequested = false;

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
    g_windowQuitRequested = false;
    Session::m_realTimer = &g_timer;
    g_hardware.m_ctrlUse.keyboard = TRUE;
    g_hardware.m_ctrlUse.mouse = FALSE;
    g_hardware.m_ctrlUse.joystick = FALSE;
    Session::m_hardware = &g_hardware;

    // Preserve the capacities used by the retail Supervisor::startSeance().
    // Small synthetic pools can exhaust once the complete WAV and Skin
    // rosters coexist, and the legacy full-pool rollback is not reliable.
    g_super.m_context = new SimulationContext(4000, 5000);
    g_super.m_publisher = new Publisher();
    g_super.m_context->addObject("Publisher", g_super.m_publisher);
    if (g_super.m_context->addObject("Hardware", &g_hardware).isNUL() ||
        !ConfigureHardwareControls()) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_SESSION_FAILURE);
      return;
    }
    g_hardwareReady = true;

    CFVector3 observerPosition(0.0, 0.0, 0.0);
    if (ZAV_Config()("Vessel", "Init", "%lg %lg %lg", &observerPosition.x,
                     &observerPosition.y, &observerPosition.z) != 3) {
      observerPosition = CFVector3(0.0, 0.0, 0.0);
    }
    g_observerInput.Reset(observerPosition);
    if (g_super.m_context
            ->addObject("RecoveredObserver", &g_observerInput)
            .isNUL()) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_SESSION_FAILURE);
      return;
    }
    g_super.m_context->addObject("DebugMap", &g_debugMap);
    g_super.m_context->addObject("LEVEL", &g_super.m_level);
    g_super.m_session.Add(g_super.m_context);
    g_sessionAttached = true;
    SUA_BindSession(&g_super.m_session);
    if (!RecoveredArenaSeance_Initialize(g_super.m_context,
                                         Session::m_moment)) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_SEANCE_FAILURE);
      return;
    }

    Frame_BindRecoveredSoftware();
    RecoveredSoftwareGraph_ConfigureWindowMessageHook(
        ForwardWindowMessageToHardware);
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
  // A legacy input device may continuously generate motion messages. Keep
  // one frame from being starved by an always-nonempty Windows queue.
  constexpr unsigned int kMaximumMessagesPerFrame = 256;
  for (unsigned int handled = 0; handled < kMaximumMessagesPerFrame;
       ++handled) {
    if (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE) == FALSE) break;
    if (message.message == WM_QUIT) {
      g_windowQuitRequested = true;
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

bool RecoveredGameServices_HardwareReady() { return g_hardwareReady; }

bool RecoveredGameServices_SeanceReady() {
  return RecoveredArenaSeance_IsOpen() &&
         RecoveredArenaSeance_ScriptCompleted();
}

bool RecoveredGameServices_RouteReady() {
  return RecoveredArenaSeance_RouteReady();
}

bool RecoveredGameServices_BirdAttributesReady() {
  return RecoveredArenaSeance_BirdAttributesReady();
}

bool RecoveredGameServices_PortalReady() {
  return RecoveredArenaSeance_PortalReady();
}

bool RecoveredGameServices_OrphanAttributesReady() {
  return RecoveredArenaSeance_OrphanAttributesReady();
}

bool RecoveredGameServices_ArtefactAttributesReady() {
  return RecoveredArenaSeance_ArtefactAttributesReady();
}

bool RecoveredGameServices_SmokeAttributesReady() {
  return RecoveredArenaSeance_SmokeAttributesReady();
}

bool RecoveredGameServices_SmokeSubjectReady() {
  return RecoveredArenaSeance_SmokeSubjectReady();
}

bool RecoveredGameServices_SmokeTerrainReady() {
  return g_super.m_context != nullptr && RecoveredDrawableScene_IsReady() &&
         SmokeSubjectState_SimulationSupported(
             g_super.m_context, "Smoke.Attr.FireArea");
}

bool RecoveredGameServices_SmokeRenderingReady() {
  return RecoveredGameServices_SmokeTerrainReady() &&
         RecoveredArenaSeance_SmokeVisualResourcesReady() &&
         SmokeSubjectState_RenderingSupported(
             g_super.m_context, "Smoke.Attr.Trace") &&
         SmokeSubjectState_RenderingSupported(
             g_super.m_context, "Smoke.Attr.FireArea");
}

bool RecoveredGameServices_SmokeVisualResourcesReady() {
  return RecoveredArenaSeance_SmokeVisualResourcesReady();
}

bool RecoveredGameServices_ExplosionAttributesReady() {
  return RecoveredArenaSeance_ExplosionAttributesReady();
}

bool RecoveredGameServices_VehicleAttributesReady() {
  return RecoveredArenaSeance_VehicleAttributesReady();
}

bool RecoveredGameServices_TaxiAttributesReady() {
  return RecoveredArenaSeance_TaxiAttributesReady();
}

bool RecoveredGameServices_TaxiReferencesReady() {
  return RecoveredArenaSeance_TaxiReferencesReady();
}

bool RecoveredGameServices_FarterAttributesReady() {
  return RecoveredArenaSeance_FarterAttributesReady();
}

bool RecoveredGameServices_FarterReferencesReady() {
  return RecoveredArenaSeance_FarterReferencesReady();
}

bool RecoveredGameServices_FarterRuntimeReady() {
  return RecoveredArenaSeance_FarterRuntimeReady();
}

bool RecoveredGameServices_FarterSubjectReady() {
  return RecoveredArenaSeance_FarterSubjectReady();
}

bool RecoveredGameServices_LampAttributesReady() {
  return RecoveredArenaSeance_LampAttributesReady();
}

bool RecoveredGameServices_CorpseAttributesReady() {
  return RecoveredArenaSeance_CorpseAttributesReady();
}

bool RecoveredGameServices_CorpseReferencesReady() {
  return RecoveredArenaSeance_CorpseReferencesReady();
}

bool RecoveredGameServices_CorpseRuntimeReady() {
  return RecoveredArenaSeance_CorpseRuntimeReady();
}

bool RecoveredGameServices_CorpseSubjectReady() {
  return RecoveredArenaSeance_CorpseSubjectReady();
}

bool RecoveredGameServices_SmokerAttributesReady() {
  return RecoveredArenaSeance_SmokerAttributesReady();
}

bool RecoveredGameServices_SmokerReferencesReady() {
  return RecoveredArenaSeance_SmokerReferencesReady();
}

bool RecoveredGameServices_SmokerRuntimeReady() {
  return RecoveredArenaSeance_SmokerRuntimeReady();
}

bool RecoveredGameServices_SmokerEmissionReady() {
  return RecoveredGameServices_SmokerRuntimeReady() &&
         RecoveredGameServices_DynSmokerReady() &&
         RecoveredGameServices_SmokeRenderingReady() &&
         SmokerSubjectState_EmissionSupported(
             g_super.m_context, "Smoker.Attr.Corpse") &&
         SmokerSubjectState_EmissionSupported(
             g_super.m_context, "Smoker.Attr.FireArea");
}

bool RecoveredGameServices_SmokerLightCoronaReady() {
  return RecoveredGameServices_SmokerEmissionReady() &&
         SmokerSubjectState_LightCoronaSupported(
             g_super.m_context, "Smoker.Attr.FireMd");
}

bool RecoveredGameServices_DynSmokerReady() {
  return RecoveredArenaSeance_DynSmokerReady();
}

bool RecoveredGameServices_WavMetadataReady() {
  return RecoveredArenaSeance_WavMetadataReady();
}

bool RecoveredGameServices_SoundObjectReady() {
  return RecoveredArenaSeance_SoundObjectReady();
}

bool RecoveredGameServices_SkinResourcesReady() {
  return RecoveredArenaSeance_SkinResourcesReady();
}

bool RecoveredGameServices_SparkAttributesReady() {
  return RecoveredArenaSeance_SparkAttributesReady();
}

bool RecoveredGameServices_VehicleReady() {
  return RecoveredArenaSeance_VehicleReady();
}

bool RecoveredGameServices_QuitRequested() {
  return g_observerInput.QuitRequested() || g_windowQuitRequested;
}

bool RecoveredGameServices_IsReady() {
  return g_platformReady && g_sessionReady && g_loopReady && g_hardwareReady &&
         RecoveredGameServices_SeanceReady() &&
         RecoveredGameServices_BirdAttributesReady() &&
         RecoveredGameServices_PortalReady() &&
         RecoveredGameServices_OrphanAttributesReady() &&
         RecoveredGameServices_ArtefactAttributesReady() &&
         RecoveredGameServices_SmokeAttributesReady() &&
         RecoveredGameServices_SmokeSubjectReady() &&
         RecoveredGameServices_SmokeTerrainReady() &&
         RecoveredGameServices_SmokeRenderingReady() &&
         RecoveredGameServices_ExplosionAttributesReady() &&
         RecoveredGameServices_VehicleAttributesReady() &&
         RecoveredGameServices_TaxiAttributesReady() &&
         RecoveredGameServices_TaxiReferencesReady() &&
         RecoveredGameServices_FarterAttributesReady() &&
         RecoveredGameServices_FarterReferencesReady() &&
         RecoveredGameServices_FarterRuntimeReady() &&
         RecoveredGameServices_FarterSubjectReady() &&
         RecoveredGameServices_LampAttributesReady() &&
         RecoveredGameServices_CorpseAttributesReady() &&
         RecoveredGameServices_CorpseReferencesReady() &&
         RecoveredGameServices_CorpseRuntimeReady() &&
         RecoveredGameServices_CorpseSubjectReady() &&
         RecoveredGameServices_SmokerAttributesReady() &&
         RecoveredGameServices_SmokerReferencesReady() &&
         RecoveredGameServices_DynSmokerReady() &&
         RecoveredGameServices_SmokerEmissionReady() &&
         RecoveredGameServices_SmokerLightCoronaReady() &&
         RecoveredGameServices_WavMetadataReady() &&
         RecoveredGameServices_SoundObjectReady() &&
         RecoveredGameServices_SkinResourcesReady() &&
         RecoveredGameServices_SparkAttributesReady() &&
         RecoveredGameServices_RouteReady() &&
         RecoveredGameServices_VehicleReady() &&
         RecoveredGameLevel_IsReady() && Frame_RuntimeReady(false);
}

unsigned int RecoveredGameServices_Issues() { return g_issues; }

const SRecoveredObserverState* RecoveredGameServices_ObserverState() {
  return g_sessionReady ? &g_observerInput.state() : nullptr;
}

int RecoveredGameServices_RunFrame() {
  if (!RecoveredGameServices_IsReady()) {
    Report(RECOVERED_GAME_SERVICES_BEGIN_LOOP_FAILURE);
    return FALSE;
  }
  if (!PumpMessages()) return FALSE;
  SUA_ProcessEvents();
  g_observerInput.Advance(Session::m_frameSec);

  Frame_ClearRuntimeIssues();
  if (!GRStartScene()) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }

  CViewDynamicList dynamics;
  CFMatrix3x4 direction;
  g_observerInput.BuildCamera(&direction);
  SUA_BeginRender(ZAV_Scene(), dynamics);
  g_debugMap.Draw();
  ZAV_RenderFrame(&direction, dynamics);
  ZAV_PrintFrameInfo();
  SUA_EndRender(ZAV_Scene());
  ZAV_EndRenderFrame();
  ZAV_NextFrame();

  if (Frame_RuntimeIssues() != 0 || !GRDumpScreen()) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }
  return TRUE;
}
