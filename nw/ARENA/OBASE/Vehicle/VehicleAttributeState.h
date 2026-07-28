#ifndef RR2NW_VEHICLE_ATTRIBUTE_STATE_H
#define RR2NW_VEHICLE_ATTRIBUTE_STATE_H

class SimulationContext;

void VehicleAttributeState_Link();
void VehicleAttributeState_SetCapacity(int capacity);
unsigned long long VehicleAttributeState_Fingerprint(
    SimulationContext *context);
int VehicleAttributeState_RosterSize(SimulationContext *context);
int VehicleAttributeState_Capacity();
bool VehicleAttributeState_IsKnownRoster(SimulationContext *context);
bool VehicleAttributeState_CachesUnresolved(SimulationContext *context);

#endif
