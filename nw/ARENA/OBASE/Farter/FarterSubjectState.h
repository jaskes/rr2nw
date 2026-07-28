#ifndef RR2NW_FARTER_SUBJECT_STATE_H
#define RR2NW_FARTER_SUBJECT_STATE_H

class SimulationContext;

void FarterSubjectState_Link();
bool FarterSubjectState_TableReady(SimulationContext *context, int capacity);
int FarterSubjectState_Capacity();
int FarterSubjectState_LiveCount();
unsigned long long FarterSubjectState_Fingerprint(SimulationContext *context);
unsigned long long FarterSubjectState_AbsentFingerprint();
bool FarterSubjectState_ProbeLifecycle(SimulationContext *context,
                                       const char *attributeName,
                                       double timeStamp);

#endif
