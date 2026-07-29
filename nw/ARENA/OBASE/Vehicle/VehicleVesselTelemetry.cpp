#define LAST_H__VIEW

#include <cstring>

#include "game.h"
#include "scene.h"
#include "Vehicle.h"
#include "phisics.h"
#include "vh_vessel.h"
#include "vs_zav.h"

#include "VehicleVesselTelemetry.h"

extern CVesselEmv g_emv;
extern CVesselWheels g_tank;
extern CVesselWheels g_walk;

int RecoveredVehicleVesselBumpFlags(const char *dynamic)
{
    if (dynamic == NULL)
        return BF_NONE;
    int flags = BF_NONE;
    if (std::strcmp(dynamic, "Dragon") == 0 ||
        std::strcmp(dynamic, "Emveshka") == 0)
        flags = g_emv.GetBumpDef().nBumpFlags;
    else if (std::strcmp(dynamic, "TankGenn0") == 0 ||
             std::strcmp(dynamic, "Dead") == 0)
        flags = g_walk.GetBumpDef().nBumpFlags;
    else if (std::strcmp(dynamic, "TankGenn1") == 0 ||
             std::strcmp(dynamic, "TankGenn2") == 0 ||
             std::strcmp(dynamic, "TankGenn3") == 0)
        flags = g_tank.GetBumpDef().nBumpFlags;
    return flags >= BF_NONE && flags <= BF_BUMPSHELTER
               ? flags : BF_NONE;
}

bool RecoveredVehicleVesselTouchesGround(const char *dynamic)
{
    if (dynamic == NULL)
        return false;
    if (std::strcmp(dynamic, "Dragon") == 0 ||
        std::strcmp(dynamic, "Emveshka") == 0)
        return g_emv.TouchingGround();
    if (std::strcmp(dynamic, "TankGenn0") == 0 ||
        std::strcmp(dynamic, "Dead") == 0)
        return g_walk.TouchingGround();
    if (std::strcmp(dynamic, "TankGenn1") == 0 ||
        std::strcmp(dynamic, "TankGenn2") == 0 ||
        std::strcmp(dynamic, "TankGenn3") == 0)
        return g_tank.TouchingGround();
    return false;
}
