#ifndef RR2NW_PORTAL_ACTIVE_WORLD_STATE_H
#define RR2NW_PORTAL_ACTIVE_WORLD_STATE_H

#include "kernel/h/krtypes.h"

#include <vector>

class SimulationContext;

struct SPortalAdmissionProbeSummary {
  int portalCount;
  int attachedRejected;
  int fullRejected;
  int consumed;
  int occupiedAdvanced;
  int eventResidueCleared;

  SPortalAdmissionProbeSummary();
};

struct SPortalTransitionProbeSummary {
  int portalCount;
  int fullPortal;
  int collisionAccepted;
  int transitionRequested;

  SPortalTransitionProbeSummary();
};

struct SPortalFinalAdmissionProbeSummary {
  int portalCount;
  int slots;
  int occupiedBefore;
  int occupiedPrepared;
  int remaining;

  SPortalFinalAdmissionProbeSummary();
};

struct SPortalPresentationProbeSummary {
  int portalCount;
  int singularStatus;
  int fewStatus;
  int manyStatus;
  int restoredStatus;
  int messagesPublished;
  int arabeskPresent;
  int arabeskRemoved;
  int arabeskRecreated;
  int arabeskRestoreRemoved;

  SPortalPresentationProbeSummary();
};

void PortalActiveWorldState_Link();
const char *PortalActiveWorldState_LastFailure();
bool PortalActiveWorldState_InitializeLevelSubjects(
    SimulationContext *context);
void PortalActiveWorldState_ReleaseLevelSubjects(
    SimulationContext *context);
bool PortalActiveWorldState_RequestTransition(
    SimulationContext *context, const KR_ObjectID &portal);
bool PortalActiveWorldState_PublishAdmissionStatus(
    SimulationContext *context, const KR_ObjectID &portal);
bool PortalActiveWorldState_TransitionPending();
bool PortalActiveWorldState_TakeTransitionRequest();
int PortalActiveWorldState_LiveCount(SimulationContext *context);
bool PortalActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes);
bool PortalActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes);
bool PortalActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool PortalActiveWorldState_PrepareStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool PortalActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes);
bool PortalActiveWorldState_RejectAttachedProbe(
    SimulationContext *context, const KR_ObjectID &artefact,
    SPortalAdmissionProbeSummary *summary);
bool PortalActiveWorldState_AdmissionProbe(
    SimulationContext *context, const KR_ObjectID &artefact,
    double timeStamp, SPortalAdmissionProbeSummary *summary);
bool PortalActiveWorldState_PrepareFinalAdmissionProbe(
    SimulationContext *context,
    SPortalFinalAdmissionProbeSummary *summary);
bool PortalActiveWorldState_StageTransitionProbe(
    SimulationContext *context, double timeStamp,
    SPortalTransitionProbeSummary *summary);
bool PortalActiveWorldState_StageReadyTransitionProbe(
    SimulationContext *context, double timeStamp,
    SPortalTransitionProbeSummary *summary);
bool PortalActiveWorldState_StagePresentationProbe(
    SimulationContext *context,
    SPortalPresentationProbeSummary *summary);

#endif
