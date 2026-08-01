#include "VehicleActiveWorldState.h"

#include "VehicleRuntimeState.h"
#include "VehicleVesselSaveState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class CGRPanel;
#include "h/vehicle.h"
#include "i/commander.i"
#include "kernel/h/context.h"
#include "message/strgmsg.h"

extern VehicleTable __classTable;

namespace
{

const std::uint32_t kVehicleMagic = 0x31484556u; // VEH1
const std::uint32_t kVehicleVersion = 1u;
const std::size_t kMaximumVehicles = 1;
const std::size_t kMaximumPlayerSides = 5;

struct StablePlayerStatus
{
    std::string commander;
    double damageDifference;
    int renegade;
    int missionSuccess;
};

struct StableVesselState
{
    int kind;
    SVehicleEmvSaveState emv;
    SVehicleWheelsSaveState wheels;

    StableVesselState() : kind(RECOVERED_VEHICLE_VESSEL_UNKNOWN), emv(), wheels()
    {
        emv.size = sizeof(emv);
        wheels.size = sizeof(wheels);
    }
};

struct StableVehicleRecord
{
    std::string name;
    std::string attribute;
    std::string defaultAttribute;
    std::string deadAttribute;
    StableVesselState vessel;
    CFVector3 subjectPosition;
    int firePrimary;
    int primaryEnabled;
    int fireSecondary;
    int secondaryEnabled;
    int firePrimaryPressed;
    int fireSecondaryPressed;
    double damage;
    CFVector3 skipPosition;
    double skipTime;
    int secondaryBulletCount;
    int briefingPlayed;
    double lastLeaveTime;
    double lastTime;
    double playerDamage;
    std::vector<StablePlayerStatus> playerSides;
    int dead;
    double currentTime;
    int takingTaxi;
    double taxiCurrentAngle;
    double taxiFinalAngle;
    double lastEventTime;
    double taxiRotateSpeed;
    double spawnX;
    double spawnY;
    double spawnZ;
    CFVector3 currentTaxiPosition;
    CFVector3 currentTaxiVehiclePosition;

    StableVehicleRecord()
        : subjectPosition(0.0, 0.0, 0.0), firePrimary(0),
          primaryEnabled(0), fireSecondary(0), secondaryEnabled(0),
          firePrimaryPressed(0), fireSecondaryPressed(0), damage(0.0),
          skipPosition(0.0, 0.0, 0.0), skipTime(-1.0),
          secondaryBulletCount(0), briefingPlayed(0), lastLeaveTime(0.0),
          lastTime(0.0), playerDamage(0.0), dead(0), currentTime(0.0),
          takingTaxi(0), taxiCurrentAngle(0.0), taxiFinalAngle(0.0),
          lastEventTime(0.0), taxiRotateSpeed(0.0), spawnX(0.0),
          spawnY(0.0), spawnZ(0.0), currentTaxiPosition(0.0, 0.0, 0.0),
          currentTaxiVehiclePosition(0.0, 0.0, 0.0)
    {
    }
};

struct VehicleRoster
{
    SimulationContext *context;
    std::vector<Vehicle *> objects;
    bool valid;
};

struct ResolvedVehicleRecord
{
    Vehicle *vehicle;
    KR_ObjectID attribute;
    KR_ObjectID defaultAttribute;
    KR_ObjectID deadAttribute;
    std::vector<KR_ObjectID> playerCommanders;
};

bool IsNul(const KR_ObjectID &object)
{
    KR_ObjectID copy = object;
    return copy.isNUL() != FALSE;
}

bool FiniteVector(const SFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool FiniteMatrix(const SFMatrix3x4 &value)
{
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            if (!std::isfinite(value.m[row][column]))
                return false;
    return true;
}

bool FiniteDoubles(const double *values, std::size_t count)
{
    for (std::size_t index = 0; index < count; ++index)
        if (!std::isfinite(values[index]))
            return false;
    return true;
}

bool ValidateEmv(const SVehicleEmvSaveState &state)
{
    const double values[] = {
        state.fuelSpeed, state.throttle, state.verticalRotation,
        state.axisRotation, state.inclination, state.inclinationRotation,
        state.turnRotation, state.strafeInclination,
        state.strafeInclinationRotation, state.verticalStrafe,
        state.mouseTurnRotation, state.mouseVerticalRotation};
    return state.size == sizeof(state) && FiniteMatrix(state.direction) &&
           FiniteVector(state.position) && FiniteVector(state.speed) &&
           FiniteDoubles(values, sizeof(values) / sizeof(values[0]));
}

bool ValidateWheels(const SVehicleWheelsSaveState &state)
{
    const double values[] = {
        state.fuelAcceleration, state.throttle, state.raise,
        state.raiseSpeed, state.turnSpeed, state.accelerationFactor,
        state.jumpOffsetFactor, state.jumpTimeout, state.suspensionTravel,
        state.walkPendulumArgument, state.walkPendulumCoefficient,
        state.walkPendulumSpeedCoefficient, state.mouseTurnSpeed,
        state.mouseRaise, state.mouseRaiseSpeed, state.jumpTime,
        state.jumpAccumulation, state.jumpDownVelocity,
        state.lastFallVelocity};
    return state.size == sizeof(state) && FiniteMatrix(state.direction) &&
           FiniteMatrix(state.base) && FiniteVector(state.position) &&
           FiniteVector(state.speed) && FiniteVector(state.moment) &&
           FiniteVector(state.groundNormal) &&
           FiniteVector(state.lastAcceleration) &&
           FiniteVector(state.maximumHorizontalSpeed) &&
           FiniteVector(state.jumpOffset) && FiniteVector(state.jumpSpeed) &&
           FiniteVector(state.walkPendulumTravel) &&
           FiniteDoubles(values, sizeof(values) / sizeof(values[0])) &&
           state.forward >= -1 && state.forward <= 1 &&
           state.braking >= 0 && state.braking <= 8 &&
           state.jumpPhase >= -1 && state.jumpPhase <= 1;
}

bool ValidateVessel(const StableVesselState &state)
{
    return (state.kind == RECOVERED_VEHICLE_VESSEL_EMV &&
            ValidateEmv(state.emv)) ||
           (state.kind == RECOVERED_VEHICLE_VESSEL_WHEELS &&
            ValidateWheels(state.wheels));
}

std::string SymbolicName(SimulationContext *context,
                         const KR_ObjectID &object)
{
    const char *name = context == NULL || IsNul(object)
                           ? NULL : context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

Vehicle *ResolveVehicle(SimulationContext *context,
                        const KR_ObjectID &object)
{
    return context == NULL || IsNul(object) || !context->isExist(object)
               ? NULL
               : static_cast<Vehicle *>(
                     context->queryInterface(object, IVehicleIID));
}

bool CollectVehicle(const KR_ObjectID object, void *user)
{
    VehicleRoster *roster = static_cast<VehicleRoster *>(user);
    Vehicle *vehicle = ResolveVehicle(roster->context, object);
    if (vehicle == NULL || roster->context->searchObject(object) == NULL)
    {
        roster->valid = false;
        return false;
    }
    roster->objects.push_back(vehicle);
    return true;
}

bool CaptureVessel(Vehicle *vehicle, int kind, StableVesselState *state)
{
    if (vehicle == NULL || state == NULL)
        return false;
    const void *native = vehicle->SaveVesselRuntimeState();
    if (native == NULL)
        return false;
    state->kind = kind;
    if (kind == RECOVERED_VEHICLE_VESSEL_EMV)
    {
        std::memcpy(&state->emv, native, sizeof(state->emv));
        return ValidateEmv(state->emv);
    }
    if (kind == RECOVERED_VEHICLE_VESSEL_WHEELS)
    {
        std::memcpy(&state->wheels, native, sizeof(state->wheels));
        return ValidateWheels(state->wheels);
    }
    return false;
}

bool CaptureRecord(SimulationContext *context, Vehicle *vehicle,
                   StableVehicleRecord *record)
{
    if (context == NULL || vehicle == NULL || record == NULL)
        return false;
    SRecoveredVehicleRuntimeState runtime = {};
    const KR_ObjectID object = vehicle->getObjectID();
    const char *attribute = VehicleRuntimeState_AttributeName(context, object);
    if (!VehicleRuntimeState_Inspect(context, object, &runtime) ||
        attribute == NULL)
        return false;
    record->name = SymbolicName(context, object);
    record->attribute = attribute;
    record->defaultAttribute =
        SymbolicName(context, vehicle->m_vehicleAttrDefaultID);
    record->deadAttribute =
        SymbolicName(context, vehicle->m_vehicleAttrDeadID);
    if (record->name.empty() || record->attribute.empty() ||
        record->defaultAttribute.empty() || record->deadAttribute.empty() ||
        !CaptureVessel(vehicle, runtime.vesselKind, &record->vessel))
        return false;

    record->subjectPosition = runtime.subjectPosition;
    record->firePrimary = vehicle->m_firePrim ? 1 : 0;
    record->primaryEnabled = vehicle->m_primEnable ? 1 : 0;
    record->fireSecondary = vehicle->m_fireSec ? 1 : 0;
    record->secondaryEnabled = vehicle->m_secEnable ? 1 : 0;
    record->firePrimaryPressed = vehicle->m_firePrimPress ? 1 : 0;
    record->fireSecondaryPressed = vehicle->m_fireSecPress ? 1 : 0;
    record->damage = vehicle->m_damage;
    record->skipTime = vehicle->m_skipTime;
    record->skipPosition = record->skipTime > 0.0
                               ? vehicle->m_skipPos
                               : CFVector3(0.0, 0.0, 0.0);
    record->secondaryBulletCount = vehicle->m_secBulletCnt;
    record->briefingPlayed = vehicle->m_playedBrief ? 1 : 0;
    record->lastLeaveTime = vehicle->m_lastLeaveTime;
    record->lastTime = vehicle->m_lastTime;

    Player &player = static_cast<Player &>(vehicle->player());
    if (player.m_sideQnty < 0 || player.m_sideQnty > 5)
        return false;
    record->playerDamage = player.m_damage;
    for (int index = 0; index < player.m_sideQnty; ++index)
    {
        StablePlayerStatus status;
        status.commander = SymbolicName(
            context, player.m_playerStatus[index].m_masterID);
        status.damageDifference =
            player.m_playerStatus[index].m_damageDiff;
        status.renegade = player.m_playerStatus[index].m_isRenegat;
        status.missionSuccess =
            player.m_playerStatus[index].m_isMissionSuccess;
        if (status.commander.empty())
            return false;
        record->playerSides.push_back(status);
    }
    std::sort(record->playerSides.begin(), record->playerSides.end(),
              [](const StablePlayerStatus &left,
                 const StablePlayerStatus &right)
              {
                  return left.commander < right.commander;
              });

    record->dead = Vehicle::m_dead ? 1 : 0;
    if (vehicle->panelReady() &&
        vehicle->panelOpen() != (record->dead == 0))
        return false;
    record->currentTime = Vehicle::s_curTime;
    record->takingTaxi = Vehicle::m_isTakingTaxiNow;
    record->taxiCurrentAngle = Vehicle::m_takingTaxiCurrentAngle;
    record->taxiFinalAngle = Vehicle::m_takingTaxiFinalAngle;
    record->lastEventTime = Vehicle::m_lastEventTime;
    record->taxiRotateSpeed = Vehicle::m_taxiRotateSpeed;
    record->spawnX = Vehicle::m_spX;
    record->spawnY = Vehicle::m_spY;
    record->spawnZ = Vehicle::m_spZ;
    record->currentTaxiPosition = Vehicle::m_currentTaxiPos;
    record->currentTaxiVehiclePosition = Vehicle::m_currentTaxiOurPos;
    return true;
}

bool CollectStableRecords(SimulationContext *context,
                          std::vector<StableVehicleRecord> *records)
{
    if (context == NULL || records == NULL)
        return false;
    records->clear();
    const ct_ClassTableID table = g_arena.searchSeanceClassTable("Vehicle");
    if (table == ct_NULLID)
        return true;
    VehicleRoster roster = {context, std::vector<Vehicle *>(), true};
    g_arena.userFind(table, CollectVehicle, &roster);
    if (!roster.valid || roster.objects.size() > kMaximumVehicles)
        return false;
    for (std::size_t index = 0; index < roster.objects.size(); ++index)
    {
        StableVehicleRecord record;
        if (!CaptureRecord(context, roster.objects[index], &record))
            return false;
        records->push_back(record);
    }
    std::sort(records->begin(), records->end(),
              [](const StableVehicleRecord &left,
                 const StableVehicleRecord &right)
              {
                  return left.name < right.name;
              });
    return true;
}

void PutU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

bool GetU32(const std::vector<unsigned char> &bytes, std::size_t *offset,
            std::uint32_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 4)
        return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
    return true;
}

void PutI32(std::vector<unsigned char> *bytes, int value)
{
    PutU32(bytes, static_cast<std::uint32_t>(
                      static_cast<std::int32_t>(value)));
}

bool GetI32(const std::vector<unsigned char> &bytes, std::size_t *offset,
            int *value)
{
    std::uint32_t encoded = 0;
    if (value == NULL || !GetU32(bytes, offset, &encoded))
        return false;
    *value = static_cast<int>(static_cast<std::int32_t>(encoded));
    return true;
}

void PutDouble(std::vector<unsigned char> *bytes, double value)
{
    std::uint64_t encoded = 0;
    std::memcpy(&encoded, &value, sizeof(encoded));
    for (int shift = 0; shift < 64; shift += 8)
        bytes->push_back(static_cast<unsigned char>(encoded >> shift));
}

bool GetDouble(const std::vector<unsigned char> &bytes, std::size_t *offset,
               double *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 8)
        return false;
    std::uint64_t encoded = 0;
    for (int shift = 0; shift < 64; shift += 8)
        encoded |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
    std::memcpy(value, &encoded, sizeof(encoded));
    return true;
}

bool PutString(std::vector<unsigned char> *bytes, const std::string &value)
{
    if (bytes == NULL || value.size() > MAX_SYMBOLIC_LENGHT)
        return false;
    PutU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
}

bool GetString(const std::vector<unsigned char> &bytes, std::size_t *offset,
               std::string *value)
{
    std::uint32_t size = 0;
    if (value == NULL || !GetU32(bytes, offset, &size) ||
        size > MAX_SYMBOLIC_LENGHT || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    value->assign(size == 0 ? "" :
                      reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return true;
}

void PutVector(std::vector<unsigned char> *bytes, const SFVector3 &value)
{
    PutDouble(bytes, value.x);
    PutDouble(bytes, value.y);
    PutDouble(bytes, value.z);
}

bool GetVector(const std::vector<unsigned char> &bytes, std::size_t *offset,
               SFVector3 *value)
{
    return value != NULL && GetDouble(bytes, offset, &value->x) &&
           GetDouble(bytes, offset, &value->y) &&
           GetDouble(bytes, offset, &value->z);
}

void PutMatrix(std::vector<unsigned char> *bytes, const SFMatrix3x4 &value)
{
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            PutDouble(bytes, value.m[row][column]);
}

bool GetMatrix(const std::vector<unsigned char> &bytes, std::size_t *offset,
               SFMatrix3x4 *value)
{
    if (value == NULL)
        return false;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            if (!GetDouble(bytes, offset, &value->m[row][column]))
                return false;
    return true;
}

void PutEmv(std::vector<unsigned char> *bytes,
            const SVehicleEmvSaveState &state)
{
    PutMatrix(bytes, state.direction);
    PutVector(bytes, state.position);
    PutVector(bytes, state.speed);
    const double values[] = {
        state.fuelSpeed, state.throttle, state.verticalRotation,
        state.axisRotation, state.inclination, state.inclinationRotation,
        state.turnRotation, state.strafeInclination,
        state.strafeInclinationRotation, state.verticalStrafe,
        state.mouseTurnRotation, state.mouseVerticalRotation};
    for (std::size_t index = 0; index < sizeof(values) / sizeof(values[0]);
         ++index)
        PutDouble(bytes, values[index]);
}

bool GetEmv(const std::vector<unsigned char> &bytes, std::size_t *offset,
            SVehicleEmvSaveState *state)
{
    if (state == NULL || !GetMatrix(bytes, offset, &state->direction) ||
        !GetVector(bytes, offset, &state->position) ||
        !GetVector(bytes, offset, &state->speed))
        return false;
    double *values[] = {
        &state->fuelSpeed, &state->throttle, &state->verticalRotation,
        &state->axisRotation, &state->inclination,
        &state->inclinationRotation, &state->turnRotation,
        &state->strafeInclination, &state->strafeInclinationRotation,
        &state->verticalStrafe, &state->mouseTurnRotation,
        &state->mouseVerticalRotation};
    for (std::size_t index = 0; index < sizeof(values) / sizeof(values[0]);
         ++index)
        if (!GetDouble(bytes, offset, values[index]))
            return false;
    state->size = sizeof(*state);
    return ValidateEmv(*state);
}

void PutWheels(std::vector<unsigned char> *bytes,
               const SVehicleWheelsSaveState &state)
{
    PutMatrix(bytes, state.direction);
    PutMatrix(bytes, state.base);
    PutVector(bytes, state.position);
    PutVector(bytes, state.speed);
    PutVector(bytes, state.moment);
    PutVector(bytes, state.groundNormal);
    PutVector(bytes, state.lastAcceleration);
    PutDouble(bytes, state.fuelAcceleration);
    PutDouble(bytes, state.throttle);
    PutI32(bytes, state.forward);
    PutU32(bytes, state.brakingDone ? 1u : 0u);
    PutU32(bytes, state.touchingGround ? 1u : 0u);
    PutI32(bytes, state.braking);
    PutDouble(bytes, state.raise);
    PutDouble(bytes, state.raiseSpeed);
    PutDouble(bytes, state.turnSpeed);
    PutVector(bytes, state.maximumHorizontalSpeed);
    const double values[] = {
        state.accelerationFactor, state.jumpOffsetFactor, state.jumpTimeout,
        state.suspensionTravel, state.walkPendulumArgument,
        state.walkPendulumCoefficient, state.walkPendulumSpeedCoefficient,
        state.mouseTurnSpeed, state.mouseRaise, state.mouseRaiseSpeed,
        state.jumpTime, state.jumpAccumulation, state.jumpDownVelocity,
        state.lastFallVelocity};
    for (std::size_t index = 0; index < sizeof(values) / sizeof(values[0]);
         ++index)
        PutDouble(bytes, values[index]);
    PutVector(bytes, state.jumpOffset);
    PutVector(bytes, state.jumpSpeed);
    PutVector(bytes, state.walkPendulumTravel);
    PutI32(bytes, state.jumpPhase);
}

bool GetBool(const std::vector<unsigned char> &bytes, std::size_t *offset,
             int *value)
{
    std::uint32_t encoded = 0;
    if (value == NULL || !GetU32(bytes, offset, &encoded) || encoded > 1u)
        return false;
    *value = static_cast<int>(encoded);
    return true;
}

bool GetWheels(const std::vector<unsigned char> &bytes, std::size_t *offset,
               SVehicleWheelsSaveState *state)
{
    int brakingDone = 0, touchingGround = 0;
    if (state == NULL || !GetMatrix(bytes, offset, &state->direction) ||
        !GetMatrix(bytes, offset, &state->base) ||
        !GetVector(bytes, offset, &state->position) ||
        !GetVector(bytes, offset, &state->speed) ||
        !GetVector(bytes, offset, &state->moment) ||
        !GetVector(bytes, offset, &state->groundNormal) ||
        !GetVector(bytes, offset, &state->lastAcceleration) ||
        !GetDouble(bytes, offset, &state->fuelAcceleration) ||
        !GetDouble(bytes, offset, &state->throttle) ||
        !GetI32(bytes, offset, &state->forward) ||
        !GetBool(bytes, offset, &brakingDone) ||
        !GetBool(bytes, offset, &touchingGround) ||
        !GetI32(bytes, offset, &state->braking) ||
        !GetDouble(bytes, offset, &state->raise) ||
        !GetDouble(bytes, offset, &state->raiseSpeed) ||
        !GetDouble(bytes, offset, &state->turnSpeed) ||
        !GetVector(bytes, offset, &state->maximumHorizontalSpeed))
        return false;
    state->brakingDone = brakingDone != 0;
    state->touchingGround = touchingGround != 0;
    double *values[] = {
        &state->accelerationFactor, &state->jumpOffsetFactor,
        &state->jumpTimeout, &state->suspensionTravel,
        &state->walkPendulumArgument, &state->walkPendulumCoefficient,
        &state->walkPendulumSpeedCoefficient, &state->mouseTurnSpeed,
        &state->mouseRaise, &state->mouseRaiseSpeed, &state->jumpTime,
        &state->jumpAccumulation, &state->jumpDownVelocity,
        &state->lastFallVelocity};
    for (std::size_t index = 0; index < sizeof(values) / sizeof(values[0]);
         ++index)
        if (!GetDouble(bytes, offset, values[index]))
            return false;
    if (!GetVector(bytes, offset, &state->jumpOffset) ||
        !GetVector(bytes, offset, &state->jumpSpeed) ||
        !GetVector(bytes, offset, &state->walkPendulumTravel) ||
        !GetI32(bytes, offset, &state->jumpPhase))
        return false;
    state->size = sizeof(*state);
    return ValidateWheels(*state);
}

bool PutRecord(std::vector<unsigned char> *bytes,
               const StableVehicleRecord &record)
{
    if (!PutString(bytes, record.name) ||
        !PutString(bytes, record.attribute) ||
        !PutString(bytes, record.defaultAttribute) ||
        !PutString(bytes, record.deadAttribute))
        return false;
    PutI32(bytes, record.vessel.kind);
    if (record.vessel.kind == RECOVERED_VEHICLE_VESSEL_EMV)
        PutEmv(bytes, record.vessel.emv);
    else if (record.vessel.kind == RECOVERED_VEHICLE_VESSEL_WHEELS)
        PutWheels(bytes, record.vessel.wheels);
    else
        return false;
    PutVector(bytes, record.subjectPosition);
    PutU32(bytes, record.firePrimary);
    PutU32(bytes, record.primaryEnabled);
    PutU32(bytes, record.fireSecondary);
    PutU32(bytes, record.secondaryEnabled);
    PutU32(bytes, record.firePrimaryPressed);
    PutU32(bytes, record.fireSecondaryPressed);
    PutDouble(bytes, record.damage);
    PutVector(bytes, record.skipPosition);
    PutDouble(bytes, record.skipTime);
    PutI32(bytes, record.secondaryBulletCount);
    PutU32(bytes, record.briefingPlayed);
    PutDouble(bytes, record.lastLeaveTime);
    PutDouble(bytes, record.lastTime);
    PutDouble(bytes, record.playerDamage);
    PutU32(bytes, static_cast<std::uint32_t>(record.playerSides.size()));
    for (std::size_t index = 0; index < record.playerSides.size(); ++index)
    {
        if (!PutString(bytes, record.playerSides[index].commander))
            return false;
        PutDouble(bytes, record.playerSides[index].damageDifference);
        PutI32(bytes, record.playerSides[index].renegade);
        PutI32(bytes, record.playerSides[index].missionSuccess);
    }
    PutU32(bytes, record.dead);
    PutDouble(bytes, record.currentTime);
    PutI32(bytes, record.takingTaxi);
    PutDouble(bytes, record.taxiCurrentAngle);
    PutDouble(bytes, record.taxiFinalAngle);
    PutDouble(bytes, record.lastEventTime);
    PutDouble(bytes, record.taxiRotateSpeed);
    PutDouble(bytes, record.spawnX);
    PutDouble(bytes, record.spawnY);
    PutDouble(bytes, record.spawnZ);
    PutVector(bytes, record.currentTaxiPosition);
    PutVector(bytes, record.currentTaxiVehiclePosition);
    return true;
}

bool GetRecord(const std::vector<unsigned char> &bytes, std::size_t *offset,
               StableVehicleRecord *record)
{
    if (record == NULL || !GetString(bytes, offset, &record->name) ||
        !GetString(bytes, offset, &record->attribute) ||
        !GetString(bytes, offset, &record->defaultAttribute) ||
        !GetString(bytes, offset, &record->deadAttribute) ||
        !GetI32(bytes, offset, &record->vessel.kind))
        return false;
    if (record->vessel.kind == RECOVERED_VEHICLE_VESSEL_EMV)
    {
        if (!GetEmv(bytes, offset, &record->vessel.emv))
            return false;
    }
    else if (record->vessel.kind == RECOVERED_VEHICLE_VESSEL_WHEELS)
    {
        if (!GetWheels(bytes, offset, &record->vessel.wheels))
            return false;
    }
    else
        return false;
    if (!GetVector(bytes, offset, &record->subjectPosition) ||
        !GetBool(bytes, offset, &record->firePrimary) ||
        !GetBool(bytes, offset, &record->primaryEnabled) ||
        !GetBool(bytes, offset, &record->fireSecondary) ||
        !GetBool(bytes, offset, &record->secondaryEnabled) ||
        !GetBool(bytes, offset, &record->firePrimaryPressed) ||
        !GetBool(bytes, offset, &record->fireSecondaryPressed) ||
        !GetDouble(bytes, offset, &record->damage) ||
        !GetVector(bytes, offset, &record->skipPosition) ||
        !GetDouble(bytes, offset, &record->skipTime) ||
        !GetI32(bytes, offset, &record->secondaryBulletCount) ||
        !GetBool(bytes, offset, &record->briefingPlayed) ||
        !GetDouble(bytes, offset, &record->lastLeaveTime) ||
        !GetDouble(bytes, offset, &record->lastTime) ||
        !GetDouble(bytes, offset, &record->playerDamage))
        return false;
    std::uint32_t sideCount = 0;
    if (!GetU32(bytes, offset, &sideCount) ||
        sideCount > kMaximumPlayerSides)
        return false;
    record->playerSides.clear();
    for (std::uint32_t index = 0; index < sideCount; ++index)
    {
        StablePlayerStatus status;
        if (!GetString(bytes, offset, &status.commander) ||
            !GetDouble(bytes, offset, &status.damageDifference) ||
            !GetI32(bytes, offset, &status.renegade) ||
            !GetI32(bytes, offset, &status.missionSuccess))
            return false;
        record->playerSides.push_back(status);
    }
    return GetBool(bytes, offset, &record->dead) &&
           GetDouble(bytes, offset, &record->currentTime) &&
           GetI32(bytes, offset, &record->takingTaxi) &&
           GetDouble(bytes, offset, &record->taxiCurrentAngle) &&
           GetDouble(bytes, offset, &record->taxiFinalAngle) &&
           GetDouble(bytes, offset, &record->lastEventTime) &&
           GetDouble(bytes, offset, &record->taxiRotateSpeed) &&
           GetDouble(bytes, offset, &record->spawnX) &&
           GetDouble(bytes, offset, &record->spawnY) &&
           GetDouble(bytes, offset, &record->spawnZ) &&
           GetVector(bytes, offset, &record->currentTaxiPosition) &&
           GetVector(bytes, offset, &record->currentTaxiVehiclePosition);
}

bool ValidateRecord(const StableVehicleRecord &record)
{
    if (record.name.empty() || record.attribute.empty() ||
        record.defaultAttribute != "Vehicle.Attr.default" ||
        record.deadAttribute != "Vehicle.Attr.dead" ||
        !ValidateVessel(record.vessel) ||
        !FiniteVector(record.subjectPosition) ||
        !std::isfinite(record.damage) || !FiniteVector(record.skipPosition) ||
        !std::isfinite(record.skipTime) ||
        record.secondaryBulletCount < 0 ||
        record.secondaryBulletCount > 1000000 ||
        !std::isfinite(record.lastLeaveTime) ||
        !std::isfinite(record.lastTime) ||
        !std::isfinite(record.playerDamage) ||
        record.takingTaxi < 0 || record.takingTaxi > 1)
        return false;
    const double staticValues[] = {
        record.currentTime, record.taxiCurrentAngle, record.taxiFinalAngle,
        record.lastEventTime, record.taxiRotateSpeed, record.spawnX,
        record.spawnY, record.spawnZ};
    if (!FiniteDoubles(staticValues,
                       sizeof(staticValues) / sizeof(staticValues[0])) ||
        !FiniteVector(record.currentTaxiPosition) ||
        !FiniteVector(record.currentTaxiVehiclePosition))
        return false;
    for (std::size_t index = 0; index < record.playerSides.size(); ++index)
    {
        const StablePlayerStatus &status = record.playerSides[index];
        if (status.commander.empty() ||
            (index != 0 &&
             record.playerSides[index - 1].commander >= status.commander) ||
            !std::isfinite(status.damageDifference) ||
            status.renegade < 0 || status.renegade > 1 ||
            status.missionSuccess < 0 || status.missionSuccess > 1)
            return false;
    }
    return true;
}

bool EncodeRecords(const std::vector<StableVehicleRecord> &records,
                   std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumVehicles)
        return false;
    bytes->clear();
    PutU32(bytes, kVehicleMagic);
    PutU32(bytes, kVehicleVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ValidateRecord(records[index]) ||
            !PutRecord(bytes, records[index]))
            return false;
    return true;
}

bool DecodeRecords(const std::vector<unsigned char> &bytes,
                   std::vector<StableVehicleRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) || magic != kVehicleMagic ||
        version != kVehicleVersion || count > kMaximumVehicles)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableVehicleRecord record;
        if (!GetRecord(bytes, &offset, &record) || !ValidateRecord(record) ||
            (index != 0 && records->back().name >= record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

bool ResolveAttribute(SimulationContext *context, const std::string &name,
                      KR_ObjectID *object)
{
    if (context == NULL || object == NULL || name.empty() ||
        !context->isExist(name.c_str()))
        return false;
    *object = context->searchObject(name.c_str());
    return __attrVehicleTable.searchAttribute(*object) != NULL;
}

bool ResolveCommander(SimulationContext *context, const std::string &name,
                      KR_ObjectID *object)
{
    if (context == NULL || object == NULL || name.empty() ||
        !context->isExist(name.c_str()))
        return false;
    *object = context->searchObject(name.c_str());
    return context->queryInterface(*object, ICommanderIID) != NULL;
}

bool ApplyAttribute(Vehicle *vehicle, const KR_ObjectID &attribute)
{
    if (vehicle == NULL)
        return false;
    KR_Event event;
    event.label = KR_SET_ATTR;
    event.source = g_arena.getObjectID();
    event.destination = vehicle->getObjectID();
    event.timeStamp = 0.0;
    event.data.open(EDO_WRITE).putObjectID(attribute).close();
    return vehicle->receiveEvent(event) == 1 &&
           vehicle->m_vehicleAttrID == attribute &&
           vehicle->VesselMass() > 0.0;
}

bool ApplyRecord(const StableVehicleRecord &record,
                 const ResolvedVehicleRecord &resolved)
{
    Vehicle *vehicle = resolved.vehicle;
    if (!vehicle->reconcilePanelPresentation(false) ||
        !ApplyAttribute(vehicle, resolved.attribute))
        return false;
    const void *vessel = record.vessel.kind == RECOVERED_VEHICLE_VESSEL_EMV
                             ? static_cast<const void *>(&record.vessel.emv)
                             : static_cast<const void *>(&record.vessel.wheels);
    if (!vehicle->LoadVesselRuntimeState(vessel))
        return false;
    vehicle->setPosition(record.subjectPosition);
    vehicle->m_vehicleAttrDefaultID = resolved.defaultAttribute;
    vehicle->m_vehicleAttrDeadID = resolved.deadAttribute;
    vehicle->m_firePrim = record.firePrimary != 0;
    vehicle->m_primEnable = record.primaryEnabled != 0;
    vehicle->m_fireSec = record.fireSecondary != 0;
    vehicle->m_secEnable = record.secondaryEnabled != 0;
    vehicle->m_firePrimPress = record.firePrimaryPressed != 0;
    vehicle->m_fireSecPress = record.fireSecondaryPressed != 0;
    vehicle->m_damage = record.damage;
    vehicle->m_skipPos = record.skipPosition;
    vehicle->m_skipTime = record.skipTime;
    vehicle->m_secBulletCnt = record.secondaryBulletCount;
    vehicle->m_playedBrief = record.briefingPlayed != 0;
    vehicle->m_lastLeaveTime = record.lastLeaveTime;
    vehicle->m_lastTime = record.lastTime;

    Player &player = static_cast<Player &>(vehicle->player());
    player.m_sideQnty = static_cast<int>(record.playerSides.size());
    player.m_damage = record.playerDamage;
    for (int index = 0; index < player.m_sideQnty; ++index)
    {
        player.m_playerStatus[index].m_masterID =
            resolved.playerCommanders[index];
        player.m_playerStatus[index].m_damageDiff =
            record.playerSides[index].damageDifference;
        player.m_playerStatus[index].m_isRenegat =
            record.playerSides[index].renegade;
        player.m_playerStatus[index].m_isMissionSuccess =
            record.playerSides[index].missionSuccess;
    }

    Vehicle::m_dead = record.dead != 0;
    Vehicle::s_curTime = record.currentTime;
    Vehicle::m_isTakingTaxiNow = record.takingTaxi;
    Vehicle::m_takingTaxiCurrentAngle = record.taxiCurrentAngle;
    Vehicle::m_takingTaxiFinalAngle = record.taxiFinalAngle;
    Vehicle::m_lastEventTime = record.lastEventTime;
    Vehicle::m_taxiRotateSpeed = record.taxiRotateSpeed;
    Vehicle::m_spX = record.spawnX;
    Vehicle::m_spY = record.spawnY;
    Vehicle::m_spZ = record.spawnZ;
    Vehicle::m_currentTaxiPos = record.currentTaxiPosition;
    Vehicle::m_currentTaxiOurPos = record.currentTaxiVehiclePosition;
    if (!vehicle->reconcilePanelPresentation(record.dead == 0))
        return false;
    if (record.name == "Vehicle.Default")
        g_vehicle = vehicle;
    return true;
}

void HashBytes(unsigned long long *hash, const void *data, std::size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        *hash ^= bytes[index];
        *hash *= 1099511628211ull;
    }
}

} // namespace

void VehicleActiveWorldState_Link()
{
    (void)__classTable.getClassTableID();
}

int VehicleActiveWorldState_LiveCount(SimulationContext *context)
{
    std::vector<StableVehicleRecord> records;
    return CollectStableRecords(context, &records)
               ? static_cast<int>(records.size()) : -1;
}

unsigned long long VehicleActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!VehicleActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = 14695981039346656037ull;
    if (!bytes.empty())
        HashBytes(&hash, &bytes[0], bytes.size());
    return hash;
}

bool VehicleActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    std::vector<StableVehicleRecord> records;
    return CollectStableRecords(context, &records) &&
           EncodeRecords(records, bytes);
}

bool VehicleActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableVehicleRecord> records;
    return DecodeRecords(bytes, &records);
}

bool VehicleActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return VehicleActiveWorldState_ValidateStable(bytes) &&
           VehicleActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool VehicleActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableVehicleRecord> records;
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeRecords(bytes, &records))
        return false;
    if (records.empty())
        return true;
    int missing = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!context->isExist(records[index].name.c_str()))
            ++missing;
    if (g_arena.searchSeanceClassTable("Vehicle") == ct_NULLID ||
        missing > __classTable.freeObjectCount())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        KR_ObjectID dependency;
        if (!ResolveAttribute(context, records[index].attribute, &dependency) ||
            !ResolveAttribute(context, records[index].defaultAttribute,
                              &dependency) ||
            !ResolveAttribute(context, records[index].deadAttribute,
                              &dependency))
            return false;
    }
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const char *name = records[index].name.c_str();
        if (context->isExist(name))
        {
            if (ResolveVehicle(context, context->searchObject(name)) == NULL)
            {
                VehicleActiveWorldState_RemoveStableOwners(context, created);
                return false;
            }
            continue;
        }
        KR_ObjectID object = g_arena.newObject("Vehicle", name);
        if (IsNul(object))
        {
            VehicleActiveWorldState_RemoveStableOwners(context, created);
            return false;
        }
        created->push_back(object);
    }
    return true;
}

bool VehicleActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableVehicleRecord> records;
    if (context == NULL || !DecodeRecords(bytes, &records))
        return false;
    std::vector<ResolvedVehicleRecord> resolved(records.size());
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const StableVehicleRecord &record = records[index];
        if (!context->isExist(record.name.c_str()))
            return false;
        resolved[index].vehicle = ResolveVehicle(
            context, context->searchObject(record.name.c_str()));
        if (resolved[index].vehicle == NULL ||
            !ResolveAttribute(context, record.attribute,
                              &resolved[index].attribute) ||
            !ResolveAttribute(context, record.defaultAttribute,
                              &resolved[index].defaultAttribute) ||
            !ResolveAttribute(context, record.deadAttribute,
                              &resolved[index].deadAttribute))
            return false;
        for (std::size_t side = 0; side < record.playerSides.size(); ++side)
        {
            KR_ObjectID commander;
            if (!ResolveCommander(context,
                                  record.playerSides[side].commander,
                                  &commander))
                return false;
            resolved[index].playerCommanders.push_back(commander);
        }
    }
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ApplyRecord(records[index], resolved[index]))
            return false;
    return VehicleActiveWorldState_MatchesStable(context, bytes);
}

void VehicleActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
    {
        VehicleRuntimeState_Reset(context);
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            if (g_vehicle != NULL && g_vehicle->getObjectID() == *object)
                g_vehicle = NULL;
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    }
    created->clear();
}
