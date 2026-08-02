#ifndef RR2NW_PEOPLE_SUBJECT_STATE_H
#define RR2NW_PEOPLE_SUBJECT_STATE_H

class SimulationContext;

struct SPeopleLifecycleProbeSummary
{
    int validStarts;
    int dynamicReady;
    int renderReady;
    int scheduledMoves;
    int cadenceBounded;
    int renderedPoseFrames;
    int viewBoundaryResets;
    int bulletDamageApplications;
    int deathTransitions;
    int saveStateRoundTrips;
    int projectileReferenceReady;
    int outgoingProjectileStarts;
    int rollbacks;
};

struct SPeopleGameplayTuningPatch
{
    bool hasMovementSpeed;
    bool hasInitialHealth;
    bool hasFireInterval;
    bool hasBurstCount;
    bool hasProjectile;
    double movementSpeed;
    double initialHealth;
    double fireInterval;
    int burstCount;
    char projectile[64];
};

struct SPeopleGameplayTuningState
{
    void *owner;
    char id[64];
    double movementSpeed;
    double initialHealth;
    double fireInterval;
    int burstCount;
    char projectile[64];
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
unsigned long long PeopleSubjectState_GameplayFingerprint(
    SimulationContext *context);
bool PeopleSubjectState_CaptureGameplayTuning(
    SimulationContext *context, const char *id,
    SPeopleGameplayTuningState *state);
bool PeopleSubjectState_ApplyGameplayTuning(
    SimulationContext *context, const SPeopleGameplayTuningState *state,
    const SPeopleGameplayTuningPatch *patch);
bool PeopleSubjectState_RestoreGameplayTuning(
    SimulationContext *context, const SPeopleGameplayTuningState *state);
bool PeopleSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    SPeopleLifecycleProbeSummary *summary);
bool PeopleSubjectState_ProbeAttributeLifecycle(
    SimulationContext *context, const char *attributeName, double timeStamp,
    SPeopleLifecycleProbeSummary *summary);
bool PeopleSubjectState_ProbeTunedAttributeLifecycle(
    SimulationContext *context, const char *attributeName,
    const char *expectedProjectile, double timeStamp,
    SPeopleLifecycleProbeSummary *summary);

#endif
