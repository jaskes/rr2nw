#ifndef RR2NW_HOWITZER_ACTIVE_WORLD_STATE_H
#define RR2NW_HOWITZER_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

void HowitzerActiveWorldState_Link();
const char* HowitzerActiveWorldState_LastFailure();
int HowitzerActiveWorldState_LiveCount(SimulationContext* context);
int HowitzerActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char>& bytes);
unsigned long long HowitzerActiveWorldState_Fingerprint(
    SimulationContext* context);
bool HowitzerActiveWorldState_CaptureStable(
    SimulationContext* context, std::vector<unsigned char>* bytes);
bool HowitzerActiveWorldState_CaptureHolder(
    SimulationContext* context, const char* holderName,
    std::vector<unsigned char>* bytes);
bool HowitzerActiveWorldState_ValidateStable(
    const std::vector<unsigned char>& bytes);
bool HowitzerActiveWorldState_MatchesStable(
    SimulationContext* context, const std::vector<unsigned char>& bytes);
bool HowitzerActiveWorldState_CollectStableOwners(
    SimulationContext* context, const std::vector<unsigned char>& bytes,
    std::vector<KR_ObjectID>* owners);
bool HowitzerActiveWorldState_CreateStableOwners(
    SimulationContext* context, const std::vector<unsigned char>& bytes,
    std::vector<KR_ObjectID>* created);
bool HowitzerActiveWorldState_ApplyStableReferences(
    SimulationContext* context, const std::vector<unsigned char>& bytes);
bool HowitzerActiveWorldState_RestoreHolder(
    SimulationContext* context, const std::vector<unsigned char>& bytes);
void HowitzerActiveWorldState_RemoveStableOwners(
    SimulationContext* context, std::vector<KR_ObjectID>* created);

#endif
