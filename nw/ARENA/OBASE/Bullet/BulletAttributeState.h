#ifndef RR2NW_BULLET_ATTRIBUTE_STATE_H
#define RR2NW_BULLET_ATTRIBUTE_STATE_H

#include <cstring>

#include "graph.h"
#include "storage/h/attr.h"
#include "obase/sound/WAVObj.h"

enum { RR2NW_BULLET_COLOR_GRAD = 16 };

class CViewObjectModel;
class SimulationContext;

class AttributeBullet : public ct_Attribute
{
 public:
    GR_HTEXTURE m_cacheImage;
    GR_HTEXTURE m_cacheImageFront;
    WAVObj *m_wav;
    ct_ClassTableID m_ctsndID;

    virtual void update(double ts);
    virtual int receiveEvent(KR_Event &event);

    unsigned long m_colorGrad[RR2NW_BULLET_COLOR_GRAD];
    ct_ClassTableID m_smokeTableID;
    KR_ObjectID m_smokeAttrID;
    int m_cacheSparkAttrTable;
    int m_cacheSparkTable;
    int m_cacheColor;
    int m_cacheSparkAttr;
    int m_cacheOutSparkAttr;
    int m_cacheSplashAttr;
    int m_cacheExplAttr;
    int m_cacheExplosionTable;
    CViewObjectModel *m_cacheSkin;

    ct_AttrItem m_array[41];
    int m_type;
    int m_RGB;
    int m_RGB0;
    double m_moveTimeIncrement;
    double m_chkClzTimeIncrement;
    ct_AttrStr m_sparkAttr;
    ct_AttrStr m_outSparkAttr;
    ct_AttrStr m_splashAttr;
    double m_massa;
    double m_startSpeed;
    ct_AttrStr m_sparkTable;
    ct_AttrStr m_sparkAttrTable;
    double m_radius0;
    double m_radius1;
    double m_length;
    double m_step0;
    double m_step;
    int m_useLight;
    double m_lightRadius;
    int m_lightBrightness;
    int m_lightColor;
    ct_AttrStr m_smokeTableName;
    ct_AttrStr m_smokeAttrName;
    ct_AttrStr m_explAttrName;
    ct_AttrStr m_trace;
    int m_hasTrace;
    double m_traceMinDist;
    ct_AttrStr m_traceWidthString;
    double m_traceFlatRatio;
    double m_traceAllFlatDist;
    ct_AttrStr m_traceAss;
    double m_traceSegmentLength;
    ct_AttrStr m_shootSndName;
    int m_traceExist;
    ct_AttrStr m_traceTexture;
    int m_useBarellSmoke;
    int m_useSkin;
    ct_AttrStr m_skinName;
    double m_rotSpeedOx;
    double m_rotSpeedOy;
    double m_rotSpeedOz;

    AttributeBullet();
};

class AttributeTableBullet : public ct_AttributeTable
{
 protected:
    AttributeBullet *m_table;

 public:
    AttributeTableBullet();
    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
};

extern AttributeTableBullet __bulletAttrTable;

void BulletAttributeState_Link();
int BulletAttributeState_RosterSize(SimulationContext *context);
const char *BulletAttributeState_AttributeNameAt(
    SimulationContext *context, int index);
int BulletAttributeState_AttributeUsesSkinAt(
    SimulationContext *context, int index);
int BulletAttributeState_Capacity();
int BulletAttributeState_SubjectCapacity();
bool BulletAttributeState_SubjectTableReady(SimulationContext *context);
unsigned long long BulletAttributeState_Fingerprint(
    SimulationContext *context);
bool BulletAttributeState_IsKnownRoster(SimulationContext *context);
bool BulletAttributeState_CachesUnresolved(SimulationContext *context);
bool BulletAttributeState_ResolveReferences(SimulationContext *context);
bool BulletAttributeState_ReferencesResolved(SimulationContext *context);
unsigned long long BulletAttributeState_ReferenceFingerprint(
    SimulationContext *context);
bool BulletAttributeState_IsKnownReferenceRoster(
    SimulationContext *context);
const char *BulletAttributeState_LastError();
bool BulletAttributeState_ResolveEncodedIndex(
    SimulationContext *context, int encodedIndex,
    AttributeBullet **attribute);
const char *BulletAttributeState_FirstAttributeName(
    SimulationContext *context);
const char *BulletAttributeState_FirstBarrelSmokeAttributeName(
    SimulationContext *context);
const char *BulletAttributeState_FirstParticleAttributeName(
    SimulationContext *context);
const char *BulletAttributeState_FirstSkinAttributeName(
    SimulationContext *context);

#endif
