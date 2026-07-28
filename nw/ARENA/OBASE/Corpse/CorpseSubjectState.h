#ifndef RR2NW_CORPSE_SUBJECT_STATE_H
#define RR2NW_CORPSE_SUBJECT_STATE_H

class SimulationContext;

void CorpseSubjectState_Link();
bool CorpseSubjectState_CreateTable(SimulationContext *context,
                                    int capacity);
bool CorpseSubjectState_TableReady(SimulationContext *context,
                                   int expectedCapacity);
int CorpseSubjectState_Capacity();
int CorpseSubjectState_LiveCount();
unsigned long long CorpseSubjectState_Fingerprint(
    SimulationContext *context);

#endif
