#ifndef RR2NW_SPARK_SUBJECT_STATE_H
#define RR2NW_SPARK_SUBJECT_STATE_H

#include "mathlib.h"
#include "storage/h/classtab.h"

#include <vector>

class SimulationContext;

struct SparkCreateRequest
{
    CFVector3 position;
    double timeStamp;
    ct_ClassTableID subjectTable;
    int attributeIndex;
    const char *objectName;
};

struct SparkLifecycleProbeSummary
{
    int invalidStarts;
    int queuedCreates;
    int queueRollbacks;
    int phaseTransitions;
    int expirations;
};

void SparkSubjectState_Link();
bool SparkSubjectState_TableReady(SimulationContext *context,
                                  int expectedCapacity);
int SparkSubjectState_Capacity();
int SparkSubjectState_LiveCount();
bool SparkSubjectState_IsPending(
    SimulationContext *context, const KR_ObjectID &object);
bool SparkSubjectState_CollectPending(
    SimulationContext *context, const char *objectName,
    std::vector<KR_ObjectID> *objects);
unsigned long long SparkSubjectState_Fingerprint(
    SimulationContext *context);
bool SparkSubjectState_QueueCreate(
    SimulationContext *context, const SparkCreateRequest &request,
    KR_ObjectID *child);
bool SparkSubjectState_RollbackQueued(
    SimulationContext *context, const KR_ObjectID &child);
bool SparkSubjectState_ExecuteNow(
    SimulationContext *context, const SparkCreateRequest &request,
    KR_ObjectID *child);
bool SparkSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    SparkLifecycleProbeSummary *summary);

#endif
