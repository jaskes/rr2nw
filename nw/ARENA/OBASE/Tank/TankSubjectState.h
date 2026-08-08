#ifndef RR2NW_TANK_SUBJECT_STATE_H
#define RR2NW_TANK_SUBJECT_STATE_H

class SimulationContext;
class KR_ObjectID;

struct STankLifecycleProbeSummary
{
    int available;
    int validStarts;
    int dynamicReady;
    int renderReady;
    int cannonReady;
    int scheduledMoves;
    int cadenceBounded;
    int renderedPoseFrames;
    int viewBoundaryResets;
    int bulletDamageApplications;
    int deathTransitions;
    int deathEffects;
    int saveStateRoundTrips;
    int massConsumerReady;
    int projectileReferenceReady;
    int outgoingProjectileStarts;
    int rollbacks;
};

struct STankGameplayTuningPatch
{
    bool hasMaxSpeed;
    bool hasAttackPower;
    bool hasAttackDelay;
    bool hasMass;
    bool hasProjectile;
    double maxSpeed;
    double attackPower;
    double attackDelay;
    double mass;
    char projectile[64];
};

struct STankGameplayTuningState
{
    void *owner;
    char id[64];
    double maxSpeed;
    double attackPower;
    double attackDelay;
    double mass;
    char projectile[64];
};

// Moves one real mission-owned Tank origin into an authored reached area.
// The caller owns the enclosing continuation checkpoint and rollback. No
// temporary owner, route, or scheduler event is introduced by this stage.
struct STankMissionReachedStageSummary
{
    int available;
    int initiallyOutside;
    int moved;
    int reached;
    double targetX;
    double targetZ;
    double radius;
    double initialDistance;
    double finalDistance;
    char actor[96];
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
bool TankSubjectState_CaptureGameplayTuning(
    SimulationContext *context, const char *id,
    STankGameplayTuningState *state);
bool TankSubjectState_ApplyGameplayTuning(
    SimulationContext *context, const STankGameplayTuningState *state,
    const STankGameplayTuningPatch *patch);
bool TankSubjectState_RestoreGameplayTuning(
    SimulationContext *context, const STankGameplayTuningState *state);
bool TankSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    STankLifecycleProbeSummary *summary);
bool TankSubjectState_ProbeAttributeLifecycle(
    SimulationContext *context, const char *attributeName, double timeStamp,
    STankLifecycleProbeSummary *summary);
bool TankSubjectState_ProbeTunedAttributeLifecycle(
    SimulationContext *context, const char *attributeName,
    bool requireMassConsumer, const char *expectedProjectile,
    double timeStamp, STankLifecycleProbeSummary *summary);
bool TankSubjectState_StageMissionReachedCondition(
    SimulationContext *context, const KR_ObjectID &actor,
    double targetX, double targetZ, double radius,
    STankMissionReachedStageSummary *summary);

#endif
