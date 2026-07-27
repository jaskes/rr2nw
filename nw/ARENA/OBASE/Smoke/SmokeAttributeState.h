#ifndef RR2NW_SMOKE_ATTRIBUTE_STATE_H
#define RR2NW_SMOKE_ATTRIBUTE_STATE_H

#include "graph.h"
#include "storage/h/attr.h"

class SimulationContext;

class AttributeSmoke : public ct_Attribute
{
 public:
    enum { MAX_COLOR = 32 };

    GR_HTEXTURE m_cacheImage;
    unsigned long m_cacheColor;
    unsigned long colors[MAX_COLOR];

    virtual void update(double ts);

    ct_AttrItem m_array[39];
    double m_radius;
    int m_onLand;
    int m_maxBlob;
    double m_minDirInc;
    double m_maxDirInc;
    double m_rndOfs;
    double m_ofsHAngle;
    double m_ofsVAngle;
    double m_ofsSpeed;
    double m_dirHAngle;
    double m_dirVAngle;
    double m_dirSpeed;
    double m_maxTimeLife;
    double m_minRA;
    double m_maxRA;
    double m_minRB;
    double m_maxRB;
    double m_minRC;
    double m_maxRC;
    double m_minTA;
    double m_maxTA;
    double m_minTB;
    double m_maxTB;
    double m_minTC;
    double m_maxTC;
    ct_AttrStr m_imageName;
    int m_RGB;
    double m_timeIncrement;
    int RGB0;
    int RGB1;
    int RGB2;
    int RGB3;
    int m_isColorGradient;
    int m_useRndDir;
    double m_rndDir;
    double m_r0;
    double m_r1;
    double m_r2;
    double m_r3;

    AttributeSmoke()
    {
        m_cacheImage = NULL;
        m_cacheColor = 0;
        for (int i = 0; i < MAX_COLOR; ++i)
            colors[i] = 0;

        m_radius = 3;
        m_onLand = 1;
        m_maxBlob = 4;
        m_minDirInc = 0.9;
        m_maxDirInc = 0.91;
        m_rndOfs = 1;
        m_ofsHAngle = 0;
        m_ofsVAngle = 0;
        m_ofsSpeed = 0.5;
        m_dirHAngle = 0.0;
        m_dirVAngle = 0.0;
        m_dirSpeed = 1.0;
        m_maxTimeLife = 20;
        m_minRA = 0;
        m_maxRA = 0;
        m_minRB = 0;
        m_maxRB = 0;
        m_minRC = 1;
        m_maxRC = 2;
        m_minTA = 0;
        m_maxTA = 0;
        m_minTB = 0;
        m_maxTB = 0;
        m_minTC = 200;
        m_maxTC = 255;
        strncpy(m_imageName, "Smoke.spr", sizeof(ct_AttrStr) - 1);
        m_imageName[sizeof(ct_AttrStr) - 1] = 0;
        m_RGB = 0;
        m_timeIncrement = 0.02;
        RGB0 = 0;
        RGB1 = 0;
        RGB2 = 0;
        RGB3 = 0;
        m_isColorGradient = 0;
        m_useRndDir = 0;
        m_rndDir = 0.2;
        m_r0 = 0.1;
        m_r1 = 1;
        m_r2 = 1;
        m_r3 = 0.5;

        m_array[0].set("m_radius", m_radius);
        m_array[1].set("m_onLand", m_onLand);
        m_array[2].set("m_maxBlob", m_maxBlob);
        m_array[3].set("m_minDirInc", m_minDirInc);
        m_array[4].set("m_maxDirInc", m_maxDirInc);
        m_array[5].set("m_rndOfs", m_rndOfs);
        m_array[6].set("m_ofsHAngle", m_ofsHAngle);
        m_array[7].set("m_ofsVAngle", m_ofsVAngle);
        m_array[8].set("m_ofsSpeed", m_ofsSpeed);
        m_array[9].set("m_dirHAngle", m_dirHAngle);
        m_array[10].set("m_dirVAngle", m_dirVAngle);
        m_array[11].set("m_dirSpeed", m_dirSpeed);
        m_array[12].set("m_maxTimeLife", m_maxTimeLife);
        m_array[13].set("m_minRA", m_minRA);
        m_array[14].set("m_maxRA", m_maxRA);
        m_array[15].set("m_minRB", m_minRB);
        m_array[16].set("m_maxRB", m_maxRB);
        m_array[17].set("m_minRC", m_minRC);
        m_array[18].set("m_maxRC", m_maxRC);
        m_array[19].set("m_minTA", m_minTA);
        m_array[20].set("m_maxTA", m_maxTA);
        m_array[21].set("m_minTB", m_minTB);
        m_array[22].set("m_maxTB", m_maxTB);
        m_array[23].set("m_minTC", m_minTC);
        m_array[24].set("m_maxTC", m_maxTC);
        m_array[25].set("m_imageName", m_imageName);
        m_array[26].set("m_RGB", m_RGB);
        m_array[27].set("m_timeIncrement", m_timeIncrement);
        m_array[28].set("RGB0", RGB0);
        m_array[29].set("RGB1", RGB1);
        m_array[30].set("RGB2", RGB2);
        m_array[31].set("RGB3", RGB3);
        m_array[32].set("m_isColorGradient", m_isColorGradient);
        m_array[33].set("m_useRndDir", m_useRndDir);
        m_array[34].set("m_rndDir", m_rndDir);
        m_array[35].set("m_r0", m_r0);
        m_array[36].set("m_r1", m_r1);
        m_array[37].set("m_r2", m_r2);
        m_array[38].set("m_r3", m_r3);
        linkTable(m_array, 39);
    }
};

class AttributeTableSmoke : public ct_AttributeTable
{
 protected:
    AttributeSmoke *m_table;

 public:
    AttributeTableSmoke()
    {
        m_table = NULL;
        registerClass("SmokeAttr");
    }

    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
};

extern AttributeTableSmoke __attrSmokeTable;

void SmokeAttributeState_Link();
unsigned long long SmokeAttributeState_RetailFingerprint(
    SimulationContext *context);
bool SmokeAttributeState_IsRetailRoster(SimulationContext *context);

#endif
