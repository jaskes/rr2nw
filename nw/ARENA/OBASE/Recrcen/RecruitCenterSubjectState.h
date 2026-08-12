#ifndef RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H
#define RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H

#include <cstdint>
#include <vector>

class SimulationContext;

struct RecruitCenterMissionProbeSummary
{
    int stagedMissions;
    int conditionReferences;
    int routeReferences;
    int deferredCommands;
    int briefingCommands;
    int scriptCommands;
    int executedScripts;
    int presentedBriefings;
    int centerPresentationAttempts;
    int presentedCenterFlicks;
    int presentedHostilityBriefings;
    int centerPresentationFailures;
    int restoredResultPresentations;
    int postBriefingCollisionEvents;
    int postBriefingCollisionSuppressions;
    int postBriefingPresentationRepeats;
    int createdMissionObjects;
    int reclaimedRouteObjects;
    int replacedHowitzerObjects;
    int reboundConditionReferences;
    int preSatisfiedKillConditionReferences;
    int capacityLimitedConditionReferences;
    int scriptRollbacks;
    int rejectedNonPlayerCollisions;
    int acceptedPlayerCollisions;
    int admissionEvents;
    int ejections;
    int deferredArtefactRewards;
    int checkpointChains;
    int checkpointCommands;
    int activeCheckpoints;
    int reachedScriptCommands;
    int reachedScriptEvents;
    char capacityLimitedConditionName[81];
    char preSatisfiedKillConditionName[81];
    char centerName[81];
    char projectName[81];
};

struct RecruitCenterCheckpointProbeSummary
{
    int chains;
    int checkpoints;
    int activeCheckpoints;
    int pendingTriggers;
    int pendingLevelTransitions;
    int targetLevelIndex;
    int completedChains;
    int executedScripts;
    int rollbacks;
    int activeOrdinal;
    int activeIsFirst;
    int activeIsComplete;
    double activeX;
    double activeY;
    double activeZ;
    double activeRadius;
    char centerName[81];
    char projectName[81];
    char activeScript[261];
};

struct RecruitCenterReachedScriptProbeSummary
{
    int events;
    int pendingTriggers;
    int targetResolved;
    int targetDynamic;
    int targetInside;
    int executedScripts;
    int rollbacks;
    int requeues;
    int pendingLevelTransitions;
    int targetLevelIndex;
    double eventTime;
    double targetX;
    double targetY;
    double targetZ;
    double radius;
    double interval;
    char centerName[81];
    char actorName[81];
    char script[261];
};

struct RecruitCenterMissionResultProbeSummary
{
    int conditionsRemoved;
    int statusTransitions;
    int completedMissions;
    int rewardsCreated;
    int rewardInterfaceReady;
    int repaired;
    int refilled;
    int repeatIdempotent;
    int missionsBefore;
    int missionsAfter;
    int totalMissionsBefore;
    int totalMissionsAfter;
    int pickupAccepted;
    int bidirectionalAttachment;
    int carryEventsCancelled;
    int carryMoveMatched;
    int dropInputAccepted;
    int bidirectionalDetach;
    int dropMoveEventScheduled;
    int dropMotionMatched;
    char centerName[81];
    char completedProjectName[81];
    char nextProjectName[81];
};

struct RecruitCenterMissionResultPresentationProbeSummary
{
    int conditionsRemoved;
    int statusPresentations;
    int resultPresentations;
    int resultPresentationRestores;
    int nextMissionStaged;
    int presentedBriefings;
    int missionsBefore;
    int missionsAfter;
    char centerName[81];
    char completedProjectName[81];
    char nextProjectName[81];
    char restoredMessage[121];
};

struct RecruitCenterMissionNoRewardResultProbeSummary
{
    int conditionsRemoved;
    int statusTransitions;
    int completedMissions;
    int resultPresentations;
    int rewardsCreated;
    int rewardAbsent;
    int completedProjectRetired;
    int repaired;
    int refilled;
    int repeatIdempotent;
    int missionsBefore;
    int missionsAfter;
    int totalMissionsBefore;
    int totalMissionsAfter;
    int scheduledChecksBefore;
    int scheduledChecksAfter;
    int reachedConditions;
    int failureGuardPreserved;
    int noNextCandidate;
    char centerName[81];
    char completedProjectName[81];
    char nextProjectName[81];
};

struct RecruitCenterMissionTerminalNoRewardStateSummary
{
    int missionAbsent;
    int projectRetired;
    int noNextCandidate;
    int scheduledChecks;
    int rewardDetached;
    char centerName[81];
    char completedProjectName[81];
};

struct RecruitCenterMissionNoRewardProgressionStateSummary
{
    int missionAbsent;
    int projectRetired;
    int nextCandidateExact;
    int scheduledChecks;
    int rewardDetached;
    char centerName[81];
    char completedProjectName[81];
    char nextProjectName[81];
};

struct RecruitCenterObjectiveStateSummary
{
    int missions;
    int totalMissions;
    int inProcessMissions;
    int successMissions;
    int failedMissions;
    int surrenderMissions;
    int summaryMissions;
    int routeMissions;
    int conditionReferences;
    int boundConditionReferences;
    int scheduledChecks;
    int distinctProjects;
    int distinctCommanders;
    int mapMissions;
    int mapTexts;
    int mapRoutes;
    int mapBindings;
    char firstProjectName[81];
    char secondProjectName[81];
};

struct RecruitCenterMissionTerminalProbeSummary
{
    int commandAccepted;
    int statusPresentations;
    int resultPresentations;
    int statusTransitions;
    int failedMissions;
    int surrenderedMissions;
    int removedMissions;
    int missionsBefore;
    int missionsAfter;
    int totalMissionsBefore;
    int totalMissionsAfter;
    int scheduledChecksBefore;
    int scheduledChecksAfter;
    int mapBindingsBefore;
    int mapBindingsAfter;
    int checkGraphExact;
    int rewardCreated;
    int damagePreserved;
    int ammunitionPreserved;
    int repeatIdempotent;
    char centerName[81];
    char projectName[81];
    char survivingProjectName[81];
};

void RecruitCenterSubjectState_Link();
bool RecruitCenterSubjectState_TableReady(SimulationContext *context);
int RecruitCenterSubjectState_Capacity();
int RecruitCenterSubjectState_LiveCount();
int RecruitCenterSubjectState_ConfiguredCount();
int RecruitCenterSubjectState_VideoCount();
int RecruitCenterSubjectState_DefaultTaxiCount();
int RecruitCenterSubjectState_DictionaryCount();
int RecruitCenterSubjectState_RejectedCollisionCount();
int RecruitCenterSubjectState_PlayerCollisionCount();
int RecruitCenterSubjectState_AdmissionCount();
int RecruitCenterSubjectState_StagedMissionCount();
int RecruitCenterSubjectState_ExistingMissionVisitCount();
int RecruitCenterSubjectState_NoProjectVisitCount();
int RecruitCenterSubjectState_EjectionCount();
int RecruitCenterSubjectState_AdmissionFailureCount();
unsigned long long RecruitCenterSubjectState_Fingerprint(
    SimulationContext *context);
bool RecruitCenterSubjectState_StageMissionProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    RecruitCenterMissionProbeSummary *summary);
bool RecruitCenterSubjectState_StageMissionExecutionProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    RecruitCenterMissionProbeSummary *summary);
bool RecruitCenterSubjectState_StageMissionExecutionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    bool *staged, RecruitCenterMissionProbeSummary *summary);
bool RecruitCenterSubjectState_StageMissionExecutionProbeForProject(
    SimulationContext *context, double timeStamp, const char *centerName,
    const char *projectName, bool *staged,
    RecruitCenterMissionProbeSummary *summary);
bool RecruitCenterSubjectState_EjectPlayerForCenter(
    SimulationContext *context, double timeStamp, const char *centerName);
bool RecruitCenterSubjectState_StageMissionPresentationProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    RecruitCenterMissionProbeSummary *summary);
bool RecruitCenterSubjectState_StageMissionPresentationProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    bool *staged, RecruitCenterMissionProbeSummary *summary);
bool RecruitCenterSubjectState_LastMissionSummary(
    RecruitCenterMissionProbeSummary *summary);
bool RecruitCenterSubjectState_ObjectiveState(
    SimulationContext *context, RecruitCenterObjectiveStateSummary *summary);
bool RecruitCenterSubjectState_CaptureCheckpointState(
    SimulationContext *context, std::vector<std::uint8_t> *bytes);
bool RecruitCenterSubjectState_ValidateCheckpointState(
    const std::vector<std::uint8_t> &bytes);
bool RecruitCenterSubjectState_ApplyCheckpointState(
    SimulationContext *context, const std::vector<std::uint8_t> &bytes);
bool RecruitCenterSubjectState_CheckpointStateMatches(
    SimulationContext *context, const std::vector<std::uint8_t> &bytes);
void RecruitCenterSubjectState_ClearCheckpointState();
bool RecruitCenterSubjectState_PollCheckpoints(
    SimulationContext *context);
bool RecruitCenterSubjectState_CheckpointPending();
bool RecruitCenterSubjectState_DeferPendingCheckpoint();
bool RecruitCenterSubjectState_ProcessPendingCheckpoint(
    SimulationContext *context, double timeStamp);
bool RecruitCenterSubjectState_LevelTransitionPending();
bool RecruitCenterSubjectState_PeekLevelTransition(int *levelIndex);
bool RecruitCenterSubjectState_RejectLevelTransition();
bool RecruitCenterSubjectState_CheckpointProbe(
    SimulationContext *context, RecruitCenterCheckpointProbeSummary *summary);
bool RecruitCenterSubjectState_StageActiveCheckpointProbe(
    SimulationContext *context);
bool RecruitCenterSubjectState_ReachedScriptProbe(
    SimulationContext *context, const char *centerName,
    RecruitCenterReachedScriptProbeSummary *summary);
bool RecruitCenterSubjectState_DispatchReachedScriptProbe(
    SimulationContext *context, const char *centerName, bool moveInside);
bool RecruitCenterSubjectState_ReachedScriptPending();
bool RecruitCenterSubjectState_DeferPendingReachedScript(
    SimulationContext *context);
bool RecruitCenterSubjectState_ProcessPendingReachedScript(
    SimulationContext *context, double timeStamp);
void RecruitCenterSubjectState_FailNextReachedScriptForTesting();
bool RecruitCenterSubjectState_CompleteMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionResultProbeSummary *summary);
bool RecruitCenterSubjectState_ResultPresentationHandoffProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionResultPresentationProbeSummary *summary);
bool RecruitCenterSubjectState_CompleteNoRewardMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionNoRewardResultProbeSummary *summary);
bool RecruitCenterSubjectState_CompleteTerminalNoRewardMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionNoRewardResultProbeSummary *summary);
bool RecruitCenterSubjectState_TerminalNoRewardStateProbeForCenter(
    SimulationContext *context, const char *centerName,
    const char *completedProjectName,
    RecruitCenterMissionTerminalNoRewardStateSummary *summary);
bool RecruitCenterSubjectState_NoRewardProgressionStateProbeForCenter(
    SimulationContext *context, const char *centerName,
    const char *completedProjectName, const char *nextProjectName,
    RecruitCenterMissionNoRewardProgressionStateSummary *summary);
bool RecruitCenterSubjectState_RewardCarrierState(
    SimulationContext *context, bool expectAttached);
bool RecruitCenterSubjectState_DropRewardProbe(
    SimulationContext *context, double timeStamp,
    RecruitCenterMissionResultProbeSummary *summary);
bool RecruitCenterSubjectState_FailMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionTerminalProbeSummary *summary);
bool RecruitCenterSubjectState_SurrenderMissionProbe(
    SimulationContext *context, double timeStamp,
    RecruitCenterMissionTerminalProbeSummary *summary);
bool RecruitCenterSubjectState_ResolveFailedMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionTerminalProbeSummary *summary);
bool RecruitCenterSubjectState_ResolveSurrenderedMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionTerminalProbeSummary *summary);
const char *RecruitCenterSubjectState_LastError();

#endif
