#ifndef RR2NW_SPARK_ATTRIBUTE_STATE_H
#define RR2NW_SPARK_ATTRIBUTE_STATE_H

#include "storage/h/attr.h"

class CViewTexture;
class SimulationContext;

class SparkPhase
{
 public:
    int u0, v0, u1, v1;
    double time;
    int brightness, color;
    double radius;

    void init(int u0l, int v0l, int u1l, int v1l, double t,
              int b, int c, double r)
    {
        u0 = u0l;
        v0 = v0l;
        u1 = u1l;
        v1 = v1l;
        time = t;
        brightness = b;
        color = c;
        radius = r;
    }
};

class AttributeSpark : public ct_Attribute
{
 public:
    enum { MAX_PHASE = 15 };

    virtual void update(double ts);
    virtual int receiveEvent(KR_Event &event);

    SparkPhase m_phase[MAX_PHASE];
    int m_phaseCnt;

    ct_AttrItem m_array[3];
    double m_maxRadius;
    ct_AttrStr m_skin;
    CViewTexture *m_cacheSkin;

    AttributeSpark()
    {
        m_maxRadius = 2.5;
        strncpy(m_skin, "sk.Fusion.0", sizeof(ct_AttrStr) - 1);
        m_cacheSkin = 0;

        m_array[0].set("m_maxRadius", m_maxRadius);
        m_array[1].set("m_skin", m_skin);
        m_array[2].set("m_cacheSkin", EDI_NONE, &m_cacheSkin);

        linkTable(m_array, 3);
        m_phaseCnt = 1;
        m_phase[0].init(2, 2, 24 * 2 - 1, 45 * 2 - 2, 0.8,
                        200, 0, 1);
    }
};

class AttributeTableSpark : public ct_AttributeTable
{
 protected:
    AttributeSpark *m_table;

 public:
    AttributeTableSpark()
    {
        m_table = NULL;
        registerClass("SparkAttr");
    }

    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
};

extern AttributeSpark __defaultSparkAttr;
extern AttributeTableSpark __attrSparkTable;

void SparkAttributeState_Link();
bool SparkAttributeState_IsRetailFlash(const KR_ObjectID &objectID);
bool SparkAttributeState_ResolveEncodedIndex(
    SimulationContext *context, int encodedIndex,
    AttributeSpark **attribute);
bool SparkAttributeState_ResolveVisualResources(SimulationContext *context);
bool SparkAttributeState_VisualResourcesResolved(
    SimulationContext *context);
void SparkAttributeState_ClearVisualResources(SimulationContext *context);
unsigned long long SparkAttributeState_VisualResourceFingerprint(
    SimulationContext *context);

#endif
