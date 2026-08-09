#ifndef RR2NW_TANK_ACTIVE_WORLD_STATE_H
#define RR2NW_TANK_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

void TankActiveWorldState_Link();
const char *TankActiveWorldState_LastFailure();
int TankActiveWorldState_LiveCount(SimulationContext *context);
int TankActiveWorldState_OwnedCannonCount(
    const std::vector<unsigned char> &bytes);
int TankActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long TankActiveWorldState_Fingerprint(
    SimulationContext *context);
// Canonical gameplay projection of TAN1. Tank/Cannon visibility and
// audibility caches are neutralized before hashing.
unsigned long long TankActiveWorldState_AuthoritativeFingerprint(
    SimulationContext *context);
bool TankActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool TankActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool TankActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool TankActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool TankActiveWorldState_CollectOwnedCannons(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *cannons);
bool TankActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool TankActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void TankActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

#endif
