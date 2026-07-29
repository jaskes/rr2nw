#ifndef RR2NW_TANK_SUBJECT_STATE_H
#define RR2NW_TANK_SUBJECT_STATE_H

class SimulationContext;

struct STankLifecycleProbeSummary
{
    int available;
    int validStarts;
    int dynamicReady;
    int renderReady;
    int cannonReady;
    int scheduledMoves;
    int bulletDamageApplications;
    int deathTransitions;
    int deathEffects;
    int saveStateRoundTrips;
    int rollbacks;
};

void TankSubjectState_Link();
void TankSubjectState_SetExpectedCapacities(int attributeCapacity,
                                            int subjectCapacity);
int TankSubjectState_AttributeCapacity();
int TankSubjectState_SubjectCapacity();
int TankSubjectState_AttributeCount(SimulationContext *context);
int TankSubjectState_LiveCount(SimulationContext *context);
unsigned long long TankSubjectState_AttributeFingerprint(
    SimulationContext *context);
unsigned long long TankSubjectState_SubjectFingerprint(
    SimulationContext *context);
bool TankSubjectState_AttributeReferencesResolved(SimulationContext *context);
const char *TankSubjectState_FirstUnresolvedReference();
bool TankSubjectState_UpdateAttributes(SimulationContext *context,
                                       double timeStamp);
bool TankSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    STankLifecycleProbeSummary *summary);

#endif
