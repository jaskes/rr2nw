#ifndef RR2NW_EXPLOSION_SUBJECT_STATE_H
#define RR2NW_EXPLOSION_SUBJECT_STATE_H

#include "mathlib.h"
#include "storage/h/classtab.h"

class SimulationContext;

struct ExplosionImpactRequest
{
    CFVector3 position;
    double timeStamp;
    KR_ObjectID damageOwner;
    ct_ClassTableID subjectTable;
    int attributeIndex;
    const char *objectName;
};

struct ExplosionImpactProbeSummary
{
    int invalidStarts;
    int allocationRollbacks;
    int queuedCommands;
    int queueRollbacks;
    int executedCommands;
    int damageApplications;
    double expectedDamage;
};

void ExplosionSubjectState_Link();
bool ExplosionSubjectState_TableReady(SimulationContext *context,
                                      int expectedCapacity);
int ExplosionSubjectState_Capacity();
int ExplosionSubjectState_LiveCount();
unsigned long long ExplosionSubjectState_Fingerprint(
    SimulationContext *context);
bool ExplosionSubjectState_QueueBatch(
    SimulationContext *context, const ExplosionImpactRequest *requests,
    int requestCount, KR_ObjectID *children);
bool ExplosionSubjectState_RollbackQueued(
    SimulationContext *context, const KR_ObjectID *children,
    int childCount);
bool ExplosionSubjectState_ExecuteNow(
    SimulationContext *context, const ExplosionImpactRequest &request,
    int *damageApplications);
bool ExplosionSubjectState_ProbeLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionImpactProbeSummary *summary);
bool ExplosionSubjectState_ProbeDamageLifecycle(
    SimulationContext *context, const char *attributeName,
    const KR_ObjectID &target, const KR_ObjectID &damageOwner,
    double timeStamp, ExplosionImpactProbeSummary *summary);

#endif
