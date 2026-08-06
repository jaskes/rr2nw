#include "PeopleObstacleRecovery.h"

#include <algorithm>
#include <cmath>

namespace
{

const double kMinimumSweepRadius = 0.01;
const double kRadiusDecayPerSecond = 2.0;
const double kRecoveryDecayPerSecond = 2.0;
const double kContactTravelFraction = 0.8;
const double kStaticSideRetrySeconds = 1.0;

bool SupportedContactCode(int code)
{
    return code == 0 || code == 1 || code == 2 || code == 3 || code == 4 ||
           code == 9 || code == 11;
}

bool Near(double left, double right)
{
    return std::fabs(left - right) <= 1e-9;
}

} // namespace

bool PeopleObstacleRecovery_Advance(
    const SPeopleObstacleRecoveryRequest &request,
    SPeopleObstacleRecoveryResult *result)
{
    if (result == NULL || !std::isfinite(request.objectRadius) ||
        !std::isfinite(request.maximumRadius) ||
        !std::isfinite(request.recoveryTime) ||
        !std::isfinite(request.deltaTime) ||
        !std::isfinite(request.collisionTime) ||
        request.objectRadius < 0.0 || request.maximumRadius < 0.0 ||
        request.recoveryTime < 0.0 || request.deltaTime < 0.0 ||
        !SupportedContactCode(request.contactCode) ||
        !SupportedContactCode(request.detectedContactCode) ||
        (request.collisionDetected && request.detectedContactCode == 0) ||
        request.stateDepth < 0)
        return false;

    result->sweepRadius = std::min(
        request.objectRadius -
            kRadiusDecayPerSecond * request.recoveryTime,
        request.maximumRadius);
    result->sweepEnabled = request.visible != 0 &&
        result->sweepRadius >= kMinimumSweepRadius ? 1 : 0;
    result->popState = 0;
    result->contactCode = request.contactCode;
    result->recoveryTime = request.recoveryTime;
    result->travelTime = request.deltaTime;

    if (result->sweepRadius < kMinimumSweepRadius)
    {
        result->recoveryTime = 0.0;
        result->contactCode = 0;
        result->popState = request.stateDepth > 0 ? 1 : 0;
        return true;
    }

    if (result->sweepEnabled && request.collisionDetected)
    {
        result->recoveryTime += request.deltaTime;
        result->contactCode = request.contactCode != 0
            ? request.contactCode : request.detectedContactCode;
        // A forward static sweep has no dynamic owner from which to derive a
        // preferred side. Retry the opposite horizontal class after one
        // continuous second instead of orbiting indefinitely on class 1.
        if (result->contactCode == 1 &&
            result->recoveryTime >= kStaticSideRetrySeconds)
            result->contactCode = 3;
        result->popState = request.stateDepth > 0 ? 1 : 0;
        const double boundedCollisionTime = std::max(
            0.0, std::min(request.collisionTime, request.deltaTime));
        result->travelTime =
            boundedCollisionTime * kContactTravelFraction;
        return true;
    }

    result->recoveryTime = std::max(
        0.0, result->recoveryTime -
            request.deltaTime * kRecoveryDecayPerSecond);
    if (result->recoveryTime <= 0.0)
    {
        result->recoveryTime = 0.0;
        result->contactCode = 0;
    }
    return true;
}

bool PeopleObstacleRecovery_Probe()
{
    SPeopleObstacleRecoveryRequest request = {};
    request.objectRadius = 3.0;
    request.maximumRadius = 2.5;
    request.recoveryTime = 0.25;
    request.deltaTime = 0.2;
    request.collisionTime = 0.1;
    request.visible = 1;
    request.collisionDetected = 1;
    request.contactCode = 0;
    request.detectedContactCode = 3;
    request.stateDepth = 2;

    SPeopleObstacleRecoveryResult hit = {};
    if (!PeopleObstacleRecovery_Advance(request, &hit) ||
        !Near(hit.sweepRadius, 2.5) || !Near(hit.recoveryTime, 0.45) ||
        !Near(hit.travelTime, 0.08) || hit.sweepEnabled != 1 ||
        hit.popState != 1 || hit.contactCode != 3)
        return false;

    request.recoveryTime = 0.9;
    request.deltaTime = 0.2;
    request.contactCode = 1;
    request.detectedContactCode = 1;
    SPeopleObstacleRecoveryResult retried = {};
    if (!PeopleObstacleRecovery_Advance(request, &retried) ||
        !Near(retried.recoveryTime, 1.1) || retried.contactCode != 3)
        return false;

    request.collisionDetected = 0;
    request.contactCode = hit.contactCode;
    request.recoveryTime = hit.recoveryTime;
    SPeopleObstacleRecoveryResult clear = {};
    if (!PeopleObstacleRecovery_Advance(request, &clear) ||
        !Near(clear.recoveryTime, 0.05) || clear.contactCode != 3 ||
        clear.popState != 0)
        return false;

    request.recoveryTime = clear.recoveryTime;
    SPeopleObstacleRecoveryResult recovered = {};
    if (!PeopleObstacleRecovery_Advance(request, &recovered) ||
        !Near(recovered.recoveryTime, 0.0) ||
        recovered.contactCode != 0)
        return false;

    request.objectRadius = 0.4;
    request.maximumRadius = 100.0;
    request.recoveryTime = 0.2;
    request.collisionDetected = 0;
    request.contactCode = 9;
    SPeopleObstacleRecoveryResult exhausted = {};
    if (!PeopleObstacleRecovery_Advance(request, &exhausted) ||
        exhausted.sweepEnabled != 0 || exhausted.popState != 1 ||
        exhausted.contactCode != 0 ||
        !Near(exhausted.recoveryTime, 0.0))
        return false;

    request.objectRadius = 3.0;
    request.recoveryTime = 0.25;
    request.visible = 0;
    request.contactCode = 11;
    SPeopleObstacleRecoveryResult invisible = {};
    return PeopleObstacleRecovery_Advance(request, &invisible) &&
           invisible.sweepEnabled == 0 && invisible.popState == 0 &&
           Near(invisible.recoveryTime, 0.0) &&
           invisible.contactCode == 0;
}
