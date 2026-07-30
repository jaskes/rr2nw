#ifndef RR2NW_COMMANDER_STATE_H
#define RR2NW_COMMANDER_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

void CommanderState_Link();
int CommanderState_LiveCount(SimulationContext *context);
int CommanderState_HostileLinkCount(SimulationContext *context);
int CommanderState_MemberCount(SimulationContext *context,
                               const KR_ObjectID &commander);
bool CommanderState_HasMember(SimulationContext *context,
                              const KR_ObjectID &commander,
                              const KR_ObjectID &member);
bool CommanderState_IsHostile(SimulationContext *context,
                              const KR_ObjectID &commander,
                              const KR_ObjectID &relativeCommander);
unsigned long long CommanderState_Fingerprint(SimulationContext *context);
bool CommanderState_StableRoundTrip(SimulationContext *context);
bool CommanderState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool CommanderState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool CommanderState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool CommanderState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created);
bool CommanderState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
void CommanderState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created);

#endif
