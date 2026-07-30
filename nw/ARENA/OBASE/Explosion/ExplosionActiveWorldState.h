#ifndef RR2NW_EXPLOSION_ACTIVE_WORLD_STATE_H
#define RR2NW_EXPLOSION_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

struct ExplosionActiveWorldProbeSummary
{
    int capturedOwners;
    int capturedBranches;
    int schedulerEvents;
    int soundChildren;
    int stagedRollbacks;
    int reconstructedOwners;
    int stableRoundTrips;
    int resumedMoves;
    unsigned long long fingerprint;
};

void ExplosionActiveWorldState_Link();
const char *ExplosionActiveWorldState_LastFailure();
int ExplosionActiveWorldState_BranchCount(
    const std::vector<unsigned char> &bytes);
int ExplosionActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long ExplosionActiveWorldState_Fingerprint(
    SimulationContext *context);
bool ExplosionActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool ExplosionActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool ExplosionActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool ExplosionActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool ExplosionActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool ExplosionActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void ExplosionActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);
bool ExplosionActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionActiveWorldProbeSummary *summary);

#endif
