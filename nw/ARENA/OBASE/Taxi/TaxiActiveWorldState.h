#ifndef RR2NW_TAXI_ACTIVE_WORLD_STATE_H
#define RR2NW_TAXI_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

void TaxiActiveWorldState_Link();
const char *TaxiActiveWorldState_LastFailure();
int TaxiActiveWorldState_LiveCount(SimulationContext *context);
int TaxiActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long TaxiActiveWorldState_Fingerprint(
    SimulationContext *context);
bool TaxiActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool TaxiActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool TaxiActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool TaxiActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool TaxiActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool TaxiActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void TaxiActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

#endif
