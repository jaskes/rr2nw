#ifndef RR2NW_BULLET_ACTIVE_WORLD_STATE_H
#define RR2NW_BULLET_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

struct BulletActiveWorldProbeSummary
{
    int capturedOwners;
    int schedulerEvents;
    int stagedRollbacks;
    int reconstructedOwners;
    int stableRoundTrips;
    int resumedMoves;
    unsigned long long fingerprint;
};

void BulletActiveWorldState_Link();
const char *BulletActiveWorldState_LastFailure();
int BulletActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long BulletActiveWorldState_Fingerprint(
    SimulationContext *context);
bool BulletActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool BulletActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool BulletActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool BulletActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool BulletActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool BulletActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void BulletActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);
bool BulletActiveWorldState_ProbeFlightRoundTrip(
    SimulationContext *context, const char *attributeName,
    const KR_ObjectID &master, double timeStamp,
    BulletActiveWorldProbeSummary *summary);

#endif
