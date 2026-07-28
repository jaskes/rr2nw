#include "ExplosionAttributeState.h"

#include <algorithm>
#include <new>
#include <string>
#include <vector>

#include "ExplosionSubjectState.h"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "storage/h/subject.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

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

bool CachesAreUnresolved(AttributeExplosion &attr)
{
    if (attr.m_color0 != 0 || attr.m_color1 != 0 || attr.m_color2 != 0 ||
        attr.m_color3 != 0 || attr.m_hTexture != NULL ||
        attr.m_colorSnTail != 0 || attr.m_colorSnHead != 0 ||
        attr.m_colorSnCenter != 0 || attr.m_cacheSkin != NULL ||
        attr.m_wav != NULL || attr.m_ctsndID != ct_NULLID ||
        attr.m_rayColor != 0 || attr.m_smokeTableID != ct_NULLID ||
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
    if (name == NULL || attribute == NULL || !CachesAreUnresolved(*attribute))
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
        7518588989293452268ull
    };
    const unsigned long long fingerprint =
        ExplosionAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}
