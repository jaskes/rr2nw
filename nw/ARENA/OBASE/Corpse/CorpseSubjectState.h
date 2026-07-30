#ifndef RR2NW_CORPSE_SUBJECT_STATE_H
#define RR2NW_CORPSE_SUBJECT_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;
class Corpse;

void CorpseSubjectState_Link();
bool CorpseSubjectState_CreateTable(SimulationContext *context,
                                    int capacity);
bool CorpseSubjectState_TableReady(SimulationContext *context,
                                   int expectedCapacity);
int CorpseSubjectState_Capacity();
int CorpseSubjectState_LiveCount();
Corpse *CorpseSubjectState_Find(SimulationContext *context,
                               const KR_ObjectID &object);
bool CorpseSubjectState_CollectObjects(std::vector<KR_ObjectID> *objects);
unsigned long long CorpseSubjectState_Fingerprint(
    SimulationContext *context);

#endif
