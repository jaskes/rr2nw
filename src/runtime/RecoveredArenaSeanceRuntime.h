#ifndef RR2NW_RECOVERED_ARENA_SEANCE_RUNTIME_H
#define RR2NW_RECOVERED_ARENA_SEANCE_RUNTIME_H

class SimulationContext;

enum ERecoveredArenaSeanceIssue : unsigned long long {
  RECOVERED_ARENA_SEANCE_INVALID_CONTEXT = 1ull << 0,
  RECOVERED_ARENA_SEANCE_OPEN_FAILURE = 1ull << 1,
  RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE = 1ull << 2,
  RECOVERED_ARENA_SEANCE_SCRIPT_INITIALIZATION_FAILURE = 1ull << 3,
  RECOVERED_ARENA_SEANCE_SCRIPT_COMPILE_FAILURE = 1ull << 4,
  RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE = 1ull << 5,
  RECOVERED_ARENA_SEANCE_SCRIPT_TIMEOUT = 1ull << 6,
  RECOVERED_ARENA_SEANCE_VEHICLE_TABLE_MISSING = 1ull << 7,
  RECOVERED_ARENA_SEANCE_VEHICLE_OBJECT_MISSING = 1ull << 8,
  RECOVERED_ARENA_SEANCE_VEHICLE_INTERFACE_MISSING = 1ull << 9,
  RECOVERED_ARENA_SEANCE_SCRIPT_HOST_FAILURE = 1ull << 10,
  RECOVERED_ARENA_SEANCE_ROUTE_TABLE_MISSING = 1ull << 11,
  RECOVERED_ARENA_SEANCE_SPARK_TABLE_MISSING = 1ull << 12,
  RECOVERED_ARENA_SEANCE_SPARK_OBJECT_MISSING = 1ull << 13,
  RECOVERED_ARENA_SEANCE_SPARK_DEFAULT_INVALID = 1ull << 14,
  RECOVERED_ARENA_SEANCE_BIRD_TABLE_MISSING = 1ull << 15,
  RECOVERED_ARENA_SEANCE_BIRD_OBJECT_MISSING = 1ull << 16,
  RECOVERED_ARENA_SEANCE_BIRD_DEFAULT_INVALID = 1ull << 17,
  RECOVERED_ARENA_SEANCE_PORTAL_TABLE_MISSING = 1ull << 18,
  RECOVERED_ARENA_SEANCE_ORPHAN_TABLE_MISSING = 1ull << 19,
  RECOVERED_ARENA_SEANCE_ORPHAN_OBJECT_MISSING = 1ull << 20,
  RECOVERED_ARENA_SEANCE_ORPHAN_DEFAULT_INVALID = 1ull << 21,
  RECOVERED_ARENA_SEANCE_ARTEFACT_TABLE_MISSING = 1ull << 22,
  RECOVERED_ARENA_SEANCE_ARTEFACT_OBJECT_MISSING = 1ull << 23,
  RECOVERED_ARENA_SEANCE_ARTEFACT_DEFAULT_INVALID = 1ull << 24,
  RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_TABLE_MISSING = 1ull << 25,
  RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_OBJECT_MISSING = 1ull << 26,
  RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_ROSTER_INVALID = 1ull << 27,
  RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 28,
  RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 29,
  RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_TABLE_MISSING = 1ull << 30,
  RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_ROSTER_INVALID = 1ull << 31,
  RECOVERED_ARENA_SEANCE_SKIN_CATALOG_INVALID = 1ull << 32,
  RECOVERED_ARENA_SEANCE_SKIN_TABLE_FAILURE = 1ull << 33,
  RECOVERED_ARENA_SEANCE_SKIN_RESOURCE_LOAD_FAILURE = 1ull << 34,
  RECOVERED_ARENA_SEANCE_SKIN_RESOURCE_ROSTER_INVALID = 1ull << 35,
  RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 36,
  RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_TABLE_MISSING = 1ull << 37,
  RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_ROSTER_INVALID = 1ull << 38,
  RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 39,
  RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_TABLE_MISSING = 1ull << 40,
  RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_ROSTER_INVALID = 1ull << 41,
  RECOVERED_ARENA_SEANCE_CORPSE_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 42,
  RECOVERED_ARENA_SEANCE_CORPSE_ATTRIBUTE_TABLE_MISSING = 1ull << 43,
  RECOVERED_ARENA_SEANCE_CORPSE_ATTRIBUTE_ROSTER_INVALID = 1ull << 44,
  RECOVERED_ARENA_SEANCE_SMOKER_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 45,
  RECOVERED_ARENA_SEANCE_SMOKER_ATTRIBUTE_TABLE_MISSING = 1ull << 46,
  RECOVERED_ARENA_SEANCE_SMOKER_ATTRIBUTE_ROSTER_INVALID = 1ull << 47,
  RECOVERED_ARENA_SEANCE_WAV_CATALOG_INVALID = 1ull << 48,
  RECOVERED_ARENA_SEANCE_WAV_SOURCE_UNAVAILABLE = 1ull << 49,
  RECOVERED_ARENA_SEANCE_WAV_TABLE_FAILURE = 1ull << 50,
  RECOVERED_ARENA_SEANCE_WAV_ROSTER_INVALID = 1ull << 51,
  RECOVERED_ARENA_SEANCE_FARTER_REFERENCE_INVALID = 1ull << 52,
  RECOVERED_ARENA_SEANCE_CORPSE_REFERENCE_INVALID = 1ull << 53,
  RECOVERED_ARENA_SEANCE_DYN_SMOKER_TABLE_FAILURE = 1ull << 54,
  RECOVERED_ARENA_SEANCE_DYN_SMOKER_LIFECYCLE_FAILURE = 1ull << 55,
  RECOVERED_ARENA_SEANCE_SMOKER_REFERENCE_INVALID = 1ull << 56,
  RECOVERED_ARENA_SEANCE_SMOKE_SUBJECT_TABLE_FAILURE = 1ull << 57,
  RECOVERED_ARENA_SEANCE_SMOKE_SUBJECT_LIFECYCLE_FAILURE = 1ull << 58,
  RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_INVALID = 1ull << 59,
  RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_LOAD_FAILURE = 1ull << 60,
  RECOVERED_ARENA_SEANCE_SOUND_OBJECT_TABLE_FAILURE = 1ull << 61,
  RECOVERED_ARENA_SEANCE_SOUND_OBJECT_LIFECYCLE_FAILURE = 1ull << 62,
  RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE = 1ull << 63
};

// The original diagnostic word is an established 64-bit compatibility
// surface and is full. New frontiers use this second word so prior issue
// values never change meaning.
enum ERecoveredArenaSeanceExtendedIssue : unsigned long long {
  RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 0,
  RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_TABLE_MISSING = 1ull << 1,
  RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_ROSTER_INVALID = 1ull << 2,
  RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 3,
  RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_TABLE_MISSING = 1ull << 4,
  RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_ROSTER_INVALID = 1ull << 5,
  RECOVERED_ARENA_SEANCE_EXT_TAXI_REFERENCE_INVALID = 1ull << 6,
  RECOVERED_ARENA_SEANCE_EXT_CORPSE_SUBJECT_TABLE_FAILURE = 1ull << 7,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 8,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_TABLE_MISSING = 1ull << 9,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_ROSTER_INVALID = 1ull << 10,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_REFERENCE_INVALID = 1ull << 11,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_SUBJECT_TABLE_FAILURE = 1ull << 12,
  RECOVERED_ARENA_SEANCE_EXT_SPARK_SUBJECT_TABLE_FAILURE = 1ull << 13
};

int RecoveredArenaSeance_Initialize(SimulationContext* context,
                                    double startTime);
void RecoveredArenaSeance_Release();
bool RecoveredArenaSeance_IsOpen();
bool RecoveredArenaSeance_ScriptCompleted();
bool RecoveredArenaSeance_BirdAttributesReady();
bool RecoveredArenaSeance_PortalReady();
bool RecoveredArenaSeance_OrphanAttributesReady();
bool RecoveredArenaSeance_ArtefactAttributesReady();
bool RecoveredArenaSeance_SmokeAttributesReady();
bool RecoveredArenaSeance_SmokeSubjectReady();
int RecoveredArenaSeance_SmokeSubjectCapacity();
unsigned long long RecoveredArenaSeance_SmokeSubjectFingerprint();
bool RecoveredArenaSeance_SmokeVisualResourcesReady();
unsigned long long RecoveredArenaSeance_SmokeVisualResourceFingerprint();
bool RecoveredArenaSeance_ExplosionAttributesReady();
bool RecoveredArenaSeance_VehicleAttributesReady();
int RecoveredArenaSeance_VehicleAttributeCount();
int RecoveredArenaSeance_VehicleAttributeCapacity();
unsigned long long RecoveredArenaSeance_VehicleAttributeFingerprint();
bool RecoveredArenaSeance_TaxiAttributesReady();
bool RecoveredArenaSeance_TaxiReferencesReady();
unsigned long long RecoveredArenaSeance_TaxiReferenceFingerprint();
bool RecoveredArenaSeance_BulletAttributesReady();
bool RecoveredArenaSeance_BulletReferencesReady();
int RecoveredArenaSeance_BulletAttributeCount();
int RecoveredArenaSeance_BulletAttributeCapacity();
unsigned long long RecoveredArenaSeance_BulletAttributeFingerprint();
unsigned long long RecoveredArenaSeance_BulletReferenceFingerprint();
bool RecoveredArenaSeance_BulletSubjectRegistrationReady();
int RecoveredArenaSeance_BulletSubjectCapacity();
bool RecoveredArenaSeance_FarterAttributesReady();
bool RecoveredArenaSeance_LampAttributesReady();
bool RecoveredArenaSeance_CorpseAttributesReady();
bool RecoveredArenaSeance_SmokerAttributesReady();
bool RecoveredArenaSeance_SmokerReferencesReady();
bool RecoveredArenaSeance_SmokerRuntimeReady();
unsigned long long RecoveredArenaSeance_SmokerReferenceFingerprint();
bool RecoveredArenaSeance_DynSmokerReady();
int RecoveredArenaSeance_DynSmokerCapacity();
unsigned long long RecoveredArenaSeance_DynSmokerFingerprint();
int RecoveredArenaSeance_SmokerAttributeCount();
int RecoveredArenaSeance_SmokerAttributeCapacity();
unsigned long long RecoveredArenaSeance_SmokerAttributeFingerprint();
int RecoveredArenaSeance_TaxiAttributeCount();
int RecoveredArenaSeance_TaxiAttributeCapacity();
unsigned long long RecoveredArenaSeance_TaxiAttributeFingerprint();
int RecoveredArenaSeance_FarterAttributeCount();
int RecoveredArenaSeance_FarterAttributeCapacity();
unsigned long long RecoveredArenaSeance_FarterAttributeFingerprint();
bool RecoveredArenaSeance_FarterReferencesReady();
bool RecoveredArenaSeance_FarterRuntimeReady();
bool RecoveredArenaSeance_FarterSubjectReady();
int RecoveredArenaSeance_FarterSubjectCapacity();
unsigned long long RecoveredArenaSeance_FarterSubjectFingerprint();
int RecoveredArenaSeance_FarterScriptObjectCount();
int RecoveredArenaSeance_FarterLiveObjectCount();
int RecoveredArenaSeance_FarterSoundObjectCount();
int RecoveredArenaSeance_FarterNearFrameAudibleCount();
int RecoveredArenaSeance_FarterFarFrameAudibleCount();
bool RecoveredArenaSeance_FarterAudibleFrameTransition();
bool RecoveredArenaSeance_SoundDistanceReady();
double RecoveredArenaSeance_SoundDistance();
double RecoveredArenaSeance_SoundDistanceSquared();
unsigned long long RecoveredArenaSeance_FarterReferenceFingerprint();
int RecoveredArenaSeance_LampAttributeCount();
int RecoveredArenaSeance_LampAttributeCapacity();
unsigned long long RecoveredArenaSeance_LampAttributeFingerprint();
int RecoveredArenaSeance_CorpseAttributeCount();
int RecoveredArenaSeance_CorpseAttributeCapacity();
unsigned long long RecoveredArenaSeance_CorpseAttributeFingerprint();
bool RecoveredArenaSeance_CorpseReferencesReady();
bool RecoveredArenaSeance_CorpseRuntimeReady();
unsigned long long RecoveredArenaSeance_CorpseReferenceFingerprint();
bool RecoveredArenaSeance_CorpseSubjectReady();
int RecoveredArenaSeance_CorpseSubjectCapacity();
unsigned long long RecoveredArenaSeance_CorpseSubjectFingerprint();
bool RecoveredArenaSeance_WavMetadataReady();
int RecoveredArenaSeance_WavMetadataCount();
int RecoveredArenaSeance_WavMetadataCapacity();
unsigned long long RecoveredArenaSeance_WavCatalogFingerprint();
unsigned long long RecoveredArenaSeance_WavResourceFingerprint();
bool RecoveredArenaSeance_SoundObjectReady();
int RecoveredArenaSeance_SoundObjectCapacity();
unsigned long long RecoveredArenaSeance_SoundObjectFingerprint();
bool RecoveredArenaSeance_SkinResourcesReady();
int RecoveredArenaSeance_SkinModelCount();
int RecoveredArenaSeance_SkinSpriteCount();
unsigned long long RecoveredArenaSeance_SkinCatalogFingerprint();
unsigned long long RecoveredArenaSeance_SkinResourceFingerprint();
bool RecoveredArenaSeance_SparkAttributesReady();
bool RecoveredArenaSeance_SparkSubjectReady();
int RecoveredArenaSeance_SparkSubjectCapacity();
bool RecoveredArenaSeance_RouteReady();
bool RecoveredArenaSeance_VehicleReady();
unsigned long long RecoveredArenaSeance_Issues();
unsigned long long RecoveredArenaSeance_ExtendedIssues();
const char* RecoveredArenaSeance_LastError();

#endif
