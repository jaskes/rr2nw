// RSX-independent sound configuration state used by modern core libraries.
#include <cmath>
#include <cstring>

#include "sound.h"

#include "SoundStateData.inl"

namespace
{

const unsigned int kSoundStateBackendAbiVersion = 3u;
SSoundStateBackend g_backend = {};
SSoundStateTelemetry g_telemetry = {};

struct SoundTelemetryInitializer
{
    SoundTelemetryInitializer()
    {
        g_telemetry.effectsVolume = 1.0f;
        g_telemetry.vehicleVolume = 1.0f;
    }
} g_soundTelemetryInitializer;

bool ValidBackend(const SSoundStateBackend *backend)
{
    return backend != NULL &&
           backend->abiVersion == kSoundStateBackendAbiVersion &&
           backend->owner != NULL && backend->admit != NULL &&
           backend->start != NULL && backend->stop != NULL &&
           backend->move != NULL && backend->setListener != NULL &&
           backend->setPitch != NULL &&
           backend->setCategoryVolume != NULL &&
           backend->setApplicationActive != NULL &&
           backend->maintain != NULL;
}

bool ValidVolume(float volume)
{
    return std::isfinite(volume) && volume >= 0.0f && volume <= 1.0f;
}

bool ValidPosition(float x, float y, float z)
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

bool ValidListener(const SSoundStateListenerPose *listener)
{
    if (listener == NULL ||
        !ValidPosition(listener->positionX, listener->positionY,
                       listener->positionZ) ||
        !ValidPosition(listener->frontX, listener->frontY,
                       listener->frontZ) ||
        !ValidPosition(listener->upX, listener->upY, listener->upZ))
        return false;
    const float front2 = listener->frontX * listener->frontX +
                         listener->frontY * listener->frontY +
                         listener->frontZ * listener->frontZ;
    const float up2 = listener->upX * listener->upX +
                      listener->upY * listener->upY +
                      listener->upZ * listener->upZ;
    return std::isfinite(front2) && std::isfinite(up2) &&
           front2 > 1.0e-8f && up2 > 1.0e-8f;
}

}  // namespace

bool SoundState_SetMaximumDistance(double maximumDistance)
{
    if (!std::isfinite(maximumDistance) || maximumDistance <= 0.0)
        return false;
    const double squared = maximumDistance * maximumDistance;
    if (!std::isfinite(squared) || squared <= 0.0)
        return false;
    snd_distMax = maximumDistance;
    snd_distMax2 = squared;
    return true;
}

bool SoundState_ConfigureVehicleEngine(int enabled, double intensity)
{
    if ((enabled != 0 && enabled != 1) || !std::isfinite(intensity) ||
        intensity < 0.0 || intensity > 1.0)
        return false;
    snd_engine = enabled;
    snd_engineIntensity = intensity;
    return true;
}

bool SoundState_ConfigureBackend(const SSoundStateBackend *backend)
{
    if (!ValidBackend(backend) || g_backend.owner != NULL)
        return false;
    g_backend = *backend;
    ++g_telemetry.backendConfigurations;
    g_backend.setCategoryVolume(g_backend.owner,
                                SOUND_STATE_CATEGORY_EFFECTS,
                                g_telemetry.effectsVolume);
    g_backend.setCategoryVolume(g_backend.owner,
                                SOUND_STATE_CATEGORY_VEHICLE,
                                g_telemetry.vehicleVolume);
    return true;
}

void SoundState_ClearBackend(void *owner)
{
    if (owner == NULL || g_backend.owner != owner)
        return;
    std::memset(&g_backend, 0, sizeof(g_backend));
    ++g_telemetry.backendReleases;
}

bool SoundState_AdmitWave(const char *fileName, int flags)
{
    ++g_telemetry.admissionRequests;
    if (fileName == NULL || fileName[0] == 0 || flags < 0 || flags > 1 ||
        g_backend.owner == NULL ||
        !g_backend.admit(g_backend.owner, fileName, flags))
    {
        ++g_telemetry.admissionFailures;
        return false;
    }
    return true;
}

bool SoundState_StartPlayback(const SSoundStatePlaybackRequest *request,
                              SoundStatePlaybackToken *token)
{
    if (token == NULL)
        return false;
    *token = 0;
    if (request == NULL || request->fileName == NULL ||
        request->fileName[0] == 0 || request->flags < 0 ||
        request->flags > 1 || request->playCount < 0 ||
        !std::isfinite(request->intensity) || request->intensity < 0.0f ||
        request->category < SOUND_STATE_CATEGORY_EFFECTS ||
        request->category >= SOUND_STATE_CATEGORY_COUNT ||
        (request->positionValid != 0 &&
         (!ValidPosition(request->positionX, request->positionY,
                         request->positionZ) ||
          !std::isfinite(request->minFront) ||
          !std::isfinite(request->minBack) ||
          !std::isfinite(request->maxFront) ||
          !std::isfinite(request->maxBack))))
    {
        ++g_telemetry.oneShotFailures;
        return false;
    }
    if (request->flags != 0)
    {
        ++g_telemetry.unsupportedStreamStarts;
        return false;
    }
    if (request->playCount != 0 && request->playCount != 1)
    {
        ++g_telemetry.unsupportedRepeatStarts;
        return false;
    }
    const bool loop = request->playCount == 0;
    if (loop)
        ++g_telemetry.loopRequests;
    else
        ++g_telemetry.oneShotRequests;
    if (g_backend.owner == NULL ||
        !g_backend.start(g_backend.owner, request, token) || *token == 0)
    {
        *token = 0;
        if (loop)
            ++g_telemetry.loopFailures;
        else
            ++g_telemetry.oneShotFailures;
        return false;
    }
    if (loop)
        ++g_telemetry.loopStarts;
    else
        ++g_telemetry.oneShotStarts;
    return true;
}

bool SoundState_MovePlayback(SoundStatePlaybackToken token,
                             float x, float y, float z)
{
    ++g_telemetry.emitterMoveRequests;
    if (token == 0 || !ValidPosition(x, y, z) || g_backend.owner == NULL ||
        !g_backend.move(g_backend.owner, token, x, y, z))
    {
        ++g_telemetry.emitterMoveFailures;
        return false;
    }
    ++g_telemetry.emitterMoveUpdates;
    return true;
}

bool SoundState_SetListener(const SSoundStateListenerPose *listener)
{
    ++g_telemetry.listenerRequests;
    if (!ValidListener(listener) || g_backend.owner == NULL ||
        !g_backend.setListener(g_backend.owner, listener))
    {
        ++g_telemetry.listenerFailures;
        return false;
    }
    ++g_telemetry.listenerUpdates;
    return true;
}

bool SoundState_SetPlaybackPitch(SoundStatePlaybackToken token, float ratio)
{
    ++g_telemetry.pitchRequests;
    if (token == 0 || !std::isfinite(ratio) || ratio < 0.15f ||
        ratio > 4.0f || g_backend.owner == NULL ||
        !g_backend.setPitch(g_backend.owner, token, ratio))
    {
        ++g_telemetry.pitchFailures;
        return false;
    }
    ++g_telemetry.pitchUpdates;
    return true;
}

void SoundState_StopPlayback(SoundStatePlaybackToken *token)
{
    if (token == NULL || *token == 0)
        return;
    if (g_backend.owner != NULL)
        g_backend.stop(g_backend.owner, *token);
    *token = 0;
    ++g_telemetry.stops;
}

bool SoundState_SetCategoryVolume(ESoundStateCategory category, float volume)
{
    if (category < 0 || category >= SOUND_STATE_CATEGORY_COUNT ||
        !ValidVolume(volume))
        return false;
    if (category == SOUND_STATE_CATEGORY_EFFECTS)
        g_telemetry.effectsVolume = volume;
    else
        g_telemetry.vehicleVolume = volume;
    if (g_backend.owner != NULL)
        g_backend.setCategoryVolume(g_backend.owner, category, volume);
    return true;
}

void SoundState_SetApplicationActive(bool active)
{
    if (g_backend.owner != NULL)
        g_backend.setApplicationActive(g_backend.owner, active);
    ++g_telemetry.focusChanges;
}

void SoundState_Maintain()
{
    if (g_backend.owner != NULL)
        g_backend.maintain(g_backend.owner);
    ++g_telemetry.maintenanceCalls;
}

bool SoundState_BackendConfigured()
{
    return g_backend.owner != NULL;
}

const SSoundStateTelemetry *SoundState_Telemetry()
{
    return &g_telemetry;
}

void SoundState_ResetTelemetryForTesting()
{
    const float effectsVolume = g_telemetry.effectsVolume;
    const float vehicleVolume = g_telemetry.vehicleVolume;
    std::memset(&g_telemetry, 0, sizeof(g_telemetry));
    g_telemetry.effectsVolume = effectsVolume;
    g_telemetry.vehicleVolume = vehicleVolume;
}
