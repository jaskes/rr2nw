#ifndef RR2NW_ORPHAN_ACTIVE_WORLD_STATE_H
#define RR2NW_ORPHAN_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

void OrphanActiveWorldState_Link();
const char *OrphanActiveWorldState_LastFailure();
int OrphanActiveWorldState_LiveCount(SimulationContext *context);
int OrphanActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes);
unsigned long long OrphanActiveWorldState_Fingerprint(
    SimulationContext *context);
bool OrphanActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool OrphanActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool OrphanActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool OrphanActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool OrphanActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool OrphanActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void OrphanActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

#endif
