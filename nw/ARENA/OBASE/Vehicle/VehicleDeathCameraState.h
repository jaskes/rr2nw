#ifndef RR2NW_VEHICLE_DEATH_CAMERA_STATE_H
#define RR2NW_VEHICLE_DEATH_CAMERA_STATE_H

#include "mathlib.h"

enum ERecoveredVehicleDeathCameraStep
{
    RECOVERED_VEHICLE_DEATH_CAMERA_INVALID = 0,
    RECOVERED_VEHICLE_DEATH_CAMERA_ASCENDING = 1,
    RECOVERED_VEHICLE_DEATH_CAMERA_COMPLETE = 2
};

// Advances only the renderer-independent offset owned by the historical
// Vehicle static state. The caller retains camera orientation and matrix
// composition. Invalid input is rejected without mutating either value.
int VehicleDeathCameraState_Advance(
    double currentMoment, double hazeMinimum, double hazeMaximum,
    double liftSpeed, double maximumStep, double *lastMoment,
    CFVector3 *cameraOffset);

bool VehicleDeathCameraState_IsComplete(
    double cameraOffsetY, double hazeMinimum, double hazeMaximum);

#endif
