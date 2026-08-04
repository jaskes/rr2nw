#ifndef RR2NW_PEOPLE_SUBJECT_STATE_H
#define RR2NW_PEOPLE_SUBJECT_STATE_H

#include <vector>

#include "kernel/h/krtypes.h"

class SimulationContext;

struct SPeopleLifecycleProbeSummary
{
    int validStarts;
    int routePhaseExact;
    int routeEndPolicies;
    int corridorProjection;
    int obstacleRecovery;
    int contactResponse;
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

struct SPeopleRouteMotionProbeSummary
{
    int available;
    int phaseExact;
    int groundedRouteEvent;
    int finiteMotion;
    int movedTowardTarget;
    int boundedStep;
    int startNode;
    int targetNode;
    int backSpaceNode;
    double startMoveDelay;
    double elapsed;
    double displacement;
    char owner[96];
};

struct SPeopleCombatProbeSummary
{
    int available;
    int attackerReady;
    int targetReady;
    int routeDisplacement;
    int targetAcquired;
    int targetCadence;
    int projectileStarted;
    int damageDelivered;
    int deathTransition;
    int deathEffects;
    int rollbacks;
    char attacker[96];
    char projectile[64];
};

struct SPeopleCombatScheduleSummary
{
    int livePeople;
    int shooters;
    int commandedShooters;
    int commanderInterfaces;
    int scheduledFindEnemy;
    int scheduledMotion;
    int attackStates;
    int malformedQueues;
};

struct SPeopleLiveCombatTelemetry
{
    unsigned long long sampleFrames;
    unsigned long long rosterSamples;
    unsigned long long moveEvents;
    unsigned long long eligibleMoveEvents;
    unsigned long long displacedMoveEvents;
    unsigned long long stationaryMoveEvents;
    unsigned long long legacySlopeReleaseOpportunities;
    unsigned long long legacySlopeReleasedMoves;
    unsigned long long attackMoveEvents;
    unsigned long long contactMoveEvents;
    unsigned long long findEvents;
    unsigned long long eligibleFindEvents;
    unsigned long long targetAcquisitions;
    unsigned long long targetMisses;
    unsigned long long attackStateSamples;
    unsigned long long shotsStarted;
    unsigned long long damageApplications;
    unsigned long long killTransitions;
    unsigned long long explosionEffects;
    unsigned long long corpseEffects;
    double maximumHorizontalDisplacement;
    double lastStationaryDeltaTime;
    double lastStationaryMoveSpeed;
    double lastStationaryX;
    double lastStationaryY;
    double lastStationaryZ;
    double lastStationaryMoveStartX;
    double lastStationaryMoveStartZ;
    double lastStationaryDirectionX;
    double lastStationaryDirectionZ;
    double lastStationaryTargetX;
    double lastStationaryTargetZ;
    double lastStationaryPredictedDisplacement;
    double lastStationaryObstacleRecoveryTime;
    int lastStationaryContactCode;
    int lastStationaryState;
    int lastStationaryStopped;
    int lastStationaryPreviousNode;
    int lastStationaryCurrentNode;
    int lastStationaryOnLand;
    int lastStationaryStopIfAttack;
    double lastStationaryDeltaZeroSpeed;
    char lastStationaryOwner[96];
    char lastStationaryAttribute[96];
    char lastStationaryRoute[96];
    char lastLegacySlopeReleasedOwner[96];
    char lastAcquiringOwner[96];
    char lastShootingOwner[96];
    char lastDamagedOwner[96];
    char lastKilledOwner[96];
};

// A bounded acceptance setup made only from People owners created by the
// selected public mission.  The caller captures/restores the complete Level
// continuation around this setup; no probe-only People owners are introduced.
struct SPeopleMissionCombatStageSummary
{
    int baselinePeople;
    int livePeople;
    int missionPeople;
    int hostilePairs;
    int staged;
    int attackerOnLand;
    int targetOnLand;
    double separation;
    double targetDamageBefore;
    double targetDamageStaged;
    double timeStamp;
    double attackerViewDistanceBefore;
    double attackerViewDistanceStaged;
    KR_ObjectID attackerID;
    KR_ObjectID targetID;
    char attacker[96];
    char target[96];
    char attackerCommander[96];
    char targetCommander[96];
    char attackerAttribute[96];
};

struct SPeopleMissionCombatLiveState
{
    int attackerExists;
    int targetExists;
    int attackerAttackState;
    int targetAttackState;
    int attackerHasExactTarget;
    int attackerShot;
    int targetDamageSourceAttacker;
    int targetKilled;
    int bullets;
    int explosions;
    int corpses;
    double targetDamage;
    char attackerTarget[96];
};

// Read-only acceptance view of a shooter created by the selected public
// mission. Unlike SPeopleMissionCombatStageSummary this never changes an
// owner, attribute, scheduler event, route, position, or health value.
struct SPeopleNaturalCombatSummary
{
    int baselinePeople;
    int livePeople;
    int missionPeople;
    int missionShooters;
    int available;
    int visible;
    int attackState;
    int hasTarget;
    int targetIsDynamic;
    int shot;
    double initialX;
    double initialY;
    double initialZ;
    double currentX;
    double currentY;
    double currentZ;
    double horizontalDisplacement;
    double targetDistance;
    double initialShootTime;
    double currentShootTime;
    KR_ObjectID actorID;
    char actor[96];
    char commander[96];
    char attribute[96];
    char route[96];
    char target[96];
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
bool PeopleSubjectState_ProbeNewestDelayedRoute(
    SimulationContext *context,
    SPeopleRouteMotionProbeSummary *summary);
bool PeopleSubjectState_ProbeCombatLifecycle(
    SimulationContext *context, double timeStamp,
    SPeopleCombatProbeSummary *summary);
bool PeopleSubjectState_AuditCombatScheduling(
    SimulationContext *context,
    SPeopleCombatScheduleSummary *summary);
void PeopleSubjectState_ResetLiveCombatTelemetry();
bool PeopleSubjectState_LiveCombatTelemetry(
    SPeopleLiveCombatTelemetry *telemetry);
bool PeopleSubjectState_SampleLiveCombat(SimulationContext *context);
bool PeopleSubjectState_ObjectIDs(
    SimulationContext *context, std::vector<KR_ObjectID> *objects);
bool PeopleSubjectState_StageMissionCombat(
    SimulationContext *context,
    const std::vector<KR_ObjectID> &baselineObjects,
    double timeStamp, SPeopleMissionCombatStageSummary *summary);
bool PeopleSubjectState_InspectMissionCombat(
    SimulationContext *context,
    const SPeopleMissionCombatStageSummary *stage,
    SPeopleMissionCombatLiveState *state);
bool PeopleSubjectState_RestoreMissionCombatTuning(
    SimulationContext *context,
    const SPeopleMissionCombatStageSummary *stage);
bool PeopleSubjectState_ScheduleMissionCombatDeath(
    SimulationContext *context,
    const SPeopleMissionCombatStageSummary *stage,
    double timeStamp);
bool PeopleSubjectState_SelectNaturalMissionCombat(
    SimulationContext *context,
    const std::vector<KR_ObjectID> &baselineObjects,
    SPeopleNaturalCombatSummary *summary);
bool PeopleSubjectState_SelectNaturalMissionCombatCohort(
    SimulationContext *context,
    const std::vector<KR_ObjectID> &baselineObjects,
    std::vector<SPeopleNaturalCombatSummary> *summaries);
bool PeopleSubjectState_InspectNaturalMissionCombat(
    SimulationContext *context,
    const SPeopleNaturalCombatSummary *selection,
    SPeopleNaturalCombatSummary *summary);

#endif
