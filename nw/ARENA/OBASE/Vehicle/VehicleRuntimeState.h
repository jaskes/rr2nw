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
    int rollbacks;
    double horizontalDistance;
};

void VehicleRuntimeState_Link();
bool VehicleRuntimeState_IsClean(SimulationContext *context);
bool VehicleRuntimeState_Inspect(
    SimulationContext *context, const KR_ObjectID &vehicle,
    SRecoveredVehicleRuntimeState *state);
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
int VehicleRuntimeState_LastFrameFailure();
int VehicleRuntimeState_LastFrameReadinessIssue();
bool VehicleRuntimeState_SynchronizeFirstFrame(
    SimulationContext *context, double startTime);
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
bool VehicleRuntimeState_Rollback(SimulationContext *context);
void VehicleRuntimeState_Reset(SimulationContext *context);
bool VehicleRuntimeState_ProbeMovement(
    SimulationContext *context, const KR_ObjectID &vehicle,
    const CFVector3 &position, double startTime,
    SRecoveredVehicleMovementProbeSummary *summary);

#endif
