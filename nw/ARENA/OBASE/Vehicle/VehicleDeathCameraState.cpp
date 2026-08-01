#include "VehicleDeathCameraState.h"

#include <algorithm>
#include <cmath>

namespace
{

const double kCompletionEpsilon = 1.0e-9;

bool CameraLimit(double hazeMinimum, double hazeMaximum, double *limit)
{
    if (limit == NULL || !std::isfinite(hazeMinimum) ||
        !std::isfinite(hazeMaximum) || hazeMinimum < 0.0 ||
        hazeMaximum < 0.0)
        return false;
    const double value = hazeMinimum + hazeMaximum;
    if (!std::isfinite(value) || value <= 0.0)
        return false;
    *limit = value;
    return true;
}

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

} // namespace

bool VehicleDeathCameraState_IsComplete(
    double cameraOffsetY, double hazeMinimum, double hazeMaximum)
{
    double limit = 0.0;
    return std::isfinite(cameraOffsetY) &&
           CameraLimit(hazeMinimum, hazeMaximum, &limit) &&
           -cameraOffsetY >= limit - kCompletionEpsilon;
}

int VehicleDeathCameraState_Advance(
    double currentMoment, double hazeMinimum, double hazeMaximum,
    double liftSpeed, double maximumStep, double *lastMoment,
    CFVector3 *cameraOffset)
{
    double limit = 0.0;
    if (lastMoment == NULL || cameraOffset == NULL ||
        !std::isfinite(currentMoment) || !std::isfinite(*lastMoment) ||
        !std::isfinite(liftSpeed) || liftSpeed <= 0.0 ||
        !std::isfinite(maximumStep) || maximumStep <= 0.0 ||
        !FiniteVector(*cameraOffset) ||
        !CameraLimit(hazeMinimum, hazeMaximum, &limit))
        return RECOVERED_VEHICLE_DEATH_CAMERA_INVALID;

    const double elapsed = currentMoment - *lastMoment;
    if (!std::isfinite(elapsed) || elapsed < 0.0)
        return RECOVERED_VEHICLE_DEATH_CAMERA_INVALID;

    const double boundedElapsed = (std::min)(elapsed, maximumStep);
    const double currentHeight = -cameraOffset->y;
    const double remaining = limit - currentHeight;
    if (remaining <= kCompletionEpsilon)
    {
        cameraOffset->y = -limit;
        *lastMoment = currentMoment;
        return RECOVERED_VEHICLE_DEATH_CAMERA_COMPLETE;
    }

    const double lift = (std::min)(remaining, liftSpeed * boundedElapsed);
    if (!std::isfinite(lift) || lift < 0.0)
        return RECOVERED_VEHICLE_DEATH_CAMERA_INVALID;
    cameraOffset->y -= lift;
    *lastMoment = currentMoment;
    if (VehicleDeathCameraState_IsComplete(
            cameraOffset->y, hazeMinimum, hazeMaximum))
    {
        cameraOffset->y = -limit;
        return RECOVERED_VEHICLE_DEATH_CAMERA_COMPLETE;
    }
    return RECOVERED_VEHICLE_DEATH_CAMERA_ASCENDING;
}
