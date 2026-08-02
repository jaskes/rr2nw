#ifndef RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H
#define RR2NW_RECRUIT_CENTER_SUBJECT_STATE_H

class SimulationContext;

struct RecruitCenterMissionProbeSummary
{
    int stagedMissions;
    int conditionReferences;
    int routeReferences;
    int deferredCommands;
};

void RecruitCenterSubjectState_Link();
bool RecruitCenterSubjectState_TableReady(SimulationContext *context);
int RecruitCenterSubjectState_Capacity();
int RecruitCenterSubjectState_LiveCount();
int RecruitCenterSubjectState_ConfiguredCount();
int RecruitCenterSubjectState_VideoCount();
int RecruitCenterSubjectState_DefaultTaxiCount();
int RecruitCenterSubjectState_DictionaryCount();
unsigned long long RecruitCenterSubjectState_Fingerprint(
    SimulationContext *context);
bool RecruitCenterSubjectState_StageMissionProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    RecruitCenterMissionProbeSummary *summary);
const char *RecruitCenterSubjectState_LastError();

#endif
