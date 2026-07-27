#ifndef RR2NW_SKIN_RESOURCE_STATE_H
#define RR2NW_SKIN_RESOURCE_STATE_H

#include "kernel/h/krtypes.h"

class SimulationContext;
class CViewObjectModel;

bool SkinResourceState_ResolveLoadedModel(SimulationContext *context,
                                          const char *objectName,
                                          KR_ObjectID *objectID,
                                          CViewObjectModel **model);

void SkinResourceState_Link();
int SkinResourceState_ModelCount(SimulationContext *context);
int SkinResourceState_SpriteCount(SimulationContext *context);
unsigned long long SkinResourceState_Fingerprint(SimulationContext *context);
bool SkinResourceState_AllLoaded(SimulationContext *context);

#endif
