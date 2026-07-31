#ifndef RR2NW_VEHICLE_GAMEPLAY_TUNING_H
#define RR2NW_VEHICLE_GAMEPLAY_TUNING_H

struct SVehicleVesselGameplayPatch
{
    bool hasMaxSpeed;
    bool hasReverseSpeed;
    bool hasAccelerationTime;
    bool hasTurnSpeed;
    double maxSpeed;
    double reverseSpeed;
    double accelerationTime;
    double turnSpeed;
};

struct SVehicleVesselGameplayState
{
    char dynamic[32];
    double maxSpeed;
    double reverseSpeed;
    double accelerationTime;
    double turnSpeed;
};

// Keeps the 1999 dynamic-name/global-layout knowledge inside Vehicle. Dead and
// unknown script dynamics are deliberately not admitted as tuning targets.
bool VehicleGameplayTuning_SupportsDynamic(const char *dynamic);
bool VehicleGameplayTuning_Capture(
    const char *dynamic, SVehicleVesselGameplayState *state);
bool VehicleGameplayTuning_Apply(
    const char *dynamic, const SVehicleVesselGameplayPatch *patch);
bool VehicleGameplayTuning_Restore(
    const SVehicleVesselGameplayState *state);

#endif
