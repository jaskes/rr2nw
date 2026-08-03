#ifndef RR2NW_HOWITZER_SUBJECT_STATE_H
#define RR2NW_HOWITZER_SUBJECT_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class Howitzer;
class SimulationContext;

void HowitzerSubjectState_Link();
void HowitzerSubjectState_SetExpectedCapacities(int attributeCapacity,
                                                int subjectCapacity);
bool HowitzerSubjectState_TableReady(SimulationContext* context);
int HowitzerSubjectState_AttributeCapacity();
int HowitzerSubjectState_AttributeCount();
int HowitzerSubjectState_SubjectCapacity();
int HowitzerSubjectState_LiveCount();
int HowitzerSubjectState_ReadyLiveCount(SimulationContext* context);
int HowitzerSubjectState_OccupiedHolderCount();
int HowitzerSubjectState_SupportedHolderCount();
bool HowitzerSubjectState_ResolveReferences(SimulationContext* context,
                                            double timeStamp);
bool HowitzerSubjectState_PrepareNewObject(SimulationContext* context,
                                           const KR_ObjectID& object);
bool HowitzerSubjectState_AttributeExists(SimulationContext* context,
                                          const KR_ObjectID& object);
unsigned long long HowitzerSubjectState_Fingerprint(
    SimulationContext* context);

bool HowitzerSubjectState_LoadHolders();
void HowitzerSubjectState_ReleaseHolders();
int HowitzerSubjectState_HolderCount();
int HowitzerSubjectState_FindHolder(const char* name);
const char* HowitzerSubjectState_HolderName(int index);
KR_ObjectID HowitzerSubjectState_HolderOccupant(const char* name);
bool HowitzerSubjectState_DeleteHolderOccupant(
    SimulationContext* context, const char* name,
    const std::vector<KR_ObjectID>& transactionObjects,
    bool transactionActive);
const char* HowitzerSubjectState_LastError();

#endif
