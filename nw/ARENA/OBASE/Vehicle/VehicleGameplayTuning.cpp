#include "VehicleGameplayTuning.h"

#include <cstring>

#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "Vehicle.h"
#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "phisics.h"
#include "vh_vessel.h"
#include "vs_zav.h"

extern SEmvAttrs g_emvAttr0;
extern SEmvAttrs g_emvAttrDragon;
extern SWheelsAttrs g_walkAttr0;
extern SWheelsAttrs g_tankAttr1;
extern SWheelsAttrs g_tankAttr2;
extern SWheelsAttrs g_tankAttr3;

namespace
{

struct DynamicView
{
    SEmvAttrs *emv;
    SWheelsAttrs *wheels;
};

DynamicView FindDynamic(const char *dynamic)
{
    DynamicView result = {};
    if (dynamic == NULL)
        return result;
    if (std::strcmp(dynamic, "Dragon") == 0)
        result.emv = &g_emvAttrDragon;
    else if (std::strcmp(dynamic, "Emveshka") == 0)
        result.emv = &g_emvAttr0;
    else if (std::strcmp(dynamic, "TankGenn0") == 0)
        result.wheels = &g_walkAttr0;
    else if (std::strcmp(dynamic, "TankGenn1") == 0)
        result.wheels = &g_tankAttr1;
    else if (std::strcmp(dynamic, "TankGenn2") == 0)
        result.wheels = &g_tankAttr2;
    else if (std::strcmp(dynamic, "TankGenn3") == 0)
        result.wheels = &g_tankAttr3;
    return result;
}

bool ReadDynamic(const char *dynamic, SVehicleVesselGameplayState *state)
{
    if (state == NULL)
        return false;
    const DynamicView view = FindDynamic(dynamic);
    if (view.emv == NULL && view.wheels == NULL)
        return false;
    std::memset(state, 0, sizeof(*state));
    std::strncpy(state->dynamic, dynamic, sizeof(state->dynamic) - 1);
    if (view.emv != NULL)
    {
        state->maxSpeed = view.emv->fMaxSpeed;
        state->reverseSpeed = view.emv->fMaxBackSpeed;
        state->accelerationTime = view.emv->fMaxSpeedTime;
        state->turnSpeed = view.emv->fMaxTurnSpeed;
    }
    else
    {
        state->maxSpeed = view.wheels->fMaxSpeed;
        state->reverseSpeed = view.wheels->fMaxBackSpeed;
        state->accelerationTime = view.wheels->fMaxSpeedTime;
        state->turnSpeed = view.wheels->fMaxTurnSpeed;
    }
    return true;
}

bool WriteDynamic(const char *dynamic,
                  bool maxSpeedSet, double maxSpeed,
                  bool reverseSpeedSet, double reverseSpeed,
                  bool accelerationTimeSet, double accelerationTime,
                  bool turnSpeedSet, double turnSpeed)
{
    const DynamicView view = FindDynamic(dynamic);
    if (view.emv == NULL && view.wheels == NULL)
        return false;
    if (view.emv != NULL)
    {
        if (maxSpeedSet) view.emv->fMaxSpeed = maxSpeed;
        if (reverseSpeedSet) view.emv->fMaxBackSpeed = reverseSpeed;
        if (accelerationTimeSet)
            view.emv->fMaxSpeedTime = accelerationTime;
        if (turnSpeedSet) view.emv->fMaxTurnSpeed = turnSpeed;
        view.emv->update();
    }
    else
    {
        if (maxSpeedSet) view.wheels->fMaxSpeed = maxSpeed;
        if (reverseSpeedSet) view.wheels->fMaxBackSpeed = reverseSpeed;
        if (accelerationTimeSet)
            view.wheels->fMaxSpeedTime = accelerationTime;
        if (turnSpeedSet) view.wheels->fMaxTurnSpeed = turnSpeed;
        view.wheels->update();
    }
    return true;
}

}  // namespace

bool VehicleGameplayTuning_SupportsDynamic(const char *dynamic)
{
    const DynamicView view = FindDynamic(dynamic);
    return view.emv != NULL || view.wheels != NULL;
}

bool VehicleGameplayTuning_Capture(
    const char *dynamic, SVehicleVesselGameplayState *state)
{
    return ReadDynamic(dynamic, state);
}

bool VehicleGameplayTuning_Apply(
    const char *dynamic, const SVehicleVesselGameplayPatch *patch)
{
    if (patch == NULL)
        return false;
    return WriteDynamic(dynamic,
                        patch->hasMaxSpeed, patch->maxSpeed,
                        patch->hasReverseSpeed, patch->reverseSpeed,
                        patch->hasAccelerationTime,
                        patch->accelerationTime,
                        patch->hasTurnSpeed, patch->turnSpeed);
}

bool VehicleGameplayTuning_Restore(
    const SVehicleVesselGameplayState *state)
{
    if (state == NULL)
        return false;
    return WriteDynamic(state->dynamic,
                        true, state->maxSpeed,
                        true, state->reverseSpeed,
                        true, state->accelerationTime,
                        true, state->turnSpeed);
}
