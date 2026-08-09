#ifndef RR2NW_VEHICLE_ACTIVE_WORLD_STATE_H
#define RR2NW_VEHICLE_ACTIVE_WORLD_STATE_H

#include <vector>

#include "kernel/h/krtypes.h"

class SimulationContext;

void VehicleActiveWorldState_Link();
int VehicleActiveWorldState_LiveCount(SimulationContext *context);
unsigned long long VehicleActiveWorldState_Fingerprint(
    SimulationContext *context);
// Canonical gameplay projection of VEH1. The one-shot briefing presentation
// latch is deliberately absent from replay state.
unsigned long long VehicleActiveWorldState_AuthoritativeFingerprint(
    SimulationContext *context);
bool VehicleActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool VehicleActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool VehicleActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool VehicleActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool VehicleActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void VehicleActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

#endif
