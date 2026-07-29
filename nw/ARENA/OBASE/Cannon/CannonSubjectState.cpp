#include "CannonSubjectState.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#define LAST_H__VIEW
#include "game.h"
#include "Cannon.h"
#include "kernel/h/context.h"
#include "storage/h/subject.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const unsigned long long kAbsentAttributeFingerprint =
    0x43414e4e41545452ull;
const unsigned long long kAbsentSubjectFingerprint =
    0x43414e4e5355424aull;
int g_attributeCapacity = 0;
int g_subjectCapacity = 0;

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
    if (value == NULL)
        value = "";
    HashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

struct Roster
{
    SimulationContext *context;
    std::vector<std::string> names;
    bool valid;
};

bool CollectName(const KR_ObjectID object, void *user)
{
    Roster *roster = static_cast<Roster *>(user);
    const char *name = roster->context->searchObject(object);
    if (name == NULL)
    {
        roster->valid = false;
        return false;
    }
    roster->names.push_back(name);
    return true;
}

bool CollectTable(SimulationContext *context, const char *name,
                  Roster &roster)
{
    roster.context = context;
    roster.valid = context != NULL;
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID : g_arena.searchSeanceClassTable(name);
    if (table == ct_NULLID)
        return false;
    g_arena.userFind(table, CollectName, &roster);
    std::sort(roster.names.begin(), roster.names.end());
    return roster.valid;
}

int Count(SimulationContext *context, const char *name, int capacity)
{
    Roster roster = {};
    if (!CollectTable(context, name, roster))
        return capacity == 0 ? 0 : -1;
    return static_cast<int>(roster.names.size());
}

unsigned long long Fingerprint(SimulationContext *context, const char *name,
                               int capacity,
                               unsigned long long absentFingerprint)
{
    Roster roster = {};
    if (!CollectTable(context, name, roster))
        return capacity == 0 ? absentFingerprint : 0;
    unsigned long long hash = kHashOffset;
    HashBytes(hash, &capacity, sizeof(capacity));
    for (std::size_t i = 0; i < roster.names.size(); ++i)
        HashString(hash, roster.names[i].c_str());
    return hash;
}

}  // namespace

void CannonSubjectState_Link()
{
    Cannon linkAnchor;
    (void)linkAnchor;
}

void CannonSubjectState_SetExpectedCapacities(int attributeCapacity,
                                              int subjectCapacity)
{
    g_attributeCapacity = attributeCapacity > 0 ? attributeCapacity : 0;
    g_subjectCapacity = subjectCapacity > 0 ? subjectCapacity : 0;
}

int CannonSubjectState_AttributeCapacity() { return g_attributeCapacity; }
int CannonSubjectState_SubjectCapacity() { return g_subjectCapacity; }

int CannonSubjectState_AttributeCount(SimulationContext *context)
{
    return Count(context, "CannonAttr", g_attributeCapacity);
}

int CannonSubjectState_LiveCount(SimulationContext *context)
{
    return Count(context, "Cannon", g_subjectCapacity);
}

unsigned long long CannonSubjectState_AttributeFingerprint(
    SimulationContext *context)
{
    return Fingerprint(context, "CannonAttr", g_attributeCapacity,
                       kAbsentAttributeFingerprint);
}

unsigned long long CannonSubjectState_SubjectFingerprint(
    SimulationContext *context)
{
    return Fingerprint(context, "Cannon", g_subjectCapacity,
                       kAbsentSubjectFingerprint);
}
