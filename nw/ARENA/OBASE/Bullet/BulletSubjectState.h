#ifndef RR2NW_BULLET_SUBJECT_STATE_H
#define RR2NW_BULLET_SUBJECT_STATE_H

class SimulationContext;
class KR_ObjectID;

struct BulletCollisionProbeSummary
{
    int scheduledChecks;
    int executedChecks;
    int sphereCases;
    int earliestHitCases;
    int waterlineCases;
    int sceneQueries;
};

struct BulletEffectProbeSummary
{
    int queuedBatches;
    int queuedChildren;
    int splashFirstCases;
    int rolledBackChildren;
};

struct BulletGroundSparkProbeSummary
{
    int queuedSparks;
    int rolledBackSparks;
};

struct BulletBarrelSmokeProbeSummary
{
    int thresholdStarts;
    int frameGateSkips;
    int attributeGateSkips;
    int rolledBackSmokes;
};

void BulletSubjectState_Link();
bool BulletSubjectState_TableReady(SimulationContext *context, int capacity);
int BulletSubjectState_Capacity();
int BulletSubjectState_LiveCount();
unsigned long long BulletSubjectState_Fingerprint(
    SimulationContext *context);
bool BulletSubjectState_ProbeBallisticLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, int *moveCount);
bool BulletSubjectState_ProbeCollisionLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletCollisionProbeSummary *summary);
bool BulletSubjectState_ProbeDynamicCollisionLifecycle(
    SimulationContext *context, const char *attributeName,
    const KR_ObjectID &target, double timeStamp);
bool BulletSubjectState_ProbeImpactEffectLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletEffectProbeSummary *summary);
bool BulletSubjectState_ProbeGroundSparkLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletGroundSparkProbeSummary *summary);
bool BulletSubjectState_ProbeBarrelSmokeLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletBarrelSmokeProbeSummary *summary);

#endif
