#ifndef RR2NW_PEOPLE_SUPPORT_SAMPLING_H
#define RR2NW_PEOPLE_SUPPORT_SAMPLING_H

struct SPeopleSupportSamplingRequest
{
    double objectRadius;
    double heading;
    double positionX;
    double positionY;
    double positionZ;
};

struct SPeopleSupportSamplingResult
{
    double sampleOffset;
    double boundedOffset;
    double sweepRadius;
    double sampleHeight;
    double sweepTime;
    double frontX;
    double frontY;
    double frontZ;
    double rearX;
    double rearY;
    double rearZ;
    double sweepVelocityX;
    double sweepVelocityY;
    double sweepVelocityZ;
};

// Exact support-probe geometry from the May 1999 ON_OBJECTS branch at
// 0x004fa1e1--0x004fa6ef. The scene queries remain at the People call site so
// this kernel can be tested without a live CViewScene.
bool PeopleSupportSampling_Build(
    const SPeopleSupportSamplingRequest &request,
    SPeopleSupportSamplingResult *result);

// May classifies dynamic support/obstacle owners by their signed horizontal
// coordinate on the actor's right axis. The caller supplies the two contact
// codes because the same predicate feeds 9/11 and 1/3 branches.
bool PeopleSupportSampling_ClassifyDynamicSide(
    double actorX, double actorZ, double objectX, double objectZ,
    double heading, int positiveCode, int nonPositiveCode,
    int *contactCode);

// May keeps the 1/3 horizontal contact only when the other movement owner is
// approaching us, or when we are not already ahead of an owner travelling in
// the same direction. This prevents a guide from turning away from a vehicle
// that is safely behind it while retaining head-on and blocking contacts.
bool PeopleSupportSampling_ShouldAvoidDynamic(
    double actorX, double actorY, double actorZ,
    double actorDirectionX, double actorDirectionY,
    double actorDirectionZ,
    double objectX, double objectY, double objectZ,
    double objectDirectionX, double objectDirectionY,
    double objectDirectionZ,
    int *shouldAvoid);

bool PeopleSupportSampling_Probe();

#endif
