#ifndef RR2NW_SOUND_OBJECT_STATE_H
#define RR2NW_SOUND_OBJECT_STATE_H

class SimulationContext;

void SoundObjectState_Link();
bool SoundObjectState_TableReady(SimulationContext *context, int capacity);
bool SoundObjectState_DeviceFree();
int SoundObjectState_Capacity();
int SoundObjectState_LiveCount();
unsigned long long SoundObjectState_Fingerprint(SimulationContext *context);
bool SoundObjectState_ProbeLifecycle(SimulationContext *context,
                                     const char *wavName,
                                     double timeStamp);

#endif
