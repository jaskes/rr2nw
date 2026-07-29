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
#include "obase/vehicle/VehicleRuntimeState.h"

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
    SetSubscribed(true);
  }

  void removeNotify() override {
    SetSubscribed(false);
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
  bool IsSubscribed() const { return m_subscribed; }
  bool Suspend() {
    ClearMotion();
    return SetSubscribed(false);
  }
  bool Resume() { return SetSubscribed(true); }
  const SRecoveredObserverState& state() const { return m_state; }

 private:
  void ClearMotion() {
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
  }

  bool SetSubscribed(bool subscribe) {
    if (m_subscribed == subscribe) return true;
    if (getContext() == nullptr) return false;
    KR_ObjectID hardware = getContext()->searchObject("Hardware");
    if (hardware.isNUL()) return false;

    KR_Event event;
    event.source = getObjectID();
    event.destination = hardware;
    event.label = subscribe ? CTRL_SUBSCRIBE : CTRL_UNSUBSCRIBE;
    event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
    event.data.open(EDO_WRITE).putObjectID(getObjectID());
    if (subscribe) event.data.putInt(EXCLUSIVE);
    event.data.close();
    getContext()->sendEventNow(event);
    m_subscribed = subscribe;
    return true;
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
  bool m_subscribed = false;
};

class RecoveredVehicleControlInput final : public KR_Object {
 public:
  void Reset(const KR_ObjectID& vehicle) {
    m_vehicle = vehicle;
    m_inputEvents = 0;
    m_forwardedEvents = 0;
    m_housekeepingEvents = 0;
    m_ignoredEvents = 0;
    m_focusLosses = 0;
    m_focusGains = 0;
    m_syntheticReleases = 0;
    m_suppressedInputs = 0;
    for (double& value : m_heldActions) value = 0.0;
    m_applicationActive = true;
    m_quitRequested = false;
    m_forwardingFailed = false;
    m_lastInputFailure = 0;
  }

  void addNotify() override { KR_Object::addNotify(); }

  void removeNotify() override {
    SetSubscribed(false);
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
    ++m_inputEvents;

    // Every legacy keyboard translation also emits SYS_KEY before its mapped
    // gameplay action. It is a raw-key notification, not a Vehicle command.
    if (action == SYS_KEY) {
      ++m_housekeepingEvents;
      return 1;
    }

    if (!m_applicationActive) {
      ++m_suppressedInputs;
      return 1;
    }

    if (action == EXIT) {
      if (down > 0.0) {
        m_quitRequested = true;
        if (_gr_hWnd != nullptr) PostMessageA(_gr_hWnd, WM_CLOSE, 0, 0);
      }
      return 1;
    }

    if (getContext() == nullptr || m_vehicle.isNUL() ||
        !getContext()->isExist(m_vehicle) ||
        !VehicleRuntimeState_ApplyLiveControlAt(
            getContext(), action, down, event.timeStamp)) {
      ++m_ignoredEvents;
      m_forwardingFailed = true;
      m_lastInputFailure = VehicleRuntimeState_LastControlFailure();
      return 1;
    }
    ++m_forwardedEvents;
    const int heldIndex = HeldActionIndex(action);
    if (heldIndex >= 0) m_heldActions[heldIndex] = down;
    return 1;
  }

  bool shouldDump() override { return false; }

  bool Subscribe() { return SetSubscribed(true); }
  bool Unsubscribe() { return SetSubscribed(false); }
  bool IsSubscribed() const { return m_subscribed; }
  bool QuitRequested() const { return m_quitRequested; }
  bool ForwardingFailed() const { return m_forwardingFailed; }
  unsigned int InputEvents() const { return m_inputEvents; }
  unsigned int ForwardedEvents() const { return m_forwardedEvents; }
  unsigned int HousekeepingEvents() const { return m_housekeepingEvents; }
  unsigned int IgnoredEvents() const { return m_ignoredEvents; }
  bool ApplicationActive() const { return m_applicationActive; }
  unsigned int FocusLosses() const { return m_focusLosses; }
  unsigned int FocusGains() const { return m_focusGains; }
  unsigned int SyntheticReleases() const { return m_syntheticReleases; }
  unsigned int SuppressedInputs() const { return m_suppressedInputs; }
  unsigned int ActiveActionCount() const {
    unsigned int count = 0;
    for (double value : m_heldActions) {
      if (value != 0.0) ++count;
    }
    return count;
  }
  int LastInputFailure() const { return m_lastInputFailure; }

  bool SetApplicationActive(bool active, double eventTime) {
    if (m_applicationActive == active) return true;
    if (active) {
      m_applicationActive = true;
      ++m_focusGains;
      return true;
    }

    m_applicationActive = false;
    ++m_focusLosses;
    bool succeeded = true;
    for (int index = 0; index < kHeldActionCount; ++index) {
      if (m_heldActions[index] == 0.0) continue;
      if (getContext() == nullptr || m_vehicle.isNUL() ||
          !getContext()->isExist(m_vehicle) ||
          !VehicleRuntimeState_ApplyLiveControlAt(
              getContext(), HeldAction(index), 0.0, eventTime)) {
        m_forwardingFailed = true;
        m_lastInputFailure = VehicleRuntimeState_LastControlFailure();
        succeeded = false;
      } else {
        ++m_syntheticReleases;
      }
      m_heldActions[index] = 0.0;
    }
    return succeeded;
  }

 private:
  static constexpr int kHeldActionCount = 10;

  static int HeldActionIndex(int action) {
    switch (action) {
      case MOVE_FORWARD: return 0;
      case MOVE_BACKWARD: return 1;
      case STRAFE_LEFT: return 2;
      case STRAFE_RIGHT: return 3;
      case STRAFE_UP: return 4;
      case STRAFE_DOWN: return 5;
      case TURN_LEFT: return 6;
      case TURN_RIGHT: return 7;
      case LOOK_UP: return 8;
      case LOOK_DOWN: return 9;
      default: return -1;
    }
  }

  static int HeldAction(int index) {
    static const int actions[kHeldActionCount] = {
        MOVE_FORWARD, MOVE_BACKWARD, STRAFE_LEFT, STRAFE_RIGHT,
        STRAFE_UP, STRAFE_DOWN, TURN_LEFT, TURN_RIGHT,
        LOOK_UP, LOOK_DOWN};
    return actions[index];
  }

  bool SetSubscribed(bool subscribe) {
    if (m_subscribed == subscribe) return true;
    if (getContext() == nullptr) return false;
    KR_ObjectID hardware = getContext()->searchObject("Hardware");
    if (hardware.isNUL()) return false;

    KR_Event event;
    event.source = getObjectID();
    event.destination = hardware;
    event.label = subscribe ? CTRL_SUBSCRIBE : CTRL_UNSUBSCRIBE;
    event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
    event.data.open(EDO_WRITE).putObjectID(getObjectID());
    if (subscribe) event.data.putInt(EXCLUSIVE);
    event.data.close();
    getContext()->sendEventNow(event);
    m_subscribed = subscribe;
    return true;
  }

  KR_ObjectID m_vehicle = KR_ObjectID::NUL();
  unsigned int m_inputEvents = 0;
  unsigned int m_forwardedEvents = 0;
  unsigned int m_housekeepingEvents = 0;
  unsigned int m_ignoredEvents = 0;
  unsigned int m_focusLosses = 0;
  unsigned int m_focusGains = 0;
  unsigned int m_syntheticReleases = 0;
  unsigned int m_suppressedInputs = 0;
  double m_heldActions[kHeldActionCount] = {};
  bool m_applicationActive = true;
  bool m_quitRequested = false;
  bool m_forwardingFailed = false;
  int m_lastInputFailure = 0;
  bool m_subscribed = false;
};

unsigned int g_issues = 0;
bool g_platformReady = false;
bool g_comOwned = false;
bool g_sessionReady = false;
bool g_sessionAttached = false;
bool g_loopReady = false;
bool g_hardwareReady = false;
bool g_windowQuitRequested = false;
bool g_vehicleMovementReady = false;
bool g_vehicleControlReady = false;
bool g_vehicleFallbackActive = false;
unsigned int g_vehicleFrameCount = 0;
unsigned int g_vehicleCameraFrameCount = 0;
unsigned int g_vehicleDroppedTimeFrameCount = 0;
unsigned int g_vehicleFallbackCount = 0;
unsigned int g_vehicleFallbackReason = 0;
unsigned long long g_vehicleRuntimeFingerprint = 0;
int g_vehicleVesselKind = RECOVERED_VEHICLE_VESSEL_UNKNOWN;
SRecoveredVehicleMovementProbeSummary g_vehicleMovementProbe = {};
SRecoveredVehicleDriveTelemetry g_vehicleDriveTelemetry = {};
CFVector3 g_vehicleTelemetryStartPosition(0.0, 0.0, 0.0);
CFVector3 g_vehicleTelemetryStartForward(0.0, 0.0, 1.0);
bool g_vehicleDriveTelemetryReady = false;
RecoveredObserverInput g_observerInput;
RecoveredVehicleControlInput g_vehicleControlInput;

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
      !BindHardwareControl(STOP_VEHICLE, "X") ||
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
  if (message == WM_ACTIVATEAPP && g_vehicleControlReady) {
    const double timerTime = g_timer.GetTime();
    const double eventTime =
        !std::isfinite(timerTime) || timerTime < 0.1 ? 0.1 : timerTime;
    g_vehicleControlInput.SetApplicationActive(wParam != FALSE, eventTime);
  }
  return g_hardware.WndProc(window, message, wParam, lParam);
}

double HorizontalLength(double x, double z) {
  return std::sqrt(x * x + z * z);
}

CFVector3 HorizontalForward(const CFMatrix3x4& direction) {
  CFVector3 forward = direction.Row(2);
  const double length = HorizontalLength(forward.x, forward.z);
  if (!std::isfinite(length) || length <= 1.0e-12) {
    return CFVector3(0.0, 0.0, 1.0);
  }
  return CFVector3(forward.x / length, 0.0, forward.z / length);
}

void UpdateVehicleDriveTelemetry(
    const SRecoveredVehicleRuntimeState& state) {
  const double dx = state.position.x - g_vehicleTelemetryStartPosition.x;
  const double dz = state.position.z - g_vehicleTelemetryStartPosition.z;
  const double horizontalDistance = HorizontalLength(dx, dz);
  const double speedMagnitude =
      std::sqrt(state.speed.x * state.speed.x +
                state.speed.y * state.speed.y +
                state.speed.z * state.speed.z);
  const CFVector3 forward = HorizontalForward(state.direction);
  const double dot = (std::max)(
      -1.0, (std::min)(1.0,
                      forward.x * g_vehicleTelemetryStartForward.x +
                          forward.z * g_vehicleTelemetryStartForward.z));
  const double headingDelta = std::acos(dot);

  g_vehicleDriveTelemetry.positionX = state.position.x;
  g_vehicleDriveTelemetry.positionY = state.position.y;
  g_vehicleDriveTelemetry.positionZ = state.position.z;
  g_vehicleDriveTelemetry.speedX = state.speed.x;
  g_vehicleDriveTelemetry.speedY = state.speed.y;
  g_vehicleDriveTelemetry.speedZ = state.speed.z;
  g_vehicleDriveTelemetry.horizontalDistance = horizontalDistance;
  g_vehicleDriveTelemetry.maximumHorizontalDistance =
      (std::max)(g_vehicleDriveTelemetry.maximumHorizontalDistance,
                 horizontalDistance);
  g_vehicleDriveTelemetry.speedMagnitude = speedMagnitude;
  g_vehicleDriveTelemetry.maximumSpeedMagnitude =
      (std::max)(g_vehicleDriveTelemetry.maximumSpeedMagnitude,
                 speedMagnitude);
  g_vehicleDriveTelemetry.headingDelta = headingDelta;
  g_vehicleDriveTelemetry.maximumHeadingDelta =
      (std::max)(g_vehicleDriveTelemetry.maximumHeadingDelta,
                 headingDelta);
  g_vehicleDriveTelemetry.lastBumpFlags = state.lastBumpFlags;
  g_vehicleDriveTelemetry.touchingGround = state.touchingGround;
  g_vehicleDriveTelemetry.groundContactFrames = static_cast<unsigned int>(
      (std::max)(state.groundContactFrameCount, 0));
  g_vehicleDriveTelemetry.staticCollisionFrames = static_cast<unsigned int>(
      (std::max)(state.staticCollisionFrameCount, 0));
  g_vehicleDriveTelemetry.landCollisionFrames = static_cast<unsigned int>(
      (std::max)(state.landCollisionFrameCount, 0));
  g_vehicleDriveTelemetry.dynamicCollisionFrames = static_cast<unsigned int>(
      (std::max)(state.dynamicCollisionFrameCount, 0));
}

void BeginVehicleDriveTelemetry(
    const SRecoveredVehicleRuntimeState& state) {
  g_vehicleDriveTelemetry = {};
  g_vehicleTelemetryStartPosition = state.position;
  g_vehicleTelemetryStartForward = HorizontalForward(state.direction);
  g_vehicleDriveTelemetryReady = true;
  UpdateVehicleDriveTelemetry(state);
}

void RemoveAttachedObject(SimulationContext* context, KR_Object* object) {
  if (context == nullptr || object == nullptr ||
      object->getContext() != context) {
    return;
  }
  const KR_ObjectID id = object->getObjectID();
  context->removeObject(id);
}

void StopVehicleControl(bool restoreObserver,
                        const CFVector3* observerPosition) {
  SimulationContext* context = g_super.m_context;
  if (context != nullptr) {
    if (g_vehicleControlInput.getContext() == context) {
      g_vehicleControlInput.Unsubscribe();
      RemoveAttachedObject(context, &g_vehicleControlInput);
    }
    VehicleRuntimeState_Reset(context);
    if (restoreObserver && g_observerInput.getContext() == context) {
      if (observerPosition != nullptr) {
        g_observerInput.Reset(*observerPosition);
      }
      g_observerInput.Resume();
    }
  }
  g_vehicleControlReady = false;
}

bool BeginVehicleControl(SimulationContext* context,
                         KR_ObjectID vehicle,
                         const CFVector3& position) {
  if (context == nullptr || vehicle.isNUL() ||
      g_observerInput.getContext() != context ||
      !g_observerInput.IsSubscribed() ||
      !VehicleRuntimeState_IsClean(context)) {
    return false;
  }

  const double timerTime = g_timer.GetTime();
  const double startTime =
      !std::isfinite(timerTime) || timerTime < 0.1 ? 0.1 : timerTime;
  if (!VehicleRuntimeState_Activate(
          context, vehicle, position, startTime)) {
    return false;
  }

  g_vehicleControlInput.Reset(vehicle);
  if (context->addObject("RecoveredVehicleControl",
                         &g_vehicleControlInput).isNUL() ||
      !g_observerInput.Suspend() ||
      !g_vehicleControlInput.Subscribe()) {
    StopVehicleControl(true, &position);
    return false;
  }

  SRecoveredVehicleRuntimeState state = {};
  if (!VehicleRuntimeState_Inspect(context, vehicle, &state) ||
      !state.active || state.frameBegun ||
      !g_vehicleControlInput.IsSubscribed() ||
      g_observerInput.IsSubscribed()) {
    StopVehicleControl(true, &position);
    return false;
  }

  BeginVehicleDriveTelemetry(state);

  g_vehicleControlReady = true;
  g_vehicleFallbackActive = false;
  g_vehicleFrameCount = 0;
  g_vehicleCameraFrameCount = 0;
  g_vehicleDroppedTimeFrameCount = 0;
  g_vehicleFallbackCount = 0;
  g_vehicleFallbackReason = 0;
  return true;
}

bool ActivateVehicleFallback(unsigned int reason) {
  if (g_vehicleFallbackActive) return true;
  CFVector3 position(g_observerInput.state().x,
                     g_observerInput.state().y,
                     g_observerInput.state().z);
  if (g_super.m_context != nullptr) {
    KR_ObjectID vehicle =
        g_super.m_context->searchObject("Vehicle.Default");
    SRecoveredVehicleRuntimeState state = {};
    if (!vehicle.isNUL() && VehicleRuntimeState_Inspect(
            g_super.m_context, vehicle, &state) && state.active &&
        std::isfinite(state.position.x) &&
        std::isfinite(state.position.y) &&
        std::isfinite(state.position.z)) {
      position = state.position;
    }
  }

  StopVehicleControl(true, &position);
  g_vehicleFallbackActive = g_observerInput.IsSubscribed();
  g_vehicleFallbackReason = reason;
  if (g_vehicleFallbackActive) ++g_vehicleFallbackCount;
  Report(RECOVERED_GAME_SERVICES_VEHICLE_CONTROL_FAILURE);
  return g_vehicleFallbackActive;
}

void EndBoundedSession() {
  RecoveredSoftwareGraph_ConfigureWindowMessageHook(nullptr);
  SUA_BindSession(nullptr);

  if (g_super.m_context != nullptr) {
    StopVehicleControl(false, nullptr);
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
  g_vehicleMovementReady = false;
  g_vehicleControlReady = false;
  g_vehicleFallbackActive = false;
  g_vehicleFrameCount = 0;
  g_vehicleCameraFrameCount = 0;
  g_vehicleDroppedTimeFrameCount = 0;
  g_vehicleFallbackCount = 0;
  g_vehicleFallbackReason = 0;
  g_vehicleRuntimeFingerprint = 0;
  g_vehicleVesselKind = RECOVERED_VEHICLE_VESSEL_UNKNOWN;
  g_vehicleMovementProbe = {};
  g_vehicleDriveTelemetry = {};
  g_vehicleTelemetryStartPosition = CFVector3(0.0, 0.0, 0.0);
  g_vehicleTelemetryStartForward = CFVector3(0.0, 0.0, 1.0);
  g_vehicleDriveTelemetryReady = false;
  g_vehicleControlInput.Reset(KR_ObjectID::NUL());

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

    KR_ObjectID vehicle =
        g_super.m_context->searchObject("Vehicle.Default");
    SRecoveredVehicleRuntimeState vehicleState = {};
    const double vehicleStartTime =
        Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
    g_vehicleRuntimeFingerprint =
        VehicleRuntimeState_IdentityFingerprint(g_super.m_context, vehicle);
    if (vehicle.isNUL() || g_vehicleRuntimeFingerprint == 0 ||
        !VehicleRuntimeState_IsKnownRetailIdentity(
            g_super.m_context, vehicle) ||
        !VehicleRuntimeState_Inspect(
            g_super.m_context, vehicle, &vehicleState) ||
        !VehicleRuntimeState_ProbeMovement(
            g_super.m_context, vehicle, observerPosition,
            vehicleStartTime, &g_vehicleMovementProbe) ||
        !VehicleRuntimeState_IsClean(g_super.m_context)) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_VEHICLE_MOVEMENT_FAILURE);
      return;
    }
    g_vehicleVesselKind = vehicleState.vesselKind;
    g_vehicleMovementReady = true;
    if (!BeginVehicleControl(g_super.m_context, vehicle,
                             observerPosition)) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_VEHICLE_CONTROL_FAILURE);
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

bool RecoveredGameServices_ExplosionSubjectReady() {
  return RecoveredArenaSeance_ExplosionSubjectReady();
}

bool RecoveredGameServices_ExplosionImpulseReady() {
  return RecoveredArenaSeance_ExplosionImpulseReady();
}

bool RecoveredGameServices_ExplosionLightReady() {
  return RecoveredArenaSeance_ExplosionLightReady();
}

bool RecoveredGameServices_ExplosionSoundReady() {
  return RecoveredArenaSeance_ExplosionSoundReady();
}

bool RecoveredGameServices_ExplosionParticlesReady() {
  return RecoveredArenaSeance_ExplosionParticlesReady();
}

bool RecoveredGameServices_ExplosionSmokeReady() {
  return RecoveredArenaSeance_ExplosionSmokeReady();
}

bool RecoveredGameServices_ExplosionPieceReady() {
  return RecoveredArenaSeance_ExplosionPieceReady();
}

bool RecoveredGameServices_ExplosionTraceReady() {
  return RecoveredArenaSeance_ExplosionTraceReady();
}

bool RecoveredGameServices_VehicleAttributesReady() {
  return RecoveredArenaSeance_VehicleAttributesReady();
}

bool RecoveredGameServices_VehicleReferencesReady() {
  return RecoveredArenaSeance_VehicleReferencesReady();
}

bool RecoveredGameServices_TaxiAttributesReady() {
  return RecoveredArenaSeance_TaxiAttributesReady();
}

bool RecoveredGameServices_TaxiReferencesReady() {
  return RecoveredArenaSeance_TaxiReferencesReady();
}

bool RecoveredGameServices_BulletAttributesReady() {
  return RecoveredArenaSeance_BulletAttributesReady();
}

bool RecoveredGameServices_BulletReferencesReady() {
  return RecoveredArenaSeance_BulletReferencesReady();
}

bool RecoveredGameServices_BulletSubjectRegistrationReady() {
  return RecoveredArenaSeance_BulletSubjectRegistrationReady();
}

bool RecoveredGameServices_BulletSubjectReady() {
  return RecoveredArenaSeance_BulletSubjectReady();
}

bool RecoveredGameServices_BulletImpactEffectsReady() {
  return RecoveredArenaSeance_BulletImpactEffectsReady();
}

bool RecoveredGameServices_BulletGroundSparkReady() {
  return RecoveredArenaSeance_BulletGroundSparkReady();
}

bool RecoveredGameServices_BulletBarrelSmokeReady() {
  return RecoveredArenaSeance_BulletBarrelSmokeReady();
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

bool RecoveredGameServices_SparkSubjectReady() {
  return RecoveredArenaSeance_SparkSubjectReady();
}

bool RecoveredGameServices_SparkRenderingReady() {
  return RecoveredArenaSeance_SparkAttributesReady() &&
         RecoveredArenaSeance_SparkSubjectReady() &&
         RecoveredArenaSeance_SparkVisualResourcesReady();
}

bool RecoveredGameServices_VehicleReady() {
  return RecoveredArenaSeance_VehicleReady();
}

double RecoveredGameServices_VehicleVesselMass() {
  return RecoveredArenaSeance_VehicleVesselMass();
}

bool RecoveredGameServices_VehicleMovementReady() {
  return g_vehicleMovementReady;
}

unsigned long long RecoveredGameServices_VehicleRuntimeFingerprint() {
  return g_vehicleMovementReady ? g_vehicleRuntimeFingerprint : 0;
}

int RecoveredGameServices_VehicleVesselKind() {
  return g_vehicleMovementReady ? g_vehicleVesselKind
                                : RECOVERED_VEHICLE_VESSEL_UNKNOWN;
}

int RecoveredGameServices_VehicleProbeInvalidActivations() {
  return g_vehicleMovementReady
             ? g_vehicleMovementProbe.invalidActivationRejections
             : -1;
}

int RecoveredGameServices_VehicleProbeActivations() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.activations : -1;
}

int RecoveredGameServices_VehicleProbeStationarySteps() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.stationarySteps : -1;
}

int RecoveredGameServices_VehicleProbeThrottleEvents() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.throttleEvents : -1;
}

int RecoveredGameServices_VehicleProbeMovementSteps() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.movementSteps : -1;
}

int RecoveredGameServices_VehicleProbeTurnEvents() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.turnEvents : -1;
}

int RecoveredGameServices_VehicleProbeCameraTransitions() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.cameraTransitions
                                : -1;
}

int RecoveredGameServices_VehicleProbeRollbacks() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.rollbacks : -1;
}

double RecoveredGameServices_VehicleProbeHorizontalDistance() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.horizontalDistance
                                : 0.0;
}

bool RecoveredGameServices_VehicleControlReady() {
  return g_vehicleControlReady;
}

bool RecoveredGameServices_VehicleFallbackActive() {
  return g_vehicleFallbackActive;
}

unsigned int RecoveredGameServices_VehicleInputEvents() {
  return g_vehicleControlInput.InputEvents();
}

unsigned int RecoveredGameServices_VehicleForwardedEvents() {
  return g_vehicleControlInput.ForwardedEvents();
}

unsigned int RecoveredGameServices_VehicleHousekeepingEvents() {
  return g_vehicleControlInput.HousekeepingEvents();
}

unsigned int RecoveredGameServices_VehicleIgnoredEvents() {
  return g_vehicleControlInput.IgnoredEvents();
}

bool RecoveredGameServices_SetApplicationActive(bool active) {
  if (!g_vehicleControlReady || g_vehicleControlInput.getContext() == nullptr) {
    return false;
  }
  const double timerTime = g_timer.GetTime();
  const double eventTime =
      !std::isfinite(timerTime) || timerTime < 0.1 ? 0.1 : timerTime;
  return g_vehicleControlInput.SetApplicationActive(active, eventTime);
}

bool RecoveredGameServices_VehicleApplicationActive() {
  return g_vehicleControlInput.ApplicationActive();
}

unsigned int RecoveredGameServices_VehicleFocusLossCount() {
  return g_vehicleControlInput.FocusLosses();
}

unsigned int RecoveredGameServices_VehicleFocusGainCount() {
  return g_vehicleControlInput.FocusGains();
}

unsigned int RecoveredGameServices_VehicleSyntheticReleaseCount() {
  return g_vehicleControlInput.SyntheticReleases();
}

unsigned int RecoveredGameServices_VehicleSuppressedInputCount() {
  return g_vehicleControlInput.SuppressedInputs();
}

unsigned int RecoveredGameServices_VehicleActiveActionCount() {
  return g_vehicleControlInput.ActiveActionCount();
}

int RecoveredGameServices_VehicleLastInputFailure() {
  return g_vehicleControlInput.LastInputFailure();
}

bool RecoveredGameServices_VehicleDriveTelemetry(
    SRecoveredVehicleDriveTelemetry* telemetry) {
  if (!g_vehicleDriveTelemetryReady || telemetry == nullptr) return false;
  *telemetry = g_vehicleDriveTelemetry;
  return true;
}

unsigned int RecoveredGameServices_VehicleFrameCount() {
  return g_vehicleFrameCount;
}

unsigned int RecoveredGameServices_VehicleCameraFrameCount() {
  return g_vehicleCameraFrameCount;
}

unsigned int RecoveredGameServices_VehicleDroppedTimeFrameCount() {
  return g_vehicleDroppedTimeFrameCount;
}

unsigned int RecoveredGameServices_VehicleFallbackCount() {
  return g_vehicleFallbackCount;
}

unsigned int RecoveredGameServices_VehicleFallbackReason() {
  return g_vehicleFallbackReason;
}

bool RecoveredGameServices_QuitRequested() {
  return g_observerInput.QuitRequested() ||
         g_vehicleControlInput.QuitRequested() ||
         g_windowQuitRequested;
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
         RecoveredGameServices_ExplosionSubjectReady() &&
         RecoveredGameServices_ExplosionImpulseReady() &&
         RecoveredGameServices_ExplosionLightReady() &&
         RecoveredGameServices_ExplosionSoundReady() &&
         RecoveredGameServices_ExplosionParticlesReady() &&
         RecoveredGameServices_ExplosionSmokeReady() &&
         RecoveredGameServices_ExplosionPieceReady() &&
         RecoveredGameServices_ExplosionTraceReady() &&
         RecoveredGameServices_VehicleAttributesReady() &&
         RecoveredGameServices_VehicleReferencesReady() &&
         RecoveredGameServices_TaxiAttributesReady() &&
         RecoveredGameServices_TaxiReferencesReady() &&
         RecoveredGameServices_BulletAttributesReady() &&
         RecoveredGameServices_BulletReferencesReady() &&
         RecoveredGameServices_BulletSubjectRegistrationReady() &&
         RecoveredGameServices_BulletSubjectReady() &&
         RecoveredGameServices_BulletImpactEffectsReady() &&
         RecoveredGameServices_BulletGroundSparkReady() &&
         RecoveredGameServices_BulletBarrelSmokeReady() &&
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
         RecoveredGameServices_SparkSubjectReady() &&
         RecoveredGameServices_SparkRenderingReady() &&
         RecoveredGameServices_RouteReady() &&
         RecoveredGameServices_VehicleReady() &&
         RecoveredGameServices_VehicleMovementReady() &&
         (RecoveredGameServices_VehicleControlReady() ||
          RecoveredGameServices_VehicleFallbackActive()) &&
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
  bool vehicleFrame = g_vehicleControlReady;
  if (vehicleFrame && g_vehicleFrameCount == 0) {
    const double timerTime = g_timer.GetTime();
    if (!VehicleRuntimeState_SynchronizeFirstFrame(
            g_super.m_context,
            !std::isfinite(timerTime) || timerTime < 0.1
                ? 0.1
                : timerTime)) {
      if (!ActivateVehicleFallback(1)) return FALSE;
      vehicleFrame = false;
    }
  }
  if (vehicleFrame &&
      !VehicleRuntimeState_BeginFrame(g_super.m_context)) {
    if (!ActivateVehicleFallback(2)) return FALSE;
    vehicleFrame = false;
  }
  SUA_ProcessEvents();
  if (vehicleFrame) {
    bool droppedTime = false;
    if (g_vehicleControlInput.ForwardingFailed()) {
      if (!ActivateVehicleFallback(3)) return FALSE;
      vehicleFrame = false;
    } else if (!VehicleRuntimeState_CompleteLiveFrame(
            g_super.m_context, Session::m_viewTime,
            &droppedTime)) {
      if (!ActivateVehicleFallback(4)) return FALSE;
      vehicleFrame = false;
    } else {
      ++g_vehicleFrameCount;
      if (droppedTime) ++g_vehicleDroppedTimeFrameCount;
      SRecoveredVehicleRuntimeState telemetryState = {};
      if (!VehicleRuntimeState_Inspect(
              g_super.m_context,
              g_super.m_context->searchObject("Vehicle.Default"),
              &telemetryState)) {
        if (!ActivateVehicleFallback(6)) return FALSE;
        vehicleFrame = false;
      } else {
        UpdateVehicleDriveTelemetry(telemetryState);
      }
    }
  }
  if (!vehicleFrame) g_observerInput.Advance(Session::m_frameSec);

  Frame_ClearRuntimeIssues();
  if (!GRStartScene()) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }

  CViewDynamicList dynamics;
  CFMatrix3x4 direction;
  if (g_vehicleControlReady) {
    if (!VehicleRuntimeState_BuildCamera(
            g_super.m_context, &direction)) {
      if (!ActivateVehicleFallback(5)) {
        Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
        return FALSE;
      }
      g_observerInput.BuildCamera(&direction);
    } else {
      ++g_vehicleCameraFrameCount;
    }
  } else {
    g_observerInput.BuildCamera(&direction);
  }
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
