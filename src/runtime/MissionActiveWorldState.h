#ifndef RR2NW_MISSION_ACTIVE_WORLD_STATE_H
#define RR2NW_MISSION_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <string>
#include <vector>

class SimulationContext;

struct SMissionRouteRequirement {
  std::string name;
  unsigned long long geometryFingerprint = 0;
};

bool MissionActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
// Canonical gameplay projection of MSH1. Mission objectives, result,
// Project/checkpoint state and authored route identity remain; player-facing
// summary text, layout and colour are neutralized.
unsigned long long MissionActiveWorldState_AuthoritativeFingerprint(
    SimulationContext *context);
bool MissionActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool MissionActiveWorldState_ProbeLegacyVersionCompatibility(
    SimulationContext *context);
bool MissionActiveWorldState_RouteRequirements(
    const std::vector<unsigned char> &bytes,
    std::vector<SMissionRouteRequirement> *requirements);
bool MissionActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool MissionActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool MissionActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void MissionActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

bool MissionActiveWorldState_StageProbe(
    SimulationContext *context, double timeStamp, bool *staged);
bool MissionActiveWorldState_ClearProbe(SimulationContext *context);
bool MissionActiveWorldState_MissionIndexValid(
    SimulationContext *context, int index);
bool MissionActiveWorldState_ProbeCounts(
    const std::vector<unsigned char> &bytes, int *missions,
    int *conditionReferences, int *routeReferences);
const char *MissionActiveWorldState_LastFailure();

#endif
