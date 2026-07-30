#ifndef RR2NW_ACTIVE_WORLD_SEMANTIC_EVENTS_H
#define RR2NW_ACTIVE_WORLD_SEMANTIC_EVENTS_H

#include "ActiveWorldSave.h"
#include "kernel/h/krtypes.h"

#include <string>
#include <vector>

class SimulationContext;

bool ActiveWorldSemanticEvents_Capture(
    SimulationContext *context, std::vector<SActiveWorldEvent> *events,
    std::string *failure);
bool ActiveWorldSemanticEvents_Validate(
    const std::vector<SActiveWorldEvent> &events, std::string *failure);
bool ActiveWorldSemanticEvents_Replace(
    SimulationContext *context,
    const std::vector<SActiveWorldEvent> &events,
    std::vector<KR_ObjectID> *created, std::string *failure);
bool ActiveWorldSemanticEvents_Matches(
    SimulationContext *context,
    const std::vector<SActiveWorldEvent> &events);
bool ActiveWorldSemanticEvents_Clear(
    SimulationContext *context, std::string *failure);

bool ActiveWorldSemanticEvents_StageProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    std::string *failure);
bool ActiveWorldSemanticEvents_ClearProbe(
    SimulationContext *context, std::string *failure);

#endif
