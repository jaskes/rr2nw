#ifndef RR2NW_ARTEFACT_ACTIVE_WORLD_STATE_H
#define RR2NW_ARTEFACT_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

void ArtefactActiveWorldState_Link();
const char *ArtefactActiveWorldState_LastFailure();
int ArtefactActiveWorldState_LiveCount(SimulationContext *context);
bool ArtefactActiveWorldState_IsReady(SimulationContext *context,
                                      const KR_ObjectID &object);
unsigned long long ArtefactActiveWorldState_Fingerprint(
    SimulationContext *context);
bool ArtefactActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool ArtefactActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool ArtefactActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool ArtefactActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners);
bool ArtefactActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool ArtefactActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void ArtefactActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

#endif
