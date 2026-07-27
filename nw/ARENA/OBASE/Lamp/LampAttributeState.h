#ifndef RR2NW_LAMP_ATTRIBUTE_STATE_H
#define RR2NW_LAMP_ATTRIBUTE_STATE_H

#include "graph.h"
#include "storage/h/attr.h"

class SimulationContext;

class AttributeLamp : public ct_Attribute
{
 public:
    GR_HTEXTURE m_coronaHText;
    unsigned long m_coronaColor;
    double m_coronaFadeCoeff;

    ct_AttrItem m_array[26];
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
    double m_createIncMin;
    double m_createIncMax;
    int m_lightMode;
    double m_maxTimeLife;
    double m_sleepTime;
    double m_maxBrightTime;
    double m_FadeInCoeff;
    double m_FadeOutCoeff;
    int m_onLand;
    int m_isMoving;
    int m_movementType;
    double m_movementDeltaT;
    int m_hasParticle;
    double m_particleWidth;

    AttributeLamp();
    virtual void update(double ts);
};

class AttributeTableLamp : public ct_AttributeTable
{
 protected:
    AttributeLamp *m_table;

 public:
    AttributeTableLamp();
    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
    int capacity() const { return m_maxObjectQnty; }
};

extern AttributeTableLamp __attrLampTable;

void LampAttributeState_Link();
unsigned long long LampAttributeState_Fingerprint(SimulationContext *context);
int LampAttributeState_RosterSize(SimulationContext *context);
int LampAttributeState_Capacity();
bool LampAttributeState_IsKnownRoster(SimulationContext *context);

#endif
