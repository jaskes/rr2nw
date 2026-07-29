#ifndef RR2NW_EXPLOSION_ATTRIBUTE_STATE_H
#define RR2NW_EXPLOSION_ATTRIBUTE_STATE_H

#include <cstring>

#include "graph.h"
#include "storage/h/attr.h"

class CViewObjectModel;
class SimulationContext;
class WAVObj;

enum EExplosionSmokeVisualResourcePresence
{
    EXPLOSION_SMOKE_VISUAL_RESOURCES_NONE = 0,
    EXPLOSION_SMOKE_VISUAL_RESOURCES_COMPLETE = 1,
    EXPLOSION_SMOKE_VISUAL_RESOURCES_PARTIAL = 2,
    EXPLOSION_SMOKE_VISUAL_RESOURCES_INVALID = 3
};

class AttributeExplosion : public ct_Attribute
{
 public:
    enum { MAX_BRIGHT = 255, COLLINE = 8 };

    int m_brightness[MAX_BRIGHT];
    unsigned long m_color0;
    unsigned long m_color1;
    unsigned long m_color2;
    unsigned long m_color3;
    unsigned long m_colBuf[COLLINE * 3];
    GR_HTEXTURE m_hTexture;
    unsigned long m_colorSnTail;
    unsigned long m_colorSnHead;
    unsigned long m_colorSnCenter;
    CViewObjectModel *m_cacheSkin;
    WAVObj *m_wav;
    ct_ClassTableID m_ctsndID;
    unsigned long m_rayColor;
    ct_ClassTableID m_smokeTableID;
    KR_ObjectID m_smokeAttrID;

    ct_AttrItem m_array[90];
    double m_moveTimeInc;
    double m_minPartSize;
    double m_maxPartSize;
    double m_minPartSnSize;
    double m_maxPartSnSize;
    int m_minPartCnt;
    int m_maxPartCnt;
    int m_minPartSnCnt;
    int m_maxPartSnCnt;
    int m_minPieceCnt;
    int m_maxPieceCnt;
    int m_minPieceSmokeCnt;
    int m_maxPieceSmokeCnt;
    int m_minSmokeCnt;
    int m_maxSmokeCnt;
    int m_RGB0;
    int m_RGB1;
    int m_RGB2;
    int m_RGB3;
    double m_radius;
    double m_createRadius;
    double m_createSmokeRadius;
    double m_minPartSpeed;
    double m_maxPartSpeed;
    double m_minPieceSpeed;
    double m_maxPieceSpeed;
    double m_minPartTimeLife;
    double m_maxPartTimeLife;
    double m_minPartSnTimeLife;
    double m_maxPartSnTimeLife;
    double m_minPieceTimeLife;
    double m_maxPieceTimeLife;
    double m_minPieceSmTimeLife;
    double m_maxPieceSmTimeLife;
    double m_minSmokeTimeLife;
    double m_maxSmokeTimeLife;
    int m_sRGB0;
    int m_sRGB1;
    int m_sRGB2;
    int m_sRGB3;
    double m_minSmokeA;
    double m_maxSmokeA;
    double m_minSmokeB;
    double m_maxSmokeB;
    double m_minSmokeC;
    double m_maxSmokeC;
    double m_minSmokeTA;
    double m_maxSmokeTA;
    double m_minSmokeTB;
    double m_maxSmokeTB;
    double m_minSmokeTC;
    double m_maxSmokeTC;
    double m_minSmokeSpeed;
    double m_maxSmokeSpeed;
    double m_minMulSpeed;
    double m_maxMulSpeed;
    ct_AttrStr m_smokeName;
    double m_ofsVAngle;
    double m_ofsHAngle;
    double m_ofsSpeed;
    int m_snRGBtail;
    int m_snRGBhead;
    int m_snRGBcenter;
    double m_snDeltaT;
    int m_snPartCnt;
    ct_AttrStr m_pieceName;
    double m_minPieceOySpeed;
    double m_maxPieceOySpeed;
    double m_minPieceOxSpeed;
    double m_maxPieceOxSpeed;
    double m_lightOffset;
    double m_lightRadius;
    int m_lightColor;
    double m_lightTimeLife;
    double m_radiusDamage;
    double m_power;
    ct_AttrStr m_soundName;
    int m_useRay;
    int m_minRayCnt;
    int m_maxRayCnt;
    double m_minRayLen;
    double m_maxRayLen;
    double m_minRayWidth;
    double m_maxRayWidth;
    int m_rayRGB;
    double m_traceNewPuffTime;
    double m_ofsSpeedMul;
    ct_AttrStr m_traceSmokeName;
    int m_useLight;
    double m_impulseCoeff;

    AttributeExplosion();
};

class AttributeTableExplosion : public ct_AttributeTable
{
 protected:
    AttributeExplosion *m_table;

 public:
    AttributeTableExplosion();
    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
};

extern AttributeTableExplosion __attrExplosionTable;

void ExplosionAttributeState_Link();
unsigned long long ExplosionAttributeState_Fingerprint(
    SimulationContext *context);
bool ExplosionAttributeState_IsKnownRoster(SimulationContext *context);
int ExplosionAttributeState_RosterSize(SimulationContext *context);
bool ExplosionAttributeState_ResolveEncodedIndex(
    SimulationContext *context, int encodedIndex,
    AttributeExplosion **attribute);
const char *ExplosionAttributeState_FirstAttributeName(
    SimulationContext *context);
bool ExplosionAttributeState_SoundCachesUnresolved(
    SimulationContext *context);
bool ExplosionAttributeState_ProbeSoundReferenceAtomicity(
    SimulationContext *context);
bool ExplosionAttributeState_ResolveSoundReferences(
    SimulationContext *context);
bool ExplosionAttributeState_SoundReferencesResolved(
    SimulationContext *context);
unsigned long long ExplosionAttributeState_SoundReferenceFingerprint(
    SimulationContext *context);
bool ExplosionAttributeState_IsKnownSoundReferenceRoster(
    SimulationContext *context);
bool ExplosionAttributeState_ParticleCachesUnresolved(
    SimulationContext *context);
bool ExplosionAttributeState_ProbeParticleVisualAtomicity(
    SimulationContext *context);
bool ExplosionAttributeState_ResolveParticleVisuals(
    SimulationContext *context);
bool ExplosionAttributeState_ParticleVisualsResolved(
    SimulationContext *context);
unsigned long long ExplosionAttributeState_ParticleVisualFingerprint(
    SimulationContext *context);
bool ExplosionAttributeState_IsKnownParticleVisualRoster(
    SimulationContext *context);
void ExplosionAttributeState_ClearParticleVisuals(
    SimulationContext *context);
EExplosionSmokeVisualResourcePresence
ExplosionAttributeState_InspectSmokeVisualResources(
    SimulationContext *context, unsigned long long *fingerprint);
bool ExplosionAttributeState_SmokeVisualCachesUnresolved(
    SimulationContext *context);
bool ExplosionAttributeState_ProbeSmokeVisualAtomicity(
    SimulationContext *context);
bool ExplosionAttributeState_ResolveSmokeVisuals(
    SimulationContext *context);
bool ExplosionAttributeState_SmokeVisualsResolved(
    SimulationContext *context);
unsigned long long ExplosionAttributeState_SmokeVisualFingerprint(
    SimulationContext *context);
bool ExplosionAttributeState_IsKnownSmokeVisualRoster(
    SimulationContext *context);
void ExplosionAttributeState_ClearSmokeVisuals(
    SimulationContext *context);

#endif
