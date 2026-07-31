#include "RecoveredGameServicesRuntime.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <new>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#define LAST_H__SCENE
#include "game.h"
#include "dmap.h"
#include "graph.h"
#include "hardware.h"
#include "h/super.h"
#include "h/vehicle.h"
#include "kernel/h/session.h"
#include "message/hardmsg.h"
#include "suavik.h"
#include "zav.h"

#include "FrameRuntimeState.h"
#include "GameEntryRuntimeState.h"
#include "LevelContinuation.h"
#include "LevelSaveSlot.h"
#include "RecoveredArenaSeanceRuntime.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredFramePreview.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredRetailScriptManifest.h"
#include "RecoveredSoftwareFrame.h"
#include "RecoveredSoftwareGraph.h"
#include "SupervisorShutdownState.h"
#include "TimeRuntimeState.h"
#include "VehicleControlJournal.h"
#include "VehicleControlReplayProbe.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/orphan/OrphanSubjectState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/spark/SparkSubjectState.h"
#include "obase/taxi/TaxiSubjectState.h"
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

    if (action == FIRE_PRIMARY && down > 0.0)
      ++m_primaryFirePresses;

    STaxiVehicleProximityState proximity = {};
    SRecoveredVehicleRuntimeState before = {};
    Vehicle* currentVehicle = getContext() == nullptr || m_vehicle.isNUL() ?
        nullptr : static_cast<Vehicle*>(
            getContext()->queryInterface(m_vehicle, IVehicleIID));
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
    if (!CanAdoptControlJournal(journal)) return false;
    SSimulationClockState clock = {};
    bool active = false;
    double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
    SVehicleControlJournal resumed = journal;
    if (!SUA_CaptureSimulationClock(&clock) ||
        clock.tick != journal.finalTick ||
        (std::max)(clock.eventMoment, clock.viewTime) != journal.finalTime ||
        !VehicleControlJournal_DeriveLifecycle(
            journal, &active, held) ||
        !VehicleRuntimeState_RebaseRestoredOwner(getContext()) ||
        !VehicleControlJournal_Resume(&resumed))
      return false;
    m_controlJournal = resumed;
    m_controlJournalRecording = true;
    m_controlJournalAppendFailures = 0;
    m_applicationActive = active;
    for (std::size_t index = 0;
         index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index)
      m_heldActions[index] = held[index];
    return true;
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
  unsigned int ActiveActionCount() const {
    unsigned int count = 0;
    for (double value : m_heldActions) {
      if (value != 0.0) ++count;
    }
    return count;
  }
  int LastInputFailure() const { return m_lastInputFailure; }
  unsigned int PrimaryFirePresses() const { return m_primaryFirePresses; }

  void ObserveVehicleHandoff() {
    if (getContext() == nullptr || m_vehicle.isNUL()) return;
    SRecoveredVehicleRuntimeState state = {};
    if (!VehicleRuntimeState_Inspect(getContext(), m_vehicle, &state)) return;
    Vehicle* vehicle = static_cast<Vehicle*>(
        getContext()->queryInterface(m_vehicle, IVehicleIID));
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
  static constexpr int kHeldActionCount = 11;

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
      default: return -1;
    }
  }

  static int HeldAction(int index) {
    static const int actions[kHeldActionCount] = {
        MOVE_FORWARD, MOVE_BACKWARD, STRAFE_LEFT, STRAFE_RIGHT,
        STRAFE_UP, STRAFE_DOWN, TURN_LEFT, TURN_RIGHT,
        LOOK_UP, LOOK_DOWN, FIRE_PRIMARY};
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
STaxiVehicleTransitionProbeSummary g_taxiVehicleTransitionProbe = {};
SRecoveredVehicleControlReplayProbeSummary g_vehicleControlReplayProbe = {};
SRecoveredVehicleDriveTelemetry g_vehicleDriveTelemetry = {};
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
std::string g_levelSaveSlotFailure;
SRecoveredSaveMenuState g_saveMenuState;
bool g_saveMenuAllowOverwrite = false;
HMENU g_nativeMenuBar = nullptr;
HMENU g_nativeGameMenu = nullptr;
HMENU g_nativeSaveMenu = nullptr;
HMENU g_nativeLoadMenu = nullptr;
RecoveredObserverInput g_observerInput;
RecoveredVehicleControlInput g_vehicleControlInput;

void Report(unsigned int issue) { g_issues |= issue; }

std::uint64_t ContinuationContentFingerprint() {
  const SRecoveredRetailScriptManifestSummary* manifest =
      RecoveredRetailScriptManifest_IsReady()
          ? RecoveredRetailScriptManifest_Summary()
          : nullptr;
  if (manifest != nullptr && manifest->contentFingerprint != 0)
    return manifest->contentFingerprint;
  return RecoveredArenaSeance_ActiveWorldFingerprint();
}

std::string ContinuationLevelIdentity() {
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
  RECT outer = {0, 0, _gr_nScreenWidth, _gr_nScreenHeight};
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

bool SlotIsCompatible(const SLevelSaveSlot& archive) {
  return archive.level == ContinuationLevelIdentity() &&
         archive.contentFingerprint == ContinuationContentFingerprint();
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
    const bool compatible = readable && SlotIsCompatible(archive);
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
      if (!compatible) label += L" - incompatible";
    }
    const UINT saveCommand = kNativeSaveSlotBase + slot;
    const UINT loadCommand = kNativeLoadSlotBase + slot;
    ModifyMenuW(g_nativeSaveMenu, saveCommand,
                MF_BYCOMMAND | MF_STRING, saveCommand, label.c_str());
    ModifyMenuW(g_nativeLoadMenu, loadCommand,
                MF_BYCOMMAND | MF_STRING, loadCommand, label.c_str());
    EnableMenuItem(g_nativeLoadMenu, loadCommand,
                   MF_BYCOMMAND |
                       (compatible ? MF_ENABLED
                                   : MF_GRAYED | MF_DISABLED));
  }
  if (_gr_hWnd != nullptr) DrawMenuBar(_gr_hWnd);
}

void DestroyNativeSaveMenu() {
  if (g_nativeMenuBar == nullptr) {
    g_saveMenuState.nativeMenuInstalled = false;
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
  g_saveMenuState.nativeMenuInstalled = false;
}

bool InstallNativeSaveMenu() {
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
  if (SetMenu(_gr_hWnd, menuBar) == FALSE) {
    return failConstruction("SetMenu failed");
  }
  g_nativeMenuBar = menuBar;
  g_nativeGameMenu = gameMenu;
  g_nativeSaveMenu = saveMenu;
  g_nativeLoadMenu = loadMenu;
  g_saveMenuState.nativeMenuInstalled = true;
  ResizeSoftwareWindowForMenu(true);
  RefreshNativeSaveMenu();
  return true;
}

void ResetSaveMenuSession() {
  DestroyNativeSaveMenu();
  g_saveMenuState.pending = false;
  g_saveMenuState.pendingAction = RECOVERED_SAVE_MENU_NONE;
  g_saveMenuState.pendingSlot = 0;
  g_saveMenuState.saveRequests = 0;
  g_saveMenuState.loadRequests = 0;
  g_saveMenuState.completedSaves = 0;
  g_saveMenuState.completedLoads = 0;
  g_saveMenuState.failedCommands = 0;
  g_saveMenuState.lastError.clear();
  g_saveMenuState.lastPreview = {};
  g_saveMenuState.lastSlot = {};
  g_saveMenuState.lastContinuation = {};
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

bool HandleNativeSaveMenuMessage(HWND window, UINT message,
                                 WPARAM wParam, LRESULT* result) {
  if (message == WM_INITMENUPOPUP && g_nativeMenuBar != nullptr) {
    RefreshNativeSaveMenu();
    *result = 0;
    return true;
  }
  if (message != WM_COMMAND || HIWORD(wParam) != 0) return false;
  const UINT command = LOWORD(wParam);
  if (command >= kNativeSaveSlotBase &&
      command < kNativeSaveSlotBase + LevelSaveSlot_Count()) {
    const std::uint32_t slot = command - kNativeSaveSlotBase;
    const std::wstring path =
        LevelSaveSlot_Path(g_saveMenuState.directory, slot);
    const bool occupied =
        !path.empty() &&
        GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (occupied) {
      const std::wstring prompt =
          L"Replace save slot " + std::to_wstring(slot + 1u) + L"?";
      if (MessageBoxW(window, prompt.c_str(), L"RR2NW save game",
                      MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        *result = 0;
        return true;
      }
    }
    if (!RecoveredGameServices_RequestSaveSlot(slot, occupied))
      ShowNativeSaveFailure();
    *result = 0;
    return true;
  }
  if (command >= kNativeLoadSlotBase &&
      command < kNativeLoadSlotBase + LevelSaveSlot_Count()) {
    const std::uint32_t slot = command - kNativeLoadSlotBase;
    const std::wstring prompt =
        L"Load save slot " + std::to_wstring(slot + 1u) +
        L"?\n\nUnsaved progress will be replaced.";
    if (MessageBoxW(window, prompt.c_str(), L"RR2NW load game",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES &&
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
      !BindHardwareControl(STOP_VEHICLE, "X") ||
      !BindHardwareControl(CHANGE_VEHICLE, "F1") ||
      !BindHardwareControl(EXIT, "Esc")) {
    return false;
  }
  g_hardware.Link();
  return true;
}

LRESULT ForwardWindowMessageToHardware(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
  LRESULT saveMenuResult = 0;
  if (HandleNativeSaveMenuMessage(window, message, wParam,
                                  &saveMenuResult))
    return saveMenuResult;
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
  ResetSaveMenuSession();
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
    const bool replayProbeReady = VehicleControlReplayProbe_Run(
            g_super.m_context, vehicle, observerPosition,
            vehicleStartTime, &g_vehicleControlReplayProbe);
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
  telemetry->recordedStateFingerprint =
      g_vehicleControlReplayProbe.recordedStateFingerprint;
  telemetry->replayedStateFingerprint =
      g_vehicleControlReplayProbe.replayedStateFingerprint;
  telemetry->encodedBytes = g_vehicleControlReplayProbe.encodedBytes;
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
  return true;
}

bool RecoveredGameServices_VehicleControlJournalTelemetry(
    SRecoveredVehicleControlJournalTelemetry* telemetry) {
  return g_vehicleControlReady &&
         g_vehicleControlInput.ControlJournalTelemetry(telemetry);
}

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
          &backupBytes, &backupSummary))
    return false;

  SVehicleControlJournal restoredJournal;
  SLevelContinuationSummary restoredSummary;
  std::string failure;
  const std::uint64_t contentFingerprint =
      ContinuationContentFingerprint();
  const std::string level = ContinuationLevelIdentity();
  if (LevelContinuation_RestoreWorld(
          g_super.m_context, bytes, contentFingerprint, level,
          &restoredJournal,
          &restoredSummary, &failure) &&
      g_vehicleControlInput.AdoptControlJournal(restoredJournal)) {
    *summary = restoredSummary;
    g_levelContinuationFailure.clear();
    return true;
  }
  g_levelContinuationFailure = failure.empty()
                                   ? "restored CTJ1 adoption failed"
                                   : failure;

  SVehicleControlJournal backupJournal;
  SLevelContinuationSummary rolledBackSummary;
  std::string rollbackFailure;
  const bool rolledBack = LevelContinuation_RestoreWorld(
      g_super.m_context, backupBytes, contentFingerprint, level,
      &backupJournal,
      &rolledBackSummary, &rollbackFailure);
  const bool controlRolledBack =
      rolledBack && g_vehicleControlInput.AdoptControlJournal(backupJournal);
  if (!controlRolledBack) {
    g_levelContinuationFailure += rolledBack
        ? "; backup CTJ1 adoption failed"
        : "; backup world restore failed: " + rollbackFailure;
  }
  return false;
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

bool RecoveredGameServices_ConfigureSaveDirectory(
    const std::wstring& directory) {
  if (directory.empty() ||
      directory.find(L'\0') != std::wstring::npos ||
      LevelSaveSlot_Path(directory, 0u).empty()) {
    g_saveMenuState.lastError = "save directory is invalid";
    return false;
  }
  if (g_saveMenuState.pending) {
    g_saveMenuState.lastError =
        "save directory cannot change while a command is pending";
    return false;
  }
  g_saveMenuState.directory = directory;
  g_saveMenuState.configured = true;
  g_saveMenuState.lastError.clear();
  if (g_sessionReady && _gr_hWnd != nullptr &&
      !InstallNativeSaveMenu()) {
    Report(RECOVERED_GAME_SERVICES_SAVE_MENU_FAILURE);
    return false;
  }
  RefreshNativeSaveMenu();
  return true;
}

bool RecoveredGameServices_RequestSaveSlot(
    std::uint32_t slot, bool allowOverwrite) {
  g_saveMenuState.lastError.clear();
  if (!g_saveMenuState.configured) {
    g_saveMenuState.lastError = "save directory is not configured";
    return false;
  }
  if (slot >= LevelSaveSlot_Count()) {
    g_saveMenuState.lastError = "save slot index is outside 0..7";
    return false;
  }
  if (g_saveMenuState.pending) {
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
  g_saveMenuAllowOverwrite = allowOverwrite;
  ++g_saveMenuState.saveRequests;
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
  if (g_saveMenuState.pending) {
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
  if (!SlotIsCompatible(archive)) {
    g_saveMenuState.lastError =
        "save slot belongs to a different Level or retail data set";
    return false;
  }
  g_saveMenuState.pending = true;
  g_saveMenuState.pendingAction = RECOVERED_SAVE_MENU_LOAD;
  g_saveMenuState.pendingSlot = slot;
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
  g_saveMenuState.pending = false;
  g_saveMenuState.pendingAction = RECOVERED_SAVE_MENU_NONE;
  g_saveMenuState.pendingSlot = 0;
  g_saveMenuAllowOverwrite = false;
  g_saveMenuState.lastError.clear();
  g_saveMenuState.lastPreview = {};
  g_saveMenuState.lastSlot = {};
  g_saveMenuState.lastContinuation = {};

  SLevelSaveSlotSummary completedSlot;
  SLevelContinuationSummary completedContinuation;
  bool completed = false;
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
            g_saveMenuState.directory, slot, BuildAutomaticSaveTitle(),
            "RR2NW recovered Windows menu checkpoint", preview,
            &completedSlot, &completedContinuation);
        if (!completed)
          g_saveMenuState.lastError =
              RecoveredGameServices_LastLevelSaveSlotError();
      }
    }
  } else if (action == RECOVERED_SAVE_MENU_LOAD) {
    completed = RecoveredGameServices_LoadLevelSlot(
        g_saveMenuState.directory, slot, &completedSlot,
        &completedContinuation);
    if (!completed)
      g_saveMenuState.lastError =
          RecoveredGameServices_LastLevelSaveSlotError();
  } else {
    g_saveMenuState.lastError = "pending save/load action is invalid";
  }

  if (!completed) {
    ++g_saveMenuState.failedCommands;
    RefreshNativeSaveMenu();
    return false;
  }
  g_saveMenuState.lastSlot = completedSlot;
  g_saveMenuState.lastContinuation = completedContinuation;
  if (action == RECOVERED_SAVE_MENU_SAVE)
    ++g_saveMenuState.completedSaves;
  else
    ++g_saveMenuState.completedLoads;
  if (slotSummary != nullptr) *slotSummary = completedSlot;
  if (continuationSummary != nullptr)
    *continuationSummary = completedContinuation;
  RefreshNativeSaveMenu();
  return true;
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

int RecoveredGameServices_RunFrame() {
  if (!RecoveredGameServices_IsReady()) {
    Report(RECOVERED_GAME_SERVICES_BEGIN_LOOP_FAILURE);
    return FALSE;
  }
  if (!PumpMessages()) return FALSE;
  if (g_saveMenuState.pending &&
      !RecoveredGameServices_ProcessPendingSaveCommand()) {
    ShowNativeSaveFailure();
  }
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
        g_vehicleControlInput.ObserveVehicleHandoff();
        UpdatePrimaryFireTelemetry(g_super.m_context);
      }
    }
  }
  if (!vehicleFrame) g_observerInput.Advance(Session::m_frameSec);

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
  ZAV_RenderFrame(&direction, dynamics);
  if (g_vehicleControlReady && g_vehicle != nullptr)
    g_vehicle->drawPanel();
  ZAV_PrintFrameInfo();
  SUA_EndRender(ZAV_Scene());
  ZAV_EndRenderFrame();
  ZAV_NextFrame();

  if (Frame_RuntimeIssues() != 0 || !GRDumpScreen()) {
    Report(RECOVERED_GAME_SERVICES_FRAME_FAILURE);
    return FALSE;
  }
  RecordPrimaryFireRenderFrame();
  return TRUE;
}
