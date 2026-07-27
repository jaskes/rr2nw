#ifndef RR2NW_WAV_RESOURCE_STATE_H
#define RR2NW_WAV_RESOURCE_STATE_H

#include "WAVObj.h"

class SimulationContext;

class WAVObjTable : public ct_ClassTable
{
 private:
    WAVObj *m_table;

 public:
    WAVObjTable();
    virtual ~WAVObjTable();
    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
    int capacity() const { return m_maxObjectQnty; }
};

extern WAVObjTable __wavObjTable;

void WAVResourceState_Link();
int WAVResourceState_RosterSize(SimulationContext *context);
int WAVResourceState_Capacity();
unsigned long long WAVResourceState_Fingerprint(SimulationContext *context);
bool WAVResourceState_AllLoaded(SimulationContext *context);

#endif
