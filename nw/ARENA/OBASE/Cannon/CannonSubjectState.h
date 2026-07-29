#ifndef RR2NW_CANNON_SUBJECT_STATE_H
#define RR2NW_CANNON_SUBJECT_STATE_H

class SimulationContext;

void CannonSubjectState_Link();
void CannonSubjectState_SetExpectedCapacities(int attributeCapacity,
                                              int subjectCapacity);
int CannonSubjectState_AttributeCapacity();
int CannonSubjectState_SubjectCapacity();
int CannonSubjectState_AttributeCount(SimulationContext *context);
int CannonSubjectState_LiveCount(SimulationContext *context);
unsigned long long CannonSubjectState_AttributeFingerprint(
    SimulationContext *context);
unsigned long long CannonSubjectState_SubjectFingerprint(
    SimulationContext *context);

#endif
