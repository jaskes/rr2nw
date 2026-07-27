#ifndef RR2NW_SKIN_RESOURCE_STATE_H
#define RR2NW_SKIN_RESOURCE_STATE_H

class SimulationContext;

void SkinResourceState_Link();
int SkinResourceState_ModelCount(SimulationContext *context);
int SkinResourceState_SpriteCount(SimulationContext *context);
unsigned long long SkinResourceState_Fingerprint(SimulationContext *context);
bool SkinResourceState_AllLoaded(SimulationContext *context);

#endif
