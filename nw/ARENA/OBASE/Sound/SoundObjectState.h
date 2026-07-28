#ifndef RR2NW_SOUND_OBJECT_STATE_H
#define RR2NW_SOUND_OBJECT_STATE_H

class SimulationContext;
class KR_ObjectID;
class WAVObj;

void SoundObjectState_Link();
bool SoundObjectState_TableReady(SimulationContext *context, int capacity);
bool SoundObjectState_DeviceFree();
int SoundObjectState_Capacity();
int SoundObjectState_LiveCount();
unsigned long long SoundObjectState_Fingerprint(SimulationContext *context);
bool SoundObjectState_Matches(const KR_ObjectID &objectID,
                              const WAVObj *wav,
                              double x,
                              double y,
                              double z,
                              bool positionValid,
                              bool playing,
                              int playCount);
bool SoundObjectState_ProbeLifecycle(SimulationContext *context,
                                     const char *wavName,
                                     double timeStamp);

#endif
