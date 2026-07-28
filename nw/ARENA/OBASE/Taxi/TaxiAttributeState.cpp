#define LAST_H__VIEW
#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "game.h"
#include "scene.h"
#include "Taxi.h"
#include "TaxiAttributeState.h"

#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "message/skinmsg.h"
#include "storage/h/subject.h"

// Extracted verbatim in behavior from Taxi.cpp so Vehicle can link the
// attribute registry without constructing the full renderer-backed Taxi class.
AttributeTableTaxi __attrTaxiTable;

namespace {

const unsigned long long kTaxiHashOffset = 14695981039346656037ull;
const unsigned long long kTaxiHashPrime = 1099511628211ull;
int g_taxiAttributeCapacity = 0;

void TaxiHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kTaxiHashPrime;
    }
}

void TaxiHashString(unsigned long long &hash, const char *value)
{
    TaxiHashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

void TaxiHashAttribute(unsigned long long &hash, AttributeTaxi &attribute)
{
    TaxiHashString(hash, attribute.m_name);
    TaxiHashString(hash, attribute.m_skinName);
    TaxiHashString(hash, attribute.m_attrForVehicleName);
    TaxiHashString(hash, attribute.m_corpseAttrName);
    TaxiHashBytes(hash, &attribute.m_initialDamage,
                  sizeof(attribute.m_initialDamage));
    TaxiHashBytes(hash, &attribute.m_yOffset, sizeof(attribute.m_yOffset));
    TaxiHashBytes(hash, &attribute.m_buzzing, sizeof(attribute.m_buzzing));
}

bool TaxiCachesAreUnresolved(AttributeTaxi &attribute)
{
    return attribute.m_cacheSkin == NULL &&
           attribute.m_cacheCorpseTable == ct_NULLID &&
           attribute.m_cacheCorpseAttr == ct_NULLID &&
           attribute.m_skinID.isNUL() &&
           attribute.m_attrForVehicle.isNUL();
}

struct TaxiRosterEntry
{
    std::string name;
    AttributeTaxi *attribute;
};

struct TaxiRosterCollector
{
    SimulationContext *context;
    std::vector<TaxiRosterEntry> entries;
    bool valid;
};

bool CollectTaxiRosterEntry(const KR_ObjectID object, void *user)
{
    TaxiRosterCollector *collector = static_cast<TaxiRosterCollector *>(user);
    const char *name = collector->context->searchObject(object);
    AttributeTaxi *attribute = static_cast<AttributeTaxi *>(
        __attrTaxiTable.searchAttribute(object));
    if (name == NULL || attribute == NULL ||
        !TaxiCachesAreUnresolved(*attribute))
    {
        collector->valid = false;
        return false;
    }
    TaxiRosterEntry entry = {name, attribute};
    collector->entries.push_back(entry);
    return true;
}

bool TaxiRosterEntryLess(const TaxiRosterEntry &left,
                         const TaxiRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectTaxiRoster(SimulationContext *context,
                       TaxiRosterCollector &collector)
{
    if (context == NULL || g_taxiAttributeCapacity <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __attrTaxiTable.userFind(CollectTaxiRosterEntry, &collector);
    if (!collector.valid || collector.entries.empty())
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              TaxiRosterEntryLess);
    return true;
}

}  // namespace

void AttributeTableTaxi::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeTaxi[objectQnty];

    if (m_table == NULL)
    {
        m_maxObjectQnty = 0;
        g_taxiAttributeCapacity = 0;
        return;
    }
    g_taxiAttributeCapacity = objectQnty;
    for (int i = 0; i < objectQnty; ++i)
    {
        m_table[i].m_cacheSkin = NULL;
        m_table[i].m_cacheCorpseTable = ct_NULLID;
        m_table[i].m_cacheCorpseAttr = ct_NULLID;
        m_table[i].m_skinID = KR_ObjectID::NUL();
        m_table[i].m_attrForVehicle = KR_ObjectID::NUL();
        m_table[i].m_name[sizeof(ct_AttrStr) - 1] = 0;
        m_table[i].m_skinName[sizeof(ct_AttrStr) - 1] = 0;
        m_table[i].m_attrForVehicleName[sizeof(ct_AttrStr) - 1] = 0;
        m_table[i].m_corpseAttrName[sizeof(ct_AttrStr) - 1] = 0;
    }
}

void AttributeTableTaxi::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
    g_taxiAttributeCapacity = 0;
}

ct_Object *AttributeTableTaxi::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTable::getObjectPTR");
    return &(m_table[index]);
}

void AttributeTaxi::update(double ts)
{
    KR_ObjectID skinID = context->searchObject(m_skinName);
    s_ASSERT(!skinID.isNUL(), "AttributeTaxi::update");

    m_skinID = skinID;

    KR_Event event;
    event.timeStamp = ts;
    event.label = sk_EV_QUERY_MODEL_PTR;
    event.destination = skinID;
    context->sendEventNow(event);
    s_ASSERT(event.label == sk_EV_QUERY_MODEL_PTR_OK, "AttributeTaxi::update");
    event.data.open(EDO_READ)
              .get(&m_cacheSkin, sizeof(void *))
              .close();

    m_attrForVehicle = context->searchObject(m_attrForVehicleName);

    m_cacheCorpseTable = g_arena.searchSeanceClassTable("Corpse");
    m_cacheCorpseAttr = g_arena.getAttributeIndex(
        g_arena.searchSeanceClassTable("CorpseAttr"),
        context->searchObject(m_corpseAttrName));
}

void TaxiAttributeState_Link()
{
}

unsigned long long TaxiAttributeState_Fingerprint(SimulationContext *context)
{
    TaxiRosterCollector collector = {};
    if (!CollectTaxiRoster(context, collector))
        return 0;
    unsigned long long hash = kTaxiHashOffset;
    TaxiHashBytes(hash, &g_taxiAttributeCapacity,
                  sizeof(g_taxiAttributeCapacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        TaxiHashString(hash, collector.entries[i].name.c_str());
        TaxiHashAttribute(hash, *collector.entries[i].attribute);
    }
    return hash;
}

int TaxiAttributeState_RosterSize(SimulationContext *context)
{
    TaxiRosterCollector collector = {};
    return CollectTaxiRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : -1;
}

int TaxiAttributeState_Capacity()
{
    return g_taxiAttributeCapacity;
}

bool TaxiAttributeState_IsKnownRoster(SimulationContext *context)
{
    // Seven canonical May 1999 identities cover all nine Levels (day/night
    // pairs share two rosters). The final value is the January public-source
    // fixture retained for deterministic CI.
    static const unsigned long long known[] = {
        8356819091171925193ull,
        14997410288666183479ull,
        10407405744231412933ull,
        9807800466153373862ull,
        18284905668689134823ull,
        17154522297671520673ull,
        3076718173379490250ull,
        2754184477989056894ull
    };
    const unsigned long long fingerprint =
        TaxiAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

bool TaxiAttributeState_CachesUnresolved(SimulationContext *context)
{
    TaxiRosterCollector collector = {};
    return CollectTaxiRoster(context, collector);
}
