#ifndef RR2NW_SPARK_ACTIVE_WORLD_STATE_H
#define RR2NW_SPARK_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

struct SparkActiveWorldProbeSummary
{
    int capturedOwners;
    int schedulerEvents;
    int stagedRollbacks;
    int reconstructedOwners;
    int stableRoundTrips;
    int resumedPhases;
    unsigned long long fingerprint;
};

void SparkActiveWorldState_Link();
const char *SparkActiveWorldState_LastFailure();
int SparkActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long SparkActiveWorldState_Fingerprint(
    SimulationContext *context);
bool SparkActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool SparkActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool SparkActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool SparkActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool SparkActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool SparkActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void SparkActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);
bool SparkActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, double timeStamp,
    SparkActiveWorldProbeSummary *summary);

#endif
