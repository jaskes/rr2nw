#include "PeopleContactResponse.h"

#include <cmath>

namespace
{

const double kPi = 3.1415926535897932384626433832795;
const double kTwoPi = kPi * 2.0;
const double kTenDegrees = kPi / 18.0;
const double kReducedRollSpeed = 0.8;

double NormalizeAngle(double angle)
{
    while (angle >= kPi)
        angle -= kTwoPi;
    while (angle <= -kPi)
        angle += kTwoPi;
    return angle;
}

double InterpolateAngle(double source, double destination,
                        double speed, double time)
{
    double difference = destination - source;
    if (difference >= kPi)
        difference -= kTwoPi;
    if (difference <= -kPi)
        difference += kTwoPi;

    double step = speed * time;
    const double distance = std::fabs(difference);
    if (step > distance)
        step = distance;
    source += difference > 0.0 ? step : -step;
    return NormalizeAngle(source);
}

bool Near(double left, double right)
{
    return std::fabs(left - right) <= 1e-9;
}

} // namespace

bool PeopleContactResponse_IsSupportedCode(int contactCode)
{
    return contactCode == 0 || contactCode == 1 || contactCode == 2 ||
           contactCode == 3 || contactCode == 4 || contactCode == 9 ||
           contactCode == 11;
}

bool PeopleContactResponse_Advance(
    const SPeopleContactResponseRequest &request,
    SPeopleContactResponseResult *result)
{
    if (result == NULL ||
        !PeopleContactResponse_IsSupportedCode(request.contactCode) ||
        !std::isfinite(request.heading) ||
        !std::isfinite(request.contactHeading) ||
        !std::isfinite(request.rollSpeed) ||
        !std::isfinite(request.deltaTime) || request.rollSpeed < 0.0 ||
        request.deltaTime < 0.0)
        return false;

    result->active = request.contactCode != 0 ? 1 : 0;
    result->targetHeading = NormalizeAngle(request.heading);
    result->angularSpeed = 0.0;
    result->heading = NormalizeAngle(request.heading);
    if (!result->active)
        return true;

    double speedScale = kReducedRollSpeed;
    switch (request.contactCode)
    {
    case 1:
    case 9:
        result->targetHeading =
            NormalizeAngle(request.heading - kTenDegrees);
        break;
    case 2:
        result->targetHeading =
            NormalizeAngle(request.heading + kTenDegrees);
        speedScale = 1.0;
        break;
    case 3:
    case 11:
        result->targetHeading =
            NormalizeAngle(request.heading + kTenDegrees);
        break;
    case 4:
        result->targetHeading = NormalizeAngle(request.contactHeading);
        break;
    default:
        return false;
    }

    result->angularSpeed = request.rollSpeed * speedScale;
    result->heading = InterpolateAngle(
        request.heading, result->targetHeading,
        result->angularSpeed, request.deltaTime);
    return true;
}

bool PeopleContactResponse_Probe()
{
    SPeopleContactResponseRequest request = {};
    request.heading = 0.0;
    request.contactHeading = 0.2;
    request.rollSpeed = 1.0;
    request.deltaTime = 0.05;

    SPeopleContactResponseResult result = {};
    request.contactCode = 0;
    if (!PeopleContactResponse_Advance(request, &result) || result.active != 0 ||
        !Near(result.heading, 0.0))
        return false;

    const int clockwiseCodes[] = {1, 9};
    for (int code : clockwiseCodes)
    {
        request.contactCode = code;
        if (!PeopleContactResponse_Advance(request, &result) ||
            result.active != 1 || !Near(result.angularSpeed, 0.8) ||
            !Near(result.heading, -0.04))
            return false;
    }

    request.contactCode = 2;
    if (!PeopleContactResponse_Advance(request, &result) ||
        !Near(result.angularSpeed, 1.0) || !Near(result.heading, 0.05))
        return false;

    const int counterClockwiseCodes[] = {3, 11};
    for (int code : counterClockwiseCodes)
    {
        request.contactCode = code;
        if (!PeopleContactResponse_Advance(request, &result) ||
            !Near(result.angularSpeed, 0.8) || !Near(result.heading, 0.04))
            return false;
    }

    request.contactCode = 4;
    if (!PeopleContactResponse_Advance(request, &result) ||
        !Near(result.targetHeading, 0.2) || !Near(result.heading, 0.04))
        return false;

    request.heading = kPi - 0.02;
    request.contactCode = 3;
    if (!PeopleContactResponse_Advance(request, &result) ||
        !Near(result.heading, -kPi + 0.02))
        return false;

    request.contactCode = 5;
    return !PeopleContactResponse_Advance(request, &result) &&
           !PeopleContactResponse_IsSupportedCode(-1) &&
           !PeopleContactResponse_IsSupportedCode(10) &&
           !PeopleContactResponse_IsSupportedCode(12);
}
