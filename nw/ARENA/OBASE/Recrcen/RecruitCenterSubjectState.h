#ifndef RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H
#define RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H

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
    int createdMissionObjects;
    int reboundConditionReferences;
    int scriptRollbacks;
    int rejectedNonPlayerCollisions;
    int acceptedPlayerCollisions;
    int admissionEvents;
    int ejections;
    int deferredArtefactRewards;
    char centerName[81];
    char projectName[81];
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
bool RecruitCenterSubjectState_CompleteMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionResultProbeSummary *summary);
bool RecruitCenterSubjectState_RewardCarrierState(
    SimulationContext *context, bool expectAttached);
bool RecruitCenterSubjectState_DropRewardProbe(
    SimulationContext *context, double timeStamp,
    RecruitCenterMissionResultProbeSummary *summary);
const char *RecruitCenterSubjectState_LastError();

#endif
