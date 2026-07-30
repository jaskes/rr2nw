#include "CorpseSubjectState.h"

#include <cstring>

#include "Corpse.h"
#include "kernel/h/context.h"
#include "storage/h/subject.h"

namespace {

const unsigned long long kCorpseSubjectHashOffset =
    14695981039346656037ull;
const unsigned long long kCorpseSubjectHashPrime = 1099511628211ull;
int g_corpseSubjectCapacity = 0;

// Corpse.cpp owns the actual class-table registration. Keeping this external
// member-function reference live makes the static-library linker retain that
// translation unit without constructing a gameplay subject.
typedef void (Corpse::*CorpseLinkMethod)();
CorpseLinkMethod g_corpseLinkMethod = &Corpse::DestroyMe;

void CorpseSubjectHashBytes(unsigned long long &hash,
                            const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kCorpseSubjectHashPrime;
    }
}

void CorpseSubjectHashString(unsigned long long &hash, const char *value)
{
    CorpseSubjectHashBytes(hash, value,
                           static_cast<int>(std::strlen(value)) + 1);
}

bool CountCorpseSubject(const KR_ObjectID, void *user)
{
    int *count = static_cast<int *>(user);
    ++*count;
    return true;
}

bool CollectCorpseSubject(const KR_ObjectID object, void *user)
{
    std::vector<KR_ObjectID> *objects =
        static_cast<std::vector<KR_ObjectID> *>(user);
    objects->push_back(object);
    return true;
}

}  // namespace

void CorpseSubjectState_Link()
{
    if (g_corpseLinkMethod == NULL)
        g_corpseSubjectCapacity = 0;
}

bool CorpseSubjectState_CreateTable(SimulationContext *context,
                                    int capacity)
{
    g_corpseSubjectCapacity = 0;
    if (context == NULL || capacity <= 0 ||
        g_arena.getContext() != context)
        return false;
    const ct_ClassTableID table = g_arena.addClassTable("Corpse", capacity);
    if (table == ct_NULLID ||
        g_arena.searchSeanceClassTable("Corpse") != table)
        return false;
    g_corpseSubjectCapacity = capacity;
    return true;
}

bool CorpseSubjectState_TableReady(SimulationContext *context,
                                   int expectedCapacity)
{
    return context != NULL && g_arena.getContext() == context &&
           expectedCapacity > 0 &&
           g_corpseSubjectCapacity == expectedCapacity &&
           g_arena.searchSeanceClassTable("Corpse") != ct_NULLID;
}

int CorpseSubjectState_Capacity()
{
    return g_arena.searchSeanceClassTable("Corpse") == ct_NULLID
               ? 0
               : g_corpseSubjectCapacity;
}

int CorpseSubjectState_LiveCount()
{
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Corpse");
    if (table == ct_NULLID)
        return 0;
    int count = 0;
    g_arena.userFind(table, CountCorpseSubject, &count);
    return count;
}

Corpse *CorpseSubjectState_Find(SimulationContext *context,
                                const KR_ObjectID &object)
{
    if (context == NULL || !context->isExist(object))
        return NULL;
    return static_cast<Corpse *>(
        context->queryInterface(object, IUnknownIID));
}

bool CorpseSubjectState_CollectObjects(std::vector<KR_ObjectID> *objects)
{
    if (objects == NULL)
        return false;
    objects->clear();
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Corpse");
    if (table == ct_NULLID)
        return true;
    g_arena.userFind(table, CollectCorpseSubject, objects);
    return true;
}

unsigned long long CorpseSubjectState_Fingerprint(
    SimulationContext *context)
{
    const int capacity = CorpseSubjectState_Capacity();
    if (!CorpseSubjectState_TableReady(context, capacity) ||
        CorpseSubjectState_LiveCount() != 0)
        return 0;
    unsigned long long hash = kCorpseSubjectHashOffset;
    CorpseSubjectHashString(hash, "Corpse");
    CorpseSubjectHashBytes(hash, &capacity, sizeof(capacity));
    const int rendering = 1;
    CorpseSubjectHashBytes(hash, &rendering, sizeof(rendering));
    return hash;
}
