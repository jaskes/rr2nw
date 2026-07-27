#ifndef RR2NW_ARTEFACT_ATTRIBUTE_STATE_H
#define RR2NW_ARTEFACT_ATTRIBUTE_STATE_H

#include "enum/spaceenum.h"
#include "graph.h"
#include "storage/h/attr.h"

class CViewObjectModel;

class AttributeArtefact : public ct_Attribute
{
 public:
    CViewObjectModel *m_cacheSkin;
    virtual void update(double ts);
    unsigned long m_rayColor;
    GR_HTEXTURE m_coronaHText;
    unsigned long m_coronaColor;
    int m_portalTable;
    virtual int receiveEvent(KR_Event &event);

    ct_AttrItem m_array[15];
    double m_radius;
    int m_riceCnt;
    ct_AttrStr m_skinName;
    int m_lightColor;
    int m_brightness;
    double m_lightRadius;
    int m_rayRGB;
    int m_useLight;
    int m_useRay;
    int m_useCorona;
    double m_maxCoronaR;
    int m_coronaAlpha;
    int m_coronaRGB;
    ct_AttrStr m_coronaName;
    double m_coronaR;

    AttributeArtefact()
    {
        m_cacheSkin = 0;
        m_rayColor = 0;
        m_coronaHText = 0;
        m_coronaColor = 0;
        m_portalTable = ct_NULLID;

        m_radius = 5;
        m_riceCnt = 10;
        strncpy(m_skinName, "", sizeof(ct_AttrStr) - 1);
        m_lightColor = LIGHT_COLOR_VIOLET;
        m_brightness = 127;
        m_lightRadius = 15;
        m_rayRGB = 0xFFFFFF;
        m_useLight = 1;
        m_useRay = 1;
        m_useCorona = 1;
        m_maxCoronaR = 8;
        m_coronaAlpha = 100;
        m_coronaRGB = 0xFFFFFF;
        strncpy(m_coronaName, "corona.spr", sizeof(ct_AttrStr) - 1);
        m_coronaR = 0.2;

        m_array[0].set("m_radius", m_radius);
        m_array[1].set("m_riceCnt", m_riceCnt);
        m_array[2].set("m_skinName", m_skinName);
        m_array[3].set("m_lightColor", m_lightColor);
        m_array[4].set("m_brightness", m_brightness);
        m_array[5].set("m_lightRadius", m_lightRadius);
        m_array[6].set("m_rayRGB", m_rayRGB);
        m_array[7].set("m_useLight", m_useLight);
        m_array[8].set("m_useRay", m_useRay);
        m_array[9].set("m_useCorona", m_useCorona);
        m_array[10].set("m_maxCoronaR", m_maxCoronaR);
        m_array[11].set("m_coronaAlpha", m_coronaAlpha);
        m_array[12].set("m_coronaRGB", m_coronaRGB);
        m_array[13].set("m_coronaName", m_coronaName);
        m_array[14].set("m_coronaR", m_coronaR);
        linkTable(m_array, 15);
    }
};

class AttributeTableArtefact : public ct_AttributeTable
{
 protected:
    AttributeArtefact *m_table;

 public:
    AttributeTableArtefact()
    {
        m_table = NULL;
        registerClass("ArtefactAttr");
    }

    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
};

extern AttributeTableArtefact __attrArtefactTable;

void ArtefactAttributeState_Link();
bool ArtefactAttributeState_IsRetailDefault(const KR_ObjectID &objectID);

#endif
