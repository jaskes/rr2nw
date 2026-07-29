#include "VehicleRuntimeState.h"

#include <cmath>
#include <cstring>
#include <limits>

class CGRPanel;
#include "h/vehicle.h"
#include "hardware.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/hardmsg.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const double kMaximumStep = 0.05;

struct VehicleRuntimeOwner
{
    SimulationContext *context;
    Vehicle *vehicle;
    KR_ObjectID object;
    CFVector3 savedPosition;
    CFVector3 savedSubjectPosition;
    CFVector3 savedSpeed;
    CFMatrix3x4 savedDirection;
    double savedLastTime;
    double savedCurrentTime;
    double savedViewTime;
    double lastTime;
    int advanceCount;
    int controlEventCount;
    bool active;

    VehicleRuntimeOwner()
        : context(NULL), vehicle(NULL), object(KR_ObjectID::NUL()),
          savedPosition(0.0, 0.0, 0.0),
          savedSubjectPosition(0.0, 0.0, 0.0),
          savedSpeed(0.0, 0.0, 0.0), savedLastTime(0.0),
          savedCurrentTime(0.0), savedViewTime(0.0), lastTime(0.0),
          advanceCount(0), controlEventCount(0), active(false)
    {
        savedDirection.LoadIdentity();
    }
};

VehicleRuntimeOwner g_owner;

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
    g_owner.savedPosition = CFVector3(0.0, 0.0, 0.0);
    g_owner.savedSubjectPosition = CFVector3(0.0, 0.0, 0.0);
    g_owner.savedSpeed = CFVector3(0.0, 0.0, 0.0);
    g_owner.savedDirection.LoadIdentity();
    g_owner.savedLastTime = 0.0;
    g_owner.savedCurrentTime = 0.0;
    g_owner.savedViewTime = 0.0;
    g_owner.lastTime = 0.0;
    g_owner.advanceCount = 0;
    g_owner.controlEventCount = 0;
    g_owner.active = false;
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
    state->advanceCount = state->active ? g_owner.advanceCount : 0;
    state->controlEventCount =
        state->active ? g_owner.controlEventCount : 0;
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
    g_owner.active = true;

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
        return true;

    RestoreOwner();
    ClearOwner();
    return false;
}

bool VehicleRuntimeState_ApplyControl(
    SimulationContext *context, int action, double down)
{
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        !std::isfinite(down) || down < -1.0 || down > 1.0)
        return false;
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
    case STOP_VEHICLE:
        break;
    default:
        return false;
    }
    KR_ObjectID source = context->searchObject("Hardware");
    if (IsNul(source))
        source = g_owner.object;
    KR_Event event;
    event.source = source;
    event.destination = g_owner.object;
    event.timeStamp = g_owner.lastTime;
    event.label = CTRL_BUTTONS_MSG;
    event.data.open(EDO_WRITE)
        .putInt(action)
        .putDouble(down)
        .putInt(0)
        .putInt(FALSE)
        .close();
    if (g_owner.vehicle->receiveEvent(event) != 1 ||
        !VehicleReady(g_owner.vehicle))
        return false;
    ++g_owner.controlEventCount;
    return true;
}

bool VehicleRuntimeState_Advance(
    SimulationContext *context, double targetTime)
{
    if (!g_owner.active || context == NULL ||
        g_owner.context != context || g_owner.vehicle == NULL ||
        !std::isfinite(targetTime))
        return false;
    const double deltaTime = targetTime - g_owner.lastTime;
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0 ||
        deltaTime > kMaximumStep)
        return false;

    g_owner.vehicle->BeginPreStep();
    Session::m_viewTime = targetTime;
    g_owner.vehicle->UpdatePos();
    g_owner.lastTime = targetTime;
    ++g_owner.advanceCount;
    SRecoveredVehicleRuntimeState state = {};
    return ReadState(g_owner.vehicle, &state) && state.active &&
           state.advanceCount == g_owner.advanceCount &&
           NearlyEqual(state.lastTime, targetTime);
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
           summary->cameraTransitions == 1 && summary->rollbacks == 1 &&
           summary->horizontalDistance > 0.01 &&
           VehicleRuntimeState_IsClean(context);
}
