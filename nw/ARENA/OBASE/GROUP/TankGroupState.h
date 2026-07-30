#ifndef RR2NW_TANK_GROUP_STATE_H
#define RR2NW_TANK_GROUP_STATE_H

#include "kernel/h/krtypes.h"

class SimulationContext;

struct STankGroupSchedulerProbeSummary
{
    int findEnemyCycles;
    int movingCycles;
};

void TankGroupState_Link();
int TankGroupState_LiveCount(SimulationContext *context);
int TankGroupState_MemberCount(SimulationContext *context,
                               const KR_ObjectID &group);
bool TankGroupState_HasMember(SimulationContext *context,
                              const KR_ObjectID &group,
                              const KR_ObjectID &member);
KR_ObjectID TankGroupState_Commander(SimulationContext *context,
                                     const KR_ObjectID &group);
unsigned long long TankGroupState_Fingerprint(SimulationContext *context);
bool TankGroupState_StableRoundTrip(SimulationContext *context,
                                    const KR_ObjectID &group);
bool TankGroupState_ProbeScheduler(
    SimulationContext *context, const KR_ObjectID &group, double timeStamp,
    STankGroupSchedulerProbeSummary *summary);

#endif
