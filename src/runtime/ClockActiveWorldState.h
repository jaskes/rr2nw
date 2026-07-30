#ifndef RR2NW_CLOCK_ACTIVE_WORLD_STATE_H
#define RR2NW_CLOCK_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <cstdint>
#include <vector>

class SimulationContext;

bool ClockActiveWorldState_CaptureStable(
    std::vector<std::uint8_t>* bytes);
bool ClockActiveWorldState_ValidateStable(
    const std::vector<std::uint8_t>& bytes);
bool ClockActiveWorldState_MatchesStable(
    const std::vector<std::uint8_t>& bytes);
bool ClockActiveWorldState_CreateStableOwners(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    std::vector<KR_ObjectID>* created);
bool ClockActiveWorldState_ApplyStableReferences(
    const std::vector<std::uint8_t>& bytes);
void ClockActiveWorldState_RemoveStableOwners(
    SimulationContext* context, std::vector<KR_ObjectID>* created);
bool ClockActiveWorldState_MetadataMatches(
    const std::vector<std::uint8_t>& bytes, std::uint64_t tick,
    double simulationTime);

#endif
