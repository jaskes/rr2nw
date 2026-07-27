#ifndef RR2NW_BIRD_ATTRIBUTE_STATE_H
#define RR2NW_BIRD_ATTRIBUTE_STATE_H

#include "storage/h/attr.h"

class CViewObjectModel;

class AttributeBird : public ct_Attribute
{
 public:
    virtual void update(double ts);

    ct_AttrItem m_array[4];
    double m_calcNewPosIncrement;
    double m_calcPosIncrement;
    ct_AttrStr m_skinName;
    double m_speed;
    CViewObjectModel *m_cacheSkin;

    AttributeBird()
    {
        m_calcNewPosIncrement = 5;
        m_calcPosIncrement = 0.080012345678;
        strncpy(m_skinName, "Bird.0", sizeof(ct_AttrStr) - 1);
        m_speed = 20;
        m_cacheSkin = 0;

        m_array[0].set("m_calcNewPosIncrement", m_calcNewPosIncrement);
        m_array[1].set("m_calcPosIncrement", m_calcPosIncrement);
        m_array[2].set("m_skinName", m_skinName);
        m_array[3].set("m_speed", m_speed);
        linkTable(m_array, 4);
    }
};

class AttributeTableBird : public ct_AttributeTable
{
 protected:
    AttributeBird *m_table;

 public:
    AttributeTableBird()
    {
        m_table = NULL;
        registerClass("BirdAttr");
    }

    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
};

extern AttributeTableBird __attrBirdTable;

void BirdAttributeState_Link();
bool BirdAttributeState_IsRetailDefault(const KR_ObjectID &objectID);

#endif
