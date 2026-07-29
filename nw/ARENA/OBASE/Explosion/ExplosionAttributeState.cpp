#include "ExplosionAttributeState.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <string>
#include <vector>

#include "ExplosionSubjectState.h"
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

struct ParticleVisualCache
{
    unsigned long color[4];
    unsigned long snakeTail;
    unsigned long snakeHead;
    unsigned long snakeCenter;
    unsigned long ray;
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

bool DeferredCachesAreUnresolved(AttributeExplosion &attr)
{
    if (attr.m_hTexture != NULL || attr.m_cacheSkin != NULL ||
        attr.m_smokeTableID != ct_NULLID ||
        !attr.m_smokeAttrID.isNUL())
        return false;
    for (int i = 0; i < AttributeExplosion::MAX_BRIGHT; ++i)
        if (attr.m_brightness[i] != LightBrightness(i))
            return false;
    for (int i = 0; i < AttributeExplosion::COLLINE * 3; ++i)
        if (attr.m_colBuf[i] != 0)
            return false;
    return true;
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
    if (!DeferredCachesAreUnresolved(attr) ||
        !SoundCacheIsCoherent(attr))
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
