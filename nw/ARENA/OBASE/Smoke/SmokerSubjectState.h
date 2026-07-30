#ifndef RR2NW_SMOKER_SUBJECT_STATE_H
#define RR2NW_SMOKER_SUBJECT_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;
class Smoker;

void SmokerSubjectState_Link();
bool SmokerSubjectState_DynTableReady(SimulationContext *context,
                                      int expectedCapacity);
int SmokerSubjectState_DynCapacity();
int SmokerSubjectState_DynLiveCount();
Smoker *SmokerSubjectState_FindDyn(const KR_ObjectID &object);
bool SmokerSubjectState_CollectDynObjects(
    std::vector<KR_ObjectID> *objects);
unsigned long long SmokerSubjectState_DynFingerprint(
    SimulationContext *context);
bool SmokerSubjectState_ProbeDynLifecycle(SimulationContext *context,
                                          const char *attributeName,
                                          double timeStamp);
bool SmokerSubjectState_EmissionSupported(SimulationContext *context,
                                           const char *attributeName);
bool SmokerSubjectState_LightCoronaSupported(
    SimulationContext *context, const char *attributeName);
bool SmokerSubjectState_ProbeEmissionLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp);
bool SmokerSubjectState_ProbeLightCoronaLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp);

#endif
