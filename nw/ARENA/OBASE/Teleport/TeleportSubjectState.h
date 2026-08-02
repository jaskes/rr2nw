#ifndef RR2NW_TELEPORT_SUBJECT_STATE_H
#define RR2NW_TELEPORT_SUBJECT_STATE_H

#include "mathlib.h"

#include <vector>

class SimulationContext;

struct TeleportDefinition
{
    CFVector3 source;
    CFVector3 destination;
    double radius;

    TeleportDefinition()
        : source(0.0, 0.0, 0.0), destination(0.0, 0.0, 0.0),
          radius(0.0) {}
};

struct TeleportLifecycleProbeSummary
{
    int rejectedNonPlayerCollisions;
    int physicsCollisionEvents;
    int appliedPlayerCollisions;
    int vehiclePoseRollbacks;
};

void TeleportSubjectState_Link();
bool TeleportSubjectState_Initialize(
    SimulationContext *context, int capacity,
    const std::vector<TeleportDefinition> &definitions,
    double timeStamp);
bool TeleportSubjectState_TableReady(SimulationContext *context,
                                     int expectedCapacity);
int TeleportSubjectState_Capacity();
int TeleportSubjectState_LiveCount();
unsigned long long TeleportSubjectState_Fingerprint(
    SimulationContext *context);
bool TeleportSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    TeleportLifecycleProbeSummary *summary);
const char *TeleportSubjectState_LastError();

#endif
