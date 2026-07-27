#ifndef RR2NW_LAMP_ATTRIBUTE_STATE_INL
#define RR2NW_LAMP_ATTRIBUTE_STATE_INL

#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

AttributeTableLamp __attrLampTable;

namespace {

const unsigned long long kLampHashOffset = 14695981039346656037ull;
const unsigned long long kLampHashPrime = 1099511628211ull;

void LampHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kLampHashPrime;
    }
}

void LampHashString(unsigned long long &hash, const char *value)
{
    LampHashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

void LampHashAttribute(unsigned long long &hash, AttributeLamp &attr)
{
#define RR2NW_LAMP_HASH(field) \
    LampHashBytes(hash, &attr.field, sizeof(attr.field))
    RR2NW_LAMP_HASH(m_lightColor);
    RR2NW_LAMP_HASH(m_minLightBright);
    RR2NW_LAMP_HASH(m_maxLightBright);
    RR2NW_LAMP_HASH(m_lightRadius);
    RR2NW_LAMP_HASH(m_lightBrightStep);
    RR2NW_LAMP_HASH(m_lightOffset);
    RR2NW_LAMP_HASH(m_useCorona);
    RR2NW_LAMP_HASH(m_coronaRGB);
    RR2NW_LAMP_HASH(m_coronaR);
    RR2NW_LAMP_HASH(m_coronaAlpha);
    LampHashString(hash, attr.m_coronaName);
    RR2NW_LAMP_HASH(m_maxCoronaR);
    RR2NW_LAMP_HASH(m_createIncMin);
    RR2NW_LAMP_HASH(m_createIncMax);
    RR2NW_LAMP_HASH(m_lightMode);
    RR2NW_LAMP_HASH(m_maxTimeLife);
    RR2NW_LAMP_HASH(m_sleepTime);
    RR2NW_LAMP_HASH(m_maxBrightTime);
    RR2NW_LAMP_HASH(m_FadeInCoeff);
    RR2NW_LAMP_HASH(m_FadeOutCoeff);
    RR2NW_LAMP_HASH(m_onLand);
    RR2NW_LAMP_HASH(m_isMoving);
    RR2NW_LAMP_HASH(m_movementType);
    RR2NW_LAMP_HASH(m_movementDeltaT);
    RR2NW_LAMP_HASH(m_hasParticle);
    RR2NW_LAMP_HASH(m_particleWidth);
#undef RR2NW_LAMP_HASH
}

bool LampCachesAreUnresolved(AttributeLamp &attr)
{
    return attr.m_coronaHText == NULL && attr.m_coronaColor == 0 &&
           attr.m_coronaFadeCoeff == 0.0;
}

struct LampRosterEntry
{
    std::string name;
    AttributeLamp *attribute;
};

struct LampRosterCollector
{
    SimulationContext *context;
    std::vector<LampRosterEntry> entries;
    bool valid;
};

bool CollectLampRosterEntry(const KR_ObjectID object, void *user)
{
    LampRosterCollector *collector = static_cast<LampRosterCollector *>(user);
    const char *name = collector->context->searchObject(object);
    AttributeLamp *attribute = static_cast<AttributeLamp *>(
        __attrLampTable.searchAttribute(object));
    if (name == NULL || attribute == NULL ||
        !LampCachesAreUnresolved(*attribute))
    {
        collector->valid = false;
        return false;
    }
    LampRosterEntry entry = {name, attribute};
    collector->entries.push_back(entry);
    return true;
}

bool LampRosterEntryLess(const LampRosterEntry &left,
                         const LampRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectLampRoster(SimulationContext *context,
                       LampRosterCollector &collector)
{
    if (context == NULL || __attrLampTable.capacity() <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __attrLampTable.userFind(CollectLampRosterEntry, &collector);
    if (!collector.valid || collector.entries.empty())
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              LampRosterEntryLess);
    return true;
}

}  // namespace

AttributeLamp::AttributeLamp()
    : m_coronaHText(NULL), m_coronaColor(0), m_coronaFadeCoeff(0.0)
{
    m_lightColor = 7;
    m_minLightBright = 20;
    m_maxLightBright = 255;
    m_lightRadius = 10.0;
    m_lightBrightStep = 30.0;
    m_lightOffset = 2.0;
    m_useCorona = 1;
    m_coronaRGB = 0xFFFFFF;
    m_coronaR = 0.2;
    m_coronaAlpha = 255;
    std::strncpy(m_coronaName, "corona.spr", sizeof(ct_AttrStr) - 1);
    m_coronaName[sizeof(ct_AttrStr) - 1] = 0;
    m_maxCoronaR = 5;
    m_createIncMin = 0.1;
    m_createIncMax = 0.3;
    m_lightMode = 0;
    m_maxTimeLife = 0;
    m_sleepTime = 1.0;
    m_maxBrightTime = 0.5;
    m_FadeInCoeff = 2.0;
    m_FadeOutCoeff = 10;
    m_onLand = 0;
    m_isMoving = 0;
    m_movementType = 0;
    m_movementDeltaT = 2.0;
    m_hasParticle = 1;
    m_particleWidth = 0.1;

    m_array[0].set("m_lightColor", m_lightColor);
    m_array[1].set("m_minLightBright", m_minLightBright);
    m_array[2].set("m_maxLightBright", m_maxLightBright);
    m_array[3].set("m_lightRadius", m_lightRadius);
    m_array[4].set("m_lightBrightStep", m_lightBrightStep);
    m_array[5].set("m_lightOffset", m_lightOffset);
    m_array[6].set("m_useCorona", m_useCorona);
    m_array[7].set("m_coronaRGB", m_coronaRGB);
    m_array[8].set("m_coronaR", m_coronaR);
    m_array[9].set("m_coronaAlpha", m_coronaAlpha);
    m_array[10].set("m_coronaName", m_coronaName);
    m_array[11].set("m_maxCoronaR", m_maxCoronaR);
    m_array[12].set("m_createIncMin", m_createIncMin);
    m_array[13].set("m_createIncMax", m_createIncMax);
    m_array[14].set("m_lightMode", m_lightMode);
    m_array[15].set("m_maxTimeLife", m_maxTimeLife);
    m_array[16].set("m_sleepTime", m_sleepTime);
    m_array[17].set("m_maxBrightTime", m_maxBrightTime);
    m_array[18].set("m_FadeInCoeff", m_FadeInCoeff);
    m_array[19].set("m_FadeOutCoeff", m_FadeOutCoeff);
    // Compatibility contract: the March/May 1999 executable contains two
    // trailing spaces here. Retail scripts use "m_onLand", so their writes
    // are intentionally ignored by the exact-name attribute serializer.
    m_array[20].set("m_onLand  ", m_onLand);
    m_array[21].set("m_isMoving", m_isMoving);
    m_array[22].set("m_movementType", m_movementType);
    m_array[23].set("m_movementDeltaT", m_movementDeltaT);
    m_array[24].set("m_hasParticle", m_hasParticle);
    m_array[25].set("m_particleWidth", m_particleWidth);
    linkTable(m_array, 26);
}

void AttributeLamp::update(double)
{
    m_coronaHText = g_loadSmoke(m_coronaName, NULL);
    m_coronaColor = GRTransparentColor(
        m_coronaRGB >> 16, (m_coronaRGB >> 8) & 255, m_coronaRGB & 255);
    m_coronaFadeCoeff =
        m_coronaAlpha / (m_maxLightBright - m_minLightBright);
}

AttributeTableLamp::AttributeTableLamp() : m_table(NULL)
{
    registerClass("LampAttr");
}

void AttributeTableLamp::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeLamp[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableLamp::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableLamp::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableLamp::getObjectPTR");
    return &(m_table[index]);
}

void LampAttributeState_Link()
{
}

unsigned long long LampAttributeState_Fingerprint(SimulationContext *context)
{
    LampRosterCollector collector = {};
    if (!CollectLampRoster(context, collector))
        return 0;
    unsigned long long hash = kLampHashOffset;
    const int capacity = __attrLampTable.capacity();
    LampHashBytes(hash, &capacity, sizeof(capacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        LampHashString(hash, collector.entries[i].name.c_str());
        LampHashAttribute(hash, *collector.entries[i].attribute);
    }
    return hash;
}

int LampAttributeState_RosterSize(SimulationContext *context)
{
    LampRosterCollector collector = {};
    return CollectLampRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : 0;
}

int LampAttributeState_Capacity()
{
    return __attrLampTable.capacity();
}

bool LampAttributeState_IsKnownRoster(SimulationContext *context)
{
    // First value is the twelve-object May 1999 retail roster. The second is
    // the ten-object public source fixture retained for deterministic CI.
    static const unsigned long long known[] = {
        16446864977750376763ull,
        6451137625588060681ull
    };
    const unsigned long long fingerprint =
        LampAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

#endif
