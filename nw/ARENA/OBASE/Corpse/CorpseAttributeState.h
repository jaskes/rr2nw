#ifndef RR2NW_CORPSE_ATTRIBUTE_STATE_H
#define RR2NW_CORPSE_ATTRIBUTE_STATE_H

#include "storage/h/attr.h"

class CViewObjectModel;
class SimulationContext;

class AttributeCorpse : public ct_Attribute
{
 public:
    CViewObjectModel *m_cacheSkin;
    KR_ObjectID m_skinID;
    KR_ObjectID m_smokerAttrID;
    KR_ObjectID m_fireAttrID;
    ct_ClassTableID m_smokerTableID;

    ct_AttrItem m_array[13];
    ct_AttrStr m_skinName;
    int m_isBurning;
    ct_AttrStr m_smokerAttr;
    double m_fireOffsetX;
    double m_fireOffsetY;
    double m_fireOffsetZ;
    double m_minLifeTime;
    ct_AttrStr m_smokerTable;
    int m_isSmoking;
    ct_AttrStr m_fireAttr;
    float m_corpseOffsetX;
    double m_corpseOffsetY;
    float m_corpseOffsetZ;

    AttributeCorpse();
    virtual void update(double ts);
};

class AttributeTableCorpse : public ct_AttributeTable
{
 protected:
    AttributeCorpse *m_table;

 public:
    AttributeTableCorpse();
    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
    int capacity() const { return m_maxObjectQnty; }
};

extern AttributeTableCorpse __attrCorpseTable;

void CorpseAttributeState_Link();
unsigned long long CorpseAttributeState_Fingerprint(
    SimulationContext *context);
int CorpseAttributeState_RosterSize(SimulationContext *context);
int CorpseAttributeState_Capacity();
bool CorpseAttributeState_IsKnownRoster(SimulationContext *context);
bool CorpseAttributeState_CachesUnresolved(SimulationContext *context);
bool CorpseAttributeState_ResolveReferences(SimulationContext *context);
bool CorpseAttributeState_ReferencesResolved(SimulationContext *context);
bool CorpseAttributeState_RuntimeReady(SimulationContext *context);
bool CorpseAttributeState_ResolveEncodedIndex(
    SimulationContext *context, int encodedIndex,
    AttributeCorpse **attribute);
unsigned long long CorpseAttributeState_ReferenceFingerprint(
    SimulationContext *context);
bool CorpseAttributeState_IsKnownReferenceRoster(
    SimulationContext *context);

#endif
