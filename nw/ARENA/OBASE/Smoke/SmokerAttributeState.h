#ifndef RR2NW_SMOKER_ATTRIBUTE_STATE_H
#define RR2NW_SMOKER_ATTRIBUTE_STATE_H

#include "graph.h"
#include "storage/h/attr.h"

class SimulationContext;

class AttributeSmoker : public ct_Attribute
{
 public:
    KR_ObjectID m_smokeAttrID;
    ct_ClassTableID m_smokeTableID;
    GR_HTEXTURE m_coronaHText;
    unsigned long m_coronaColor;

    virtual void update(double ts);

    ct_AttrItem m_array[19];
    double m_createIncMin;
    double m_createIncMax;
    ct_AttrStr m_smokeAttrName;
    double m_maxTimeLife;
    ct_AttrStr m_smokeTableName;
    int m_onLand;
    int m_useLight;
    int m_lightColor;
    double m_minLightBright;
    double m_maxLightBright;
    double m_lightRadius;
    double m_lightBrightStep;
    double m_lightOffset;
    int m_useCorona;
    int m_coronaRGB;
    double m_coronaR;
    int m_coronaAlpha;
    ct_AttrStr m_coronaName;
    double m_maxCoronaR;

    AttributeSmoker();
};

class AttributeTableSmoker : public ct_AttributeTable
{
 protected:
    AttributeSmoker *m_table;

 public:
    AttributeTableSmoker();
    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
    int capacity() const { return m_maxObjectQnty; }
};

extern AttributeTableSmoker __attrSmokerTable;

void SmokerAttributeState_Link();
bool SmokerAttributeState_Resolve(SimulationContext *context,
                                  const char *objectName,
                                  KR_ObjectID *objectID);
unsigned long long SmokerAttributeState_Fingerprint(
    SimulationContext *context);
int SmokerAttributeState_RosterSize(SimulationContext *context);
int SmokerAttributeState_Capacity();
bool SmokerAttributeState_IsKnownRoster(SimulationContext *context);
bool SmokerAttributeState_CachesUnresolved(SimulationContext *context);
bool SmokerAttributeState_ResolveReferences(SimulationContext *context);
bool SmokerAttributeState_ReferencesResolved(SimulationContext *context);
bool SmokerAttributeState_RuntimeReady(SimulationContext *context);
unsigned long long SmokerAttributeState_ReferenceFingerprint(
    SimulationContext *context);
bool SmokerAttributeState_IsKnownReferenceRoster(
    SimulationContext *context);

#endif
