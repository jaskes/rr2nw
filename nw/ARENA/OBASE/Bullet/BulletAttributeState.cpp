#include "BulletAttributeState.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "BulletSubjectState.h"
#include "h/cachesmoke.h"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "message/funitmsg.h"
#include "obase/skin/SkinResourceState.h"
#include "obase/sound/WAVResourceState.h"

AttributeTableBullet __bulletAttrTable;

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
int g_attributeCapacity = 0;
char g_lastError[192] = {};

void SetLastError(const char *kind, const char *name)
{
    std::snprintf(g_lastError, sizeof(g_lastError),
                  "Bullet dependency missing: %s <%s>", kind,
                  name == NULL ? "" : name);
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

void HashAttribute(unsigned long long &hash, AttributeBullet &attribute)
{
#define RR2NW_BULLET_HASH(field) \
    HashBytes(hash, &attribute.field, sizeof(attribute.field))
    RR2NW_BULLET_HASH(m_type);
    RR2NW_BULLET_HASH(m_RGB);
    RR2NW_BULLET_HASH(m_RGB0);
    RR2NW_BULLET_HASH(m_moveTimeIncrement);
    RR2NW_BULLET_HASH(m_chkClzTimeIncrement);
    HashString(hash, attribute.m_sparkAttr);
    HashString(hash, attribute.m_outSparkAttr);
    HashString(hash, attribute.m_splashAttr);
    RR2NW_BULLET_HASH(m_massa);
    RR2NW_BULLET_HASH(m_startSpeed);
    HashString(hash, attribute.m_sparkTable);
    HashString(hash, attribute.m_sparkAttrTable);
    RR2NW_BULLET_HASH(m_radius0);
    RR2NW_BULLET_HASH(m_radius1);
    RR2NW_BULLET_HASH(m_length);
    RR2NW_BULLET_HASH(m_step0);
    RR2NW_BULLET_HASH(m_step);
    RR2NW_BULLET_HASH(m_useLight);
    RR2NW_BULLET_HASH(m_lightRadius);
    RR2NW_BULLET_HASH(m_lightBrightness);
    RR2NW_BULLET_HASH(m_lightColor);
    HashString(hash, attribute.m_smokeTableName);
    HashString(hash, attribute.m_smokeAttrName);
    HashString(hash, attribute.m_explAttrName);
    HashString(hash, attribute.m_trace);
    RR2NW_BULLET_HASH(m_hasTrace);
    RR2NW_BULLET_HASH(m_traceMinDist);
    HashString(hash, attribute.m_traceWidthString);
    RR2NW_BULLET_HASH(m_traceFlatRatio);
    RR2NW_BULLET_HASH(m_traceAllFlatDist);
    HashString(hash, attribute.m_traceAss);
    RR2NW_BULLET_HASH(m_traceSegmentLength);
    HashString(hash, attribute.m_shootSndName);
    RR2NW_BULLET_HASH(m_traceExist);
    HashString(hash, attribute.m_traceTexture);
    RR2NW_BULLET_HASH(m_useBarellSmoke);
    RR2NW_BULLET_HASH(m_useSkin);
    HashString(hash, attribute.m_skinName);
    RR2NW_BULLET_HASH(m_rotSpeedOx);
    RR2NW_BULLET_HASH(m_rotSpeedOy);
    RR2NW_BULLET_HASH(m_rotSpeedOz);
#undef RR2NW_BULLET_HASH
}

void ClearCaches(AttributeBullet &attribute)
{
    attribute.m_cacheImage = NULL;
    attribute.m_cacheImageFront = NULL;
    attribute.m_wav = NULL;
    attribute.m_ctsndID = ct_NULLID;
    std::memset(attribute.m_colorGrad, 0, sizeof(attribute.m_colorGrad));
    attribute.m_smokeTableID = ct_NULLID;
    attribute.m_smokeAttrID = KR_ObjectID::NUL();
    attribute.m_cacheSparkAttrTable = ct_NULLID;
    attribute.m_cacheSparkTable = ct_NULLID;
    attribute.m_cacheColor = 0;
    attribute.m_cacheSparkAttr = ct_NULLID;
    attribute.m_cacheOutSparkAttr = ct_NULLID;
    attribute.m_cacheSplashAttr = ct_NULLID;
    attribute.m_cacheExplAttr = ct_NULLID;
    attribute.m_cacheExplosionTable = ct_NULLID;
    attribute.m_cacheSkin = NULL;
}

bool CachesAreUnresolved(AttributeBullet &attribute)
{
    if (attribute.m_cacheImage != NULL ||
        attribute.m_cacheImageFront != NULL || attribute.m_wav != NULL ||
        attribute.m_ctsndID != ct_NULLID ||
        attribute.m_smokeTableID != ct_NULLID ||
        !attribute.m_smokeAttrID.isNUL() ||
        attribute.m_cacheSparkAttrTable != ct_NULLID ||
        attribute.m_cacheSparkTable != ct_NULLID ||
        attribute.m_cacheColor != 0 ||
        attribute.m_cacheSparkAttr != ct_NULLID ||
        attribute.m_cacheOutSparkAttr != ct_NULLID ||
        attribute.m_cacheSplashAttr != ct_NULLID ||
        attribute.m_cacheExplAttr != ct_NULLID ||
        attribute.m_cacheExplosionTable != ct_NULLID ||
        attribute.m_cacheSkin != NULL)
        return false;
    for (int i = 0; i < RR2NW_BULLET_COLOR_GRAD; ++i)
        if (attribute.m_colorGrad[i] != 0)
            return false;
    return true;
}

struct RosterEntry
{
    std::string name;
    AttributeBullet *attribute;
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
    AttributeBullet *attribute = static_cast<AttributeBullet *>(
        __bulletAttrTable.searchAttribute(object));
    if (name == NULL || attribute == NULL)
    {
        collector->valid = false;
        return false;
    }
    RosterEntry entry = {name, attribute};
    collector->entries.push_back(entry);
    return true;
}

bool RosterEntryLess(const RosterEntry &left, const RosterEntry &right)
{
    return left.name < right.name;
}

bool CollectRoster(SimulationContext *context, RosterCollector &collector)
{
    if (context == NULL || g_attributeCapacity <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __bulletAttrTable.userFind(CollectRosterEntry, &collector);
    if (!collector.valid || collector.entries.empty())
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              RosterEntryLess);
    return true;
}

int ResolveAttributeIndex(SimulationContext *context,
                          ct_ClassTableID table,
                          const char *name)
{
    if (table == ct_NULLID || name == NULL || name[0] == 0)
        return ct_NULLID;
    KR_ObjectID object = context->searchObject(name);
    return object.isNUL() ? ct_NULLID
                          : g_arena.getAttributeIndex(table, object);
}

GR_HTEXTURE FindCachedTexture(const char *name)
{
    if (name == NULL || name[0] == 0)
        return NULL;
    const int count = SmokeTextureCache_Checkpoint();
    for (int i = 0; i < count; ++i)
        if (strcmpi(g_cacheSmoke[i].fname, name) == 0)
            return g_cacheSmoke[i].hand;
    return NULL;
}

struct ResolvedReferences
{
    GR_HTEXTURE image;
    GR_HTEXTURE imageFront;
    WAVObj *wav;
    ct_ClassTableID soundTable;
    unsigned long colorGradient[RR2NW_BULLET_COLOR_GRAD];
    ct_ClassTableID smokeTable;
    KR_ObjectID smokeAttribute;
    ct_ClassTableID sparkAttributeTable;
    ct_ClassTableID sparkTable;
    int color;
    int sparkAttribute;
    int outSparkAttribute;
    int splashAttribute;
    int explosionAttribute;
    ct_ClassTableID explosionTable;
    CViewObjectModel *skin;
};

bool ResolveNonVisualReferences(SimulationContext *context,
                                AttributeBullet &attribute,
                                ResolvedReferences &references)
{
    references = {};
    references.soundTable = ct_NULLID;
    references.smokeTable =
        g_arena.searchSeanceClassTable(attribute.m_smokeTableName);
    references.sparkAttributeTable =
        g_arena.searchSeanceClassTable(attribute.m_sparkAttrTable);
    references.sparkTable =
        g_arena.searchSeanceClassTable(attribute.m_sparkTable);
    references.explosionTable =
        g_arena.searchSeanceClassTable("Explosion");
    const ct_ClassTableID smokeAttributeTable =
        g_arena.searchSeanceClassTable("SmokeAttr");
    const ct_ClassTableID explosionAttributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    if (references.smokeTable == ct_NULLID ||
        references.sparkAttributeTable == ct_NULLID ||
        references.sparkTable == ct_NULLID ||
        references.explosionTable == ct_NULLID ||
        smokeAttributeTable == ct_NULLID ||
        explosionAttributeTable == ct_NULLID)
    {
        SetLastError("class table", "Spark/SparkAttr/Explosion/"
                     "ExplosionAttr/Smoke/SmokeAttr");
        return false;
    }

    references.smokeAttribute =
        context->searchObject(attribute.m_smokeAttrName);
    if (references.smokeAttribute.isNUL() ||
        g_arena.getAttributeIndex(smokeAttributeTable,
                                  references.smokeAttribute) == ct_NULLID)
    {
        SetLastError("SmokeAttr", attribute.m_smokeAttrName);
        return false;
    }
    references.sparkAttribute = ResolveAttributeIndex(
        context, references.sparkAttributeTable, attribute.m_sparkAttr);
    references.outSparkAttribute = ResolveAttributeIndex(
        context, references.sparkAttributeTable, attribute.m_outSparkAttr);
    references.explosionAttribute = ResolveAttributeIndex(
        context, explosionAttributeTable, attribute.m_explAttrName);
    references.splashAttribute = ResolveAttributeIndex(
        context, explosionAttributeTable, attribute.m_splashAttr);
    if (references.sparkAttribute == ct_NULLID)
    {
        SetLastError("SparkAttr", attribute.m_sparkAttr);
        return false;
    }
    if (references.outSparkAttribute == ct_NULLID)
    {
        SetLastError("SparkAttr", attribute.m_outSparkAttr);
        return false;
    }
    if (references.explosionAttribute == ct_NULLID)
    {
        SetLastError("ExplosionAttr", attribute.m_explAttrName);
        return false;
    }
    if (references.splashAttribute == ct_NULLID)
    {
        SetLastError("ExplosionAttr", attribute.m_splashAttr);
        return false;
    }

    references.color = GRCreateColor(attribute.m_RGB >> 16,
        (attribute.m_RGB >> 8) & 0xff, attribute.m_RGB & 0xff);
    const int r0 = attribute.m_RGB0 >> 16;
    const int g0 = (attribute.m_RGB0 >> 8) & 0xff;
    const int b0 = attribute.m_RGB0 & 0xff;
    const int r1 = attribute.m_RGB >> 16;
    const int g1 = (attribute.m_RGB >> 8) & 0xff;
    const int b1 = attribute.m_RGB & 0xff;
    for (int i = 0; i < RR2NW_BULLET_COLOR_GRAD; ++i)
    {
        int r = r0 + (r1 - r0) * i / (RR2NW_BULLET_COLOR_GRAD - 1);
        int g = g0 + (g1 - g0) * i / (RR2NW_BULLET_COLOR_GRAD - 1);
        int b = b0 + (b1 - b0) * i / (RR2NW_BULLET_COLOR_GRAD - 1);
        r = (std::max)(0, (std::min)(255, r));
        g = (std::max)(0, (std::min)(255, g));
        b = (std::max)(0, (std::min)(255, b));
        references.colorGradient[i] = GRCreateColor(r, g, b);
    }

    if (attribute.m_shootSndName[0] != 0)
    {
        references.soundTable =
            g_arena.searchSeanceClassTable("SoundObj");
        if (references.soundTable == ct_NULLID ||
            !WAVResourceState_ResolveLoaded(
                context, attribute.m_shootSndName, &references.wav))
        {
            SetLastError("WAVObj", attribute.m_shootSndName);
            return false;
        }
    }
    if (attribute.m_useSkin)
    {
        KR_ObjectID skinID;
        if (!SkinResourceState_ResolveLoadedModel(
                context, attribute.m_skinName, &skinID, &references.skin))
        {
            SetLastError("Skin", attribute.m_skinName);
            return false;
        }
    }
    return true;
}

bool ReferencesEqual(AttributeBullet &attribute,
                     const ResolvedReferences &references)
{
    if (attribute.m_cacheImage != references.image ||
        attribute.m_cacheImageFront != references.imageFront ||
        attribute.m_wav != references.wav ||
        attribute.m_ctsndID != references.soundTable ||
        attribute.m_smokeTableID != references.smokeTable ||
        attribute.m_smokeAttrID != references.smokeAttribute ||
        attribute.m_cacheSparkAttrTable !=
            references.sparkAttributeTable ||
        attribute.m_cacheSparkTable != references.sparkTable ||
        attribute.m_cacheColor != references.color ||
        attribute.m_cacheSparkAttr != references.sparkAttribute ||
        attribute.m_cacheOutSparkAttr != references.outSparkAttribute ||
        attribute.m_cacheSplashAttr != references.splashAttribute ||
        attribute.m_cacheExplAttr != references.explosionAttribute ||
        attribute.m_cacheExplosionTable != references.explosionTable ||
        attribute.m_cacheSkin != references.skin)
        return false;
    return std::memcmp(attribute.m_colorGrad, references.colorGradient,
                       sizeof(attribute.m_colorGrad)) == 0;
}

void CommitReferences(AttributeBullet &attribute,
                      const ResolvedReferences &references)
{
    attribute.m_cacheImage = references.image;
    attribute.m_cacheImageFront = references.imageFront;
    attribute.m_wav = references.wav;
    attribute.m_ctsndID = references.soundTable;
    std::memcpy(attribute.m_colorGrad, references.colorGradient,
                sizeof(attribute.m_colorGrad));
    attribute.m_smokeTableID = references.smokeTable;
    attribute.m_smokeAttrID = references.smokeAttribute;
    attribute.m_cacheSparkAttrTable = references.sparkAttributeTable;
    attribute.m_cacheSparkTable = references.sparkTable;
    attribute.m_cacheColor = references.color;
    attribute.m_cacheSparkAttr = references.sparkAttribute;
    attribute.m_cacheOutSparkAttr = references.outSparkAttribute;
    attribute.m_cacheSplashAttr = references.splashAttribute;
    attribute.m_cacheExplAttr = references.explosionAttribute;
    attribute.m_cacheExplosionTable = references.explosionTable;
    attribute.m_cacheSkin = references.skin;
}

}  // namespace

AttributeBullet::AttributeBullet()
{
    ClearCaches(*this);
    m_type = 0;
    m_RGB = 0x00ffd050;
    m_RGB0 = 0xffffff;
    m_moveTimeIncrement = 0.02;
    m_chkClzTimeIncrement = 0.1;
#define RR2NW_BULLET_STRING(field, value) \
    std::strncpy(field, value, sizeof(field) - 1); \
    field[sizeof(field) - 1] = 0
    RR2NW_BULLET_STRING(m_sparkAttr, "Spark.Flash");
    RR2NW_BULLET_STRING(m_outSparkAttr, "Spark.Flash");
    RR2NW_BULLET_STRING(m_splashAttr, "BulletSplash");
    m_massa = 0.009;
    m_startSpeed = 40.0;
    RR2NW_BULLET_STRING(m_sparkTable, "Spark");
    RR2NW_BULLET_STRING(m_sparkAttrTable, "SparkAttr");
    m_radius0 = 0.2;
    m_radius1 = 0.6;
    m_length = 6.0;
    m_step0 = 0.1;
    m_step = 0.08;
    m_useLight = 0;
    m_lightRadius = 2.0;
    m_lightBrightness = 250;
    m_lightColor = 7;
    RR2NW_BULLET_STRING(m_smokeTableName, "Smoke");
    RR2NW_BULLET_STRING(m_smokeAttrName, "Smoke.Attr.Led");
    RR2NW_BULLET_STRING(m_explAttrName, "Expl.Attr.Default");
    RR2NW_BULLET_STRING(m_trace, "trace.spr");
    m_hasTrace = 1;
    m_traceMinDist = 15.0;
    RR2NW_BULLET_STRING(m_traceWidthString, "BEFKLMMLKFEB");
    m_traceFlatRatio = 0.8;
    m_traceAllFlatDist = 90.0;
    RR2NW_BULLET_STRING(m_traceAss, "smoke.spr");
    m_traceSegmentLength = 10.0;
    RR2NW_BULLET_STRING(m_shootSndName, "wav.Shoot.Gun2");
    m_traceExist = 0;
    RR2NW_BULLET_STRING(m_traceTexture, "trace.spr");
    m_useBarellSmoke = 1;
    m_useSkin = 0;
    RR2NW_BULLET_STRING(m_skinName, "");
    m_rotSpeedOx = 0;
    m_rotSpeedOy = 0;
    m_rotSpeedOz = 0;
#undef RR2NW_BULLET_STRING

#define RR2NW_BULLET_LINK(index, field) m_array[index].set(#field, field)
    RR2NW_BULLET_LINK(0, m_type);
    RR2NW_BULLET_LINK(1, m_RGB);
    RR2NW_BULLET_LINK(2, m_RGB0);
    RR2NW_BULLET_LINK(3, m_moveTimeIncrement);
    RR2NW_BULLET_LINK(4, m_chkClzTimeIncrement);
    RR2NW_BULLET_LINK(5, m_sparkAttr);
    RR2NW_BULLET_LINK(6, m_outSparkAttr);
    RR2NW_BULLET_LINK(7, m_splashAttr);
    RR2NW_BULLET_LINK(8, m_massa);
    RR2NW_BULLET_LINK(9, m_startSpeed);
    RR2NW_BULLET_LINK(10, m_sparkTable);
    RR2NW_BULLET_LINK(11, m_sparkAttrTable);
    RR2NW_BULLET_LINK(12, m_radius0);
    RR2NW_BULLET_LINK(13, m_radius1);
    RR2NW_BULLET_LINK(14, m_length);
    RR2NW_BULLET_LINK(15, m_step0);
    RR2NW_BULLET_LINK(16, m_step);
    RR2NW_BULLET_LINK(17, m_useLight);
    RR2NW_BULLET_LINK(18, m_lightRadius);
    RR2NW_BULLET_LINK(19, m_lightBrightness);
    RR2NW_BULLET_LINK(20, m_lightColor);
    RR2NW_BULLET_LINK(21, m_smokeTableName);
    RR2NW_BULLET_LINK(22, m_smokeAttrName);
    RR2NW_BULLET_LINK(23, m_explAttrName);
    RR2NW_BULLET_LINK(24, m_trace);
    RR2NW_BULLET_LINK(25, m_hasTrace);
    RR2NW_BULLET_LINK(26, m_traceMinDist);
    RR2NW_BULLET_LINK(27, m_traceWidthString);
    RR2NW_BULLET_LINK(28, m_traceFlatRatio);
    RR2NW_BULLET_LINK(29, m_traceAllFlatDist);
    RR2NW_BULLET_LINK(30, m_traceAss);
    RR2NW_BULLET_LINK(31, m_traceSegmentLength);
    RR2NW_BULLET_LINK(32, m_shootSndName);
    RR2NW_BULLET_LINK(33, m_traceExist);
    RR2NW_BULLET_LINK(34, m_traceTexture);
    RR2NW_BULLET_LINK(35, m_useBarellSmoke);
    RR2NW_BULLET_LINK(36, m_useSkin);
    RR2NW_BULLET_LINK(37, m_skinName);
    RR2NW_BULLET_LINK(38, m_rotSpeedOx);
    RR2NW_BULLET_LINK(39, m_rotSpeedOy);
    RR2NW_BULLET_LINK(40, m_rotSpeedOz);
#undef RR2NW_BULLET_LINK
    linkTable(m_array, 41);
}

AttributeTableBullet::AttributeTableBullet() : m_table(NULL)
{
    registerClass("BulletAttr");
}

void AttributeTableBullet::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeBullet[objectQnty];
    if (m_table == NULL)
    {
        m_maxObjectQnty = 0;
        g_attributeCapacity = 0;
        return;
    }
    g_attributeCapacity = objectQnty;
}

void AttributeTableBullet::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
    g_attributeCapacity = 0;
}

ct_Object *AttributeTableBullet::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableBullet::getObjectPTR");
    return &(m_table[index]);
}

void AttributeBullet::update(double)
{
    // Retail setters call update repeatedly while an object is incomplete.
    // Keep those writes side-effect free; the owner resolves the full roster
    // in one transaction once every dependent table/resource exists.
    ClearCaches(*this);
}

int AttributeBullet::receiveEvent(KR_Event &event)
{
    if (event.label != fu_EV_QUERY_SPEED)
        return ct_Attribute::receiveEvent(event);
    event.data.open(EDO_WRITE).putDouble(m_startSpeed).close();
    event.label = fu_EV_QUERY_SPEED_OK;
    return 1;
}

void BulletAttributeState_Link()
{
    BulletSubjectState_Link();
}

int BulletAttributeState_RosterSize(SimulationContext *context)
{
    RosterCollector collector = {};
    return CollectRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : -1;
}

int BulletAttributeState_Capacity()
{
    return g_attributeCapacity;
}

int BulletAttributeState_SubjectCapacity()
{
    return BulletSubjectState_Capacity();
}

bool BulletAttributeState_SubjectTableReady(SimulationContext *context)
{
    const int capacity = BulletSubjectState_Capacity();
    if (context == NULL || g_arena.getContext() != context || capacity <= 0)
        return false;
    const int known[] = {50, 100, 250, 500};
    bool capacityKnown = false;
    for (int i = 0; i < 4; ++i)
        if (capacity == known[i])
            capacityKnown = true;
    return capacityKnown &&
           BulletSubjectState_TableReady(context, capacity);
}

unsigned long long BulletAttributeState_Fingerprint(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return 0;
    unsigned long long hash = kHashOffset;
    const int subjectCapacity = BulletSubjectState_Capacity();
    HashBytes(hash, &g_attributeCapacity, sizeof(g_attributeCapacity));
    HashBytes(hash, &subjectCapacity, sizeof(subjectCapacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        HashString(hash, collector.entries[i].name.c_str());
        HashAttribute(hash, *collector.entries[i].attribute);
    }
    return hash;
}

bool BulletAttributeState_IsKnownRoster(SimulationContext *context)
{
    static const unsigned long long known[] = {
        // January public source fixture: 4 BulletAttr / Bullet(500).
        1712455478039360212ull,
        // May retail: 01D/01N, 02D/02N, 03N, 04D, 05D, 06N, 07N.
        7049523956833959095ull,
        4765450848671018588ull,
        14925665994745469719ull,
        5727703390122206721ull,
        8941119509838881546ull,
        17515670196467174251ull,
        2736248886668568454ull
    };
    if (!BulletAttributeState_SubjectTableReady(context))
        return false;
    const unsigned long long fingerprint =
        BulletAttributeState_Fingerprint(context);
    for (std::size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

bool BulletAttributeState_CachesUnresolved(SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (!CachesAreUnresolved(*collector.entries[i].attribute))
            return false;
    return true;
}

bool BulletAttributeState_ResolveReferences(SimulationContext *context)
{
    g_lastError[0] = 0;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) ||
        !BulletAttributeState_SubjectTableReady(context))
    {
        SetLastError("roster/table", "BulletAttr/Bullet");
        return false;
    }
    std::vector<ResolvedReferences> references(collector.entries.size());
    std::vector<const char *> textures;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeBullet &attribute = *collector.entries[i].attribute;
        if (!ResolveNonVisualReferences(context, attribute, references[i]))
            return false;
        if (attribute.m_traceExist)
        {
            textures.push_back(attribute.m_traceTexture);
            textures.push_back("smoke.spr");
        }
    }
    if (!textures.empty() &&
        !SmokeTextureCache_CanLoad(textures.data(),
                                   static_cast<int>(textures.size())))
    {
        SetLastError("texture cache capacity", "trace resources");
        return false;
    }

    const int textureCheckpoint = SmokeTextureCache_Checkpoint();
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeBullet &attribute = *collector.entries[i].attribute;
        if (!attribute.m_traceExist)
            continue;
        references[i].image = g_loadSmoke(attribute.m_traceTexture, NULL);
        references[i].imageFront = g_loadSmoke("smoke.spr", NULL);
        if (references[i].image == NULL || references[i].imageFront == NULL)
        {
            SmokeTextureCache_Rollback(textureCheckpoint);
            SetLastError("texture", attribute.m_traceTexture);
            return false;
        }
    }
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        CommitReferences(*collector.entries[i].attribute, references[i]);
    return true;
}

bool BulletAttributeState_ReferencesResolved(SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeBullet &attribute = *collector.entries[i].attribute;
        ResolvedReferences expected = {};
        if (!ResolveNonVisualReferences(context, attribute, expected))
            return false;
        if (attribute.m_traceExist)
        {
            expected.image = FindCachedTexture(attribute.m_traceTexture);
            expected.imageFront = FindCachedTexture("smoke.spr");
            if (expected.image == NULL || expected.imageFront == NULL)
                return false;
        }
        if (!ReferencesEqual(attribute, expected))
            return false;
    }
    return true;
}

unsigned long long BulletAttributeState_ReferenceFingerprint(
    SimulationContext *context)
{
    if (!BulletAttributeState_ReferencesResolved(context))
        return 0;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return 0;
    unsigned long long hash = kHashOffset;
    const int subjectCapacity = BulletSubjectState_Capacity();
    HashBytes(hash, &g_attributeCapacity, sizeof(g_attributeCapacity));
    HashBytes(hash, &subjectCapacity, sizeof(subjectCapacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeBullet &attribute = *collector.entries[i].attribute;
        HashString(hash, collector.entries[i].name.c_str());
        HashAttribute(hash, attribute);
        HashBytes(hash, &attribute.m_cacheColor,
                  sizeof(attribute.m_cacheColor));
        HashBytes(hash, attribute.m_colorGrad,
                  sizeof(attribute.m_colorGrad));
        HashString(hash, attribute.m_sparkTable);
        HashString(hash, attribute.m_sparkAttrTable);
        HashString(hash, attribute.m_sparkAttr);
        HashString(hash, attribute.m_outSparkAttr);
        HashString(hash, "Explosion");
        HashString(hash, attribute.m_explAttrName);
        HashString(hash, attribute.m_splashAttr);
        HashString(hash, attribute.m_smokeTableName);
        HashString(hash, attribute.m_smokeAttrName);
        HashString(hash, attribute.m_shootSndName);
        HashString(hash, attribute.m_useSkin ? attribute.m_skinName : "");
        HashString(hash,
                   attribute.m_traceExist ? attribute.m_traceTexture : "");
        HashString(hash, attribute.m_traceExist ? "smoke.spr" : "");
    }
    return hash;
}

bool BulletAttributeState_IsKnownReferenceRoster(
    SimulationContext *context)
{
    static const unsigned long long known[] = {
        16411189436502945310ull,  // 01D
        10085683157827810974ull,  // 01N
        17098152857370235907ull,  // 02D
        10265746233383620522ull,  // 02N
        15908499397651653066ull,  // 03N
        14908054844878695565ull,  // 04D
        11154395558747041286ull,  // 05D
        3445417319787692654ull,   // 06N
        10893998309281092758ull   // 07N
    };
    if (!BulletAttributeState_IsKnownRoster(context))
        return false;
    const unsigned long long fingerprint =
        BulletAttributeState_ReferenceFingerprint(context);
    for (std::size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

const char *BulletAttributeState_LastError()
{
    return g_lastError;
}

bool BulletAttributeState_ResolveEncodedIndex(
    SimulationContext *context, int encodedIndex,
    AttributeBullet **attribute)
{
    if (attribute == NULL)
        return false;
    *attribute = NULL;
    if (context == NULL || g_arena.getContext() != context ||
        encodedIndex == -1)
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("BulletAttr");
    if (table == ct_NULLID)
        return false;
    RosterCollector collector = {};
    if (!CollectRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeBullet *candidate = collector.entries[i].attribute;
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

const char *BulletAttributeState_FirstAttributeName(
    SimulationContext *context)
{
    RosterCollector collector = {};
    if (!CollectRoster(context, collector) || collector.entries.empty())
        return NULL;
    return context->searchObject(
        collector.entries.front().attribute->getObjectID());
}
