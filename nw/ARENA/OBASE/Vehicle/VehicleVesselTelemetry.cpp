#define LAST_H__VIEW

#include <cstring>

#include "game.h"
#include "scene.h"
#include "Vehicle.h"
#include "phisics.h"
#include "vh_vessel.h"
#include "vs_zav.h"

#include "VehicleRuntimeState.h"
#include "VehicleVesselTelemetry.h"

extern CVesselEmv g_emv;
extern CVesselWheels g_tank;
extern CVesselWheels g_walk;

namespace
{
bool UsesTankVessel(int profile)
{
    return profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN1 ||
           profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN2 ||
           profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN3 ||
           profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN4 ||
           profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN5;
}
}

int RecoveredVehicleVesselBumpFlags(const char *dynamic)
{
    const int profile = VehicleRuntimeState_VesselProfile(dynamic);
    int flags = BF_NONE;
    if (profile == RECOVERED_VEHICLE_PROFILE_DRAGON ||
        profile == RECOVERED_VEHICLE_PROFILE_EMVESHKA ||
        profile == RECOVERED_VEHICLE_PROFILE_EMVESHKA1)
        flags = g_emv.GetBumpDef().nBumpFlags;
    else if (profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN0 ||
             profile == RECOVERED_VEHICLE_PROFILE_DEAD)
        flags = g_walk.GetBumpDef().nBumpFlags;
    else if (UsesTankVessel(profile))
        flags = g_tank.GetBumpDef().nBumpFlags;
    return flags >= BF_NONE && flags <= BF_BUMPSHELTER
               ? flags : BF_NONE;
}

bool RecoveredVehicleVesselTouchesGround(const char *dynamic)
{
    const int profile = VehicleRuntimeState_VesselProfile(dynamic);
    if (profile == RECOVERED_VEHICLE_PROFILE_DRAGON ||
        profile == RECOVERED_VEHICLE_PROFILE_EMVESHKA ||
        profile == RECOVERED_VEHICLE_PROFILE_EMVESHKA1)
        return g_emv.TouchingGround();
    if (profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN0 ||
        profile == RECOVERED_VEHICLE_PROFILE_DEAD)
        return g_walk.TouchingGround();
    if (UsesTankVessel(profile))
        return g_tank.TouchingGround();
    return false;
}

bool RecoveredVehicleVesselWheelsSurface(
    const char *dynamic, SRecoveredWheelsSurfaceTelemetry *telemetry)
{
    if (telemetry == NULL)
        return false;
    std::memset(telemetry, 0, sizeof(*telemetry));
    const int profile = VehicleRuntimeState_VesselProfile(dynamic);
    const CVesselWheels *wheels = NULL;
    if (profile == RECOVERED_VEHICLE_PROFILE_TANK_GENN0 ||
        profile == RECOVERED_VEHICLE_PROFILE_DEAD)
        wheels = &g_walk;
    else if (UsesTankVessel(profile))
        wheels = &g_tank;
    if (wheels == NULL)
        return false;

    const CFVector3 ground = wheels->GroundNormal();
    const CFMatrix3x4 &base = wheels->BaseDirection();
    const CFVector3 forwardAxis(base.m[2][2], 0.0, -base.m[2][0]);
    const CFVector3 rightAxis(-base.m[0][2], 0.0, base.m[0][0]);
    const CFVector3 forwardTangent = ground % forwardAxis;
    const CFVector3 rightTangent = ground % rightAxis;
    const CFVector3 forwardNormal = Normal(forwardTangent);
    const CFVector3 rightNormal = Normal(rightTangent);
    telemetry->ready = 1;
    telemetry->groundX = ground.x;
    telemetry->groundY = ground.y;
    telemetry->groundZ = ground.z;
    telemetry->groundLength = Abs(ground);
    telemetry->forwardTangentLength = Abs(forwardTangent);
    telemetry->rightTangentLength = Abs(rightTangent);
    telemetry->tangentDot = forwardNormal * rightNormal;
    telemetry->suspensionTravel = wheels->SuspensionTravel();
    telemetry->accelerationFactor = wheels->AccelerationFactor();
    telemetry->throttle = wheels->ThrottleValue();
    return true;
}
