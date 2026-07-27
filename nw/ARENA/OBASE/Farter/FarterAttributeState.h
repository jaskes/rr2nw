#ifndef RR2NW_FARTER_ATTRIBUTE_STATE_H
#define RR2NW_FARTER_ATTRIBUTE_STATE_H

#include "storage/h/attr.h"

class SimulationContext;
class WAVObj;

class AttributeFarter : public ct_Attribute
{
 public:
    WAVObj *m_wav;
    ct_ClassTableID m_ctsndID;

    ct_AttrItem m_array[1];
    ct_AttrStr m_soundName;

    AttributeFarter();
    virtual void update(double ts);
};

class AttributeTableFarter : public ct_AttributeTable
{
 protected:
    AttributeFarter *m_table;

 public:
    AttributeTableFarter();
    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
    int capacity() const { return m_maxObjectQnty; }
};

extern AttributeTableFarter __attrFarterTable;

void FarterAttributeState_Link();
unsigned long long FarterAttributeState_Fingerprint(
    SimulationContext *context);
int FarterAttributeState_RosterSize(SimulationContext *context);
int FarterAttributeState_Capacity();
bool FarterAttributeState_IsKnownRoster(SimulationContext *context);
bool FarterAttributeState_CachesUnresolved(SimulationContext *context);
bool FarterAttributeState_ResolveReferences(SimulationContext *context);
bool FarterAttributeState_ReferencesResolved(SimulationContext *context);
bool FarterAttributeState_RuntimeReady(SimulationContext *context);
unsigned long long FarterAttributeState_ReferenceFingerprint(
    SimulationContext *context);
bool FarterAttributeState_IsKnownReferenceRoster(
    SimulationContext *context);

#endif
