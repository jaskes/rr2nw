#ifndef RR2NW_TAXI_SUBJECT_STATE_H
#define RR2NW_TAXI_SUBJECT_STATE_H

#include "kernel/h/krtypes.h"

class SimulationContext;

struct STaxiSubjectLifecycleProbeSummary
{
    int invalidStarts;
    int validStarts;
    int renderReady;
    int soundReady;
    int rollbacks;
};

struct STaxiVehicleTransitionProbeSummary
{
    int availableTaxis;
    int invalidTargets;
    int transitions;
    int attributeTransfers;
    int poseTransfers;
    int payloadTransfers;
    int removedTaxis;
    int rollbacks;
};

struct STaxiVehicleProximityState
{
    KR_ObjectID nearestTaxi;
    double nearestDistance;
    double activationDistance;
    int availableTaxis;
    int nearbyTaxis;
};

void TaxiSubjectState_Link();
bool TaxiSubjectState_TableReady(SimulationContext *context,
                                 int expectedCapacity);
int TaxiSubjectState_Capacity();
int TaxiSubjectState_LiveCount();
int TaxiSubjectState_SoundCount();
bool TaxiSubjectState_AllReady(SimulationContext *context);
unsigned long long TaxiSubjectState_Fingerprint(
    SimulationContext *context);
bool TaxiSubjectState_IsKnownRetailRoster(SimulationContext *context);
bool TaxiSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    STaxiSubjectLifecycleProbeSummary *summary);
bool TaxiSubjectState_ProbeVehicleTransition(
    SimulationContext *context, const KR_ObjectID &vehicle,
    double timeStamp, STaxiVehicleTransitionProbeSummary *summary);
bool TaxiSubjectState_InspectVehicleProximity(
    SimulationContext *context, const KR_ObjectID &vehicle,
    STaxiVehicleProximityState *state);
KR_ObjectID TaxiSubjectState_FirstObject(SimulationContext *context);
KR_ObjectID TaxiSubjectState_FirstPanelVehicleObject(
    SimulationContext *context);

#endif
