#ifndef RR2NW_TAXI_ATTRIBUTE_STATE_H
#define RR2NW_TAXI_ATTRIBUTE_STATE_H

class SimulationContext;

void TaxiAttributeState_Link();
unsigned long long TaxiAttributeState_Fingerprint(SimulationContext *context);
int TaxiAttributeState_RosterSize(SimulationContext *context);
int TaxiAttributeState_Capacity();
bool TaxiAttributeState_IsKnownRoster(SimulationContext *context);
bool TaxiAttributeState_CachesUnresolved(SimulationContext *context);

#endif
