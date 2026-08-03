#ifndef RR2NW_PEOPLE_ACTIVE_WORLD_STATE_H
#define RR2NW_PEOPLE_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <cstddef>
#include <string>
#include <vector>

class SimulationContext;

struct SPeopleRouteRequirement
{
    std::string name;
    unsigned long long geometryFingerprint;

    SPeopleRouteRequirement() : geometryFingerprint(0) {}
};

void PeopleActiveWorldState_Link();
const char *PeopleActiveWorldState_LastFailure();
int PeopleActiveWorldState_LiveCount(SimulationContext *context);
int PeopleActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long PeopleActiveWorldState_Fingerprint(
    SimulationContext *context);
bool PeopleActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool PeopleActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool PeopleActiveWorldState_RouteNames(
    const std::vector<unsigned char> &bytes,
    std::vector<std::string> *routeNames);
bool PeopleActiveWorldState_RouteRequirements(
    const std::vector<unsigned char> &bytes,
    std::vector<SPeopleRouteRequirement> *requirements);
unsigned long long PeopleActiveWorldState_RouteGeometryFingerprint(
    const double *coordinates, std::size_t coordinateCount);
bool PeopleActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool PeopleActiveWorldState_ProbeLegacyVersionCompatibility(
    SimulationContext *context);
bool PeopleActiveWorldState_ProbeDetailedCaptureFailure(
    SimulationContext *context);
bool PeopleActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool PeopleActiveWorldState_HoldRouteReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *heldRoutes);
void PeopleActiveWorldState_ReleaseRouteReferences(
    SimulationContext *context, std::vector<KR_ObjectID> *heldRoutes);
bool PeopleActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool PeopleActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void PeopleActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

#endif
