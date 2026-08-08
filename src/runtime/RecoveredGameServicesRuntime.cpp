#include "RecoveredGameServicesRuntime.h"

#include "ActiveWorldSave.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <iomanip>
#include <limits>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#define LAST_H__SCENE
#include "game.h"
#include "briefing.h"
#include "dmap.h"
#include "font.h"
#include "graph.h"
#include "hardware.h"
#include "h/super.h"
#include "h/vehicle.h"
#include "kernel/h/session.h"
#include "message/hardmsg.h"
#include "olevel.h"
#include "suavik.h"
#include "zav.h"

#include "FrameRuntimeState.h"
#include "GameEntryRuntimeState.h"
#include "LevelContinuation.h"
#include "LevelSaveSlot.h"
#include "MissionActiveWorldState.h"
#include "RecoveredArenaSeanceRuntime.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredFramePreview.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredModRuntime.h"
#include "RecoveredRetailScriptManifest.h"
#include "RecoveredSaveSlotCatalog.h"
#include "RecoveredSaveSlotDialog.h"
#include "RecoveredSoftwareFrame.h"
#include "RecoveredSoftwareGraph.h"
#include "RecoveredWindowsInputAdapter.h"
#include "SupervisorShutdownState.h"
#include "TimeRuntimeState.h"
#include "VehicleControlJournal.h"
#include "VehicleControlReplayProbe.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/corpse/CorpseSubjectState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/orphan/OrphanActiveWorldState.h"
#include "obase/orphan/OrphanSubjectState.h"
#include "obase/people/PeopleSubjectState.h"
#include "obase/portal/PortalActiveWorldState.h"
#include "obase/recrcen/RecruitCenterSubjectState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "sound.h"
#include "obase/spark/SparkSubjectState.h"
#include "obase/taxi/TaxiSubjectState.h"
#include "obase/vehicle/VehicleRuntimeState.h"

extern int g_godMode;
extern unsigned char _currPalette[256u * 3u];

void RecoveredGameServices_RefreshBriefingViewport() {
  if (g_super.m_context != nullptr &&
      g_briefing.getContext() == g_super.m_context) {
    g_briefing.ChangeResEvent();
  }
}

bool RecoveredGameServices_PlayLevelBriefing(const char* resolvedPath) {
  if (resolvedPath == nullptr || resolvedPath[0] == '\0' ||
      g_super.m_context == nullptr ||
      g_briefing.getContext() != g_super.m_context ||
      !g_super.m_context->isExist("Briefing") || g_vehicle == nullptr)
    return false;
  g_briefing.PlayBriefing(resolvedPath);
  return true;
}

namespace {

bool CanonicalizeDirectionalAction(
    SRecoveredObserverAxes* axes, int action, double value,
    int* canonicalAction, double* canonicalValue) {
  if (axes == nullptr || canonicalAction == nullptr ||
      canonicalValue == nullptr ||
      !RecoveredObserverAxes_ApplyLegacyAction(axes, action, value)) {
    return false;
  }

  switch (action) {
    case MOVE_FORWARD:
    case MOVE_BACKWARD:
      *canonicalAction = MOVE_FORWARD;
      *canonicalValue = axes->forward;
      return true;
    case STRAFE_LEFT:
    case STRAFE_RIGHT:
      *canonicalAction = STRAFE_RIGHT;
      *canonicalValue = axes->strafe;
      return true;
    case STRAFE_UP:
    case STRAFE_DOWN:
      *canonicalAction = STRAFE_UP;
      *canonicalValue = axes->vertical;
      return true;
    case TURN_LEFT:
    case TURN_RIGHT:
      *canonicalAction = TURN_RIGHT;
      *canonicalValue = axes->turn;
      return true;
    case LOOK_UP:
    case LOOK_DOWN:
      *canonicalAction = LOOK_UP;
      *canonicalValue = axes->look;
      return true;
    default:
      return false;
  }
}

class RecoveredObserverInput final : public KR_Object {
 public:
  RecoveredObserverInput() { Reset(CFVector3(0.0, 0.0, 0.0)); }

  void Reset(const CFVector3& position) {
    m_state = {position.x, position.y, position.z, 0.0, 0.0, 0};
    m_axes = {};
    m_applicationActive = true;
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

    if (action == EXIT) {
      if (down > 0.0) {
        m_quitRequested = true;
        if (_gr_hWnd != nullptr) PostMessageA(_gr_hWnd, WM_CLOSE, 0, 0);
      }
      return 1;
    }

    if (!m_applicationActive) return 1;
    RecoveredObserverAxes_ApplyLegacyAction(&m_axes, action, down);
    return 1;
  }

  bool shouldDump() override { return false; }

  void Advance(double deltaTime) {
    const double boundedDelta =
        (std::max)(0.0, (std::min)(deltaTime, 0.1));
    const double turnSpeed = 1.5;
    const double moveSpeed = 320.0;
    m_state.yaw += m_axes.turn * turnSpeed * boundedDelta;
    m_state.pitch += m_axes.look * turnSpeed * boundedDelta;
    m_state.pitch = (std::max)(-1.4, (std::min)(m_state.pitch, 1.4));

    const double distance = moveSpeed * boundedDelta;
    const double sinYaw = std::sin(m_state.yaw);
    const double cosYaw = std::cos(m_state.yaw);
    m_state.x +=
        (sinYaw * m_axes.forward + cosYaw * m_axes.strafe) * distance;
    m_state.y += m_axes.vertical * distance;
    m_state.z +=
        (-cosYaw * m_axes.forward + sinYaw * m_axes.strafe) * distance;
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
  void SetApplicationActive(bool active) {
    if (m_applicationActive == active) return;
    m_applicationActive = active;
    if (!active) ClearMotion();
  }
  const SRecoveredObserverState& state() const { return m_state; }

 private:
  void ClearMotion() { m_axes = {}; }

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
  SRecoveredObserverAxes m_axes = {};
  bool m_applicationActive = true;
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
    m_physicalReconciliations = 0;
    for (double& value : m_heldActions) value = 0.0;
    m_directionalAxes = {};
    m_applicationActive = true;
    m_quitRequested = false;
    m_forwardingFailed = false;
    m_lastInputFailure = 0;
    m_handoffAttempts = 0;
    m_handoffPending = false;
    m_handoffSuccesses = 0;
    m_handoffNoTargets = 0;
    m_handoffRemovedTaxis = 0;
    m_handoffPanelOpens = 0;
    m_handoffPanelDrawBaseline = 0;
    m_handoffPostFrames = 0;
    m_handoffPostDistance = 0.0;
    m_handoffAttribute = KR_ObjectID::NUL();
    m_handoffTaxiCount = 0;
    m_handoffPosition = CFVector3(0.0, 0.0, 0.0);
    m_primaryFirePresses = 0;
    m_secondaryFirePresses = 0;
    m_secondaryFireAcceptedShots = 0;
    m_secondaryAmmoTracking = false;
    m_secondaryTrackedAttribute = KR_ObjectID::NUL();
    m_secondaryLastAmmo = 0;
    m_jumpPresses = 0;
    m_exitAttempts = 0;
    m_exitPending = false;
    m_exitSafeCompletions = 0;
    m_exitUnsafeCompletions = 0;
    m_exitDroppedTaxis = 0;
    m_exitDroppedOrphans = 0;
    m_exitTaxiCount = 0;
    m_exitOrphanCount = 0;
    m_exitAttribute = KR_ObjectID::NUL();
    m_exitPanelWasOpen = false;
    m_exitPanelCloses = 0;
    m_reentryAttempts = 0;
    m_reentryCompletions = 0;
    m_reentryPanelOpens = 0;
    m_controlJournal = {};
    m_controlJournalRecording = false;
    m_controlJournalAppendFailures = 0;
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

    Vehicle* controlledVehicle =
        getContext() == nullptr || m_vehicle.isNUL()
            ? nullptr
            : static_cast<Vehicle*>(
                  getContext()->queryInterface(m_vehicle, IVehicleIID));
    if (controlledVehicle != nullptr && Vehicle::m_dead) {
      // The original Vehicle rejects gameplay controls after death. Keep the
      // modern semantic owner equally quiescent so a dead save cannot acquire
      // freshly held axes which would reappear after a later recovery.
      ++m_suppressedInputs;
      return 1;
    }

    if (action == FIRE_PRIMARY && down > 0.0)
      ++m_primaryFirePresses;
    if (action == FIRE_SECONDARY && down > 0.0)
      ++m_secondaryFirePresses;
    if (action == JUMP && down > 0.0)
      ++m_jumpPresses;

    const SRecoveredObserverAxes previousDirectionalAxes =
        m_directionalAxes;
    int canonicalAction = action;
    double canonicalDown = down;
    const bool directionalAction = CanonicalizeDirectionalAction(
            &m_directionalAxes, action, down,
            &canonicalAction, &canonicalDown);
    if (directionalAction) {
      action = canonicalAction;
      down = canonicalDown;
    }

    STaxiVehicleProximityState proximity = {};
    SRecoveredVehicleRuntimeState before = {};
    Vehicle* currentVehicle = controlledVehicle;
    const bool handoffAttempt = action == CHANGE_VEHICLE && down > 0.0 &&
        currentVehicle != nullptr && currentVehicle->taxiChangeEnabled();
    const bool exitAttempt = action == CHANGE_VEHICLE && down > 0.0 &&
        currentVehicle != nullptr && !currentVehicle->taxiChangeEnabled();
    if (handoffAttempt) {
      ++m_handoffAttempts;
      ++m_reentryAttempts;
      if (!TaxiSubjectState_InspectVehicleProximity(
              getContext(), m_vehicle, &proximity) ||
          !VehicleRuntimeState_Inspect(getContext(), m_vehicle, &before)) {
        ++m_ignoredEvents;
        m_forwardingFailed = true;
        m_lastInputFailure = 7;
        return 1;
      }
    }
    if (exitAttempt) {
      if (!VehicleRuntimeState_Inspect(getContext(), m_vehicle, &before)) {
        ++m_ignoredEvents;
        m_forwardingFailed = true;
        m_lastInputFailure = 8;
        return 1;
      }
      ++m_exitAttempts;
      m_exitPending = true;
      m_exitAttribute = before.attribute;
      m_exitTaxiCount = TaxiSubjectState_LiveCount();
      m_exitOrphanCount = OrphanSubjectState_LiveCount();
      m_exitPanelWasOpen = currentVehicle->panelOpen();
    }

    if (getContext() == nullptr || m_vehicle.isNUL() ||
        !getContext()->isExist(m_vehicle) ||
        !VehicleRuntimeState_ApplyLiveControlAt(
            getContext(), action, down, event.timeStamp)) {
      ++m_ignoredEvents;
      m_forwardingFailed = true;
      m_lastInputFailure = VehicleRuntimeState_LastControlFailure();
      if (directionalAction) m_directionalAxes = previousDirectionalAxes;
      if (exitAttempt) m_exitPending = false;
      return 1;
    }
    ++m_forwardedEvents;
    if (m_controlJournalRecording &&
        !VehicleControlJournal_AppendAction(
            &m_controlJournal, Session::m_simulationTick,
            VehicleRuntimeState_LastAppliedControlTime(), action, down)) {
      ++m_controlJournalAppendFailures;
      m_controlJournalRecording = false;
    }
    if (handoffAttempt) {
      if (proximity.nearbyTaxis == 0) {
        ++m_handoffNoTargets;
      } else {
        m_handoffPending = true;
        m_handoffAttribute = before.attribute;
        m_handoffTaxiCount = proximity.availableTaxis;
      }
    }
    const int heldIndex = HeldActionIndex(action);
    if (heldIndex >= 0) m_heldActions[heldIndex] = down;
    return 1;
  }

  bool shouldDump() override { return false; }

  bool Subscribe() { return SetSubscribed(true); }
  bool Unsubscribe() { return SetSubscribed(false); }
  bool IsSubscribed() const { return m_subscribed; }
  bool NeutralizeForOverlay(double eventTime) {
    if (getContext() == nullptr || m_vehicle.isNUL() ||
        !getContext()->isExist(m_vehicle) || !std::isfinite(eventTime))
      return false;
    bool succeeded = true;
    for (int index = 0; index < kHeldActionCount; ++index) {
      if (m_heldActions[index] == 0.0) continue;
      const int action = HeldAction(index);
      if (!VehicleRuntimeState_ApplyLiveControlAt(
              getContext(), action, 0.0, eventTime)) {
        m_forwardingFailed = true;
        m_lastInputFailure = VehicleRuntimeState_LastControlFailure();
        succeeded = false;
      } else if (m_controlJournalRecording &&
                 !VehicleControlJournal_AppendAction(
                     &m_controlJournal, Session::m_simulationTick,
                     VehicleRuntimeState_LastAppliedControlTime(),
                     action, 0.0)) {
        ++m_controlJournalAppendFailures;
        m_controlJournalRecording = false;
      }
      m_heldActions[index] = 0.0;
    }
    m_directionalAxes = {};
    return succeeded;
  }
  bool ReconcilePhysicalDirectionalAxes(double eventTime) {
    if (!m_applicationActive || _gr_hWnd == nullptr ||
        GetForegroundWindow() != _gr_hWnd) {
      return true;
    }
    if (getContext() == nullptr || m_vehicle.isNUL() ||
        !getContext()->isExist(m_vehicle) || !std::isfinite(eventTime)) {
      m_forwardingFailed = true;
      m_lastInputFailure = 9;
      return false;
    }

    const auto keyDown = [](int key) {
      return (GetAsyncKeyState(key) & 0x8000) != 0 ? 1.0 : 0.0;
    };
    const double keySensitivity = (std::max)(
        0.01, (std::min)(1.0, g_levelAttr.get_double("keySens")));
    SRecoveredObserverAxes physical = {};
    physical.forward =
        (keyDown('W') - keyDown('S')) * keySensitivity;
    physical.strafe =
        (keyDown('D') - keyDown('A')) * keySensitivity;
    physical.vertical =
        (keyDown(VK_SPACE) - keyDown(VK_LCONTROL)) * keySensitivity;
    physical.turn =
        (keyDown(VK_RIGHT) - keyDown(VK_LEFT)) * keySensitivity;
    physical.look =
        (keyDown(VK_UP) - keyDown(VK_DOWN)) * keySensitivity;

    const auto reconcileAxis = [this, eventTime](
        int action, double desired, double* current) {
      if (*current == desired) return true;
      if (!VehicleRuntimeState_ApplyLiveControlAt(
              getContext(), action, desired, eventTime)) {
        m_forwardingFailed = true;
        m_lastInputFailure = VehicleRuntimeState_LastControlFailure();
        return false;
      }
      *current = desired;
      const int heldIndex = HeldActionIndex(action);
      if (heldIndex >= 0) m_heldActions[heldIndex] = desired;
      ++m_physicalReconciliations;
      if (m_controlJournalRecording &&
          !VehicleControlJournal_AppendAction(
              &m_controlJournal, Session::m_simulationTick,
              VehicleRuntimeState_LastAppliedControlTime(), action,
              desired)) {
        ++m_controlJournalAppendFailures;
        m_controlJournalRecording = false;
      }
      return true;
    };

    return reconcileAxis(
               MOVE_FORWARD, physical.forward,
               &m_directionalAxes.forward) &&
           reconcileAxis(
               STRAFE_RIGHT, physical.strafe,
               &m_directionalAxes.strafe) &&
           reconcileAxis(
               STRAFE_UP, physical.vertical,
               &m_directionalAxes.vertical) &&
           reconcileAxis(
               TURN_RIGHT, physical.turn,
               &m_directionalAxes.turn) &&
           reconcileAxis(
               LOOK_UP, physical.look,
               &m_directionalAxes.look);
  }
  bool BeginControlJournal() {
    if (getContext() == nullptr || m_vehicle.isNUL() ||
        !getContext()->isExist(m_vehicle) || m_controlJournalRecording)
      return false;
    m_controlJournal = {};
    m_controlJournalAppendFailures = 0;
    m_controlJournalRecording = VehicleControlJournal_Begin(
        "Vehicle.Default", m_applicationActive, m_heldActions,
        &m_controlJournal);
    return m_controlJournalRecording;
  }
  bool ControlJournalTelemetry(
      SRecoveredVehicleControlJournalTelemetry* telemetry) const {
    if (telemetry == nullptr || m_controlJournal.target.empty()) return false;
    SVehicleControlJournalStatistics statistics = {};
    std::vector<std::uint8_t> encoded;
    if (!VehicleControlJournal_Statistics(
            m_controlJournal, &statistics) ||
        !VehicleControlJournal_Encode(m_controlJournal, &encoded))
      return false;
    *telemetry = {};
    telemetry->checkpointTick = m_controlJournal.checkpointTick;
    telemetry->lastRecordTick = statistics.lastTick;
    telemetry->journalFingerprint =
        VehicleControlJournal_Fingerprint(m_controlJournal);
    telemetry->recordCount = static_cast<unsigned int>(
        m_controlJournal.records.size());
    telemetry->actionRecords = statistics.actionRecords;
    telemetry->focusRecords = statistics.focusRecords;
    telemetry->encodedBytes = static_cast<unsigned int>(encoded.size());
    telemetry->appendFailures = m_controlJournalAppendFailures;
    telemetry->recording = m_controlJournalRecording ? 1 : 0;
    telemetry->applicationActive = m_applicationActive ? 1 : 0;
    return telemetry->journalFingerprint != 0;
  }
  bool CopyControlJournal(SVehicleControlJournal* journal) const {
    if (journal == nullptr || m_controlJournal.target.empty() ||
        !VehicleControlJournal_Validate(m_controlJournal))
      return false;
    *journal = m_controlJournal;
    return true;
  }
  bool CanAdoptControlJournal(
      const SVehicleControlJournal& journal) const {
    KR_ObjectID vehicle = m_vehicle;
    if (getContext() == nullptr || vehicle.isNUL() ||
        !getContext()->isExist(vehicle) || !journal.sealed ||
        !VehicleControlJournal_Validate(journal))
      return false;
    const KR_ObjectID target =
        getContext()->searchObject(journal.target.c_str());
    bool active = false;
    double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
    return target == vehicle &&
           VehicleControlJournal_DeriveLifecycle(
               journal, &active, held);
  }
  bool AdoptControlJournal(const SVehicleControlJournal& journal) {
    m_controlJournalAdoptionFailure.clear();
    if (!CanAdoptControlJournal(journal)) {
      m_controlJournalAdoptionFailure =
          "journal no longer binds to the live Vehicle controller";
      return false;
    }
    SSimulationClockState clock = {};
    bool active = false;
    double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
    SVehicleControlJournal resumed = journal;
    if (!SUA_CaptureSimulationClock(&clock)) {
      m_controlJournalAdoptionFailure =
          "restored simulation clock is unavailable";
      return false;
    }
    if (clock.tick != journal.finalTick ||
        (std::max)(clock.eventMoment, clock.viewTime) != journal.finalTime) {
      std::ostringstream detail;
      detail << "restored simulation clock does not match CTJ1 (tick="
             << clock.tick << "/" << journal.finalTick << ", time="
             << (std::max)(clock.eventMoment, clock.viewTime) << "/"
             << journal.finalTime << ")";
      m_controlJournalAdoptionFailure = detail.str();
      return false;
    }
    if (!VehicleControlJournal_DeriveLifecycle(journal, &active, held)) {
      m_controlJournalAdoptionFailure =
          "restored CTJ1 lifecycle derivation failed";
      return false;
    }
    if (!VehicleRuntimeState_RebaseRestoredOwner(getContext())) {
      m_controlJournalAdoptionFailure =
          "restored Vehicle owner rebase failed (reason=" +
          std::to_string(VehicleRuntimeState_LastControlFailure()) + ")";
      return false;
    }
    if (!VehicleControlJournal_Resume(&resumed)) {
      m_controlJournalAdoptionFailure = "restored CTJ1 resume failed";
      return false;
    }
    m_controlJournal = resumed;
    m_controlJournalRecording = true;
    m_controlJournalAppendFailures = 0;
    m_applicationActive = active;
    for (std::size_t index = 0;
         index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index) {
      m_heldActions[index] = held[index];
    }
    m_directionalAxes = {};
    if (journal.initialApplicationActive) {
      for (std::size_t index = 0;
           index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index) {
        if (journal.initialHeldActions[index] == 0.0) continue;
        int canonicalAction = VehicleControlJournal_HeldAction(index);
        double canonicalValue = journal.initialHeldActions[index];
        CanonicalizeDirectionalAction(
            &m_directionalAxes, canonicalAction, canonicalValue,
            &canonicalAction, &canonicalValue);
      }
    }
    bool journalApplicationActive = journal.initialApplicationActive;
    for (const SVehicleControlJournalRecord& record : journal.records) {
      if (record.kind == VEHICLE_CONTROL_JOURNAL_FOCUS) {
        journalApplicationActive = record.value != 0.0;
        if (!journalApplicationActive) m_directionalAxes = {};
        continue;
      }
      if (record.kind != VEHICLE_CONTROL_JOURNAL_ACTION ||
          !journalApplicationActive) {
        continue;
      }
      int canonicalAction = record.action;
      double canonicalValue = record.value;
      CanonicalizeDirectionalAction(
          &m_directionalAxes, record.action, record.value,
          &canonicalAction, &canonicalValue);
    }
    for (int index = 0; index < 10; ++index) m_heldActions[index] = 0.0;
    m_heldActions[HeldActionIndex(MOVE_FORWARD)] =
        m_directionalAxes.forward;
    m_heldActions[HeldActionIndex(STRAFE_RIGHT)] =
        m_directionalAxes.strafe;
    m_heldActions[HeldActionIndex(STRAFE_UP)] =
        m_directionalAxes.vertical;
    m_heldActions[HeldActionIndex(TURN_RIGHT)] =
        m_directionalAxes.turn;
    m_heldActions[HeldActionIndex(LOOK_UP)] =
        m_directionalAxes.look;
    return true;
  }
  const std::string& ControlJournalAdoptionFailure() const {
    return m_controlJournalAdoptionFailure;
  }
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
  unsigned int PhysicalReconciliationCount() const {
    return m_physicalReconciliations;
  }
  unsigned int ActiveActionCount() const {
    unsigned int count = 0;
    for (double value : m_heldActions) {
      if (value != 0.0) ++count;
    }
    return count;
  }
  const SRecoveredObserverAxes& DirectionalAxes() const {
    return m_directionalAxes;
  }
  int LastInputFailure() const { return m_lastInputFailure; }
  unsigned int PrimaryFirePresses() const { return m_primaryFirePresses; }
  unsigned int SecondaryFirePresses() const {
    return m_secondaryFirePresses;
  }
  unsigned int SecondaryFireAcceptedShots() const {
    return m_secondaryFireAcceptedShots;
  }
  unsigned int JumpPresses() const { return m_jumpPresses; }

  void ObserveVehicleHandoff() {
    if (getContext() == nullptr || m_vehicle.isNUL()) return;
    SRecoveredVehicleRuntimeState state = {};
    if (!VehicleRuntimeState_Inspect(getContext(), m_vehicle, &state)) return;
    Vehicle* vehicle = static_cast<Vehicle*>(
        getContext()->queryInterface(m_vehicle, IVehicleIID));
    if (vehicle == nullptr) {
      m_secondaryAmmoTracking = false;
    } else if (!m_secondaryAmmoTracking ||
               state.attribute != m_secondaryTrackedAttribute ||
               vehicle->m_secBulletCnt > m_secondaryLastAmmo) {
      m_secondaryAmmoTracking = true;
      m_secondaryTrackedAttribute = state.attribute;
      m_secondaryLastAmmo = vehicle->m_secBulletCnt;
    } else if (vehicle->m_secBulletCnt < m_secondaryLastAmmo) {
      m_secondaryFireAcceptedShots += static_cast<unsigned int>(
          m_secondaryLastAmmo - vehicle->m_secBulletCnt);
      m_secondaryLastAmmo = vehicle->m_secBulletCnt;
    }
    const int taxiCount = TaxiSubjectState_LiveCount();
    const int orphanCount = OrphanSubjectState_LiveCount();
    if (m_exitPending && state.attribute != m_exitAttribute) {
      if (taxiCount == m_exitTaxiCount + 1) {
        ++m_exitSafeCompletions;
        ++m_exitDroppedTaxis;
        m_exitPending = false;
      } else if (orphanCount == m_exitOrphanCount + 1) {
        ++m_exitUnsafeCompletions;
        ++m_exitDroppedOrphans;
        m_exitPending = false;
      }
      if (!m_exitPending && m_exitPanelWasOpen &&
          vehicle != nullptr && !vehicle->panelOpen())
        ++m_exitPanelCloses;
    }
    if (m_handoffPending &&
        (state.attribute != m_handoffAttribute ||
         taxiCount + 1 == m_handoffTaxiCount)) {
      ++m_handoffSuccesses;
      if (taxiCount + 1 == m_handoffTaxiCount) ++m_handoffRemovedTaxis;
      if (vehicle != nullptr && vehicle->panelOpen()) ++m_handoffPanelOpens;
      ++m_reentryCompletions;
      if (vehicle != nullptr && vehicle->panelOpen()) ++m_reentryPanelOpens;
      m_handoffPanelDrawBaseline =
          vehicle == nullptr ? 0 : vehicle->panelDrawCount();
      m_handoffPosition = state.position;
      m_handoffPostFrames = 0;
      m_handoffPostDistance = 0.0;
      m_handoffPending = false;
    } else if (m_handoffSuccesses != 0) {
      const double dx = state.position.x - m_handoffPosition.x;
      const double dz = state.position.z - m_handoffPosition.z;
      m_handoffPostDistance = (std::max)(
          m_handoffPostDistance, std::sqrt(dx * dx + dz * dz));
      ++m_handoffPostFrames;
    }
  }

  bool HandoffTelemetry(SRecoveredTaxiVehicleHandoffTelemetry* telemetry) {
    if (telemetry == nullptr || getContext() == nullptr ||
        m_vehicle.isNUL()) return false;
    STaxiVehicleProximityState proximity = {};
    if (!TaxiSubjectState_InspectVehicleProximity(
            getContext(), m_vehicle, &proximity)) return false;
    Vehicle* vehicle = static_cast<Vehicle*>(
        getContext()->queryInterface(m_vehicle, IVehicleIID));
    *telemetry = {};
    telemetry->nearestTaxiDistance = proximity.nearestDistance;
    telemetry->activationDistance = proximity.activationDistance;
    telemetry->postTransitionDistance = m_handoffPostDistance;
    telemetry->availableTaxis = static_cast<unsigned int>(proximity.availableTaxis);
    telemetry->nearbyTaxis = static_cast<unsigned int>(proximity.nearbyTaxis);
    telemetry->attempts = m_handoffAttempts;
    telemetry->pendingTransitions = m_handoffPending ? 1u : 0u;
    telemetry->successfulTransitions = m_handoffSuccesses;
    telemetry->noTargetAttempts = m_handoffNoTargets;
    telemetry->removedTaxis = m_handoffRemovedTaxis;
    telemetry->panelOpenTransitions = m_handoffPanelOpens;
    const unsigned int panelDrawCount =
        vehicle == nullptr ? 0u : vehicle->panelDrawCount();
    telemetry->panelDraws = panelDrawCount < m_handoffPanelDrawBaseline ?
        0u : panelDrawCount - m_handoffPanelDrawBaseline;
    telemetry->postTransitionFrames = m_handoffPostFrames;
    telemetry->panelReady = vehicle != nullptr && vehicle->panelReady();
    telemetry->panelOpen = vehicle != nullptr && vehicle->panelOpen();
    telemetry->hardwareSubscriptionPreserved = m_subscribed ? 1 : 0;
    return true;
  }

  bool EmbodimentTelemetry(SRecoveredVehicleEmbodimentTelemetry* telemetry) {
    if (telemetry == nullptr || getContext() == nullptr ||
        m_vehicle.isNUL()) return false;
    SOrphanSubjectRuntimeTelemetry orphan = {};
    if (!OrphanSubjectState_RuntimeTelemetry(getContext(), &orphan))
      return false;
    *telemetry = {};
    telemetry->exitAttempts = m_exitAttempts;
    telemetry->safeExitCompletions = m_exitSafeCompletions;
    telemetry->unsafeExitCompletions = m_exitUnsafeCompletions;
    telemetry->droppedTaxis = m_exitDroppedTaxis;
    telemetry->droppedOrphans = m_exitDroppedOrphans;
    telemetry->reentryAttempts = m_reentryAttempts;
    telemetry->reentryCompletions = m_reentryCompletions;
    telemetry->panelCloseTransitions = m_exitPanelCloses;
    telemetry->panelReopenTransitions = m_reentryPanelOpens;
    telemetry->orphanMoveEvents =
        static_cast<unsigned int>(orphan.moveEvents);
    telemetry->orphanImpacts = static_cast<unsigned int>(orphan.impacts);
    telemetry->orphanExplosions =
        static_cast<unsigned int>(orphan.explosionStarts);
    telemetry->orphanSmokeStarts =
        static_cast<unsigned int>(orphan.smokeStarts);
    telemetry->orphanRenderFrames =
        static_cast<unsigned int>(orphan.renderFrames);
    telemetry->liveOrphans =
        static_cast<unsigned int>(OrphanSubjectState_LiveCount());
    telemetry->exitPending = m_exitPending ? 1 : 0;
    telemetry->hardwareSubscriptionPreserved = m_subscribed ? 1 : 0;
    return true;
  }

  bool SetApplicationActive(bool active, double eventTime) {
    if (m_applicationActive == active) return true;
    if (active) {
      m_applicationActive = true;
      ++m_focusGains;
      if (m_controlJournalRecording &&
          !VehicleControlJournal_AppendFocus(
              &m_controlJournal, Session::m_simulationTick,
              JournalEventTime(eventTime), true)) {
        ++m_controlJournalAppendFailures;
        m_controlJournalRecording = false;
      }
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
    m_directionalAxes = {};
    if (succeeded && m_controlJournalRecording &&
        !VehicleControlJournal_AppendFocus(
            &m_controlJournal, Session::m_simulationTick,
            JournalEventTime(eventTime), false)) {
      ++m_controlJournalAppendFailures;
      m_controlJournalRecording = false;
    }
    return succeeded;
  }

 private:
  static constexpr int kHeldActionCount =
      static_cast<int>(VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT);

  double JournalEventTime(double requested) const {
    SRecoveredVehicleRuntimeState state = {};
    KR_ObjectID vehicle = m_vehicle;
    if (getContext() == nullptr || vehicle.isNUL() ||
        !VehicleRuntimeState_Inspect(getContext(), m_vehicle, &state) ||
        !std::isfinite(requested))
      return -1.0;
    return (std::max)(state.lastTime,
                      (std::min)(requested, state.lastTime + 0.05));
  }

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
      case FIRE_PRIMARY: return 10;
      case FIRE_SECONDARY: return 11;
      default: return -1;
    }
  }

  static int HeldAction(int index) {
    static const int actions[kHeldActionCount] = {
        MOVE_FORWARD, MOVE_BACKWARD, STRAFE_LEFT, STRAFE_RIGHT,
        STRAFE_UP, STRAFE_DOWN, TURN_LEFT, TURN_RIGHT,
        LOOK_UP, LOOK_DOWN, FIRE_PRIMARY, FIRE_SECONDARY};
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
  unsigned int m_physicalReconciliations = 0;
  double m_heldActions[kHeldActionCount] = {};
  SRecoveredObserverAxes m_directionalAxes = {};
  bool m_applicationActive = true;
  bool m_quitRequested = false;
  bool m_forwardingFailed = false;
  int m_lastInputFailure = 0;
  std::string m_controlJournalAdoptionFailure;
  bool m_subscribed = false;
  unsigned int m_handoffAttempts = 0;
  bool m_handoffPending = false;
  unsigned int m_handoffSuccesses = 0;
  unsigned int m_handoffNoTargets = 0;
  unsigned int m_handoffRemovedTaxis = 0;
  unsigned int m_handoffPanelOpens = 0;
  unsigned int m_handoffPanelDrawBaseline = 0;
  unsigned int m_handoffPostFrames = 0;
  double m_handoffPostDistance = 0.0;
  KR_ObjectID m_handoffAttribute = KR_ObjectID::NUL();
  int m_handoffTaxiCount = 0;
  CFVector3 m_handoffPosition = CFVector3(0.0, 0.0, 0.0);
  unsigned int m_primaryFirePresses = 0;
  unsigned int m_secondaryFirePresses = 0;
  unsigned int m_secondaryFireAcceptedShots = 0;
  bool m_secondaryAmmoTracking = false;
  KR_ObjectID m_secondaryTrackedAttribute = KR_ObjectID::NUL();
  int m_secondaryLastAmmo = 0;
  unsigned int m_jumpPresses = 0;
  unsigned int m_exitAttempts = 0;
  bool m_exitPending = false;
  unsigned int m_exitSafeCompletions = 0;
  unsigned int m_exitUnsafeCompletions = 0;
  unsigned int m_exitDroppedTaxis = 0;
  unsigned int m_exitDroppedOrphans = 0;
  int m_exitTaxiCount = 0;
  int m_exitOrphanCount = 0;
  KR_ObjectID m_exitAttribute = KR_ObjectID::NUL();
  bool m_exitPanelWasOpen = false;
  unsigned int m_exitPanelCloses = 0;
  unsigned int m_reentryAttempts = 0;
  unsigned int m_reentryCompletions = 0;
  unsigned int m_reentryPanelOpens = 0;
  SVehicleControlJournal m_controlJournal;
  bool m_controlJournalRecording = false;
  unsigned int m_controlJournalAppendFailures = 0;
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
bool g_vehicleDeathCameraReady = false;
bool g_taxiVehicleTransitionReady = false;
bool g_vehicleControlReplayReady = false;
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
SRecoveredVehicleDeathCameraProbeSummary g_vehicleDeathCameraProbe = {};
STaxiVehicleTransitionProbeSummary g_taxiVehicleTransitionProbe = {};
SRecoveredVehicleControlReplayProbeSummary g_vehicleControlReplayProbe = {};
SRecoveredVehicleDriveTelemetry g_vehicleDriveTelemetry = {};
SRecoveredFrameTimingTelemetry g_frameTimingTelemetry = {};
CFVector3 g_vehicleTelemetryStartPosition(0.0, 0.0, 0.0);
CFVector3 g_vehicleTelemetryStartForward(0.0, 0.0, 1.0);
bool g_vehicleDriveTelemetryReady = false;
bool g_primaryFireTelemetryReady = false;
BulletRuntimeTelemetry g_primaryFireOwnerBaseline = {};
SRecoveredVehiclePrimaryFireTelemetry g_primaryFireTelemetry = {};
unsigned int g_primaryFireTriggerBaseline = 0;
int g_primaryFireExplosionBaseline = 0;
int g_primaryFireParticleBaseline = 0;
int g_primaryFireSmokeBaseline = 0;
int g_primaryFireSparkBaseline = 0;
int g_primaryFireSoundBaseline = 0;
bool g_primaryFireEffectPresent = false;
std::string g_levelContinuationFailure;
bool g_failNextRestoredGameplayAuthorityForTesting = false;
std::string g_levelSaveSlotFailure;
SRecoveredSaveMenuState g_saveMenuState;
SRecoveredCrossLevelLoadRequest g_crossLevelLoadRequest;
SRecoveredCampaignRestartState g_campaignRestartState;
SRecoveredCampaignRestartRequest g_campaignRestartRequest;
SRecoveredScriptedLevelTransitionState g_scriptedLevelTransitionState;
SRecoveredScriptedLevelTransitionRequest g_scriptedLevelTransitionRequest;
bool g_saveMenuAllowOverwrite = false;
SRecoveredDebugMenuState g_debugMenuState;
SRecoveredDebugLevelSwitchRequest g_debugLevelSwitchRequest;
std::vector<std::uint8_t> g_debugPreDeathCheckpoint;
SLevelContinuationSummary g_debugPreDeathCheckpointSummary;
int g_debugPreDeathCorpseCount = -1;
std::vector<std::uint8_t> g_debugPreVehicleDestructionCheckpoint;
SLevelContinuationSummary g_debugPreVehicleDestructionCheckpointSummary;
int g_debugPreVehicleDestructionOrphanCount = -1;
bool g_debugPreVehicleDestructionPanelReady = false;
bool g_debugPreVehicleDestructionPanelOpen = false;
std::vector<std::string> g_debugLevelCatalog;
std::vector<SRecoveredDebugVehicleType> g_debugVehicleCatalog;
SRecoveredDeveloperCatalogSnapshot g_inGameShellDeveloperCatalog;
std::uint64_t g_inGameShellDeveloperCatalogNextGeneration = 1u;
struct SPendingDebugTaxiSettlement {
  std::string objectName;
  CFVector3 expectedPosition;
  unsigned int remainingFrames = 0;
  unsigned int observedFrames = 0;
  double maxDrift = 0.0;
};
constexpr unsigned int kDebugTaxiSettlementFrames = 3u;
constexpr double kDebugTaxiSettlementTolerance = 1.0e-6;
std::vector<SPendingDebugTaxiSettlement> g_debugTaxiSettlements;
HMENU g_nativeMenuBar = nullptr;
HMENU g_nativeGameMenu = nullptr;
HMENU g_nativeSaveMenu = nullptr;
HMENU g_nativeLoadMenu = nullptr;
HMENU g_nativeDebugMenu = nullptr;
HMENU g_nativeDebugSpawnMenu = nullptr;
HMENU g_nativeDebugSpawnEnterMenu = nullptr;
HMENU g_nativeDebugLevelMenu = nullptr;
bool g_nativeDiagnosticMenuEnabled = false;
RecoveredObserverInput g_observerInput;
RecoveredVehicleControlInput g_vehicleControlInput;
RecoveredWindowsInputAdapter g_windowsInputAdapter;
SRecoveredInGameShellState g_inGameShellState;
SRecoveredSaveSlotCatalogSnapshot g_inGameShellSaveCatalog;
SRecoveredSaveSlotCatalogSnapshot g_pendingInGameShellSaveCatalog;
std::thread g_inGameShellSaveCatalogWorker;
std::mutex g_inGameShellSaveCatalogMutex;
bool g_inGameShellSaveCatalogRunning = false;
bool g_inGameShellSaveCatalogCompleted = false;
bool g_inGameShellSaveCatalogRefreshRequested = false;
bool g_inGameShellSaveCatalogBuildSucceeded = false;
std::uint64_t g_inGameShellSaveCatalogNextGeneration = 1u;
std::string g_inGameShellSaveCatalogFailure;
SRecoveredInputBindings g_inGameShellBindings =
    RecoveredWindowsInput_DefaultBindings();
SRecoveredWindowPresentation g_inGameShellAppliedPresentation = {};
SRecoveredWindowPresentation g_inGameShellRollbackPresentation = {};
ULONGLONG g_inGameShellVideoDeadline = 0;
int g_inGameShellPersistedWindowMode = 0;
int g_inGameShellPersistedWindowScale = 1;
std::size_t g_inGameShellPersistedExclusiveModeIndex = 0;
std::size_t g_inGameShellRollbackExclusiveModeIndex = 0;
int g_inGameShellRequestedExclusiveWidth = 640;
int g_inGameShellRequestedExclusiveHeight = 480;
int g_inGameShellRequestedExclusiveBits = 32;
int g_inGameShellRequestedExclusiveFrequency = 60;
unsigned int g_mapTogglePresses = 0;
FixedFontOBJ* g_debugMapMissionFont = nullptr;
FixedFontOBJ* g_gameConsoleFont = nullptr;
SRecoveredMissionMapProbeTelemetry g_missionMapProbe = {};
SRecoveredDebugMapControlProbeTelemetry g_debugMapControlProbe = {};
int g_missionMapBaselineMissions = 0;
int g_missionMapBaselineTexts = 0;
int g_missionMapBaselineRoutes = 0;
unsigned int g_missionMapBaselineDrawFrames = 0;
bool g_missionMapProbeLive = false;
struct SRecoveredPendingWindowsInput {
  bool focus = false;
  bool applicationActive = true;
  SRecoveredWindowsInputAction action = {};
};
constexpr std::size_t kMaximumPendingWindowsInput = 4096u;
std::vector<SRecoveredPendingWindowsInput> g_pendingWindowsInput;

void Report(unsigned int issue) { g_issues |= issue; }

std::uint64_t ContinuationContentFingerprint() {
  const SRecoveredRetailScriptManifestSummary* manifest =
      RecoveredRetailScriptManifest_IsReady()
          ? RecoveredRetailScriptManifest_Summary()
          : nullptr;
  const std::uint64_t baseFingerprint =
      manifest != nullptr && manifest->contentFingerprint != 0
          ? manifest->contentFingerprint
          : RecoveredArenaSeance_ContentFingerprint();
  return RecoveredModRuntime_CombineContentFingerprint(baseFingerprint);
}

std::string ContinuationLevelIdentity() {
  const char* catalogIdentity =
      RecoveredModRuntime_ActiveLevelIdentity();
  if (catalogIdentity != nullptr && catalogIdentity[0] != '\0')
    return catalogIdentity;
  const char* directory = RecoveredLevelRuntime_Directory();
  if (directory == nullptr || directory[0] == '\0') return "direct-context";
  std::string path(directory);
  while (!path.empty() && (path.back() == '\\' || path.back() == '/'))
    path.pop_back();
  const std::size_t separator = path.find_last_of("\\/");
  const std::string name = separator == std::string::npos
                               ? path
                               : path.substr(separator + 1);
  return name.empty() ? "direct-context" : name;
}

constexpr UINT kNativeSaveSlotBase = 0x7200u;
constexpr UINT kNativeLoadSlotBase = 0x7210u;
constexpr UINT kNativeOpenSaveDirectory = 0x7220u;
constexpr UINT kNativeExitGame = 0x7221u;
constexpr UINT kNativeRestartCurrentLevel = 0x7222u;
constexpr UINT kNativeDebugSpawnBase = 0x7300u;
constexpr UINT kNativeDebugSpawnEnterBase = 0x7340u;
constexpr UINT kNativeDebugShowState = 0x7380u;
constexpr UINT kNativeDebugStabilize = 0x7381u;
constexpr UINT kNativeDebugKillPlayer = 0x7382u;
constexpr UINT kNativeDebugRestorePreDeath = 0x7383u;
constexpr UINT kNativeDebugDestroyOccupiedVehicle = 0x7384u;
constexpr UINT kNativeDebugRestorePreVehicleDestruction = 0x7385u;
constexpr UINT kNativeDebugDamageOccupiedVehicle = 0x7386u;
constexpr UINT kNativeDebugLevelBase = 0x7400u;
constexpr std::size_t kMaximumNativeDebugVehicleTypes = 64u;
constexpr std::size_t kMaximumNativeDebugLevels = 256u;
constexpr unsigned int kMaximumStableBoundaryAttempts = 120u;
constexpr unsigned int kMaximumDebugStableBoundaryAttempts = 120u;

bool IsRetryableSaveBoundaryFailure(const std::string& detail) {
  return detail.find("frame boundary") != std::string::npos ||
         detail.find("published in a frame") != std::string::npos ||
         detail.find("stable capture failed") != std::string::npos;
}

bool IsRetryableDebugBoundaryFailure(const std::string& detail) {
  return IsRetryableSaveBoundaryFailure(detail) ||
         detail.find("stable capture failed") != std::string::npos ||
         detail.find("neutral Vehicle controls") != std::string::npos;
}

std::wstring Utf8ToWide(const std::string& text) {
  if (text.empty()) return std::wstring();
  const int count = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
      static_cast<int>(text.size()), nullptr, 0);
  if (count <= 0) return std::wstring();
  std::wstring wide(static_cast<std::size_t>(count), L'\0');
  if (MultiByteToWideChar(
          CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
          static_cast<int>(text.size()), &wide[0], count) != count)
    return std::wstring();
  return wide;
}

constexpr unsigned int kInGameSettingsVersion = 6u;
constexpr ULONGLONG kVideoConfirmationMilliseconds = 15000u;
constexpr double kDefaultMouseSensitivity = 0.5;
constexpr double kMinimumMouseSensitivity = 0.01;
constexpr double kMaximumMouseSensitivity = 1.01;
constexpr double kMouseSensitivityStep = 0.1;
constexpr double kDefaultEffectsVolume = 1.0;
constexpr double kEffectsVolumeStep = 0.1;
constexpr double kDefaultVehicleVolume = 1.0;
constexpr double kDefaultCinematicVolume = 1.0;

SRecoveredWindowPresentation ShellPresentation(
    int mode, int scale, std::size_t exclusiveModeIndex) {
  SRecoveredWindowPresentation presentation;
  presentation.mode = mode == 2
      ? RECOVERED_WINDOW_MODE_EXCLUSIVE
      : mode == 1 ? RECOVERED_WINDOW_MODE_BORDERLESS
                  : RECOVERED_WINDOW_MODE_WINDOWED;
  scale = (std::max)(1, (std::min)(3, scale));
  presentation.clientWidth = 640 * scale;
  presentation.clientHeight = 480 * scale;
  if (presentation.mode == RECOVERED_WINDOW_MODE_EXCLUSIVE) {
    SRecoveredDisplayMode displayMode;
    if (RecoveredSoftwareGraph_DisplayMode(exclusiveModeIndex, &displayMode)) {
      presentation.clientWidth = displayMode.width;
      presentation.clientHeight = displayMode.height;
      presentation.bitsPerPixel = displayMode.bitsPerPixel;
      presentation.displayFrequency = displayMode.displayFrequency;
      presentation.displayDevice = displayMode.displayDevice;
    }
  }
  return presentation;
}

bool ReadSmallFile(const std::wstring& path, std::string* bytes) {
  if (bytes == nullptr) return false;
  bytes->clear();
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size = {};
  const bool bounded = GetFileSizeEx(file, &size) != FALSE &&
                       size.QuadPart >= 0 && size.QuadPart <= 65536;
  if (!bounded) {
    CloseHandle(file);
    return false;
  }
  bytes->resize(static_cast<std::size_t>(size.QuadPart));
  DWORD read = 0;
  const bool ok = bytes->empty() ||
      (ReadFile(file, &(*bytes)[0], static_cast<DWORD>(bytes->size()),
                &read, nullptr) != FALSE && read == bytes->size());
  CloseHandle(file);
  if (!ok) bytes->clear();
  return ok;
}

bool ParseUnsignedSetting(const std::string& line, const char* name,
                          unsigned int* value) {
  const std::string prefix = std::string(name) + "=";
  if (line.compare(0, prefix.size(), prefix) != 0) return false;
  const char* begin = line.c_str() + prefix.size();
  char* end = nullptr;
  errno = 0;
  const unsigned long parsed = std::strtoul(begin, &end, 10);
  if (errno != 0 || end == begin || *end != '\0' ||
      parsed > (std::numeric_limits<unsigned int>::max)())
    return false;
  *value = static_cast<unsigned int>(parsed);
  return true;
}

bool ParseDoubleSetting(const std::string& line, const char* name,
                        double* value) {
  if (value == nullptr) return false;
  const std::string prefix = std::string(name) + "=";
  if (line.compare(0, prefix.size(), prefix) != 0) return false;
  const char* begin = line.c_str() + prefix.size();
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(begin, &end);
  if (errno != 0 || end == begin || *end != '\0' ||
      !std::isfinite(parsed))
    return false;
  *value = parsed;
  return true;
}

bool ValidMouseSensitivity(double value) {
  return std::isfinite(value) && value >= kMinimumMouseSensitivity &&
         value <= kMaximumMouseSensitivity;
}

bool ValidEffectsVolume(double value) {
  return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool LoadInGameShellSettings(const std::wstring& path,
                             SRecoveredInputBindings* bindings,
                             int* windowMode, int* windowScale,
                             int* exclusiveWidth, int* exclusiveHeight,
                             int* exclusiveBits, int* exclusiveFrequency,
                             double* mouseSensitivityX,
                             double* mouseSensitivityY,
                             bool* mouseInvertY, double* effectsVolume,
                             double* vehicleVolume,
                             double* cinematicVolume,
                             bool* migrated) {
  if (bindings == nullptr || windowMode == nullptr || windowScale == nullptr ||
      exclusiveWidth == nullptr || exclusiveHeight == nullptr ||
      exclusiveBits == nullptr || exclusiveFrequency == nullptr ||
      mouseSensitivityX == nullptr || mouseSensitivityY == nullptr ||
      mouseInvertY == nullptr || effectsVolume == nullptr ||
      vehicleVolume == nullptr || cinematicVolume == nullptr ||
      migrated == nullptr)
    return false;
  std::string bytes;
  if (!ReadSmallFile(path, &bytes)) return false;
  SRecoveredInputBindings parsed = RecoveredWindowsInput_DefaultBindings();
  unsigned int version = 0;
  unsigned int mode = 0;
  unsigned int scale = 0;
  unsigned int exclusiveWidthValue = 640;
  unsigned int exclusiveHeightValue = 480;
  unsigned int exclusiveBitsValue = 32;
  unsigned int exclusiveFrequencyValue = 60;
  double sensitivityX = kDefaultMouseSensitivity;
  double sensitivityY = kDefaultMouseSensitivity;
  unsigned int invertY = 0;
  double effects = kDefaultEffectsVolume;
  double vehicle = kDefaultVehicleVolume;
  double cinematic = kDefaultCinematicVolume;
  bool haveVersion = false;
  bool haveMode = false;
  bool haveScale = false;
  bool haveExclusiveWidth = false;
  bool haveExclusiveHeight = false;
  bool haveExclusiveBits = false;
  bool haveExclusiveFrequency = false;
  bool haveSensitivityX = false;
  bool haveSensitivityY = false;
  bool haveInvertY = false;
  bool haveEffects = false;
  bool haveVehicle = false;
  bool haveCinematic = false;
  bool haveBinding[RECOVERED_BIND_COUNT] = {};
  std::istringstream input(bytes);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    unsigned int value = 0;
    if (ParseUnsignedSetting(line, "version", &value)) {
      version = value;
      haveVersion = true;
      continue;
    }
    if (ParseUnsignedSetting(line, "window_mode", &value)) {
      mode = value;
      haveMode = true;
      continue;
    }
    if (ParseUnsignedSetting(line, "window_scale", &value)) {
      scale = value;
      haveScale = true;
      continue;
    }
    if (ParseUnsignedSetting(line, "exclusive_width", &value)) {
      exclusiveWidthValue = value;
      haveExclusiveWidth = true;
      continue;
    }
    if (ParseUnsignedSetting(line, "exclusive_height", &value)) {
      exclusiveHeightValue = value;
      haveExclusiveHeight = true;
      continue;
    }
    if (ParseUnsignedSetting(line, "exclusive_bits", &value)) {
      exclusiveBitsValue = value;
      haveExclusiveBits = true;
      continue;
    }
    if (ParseUnsignedSetting(line, "exclusive_frequency", &value)) {
      exclusiveFrequencyValue = value;
      haveExclusiveFrequency = true;
      continue;
    }
    double doubleValue = 0.0;
    if (ParseDoubleSetting(line, "mouse_sensitivity_x", &doubleValue)) {
      sensitivityX = doubleValue;
      haveSensitivityX = true;
      continue;
    }
    if (ParseDoubleSetting(line, "mouse_sensitivity_y", &doubleValue)) {
      sensitivityY = doubleValue;
      haveSensitivityY = true;
      continue;
    }
    if (ParseUnsignedSetting(line, "mouse_invert_y", &value)) {
      invertY = value;
      haveInvertY = true;
      continue;
    }
    if (ParseDoubleSetting(line, "effects_volume", &doubleValue)) {
      effects = doubleValue;
      haveEffects = true;
      continue;
    }
    if (ParseDoubleSetting(line, "vehicle_volume", &doubleValue)) {
      vehicle = doubleValue;
      haveVehicle = true;
      continue;
    }
    if (ParseDoubleSetting(line, "cinematic_volume", &doubleValue)) {
      cinematic = doubleValue;
      haveCinematic = true;
      continue;
    }
    for (std::size_t index = 0; index < RECOVERED_BIND_COUNT; ++index) {
      const std::string name = "binding_" + std::to_string(index);
      if (!ParseUnsignedSetting(line, name.c_str(), &value)) continue;
      parsed.key[index] = value;
      haveBinding[index] = true;
      break;
    }
  }
  if (!haveVersion || version < 1u || version > kInGameSettingsVersion ||
      !haveMode ||
       !haveScale || mode > (version >= 3u ? 2u : 1u) ||
       scale < 1u || scale > 3u ||
       (mode < 2u &&
        !RecoveredSoftwareGraph_ValidatePresentation(
            ShellPresentation(static_cast<int>(mode),
                              static_cast<int>(scale), 0u))))
    return false;
  if (version >= 3u &&
      (!haveExclusiveWidth || !haveExclusiveHeight || !haveExclusiveBits ||
       !haveExclusiveFrequency || exclusiveWidthValue < 640u ||
       exclusiveHeightValue < 480u || exclusiveWidthValue > 7680u ||
       exclusiveHeightValue > 4320u ||
       static_cast<unsigned long long>(exclusiveWidthValue) * 3u !=
           static_cast<unsigned long long>(exclusiveHeightValue) * 4u ||
       exclusiveBitsValue < 16u || exclusiveBitsValue > 64u ||
       exclusiveFrequencyValue < 1u || exclusiveFrequencyValue > 1000u))
    return false;
  if (version >= 2u &&
      (!haveSensitivityX || !haveSensitivityY || !haveInvertY))
    return false;
  if (version >= 4u && !haveEffects) return false;
  if (version >= 5u && !haveVehicle) return false;
  if (version >= 6u && !haveCinematic) return false;
  if (!ValidMouseSensitivity(sensitivityX) ||
      !ValidMouseSensitivity(sensitivityY) || invertY > 1u ||
      !ValidEffectsVolume(effects) || !ValidEffectsVolume(vehicle) ||
      !ValidEffectsVolume(cinematic))
    return false;
  const std::size_t requiredBindings =
      version >= 2u ? RECOVERED_BIND_COUNT : RECOVERED_BIND_MAP + 1u;
  for (std::size_t index = 0; index < requiredBindings; ++index)
    if (!haveBinding[index]) return false;
  if (!RecoveredWindowsInput_ValidateBindings(parsed, nullptr, nullptr))
    return false;
  *bindings = parsed;
  *windowMode = static_cast<int>(mode);
  *windowScale = static_cast<int>(scale);
  *exclusiveWidth = static_cast<int>(exclusiveWidthValue);
  *exclusiveHeight = static_cast<int>(exclusiveHeightValue);
  *exclusiveBits = static_cast<int>(exclusiveBitsValue);
  *exclusiveFrequency = static_cast<int>(exclusiveFrequencyValue);
  *mouseSensitivityX = sensitivityX;
  *mouseSensitivityY = sensitivityY;
  *mouseInvertY = invertY != 0u;
  *effectsVolume = effects;
  *vehicleVolume = vehicle;
  *cinematicVolume = cinematic;
  *migrated = version < kInGameSettingsVersion;
  return true;
}

bool WriteInGameShellSettings() {
  if (g_inGameShellState.settingsPath.empty()) return false;
  std::ostringstream output;
  SRecoveredDisplayMode exclusiveMode;
  if (!RecoveredSoftwareGraph_DisplayMode(
          g_inGameShellPersistedExclusiveModeIndex, &exclusiveMode)) {
    exclusiveMode.width = 640;
    exclusiveMode.height = 480;
    exclusiveMode.bitsPerPixel = 32;
    exclusiveMode.displayFrequency = 60;
  }
  output << "version=" << kInGameSettingsVersion << "\r\n"
         << "window_mode=" << g_inGameShellPersistedWindowMode << "\r\n"
         << "window_scale=" << g_inGameShellPersistedWindowScale << "\r\n"
         << "exclusive_width=" << exclusiveMode.width << "\r\n"
         << "exclusive_height=" << exclusiveMode.height << "\r\n"
         << "exclusive_bits=" << exclusiveMode.bitsPerPixel << "\r\n"
         << "exclusive_frequency=" << exclusiveMode.displayFrequency
         << "\r\n"
         << std::fixed << std::setprecision(3)
         << "mouse_sensitivity_x="
         << g_inGameShellState.mouseSensitivityX << "\r\n"
         << "mouse_sensitivity_y="
         << g_inGameShellState.mouseSensitivityY << "\r\n"
         << "mouse_invert_y="
         << (g_inGameShellState.mouseInvertY ? 1 : 0) << "\r\n"
         << "effects_volume=" << g_inGameShellState.effectsVolume
         << "\r\n"
         << "vehicle_volume=" << g_inGameShellState.vehicleVolume
         << "\r\n"
         << "cinematic_volume=" << g_inGameShellState.cinematicVolume
         << "\r\n";
  for (std::size_t index = 0; index < RECOVERED_BIND_COUNT; ++index)
    output << "binding_" << index << "="
           << g_inGameShellBindings.key[index] << "\r\n";
  const std::string bytes = output.str();
  const std::wstring temporary =
      g_inGameShellState.settingsPath + L".tmp";
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool writeOk = WriteFile(file, bytes.data(),
                                 static_cast<DWORD>(bytes.size()),
                                 &written, nullptr) != FALSE &&
                       written == bytes.size() &&
                       FlushFileBuffers(file) != FALSE;
  CloseHandle(file);
  if (!writeOk || MoveFileExW(
          temporary.c_str(), g_inGameShellState.settingsPath.c_str(),
          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
    DeleteFileW(temporary.c_str());
    return false;
  }
  ++g_inGameShellState.settingsWrites;
  return true;
}

void ApplyShellMouseSettingsToRuntime() {
  if (!g_inGameShellState.configured ||
      !ValidMouseSensitivity(g_inGameShellState.mouseSensitivityX) ||
      !ValidMouseSensitivity(g_inGameShellState.mouseSensitivityY))
    return;
  g_levelAttr.set_double("msSensX", g_inGameShellState.mouseSensitivityX);
  g_levelAttr.set_double("msSensY", g_inGameShellState.mouseSensitivityY);
  g_levelAttr.set_int("msInvY", g_inGameShellState.mouseInvertY ? 1 : 0);
  g_hardware.m_ms.sensX = g_inGameShellState.mouseSensitivityX;
  g_hardware.m_ms.sensY = g_inGameShellState.mouseSensitivityY;
  g_hardware.m_ms.invY = g_inGameShellState.mouseInvertY ? 1 : 0;
}

void ApplyShellAudioSettingsToRuntime() {
  if (!g_inGameShellState.configured ||
      !ValidEffectsVolume(g_inGameShellState.effectsVolume) ||
      !ValidEffectsVolume(g_inGameShellState.vehicleVolume) ||
      !ValidEffectsVolume(g_inGameShellState.cinematicVolume))
    return;
  (void)SoundState_SetCategoryVolume(
      SOUND_STATE_CATEGORY_EFFECTS,
      static_cast<float>(g_inGameShellState.effectsVolume));
  (void)SoundState_SetCategoryVolume(
      SOUND_STATE_CATEGORY_VEHICLE,
      static_cast<float>(g_inGameShellState.vehicleVolume));
  (void)SoundState_SetCategoryVolume(
      SOUND_STATE_CATEGORY_CINEMATIC,
      static_cast<float>(g_inGameShellState.cinematicVolume));
}

std::wstring EscapeNativeMenuText(const std::wstring& text) {
  std::wstring escaped;
  escaped.reserve(text.size());
  for (wchar_t character : text) {
    if (character == L'&') escaped.push_back(L'&');
    escaped.push_back(character);
  }
  return escaped;
}

void ResizeSoftwareWindowForMenu(bool hasMenu) {
  if (_gr_hWnd == nullptr || _gr_nScreenWidth <= 0 ||
      _gr_nScreenHeight <= 0)
    return;
  const SRecoveredWindowPresentation presentation =
      RecoveredSoftwareGraph_Presentation();
  if (presentation.mode != RECOVERED_WINDOW_MODE_WINDOWED) return;
  RECT outer = {0, 0, presentation.clientWidth,
                presentation.clientHeight};
  const DWORD style =
      static_cast<DWORD>(GetWindowLongPtrW(_gr_hWnd, GWL_STYLE));
  const DWORD extendedStyle =
      static_cast<DWORD>(GetWindowLongPtrW(_gr_hWnd, GWL_EXSTYLE));
  if (AdjustWindowRectEx(&outer, style, hasMenu ? TRUE : FALSE,
                         extendedStyle) == FALSE)
    return;
  SetWindowPos(_gr_hWnd, nullptr, 0, 0, outer.right - outer.left,
               outer.bottom - outer.top,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool LevelIdentityMatches(const std::string& left,
                          const std::string& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(),
                    [](char first, char second) {
                      return std::tolower(
                                 static_cast<unsigned char>(first)) ==
                             std::tolower(
                                 static_cast<unsigned char>(second));
                    });
}

bool SlotTargetsCurrentLevel(const SLevelSaveSlot& archive) {
  return LevelIdentityMatches(archive.level,
                              ContinuationLevelIdentity());
}

bool SlotIsCompatible(const SLevelSaveSlot& archive) {
  return SlotTargetsCurrentLevel(archive) &&
         archive.contentFingerprint == ContinuationContentFingerprint();
}

bool SlotCanBeRequested(const SLevelSaveSlot& archive) {
  return !SlotTargetsCurrentLevel(archive) || SlotIsCompatible(archive);
}

constexpr std::uint32_t kShellSavePreviewWidth = 176u;
constexpr std::uint32_t kShellSavePreviewHeight = 132u;
constexpr std::size_t kShellPaletteBytes = 256u * 3u;
constexpr std::uint64_t kShellPaletteHashOffset =
    UINT64_C(14695981039346656037);
constexpr std::uint64_t kShellPaletteHashPrime = UINT64_C(1099511628211);

std::uint64_t ShellPaletteFingerprint() {
  std::uint64_t hash = kShellPaletteHashOffset;
  for (std::size_t index = 0; index < kShellPaletteBytes; ++index) {
    hash ^= _currPalette[index];
    hash *= kShellPaletteHashPrime;
  }
  return hash;
}

void PublishShellSaveCatalog(
    bool succeeded, SRecoveredSaveSlotCatalogSnapshot snapshot,
    const std::string& failure) {
  if (succeeded && snapshot.ready) {
    g_inGameShellSaveCatalog = std::move(snapshot);
    ++g_inGameShellState.saveCatalogPublications;
    return;
  }
  ++g_inGameShellState.saveCatalogFailures;
  if (!failure.empty())
    g_inGameShellState.status = "Save catalog unavailable: " + failure;
}

bool RequestShellSaveCatalogRefresh();

void PollShellSaveCatalog() {
  bool completed = false;
  bool succeeded = false;
  bool refreshAgain = false;
  SRecoveredSaveSlotCatalogSnapshot snapshot;
  std::string failure;
  {
    std::lock_guard<std::mutex> lock(g_inGameShellSaveCatalogMutex);
    if (g_inGameShellSaveCatalogCompleted) {
      completed = true;
      succeeded = g_inGameShellSaveCatalogBuildSucceeded;
      snapshot = std::move(g_pendingInGameShellSaveCatalog);
      failure = g_inGameShellSaveCatalogFailure;
      refreshAgain = g_inGameShellSaveCatalogRefreshRequested;
      g_inGameShellSaveCatalogCompleted = false;
      g_inGameShellSaveCatalogRefreshRequested = false;
      g_inGameShellSaveCatalogFailure.clear();
    }
  }
  if (!completed) return;
  if (g_inGameShellSaveCatalogWorker.joinable())
    g_inGameShellSaveCatalogWorker.join();
  PublishShellSaveCatalog(succeeded, std::move(snapshot), failure);
  if (refreshAgain) RequestShellSaveCatalogRefresh();
}

bool RequestShellSaveCatalogRefresh() {
  if (!g_saveMenuState.configured || g_saveMenuState.directory.empty() ||
      !g_sessionReady || !RecoveredSoftwareGraph_IsReady())
    return false;
  {
    std::lock_guard<std::mutex> lock(g_inGameShellSaveCatalogMutex);
    if (g_inGameShellSaveCatalogRunning ||
        g_inGameShellSaveCatalogCompleted) {
      g_inGameShellSaveCatalogRefreshRequested = true;
      return true;
    }
  }
  if (g_inGameShellSaveCatalogWorker.joinable())
    g_inGameShellSaveCatalogWorker.join();

  std::array<std::uint8_t, kShellPaletteBytes> palette = {};
  std::copy(_currPalette, _currPalette + kShellPaletteBytes,
            palette.begin());
  const std::wstring directory = g_saveMenuState.directory;
  const std::string level = ContinuationLevelIdentity();
  const std::uint64_t contentFingerprint =
      ContinuationContentFingerprint();
  const std::uint64_t generation =
      g_inGameShellSaveCatalogNextGeneration++;
  {
    std::lock_guard<std::mutex> lock(g_inGameShellSaveCatalogMutex);
    g_inGameShellSaveCatalogRunning = true;
    g_inGameShellSaveCatalogRefreshRequested = false;
  }
  try {
    g_inGameShellSaveCatalogWorker = std::thread(
        [directory, level, contentFingerprint, palette, generation]() {
          SRecoveredSaveSlotCatalogSnapshot snapshot;
          std::string failure;
          bool succeeded = false;
          try {
            succeeded = RecoveredSaveSlotCatalog_Build(
                directory, level, contentFingerprint, palette.data(),
                palette.size(), kShellSavePreviewWidth,
                kShellSavePreviewHeight, generation, &snapshot, &failure);
          } catch (const std::exception& exception) {
            failure = "save catalog worker exception: ";
            failure += exception.what();
          } catch (...) {
            failure = "save catalog worker failed unexpectedly";
          }
          std::lock_guard<std::mutex> lock(
              g_inGameShellSaveCatalogMutex);
          g_pendingInGameShellSaveCatalog = std::move(snapshot);
          g_inGameShellSaveCatalogFailure = failure;
          g_inGameShellSaveCatalogBuildSucceeded = succeeded;
          g_inGameShellSaveCatalogRunning = false;
          g_inGameShellSaveCatalogCompleted = true;
        });
  } catch (...) {
    std::lock_guard<std::mutex> lock(g_inGameShellSaveCatalogMutex);
    g_inGameShellSaveCatalogRunning = false;
    ++g_inGameShellState.saveCatalogFailures;
    g_inGameShellState.status =
        "Save catalog worker could not be started";
    return false;
  }
  ++g_inGameShellState.saveCatalogRefreshes;
  return true;
}

void StopShellSaveCatalog() {
  {
    std::lock_guard<std::mutex> lock(g_inGameShellSaveCatalogMutex);
    g_inGameShellSaveCatalogRefreshRequested = false;
  }
  if (g_inGameShellSaveCatalogWorker.joinable())
    g_inGameShellSaveCatalogWorker.join();

  bool completed = false;
  bool succeeded = false;
  SRecoveredSaveSlotCatalogSnapshot snapshot;
  std::string failure;
  {
    std::lock_guard<std::mutex> lock(g_inGameShellSaveCatalogMutex);
    completed = g_inGameShellSaveCatalogCompleted;
    succeeded = g_inGameShellSaveCatalogBuildSucceeded;
    if (completed) {
      snapshot = std::move(g_pendingInGameShellSaveCatalog);
      failure = g_inGameShellSaveCatalogFailure;
    }
    g_inGameShellSaveCatalogRunning = false;
    g_inGameShellSaveCatalogCompleted = false;
    g_inGameShellSaveCatalogBuildSucceeded = false;
    g_inGameShellSaveCatalogFailure.clear();
  }
  if (completed)
    PublishShellSaveCatalog(succeeded, std::move(snapshot), failure);
}

void ClearShellSaveCatalog() {
  g_inGameShellSaveCatalog = {};
  g_pendingInGameShellSaveCatalog = {};
  g_inGameShellSaveCatalogNextGeneration = 1u;
}

bool BuildDebugVehicleCatalog() {
  g_debugVehicleCatalog.clear();
  g_debugMenuState.vehicleTypeCount = 0;
  g_debugMenuState.firstOccupiedVehicleIndex = -1;
  g_debugMenuState.currentLevel = ContinuationLevelIdentity();
  if (!g_debugMenuState.configured) return true;

  std::vector<STaxiDebugVehicleType> taxiCatalog;
  std::string failure;
  ++g_debugMenuState.catalogBuilds;
  if (!TaxiSubjectState_DebugVehicleCatalog(
          g_super.m_context, &taxiCatalog, &failure)) {
    ++g_debugMenuState.catalogFailures;
    g_debugMenuState.lastError = failure;
    return false;
  }
  if (taxiCatalog.size() > kMaximumNativeDebugVehicleTypes) {
    ++g_debugMenuState.catalogFailures;
    g_debugMenuState.lastError =
        "Level defines more TaxiAttr entries than the native debug menu can "
        "represent";
    return false;
  }
  g_debugVehicleCatalog.reserve(taxiCatalog.size());
  for (const STaxiDebugVehicleType& taxi : taxiCatalog) {
    SRecoveredDebugVehicleType type;
    type.taxiAttribute = taxi.taxiAttribute;
    type.vehicleAttribute = taxi.vehicleAttribute;
    AttributeVehicle* attribute = nullptr;
    if (g_super.m_context != nullptr &&
        g_super.m_context->isExist(type.vehicleAttribute.c_str())) {
      attribute = static_cast<AttributeVehicle*>(
          __attrVehicleTable.searchAttribute(g_super.m_context->searchObject(
              type.vehicleAttribute.c_str())));
    }
    if (attribute != nullptr) {
      type.dynamic = attribute->m_dynamic;
      type.vehicleType = attribute->m_type;
      type.vesselProfile =
          VehicleRuntimeState_VesselProfile(attribute->m_dynamic);
      type.vesselKind =
          VehicleRuntimeState_VesselKind(attribute->m_dynamic);
      if (g_debugMenuState.firstOccupiedVehicleIndex < 0 &&
          type.vehicleType == 1 &&
          type.vesselProfile != RECOVERED_VEHICLE_PROFILE_UNKNOWN)
        g_debugMenuState.firstOccupiedVehicleIndex =
            static_cast<int>(g_debugVehicleCatalog.size());
    }
    g_debugVehicleCatalog.push_back(type);
  }
  g_debugMenuState.vehicleTypeCount =
      static_cast<unsigned int>(g_debugVehicleCatalog.size());
  g_debugMenuState.lastError.clear();
  return true;
}

void ObserveDebugTaxiSettlements() {
  if (g_debugTaxiSettlements.empty() || g_super.m_context == nullptr)
    return;
  for (std::vector<SPendingDebugTaxiSettlement>::iterator settlement =
           g_debugTaxiSettlements.begin();
       settlement != g_debugTaxiSettlements.end();) {
    double drift = 0.0;
    const bool stable = TaxiSubjectState_DebugPlacementDrift(
        g_super.m_context, settlement->objectName.c_str(),
        settlement->expectedPosition, &drift) &&
        drift <= kDebugTaxiSettlementTolerance;
    if (!stable) {
      ++g_debugMenuState.spawnSettlementFailures;
      settlement = g_debugTaxiSettlements.erase(settlement);
      continue;
    }
    settlement->maxDrift = (std::max)(settlement->maxDrift, drift);
    ++settlement->observedFrames;
    if (settlement->remainingFrames != 0)
      --settlement->remainingFrames;
    if (settlement->remainingFrames == 0) {
      ++g_debugMenuState.spawnSettlementProofs;
      g_debugMenuState.lastSpawnSettlementFrames =
          settlement->observedFrames;
      g_debugMenuState.maxSpawnSettlementDrift =
          (std::max)(g_debugMenuState.maxSpawnSettlementDrift,
                     settlement->maxDrift);
      settlement = g_debugTaxiSettlements.erase(settlement);
    } else {
      ++settlement;
    }
  }
}

bool DebugCommandCanStage() {
  if (!g_debugMenuState.configured || !g_sessionReady ||
      g_super.m_context == nullptr) {
    g_debugMenuState.lastError =
        "debug menu requires an active recovered Level session";
    return false;
  }
  if (g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_saveMenuState.pending || g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready ||
      g_scriptedLevelTransitionRequest.ready) {
    g_debugMenuState.lastError =
        "another world command is already pending";
    return false;
  }
  return true;
}

std::string DebugCommandLabel(ERecoveredDebugMenuAction action,
                              std::size_t index) {
  switch (action) {
    case RECOVERED_DEBUG_MENU_SHOW_STATE:
      return "Show current state";
    case RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE:
      return "Stabilize occupied vehicle";
    case RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE:
      return "Damage occupied vehicle by 25%";
    case RECOVERED_DEBUG_MENU_KILL_PLAYER:
      return "Kill player (transactional)";
    case RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH:
      return "Restore checkpoint before debug death";
    case RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE:
      return "Destroy occupied vehicle (transactional)";
    case RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION:
      return "Restore checkpoint before vehicle destruction";
    case RECOVERED_DEBUG_MENU_SPAWN_VEHICLE:
    case RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE: {
      if (index >= g_debugVehicleCatalog.size()) return "Vehicle unavailable";
      const SRecoveredDebugVehicleType& type = g_debugVehicleCatalog[index];
      std::string label = type.vehicleAttribute.empty()
                              ? "Vehicle"
                              : type.vehicleAttribute;
      if (!type.taxiAttribute.empty()) label += " [" + type.taxiAttribute + "]";
      if (!type.dynamic.empty()) label += " {" + type.dynamic + "}";
      return label;
    }
    case RECOVERED_DEBUG_MENU_SWITCH_LEVEL:
      return index < g_debugLevelCatalog.size()
                 ? g_debugLevelCatalog[index]
                 : "Level unavailable";
    default:
      return "Unsupported developer command";
  }
}

bool DebugCommandAvailability(ERecoveredDebugMenuAction action,
                              std::size_t index, std::string* reason) {
  const auto blocked = [&](const char* detail) {
    if (reason != nullptr) *reason = detail;
    return false;
  };
  if (!g_inGameShellState.developerMode || !g_debugMenuState.configured)
    return blocked("developer capability is disabled");
  if (!g_sessionReady || g_super.m_context == nullptr)
    return blocked("active recovered Level session is unavailable");
  if (g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_saveMenuState.pending || g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready ||
      g_scriptedLevelTransitionRequest.ready)
    return blocked("another world command is pending");

  if ((action == RECOVERED_DEBUG_MENU_SPAWN_VEHICLE ||
       action == RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE) &&
      index >= g_debugVehicleCatalog.size())
    return blocked("vehicle is outside the active Level catalog");
  if (action == RECOVERED_DEBUG_MENU_SWITCH_LEVEL &&
      index >= g_debugLevelCatalog.size())
    return blocked("Level is outside the configured catalog");
  if (action == RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH) {
    if (!g_debugMenuState.preDeathCheckpointAvailable ||
        g_debugPreDeathCheckpoint.empty())
      return blocked("no committed pre-death checkpoint");
    return true;
  }
  if (action == RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION) {
    if (!g_debugMenuState.preVehicleDestructionCheckpointAvailable ||
        g_debugPreVehicleDestructionCheckpoint.empty())
      return blocked("no committed pre-destruction checkpoint");
    return true;
  }
  if (action == RECOVERED_DEBUG_MENU_SPAWN_VEHICLE ||
      action == RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE ||
      action == RECOVERED_DEBUG_MENU_SWITCH_LEVEL)
    return true;

  KR_ObjectID vehicle =
      g_super.m_context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState vehicleState = {};
  if (vehicle.isNUL() || !VehicleRuntimeState_Inspect(
                              g_super.m_context, vehicle, &vehicleState))
    return blocked("default player Vehicle state is unavailable");
  Vehicle* controlled = static_cast<Vehicle*>(
      g_super.m_context->queryInterface(vehicle, IVehicleIID));

  if (action == RECOVERED_DEBUG_MENU_KILL_PLAYER) {
    if (g_debugMenuState.preDeathCheckpointAvailable ||
        g_debugMenuState.preVehicleDestructionCheckpointAvailable ||
        !g_debugPreDeathCheckpoint.empty() ||
        !g_debugPreVehicleDestructionCheckpoint.empty())
      return blocked("restore the existing debug checkpoint first");
    if (vehicleState.dead || vehicleState.takingTaxi ||
        controlled == nullptr || !controlled->taxiChangeEnabled())
      return blocked("requires the living default player body");
    if (g_vehicleControlInput.ActiveActionCount() != 0u)
      return blocked("neutral Vehicle controls are required");
  } else if (action == RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE ||
             action == RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE) {
    if (g_debugMenuState.preDeathCheckpointAvailable ||
        g_debugMenuState.preVehicleDestructionCheckpointAvailable ||
        !g_debugPreDeathCheckpoint.empty() ||
        !g_debugPreVehicleDestructionCheckpoint.empty())
      return blocked("restore the existing debug checkpoint first");
    if (vehicleState.dead || vehicleState.takingTaxi ||
        controlled == nullptr || controlled->taxiChangeEnabled())
      return blocked("requires a living occupied type-1 vehicle");
    if (action == RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE &&
        g_godMode != 0)
      return blocked("disable god mode before authentic destruction");
    if (g_vehicleControlInput.ActiveActionCount() != 0u)
      return blocked("neutral Vehicle controls are required");
  } else if (action != RECOVERED_DEBUG_MENU_SHOW_STATE &&
             action != RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE) {
    return blocked("command is not part of the supported catalog");
  }
  if (reason != nullptr) reason->clear();
  return true;
}

bool DeveloperCatalogEquivalent(
    const SRecoveredDeveloperCatalogSnapshot& left,
    const SRecoveredDeveloperCatalogSnapshot& right) {
  if (left.capabilityEnabled != right.capabilityEnabled ||
      left.ready != right.ready || left.reason != right.reason ||
      left.availableCommands != right.availableCommands ||
      left.blockedCommands != right.blockedCommands ||
      left.commands.size() != right.commands.size())
    return false;
  for (std::size_t index = 0; index < left.commands.size(); ++index) {
    const SRecoveredDeveloperCatalogEntry& a = left.commands[index];
    const SRecoveredDeveloperCatalogEntry& b = right.commands[index];
    if (a.action != b.action || a.index != b.index ||
        a.available != b.available || a.label != b.label ||
        a.reason != b.reason)
      return false;
  }
  return true;
}

void RefreshInGameDeveloperCatalog() {
  SRecoveredDeveloperCatalogSnapshot next;
  next.capabilityEnabled = g_inGameShellState.developerMode &&
                           g_debugMenuState.configured;
  if (!next.capabilityEnabled) {
    next.reason = "developer capability is disabled";
  } else {
    next.ready = true;
    const auto append = [&](ERecoveredDebugMenuAction action,
                            std::size_t index) {
      SRecoveredDeveloperCatalogEntry entry;
      entry.action = action;
      entry.index = index;
      entry.label = DebugCommandLabel(action, index);
      entry.available = DebugCommandAvailability(action, index, &entry.reason);
      if (entry.available)
        ++next.availableCommands;
      else
        ++next.blockedCommands;
      next.commands.push_back(std::move(entry));
    };
    append(RECOVERED_DEBUG_MENU_SHOW_STATE, 0u);
    append(RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE, 0u);
    append(RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE, 0u);
    append(RECOVERED_DEBUG_MENU_KILL_PLAYER, 0u);
    append(RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH, 0u);
    append(RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE, 0u);
    append(RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION, 0u);
    for (std::size_t index = 0; index < g_debugVehicleCatalog.size(); ++index) {
      append(RECOVERED_DEBUG_MENU_SPAWN_VEHICLE, index);
      append(RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE, index);
    }
    for (std::size_t index = 0; index < g_debugLevelCatalog.size(); ++index)
      append(RECOVERED_DEBUG_MENU_SWITCH_LEVEL, index);
  }
  if (!DeveloperCatalogEquivalent(g_inGameShellDeveloperCatalog, next)) {
    next.generation = g_inGameShellDeveloperCatalogNextGeneration++;
    g_inGameShellDeveloperCatalog = std::move(next);
    ++g_inGameShellState.developerCatalogPublications;
  }
}

bool StageDebugCommand(ERecoveredDebugMenuAction action,
                       std::size_t index) {
  g_debugMenuState.lastError.clear();
  if (!DebugCommandCanStage()) return false;
  g_debugMenuState.pending = true;
  g_debugMenuState.pendingAction = action;
  g_debugMenuState.pendingIndex = index;
  g_debugMenuState.pendingAttempts = 0;
  g_debugMenuState.lastCommandAttempts = 0;
  ++g_debugMenuState.requests;
  return true;
}

void RefreshNativeSaveMenu() {
  if (g_nativeSaveMenu == nullptr || g_nativeLoadMenu == nullptr ||
      !g_saveMenuState.configured)
    return;
  for (std::uint32_t slot = 0; slot < LevelSaveSlot_Count(); ++slot) {
    SLevelSaveSlot archive;
    SLevelSaveSlotStatus status;
    const bool readable = LevelSaveSlot_Read(
        g_saveMenuState.directory, slot, &archive, &status);
    const bool sameLevel = readable && SlotTargetsCurrentLevel(archive);
    const bool loadable = readable && SlotCanBeRequested(archive);
    std::wstring label =
        L"Slot " + std::to_wstring(slot + 1u) + L" - ";
    if (!readable) {
      const std::wstring path =
          LevelSaveSlot_Path(g_saveMenuState.directory, slot);
      const DWORD attributes =
          path.empty() ? INVALID_FILE_ATTRIBUTES
                       : GetFileAttributesW(path.c_str());
      label += attributes == INVALID_FILE_ATTRIBUTES
                   ? L"Empty"
                   : L"Corrupt or unsupported";
    } else {
      std::wstring title = Utf8ToWide(archive.title);
      if (title.empty()) title = L"Saved game";
      label += EscapeNativeMenuText(title);
      label += L" [";
      label += EscapeNativeMenuText(Utf8ToWide(archive.level));
      label += L"]";
      if (!sameLevel) {
        label += L" - switch Level";
      } else if (!loadable) {
        label += L" - incompatible retail data";
      }
    }
    const UINT saveCommand = kNativeSaveSlotBase + slot;
    const UINT loadCommand = kNativeLoadSlotBase + slot;
    ModifyMenuW(g_nativeSaveMenu, saveCommand,
                MF_BYCOMMAND | MF_STRING, saveCommand, label.c_str());
    ModifyMenuW(g_nativeLoadMenu, loadCommand,
                MF_BYCOMMAND | MF_STRING, loadCommand, label.c_str());
    EnableMenuItem(g_nativeLoadMenu, loadCommand,
                   MF_BYCOMMAND |
                       (loadable ? MF_ENABLED
                                 : MF_GRAYED | MF_DISABLED));
  }
  if (g_nativeGameMenu != nullptr) {
    const bool restartAvailable = g_sessionReady &&
        !g_saveMenuState.pending && !g_crossLevelLoadRequest.ready &&
        !g_saveMenuState.crossLevelRestartPending &&
        !g_debugMenuState.pending && !g_debugLevelSwitchRequest.ready &&
        !g_campaignRestartState.pending &&
        !g_campaignRestartState.coordinatorPending &&
        !g_campaignRestartRequest.ready &&
        !g_scriptedLevelTransitionRequest.ready;
    EnableMenuItem(
        g_nativeGameMenu, kNativeRestartCurrentLevel,
        MF_BYCOMMAND |
            (restartAvailable ? MF_ENABLED : MF_GRAYED | MF_DISABLED));
  }
  if (_gr_hWnd != nullptr) DrawMenuBar(_gr_hWnd);
}

void RefreshNativeDebugMenu() {
  if (g_nativeDebugMenu == nullptr || !g_debugMenuState.configured)
    return;
  const bool commandAvailable =
      g_sessionReady && !g_debugMenuState.pending &&
      !g_debugLevelSwitchRequest.ready && !g_saveMenuState.pending &&
      !g_crossLevelLoadRequest.ready &&
      !g_saveMenuState.crossLevelRestartPending &&
      !g_campaignRestartState.pending &&
      !g_campaignRestartState.coordinatorPending &&
      !g_campaignRestartRequest.ready &&
      !g_scriptedLevelTransitionRequest.ready;
  const UINT state = MF_BYCOMMAND |
      (commandAvailable ? MF_ENABLED : MF_GRAYED | MF_DISABLED);
  EnableMenuItem(g_nativeDebugMenu, kNativeDebugShowState, state);
  EnableMenuItem(g_nativeDebugMenu, kNativeDebugStabilize, state);
  const UINT killState = MF_BYCOMMAND |
      (commandAvailable && !g_debugMenuState.preDeathCheckpointAvailable &&
       !g_debugMenuState.preVehicleDestructionCheckpointAvailable
           ? MF_ENABLED
           : MF_GRAYED | MF_DISABLED);
  EnableMenuItem(g_nativeDebugMenu, kNativeDebugKillPlayer, killState);
  const UINT restoreState = MF_BYCOMMAND |
      (commandAvailable && g_debugMenuState.preDeathCheckpointAvailable
           ? MF_ENABLED
           : MF_GRAYED | MF_DISABLED);
  EnableMenuItem(g_nativeDebugMenu, kNativeDebugRestorePreDeath,
                 restoreState);
  const UINT destroyVehicleState = MF_BYCOMMAND |
      (commandAvailable && !g_debugMenuState.preDeathCheckpointAvailable &&
       !g_debugMenuState.preVehicleDestructionCheckpointAvailable
           ? MF_ENABLED
           : MF_GRAYED | MF_DISABLED);
  EnableMenuItem(g_nativeDebugMenu, kNativeDebugDestroyOccupiedVehicle,
                 destroyVehicleState);
  const UINT restoreVehicleState = MF_BYCOMMAND |
      (commandAvailable &&
       g_debugMenuState.preVehicleDestructionCheckpointAvailable
           ? MF_ENABLED
           : MF_GRAYED | MF_DISABLED);
  EnableMenuItem(g_nativeDebugMenu,
                 kNativeDebugRestorePreVehicleDestruction,
                 restoreVehicleState);
  EnableMenuItem(g_nativeDebugMenu, kNativeDebugDamageOccupiedVehicle,
                 destroyVehicleState);
  if (g_nativeDebugSpawnMenu != nullptr) {
    for (std::size_t index = 0; index < g_debugVehicleCatalog.size(); ++index) {
      EnableMenuItem(g_nativeDebugSpawnMenu,
                     kNativeDebugSpawnBase + static_cast<UINT>(index), state);
      EnableMenuItem(g_nativeDebugSpawnEnterMenu,
                     kNativeDebugSpawnEnterBase + static_cast<UINT>(index),
                     state);
    }
  }
  if (g_nativeDebugLevelMenu != nullptr) {
    for (std::size_t index = 0; index < g_debugLevelCatalog.size(); ++index) {
      EnableMenuItem(g_nativeDebugLevelMenu,
                     kNativeDebugLevelBase + static_cast<UINT>(index), state);
      CheckMenuItem(
          g_nativeDebugLevelMenu,
          kNativeDebugLevelBase + static_cast<UINT>(index),
          MF_BYCOMMAND |
              (LevelIdentityMatches(g_debugLevelCatalog[index],
                                    ContinuationLevelIdentity())
                   ? MF_CHECKED
                   : MF_UNCHECKED));
    }
  }
  if (_gr_hWnd != nullptr) DrawMenuBar(_gr_hWnd);
}

bool DeferDebugCommand(ERecoveredDebugMenuAction action,
                       std::size_t index, unsigned int attempt) {
  if (!IsRetryableDebugBoundaryFailure(g_debugMenuState.lastError) ||
      attempt >= kMaximumDebugStableBoundaryAttempts)
    return false;
  if (attempt == 1u)
    g_debugMenuState.firstDeferredError = g_debugMenuState.lastError;
  g_debugMenuState.pending = true;
  g_debugMenuState.pendingAction = action;
  g_debugMenuState.pendingIndex = index;
  g_debugMenuState.pendingAttempts = attempt;
  ++g_debugMenuState.deferredCommands;
  RefreshNativeDebugMenu();
  return true;
}

void DestroyNativeSaveMenu() {
  if (g_nativeMenuBar == nullptr) {
    g_saveMenuState.nativeMenuInstalled = false;
    g_debugMenuState.nativeMenuInstalled = false;
    return;
  }
  if (_gr_hWnd != nullptr && GetMenu(_gr_hWnd) == g_nativeMenuBar) {
    SetMenu(_gr_hWnd, nullptr);
    ResizeSoftwareWindowForMenu(false);
    DrawMenuBar(_gr_hWnd);
  }
  DestroyMenu(g_nativeMenuBar);
  g_nativeMenuBar = nullptr;
  g_nativeGameMenu = nullptr;
  g_nativeSaveMenu = nullptr;
  g_nativeLoadMenu = nullptr;
  g_nativeDebugMenu = nullptr;
  g_nativeDebugSpawnMenu = nullptr;
  g_nativeDebugSpawnEnterMenu = nullptr;
  g_nativeDebugLevelMenu = nullptr;
  g_saveMenuState.nativeMenuInstalled = false;
  g_debugMenuState.nativeMenuInstalled = false;
}

bool InstallNativeSaveMenu() {
  if (!g_nativeDiagnosticMenuEnabled) {
    DestroyNativeSaveMenu();
    return true;
  }
  if (!g_saveMenuState.configured || _gr_hWnd == nullptr ||
      !g_sessionReady)
    return false;
  if (g_nativeMenuBar != nullptr) {
    RefreshNativeSaveMenu();
    return true;
  }

  HMENU menuBar = CreateMenu();
  HMENU gameMenu = CreatePopupMenu();
  HMENU saveMenu = CreatePopupMenu();
  HMENU loadMenu = CreatePopupMenu();
  if (menuBar == nullptr || gameMenu == nullptr ||
      saveMenu == nullptr || loadMenu == nullptr) {
    if (menuBar != nullptr) DestroyMenu(menuBar);
    if (gameMenu != nullptr) DestroyMenu(gameMenu);
    if (saveMenu != nullptr) DestroyMenu(saveMenu);
    if (loadMenu != nullptr) DestroyMenu(loadMenu);
    g_saveMenuState.lastError = "CreateMenu failed";
    return false;
  }
  bool saveMenuAttached = false;
  bool loadMenuAttached = false;
  bool gameMenuAttached = false;
  const auto failConstruction = [&](const char* detail) {
    if (gameMenuAttached) {
      DestroyMenu(menuBar);
    } else {
      DestroyMenu(menuBar);
      DestroyMenu(gameMenu);
    }
    if (!saveMenuAttached) DestroyMenu(saveMenu);
    if (!loadMenuAttached) DestroyMenu(loadMenu);
    g_saveMenuState.lastError = detail;
    return false;
  };
  for (std::uint32_t slot = 0; slot < LevelSaveSlot_Count(); ++slot) {
    const std::wstring label =
        L"Slot " + std::to_wstring(slot + 1u) + L" - Empty";
    if (AppendMenuW(saveMenu, MF_STRING,
                    kNativeSaveSlotBase + slot, label.c_str()) == FALSE ||
        AppendMenuW(loadMenu, MF_STRING,
                    kNativeLoadSlotBase + slot, label.c_str()) == FALSE) {
      return failConstruction("AppendMenuW failed");
    }
  }
  if (AppendMenuW(gameMenu, MF_POPUP,
                  reinterpret_cast<UINT_PTR>(saveMenu),
                  L"&Save game") == FALSE)
    return failConstruction("native save submenu construction failed");
  saveMenuAttached = true;
  if (AppendMenuW(gameMenu, MF_POPUP,
                  reinterpret_cast<UINT_PTR>(loadMenu),
                  L"&Load game") == FALSE)
    return failConstruction("native load submenu construction failed");
  loadMenuAttached = true;
  if (AppendMenuW(gameMenu, MF_SEPARATOR, 0, nullptr) == FALSE ||
      AppendMenuW(gameMenu, MF_STRING, kNativeRestartCurrentLevel,
                  L"&Restart current Level") == FALSE ||
      AppendMenuW(gameMenu, MF_SEPARATOR, 0, nullptr) == FALSE ||
      AppendMenuW(gameMenu, MF_STRING, kNativeOpenSaveDirectory,
                  L"Open save &folder") == FALSE ||
      AppendMenuW(gameMenu, MF_SEPARATOR, 0, nullptr) == FALSE ||
      AppendMenuW(gameMenu, MF_STRING, kNativeExitGame,
                  L"E&xit") == FALSE)
    return failConstruction("native game menu construction failed");
  if (AppendMenuW(menuBar, MF_POPUP,
                  reinterpret_cast<UINT_PTR>(gameMenu),
                  L"&Game") == FALSE)
    return failConstruction("native menu bar construction failed");
  gameMenuAttached = true;

  HMENU debugMenu = nullptr;
  HMENU debugSpawnMenu = nullptr;
  HMENU debugSpawnEnterMenu = nullptr;
  HMENU debugLevelMenu = nullptr;
  if (g_debugMenuState.configured) {
    debugMenu = CreatePopupMenu();
    debugSpawnMenu = CreatePopupMenu();
    debugSpawnEnterMenu = CreatePopupMenu();
    debugLevelMenu = CreatePopupMenu();
    if (debugMenu == nullptr || debugSpawnMenu == nullptr ||
        debugSpawnEnterMenu == nullptr || debugLevelMenu == nullptr) {
      if (debugMenu != nullptr) DestroyMenu(debugMenu);
      if (debugSpawnMenu != nullptr) DestroyMenu(debugSpawnMenu);
      if (debugSpawnEnterMenu != nullptr)
        DestroyMenu(debugSpawnEnterMenu);
      if (debugLevelMenu != nullptr) DestroyMenu(debugLevelMenu);
      return failConstruction("CreatePopupMenu for Debug failed");
    }
    bool spawnAttached = false;
    bool enterAttached = false;
    bool levelAttached = false;
    const auto failDebugConstruction = [&](const char* detail) {
      DestroyMenu(debugMenu);
      if (!spawnAttached) DestroyMenu(debugSpawnMenu);
      if (!enterAttached) DestroyMenu(debugSpawnEnterMenu);
      if (!levelAttached) DestroyMenu(debugLevelMenu);
      return failConstruction(detail);
    };
    for (std::size_t index = 0; index < g_debugVehicleCatalog.size();
         ++index) {
      std::wstring label =
          EscapeNativeMenuText(Utf8ToWide(
              g_debugVehicleCatalog[index].vehicleAttribute));
      const std::wstring taxi =
          EscapeNativeMenuText(Utf8ToWide(
              g_debugVehicleCatalog[index].taxiAttribute));
      if (label.empty()) label = L"Vehicle";
      if (!taxi.empty()) label += L"  [" + taxi + L"]";
      if (!g_debugVehicleCatalog[index].dynamic.empty()) {
        label += L"  {" + EscapeNativeMenuText(Utf8ToWide(
            g_debugVehicleCatalog[index].dynamic)) + L"}";
      }
      if (AppendMenuW(
              debugSpawnMenu, MF_STRING,
              kNativeDebugSpawnBase + static_cast<UINT>(index),
              label.c_str()) == FALSE ||
          AppendMenuW(
              debugSpawnEnterMenu, MF_STRING,
              kNativeDebugSpawnEnterBase + static_cast<UINT>(index),
              label.c_str()) == FALSE)
        return failDebugConstruction(
            "native Debug vehicle catalog construction failed");
    }
    if (AppendMenuW(debugMenu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(debugSpawnMenu),
                    L"Spawn &vehicle nearby") == FALSE)
      return failDebugConstruction("native Debug spawn submenu failed");
    spawnAttached = true;
    if (AppendMenuW(debugMenu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(debugSpawnEnterMenu),
                    L"Spawn and &enter") == FALSE)
      return failDebugConstruction("native Debug enter submenu failed");
    enterAttached = true;
    if (AppendMenuW(debugMenu, MF_SEPARATOR, 0, nullptr) == FALSE ||
        AppendMenuW(debugMenu, MF_STRING, kNativeDebugShowState,
                    L"Show current &state") == FALSE ||
        AppendMenuW(debugMenu, MF_STRING, kNativeDebugStabilize,
                    L"Stop and move to last stable &position") == FALSE ||
        AppendMenuW(debugMenu, MF_SEPARATOR, 0, nullptr) == FALSE ||
        AppendMenuW(debugMenu, MF_STRING, kNativeDebugKillPlayer,
                    L"&Kill player (transactional)") == FALSE ||
        AppendMenuW(debugMenu, MF_STRING, kNativeDebugRestorePreDeath,
                    L"&Restore before debug death") == FALSE ||
        AppendMenuW(debugMenu, MF_SEPARATOR, 0, nullptr) == FALSE ||
        AppendMenuW(debugMenu, MF_STRING,
                    kNativeDebugDestroyOccupiedVehicle,
                    L"Destroy occupied &vehicle (transactional)") == FALSE ||
        AppendMenuW(debugMenu, MF_STRING,
                    kNativeDebugRestorePreVehicleDestruction,
                    L"Restore before vehicle destruction") == FALSE ||
        AppendMenuW(debugMenu, MF_STRING,
                    kNativeDebugDamageOccupiedVehicle,
                    L"Damage occupied vehicle by 25%") == FALSE ||
        AppendMenuW(debugMenu, MF_SEPARATOR, 0, nullptr) == FALSE)
      return failDebugConstruction("native Debug actions failed");
    for (std::size_t index = 0; index < g_debugLevelCatalog.size(); ++index) {
      std::wstring label = EscapeNativeMenuText(
          Utf8ToWide(g_debugLevelCatalog[index]));
      if (label.empty()) label = L"Level";
      if (AppendMenuW(
              debugLevelMenu, MF_STRING,
              kNativeDebugLevelBase + static_cast<UINT>(index),
              label.c_str()) == FALSE)
        return failDebugConstruction(
            "native Debug Level catalog construction failed");
    }
    if (AppendMenuW(debugMenu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(debugLevelMenu),
                    L"Switch &Level (fresh)") == FALSE)
      return failDebugConstruction("native Debug Level submenu failed");
    levelAttached = true;
    if (AppendMenuW(menuBar, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(debugMenu),
                    L"&Debug") == FALSE)
      return failDebugConstruction("native Debug menu bar failed");
  }
  if (SetMenu(_gr_hWnd, menuBar) == FALSE) {
    return failConstruction("SetMenu failed");
  }
  g_nativeMenuBar = menuBar;
  g_nativeGameMenu = gameMenu;
  g_nativeSaveMenu = saveMenu;
  g_nativeLoadMenu = loadMenu;
  g_nativeDebugMenu = debugMenu;
  g_nativeDebugSpawnMenu = debugSpawnMenu;
  g_nativeDebugSpawnEnterMenu = debugSpawnEnterMenu;
  g_nativeDebugLevelMenu = debugLevelMenu;
  g_saveMenuState.nativeMenuInstalled = true;
  g_debugMenuState.nativeMenuInstalled =
      g_debugMenuState.configured;
  ResizeSoftwareWindowForMenu(true);
  RefreshNativeSaveMenu();
  RefreshNativeDebugMenu();
  return true;
}

void RebasePausedRuntimeClock() {
  // Synchronous native dialogs and the in-frame pause shell both stop the
  // authoritative simulation loop. Their wall-clock dwell must not become a
  // later physics delta or make the next LCN1 capture perpetually unstable.
  g_timer.m_prevTime = static_cast<long>(GetTickCount());
  Session::m_frameSec = 0.0;
}

void ResetSaveMenuSession() {
  StopShellSaveCatalog();
  ClearShellSaveCatalog();
  if (g_inGameShellState.videoConfirmationActive &&
      RecoveredSoftwareGraph_IsReady()) {
    if (RecoveredSoftwareGraph_ApplyPresentation(
            g_inGameShellRollbackPresentation, nullptr)) {
      g_inGameShellAppliedPresentation =
          g_inGameShellRollbackPresentation;
      g_inGameShellState.windowMode = g_inGameShellPersistedWindowMode;
      g_inGameShellState.windowScale = g_inGameShellPersistedWindowScale;
      g_inGameShellState.exclusiveModeIndex =
          g_inGameShellRollbackExclusiveModeIndex;
      ++g_inGameShellState.videoRollbacks;
    }
  }
  g_windowsInputAdapter.LeaveOverlay();
  g_inGameShellState.open = false;
  g_inGameShellState.page = RECOVERED_SHELL_PAGE_ROOT;
  g_inGameShellState.selected = 0;
  g_inGameShellState.captureBinding = -1;
  g_inGameShellState.conflictBinding = -1;
  g_inGameShellState.overwriteConfirmation = false;
  g_inGameShellState.videoConfirmationActive = false;
  g_inGameShellState.pendingVideoCommand = RECOVERED_SHELL_VIDEO_NONE;
  g_inGameShellVideoDeadline = 0;
  DestroyNativeSaveMenu();
  g_debugVehicleCatalog.clear();
  g_debugTaxiSettlements.clear();
  g_debugPreDeathCheckpoint.clear();
  g_debugPreDeathCheckpointSummary = {};
  g_debugPreDeathCorpseCount = -1;
  g_debugPreVehicleDestructionCheckpoint.clear();
  g_debugPreVehicleDestructionCheckpointSummary = {};
  g_debugPreVehicleDestructionOrphanCount = -1;
  g_debugPreVehicleDestructionPanelReady = false;
  g_debugPreVehicleDestructionPanelOpen = false;
  g_debugMenuState.nativeMenuInstalled = false;
  g_debugMenuState.pending = false;
  g_debugMenuState.pendingAction = RECOVERED_DEBUG_MENU_NONE;
  g_debugMenuState.pendingIndex = 0;
  g_debugMenuState.pendingAttempts = 0;
  g_debugMenuState.vehicleTypeCount = 0;
  g_debugMenuState.preDeathCheckpointAvailable = false;
  g_debugMenuState.preVehicleDestructionCheckpointAvailable = false;
  g_debugMenuState.currentLevel.clear();
  g_saveMenuState.pending = false;
  g_saveMenuState.pendingAction = RECOVERED_SAVE_MENU_NONE;
  g_saveMenuState.pendingSlot = 0;
  g_saveMenuState.saveRequests = 0;
  g_saveMenuState.loadRequests = 0;
  g_saveMenuState.completedSaves = 0;
  g_saveMenuState.completedLoads = 0;
  g_saveMenuState.failedCommands = 0;
  g_saveMenuState.pendingAttempts = 0;
  g_saveMenuState.deferredCommands = 0;
  g_saveMenuState.lastCommandAttempts = 0;
  g_saveMenuState.lastError.clear();
  g_saveMenuState.pendingTitle.clear();
  g_saveMenuState.pendingDescription.clear();
  g_saveMenuState.lastRequestedTitle.clear();
  g_saveMenuState.lastRequestedDescription.clear();
  g_saveMenuState.lastPreview = {};
  g_saveMenuState.lastSlot = {};
  g_saveMenuState.lastContinuation = {};
  g_campaignRestartState = {};
  g_campaignRestartRequest = {};
  g_scriptedLevelTransitionState = {};
  g_scriptedLevelTransitionRequest = {};
  g_saveMenuAllowOverwrite = false;
}

std::string BuildAutomaticSaveTitle() {
  std::time_t now = std::time(nullptr);
  std::tm utc = {};
  char timestamp[32] = {};
  if (now > 0 && gmtime_s(&utc, &now) == 0)
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S UTC",
                  &utc);
  std::string title = ContinuationLevelIdentity();
  if (timestamp[0] != '\0') {
    title += " - ";
    title += timestamp;
  }
  return title;
}

void ShowNativeSaveFailure() {
  if (_gr_hWnd == nullptr || g_saveMenuState.lastError.empty()) return;
  const std::wstring detail = Utf8ToWide(g_saveMenuState.lastError);
  MessageBoxW(_gr_hWnd,
              detail.empty() ? L"Save/load command failed."
                             : detail.c_str(),
              L"RR2NW save/load error", MB_OK | MB_ICONERROR);
}

void ShowNativeDebugFailure() {
  if (_gr_hWnd == nullptr || g_debugMenuState.lastError.empty()) return;
  const std::wstring detail = Utf8ToWide(g_debugMenuState.lastError);
  MessageBoxW(_gr_hWnd,
              detail.empty() ? L"Debug command failed." : detail.c_str(),
              L"RR2NW debug command error", MB_OK | MB_ICONERROR);
}

void ShowNativeCampaignRestartFailure() {
  if (_gr_hWnd == nullptr || g_campaignRestartState.lastError.empty())
    return;
  const std::wstring detail =
      Utf8ToWide(g_campaignRestartState.lastError);
  MessageBoxW(_gr_hWnd,
              detail.empty() ? L"Current Level restart failed."
                             : detail.c_str(),
              L"RR2NW Level restart error", MB_OK | MB_ICONERROR);
}

bool HandleNativeSaveMenuMessage(HWND window, UINT message,
                                 WPARAM wParam, LRESULT* result) {
  if (!g_nativeDiagnosticMenuEnabled) return false;
  if (message == WM_INITMENUPOPUP && g_nativeMenuBar != nullptr) {
    RefreshNativeSaveMenu();
    RefreshNativeDebugMenu();
    *result = 0;
    return true;
  }
  if (message != WM_COMMAND || HIWORD(wParam) != 0) return false;
  const UINT command = LOWORD(wParam);
  if (command == kNativeRestartCurrentLevel) {
    if (!RecoveredGameServices_RequestCampaignRestart())
      ShowNativeCampaignRestartFailure();
    *result = 0;
    return true;
  }
  if (command >= kNativeDebugSpawnBase &&
      command < kNativeDebugSpawnBase +
                    static_cast<UINT>(g_debugVehicleCatalog.size())) {
    if (!RecoveredGameServices_RequestDebugVehicleSpawn(
            command - kNativeDebugSpawnBase, false))
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command >= kNativeDebugSpawnEnterBase &&
      command < kNativeDebugSpawnEnterBase +
                    static_cast<UINT>(g_debugVehicleCatalog.size())) {
    if (!RecoveredGameServices_RequestDebugVehicleSpawn(
            command - kNativeDebugSpawnEnterBase, true))
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeDebugShowState) {
    if (!RecoveredGameServices_RequestDebugShowState())
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeDebugStabilize) {
    if (!RecoveredGameServices_RequestDebugStabilizeVehicle())
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeDebugKillPlayer) {
    if (!RecoveredGameServices_RequestDebugKillPlayer())
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeDebugRestorePreDeath) {
    if (!RecoveredGameServices_RequestDebugRestorePreDeath())
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeDebugDestroyOccupiedVehicle) {
    if (!RecoveredGameServices_RequestDebugDestroyOccupiedVehicle())
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeDebugRestorePreVehicleDestruction) {
    if (!RecoveredGameServices_RequestDebugRestorePreVehicleDestruction())
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeDebugDamageOccupiedVehicle) {
    if (!RecoveredGameServices_RequestDebugDamageOccupiedVehicle())
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command >= kNativeDebugLevelBase &&
      command < kNativeDebugLevelBase +
                    static_cast<UINT>(g_debugLevelCatalog.size())) {
    if (!RecoveredGameServices_RequestDebugLevelSwitch(
            command - kNativeDebugLevelBase))
      ShowNativeDebugFailure();
    *result = 0;
    return true;
  }
  if (command >= kNativeSaveSlotBase &&
      command < kNativeSaveSlotBase + LevelSaveSlot_Count()) {
    const std::uint32_t slot = command - kNativeSaveSlotBase;
    const std::wstring path =
        LevelSaveSlot_Path(g_saveMenuState.directory, slot);
    const bool occupied =
        !path.empty() &&
        GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    SRecoveredSaveSlotDialogInput input;
    input.parent = window;
    input.mode = RECOVERED_SAVE_SLOT_DIALOG_SAVE;
    input.slot = slot;
    input.occupied = occupied;
    input.automaticTitle = BuildAutomaticSaveTitle();
    input.automaticDescription =
        "RR2NW recovered Windows menu checkpoint";
    SLevelSaveSlotStatus status;
    input.readable = occupied && LevelSaveSlot_Read(
        g_saveMenuState.directory, slot, &input.archive, &status);
    input.loadable = input.readable && SlotCanBeRequested(input.archive);
    input.switchesLevel =
        input.readable && !SlotTargetsCurrentLevel(input.archive);
    SRecoveredSaveSlotDialogResult dialog;
    std::string dialogFailure;
    if (!RecoveredSaveSlotDialog_Show(input, &dialog, &dialogFailure)) {
      g_saveMenuState.lastError = dialogFailure;
      ShowNativeSaveFailure();
      *result = 0;
      return true;
    }
    RebasePausedRuntimeClock();
    ++g_saveMenuState.slotDetailViews;
    if (dialog.previewDisplayed) ++g_saveMenuState.previewViews;
    if (dialog.previewDecodeFailed)
      ++g_saveMenuState.previewDecodeFailures;
    if (!dialog.accepted) {
      *result = 0;
      return true;
    }
    if (occupied) {
      const std::wstring prompt =
          L"Replace save slot " + std::to_wstring(slot + 1u) + L"?";
      if (MessageBoxW(window, prompt.c_str(), L"RR2NW save game",
                      MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        *result = 0;
        return true;
      }
    }
    if (!RecoveredGameServices_RequestSaveSlotWithMetadata(
            slot, occupied, dialog.title, dialog.description))
      ShowNativeSaveFailure();
    *result = 0;
    return true;
  }
  if (command >= kNativeLoadSlotBase &&
      command < kNativeLoadSlotBase + LevelSaveSlot_Count()) {
    const std::uint32_t slot = command - kNativeLoadSlotBase;
    SRecoveredSaveSlotDialogInput input;
    input.parent = window;
    input.mode = RECOVERED_SAVE_SLOT_DIALOG_LOAD;
    input.slot = slot;
    input.occupied = true;
    SLevelSaveSlotStatus status;
    input.readable = LevelSaveSlot_Read(
        g_saveMenuState.directory, slot, &input.archive, &status);
    if (!input.readable) {
      g_saveMenuState.lastError = status.detail;
      ShowNativeSaveFailure();
      *result = 0;
      return true;
    }
    input.loadable = SlotCanBeRequested(input.archive);
    input.switchesLevel = !SlotTargetsCurrentLevel(input.archive);
    SRecoveredSaveSlotDialogResult dialog;
    std::string dialogFailure;
    if (!RecoveredSaveSlotDialog_Show(input, &dialog, &dialogFailure)) {
      g_saveMenuState.lastError = dialogFailure;
      ShowNativeSaveFailure();
      *result = 0;
      return true;
    }
    RebasePausedRuntimeClock();
    ++g_saveMenuState.slotDetailViews;
    if (dialog.previewDisplayed) ++g_saveMenuState.previewViews;
    if (dialog.previewDecodeFailed)
      ++g_saveMenuState.previewDecodeFailures;
    if (dialog.accepted &&
        !RecoveredGameServices_RequestLoadSlot(slot))
      ShowNativeSaveFailure();
    *result = 0;
    return true;
  }
  if (command == kNativeOpenSaveDirectory) {
    const HINSTANCE launched =
        ShellExecuteW(window, L"open", g_saveMenuState.directory.c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(launched) <= 32) {
      g_saveMenuState.lastError = "Windows could not open the save folder";
      ShowNativeSaveFailure();
    }
    *result = 0;
    return true;
  }
  if (command == kNativeExitGame) {
    PostMessageW(window, WM_CLOSE, 0, 0);
    *result = 0;
    return true;
  }
  return false;
}

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
      !BindHardwareControl(FIRE_PRIMARY, "MouseL") ||
      !BindHardwareControl(FIRE_SECONDARY, "MouseR") ||
      !BindHardwareControl(STOP_VEHICLE, "X") ||
      !BindHardwareControl(CHANGE_VEHICLE, "F1") ||
      !BindHardwareControl(EXIT, "Esc")) {
    return false;
  }
  g_hardware.Link();
  return true;
}

double CurrentInputEventTime() {
  // Window messages are queued into the simulation, so stamp them at the
  // currently owned event boundary. A wall-clock stamp can sit ahead of the
  // simulation and let a later focus transition overtake still-future input.
  const double eventMoment = Session::m_moment;
  return !std::isfinite(eventMoment) || eventMoment < 0.1
             ? 0.1
             : eventMoment;
}

bool DispatchWindowsInputAction(
    const SRecoveredWindowsInputAction& input, double eventTime) {
  if (input.action == DMAP_TOGGLE) {
    if (input.value <= 0.0) return true;
    ++g_mapTogglePresses;
    if (!g_debugMap.IsInitialized() ||
        g_debugMap.getContext() == nullptr)
      return false;
    const bool opening = !g_debugMap.IsActive();
    if (opening && g_vehicleControlReady &&
        !g_vehicleControlInput.NeutralizeForOverlay(eventTime))
      return false;
    KR_Event event;
    event.source = g_hardware.getObjectID();
    event.destination = g_debugMap.getObjectID();
    event.timeStamp = eventTime;
    event.label = CTRL_BUTTONS_MSG;
    event.data.open(EDO_WRITE)
        .putInt(input.action)
        .putDouble(input.value)
        .putInt(static_cast<int>(input.code))
        .putInt(input.repeat)
        .close();
    const bool wasActive = g_debugMap.IsActive() != 0;
    return g_debugMap.receiveEvent(event) == 1 &&
           (g_debugMap.IsActive() != 0) != wasActive;
  }
  if (g_debugMap.IsActive()) {
    KR_Event event;
    event.source = g_hardware.getObjectID();
    event.destination = g_debugMap.getObjectID();
    event.timeStamp = eventTime;
    event.label = CTRL_BUTTONS_MSG;
    event.data.open(EDO_WRITE)
        .putInt(input.action)
        .putDouble(input.value)
        .putInt(static_cast<int>(input.code))
        .putInt(input.repeat)
        .close();
    return g_debugMap.receiveEvent(event) == 1;
  }
  switch (input.action) {
    case DMAP_TOGGLE_OBST:
    case DMAP_TOGGLE_ROUTE:
    case DMAP_TOGGLE_OBJ:
    case DMAP_TOGGLE_OBJINFO:
    case DMAP_SCROLL_UP:
    case DMAP_SCROLL_DOWN:
    case DMAP_SCROLL_LEFT:
    case DMAP_SCROLL_RIGHT:
    case DMAP_TOGGLE_FOLLOW_MODE:
    case DMAP_NEXT_MISSION:
    case DMAP_PREVIOUS_MISSION:
    case DMAP_TEXT_BOX_UP:
    case DMAP_TEXT_BOX_DOWN:
      return true;
    default:
      break;
  }
  KR_Event event;
  event.source = g_hardware.getObjectID();
  event.timeStamp = eventTime;
  event.label = CTRL_BUTTONS_MSG;
  event.data.open(EDO_WRITE)
      .putInt(input.action)
      .putDouble(input.value)
      .putInt(static_cast<int>(input.code))
      .putInt(input.repeat)
      .close();
  if (g_vehicleControlReady && g_vehicleControlInput.getContext() != nullptr) {
    event.destination = g_vehicleControlInput.getObjectID();
    return g_vehicleControlInput.receiveEvent(event) == 1;
  }
  if (g_observerInput.getContext() != nullptr) {
    event.destination = g_observerInput.getObjectID();
    return g_observerInput.receiveEvent(event) == 1;
  }
  return false;
}

bool DispatchWindowsInputBatch(const SRecoveredWindowsInputBatch& batch) {
  const std::size_t required = batch.count +
      (batch.applicationActiveChanged ? 1u : 0u);
  if (required > kMaximumPendingWindowsInput -
                     (std::min)(kMaximumPendingWindowsInput,
                                g_pendingWindowsInput.size()))
    return false;
  for (std::size_t index = 0; index < batch.count; ++index) {
    SRecoveredPendingWindowsInput pending;
    pending.action = batch.actions[index];
    g_pendingWindowsInput.push_back(pending);
  }
  if (batch.applicationActiveChanged) {
    SRecoveredPendingWindowsInput pending;
    pending.focus = true;
    pending.applicationActive = batch.applicationActive;
    g_pendingWindowsInput.push_back(pending);
  }
  return true;
}

bool FlushPendingWindowsInput(double eventTime) {
  bool succeeded = true;
  for (const SRecoveredPendingWindowsInput& pending : g_pendingWindowsInput) {
    if (pending.focus) {
      g_observerInput.SetApplicationActive(pending.applicationActive);
      if (g_vehicleControlReady &&
          !g_vehicleControlInput.SetApplicationActive(
              pending.applicationActive, eventTime)) {
        succeeded = false;
        break;
      }
      continue;
    }
    if (!DispatchWindowsInputAction(pending.action, eventTime)) {
      succeeded = false;
      break;
    }
  }
  // Never replay a partially dispatched batch after an input failure. The
  // caller owns fallback activation, while this FIFO owns batch lifetime.
  g_pendingWindowsInput.clear();
  return succeeded;
}

std::size_t ShellPageItemCount() {
  RefreshInGameDeveloperCatalog();
  switch (g_inGameShellState.page) {
    case RECOVERED_SHELL_PAGE_ROOT:
      return g_inGameShellState.developerMode &&
                     g_debugMenuState.configured
                 ? 9u
                 : 8u;
    case RECOVERED_SHELL_PAGE_SAVE:
    case RECOVERED_SHELL_PAGE_LOAD:
      return LevelSaveSlot_Count() + 1u;
    case RECOVERED_SHELL_PAGE_CONTROLS:
      return RECOVERED_BIND_COUNT + 5u;
    case RECOVERED_SHELL_PAGE_VIDEO:
      return 4u;
    case RECOVERED_SHELL_PAGE_AUDIO:
      return 4u;
    case RECOVERED_SHELL_PAGE_DEVELOPER:
      return 11u;
    case RECOVERED_SHELL_PAGE_VIDEO_CONFIRM:
      return 2u;
    case RECOVERED_SHELL_PAGE_DEVELOPER_SPAWN:
    case RECOVERED_SHELL_PAGE_DEVELOPER_ENTER:
      return g_debugVehicleCatalog.size() + 1u;
    case RECOVERED_SHELL_PAGE_DEVELOPER_LEVEL:
      return g_debugLevelCatalog.size() + 1u;
    default:
      return 1u;
  }
}

void ShellSelectPage(ERecoveredInGameShellPage page) {
  g_inGameShellState.page = page;
  g_inGameShellState.selected = 0;
  g_inGameShellState.captureBinding = -1;
  g_inGameShellState.conflictBinding = -1;
  g_inGameShellState.overwriteConfirmation = false;
  if (page == RECOVERED_SHELL_PAGE_SAVE ||
      page == RECOVERED_SHELL_PAGE_LOAD)
    RequestShellSaveCatalogRefresh();
  if (page == RECOVERED_SHELL_PAGE_DEVELOPER ||
      page == RECOVERED_SHELL_PAGE_DEVELOPER_SPAWN ||
      page == RECOVERED_SHELL_PAGE_DEVELOPER_ENTER ||
      page == RECOVERED_SHELL_PAGE_DEVELOPER_LEVEL)
    RefreshInGameDeveloperCatalog();
}

bool OpenInGameShell() {
  if (!g_inGameShellState.configured || g_inGameShellState.open)
    return false;
  SRecoveredWindowsInputBatch releases = {};
  const double sensitivity = g_levelAttr.get_double("keySens");
  if (!g_windowsInputAdapter.EnterOverlay(sensitivity, &releases) ||
      !DispatchWindowsInputBatch(releases)) {
    g_inGameShellState.lastError =
        "held gameplay input could not be neutralized";
    Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
    return false;
  }
  g_inGameShellState.open = true;
  ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
  ++g_inGameShellState.opens;
  ++g_inGameShellState.inputNeutralizations;
  g_inGameShellState.status = "Simulation paused";
  return true;
}

void CloseInGameShell() {
  if (!g_inGameShellState.open) return;
  if (g_inGameShellState.videoConfirmationActive) {
    g_inGameShellState.pendingVideoCommand =
        RECOVERED_SHELL_VIDEO_REVERT;
    return;
  }
  g_windowsInputAdapter.LeaveOverlay();
  g_inGameShellState.open = false;
  ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
  ++g_inGameShellState.closes;
}

bool PersistShellSettings(const char* success) {
  if (!WriteInGameShellSettings()) {
    g_inGameShellState.lastError = "settings.cfg atomic write failed";
    Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
    return false;
  }
  g_inGameShellState.lastError.clear();
  g_inGameShellState.status = success;
  return true;
}

bool AdjustShellMouseSensitivity(double* value, int direction) {
  if (value == nullptr || direction == 0) return false;
  const double adjusted = (std::max)(
      kMinimumMouseSensitivity,
      (std::min)(kMaximumMouseSensitivity,
                 *value + (direction < 0 ? -kMouseSensitivityStep
                                         : kMouseSensitivityStep)));
  const double rounded = std::round(adjusted * 100.0) / 100.0;
  if (std::fabs(rounded - *value) <= 1.0e-12) return true;
  *value = rounded;
  ++g_inGameShellState.mouseSettingChanges;
  ApplyShellMouseSettingsToRuntime();
  return PersistShellSettings("Mouse sensitivity saved");
}

bool AdjustShellEffectsVolume(int direction) {
  if (direction == 0) return false;
  const double adjusted = (std::max)(
      0.0, (std::min)(1.0, g_inGameShellState.effectsVolume +
                              (direction < 0 ? -kEffectsVolumeStep
                                             : kEffectsVolumeStep)));
  const double rounded = std::round(adjusted * 10.0) / 10.0;
  if (std::fabs(rounded - g_inGameShellState.effectsVolume) <= 1.0e-12)
    return true;
  g_inGameShellState.effectsVolume = rounded;
  ++g_inGameShellState.audioSettingChanges;
  ApplyShellAudioSettingsToRuntime();
  return PersistShellSettings("Effects volume saved");
}

bool AdjustShellVehicleVolume(int direction) {
  if (direction == 0) return false;
  const double adjusted = (std::max)(
      0.0, (std::min)(1.0, g_inGameShellState.vehicleVolume +
                              (direction < 0 ? -kEffectsVolumeStep
                                             : kEffectsVolumeStep)));
  const double rounded = std::round(adjusted * 10.0) / 10.0;
  if (std::fabs(rounded - g_inGameShellState.vehicleVolume) <= 1.0e-12)
    return true;
  g_inGameShellState.vehicleVolume = rounded;
  ++g_inGameShellState.audioSettingChanges;
  ApplyShellAudioSettingsToRuntime();
  return PersistShellSettings("Vehicle volume saved");
}

bool AdjustShellCinematicVolume(int direction) {
  if (direction == 0) return false;
  const double adjusted = (std::max)(
      0.0, (std::min)(1.0, g_inGameShellState.cinematicVolume +
                              (direction < 0 ? -kEffectsVolumeStep
                                             : kEffectsVolumeStep)));
  const double rounded = std::round(adjusted * 10.0) / 10.0;
  if (std::fabs(rounded - g_inGameShellState.cinematicVolume) <= 1.0e-12)
    return true;
  g_inGameShellState.cinematicVolume = rounded;
  ++g_inGameShellState.audioSettingChanges;
  ApplyShellAudioSettingsToRuntime();
  return PersistShellSettings("Briefing/cinematic volume saved");
}

bool ToggleShellMouseInvertY() {
  g_inGameShellState.mouseInvertY = !g_inGameShellState.mouseInvertY;
  ++g_inGameShellState.mouseSettingChanges;
  ApplyShellMouseSettingsToRuntime();
  return PersistShellSettings("Mouse invert-Y saved");
}

bool CaptureShellBinding(std::uint32_t key) {
  if (g_inGameShellState.captureBinding < 0 ||
      g_inGameShellState.captureBinding >=
          static_cast<int>(RECOVERED_BIND_COUNT))
    return false;
  SRecoveredInputBindings candidate = g_inGameShellBindings;
  const std::size_t changed = static_cast<std::size_t>(
      g_inGameShellState.captureBinding);
  candidate.key[changed] = key;
  std::size_t first = 0;
  std::size_t second = 0;
  if (!RecoveredWindowsInput_ValidateBindings(candidate, &first, &second)) {
    g_inGameShellState.conflictBinding =
        first == changed ? static_cast<int>(second)
                         : static_cast<int>(first);
    ++g_inGameShellState.bindingConflicts;
    g_inGameShellState.status = "Binding conflict; choose another key";
    return true;
  }
  if (!g_windowsInputAdapter.SetBindings(candidate)) {
    g_inGameShellState.lastError =
        "binding change rejected while physical input is active";
    return true;
  }
  g_inGameShellBindings = candidate;
  g_inGameShellState.captureBinding = -1;
  g_inGameShellState.conflictBinding = -1;
  ++g_inGameShellState.bindingChanges;
  PersistShellSettings("Control binding saved");
  return true;
}

bool ActivateShellSaveSlot(std::uint32_t slot) {
  const std::wstring path =
      LevelSaveSlot_Path(g_saveMenuState.directory, slot);
  const bool occupied = !path.empty() &&
      GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
  if (occupied && (!g_inGameShellState.overwriteConfirmation ||
                   g_inGameShellState.overwriteSlot != slot)) {
    g_inGameShellState.overwriteConfirmation = true;
    g_inGameShellState.overwriteSlot = slot;
    ++g_inGameShellState.saveOverwriteConfirmations;
    g_inGameShellState.status =
        "Press Enter again to replace this save slot";
    return true;
  }
  if (!RecoveredGameServices_RequestSaveSlotWithMetadata(
          slot, occupied, BuildAutomaticSaveTitle(),
          "RR2NW in-game shell checkpoint")) {
    g_inGameShellState.lastError = g_saveMenuState.lastError;
    return true;
  }
  ++g_inGameShellState.saveRequests;
  g_inGameShellState.status = "Save queued at the closed frame boundary";
  CloseInGameShell();
  return true;
}

bool ActivateShellLoadSlot(std::uint32_t slot) {
  if (!RecoveredGameServices_RequestLoadSlot(slot)) {
    g_inGameShellState.lastError = g_saveMenuState.lastError;
    return true;
  }
  ++g_inGameShellState.loadRequests;
  g_inGameShellState.status = "Load queued at the closed frame boundary";
  CloseInGameShell();
  return true;
}

const SRecoveredDeveloperCatalogEntry* DeveloperCatalogEntry(
    ERecoveredDebugMenuAction action, std::size_t index) {
  RefreshInGameDeveloperCatalog();
  for (const SRecoveredDeveloperCatalogEntry& entry :
       g_inGameShellDeveloperCatalog.commands) {
    if (entry.action == action && entry.index == index) return &entry;
  }
  return nullptr;
}

bool StageDeveloperShellCommand(ERecoveredDebugMenuAction action,
                                std::size_t index) {
  const SRecoveredDeveloperCatalogEntry* entry =
      DeveloperCatalogEntry(action, index);
  if (entry == nullptr || !entry->available) {
    ++g_inGameShellState.developerCatalogBlockedSelections;
    g_inGameShellState.status = "Unavailable: " +
        (entry == nullptr ? std::string("command is outside the catalog")
                          : entry->reason);
    return true;
  }
  bool staged = false;
  switch (action) {
    case RECOVERED_DEBUG_MENU_SHOW_STATE:
      staged = RecoveredGameServices_RequestDebugShowState();
      break;
    case RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE:
      staged = RecoveredGameServices_RequestDebugStabilizeVehicle();
      break;
    case RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE:
      staged = RecoveredGameServices_RequestDebugDamageOccupiedVehicle();
      break;
    case RECOVERED_DEBUG_MENU_KILL_PLAYER:
      staged = RecoveredGameServices_RequestDebugKillPlayer();
      break;
    case RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH:
      staged = RecoveredGameServices_RequestDebugRestorePreDeath();
      break;
    case RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE:
      staged = RecoveredGameServices_RequestDebugDestroyOccupiedVehicle();
      break;
    case RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION:
      staged = RecoveredGameServices_RequestDebugRestorePreVehicleDestruction();
      break;
    case RECOVERED_DEBUG_MENU_SPAWN_VEHICLE:
      staged = RecoveredGameServices_RequestDebugVehicleSpawn(index, false);
      break;
    case RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE:
      staged = RecoveredGameServices_RequestDebugVehicleSpawn(index, true);
      break;
    case RECOVERED_DEBUG_MENU_SWITCH_LEVEL:
      staged = RecoveredGameServices_RequestDebugLevelSwitch(index);
      break;
    default: return false;
  }
  if (!staged) {
    g_inGameShellState.lastError = g_debugMenuState.lastError;
    return true;
  }
  ++g_inGameShellState.developerCommandsQueued;
  g_inGameShellState.status =
      "Developer command queued at the closed frame boundary";
  CloseInGameShell();
  return true;
}

bool ActivateDeveloperShellItem(std::size_t item) {
  static const ERecoveredDebugMenuAction kFixedActions[] = {
      RECOVERED_DEBUG_MENU_SHOW_STATE,
      RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE,
      RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE,
      RECOVERED_DEBUG_MENU_KILL_PLAYER,
      RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH,
      RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE,
      RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION};
  if (item < sizeof(kFixedActions) / sizeof(kFixedActions[0]))
    return StageDeveloperShellCommand(kFixedActions[item], 0u);
  if (item == 7u) {
    ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER_SPAWN);
    return true;
  }
  if (item == 8u) {
    ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER_ENTER);
    return true;
  }
  if (item == 9u) {
    ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER_LEVEL);
    return true;
  }
  if (item == 10u) {
    ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
    return true;
  }
  return false;
}

bool ActivateShellSelection() {
  const std::size_t selected = g_inGameShellState.selected;
  switch (g_inGameShellState.page) {
    case RECOVERED_SHELL_PAGE_ROOT: {
      if (selected == 0u) {
        CloseInGameShell();
      } else if (selected == 1u) {
        ShellSelectPage(RECOVERED_SHELL_PAGE_SAVE);
      } else if (selected == 2u) {
        ShellSelectPage(RECOVERED_SHELL_PAGE_LOAD);
      } else if (selected == 3u) {
        if (RecoveredGameServices_RequestCampaignRestart()) {
          ++g_inGameShellState.restartRequests;
          CloseInGameShell();
        } else {
          g_inGameShellState.lastError = g_campaignRestartState.lastError;
        }
      } else if (selected == 4u) {
        ShellSelectPage(RECOVERED_SHELL_PAGE_CONTROLS);
      } else if (selected == 5u) {
        ShellSelectPage(RECOVERED_SHELL_PAGE_VIDEO);
      } else if (selected == 6u) {
        ShellSelectPage(RECOVERED_SHELL_PAGE_AUDIO);
      } else if (selected == 7u && g_inGameShellState.developerMode &&
                 g_debugMenuState.configured) {
        ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER);
      } else {
        if (_gr_hWnd != nullptr) PostMessageW(_gr_hWnd, WM_CLOSE, 0, 0);
        CloseInGameShell();
      }
      return true;
    }
    case RECOVERED_SHELL_PAGE_SAVE:
      if (selected < LevelSaveSlot_Count())
        return ActivateShellSaveSlot(static_cast<std::uint32_t>(selected));
      ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
      return true;
    case RECOVERED_SHELL_PAGE_LOAD:
      if (selected < LevelSaveSlot_Count())
        return ActivateShellLoadSlot(static_cast<std::uint32_t>(selected));
      ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
      return true;
    case RECOVERED_SHELL_PAGE_CONTROLS:
      if (selected < RECOVERED_BIND_COUNT) {
        g_inGameShellState.captureBinding = static_cast<int>(selected);
        g_inGameShellState.conflictBinding = -1;
        g_inGameShellState.status = "Press a new key or mouse button";
      } else if (selected == RECOVERED_BIND_COUNT) {
        return AdjustShellMouseSensitivity(
            &g_inGameShellState.mouseSensitivityX, 1);
      } else if (selected == RECOVERED_BIND_COUNT + 1u) {
        return AdjustShellMouseSensitivity(
            &g_inGameShellState.mouseSensitivityY, 1);
      } else if (selected == RECOVERED_BIND_COUNT + 2u) {
        return ToggleShellMouseInvertY();
      } else if (selected == RECOVERED_BIND_COUNT + 3u) {
        const SRecoveredInputBindings defaults =
            RecoveredWindowsInput_DefaultBindings();
        if (g_windowsInputAdapter.SetBindings(defaults)) {
          g_inGameShellBindings = defaults;
          g_inGameShellState.mouseSensitivityX =
              kDefaultMouseSensitivity;
          g_inGameShellState.mouseSensitivityY =
              kDefaultMouseSensitivity;
          g_inGameShellState.mouseInvertY = false;
          ApplyShellMouseSettingsToRuntime();
          ++g_inGameShellState.bindingChanges;
          PersistShellSettings("Default controls restored");
        }
      } else {
        ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
      }
      return true;
    case RECOVERED_SHELL_PAGE_VIDEO:
      if (selected == 2u) {
        g_inGameShellState.pendingVideoCommand =
            RECOVERED_SHELL_VIDEO_APPLY;
        g_inGameShellState.status =
            "Video change queued at the closed frame boundary";
      } else if (selected == 3u) {
        ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
      }
      return true;
    case RECOVERED_SHELL_PAGE_AUDIO:
      if (selected == 0u)
        return AdjustShellEffectsVolume(1);
      if (selected == 1u)
        return AdjustShellVehicleVolume(1);
      if (selected == 2u)
        return AdjustShellCinematicVolume(1);
      ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
      return true;
    case RECOVERED_SHELL_PAGE_DEVELOPER:
      return ActivateDeveloperShellItem(selected);
    case RECOVERED_SHELL_PAGE_DEVELOPER_SPAWN:
      if (selected < g_debugVehicleCatalog.size())
        return StageDeveloperShellCommand(
            RECOVERED_DEBUG_MENU_SPAWN_VEHICLE, selected);
      ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER);
      return true;
    case RECOVERED_SHELL_PAGE_DEVELOPER_ENTER:
      if (selected < g_debugVehicleCatalog.size())
        return StageDeveloperShellCommand(
            RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE, selected);
      ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER);
      return true;
    case RECOVERED_SHELL_PAGE_DEVELOPER_LEVEL:
      if (selected < g_debugLevelCatalog.size())
        return StageDeveloperShellCommand(
            RECOVERED_DEBUG_MENU_SWITCH_LEVEL, selected);
      ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER);
      return true;
    case RECOVERED_SHELL_PAGE_VIDEO_CONFIRM:
      g_inGameShellState.pendingVideoCommand =
          selected == 0u ? RECOVERED_SHELL_VIDEO_CONFIRM
                         : RECOVERED_SHELL_VIDEO_REVERT;
      return true;
    default:
      return false;
  }
}

bool HandleInGameShellKey(std::uint32_t key) {
  if (!g_inGameShellState.open) return key == VK_ESCAPE && OpenInGameShell();
  g_inGameShellState.lastError.clear();
  if (g_inGameShellState.captureBinding >= 0) {
    if (key == VK_ESCAPE) {
      g_inGameShellState.captureBinding = -1;
      g_inGameShellState.conflictBinding = -1;
      g_inGameShellState.status = "Binding capture cancelled";
      return true;
    }
    return CaptureShellBinding(key);
  }
  if (key == VK_ESCAPE) {
    if (g_inGameShellState.page == RECOVERED_SHELL_PAGE_ROOT ||
        g_inGameShellState.page == RECOVERED_SHELL_PAGE_VIDEO_CONFIRM)
      CloseInGameShell();
    else if (g_inGameShellState.page ==
                 RECOVERED_SHELL_PAGE_DEVELOPER_SPAWN ||
             g_inGameShellState.page ==
                 RECOVERED_SHELL_PAGE_DEVELOPER_ENTER ||
             g_inGameShellState.page ==
                 RECOVERED_SHELL_PAGE_DEVELOPER_LEVEL)
      ShellSelectPage(RECOVERED_SHELL_PAGE_DEVELOPER);
    else
      ShellSelectPage(RECOVERED_SHELL_PAGE_ROOT);
    return true;
  }
  const std::size_t count = ShellPageItemCount();
  if (key == VK_UP) {
    g_inGameShellState.selected =
        g_inGameShellState.selected == 0u
            ? count - 1u
            : g_inGameShellState.selected - 1u;
    g_inGameShellState.overwriteConfirmation = false;
    return true;
  }
  if (key == VK_DOWN) {
    g_inGameShellState.selected =
        (g_inGameShellState.selected + 1u) % count;
    g_inGameShellState.overwriteConfirmation = false;
    return true;
  }
  if (g_inGameShellState.page == RECOVERED_SHELL_PAGE_VIDEO &&
      (key == VK_LEFT || key == VK_RIGHT)) {
    const int delta = key == VK_RIGHT ? 1 : -1;
    if (g_inGameShellState.selected == 0u) {
      const int modeCount = g_inGameShellState.displayModeCount == 0u ? 2 : 3;
      g_inGameShellState.windowMode =
          (g_inGameShellState.windowMode + delta + modeCount) % modeCount;
    }
    if (g_inGameShellState.selected == 1u) {
      if (g_inGameShellState.windowMode == 2 &&
          g_inGameShellState.displayModeCount != 0u) {
        const std::size_t displayCount =
            g_inGameShellState.displayModeCount;
        g_inGameShellState.exclusiveModeIndex =
            (g_inGameShellState.exclusiveModeIndex + displayCount +
             (delta > 0 ? 1u : displayCount - 1u)) % displayCount;
      } else {
        g_inGameShellState.windowScale =
            1 + (g_inGameShellState.windowScale - 1 + delta + 3) % 3;
      }
    }
    return true;
  }
  if (g_inGameShellState.page == RECOVERED_SHELL_PAGE_CONTROLS &&
      (key == VK_LEFT || key == VK_RIGHT)) {
    const int direction = key == VK_LEFT ? -1 : 1;
    if (g_inGameShellState.selected == RECOVERED_BIND_COUNT)
      return AdjustShellMouseSensitivity(
          &g_inGameShellState.mouseSensitivityX, direction);
    if (g_inGameShellState.selected == RECOVERED_BIND_COUNT + 1u)
      return AdjustShellMouseSensitivity(
          &g_inGameShellState.mouseSensitivityY, direction);
    if (g_inGameShellState.selected == RECOVERED_BIND_COUNT + 2u)
      return ToggleShellMouseInvertY();
  }
  if (g_inGameShellState.page == RECOVERED_SHELL_PAGE_AUDIO &&
      (key == VK_LEFT || key == VK_RIGHT)) {
    if (g_inGameShellState.selected == 0u)
      return AdjustShellEffectsVolume(key == VK_LEFT ? -1 : 1);
    if (g_inGameShellState.selected == 1u)
      return AdjustShellVehicleVolume(key == VK_LEFT ? -1 : 1);
    if (g_inGameShellState.selected == 2u)
      return AdjustShellCinematicVolume(key == VK_LEFT ? -1 : 1);
  }
  return key == VK_RETURN ? ActivateShellSelection() : true;
}

bool HandleInGameShellMessage(UINT message, WPARAM wParam,
                              LPARAM lParam, LRESULT* result) {
  if (!g_inGameShellState.configured) return false;
  const bool keyboard = message == WM_KEYDOWN || message == WM_KEYUP ||
                        message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
  const bool character = message == WM_CHAR || message == WM_DEADCHAR ||
                         message == WM_SYSCHAR || message == WM_SYSDEADCHAR;
  const bool mouse = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
                     message == WM_RBUTTONDOWN || message == WM_RBUTTONUP;
  if (!g_inGameShellState.open) {
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) &&
        wParam == VK_ESCAPE && (lParam & 0x40000000) == 0) {
      HandleInGameShellKey(VK_ESCAPE);
      *result = 0;
      return true;
    }
    return false;
  }
  if (!keyboard && !character && !mouse) return false;
  if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) &&
      (lParam & 0x40000000) == 0)
    HandleInGameShellKey(static_cast<std::uint32_t>(wParam));
  if (g_inGameShellState.captureBinding >= 0 &&
      (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN))
    CaptureShellBinding(message == WM_LBUTTONDOWN ? VK_LBUTTON
                                                  : VK_RBUTTON);
  *result = 0;
  return true;
}

void ShellPrint(int x, int y, const std::string& text,
                bool selected = false) {
  if (g_gameConsoleFont == nullptr) return;
  const unsigned long color = selected ? GRFillColor(255, 220, 80)
                                       : GRFillColor(230, 230, 230);
  std::string bounded = text.substr(0, 72);
  g_gameConsoleFont->PrintColorAt(
      x - _gr_nScreenOriginX, y - _gr_nScreenOriginY,
      bounded.c_str(), color);
}

std::string ShellSlotLabel(std::uint32_t slot) {
  std::string label = "Slot " + std::to_string(slot + 1u) + " - ";
  if (!g_inGameShellSaveCatalog.ready ||
      slot >= g_inGameShellSaveCatalog.entry.size())
    return label + "Scanning...";
  const SRecoveredSaveSlotCatalogEntry& entry =
      g_inGameShellSaveCatalog.entry[slot];
  switch (entry.state) {
    case RECOVERED_SAVE_SLOT_CATALOG_EMPTY:
      return label + "Empty";
    case RECOVERED_SAVE_SLOT_CATALOG_CORRUPT:
      return label + "Corrupt / unsupported";
    case RECOVERED_SAVE_SLOT_CATALOG_INCOMPATIBLE:
      return label + "Incompatible - " + entry.summary.level;
    case RECOVERED_SAVE_SLOT_CATALOG_READY:
      label += entry.summary.title.empty() ? entry.summary.level
                                           : entry.summary.title;
      if (entry.switchesLevel) label += " [" + entry.summary.level + "]";
      break;
    default:
      return label + "Unknown";
  }
  if (label.size() > 34u) label = label.substr(0u, 31u) + "...";
  return label;
}

std::string ShellSavedAt(std::uint64_t seconds) {
  if (seconds == 0u ||
      seconds > static_cast<std::uint64_t>(
                    (std::numeric_limits<std::time_t>::max)()))
    return "Unknown";
  const std::time_t value = static_cast<std::time_t>(seconds);
  std::tm utc = {};
  char timestamp[32] = {};
  if (gmtime_s(&utc, &value) != 0 ||
      std::strftime(timestamp, sizeof(timestamp),
                    "%Y-%m-%d %H:%M UTC", &utc) == 0)
    return "Unknown";
  return timestamp;
}

void DrawShellSaveSlotDetail(std::uint32_t slot) {
  constexpr int kPreviewX = 380;
  constexpr int kPreviewY = 92;
  constexpr int kPreviewRight =
      kPreviewX + static_cast<int>(kShellSavePreviewWidth) - 1;
  constexpr int kPreviewBottom =
      kPreviewY + static_cast<int>(kShellSavePreviewHeight) - 1;
  const unsigned long panel = GRFillColor(8, 12, 18);
  const unsigned long border = GRFillColor(105, 125, 145);
  GREnable2D();
  GRBar(kPreviewX - 5, kPreviewY - 5, kPreviewRight + 5,
        kPreviewBottom + 78, panel);
  GRRect(kPreviewX - 1, kPreviewY - 1, kPreviewRight + 1,
         kPreviewBottom + 1, border);
  GRDisable2D();

  if (!g_inGameShellSaveCatalog.ready ||
      slot >= g_inGameShellSaveCatalog.entry.size()) {
    ShellPrint(kPreviewX + 42, kPreviewY + 58, "Scanning slots...");
    return;
  }
  const SRecoveredSaveSlotCatalogEntry& entry =
      g_inGameShellSaveCatalog.entry[slot];
  bool drewPreview = false;
  if (entry.previewReady && _gr_pScreen != nullptr &&
      _gr_nScreenWidth >= kPreviewRight + 1 &&
      _gr_nScreenHeight >= kPreviewBottom + 1 &&
      entry.previewWidth == kShellSavePreviewWidth &&
      entry.previewHeight == kShellSavePreviewHeight &&
      entry.previewIndices.size() ==
          static_cast<std::size_t>(kShellSavePreviewWidth) *
              kShellSavePreviewHeight) {
    const std::uint64_t paletteFingerprint = ShellPaletteFingerprint();
    if (paletteFingerprint ==
        g_inGameShellSaveCatalog.paletteFingerprint) {
      for (std::uint32_t y = 0; y < kShellSavePreviewHeight; ++y) {
        std::memcpy(
            _gr_pScreen +
                static_cast<std::size_t>(kPreviewY + y) *
                    static_cast<std::size_t>(_gr_nScreenWidth) +
                kPreviewX,
            entry.previewIndices.data() +
                static_cast<std::size_t>(y) * kShellSavePreviewWidth,
            kShellSavePreviewWidth);
      }
      drewPreview = true;
      ++g_inGameShellState.saveCatalogPreviewDrawFrames;
    } else {
      RequestShellSaveCatalogRefresh();
    }
  }
  if (!drewPreview) {
    std::string placeholder;
    if (entry.state == RECOVERED_SAVE_SLOT_CATALOG_EMPTY)
      placeholder = "EMPTY SLOT";
    else if (entry.state == RECOVERED_SAVE_SLOT_CATALOG_CORRUPT)
      placeholder = "CORRUPT / UNSUPPORTED";
    else if (entry.state == RECOVERED_SAVE_SLOT_CATALOG_INCOMPATIBLE)
      placeholder = "INCOMPATIBLE CONTENT";
    else if (entry.previewDecodeFailed)
      placeholder = "PREVIEW CORRUPT";
    else if (entry.previewMissing)
      placeholder = "PREVIEW NOT STORED";
    else
      placeholder = "PREVIEW REFRESHING";
    ShellPrint(kPreviewX + 18, kPreviewY + 58, placeholder);
  }

  if (!entry.readable) {
    ShellPrint(kPreviewX, kPreviewBottom + 16, entry.detail);
    return;
  }
  ShellPrint(kPreviewX, kPreviewBottom + 16,
             "Level: " + entry.summary.level);
  ShellPrint(kPreviewX, kPreviewBottom + 32,
             "Saved: " + ShellSavedAt(entry.summary.savedAtUnixSeconds));
  std::ostringstream time;
  time << std::fixed << std::setprecision(1) << entry.summary.simulationTime;
  ShellPrint(kPreviewX, kPreviewBottom + 48,
             "World: " + time.str() + "s  tick " +
                 std::to_string(entry.summary.simulationTick));
  ShellPrint(kPreviewX, kPreviewBottom + 64,
             "Status: " + entry.detail);
}

void DrawInGameShell() {
  if (!g_inGameShellState.open || g_gameConsoleFont == nullptr) return;
  PollShellSaveCatalog();
  const unsigned long background = GRFillColor(18, 24, 32);
  const unsigned long border = GRFillColor(150, 165, 180);
  GREnable2D();
  GRBar(52, 32, 587, 447, background);
  GRRect(52, 32, 587, 447, border);
  GRDisable2D();
  ShellPrint(72, 50, "RR2NW - IN-GAME MENU");

  std::vector<std::string> lines;
  switch (g_inGameShellState.page) {
    case RECOVERED_SHELL_PAGE_ROOT:
      lines = {"Continue", "Save game", "Load game",
               "Restart current Level", "Controls", "Video", "Audio"};
      if (g_inGameShellState.developerMode && g_debugMenuState.configured)
        lines.push_back("Developer");
      lines.push_back("Exit game");
      break;
    case RECOVERED_SHELL_PAGE_SAVE:
    case RECOVERED_SHELL_PAGE_LOAD:
      for (std::uint32_t slot = 0; slot < LevelSaveSlot_Count(); ++slot)
        lines.push_back(ShellSlotLabel(slot));
      lines.push_back("Back");
      if (g_inGameShellState.selected < LevelSaveSlot_Count())
        DrawShellSaveSlotDetail(
            static_cast<std::uint32_t>(g_inGameShellState.selected));
      break;
    case RECOVERED_SHELL_PAGE_CONTROLS:
      for (std::size_t index = 0; index < RECOVERED_BIND_COUNT; ++index) {
        std::string line = RecoveredWindowsInput_BindingName(index);
        line += " : ";
        line += RecoveredWindowsInput_KeyName(g_inGameShellBindings.key[index]);
        if (g_inGameShellState.captureBinding == static_cast<int>(index))
          line += "  <press new input>";
        if (g_inGameShellState.conflictBinding == static_cast<int>(index))
          line += "  <conflict>";
        lines.push_back(line);
      }
      {
        std::ostringstream x;
        x << std::fixed << std::setprecision(2)
          << "Mouse sensitivity X : "
          << g_inGameShellState.mouseSensitivityX;
        lines.push_back(x.str());
        std::ostringstream y;
        y << std::fixed << std::setprecision(2)
          << "Mouse sensitivity Y : "
          << g_inGameShellState.mouseSensitivityY;
        lines.push_back(y.str());
      }
      lines.push_back(std::string("Invert mouse Y : ") +
                      (g_inGameShellState.mouseInvertY ? "Yes" : "No"));
      lines.push_back("Restore defaults");
      lines.push_back("Back");
      break;
    case RECOVERED_SHELL_PAGE_VIDEO:
      lines.push_back(std::string("Window mode : ") +
                      (g_inGameShellState.windowMode == 0
                           ? "Windowed"
                           : g_inGameShellState.windowMode == 1
                                 ? "Borderless fullscreen"
                                 : "Exclusive fullscreen"));
      if (g_inGameShellState.windowMode == 2) {
        SRecoveredDisplayMode mode;
        if (RecoveredSoftwareGraph_DisplayMode(
                g_inGameShellState.exclusiveModeIndex, &mode)) {
          lines.push_back("Display mode : " + std::to_string(mode.width) +
                          "x" + std::to_string(mode.height) + " " +
                          std::to_string(mode.bitsPerPixel) + "bpp @ " +
                          std::to_string(mode.displayFrequency) + "Hz (" +
                          std::to_string(g_inGameShellState.exclusiveModeIndex +
                                         1u) +
                          "/" +
                          std::to_string(g_inGameShellState.displayModeCount) +
                          ")");
        } else {
          lines.push_back("Display mode : unavailable");
        }
      } else {
        lines.push_back("Window size : " +
                        std::to_string(640 * g_inGameShellState.windowScale) +
                        "x" +
                        std::to_string(480 * g_inGameShellState.windowScale) +
                        " (4:3 internal)");
      }
      lines.push_back("Apply (15 second safety confirmation)");
      lines.push_back("Back");
      break;
    case RECOVERED_SHELL_PAGE_AUDIO: {
      std::ostringstream effects;
      effects << std::fixed << std::setprecision(0)
              << "Gameplay effects volume : "
              << g_inGameShellState.effectsVolume * 100.0 << "%";
      lines.push_back(effects.str());
      std::ostringstream vehicle;
      vehicle << std::fixed << std::setprecision(0)
              << "Player vehicle volume : "
              << g_inGameShellState.vehicleVolume * 100.0 << "%";
      lines.push_back(vehicle.str());
      std::ostringstream cinematic;
      cinematic << std::fixed << std::setprecision(0)
                << "Briefing/cinematic volume : "
                << g_inGameShellState.cinematicVolume * 100.0 << "%";
      lines.push_back(cinematic.str());
      lines.push_back("Back");
      ShellPrint(72, 78,
                 "Effects, player engine and admitted briefing streams");
      break;
    }
    case RECOVERED_SHELL_PAGE_DEVELOPER:
      RefreshInGameDeveloperCatalog();
      for (ERecoveredDebugMenuAction action : {
               RECOVERED_DEBUG_MENU_SHOW_STATE,
               RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE,
               RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE,
               RECOVERED_DEBUG_MENU_KILL_PLAYER,
               RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH,
               RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE,
               RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION}) {
        const SRecoveredDeveloperCatalogEntry* entry =
            DeveloperCatalogEntry(action, 0u);
        if (entry != nullptr) {
          lines.push_back(entry->label +
                          (entry->available
                               ? "  [ready]"
                               : "  [blocked: " + entry->reason + "]"));
        }
      }
      lines.push_back("Spawn vehicle nearby  >");
      lines.push_back("Spawn and enter vehicle  >");
      lines.push_back("Switch Level (fresh)  >");
      lines.push_back("Back");
      break;
    case RECOVERED_SHELL_PAGE_DEVELOPER_SPAWN:
    case RECOVERED_SHELL_PAGE_DEVELOPER_ENTER: {
      const ERecoveredDebugMenuAction action =
          g_inGameShellState.page == RECOVERED_SHELL_PAGE_DEVELOPER_SPAWN
              ? RECOVERED_DEBUG_MENU_SPAWN_VEHICLE
              : RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE;
      for (std::size_t index = 0; index < g_debugVehicleCatalog.size(); ++index) {
        const SRecoveredDeveloperCatalogEntry* entry =
            DeveloperCatalogEntry(action, index);
        if (entry != nullptr)
          lines.push_back(entry->label +
                          (entry->available
                               ? "  [ready]"
                               : "  [blocked: " + entry->reason + "]"));
      }
      lines.push_back("Back");
      break;
    }
    case RECOVERED_SHELL_PAGE_DEVELOPER_LEVEL:
      for (std::size_t index = 0; index < g_debugLevelCatalog.size(); ++index) {
        const SRecoveredDeveloperCatalogEntry* entry = DeveloperCatalogEntry(
            RECOVERED_DEBUG_MENU_SWITCH_LEVEL, index);
        if (entry != nullptr) {
          std::string label = entry->label;
          if (LevelIdentityMatches(entry->label, ContinuationLevelIdentity()))
            label += "  [current]";
          label += entry->available
                       ? "  [ready]"
                       : "  [blocked: " + entry->reason + "]";
          lines.push_back(label);
        }
      }
      lines.push_back("Back");
      break;
    case RECOVERED_SHELL_PAGE_VIDEO_CONFIRM: {
      const ULONGLONG now = GetTickCount64();
      const ULONGLONG remaining =
          now >= g_inGameShellVideoDeadline
              ? 0
              : g_inGameShellVideoDeadline - now;
      lines.push_back("Keep this video mode");
      lines.push_back("Revert now");
      ShellPrint(72, 78, "Automatic revert in " +
                           std::to_string((remaining + 999u) / 1000u) +
                           " seconds");
      break;
    }
    default:
      break;
  }

  std::size_t first = 0;
  const std::size_t visible = 17u;
  if (g_inGameShellState.selected >= visible)
    first = g_inGameShellState.selected - visible + 1u;
  int y = 92;
  for (std::size_t index = first;
       index < lines.size() && index < first + visible; ++index, y += 18) {
    ShellPrint(78, y, std::string(index == g_inGameShellState.selected
                                      ? "> " : "  ") + lines[index],
               index == g_inGameShellState.selected);
  }
  if (!g_inGameShellState.lastError.empty())
    ShellPrint(72, 410, "ERROR: " + g_inGameShellState.lastError);
  else if (!g_inGameShellState.status.empty())
    ShellPrint(72, 410, g_inGameShellState.status);
  ShellPrint(72, 428, "Arrows: select   Enter: accept   Esc: back");
}

bool ProcessPendingInGameShellVideoCommand() {
  if (!g_inGameShellState.configured) return true;
  if (g_inGameShellState.videoConfirmationActive &&
      g_inGameShellState.pendingVideoCommand == RECOVERED_SHELL_VIDEO_NONE &&
      GetTickCount64() >= g_inGameShellVideoDeadline) {
    g_inGameShellState.pendingVideoCommand = RECOVERED_SHELL_VIDEO_REVERT;
    ++g_inGameShellState.videoTimeoutRollbacks;
  }
  const ERecoveredInGameVideoCommand command =
      g_inGameShellState.pendingVideoCommand;
  if (command == RECOVERED_SHELL_VIDEO_NONE) return true;
  g_inGameShellState.pendingVideoCommand = RECOVERED_SHELL_VIDEO_NONE;

  if (command == RECOVERED_SHELL_VIDEO_APPLY) {
    const SRecoveredWindowPresentation requested = ShellPresentation(
        g_inGameShellState.windowMode, g_inGameShellState.windowScale,
        g_inGameShellState.exclusiveModeIndex);
    SRecoveredWindowPresentation previous;
    if (!RecoveredSoftwareGraph_ApplyPresentation(requested, &previous)) {
      g_inGameShellState.lastError =
          "video mode application failed (win32=" +
          std::to_string(RecoveredSoftwareGraph_LastPresentationError()) +
          ")";
      Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
      return false;
    }
    g_inGameShellRollbackPresentation = previous;
    g_inGameShellRollbackExclusiveModeIndex =
        g_inGameShellPersistedExclusiveModeIndex;
    g_inGameShellAppliedPresentation = requested;
    g_inGameShellState.videoConfirmationActive = true;
    g_inGameShellVideoDeadline =
        GetTickCount64() + kVideoConfirmationMilliseconds;
    ShellSelectPage(RECOVERED_SHELL_PAGE_VIDEO_CONFIRM);
    ++g_inGameShellState.videoApplies;
    g_inGameShellState.status = "Confirm the new mode or it will be reverted";
    return true;
  }
  if (command == RECOVERED_SHELL_VIDEO_CONFIRM) {
    if (!g_inGameShellState.videoConfirmationActive) return false;
    g_inGameShellState.videoConfirmationActive = false;
    g_inGameShellVideoDeadline = 0;
    g_inGameShellPersistedWindowMode = g_inGameShellState.windowMode;
    g_inGameShellPersistedWindowScale = g_inGameShellState.windowScale;
    g_inGameShellPersistedExclusiveModeIndex =
        g_inGameShellState.exclusiveModeIndex;
    ++g_inGameShellState.videoConfirms;
    PersistShellSettings("Video mode confirmed and saved");
    ShellSelectPage(RECOVERED_SHELL_PAGE_VIDEO);
    return true;
  }
  if (command == RECOVERED_SHELL_VIDEO_REVERT) {
    if (!g_inGameShellState.videoConfirmationActive) return false;
    if (!RecoveredSoftwareGraph_ApplyPresentation(
            g_inGameShellRollbackPresentation, nullptr)) {
      g_inGameShellState.lastError = "video rollback failed";
      Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
      return false;
    }
    g_inGameShellAppliedPresentation = g_inGameShellRollbackPresentation;
    g_inGameShellState.windowMode = g_inGameShellPersistedWindowMode;
    g_inGameShellState.windowScale = g_inGameShellPersistedWindowScale;
    g_inGameShellState.exclusiveModeIndex =
        g_inGameShellRollbackExclusiveModeIndex;
    g_inGameShellState.videoConfirmationActive = false;
    g_inGameShellVideoDeadline = 0;
    ++g_inGameShellState.videoRollbacks;
    g_inGameShellState.status = "Previous video mode restored";
    ShellSelectPage(RECOVERED_SHELL_PAGE_VIDEO);
    return true;
  }
  return false;
}

void PresentClosedFrameCommandFailure(
    const char* owner, const std::string& detail,
    void (*nativePresenter)()) {
  if (g_nativeDiagnosticMenuEnabled) {
    nativePresenter();
    return;
  }
  if (!g_inGameShellState.configured) return;
  if (!g_inGameShellState.open && !OpenInGameShell())
    Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
  g_inGameShellState.lastError =
      detail.empty() ? std::string(owner) + " command failed" : detail;
  g_inGameShellState.status =
      std::string(owner) + " failed at the closed frame boundary";
  ++g_inGameShellState.commandFailurePresentations;
}

void ShowNativeMissionCheckpointFailure() {
  if (_gr_hWnd == nullptr) return;
  const std::wstring detail = Utf8ToWide(
      RecruitCenterSubjectState_LastError());
  MessageBoxW(_gr_hWnd, detail.c_str(), L"RR2NW mission checkpoint error",
              MB_OK | MB_ICONERROR);
}

void ShowNativeScriptedLevelTransitionFailure() {
  if (_gr_hWnd == nullptr) return;
  const std::wstring detail = Utf8ToWide(
      g_scriptedLevelTransitionState.lastError);
  MessageBoxW(_gr_hWnd, detail.c_str(),
              L"RR2NW scripted Level transition error",
              MB_OK | MB_ICONERROR);
}

LRESULT ForwardWindowMessageToHardware(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
  if (message == WM_ACTIVATEAPP)
    SoundState_SetApplicationActive(wParam != FALSE);
  LRESULT shellResult = 0;
  if (HandleInGameShellMessage(message, wParam, lParam, &shellResult))
    return shellResult;
  LRESULT saveMenuResult = 0;
  if (HandleNativeSaveMenuMessage(window, message, wParam,
                                  &saveMenuResult))
    return saveMenuResult;
  if (!g_hardwareReady || g_hardware.getContext() == nullptr) {
    return DefWindowProcA(window, message, wParam, lParam);
  }
  SRecoveredWindowsInputBatch inputBatch = {};
  g_windowsInputAdapter.SetMapOverlayActive(g_debugMap.IsActive() != 0);
  if (!g_windowsInputAdapter.ProcessWindowMessage(
          message, static_cast<std::uintptr_t>(wParam),
          static_cast<std::intptr_t>(lParam),
          g_levelAttr.get_double("keySens"), &inputBatch) ||
      !DispatchWindowsInputBatch(inputBatch)) {
    Report(RECOVERED_GAME_SERVICES_VEHICLE_CONTROL_FAILURE);
  }
  // The adapter is the sole production owner of keyboard and mouse-button
  // state. Do not let these messages enter CtrlSet::Translate as a second,
  // polling-based state machine. Mouse motion, joystick, paint and capture
  // transitions remain delegated to the compatibility Hardware object.
  if (inputBatch.consumed)
    return 0;
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
  g_vehicleDriveTelemetry.stabilityRecoveries = static_cast<unsigned int>(
      (std::max)(state.stabilityRecoveryCount, 0));
  g_vehicleDriveTelemetry.lastStabilityReason = state.lastStabilityReason;
  SRecoveredVehicleStabilityTelemetry recovery = {};
  if (VehicleRuntimeState_InspectStability(
          g_super.m_context, &recovery)) {
    g_vehicleDriveTelemetry.recoveryVesselKind = recovery.vesselKind;
    g_vehicleDriveTelemetry.recoveryBumpFlags = recovery.bumpFlags;
    g_vehicleDriveTelemetry.recoveryTouchingGround =
        recovery.touchingGround;
    g_vehicleDriveTelemetry.recoveryFrameStartTime =
        recovery.frameStartTime;
    g_vehicleDriveTelemetry.recoveryRejectedTime = recovery.rejectedTime;
    g_vehicleDriveTelemetry.recoveryTargetTime =
        recovery.requestedTargetTime;
    g_vehicleDriveTelemetry.recoveryFrameStartPositionX =
        recovery.frameStartPosition.x;
    g_vehicleDriveTelemetry.recoveryFrameStartPositionY =
        recovery.frameStartPosition.y;
    g_vehicleDriveTelemetry.recoveryFrameStartPositionZ =
        recovery.frameStartPosition.z;
    g_vehicleDriveTelemetry.recoveryFrameStartSpeedX =
        recovery.frameStartSpeed.x;
    g_vehicleDriveTelemetry.recoveryFrameStartSpeedY =
        recovery.frameStartSpeed.y;
    g_vehicleDriveTelemetry.recoveryFrameStartSpeedZ =
        recovery.frameStartSpeed.z;
    g_vehicleDriveTelemetry.recoveryRejectedPositionX =
        recovery.rejectedPosition.x;
    g_vehicleDriveTelemetry.recoveryRejectedPositionY =
        recovery.rejectedPosition.y;
    g_vehicleDriveTelemetry.recoveryRejectedPositionZ =
        recovery.rejectedPosition.z;
    g_vehicleDriveTelemetry.recoveryRejectedSpeedX =
        recovery.rejectedSpeed.x;
    g_vehicleDriveTelemetry.recoveryRejectedSpeedY =
        recovery.rejectedSpeed.y;
    g_vehicleDriveTelemetry.recoveryRejectedSpeedZ =
        recovery.rejectedSpeed.z;
    g_vehicleDriveTelemetry.recoveryGroundX = recovery.groundX;
    g_vehicleDriveTelemetry.recoveryGroundY = recovery.groundY;
    g_vehicleDriveTelemetry.recoveryGroundZ = recovery.groundZ;
    g_vehicleDriveTelemetry.recoveryGroundLength = recovery.groundLength;
    g_vehicleDriveTelemetry.recoveryForwardTangentLength =
        recovery.forwardTangentLength;
    g_vehicleDriveTelemetry.recoveryRightTangentLength =
        recovery.rightTangentLength;
    g_vehicleDriveTelemetry.recoveryTangentDot = recovery.tangentDot;
    g_vehicleDriveTelemetry.recoverySuspensionTravel =
        recovery.suspensionTravel;
    g_vehicleDriveTelemetry.recoveryAccelerationFactor =
        recovery.accelerationFactor;
    g_vehicleDriveTelemetry.recoveryThrottle = recovery.throttle;
  }
}

unsigned int NonNegativeDifference(int current, int baseline) {
  return static_cast<unsigned int>((std::max)(current - baseline, 0));
}

unsigned int CounterDifference(unsigned int current,
                               unsigned int baseline) {
  return current < baseline ? 0u : current - baseline;
}

bool BeginPrimaryFireTelemetry(SimulationContext* context) {
  if (!BulletSubjectState_OwnerRuntimeTelemetry(
          context, "Vehicle.Default", &g_primaryFireOwnerBaseline))
    return false;
  g_primaryFireTelemetry = {};
  g_primaryFireTriggerBaseline =
      g_vehicleControlInput.PrimaryFirePresses();
  g_primaryFireExplosionBaseline = ExplosionSubjectState_LiveCount();
  g_primaryFireParticleBaseline =
      ExplosionSubjectState_ParticleBranchLiveCount();
  g_primaryFireSmokeBaseline = SmokeSubjectState_LiveCount();
  g_primaryFireSparkBaseline = SparkSubjectState_LiveCount();
  g_primaryFireSoundBaseline = SoundObjectState_LiveCount();
  g_primaryFireTelemetryReady = true;
  g_primaryFireEffectPresent = false;
  g_windowsInputAdapter.Reset(true);
  g_mapTogglePresses = 0;
  g_missionMapProbe = {};
  g_debugMapControlProbe = {};
  g_missionMapBaselineMissions = 0;
  g_missionMapBaselineTexts = 0;
  g_missionMapBaselineRoutes = 0;
  g_missionMapBaselineDrawFrames = 0;
  g_missionMapProbeLive = false;
  g_pendingWindowsInput.clear();
  return true;
}

void UpdatePrimaryFireTelemetry(SimulationContext* context) {
  if (!g_primaryFireTelemetryReady || context == nullptr)
    return;
  BulletRuntimeTelemetry current = {};
  if (!BulletSubjectState_OwnerRuntimeTelemetry(
          context, "Vehicle.Default", &current))
    return;
  g_primaryFireTelemetry.triggerPresses =
      CounterDifference(g_vehicleControlInput.PrimaryFirePresses(),
                        g_primaryFireTriggerBaseline);
  g_primaryFireTelemetry.acceptedShots = CounterDifference(
      current.acceptedStarts, g_primaryFireOwnerBaseline.acceptedStarts);
  g_primaryFireTelemetry.rolledBackShots = CounterDifference(
      current.rolledBackStarts,
      g_primaryFireOwnerBaseline.rolledBackStarts);
  g_primaryFireTelemetry.moveEvents = CounterDifference(
      current.moveEvents, g_primaryFireOwnerBaseline.moveEvents);
  g_primaryFireTelemetry.collisionChecks = CounterDifference(
      current.collisionChecks,
      g_primaryFireOwnerBaseline.collisionChecks);
  g_primaryFireTelemetry.sceneImpacts = CounterDifference(
      current.sceneImpacts, g_primaryFireOwnerBaseline.sceneImpacts);
  g_primaryFireTelemetry.dynamicImpacts = CounterDifference(
      current.dynamicImpacts, g_primaryFireOwnerBaseline.dynamicImpacts);
  g_primaryFireTelemetry.waterlineSplashes = CounterDifference(
      current.waterlineSplashes,
      g_primaryFireOwnerBaseline.waterlineSplashes);
  g_primaryFireTelemetry.impactEffectChildren = CounterDifference(
      current.impactEffectChildren,
      g_primaryFireOwnerBaseline.impactEffectChildren);
  g_primaryFireTelemetry.groundRemovals = CounterDifference(
      current.groundRemovals,
      g_primaryFireOwnerBaseline.groundRemovals);
  g_primaryFireTelemetry.barrelSmokeStarts = CounterDifference(
      current.barrelSmokeStarts,
      g_primaryFireOwnerBaseline.barrelSmokeStarts);
  g_primaryFireTelemetry.bulletRenderSubmissions = CounterDifference(
      current.renderSubmissions,
      g_primaryFireOwnerBaseline.renderSubmissions);
  g_primaryFireTelemetry.particleRenderSubmissions = CounterDifference(
      current.particleRenderSubmissions,
      g_primaryFireOwnerBaseline.particleRenderSubmissions);
  g_primaryFireTelemetry.skinRenderSubmissions = CounterDifference(
      current.skinRenderSubmissions,
      g_primaryFireOwnerBaseline.skinRenderSubmissions);
  g_primaryFireTelemetry.skippedSkinRenderSubmissions = CounterDifference(
      current.skippedSkinRenderSubmissions,
      g_primaryFireOwnerBaseline.skippedSkinRenderSubmissions);
  g_primaryFireTelemetry.liveBullets = current.liveBullets;
  g_primaryFireTelemetry.tablePeakLiveBullets =
      current.peakLiveBullets;
  const unsigned int explosionSubjects = NonNegativeDifference(
      ExplosionSubjectState_LiveCount(), g_primaryFireExplosionBaseline);
  const unsigned int particleBranches = NonNegativeDifference(
      ExplosionSubjectState_ParticleBranchLiveCount(),
      g_primaryFireParticleBaseline);
  const unsigned int smokeSubjects = NonNegativeDifference(
      SmokeSubjectState_LiveCount(), g_primaryFireSmokeBaseline);
  const unsigned int sparkSubjects = NonNegativeDifference(
      SparkSubjectState_LiveCount(), g_primaryFireSparkBaseline);
  const unsigned int soundObjects = NonNegativeDifference(
      SoundObjectState_LiveCount(), g_primaryFireSoundBaseline);
  g_primaryFireTelemetry.maximumExplosionSubjects = (std::max)(
      g_primaryFireTelemetry.maximumExplosionSubjects,
      explosionSubjects);
  g_primaryFireTelemetry.maximumParticleBranches = (std::max)(
      g_primaryFireTelemetry.maximumParticleBranches,
      particleBranches);
  g_primaryFireTelemetry.maximumSmokeSubjects = (std::max)(
      g_primaryFireTelemetry.maximumSmokeSubjects,
      smokeSubjects);
  g_primaryFireTelemetry.maximumSparkSubjects = (std::max)(
      g_primaryFireTelemetry.maximumSparkSubjects,
      sparkSubjects);
  g_primaryFireTelemetry.maximumSoundObjects = (std::max)(
      g_primaryFireTelemetry.maximumSoundObjects,
      soundObjects);
  g_primaryFireEffectPresent = current.liveBullets != 0 ||
      explosionSubjects != 0 || particleBranches != 0 ||
      smokeSubjects != 0 || sparkSubjects != 0;
  g_primaryFireTelemetry.hardwareSubscriptionPreserved =
      g_vehicleControlInput.IsSubscribed() ? 1 : 0;
}

void RecordPrimaryFireRenderFrame() {
  if (!g_primaryFireTelemetryReady ||
      g_primaryFireTelemetry.acceptedShots == 0)
    return;
  ++g_primaryFireTelemetry.renderedFramesAfterShot;
  if (g_primaryFireEffectPresent)
    ++g_primaryFireTelemetry.effectRenderFrames;
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
    KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
    Vehicle* vehicle = vehicleID.isNUL() ? nullptr : static_cast<Vehicle*>(
        context->queryInterface(vehicleID, IVehicleIID));
    if (vehicle != nullptr) vehicle->closePanel(Session::m_moment);
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
  Vehicle::preserveExternalControlSubscription(false);
  g_vehicleControlReady = false;
  g_primaryFireTelemetryReady = false;
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
  Vehicle::preserveExternalControlSubscription(false);

  const double timerTime = g_timer.GetTime();
  const double startTime =
      !std::isfinite(timerTime) || timerTime < 0.1 ? 0.1 : timerTime;
  if (!VehicleRuntimeState_Activate(
          context, vehicle, position, startTime)) {
    return false;
  }

  g_vehicleControlInput.Reset(vehicle);
  const bool controlAdded = !context->addObject(
      "RecoveredVehicleControl", &g_vehicleControlInput).isNUL();
  const bool journalReady = controlAdded &&
      g_vehicleControlInput.BeginControlJournal();
  if (!controlAdded || !journalReady ||
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
  if (!BeginPrimaryFireTelemetry(context)) {
    StopVehicleControl(true, &position);
    return false;
  }

  Vehicle::preserveExternalControlSubscription(true);
  Vehicle* controlledVehicle = static_cast<Vehicle*>(
      context->queryInterface(vehicle, IVehicleIID));
  if (controlledVehicle == nullptr) {
    StopVehicleControl(true, &position);
    return false;
  }
  controlledVehicle->openPanel(startTime);
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
    CFVector3 stablePosition;
    if (VehicleRuntimeState_LastStablePosition(
            g_super.m_context, &stablePosition)) {
      position = stablePosition;
    } else if (!vehicle.isNUL() && VehicleRuntimeState_Inspect(
                   g_super.m_context, vehicle, &state) && state.active &&
               std::isfinite(state.position.x) &&
               std::isfinite(state.position.y) &&
               std::isfinite(state.position.z)) {
      // This path is only for failures before the first completed frame.
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
  ResetSaveMenuSession();
  RecoveredSoftwareGraph_ConfigureWindowMessageHook(nullptr);
  SUA_BindSession(nullptr);

  if (g_super.m_context != nullptr) {
    StopVehicleControl(false, nullptr);
    g_debugMap.DeInit();
    // Arena owns all script-created class-table objects. Release that graph
    // while its context and the legacy services it may notify still exist.
    RecoveredArenaSeance_Release();
    // Briefing and console subscribe to Hardware. Detach them while Hardware
    // is still alive, matching the inverse of the retail session order.
    RemoveAttachedObject(g_super.m_context, &g_briefing);
    RemoveAttachedObject(g_super.m_context, &g_GameConsole);
    RemoveAttachedObject(g_super.m_context, g_gameConsoleFont);
    delete g_gameConsoleFont;
    g_gameConsoleFont = nullptr;
    RemoveAttachedObject(g_super.m_context, g_debugMapMissionFont);
    delete g_debugMapMissionFont;
    g_debugMapMissionFont = nullptr;
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
  g_vehicleDeathCameraReady = false;
  g_taxiVehicleTransitionReady = false;
  g_vehicleControlReplayReady = false;
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
  g_vehicleDeathCameraProbe = {};
  g_taxiVehicleTransitionProbe = {};
  g_vehicleControlReplayProbe = {};
  g_vehicleDriveTelemetry = {};
  g_vehicleTelemetryStartPosition = CFVector3(0.0, 0.0, 0.0);
  g_vehicleTelemetryStartForward = CFVector3(0.0, 0.0, 1.0);
  g_vehicleDriveTelemetryReady = false;
  g_primaryFireTelemetryReady = false;
  g_primaryFireOwnerBaseline = {};
  g_primaryFireTelemetry = {};
  g_primaryFireTriggerBaseline = 0;
  g_primaryFireExplosionBaseline = 0;
  g_primaryFireParticleBaseline = 0;
  g_primaryFireSmokeBaseline = 0;
  g_primaryFireSparkBaseline = 0;
  g_primaryFireSoundBaseline = 0;
  g_primaryFireEffectPresent = false;
  g_levelContinuationFailure.clear();
  g_levelSaveSlotFailure.clear();
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
    g_hardware.m_ctrlUse.mouse = TRUE;
    g_hardware.m_ctrlUse.joystick = FALSE;
    g_windowsInputAdapter.Reset(true);
    if (g_inGameShellState.configured) {
      if (_gr_hWnd != nullptr) {
        if (!RecoveredSoftwareGraph_RefreshDisplayModes()) {
          EndBoundedSession();
          Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
          return;
        }
        g_inGameShellState.displayModeCount =
            RecoveredSoftwareGraph_DisplayModeCount();
        std::size_t selected = 0u;
        if (RecoveredSoftwareGraph_FindDisplayMode(
                g_inGameShellRequestedExclusiveWidth,
                g_inGameShellRequestedExclusiveHeight,
                g_inGameShellRequestedExclusiveBits,
                g_inGameShellRequestedExclusiveFrequency, &selected)) {
          g_inGameShellState.exclusiveModeIndex = selected;
        } else {
          g_inGameShellState.exclusiveModeIndex = 0u;
          if (g_inGameShellState.windowMode == 2) {
            g_inGameShellState.windowMode = 0;
            g_inGameShellPersistedWindowMode = 0;
            g_inGameShellState.status =
                "Unavailable exclusive mode recovered to windowed";
          }
        }
        g_inGameShellPersistedExclusiveModeIndex =
            g_inGameShellState.exclusiveModeIndex;
        g_inGameShellRollbackExclusiveModeIndex =
            g_inGameShellState.exclusiveModeIndex;
        g_inGameShellAppliedPresentation = ShellPresentation(
            g_inGameShellState.windowMode, g_inGameShellState.windowScale,
            g_inGameShellState.exclusiveModeIndex);
        g_inGameShellRollbackPresentation =
            g_inGameShellAppliedPresentation;
        const SRecoveredWindowsPresentationState presentationState =
            RecoveredSoftwareGraph_WindowsPresentationState();
        g_inGameShellState.displayCatalogRefreshes =
            presentationState.catalogRefreshes;
        g_inGameShellState.staleDisplayRecoveries =
            presentationState.staleModeRecovered ? 1u : 0u;
      }
      if (!g_windowsInputAdapter.SetBindings(g_inGameShellBindings) ||
          (_gr_hWnd != nullptr &&
           !RecoveredSoftwareGraph_ApplyPresentation(
               g_inGameShellAppliedPresentation, nullptr))) {
        EndBoundedSession();
        Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
        return;
      }
    }
    g_pendingWindowsInput.clear();
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
    ApplyShellMouseSettingsToRuntime();

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
    if (g_super.m_context->addObject("DebugMap", &g_debugMap).isNUL() ||
        !g_debugMap.Init("level04s.bmp")) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_DEBUG_MAP_INITIALIZATION_FAILURE);
      return;
    }
    // green_menu.sci published this exact object from the data-root font.
    // Keep the original symbolic identity and relative path so mission text
    // uses the retail FixedFont contract instead of a replacement renderer.
    if (GetFileAttributesA("..\\fnt16x16.fnt") != INVALID_FILE_ATTRIBUTES) {
      g_debugMapMissionFont =
          new (std::nothrow) FixedFontOBJ("..\\fnt16x16.fnt");
      // FixedFontOBJ's legacy constructor discards Read()'s status and Height()
      // is not safe after a short/corrupt header.  An empty software print
      // validates the loaded buffers and active framebuffer without writing.
      if (g_debugMapMissionFont == nullptr ||
          g_super.m_context
              ->addObject("Font.fnt16x16.fnt", g_debugMapMissionFont)
              .isNUL() ||
          g_debugMapMissionFont->PrintAt(0, 0, "") != 1) {
        EndBoundedSession();
        Report(RECOVERED_GAME_SERVICES_DEBUG_MAP_INITIALIZATION_FAILURE);
        return;
      }
    }
    // green_menu.sci published the small console font separately. The
    // recovered session originally restored only the mission-map font, which
    // left GameConsole and Briefing absent from Context and caused every
    // deferred mission briefing to be skipped at its safety gate.
    if (GetFileAttributesA("..\\fig8x8.fnt") != INVALID_FILE_ATTRIBUTES) {
      g_gameConsoleFont =
          new (std::nothrow) FixedFontOBJ("..\\fig8x8.fnt");
      if (g_gameConsoleFont == nullptr ||
          g_super.m_context
              ->addObject("Font.fig8x8.fnt", g_gameConsoleFont)
              .isNUL() ||
          g_gameConsoleFont->PrintAt(0, 0, "") != 1) {
        EndBoundedSession();
        Report(RECOVERED_GAME_SERVICES_DEBUG_MAP_INITIALIZATION_FAILURE);
        return;
      }
    }
    if (g_debugMapMissionFont != nullptr && g_gameConsoleFont != nullptr) {
      if (g_super.m_context->addObject("GameConsole", &g_GameConsole).isNUL() ||
          g_super.m_context->addObject("Briefing", &g_briefing).isNUL()) {
        EndBoundedSession();
        Report(RECOVERED_GAME_SERVICES_SESSION_FAILURE);
        return;
      }
      g_GameConsole.Init("Font.fig8x8.fnt", "Font.fnt16x16.fnt",
                         GRTransparentColor(150, 150, 150), 128, 4, nullptr);
    }
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
    if (!VehicleRuntimeState_ProbeDeathCamera(
            g_super.m_context, vehicle, observerPosition,
            vehicleStartTime, &g_vehicleDeathCameraProbe) ||
        !VehicleRuntimeState_IsClean(g_super.m_context)) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_VEHICLE_DEATH_CAMERA_FAILURE);
      return;
    }
    g_vehicleDeathCameraReady = true;
    const bool replayProbeReady = VehicleControlReplayProbe_Run(
            g_super.m_context, vehicle, observerPosition,
            vehicleStartTime, ContinuationContentFingerprint(),
            &g_vehicleControlReplayProbe);
    if (!replayProbeReady) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_VEHICLE_CONTROL_REPLAY_FAILURE);
      return;
    }
    g_vehicleControlReplayReady = true;
    if (!TaxiSubjectState_ProbeVehicleTransition(
            g_super.m_context, vehicle, vehicleStartTime,
            &g_taxiVehicleTransitionProbe)) {
      EndBoundedSession();
      Report(RECOVERED_GAME_SERVICES_TAXI_VEHICLE_TRANSITION_FAILURE);
      return;
    }
    g_taxiVehicleTransitionReady = true;
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
    g_campaignRestartState.currentLevel = ContinuationLevelIdentity();
    if (g_debugMenuState.configured && !BuildDebugVehicleCatalog()) {
      Report(RECOVERED_GAME_SERVICES_DEBUG_MENU_FAILURE);
      EndBoundedSession();
      return;
    }
    if (g_saveMenuState.configured && _gr_hWnd != nullptr &&
        !InstallNativeSaveMenu()) {
      Report(RECOVERED_GAME_SERVICES_SAVE_MENU_FAILURE);
      EndBoundedSession();
      return;
    }
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
  g_frameTimingTelemetry = {};
  pScene->CheckDynamicMap();
  GRSetViewport(ppViewports[0]);
  g_loopReady = true;
}

void DrawDebugMap() {
  if (g_debugMap.IsActive() && !g_debugMap.DrawRecovered())
    Report(RECOVERED_GAME_SERVICES_DEBUG_MAP_RENDER_FAILURE);
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
      // A closing window can deliver WM_KILLFOCUS immediately before
      // WM_QUIT. There is no subsequent simulation boundary at which that
      // focus transition could be consumed, and the session is about to be
      // torn down, so do not report it as live pending input.
      g_pendingWindowsInput.clear();
      return false;
    }
    TranslateMessage(&message);
    DispatchMessageA(&message);
  }
  return true;
}

}  // namespace

bool RecoveredObserverAxes_ApplyLegacyAction(
    SRecoveredObserverAxes* axes, int action, double value) {
  if (axes == nullptr || !std::isfinite(value)) return false;

  switch (action) {
    case MOVE_FORWARD:
      axes->forward = value;
      return true;
    case MOVE_BACKWARD:
      axes->forward = -value;
      return true;
    case STRAFE_LEFT:
      axes->strafe = -value;
      return true;
    case STRAFE_RIGHT:
      axes->strafe = value;
      return true;
    case STRAFE_UP:
      axes->vertical = value;
      return true;
    case STRAFE_DOWN:
      axes->vertical = -value;
      return true;
    case TURN_LEFT:
      axes->turn = -value;
      return true;
    case TURN_RIGHT:
      axes->turn = value;
      return true;
    case LOOK_UP:
      axes->look = value;
      return true;
    case LOOK_DOWN:
      axes->look = -value;
      return true;
    default:
      return false;
  }
}

bool RecoveredObserverAxes_IsNeutral(
    const SRecoveredObserverAxes& axes) {
  return axes.forward == 0.0 && axes.strafe == 0.0 &&
         axes.vertical == 0.0 && axes.turn == 0.0 && axes.look == 0.0;
}

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

bool RecoveredGameServices_OrphanReferencesReady() {
  return RecoveredArenaSeance_OrphanReferencesReady();
}

unsigned long long RecoveredGameServices_OrphanReferenceFingerprint() {
  return RecoveredArenaSeance_OrphanReferenceFingerprint();
}

bool RecoveredGameServices_OrphanSubjectReady() {
  return RecoveredArenaSeance_OrphanSubjectReady();
}

int RecoveredGameServices_OrphanSubjectCapacity() {
  return RecoveredArenaSeance_OrphanSubjectCapacity();
}

int RecoveredGameServices_OrphanSubjectCount() {
  return RecoveredArenaSeance_OrphanSubjectCount();
}

unsigned long long RecoveredGameServices_OrphanSubjectFingerprint() {
  return RecoveredArenaSeance_OrphanSubjectFingerprint();
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

bool RecoveredGameServices_TaxiSubjectReady() {
  return RecoveredArenaSeance_TaxiSubjectReady();
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

bool RecoveredGameServices_PeopleAttributesReady() {
  return RecoveredArenaSeance_PeopleAttributesReady();
}

bool RecoveredGameServices_PeopleReferencesReady() {
  return RecoveredArenaSeance_PeopleReferencesReady();
}

bool RecoveredGameServices_PeopleSubjectReady() {
  return RecoveredArenaSeance_PeopleSubjectReady();
}

bool RecoveredGameServices_TankCannonAttributesReady() {
  return RecoveredArenaSeance_TankCannonAttributesReady();
}

bool RecoveredGameServices_TankReferencesReady() {
  return RecoveredArenaSeance_TankReferencesReady();
}

bool RecoveredGameServices_TankCannonSubjectTablesReady() {
  return RecoveredArenaSeance_TankCannonSubjectTablesReady();
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

bool RecoveredGameServices_VehicleDeathCameraReady() {
  return g_vehicleDeathCameraReady;
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

int RecoveredGameServices_VehicleProbeStabilityRecoveries() {
  return g_vehicleMovementReady
             ? g_vehicleMovementProbe.stabilityRecoveries
             : -1;
}

int RecoveredGameServices_VehicleProbeRollbacks() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.rollbacks : -1;
}

double RecoveredGameServices_VehicleProbeHorizontalDistance() {
  return g_vehicleMovementReady ? g_vehicleMovementProbe.horizontalDistance
                                : 0.0;
}

int RecoveredGameServices_VehicleDeathCameraProbeActivations() {
  return g_vehicleDeathCameraReady ? g_vehicleDeathCameraProbe.activations
                                   : -1;
}

int RecoveredGameServices_VehicleDeathCameraProbeAscentFrames() {
  return g_vehicleDeathCameraReady ? g_vehicleDeathCameraProbe.ascentFrames
                                   : -1;
}

int RecoveredGameServices_VehicleDeathCameraProbeTerminalFrames() {
  return g_vehicleDeathCameraReady ? g_vehicleDeathCameraProbe.terminalFrames
                                   : -1;
}

int RecoveredGameServices_VehicleDeathCameraProbeCompletionTransitions() {
  return g_vehicleDeathCameraReady
             ? g_vehicleDeathCameraProbe.completionTransitions
             : -1;
}

int RecoveredGameServices_VehicleDeathCameraProbeFiniteCameras() {
  return g_vehicleDeathCameraReady ? g_vehicleDeathCameraProbe.finiteCameras
                                   : -1;
}

int RecoveredGameServices_VehicleDeathCameraProbeRollbacks() {
  return g_vehicleDeathCameraReady ? g_vehicleDeathCameraProbe.rollbacks
                                   : -1;
}

bool RecoveredGameServices_TaxiVehicleTransitionReady() {
  return g_taxiVehicleTransitionReady;
}

int RecoveredGameServices_TaxiVehicleProbeAvailableTaxis() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.availableTaxis
             : -1;
}

int RecoveredGameServices_TaxiVehicleProbeInvalidTargets() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.invalidTargets
             : -1;
}

int RecoveredGameServices_TaxiVehicleProbeTransitions() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.transitions
             : -1;
}

int RecoveredGameServices_TaxiVehicleProbeAttributeTransfers() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.attributeTransfers
             : -1;
}

int RecoveredGameServices_TaxiVehicleProbePoseTransfers() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.poseTransfers
             : -1;
}

int RecoveredGameServices_TaxiVehicleProbePayloadTransfers() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.payloadTransfers
             : -1;
}

int RecoveredGameServices_TaxiVehicleProbeRemovedTaxis() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.removedTaxis
             : -1;
}

int RecoveredGameServices_TaxiVehicleProbeRollbacks() {
  return g_taxiVehicleTransitionReady
             ? g_taxiVehicleTransitionProbe.rollbacks
             : -1;
}

bool RecoveredGameServices_TaxiVehicleHandoffTelemetry(
    SRecoveredTaxiVehicleHandoffTelemetry* telemetry) {
  return g_vehicleControlReady &&
         g_vehicleControlInput.HandoffTelemetry(telemetry);
}

bool RecoveredGameServices_VehicleEmbodimentTelemetry(
    SRecoveredVehicleEmbodimentTelemetry* telemetry) {
  return g_vehicleControlReady &&
         g_vehicleControlInput.EmbodimentTelemetry(telemetry);
}

bool RecoveredGameServices_BeginVehiclePrimaryFireObservation() {
  return g_vehicleControlReady &&
         BeginPrimaryFireTelemetry(g_super.m_context);
}

bool RecoveredGameServices_VehiclePrimaryFireTelemetry(
    SRecoveredVehiclePrimaryFireTelemetry* telemetry) {
  if (!g_vehicleControlReady || !g_primaryFireTelemetryReady ||
      telemetry == nullptr)
    return false;
  UpdatePrimaryFireTelemetry(g_super.m_context);
  *telemetry = g_primaryFireTelemetry;
  return true;
}

bool RecoveredGameServices_VehicleControlReady() {
  return g_vehicleControlReady;
}

bool RecoveredGameServices_VehicleControlReplayReady() {
  return g_vehicleControlReplayReady;
}

bool RecoveredGameServices_VehicleControlReplayTelemetry(
    SRecoveredVehicleControlReplayTelemetry* telemetry) {
  if (!g_vehicleControlReplayReady || telemetry == nullptr) return false;
  *telemetry = {};
  telemetry->journalFingerprint =
      g_vehicleControlReplayProbe.journalFingerprint;
  telemetry->replayFingerprint =
      g_vehicleControlReplayProbe.replayFingerprint;
  telemetry->hashStreamFingerprint =
      g_vehicleControlReplayProbe.hashStreamFingerprint;
  telemetry->contentFingerprint =
      g_vehicleControlReplayProbe.contentFingerprint;
  telemetry->recordedStateFingerprint =
      g_vehicleControlReplayProbe.recordedStateFingerprint;
  telemetry->replayedStateFingerprint =
      g_vehicleControlReplayProbe.replayedStateFingerprint;
  telemetry->encodedBytes = g_vehicleControlReplayProbe.encodedBytes;
  telemetry->replayEncodedBytes =
      g_vehicleControlReplayProbe.replayEncodedBytes;
  telemetry->recordings = g_vehicleControlReplayProbe.recordings;
  telemetry->replays = g_vehicleControlReplayProbe.replays;
  telemetry->codecRoundTrips = g_vehicleControlReplayProbe.codecRoundTrips;
  telemetry->actionRecords = g_vehicleControlReplayProbe.actionRecords;
  telemetry->focusRecords = g_vehicleControlReplayProbe.focusRecords;
  telemetry->syntheticReleases =
      g_vehicleControlReplayProbe.syntheticReleases;
  telemetry->simulationFrames =
      g_vehicleControlReplayProbe.simulationFrames;
  telemetry->stateMatches = g_vehicleControlReplayProbe.stateMatches;
  telemetry->clockMatches = g_vehicleControlReplayProbe.clockMatches;
  telemetry->randomMatches = g_vehicleControlReplayProbe.randomMatches;
  telemetry->rollbacks = g_vehicleControlReplayProbe.rollbacks;
  telemetry->hashMatches = g_vehicleControlReplayProbe.hashMatches;
  telemetry->hashSamples = g_vehicleControlReplayProbe.hashSamples;
  telemetry->densePresentationSamples =
      g_vehicleControlReplayProbe.densePresentationSamples;
  telemetry->sparsePresentationSamples =
      g_vehicleControlReplayProbe.sparsePresentationSamples;
  telemetry->denseSimulationTicks =
      g_vehicleControlReplayProbe.denseSimulationTicks;
  telemetry->sparseSimulationTicks =
      g_vehicleControlReplayProbe.sparseSimulationTicks;
  return true;
}

bool RecoveredGameServices_VehicleControlJournalTelemetry(
    SRecoveredVehicleControlJournalTelemetry* telemetry) {
  return g_vehicleControlReady &&
         g_vehicleControlInput.ControlJournalTelemetry(telemetry);
}

namespace {

bool RestoredGameplayAuthorityReady(
    const SLevelContinuationSummary& continuation,
    std::string* failure) {
  KR_ObjectID vehicle = g_super.m_context == nullptr
                            ? KR_ObjectID::NUL()
                            : g_super.m_context->searchObject(
                                  "Vehicle.Default");
  SRecoveredVehicleRuntimeState state = {};
  SRecoveredVehicleCameraTelemetry camera = {};
  SRecoveredVehicleControlJournalTelemetry journal = {};
  const bool vehicleReady = g_super.m_context != nullptr &&
      !vehicle.isNUL() &&
      VehicleRuntimeState_Inspect(
          g_super.m_context, vehicle, &state);
  const int resolvedExpectedCamera = vehicleReady
      ? (state.dead ? RECOVERED_VEHICLE_CAMERA_DEATH_ASCENT
                    : (state.takingTaxi
                           ? RECOVERED_VEHICLE_CAMERA_TAXI
                           : RECOVERED_VEHICLE_CAMERA_LIVE))
      : RECOVERED_VEHICLE_CAMERA_UNKNOWN;
  const int expectedPanelOpen =
      vehicleReady && state.panelReady && !state.dead ? 1 : 0;
  const bool ready = continuation.ready && continuation.worldMatches &&
      continuation.boundaryMatches &&
      continuation.worldFingerprint != 0 &&
      continuation.restoredWorldFingerprint ==
          continuation.worldFingerprint &&
      vehicleReady && state.active && !state.frameBegun &&
      state.panelOpen == expectedPanelOpen &&
      VehicleRuntimeState_InspectCamera(
          g_super.m_context, &camera) &&
      camera.mode == resolvedExpectedCamera &&
      g_vehicleControlInput.IsSubscribed() &&
      g_vehicleControlInput.ControlJournalTelemetry(&journal) &&
      journal.recording == 1 && journal.appendFailures == 0 &&
      journal.checkpointTick <= journal.lastRecordTick &&
      TaxiSubjectState_LiveCount() >= 0 &&
      OrphanSubjectState_LiveCount() >= 0;
  if (!ready && failure != nullptr) {
    std::ostringstream detail;
    detail << "restored gameplay authority is incomplete"
           << " (continuation=" << (continuation.ready ? 1 : 0)
           << ", world=" << (continuation.worldMatches ? 1 : 0)
           << ", boundary=" << (continuation.boundaryMatches ? 1 : 0)
           << ", vehicle=" << (vehicleReady ? 1 : 0)
           << ", active=" << (vehicleReady ? state.active : 0)
           << ", frame=" << (vehicleReady ? state.frameBegun : 0)
           << ", panel=" << (vehicleReady ? state.panelOpen : -1)
           << "/" << expectedPanelOpen
           << ", camera=" << camera.mode << "/"
           << resolvedExpectedCamera
           << ", controls="
           << (g_vehicleControlInput.IsSubscribed() ? 1 : 0)
           << ", journal=" << journal.recording
           << ", append_failures=" << journal.appendFailures << ")";
    *failure = detail.str();
  }
  return ready;
}

}  // namespace

bool RecoveredGameServices_CaptureLevelContinuation(
    std::vector<std::uint8_t>* bytes,
    SLevelContinuationSummary* summary) {
  g_levelContinuationFailure.clear();
  SVehicleControlJournal journal;
  if (!g_vehicleControlReady || g_super.m_context == nullptr ||
      !g_vehicleControlInput.CopyControlJournal(&journal)) {
    g_levelContinuationFailure =
        "live Vehicle control journal is unavailable";
    return false;
  }
  return LevelContinuation_Capture(
      g_super.m_context, ContinuationContentFingerprint(),
      ContinuationLevelIdentity(), journal, bytes, summary,
      &g_levelContinuationFailure);
}

bool RecoveredGameServices_RestoreLevelContinuation(
    const std::vector<std::uint8_t>& bytes,
    SLevelContinuationSummary* summary) {
  if (!g_vehicleControlReady || g_super.m_context == nullptr ||
      summary == nullptr) {
    g_levelContinuationFailure =
        "live Vehicle continuation target is unavailable";
    return false;
  }
  g_levelContinuationFailure.clear();
  const bool teleportRoutesReadyBefore =
      RecoveredArenaSeance_TeleportRoutesReady();
  const bool teleportTargetBefore =
      RecoveredArenaSeance_TeleportTargetLevel();
  const int teleportRouteCountBefore =
      RecoveredArenaSeance_TeleportRouteCount();
  const unsigned long long teleportFingerprintBefore =
      RecoveredArenaSeance_TeleportFingerprint();
  SLevelContinuation incoming;
  if (!LevelContinuation_Decode(bytes, &incoming) ||
      !g_vehicleControlInput.CanAdoptControlJournal(
          incoming.controlJournal)) {
    g_levelContinuationFailure =
        "LCN1 journal cannot bind to the live Vehicle controller";
    return false;
  }

  std::vector<std::uint8_t> backupBytes;
  SLevelContinuationSummary backupSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &backupBytes, &backupSummary)) {
    g_levelContinuationFailure =
        "restore preflight capture failed: " + g_levelContinuationFailure;
    return false;
  }

  SVehicleControlJournal restoredJournal;
  SLevelContinuationSummary restoredSummary;
  std::string failure;
  const std::uint64_t contentFingerprint =
      ContinuationContentFingerprint();
  const std::string level = ContinuationLevelIdentity();
  // Consume the failpoint on this restore attempt even if reconstruction
  // fails earlier. A failed target must never poison the coordinator's later
  // source rollback.
  const bool failRestoredAuthorityForTesting =
      g_failNextRestoredGameplayAuthorityForTesting;
  g_failNextRestoredGameplayAuthorityForTesting = false;
  const bool worldRestored = LevelContinuation_RestoreWorld(
      g_super.m_context, bytes, contentFingerprint, level,
      &restoredJournal, &restoredSummary, &failure);
  const bool controlsAdopted = worldRestored &&
      g_vehicleControlInput.AdoptControlJournal(restoredJournal);
  bool authorityReady = controlsAdopted &&
      RestoredGameplayAuthorityReady(restoredSummary, &failure);
  const bool teleportRoutesPreserved =
      RecoveredArenaSeance_TeleportRoutesReady() ==
          teleportRoutesReadyBefore &&
      RecoveredArenaSeance_TeleportTargetLevel() ==
          teleportTargetBefore &&
      RecoveredArenaSeance_TeleportRouteCount() ==
          teleportRouteCountBefore &&
      RecoveredArenaSeance_TeleportFingerprint() ==
          teleportFingerprintBefore;
  if (authorityReady && !teleportRoutesPreserved) {
    authorityReady = false;
    failure = "Level-local Teleport routes changed during LCN1 restore";
  }
  if (authorityReady && failRestoredAuthorityForTesting) {
    authorityReady = false;
    failure =
        "injected post-restore gameplay authority failure";
  }
  if (authorityReady) {
    *summary = restoredSummary;
    g_levelContinuationFailure.clear();
    return true;
  }
  if (failure.empty()) {
    g_levelContinuationFailure = worldRestored
        ? "restored CTJ1 adoption failed: " +
              g_vehicleControlInput.ControlJournalAdoptionFailure()
        : "restored LCN1 world reconstruction failed";
  } else {
    g_levelContinuationFailure = failure;
  }

  SVehicleControlJournal backupJournal;
  SLevelContinuationSummary rolledBackSummary;
  std::string rollbackFailure;
  const bool rolledBack = LevelContinuation_RestoreWorld(
      g_super.m_context, backupBytes, contentFingerprint, level,
      &backupJournal,
      &rolledBackSummary, &rollbackFailure);
  const bool controlRolledBack = rolledBack &&
      g_vehicleControlInput.AdoptControlJournal(backupJournal) &&
      RestoredGameplayAuthorityReady(
          rolledBackSummary, &rollbackFailure) &&
      RecoveredArenaSeance_TeleportRoutesReady() ==
          teleportRoutesReadyBefore &&
      RecoveredArenaSeance_TeleportTargetLevel() ==
          teleportTargetBefore &&
      RecoveredArenaSeance_TeleportRouteCount() ==
          teleportRouteCountBefore &&
      RecoveredArenaSeance_TeleportFingerprint() ==
          teleportFingerprintBefore;
  if (!controlRolledBack) {
    g_levelContinuationFailure += rolledBack
        ? "; backup CTJ1 adoption failed: " +
              g_vehicleControlInput.ControlJournalAdoptionFailure()
        : "; backup world restore failed: " + rollbackFailure;
  }
  return false;
}

bool RecoveredGameServices_ProbeMissionTaxiForwardTravel(
    const char* taxiObjectName,
    const std::vector<KR_ObjectID>& preMissionTaxis,
    SRecoveredMissionVehicleDriveProbe* summary) {
  if (summary == nullptr) return false;
  *summary = {};
  SimulationContext* context = g_super.m_context;
  if (!g_vehicleControlReady || context == nullptr ||
      taxiObjectName == nullptr || taxiObjectName[0] == '\0' ||
      g_vehicleControlInput.ActiveActionCount() != 0u) {
    return false;
  }

  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  Vehicle* vehicle = vehicleID.isNUL()
      ? nullptr
      : static_cast<Vehicle*>(
            context->queryInterface(vehicleID, IVehicleIID));
  const auto collectMissionTaxis = [&]() {
    std::vector<KR_ObjectID> allTaxis;
    std::vector<KR_ObjectID> missionTaxis;
    if (!TaxiSubjectState_ObjectIDs(context, &allTaxis))
      return missionTaxis;
    for (KR_ObjectID candidate : allTaxis) {
      const char* candidateName = context->searchObject(candidate);
      const bool existedBefore = std::find(
          preMissionTaxis.begin(), preMissionTaxis.end(), candidate) !=
          preMissionTaxis.end();
      if (!existedBefore && candidateName != nullptr &&
          std::strcmp(candidateName, taxiObjectName) == 0) {
        missionTaxis.push_back(candidate);
      }
    }
    return missionTaxis;
  };
  const std::vector<KR_ObjectID> initialMissionTaxis =
      collectMissionTaxis();
  if (vehicle == nullptr || initialMissionTaxis.empty()) return false;
  const int taxiCount = static_cast<int>(initialMissionTaxis.size());
  summary->availableTaxis = static_cast<unsigned int>(taxiCount);

  std::vector<std::uint8_t> checkpoint;
  SLevelContinuationSummary captured;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &checkpoint, &captured) || !captured.ready) {
    return false;
  }

  summary->minimumHorizontalDistance =
      std::numeric_limits<double>::infinity();
  summary->minimumForwardTravel =
      std::numeric_limits<double>::infinity();
  constexpr unsigned int kMovementFrames = 80u;
  for (int ordinal = 0; ordinal < taxiCount; ++ordinal) {
    const std::vector<KR_ObjectID> missionTaxis = collectMissionTaxis();
    KR_ObjectID taxiID = ordinal < static_cast<int>(missionTaxis.size())
        ? missionTaxis[static_cast<std::size_t>(ordinal)]
        : KR_ObjectID::NUL();
    SRecoveredVehicleRuntimeState before = {};
    SRecoveredVehicleRuntimeState entered = {};
    SRecoveredVehicleRuntimeState driven = {};
    std::string transitionFailure;
    bool movementReady = !taxiID.isNUL() && context->isExist(taxiID) &&
        VehicleRuntimeState_Inspect(context, vehicleID, &before);
    const double transitionTime = movementReady
        ? (std::max)(0.1, before.lastTime)
        : 0.1;
    movementReady = movementReady &&
        TaxiSubjectState_DebugTakeVehicle(
            context, vehicleID, taxiID, transitionTime,
            &transitionFailure) &&
        VehicleRuntimeState_RebaseRestoredOwner(context) &&
        VehicleRuntimeState_Inspect(context, vehicleID, &entered) &&
        entered.attribute != before.attribute && !entered.dead &&
        !entered.takingTaxi && !vehicle->taxiChangeEnabled() &&
        !context->isExist(taxiID);
    if (movementReady) {
      ++summary->transitionedTaxis;
      if (entered.panelReady) ++summary->panelReadyTaxis;
      if (entered.panelOpen) ++summary->panelOpenTaxis;
      movementReady = entered.panelReady == entered.panelOpen;
    }

    const CFVector3 back = entered.direction.Row(2);
    const double basisLength = std::sqrt(
        back.x * back.x + back.z * back.z);
    double forwardX = 0.0;
    double forwardZ = 0.0;
    if (movementReady && std::isfinite(basisLength) &&
        basisLength > 1.0e-12) {
      forwardX = -back.x / basisLength;
      forwardZ = -back.z / basisLength;
    } else {
      movementReady = false;
    }

    double currentTime = entered.lastTime;
    bool throttleApplied = false;
    if (movementReady) {
      throttleApplied = VehicleRuntimeState_ApplyControlAt(
          context, MOVE_FORWARD, 1.0, currentTime);
      movementReady = throttleApplied;
    }
    for (unsigned int frame = 0u;
         movementReady && frame < kMovementFrames; ++frame) {
      currentTime += 0.025;
      movementReady = VehicleRuntimeState_Advance(context, currentTime);
      if (movementReady) ++summary->movementFrames;
    }
    if (throttleApplied) {
      const bool released = VehicleRuntimeState_ApplyControlAt(
          context, MOVE_FORWARD, 0.0, currentTime);
      movementReady = movementReady && released;
    }
    movementReady = movementReady &&
        VehicleRuntimeState_Inspect(context, vehicleID, &driven);
    if (movementReady) {
      const double dx = driven.position.x - entered.position.x;
      const double dz = driven.position.z - entered.position.z;
      const double distance = std::sqrt(dx * dx + dz * dz);
      const double forwardTravel = dx * forwardX + dz * forwardZ;
      const double lateralTravel =
          std::fabs(dx * forwardZ - dz * forwardX);
      const double lateralRatio = lateralTravel /
          (std::max)(std::fabs(forwardTravel), 1.0e-12);
      summary->minimumHorizontalDistance = (std::min)(
          summary->minimumHorizontalDistance, distance);
      summary->minimumForwardTravel = (std::min)(
          summary->minimumForwardTravel, forwardTravel);
      summary->maximumLateralTravel = (std::max)(
          summary->maximumLateralTravel, lateralTravel);
      summary->maximumLateralRatio = (std::max)(
          summary->maximumLateralRatio, lateralRatio);
      if (std::isfinite(distance) && std::isfinite(forwardTravel) &&
          std::isfinite(lateralTravel) && forwardTravel > 0.01 &&
          lateralTravel <= (std::max)(0.05, forwardTravel * 0.5)) {
        ++summary->alignedTaxis;
      }
    }

    SLevelContinuationSummary restored;
    if (RecoveredGameServices_RestoreLevelContinuation(
            checkpoint, &restored)) {
      ++summary->rollbackRestores;
    } else {
      return false;
    }
    std::vector<std::uint8_t> verifiedBytes;
    SLevelContinuationSummary verified;
    if (RecoveredGameServices_CaptureLevelContinuation(
            &verifiedBytes, &verified) && verified.ready &&
        verifiedBytes == checkpoint &&
        verified.worldFingerprint == captured.worldFingerprint &&
        verified.containerFingerprint == captured.containerFingerprint) {
      ++summary->exactRollbacks;
    } else {
      return false;
    }
  }

  return summary->transitionedTaxis == summary->availableTaxis &&
         summary->panelReadyTaxis == summary->panelOpenTaxis &&
         summary->alignedTaxis == summary->availableTaxis &&
         summary->rollbackRestores == summary->availableTaxis &&
         summary->exactRollbacks == summary->availableTaxis &&
         summary->movementFrames ==
             summary->availableTaxis * kMovementFrames;
}

void RecoveredGameServices_FailNextRestoredGameplayAuthorityForTesting() {
  g_failNextRestoredGameplayAuthorityForTesting = true;
}

const char* RecoveredGameServices_LastLevelContinuationError() {
  return g_levelContinuationFailure.c_str();
}

bool RecoveredGameServices_SaveLevelSlot(
    const std::wstring& directory, std::uint32_t slot,
    const std::string& title, const std::string& description,
    const std::vector<std::uint8_t>& previewPng,
    SLevelSaveSlotSummary* slotSummary,
    SLevelContinuationSummary* continuationSummary) {
  g_levelSaveSlotFailure.clear();
  if (directory.empty() || slotSummary == nullptr ||
      continuationSummary == nullptr) {
    g_levelSaveSlotFailure = "save slot arguments are invalid";
    return false;
  }
  *slotSummary = {};
  *continuationSummary = {};

  std::vector<std::uint8_t> continuationBytes;
  SLevelContinuationSummary captured;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &continuationBytes, &captured)) {
    g_levelSaveSlotFailure =
        RecoveredGameServices_LastLevelContinuationError();
    return false;
  }
  const std::time_t now = std::time(nullptr);
  if (now <= 0) {
    g_levelSaveSlotFailure = "current UTC save time is unavailable";
    return false;
  }

  SLevelSaveSlotStatus status;
  SLevelSaveSlot archive;
  if (!LevelSaveSlot_Create(
          slot, static_cast<std::uint64_t>(now), title, description,
          previewPng, continuationBytes, &archive, &status) ||
      !LevelSaveSlot_WriteAtomic(directory, archive, &status)) {
    g_levelSaveSlotFailure = status.detail;
    return false;
  }

  // Read the committed name back through the same bounded decoder. A success
  // result therefore means the directory contains one complete, loadable
  // RR2SLOT1 file rather than merely a flushed temporary file.
  SLevelSaveSlot committed;
  SLevelSaveSlotSummary committedSummary;
  if (!LevelSaveSlot_Read(directory, slot, &committed, &status) ||
      committed.archiveFingerprint != archive.archiveFingerprint ||
      !LevelSaveSlot_Summarize(committed, &committedSummary, &status)) {
    g_levelSaveSlotFailure =
        status.detail.empty()
            ? "committed save slot read-back fingerprint differs"
            : status.detail;
    return false;
  }
  *slotSummary = committedSummary;
  *continuationSummary = captured;
  return true;
}

bool RecoveredGameServices_LoadLevelSlot(
    const std::wstring& directory, std::uint32_t slot,
    SLevelSaveSlotSummary* slotSummary,
    SLevelContinuationSummary* continuationSummary) {
  g_levelSaveSlotFailure.clear();
  if (directory.empty() || slotSummary == nullptr ||
      continuationSummary == nullptr) {
    g_levelSaveSlotFailure = "load slot arguments are invalid";
    return false;
  }
  *slotSummary = {};
  *continuationSummary = {};

  SLevelSaveSlotStatus status;
  SLevelSaveSlot archive;
  SLevelSaveSlotSummary decodedSummary;
  if (!LevelSaveSlot_Read(directory, slot, &archive, &status) ||
      !LevelSaveSlot_Summarize(archive, &decodedSummary, &status)) {
    g_levelSaveSlotFailure = status.detail;
    return false;
  }
  SLevelContinuationSummary restored;
  if (!RecoveredGameServices_RestoreLevelContinuation(
          archive.continuation, &restored)) {
    g_levelSaveSlotFailure =
        RecoveredGameServices_LastLevelContinuationError();
    return false;
  }
  // RR2SLOT1 validation already bound these fields to the same decoded LCN1,
  // and RestoreLevelContinuation admits that exact byte vector. Therefore
  // there is no fallible post-mutation phase here: success publishes both
  // summaries, while every possible failure occurred before commit or inside
  // the continuation's own rollback transaction.
  *slotSummary = decodedSummary;
  *continuationSummary = restored;
  return true;
}

const char* RecoveredGameServices_LastLevelSaveSlotError() {
  return g_levelSaveSlotFailure.c_str();
}

namespace {

bool RequestSaveSlotInternal(std::uint32_t slot, bool allowOverwrite,
                             const std::string& title,
                             const std::string& description) {
  g_saveMenuState.lastError.clear();
  if (!g_saveMenuState.configured) {
    g_saveMenuState.lastError = "save directory is not configured";
    return false;
  }
  if (slot >= LevelSaveSlot_Count()) {
    g_saveMenuState.lastError = "save slot index is outside 0..7";
    return false;
  }
  SLevelSaveSlotStatus metadataStatus;
  if (!LevelSaveSlot_ValidateDisplayMetadata(
          title, description, &metadataStatus)) {
    g_saveMenuState.lastError = metadataStatus.detail;
    return false;
  }
  if (g_saveMenuState.pending || g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready ||
      g_scriptedLevelTransitionRequest.ready) {
    g_saveMenuState.lastError =
        "another save/load command is already pending";
    return false;
  }
  const std::wstring path =
      LevelSaveSlot_Path(g_saveMenuState.directory, slot);
  if (!allowOverwrite &&
      GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
    g_saveMenuState.lastError =
        "save slot already exists and overwrite was not confirmed";
    return false;
  }
  g_saveMenuState.pending = true;
  g_saveMenuState.pendingAction = RECOVERED_SAVE_MENU_SAVE;
  g_saveMenuState.pendingSlot = slot;
  g_saveMenuState.pendingAttempts = 0;
  g_saveMenuState.lastCommandAttempts = 0;
  g_saveMenuState.pendingTitle = title;
  g_saveMenuState.pendingDescription = description;
  g_saveMenuState.lastRequestedTitle = title;
  g_saveMenuState.lastRequestedDescription = description;
  g_saveMenuAllowOverwrite = allowOverwrite;
  ++g_saveMenuState.saveRequests;
  return true;
}

}  // namespace

bool RecoveredGameServices_ConfigureSaveDirectory(
    const std::wstring& directory) {
  if (directory.empty() ||
      directory.find(L'\0') != std::wstring::npos ||
      LevelSaveSlot_Path(directory, 0u).empty()) {
    g_saveMenuState.lastError = "save directory is invalid";
    return false;
  }
  if (g_saveMenuState.pending || g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready ||
      g_scriptedLevelTransitionRequest.ready) {
    g_saveMenuState.lastError =
        "save directory cannot change while a command is pending";
    return false;
  }
  StopShellSaveCatalog();
  ClearShellSaveCatalog();
  g_crossLevelLoadRequest = {};
  g_saveMenuState.crossLevelRestartPending = false;
  g_saveMenuState.crossLevelRequests = 0;
  g_saveMenuState.completedCrossLevelLoads = 0;
  g_saveMenuState.crossLevelRollbacks = 0;
  g_saveMenuState.crossLevelRollbackFailures = 0;
  g_saveMenuState.slotDetailViews = 0;
  g_saveMenuState.previewViews = 0;
  g_saveMenuState.previewDecodeFailures = 0;
  g_saveMenuState.customMetadataSaveRequests = 0;
  g_saveMenuState.crossLevelSourceLevel.clear();
  g_saveMenuState.crossLevelTargetLevel.clear();
  g_saveMenuState.directory = directory;
  g_saveMenuState.configured = true;
  g_saveMenuState.lastError.clear();
  if (g_sessionReady && _gr_hWnd != nullptr &&
      !InstallNativeSaveMenu()) {
    Report(RECOVERED_GAME_SERVICES_SAVE_MENU_FAILURE);
    return false;
  }
  RefreshNativeSaveMenu();
  RequestShellSaveCatalogRefresh();
  return true;
}

bool RecoveredGameServices_ConfigureNativeDiagnosticMenu(bool enabled) {
  if (g_saveMenuState.pending || g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready ||
      g_scriptedLevelTransitionRequest.ready) {
    g_saveMenuState.lastError =
        "native diagnostic menu capability cannot change while a command "
        "is pending";
    return false;
  }
  const bool previous = g_nativeDiagnosticMenuEnabled;
  if (!enabled) DestroyNativeSaveMenu();
  g_nativeDiagnosticMenuEnabled = enabled;
  if (enabled && g_sessionReady && _gr_hWnd != nullptr &&
      g_saveMenuState.configured && !InstallNativeSaveMenu()) {
    g_nativeDiagnosticMenuEnabled = previous;
    if (!previous) DestroyNativeSaveMenu();
    Report(RECOVERED_GAME_SERVICES_SAVE_MENU_FAILURE);
    return false;
  }
  return true;
}

bool RecoveredGameServices_NativeDiagnosticMenuEnabled() {
  return g_nativeDiagnosticMenuEnabled;
}

bool RecoveredGameServices_ConfigureDebugMenu(
    bool enabled, const std::vector<std::string>& levelCatalog) {
  if (g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready ||
      g_scriptedLevelTransitionRequest.ready) {
    g_debugMenuState.lastError =
        "debug menu cannot be reconfigured while a command is pending";
    return false;
  }
  if (enabled && (levelCatalog.empty() ||
                  levelCatalog.size() > kMaximumNativeDebugLevels)) {
    g_debugMenuState.lastError =
        "debug Level catalog is empty or exceeds the native menu limit";
    return false;
  }
  if (enabled) {
    for (std::size_t index = 0; index < levelCatalog.size(); ++index) {
      if (levelCatalog[index].empty()) {
        g_debugMenuState.lastError =
            "debug Level catalog contains an empty identity";
        return false;
      }
      for (std::size_t previous = 0; previous < index; ++previous) {
        if (LevelIdentityMatches(levelCatalog[index],
                                 levelCatalog[previous])) {
          g_debugMenuState.lastError =
              "debug Level catalog contains duplicate identities";
          return false;
        }
      }
    }
  }

  DestroyNativeSaveMenu();
  g_debugMenuState = {};
  g_debugMenuState.configured = enabled;
  g_debugLevelCatalog = enabled ? levelCatalog
                                : std::vector<std::string>();
  g_debugVehicleCatalog.clear();
  g_debugTaxiSettlements.clear();
  g_debugLevelSwitchRequest = {};
  g_debugPreDeathCheckpoint.clear();
  g_debugPreDeathCheckpointSummary = {};
  g_debugPreDeathCorpseCount = -1;
  g_debugPreVehicleDestructionCheckpoint.clear();
  g_debugPreVehicleDestructionCheckpointSummary = {};
  g_debugPreVehicleDestructionOrphanCount = -1;
  g_debugPreVehicleDestructionPanelReady = false;
  g_debugPreVehicleDestructionPanelOpen = false;
  if (enabled && g_sessionReady && !BuildDebugVehicleCatalog()) {
    Report(RECOVERED_GAME_SERVICES_DEBUG_MENU_FAILURE);
    return false;
  }
  if (g_sessionReady && _gr_hWnd != nullptr &&
      g_saveMenuState.configured && !InstallNativeSaveMenu()) {
    Report(RECOVERED_GAME_SERVICES_DEBUG_MENU_FAILURE);
    return false;
  }
  RefreshInGameDeveloperCatalog();
  return true;
}

bool RecoveredGameServices_ConfigureInGameShell(
    const std::wstring& settingsPath, bool developerMode, bool safeMode) {
  if (settingsPath.empty() || settingsPath.find(L'\0') != std::wstring::npos)
    return false;
  if (g_inGameShellState.open ||
      g_inGameShellState.pendingVideoCommand != RECOVERED_SHELL_VIDEO_NONE)
    return false;

  StopShellSaveCatalog();
  ClearShellSaveCatalog();
  g_inGameShellState = {};
  g_inGameShellDeveloperCatalog = {};
  g_inGameShellDeveloperCatalogNextGeneration = 1u;
  g_inGameShellState.configured = true;
  g_inGameShellState.developerMode = developerMode;
  g_inGameShellState.safeMode = safeMode;
  g_inGameShellState.settingsPath = settingsPath;
  g_inGameShellState.windowMode = 0;
  g_inGameShellState.windowScale = 1;
  g_inGameShellState.exclusiveModeIndex = 0u;
  g_inGameShellState.mouseSensitivityX = kDefaultMouseSensitivity;
  g_inGameShellState.mouseSensitivityY = kDefaultMouseSensitivity;
  g_inGameShellState.mouseInvertY = false;
  g_inGameShellState.effectsVolume = kDefaultEffectsVolume;
  g_inGameShellState.vehicleVolume = kDefaultVehicleVolume;
  g_inGameShellState.cinematicVolume = kDefaultCinematicVolume;
  g_inGameShellBindings = RecoveredWindowsInput_DefaultBindings();

  const std::wstring displayRecoveryPath =
      settingsPath + L".display-recovery";
  if (!RecoveredSoftwareGraph_ConfigureDisplayRecovery(displayRecoveryPath)) {
    g_inGameShellState.lastError =
        "display recovery/catalog configuration failed";
    Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
    return false;
  }
  g_inGameShellState.displayModeCount =
      RecoveredSoftwareGraph_DisplayModeCount();
  const SRecoveredWindowsPresentationState presentationState =
      RecoveredSoftwareGraph_WindowsPresentationState();
  g_inGameShellState.displayCatalogRefreshes =
      presentationState.catalogRefreshes;
  g_inGameShellState.staleDisplayRecoveries =
      presentationState.staleModeRecovered ? 1u : 0u;

  const bool exists =
      GetFileAttributesW(settingsPath.c_str()) != INVALID_FILE_ATTRIBUTES;
  bool rewriteSettings = false;
  int exclusiveWidth = 640;
  int exclusiveHeight = 480;
  int exclusiveBits = 32;
  int exclusiveFrequency = 60;
  if (!safeMode && exists) {
    bool migrated = false;
    if (LoadInGameShellSettings(settingsPath, &g_inGameShellBindings,
                                 &g_inGameShellState.windowMode,
                                 &g_inGameShellState.windowScale,
                                 &exclusiveWidth, &exclusiveHeight,
                                 &exclusiveBits, &exclusiveFrequency,
                                 &g_inGameShellState.mouseSensitivityX,
                                 &g_inGameShellState.mouseSensitivityY,
                                 &g_inGameShellState.mouseInvertY,
                                 &g_inGameShellState.effectsVolume,
                                 &g_inGameShellState.vehicleVolume,
                                 &g_inGameShellState.cinematicVolume,
                                 &migrated)) {
      ++g_inGameShellState.settingsLoads;
      if (migrated) {
        ++g_inGameShellState.settingsMigrations;
        rewriteSettings = true;
        g_inGameShellState.status =
            "Legacy settings migrated with safe display defaults";
      } else {
        g_inGameShellState.status = "Settings loaded";
      }
    } else {
      ++g_inGameShellState.corruptSettingsRecoveries;
      g_inGameShellState.status =
          "Invalid settings ignored; safe windowed defaults restored";
      rewriteSettings = true;
    }
  } else if (safeMode) {
    g_inGameShellState.status = "Safe mode: settings bypassed";
  }
  if (g_inGameShellState.displayModeCount != 0u) {
    std::size_t selected = 0u;
    if (RecoveredSoftwareGraph_FindDisplayMode(
            exclusiveWidth, exclusiveHeight, exclusiveBits,
            exclusiveFrequency, &selected)) {
      g_inGameShellState.exclusiveModeIndex = selected;
    } else {
      g_inGameShellState.exclusiveModeIndex = 0u;
      rewriteSettings = rewriteSettings || exists;
      if (g_inGameShellState.windowMode == 2) {
        g_inGameShellState.windowMode = 0;
        g_inGameShellState.status =
            "Unavailable exclusive mode recovered to windowed";
      }
    }
  } else if (_gr_hWnd != nullptr && g_inGameShellState.windowMode == 2) {
    g_inGameShellState.windowMode = 0;
    rewriteSettings = true;
    g_inGameShellState.status =
        "Exclusive display unavailable; windowed mode restored";
  }
  g_inGameShellRequestedExclusiveWidth = exclusiveWidth;
  g_inGameShellRequestedExclusiveHeight = exclusiveHeight;
  g_inGameShellRequestedExclusiveBits = exclusiveBits;
  g_inGameShellRequestedExclusiveFrequency = exclusiveFrequency;
  if (!g_windowsInputAdapter.SetBindings(g_inGameShellBindings)) {
    g_inGameShellState.lastError = "input bindings could not be activated";
    Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
    return false;
  }
  ApplyShellMouseSettingsToRuntime();
  ApplyShellAudioSettingsToRuntime();
  g_inGameShellAppliedPresentation = ShellPresentation(
      g_inGameShellState.windowMode, g_inGameShellState.windowScale,
      g_inGameShellState.exclusiveModeIndex);
  g_inGameShellRollbackPresentation = g_inGameShellAppliedPresentation;
  g_inGameShellPersistedWindowMode = g_inGameShellState.windowMode;
  g_inGameShellPersistedWindowScale = g_inGameShellState.windowScale;
  g_inGameShellPersistedExclusiveModeIndex =
      g_inGameShellState.exclusiveModeIndex;
  g_inGameShellRollbackExclusiveModeIndex =
      g_inGameShellState.exclusiveModeIndex;
  g_inGameShellVideoDeadline = 0;
  if (rewriteSettings && !WriteInGameShellSettings()) {
    g_inGameShellState.lastError =
        "invalid settings were recovered but could not be replaced";
    Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
    return false;
  }
  RefreshInGameDeveloperCatalog();
  return true;
}

const SRecoveredInGameShellState*
RecoveredGameServices_InGameShellState() {
  PollShellSaveCatalog();
  return &g_inGameShellState;
}

const SRecoveredSaveSlotCatalogSnapshot*
RecoveredGameServices_InGameShellSaveCatalog() {
  PollShellSaveCatalog();
  return &g_inGameShellSaveCatalog;
}

const SRecoveredDeveloperCatalogSnapshot*
RecoveredGameServices_InGameShellDeveloperCatalog() {
  RefreshInGameDeveloperCatalog();
  return &g_inGameShellDeveloperCatalog;
}

const SRecoveredInputBindings* RecoveredGameServices_InputBindings() {
  return g_inGameShellState.configured ? &g_inGameShellBindings : nullptr;
}

bool RecoveredGameServices_InGameShellKeyForTesting(std::uint32_t key) {
  return HandleInGameShellKey(key);
}

const SRecoveredDebugMenuState* RecoveredGameServices_DebugMenuState() {
  return &g_debugMenuState;
}

std::size_t RecoveredGameServices_DebugVehicleTypeCount() {
  return g_debugVehicleCatalog.size();
}

bool RecoveredGameServices_DebugVehicleType(
    std::size_t index, SRecoveredDebugVehicleType* type) {
  if (type == nullptr || index >= g_debugVehicleCatalog.size()) return false;
  *type = g_debugVehicleCatalog[index];
  return true;
}

bool RecoveredGameServices_RequestDebugVehicleSpawn(
    std::size_t index, bool enterVehicle) {
  if (index >= g_debugVehicleCatalog.size()) {
    g_debugMenuState.lastError =
        "debug vehicle catalog index is outside the active Level";
    return false;
  }
  return StageDebugCommand(
      enterVehicle ? RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE
                   : RECOVERED_DEBUG_MENU_SPAWN_VEHICLE,
      index);
}

bool RecoveredGameServices_RequestDebugShowState() {
  return StageDebugCommand(RECOVERED_DEBUG_MENU_SHOW_STATE, 0u);
}

bool RecoveredGameServices_RequestDebugStabilizeVehicle() {
  return StageDebugCommand(RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE, 0u);
}

bool RecoveredGameServices_RequestDebugKillPlayer() {
  return StageDebugCommand(RECOVERED_DEBUG_MENU_KILL_PLAYER, 0u);
}

bool RecoveredGameServices_RequestDebugRestorePreDeath() {
  if (!g_debugMenuState.preDeathCheckpointAvailable ||
      g_debugPreDeathCheckpoint.empty()) {
    g_debugMenuState.lastError =
        "no committed debug-death checkpoint is available";
    return false;
  }
  return StageDebugCommand(
      RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH, 0u);
}

bool RecoveredGameServices_RequestDebugDestroyOccupiedVehicle() {
  return StageDebugCommand(
      RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE, 0u);
}

bool RecoveredGameServices_RequestDebugRestorePreVehicleDestruction() {
  if (!g_debugMenuState.preVehicleDestructionCheckpointAvailable ||
      g_debugPreVehicleDestructionCheckpoint.empty()) {
    g_debugMenuState.lastError =
        "no committed pre-destruction vehicle checkpoint is available";
    return false;
  }
  return StageDebugCommand(
      RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION, 0u);
}

bool RecoveredGameServices_RequestDebugDamageOccupiedVehicle() {
  return StageDebugCommand(
      RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE, 0u);
}

bool RecoveredGameServices_RequestDebugLevelSwitch(std::size_t index) {
  if (index >= g_debugLevelCatalog.size()) {
    g_debugMenuState.lastError =
        "debug Level catalog index is outside the active catalog";
    return false;
  }
  return StageDebugCommand(RECOVERED_DEBUG_MENU_SWITCH_LEVEL, index);
}

bool RecoveredGameServices_ProcessPendingDebugCommand() {
  if (!g_debugMenuState.pending) {
    g_debugMenuState.lastError = "no debug command is pending";
    return false;
  }
  const ERecoveredDebugMenuAction action =
      g_debugMenuState.pendingAction;
  const std::size_t index = g_debugMenuState.pendingIndex;
  const unsigned int attempt = g_debugMenuState.pendingAttempts + 1u;
  g_debugMenuState.pending = false;
  g_debugMenuState.pendingAction = RECOVERED_DEBUG_MENU_NONE;
  g_debugMenuState.pendingIndex = 0;
  g_debugMenuState.pendingAttempts = 0;
  g_debugMenuState.lastError.clear();
  g_debugMenuState.lastAction.clear();

  KR_ObjectID vehicle = g_super.m_context == nullptr
                            ? KR_ObjectID::NUL()
                            : g_super.m_context->searchObject(
                                  "Vehicle.Default");
  SRecoveredVehicleRuntimeState vehicleState = {};
  if (g_super.m_context == nullptr || vehicle.isNUL() ||
      !VehicleRuntimeState_Inspect(
          g_super.m_context, vehicle, &vehicleState) ||
      vehicleState.frameBegun) {
    g_debugMenuState.lastError =
        "debug command did not reach a closed Vehicle frame boundary";
    if (DeferDebugCommand(action, index, attempt)) return false;
    g_debugMenuState.lastCommandAttempts = attempt;
    ++g_debugMenuState.failedCommands;
    RefreshNativeDebugMenu();
    return false;
  }

  if (action == RECOVERED_DEBUG_MENU_SHOW_STATE) {
    const char* attribute = VehicleRuntimeState_AttributeName(
        g_super.m_context, vehicle);
    const char* dynamic = VehicleRuntimeState_DynamicName(
        g_super.m_context, vehicle);
    std::ostringstream text;
    text << std::fixed << std::setprecision(3)
         << "Level: " << ContinuationLevelIdentity() << "\n"
         << "VehicleAttr: " << (attribute == nullptr ? "<none>" : attribute)
         << "\nDynamic: " << (dynamic == nullptr ? "<none>" : dynamic)
         << "\nPosition: " << vehicleState.position.x << ", "
         << vehicleState.position.y << ", " << vehicleState.position.z
         << "\nSpeed: " << vehicleState.speed.x << ", "
         << vehicleState.speed.y << ", " << vehicleState.speed.z
         << "\nGround contact: " << vehicleState.touchingGround
         << "\nDead: " << vehicleState.dead
         << "\nTaxi/death transform: " << vehicleState.takingTaxi
         << "\nPre-death checkpoint: "
         << (g_debugMenuState.preDeathCheckpointAvailable ? 1 : 0)
         << "\nPre-vehicle-destruction checkpoint: "
         << (g_debugMenuState.preVehicleDestructionCheckpointAvailable
                 ? 1 : 0)
         << "\nTaxi types: " << g_debugVehicleCatalog.size();
    g_debugMenuState.lastAction = "show-state";
    g_debugMenuState.lastCommandAttempts = attempt;
    ++g_debugMenuState.completedCommands;
    if (_gr_hWnd != nullptr) {
      const std::wstring wide = Utf8ToWide(text.str());
      MessageBoxW(_gr_hWnd, wide.c_str(), L"RR2NW debug state",
                  MB_OK | MB_ICONINFORMATION);
    }
    RefreshNativeDebugMenu();
    return true;
  }

  if (action == RECOVERED_DEBUG_MENU_SWITCH_LEVEL) {
    if (index >= g_debugLevelCatalog.size()) {
      g_debugMenuState.lastError = "debug Level selection is stale";
    } else {
      std::vector<std::uint8_t> source;
      SLevelContinuationSummary sourceSummary;
      if (!RecoveredGameServices_CaptureLevelContinuation(
              &source, &sourceSummary)) {
        g_debugMenuState.lastError =
            RecoveredGameServices_LastLevelContinuationError();
        if (DeferDebugCommand(action, index, attempt)) return false;
      } else {
        g_debugLevelSwitchRequest = {};
        g_debugLevelSwitchRequest.ready = true;
        g_debugLevelSwitchRequest.sourceLevel =
            ContinuationLevelIdentity();
        g_debugLevelSwitchRequest.targetLevel =
            g_debugLevelCatalog[index];
        g_debugLevelSwitchRequest.sourceContinuation = std::move(source);
        g_debugLevelSwitchRequest.sourceContinuationSummary = sourceSummary;
        g_debugMenuState.lastAction = "switch-level-staged";
        g_debugMenuState.lastCommandAttempts = attempt;
        ++g_debugMenuState.levelSwitchRequests;
        RefreshNativeDebugMenu();
        return true;
      }
    }
    g_debugMenuState.lastCommandAttempts = attempt;
    ++g_debugMenuState.failedCommands;
    RefreshNativeDebugMenu();
    return false;
  }

  if (action == RECOVERED_DEBUG_MENU_KILL_PLAYER) {
    Vehicle* controlled = static_cast<Vehicle*>(
        g_super.m_context->queryInterface(vehicle, IVehicleIID));
    if (g_debugMenuState.preDeathCheckpointAvailable ||
        g_debugMenuState.preVehicleDestructionCheckpointAvailable ||
        !g_debugPreDeathCheckpoint.empty() ||
        !g_debugPreVehicleDestructionCheckpoint.empty()) {
      g_debugMenuState.lastError =
          "restore the existing pre-death checkpoint before killing again";
    } else if (vehicleState.dead || vehicleState.takingTaxi ||
               controlled == nullptr ||
               !controlled->taxiChangeEnabled()) {
      g_debugMenuState.lastError =
          "debug death requires the living default player body";
    } else if (g_vehicleControlInput.ActiveActionCount() != 0u) {
      g_debugMenuState.lastError =
          "debug death requires neutral Vehicle controls";
      if (DeferDebugCommand(action, index, attempt)) return false;
    }
    if (!g_debugMenuState.lastError.empty()) {
      g_debugMenuState.lastCommandAttempts = attempt;
      ++g_debugMenuState.failedCommands;
      RefreshNativeDebugMenu();
      return false;
    }
  }

  if (action == RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE) {
    Vehicle* controlled = static_cast<Vehicle*>(
        g_super.m_context->queryInterface(vehicle, IVehicleIID));
    if (g_debugMenuState.preDeathCheckpointAvailable ||
        g_debugMenuState.preVehicleDestructionCheckpointAvailable ||
        !g_debugPreDeathCheckpoint.empty() ||
        !g_debugPreVehicleDestructionCheckpoint.empty()) {
      g_debugMenuState.lastError =
          "restore the existing debug checkpoint before destroying again";
    } else if (vehicleState.dead || vehicleState.takingTaxi ||
               controlled == nullptr ||
               controlled->taxiChangeEnabled()) {
      g_debugMenuState.lastError =
          "debug destruction requires a living occupied type-1 vehicle";
    } else if (g_godMode != 0) {
      g_debugMenuState.lastError =
          "disable god mode before exercising authentic vehicle damage";
    } else if (g_vehicleControlInput.ActiveActionCount() != 0u) {
      g_debugMenuState.lastError =
          "debug destruction requires neutral Vehicle controls";
      if (DeferDebugCommand(action, index, attempt)) return false;
    }
    if (!g_debugMenuState.lastError.empty()) {
      g_debugMenuState.lastCommandAttempts = attempt;
      ++g_debugMenuState.failedCommands;
      RefreshNativeDebugMenu();
      return false;
    }
  }

  if (action == RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE) {
    Vehicle* controlled = static_cast<Vehicle*>(
        g_super.m_context->queryInterface(vehicle, IVehicleIID));
    if (g_debugMenuState.preDeathCheckpointAvailable ||
        g_debugMenuState.preVehicleDestructionCheckpointAvailable ||
        !g_debugPreDeathCheckpoint.empty() ||
        !g_debugPreVehicleDestructionCheckpoint.empty()) {
      g_debugMenuState.lastError =
          "restore the existing debug checkpoint before damaging again";
    } else if (vehicleState.dead || vehicleState.takingTaxi ||
               controlled == nullptr ||
               controlled->taxiChangeEnabled()) {
      g_debugMenuState.lastError =
          "debug damage requires a living occupied type-1 vehicle";
    } else if (g_vehicleControlInput.ActiveActionCount() != 0u) {
      g_debugMenuState.lastError =
          "debug damage requires neutral Vehicle controls";
      if (DeferDebugCommand(action, index, attempt)) return false;
    }
    if (!g_debugMenuState.lastError.empty()) {
      g_debugMenuState.lastCommandAttempts = attempt;
      ++g_debugMenuState.failedCommands;
      RefreshNativeDebugMenu();
      return false;
    }
  }

  std::vector<std::uint8_t> backup;
  SLevelContinuationSummary backupSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &backup, &backupSummary)) {
    g_debugMenuState.lastError =
        RecoveredGameServices_LastLevelContinuationError();
    if (DeferDebugCommand(action, index, attempt)) return false;
    g_debugMenuState.lastCommandAttempts = attempt;
    ++g_debugMenuState.failedCommands;
    RefreshNativeDebugMenu();
    return false;
  }

  bool completed = false;
  bool mutationStarted = false;
  if (action == RECOVERED_DEBUG_MENU_STABILIZE_VEHICLE) {
    mutationStarted = true;
    completed = VehicleRuntimeState_DebugStabilize(g_super.m_context);
    g_debugMenuState.lastAction = "stabilize-vehicle";
    if (completed) ++g_debugMenuState.stabilizedVehicles;
    if (!completed)
      g_debugMenuState.lastError =
          "Vehicle runtime rejected last-stable-position recovery";
  } else if (action == RECOVERED_DEBUG_MENU_DAMAGE_OCCUPIED_VEHICLE) {
    const double damageBefore = vehicleState.damage;
    const double eventTime =
        (std::max)(0.1, (std::max)(Session::m_viewTime,
                                  vehicleState.lastTime));
    mutationStarted = true;
    completed = VehicleRuntimeState_DebugDamageOccupiedVehicle(
        g_super.m_context, eventTime);
    SRecoveredVehicleRuntimeState damaged = {};
    completed = completed && VehicleRuntimeState_Inspect(
        g_super.m_context, vehicle, &damaged) &&
        damaged.damage > 0.0 && damaged.damage < damageBefore &&
        damaged.panelReady == vehicleState.panelReady &&
        damaged.panelOpen == vehicleState.panelOpen &&
        damaged.taxiChangeEnabled == 0 &&
        RecoveredGameServices_VehicleCameraMode() ==
            RECOVERED_VEHICLE_CAMERA_LIVE;
    if (completed) {
      ++g_debugMenuState.damagedOccupiedVehicles;
      g_debugMenuState.lastVehicleDamageBefore = damageBefore;
      g_debugMenuState.lastVehicleDamageAfter = damaged.damage;
      g_debugMenuState.lastAction = "damage-occupied-vehicle";
    } else {
      g_debugMenuState.lastError =
          "occupied Vehicle rejected bounded nonlethal damage";
    }
  } else if (action == RECOVERED_DEBUG_MENU_KILL_PLAYER) {
    const int corpseBaseline = CorpseSubjectState_LiveCount();
    const double eventTime =
        (std::max)(0.1, (std::max)(Session::m_viewTime,
                                  vehicleState.lastTime));
    Vehicle* controlled = static_cast<Vehicle*>(
        g_super.m_context->queryInterface(vehicle, IVehicleIID));
    CFMatrix3x4 deathCamera;
    std::vector<std::uint8_t> deadContinuation;
    SLevelContinuationSummary deadSummary;
    g_levelContinuationFailure.clear();
    mutationStarted = true;
    const bool deathMutated = corpseBaseline >= 0 &&
        controlled != nullptr &&
        VehicleRuntimeState_DebugKill(g_super.m_context, eventTime);
    const bool deathOwned = deathMutated &&
        CorpseSubjectState_LiveCount() == corpseBaseline + 1 &&
        !controlled->panelOpen() && g_vehicleControlInput.IsSubscribed();
    const bool cameraReady = deathOwned &&
        VehicleRuntimeState_BuildCamera(g_super.m_context, &deathCamera);
    const bool deathCaptured = cameraReady &&
        RecoveredGameServices_CaptureLevelContinuation(
            &deadContinuation, &deadSummary);
    completed = deathCaptured && deadSummary.ready &&
        deadSummary.worldFingerprint != 0 &&
        deadSummary.containerFingerprint != 0;
    if (completed) {
      g_debugPreDeathCheckpoint = backup;
      g_debugPreDeathCheckpointSummary = backupSummary;
      g_debugPreDeathCorpseCount = corpseBaseline;
      g_debugMenuState.preDeathCheckpointAvailable = true;
      g_debugMenuState.deathWorldFingerprint =
          deadSummary.worldFingerprint;
      g_debugMenuState.deathContinuationFingerprint =
          deadSummary.containerFingerprint;
      ++g_debugMenuState.forcedDeaths;
      ++g_debugMenuState.deathCorpseCreations;
      ++g_debugMenuState.deathCameraProofs;
      ++g_debugMenuState.deathSaveProofs;
      g_debugMenuState.lastAction = "kill-player";
    } else {
      g_debugMenuState.lastError =
          RecoveredGameServices_LastLevelContinuationError();
      if (g_debugMenuState.lastError.empty())
        g_debugMenuState.lastError =
            "real player death lifecycle did not reach a saveable state";
    }
  } else if (action ==
             RECOVERED_DEBUG_MENU_DESTROY_OCCUPIED_VEHICLE) {
    const int orphanBaseline = OrphanSubjectState_LiveCount();
    const double eventTime =
        (std::max)(0.1, (std::max)(Session::m_viewTime,
                                  vehicleState.lastTime));
    Vehicle* controlled = static_cast<Vehicle*>(
        g_super.m_context->queryInterface(vehicle, IVehicleIID));
    const bool preDestructionPanelReady =
        controlled != nullptr && controlled->panelReady();
    const bool preDestructionPanelOpen =
        controlled != nullptr && controlled->panelOpen();
    SRecoveredVehicleRuntimeState destroyedVehicle = {};
    std::vector<unsigned char> orphanState;
    std::vector<std::uint8_t> destroyedContinuation;
    SLevelContinuationSummary destroyedSummary;
    mutationStarted = true;
    const bool destructionMutated = orphanBaseline >= 0 &&
        controlled != nullptr &&
        VehicleRuntimeState_DebugDestroyOccupiedVehicle(
            g_super.m_context, eventTime);
    const bool destructionOwned = destructionMutated &&
        VehicleRuntimeState_Inspect(
            g_super.m_context, vehicle, &destroyedVehicle) &&
        !destroyedVehicle.dead && !destroyedVehicle.takingTaxi &&
        controlled->taxiChangeEnabled() &&
        OrphanSubjectState_LiveCount() == orphanBaseline + 1 &&
        g_vehicleControlInput.IsSubscribed() &&
        g_vehicleControlInput.ActiveActionCount() == 0u;
    const bool orphanCaptured = destructionOwned &&
        OrphanActiveWorldState_CaptureStable(
            g_super.m_context, &orphanState) &&
        OrphanActiveWorldState_SchedulerEventCount(orphanState) ==
            orphanBaseline + 1 &&
        OrphanActiveWorldState_Fingerprint(g_super.m_context) != 0;
    const bool destructionCaptured = orphanCaptured &&
        RecoveredGameServices_CaptureLevelContinuation(
            &destroyedContinuation, &destroyedSummary);
    completed = destructionCaptured && destroyedSummary.ready &&
        destroyedSummary.sections == kActiveWorldOwnerSectionCount &&
        destroyedSummary.worldFingerprint != 0 &&
        destroyedSummary.containerFingerprint != 0;
    if (completed) {
      g_debugPreVehicleDestructionCheckpoint = backup;
      g_debugPreVehicleDestructionCheckpointSummary = backupSummary;
      g_debugPreVehicleDestructionOrphanCount = orphanBaseline;
      g_debugPreVehicleDestructionPanelReady = preDestructionPanelReady;
      g_debugPreVehicleDestructionPanelOpen = preDestructionPanelOpen;
      g_debugMenuState.preVehicleDestructionCheckpointAvailable = true;
      g_debugMenuState.destructionWorldFingerprint =
          destroyedSummary.worldFingerprint;
      g_debugMenuState.destructionContinuationFingerprint =
          destroyedSummary.containerFingerprint;
      g_debugMenuState.destructionOrphanFingerprint =
          OrphanActiveWorldState_Fingerprint(g_super.m_context);
      ++g_debugMenuState.forcedVehicleDestructions;
      ++g_debugMenuState.destructionOrphanCreations;
      ++g_debugMenuState.destructionSaveProofs;
      g_debugMenuState.lastAction = "destroy-occupied-vehicle";
    } else {
      g_debugMenuState.lastError =
          RecoveredGameServices_LastLevelContinuationError();
      if (g_debugMenuState.lastError.empty())
        g_debugMenuState.lastError =
            OrphanActiveWorldState_LastFailure();
      if (g_debugMenuState.lastError.empty())
        g_debugMenuState.lastError =
            "occupied Vehicle destruction did not reach a saveable ORP1 state";
    }
  } else if (action == RECOVERED_DEBUG_MENU_RESTORE_PRE_DEATH) {
    mutationStarted = true;
    SLevelContinuationSummary restored;
    completed = g_debugMenuState.preDeathCheckpointAvailable &&
        !g_debugPreDeathCheckpoint.empty() &&
        g_debugPreDeathCorpseCount >= 0 &&
        RecoveredGameServices_RestoreLevelContinuation(
            g_debugPreDeathCheckpoint, &restored);
    SRecoveredVehicleRuntimeState restoredVehicle = {};
    completed = completed && restored.ready &&
        restored.worldFingerprint ==
            g_debugPreDeathCheckpointSummary.worldFingerprint &&
        restored.containerFingerprint ==
            g_debugPreDeathCheckpointSummary.containerFingerprint &&
        VehicleRuntimeState_Inspect(
            g_super.m_context, vehicle, &restoredVehicle) &&
        !restoredVehicle.dead && !restoredVehicle.takingTaxi &&
        CorpseSubjectState_LiveCount() == g_debugPreDeathCorpseCount &&
        g_vehicleControlInput.IsSubscribed();
    if (completed) {
      g_debugPreDeathCheckpoint.clear();
      g_debugPreDeathCheckpointSummary = {};
      g_debugPreDeathCorpseCount = -1;
      g_debugMenuState.preDeathCheckpointAvailable = false;
      ++g_debugMenuState.restoredPreDeathCheckpoints;
      g_debugMenuState.lastAction = "restore-pre-death";
    } else {
      g_debugMenuState.lastError =
          RecoveredGameServices_LastLevelContinuationError();
      if (g_debugMenuState.lastError.empty())
        g_debugMenuState.lastError =
            "pre-death checkpoint restore did not rebind the live player";
    }
  } else if (action ==
             RECOVERED_DEBUG_MENU_RESTORE_PRE_VEHICLE_DESTRUCTION) {
    mutationStarted = true;
    SLevelContinuationSummary restored;
    completed =
        g_debugMenuState.preVehicleDestructionCheckpointAvailable &&
        !g_debugPreVehicleDestructionCheckpoint.empty() &&
        g_debugPreVehicleDestructionOrphanCount >= 0 &&
        RecoveredGameServices_RestoreLevelContinuation(
            g_debugPreVehicleDestructionCheckpoint, &restored);
    KR_ObjectID restoredVehicleID =
        g_super.m_context->searchObject("Vehicle.Default");
    Vehicle* restoredObject = restoredVehicleID.isNUL()
        ? nullptr
        : static_cast<Vehicle*>(g_super.m_context->queryInterface(
              restoredVehicleID, IVehicleIID));
    SRecoveredVehicleRuntimeState restoredVehicle = {};
    SRecoveredVehicleCameraTelemetry restoredCamera = {};
    std::vector<std::uint8_t> recapturedBytes;
    SLevelContinuationSummary recaptured;
    completed = completed && restored.ready &&
        restored.worldFingerprint ==
            g_debugPreVehicleDestructionCheckpointSummary.worldFingerprint &&
        restored.containerFingerprint ==
            g_debugPreVehicleDestructionCheckpointSummary
                .containerFingerprint &&
        restoredObject != nullptr &&
        VehicleRuntimeState_Inspect(
            g_super.m_context, restoredVehicleID, &restoredVehicle) &&
        !restoredVehicle.dead && !restoredVehicle.takingTaxi &&
        !restoredObject->taxiChangeEnabled() &&
        restoredObject->panelReady() ==
            g_debugPreVehicleDestructionPanelReady &&
        restoredObject->panelOpen() ==
            g_debugPreVehicleDestructionPanelOpen &&
        OrphanSubjectState_LiveCount() ==
            g_debugPreVehicleDestructionOrphanCount &&
        VehicleRuntimeState_InspectCamera(
            g_super.m_context, &restoredCamera) &&
        restoredCamera.mode == RECOVERED_VEHICLE_CAMERA_LIVE &&
        g_vehicleControlInput.IsSubscribed() &&
        g_vehicleControlInput.ActiveActionCount() == 0u &&
        RecoveredGameServices_CaptureLevelContinuation(
            &recapturedBytes, &recaptured) && recaptured.ready &&
        recaptured.worldFingerprint ==
            g_debugPreVehicleDestructionCheckpointSummary.worldFingerprint &&
        recaptured.containerFingerprint ==
            g_debugPreVehicleDestructionCheckpointSummary
                .containerFingerprint;
    if (completed) {
      g_debugPreVehicleDestructionCheckpoint.clear();
      g_debugPreVehicleDestructionCheckpointSummary = {};
      g_debugPreVehicleDestructionOrphanCount = -1;
      g_debugPreVehicleDestructionPanelReady = false;
      g_debugPreVehicleDestructionPanelOpen = false;
      g_debugMenuState.preVehicleDestructionCheckpointAvailable = false;
      ++g_debugMenuState.restoredPreVehicleDestructionCheckpoints;
      g_debugMenuState.lastAction =
          "restore-pre-vehicle-destruction";
    } else {
      g_debugMenuState.lastError =
          RecoveredGameServices_LastLevelContinuationError();
      if (g_debugMenuState.lastError.empty())
        g_debugMenuState.lastError =
            "pre-destruction checkpoint restore did not rebind the occupied Vehicle";
    }
  } else if (action == RECOVERED_DEBUG_MENU_SPAWN_VEHICLE ||
             action == RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE) {
    if (index >= g_debugVehicleCatalog.size()) {
      g_debugMenuState.lastError = "debug vehicle selection is stale";
    } else {
      const SRecoveredDebugVehicleType type =
          g_debugVehicleCatalog[index];
      // HorizontalForward historically returns Row(2), the view/model back
      // vector used by the vessel.  Keep its yaw convention for Taxi, but
      // place debug vehicles along the physical forward direction so the
      // requested object actually appears in front of the player.
      const CFVector3 back = HorizontalForward(vehicleState.direction);
      const CFVector3 position =
          vehicleState.position - back * 16.0 +
          CFVector3(0.0, 24.0, 0.0);
      const double angle = std::atan2(back.x, back.z);
      const double timeStamp =
          (std::max)(0.1, (std::max)(Session::m_viewTime,
                                    vehicleState.lastTime));
      char objectName[64] = {};
      std::snprintf(objectName, sizeof(objectName), "Debug.Taxi.%04u",
                    g_debugMenuState.nextObjectOrdinal);
      KR_ObjectID spawned = KR_ObjectID::NUL();
      STaxiDebugSpawnPlacement placement;
      std::string failure;
      mutationStarted = true;
      const bool spawnPlaced = TaxiSubjectState_DebugSpawn(
          g_super.m_context, type.taxiAttribute.c_str(), objectName,
          position, angle, timeStamp, &spawned, &placement, &failure);
      completed = spawnPlaced;
      if (!spawnPlaced)
        ++g_debugMenuState.spawnPlacementFailures;
      if (completed &&
          action == RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE) {
        completed = TaxiSubjectState_DebugTakeVehicle(
            g_super.m_context, vehicle, spawned, timeStamp, &failure) &&
                    VehicleRuntimeState_RebaseRestoredOwner(
                        g_super.m_context);
      }
      if (completed) {
        g_debugMenuState.lastObject = objectName;
        g_debugMenuState.lastTaxiAttribute = type.taxiAttribute;
        g_debugMenuState.lastVehicleAttribute = type.vehicleAttribute;
        ++g_debugMenuState.nextObjectOrdinal;
        ++g_debugMenuState.spawnedVehicles;
        ++g_debugMenuState.groundedVehicleSpawns;
        g_debugMenuState.sweepGroundedVehicleSpawns +=
            placement.sweepHit != 0 ? 1u : 0u;
        g_debugMenuState.terrainFallbackVehicleSpawns +=
            placement.terrainFallback != 0 ? 1u : 0u;
        g_debugMenuState.lastSpawnBumpKind = placement.bumpKind;
        g_debugMenuState.lastSpawnSweepTime = placement.sweepTime;
        g_debugMenuState.lastSpawnDropDistance = placement.dropDistance;
        g_debugMenuState.lastSpawnOriginClearance =
            placement.originClearance;
        g_debugMenuState.lastSpawnModelBottomClearance =
            placement.modelBottomClearance;
        g_debugMenuState.lastSpawnRequestedY =
            placement.requestedPosition.y;
        g_debugMenuState.lastSpawnSurfaceY = placement.surfacePosition.y;
        g_debugMenuState.lastSpawnResolvedY =
            placement.resolvedPosition.y;
        if (action == RECOVERED_DEBUG_MENU_SPAWN_VEHICLE) {
          SPendingDebugTaxiSettlement settlement;
          settlement.objectName = objectName;
          settlement.expectedPosition = placement.resolvedPosition;
          settlement.remainingFrames = kDebugTaxiSettlementFrames;
          g_debugTaxiSettlements.push_back(settlement);
        }
        if (action == RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE)
          ++g_debugMenuState.enteredVehicles;
        g_debugMenuState.lastAction =
            action == RECOVERED_DEBUG_MENU_SPAWN_AND_ENTER_VEHICLE
                ? "spawn-and-enter-vehicle"
                : "spawn-vehicle";
      } else {
        g_debugMenuState.lastError = failure.empty()
                                         ? "debug vehicle transition failed"
                                         : failure;
      }
    }
  } else {
    g_debugMenuState.lastError = "debug command action is invalid";
  }

  if (!completed && mutationStarted) {
    ++g_debugMenuState.rollbackAttempts;
    SLevelContinuationSummary restored;
    if (RecoveredGameServices_RestoreLevelContinuation(backup, &restored)) {
      ++g_debugMenuState.rollbackCompletions;
    } else {
      const std::string rollbackFailure =
          RecoveredGameServices_LastLevelContinuationError();
      if (!g_debugMenuState.lastError.empty())
        g_debugMenuState.lastError += "; ";
      g_debugMenuState.lastError += "debug rollback failed: " +
                                    rollbackFailure;
      Report(RECOVERED_GAME_SERVICES_DEBUG_MENU_FAILURE);
    }
  }
  if (completed)
    ++g_debugMenuState.completedCommands;
  else
    ++g_debugMenuState.failedCommands;
  g_debugMenuState.lastCommandAttempts = attempt;
  RefreshNativeDebugMenu();
  return completed;
}

bool RecoveredGameServices_DebugLevelSwitchPending() {
  return g_debugLevelSwitchRequest.ready;
}

bool RecoveredGameServices_TakeDebugLevelSwitchRequest(
    SRecoveredDebugLevelSwitchRequest* request) {
  if (request == nullptr || !g_debugLevelSwitchRequest.ready) return false;
  *request = std::move(g_debugLevelSwitchRequest);
  g_debugLevelSwitchRequest = {};
  return request->ready;
}

void RecoveredGameServices_RecordDebugLevelSwitchResult(
    const SRecoveredDebugLevelSwitchRequest& request,
    bool committed, bool rollbackAttempted, bool rollbackRestored,
    const std::string& detail) {
  g_debugMenuState.currentLevel = ContinuationLevelIdentity();
  if (committed) {
    ++g_debugMenuState.completedLevelSwitches;
    ++g_debugMenuState.completedCommands;
    g_debugMenuState.lastAction = "switch-level-commit:" +
                                  request.sourceLevel + "->" +
                                  request.targetLevel;
    g_debugMenuState.lastError.clear();
  } else {
    ++g_debugMenuState.failedCommands;
    g_debugMenuState.lastAction = "switch-level-failure:" +
                                  request.sourceLevel + "->" +
                                  request.targetLevel;
    g_debugMenuState.lastError =
        detail.empty() ? "debug Level switch failed" : detail;
    if (rollbackAttempted) {
      if (rollbackRestored)
        ++g_debugMenuState.levelSwitchRollbacks;
      else
        ++g_debugMenuState.levelSwitchRollbackFailures;
    }
  }
  RefreshNativeDebugMenu();
}

bool RecoveredGameServices_RequestSaveSlot(
    std::uint32_t slot, bool allowOverwrite) {
  return RequestSaveSlotInternal(
      slot, allowOverwrite, BuildAutomaticSaveTitle(),
      "RR2NW recovered Windows menu checkpoint");
}

bool RecoveredGameServices_RequestSaveSlotWithMetadata(
    std::uint32_t slot, bool allowOverwrite, const std::string& title,
    const std::string& description) {
  if (!RequestSaveSlotInternal(slot, allowOverwrite, title, description))
    return false;
  ++g_saveMenuState.customMetadataSaveRequests;
  return true;
}

bool RecoveredGameServices_RequestLoadSlot(std::uint32_t slot) {
  g_saveMenuState.lastError.clear();
  if (!g_saveMenuState.configured) {
    g_saveMenuState.lastError = "save directory is not configured";
    return false;
  }
  if (slot >= LevelSaveSlot_Count()) {
    g_saveMenuState.lastError = "load slot index is outside 0..7";
    return false;
  }
  if (g_saveMenuState.pending || g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready ||
      g_scriptedLevelTransitionRequest.ready) {
    g_saveMenuState.lastError =
        "another save/load command is already pending";
    return false;
  }
  SLevelSaveSlot archive;
  SLevelSaveSlotStatus status;
  if (!LevelSaveSlot_Read(g_saveMenuState.directory, slot,
                          &archive, &status)) {
    g_saveMenuState.lastError = status.detail;
    return false;
  }
  if (!SlotCanBeRequested(archive)) {
    g_saveMenuState.lastError =
        "save slot belongs to the current Level but a different content/mod "
        "set";
    return false;
  }
  g_saveMenuState.pending = true;
  g_saveMenuState.pendingAction = RECOVERED_SAVE_MENU_LOAD;
  g_saveMenuState.pendingSlot = slot;
  g_saveMenuState.pendingAttempts = 0;
  g_saveMenuState.lastCommandAttempts = 0;
  g_saveMenuState.pendingTitle.clear();
  g_saveMenuState.pendingDescription.clear();
  g_saveMenuAllowOverwrite = false;
  ++g_saveMenuState.loadRequests;
  return true;
}

bool RecoveredGameServices_ProcessPendingSaveCommand(
    SLevelSaveSlotSummary* slotSummary,
    SLevelContinuationSummary* continuationSummary) {
  if (slotSummary != nullptr) *slotSummary = {};
  if (continuationSummary != nullptr) *continuationSummary = {};
  if (!g_saveMenuState.pending) {
    g_saveMenuState.lastError = "no save/load command is pending";
    return false;
  }
  const ERecoveredSaveMenuAction action =
      g_saveMenuState.pendingAction;
  const std::uint32_t slot = g_saveMenuState.pendingSlot;
  const bool allowOverwrite = g_saveMenuAllowOverwrite;
  const std::string pendingTitle = g_saveMenuState.pendingTitle;
  const std::string pendingDescription =
      g_saveMenuState.pendingDescription;
  const unsigned int attempt = g_saveMenuState.pendingAttempts + 1u;
  g_saveMenuState.pending = false;
  g_saveMenuState.pendingAction = RECOVERED_SAVE_MENU_NONE;
  g_saveMenuState.pendingSlot = 0;
  g_saveMenuState.pendingAttempts = 0;
  g_saveMenuState.pendingTitle.clear();
  g_saveMenuState.pendingDescription.clear();
  g_saveMenuAllowOverwrite = false;
  g_saveMenuState.lastError.clear();
  g_saveMenuState.lastPreview = {};
  g_saveMenuState.lastSlot = {};
  g_saveMenuState.lastContinuation = {};

  SLevelSaveSlotSummary completedSlot;
  SLevelContinuationSummary completedContinuation;
  bool completed = false;
  bool crossLevelStaged = false;
  if (action == RECOVERED_SAVE_MENU_SAVE) {
    const std::wstring path =
        LevelSaveSlot_Path(g_saveMenuState.directory, slot);
    if (!allowOverwrite &&
        GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
      g_saveMenuState.lastError =
          "save slot appeared before commit and overwrite was not confirmed";
    } else {
      std::vector<std::uint8_t> preview;
      std::string previewFailure;
      if (!RecoveredFramePreview_CapturePng(
              &preview, &g_saveMenuState.lastPreview,
              &previewFailure)) {
        g_saveMenuState.lastError = previewFailure;
      } else {
        completed = RecoveredGameServices_SaveLevelSlot(
            g_saveMenuState.directory, slot, pendingTitle,
            pendingDescription, preview,
            &completedSlot, &completedContinuation);
        if (!completed)
          g_saveMenuState.lastError =
              RecoveredGameServices_LastLevelSaveSlotError();
      }
    }
  } else if (action == RECOVERED_SAVE_MENU_LOAD) {
    SLevelSaveSlot archive;
    SLevelSaveSlotStatus status;
    if (!LevelSaveSlot_Read(g_saveMenuState.directory, slot,
                            &archive, &status) ||
        !LevelSaveSlot_Summarize(archive, &completedSlot, &status)) {
      g_saveMenuState.lastError = status.detail;
    } else if (SlotTargetsCurrentLevel(archive)) {
      if (!SlotIsCompatible(archive)) {
        g_saveMenuState.lastError =
            "save slot belongs to the current Level but a different "
            "content/mod set";
      } else {
        completed = RecoveredGameServices_RestoreLevelContinuation(
            archive.continuation, &completedContinuation);
        if (!completed)
          g_saveMenuState.lastError =
              RecoveredGameServices_LastLevelContinuationError();
      }
    } else {
      std::vector<std::uint8_t> sourceContinuation;
      SLevelContinuationSummary sourceSummary;
      if (!RecoveredGameServices_CaptureLevelContinuation(
              &sourceContinuation, &sourceSummary)) {
        g_saveMenuState.lastError =
            RecoveredGameServices_LastLevelContinuationError();
      } else {
        g_crossLevelLoadRequest = {};
        g_crossLevelLoadRequest.ready = true;
        g_crossLevelLoadRequest.slot = slot;
        g_crossLevelLoadRequest.sourceLevel =
            ContinuationLevelIdentity();
        g_crossLevelLoadRequest.targetLevel = archive.level;
        g_crossLevelLoadRequest.sourceContinuation =
            std::move(sourceContinuation);
        g_crossLevelLoadRequest.targetContinuation = archive.continuation;
        g_crossLevelLoadRequest.targetSlot = completedSlot;
        g_crossLevelLoadRequest.sourceContinuationSummary = sourceSummary;
        g_saveMenuState.crossLevelRestartPending = true;
        g_saveMenuState.crossLevelSourceLevel =
            g_crossLevelLoadRequest.sourceLevel;
        g_saveMenuState.crossLevelTargetLevel =
            g_crossLevelLoadRequest.targetLevel;
        ++g_saveMenuState.crossLevelRequests;
        completedContinuation = sourceSummary;
        completed = true;
        crossLevelStaged = true;
      }
    }
  } else {
    g_saveMenuState.lastError = "pending save/load action is invalid";
  }

  if (!completed) {
    if (IsRetryableSaveBoundaryFailure(g_saveMenuState.lastError) &&
        attempt < kMaximumStableBoundaryAttempts) {
      g_saveMenuState.pending = true;
      g_saveMenuState.pendingAction = action;
      g_saveMenuState.pendingSlot = slot;
      g_saveMenuState.pendingAttempts = attempt;
      g_saveMenuState.pendingTitle = pendingTitle;
      g_saveMenuState.pendingDescription = pendingDescription;
      g_saveMenuAllowOverwrite = allowOverwrite;
      ++g_saveMenuState.deferredCommands;
      RefreshNativeSaveMenu();
      return false;
    }
    g_saveMenuState.lastCommandAttempts = attempt;
    ++g_saveMenuState.failedCommands;
    RefreshNativeSaveMenu();
    return false;
  }
  g_saveMenuState.lastCommandAttempts = attempt;
  g_saveMenuState.lastSlot = completedSlot;
  g_saveMenuState.lastContinuation = completedContinuation;
  if (action == RECOVERED_SAVE_MENU_SAVE)
    ++g_saveMenuState.completedSaves;
  else if (!crossLevelStaged)
    ++g_saveMenuState.completedLoads;
  if (slotSummary != nullptr) *slotSummary = completedSlot;
  if (continuationSummary != nullptr)
    *continuationSummary = completedContinuation;
  RefreshNativeSaveMenu();
  RequestShellSaveCatalogRefresh();
  return true;
}

bool RecoveredGameServices_CrossLevelLoadPending() {
  return g_crossLevelLoadRequest.ready;
}

bool RecoveredGameServices_TakeCrossLevelLoadRequest(
    SRecoveredCrossLevelLoadRequest* request) {
  if (request == nullptr || !g_crossLevelLoadRequest.ready) return false;
  *request = std::move(g_crossLevelLoadRequest);
  g_crossLevelLoadRequest = {};
  return request->ready;
}

bool RecoveredGameServices_ApplyCrossLevelLoad(
    const SRecoveredCrossLevelLoadRequest& request,
    SLevelContinuationSummary* continuationSummary) {
  if (continuationSummary != nullptr) *continuationSummary = {};
  g_saveMenuState.lastError.clear();
  if (!request.ready || continuationSummary == nullptr ||
      request.targetContinuation.empty() ||
      !LevelIdentityMatches(request.targetLevel,
                            ContinuationLevelIdentity())) {
    g_saveMenuState.lastError =
        "cross-Level load target does not match the initialized Level";
    return false;
  }
  if (request.targetSlot.contentFingerprint !=
      ContinuationContentFingerprint()) {
    g_saveMenuState.lastError =
        "cross-Level save belongs to a different content/mod set";
    return false;
  }
  SLevelContinuationSummary restored;
  if (!RecoveredGameServices_RestoreLevelContinuation(
          request.targetContinuation, &restored)) {
    g_saveMenuState.lastError =
        RecoveredGameServices_LastLevelContinuationError();
    return false;
  }
  g_saveMenuState.crossLevelRestartPending = false;
  g_saveMenuState.crossLevelSourceLevel = request.sourceLevel;
  g_saveMenuState.crossLevelTargetLevel = request.targetLevel;
  g_saveMenuState.lastSlot = request.targetSlot;
  g_saveMenuState.lastContinuation = restored;
  g_saveMenuState.lastCommandAttempts = 1u;
  ++g_saveMenuState.loadRequests;
  ++g_saveMenuState.completedLoads;
  ++g_saveMenuState.completedCrossLevelLoads;
  *continuationSummary = restored;
  RefreshNativeSaveMenu();
  RequestShellSaveCatalogRefresh();
  return true;
}

void RecoveredGameServices_RecordCrossLevelLoadFailure(
    const SRecoveredCrossLevelLoadRequest& request,
    const std::string& detail, bool restartAttempted,
    bool rollbackRestored) {
  g_saveMenuState.crossLevelRestartPending = false;
  g_saveMenuState.crossLevelSourceLevel = request.sourceLevel;
  g_saveMenuState.crossLevelTargetLevel = request.targetLevel;
  g_saveMenuState.lastSlot = request.targetSlot;
  g_saveMenuState.lastError =
      detail.empty() ? "cross-Level load transaction failed" : detail;
  g_saveMenuState.lastCommandAttempts = 1u;
  if (restartAttempted) ++g_saveMenuState.loadRequests;
  ++g_saveMenuState.failedCommands;
  if (restartAttempted) {
    if (rollbackRestored)
      ++g_saveMenuState.crossLevelRollbacks;
    else
      ++g_saveMenuState.crossLevelRollbackFailures;
  }
  RefreshNativeSaveMenu();
}

bool RecoveredGameServices_RequestCampaignRestart() {
  g_campaignRestartState.lastError.clear();
  if (!g_sessionReady || g_super.m_context == nullptr) {
    g_campaignRestartState.lastError =
        "current Level restart requires an active recovered session";
    return false;
  }
  if (g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready || g_saveMenuState.pending ||
      g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_scriptedLevelTransitionRequest.ready) {
    g_campaignRestartState.lastError =
        "another world command is already pending";
    return false;
  }
  g_campaignRestartState.pending = true;
  g_campaignRestartState.pendingAttempts = 0;
  g_campaignRestartState.lastCommandAttempts = 0;
  ++g_campaignRestartState.requests;
  RefreshNativeSaveMenu();
  RefreshNativeDebugMenu();
  return true;
}

bool RecoveredGameServices_ProcessPendingCampaignRestart() {
  if (!g_campaignRestartState.pending) {
    g_campaignRestartState.lastError =
        "no current Level restart is pending";
    return false;
  }
  const unsigned int attempt =
      g_campaignRestartState.pendingAttempts + 1u;
  g_campaignRestartState.pending = false;
  g_campaignRestartState.pendingAttempts = 0;
  g_campaignRestartState.lastError.clear();

  std::vector<std::uint8_t> source;
  SLevelContinuationSummary sourceSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &source, &sourceSummary)) {
    g_campaignRestartState.lastError =
        RecoveredGameServices_LastLevelContinuationError();
    if (IsRetryableSaveBoundaryFailure(
            g_campaignRestartState.lastError) &&
        attempt < kMaximumStableBoundaryAttempts) {
      g_campaignRestartState.pending = true;
      g_campaignRestartState.pendingAttempts = attempt;
      ++g_campaignRestartState.deferredCommands;
      RefreshNativeSaveMenu();
      RefreshNativeDebugMenu();
      return false;
    }
    g_campaignRestartState.lastCommandAttempts = attempt;
    ++g_campaignRestartState.failedRestarts;
    RefreshNativeSaveMenu();
    RefreshNativeDebugMenu();
    return false;
  }

  g_campaignRestartRequest = {};
  g_campaignRestartRequest.ready = true;
  g_campaignRestartRequest.requestOrdinal =
      g_campaignRestartState.requests;
  g_campaignRestartRequest.attempts = attempt;
  g_campaignRestartRequest.deferredCommands =
      g_campaignRestartState.deferredCommands;
  g_campaignRestartRequest.completedBefore =
      g_campaignRestartState.completedRestarts;
  g_campaignRestartRequest.failuresBefore =
      g_campaignRestartState.failedRestarts;
  g_campaignRestartRequest.deadSourceRestartsBefore =
      g_campaignRestartState.deadSourceRestarts;
  g_campaignRestartRequest.rollbacksBefore =
      g_campaignRestartState.rollbacks;
  g_campaignRestartRequest.rollbackFailuresBefore =
      g_campaignRestartState.rollbackFailures;
  g_campaignRestartRequest.level = ContinuationLevelIdentity();
  KR_ObjectID sourceVehicle = g_super.m_context->searchObject(
      "Vehicle.Default");
  SRecoveredVehicleRuntimeState sourceVehicleState = {};
  g_campaignRestartRequest.sourceDead = !sourceVehicle.isNUL() &&
      VehicleRuntimeState_Inspect(
          g_super.m_context, sourceVehicle, &sourceVehicleState) &&
      sourceVehicleState.dead != 0;
  g_campaignRestartRequest.sourceContinuation = std::move(source);
  g_campaignRestartRequest.sourceContinuationSummary = sourceSummary;
  g_campaignRestartState.coordinatorPending = true;
  g_campaignRestartState.currentLevel =
      g_campaignRestartRequest.level;
  g_campaignRestartState.lastCommandAttempts = attempt;
  RefreshNativeSaveMenu();
  RefreshNativeDebugMenu();
  return true;
}

bool RecoveredGameServices_CampaignRestartPending() {
  return g_campaignRestartRequest.ready;
}

bool RecoveredGameServices_TakeCampaignRestartRequest(
    SRecoveredCampaignRestartRequest* request) {
  if (request == nullptr || !g_campaignRestartRequest.ready) return false;
  *request = std::move(g_campaignRestartRequest);
  g_campaignRestartRequest = {};
  return request->ready;
}

void RecoveredGameServices_RecordCampaignRestartResult(
    const SRecoveredCampaignRestartRequest& request,
    bool committed, bool rollbackAttempted, bool rollbackRestored,
    const std::string& detail) {
  g_campaignRestartState = {};
  g_campaignRestartState.requests = request.requestOrdinal;
  g_campaignRestartState.completedRestarts = request.completedBefore;
  g_campaignRestartState.failedRestarts = request.failuresBefore;
  g_campaignRestartState.deadSourceRestarts =
      request.deadSourceRestartsBefore;
  g_campaignRestartState.deferredCommands = request.deferredCommands;
  g_campaignRestartState.lastCommandAttempts = request.attempts;
  g_campaignRestartState.rollbacks = request.rollbacksBefore;
  g_campaignRestartState.rollbackFailures =
      request.rollbackFailuresBefore;
  g_campaignRestartState.currentLevel = ContinuationLevelIdentity();
  if (committed) {
    ++g_campaignRestartState.completedRestarts;
    if (request.sourceDead)
      ++g_campaignRestartState.deadSourceRestarts;
  } else {
    ++g_campaignRestartState.failedRestarts;
    g_campaignRestartState.lastError =
        detail.empty() ? "current Level restart failed" : detail;
    if (rollbackAttempted) {
      if (rollbackRestored)
        ++g_campaignRestartState.rollbacks;
      else
        ++g_campaignRestartState.rollbackFailures;
    }
  }
  RefreshNativeSaveMenu();
  RefreshNativeDebugMenu();
}

const SRecoveredCampaignRestartState*
RecoveredGameServices_CampaignRestartState() {
  return &g_campaignRestartState;
}

bool RecoveredGameServices_ScriptedLevelTransitionPending() {
  return g_scriptedLevelTransitionRequest.ready;
}

bool RecoveredGameServices_TakeScriptedLevelTransitionRequest(
    SRecoveredScriptedLevelTransitionRequest* request) {
  if (request == nullptr || !g_scriptedLevelTransitionRequest.ready)
    return false;
  *request = std::move(g_scriptedLevelTransitionRequest);
  g_scriptedLevelTransitionRequest = {};
  return request->ready;
}

void RecoveredGameServices_RecordScriptedLevelTransitionResult(
    const SRecoveredScriptedLevelTransitionRequest& request,
    const std::string& targetLevel, bool committed,
    bool rollbackAttempted, bool rollbackRestored,
    const std::string& detail) {
  g_scriptedLevelTransitionState = {};
  g_scriptedLevelTransitionState.requests = request.requestOrdinal;
  g_scriptedLevelTransitionState.completedTransitions =
      request.completedBefore;
  g_scriptedLevelTransitionState.failedTransitions =
      request.failuresBefore;
  g_scriptedLevelTransitionState.rollbacks = request.rollbacksBefore;
  g_scriptedLevelTransitionState.rollbackFailures =
      request.rollbackFailuresBefore;
  g_scriptedLevelTransitionState.lastTargetLevelIndex =
      request.targetLevelIndex;
  g_scriptedLevelTransitionState.sourceLevel = request.sourceLevel;
  g_scriptedLevelTransitionState.targetLevel = targetLevel;
  if (committed) {
    ++g_scriptedLevelTransitionState.completedTransitions;
  } else {
    ++g_scriptedLevelTransitionState.failedTransitions;
    g_scriptedLevelTransitionState.lastError =
        detail.empty() ? "scripted Level transition failed" : detail;
    if (rollbackAttempted) {
      if (rollbackRestored)
        ++g_scriptedLevelTransitionState.rollbacks;
      else
        ++g_scriptedLevelTransitionState.rollbackFailures;
    }
  }
  RefreshNativeSaveMenu();
  RefreshNativeDebugMenu();
}

const SRecoveredScriptedLevelTransitionState*
RecoveredGameServices_ScriptedLevelTransitionState() {
  return &g_scriptedLevelTransitionState;
}

const SRecoveredSaveMenuState* RecoveredGameServices_SaveMenuState() {
  return &g_saveMenuState;
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

unsigned int RecoveredGameServices_VehiclePhysicalReconciliationCount() {
  return g_vehicleControlInput.PhysicalReconciliationCount();
}

bool RecoveredGameServices_WindowsInputTelemetry(
    SRecoveredWindowsInputTelemetry* telemetry) {
  if (telemetry == nullptr) return false;
  *telemetry = g_windowsInputAdapter.Telemetry();
  return true;
}

unsigned int RecoveredGameServices_MapTogglePresses() {
  return g_mapTogglePresses;
}

bool RecoveredGameServices_DebugMapReady() {
  return g_debugMap.IsInitialized() != 0 && g_debugMap.m_vPort != nullptr;
}

bool RecoveredGameServices_DebugMapActive() {
  return g_debugMap.IsActive() != 0;
}

int RecoveredGameServices_DebugMapWidth() {
  return g_debugMap.MapWidth();
}

int RecoveredGameServices_DebugMapHeight() {
  return g_debugMap.MapHeight();
}

unsigned int RecoveredGameServices_DebugMapDrawFrames() {
  return g_debugMap.DrawFrames();
}

unsigned int RecoveredGameServices_DebugMapOpenTransitions() {
  return g_debugMap.OpenTransitions();
}

unsigned int RecoveredGameServices_DebugMapCloseTransitions() {
  return g_debugMap.CloseTransitions();
}

bool RecoveredGameServices_RequestDebugMapToggle() {
  if (!g_sessionReady || !g_loopReady) return false;
  SRecoveredWindowsInputAction input = {};
  input.action = DMAP_TOGGLE;
  input.value = 1.0;
  input.code = 'M';
  return DispatchWindowsInputAction(input, CurrentInputEventTime());
}

bool RecoveredGameServices_StageMissionMapProbe() {
  if (!RecoveredGameServices_DebugMapReady() || g_super.m_context == nullptr ||
      g_missionMapProbeLive) {
    return false;
  }
  g_missionMapProbe = {};
  g_debugMapControlProbe = {};
  g_missionMapBaselineMissions = g_debugMap.MissionCount();
  g_missionMapBaselineTexts = g_debugMap.MissionTextCount();
  g_missionMapBaselineRoutes = g_debugMap.MissionRouteCount();
  g_missionMapBaselineDrawFrames = g_debugMap.DrawFrames();

  bool staged = false;
  const double probeTime =
      (std::max)(Session::m_moment, Session::m_viewTime) + 30.0;
  if (!MissionActiveWorldState_StageProbe(
          g_super.m_context, probeTime, &staged) || !staged) {
    return false;
  }
  g_missionMapProbeLive = true;
  g_missionMapProbe.staged = 1;
  g_missionMapProbe.missionCount =
      g_debugMap.MissionCount() - g_missionMapBaselineMissions;
  g_missionMapProbe.textCount =
      g_debugMap.MissionTextCount() - g_missionMapBaselineTexts;
  g_missionMapProbe.routeCount =
      g_debugMap.MissionRouteCount() - g_missionMapBaselineRoutes;
  g_missionMapProbe.summaryPublished =
      g_missionMapProbe.missionCount == 1 &&
      g_missionMapProbe.textCount == 1;
  return g_missionMapProbe.missionCount == 1;
}

bool RecoveredGameServices_VerifyMissionMapProbe() {
  if (!g_missionMapProbeLive || !g_debugMap.IsActive() ||
      g_debugMap.DrawFrames() <= g_missionMapBaselineDrawFrames) {
    return false;
  }
  SGRSoftwareRasterStats stats = {};
  GRSoftwareGetFrameStats(&stats);
  g_missionMapProbe.renderedFrames = static_cast<int>(
      g_debugMap.DrawFrames() - g_missionMapBaselineDrawFrames);
  g_missionMapProbe.framebufferHash = stats.framebufferHash;
  g_missionMapProbe.framebufferNonClearPixels =
      stats.framebufferNonClearPixels;
  return g_debugMap.MissionCount() - g_missionMapBaselineMissions ==
             g_missionMapProbe.missionCount &&
         g_debugMap.MissionTextCount() - g_missionMapBaselineTexts ==
             g_missionMapProbe.textCount &&
         g_debugMap.MissionRouteCount() - g_missionMapBaselineRoutes ==
             g_missionMapProbe.routeCount &&
         stats.framebufferHash != 0 && stats.framebufferNonClearPixels != 0;
}

bool RecoveredGameServices_ProbeDebugMapControls() {
  // The navigation adapter belongs to the real active DebugMap, not to the
  // synthetic startup objective. Keeping this usable after authored missions
  // lets the retail gate prove simultaneous objective selection as well.
  if (!g_debugMap.IsActive()) return false;
  g_debugMapControlProbe = {};
  g_debugMapControlProbe.available = 1;

  const int initialFollow = g_debugMap.FollowMode();
  const int initialX = g_debugMap.WindowBaseX();
  const int initialY = g_debugMap.WindowBaseY();
  const int initialMission = g_debugMap.CurrentMission();
  const int initialTextLine = g_debugMap.CurrentMissionTextLine();
  const double eventTime = CurrentInputEventTime();
  const auto send = [eventTime](int action, double value,
                                std::uint32_t code) {
    SRecoveredWindowsInputAction input = {};
    input.action = action;
    input.value = value;
    input.code = code;
    return DispatchWindowsInputAction(input, eventTime);
  };

  if (!send(DMAP_TOGGLE_FOLLOW_MODE, 1.0,
            static_cast<std::uint32_t>(g_hardware.SearchCode("Del"))))
    return false;
  const int toggledFollow = g_debugMap.FollowMode();
  g_debugMapControlProbe.followTogglePair =
      toggledFollow != initialFollow &&
      send(DMAP_TOGGLE_FOLLOW_MODE, 1.0,
           static_cast<std::uint32_t>(g_hardware.SearchCode("Del"))) &&
      g_debugMap.FollowMode() == initialFollow ? 1 : 0;
  if (g_debugMapControlProbe.followTogglePair != 1) return false;

  // Enter free-scroll mode for the paired arrow proof, preserving either
  // valid initial mode for callers outside the startup smoke.
  if (g_debugMap.FollowMode() != FALSE &&
      !send(DMAP_TOGGLE_FOLLOW_MODE, 1.0,
            static_cast<std::uint32_t>(g_hardware.SearchCode("Del"))))
    return false;

  const auto scrollPair = [&send](int firstAction, const char* firstCode,
                                  int secondAction, const char* secondCode,
                                  bool horizontal) {
    const int before = horizontal ? g_debugMap.WindowBaseX()
                                  : g_debugMap.WindowBaseY();
    if (!send(firstAction, 1.0, static_cast<std::uint32_t>(
                                      g_hardware.SearchCode(firstCode))))
      return false;
    int changed = horizontal ? g_debugMap.WindowBaseX()
                             : g_debugMap.WindowBaseY();
    if (changed != before) {
      return send(secondAction, 1.0, static_cast<std::uint32_t>(
                                          g_hardware.SearchCode(secondCode))) &&
             (horizontal ? g_debugMap.WindowBaseX()
                         : g_debugMap.WindowBaseY()) == before;
    }
    if (!send(secondAction, 1.0, static_cast<std::uint32_t>(
                                       g_hardware.SearchCode(secondCode))))
      return false;
    changed = horizontal ? g_debugMap.WindowBaseX()
                         : g_debugMap.WindowBaseY();
    return changed != before &&
           send(firstAction, 1.0, static_cast<std::uint32_t>(
                                      g_hardware.SearchCode(firstCode))) &&
           (horizontal ? g_debugMap.WindowBaseX()
                       : g_debugMap.WindowBaseY()) == before;
  };
  g_debugMapControlProbe.horizontalScrollPair =
      scrollPair(TURN_RIGHT, "Right", TURN_RIGHT, "Left", true) ? 1 : 0;
  g_debugMapControlProbe.verticalScrollPair =
      scrollPair(LOOK_UP, "Down", LOOK_UP, "Up", false) ? 1 : 0;
  if (g_debugMapControlProbe.horizontalScrollPair != 1 ||
      g_debugMapControlProbe.verticalScrollPair != 1)
    return false;

  const int textLines = g_debugMap.CurrentMissionTextLineCount();
  g_debugMapControlProbe.textScrollable = textLines > 5 ? 1 : 0;
  if (g_debugMapControlProbe.textScrollable) {
    const int before = g_debugMap.CurrentMissionTextLine();
    const bool downChanged =
        send(DMAP_TEXT_BOX_DOWN, 1.0, static_cast<std::uint32_t>(
                                           g_hardware.SearchCode("PgDn"))) &&
        g_debugMap.CurrentMissionTextLine() != before;
    const bool restoredDown = downChanged &&
        send(DMAP_TEXT_BOX_UP, 1.0, static_cast<std::uint32_t>(
                                         g_hardware.SearchCode("PgUp"))) &&
        g_debugMap.CurrentMissionTextLine() == before;
    const bool upChanged = !downChanged &&
        send(DMAP_TEXT_BOX_UP, 1.0, static_cast<std::uint32_t>(
                                         g_hardware.SearchCode("PgUp"))) &&
        g_debugMap.CurrentMissionTextLine() != before;
    const bool restoredUp = upChanged &&
        send(DMAP_TEXT_BOX_DOWN, 1.0, static_cast<std::uint32_t>(
                                           g_hardware.SearchCode("PgDn"))) &&
        g_debugMap.CurrentMissionTextLine() == before;
    g_debugMapControlProbe.textScrollPair =
        restoredDown || restoredUp ? 1 : 0;
    if (g_debugMapControlProbe.textScrollPair != 1) return false;
  }

  g_debugMapControlProbe.missionSelectable =
      g_debugMap.MissionCount() > 1 ? 1 : 0;
  if (g_debugMapControlProbe.missionSelectable) {
    const int before = g_debugMap.CurrentMission();
    const bool nextChanged =
        send(DMAP_NEXT_MISSION, 1.0, VK_OEM_6) &&
        g_debugMap.CurrentMission() != before;
    const bool restoredNext = nextChanged &&
        send(DMAP_PREVIOUS_MISSION, 1.0, VK_OEM_4) &&
        g_debugMap.CurrentMission() == before;
    const bool previousChanged = !nextChanged &&
        send(DMAP_PREVIOUS_MISSION, 1.0, VK_OEM_4) &&
        g_debugMap.CurrentMission() != before;
    const bool restoredPrevious = previousChanged &&
        send(DMAP_NEXT_MISSION, 1.0, VK_OEM_6) &&
        g_debugMap.CurrentMission() == before;
    g_debugMapControlProbe.missionSelectionPair =
        restoredNext || restoredPrevious ? 1 : 0;
    if (g_debugMapControlProbe.missionSelectionPair != 1) return false;
  }

  if (g_debugMap.FollowMode() != initialFollow &&
      !send(DMAP_TOGGLE_FOLLOW_MODE, 1.0,
            static_cast<std::uint32_t>(g_hardware.SearchCode("Del"))))
    return false;
  g_debugMapControlProbe.stateRestored =
      g_debugMap.FollowMode() == initialFollow &&
      g_debugMap.WindowBaseX() == initialX &&
      g_debugMap.WindowBaseY() == initialY &&
      g_debugMap.CurrentMission() == initialMission &&
      g_debugMap.CurrentMissionTextLine() == initialTextLine ? 1 : 0;
  return g_debugMapControlProbe.stateRestored == 1;
}

bool RecoveredGameServices_ClearMissionMapProbe() {
  if (!g_missionMapProbeLive || g_super.m_context == nullptr ||
      g_debugMap.IsActive() ||
      !MissionActiveWorldState_ClearProbe(g_super.m_context)) {
    return false;
  }
  const bool clean =
      g_debugMap.MissionCount() == g_missionMapBaselineMissions &&
      g_debugMap.MissionTextCount() == g_missionMapBaselineTexts &&
      g_debugMap.MissionRouteCount() == g_missionMapBaselineRoutes;
  g_missionMapProbe.rollbacks = clean ? 1 : 0;
  g_missionMapProbeLive = false;
  return clean;
}

bool RecoveredGameServices_MissionMapProbeTelemetry(
    SRecoveredMissionMapProbeTelemetry* telemetry) {
  if (telemetry == nullptr || g_missionMapProbe.staged == 0) return false;
  *telemetry = g_missionMapProbe;
  return true;
}

bool RecoveredGameServices_DebugMapControlProbeTelemetry(
    SRecoveredDebugMapControlProbeTelemetry* telemetry) {
  if (telemetry == nullptr || g_debugMapControlProbe.available == 0)
    return false;
  *telemetry = g_debugMapControlProbe;
  return true;
}

unsigned int RecoveredGameServices_VehiclePrimaryFirePresses() {
  return g_vehicleControlInput.PrimaryFirePresses();
}

unsigned int RecoveredGameServices_VehicleSecondaryFirePresses() {
  return g_vehicleControlInput.SecondaryFirePresses();
}

unsigned int RecoveredGameServices_VehicleSecondaryFireAcceptedShots() {
  return g_vehicleControlInput.SecondaryFireAcceptedShots();
}

unsigned int RecoveredGameServices_VehicleJumpPresses() {
  return g_vehicleControlInput.JumpPresses();
}

std::size_t RecoveredGameServices_WindowsInputPendingEvents() {
  return g_pendingWindowsInput.size();
}

bool RecoveredGameServices_VehicleControlAxes(
    SRecoveredObserverAxes* axes) {
  if (axes == nullptr || !g_vehicleControlReady) return false;
  *axes = g_vehicleControlInput.DirectionalAxes();
  return true;
}

int RecoveredGameServices_VehicleLastInputFailure() {
  return g_vehicleControlInput.LastInputFailure();
}

int RecoveredGameServices_VehicleLastFrameFailure() {
  return VehicleRuntimeState_LastFrameFailure();
}

int RecoveredGameServices_VehicleLastFrameReadinessIssue() {
  return VehicleRuntimeState_LastFrameReadinessIssue();
}

bool RecoveredGameServices_VehicleDriveTelemetry(
    SRecoveredVehicleDriveTelemetry* telemetry) {
  if (!g_vehicleDriveTelemetryReady || telemetry == nullptr) return false;
  *telemetry = g_vehicleDriveTelemetry;
  return true;
}

bool RecoveredGameServices_FrameTimingTelemetry(
    SRecoveredFrameTimingTelemetry* telemetry) {
  if (telemetry == nullptr || g_frameTimingTelemetry.frames == 0)
    return false;
  *telemetry = g_frameTimingTelemetry;
  return true;
}

bool RecoveredGameServices_VehicleAuthorityState(
    SRecoveredVehicleAuthorityState* authority) {
  if (authority == nullptr || g_super.m_context == nullptr) return false;
  KR_ObjectID vehicle =
      g_super.m_context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState state = {};
  if (vehicle.isNUL() || !VehicleRuntimeState_Inspect(
          g_super.m_context, vehicle, &state))
    return false;
  *authority = {};
  authority->identityFingerprint =
      VehicleRuntimeState_IdentityFingerprint(g_super.m_context, vehicle);
  authority->damage = state.damage;
  authority->vesselKind = state.vesselKind;
  authority->vesselProfile = VehicleRuntimeState_VesselProfile(
      VehicleRuntimeState_DynamicName(g_super.m_context, vehicle));
  authority->active = state.active;
  authority->frameBegun = state.frameBegun;
  authority->dead = state.dead;
  authority->takingTaxi = state.takingTaxi;
  authority->panelReady = state.panelReady;
  authority->panelOpen = state.panelOpen;
  authority->taxiChangeEnabled = state.taxiChangeEnabled;
  return authority->identityFingerprint != 0 &&
         authority->vesselKind != RECOVERED_VEHICLE_VESSEL_UNKNOWN &&
         authority->vesselProfile != RECOVERED_VEHICLE_PROFILE_UNKNOWN;
}

unsigned int RecoveredGameServices_VehicleFrameCount() {
  return g_vehicleFrameCount;
}

unsigned int RecoveredGameServices_VehicleCameraFrameCount() {
  return g_vehicleCameraFrameCount;
}

int RecoveredGameServices_VehicleCameraMode() {
  SRecoveredVehicleCameraTelemetry telemetry = {};
  return g_super.m_context != nullptr &&
                 VehicleRuntimeState_InspectCamera(
                     g_super.m_context, &telemetry)
             ? telemetry.mode
             : RECOVERED_VEHICLE_CAMERA_UNKNOWN;
}

unsigned int RecoveredGameServices_VehicleCameraTransformFrameCount() {
  SRecoveredVehicleCameraTelemetry telemetry = {};
  return g_super.m_context != nullptr &&
                 VehicleRuntimeState_InspectCamera(
                     g_super.m_context, &telemetry)
             ? telemetry.transformFrames
             : 0u;
}

unsigned int RecoveredGameServices_VehicleDeathCameraFrameCount() {
  SRecoveredVehicleCameraTelemetry telemetry = {};
  return g_super.m_context != nullptr &&
                 VehicleRuntimeState_InspectCamera(
                     g_super.m_context, &telemetry)
             ? telemetry.deathFrames
             : 0u;
}

unsigned int RecoveredGameServices_VehicleDeathCameraCompletions() {
  SRecoveredVehicleCameraTelemetry telemetry = {};
  return g_super.m_context != nullptr &&
                 VehicleRuntimeState_InspectCamera(
                     g_super.m_context, &telemetry)
             ? telemetry.deathCompletions
             : 0u;
}

double RecoveredGameServices_VehicleDeathCameraOffsetY() {
  SRecoveredVehicleCameraTelemetry telemetry = {};
  return g_super.m_context != nullptr &&
                 VehicleRuntimeState_InspectCamera(
                     g_super.m_context, &telemetry)
             ? telemetry.deathOffsetY
             : 0.0;
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
         RecoveredGameServices_DebugMapReady() &&
         RecoveredGameServices_SeanceReady() &&
         RecoveredGameServices_BirdAttributesReady() &&
         RecoveredGameServices_PortalReady() &&
         RecoveredGameServices_OrphanAttributesReady() &&
         RecoveredGameServices_OrphanReferencesReady() &&
         RecoveredGameServices_OrphanSubjectReady() &&
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
         RecoveredGameServices_TaxiSubjectReady() &&
         RecoveredGameServices_TaxiVehicleTransitionReady() &&
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
         RecoveredGameServices_PeopleAttributesReady() &&
         RecoveredGameServices_PeopleReferencesReady() &&
         RecoveredGameServices_PeopleSubjectReady() &&
         RecoveredGameServices_TankCannonAttributesReady() &&
         RecoveredGameServices_TankReferencesReady() &&
         RecoveredGameServices_TankCannonSubjectTablesReady() &&
         RecoveredGameServices_VehicleReady() &&
         RecoveredGameServices_VehicleMovementReady() &&
         RecoveredGameServices_VehicleControlReplayReady() &&
         (RecoveredGameServices_VehicleControlReady() ||
          RecoveredGameServices_VehicleFallbackActive()) &&
         RecoveredGameLevel_IsReady() && Frame_RuntimeReady(false);
}

unsigned int RecoveredGameServices_Issues() { return g_issues; }

const SRecoveredObserverState* RecoveredGameServices_ObserverState() {
  return g_sessionReady ? &g_observerInput.state() : nullptr;
}

namespace {

bool ExplicitWorldCommandPending() {
  return PortalActiveWorldState_TransitionPending() ||
      g_debugMenuState.pending || g_debugLevelSwitchRequest.ready ||
      g_campaignRestartState.pending ||
      g_campaignRestartState.coordinatorPending ||
      g_campaignRestartRequest.ready || g_saveMenuState.pending ||
      g_crossLevelLoadRequest.ready ||
      g_saveMenuState.crossLevelRestartPending ||
      g_scriptedLevelTransitionRequest.ready;
}

bool StageRecruitCenterLevelTransition() {
  int targetLevelIndex = -1;
  const unsigned int requestOrdinal =
      g_scriptedLevelTransitionState.requests + 1u;
  if (!RecruitCenterSubjectState_PeekLevelTransition(
          &targetLevelIndex)) {
    g_scriptedLevelTransitionState.lastError =
        "scripted Level transition lost its authored target";
    return false;
  }
  g_scriptedLevelTransitionState.requests = requestOrdinal;
  g_scriptedLevelTransitionState.lastTargetLevelIndex = targetLevelIndex;
  g_scriptedLevelTransitionState.sourceLevel =
      ContinuationLevelIdentity();
  g_scriptedLevelTransitionState.targetLevel.clear();
  g_scriptedLevelTransitionState.lastError.clear();
  if (ExplicitWorldCommandPending()) {
    RecruitCenterSubjectState_RejectLevelTransition();
    ++g_scriptedLevelTransitionState.failedTransitions;
    g_scriptedLevelTransitionState.lastError =
        "scripted Level transition conflicted with another world command";
    return false;
  }

  // Convert the checkpoint-local request into process ownership before
  // capturing the source continuation. RejectLevelTransition deliberately
  // rearms the still-active terminal node, so a failed target activation can
  // restore the exact pre-trigger LCN1 bytes and retry on a later entry.
  if (!RecruitCenterSubjectState_RejectLevelTransition()) {
    ++g_scriptedLevelTransitionState.failedTransitions;
    g_scriptedLevelTransitionState.lastError =
        "scripted Level transition could not rearm its source checkpoint";
    return false;
  }

  std::vector<std::uint8_t> source;
  SLevelContinuationSummary sourceSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &source, &sourceSummary) || !sourceSummary.ready) {
    const std::string detail =
        RecoveredGameServices_LastLevelContinuationError();
    ++g_scriptedLevelTransitionState.failedTransitions;
    g_scriptedLevelTransitionState.lastError = detail.empty()
        ? "scripted Level transition source capture failed" : detail;
    return false;
  }
  g_scriptedLevelTransitionRequest = {};
  g_scriptedLevelTransitionRequest.ready = true;
  g_scriptedLevelTransitionRequest.requestOrdinal = requestOrdinal;
  g_scriptedLevelTransitionRequest.completedBefore =
      g_scriptedLevelTransitionState.completedTransitions;
  g_scriptedLevelTransitionRequest.failuresBefore =
      g_scriptedLevelTransitionState.failedTransitions;
  g_scriptedLevelTransitionRequest.rollbacksBefore =
      g_scriptedLevelTransitionState.rollbacks;
  g_scriptedLevelTransitionRequest.rollbackFailuresBefore =
      g_scriptedLevelTransitionState.rollbackFailures;
  g_scriptedLevelTransitionRequest.targetLevelIndex = targetLevelIndex;
  g_scriptedLevelTransitionRequest.sourceLevel =
      ContinuationLevelIdentity();
  g_scriptedLevelTransitionRequest.sourceContinuation = std::move(source);
  g_scriptedLevelTransitionRequest.sourceContinuationSummary = sourceSummary;
  return true;
}

}  // namespace

int RecoveredGameServices_RunFrame() {
  if (!RecoveredGameServices_IsReady()) {
    Report(RECOVERED_GAME_SERVICES_BEGIN_LOOP_FAILURE);
    return FALSE;
  }
  typedef std::chrono::steady_clock FrameClock;
  const FrameClock::time_point frameStart = FrameClock::now();
  if (!PumpMessages()) return FALSE;
  const FrameClock::time_point inputEnd = FrameClock::now();
  const bool shellPaused = g_inGameShellState.open;
  if (shellPaused) {
    // The session poll is intentionally skipped while the shell is open.
    // Rebase the timer sample owner as well, otherwise the first unpaused
    // frame inherits the complete menu dwell and violates continuation
    // clock invariants even though no simulation tick owned that interval.
    RebasePausedRuntimeClock();
  }
  bool vehicleFrame = g_vehicleControlReady && !shellPaused;
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
  if (!FlushPendingWindowsInput(CurrentInputEventTime())) {
    if (!ActivateVehicleFallback(3)) return FALSE;
    vehicleFrame = false;
  }
  if (!shellPaused) {
    SUA_ProcessEvents();
    // Observe the real post-event roster once per rendered frame.  This keeps
    // diagnostics out of the encoding-preserved People implementation and
    // distinguishes an advancing MOVE deadline from actual displacement.
    PeopleSubjectState_SampleLiveCombat(g_super.m_context);
    ObserveDebugTaxiSettlements();
  }
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
        g_vehicleControlInput.ObserveVehicleHandoff();
        UpdatePrimaryFireTelemetry(g_super.m_context);
      }
    }
  }
  if (!vehicleFrame && !shellPaused)
    g_observerInput.Advance(Session::m_frameSec);
  if (!shellPaused &&
      !RecruitCenterSubjectState_PollCheckpoints(g_super.m_context)) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }
  const FrameClock::time_point simulationEnd = FrameClock::now();

  Frame_ClearRuntimeIssues();
  // Own the complete physical framebuffer at the recovered loop boundary.
  // Viewport-local clears performed by the scene remain valid, while pixels
  // outside a future cockpit/secondary viewport can never retain an old frame.
  if (!GRSoftwareBeginFrame(GRFillColor(0, 0, 0)) || !GRStartScene()) {
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
  if ((g_issues & RECOVERED_GAME_SERVICES_DEBUG_MAP_RENDER_FAILURE) != 0) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }
  if (!g_debugMap.IsActive()) {
    ZAV_RenderFrame(&direction, dynamics);
    if (g_vehicleControlReady && g_vehicle != nullptr)
      g_vehicle->drawPanel();
  } else {
    SGRViewport* oldViewport = GRGetViewport();
    const SRecoveredLevelSettings* settings =
        RecoveredLevelRuntime_Settings();
    if (oldViewport == nullptr || g_debugMap.m_vPort == nullptr ||
        settings == nullptr) {
      Report(RECOVERED_GAME_SERVICES_DEBUG_MAP_RENDER_FAILURE);
      return FALSE;
    }
    const double aspect = RecoveredSoftwareGraph_Height() * 4.0 / 3.0 /
                          RecoveredSoftwareGraph_Width();
    const double mapFocus = g_debugMap.m_winDx * 5.0 / 8.0;
    ZAV_Scene()->SetScale(mapFocus, mapFocus * aspect);
    CViewObject::SetClipRect(g_debugMap.m_vPort->clipRect);
    GRSetViewport(g_debugMap.m_vPort);
    ZAV_RenderFrame(&direction, dynamics);
    ZAV_Scene()->SetScale(settings->focus, settings->focus * aspect);
    CViewObject::SetClipRect(oldViewport->clipRect);
    GRSetViewport(oldViewport);
  }
  DrawInGameShell();
  ZAV_PrintFrameInfo();
  SUA_EndRender(ZAV_Scene());
  ZAV_EndRenderFrame();
  ZAV_NextFrame();
  const FrameClock::time_point renderEnd = FrameClock::now();

  if (Frame_RuntimeIssues() != 0 || !GRDumpScreen()) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }
  const FrameClock::time_point presentEnd = FrameClock::now();
  RecordPrimaryFireRenderFrame();
  bool missionCommandBoundaryUsed = false;
  if (RecruitCenterSubjectState_CheckpointPending()) {
    missionCommandBoundaryUsed = true;
    if (ExplicitWorldCommandPending()) {
      if (!RecruitCenterSubjectState_DeferPendingCheckpoint()) {
        PresentClosedFrameCommandFailure(
            "Mission checkpoint", RecruitCenterSubjectState_LastError(),
            ShowNativeMissionCheckpointFailure);
      }
    } else if (!RecruitCenterSubjectState_ProcessPendingCheckpoint(
                   g_super.m_context, Session::m_viewTime)) {
      PresentClosedFrameCommandFailure(
          "Mission checkpoint", RecruitCenterSubjectState_LastError(),
          ShowNativeMissionCheckpointFailure);
    }
  }
  if (RecruitCenterSubjectState_ReachedScriptPending()) {
    if (ExplicitWorldCommandPending() || missionCommandBoundaryUsed ||
        RecruitCenterSubjectState_LevelTransitionPending()) {
      if (!RecruitCenterSubjectState_DeferPendingReachedScript(
              g_super.m_context)) {
        PresentClosedFrameCommandFailure(
            "Mission reached script",
            RecruitCenterSubjectState_LastError(),
            ShowNativeMissionCheckpointFailure);
      }
    } else if (!RecruitCenterSubjectState_ProcessPendingReachedScript(
                   g_super.m_context, Session::m_viewTime)) {
      PresentClosedFrameCommandFailure(
          "Mission reached script", RecruitCenterSubjectState_LastError(),
          ShowNativeMissionCheckpointFailure);
    }
  }
  if (RecruitCenterSubjectState_LevelTransitionPending() &&
      !StageRecruitCenterLevelTransition()) {
    PresentClosedFrameCommandFailure(
        "Mission Level transition",
        g_scriptedLevelTransitionState.lastError,
        ShowNativeScriptedLevelTransitionFailure);
  }
  // Debug mutations share the same fully closed boundary as save/load and
  // are processed first so a later save request can only observe a committed
  // debug world. WM_COMMAND itself merely stages the operation.
  if (g_debugMenuState.pending &&
      !RecoveredGameServices_ProcessPendingDebugCommand() &&
      !g_debugMenuState.pending) {
    PresentClosedFrameCommandFailure(
        "Developer", g_debugMenuState.lastError, ShowNativeDebugFailure);
  }
  if (g_campaignRestartState.pending &&
      !RecoveredGameServices_ProcessPendingCampaignRestart() &&
      !g_campaignRestartState.pending) {
    PresentClosedFrameCommandFailure(
        "Restart", g_campaignRestartState.lastError,
        ShowNativeCampaignRestartFailure);
  }
  // Save/load owns the last boundary of a fully simulated, rendered and
  // presented frame. In particular, every drawable Subject has received its
  // endRender callback before LCN1 attempts to capture the live-world backup.
  // A transient owner-publication failure is retained as one pending command
  // and retried on a later closed frame; only a terminal failure reaches UI.
  if (g_saveMenuState.pending &&
      !RecoveredGameServices_ProcessPendingSaveCommand() &&
      !g_saveMenuState.pending) {
    PresentClosedFrameCommandFailure(
        "Save/load", g_saveMenuState.lastError, ShowNativeSaveFailure);
  }
  if (!ProcessPendingInGameShellVideoCommand()) {
    Report(RECOVERED_GAME_SERVICES_IN_GAME_SHELL_FAILURE);
  }
  const FrameClock::time_point boundaryEnd = FrameClock::now();
  const std::uint64_t inputMicroseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          inputEnd - frameStart).count());
  const std::uint64_t simulationMicroseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          simulationEnd - inputEnd).count());
  const std::uint64_t renderMicroseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          renderEnd - simulationEnd).count());
  const std::uint64_t presentMicroseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          presentEnd - renderEnd).count());
  const std::uint64_t boundaryMicroseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          boundaryEnd - presentEnd).count());
  const std::uint64_t totalMicroseconds = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          boundaryEnd - frameStart).count());
  ++g_frameTimingTelemetry.frames;
  g_frameTimingTelemetry.totalMicroseconds += totalMicroseconds;
  g_frameTimingTelemetry.maximumFrameMicroseconds = (std::max)(
      g_frameTimingTelemetry.maximumFrameMicroseconds, totalMicroseconds);
  g_frameTimingTelemetry.inputMicroseconds += inputMicroseconds;
  g_frameTimingTelemetry.maximumInputMicroseconds = (std::max)(
      g_frameTimingTelemetry.maximumInputMicroseconds, inputMicroseconds);
  g_frameTimingTelemetry.simulationMicroseconds += simulationMicroseconds;
  g_frameTimingTelemetry.maximumSimulationMicroseconds = (std::max)(
      g_frameTimingTelemetry.maximumSimulationMicroseconds,
      simulationMicroseconds);
  g_frameTimingTelemetry.renderMicroseconds += renderMicroseconds;
  g_frameTimingTelemetry.maximumRenderMicroseconds = (std::max)(
      g_frameTimingTelemetry.maximumRenderMicroseconds, renderMicroseconds);
  g_frameTimingTelemetry.presentMicroseconds += presentMicroseconds;
  g_frameTimingTelemetry.maximumPresentMicroseconds = (std::max)(
      g_frameTimingTelemetry.maximumPresentMicroseconds,
      presentMicroseconds);
  g_frameTimingTelemetry.boundaryMicroseconds += boundaryMicroseconds;
  g_frameTimingTelemetry.maximumBoundaryMicroseconds = (std::max)(
      g_frameTimingTelemetry.maximumBoundaryMicroseconds,
      boundaryMicroseconds);
  return TRUE;
}
