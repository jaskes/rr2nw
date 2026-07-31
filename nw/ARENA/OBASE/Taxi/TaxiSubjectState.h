#ifndef RR2NW_TAXI_SUBJECT_STATE_H
#define RR2NW_TAXI_SUBJECT_STATE_H

#include <string>
#include <vector>

#include "kernel/h/krtypes.h"
#include "mathlib.h"

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

// Debug tooling deliberately exposes the same Level-local TaxiAttr ->
// VehicleAttr mapping consumed by the retail transition path.  It does not
// invent a parallel vehicle registry or accept arbitrary object identifiers.
struct STaxiDebugVehicleType
{
    std::string taxiAttribute;
    std::string vehicleAttribute;
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
bool TaxiSubjectState_DebugVehicleCatalog(
    SimulationContext *context,
    std::vector<STaxiDebugVehicleType> *catalog,
    std::string *failure);
bool TaxiSubjectState_DebugSpawn(
    SimulationContext *context, const char *taxiAttribute,
    const char *objectName, const CFVector3 &position,
    double angle, double timeStamp, KR_ObjectID *spawned,
    std::string *failure);
bool TaxiSubjectState_DebugTakeVehicle(
    SimulationContext *context, const KR_ObjectID &vehicle,
    const KR_ObjectID &taxi, double timeStamp,
    std::string *failure);

#endif
