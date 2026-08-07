#ifndef __SOUND_H__
#define __SOUND_H__

#include <windows.h>
#include <windowsx.h>
#include <olectl.h>

#include <rsx.h>


extern IUnknown* lpRSX2Unk;
extern IRSX2* lpRSX2;

HRESULT InitializeRSX();
void DeInitializeRSX();

// Keeps the audible-zone radius usable when the legacy RSX device is not
// initialized. The update is transactional: invalid or overflowing values do
// not modify either public distance value.
bool SoundState_SetMaximumDistance(double maximumDistance);

// Maintained physical playback is attached through this narrow callback
// boundary.  Authored SoundObj state remains owned by the simulation and does
// not depend on a device being present.  The first production backend admits
// only cached PCM effects. START(1) is a one-shot and the source-proven
// START(0)/END pair owns a long-lived loop. Streamed media, positional
// attenuation and spatialization stay explicit unsupported branches until
// their owners are recovered.
enum ESoundStateCategory
{
    SOUND_STATE_CATEGORY_EFFECTS = 0,
    SOUND_STATE_CATEGORY_COUNT = 1
};

struct SSoundStatePlaybackRequest
{
    const char *fileName;
    int flags;
    int playCount;
    float intensity;
};

typedef unsigned long long SoundStatePlaybackToken;

struct SSoundStateBackend
{
    unsigned int abiVersion;
    void *owner;
    bool (*admit)(void *owner, const char *fileName, int flags);
    bool (*start)(void *owner, const SSoundStatePlaybackRequest *request,
                  SoundStatePlaybackToken *token);
    void (*stop)(void *owner, SoundStatePlaybackToken token);
    void (*setCategoryVolume)(void *owner, ESoundStateCategory category,
                              float volume);
    void (*setApplicationActive)(void *owner, bool active);
    void (*maintain)(void *owner);
};

struct SSoundStateTelemetry
{
    unsigned int backendConfigurations;
    unsigned int backendReleases;
    unsigned int admissionRequests;
    unsigned int admissionFailures;
    unsigned int oneShotRequests;
    unsigned int oneShotStarts;
    unsigned int oneShotFailures;
    unsigned int loopRequests;
    unsigned int loopStarts;
    unsigned int loopFailures;
    unsigned int unsupportedStreamStarts;
    unsigned int unsupportedRepeatStarts;
    unsigned int stops;
    unsigned int focusChanges;
    unsigned int maintenanceCalls;
    float effectsVolume;
};

bool SoundState_ConfigureBackend(const SSoundStateBackend *backend);
void SoundState_ClearBackend(void *owner);
bool SoundState_AdmitWave(const char *fileName, int flags);
bool SoundState_StartPlayback(const SSoundStatePlaybackRequest *request,
                              SoundStatePlaybackToken *token);
void SoundState_StopPlayback(SoundStatePlaybackToken *token);
bool SoundState_SetCategoryVolume(ESoundStateCategory category, float volume);
void SoundState_SetApplicationActive(bool active);
void SoundState_Maintain();
bool SoundState_BackendConfigured();
const SSoundStateTelemetry *SoundState_Telemetry();
void SoundState_ResetTelemetryForTesting();

extern int snd_engine ;
extern int snd_true3d ;
extern int snd_useDS  ;
extern double snd_distMax ;
extern double snd_distMax2;
extern double snd_engineIntensity;


#endif
