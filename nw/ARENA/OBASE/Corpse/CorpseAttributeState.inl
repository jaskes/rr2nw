#ifndef RR2NW_CORPSE_ATTRIBUTE_STATE_INL
#define RR2NW_CORPSE_ATTRIBUTE_STATE_INL

#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

AttributeTableCorpse __attrCorpseTable;

namespace {

const unsigned long long kCorpseHashOffset = 14695981039346656037ull;
const unsigned long long kCorpseHashPrime = 1099511628211ull;

void CorpseHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kCorpseHashPrime;
    }
}

void CorpseHashString(unsigned long long &hash, const char *value)
{
    CorpseHashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

void CorpseHashAttribute(unsigned long long &hash, AttributeCorpse &attr)
{
#define RR2NW_CORPSE_HASH(field) \
    CorpseHashBytes(hash, &attr.field, sizeof(attr.field))
    CorpseHashString(hash, attr.m_skinName);
    RR2NW_CORPSE_HASH(m_isBurning);
    CorpseHashString(hash, attr.m_smokerAttr);
    RR2NW_CORPSE_HASH(m_fireOffsetX);
    RR2NW_CORPSE_HASH(m_fireOffsetY);
    RR2NW_CORPSE_HASH(m_fireOffsetZ);
    RR2NW_CORPSE_HASH(m_minLifeTime);
    CorpseHashString(hash, attr.m_smokerTable);
    RR2NW_CORPSE_HASH(m_isSmoking);
    CorpseHashString(hash, attr.m_fireAttr);
    RR2NW_CORPSE_HASH(m_corpseOffsetX);
    RR2NW_CORPSE_HASH(m_corpseOffsetY);
    RR2NW_CORPSE_HASH(m_corpseOffsetZ);
#undef RR2NW_CORPSE_HASH
}

bool CorpseCachesAreUnresolved(AttributeCorpse &attr)
{
    return attr.m_cacheSkin == NULL && attr.m_skinID.isNUL() &&
           attr.m_smokerAttrID.isNUL() && attr.m_fireAttrID.isNUL() &&
           attr.m_smokerTableID == ct_NULLID;
}

struct CorpseRosterEntry
{
    std::string name;
    AttributeCorpse *attribute;
};

struct CorpseRosterCollector
{
    SimulationContext *context;
    std::vector<CorpseRosterEntry> entries;
    bool valid;
};

bool CollectCorpseRosterEntry(const KR_ObjectID object, void *user)
{
    CorpseRosterCollector *collector =
        static_cast<CorpseRosterCollector *>(user);
    const char *name = collector->context->searchObject(object);
    AttributeCorpse *attribute = static_cast<AttributeCorpse *>(
        __attrCorpseTable.searchAttribute(object));
    if (name == NULL || attribute == NULL ||
        !CorpseCachesAreUnresolved(*attribute))
    {
        collector->valid = false;
        return false;
    }
    CorpseRosterEntry entry = {name, attribute};
    collector->entries.push_back(entry);
    return true;
}

bool CorpseRosterEntryLess(const CorpseRosterEntry &left,
                           const CorpseRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectCorpseRoster(SimulationContext *context,
                         CorpseRosterCollector &collector)
{
    if (context == NULL || __attrCorpseTable.capacity() <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __attrCorpseTable.userFind(CollectCorpseRosterEntry, &collector);
    if (!collector.valid || collector.entries.empty())
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              CorpseRosterEntryLess);
    return true;
}

}  // namespace

AttributeCorpse::AttributeCorpse()
    : m_cacheSkin(NULL),
      m_skinID(KR_ObjectID::NUL()),
      m_smokerAttrID(KR_ObjectID::NUL()),
      m_fireAttrID(KR_ObjectID::NUL()),
      m_smokerTableID(ct_NULLID)
{
    std::strncpy(m_skinName, "sk.corpse.default", sizeof(ct_AttrStr) - 1);
    m_skinName[sizeof(ct_AttrStr) - 1] = 0;
    m_isBurning = 1;
    std::strncpy(m_smokerAttr, "Smoker.Attr.Corpse",
                 sizeof(ct_AttrStr) - 1);
    m_smokerAttr[sizeof(ct_AttrStr) - 1] = 0;
    m_fireOffsetX = 0;
    m_fireOffsetY = 1;
    m_fireOffsetZ = 0;
    m_minLifeTime = 30;
    std::strncpy(m_smokerTable, "DynSmoker", sizeof(ct_AttrStr) - 1);
    m_smokerTable[sizeof(ct_AttrStr) - 1] = 0;
    m_isSmoking = 1;
    std::strncpy(m_fireAttr, "Smoker.Attr.Fire.Corpse",
                 sizeof(ct_AttrStr) - 1);
    m_fireAttr[sizeof(ct_AttrStr) - 1] = 0;
    m_corpseOffsetX = 0;
    m_corpseOffsetY = -1;
    m_corpseOffsetZ = 0;

    m_array[0].set("m_skinName", m_skinName);
    m_array[1].set("m_isBurning", m_isBurning);
    m_array[2].set("m_smokerAttr", m_smokerAttr);
    m_array[3].set("m_fireOffsetX", m_fireOffsetX);
    m_array[4].set("m_fireOffsetY", m_fireOffsetY);
    m_array[5].set("m_fireOffsetZ", m_fireOffsetZ);
    m_array[6].set("m_minLifeTime", m_minLifeTime);
    m_array[7].set("m_smokerTable", m_smokerTable);
    m_array[8].set("m_isSmoking", m_isSmoking);
    m_array[9].set("m_fireAttr", m_fireAttr);
    m_array[10].set("m_corpseOffsetX", m_corpseOffsetX);
    m_array[11].set("m_corpseOffsetY", m_corpseOffsetY);
    m_array[12].set("m_corpseOffsetZ", m_corpseOffsetZ);
    linkTable(m_array, 13);
}

void AttributeCorpse::update(double ts)
{
    m_cacheSkin = NULL;
    m_skinID = context->searchObject(m_skinName);
    if (!m_skinID.isNUL())
    {
        KR_Event event;
        event.timeStamp = ts;
        event.label = sk_EV_QUERY_MODEL_PTR;
        event.destination = m_skinID;
        context->sendEventNow(event);
        if (event.label == sk_EV_QUERY_MODEL_PTR_OK)
            event.data.open(EDO_READ)
                      .get(&m_cacheSkin, sizeof(void *))
                      .close();
    }
    m_smokerTableID = g_arena.searchSeanceClassTable(m_smokerTable);
    m_smokerAttrID = m_isSmoking ? context->searchObject(m_smokerAttr)
                                 : KR_ObjectID::NUL();
    m_fireAttrID = m_isBurning ? context->searchObject(m_fireAttr)
                               : KR_ObjectID::NUL();
}

AttributeTableCorpse::AttributeTableCorpse() : m_table(NULL)
{
    registerClass("CorpseAttr");
}

void AttributeTableCorpse::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeCorpse[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableCorpse::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableCorpse::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableCorpse::getObjectPTR");
    return &(m_table[index]);
}

void CorpseAttributeState_Link()
{
}

unsigned long long CorpseAttributeState_Fingerprint(
    SimulationContext *context)
{
    CorpseRosterCollector collector = {};
    if (!CollectCorpseRoster(context, collector))
        return 0;
    unsigned long long hash = kCorpseHashOffset;
    const int capacity = __attrCorpseTable.capacity();
    CorpseHashBytes(hash, &capacity, sizeof(capacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        CorpseHashString(hash, collector.entries[i].name.c_str());
        CorpseHashAttribute(hash, *collector.entries[i].attribute);
    }
    return hash;
}

int CorpseAttributeState_RosterSize(SimulationContext *context)
{
    CorpseRosterCollector collector = {};
    return CollectCorpseRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : 0;
}

int CorpseAttributeState_Capacity()
{
    return __attrCorpseTable.capacity();
}

bool CorpseAttributeState_IsKnownRoster(SimulationContext *context)
{
    // The first nine fingerprints are the nine May 1999 Levels. The final
    // value is the smaller public Level.03N source fixture used by CI.
    static const unsigned long long known[] = {
        4638798848437234605ull,
        11155175483122396646ull,
        10086583070732431402ull,
        13316459317288169687ull,
        16670294511977263743ull,
        17707082791514824439ull,
        16181431137439848755ull,
        9116715007723704035ull,
        5409415849671832200ull,
        13616153644131130817ull
    };
    const unsigned long long fingerprint =
        CorpseAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

#endif
