#ifndef RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H
#define RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H

class SimulationContext;

struct RecruitCenterMissionProbeSummary
{
    int stagedMissions;
    int conditionReferences;
    int routeReferences;
    int deferredCommands;
    int executedScripts;
    int presentedBriefings;
    int createdMissionObjects;
    int reboundConditionReferences;
    int scriptRollbacks;
    int rejectedNonPlayerCollisions;
    int acceptedPlayerCollisions;
    int admissionEvents;
    int ejections;
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
const char *RecruitCenterSubjectState_LastError();

#endif
