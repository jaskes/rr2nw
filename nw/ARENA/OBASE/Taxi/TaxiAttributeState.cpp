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

struct TaxiResolvedReferences
{
    CViewObjectModel *skin;
    ct_ClassTableID corpseTable;
    int corpseAttr;
    KR_ObjectID skinID;
    KR_ObjectID vehicleAttrID;
};

bool ResolveTaxiReferences(SimulationContext *context,
                           AttributeTaxi &attribute,
                           TaxiResolvedReferences &references)
{
    references.skin = NULL;
    references.corpseTable = ct_NULLID;
    references.corpseAttr = ct_NULLID;
    references.skinID = KR_ObjectID::NUL();
    references.vehicleAttrID = KR_ObjectID::NUL();
    if (context == NULL)
        return false;

    const ct_ClassTableID vehicleAttrTable =
        g_arena.searchSeanceClassTable("VehicleAttr");
    references.vehicleAttrID =
        context->searchObject(attribute.m_attrForVehicleName);
    if (vehicleAttrTable == ct_NULLID ||
        references.vehicleAttrID.isNUL() ||
        g_arena.getAttributeIndex(vehicleAttrTable,
                                  references.vehicleAttrID) == ct_NULLID)
        return false;

    references.corpseTable =
        g_arena.searchSeanceClassTable("Corpse");
    const ct_ClassTableID corpseAttrTable =
        g_arena.searchSeanceClassTable("CorpseAttr");
    KR_ObjectID corpseAttrID =
        context->searchObject(attribute.m_corpseAttrName);
    if (references.corpseTable == ct_NULLID ||
        corpseAttrTable == ct_NULLID || corpseAttrID.isNUL())
        return false;
    references.corpseAttr =
        g_arena.getAttributeIndex(corpseAttrTable, corpseAttrID);
    if (references.corpseAttr == ct_NULLID)
        return false;

    // Resolve the loaded model last through the original object/event
    // boundary. This keeps the attribute owner independent from the concrete
    // Skin table while validating the exact response before reading it. A
    // source-only fixture therefore proves that successful VehicleAttr/Corpse
    // preflight never leaks a partial commit when the visual dependency is
    // unavailable.
    references.skinID = context->searchObject(attribute.m_skinName);
    if (references.skinID.isNUL())
        return false;
    KR_Event event;
    event.label = sk_EV_QUERY_MODEL_PTR;
    event.destination = references.skinID;
    context->sendEventNow(event);
    if (event.label != sk_EV_QUERY_MODEL_PTR_OK ||
        event.data.size() != static_cast<int>(sizeof(references.skin)))
        return false;
    event.data.open(EDO_READ)
              .get(&references.skin, sizeof(references.skin))
              .close();
    return references.skin != NULL;
}

void CommitTaxiReferences(AttributeTaxi &attribute,
                          const TaxiResolvedReferences &references)
{
    attribute.m_cacheSkin = references.skin;
    attribute.m_cacheCorpseTable = references.corpseTable;
    attribute.m_cacheCorpseAttr = references.corpseAttr;
    attribute.m_skinID = references.skinID;
    attribute.m_attrForVehicle = references.vehicleAttrID;
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
    if (name == NULL || attribute == NULL)
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
    (void)ts;
    TaxiResolvedReferences references = {};
    if (ResolveTaxiReferences(context, *this, references))
        CommitTaxiReferences(*this, references);
    else
    {
        m_cacheSkin = NULL;
        m_cacheCorpseTable = ct_NULLID;
        m_cacheCorpseAttr = ct_NULLID;
        m_skinID = KR_ObjectID::NUL();
        m_attrForVehicle = KR_ObjectID::NUL();
    }
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
    if (!CollectTaxiRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (!TaxiCachesAreUnresolved(*collector.entries[i].attribute))
            return false;
    return true;
}

bool TaxiAttributeState_ResolveReferences(SimulationContext *context)
{
    TaxiRosterCollector collector = {};
    if (!CollectTaxiRoster(context, collector))
        return false;
    std::vector<TaxiResolvedReferences> references(collector.entries.size());
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (!ResolveTaxiReferences(context, *collector.entries[i].attribute,
                                   references[i]))
            return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        CommitTaxiReferences(*collector.entries[i].attribute,
                             references[i]);
    return true;
}

bool TaxiAttributeState_ReferencesResolved(SimulationContext *context)
{
    TaxiRosterCollector collector = {};
    if (!CollectTaxiRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeTaxi *attribute = collector.entries[i].attribute;
        TaxiResolvedReferences expected = {};
        if (!ResolveTaxiReferences(context, *attribute, expected) ||
            attribute->m_cacheSkin != expected.skin ||
            attribute->m_cacheCorpseTable != expected.corpseTable ||
            attribute->m_cacheCorpseAttr != expected.corpseAttr ||
            attribute->m_skinID != expected.skinID ||
            attribute->m_attrForVehicle != expected.vehicleAttrID)
            return false;
    }
    return true;
}

unsigned long long TaxiAttributeState_ReferenceFingerprint(
    SimulationContext *context)
{
    if (!TaxiAttributeState_ReferencesResolved(context))
        return 0;
    TaxiRosterCollector collector = {};
    if (!CollectTaxiRoster(context, collector))
        return 0;
    unsigned long long hash = kTaxiHashOffset;
    TaxiHashBytes(hash, &g_taxiAttributeCapacity,
                  sizeof(g_taxiAttributeCapacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeTaxi *attribute = collector.entries[i].attribute;
        TaxiHashString(hash, collector.entries[i].name.c_str());
        TaxiHashAttribute(hash, *attribute);
        const char *skinName = context->searchObject(attribute->m_skinID);
        const char *vehicleName =
            context->searchObject(attribute->m_attrForVehicle);
        const char *corpseTableName = g_arena.searchSeanceClassTable(
            attribute->m_cacheCorpseTable);
        TaxiHashString(hash, skinName == NULL ? "" : skinName);
        TaxiHashString(hash, vehicleName == NULL ? "" : vehicleName);
        TaxiHashString(hash,
                       corpseTableName == NULL ? "" : corpseTableName);
        TaxiHashString(hash, attribute->m_corpseAttrName);
    }
    return hash;
}

bool TaxiAttributeState_IsKnownReferenceRoster(
    SimulationContext *context)
{
    // Seven canonical May identities cover all nine Levels. The public
    // source-only fixture is not admitted because it intentionally owns no
    // loaded Skin models.
    static const unsigned long long known[] = {
        9175343944702536723ull,
        17235045383519457016ull,
        8799760472968283833ull,
        7830645074479408122ull,
        5874980028233070888ull,
        1305593298262665297ull,
        4383146699719690126ull
    };
    const unsigned long long fingerprint =
        TaxiAttributeState_ReferenceFingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}
