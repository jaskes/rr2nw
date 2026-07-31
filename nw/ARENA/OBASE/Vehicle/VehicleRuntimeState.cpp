#include "VehicleRuntimeState.h"
#include "VehicleVesselTelemetry.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

class CGRPanel;
#include "h/vehicle.h"
#include "bumpdef.h"
#include "hardware.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/hardmsg.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const double kMaximumStep = 0.05;
const double kTimeEpsilon = 1.0e-9;
// Retail vehicle attributes top out in the low double digits. These limits
// leave orders of magnitude of headroom while rejecting finite-but-
// astronomical values produced by a runaway legacy collision response.
const double kMaximumStableSpeedComponent = 2048.0;
const double kMaximumStableFrameDisplacement = 4096.0;
const double kMaximumStableDirectionComponent = 4.0;

struct VehicleRuntimeOwner
{
    SimulationContext *context;
    Vehicle *vehicle;
    KR_ObjectID object;
    KR_ObjectID frameAttribute;
    CFVector3 savedPosition;
    CFVector3 savedSubjectPosition;
    CFVector3 savedSpeed;
    CFMatrix3x4 savedDirection;
    CFVector3 frameStartPosition;
    CFVector3 frameStartSubjectPosition;
    CFMatrix3x4 frameStartDirection;
    CFVector3 lastStablePosition;
    double savedLastTime;
    double savedCurrentTime;
    double savedViewTime;
    double lastTime;
    int advanceCount;
    int controlEventCount;
    int lastBumpFlags;
    int groundContactFrameCount;
    int staticCollisionFrameCount;
    int landCollisionFrameCount;
    int dynamicCollisionFrameCount;
    int stabilityRecoveryCount;
    int lastStabilityReason;
    bool active;
    bool frameBegun;
    bool frameStartValid;
    bool lastStableValid;

    VehicleRuntimeOwner()
        : context(NULL), vehicle(NULL), object(KR_ObjectID::NUL()),
          frameAttribute(KR_ObjectID::NUL()),
          savedPosition(0.0, 0.0, 0.0),
          savedSubjectPosition(0.0, 0.0, 0.0),
          savedSpeed(0.0, 0.0, 0.0),
          frameStartPosition(0.0, 0.0, 0.0),
          frameStartSubjectPosition(0.0, 0.0, 0.0),
          lastStablePosition(0.0, 0.0, 0.0), savedLastTime(0.0),
          savedCurrentTime(0.0), savedViewTime(0.0), lastTime(0.0),
          advanceCount(0), controlEventCount(0), lastBumpFlags(BF_NONE),
          groundContactFrameCount(0), staticCollisionFrameCount(0),
          landCollisionFrameCount(0), dynamicCollisionFrameCount(0),
          stabilityRecoveryCount(0),
          lastStabilityReason(RECOVERED_VEHICLE_STABILITY_NONE),
          active(false), frameBegun(false), frameStartValid(false),
          lastStableValid(false)
    {
        savedDirection.LoadIdentity();
        frameStartDirection.LoadIdentity();
    }
};

VehicleRuntimeOwner g_owner;
int g_lastControlFailure = 0;
double g_lastAppliedControlTime = -1.0;
int g_lastFrameFailure = 0;
int g_lastFrameReadinessIssue = 0;

bool IsNul(const KR_ObjectID &object)
{
    KR_ObjectID copy = object;
    return copy.isNUL() != FALSE;
}

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool FiniteMatrix(const CFMatrix3x4 &value)
{
    return FiniteVector(value.Row(0)) && FiniteVector(value.Row(1)) &&
           FiniteVector(value.Row(2)) && FiniteVector(value.Offset());
}

bool NearlyEqual(double left, double right, double tolerance = 1.0e-7)
{
    return std::fabs(left - right) <= tolerance;
}

bool NearlyEqual(const CFVector3 &left, const CFVector3 &right,
                 double tolerance = 1.0e-7)
{
    return NearlyEqual(left.x, right.x, tolerance) &&
           NearlyEqual(left.y, right.y, tolerance) &&
           NearlyEqual(left.z, right.z, tolerance);
}

bool NearlyEqual(const CFMatrix3x4 &left, const CFMatrix3x4 &right,
                 double tolerance = 1.0e-7)
{
    return NearlyEqual(left.Row(0), right.Row(0), tolerance) &&
           NearlyEqual(left.Row(1), right.Row(1), tolerance) &&
           NearlyEqual(left.Row(2), right.Row(2), tolerance) &&
           NearlyEqual(left.Offset(), right.Offset(), tolerance);
}

void HashBytes(unsigned long long &hash, const void *data,
               unsigned int size)
{
    const unsigned char *bytes =
        static_cast<const unsigned char *>(data);
    for (unsigned int index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= kHashPrime;
    }
}

void HashString(unsigned long long &hash, const char *value)
{
    if (value == NULL)
        return;
    HashBytes(hash, value,
              static_cast<unsigned int>(std::strlen(value) + 1));
}

void ClearOwner()
{
    g_owner.context = NULL;
    g_owner.vehicle = NULL;
    g_owner.object = KR_ObjectID::NUL();
    g_owner.frameAttribute = KR_ObjectID::NUL();
    g_owner.savedPosition = CFVector3(0.0, 0.0, 0.0);
    g_owner.savedSubjectPosition = CFVector3(0.0, 0.0, 0.0);
    g_owner.savedSpeed = CFVector3(0.0, 0.0, 0.0);
    g_owner.savedDirection.LoadIdentity();
    g_owner.frameStartPosition = CFVector3(0.0, 0.0, 0.0);
    g_owner.frameStartSubjectPosition = CFVector3(0.0, 0.0, 0.0);
    g_owner.frameStartDirection.LoadIdentity();
    g_owner.lastStablePosition = CFVector3(0.0, 0.0, 0.0);
    g_owner.savedLastTime = 0.0;
    g_owner.savedCurrentTime = 0.0;
    g_owner.savedViewTime = 0.0;
    g_owner.lastTime = 0.0;
    g_owner.advanceCount = 0;
    g_owner.controlEventCount = 0;
    g_owner.lastBumpFlags = BF_NONE;
    g_owner.groundContactFrameCount = 0;
    g_owner.staticCollisionFrameCount = 0;
    g_owner.landCollisionFrameCount = 0;
    g_owner.dynamicCollisionFrameCount = 0;
    g_owner.stabilityRecoveryCount = 0;
    g_owner.lastStabilityReason = RECOVERED_VEHICLE_STABILITY_NONE;
    g_owner.active = false;
    g_owner.frameBegun = false;
    g_owner.frameStartValid = false;
    g_owner.lastStableValid = false;
    g_lastControlFailure = 0;
}

Vehicle *ResolveVehicle(SimulationContext *context,
                        const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object) || !context->isExist(object))
        return NULL;
    return static_cast<Vehicle *>(
        context->queryInterface(object, IVehicleIID));
}

AttributeVehicle *ResolveAttribute(Vehicle *vehicle)
{
    return vehicle == NULL || IsNul(vehicle->m_vehicleAttrID)
               ? NULL
               : static_cast<AttributeVehicle *>(
                     __attrVehicleTable.searchAttribute(
                         vehicle->m_vehicleAttrID));
}

int VesselKind(const AttributeVehicle *attribute)
{
    if (attribute == NULL)
        return RECOVERED_VEHICLE_VESSEL_UNKNOWN;
    const char *dynamic = attribute->m_dynamic;
    if (std::strcmp(dynamic, "Dragon") == 0 ||
        std::strcmp(dynamic, "Emveshka") == 0)
        return RECOVERED_VEHICLE_VESSEL_EMV;
    if (std::strcmp(dynamic, "TankGenn0") == 0 ||
        std::strcmp(dynamic, "TankGenn1") == 0 ||
        std::strcmp(dynamic, "TankGenn2") == 0 ||
        std::strcmp(dynamic, "TankGenn3") == 0 ||
        std::strcmp(dynamic, "Dead") == 0)
        return RECOVERED_VEHICLE_VESSEL_WHEELS;
    return RECOVERED_VEHICLE_VESSEL_UNKNOWN;
}

int VesselBumpFlags(const AttributeVehicle *attribute)
{
    return attribute == NULL
               ? BF_NONE
               : RecoveredVehicleVesselBumpFlags(attribute->m_dynamic);
}

bool VesselTouchesGround(const AttributeVehicle *attribute)
{
    return attribute != NULL &&
           RecoveredVehicleVesselTouchesGround(attribute->m_dynamic);
}

void RecordFrameCollision()
{
    AttributeVehicle *attribute = ResolveAttribute(g_owner.vehicle);
    g_owner.lastBumpFlags = VesselBumpFlags(attribute);
    if (VesselTouchesGround(attribute))
        ++g_owner.groundContactFrameCount;
    if (g_owner.lastBumpFlags == BF_BUMPSTATIC)
        ++g_owner.staticCollisionFrameCount;
    if (g_owner.lastBumpFlags == BF_BUMPLAND)
        ++g_owner.landCollisionFrameCount;
    if (g_owner.lastBumpFlags == BF_BUMPDYNAMIC)
        ++g_owner.dynamicCollisionFrameCount;
}

bool VehicleReady(Vehicle *vehicle)
{
    AttributeVehicle *attribute = ResolveAttribute(vehicle);
    if (vehicle == NULL || vehicle->getContext() == NULL ||
        attribute == NULL ||
        VesselKind(attribute) == RECOVERED_VEHICLE_VESSEL_UNKNOWN)
        return false;
    const double mass = vehicle->VesselMass();
    return std::isfinite(mass) && mass > 0.0 &&
           FiniteVector(vehicle->Pos()) && FiniteVector(vehicle->Speed()) &&
           FiniteMatrix(vehicle->GetDir()) &&
           FiniteVector(vehicle->getPosition()) &&
           std::isfinite(vehicle->m_lastTime);
}

int VehicleReadinessIssue(Vehicle *vehicle)
{
    int issue = 0;
    AttributeVehicle *attribute = ResolveAttribute(vehicle);
    if (vehicle == NULL || vehicle->getContext() == NULL) issue |= 1;
    if (attribute == NULL) issue |= 2;
    if (attribute == NULL ||
        VesselKind(attribute) == RECOVERED_VEHICLE_VESSEL_UNKNOWN) issue |= 4;
    if (vehicle != NULL)
    {
        const double mass = vehicle->VesselMass();
        if (!std::isfinite(mass) || mass <= 0.0)
        {
            // VesselMass is the null-safe ownership probe.  The remaining
            // accessors delegate directly to m_vessel and must not be used to
            // diagnose a missing vessel.
            issue |= 8 | 16 | 32 | 64;
        }
        else
        {
            if (!FiniteVector(vehicle->Pos())) issue |= 16;
            if (!FiniteVector(vehicle->Speed())) issue |= 32;
            if (!FiniteMatrix(vehicle->GetDir())) issue |= 64;
        }
        if (!FiniteVector(vehicle->getPosition())) issue |= 128;
        if (!std::isfinite(vehicle->m_lastTime)) issue |= 256;
    }
    return issue;
}

double MaximumAbsoluteComponent(const CFVector3 &value)
{
    return (std::max)((std::max)(std::fabs(value.x), std::fabs(value.y)),
                      std::fabs(value.z));
}

double MaximumDirectionComponent(const CFMatrix3x4 &value)
{
    return (std::max)(
        MaximumAbsoluteComponent(value.Row(0)),
        (std::max)(MaximumAbsoluteComponent(value.Row(1)),
                   MaximumAbsoluteComponent(value.Row(2))));
}

bool CaptureFrameStart()
{
    if (!VehicleReady(g_owner.vehicle))
        return false;
    g_owner.frameStartPosition = g_owner.vehicle->Pos();
    g_owner.frameStartSubjectPosition = g_owner.vehicle->getPosition();
    g_owner.frameStartDirection = g_owner.vehicle->GetDir();
    g_owner.frameStartValid =
        FiniteVector(g_owner.frameStartPosition) &&
        FiniteVector(g_owner.frameStartSubjectPosition) &&
        FiniteMatrix(g_owner.frameStartDirection) &&
        MaximumAbsoluteComponent(g_owner.vehicle->Speed()) <=
            kMaximumStableSpeedComponent &&
        MaximumDirectionComponent(g_owner.frameStartDirection) <=
            kMaximumStableDirectionComponent;
    return g_owner.frameStartValid;
}

int CompletedFrameStabilityIssue()
{
    if (!g_owner.frameStartValid || g_owner.vehicle == NULL)
        return RECOVERED_VEHICLE_STABILITY_RESTORE_FAILED;
    const double mass = g_owner.vehicle->VesselMass();
    if (!std::isfinite(mass) || mass <= 0.0)
        return RECOVERED_VEHICLE_STABILITY_NONFINITE;

    const CFVector3 position = g_owner.vehicle->Pos();
    const CFVector3 subjectPosition = g_owner.vehicle->getPosition();
    const CFVector3 speed = g_owner.vehicle->Speed();
    const CFMatrix3x4 direction = g_owner.vehicle->GetDir();
    if (!FiniteVector(position) || !FiniteVector(subjectPosition) ||
        !FiniteVector(speed) || !FiniteMatrix(direction) ||
        !std::isfinite(g_owner.vehicle->m_lastTime))
        return RECOVERED_VEHICLE_STABILITY_NONFINITE;
    if (MaximumDirectionComponent(direction) >
        kMaximumStableDirectionComponent)
        return RECOVERED_VEHICLE_STABILITY_INVALID_DIRECTION;
    if (MaximumAbsoluteComponent(speed) > kMaximumStableSpeedComponent)
        return RECOVERED_VEHICLE_STABILITY_EXCESSIVE_SPEED;
    if (MaximumAbsoluteComponent(position - g_owner.frameStartPosition) >
        kMaximumStableFrameDisplacement)
        return RECOVERED_VEHICLE_STABILITY_EXCESSIVE_DISPLACEMENT;
    return RECOVERED_VEHICLE_STABILITY_NONE;
}

bool RestoreStableFrame(int reason, double targetTime)
{
    if (!g_owner.frameStartValid || g_owner.vehicle == NULL ||
        !std::isfinite(targetTime))
    {
        g_owner.lastStabilityReason =
            RECOVERED_VEHICLE_STABILITY_RESTORE_FAILED;
        return false;
    }

    Vehicle *vehicle = g_owner.vehicle;
    vehicle->Restart();
    vehicle->SetDir(g_owner.frameStartDirection);
    vehicle->SetPos(g_owner.frameStartPosition);
    vehicle->Stop();
    vehicle->setPosition(g_owner.frameStartSubjectPosition);
    vehicle->m_lastTime = targetTime;
    Vehicle::s_curTime = targetTime;
    Session::m_viewTime = targetTime;
    if (!VehicleReady(vehicle) ||
        !NearlyEqual(vehicle->Pos(), g_owner.frameStartPosition, 1.0e-5) ||
        !NearlyEqual(vehicle->getPosition(),
                     g_owner.frameStartSubjectPosition, 1.0e-5) ||
        !NearlyEqual(vehicle->Speed(), CFVector3(0.0, 0.0, 0.0), 1.0e-5) ||
        !NearlyEqual(vehicle->GetDir(), g_owner.frameStartDirection, 1.0e-5))
    {
        g_owner.lastStabilityReason =
            RECOVERED_VEHICLE_STABILITY_RESTORE_FAILED;
        return false;
    }

    ++g_owner.stabilityRecoveryCount;
    g_owner.lastStabilityReason = reason;
    g_owner.lastStablePosition = g_owner.frameStartPosition;
    g_owner.lastStableValid = true;
    g_owner.lastBumpFlags = BF_NONE;
    return true;
}

bool StabilizeCompletedFrame(double targetTime)
{
    const int issue = CompletedFrameStabilityIssue();
    if (issue != RECOVERED_VEHICLE_STABILITY_NONE)
        return RestoreStableFrame(issue, targetTime);
    g_owner.lastStablePosition = g_owner.vehicle->Pos();
    g_owner.lastStableValid = true;
    return true;
}

bool ReadState(Vehicle *vehicle, SRecoveredVehicleRuntimeState *state)
{
    if (state == NULL)
        return false;
    std::memset(state, 0, sizeof(*state));
    state->object = KR_ObjectID::NUL();
    state->attribute = KR_ObjectID::NUL();
    AttributeVehicle *attribute = ResolveAttribute(vehicle);
    if (!VehicleReady(vehicle) || attribute == NULL)
        return false;
    state->object = vehicle->getObjectID();
    state->attribute = vehicle->m_vehicleAttrID;
    state->position = vehicle->Pos();
    state->subjectPosition = vehicle->getPosition();
    state->speed = vehicle->Speed();
    state->direction = vehicle->GetDir();
    state->mass = vehicle->VesselMass();
    state->lastTime = vehicle->m_lastTime;
    state->vesselKind = VesselKind(attribute);
    state->active = g_owner.active && g_owner.vehicle == vehicle;
    state->frameBegun = state->active && g_owner.frameBegun;
    state->advanceCount = state->active ? g_owner.advanceCount : 0;
    state->controlEventCount =
        state->active ? g_owner.controlEventCount : 0;
    state->lastBumpFlags =
        state->active ? g_owner.lastBumpFlags : VesselBumpFlags(attribute);
    state->touchingGround = VesselTouchesGround(attribute) ? 1 : 0;
    state->groundContactFrameCount =
        state->active ? g_owner.groundContactFrameCount : 0;
    state->staticCollisionFrameCount =
        state->active ? g_owner.staticCollisionFrameCount : 0;
    state->landCollisionFrameCount =
        state->active ? g_owner.landCollisionFrameCount : 0;
    state->dynamicCollisionFrameCount =
        state->active ? g_owner.dynamicCollisionFrameCount : 0;
    state->stabilityRecoveryCount =
        state->active ? g_owner.stabilityRecoveryCount : 0;
    state->lastStabilityReason =
        state->active ? g_owner.lastStabilityReason
                      : RECOVERED_VEHICLE_STABILITY_NONE;
    return !IsNul(state->object) && !IsNul(state->attribute) &&
           FiniteVector(state->position) &&
           FiniteVector(state->subjectPosition) &&
           FiniteVector(state->speed) && FiniteMatrix(state->direction) &&
           std::isfinite(state->mass) && state->mass > 0.0 &&
           std::isfinite(state->lastTime) &&
           state->vesselKind != RECOVERED_VEHICLE_VESSEL_UNKNOWN;
}

bool RestoreOwner()
{
    if (!g_owner.active || g_owner.vehicle == NULL ||
        !VehicleReady(g_owner.vehicle))
        return false;
    Vehicle *vehicle = g_owner.vehicle;
    vehicle->Restart();
    vehicle->SetDir(g_owner.savedDirection);
    vehicle->SetPos(g_owner.savedPosition);
    vehicle->Stop();
    vehicle->setPosition(g_owner.savedSubjectPosition);
    vehicle->m_lastTime = g_owner.savedLastTime;
    Vehicle::s_curTime = g_owner.savedCurrentTime;
    Session::m_viewTime = g_owner.savedViewTime;
    return VehicleReady(vehicle) &&
           NearlyEqual(vehicle->Pos(), g_owner.savedPosition) &&
           NearlyEqual(vehicle->getPosition(),
                       g_owner.savedSubjectPosition) &&
           NearlyEqual(vehicle->Speed(), g_owner.savedSpeed) &&
           NearlyEqual(vehicle->GetDir(), g_owner.savedDirection) &&
           NearlyEqual(vehicle->m_lastTime, g_owner.savedLastTime);
}

bool PublicStatesMatch(const SRecoveredVehicleRuntimeState &before,
                       const SRecoveredVehicleRuntimeState &after)
{
    return before.object == after.object &&
           before.attribute == after.attribute &&
           NearlyEqual(before.position, after.position) &&
           NearlyEqual(before.subjectPosition, after.subjectPosition) &&
           NearlyEqual(before.speed, after.speed) &&
           NearlyEqual(before.direction, after.direction) &&
           NearlyEqual(before.mass, after.mass) &&
           NearlyEqual(before.lastTime, after.lastTime) &&
           before.vesselKind == after.vesselKind;
}

bool StatesMatch(const SRecoveredVehicleRuntimeState &before,
                 const SRecoveredVehicleRuntimeState &after)
{
    return PublicStatesMatch(before, after) && !after.active;
}

}  // namespace

void VehicleRuntimeState_Link()
{
}

bool VehicleRuntimeState_IsClean(SimulationContext *context)
{
    (void)context;
    return !g_owner.active && g_owner.context == NULL &&
           g_owner.vehicle == NULL && IsNul(g_owner.object);
}

bool VehicleRuntimeState_Inspect(
    SimulationContext *context, const KR_ObjectID &vehicle,
    SRecoveredVehicleRuntimeState *state)
{
    Vehicle *resolved = ResolveVehicle(context, vehicle);
    return resolved != NULL && ReadState(resolved, state);
}

const char *VehicleRuntimeState_AttributeName(
    SimulationContext *context, const KR_ObjectID &vehicle)
{
    Vehicle *resolved = ResolveVehicle(context, vehicle);
    return VehicleReady(resolved)
               ? context->searchObject(resolved->m_vehicleAttrID)
               : NULL;
}

const char *VehicleRuntimeState_DynamicName(
    SimulationContext *context, const KR_ObjectID &vehicle)
{
    AttributeVehicle *attribute =
        ResolveAttribute(ResolveVehicle(context, vehicle));
    return attribute == NULL ? NULL : attribute->m_dynamic;
}

unsigned long long VehicleRuntimeState_IdentityFingerprint(
    SimulationContext *context, const KR_ObjectID &vehicle)
{
    SRecoveredVehicleRuntimeState state = {};
    const char *attribute =
        VehicleRuntimeState_AttributeName(context, vehicle);
    const char *dynamic = VehicleRuntimeState_DynamicName(context, vehicle);
    if (!VehicleRuntimeState_Inspect(context, vehicle, &state) ||
        attribute == NULL || dynamic == NULL)
        return 0;
    unsigned long long hash = kHashOffset;
    HashString(hash, attribute);
    HashString(hash, dynamic);
    HashBytes(hash, &state.vesselKind, sizeof(state.vesselKind));
    HashBytes(hash, &state.mass, sizeof(state.mass));
    return hash;
}

bool VehicleRuntimeState_IsKnownRetailIdentity(
    SimulationContext *context, const KR_ObjectID &vehicle)
{
    // All nine May retail levels resolve Vehicle.Default to the same
    // AttributeVehicle/CVesselWheels/mass contract. Reject unknown script
    // variants before exercising their real physics through UpdatePos().
    return VehicleRuntimeState_IdentityFingerprint(context, vehicle) ==
           14754063850192062311ull;
}

bool VehicleRuntimeState_Activate(
    SimulationContext *context, const KR_ObjectID &vehicle,
    const CFVector3 &position, double startTime)
{
    if (!VehicleRuntimeState_IsClean(context) || context == NULL ||
        !FiniteVector(position) || !std::isfinite(startTime) ||
        startTime < 0.1)
        return false;
    Vehicle *resolved = ResolveVehicle(context, vehicle);
    SRecoveredVehicleRuntimeState before = {};
    if (!ReadState(resolved, &before) ||
        !NearlyEqual(before.speed, CFVector3(0.0, 0.0, 0.0)))
        return false;

    g_owner.context = context;
    g_owner.vehicle = resolved;
    g_owner.object = vehicle;
    g_owner.savedPosition = before.position;
    g_owner.savedSubjectPosition = before.subjectPosition;
    g_owner.savedSpeed = before.speed;
    g_owner.savedDirection = before.direction;
    g_owner.savedLastTime = before.lastTime;
    g_owner.savedCurrentTime = Vehicle::s_curTime;
    g_owner.savedViewTime = Session::m_viewTime;
    g_owner.lastTime = startTime;
    g_owner.advanceCount = 0;
    g_owner.controlEventCount = 0;
    g_owner.lastBumpFlags = BF_NONE;
    g_owner.groundContactFrameCount = 0;
    g_owner.staticCollisionFrameCount = 0;
    g_owner.landCollisionFrameCount = 0;
    g_owner.dynamicCollisionFrameCount = 0;
    g_owner.stabilityRecoveryCount = 0;
    g_owner.lastStabilityReason = RECOVERED_VEHICLE_STABILITY_NONE;
    g_owner.active = true;
    g_owner.frameBegun = false;
    g_owner.frameStartValid = false;
    g_owner.lastStableValid = false;

    resolved->Restart();
    resolved->GetDir().LoadIdentity();
    resolved->SetPos(position);
    resolved->Stop();
    resolved->m_lastTime = startTime;
    Vehicle::s_curTime = startTime;
    Session::m_viewTime = startTime;

    SRecoveredVehicleRuntimeState activated = {};
    if (ReadState(resolved, &activated) && activated.active &&
        NearlyEqual(activated.position, position, 1.0e-5) &&
        FiniteVector(activated.subjectPosition) &&
        NearlyEqual(activated.speed, CFVector3(0.0, 0.0, 0.0)) &&
        NearlyEqual(activated.lastTime, startTime))
    {
        g_owner.lastStablePosition = activated.position;
        g_owner.lastStableValid = true;
        return true;
    }

    RestoreOwner();
    ClearOwner();
    return false;
}

bool VehicleRuntimeState_ApplyControl(
    SimulationContext *context, int action, double down)
{
    return VehicleRuntimeState_ApplyControlAt(
        context, action, down, g_owner.lastTime);
}

bool VehicleRuntimeState_ApplyControlAt(
    SimulationContext *context, int action, double down,
    double eventTime)
{
    g_lastControlFailure = 0;
    g_lastAppliedControlTime = -1.0;
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL)
    {
        g_lastControlFailure = 1;
        return false;
    }
    if (!std::isfinite(down) || down < -1.0 || down > 1.0 ||
        !std::isfinite(eventTime))
    {
        g_lastControlFailure = 2;
        return false;
    }
    if (eventTime < g_owner.lastTime)
        eventTime = g_owner.lastTime;
    if (eventTime - g_owner.lastTime > kMaximumStep + kTimeEpsilon)
    {
        g_lastControlFailure = 3;
        return false;
    }
    switch (action)
    {
    case MOVE_FORWARD:
    case MOVE_BACKWARD:
    case STRAFE_LEFT:
    case STRAFE_RIGHT:
    case STRAFE_UP:
    case STRAFE_DOWN:
    case LOOK_UP:
    case LOOK_DOWN:
    case TURN_LEFT:
    case TURN_RIGHT:
    case FIRE_PRIMARY:
    case STOP_VEHICLE:
    case CHANGE_VEHICLE:
        break;
    default:
        g_lastControlFailure = 4;
        return false;
    }
    KR_ObjectID source = context->searchObject("Hardware");
    if (IsNul(source))
        source = g_owner.object;
    KR_Event event;
    event.source = source;
    event.destination = g_owner.object;
    event.timeStamp = eventTime;
    event.label = CTRL_BUTTONS_MSG;
    event.data.open(EDO_WRITE)
        .putInt(action)
        .putDouble(down)
        .putInt(0)
        .putInt(FALSE)
        .close();
    if (g_owner.vehicle->receiveEvent(event) != 1)
    {
        g_lastControlFailure = 5;
        return false;
    }
    if (!VehicleReady(g_owner.vehicle))
    {
        g_lastControlFailure = 6;
        return false;
    }
    ++g_owner.controlEventCount;
    g_lastAppliedControlTime = eventTime;
    return true;
}

int VehicleRuntimeState_LastControlFailure()
{
    return g_lastControlFailure;
}

double VehicleRuntimeState_LastAppliedControlTime()
{
    return g_lastAppliedControlTime;
}

bool VehicleRuntimeState_ApplyLiveControlAt(
    SimulationContext *context, int action, double down,
    double eventTime)
{
    g_lastControlFailure = 0;
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL)
    {
        g_lastControlFailure = 1;
        return false;
    }
    if (!std::isfinite(eventTime))
    {
        g_lastControlFailure = 2;
        return false;
    }
    const double boundedTime =
        (std::max)(g_owner.lastTime,
                   (std::min)(eventTime,
                              g_owner.lastTime + kMaximumStep));
    return VehicleRuntimeState_ApplyControlAt(
        context, action, down, boundedTime);
}

bool VehicleRuntimeState_SynchronizeFirstFrame(
    SimulationContext *context, double startTime)
{
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        g_owner.frameBegun || g_owner.advanceCount != 0 ||
        g_owner.controlEventCount != 0 || !std::isfinite(startTime) ||
        startTime < 0.1 || !VehicleReady(g_owner.vehicle))
        return false;
    g_owner.lastTime = startTime;
    g_owner.vehicle->m_lastTime = startTime;
    Vehicle::s_curTime = startTime;
    Session::m_viewTime = startTime;
    return VehicleReady(g_owner.vehicle);
}

bool VehicleRuntimeState_RebaseRestoredOwner(SimulationContext *context)
{
    if (!g_owner.active || context == NULL || g_owner.context != context ||
        g_owner.vehicle == NULL || g_owner.frameBegun ||
        !context->isExist(g_owner.object) || !VehicleReady(g_owner.vehicle))
        return false;
    SRecoveredVehicleRuntimeState restored = {};
    if (!ReadState(g_owner.vehicle, &restored) || !restored.active ||
        !std::isfinite(restored.lastTime) || restored.lastTime < 0.1 ||
        !std::isfinite(Session::m_viewTime) ||
        Session::m_viewTime + kTimeEpsilon < restored.lastTime)
        return false;
    g_owner.lastTime = restored.lastTime;
    g_owner.frameAttribute = KR_ObjectID::NUL();
    g_owner.frameStartValid = false;
    g_owner.lastStablePosition = restored.position;
    g_owner.lastStableValid = true;
    return true;
}

bool VehicleRuntimeState_Advance(
    SimulationContext *context, double targetTime)
{
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        g_owner.frameBegun || !std::isfinite(targetTime))
        return false;
    const double deltaTime = targetTime - g_owner.lastTime;
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0 ||
        deltaTime > kMaximumStep + kTimeEpsilon)
        return false;

    return VehicleRuntimeState_BeginFrame(context) &&
           VehicleRuntimeState_CompleteFrame(context, targetTime);
}

bool VehicleRuntimeState_BeginFrame(SimulationContext *context)
{
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        g_owner.frameBegun || !VehicleReady(g_owner.vehicle))
        return false;

    g_owner.frameAttribute = g_owner.vehicle->m_vehicleAttrID;
    g_owner.vehicle->BeginPreStep();
    if (!CaptureFrameStart())
    {
        g_owner.frameAttribute = KR_ObjectID::NUL();
        g_owner.frameStartValid = false;
        return false;
    }
    g_owner.frameBegun = true;
    return true;
}

static bool PrepareCurrentVesselForFrameCompletion()
{
    g_lastFrameReadinessIssue = 0;
    if (!g_owner.frameBegun || g_owner.vehicle == NULL)
        return false;
    if (g_owner.vehicle->m_vehicleAttrID != g_owner.frameAttribute)
    {
        // SET_TAXI/F1 can replace and restart the underlying vessel while the
        // event queue is processed between BeginFrame and CompleteFrame.  The
        // old vessel owned the earlier BeginPreStep; initialize the new one
        // before UpdatePos instead of feeding it a half-open frame.
        if (!VehicleReady(g_owner.vehicle))
        {
            g_lastFrameReadinessIssue =
                VehicleReadinessIssue(g_owner.vehicle);
            return false;
        }
        g_owner.vehicle->BeginPreStep();
        g_owner.frameAttribute = g_owner.vehicle->m_vehicleAttrID;
        // The Taxi event has atomically replaced, restarted and positioned the
        // vessel. Roll back any subsequent solver failure to this new car,
        // never to the body and pose that owned the beginning of the frame.
        if (!CaptureFrameStart())
            return false;
    }
    const bool ready = VehicleReady(g_owner.vehicle);
    if (!ready)
        g_lastFrameReadinessIssue = VehicleReadinessIssue(g_owner.vehicle);
    return ready;
}

bool VehicleRuntimeState_CompleteFrame(
    SimulationContext *context, double targetTime)
{
    g_lastFrameFailure = 0;
    g_lastFrameReadinessIssue = 0;
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        !g_owner.frameBegun || !std::isfinite(targetTime))
    {
        g_lastFrameFailure = 10;
        return false;
    }
    const double deltaTime = targetTime - g_owner.lastTime;
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0 ||
        deltaTime > kMaximumStep + kTimeEpsilon)
    {
        g_lastFrameFailure = 11;
        g_owner.frameBegun = false;
        g_owner.frameAttribute = KR_ObjectID::NUL();
        g_owner.frameStartValid = false;
        return false;
    }

    if (!PrepareCurrentVesselForFrameCompletion())
    {
        g_lastFrameFailure = 12;
        g_owner.frameBegun = false;
        g_owner.frameAttribute = KR_ObjectID::NUL();
        g_owner.frameStartValid = false;
        return false;
    }
    Session::m_viewTime = targetTime;
    g_owner.vehicle->UpdatePos();
    const int recoveriesBefore = g_owner.stabilityRecoveryCount;
    const bool stable = StabilizeCompletedFrame(targetTime);
    if (stable && g_owner.stabilityRecoveryCount == recoveriesBefore)
        RecordFrameCollision();
    g_owner.lastTime = targetTime;
    ++g_owner.advanceCount;
    g_owner.frameBegun = false;
    g_owner.frameAttribute = KR_ObjectID::NUL();
    g_owner.frameStartValid = false;
    if (!stable)
    {
        g_lastFrameFailure = 13;
        return false;
    }
    SRecoveredVehicleRuntimeState state = {};
    const bool valid = ReadState(g_owner.vehicle, &state) && state.active &&
        !state.frameBegun &&
        state.advanceCount == g_owner.advanceCount &&
        NearlyEqual(state.lastTime, targetTime);
    if (!valid) g_lastFrameFailure = 13;
    return valid;
}

bool VehicleRuntimeState_CompleteLiveFrame(
    SimulationContext *context, double targetTime,
    bool *droppedTime)
{
    g_lastFrameFailure = 0;
    g_lastFrameReadinessIssue = 0;
    if (droppedTime == NULL)
    {
        g_lastFrameFailure = 1;
        return false;
    }
    *droppedTime = false;
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        !g_owner.frameBegun || !std::isfinite(targetTime))
    {
        g_lastFrameFailure = 2;
        return false;
    }
    const double deltaTime = targetTime - g_owner.lastTime;
    if (!std::isfinite(deltaTime) || deltaTime < 0.0)
    {
        g_lastFrameFailure = 3;
        g_owner.frameBegun = false;
        g_owner.frameAttribute = KR_ObjectID::NUL();
        g_owner.frameStartValid = false;
        return false;
    }
    if (deltaTime == 0.0)
    {
        if (!PrepareCurrentVesselForFrameCompletion())
        {
            g_lastFrameFailure = 4;
            g_owner.frameBegun = false;
            g_owner.frameAttribute = KR_ObjectID::NUL();
            g_owner.frameStartValid = false;
            return false;
        }
        Session::m_viewTime = targetTime;
        g_owner.vehicle->UpdatePos();
        const int recoveriesBefore = g_owner.stabilityRecoveryCount;
        const bool stable = StabilizeCompletedFrame(targetTime);
        if (stable && g_owner.stabilityRecoveryCount == recoveriesBefore)
            RecordFrameCollision();
        ++g_owner.advanceCount;
        g_owner.frameBegun = false;
        g_owner.frameAttribute = KR_ObjectID::NUL();
        g_owner.frameStartValid = false;
        if (!stable)
        {
            g_lastFrameFailure = 5;
            return false;
        }
        SRecoveredVehicleRuntimeState state = {};
        const bool valid = ReadState(g_owner.vehicle, &state) && state.active &&
            !state.frameBegun &&
            state.advanceCount == g_owner.advanceCount &&
            NearlyEqual(state.lastTime, targetTime);
        if (!valid) g_lastFrameFailure = 5;
        return valid;
    }
    if (deltaTime <= kMaximumStep + kTimeEpsilon)
        return VehicleRuntimeState_CompleteFrame(context, targetTime);

    if (!PrepareCurrentVesselForFrameCompletion())
    {
        g_lastFrameFailure = 6;
        g_owner.frameBegun = false;
        g_owner.frameAttribute = KR_ObjectID::NUL();
        g_owner.frameStartValid = false;
        return false;
    }
    const double physicsTarget = g_owner.lastTime + kMaximumStep;
    Session::m_viewTime = physicsTarget;
    g_owner.vehicle->UpdatePos();
    const int recoveriesBefore = g_owner.stabilityRecoveryCount;
    const bool stable = StabilizeCompletedFrame(targetTime);
    if (stable && g_owner.stabilityRecoveryCount == recoveriesBefore)
        RecordFrameCollision();
    g_owner.vehicle->m_lastTime = targetTime;
    Vehicle::s_curTime = targetTime;
    Session::m_viewTime = targetTime;
    g_owner.lastTime = targetTime;
    ++g_owner.advanceCount;
    g_owner.frameBegun = false;
    g_owner.frameAttribute = KR_ObjectID::NUL();
    g_owner.frameStartValid = false;
    *droppedTime = true;
    if (!stable)
    {
        g_lastFrameFailure = 7;
        return false;
    }
    SRecoveredVehicleRuntimeState state = {};
    const bool valid = ReadState(g_owner.vehicle, &state) && state.active &&
        !state.frameBegun &&
        state.advanceCount == g_owner.advanceCount &&
        NearlyEqual(state.lastTime, targetTime);
    if (!valid) g_lastFrameFailure = 7;
    return valid;
}

int VehicleRuntimeState_LastFrameFailure()
{
    return g_lastFrameFailure;
}

int VehicleRuntimeState_LastFrameReadinessIssue()
{
    return g_lastFrameReadinessIssue;
}

bool VehicleRuntimeState_BuildCamera(
    SimulationContext *context, CFMatrix3x4 *direction)
{
    if (!g_owner.active || context == NULL || direction == NULL ||
        g_owner.context != context || !VehicleReady(g_owner.vehicle))
        return false;
    *direction = g_owner.vehicle->GetDir();
    direction->TranslateR(-g_owner.vehicle->Pos());
    return FiniteMatrix(*direction);
}

bool VehicleRuntimeState_LastStablePosition(
    SimulationContext *context, CFVector3 *position)
{
    if (!g_owner.active || context == NULL || position == NULL ||
        g_owner.context != context || !g_owner.lastStableValid ||
        !FiniteVector(g_owner.lastStablePosition))
        return false;
    *position = g_owner.lastStablePosition;
    return true;
}

bool VehicleRuntimeState_Rollback(SimulationContext *context)
{
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        !context->isExist(g_owner.object))
        return false;
    const bool restored = RestoreOwner();
    ClearOwner();
    return restored;
}

void VehicleRuntimeState_Reset(SimulationContext *context)
{
    if (g_owner.active && context != NULL && g_owner.context == context &&
        g_owner.vehicle != NULL && context->isExist(g_owner.object))
        VehicleRuntimeState_Rollback(context);
    ClearOwner();
}

bool VehicleRuntimeState_ProbeMovement(
    SimulationContext *context, const KR_ObjectID &vehicle,
    const CFVector3 &position, double startTime,
    SRecoveredVehicleMovementProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || IsNul(vehicle) ||
        !VehicleRuntimeState_IsClean(context))
        return false;
    SRecoveredVehicleRuntimeState before = {};
    if (!VehicleRuntimeState_Inspect(context, vehicle, &before))
        return false;

    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (!VehicleRuntimeState_Activate(
            context, vehicle, CFVector3(nan, position.y, position.z),
            startTime))
        ++summary->invalidActivationRejections;
    if (!VehicleRuntimeState_Activate(context, vehicle, position, 0.0))
        ++summary->invalidActivationRejections;
    if (summary->invalidActivationRejections != 2 ||
        !VehicleRuntimeState_IsClean(context))
        return false;

    const double ts = startTime < 0.1 ? 0.1 : startTime;
    if (!VehicleRuntimeState_Activate(context, vehicle, position, ts))
        return false;
    summary->activations = 1;

    SRecoveredVehicleRuntimeState activated = {};
    CFMatrix3x4 cameraBefore;
    bool succeeded = VehicleRuntimeState_Inspect(
                         context, vehicle, &activated) &&
                     activated.active &&
                     VehicleRuntimeState_BuildCamera(
                         context, &cameraBefore);
    SRecoveredVehicleRuntimeState rejectedAdvance = {};
    if (succeeded)
    {
        succeeded = !VehicleRuntimeState_Advance(context, ts) &&
                    !VehicleRuntimeState_Advance(
                        context, ts + kMaximumStep + 0.001) &&
                    VehicleRuntimeState_Inspect(
                        context, vehicle, &rejectedAdvance) &&
                    rejectedAdvance.active &&
                    rejectedAdvance.advanceCount == activated.advanceCount &&
                    PublicStatesMatch(activated, rejectedAdvance);
    }
    double currentTime = ts;
    if (succeeded)
    {
        currentTime += 0.02;
        succeeded = VehicleRuntimeState_Advance(context, currentTime);
        if (succeeded)
            summary->stationarySteps = 1;
    }

    SRecoveredVehicleRuntimeState movementStart = {};
    if (succeeded)
        succeeded = VehicleRuntimeState_Inspect(
            context, vehicle, &movementStart);
    if (succeeded)
    {
        succeeded = VehicleRuntimeState_ApplyControl(
            context, MOVE_FORWARD, 1.0);
        if (succeeded)
            ++summary->throttleEvents;
    }
    for (int index = 0; succeeded && index < 160; ++index)
    {
        currentTime += 0.025;
        succeeded = VehicleRuntimeState_Advance(context, currentTime);
        if (succeeded)
            ++summary->movementSteps;
    }
    if (succeeded)
    {
        succeeded = VehicleRuntimeState_ApplyControl(
            context, MOVE_FORWARD, 0.0);
        if (succeeded)
            ++summary->throttleEvents;
    }
    if (succeeded)
    {
        succeeded = VehicleRuntimeState_ApplyControl(
            context, TURN_RIGHT, 1.0);
        if (succeeded)
            ++summary->turnEvents;
    }
    for (int index = 0; succeeded && index < 12; ++index)
    {
        currentTime += 0.025;
        succeeded = VehicleRuntimeState_Advance(context, currentTime);
        if (succeeded)
            ++summary->movementSteps;
    }
    if (succeeded)
    {
        succeeded = VehicleRuntimeState_ApplyControl(
            context, TURN_RIGHT, 0.0);
        if (succeeded)
            ++summary->turnEvents;
    }

    SRecoveredVehicleRuntimeState moved = {};
    CFMatrix3x4 cameraAfter;
    if (succeeded)
        succeeded = VehicleRuntimeState_Inspect(context, vehicle, &moved) &&
                    VehicleRuntimeState_BuildCamera(context, &cameraAfter);
    if (succeeded)
    {
        const double dx = moved.position.x - movementStart.position.x;
        const double dz = moved.position.z - movementStart.position.z;
        summary->horizontalDistance = std::sqrt(dx * dx + dz * dz);
        if (summary->horizontalDistance > 0.01 &&
            !NearlyEqual(cameraBefore, cameraAfter, 1.0e-5))
            summary->cameraTransitions = 1;
        else
            succeeded = false;
    }

    // Prove that a finite collision impulse large enough to reproduce the
    // retail wheels runaway is contained at the active frame boundary. The
    // owner must keep control and camera ownership while restoring the exact
    // pre-step pose and stopping the vessel.
    if (succeeded)
    {
        currentTime += 0.025;
        succeeded = VehicleRuntimeState_BeginFrame(context) &&
                    g_owner.vehicle->ApplyExplosionImpulse(
                        CFVector3(1.0e8, 0.0, -1.0e8), 1.0) &&
                    VehicleRuntimeState_CompleteFrame(context, currentTime);
    }
    SRecoveredVehicleRuntimeState recovered = {};
    CFMatrix3x4 recoveredCamera;
    if (succeeded)
    {
        succeeded = VehicleRuntimeState_Inspect(
                        context, vehicle, &recovered) &&
                    recovered.active && !recovered.frameBegun &&
                    recovered.stabilityRecoveryCount == 1 &&
                    recovered.lastStabilityReason !=
                        RECOVERED_VEHICLE_STABILITY_NONE &&
                    recovered.lastStabilityReason !=
                        RECOVERED_VEHICLE_STABILITY_RESTORE_FAILED &&
                    NearlyEqual(recovered.position, moved.position, 1.0e-5) &&
                    NearlyEqual(recovered.speed,
                                CFVector3(0.0, 0.0, 0.0), 1.0e-5) &&
                    VehicleRuntimeState_BuildCamera(
                        context, &recoveredCamera);
        if (succeeded)
            summary->stabilityRecoveries = 1;
    }

    const bool rolledBack = VehicleRuntimeState_Rollback(context);
    if (rolledBack)
        summary->rollbacks = 1;
    SRecoveredVehicleRuntimeState after = {};
    const bool restored =
        VehicleRuntimeState_Inspect(context, vehicle, &after);
    return succeeded && rolledBack && restored &&
           StatesMatch(before, after) &&
           summary->invalidActivationRejections == 2 &&
           summary->activations == 1 && summary->stationarySteps == 1 &&
           summary->throttleEvents == 2 &&
           summary->movementSteps == 172 && summary->turnEvents == 2 &&
           summary->cameraTransitions == 1 &&
           summary->stabilityRecoveries == 1 && summary->rollbacks == 1 &&
           summary->horizontalDistance > 0.01 &&
           VehicleRuntimeState_IsClean(context);
}
