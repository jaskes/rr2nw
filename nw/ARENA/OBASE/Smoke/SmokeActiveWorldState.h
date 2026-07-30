#ifndef RR2NW_SMOKE_ACTIVE_WORLD_STATE_H
#define RR2NW_SMOKE_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

struct SmokeActiveWorldProbeSummary
{
    int capturedOwners;
    int capturedBlobs;
    int schedulerEvents;
    int stagedRollbacks;
    int reconstructedOwners;
    int stableRoundTrips;
    int resumedMoves;
    unsigned long long fingerprint;
};

void SmokeActiveWorldState_Link();
const char *SmokeActiveWorldState_LastFailure();
int SmokeActiveWorldState_BlobCount(
    const std::vector<unsigned char> &bytes);
int SmokeActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long SmokeActiveWorldState_Fingerprint(
    SimulationContext *context);
bool SmokeActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool SmokeActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool SmokeActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool SmokeActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool SmokeActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool SmokeActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void SmokeActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);
bool SmokeActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, const char *attributeName,
    double timeStamp, SmokeActiveWorldProbeSummary *summary);

#endif
