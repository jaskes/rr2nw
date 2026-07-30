#ifndef RR2NW_CORPSE_ACTIVE_WORLD_STATE_H
#define RR2NW_CORPSE_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

struct CorpseActiveWorldProbeSummary
{
    int capturedCorpses;
    int ownedSmokers;
    int schedulerEvents;
    int stagedRollbacks;
    int reconstructedObjects;
    int stableRoundTrips;
    int resumedEmissions;
    int resumedDeaths;
    unsigned long long fingerprint;
};

void CorpseActiveWorldState_Link();
const char *CorpseActiveWorldState_LastFailure();
int CorpseActiveWorldState_OwnedSmokerCount(
    const std::vector<unsigned char> &bytes);
int CorpseActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long CorpseActiveWorldState_Fingerprint(
    SimulationContext *context);
bool CorpseActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool CorpseActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool CorpseActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool CorpseActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool CorpseActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool CorpseActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void CorpseActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);
bool CorpseActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, double timeStamp,
    CorpseActiveWorldProbeSummary *summary);

#endif
