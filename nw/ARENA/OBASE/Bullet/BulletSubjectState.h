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

#endif
