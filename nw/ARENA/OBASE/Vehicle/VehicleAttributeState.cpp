#include "VehicleAttributeState.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "graph.h"
#include "h/vehicle.h"
#include "kernel/h/context.h"
#include "obase/bullet/BulletAttributeState.h"

namespace {

const unsigned long long kVehicleHashOffset = 14695981039346656037ull;
const unsigned long long kVehicleHashPrime = 1099511628211ull;
int g_vehicleAttributeCapacity = 0;
char g_vehicleLastError[192] = {};

struct VehicleReferenceState
{
    SimulationContext *context;
    unsigned long long fingerprint;
    bool ready;
};

VehicleReferenceState g_vehicleReferenceState = {};

void SetVehicleLastError(const char *kind, const char *name)
{
    std::snprintf(g_vehicleLastError, sizeof(g_vehicleLastError),
                  "Vehicle dependency missing: %s <%s>", kind,
                  name == NULL ? "" : name);
}

void VehicleHashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kVehicleHashPrime;
    }
}

void VehicleHashString(unsigned long long &hash, const char *value)
{
    VehicleHashBytes(hash, value,
                     static_cast<int>(std::strlen(value)) + 1);
}

void VehicleHashVector(unsigned long long &hash, const CFVector3 &value)
{
    VehicleHashBytes(hash, &value.x, sizeof(value.x));
    VehicleHashBytes(hash, &value.y, sizeof(value.y));
    VehicleHashBytes(hash, &value.z, sizeof(value.z));
}

void VehicleHashAttribute(unsigned long long &hash,
                          const AttributeVehicle &attribute)
{
#define RR2NW_VEHICLE_HASH(field) \
    VehicleHashBytes(hash, &attribute.field, sizeof(attribute.field))
    RR2NW_VEHICLE_HASH(m_type);
    RR2NW_VEHICLE_HASH(m_timeInc);
    RR2NW_VEHICLE_HASH(m_radius);
    RR2NW_VEHICLE_HASH(m_centerOffsetY);
    VehicleHashString(hash, attribute.m_panelName);
    VehicleHashString(hash, attribute.m_dynamic);
    RR2NW_VEHICLE_HASH(m_radius0);
    RR2NW_VEHICLE_HASH(m_outOfsX);
    RR2NW_VEHICLE_HASH(m_outOfsZ);
    RR2NW_VEHICLE_HASH(m_bornY);
    VehicleHashString(hash, attribute.m_taxiName);
    VehicleHashString(hash, attribute.m_soundName);
    VehicleHashVector(hash, attribute.m_cannonOffset);
    VehicleHashVector(hash, attribute.m_hAxis);
    VehicleHashString(hash, attribute.m_bulletAttrName);
    RR2NW_VEHICLE_HASH(m_bulletSlipTime);
    VehicleHashString(hash, attribute.m_bulletSecAttrName);
    VehicleHashString(hash, attribute.m_shootSecAttrName);
    RR2NW_VEHICLE_HASH(m_bulletSecSlipTime);
    RR2NW_VEHICLE_HASH(m_taxiRotateSpeed);
    RR2NW_VEHICLE_HASH(m_taxiMoveSpeed);
    RR2NW_VEHICLE_HASH(m_engineStartMaxPitch);
    RR2NW_VEHICLE_HASH(m_engineStartRecalcTime);
    RR2NW_VEHICLE_HASH(m_power);
    RR2NW_VEHICLE_HASH(m_soundMinPitch);
    RR2NW_VEHICLE_HASH(m_soundMaxPitch);
    VehicleHashString(hash, attribute.m_outFlicName);
#undef RR2NW_VEHICLE_HASH
}

bool VehicleCachesAreUnresolved(AttributeVehicle &attribute)
{
    return attribute.m_panel == NULL && attribute.m_taxiID.isNUL() &&
           attribute.m_bulletTable == ct_NULLID &&
           attribute.m_bulletAttrIndex == -1 &&
           attribute.m_bulletSecAttrIndex == -1;
}

void ClearVehicleCaches(AttributeVehicle &attribute, bool releasePanel)
{
    if (releasePanel)
        delete attribute.m_panel;
    attribute.m_panel = NULL;
    attribute.m_taxiID = KR_ObjectID::NUL();
    attribute.m_bulletTable = ct_NULLID;
    attribute.m_bulletAttrIndex = -1;
    attribute.m_bulletSecAttrIndex = -1;
}

struct VehicleResolvedReferences
{
    CGRPanel *panel;
    KR_ObjectID taxi;
    ct_ClassTableID bulletTable;
    int primaryBullet;
    int secondaryBullet;

    VehicleResolvedReferences()
        : panel(NULL), taxi(KR_ObjectID::NUL()),
          bulletTable(ct_NULLID), primaryBullet(-1),
          secondaryBullet(-1)
    {
    }
};

bool ResolveAttributeIndex(SimulationContext *context,
                           ct_ClassTableID table, const char *name,
                           int *index)
{
    if (index == NULL)
        return false;
    *index = -1;
    if (name == NULL || name[0] == 0)
        return true;
    KR_ObjectID object = context->searchObject(name);
    if (object.isNUL())
        return false;
    *index = g_arena.getAttributeIndex(table, object);
    return *index != ct_NULLID;
}

bool ResolveNonPanelReferences(SimulationContext *context,
                               AttributeVehicle &attribute,
                               ct_ClassTableID taxiAttributeTable,
                               ct_ClassTableID bulletTable,
                               ct_ClassTableID bulletAttributeTable,
                               VehicleResolvedReferences &references)
{
    references.bulletTable = bulletTable;
    if (attribute.m_taxiName[0] != 0)
    {
        references.taxi = context->searchObject(attribute.m_taxiName);
        if (references.taxi.isNUL() ||
            g_arena.getAttributeIndex(taxiAttributeTable,
                                      references.taxi) == ct_NULLID)
        {
            SetVehicleLastError("TaxiAttr", attribute.m_taxiName);
            return false;
        }
    }
    else if (attribute.m_type != 0)
    {
        SetVehicleLastError("TaxiAttr", attribute.m_taxiName);
        return false;
    }
    if (!ResolveAttributeIndex(context, bulletAttributeTable,
                               attribute.m_bulletAttrName,
                               &references.primaryBullet))
    {
        SetVehicleLastError("BulletAttr", attribute.m_bulletAttrName);
        return false;
    }
    if (!ResolveAttributeIndex(context, bulletAttributeTable,
                               attribute.m_bulletSecAttrName,
                               &references.secondaryBullet))
    {
        SetVehicleLastError("BulletAttr", attribute.m_bulletSecAttrName);
        return false;
    }
    return true;
}

void ReleaseResolvedPanels(std::vector<VehicleResolvedReferences> &references)
{
    for (std::size_t i = 0; i < references.size(); ++i)
    {
        delete references[i].panel;
        references[i].panel = NULL;
    }
}

void CommitVehicleReferences(
    AttributeVehicle &attribute,
    VehicleResolvedReferences &references)
{
    attribute.m_panel = references.panel;
    references.panel = NULL;
    attribute.m_taxiID = references.taxi;
    attribute.m_bulletTable = references.bulletTable;
    attribute.m_bulletAttrIndex = references.primaryBullet;
    attribute.m_bulletSecAttrIndex = references.secondaryBullet;
}

struct VehicleRosterEntry
{
    std::string name;
    AttributeVehicle *attribute;
};

struct VehicleRosterCollector
{
    SimulationContext *context;
    std::vector<VehicleRosterEntry> entries;
    bool valid;
};

bool CollectVehicleRosterEntry(const KR_ObjectID object, void *user)
{
    VehicleRosterCollector *collector =
        static_cast<VehicleRosterCollector *>(user);
    const char *name = collector->context->searchObject(object);
    AttributeVehicle *attribute = static_cast<AttributeVehicle *>(
        __attrVehicleTable.searchAttribute(object));
    if (name == NULL || attribute == NULL)
    {
        collector->valid = false;
        return false;
    }
    VehicleRosterEntry entry = {name, attribute};
    collector->entries.push_back(entry);
    return true;
}

bool VehicleRosterEntryLess(const VehicleRosterEntry &left,
                            const VehicleRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectVehicleRoster(SimulationContext *context,
                          VehicleRosterCollector &collector)
{
    if (context == NULL || g_vehicleAttributeCapacity <= 0)
        return false;
    collector.context = context;
    collector.valid = true;
    __attrVehicleTable.userFind(CollectVehicleRosterEntry, &collector);
    if (!collector.valid || collector.entries.empty())
        return false;
    std::sort(collector.entries.begin(), collector.entries.end(),
              VehicleRosterEntryLess);
    return true;
}

}  // namespace

void VehicleAttributeState_Link()
{
}

void VehicleAttributeState_SetCapacity(int capacity)
{
    g_vehicleAttributeCapacity = capacity > 0 ? capacity : 0;
    if (g_vehicleAttributeCapacity == 0)
    {
        g_vehicleReferenceState.context = NULL;
        g_vehicleReferenceState.fingerprint = 0;
        g_vehicleReferenceState.ready = false;
    }
}

unsigned long long VehicleAttributeState_Fingerprint(
    SimulationContext *context)
{
    VehicleRosterCollector collector = {};
    if (!CollectVehicleRoster(context, collector))
        return 0;
    unsigned long long hash = kVehicleHashOffset;
    VehicleHashBytes(hash, &g_vehicleAttributeCapacity,
                     sizeof(g_vehicleAttributeCapacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        VehicleHashString(hash, collector.entries[i].name.c_str());
        VehicleHashAttribute(hash, *collector.entries[i].attribute);
    }
    return hash;
}

int VehicleAttributeState_RosterSize(SimulationContext *context)
{
    VehicleRosterCollector collector = {};
    return CollectVehicleRoster(context, collector)
               ? static_cast<int>(collector.entries.size())
               : -1;
}

int VehicleAttributeState_Capacity()
{
    return g_vehicleAttributeCapacity;
}

bool VehicleAttributeState_IsKnownRoster(SimulationContext *context)
{
    // Seven canonical May identities cover all nine Levels; day/night pairs
    // share the first two rosters. The January public fixture is appended
    // after its hermetic execution is measured below the retail gate.
    static const unsigned long long known[] = {
        4820723311424334637ull,
        11460174472260063041ull,
        1438011491898955296ull,
        4531502674543477175ull,
        10038446168503949478ull,
        1803529506760166992ull,
        13479800410678345611ull,
        14035231734239706959ull
    };
    const unsigned long long fingerprint =
        VehicleAttributeState_Fingerprint(context);
    for (int i = 0; i < static_cast<int>(sizeof(known) / sizeof(known[0]));
         ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

bool VehicleAttributeState_CachesUnresolved(SimulationContext *context)
{
    VehicleRosterCollector collector = {};
    if (!CollectVehicleRoster(context, collector))
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (!VehicleCachesAreUnresolved(*collector.entries[i].attribute))
            return false;
    return true;
}

bool VehicleAttributeState_ProbeReferenceAtomicity(
    SimulationContext *context)
{
    VehicleRosterCollector collector = {};
    if (!CollectVehicleRoster(context, collector) ||
        !VehicleAttributeState_CachesUnresolved(context))
        return false;
    AttributeVehicle *probe = NULL;
    char *field = NULL;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeVehicle *candidate = collector.entries[i].attribute;
        if (candidate != NULL && candidate->m_panelName[0] != 0)
        {
            probe = candidate;
            field = candidate->m_panelName;
        }
    }
    if (field == NULL)
        for (std::size_t i = 0; i < collector.entries.size(); ++i)
        {
            AttributeVehicle *candidate = collector.entries[i].attribute;
            if (candidate != NULL && candidate->m_bulletSecAttrName[0] != 0)
            {
                probe = candidate;
                field = candidate->m_bulletSecAttrName;
            }
        }
    if (field == NULL || probe == NULL)
        return false;
    const unsigned long long before =
        VehicleAttributeState_Fingerprint(context);
    ct_AttrStr saved = {};
    std::memcpy(saved, field, sizeof(saved));
    std::strncpy(field, "Vehicle.Missing.Reference.Atomicity.Probe",
                 sizeof(ct_AttrStr) - 1);
    field[sizeof(ct_AttrStr) - 1] = 0;
    const bool rejected =
        !VehicleAttributeState_ResolveReferences(context) &&
        VehicleAttributeState_CachesUnresolved(context);
    std::memcpy(field, saved, sizeof(saved));
    return rejected && before != 0 &&
           VehicleAttributeState_Fingerprint(context) == before &&
           VehicleAttributeState_CachesUnresolved(context);
}

bool VehicleAttributeState_ResolveReferences(SimulationContext *context)
{
    g_vehicleLastError[0] = 0;
    if (g_vehicleReferenceState.ready)
        return VehicleAttributeState_ReferencesResolved(context);
    VehicleRosterCollector collector = {};
    if (context == NULL || g_arena.getContext() != context ||
        !CollectVehicleRoster(context, collector) ||
        !VehicleAttributeState_CachesUnresolved(context))
    {
        SetVehicleLastError("roster/cache state", "VehicleAttr");
        return false;
    }
    const ct_ClassTableID taxiAttributeTable =
        g_arena.searchSeanceClassTable("TaxiAttr");
    const ct_ClassTableID bulletTable =
        g_arena.searchSeanceClassTable("Bullet");
    const ct_ClassTableID bulletAttributeTable =
        g_arena.searchSeanceClassTable("BulletAttr");
    if (taxiAttributeTable == ct_NULLID)
    {
        SetVehicleLastError("table", "TaxiAttr");
        return false;
    }
    if (bulletTable == ct_NULLID)
    {
        SetVehicleLastError("table", "Bullet");
        return false;
    }
    if (bulletAttributeTable == ct_NULLID)
    {
        SetVehicleLastError("table", "BulletAttr");
        return false;
    }

    std::vector<VehicleResolvedReferences> references(
        collector.entries.size());
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        if (!ResolveNonPanelReferences(
                context, *collector.entries[i].attribute,
                taxiAttributeTable, bulletTable, bulletAttributeTable,
                references[i]))
            return false;

    try
    {
        for (std::size_t i = 0; i < collector.entries.size(); ++i)
        {
            AttributeVehicle &attribute =
                *collector.entries[i].attribute;
            if (attribute.m_panelName[0] == 0)
                continue;
            references[i].panel =
                new (std::nothrow) CGRPanel(attribute.m_panelName);
            if (references[i].panel == NULL)
            {
                ReleaseResolvedPanels(references);
                SetVehicleLastError("Panel allocation",
                                    attribute.m_panelName);
                return false;
            }
            references[i].panel->SetResolution(_gr_nScreenWidth,
                                                _gr_nScreenHeight);
            if (!references[i].panel->IsReady())
            {
                ReleaseResolvedPanels(references);
                SetVehicleLastError("Panel/resolution",
                                    attribute.m_panelName);
                return false;
            }
        }
    }
    catch (...)
    {
        ReleaseResolvedPanels(references);
        SetVehicleLastError("Panel allocation", "exception");
        return false;
    }

    for (std::size_t i = 0; i < collector.entries.size(); ++i)
        CommitVehicleReferences(*collector.entries[i].attribute,
                                references[i]);
    g_vehicleReferenceState.context = context;
    g_vehicleReferenceState.fingerprint = 0;
    g_vehicleReferenceState.ready = true;
    g_vehicleReferenceState.fingerprint =
        VehicleAttributeState_ReferenceFingerprint(context);
    if (g_vehicleReferenceState.fingerprint != 0)
        return true;
    VehicleAttributeState_ClearReferences(context);
    SetVehicleLastError("committed reference validation", "VehicleAttr");
    return false;
}

bool VehicleAttributeState_ReferencesResolved(SimulationContext *context)
{
    if (!g_vehicleReferenceState.ready || context == NULL ||
        g_vehicleReferenceState.context != context ||
        g_arena.getContext() != context)
        return false;
    VehicleRosterCollector collector = {};
    if (!CollectVehicleRoster(context, collector))
        return false;
    const ct_ClassTableID taxiAttributeTable =
        g_arena.searchSeanceClassTable("TaxiAttr");
    const ct_ClassTableID bulletTable =
        g_arena.searchSeanceClassTable("Bullet");
    const ct_ClassTableID bulletAttributeTable =
        g_arena.searchSeanceClassTable("BulletAttr");
    if (taxiAttributeTable == ct_NULLID || bulletTable == ct_NULLID ||
        bulletAttributeTable == ct_NULLID)
        return false;
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeVehicle &attribute = *collector.entries[i].attribute;
        VehicleResolvedReferences expected;
        if (!ResolveNonPanelReferences(
                context, attribute, taxiAttributeTable, bulletTable,
                bulletAttributeTable, expected) ||
            attribute.m_taxiID != expected.taxi ||
            attribute.m_bulletTable != expected.bulletTable ||
            attribute.m_bulletAttrIndex != expected.primaryBullet ||
            attribute.m_bulletSecAttrIndex != expected.secondaryBullet)
            return false;
        if ((attribute.m_panelName[0] == 0 && attribute.m_panel != NULL) ||
            (attribute.m_panelName[0] != 0 &&
             (attribute.m_panel == NULL || !attribute.m_panel->IsReady())))
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (attribute.m_panel != NULL &&
                attribute.m_panel == collector.entries[j].attribute->m_panel)
                return false;
    }
    return true;
}

unsigned long long VehicleAttributeState_ReferenceFingerprint(
    SimulationContext *context)
{
    if (!VehicleAttributeState_ReferencesResolved(context))
        return 0;
    VehicleRosterCollector collector = {};
    if (!CollectVehicleRoster(context, collector))
        return 0;
    unsigned long long hash = kVehicleHashOffset;
    VehicleHashBytes(hash, &g_vehicleAttributeCapacity,
                     sizeof(g_vehicleAttributeCapacity));
    for (std::size_t i = 0; i < collector.entries.size(); ++i)
    {
        AttributeVehicle &attribute = *collector.entries[i].attribute;
        VehicleHashString(hash, collector.entries[i].name.c_str());
        VehicleHashAttribute(hash, attribute);
        const int panelReady = attribute.m_panel != NULL ? 1 : 0;
        VehicleHashBytes(hash, &panelReady, sizeof(panelReady));
        const char *taxiName = attribute.m_taxiID.isNUL()
                                   ? ""
                                   : context->searchObject(
                                         attribute.m_taxiID);
        const char *bulletTableName =
            g_arena.searchSeanceClassTable(attribute.m_bulletTable);
        VehicleHashString(hash, taxiName == NULL ? "" : taxiName);
        VehicleHashString(hash,
                          bulletTableName == NULL ? "" : bulletTableName);
        AttributeBullet *primary = NULL;
        AttributeBullet *secondary = NULL;
        if (attribute.m_bulletAttrIndex != -1 &&
            !BulletAttributeState_ResolveEncodedIndex(
                context, attribute.m_bulletAttrIndex, &primary))
            return 0;
        if (attribute.m_bulletSecAttrIndex != -1 &&
            !BulletAttributeState_ResolveEncodedIndex(
                context, attribute.m_bulletSecAttrIndex, &secondary))
            return 0;
        const char *primaryName =
            primary == NULL ? "" : context->searchObject(
                                       primary->getObjectID());
        const char *secondaryName =
            secondary == NULL ? "" : context->searchObject(
                                         secondary->getObjectID());
        VehicleHashString(hash, primaryName == NULL ? "" : primaryName);
        VehicleHashString(hash,
                          secondaryName == NULL ? "" : secondaryName);
    }
    return hash;
}

bool VehicleAttributeState_IsKnownReferenceRoster(
    SimulationContext *context)
{
    // Seven May identities cover all nine Levels; 01D/01N and 02D/02N share
    // their complete semantic graphs. The last value is the hermetic January
    // fixture, whose exact roster intentionally owns no Panel files.
    static const unsigned long long known[] = {
        11147578212364682483ull,
        8581060582414102617ull,
        14583411795748371463ull,
        11044825111055254158ull,
        972386879584597554ull,
        4619298710525903342ull,
        12337669689485639293ull,
        9664253753635626231ull
    };
    if (!VehicleAttributeState_IsKnownRoster(context))
        return false;
    const unsigned long long fingerprint =
        VehicleAttributeState_ReferenceFingerprint(context);
    for (std::size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
        if (fingerprint == known[i])
            return true;
    return false;
}

void VehicleAttributeState_ClearReferences(SimulationContext *context)
{
    if (context == NULL)
        context = g_vehicleReferenceState.context;
    VehicleRosterCollector collector = {};
    if (CollectVehicleRoster(context, collector))
        for (std::size_t i = 0; i < collector.entries.size(); ++i)
            ClearVehicleCaches(*collector.entries[i].attribute, true);
    g_vehicleReferenceState.context = NULL;
    g_vehicleReferenceState.fingerprint = 0;
    g_vehicleReferenceState.ready = false;
}

const char *VehicleAttributeState_LastError()
{
    return g_vehicleLastError;
}
