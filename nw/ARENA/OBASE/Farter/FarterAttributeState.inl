#ifndef RR2NW_FARTER_ATTRIBUTE_STATE_INL
#define RR2NW_FARTER_ATTRIBUTE_STATE_INL

#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "obase/sound/WAVResourceState.h"

AttributeTableFarter __attrFarterTable;

namespace {

const unsigned long long kFarterHashOffset = 14695981039346656037ull;
const unsigned long long kFarterHashPrime = 1099511628211ull;

void FarterHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kFarterHashPrime;
    }
}

void FarterHashString(unsigned long long &hash, const char *value)
{
    FarterHashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

struct FarterRosterEntry
{
    std::string name;
    AttributeFarter *attribute;
};

struct FarterRosterCollector
{
    SimulationContext *context;
    std::vector<FarterRosterEntry> entries;
    bool valid;
};

bool CollectFarterRosterEntry(const KR_ObjectID object, void *user)
{
    FarterRosterCollector *collector =
        static_cast<FarterRosterCollector *>(user);
    const char *name = collector->context->searchObject(object);
    AttributeFarter *attribute = static_cast<AttributeFarter *>(
        __attrFarterTable.searchAttribute(object));
    if (name == NULL || attribute == NULL)
    {
        collector->valid = false;
        return false;
    }
    FarterRosterEntry entry = {name, attribute};
    collector->entries.push_back(entry);
    return true;
}

bool FarterRosterEntryLess(const FarterRosterEntry &left,
                           const FarterRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectFarterRoster(SimulationContext *context,
                         FarterRosterCollector &collector)
{
    if (context == NULL || __attrFarterTable.capacity() <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __attrFarterTable.userFind(CollectFarterRosterEntry, &collector);
    if (!collector.valid)
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              FarterRosterEntryLess);
    return true;
}

}  // namespace

AttributeFarter::AttributeFarter()
    : m_wav(NULL), m_ctsndID(ct_NULLID)
{
    std::strncpy(m_soundName, "", sizeof(ct_AttrStr) - 1);
    m_soundName[sizeof(ct_AttrStr) - 1] = 0;
    m_array[0].set("m_soundName", m_soundName);
    linkTable(m_array, 1);
}

void AttributeFarter::update(double)
{
    WAVObj *wav = NULL;
    if (m_soundName[0] == 0 ||
        !WAVResourceState_ResolveLoaded(context, m_soundName, &wav))
    {
        m_wav = NULL;
        m_ctsndID = ct_NULLID;
        return;
    }
    m_wav = wav;
    m_ctsndID = g_arena.searchSeanceClassTable("SoundObj");
}

AttributeTableFarter::AttributeTableFarter() : m_table(NULL)
{
    registerClass("FarterAttr");
}

void AttributeTableFarter::allocObjects(int objectQnty)
{
    m_table = new (std::nothrow) AttributeFarter[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void AttributeTableFarter::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *AttributeTableFarter::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "AttributeTableFarter::getObjectPTR");
    return &(m_table[index]);
}

void FarterAttributeState_Link()
{
}

unsigned long long FarterAttributeState_Fingerprint(
    SimulationContext *context)
{
    FarterRosterCollector collector = {};
    if (!CollectFarterRoster(context, collector))
        return 0;
    unsigned long long hash = kFarterHashOffset;
    const int capacity = __attrFarterTable.capacity();
    FarterHashBytes(hash, &capacity, sizeof(capacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        FarterHashString(hash, collector.entries[i].name.c_str());
        FarterHashString(hash, collector.entries[i].attribute->m_soundName);
    }
    return hash;
}

int FarterAttributeState_RosterSize(SimulationContext *context)
{
    FarterRosterCollector collector = {};
    return CollectFarterRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : -1;
}

int FarterAttributeState_Capacity()
{
    return __attrFarterTable.capacity();
}

bool FarterAttributeState_IsKnownRoster(SimulationContext *context)
{
    // The empty roster is shared by eight May 1999 Levels; Level.04D owns the
    // four factory/windmill sounds. The public CI fixture shares the empty
    // retail fingerprint.
    static const unsigned long long known[] = {
        10155668643424727455ull,
        5223394802105552525ull
    };
    const unsigned long long fingerprint =
        FarterAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

bool FarterAttributeState_CachesUnresolved(SimulationContext *context)
{
    FarterRosterCollector collector = {};
    if (!CollectFarterRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (collector.entries[i].attribute->m_wav != NULL ||
            collector.entries[i].attribute->m_ctsndID != ct_NULLID)
            return false;
    return true;
}

bool FarterAttributeState_ResolveReferences(SimulationContext *context)
{
    FarterRosterCollector collector = {};
    if (!CollectFarterRoster(context, collector))
        return false;
    std::vector<WAVObj *> waves(collector.entries.size(), NULL);
    const ct_ClassTableID soundTable =
        g_arena.searchSeanceClassTable("SoundObj");
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        const char *soundName = collector.entries[i].attribute->m_soundName;
        if (soundName[0] != 0 &&
            !WAVResourceState_ResolveLoaded(context, soundName, &waves[i]))
            return false;
    }
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeFarter *attribute = collector.entries[i].attribute;
        attribute->m_wav = waves[i];
        attribute->m_ctsndID = attribute->m_soundName[0] == 0
                                   ? ct_NULLID
                                   : soundTable;
    }
    return true;
}

bool FarterAttributeState_ReferencesResolved(SimulationContext *context)
{
    FarterRosterCollector collector = {};
    if (!CollectFarterRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeFarter *attribute = collector.entries[i].attribute;
        if (attribute->m_soundName[0] == 0)
        {
            if (attribute->m_wav != NULL ||
                attribute->m_ctsndID != ct_NULLID)
                return false;
            continue;
        }
        WAVObj *expected = NULL;
        if (!WAVResourceState_ResolveLoaded(context, attribute->m_soundName,
                                            &expected) ||
            attribute->m_wav != expected)
            return false;
    }
    return true;
}

bool FarterAttributeState_RuntimeReady(SimulationContext *context)
{
    if (!FarterAttributeState_ReferencesResolved(context))
        return false;
    FarterRosterCollector collector = {};
    if (!CollectFarterRoster(context, collector))
        return false;
    const ct_ClassTableID soundTable =
        g_arena.searchSeanceClassTable("SoundObj");
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeFarter *attribute = collector.entries[i].attribute;
        if (attribute->m_soundName[0] != 0 &&
            (soundTable == ct_NULLID || attribute->m_ctsndID != soundTable))
            return false;
    }
    return true;
}

unsigned long long FarterAttributeState_ReferenceFingerprint(
    SimulationContext *context)
{
    if (!FarterAttributeState_ReferencesResolved(context))
        return 0;
    FarterRosterCollector collector = {};
    if (!CollectFarterRoster(context, collector))
        return 0;
    unsigned long long hash = kFarterHashOffset;
    const int capacity = __attrFarterTable.capacity();
    FarterHashBytes(hash, &capacity, sizeof(capacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeFarter *attribute = collector.entries[i].attribute;
        FarterHashString(hash, collector.entries[i].name.c_str());
        FarterHashString(hash, attribute->m_soundName);
        const int hasWave = attribute->m_wav != NULL ? 1 : 0;
        FarterHashBytes(hash, &hasWave, sizeof(hasWave));
        const char *tableName =
            g_arena.searchSeanceClassTable(attribute->m_ctsndID);
        FarterHashString(hash, tableName == NULL ? "" : tableName);
    }
    return hash;
}

bool FarterAttributeState_IsKnownReferenceRoster(
    SimulationContext *context)
{
    static const unsigned long long known[] = {
        10155668643424727455ull,
        6949774498761611553ull
    };
    const unsigned long long fingerprint =
        FarterAttributeState_ReferenceFingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

#endif
