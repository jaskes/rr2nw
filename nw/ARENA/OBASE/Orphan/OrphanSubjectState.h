#ifndef RR2NW_ORPHAN_SUBJECT_STATE_H
#define RR2NW_ORPHAN_SUBJECT_STATE_H

#include "kernel/h/krtypes.h"

class SimulationContext;

struct SOrphanSubjectRuntimeTelemetry
{
    int acceptedDrops;
    int rejectedDrops;
    int moveEvents;
    int impacts;
    int explosionStarts;
    int smokeStarts;
    int soundStarts;
    int renderFrames;
    int peakLiveObjects;
};

void OrphanSubjectState_Link();
bool OrphanSubjectState_CreateTable(SimulationContext *context,
                                    int capacity);
bool OrphanSubjectState_TableReady(SimulationContext *context,
                                   int expectedCapacity);
int OrphanSubjectState_Capacity();
int OrphanSubjectState_LiveCount();
bool OrphanSubjectState_AllReady(SimulationContext *context);
unsigned long long OrphanSubjectState_Fingerprint(
    SimulationContext *context);
bool OrphanSubjectState_RuntimeTelemetry(
    SimulationContext *context,
    SOrphanSubjectRuntimeTelemetry *telemetry);
KR_ObjectID OrphanSubjectState_FirstObject(SimulationContext *context);

#endif
