#ifndef RR2NW_SMOKE_VISUAL_STATE_H
#define RR2NW_SMOKE_VISUAL_STATE_H

class SimulationContext;

enum ESmokeVisualResourcePresence
{
    SMOKE_VISUAL_RESOURCES_NONE = 0,
    SMOKE_VISUAL_RESOURCES_COMPLETE = 1,
    SMOKE_VISUAL_RESOURCES_PARTIAL = 2,
    SMOKE_VISUAL_RESOURCES_INVALID = 3
};

void SmokeVisualState_Link();
ESmokeVisualResourcePresence SmokeVisualState_InspectResources(
    unsigned long long *fingerprint);
bool SmokeVisualState_Resolve(SimulationContext *context);
bool SmokeVisualState_Ready(SimulationContext *context);
unsigned long long SmokeVisualState_Fingerprint(SimulationContext *context);
void SmokeVisualState_Release();

#endif
