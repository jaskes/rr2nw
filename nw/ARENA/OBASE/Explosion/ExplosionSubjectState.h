#ifndef RR2NW_EXPLOSION_SUBJECT_STATE_H
#define RR2NW_EXPLOSION_SUBJECT_STATE_H

#include "mathlib.h"
#include "storage/h/classtab.h"

class SimulationContext;
class WAVObj;

typedef bool (*ExplosionImpulseDispatch)(void *user,
                                         const CFVector3 &impulse,
                                         double factor);

struct ExplosionImpactRequest
{
    CFVector3 position;
    double timeStamp;
    KR_ObjectID damageOwner;
    ct_ClassTableID subjectTable;
    int attributeIndex;
    const char *objectName;
};

struct ExplosionImpactProbeSummary
{
    int invalidStarts;
    int allocationRollbacks;
    int queuedCommands;
    int queueRollbacks;
    int executedCommands;
    int damageApplications;
    int impulseApplications;
    double expectedDamage;
    double impactPositionX;
    double impactPositionY;
    double impactPositionZ;
    double expectedImpulseX;
    double expectedImpulseY;
    double expectedImpulseZ;
    double impulseFactor;
};

struct ExplosionSoundProbeSummary
{
    int startedSounds;
    int dependencyGateSkips;
    int rolledBackSounds;
};

struct ExplosionParticleProbeSummary
{
    int startedBranches;
    int simpleParticles;
    int snakeParticles;
    int rays;
    int dependencyGateSkips;
    int moveSteps;
    int expiredParents;
    int rolledBackBranches;
};

void ExplosionSubjectState_Link();
bool ExplosionSubjectState_TableReady(SimulationContext *context,
                                      int expectedCapacity);
int ExplosionSubjectState_Capacity();
int ExplosionSubjectState_LiveCount();
bool ExplosionSubjectState_BindImpulseTarget(
    SimulationContext *context, const KR_ObjectID &target,
    void *user, ExplosionImpulseDispatch dispatch);
void ExplosionSubjectState_UnbindImpulseTarget(
    SimulationContext *context);
bool ExplosionSubjectState_ImpulseTargetReady(
    SimulationContext *context, const KR_ObjectID &target);
bool ExplosionSubjectState_ParentSoundMatches(
    SimulationContext *context, const KR_ObjectID &parent,
    const WAVObj *wav, const CFVector3 &position,
    bool playing, int playCount);
bool ExplosionSubjectState_ParentParticleCounts(
    SimulationContext *context, const KR_ObjectID &parent,
    int *simpleParticles, int *snakeParticles, int *rays);
int ExplosionSubjectState_ParticleBranchLiveCount();
int ExplosionSubjectState_ParticleBranchCapacity();
bool ExplosionSubjectState_LightRosterReady(SimulationContext *context);
const char *ExplosionSubjectState_LightProbeAttributeName(
    SimulationContext *context);
const char *ExplosionSubjectState_SoundProbeAttributeName(
    SimulationContext *context);
void ExplosionSubjectState_ReleaseLightFrame();
unsigned long long ExplosionSubjectState_Fingerprint(
    SimulationContext *context);
bool ExplosionSubjectState_QueueBatch(
    SimulationContext *context, const ExplosionImpactRequest *requests,
    int requestCount, KR_ObjectID *children);
bool ExplosionSubjectState_RollbackQueued(
    SimulationContext *context, const KR_ObjectID *children,
    int childCount);
bool ExplosionSubjectState_ExecuteNow(
    SimulationContext *context, const ExplosionImpactRequest &request,
    int *damageApplications);
bool ExplosionSubjectState_ProbeLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionImpactProbeSummary *summary);
bool ExplosionSubjectState_ProbeDamageLifecycle(
    SimulationContext *context, const char *attributeName,
    const KR_ObjectID &target, const KR_ObjectID &damageOwner,
    double timeStamp, ExplosionImpactProbeSummary *summary);
bool ExplosionSubjectState_ProbeLightLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp);
bool ExplosionSubjectState_ProbeSoundLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionSoundProbeSummary *summary);
bool ExplosionSubjectState_ProbeParticleLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionParticleProbeSummary *summary);

#endif
