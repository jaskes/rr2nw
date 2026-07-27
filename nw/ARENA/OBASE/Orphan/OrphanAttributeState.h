#ifndef RR2NW_ORPHAN_ATTRIBUTE_STATE_H
#define RR2NW_ORPHAN_ATTRIBUTE_STATE_H

#include "storage/h/attr.h"

class CViewObjectModel;

class AttributeOrphan : public ct_Attribute
{
 public:
    CViewObjectModel *m_cacheSkin;
    KR_ObjectID m_skinID;
    KR_ObjectID m_attrForVehicle;
    virtual void update(double ts);

    int m_cacheExplAttr;
    int m_cacheExplosionTable;
    ct_ClassTableID m_smokeTableID;
    KR_ObjectID m_smokeAttrID;

    ct_AttrItem m_array[7];
    ct_AttrStr m_explAttrName;
    double m_deltaT;
    double m_collisionT;
    double m_explosionTime;
    double m_minSpeed;
    ct_AttrStr m_smokeAttrName;
    double m_smokeDamage;

    AttributeOrphan()
    {
        m_cacheSkin = 0;
        m_skinID = KR_ObjectID::NUL();
        m_attrForVehicle = KR_ObjectID::NUL();
        m_cacheExplAttr = ct_NULLID;
        m_cacheExplosionTable = ct_NULLID;
        m_smokeTableID = ct_NULLID;
        m_smokeAttrID = KR_ObjectID::NUL();

        strncpy(m_explAttrName, "Expl.Attr.Default",
                sizeof(ct_AttrStr) - 1);
        m_deltaT = 0.2;
        m_collisionT = 0.1;
        m_explosionTime = 0.5;
        m_minSpeed = 1;
        strncpy(m_smokeAttrName, "Smoke.Attr.Small",
                sizeof(ct_AttrStr) - 1);
        m_smokeDamage = 0.5;

        m_array[0].set("m_explAttrName", m_explAttrName);
        m_array[1].set("m_deltaT", m_deltaT);
        m_array[2].set("m_collisionT", m_collisionT);
        m_array[3].set("m_explosionTime", m_explosionTime);
        m_array[4].set("m_minSpeed", m_minSpeed);
        m_array[5].set("m_smokeAttrName", m_smokeAttrName);
        m_array[6].set("m_smokeDamage", m_smokeDamage);
        linkTable(m_array, 7);
    }
};

class AttributeTableOrphan : public ct_AttributeTable
{
 protected:
    AttributeOrphan *m_table;

 public:
    AttributeTableOrphan()
    {
        m_table = NULL;
        registerClass("OrphanAttr");
    }

    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
};

extern AttributeTableOrphan __attrOrphanTable;

void OrphanAttributeState_Link();
bool OrphanAttributeState_IsRetailDefault(const KR_ObjectID &objectID);

#endif
