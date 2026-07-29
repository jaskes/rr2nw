#ifndef RR2NW_SMOKE_SUBJECT_STATE_H
#define RR2NW_SMOKE_SUBJECT_STATE_H

#include "mathlib.h"
#include "storage/h/classtab.h"

class SimulationContext;

struct SmokeDirectionalStartRequest
{
    CFVector3 position;
    CFVector3 direction;
    double timeStamp;
    KR_ObjectID source;
    ct_ClassTableID subjectTable;
    KR_ObjectID attribute;
    const char *attributeName;
    const char *objectName;
};

struct SmokeStaticStartRequest
{
    CFVector3 position;
    double timeStamp;
    KR_ObjectID source;
    ct_ClassTableID subjectTable;
    KR_ObjectID attribute;
    const char *attributeName;
    const char *objectName;
};

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
bool SmokeSubjectState_StartWithDirection(
    SimulationContext *context,
    const SmokeDirectionalStartRequest &request,
    KR_ObjectID *child);
bool SmokeSubjectState_StartAtPosition(
    SimulationContext *context,
    const SmokeStaticStartRequest &request,
    KR_ObjectID *child);
bool SmokeSubjectState_RollbackStarted(
    SimulationContext *context, const KR_ObjectID &child);

#endif
