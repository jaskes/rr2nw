#include "VehicleAttributeState.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

class CGRPanel;
#include "h/vehicle.h"
#include "kernel/h/context.h"

namespace {

const unsigned long long kVehicleHashOffset = 14695981039346656037ull;
const unsigned long long kVehicleHashPrime = 1099511628211ull;
int g_vehicleAttributeCapacity = 0;

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
