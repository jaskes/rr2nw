#ifndef RR2NW_PEOPLE_SUBJECT_STATE_H
#define RR2NW_PEOPLE_SUBJECT_STATE_H

class SimulationContext;

struct SPeopleLifecycleProbeSummary
{
    int validStarts;
    int dynamicReady;
    int renderReady;
    int scheduledMoves;
    int bulletDamageApplications;
    int deathTransitions;
    int saveStateRoundTrips;
    int rollbacks;
};

void PeopleSubjectState_Link();
void PeopleSubjectState_SetExpectedCapacities(int attributeCapacity,
                                              int subjectCapacity);
int PeopleSubjectState_AttributeCapacity();
int PeopleSubjectState_SubjectCapacity();
int PeopleSubjectState_AttributeCount(SimulationContext *context);
int PeopleSubjectState_LiveCount(SimulationContext *context);
int PeopleSubjectState_SoundCount(SimulationContext *context);
bool PeopleSubjectState_UpdateAttributes(SimulationContext *context,
                                         double timeStamp);
bool PeopleSubjectState_TablesReady(SimulationContext *context,
                                    int expectedAttributeCapacity,
                                    int expectedSubjectCapacity);
bool PeopleSubjectState_AllReady(SimulationContext *context);
const char *PeopleSubjectState_FirstNotReady();
unsigned long long PeopleSubjectState_AttributeFingerprint(
    SimulationContext *context);
unsigned long long PeopleSubjectState_SubjectFingerprint(
    SimulationContext *context);
unsigned long long PeopleSubjectState_AbsentAttributeFingerprint();
unsigned long long PeopleSubjectState_AbsentSubjectFingerprint();
bool PeopleSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    SPeopleLifecycleProbeSummary *summary);

#endif
