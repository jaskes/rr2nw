#ifndef RR2NW_SMOKER_ATTRIBUTE_STATE_INL
#define RR2NW_SMOKER_ATTRIBUTE_STATE_INL

#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

AttributeTableSmoker __attrSmokerTable;

namespace {

const unsigned long long kSmokerHashOffset = 14695981039346656037ull;
const unsigned long long kSmokerHashPrime = 1099511628211ull;

void SmokerHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kSmokerHashPrime;
    }
}

void SmokerHashString(unsigned long long &hash, const char *value)
{
    SmokerHashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

void SmokerHashAttribute(unsigned long long &hash, AttributeSmoker &attr)
{
#define RR2NW_SMOKER_HASH_FIELD(field) \
    SmokerHashBytes(hash, &attr.field, sizeof(attr.field))
    RR2NW_SMOKER_HASH_FIELD(m_createIncMin);
    RR2NW_SMOKER_HASH_FIELD(m_createIncMax);
    SmokerHashString(hash, attr.m_smokeAttrName);
    RR2NW_SMOKER_HASH_FIELD(m_maxTimeLife);
    SmokerHashString(hash, attr.m_smokeTableName);
    RR2NW_SMOKER_HASH_FIELD(m_onLand);
    RR2NW_SMOKER_HASH_FIELD(m_useLight);
    RR2NW_SMOKER_HASH_FIELD(m_lightColor);
    RR2NW_SMOKER_HASH_FIELD(m_minLightBright);
    RR2NW_SMOKER_HASH_FIELD(m_maxLightBright);
    RR2NW_SMOKER_HASH_FIELD(m_lightRadius);
    RR2NW_SMOKER_HASH_FIELD(m_lightBrightStep);
    RR2NW_SMOKER_HASH_FIELD(m_lightOffset);
    RR2NW_SMOKER_HASH_FIELD(m_useCorona);
    RR2NW_SMOKER_HASH_FIELD(m_coronaRGB);
    RR2NW_SMOKER_HASH_FIELD(m_coronaR);
    RR2NW_SMOKER_HASH_FIELD(m_coronaAlpha);
    SmokerHashString(hash, attr.m_coronaName);
    RR2NW_SMOKER_HASH_FIELD(m_maxCoronaR);
#undef RR2NW_SMOKER_HASH_FIELD
}

struct SmokerRosterEntry
{
    std::string name;
    AttributeSmoker *attribute;
};

struct SmokerRosterCollector
{
    SimulationContext *context;
    std::vector<SmokerRosterEntry> entries;
    bool valid;
};

bool CollectSmokerRosterEntry(const KR_ObjectID object, void *user)
{
    SmokerRosterCollector *collector =
        static_cast<SmokerRosterCollector *>(user);
    const char *name = collector->context->searchObject(object);
    AttributeSmoker *attribute = static_cast<AttributeSmoker *>(
        __attrSmokerTable.searchAttribute(object));
    if (name == NULL || attribute == NULL ||
        !attribute->m_smokeAttrID.isNUL() ||
        attribute->m_smokeTableID != ct_NULLID ||
        attribute->m_coronaHText != NULL || attribute->m_coronaColor != 0)
    {
        collector->valid = false;
        return false;
    }
    SmokerRosterEntry entry = {name, attribute};
    collector->entries.push_back(entry);
    return true;
}

bool SmokerRosterEntryLess(const SmokerRosterEntry &left,
                           const SmokerRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectSmokerRoster(SimulationContext *context,
                         SmokerRosterCollector &collector)
{
    if (context == NULL || __attrSmokerTable.capacity() <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __attrSmokerTable.userFind(CollectSmokerRosterEntry, &collector);
    if (!collector.valid)
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              SmokerRosterEntryLess);
    return true;
}

}  // namespace

AttributeSmoker::AttributeSmoker()
    : m_smokeAttrID(KR_ObjectID::NUL()), m_smokeTableID(ct_NULLID),
      m_coronaHText(NULL), m_coronaColor(0)
{
    m_createIncMin = 0.1;
    m_createIncMax = 0.3;
    std::strncpy(m_smokeAttrName, "Smoke.Attr.Def", sizeof(ct_AttrStr) - 1);
    m_smokeAttrName[sizeof(ct_AttrStr) - 1] = 0;
    m_maxTimeLife = -1;
    std::strncpy(m_smokeTableName, "Smoke", sizeof(ct_AttrStr) - 1);
    m_smokeTableName[sizeof(ct_AttrStr) - 1] = 0;
    m_onLand = 0;
    m_useLight = 0;
    m_lightColor = 7;
    m_minLightBright = 20;
    m_maxLightBright = 255;
    m_lightRadius = 10.0;
    m_lightBrightStep = 30.0;
    m_lightOffset = 2.0;
    m_useCorona = 0;
    m_coronaRGB = 0xFFFFFF;
    m_coronaR = 0.2;
    m_coronaAlpha = 255;
    std::strncpy(m_coronaName, "Smoke.spr", sizeof(ct_AttrStr) - 1);
    m_coronaName[sizeof(ct_AttrStr) - 1] = 0;
    m_maxCoronaR = 5;

    m_array[0].set("m_createIncMin", m_createIncMin);
    m_array[1].set("m_createIncMax", m_createIncMax);
    m_array[2].set("m_smokeAttrName", m_smokeAttrName);
    m_array[3].set("m_maxTimeLife", m_maxTimeLife);
    m_array[4].set("m_smokeTableName", m_smokeTableName);
    m_array[5].set("m_onLand", m_onLand);
    m_array[6].set("m_useLight", m_useLight);
    m_array[7].set("m_lightColor", m_lightColor);
    m_array[8].set("m_minLightBright", m_minLightBright);
    m_array[9].set("m_maxLightBright", m_maxLightBright);
    m_array[10].set("m_lightRadius", m_lightRadius);
    m_array[11].set("m_lightBrightStep", m_lightBrightStep);
    m_array[12].set("m_lightOffset", m_lightOffset);
    m_array[13].set("m_useCorona", m_useCorona);
    m_array[14].set("m_coronaRGB", m_coronaRGB);
    m_array[15].set("m_coronaR", m_coronaR);
    m_array[16].set("m_coronaAlpha", m_coronaAlpha);
    m_array[17].set("m_coronaName", m_coronaName);
    m_array[18].set("m_maxCoronaR", m_maxCoronaR);
    linkTable(m_array, 19);
}

AttributeTableSmoker::AttributeTableSmoker() : m_table(NULL)
{
    registerClass("SmokerAttr");
}

void AttributeTableSmoker::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeSmoker[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableSmoker::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableSmoker::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableSmoker::getObjectPTR");
    return &(m_table[index]);
}

void AttributeSmoker::update(double)
{
    m_smokeTableID = g_arena.searchSeanceClassTable(m_smokeTableName);
    m_smokeAttrID = context->searchObject(m_smokeAttrName);
    m_coronaHText = g_loadSmoke(m_coronaName, NULL);
    m_coronaColor = GRTransparentColor(m_coronaRGB >> 16,
                                       (m_coronaRGB >> 8) & 255,
                                       m_coronaRGB & 255);
}

void SmokerAttributeState_Link()
{
}

unsigned long long SmokerAttributeState_Fingerprint(
    SimulationContext *context)
{
    SmokerRosterCollector collector = {};
    if (!CollectSmokerRoster(context, collector))
        return 0;
    unsigned long long hash = kSmokerHashOffset;
    const int capacity = __attrSmokerTable.capacity();
    SmokerHashBytes(hash, &capacity, sizeof(capacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        SmokerHashString(hash, collector.entries[i].name.c_str());
        SmokerHashAttribute(hash, *collector.entries[i].attribute);
    }
    return hash;
}

int SmokerAttributeState_RosterSize(SimulationContext *context)
{
    SmokerRosterCollector collector = {};
    return CollectSmokerRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : -1;
}

int SmokerAttributeState_Capacity()
{
    return __attrSmokerTable.capacity();
}

bool SmokerAttributeState_IsKnownRoster(SimulationContext *context)
{
    // Filled from the public January snapshot and canonical May retail root.
    static const unsigned long long known[] = {
        7057393947133380293ull,
        5654440424696413223ull
    };
    const unsigned long long fingerprint =
        SmokerAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

#endif
