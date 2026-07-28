#ifndef RR2NW_SMOKE_SUBJECT_STATE_H
#define RR2NW_SMOKE_SUBJECT_STATE_H

class SimulationContext;

void SmokeSubjectState_Link();
bool SmokeSubjectState_TableReady(SimulationContext *context,
                                  int expectedCapacity);
int SmokeSubjectState_Capacity();
int SmokeSubjectState_LiveCount();
unsigned long long SmokeSubjectState_Fingerprint(SimulationContext *context);
bool SmokeSubjectState_ProbeLifecycle(SimulationContext *context);
bool SmokeSubjectState_SimulationSupported(
    SimulationContext *context, const char *attributeName);
bool SmokeSubjectState_RenderingSupported(
    SimulationContext *context, const char *attributeName);
bool SmokeSubjectState_ProbeSimulationLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp);

#endif
