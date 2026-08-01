#ifndef RR2NW_VEHICLE_RUNTIME_STATE_H
#define RR2NW_VEHICLE_RUNTIME_STATE_H

#include "mathlib.h"
#include "kernel/h/krtypes.h"

class SimulationContext;

enum ERecoveredVehicleVesselKind
{
    RECOVERED_VEHICLE_VESSEL_UNKNOWN = 0,
    RECOVERED_VEHICLE_VESSEL_EMV = 1,
    RECOVERED_VEHICLE_VESSEL_WHEELS = 2
};

enum ERecoveredVehicleStabilityReason
{
    RECOVERED_VEHICLE_STABILITY_NONE = 0,
    RECOVERED_VEHICLE_STABILITY_NONFINITE = 1,
    RECOVERED_VEHICLE_STABILITY_EXCESSIVE_SPEED = 2,
    RECOVERED_VEHICLE_STABILITY_EXCESSIVE_DISPLACEMENT = 3,
    RECOVERED_VEHICLE_STABILITY_INVALID_DIRECTION = 4,
    RECOVERED_VEHICLE_STABILITY_RESTORE_FAILED = 5
};

enum ERecoveredVehicleCameraMode
{
    RECOVERED_VEHICLE_CAMERA_UNKNOWN = 0,
    RECOVERED_VEHICLE_CAMERA_LIVE = 1,
    RECOVERED_VEHICLE_CAMERA_TAXI = 2,
    RECOVERED_VEHICLE_CAMERA_DEATH_ASCENT = 3,
    RECOVERED_VEHICLE_CAMERA_DEATH_COMPLETE = 4
};

struct SRecoveredVehicleCameraTelemetry
{
    int mode;
    unsigned int transformFrames;
    unsigned int taxiFrames;
    unsigned int deathFrames;
    unsigned int deathCompletions;
    double deathOffsetY;
};

struct SRecoveredVehicleRuntimeState
{
    KR_ObjectID object;
    KR_ObjectID attribute;
    CFVector3 position;
    CFVector3 subjectPosition;
    CFVector3 speed;
    CFMatrix3x4 direction;
    double mass;
    double lastTime;
    int vesselKind;
    int active;
    int frameBegun;
    int advanceCount;
    int controlEventCount;
    int lastBumpFlags;
    int touchingGround;
    int groundContactFrameCount;
    int staticCollisionFrameCount;
    int landCollisionFrameCount;
    int dynamicCollisionFrameCount;
    int stabilityRecoveryCount;
    int lastStabilityReason;
    int dead;
    int takingTaxi;
};

struct SRecoveredVehicleMovementProbeSummary
{
    int invalidActivationRejections;
    int activations;
    int stationarySteps;
    int throttleEvents;
    int movementSteps;
    int turnEvents;
    int cameraTransitions;
    int stabilityRecoveries;
    int rollbacks;
    double horizontalDistance;
};

struct SRecoveredVehicleDeathCameraProbeSummary
{
    int activations;
    int ascentFrames;
    int terminalFrames;
    int completionTransitions;
    int finiteCameras;
    int rollbacks;
};

struct SRecoveredVehicleStabilityTelemetry
{
    int recoveryCount;
    int lastReason;
    int vesselKind;
    int bumpFlags;
    int touchingGround;
    double frameStartTime;
    double rejectedTime;
    double requestedTargetTime;
    CFVector3 frameStartPosition;
    CFVector3 frameStartSpeed;
    CFVector3 rejectedPosition;
    CFVector3 rejectedSpeed;
    double groundX;
    double groundY;
    double groundZ;
    double groundLength;
    double forwardTangentLength;
    double rightTangentLength;
    double tangentDot;
    double suspensionTravel;
    double accelerationFactor;
    double throttle;
};

void VehicleRuntimeState_Link();
bool VehicleRuntimeState_IsClean(SimulationContext *context);
bool VehicleRuntimeState_Inspect(
    SimulationContext *context, const KR_ObjectID &vehicle,
    SRecoveredVehicleRuntimeState *state);
bool VehicleRuntimeState_InspectStability(
    SimulationContext *context,
    SRecoveredVehicleStabilityTelemetry *telemetry);
bool VehicleRuntimeState_InspectCamera(
    SimulationContext *context,
    SRecoveredVehicleCameraTelemetry *telemetry);
const char *VehicleRuntimeState_AttributeName(
    SimulationContext *context, const KR_ObjectID &vehicle);
const char *VehicleRuntimeState_DynamicName(
    SimulationContext *context, const KR_ObjectID &vehicle);
unsigned long long VehicleRuntimeState_IdentityFingerprint(
    SimulationContext *context, const KR_ObjectID &vehicle);
bool VehicleRuntimeState_IsKnownRetailIdentity(
    SimulationContext *context, const KR_ObjectID &vehicle);
bool VehicleRuntimeState_Activate(
    SimulationContext *context, const KR_ObjectID &vehicle,
    const CFVector3 &position, double startTime);
bool VehicleRuntimeState_ApplyControl(
    SimulationContext *context, int action, double down);
bool VehicleRuntimeState_ApplyControlAt(
    SimulationContext *context, int action, double down,
    double eventTime);
bool VehicleRuntimeState_ApplyLiveControlAt(
    SimulationContext *context, int action, double down,
    double eventTime);
int VehicleRuntimeState_LastControlFailure();
double VehicleRuntimeState_LastAppliedControlTime();
int VehicleRuntimeState_LastFrameFailure();
int VehicleRuntimeState_LastFrameReadinessIssue();
bool VehicleRuntimeState_SynchronizeFirstFrame(
    SimulationContext *context, double startTime);
// Rebind the modern live-control owner to a Vehicle restored in place by the
// active-world transaction. This preserves controller ownership and counters
// while adopting the authoritative restored frame boundary.
bool VehicleRuntimeState_RebaseRestoredOwner(SimulationContext *context);
// Debug tooling may call this only after a completely closed frame. It uses
// the runtime owner's proven last-stable pose and leaves the same Vehicle
// object/attribute under live-control ownership.
bool VehicleRuntimeState_DebugStabilize(SimulationContext *context);
bool VehicleRuntimeState_DebugKill(
    SimulationContext *context, double eventTime);
bool VehicleRuntimeState_BeginFrame(SimulationContext *context);
bool VehicleRuntimeState_CompleteFrame(
    SimulationContext *context, double targetTime);
bool VehicleRuntimeState_CompleteLiveFrame(
    SimulationContext *context, double targetTime,
    bool *droppedTime);
bool VehicleRuntimeState_Advance(
    SimulationContext *context, double targetTime);
bool VehicleRuntimeState_BuildCamera(
    SimulationContext *context, CFMatrix3x4 *direction);
bool VehicleRuntimeState_LastStablePosition(
    SimulationContext *context, CFVector3 *position);
bool VehicleRuntimeState_Rollback(SimulationContext *context);
void VehicleRuntimeState_Reset(SimulationContext *context);
bool VehicleRuntimeState_ProbeMovement(
    SimulationContext *context, const KR_ObjectID &vehicle,
    const CFVector3 &position, double startTime,
    SRecoveredVehicleMovementProbeSummary *summary);
bool VehicleRuntimeState_ProbeDeathCamera(
    SimulationContext *context, const KR_ObjectID &vehicle,
    const CFVector3 &position, double startTime,
    SRecoveredVehicleDeathCameraProbeSummary *summary);

#endif
