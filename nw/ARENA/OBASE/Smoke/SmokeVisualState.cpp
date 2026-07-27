#include "SmokeVisualState.h"

#include <cstdio>
#include <cstring>

#include "filesys.h"
#include "graph.h"
#include "h/cachesmoke.h"
#include "kernel/h/context.h"
#include "storage/h/subject.h"

#include "SmokeAttributeState.h"
#include "SmokeSubjectState.h"
#include "SmokerAttributeState.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const char *const kResourceNames[] = {
    "smoke.spr", "flame.spr", "corona.spr"
};
const int kResourceCount =
    static_cast<int>(sizeof(kResourceNames) / sizeof(kResourceNames[0]));
const long kRetailSpriteLength = 5 + 256 * 256;

struct SmokeVisualRuntimeState
{
    SimulationContext *context;
    int textureCheckpoint;
    unsigned long long fingerprint;
    bool ready;
};

SmokeVisualRuntimeState g_state = {};

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
    HashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

bool InspectResource(const char *name, unsigned long long &hash)
{
    long length = 0;
    FILE *file = CFileResource::FOpenCurrent(name, &length);
    if (file == NULL)
        return false;

    unsigned char header[5] = {};
    const bool headerRead =
        std::fread(header, 1, sizeof(header), file) == sizeof(header);
    const unsigned int width = header[0] | (header[1] << 8);
    const unsigned int height = header[2] | (header[3] << 8);
    if (!headerRead || width != 256 || height != 256 ||
        length != kRetailSpriteLength)
    {
        std::fclose(file);
        return false;
    }

    HashString(hash, name);
    HashBytes(hash, header, sizeof(header));
    unsigned char buffer[4096];
    long remaining = length - static_cast<long>(sizeof(header));
    while (remaining > 0)
    {
        const int request = remaining < static_cast<long>(sizeof(buffer))
                                ? static_cast<int>(remaining)
                                : static_cast<int>(sizeof(buffer));
        const int received = static_cast<int>(
            std::fread(buffer, 1, request, file));
        if (received != request)
        {
            std::fclose(file);
            return false;
        }
        HashBytes(hash, buffer, received);
        remaining -= received;
    }
    std::fclose(file);
    return true;
}

bool ResourceExists(const char *name)
{
    long length = 0;
    FILE *file = CFileResource::FOpenCurrent(name, &length);
    if (file == NULL)
        return false;
    std::fclose(file);
    return true;
}

void Rollback(SimulationContext *context, int checkpoint)
{
    SmokerAttributeState_ClearVisualResources(context);
    SmokeAttributeState_ClearVisualResources(context);
    SmokeTextureCache_Rollback(checkpoint);
}

}  // namespace

void SmokeVisualState_Link()
{
}

ESmokeVisualResourcePresence SmokeVisualState_InspectResources(
    unsigned long long *fingerprint)
{
    if (fingerprint != NULL)
        *fingerprint = 0;
    int present = 0;
    for (int i = 0; i < kResourceCount; ++i)
        if (ResourceExists(kResourceNames[i]))
            ++present;
    if (present == 0)
        return SMOKE_VISUAL_RESOURCES_NONE;
    if (present != kResourceCount)
        return SMOKE_VISUAL_RESOURCES_PARTIAL;

    unsigned long long hash = kHashOffset;
    for (int i = 0; i < kResourceCount; ++i)
        if (!InspectResource(kResourceNames[i], hash))
            return SMOKE_VISUAL_RESOURCES_INVALID;
    if (fingerprint != NULL)
        *fingerprint = hash;
    return SMOKE_VISUAL_RESOURCES_COMPLETE;
}

bool SmokeVisualState_Resolve(SimulationContext *context)
{
    if (g_state.ready)
        return SmokeVisualState_Ready(context);
    unsigned long long fingerprint = 0;
    if (context == NULL || g_arena.getContext() != context ||
        !SmokeSubjectState_TableReady(context, 300) ||
        !SmokeAttributeState_IsRetailRoster(context) ||
        !SmokerAttributeState_IsKnownRoster(context) ||
        !SmokerAttributeState_ReferencesResolved(context) ||
        SmokeVisualState_InspectResources(&fingerprint) !=
            SMOKE_VISUAL_RESOURCES_COMPLETE ||
        fingerprint == 0 || GRTransparentColor(1, 2, 3) == 0 ||
        _pGRLoadTextureToDB == NULL ||
        _pGRDeleteTextureFromDB == NULL ||
        !SmokeTextureCache_CanLoad(kResourceNames, kResourceCount))
        return false;

    const int checkpoint = SmokeTextureCache_Checkpoint();
    if (!SmokeAttributeState_ResolveVisualResources(context) ||
        !SmokerAttributeState_ResolveVisualResources(context) ||
        !SmokeAttributeState_VisualResourcesResolved(context) ||
        !SmokerAttributeState_VisualResourcesResolved(context) ||
        !SmokerAttributeState_RuntimeReady(context))
    {
        Rollback(context, checkpoint);
        return false;
    }

    g_state.context = context;
    g_state.textureCheckpoint = checkpoint;
    g_state.fingerprint = fingerprint;
    g_state.ready = true;
    return true;
}

bool SmokeVisualState_Ready(SimulationContext *context)
{
    return g_state.ready && context != NULL && g_state.context == context &&
           g_arena.getContext() == context &&
           SmokeSubjectState_TableReady(context, 300) &&
           SmokeAttributeState_VisualResourcesResolved(context) &&
           SmokerAttributeState_VisualResourcesResolved(context) &&
           SmokerAttributeState_RuntimeReady(context);
}

unsigned long long SmokeVisualState_Fingerprint(SimulationContext *context)
{
    return SmokeVisualState_Ready(context) ? g_state.fingerprint : 0;
}

void SmokeVisualState_Release()
{
    if (g_state.ready)
        Rollback(g_state.context, g_state.textureCheckpoint);
    g_state = SmokeVisualRuntimeState{};
}
