#include "PeopleSupportSampling.h"

#include <algorithm>
#include <cmath>

namespace
{

const double kMinimumOffset = 0.3;
const double kOffsetRadiusScale = 0.3;
const double kMaximumBoundedOffset = 12.0;
const double kSweepRadiusScale = 0.4;
const double kMinimumSweepRadius = 1.0;
const double kMaximumSweepRadius = 12.0;
const double kOffsetHeightScale = 1.4;
const double kSweepHeightScale = 1.1;
const double kSweepSpeed = 100.0;
const double kSweepTimeDivisor = 45.0;

bool Near(double left, double right)
{
    return std::fabs(left - right) <= 1e-9;
}

bool Finite(double value)
{
    return std::isfinite(value) != 0;
}

} // namespace

bool PeopleSupportSampling_Build(
    const SPeopleSupportSamplingRequest &request,
    SPeopleSupportSamplingResult *result)
{
    if (result == NULL || !Finite(request.objectRadius) ||
        !Finite(request.heading) || !Finite(request.positionX) ||
        !Finite(request.positionY) || !Finite(request.positionZ) ||
        request.objectRadius < 0.0)
        return false;

    result->sampleOffset = std::max(
        request.objectRadius * kOffsetRadiusScale, kMinimumOffset);
    result->boundedOffset = std::min(
        result->sampleOffset, kMaximumBoundedOffset);
    result->sweepRadius = std::max(
        kMinimumSweepRadius,
        std::min(request.objectRadius * kSweepRadiusScale,
                 kMaximumSweepRadius));
    result->sampleHeight =
        result->boundedOffset * kOffsetHeightScale +
        result->sweepRadius * kSweepHeightScale;
    result->sweepTime = result->sampleHeight / kSweepTimeDivisor;
    if (!Finite(result->sampleOffset) || !Finite(result->boundedOffset) ||
        !Finite(result->sweepRadius) || !Finite(result->sampleHeight) ||
        !Finite(result->sweepTime))
        return false;

    const double forwardX = std::cos(request.heading);
    const double forwardZ = std::sin(request.heading);
    const double offsetX = forwardX * result->sampleOffset;
    const double offsetZ = forwardZ * result->sampleOffset;
    result->frontX = request.positionX + offsetX;
    result->frontY = request.positionY + result->sampleHeight;
    result->frontZ = request.positionZ + offsetZ;
    result->rearX = request.positionX - offsetX;
    result->rearY = result->frontY;
    result->rearZ = request.positionZ - offsetZ;
    result->sweepVelocityX = 0.0;
    result->sweepVelocityY = -kSweepSpeed;
    result->sweepVelocityZ = 0.0;
    return true;
}

bool PeopleSupportSampling_ClassifyDynamicSide(
    double actorX, double actorZ, double objectX, double objectZ,
    double heading, int positiveCode, int nonPositiveCode,
    int *contactCode)
{
    if (contactCode == NULL || !Finite(actorX) || !Finite(actorZ) ||
        !Finite(objectX) || !Finite(objectZ) || !Finite(heading) ||
        positiveCode == 0 || nonPositiveCode == 0)
        return false;

    const double deltaX = objectX - actorX;
    const double deltaZ = objectZ - actorZ;
    const double rightCoordinate =
        deltaZ * std::cos(heading) - deltaX * std::sin(heading);
    if (!Finite(rightCoordinate))
        return false;
    *contactCode = rightCoordinate > 0.0
        ? positiveCode : nonPositiveCode;
    return true;
}

bool PeopleSupportSampling_ShouldAvoidDynamic(
    double actorX, double actorY, double actorZ,
    double actorDirectionX, double actorDirectionY,
    double actorDirectionZ,
    double objectX, double objectY, double objectZ,
    double objectDirectionX, double objectDirectionY,
    double objectDirectionZ,
    int *shouldAvoid)
{
    if (shouldAvoid == NULL || !Finite(actorX) || !Finite(actorY) ||
        !Finite(actorZ) || !Finite(actorDirectionX) ||
        !Finite(actorDirectionY) || !Finite(actorDirectionZ) ||
        !Finite(objectX) || !Finite(objectY) || !Finite(objectZ) ||
        !Finite(objectDirectionX) || !Finite(objectDirectionY) ||
        !Finite(objectDirectionZ))
        return false;

    const double alignment =
        actorDirectionX * objectDirectionX +
        actorDirectionY * objectDirectionY +
        actorDirectionZ * objectDirectionZ;
    if (!Finite(alignment))
        return false;

    if (alignment < 0.0)
    {
        *shouldAvoid = 1;
        return true;
    }

    const double relativeForward =
        (actorX - objectX) * objectDirectionX +
        (actorY - objectY) * objectDirectionY +
        (actorZ - objectZ) * objectDirectionZ;
    if (!Finite(relativeForward))
        return false;
    *shouldAvoid = relativeForward <= 0.0 ? 1 : 0;
    return true;
}

bool PeopleSupportSampling_Probe()
{
    SPeopleSupportSamplingRequest request = {};
    request.objectRadius = 10.0;
    request.heading = 0.0;
    request.positionX = 5.0;
    request.positionY = 7.0;
    request.positionZ = 11.0;

    SPeopleSupportSamplingResult sample = {};
    if (!PeopleSupportSampling_Build(request, &sample) ||
        !Near(sample.sampleOffset, 3.0) ||
        !Near(sample.boundedOffset, 3.0) ||
        !Near(sample.sweepRadius, 4.0) ||
        !Near(sample.sampleHeight, 8.6) ||
        !Near(sample.sweepTime, 8.6 / 45.0) ||
        !Near(sample.frontX, 8.0) || !Near(sample.frontY, 15.6) ||
        !Near(sample.frontZ, 11.0) || !Near(sample.rearX, 2.0) ||
        !Near(sample.rearY, 15.6) || !Near(sample.rearZ, 11.0) ||
        !Near(sample.sweepVelocityX, 0.0) ||
        !Near(sample.sweepVelocityY, -100.0) ||
        !Near(sample.sweepVelocityZ, 0.0))
        return false;

    request.objectRadius = 0.5;
    if (!PeopleSupportSampling_Build(request, &sample) ||
        !Near(sample.sampleOffset, 0.3) ||
        !Near(sample.sweepRadius, 1.0) ||
        !Near(sample.sampleHeight, 1.52))
        return false;

    request.objectRadius = 100.0;
    if (!PeopleSupportSampling_Build(request, &sample) ||
        !Near(sample.sampleOffset, 30.0) ||
        !Near(sample.boundedOffset, 12.0) ||
        !Near(sample.sweepRadius, 12.0) ||
        !Near(sample.sampleHeight, 30.0))
        return false;

    int code = 0;
    if (!PeopleSupportSampling_ClassifyDynamicSide(
            0.0, 0.0, 0.0, 2.0, 0.0, 9, 11, &code) || code != 9 ||
        !PeopleSupportSampling_ClassifyDynamicSide(
            0.0, 0.0, 0.0, -2.0, 0.0, 9, 11, &code) || code != 11)
        return false;

    const double halfPi = 1.57079632679489661923;
    int shouldAvoid = 0;
    return PeopleSupportSampling_ClassifyDynamicSide(
               0.0, 0.0, -2.0, 0.0, halfPi, 1, 3, &code) &&
           code == 1 &&
           PeopleSupportSampling_ShouldAvoidDynamic(
               0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
               2.0, 0.0, 0.0, -1.0, 0.0, 0.0,
               &shouldAvoid) && shouldAvoid == 1 &&
           PeopleSupportSampling_ShouldAvoidDynamic(
               0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
               2.0, 0.0, 0.0, 1.0, 0.0, 0.0,
               &shouldAvoid) && shouldAvoid == 1 &&
           PeopleSupportSampling_ShouldAvoidDynamic(
               2.0, 0.0, 0.0, 1.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
               &shouldAvoid) && shouldAvoid == 0 &&
           PeopleSupportSampling_ShouldAvoidDynamic(
               0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
               &shouldAvoid) && shouldAvoid == 1 &&
           !PeopleSupportSampling_Build(request, NULL) &&
           !PeopleSupportSampling_ClassifyDynamicSide(
               0.0, 0.0, 0.0, 0.0, 0.0, 0, 3, &code) &&
           !PeopleSupportSampling_ShouldAvoidDynamic(
               0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0, NULL);
}
