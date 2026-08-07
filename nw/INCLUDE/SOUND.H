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
// START(0)/END pair owns a long-lived loop. The ABI-2 extension carries the
// source-proven emitter model and CViewObject listener pose; the maintained
// backend may apply a documented compatibility spatial model. Streamed media
// remains an explicit unsupported branch. ABI-3 adds separately routed
// Vehicle loops and their source-proven frequency-ratio updates.
enum ESoundStateCategory
{
    SOUND_STATE_CATEGORY_EFFECTS = 0,
    SOUND_STATE_CATEGORY_VEHICLE = 1,
    SOUND_STATE_CATEGORY_COUNT = 2
};

struct SSoundStatePlaybackRequest
{
    const char *fileName;
    int flags;
    int playCount;
    float intensity;
    int positionValid;
    float positionX;
    float positionY;
    float positionZ;
    float minFront;
    float minBack;
    float maxFront;
    float maxBack;
    ESoundStateCategory category;
};

struct SSoundStateListenerPose
{
    float positionX;
    float positionY;
    float positionZ;
    float frontX;
    float frontY;
    float frontZ;
    float upX;
    float upY;
    float upZ;
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
    bool (*move)(void *owner, SoundStatePlaybackToken token,
                 float x, float y, float z);
    bool (*setListener)(void *owner,
                        const SSoundStateListenerPose *listener);
    bool (*setPitch)(void *owner, SoundStatePlaybackToken token,
                     float ratio);
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
    unsigned int emitterMoveRequests;
    unsigned int emitterMoveUpdates;
    unsigned int emitterMoveFailures;
    unsigned int listenerRequests;
    unsigned int listenerUpdates;
    unsigned int listenerFailures;
    unsigned int pitchRequests;
    unsigned int pitchUpdates;
    unsigned int pitchFailures;
    unsigned int focusChanges;
    unsigned int maintenanceCalls;
    float effectsVolume;
    float vehicleVolume;
};

bool SoundState_ConfigureBackend(const SSoundStateBackend *backend);
void SoundState_ClearBackend(void *owner);
bool SoundState_AdmitWave(const char *fileName, int flags);
bool SoundState_StartPlayback(const SSoundStatePlaybackRequest *request,
                              SoundStatePlaybackToken *token);
void SoundState_StopPlayback(SoundStatePlaybackToken *token);
bool SoundState_MovePlayback(SoundStatePlaybackToken token,
                             float x, float y, float z);
bool SoundState_SetListener(const SSoundStateListenerPose *listener);
bool SoundState_SetPlaybackPitch(SoundStatePlaybackToken token, float ratio);
bool SoundState_SetCategoryVolume(ESoundStateCategory category, float volume);
void SoundState_SetApplicationActive(bool active);
void SoundState_Maintain();
bool SoundState_BackendConfigured();
const SSoundStateTelemetry *SoundState_Telemetry();
void SoundState_ResetTelemetryForTesting();
bool SoundState_ConfigureVehicleEngine(int enabled, double intensity);

extern int snd_engine ;
extern int snd_true3d ;
extern int snd_useDS  ;
extern double snd_distMax ;
extern double snd_distMax2;
extern double snd_engineIntensity;


#endif
