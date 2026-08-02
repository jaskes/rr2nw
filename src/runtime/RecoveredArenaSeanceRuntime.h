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
  RECOVERED_ARENA_SEANCE_EXT_SPARK_SUBJECT_TABLE_FAILURE = 1ull << 13,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_SUBJECT_LIFECYCLE_FAILURE = 1ull << 14,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_COLLISION_LIFECYCLE_FAILURE = 1ull << 15,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SUBJECT_TABLE_FAILURE = 1ull << 16,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SUBJECT_LIFECYCLE_FAILURE = 1ull << 17,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_EFFECT_TRANSACTION_FAILURE = 1ull << 18,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_IMPULSE_BINDING_FAILURE = 1ull << 19,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_LIGHT_LIFECYCLE_FAILURE = 1ull << 20,
  RECOVERED_ARENA_SEANCE_EXT_SPARK_VISUAL_RESOURCE_FAILURE = 1ull << 21,
  RECOVERED_ARENA_SEANCE_EXT_SPARK_SUBJECT_LIFECYCLE_FAILURE = 1ull << 22,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_GROUND_SPARK_FAILURE = 1ull << 23,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_BARREL_SMOKE_FAILURE = 1ull << 24,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SOUND_LIFECYCLE_FAILURE = 1ull << 25,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PARTICLE_LIFECYCLE_FAILURE = 1ull << 26,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SMOKE_LIFECYCLE_FAILURE = 1ull << 27,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PIECE_LIFECYCLE_FAILURE = 1ull << 28,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_TRACE_LIFECYCLE_FAILURE = 1ull << 29,
  RECOVERED_ARENA_SEANCE_EXT_VEHICLE_REFERENCE_INVALID = 1ull << 30,
  RECOVERED_ARENA_SEANCE_EXT_TAXI_SUBJECT_SOURCE_UNAVAILABLE = 1ull << 31,
  RECOVERED_ARENA_SEANCE_EXT_TAXI_SUBJECT_ROSTER_INVALID = 1ull << 32,
  RECOVERED_ARENA_SEANCE_EXT_TAXI_SUBJECT_LIFECYCLE_FAILURE = 1ull << 33,
  RECOVERED_ARENA_SEANCE_EXT_ORPHAN_REFERENCE_INVALID = 1ull << 34,
  RECOVERED_ARENA_SEANCE_EXT_ORPHAN_SUBJECT_TABLE_FAILURE = 1ull << 35,
  RECOVERED_ARENA_SEANCE_EXT_ORPHAN_SUBJECT_LIFECYCLE_FAILURE = 1ull << 36,
  RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_SOURCE_UNAVAILABLE = 1ull << 37,
  RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_ROSTER_INVALID = 1ull << 38,
  RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_SOURCE_UNAVAILABLE = 1ull << 39,
  RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_ROSTER_INVALID = 1ull << 40,
  RECOVERED_ARENA_SEANCE_EXT_PEOPLE_REFERENCE_INVALID = 1ull << 41,
  RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE = 1ull << 42,
  RECOVERED_ARENA_SEANCE_EXT_ROUTE_SOURCE_UNAVAILABLE = 1ull << 43,
  RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SOURCE_UNAVAILABLE = 1ull << 44,
  RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_ATTRIBUTE_ROSTER_INVALID = 1ull << 45,
  RECOVERED_ARENA_SEANCE_EXT_TANK_REFERENCE_INVALID = 1ull << 46,
  RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SUBJECT_TABLE_FAILURE = 1ull << 47,
  RECOVERED_ARENA_SEANCE_EXT_TANK_LIFECYCLE_FAILURE = 1ull << 48,
  RECOVERED_ARENA_SEANCE_EXT_COMMANDER_SOURCE_OR_ROSTER_INVALID = 1ull << 49,
  RECOVERED_ARENA_SEANCE_EXT_TANK_GROUP_SUBJECT_TABLE_FAILURE = 1ull << 50,
  RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE = 1ull << 51,
  RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_ENVELOPE_FAILURE = 1ull << 52,
  RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_RESTORE_FAILURE = 1ull << 53,
  RECOVERED_ARENA_SEANCE_EXT_BULLET_ACTIVE_WORLD_FAILURE = 1ull << 54,
  RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_ACTIVE_WORLD_FAILURE = 1ull << 55,
  RECOVERED_ARENA_SEANCE_EXT_SPARK_ACTIVE_WORLD_FAILURE = 1ull << 56,
  RECOVERED_ARENA_SEANCE_EXT_SMOKE_ACTIVE_WORLD_FAILURE = 1ull << 57,
  RECOVERED_ARENA_SEANCE_EXT_CORPSE_ACTIVE_WORLD_FAILURE = 1ull << 58,
  RECOVERED_ARENA_SEANCE_EXT_GAMEPLAY_TUNING_FAILURE = 1ull << 59,
  RECOVERED_ARENA_SEANCE_EXT_SCRIPT_EVENT_FAILURE = 1ull << 60,
  RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_SCRIPT_FAILURE = 1ull << 61,
  RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_STATE_FAILURE = 1ull << 62,
  RECOVERED_ARENA_SEANCE_EXT_STATIC_MECHANISM_FAILURE = 1ull << 63
};

int RecoveredArenaSeance_Initialize(SimulationContext* context,
                                    double startTime);
void RecoveredArenaSeance_Release();
bool RecoveredArenaSeance_IsOpen();
bool RecoveredArenaSeance_ScriptCompleted();
bool RecoveredArenaSeance_BirdAttributesReady();
bool RecoveredArenaSeance_PortalReady();
bool RecoveredArenaSeance_TeleportRoutesReady();
bool RecoveredArenaSeance_TeleportTargetLevel();
int RecoveredArenaSeance_TeleportCapacity();
int RecoveredArenaSeance_TeleportRouteCount();
int RecoveredArenaSeance_TeleportProbeRejectedNonPlayer();
int RecoveredArenaSeance_TeleportProbePhysicsCollisions();
int RecoveredArenaSeance_TeleportProbeAppliedPlayer();
int RecoveredArenaSeance_TeleportProbeVehicleRollbacks();
unsigned long long RecoveredArenaSeance_TeleportFingerprint();
bool RecoveredArenaSeance_OrphanAttributesReady();
bool RecoveredArenaSeance_OrphanReferencesReady();
unsigned long long RecoveredArenaSeance_OrphanReferenceFingerprint();
bool RecoveredArenaSeance_OrphanSubjectReady();
int RecoveredArenaSeance_OrphanSubjectCapacity();
int RecoveredArenaSeance_OrphanSubjectCount();
unsigned long long RecoveredArenaSeance_OrphanSubjectFingerprint();
bool RecoveredArenaSeance_ArtefactAttributesReady();
bool RecoveredArenaSeance_SmokeAttributesReady();
bool RecoveredArenaSeance_SmokeSubjectReady();
int RecoveredArenaSeance_SmokeSubjectCapacity();
unsigned long long RecoveredArenaSeance_SmokeSubjectFingerprint();
bool RecoveredArenaSeance_SmokeVisualResourcesReady();
unsigned long long RecoveredArenaSeance_SmokeVisualResourceFingerprint();
bool RecoveredArenaSeance_ExplosionAttributesReady();
bool RecoveredArenaSeance_ExplosionSubjectReady();
bool RecoveredArenaSeance_ExplosionImpulseReady();
bool RecoveredArenaSeance_ExplosionLightReady();
bool RecoveredArenaSeance_ExplosionSoundReady();
unsigned long long RecoveredArenaSeance_ExplosionSoundReferenceFingerprint();
int RecoveredArenaSeance_ExplosionSoundProbeStarted();
int RecoveredArenaSeance_ExplosionSoundProbeDependencySkips();
int RecoveredArenaSeance_ExplosionSoundProbeRollbacks();
bool RecoveredArenaSeance_ExplosionParticlesReady();
unsigned long long RecoveredArenaSeance_ExplosionParticleVisualFingerprint();
int RecoveredArenaSeance_ExplosionParticleProbeStartedBranches();
int RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles();
int RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles();
int RecoveredArenaSeance_ExplosionParticleProbeRays();
int RecoveredArenaSeance_ExplosionParticleProbeDependencySkips();
int RecoveredArenaSeance_ExplosionParticleProbeMoveSteps();
int RecoveredArenaSeance_ExplosionParticleProbeExpiredParents();
int RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches();
bool RecoveredArenaSeance_ExplosionSmokeReady();
unsigned long long RecoveredArenaSeance_ExplosionSmokeVisualFingerprint();
int RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites();
int RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips();
int RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps();
int RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents();
int RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites();
bool RecoveredArenaSeance_ExplosionPieceReady();
unsigned long long RecoveredArenaSeance_ExplosionPieceReferenceFingerprint();
int RecoveredArenaSeance_ExplosionPieceProbeStartedPieces();
int RecoveredArenaSeance_ExplosionPieceProbeDependencySkips();
int RecoveredArenaSeance_ExplosionPieceProbeMoveSteps();
int RecoveredArenaSeance_ExplosionPieceProbeExpiredParents();
int RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces();
bool RecoveredArenaSeance_ExplosionTraceReady();
unsigned long long RecoveredArenaSeance_ExplosionTraceReferenceFingerprint();
int RecoveredArenaSeance_ExplosionTraceProbeStartedPieces();
int RecoveredArenaSeance_ExplosionTraceProbeQuotaGateSkips();
int RecoveredArenaSeance_ExplosionTraceProbePuffEvents();
int RecoveredArenaSeance_ExplosionTraceProbeSmokeChildren();
int RecoveredArenaSeance_ExplosionTraceProbeMoveSteps();
int RecoveredArenaSeance_ExplosionTraceProbeExpiredParents();
int RecoveredArenaSeance_ExplosionTraceProbeRolledBackPieces();
int RecoveredArenaSeance_ExplosionSubjectCapacity();
unsigned long long RecoveredArenaSeance_ExplosionSubjectFingerprint();
int RecoveredArenaSeance_ExplosionProbeInvalidStarts();
int RecoveredArenaSeance_ExplosionProbeAllocationRollbacks();
int RecoveredArenaSeance_ExplosionProbeQueuedCommands();
int RecoveredArenaSeance_ExplosionProbeQueueRollbacks();
int RecoveredArenaSeance_ExplosionProbeExecutedCommands();
int RecoveredArenaSeance_ExplosionProbeDamageApplications();
bool RecoveredArenaSeance_ExplosionActiveWorldReady();
int RecoveredArenaSeance_ExplosionActiveWorldCapturedOwners();
int RecoveredArenaSeance_ExplosionActiveWorldCapturedBranches();
int RecoveredArenaSeance_ExplosionActiveWorldSchedulerEvents();
int RecoveredArenaSeance_ExplosionActiveWorldSoundChildren();
int RecoveredArenaSeance_ExplosionActiveWorldRollbacks();
int RecoveredArenaSeance_ExplosionActiveWorldReconstructedIDs();
int RecoveredArenaSeance_ExplosionActiveWorldStableRoundTrips();
int RecoveredArenaSeance_ExplosionActiveWorldResumedMoves();
unsigned long long RecoveredArenaSeance_ExplosionActiveWorldFingerprint();
bool RecoveredArenaSeance_VehicleAttributesReady();
bool RecoveredArenaSeance_VehicleReferencesReady();
int RecoveredArenaSeance_VehicleAttributeCount();
int RecoveredArenaSeance_VehicleAttributeCapacity();
unsigned long long RecoveredArenaSeance_VehicleAttributeFingerprint();
unsigned long long RecoveredArenaSeance_VehicleReferenceFingerprint();
bool RecoveredArenaSeance_TaxiAttributesReady();
bool RecoveredArenaSeance_TaxiReferencesReady();
unsigned long long RecoveredArenaSeance_TaxiReferenceFingerprint();
bool RecoveredArenaSeance_TaxiSubjectReady();
int RecoveredArenaSeance_TaxiSubjectCapacity();
int RecoveredArenaSeance_TaxiSubjectCount();
int RecoveredArenaSeance_TaxiSubjectSoundCount();
unsigned long long RecoveredArenaSeance_TaxiSubjectFingerprint();
int RecoveredArenaSeance_TaxiProbeInvalidStarts();
int RecoveredArenaSeance_TaxiProbeValidStarts();
int RecoveredArenaSeance_TaxiProbeRenderReady();
int RecoveredArenaSeance_TaxiProbeSoundReady();
int RecoveredArenaSeance_TaxiProbeRollbacks();
bool RecoveredArenaSeance_BulletAttributesReady();
bool RecoveredArenaSeance_BulletReferencesReady();
int RecoveredArenaSeance_BulletAttributeCount();
int RecoveredArenaSeance_BulletAttributeCapacity();
unsigned long long RecoveredArenaSeance_BulletAttributeFingerprint();
unsigned long long RecoveredArenaSeance_BulletReferenceFingerprint();
bool RecoveredArenaSeance_BulletSubjectRegistrationReady();
bool RecoveredArenaSeance_BulletSubjectReady();
bool RecoveredArenaSeance_BulletImpactEffectsReady();
bool RecoveredArenaSeance_BulletGroundSparkReady();
bool RecoveredArenaSeance_BulletBarrelSmokeReady();
int RecoveredArenaSeance_BulletSubjectCapacity();
unsigned long long RecoveredArenaSeance_BulletSubjectFingerprint();
int RecoveredArenaSeance_BulletSubjectProbeMoveCount();
int RecoveredArenaSeance_BulletCollisionScheduledChecks();
int RecoveredArenaSeance_BulletCollisionExecutedChecks();
int RecoveredArenaSeance_BulletCollisionSphereCases();
int RecoveredArenaSeance_BulletCollisionEarliestHitCases();
int RecoveredArenaSeance_BulletCollisionWaterlineCases();
int RecoveredArenaSeance_BulletCollisionSceneQueries();
int RecoveredArenaSeance_BulletEffectQueuedBatches();
int RecoveredArenaSeance_BulletEffectQueuedChildren();
int RecoveredArenaSeance_BulletEffectSplashFirstCases();
int RecoveredArenaSeance_BulletEffectRolledBackChildren();
int RecoveredArenaSeance_BulletGroundSparkQueued();
int RecoveredArenaSeance_BulletGroundSparkRolledBack();
int RecoveredArenaSeance_BulletBarrelSmokeThresholdStarts();
int RecoveredArenaSeance_BulletBarrelSmokeFrameGateSkips();
int RecoveredArenaSeance_BulletBarrelSmokeAttributeGateSkips();
int RecoveredArenaSeance_BulletBarrelSmokeRollbacks();
bool RecoveredArenaSeance_BulletActiveWorldReady();
int RecoveredArenaSeance_BulletActiveWorldCapturedOwners();
int RecoveredArenaSeance_BulletActiveWorldSchedulerEvents();
int RecoveredArenaSeance_BulletActiveWorldRollbacks();
int RecoveredArenaSeance_BulletActiveWorldReconstructedIDs();
int RecoveredArenaSeance_BulletActiveWorldStableRoundTrips();
int RecoveredArenaSeance_BulletActiveWorldResumedMoves();
int RecoveredArenaSeance_BulletActiveWorldTombstonedMasters();
unsigned long long RecoveredArenaSeance_BulletActiveWorldFingerprint();
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
bool RecoveredArenaSeance_SkinAnimationsReady();
int RecoveredArenaSeance_SkinAnimationEntryCallCount();
int RecoveredArenaSeance_SkinAnimatedModelCount();
int RecoveredArenaSeance_SkinAnimationCommandCount();
unsigned long long RecoveredArenaSeance_SkinAnimationSourceFingerprint();
unsigned long long RecoveredArenaSeance_SkinAnimationStateFingerprint();
int RecoveredArenaSeance_SkinAnimationPoseTemporalModelCount();
int RecoveredArenaSeance_SkinAnimationPoseChangedModelCount();
int RecoveredArenaSeance_SkinAnimationPoseSampleCount();
int RecoveredArenaSeance_SkinAnimationPoseRestoredModifierCount();
unsigned long long RecoveredArenaSeance_SkinAnimationPoseFingerprint();
bool RecoveredArenaSeance_StaticMechanismsReady();
bool RecoveredArenaSeance_StaticMechanismTargetLevel();
bool RecoveredArenaSeance_StaticMechanismLevelOne();
bool RecoveredArenaSeance_StaticMechanismLevelFive();
int RecoveredArenaSeance_StaticMechanismBindingCount();
int RecoveredArenaSeance_StaticMechanismWaterwheelCount();
int RecoveredArenaSeance_StaticMechanismFlagCount();
int RecoveredArenaSeance_StaticMechanismRotatingCount();
int RecoveredArenaSeance_StaticMechanismDoorCount();
int RecoveredArenaSeance_StaticMechanismPol16Count();
int RecoveredArenaSeance_StaticMechanismChangedBindingCount();
int RecoveredArenaSeance_StaticMechanismPoseSampleCount();
int RecoveredArenaSeance_StaticMechanismRestoredModifierCount();
unsigned long long RecoveredArenaSeance_StaticMechanismFingerprint();
bool RecoveredArenaSeance_SparkAttributesReady();
bool RecoveredArenaSeance_SparkSubjectReady();
bool RecoveredArenaSeance_SparkVisualResourcesReady();
int RecoveredArenaSeance_SparkSubjectCapacity();
unsigned long long RecoveredArenaSeance_SparkSubjectFingerprint();
unsigned long long RecoveredArenaSeance_SparkVisualResourceFingerprint();
int RecoveredArenaSeance_SparkProbeInvalidStarts();
int RecoveredArenaSeance_SparkProbeQueuedCreates();
int RecoveredArenaSeance_SparkProbeQueueRollbacks();
int RecoveredArenaSeance_SparkProbePhaseTransitions();
int RecoveredArenaSeance_SparkProbeExpirations();
bool RecoveredArenaSeance_SparkActiveWorldReady();
int RecoveredArenaSeance_SparkActiveWorldCapturedOwners();
int RecoveredArenaSeance_SparkActiveWorldSchedulerEvents();
int RecoveredArenaSeance_SparkActiveWorldRollbacks();
int RecoveredArenaSeance_SparkActiveWorldReconstructedIDs();
int RecoveredArenaSeance_SparkActiveWorldStableRoundTrips();
int RecoveredArenaSeance_SparkActiveWorldResumedPhases();
unsigned long long RecoveredArenaSeance_SparkActiveWorldFingerprint();
bool RecoveredArenaSeance_SmokeActiveWorldReady();
int RecoveredArenaSeance_SmokeActiveWorldCapturedOwners();
int RecoveredArenaSeance_SmokeActiveWorldCapturedBlobs();
int RecoveredArenaSeance_SmokeActiveWorldSchedulerEvents();
int RecoveredArenaSeance_SmokeActiveWorldRollbacks();
int RecoveredArenaSeance_SmokeActiveWorldReconstructedIDs();
int RecoveredArenaSeance_SmokeActiveWorldStableRoundTrips();
int RecoveredArenaSeance_SmokeActiveWorldResumedMoves();
unsigned long long RecoveredArenaSeance_SmokeActiveWorldFingerprint();
bool RecoveredArenaSeance_CorpseActiveWorldReady();
int RecoveredArenaSeance_CorpseActiveWorldCapturedOwners();
int RecoveredArenaSeance_CorpseActiveWorldOwnedSmokers();
int RecoveredArenaSeance_CorpseActiveWorldSchedulerEvents();
int RecoveredArenaSeance_CorpseActiveWorldRollbacks();
int RecoveredArenaSeance_CorpseActiveWorldReconstructedObjects();
int RecoveredArenaSeance_CorpseActiveWorldStableRoundTrips();
int RecoveredArenaSeance_CorpseActiveWorldResumedEmissions();
int RecoveredArenaSeance_CorpseActiveWorldResumedDeaths();
unsigned long long RecoveredArenaSeance_CorpseActiveWorldFingerprint();
bool RecoveredArenaSeance_RouteReady();
bool RecoveredArenaSeance_PeopleAttributesReady();
bool RecoveredArenaSeance_PeopleReferencesReady();
bool RecoveredArenaSeance_PeopleSubjectReady();
int RecoveredArenaSeance_PeopleAttributeCapacity();
int RecoveredArenaSeance_PeopleAttributeCount();
int RecoveredArenaSeance_PeopleSubjectCapacity();
int RecoveredArenaSeance_PeopleSubjectCount();
int RecoveredArenaSeance_PeopleSubjectSoundCount();
unsigned long long RecoveredArenaSeance_PeopleAttributeFingerprint();
unsigned long long RecoveredArenaSeance_PeopleSubjectFingerprint();
int RecoveredArenaSeance_PeopleProbeScheduledMoves();
int RecoveredArenaSeance_PeopleProbeCadenceBounded();
int RecoveredArenaSeance_PeopleProbeRenderedPoseFrames();
int RecoveredArenaSeance_PeopleProbeViewBoundaryResets();
int RecoveredArenaSeance_PeopleProbeBulletDamageApplications();
int RecoveredArenaSeance_PeopleProbeDeathTransitions();
int RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips();
int RecoveredArenaSeance_PeopleProbeRollbacks();
int RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs();
int RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents();
int RecoveredArenaSeance_PeopleActiveWorldRollbacks();
unsigned long long RecoveredArenaSeance_PeopleActiveWorldFingerprint();
bool RecoveredArenaSeance_TankCannonAttributesReady();
bool RecoveredArenaSeance_TankReferencesReady();
bool RecoveredArenaSeance_TankCannonSubjectTablesReady();
int RecoveredArenaSeance_CannonAttributeCapacity();
int RecoveredArenaSeance_CannonAttributeCount();
int RecoveredArenaSeance_CannonSubjectCapacity();
int RecoveredArenaSeance_CannonSubjectCount();
unsigned long long RecoveredArenaSeance_CannonAttributeFingerprint();
unsigned long long RecoveredArenaSeance_CannonSubjectFingerprint();
int RecoveredArenaSeance_TankAttributeCapacity();
int RecoveredArenaSeance_TankAttributeCount();
int RecoveredArenaSeance_TankSubjectCapacity();
int RecoveredArenaSeance_TankSubjectCount();
unsigned long long RecoveredArenaSeance_TankAttributeFingerprint();
unsigned long long RecoveredArenaSeance_TankSubjectFingerprint();
int RecoveredArenaSeance_TankProbeAvailable();
int RecoveredArenaSeance_TankProbeValidStarts();
int RecoveredArenaSeance_TankProbeDynamicReady();
int RecoveredArenaSeance_TankProbeRenderReady();
int RecoveredArenaSeance_TankProbeCannonReady();
int RecoveredArenaSeance_TankProbeScheduledMoves();
int RecoveredArenaSeance_TankProbeCadenceBounded();
int RecoveredArenaSeance_TankProbeRenderedPoseFrames();
int RecoveredArenaSeance_TankProbeViewBoundaryResets();
int RecoveredArenaSeance_TankProbeBulletDamageApplications();
int RecoveredArenaSeance_TankProbeDeathTransitions();
int RecoveredArenaSeance_TankProbeDeathEffects();
int RecoveredArenaSeance_TankProbeSaveStateRoundTrips();
int RecoveredArenaSeance_TankProbeRollbacks();
bool RecoveredArenaSeance_CommanderReady();
int RecoveredArenaSeance_CommanderCapacity();
int RecoveredArenaSeance_CommanderCount();
int RecoveredArenaSeance_CommanderHostileLinks();
unsigned long long RecoveredArenaSeance_CommanderFingerprint();
bool RecoveredArenaSeance_MissionProjectsReady();
int RecoveredArenaSeance_MissionProjectCapacity();
int RecoveredArenaSeance_MissionProjectNodeCapacity();
int RecoveredArenaSeance_MissionProjectHeapCapacity();
int RecoveredArenaSeance_MissionProjectCount();
int RecoveredArenaSeance_MissionProjectNodeCount();
int RecoveredArenaSeance_MissionProjectDataBytes();
int RecoveredArenaSeance_MissionProjectSummaryCount();
int RecoveredArenaSeance_MissionProjectPermanentCount();
int RecoveredArenaSeance_MissionProjectDeferredHowitzerCount();
int RecoveredArenaSeance_MissionProjectDeferredDestroyableCount();
unsigned long long RecoveredArenaSeance_MissionProjectFingerprint();
bool RecoveredArenaSeance_RecruitCentersReady();
int RecoveredArenaSeance_RecruitCenterCapacity();
int RecoveredArenaSeance_RecruitCenterCount();
int RecoveredArenaSeance_RecruitCenterVideoCount();
int RecoveredArenaSeance_RecruitCenterDefaultTaxiCount();
int RecoveredArenaSeance_RecruitCenterDictionaryCount();
unsigned long long RecoveredArenaSeance_RecruitCenterFingerprint();
bool RecoveredArenaSeance_MissionTankLifecycleReady();
int RecoveredArenaSeance_TankGroupSubjectCapacity();
int RecoveredArenaSeance_MissionTankAvailable();
int RecoveredArenaSeance_MissionTankSpawns();
int RecoveredArenaSeance_MissionTankMembershipLinks();
int RecoveredArenaSeance_MissionTankFindEnemyCycles();
int RecoveredArenaSeance_MissionTankMovingCycles();
int RecoveredArenaSeance_MissionTankStableRoundTrips();
int RecoveredArenaSeance_MissionTankReconstructedIDs();
int RecoveredArenaSeance_MissionTankRollbacks();
unsigned long long RecoveredArenaSeance_MissionTankFingerprint();
bool RecoveredArenaSeance_ActiveWorldPersistenceReady();
int RecoveredArenaSeance_ActiveWorldFormatVersion();
int RecoveredArenaSeance_ActiveWorldEngineCompatibility();
int RecoveredArenaSeance_ActiveWorldSections();
int RecoveredArenaSeance_ActiveWorldEvents();
int RecoveredArenaSeance_ActiveWorldOwnerPhases();
int RecoveredArenaSeance_ActiveWorldReferencePhases();
int RecoveredArenaSeance_ActiveWorldEventPhases();
int RecoveredArenaSeance_ActiveWorldCreatedOwners();
int RecoveredArenaSeance_ActiveWorldMissionRecords();
int RecoveredArenaSeance_ActiveWorldMissionConditionReferences();
int RecoveredArenaSeance_ActiveWorldMissionRouteReferences();
int RecoveredArenaSeance_ActiveWorldMissionCheckEvents();
int RecoveredArenaSeance_ActiveWorldClockRecords();
unsigned int RecoveredArenaSeance_ActiveWorldRngAlgorithm();
int RecoveredArenaSeance_ActiveWorldRngStateBytes();
unsigned long long RecoveredArenaSeance_ActiveWorldRngDrawCount();
int RecoveredArenaSeance_ActiveWorldCorruptionRejects();
int RecoveredArenaSeance_ActiveWorldRollbacks();
unsigned long long RecoveredArenaSeance_ActiveWorldContainerBytes();
unsigned long long RecoveredArenaSeance_ActiveWorldFingerprint();
bool RecoveredArenaSeance_VehicleReady();
int RecoveredArenaSeance_VehicleActiveWorldReconstructedIDs();
int RecoveredArenaSeance_VehicleActiveWorldRollbacks();
unsigned long long RecoveredArenaSeance_VehicleActiveWorldFingerprint();
double RecoveredArenaSeance_VehicleVesselMass();
unsigned long long RecoveredArenaSeance_Issues();
unsigned long long RecoveredArenaSeance_ExtendedIssues();
const char* RecoveredArenaSeance_LastError();

#endif
