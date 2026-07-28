#ifndef RR2NW_BULLET_SUBJECT_STATE_H
#define RR2NW_BULLET_SUBJECT_STATE_H

class SimulationContext;

void BulletSubjectState_Link();
bool BulletSubjectState_TableReady(SimulationContext *context, int capacity);
int BulletSubjectState_Capacity();
int BulletSubjectState_LiveCount();
unsigned long long BulletSubjectState_Fingerprint(
    SimulationContext *context);
bool BulletSubjectState_ProbeBallisticLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, int *moveCount);

#endif
