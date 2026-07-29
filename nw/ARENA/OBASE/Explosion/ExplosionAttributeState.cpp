#include "ExplosionAttributeState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>
#include <string>
#include <vector>

#include "filesys.h"
#include "ExplosionSubjectState.h"
#include "h/cachesmoke.h"
#include "obase/skin/SkinResourceState.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "storage/h/subject.h"

extern SDeviceList _dL;

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const int kExplosionParticleBranchCapacity = 128;

struct ParticleVisualRuntimeState
{
    SimulationContext *context;
    unsigned long long fingerprint;
    bool ready;
};

ParticleVisualRuntimeState g_particleVisualState = {};

struct PieceReferenceRuntimeState
{
    SimulationContext *context;
    unsigned long long fingerprint;
    bool ready;
};

PieceReferenceRuntimeState g_pieceReferenceState = {};

struct TraceReferenceRuntimeState
{
    SimulationContext *context;
    unsigned long long fingerprint;
    bool ready;
};

TraceReferenceRuntimeState g_traceReferenceState = {};

struct SmokeVisualRuntimeState
{
    SimulationContext *context;
    int textureCheckpoint;
    unsigned long long fingerprint;
    bool ready;
};

SmokeVisualRuntimeState g_smokeVisualState = {};

struct ParticleVisualCache
{
    unsigned long color[4];
    unsigned long snakeTail;
    unsigned long snakeHead;
    unsigned long snakeCenter;
    unsigned long ray;
};

struct SmokeVisualCache
{
    unsigned long colors[AttributeExplosion::COLLINE * 3];
    GR_HTEXTURE texture;
};

int LightBrightness(int index)
{
    int brightness = index * 20;
    if (brightness > 255)
        brightness = 255;
    if (index > AttributeExplosion::MAX_BRIGHT / 3)
    {
        const int tailIndex =
            index - AttributeExplosion::MAX_BRIGHT / 3;
        brightness = static_cast<int>(255.0 / tailIndex);
    }
    return brightness;
}

void InitializeLightBrightness(AttributeExplosion &attr)
{
    for (int index = 0; index < AttributeExplosion::MAX_BRIGHT; ++index)
        attr.m_brightness[index] = LightBrightness(index);
}

void HashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kHashPrime;
    }
}

void HashString(unsigned long long &hash, const char *value)
{
    HashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

void HashAttribute(unsigned long long &hash, AttributeExplosion &attr)
{
#define RR2NW_EXPLOSION_HASH(field) \
    HashBytes(hash, &attr.field, sizeof(attr.field))
    RR2NW_EXPLOSION_HASH(m_moveTimeInc);
    RR2NW_EXPLOSION_HASH(m_minPartSize);
    RR2NW_EXPLOSION_HASH(m_maxPartSize);
    RR2NW_EXPLOSION_HASH(m_minPartSnSize);
    RR2NW_EXPLOSION_HASH(m_maxPartSnSize);
    RR2NW_EXPLOSION_HASH(m_minPartCnt);
    RR2NW_EXPLOSION_HASH(m_maxPartCnt);
    RR2NW_EXPLOSION_HASH(m_minPartSnCnt);
    RR2NW_EXPLOSION_HASH(m_maxPartSnCnt);
    RR2NW_EXPLOSION_HASH(m_minPieceCnt);
    RR2NW_EXPLOSION_HASH(m_maxPieceCnt);
    RR2NW_EXPLOSION_HASH(m_minPieceSmokeCnt);
    RR2NW_EXPLOSION_HASH(m_maxPieceSmokeCnt);
    RR2NW_EXPLOSION_HASH(m_minSmokeCnt);
    RR2NW_EXPLOSION_HASH(m_maxSmokeCnt);
    RR2NW_EXPLOSION_HASH(m_RGB0);
    RR2NW_EXPLOSION_HASH(m_RGB1);
    RR2NW_EXPLOSION_HASH(m_RGB2);
    RR2NW_EXPLOSION_HASH(m_RGB3);
    RR2NW_EXPLOSION_HASH(m_radius);
    RR2NW_EXPLOSION_HASH(m_createRadius);
    RR2NW_EXPLOSION_HASH(m_createSmokeRadius);
    RR2NW_EXPLOSION_HASH(m_minPartSpeed);
    RR2NW_EXPLOSION_HASH(m_maxPartSpeed);
    RR2NW_EXPLOSION_HASH(m_minPieceSpeed);
    RR2NW_EXPLOSION_HASH(m_maxPieceSpeed);
    RR2NW_EXPLOSION_HASH(m_minPartTimeLife);
    RR2NW_EXPLOSION_HASH(m_maxPartTimeLife);
    RR2NW_EXPLOSION_HASH(m_minPartSnTimeLife);
    RR2NW_EXPLOSION_HASH(m_maxPartSnTimeLife);
    RR2NW_EXPLOSION_HASH(m_minPieceTimeLife);
    RR2NW_EXPLOSION_HASH(m_maxPieceTimeLife);
    RR2NW_EXPLOSION_HASH(m_minPieceSmTimeLife);
    RR2NW_EXPLOSION_HASH(m_maxPieceSmTimeLife);
    RR2NW_EXPLOSION_HASH(m_minSmokeTimeLife);
    RR2NW_EXPLOSION_HASH(m_maxSmokeTimeLife);
    RR2NW_EXPLOSION_HASH(m_sRGB0);
    RR2NW_EXPLOSION_HASH(m_sRGB1);
    RR2NW_EXPLOSION_HASH(m_sRGB2);
    RR2NW_EXPLOSION_HASH(m_sRGB3);
    RR2NW_EXPLOSION_HASH(m_minSmokeA);
    RR2NW_EXPLOSION_HASH(m_maxSmokeA);
    RR2NW_EXPLOSION_HASH(m_minSmokeB);
    RR2NW_EXPLOSION_HASH(m_maxSmokeB);
    RR2NW_EXPLOSION_HASH(m_minSmokeC);
    RR2NW_EXPLOSION_HASH(m_maxSmokeC);
    RR2NW_EXPLOSION_HASH(m_minSmokeTA);
    RR2NW_EXPLOSION_HASH(m_maxSmokeTA);
    RR2NW_EXPLOSION_HASH(m_minSmokeTB);
    RR2NW_EXPLOSION_HASH(m_maxSmokeTB);
    RR2NW_EXPLOSION_HASH(m_minSmokeTC);
    RR2NW_EXPLOSION_HASH(m_maxSmokeTC);
    RR2NW_EXPLOSION_HASH(m_minSmokeSpeed);
    RR2NW_EXPLOSION_HASH(m_maxSmokeSpeed);
    RR2NW_EXPLOSION_HASH(m_minMulSpeed);
    RR2NW_EXPLOSION_HASH(m_maxMulSpeed);
    HashString(hash, attr.m_smokeName);
    RR2NW_EXPLOSION_HASH(m_ofsVAngle);
    RR2NW_EXPLOSION_HASH(m_ofsHAngle);
    RR2NW_EXPLOSION_HASH(m_ofsSpeed);
    RR2NW_EXPLOSION_HASH(m_snRGBtail);
    RR2NW_EXPLOSION_HASH(m_snRGBhead);
    RR2NW_EXPLOSION_HASH(m_snRGBcenter);
    RR2NW_EXPLOSION_HASH(m_snDeltaT);
    RR2NW_EXPLOSION_HASH(m_snPartCnt);
    HashString(hash, attr.m_pieceName);
    RR2NW_EXPLOSION_HASH(m_minPieceOySpeed);
    RR2NW_EXPLOSION_HASH(m_maxPieceOySpeed);
    RR2NW_EXPLOSION_HASH(m_minPieceOxSpeed);
    RR2NW_EXPLOSION_HASH(m_maxPieceOxSpeed);
    RR2NW_EXPLOSION_HASH(m_lightOffset);
    RR2NW_EXPLOSION_HASH(m_lightRadius);
    RR2NW_EXPLOSION_HASH(m_lightColor);
    RR2NW_EXPLOSION_HASH(m_lightTimeLife);
    RR2NW_EXPLOSION_HASH(m_radiusDamage);
    RR2NW_EXPLOSION_HASH(m_power);
    HashString(hash, attr.m_soundName);
    RR2NW_EXPLOSION_HASH(m_useRay);
    RR2NW_EXPLOSION_HASH(m_minRayCnt);
    RR2NW_EXPLOSION_HASH(m_maxRayCnt);
    RR2NW_EXPLOSION_HASH(m_minRayLen);
    RR2NW_EXPLOSION_HASH(m_maxRayLen);
    RR2NW_EXPLOSION_HASH(m_minRayWidth);
    RR2NW_EXPLOSION_HASH(m_maxRayWidth);
    RR2NW_EXPLOSION_HASH(m_rayRGB);
    RR2NW_EXPLOSION_HASH(m_traceNewPuffTime);
    RR2NW_EXPLOSION_HASH(m_ofsSpeedMul);
    HashString(hash, attr.m_traceSmokeName);
    RR2NW_EXPLOSION_HASH(m_useLight);
    RR2NW_EXPLOSION_HASH(m_impulseCoeff);
#undef RR2NW_EXPLOSION_HASH
}

bool ParticleCacheIsZero(const AttributeExplosion &attr)
{
    return attr.m_color0 == 0 && attr.m_color1 == 0 &&
           attr.m_color2 == 0 && attr.m_color3 == 0 &&
           attr.m_colorSnTail == 0 && attr.m_colorSnHead == 0 &&
           attr.m_colorSnCenter == 0 && attr.m_rayColor == 0;
}

bool SmokeVisualCacheIsZero(const AttributeExplosion &attr)
{
    if (attr.m_hTexture != NULL)
        return false;
    for (int i = 0; i < AttributeExplosion::COLLINE * 3; ++i)
        if (attr.m_colBuf[i] != 0)
            return false;
    return true;
}

bool DeferredReferencesAreUnresolved(AttributeExplosion &attr)
{
    for (int i = 0; i < AttributeExplosion::MAX_BRIGHT; ++i)
        if (attr.m_brightness[i] != LightBrightness(i))
            return false;
    return true;
}

bool TraceCacheIsCoherent(SimulationContext *context,
                          AttributeExplosion &attr)
{
    if (!g_traceReferenceState.ready)
        return attr.m_smokeTableID == ct_NULLID &&
               attr.m_smokeAttrID.isNUL();
    if (g_traceReferenceState.context != context ||
        attr.m_smokeTableID == ct_NULLID || attr.m_smokeAttrID.isNUL())
        return false;
    KR_ObjectID smokeAttribute = KR_ObjectID::NUL();
    return attr.m_smokeTableID ==
               g_arena.searchSeanceClassTable("Smoke") &&
           SmokeAttributeState_Resolve(
               context, attr.m_traceSmokeName, &smokeAttribute) &&
           smokeAttribute == attr.m_smokeAttrID &&
           SmokeSubjectState_SimulationSupported(
               context, attr.m_traceSmokeName);
}

bool PieceCacheIsCoherent(SimulationContext *context,
                          AttributeExplosion &attr)
{
    if (!g_pieceReferenceState.ready)
        return attr.m_cacheSkin == NULL;
    if (g_pieceReferenceState.context != context ||
        attr.m_cacheSkin == NULL)
        return false;
    KR_ObjectID modelID = KR_ObjectID::NUL();
    CViewObjectModel *model = NULL;
    return SkinResourceState_ResolveLoadedModel(
               context, attr.m_pieceName, &modelID, &model) &&
           model == attr.m_cacheSkin;
}

int Red(const int color) { return (color >> 16) & 255; }
int Green(const int color) { return (color >> 8) & 255; }
int Blue(const int color) { return color & 255; }

unsigned long CreateParticleColor(int red, int green, int blue)
{
    if (_dL.currDevice == NULL)
        return 0;
    if (_dL.currDevice->swHw == GR_HARDWARE)
        return (static_cast<unsigned long>(red) << 24) |
               (static_cast<unsigned long>(green) << 16) |
               (static_cast<unsigned long>(blue) << 8);
    return GRCreateColor(red, green, blue);
}

ParticleVisualCache BuildParticleVisualCache(
    const AttributeExplosion &attr)
{
    ParticleVisualCache cache = {};
    cache.color[0] = CreateParticleColor(
        Red(attr.m_RGB0), Green(attr.m_RGB0), Blue(attr.m_RGB0));
    cache.color[1] = CreateParticleColor(
        Red(attr.m_RGB1), Green(attr.m_RGB1), Blue(attr.m_RGB1));
    cache.color[2] = CreateParticleColor(
        Red(attr.m_RGB2), Green(attr.m_RGB2), Blue(attr.m_RGB2));
    cache.color[3] = CreateParticleColor(
        Red(attr.m_RGB3), Green(attr.m_RGB3), Blue(attr.m_RGB3));
    cache.snakeTail = CreateParticleColor(
        Red(attr.m_snRGBtail), Green(attr.m_snRGBtail),
        Blue(attr.m_snRGBtail));
    cache.snakeHead = CreateParticleColor(
        Red(attr.m_snRGBhead), Green(attr.m_snRGBhead),
        Blue(attr.m_snRGBhead));
    cache.snakeCenter = CreateParticleColor(
        Red(attr.m_snRGBcenter), Green(attr.m_snRGBcenter),
        Blue(attr.m_snRGBcenter));
    // The recovered ray path is deliberately particle-sampled; retain the
    // source RGB in the same device color form as the other particle limbs.
    cache.ray = CreateParticleColor(
        Red(attr.m_rayRGB), Green(attr.m_rayRGB), Blue(attr.m_rayRGB));
    return cache;
}

bool ParticleCacheMatches(const AttributeExplosion &attr,
                          const ParticleVisualCache &cache)
{
    return attr.m_color0 == cache.color[0] &&
           attr.m_color1 == cache.color[1] &&
           attr.m_color2 == cache.color[2] &&
           attr.m_color3 == cache.color[3] &&
           attr.m_colorSnTail == cache.snakeTail &&
           attr.m_colorSnHead == cache.snakeHead &&
           attr.m_colorSnCenter == cache.snakeCenter &&
           attr.m_rayColor == cache.ray;
}

bool FiniteRange(double minimum, double maximum)
{
    return std::isfinite(minimum) && std::isfinite(maximum) &&
           minimum <= maximum;
}

bool CountRange(int minimum, int maximum)
{
    return minimum >= 0 && maximum >= minimum &&
           maximum <= kExplosionParticleBranchCapacity;
}

bool ParticleNumbersReady(const AttributeExplosion &attr)
{
    const int maximumBranches = attr.m_maxRayCnt + attr.m_maxPartCnt +
                                attr.m_maxPartSnCnt;
    if (!std::isfinite(attr.m_moveTimeInc) || attr.m_moveTimeInc <= 0.0 ||
        attr.m_moveTimeInc > 1.0 || !CountRange(attr.m_minRayCnt,
                                                attr.m_maxRayCnt) ||
        !CountRange(attr.m_minPartCnt, attr.m_maxPartCnt) ||
        !CountRange(attr.m_minPartSnCnt, attr.m_maxPartSnCnt) ||
        maximumBranches > kExplosionParticleBranchCapacity ||
        !std::isfinite(attr.m_createRadius) ||
        attr.m_createRadius < 0.0)
        return false;
    if (attr.m_maxPartCnt > 0 &&
        (!FiniteRange(attr.m_minPartSize, attr.m_maxPartSize) ||
         attr.m_minPartSize <= 0.0 ||
         !FiniteRange(attr.m_minPartSpeed, attr.m_maxPartSpeed) ||
         attr.m_minPartSpeed < 0.0 ||
         !FiniteRange(attr.m_minPartTimeLife,
                      attr.m_maxPartTimeLife) ||
         attr.m_minPartTimeLife <= 0.0))
        return false;
    if (attr.m_maxPartSnCnt > 0 &&
        (!FiniteRange(attr.m_minPartSnSize, attr.m_maxPartSnSize) ||
         attr.m_minPartSnSize <= 0.0 || attr.m_snPartCnt <= 0 ||
         attr.m_snPartCnt > 64 || !std::isfinite(attr.m_snDeltaT) ||
         attr.m_snDeltaT < 0.0 ||
         !FiniteRange(attr.m_minPartSnTimeLife,
                      attr.m_maxPartSnTimeLife) ||
         attr.m_minPartSnTimeLife <= 0.0))
        return false;
    if (attr.m_maxRayCnt > 0 &&
        (!FiniteRange(attr.m_minRayLen, attr.m_maxRayLen) ||
         attr.m_minRayLen <= 0.0 ||
         !FiniteRange(attr.m_minRayWidth, attr.m_maxRayWidth) ||
         attr.m_minRayWidth <= 0.0))
        return false;
    return true;
}

double PositiveRoot(double a, double b, double c)
{
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c))
        return 0.0;
    if (std::fabs(a) < 1.0e-5)
    {
        if (std::fabs(b) < 1.0e-5)
            return 1.0e10;
        const double result = -c / b;
        return result < 1.001 ? 1.0e10 : result;
    }
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
        return 1.0e10;
    const double root = std::sqrt(discriminant);
    const double first = (-b + root) / (2.0 * a);
    const double second = (-b - root) / (2.0 * a);
    const double result = first > second ? first : second;
    return result < 0.001 ? 1.0e10 : result;
}

bool SmokeNumbersReady(const AttributeExplosion &attr)
{
    const int maximumBranches = attr.m_maxRayCnt + attr.m_maxPartCnt +
        attr.m_maxPartSnCnt + attr.m_maxPieceCnt +
        attr.m_maxPieceSmokeCnt + attr.m_maxSmokeCnt;
    if (!CountRange(attr.m_minSmokeCnt, attr.m_maxSmokeCnt) ||
        maximumBranches > kExplosionParticleBranchCapacity ||
        attr.m_smokeName[0] == 0)
        return false;
    if (attr.m_maxSmokeCnt == 0)
        return true;
    if (!std::isfinite(attr.m_createSmokeRadius) ||
        attr.m_createSmokeRadius < 0.0 ||
        !FiniteRange(attr.m_minSmokeTimeLife,
                     attr.m_maxSmokeTimeLife) ||
        attr.m_minSmokeTimeLife <= 0.0 ||
        !FiniteRange(attr.m_minSmokeA, attr.m_maxSmokeA) ||
        !FiniteRange(attr.m_minSmokeB, attr.m_maxSmokeB) ||
        !FiniteRange(attr.m_minSmokeC, attr.m_maxSmokeC) ||
        !FiniteRange(attr.m_minSmokeTA, attr.m_maxSmokeTA) ||
        !FiniteRange(attr.m_minSmokeTB, attr.m_maxSmokeTB) ||
        !FiniteRange(attr.m_minSmokeTC, attr.m_maxSmokeTC) ||
        !FiniteRange(attr.m_minSmokeSpeed, attr.m_maxSmokeSpeed) ||
        attr.m_minSmokeSpeed < 0.0 ||
        !FiniteRange(attr.m_minMulSpeed, attr.m_maxMulSpeed) ||
        attr.m_minMulSpeed < 0.0 ||
        !std::isfinite(attr.m_ofsVAngle) ||
        !std::isfinite(attr.m_ofsHAngle) ||
        !std::isfinite(attr.m_ofsSpeed) ||
        !std::isfinite(attr.m_ofsSpeedMul))
        return false;
    const double opacityLife = PositiveRoot(
        attr.m_minSmokeTA, attr.m_minSmokeTB, attr.m_minSmokeTC);
    const double radiusLife = PositiveRoot(
        attr.m_minSmokeA, attr.m_minSmokeB, attr.m_minSmokeC);
    return std::isfinite(opacityLife) && opacityLife > 0.0 &&
           std::isfinite(radiusLife) && radiusLife > 0.0;
}

bool PieceNumbersReady(const AttributeExplosion &attr)
{
    const int maximumBranches = attr.m_maxRayCnt + attr.m_maxPartCnt +
        attr.m_maxPartSnCnt + attr.m_maxPieceCnt +
        attr.m_maxPieceSmokeCnt + attr.m_maxSmokeCnt;
    if (!CountRange(attr.m_minPieceCnt, attr.m_maxPieceCnt) ||
        maximumBranches > kExplosionParticleBranchCapacity ||
        attr.m_pieceName[0] == 0)
        return false;
    if (attr.m_maxPieceCnt == 0)
        return true;
    return FiniteRange(attr.m_minPieceSpeed, attr.m_maxPieceSpeed) &&
           attr.m_minPieceSpeed >= 0.0 &&
           FiniteRange(attr.m_minPieceTimeLife,
                       attr.m_maxPieceTimeLife) &&
           attr.m_minPieceTimeLife >= 0.0 &&
           FiniteRange(attr.m_minPieceOySpeed,
                       attr.m_maxPieceOySpeed) &&
           FiniteRange(attr.m_minPieceOxSpeed,
                       attr.m_maxPieceOxSpeed) &&
           std::isfinite(attr.m_createRadius) &&
           attr.m_createRadius >= 0.0;
}

bool TraceNumbersReady(const AttributeExplosion &attr)
{
    const int maximumBranches = attr.m_maxRayCnt + attr.m_maxPartCnt +
        attr.m_maxPartSnCnt + attr.m_maxPieceCnt +
        attr.m_maxPieceSmokeCnt + attr.m_maxSmokeCnt;
    return CountRange(attr.m_minPieceSmokeCnt,
                      attr.m_maxPieceSmokeCnt) &&
           maximumBranches <= kExplosionParticleBranchCapacity &&
           attr.m_traceSmokeName[0] != 0 &&
           std::isfinite(attr.m_traceNewPuffTime) &&
           attr.m_traceNewPuffTime > 0.0 &&
           attr.m_traceNewPuffTime <= 15.0 &&
           PieceNumbersReady(attr);
}

bool BuildSmokeVisualCache(const AttributeExplosion &attr,
                           SmokeVisualCache &cache)
{
    if (_dL.currDevice == NULL)
        return false;
    const int rgb[4] = {
        attr.m_sRGB0, attr.m_sRGB1, attr.m_sRGB2, attr.m_sRGB3
    };
    for (int segment = 0; segment < 3; ++segment)
        for (int index = 0; index < AttributeExplosion::COLLINE; ++index)
        {
            const int red = Red(rgb[segment]) +
                (Red(rgb[segment + 1]) - Red(rgb[segment])) * index /
                    (AttributeExplosion::COLLINE - 1);
            const int green = Green(rgb[segment]) +
                (Green(rgb[segment + 1]) - Green(rgb[segment])) * index /
                    (AttributeExplosion::COLLINE - 1);
            const int blue = Blue(rgb[segment]) +
                (Blue(rgb[segment + 1]) - Blue(rgb[segment])) * index /
                    (AttributeExplosion::COLLINE - 1);
            cache.colors[segment * AttributeExplosion::COLLINE + index] =
                GRTransparentColor((std::max)(0, (std::min)(255, red)),
                                   (std::max)(0, (std::min)(255, green)),
                                   (std::max)(0, (std::min)(255, blue)));
        }
    cache.texture = NULL;
    return true;
}

GR_HTEXTURE FindCachedSmokeTexture(const char *name)
{
    if (name == NULL || name[0] == 0)
        return NULL;
    const int count = SmokeTextureCache_Checkpoint();
    for (int i = 0; i < count; ++i)
        if (strcmpi(g_cacheSmoke[i].fname, name) == 0)
            return g_cacheSmoke[i].hand;
    return NULL;
}

bool SmokeVisualCacheMatches(const AttributeExplosion &attr)
{
    SmokeVisualCache expected = {};
    if (!BuildSmokeVisualCache(attr, expected))
        return false;
    expected.texture = FindCachedSmokeTexture(attr.m_smokeName);
    if (expected.texture == NULL || attr.m_hTexture != expected.texture)
        return false;
    for (int i = 0; i < AttributeExplosion::COLLINE * 3; ++i)
        if (attr.m_colBuf[i] != expected.colors[i])
            return false;
    return true;
}

bool SoundCacheIsCoherent(AttributeExplosion &attr)
{
    if (attr.m_soundName[0] == 0)
        return attr.m_wav == NULL && attr.m_ctsndID == ct_NULLID;
    if (attr.m_wav == NULL || attr.m_ctsndID == ct_NULLID)
        return attr.m_wav == NULL && attr.m_ctsndID == ct_NULLID;
    return WAVResourceState_IsLoadedPointer(attr.m_wav) &&
           attr.m_ctsndID ==
               g_arena.searchSeanceClassTable("SoundObj");
}

bool CachesAreCoherent(AttributeExplosion &attr)
{
    if (!DeferredReferencesAreUnresolved(attr) ||
        !SoundCacheIsCoherent(attr) ||
        !PieceCacheIsCoherent(g_arena.getContext(), attr) ||
        !TraceCacheIsCoherent(g_arena.getContext(), attr))
        return false;
    if (g_smokeVisualState.ready)
    {
        if (!SmokeNumbersReady(attr) || !SmokeVisualCacheMatches(attr))
            return false;
    }
    else if (!SmokeVisualCacheIsZero(attr))
        return false;
    if (!g_particleVisualState.ready)
        return ParticleCacheIsZero(attr);
    return ParticleCacheMatches(attr, BuildParticleVisualCache(attr));
}

struct RosterEntry
{
    std::string name;
    AttributeExplosion *attribute;
};

struct RosterCollector
{
    SimulationContext *context;
    std::vector<RosterEntry> entries;
    bool valid;
};

bool CollectRosterEntry(const KR_ObjectID object, void *user)
{
    RosterCollector *collector = static_cast<RosterCollector *>(user);
    const char *name = collector->context->searchObject(object);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (name == NULL || attribute == NULL || !CachesAreCoherent(*attribute))
    {
        collector->valid = false;
        return false;
    }
    RosterEntry entry;
    entry.name = name;
    entry.attribute = attribute;
    collector->entries.push_back(entry);
    return true;
}

bool RosterEntryLess(const RosterEntry &left, const RosterEntry &right)
{
    return left.name < right.name;
}

bool CollectRoster(SimulationContext *context, RosterCollector &collector)
{
    if (context == NULL)
        return false;
    collector.context = context;
    collector.valid = true;
    __attrExplosionTable.userFind(CollectRosterEntry, &collector);
    if (!collector.valid || collector.entries.empty())
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              RosterEntryLess);
    return true;
}

bool SoundReferencesMatch(SimulationContext *context,
                          const RosterCollector &collector)
{
    if (context == NULL)
        return false;
    const ct_ClassTableID soundTable =
        g_arena.searchSeanceClassTable("SoundObj");
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (attribute == NULL)
            return false;
        if (attribute->m_soundName[0] == 0)
        {
            if (attribute->m_wav != NULL ||
                attribute->m_ctsndID != ct_NULLID)
                return false;
            continue;
        }
        WAVObj *expected = NULL;
        if (soundTable == ct_NULLID ||
            !WAVResourceState_ResolveLoaded(
                context, attribute->m_soundName, &expected) ||
            attribute->m_wav != expected ||
            attribute->m_ctsndID != soundTable)
            return false;
    }
    return true;
}

}  // namespace

AttributeTableExplosion __attrExplosionTable;

AttributeExplosion::AttributeExplosion()
{
    InitializeLightBrightness(*this);
    m_color0 = m_color1 = m_color2 = m_color3 = 0;
    std::memset(m_colBuf, 0, sizeof(m_colBuf));
    m_hTexture = NULL;
    m_colorSnTail = m_colorSnHead = m_colorSnCenter = 0;
    m_cacheSkin = NULL;
    m_wav = NULL;
    m_ctsndID = ct_NULLID;
    m_rayColor = 0;
    m_smokeTableID = ct_NULLID;
    m_smokeAttrID = KR_ObjectID::NUL();

    m_moveTimeInc = 0.03;
    m_minPartSize = 0.2;
    m_maxPartSize = 0.8;
    m_minPartSnSize = 0.5;
    m_maxPartSnSize = 0.6;
    m_minPartCnt = 10;
    m_maxPartCnt = 20;
    m_minPartSnCnt = 8;
    m_maxPartSnCnt = 12;
    m_minPieceCnt = 5;
    m_maxPieceCnt = 8;
    m_minPieceSmokeCnt = 3;
    m_maxPieceSmokeCnt = 6;
    m_minSmokeCnt = 3;
    m_maxSmokeCnt = 6;
    m_RGB0 = m_RGB1 = m_RGB2 = m_RGB3 = 0;
    m_radius = 5;
    m_createRadius = 2;
    m_createSmokeRadius = 0.5;
    m_minPartSpeed = 3.0;
    m_maxPartSpeed = 10.0;
    m_minPieceSpeed = 3;
    m_maxPieceSpeed = 10;
    m_minPartTimeLife = 0.5;
    m_maxPartTimeLife = 1.0;
    m_minPartSnTimeLife = 1.0;
    m_maxPartSnTimeLife = 1;
    m_minPieceTimeLife = 1.0;
    m_maxPieceTimeLife = 1.0;
    m_minPieceSmTimeLife = 1;
    m_maxPieceSmTimeLife = 1;
    m_minSmokeTimeLife = 1;
    m_maxSmokeTimeLife = 1;
    m_sRGB0 = m_sRGB1 = m_sRGB2 = m_sRGB3 = 0;
    m_minSmokeA = -1;
    m_maxSmokeA = -1;
    m_minSmokeB = 10;
    m_maxSmokeB = 12;
    m_minSmokeC = 0.5;
    m_maxSmokeC = 0.8;
    m_minSmokeTA = 0;
    m_maxSmokeTA = 0;
    m_minSmokeTB = 10;
    m_maxSmokeTB = 12;
    m_minSmokeTC = 200;
    m_maxSmokeTC = 255;
    m_minSmokeSpeed = 0;
    m_maxSmokeSpeed = 10;
    m_minMulSpeed = 1;
    m_maxMulSpeed = 1;
    std::strncpy(m_smokeName, "smoke.spr", sizeof(ct_AttrStr) - 1);
    m_smokeName[sizeof(ct_AttrStr) - 1] = 0;
    m_ofsVAngle = 0;
    m_ofsHAngle = 0;
    m_ofsSpeed = 4;
    m_snRGBtail = 0;
    m_snRGBhead = 0;
    m_snRGBcenter = 0xFFFFFF;
    m_snDeltaT = 0.1;
    m_snPartCnt = 8;
    std::strncpy(m_pieceName, "Expl.Piece", sizeof(ct_AttrStr) - 1);
    m_pieceName[sizeof(ct_AttrStr) - 1] = 0;
    m_minPieceOySpeed = 2.0;
    m_maxPieceOySpeed = 2.5;
    m_minPieceOxSpeed = 0.5;
    m_maxPieceOxSpeed = 1;
    m_lightOffset = 5;
    m_lightRadius = 15;
    m_lightColor = 7;
    m_lightTimeLife = 1;
    m_radiusDamage = 5;
    m_power = 0.6;
    m_soundName[0] = 0;
    m_useRay = 0;
    m_minRayCnt = 3;
    m_maxRayCnt = 10;
    m_minRayLen = 15;
    m_maxRayLen = 45;
    m_minRayWidth = 0.1;
    m_maxRayWidth = 0.3;
    m_rayRGB = 0xFFFFFF;
    m_traceNewPuffTime = 0.2;
    m_ofsSpeedMul = 1;
    std::strncpy(m_traceSmokeName, "Smoke.Attr.Trace",
                 sizeof(ct_AttrStr) - 1);
    m_traceSmokeName[sizeof(ct_AttrStr) - 1] = 0;
    m_useLight = 1;
    m_impulseCoeff = 10000;

#define RR2NW_EXPLOSION_LINK(index, field) m_array[index].set(#field, field)
    RR2NW_EXPLOSION_LINK(0, m_moveTimeInc);
    RR2NW_EXPLOSION_LINK(1, m_minPartSize);
    RR2NW_EXPLOSION_LINK(2, m_maxPartSize);
    RR2NW_EXPLOSION_LINK(3, m_minPartSnSize);
    RR2NW_EXPLOSION_LINK(4, m_maxPartSnSize);
    RR2NW_EXPLOSION_LINK(5, m_minPartCnt);
    RR2NW_EXPLOSION_LINK(6, m_maxPartCnt);
    RR2NW_EXPLOSION_LINK(7, m_minPartSnCnt);
    RR2NW_EXPLOSION_LINK(8, m_maxPartSnCnt);
    RR2NW_EXPLOSION_LINK(9, m_minPieceCnt);
    RR2NW_EXPLOSION_LINK(10, m_maxPieceCnt);
    RR2NW_EXPLOSION_LINK(11, m_minPieceSmokeCnt);
    RR2NW_EXPLOSION_LINK(12, m_maxPieceSmokeCnt);
    RR2NW_EXPLOSION_LINK(13, m_minSmokeCnt);
    RR2NW_EXPLOSION_LINK(14, m_maxSmokeCnt);
    RR2NW_EXPLOSION_LINK(15, m_RGB0);
    RR2NW_EXPLOSION_LINK(16, m_RGB1);
    RR2NW_EXPLOSION_LINK(17, m_RGB2);
    RR2NW_EXPLOSION_LINK(18, m_RGB3);
    RR2NW_EXPLOSION_LINK(19, m_radius);
    RR2NW_EXPLOSION_LINK(20, m_createRadius);
    RR2NW_EXPLOSION_LINK(21, m_createSmokeRadius);
    RR2NW_EXPLOSION_LINK(22, m_minPartSpeed);
    RR2NW_EXPLOSION_LINK(23, m_maxPartSpeed);
    RR2NW_EXPLOSION_LINK(24, m_minPieceSpeed);
    RR2NW_EXPLOSION_LINK(25, m_maxPieceSpeed);
    RR2NW_EXPLOSION_LINK(26, m_minPartTimeLife);
    RR2NW_EXPLOSION_LINK(27, m_maxPartTimeLife);
    RR2NW_EXPLOSION_LINK(28, m_minPartSnTimeLife);
    RR2NW_EXPLOSION_LINK(29, m_maxPartSnTimeLife);
    RR2NW_EXPLOSION_LINK(30, m_minPieceTimeLife);
    RR2NW_EXPLOSION_LINK(31, m_maxPieceTimeLife);
    RR2NW_EXPLOSION_LINK(32, m_minPieceSmTimeLife);
    RR2NW_EXPLOSION_LINK(33, m_maxPieceSmTimeLife);
    RR2NW_EXPLOSION_LINK(34, m_minSmokeTimeLife);
    RR2NW_EXPLOSION_LINK(35, m_maxSmokeTimeLife);
    RR2NW_EXPLOSION_LINK(36, m_sRGB0);
    RR2NW_EXPLOSION_LINK(37, m_sRGB1);
    RR2NW_EXPLOSION_LINK(38, m_sRGB2);
    RR2NW_EXPLOSION_LINK(39, m_sRGB3);
    RR2NW_EXPLOSION_LINK(40, m_minSmokeA);
    RR2NW_EXPLOSION_LINK(41, m_maxSmokeA);
    RR2NW_EXPLOSION_LINK(42, m_minSmokeB);
    RR2NW_EXPLOSION_LINK(43, m_maxSmokeB);
    RR2NW_EXPLOSION_LINK(44, m_minSmokeC);
    RR2NW_EXPLOSION_LINK(45, m_maxSmokeC);
    RR2NW_EXPLOSION_LINK(46, m_minSmokeTA);
    RR2NW_EXPLOSION_LINK(47, m_maxSmokeTA);
    RR2NW_EXPLOSION_LINK(48, m_minSmokeTB);
    RR2NW_EXPLOSION_LINK(49, m_maxSmokeTB);
    RR2NW_EXPLOSION_LINK(50, m_minSmokeTC);
    RR2NW_EXPLOSION_LINK(51, m_maxSmokeTC);
    RR2NW_EXPLOSION_LINK(52, m_minSmokeSpeed);
    RR2NW_EXPLOSION_LINK(53, m_maxSmokeSpeed);
    RR2NW_EXPLOSION_LINK(54, m_minMulSpeed);
    RR2NW_EXPLOSION_LINK(55, m_maxMulSpeed);
    RR2NW_EXPLOSION_LINK(56, m_smokeName);
    RR2NW_EXPLOSION_LINK(57, m_ofsVAngle);
    RR2NW_EXPLOSION_LINK(58, m_ofsHAngle);
    RR2NW_EXPLOSION_LINK(59, m_ofsSpeed);
    RR2NW_EXPLOSION_LINK(60, m_snRGBtail);
    RR2NW_EXPLOSION_LINK(61, m_snRGBhead);
    RR2NW_EXPLOSION_LINK(62, m_snRGBcenter);
    RR2NW_EXPLOSION_LINK(63, m_snDeltaT);
    RR2NW_EXPLOSION_LINK(64, m_snPartCnt);
    RR2NW_EXPLOSION_LINK(65, m_pieceName);
    RR2NW_EXPLOSION_LINK(66, m_minPieceOySpeed);
    RR2NW_EXPLOSION_LINK(67, m_maxPieceOySpeed);
    RR2NW_EXPLOSION_LINK(68, m_minPieceOxSpeed);
    RR2NW_EXPLOSION_LINK(69, m_maxPieceOxSpeed);
    RR2NW_EXPLOSION_LINK(70, m_lightOffset);
    RR2NW_EXPLOSION_LINK(71, m_lightRadius);
    RR2NW_EXPLOSION_LINK(72, m_lightColor);
    RR2NW_EXPLOSION_LINK(73, m_lightTimeLife);
    RR2NW_EXPLOSION_LINK(74, m_radiusDamage);
    RR2NW_EXPLOSION_LINK(75, m_power);
    RR2NW_EXPLOSION_LINK(76, m_soundName);
    RR2NW_EXPLOSION_LINK(77, m_useRay);
    RR2NW_EXPLOSION_LINK(78, m_minRayCnt);
    RR2NW_EXPLOSION_LINK(79, m_maxRayCnt);
    RR2NW_EXPLOSION_LINK(80, m_minRayLen);
    RR2NW_EXPLOSION_LINK(81, m_maxRayLen);
    RR2NW_EXPLOSION_LINK(82, m_minRayWidth);
    RR2NW_EXPLOSION_LINK(83, m_maxRayWidth);
    RR2NW_EXPLOSION_LINK(84, m_rayRGB);
    RR2NW_EXPLOSION_LINK(85, m_traceNewPuffTime);
    RR2NW_EXPLOSION_LINK(86, m_ofsSpeedMul);
    RR2NW_EXPLOSION_LINK(87, m_traceSmokeName);
    RR2NW_EXPLOSION_LINK(88, m_useLight);
    RR2NW_EXPLOSION_LINK(89, m_impulseCoeff);
#undef RR2NW_EXPLOSION_LINK
    linkTable(m_array, 90);
}

AttributeTableExplosion::AttributeTableExplosion() : m_table(NULL)
{
    registerClass("ExplosionAttr");
}

void AttributeTableExplosion::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeExplosion[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableExplosion::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableExplosion::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableExplosion::getObjectPTR");
    return &(m_table[index]);
}

void ExplosionAttributeState_Link()
{
    ExplosionSubjectState_Link();
}

unsigned long long ExplosionAttributeState_Fingerprint(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return 0;
    unsigned long long hash = kHashOffset;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        HashString(hash, collector.entries[i].name.c_str());
        HashAttribute(hash, *collector.entries[i].attribute);
    }
    return hash;
}

int ExplosionAttributeState_RosterSize(SimulationContext *context)
{
    RosterCollector collector = {};
    return CollectRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : 0;
}

bool ExplosionAttributeState_ResolveEncodedIndex(
    SimulationContext *context, int encodedIndex,
    AttributeExplosion **attribute)
{
    if (attribute == NULL)
        return false;
    *attribute = NULL;
    if (context == NULL || g_arena.getContext() != context ||
        encodedIndex == -1)
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    if (table == ct_NULLID)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *candidate = collector.entries[index].attribute;
        if (candidate != NULL &&
            g_arena.getAttributeIndex(table, candidate->getObjectID()) ==
                encodedIndex)
        {
            *attribute = candidate;
            return true;
        }
    }
    return false;
}

const char *ExplosionAttributeState_FirstAttributeName(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) || collector.entries.empty())
        return NULL;
    return context->searchObject(
        collector.entries.front().attribute->getObjectID());
}

bool ExplosionAttributeState_SoundCachesUnresolved(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        const AttributeExplosion *attribute =
            collector.entries[index].attribute;
        if (attribute == NULL || attribute->m_wav != NULL ||
            attribute->m_ctsndID != ct_NULLID)
            return false;
    }
    return true;
}

bool ExplosionAttributeState_ProbeSoundReferenceAtomicity(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) ||
        !ExplosionAttributeState_SoundCachesUnresolved(context))
        return false;
    AttributeExplosion *probe = NULL;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        if (collector.entries[index].attribute->m_soundName[0] != 0)
        {
            probe = collector.entries[index].attribute;
            break;
        }
    if (probe == NULL)
        return false;

    const unsigned long long before =
        ExplosionAttributeState_Fingerprint(context);
    ct_AttrStr saved = {};
    std::memcpy(saved, probe->m_soundName, sizeof(saved));
    static const char missing[] =
        "wav.Explosion.Missing.Reference.Probe";
    std::strncpy(probe->m_soundName, missing, sizeof(ct_AttrStr) - 1);
    probe->m_soundName[sizeof(ct_AttrStr) - 1] = 0;
    const bool rejected =
        !ExplosionAttributeState_ResolveSoundReferences(context) &&
        ExplosionAttributeState_SoundCachesUnresolved(context);
    std::memcpy(probe->m_soundName, saved, sizeof(saved));
    return rejected && before != 0 &&
           ExplosionAttributeState_Fingerprint(context) == before &&
           ExplosionAttributeState_SoundCachesUnresolved(context);
}

bool ExplosionAttributeState_ResolveSoundReferences(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    if (ExplosionAttributeState_SoundReferencesResolved(context))
        return true;
    if (!ExplosionAttributeState_SoundCachesUnresolved(context))
        return false;

    const ct_ClassTableID soundTable =
        g_arena.searchSeanceClassTable("SoundObj");
    if (soundTable == ct_NULLID)
        return false;
    std::vector<WAVObj *> resolved(collector.entries.size(), NULL);
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (attribute->m_soundName[0] != 0 &&
            !WAVResourceState_ResolveLoaded(
                context, attribute->m_soundName, &resolved[index]))
            return false;
    }
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (resolved[index] != NULL)
        {
            attribute->m_wav = resolved[index];
            attribute->m_ctsndID = soundTable;
        }
    }
    return ExplosionAttributeState_SoundReferencesResolved(context);
}

bool ExplosionAttributeState_SoundReferencesResolved(
    SimulationContext *context)
{
    RosterCollector collector = {};
    return CollectRoster(context, collector) &&
           SoundReferencesMatch(context, collector);
}

unsigned long long ExplosionAttributeState_SoundReferenceFingerprint(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) ||
        !SoundReferencesMatch(context, collector))
        return 0;
    unsigned long long hash = kHashOffset;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        const AttributeExplosion *attribute =
            collector.entries[index].attribute;
        const int hasSound = attribute->m_wav != NULL ? 1 : 0;
        HashString(hash, collector.entries[index].name.c_str());
        HashString(hash, attribute->m_soundName);
        HashBytes(hash, &hasSound, sizeof(hasSound));
        HashString(hash, hasSound != 0 ? "SoundObj" : "");
    }
    return hash;
}

bool ExplosionAttributeState_IsKnownSoundReferenceRoster(
    SimulationContext *context)
{
    // Eight unique fingerprints cover all nine May 1999 retail Levels;
    // Level.02D and Level.02N intentionally share one Explosion roster. The
    // final value is the public synthetic CI fixture.
    static const unsigned long long known[] = {
        7051910668383799273ull,
        15092396144486889759ull,
        17401771998902862174ull,
        14238615547237436625ull,
        2531665149624624247ull,
        5871769213204022095ull,
        1730424902088667495ull,
        642793402395061051ull,
        269906130094892008ull
    };
    const unsigned long long fingerprint =
        ExplosionAttributeState_SoundReferenceFingerprint(context);
    for (int index = 0;
         index < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++index)
        if (fingerprint == known[index])
            return true;
    return false;
}

bool ExplosionAttributeState_PieceCachesUnresolved(
    SimulationContext *context)
{
    if (g_pieceReferenceState.ready)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        if (collector.entries[index].attribute == NULL ||
            collector.entries[index].attribute->m_cacheSkin != NULL)
            return false;
    return true;
}

bool ExplosionAttributeState_ProbePieceReferenceAtomicity(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) || collector.entries.empty() ||
        !ExplosionAttributeState_PieceCachesUnresolved(context))
        return false;
    AttributeExplosion *probe = collector.entries.front().attribute;
    if (probe == NULL)
        return false;
    const unsigned long long before =
        ExplosionAttributeState_Fingerprint(context);
    ct_AttrStr saved = {};
    std::memcpy(saved, probe->m_pieceName, sizeof(saved));
    std::strncpy(probe->m_pieceName,
                 "Explosion.Missing.Piece.Reference.Probe",
                 sizeof(ct_AttrStr) - 1);
    probe->m_pieceName[sizeof(ct_AttrStr) - 1] = 0;
    const bool rejected =
        !ExplosionAttributeState_ResolvePieceReferences(context) &&
        ExplosionAttributeState_PieceCachesUnresolved(context);
    std::memcpy(probe->m_pieceName, saved, sizeof(saved));
    return rejected && before != 0 &&
           ExplosionAttributeState_Fingerprint(context) == before &&
           ExplosionAttributeState_PieceCachesUnresolved(context);
}

bool ExplosionAttributeState_ResolvePieceReferences(
    SimulationContext *context)
{
    if (g_pieceReferenceState.ready)
        return ExplosionAttributeState_PieceReferencesResolved(context);
    RosterCollector collector = {};
    if (context == NULL || g_arena.getContext() != context ||
        !CollectRoster(context, collector) ||
        !ExplosionAttributeState_PieceCachesUnresolved(context))
        return false;

    std::vector<CViewObjectModel *> models(collector.entries.size(), NULL);
    unsigned long long hash = kHashOffset;
    const unsigned long long skinFingerprint =
        SkinResourceState_Fingerprint(context);
    if (skinFingerprint == 0)
        return false;
    HashBytes(hash, &skinFingerprint, sizeof(skinFingerprint));
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        KR_ObjectID modelID = KR_ObjectID::NUL();
        if (attribute == NULL || !PieceNumbersReady(*attribute) ||
            !SkinResourceState_ResolveLoadedModel(
                context, attribute->m_pieceName, &modelID, &models[index]) ||
            models[index] == NULL)
            return false;
        HashString(hash, collector.entries[index].name.c_str());
        HashString(hash, attribute->m_pieceName);
        HashBytes(hash, &attribute->m_minPieceCnt,
                  sizeof(attribute->m_minPieceCnt));
        HashBytes(hash, &attribute->m_maxPieceCnt,
                  sizeof(attribute->m_maxPieceCnt));
        HashBytes(hash, &attribute->m_createRadius,
                  sizeof(attribute->m_createRadius));
        HashBytes(hash, &attribute->m_minPieceSpeed,
                  sizeof(attribute->m_minPieceSpeed));
        HashBytes(hash, &attribute->m_maxPieceSpeed,
                  sizeof(attribute->m_maxPieceSpeed));
        HashBytes(hash, &attribute->m_minPieceTimeLife,
                  sizeof(attribute->m_minPieceTimeLife));
        HashBytes(hash, &attribute->m_maxPieceTimeLife,
                  sizeof(attribute->m_maxPieceTimeLife));
        HashBytes(hash, &attribute->m_minPieceOySpeed,
                  sizeof(attribute->m_minPieceOySpeed));
        HashBytes(hash, &attribute->m_maxPieceOySpeed,
                  sizeof(attribute->m_maxPieceOySpeed));
        HashBytes(hash, &attribute->m_minPieceOxSpeed,
                  sizeof(attribute->m_minPieceOxSpeed));
        HashBytes(hash, &attribute->m_maxPieceOxSpeed,
                  sizeof(attribute->m_maxPieceOxSpeed));
    }
    if (hash == 0)
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        collector.entries[index].attribute->m_cacheSkin = models[index];
    g_pieceReferenceState.context = context;
    g_pieceReferenceState.fingerprint = hash;
    g_pieceReferenceState.ready = true;
    if (ExplosionAttributeState_PieceReferencesResolved(context))
        return true;
    ExplosionAttributeState_ClearPieceReferences(context);
    return false;
}

bool ExplosionAttributeState_PieceReferencesResolved(
    SimulationContext *context)
{
    if (!g_pieceReferenceState.ready || context == NULL ||
        g_pieceReferenceState.context != context ||
        g_arena.getContext() != context)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        if (collector.entries[index].attribute == NULL ||
            !PieceNumbersReady(*collector.entries[index].attribute) ||
            !PieceCacheIsCoherent(
                context, *collector.entries[index].attribute))
            return false;
    return true;
}

unsigned long long ExplosionAttributeState_PieceReferenceFingerprint(
    SimulationContext *context)
{
    return ExplosionAttributeState_PieceReferencesResolved(context)
               ? g_pieceReferenceState.fingerprint
               : 0;
}

bool ExplosionAttributeState_IsKnownPieceReferenceRoster(
    SimulationContext *context)
{
    // All nine May Levels have distinct identities because the stable Skin
    // resource fingerprint is part of this reference boundary. The public
    // source-only fixture has no models and therefore no admitted identity.
    static const unsigned long long known[] = {
        10858579075849477158ull,
        15412155324146565245ull,
        12088847358046740838ull,
        1447488070421285330ull,
        2283975727666402247ull,
        3811121173281572650ull,
        17413076670720599451ull,
        466559467415829808ull,
        5156984387642384829ull
    };
    const unsigned long long fingerprint =
        ExplosionAttributeState_PieceReferenceFingerprint(context);
    for (int index = 0;
         index < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++index)
        if (fingerprint == known[index])
            return true;
    return false;
}

namespace {

bool ClearPieceReference(const KR_ObjectID object, void *)
{
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (attribute == NULL)
        return false;
    attribute->m_cacheSkin = NULL;
    return true;
}

}  // namespace

void ExplosionAttributeState_ClearPieceReferences(
    SimulationContext *context)
{
    if (!g_pieceReferenceState.ready ||
        g_pieceReferenceState.context != context)
        return;
    __attrExplosionTable.userFind(ClearPieceReference, NULL);
    g_pieceReferenceState = PieceReferenceRuntimeState{};
}

bool ExplosionAttributeState_TraceCachesUnresolved(
    SimulationContext *context)
{
    if (g_traceReferenceState.ready)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (attribute == NULL || attribute->m_smokeTableID != ct_NULLID ||
            !attribute->m_smokeAttrID.isNUL())
            return false;
    }
    return true;
}

bool ExplosionAttributeState_ProbeTraceReferenceAtomicity(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) || collector.entries.empty() ||
        !ExplosionAttributeState_PieceReferencesResolved(context) ||
        !ExplosionAttributeState_TraceCachesUnresolved(context))
        return false;
    AttributeExplosion *probe = collector.entries.front().attribute;
    if (probe == NULL)
        return false;
    const unsigned long long before =
        ExplosionAttributeState_Fingerprint(context);
    ct_AttrStr saved = {};
    std::memcpy(saved, probe->m_traceSmokeName, sizeof(saved));
    std::strncpy(probe->m_traceSmokeName,
                 "Explosion.Missing.Trace.Smoke.Reference.Probe",
                 sizeof(ct_AttrStr) - 1);
    probe->m_traceSmokeName[sizeof(ct_AttrStr) - 1] = 0;
    const bool rejected =
        !ExplosionAttributeState_ResolveTraceReferences(context) &&
        ExplosionAttributeState_TraceCachesUnresolved(context);
    std::memcpy(probe->m_traceSmokeName, saved, sizeof(saved));
    return rejected && before != 0 &&
           ExplosionAttributeState_Fingerprint(context) == before &&
           ExplosionAttributeState_TraceCachesUnresolved(context);
}

bool ExplosionAttributeState_ResolveTraceReferences(
    SimulationContext *context)
{
    if (g_traceReferenceState.ready)
        return ExplosionAttributeState_TraceReferencesResolved(context);
    RosterCollector collector = {};
    if (context == NULL || g_arena.getContext() != context ||
        !CollectRoster(context, collector) ||
        !ExplosionAttributeState_PieceReferencesResolved(context) ||
        !ExplosionAttributeState_TraceCachesUnresolved(context))
        return false;

    const ct_ClassTableID smokeTable =
        g_arena.searchSeanceClassTable("Smoke");
    const unsigned long long smokeFingerprint =
        SmokeAttributeState_RetailFingerprint(context);
    const unsigned long long pieceFingerprint =
        ExplosionAttributeState_PieceReferenceFingerprint(context);
    if (smokeTable == ct_NULLID || smokeFingerprint == 0 ||
        pieceFingerprint == 0)
        return false;
    std::vector<KR_ObjectID> smokeAttributes(
        collector.entries.size(), KR_ObjectID::NUL());
    unsigned long long hash = kHashOffset;
    HashBytes(hash, &smokeFingerprint, sizeof(smokeFingerprint));
    HashBytes(hash, &pieceFingerprint, sizeof(pieceFingerprint));
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (attribute == NULL || !TraceNumbersReady(*attribute) ||
            !SmokeAttributeState_Resolve(
                context, attribute->m_traceSmokeName,
                &smokeAttributes[index]) ||
            !SmokeSubjectState_SimulationSupported(
                context, attribute->m_traceSmokeName))
            return false;
        HashString(hash, collector.entries[index].name.c_str());
        HashString(hash, attribute->m_traceSmokeName);
        HashBytes(hash, &attribute->m_minPieceSmokeCnt,
                  sizeof(attribute->m_minPieceSmokeCnt));
        HashBytes(hash, &attribute->m_maxPieceSmokeCnt,
                  sizeof(attribute->m_maxPieceSmokeCnt));
        HashBytes(hash, &attribute->m_traceNewPuffTime,
                  sizeof(attribute->m_traceNewPuffTime));
    }
    if (hash == 0)
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        collector.entries[index].attribute->m_smokeTableID = smokeTable;
        collector.entries[index].attribute->m_smokeAttrID =
            smokeAttributes[index];
    }
    g_traceReferenceState.context = context;
    g_traceReferenceState.fingerprint = hash;
    g_traceReferenceState.ready = true;
    if (ExplosionAttributeState_TraceReferencesResolved(context))
        return true;
    ExplosionAttributeState_ClearTraceReferences(context);
    return false;
}

bool ExplosionAttributeState_TraceReferencesResolved(
    SimulationContext *context)
{
    if (!g_traceReferenceState.ready || context == NULL ||
        g_traceReferenceState.context != context ||
        g_arena.getContext() != context ||
        !ExplosionAttributeState_PieceReferencesResolved(context))
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (attribute == NULL || !TraceNumbersReady(*attribute) ||
            !TraceCacheIsCoherent(context, *attribute))
            return false;
    }
    return true;
}

unsigned long long ExplosionAttributeState_TraceReferenceFingerprint(
    SimulationContext *context)
{
    return ExplosionAttributeState_TraceReferencesResolved(context)
               ? g_traceReferenceState.fingerprint
               : 0;
}

bool ExplosionAttributeState_IsKnownTraceReferenceRoster(
    SimulationContext *context)
{
    switch (ExplosionAttributeState_TraceReferenceFingerprint(context))
    {
    case 15479875903557427417ull:  // Level.01D
    case 14176899255950351083ull:  // Level.01N
    case 8549830335675231037ull:   // Level.02D
    case 7224868523921463240ull:   // Level.02N
    case 2178156965531948188ull:   // Level.03N
    case 18052888668054315656ull:  // Level.04D
    case 1363236821580030428ull:   // Level.05D
    case 410141187708350623ull:    // Level.06N
    case 4161868981050679744ull:   // Level.07N
        return true;
    default:
        return false;
    }
}

namespace {

bool ClearTraceReference(const KR_ObjectID object, void *)
{
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (attribute == NULL)
        return false;
    attribute->m_smokeTableID = ct_NULLID;
    attribute->m_smokeAttrID = KR_ObjectID::NUL();
    return true;
}

}  // namespace

void ExplosionAttributeState_ClearTraceReferences(
    SimulationContext *context)
{
    if (!g_traceReferenceState.ready ||
        g_traceReferenceState.context != context)
        return;
    __attrExplosionTable.userFind(ClearTraceReference, NULL);
    g_traceReferenceState = TraceReferenceRuntimeState{};
}

bool ExplosionAttributeState_ParticleCachesUnresolved(
    SimulationContext *context)
{
    if (g_particleVisualState.ready)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        if (!ParticleCacheIsZero(*collector.entries[index].attribute))
            return false;
    return true;
}

bool ExplosionAttributeState_ProbeParticleVisualAtomicity(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) ||
        !ExplosionAttributeState_ParticleCachesUnresolved(context))
        return false;
    AttributeExplosion *probe = collector.entries.front().attribute;
    if (probe == NULL)
        return false;
    const unsigned long long before =
        ExplosionAttributeState_Fingerprint(context);
    const int savedMaximum = probe->m_maxPartCnt;
    probe->m_maxPartCnt = -1;
    const bool rejected =
        !ExplosionAttributeState_ResolveParticleVisuals(context) &&
        ExplosionAttributeState_ParticleCachesUnresolved(context);
    probe->m_maxPartCnt = savedMaximum;
    return rejected && before != 0 &&
           ExplosionAttributeState_Fingerprint(context) == before &&
           ExplosionAttributeState_ParticleCachesUnresolved(context);
}

bool ExplosionAttributeState_ResolveParticleVisuals(
    SimulationContext *context)
{
    if (g_particleVisualState.ready)
        return ExplosionAttributeState_ParticleVisualsResolved(context);
    RosterCollector collector = {};
    if (context == NULL || g_arena.getContext() != context ||
        _pGRDrawParticle == NULL ||
        !CollectRoster(context, collector) ||
        !ExplosionAttributeState_ParticleCachesUnresolved(context))
        return false;

    std::vector<ParticleVisualCache> caches(collector.entries.size());
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (attribute == NULL || !ParticleNumbersReady(*attribute))
            return false;
        caches[index] = BuildParticleVisualCache(*attribute);
    }

    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        attribute->m_color0 = caches[index].color[0];
        attribute->m_color1 = caches[index].color[1];
        attribute->m_color2 = caches[index].color[2];
        attribute->m_color3 = caches[index].color[3];
        attribute->m_colorSnTail = caches[index].snakeTail;
        attribute->m_colorSnHead = caches[index].snakeHead;
        attribute->m_colorSnCenter = caches[index].snakeCenter;
        attribute->m_rayColor = caches[index].ray;
    }

    unsigned long long hash = kHashOffset;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        const AttributeExplosion *attribute =
            collector.entries[index].attribute;
        HashString(hash, collector.entries[index].name.c_str());
        HashBytes(hash, &attribute->m_RGB0, sizeof(attribute->m_RGB0));
        HashBytes(hash, &attribute->m_RGB1, sizeof(attribute->m_RGB1));
        HashBytes(hash, &attribute->m_RGB2, sizeof(attribute->m_RGB2));
        HashBytes(hash, &attribute->m_RGB3, sizeof(attribute->m_RGB3));
        HashBytes(hash, &attribute->m_snRGBtail,
                  sizeof(attribute->m_snRGBtail));
        HashBytes(hash, &attribute->m_snRGBhead,
                  sizeof(attribute->m_snRGBhead));
        HashBytes(hash, &attribute->m_snRGBcenter,
                  sizeof(attribute->m_snRGBcenter));
        HashBytes(hash, &attribute->m_rayRGB,
                  sizeof(attribute->m_rayRGB));
        HashBytes(hash, &attribute->m_moveTimeInc,
                  sizeof(attribute->m_moveTimeInc));
        HashBytes(hash, &attribute->m_minPartSize,
                  sizeof(attribute->m_minPartSize));
        HashBytes(hash, &attribute->m_maxPartSize,
                  sizeof(attribute->m_maxPartSize));
        HashBytes(hash, &attribute->m_minPartSnSize,
                  sizeof(attribute->m_minPartSnSize));
        HashBytes(hash, &attribute->m_maxPartSnSize,
                  sizeof(attribute->m_maxPartSnSize));
        HashBytes(hash, &attribute->m_minPartCnt,
                  sizeof(attribute->m_minPartCnt));
        HashBytes(hash, &attribute->m_maxPartCnt,
                  sizeof(attribute->m_maxPartCnt));
        HashBytes(hash, &attribute->m_minPartSnCnt,
                  sizeof(attribute->m_minPartSnCnt));
        HashBytes(hash, &attribute->m_maxPartSnCnt,
                  sizeof(attribute->m_maxPartSnCnt));
        HashBytes(hash, &attribute->m_minRayCnt,
                  sizeof(attribute->m_minRayCnt));
        HashBytes(hash, &attribute->m_maxRayCnt,
                  sizeof(attribute->m_maxRayCnt));
        HashBytes(hash, &attribute->m_radius,
                  sizeof(attribute->m_radius));
        HashBytes(hash, &attribute->m_createRadius,
                  sizeof(attribute->m_createRadius));
        HashBytes(hash, &attribute->m_minPartSpeed,
                  sizeof(attribute->m_minPartSpeed));
        HashBytes(hash, &attribute->m_maxPartSpeed,
                  sizeof(attribute->m_maxPartSpeed));
        HashBytes(hash, &attribute->m_minPartTimeLife,
                  sizeof(attribute->m_minPartTimeLife));
        HashBytes(hash, &attribute->m_maxPartTimeLife,
                  sizeof(attribute->m_maxPartTimeLife));
        HashBytes(hash, &attribute->m_minPartSnTimeLife,
                  sizeof(attribute->m_minPartSnTimeLife));
        HashBytes(hash, &attribute->m_maxPartSnTimeLife,
                  sizeof(attribute->m_maxPartSnTimeLife));
        HashBytes(hash, &attribute->m_snDeltaT,
                  sizeof(attribute->m_snDeltaT));
        HashBytes(hash, &attribute->m_snPartCnt,
                  sizeof(attribute->m_snPartCnt));
        HashBytes(hash, &attribute->m_useRay,
                  sizeof(attribute->m_useRay));
        HashBytes(hash, &attribute->m_minRayLen,
                  sizeof(attribute->m_minRayLen));
        HashBytes(hash, &attribute->m_maxRayLen,
                  sizeof(attribute->m_maxRayLen));
        HashBytes(hash, &attribute->m_minRayWidth,
                  sizeof(attribute->m_minRayWidth));
        HashBytes(hash, &attribute->m_maxRayWidth,
                  sizeof(attribute->m_maxRayWidth));
    }
    if (hash == 0)
        return false;
    g_particleVisualState.context = context;
    g_particleVisualState.fingerprint = hash;
    g_particleVisualState.ready = true;
    if (ExplosionAttributeState_ParticleVisualsResolved(context))
        return true;
    ExplosionAttributeState_ClearParticleVisuals(context);
    return false;
}

bool ExplosionAttributeState_ParticleVisualsResolved(
    SimulationContext *context)
{
    if (!g_particleVisualState.ready || context == NULL ||
        g_particleVisualState.context != context ||
        g_arena.getContext() != context || _pGRDrawParticle == NULL)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        if (attribute == NULL || !ParticleNumbersReady(*attribute) ||
            !ParticleCacheMatches(
                *attribute, BuildParticleVisualCache(*attribute)))
            return false;
    }
    return true;
}

unsigned long long ExplosionAttributeState_ParticleVisualFingerprint(
    SimulationContext *context)
{
    return ExplosionAttributeState_ParticleVisualsResolved(context)
               ? g_particleVisualState.fingerprint
               : 0;
}

bool ExplosionAttributeState_IsKnownParticleVisualRoster(
    SimulationContext *context)
{
    // Eight unique values cover nine May Levels; Level.02D and Level.02N
    // intentionally share a roster. The final value is the public fixture.
    static const unsigned long long known[] = {
        15405245879790332505ull,
        6832843287917630389ull,
        10902720985337932439ull,
        10669768949891345271ull,
        3307987323279664573ull,
        12112644173111710481ull,
        1177190502280645556ull,
        17815537380847575576ull,
        8630148845058022144ull
    };
    const unsigned long long fingerprint =
        ExplosionAttributeState_ParticleVisualFingerprint(context);
    for (int index = 0;
         index < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++index)
        if (fingerprint == known[index])
            return true;
    return false;
}

namespace {

bool ClearParticleVisualCache(const KR_ObjectID object, void *)
{
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (attribute == NULL)
        return false;
    attribute->m_color0 = 0;
    attribute->m_color1 = 0;
    attribute->m_color2 = 0;
    attribute->m_color3 = 0;
    attribute->m_colorSnTail = 0;
    attribute->m_colorSnHead = 0;
    attribute->m_colorSnCenter = 0;
    attribute->m_rayColor = 0;
    return true;
}

}  // namespace

void ExplosionAttributeState_ClearParticleVisuals(
    SimulationContext *context)
{
    if (!g_particleVisualState.ready ||
        g_particleVisualState.context != context)
        return;
    __attrExplosionTable.userFind(ClearParticleVisualCache, NULL);
    g_particleVisualState = ParticleVisualRuntimeState{};
}

namespace {

void HashSmokeVisualSource(unsigned long long &hash,
                           const RosterEntry &entry)
{
    const AttributeExplosion &attribute = *entry.attribute;
    HashString(hash, entry.name.c_str());
    HashString(hash, attribute.m_smokeName);
    HashBytes(hash, &attribute.m_minSmokeCnt,
              sizeof(attribute.m_minSmokeCnt));
    HashBytes(hash, &attribute.m_maxSmokeCnt,
              sizeof(attribute.m_maxSmokeCnt));
    HashBytes(hash, &attribute.m_createSmokeRadius,
              sizeof(attribute.m_createSmokeRadius));
    HashBytes(hash, &attribute.m_minSmokeTimeLife,
              sizeof(attribute.m_minSmokeTimeLife));
    HashBytes(hash, &attribute.m_maxSmokeTimeLife,
              sizeof(attribute.m_maxSmokeTimeLife));
    HashBytes(hash, &attribute.m_sRGB0, sizeof(attribute.m_sRGB0));
    HashBytes(hash, &attribute.m_sRGB1, sizeof(attribute.m_sRGB1));
    HashBytes(hash, &attribute.m_sRGB2, sizeof(attribute.m_sRGB2));
    HashBytes(hash, &attribute.m_sRGB3, sizeof(attribute.m_sRGB3));
    HashBytes(hash, &attribute.m_minSmokeA,
              sizeof(attribute.m_minSmokeA));
    HashBytes(hash, &attribute.m_maxSmokeA,
              sizeof(attribute.m_maxSmokeA));
    HashBytes(hash, &attribute.m_minSmokeB,
              sizeof(attribute.m_minSmokeB));
    HashBytes(hash, &attribute.m_maxSmokeB,
              sizeof(attribute.m_maxSmokeB));
    HashBytes(hash, &attribute.m_minSmokeC,
              sizeof(attribute.m_minSmokeC));
    HashBytes(hash, &attribute.m_maxSmokeC,
              sizeof(attribute.m_maxSmokeC));
    HashBytes(hash, &attribute.m_minSmokeTA,
              sizeof(attribute.m_minSmokeTA));
    HashBytes(hash, &attribute.m_maxSmokeTA,
              sizeof(attribute.m_maxSmokeTA));
    HashBytes(hash, &attribute.m_minSmokeTB,
              sizeof(attribute.m_minSmokeTB));
    HashBytes(hash, &attribute.m_maxSmokeTB,
              sizeof(attribute.m_maxSmokeTB));
    HashBytes(hash, &attribute.m_minSmokeTC,
              sizeof(attribute.m_minSmokeTC));
    HashBytes(hash, &attribute.m_maxSmokeTC,
              sizeof(attribute.m_maxSmokeTC));
    HashBytes(hash, &attribute.m_minSmokeSpeed,
              sizeof(attribute.m_minSmokeSpeed));
    HashBytes(hash, &attribute.m_maxSmokeSpeed,
              sizeof(attribute.m_maxSmokeSpeed));
    HashBytes(hash, &attribute.m_minMulSpeed,
              sizeof(attribute.m_minMulSpeed));
    HashBytes(hash, &attribute.m_maxMulSpeed,
              sizeof(attribute.m_maxMulSpeed));
    HashBytes(hash, &attribute.m_ofsVAngle,
              sizeof(attribute.m_ofsVAngle));
    HashBytes(hash, &attribute.m_ofsHAngle,
              sizeof(attribute.m_ofsHAngle));
    HashBytes(hash, &attribute.m_ofsSpeed,
              sizeof(attribute.m_ofsSpeed));
    HashBytes(hash, &attribute.m_ofsSpeedMul,
              sizeof(attribute.m_ofsSpeedMul));
}

bool CollectSmokeTextureNames(const RosterCollector &collector,
                              std::vector<std::string> &names)
{
    names.clear();
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        const AttributeExplosion *attribute =
            collector.entries[index].attribute;
        if (attribute == NULL || attribute->m_smokeName[0] == 0)
            return false;
        bool known = false;
        for (std::size_t existing = 0; existing < names.size(); ++existing)
            if (strcmpi(names[existing].c_str(),
                        attribute->m_smokeName) == 0)
                known = true;
        if (!known)
            names.push_back(attribute->m_smokeName);
    }
    std::sort(names.begin(), names.end());
    return !names.empty();
}

bool SmokeTextureResourceExists(const char *name)
{
    long length = 0;
    FILE *file = CFileResource::FOpenCurrent(name, &length);
    if (file == NULL)
        return false;
    std::fclose(file);
    return true;
}

bool HashSmokeTextureResource(const char *name,
                              unsigned long long &hash)
{
    long length = 0;
    FILE *file = CFileResource::FOpenCurrent(name, &length);
    if (file == NULL)
        return false;
    unsigned char header[5] = {};
    const bool headerReady =
        std::fread(header, 1, sizeof(header), file) == sizeof(header);
    const unsigned int width = header[0] | (header[1] << 8);
    const unsigned int height = header[2] | (header[3] << 8);
    const long expectedLength = 5 + 256 * 256;
    if (!headerReady || width != 256 || height != 256 ||
        length != expectedLength)
    {
        std::fclose(file);
        return false;
    }
    HashString(hash, name);
    HashBytes(hash, header, sizeof(header));
    unsigned char buffer[4096];
    long remaining = length - static_cast<long>(sizeof(header));
    while (remaining > 0)
    {
        const int request = remaining < static_cast<long>(sizeof(buffer))
                                ? static_cast<int>(remaining)
                                : static_cast<int>(sizeof(buffer));
        const int received = static_cast<int>(
            std::fread(buffer, 1, request, file));
        if (received != request)
        {
            std::fclose(file);
            return false;
        }
        HashBytes(hash, buffer, received);
        remaining -= received;
    }
    std::fclose(file);
    return true;
}

bool ClearSmokeVisualCache(const KR_ObjectID object, void *)
{
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (attribute == NULL)
        return false;
    std::memset(attribute->m_colBuf, 0, sizeof(attribute->m_colBuf));
    attribute->m_hTexture = NULL;
    return true;
}

}  // namespace

EExplosionSmokeVisualResourcePresence
ExplosionAttributeState_InspectSmokeVisualResources(
    SimulationContext *context, unsigned long long *fingerprint)
{
    if (fingerprint != NULL)
        *fingerprint = 0;
    RosterCollector collector = {};
    std::vector<std::string> names;
    if (!CollectRoster(context, collector) ||
        !CollectSmokeTextureNames(collector, names))
        return EXPLOSION_SMOKE_VISUAL_RESOURCES_INVALID;
    int present = 0;
    for (std::size_t index = 0; index < names.size(); ++index)
        if (SmokeTextureResourceExists(names[index].c_str()))
            ++present;
    if (present == 0)
        return EXPLOSION_SMOKE_VISUAL_RESOURCES_NONE;
    if (present != static_cast<int>(names.size()))
        return EXPLOSION_SMOKE_VISUAL_RESOURCES_PARTIAL;

    unsigned long long hash = kHashOffset;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        if (!SmokeNumbersReady(*collector.entries[index].attribute))
            return EXPLOSION_SMOKE_VISUAL_RESOURCES_INVALID;
        HashSmokeVisualSource(hash, collector.entries[index]);
    }
    for (std::size_t index = 0; index < names.size(); ++index)
        if (!HashSmokeTextureResource(names[index].c_str(), hash))
            return EXPLOSION_SMOKE_VISUAL_RESOURCES_INVALID;
    if (hash == 0)
        return EXPLOSION_SMOKE_VISUAL_RESOURCES_INVALID;
    if (fingerprint != NULL)
        *fingerprint = hash;
    return EXPLOSION_SMOKE_VISUAL_RESOURCES_COMPLETE;
}

bool ExplosionAttributeState_SmokeVisualCachesUnresolved(
    SimulationContext *context)
{
    if (g_smokeVisualState.ready)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        if (!SmokeVisualCacheIsZero(*collector.entries[index].attribute))
            return false;
    return true;
}

bool ExplosionAttributeState_ProbeSmokeVisualAtomicity(
    SimulationContext *context)
{
    RosterCollector collector = {};
    unsigned long long resourceFingerprint = 0;
    if (!CollectRoster(context, collector) || collector.entries.empty() ||
        !ExplosionAttributeState_SmokeVisualCachesUnresolved(context) ||
        ExplosionAttributeState_InspectSmokeVisualResources(
            context, &resourceFingerprint) !=
                EXPLOSION_SMOKE_VISUAL_RESOURCES_COMPLETE)
        return false;
    AttributeExplosion *probe = collector.entries.front().attribute;
    if (probe == NULL)
        return false;
    ct_AttrStr saved = {};
    std::memcpy(saved, probe->m_smokeName, sizeof(saved));
    const unsigned long long before =
        ExplosionAttributeState_Fingerprint(context);
    const int textureCheckpoint = SmokeTextureCache_Checkpoint();
    std::strncpy(probe->m_smokeName,
                 "Explosion.Missing.Smoke.Visual.Probe.spr",
                 sizeof(ct_AttrStr) - 1);
    probe->m_smokeName[sizeof(ct_AttrStr) - 1] = 0;
    const bool rejected =
        !ExplosionAttributeState_ResolveSmokeVisuals(context) &&
        ExplosionAttributeState_SmokeVisualCachesUnresolved(context) &&
        SmokeTextureCache_Checkpoint() == textureCheckpoint;
    std::memcpy(probe->m_smokeName, saved, sizeof(saved));
    return rejected && before != 0 && resourceFingerprint != 0 &&
           ExplosionAttributeState_Fingerprint(context) == before &&
           ExplosionAttributeState_SmokeVisualCachesUnresolved(context);
}

bool ExplosionAttributeState_ResolveSmokeVisuals(
    SimulationContext *context)
{
    if (g_smokeVisualState.ready)
        return ExplosionAttributeState_SmokeVisualsResolved(context);
    RosterCollector collector = {};
    unsigned long long fingerprint = 0;
    if (context == NULL || g_arena.getContext() != context ||
        _pGRDrawAlphaSprite == NULL || _pGRLoadTextureToDB == NULL ||
        _pGRDeleteTextureFromDB == NULL ||
        !CollectRoster(context, collector) ||
        !ExplosionAttributeState_SmokeVisualCachesUnresolved(context) ||
        ExplosionAttributeState_InspectSmokeVisualResources(
            context, &fingerprint) !=
                EXPLOSION_SMOKE_VISUAL_RESOURCES_COMPLETE)
        return false;

    std::vector<std::string> names;
    if (!CollectSmokeTextureNames(collector, names))
        return false;
    std::vector<const char *> textureNames;
    for (std::size_t index = 0; index < names.size(); ++index)
        textureNames.push_back(names[index].c_str());
    if (!SmokeTextureCache_CanLoad(
            textureNames.data(), static_cast<int>(textureNames.size())))
        return false;

    std::vector<SmokeVisualCache> caches(collector.entries.size());
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        if (!SmokeNumbersReady(*collector.entries[index].attribute) ||
            !BuildSmokeVisualCache(
                *collector.entries[index].attribute, caches[index]))
            return false;

    const int checkpoint = SmokeTextureCache_Checkpoint();
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        caches[index].texture = g_loadSmoke(
            collector.entries[index].attribute->m_smokeName, NULL);
        if (caches[index].texture == NULL)
        {
            SmokeTextureCache_Rollback(checkpoint);
            return false;
        }
    }
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
    {
        AttributeExplosion *attribute = collector.entries[index].attribute;
        std::memcpy(attribute->m_colBuf, caches[index].colors,
                    sizeof(attribute->m_colBuf));
        attribute->m_hTexture = caches[index].texture;
    }
    g_smokeVisualState.context = context;
    g_smokeVisualState.textureCheckpoint = checkpoint;
    g_smokeVisualState.fingerprint = fingerprint;
    g_smokeVisualState.ready = true;
    if (ExplosionAttributeState_SmokeVisualsResolved(context))
        return true;
    ExplosionAttributeState_ClearSmokeVisuals(context);
    return false;
}

bool ExplosionAttributeState_SmokeVisualsResolved(
    SimulationContext *context)
{
    if (!g_smokeVisualState.ready || context == NULL ||
        g_smokeVisualState.context != context ||
        g_arena.getContext() != context || _pGRDrawAlphaSprite == NULL)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t index = 0; index < collector.entries.size(); ++index)
        if (!SmokeNumbersReady(*collector.entries[index].attribute) ||
            !SmokeVisualCacheMatches(*collector.entries[index].attribute))
            return false;
    return true;
}

unsigned long long ExplosionAttributeState_SmokeVisualFingerprint(
    SimulationContext *context)
{
    return ExplosionAttributeState_SmokeVisualsResolved(context)
               ? g_smokeVisualState.fingerprint
               : 0;
}

bool ExplosionAttributeState_IsKnownSmokeVisualRoster(
    SimulationContext *context)
{
    // Eight identities cover the nine May Levels because 02D/02N share one
    // Explosion roster. The final identity is the public synthetic fixture.
    static const unsigned long long known[] = {
        7038031820659649713ull,
        6559137887547133221ull,
        6173607222118504530ull,
        5686558409198199324ull,
        6719445918051910172ull,
        12520912501699516820ull,
        10358437977799102119ull,
        16738263764033403268ull,
        17579349666034557707ull
    };
    const unsigned long long fingerprint =
        ExplosionAttributeState_SmokeVisualFingerprint(context);
    for (std::size_t index = 0;
         index < sizeof(known) / sizeof(known[0]); ++index)
        if (fingerprint == known[index])
            return true;
    return false;
}

void ExplosionAttributeState_ClearSmokeVisuals(
    SimulationContext *context)
{
    if (!g_smokeVisualState.ready ||
        g_smokeVisualState.context != context)
        return;
    __attrExplosionTable.userFind(ClearSmokeVisualCache, NULL);
    SmokeTextureCache_Rollback(g_smokeVisualState.textureCheckpoint);
    g_smokeVisualState = SmokeVisualRuntimeState{};
}

bool ExplosionAttributeState_IsKnownRoster(SimulationContext *context)
{
    // Eight unique fingerprints cover all nine May 1999 retail Levels;
    // Level.02D and Level.02N intentionally share one Explosion roster. The
    // final value is the public synthetic CI fixture, not retail content.
    static const unsigned long long known[] = {
        10273680374400970482ull,
        3427434765057244795ull,
        8445666927216049330ull,
        17917608726709018727ull,
        3362487258264023603ull,
        17713080548385097020ull,
        2278948764680578997ull,
        13266148710419836005ull,
        2082493637237996457ull
    };
    const unsigned long long fingerprint =
        ExplosionAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}
