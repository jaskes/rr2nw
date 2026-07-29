#pragma once

enum ERecoveredGameServicesIssue {
  RECOVERED_GAME_SERVICES_COM_FAILURE = 1u << 0,
  RECOVERED_GAME_SERVICES_MISSING_PLATFORM = 1u << 1,
  RECOVERED_GAME_SERVICES_MISSING_LEVEL = 1u << 2,
  RECOVERED_GAME_SERVICES_SESSION_FAILURE = 1u << 3,
  RECOVERED_GAME_SERVICES_BEGIN_LOOP_FAILURE = 1u << 4,
  RECOVERED_GAME_SERVICES_FRAME_FAILURE = 1u << 5,
  RECOVERED_GAME_SERVICES_ACTIVE_DEBUG_MAP_UNAVAILABLE = 1u << 6,
  RECOVERED_GAME_SERVICES_UNSUPPORTED_LEVEL_EVENT = 1u << 7,
  RECOVERED_GAME_SERVICES_SEANCE_FAILURE = 1u << 8,
  RECOVERED_GAME_SERVICES_VEHICLE_MOVEMENT_FAILURE = 1u << 9,
  RECOVERED_GAME_SERVICES_VEHICLE_CONTROL_FAILURE = 1u << 10,
  RECOVERED_GAME_SERVICES_TAXI_VEHICLE_TRANSITION_FAILURE = 1u << 11
};

struct SRecoveredObserverState {
  double x;
  double y;
  double z;
  double yaw;
  double pitch;
  unsigned int inputEvents;
};

struct SRecoveredVehicleDriveTelemetry {
  double positionX;
  double positionY;
  double positionZ;
  double speedX;
  double speedY;
  double speedZ;
  double horizontalDistance;
  double maximumHorizontalDistance;
  double speedMagnitude;
  double maximumSpeedMagnitude;
  double headingDelta;
  double maximumHeadingDelta;
  int lastBumpFlags;
  int touchingGround;
  unsigned int groundContactFrames;
  unsigned int staticCollisionFrames;
  unsigned int landCollisionFrames;
  unsigned int dynamicCollisionFrames;
};

void RecoveredGameServices_UseRuntime();
void RecoveredGameServices_Release();
bool RecoveredGameServices_PlatformReady();
bool RecoveredGameServices_SessionReady();
bool RecoveredGameServices_LoopReady();
bool RecoveredGameServices_HardwareReady();
bool RecoveredGameServices_SeanceReady();
bool RecoveredGameServices_BirdAttributesReady();
bool RecoveredGameServices_PortalReady();
bool RecoveredGameServices_OrphanAttributesReady();
bool RecoveredGameServices_ArtefactAttributesReady();
bool RecoveredGameServices_SmokeAttributesReady();
bool RecoveredGameServices_SmokeSubjectReady();
bool RecoveredGameServices_SmokeTerrainReady();
bool RecoveredGameServices_SmokeRenderingReady();
bool RecoveredGameServices_SmokeVisualResourcesReady();
bool RecoveredGameServices_ExplosionAttributesReady();
bool RecoveredGameServices_ExplosionSubjectReady();
bool RecoveredGameServices_ExplosionImpulseReady();
bool RecoveredGameServices_ExplosionLightReady();
bool RecoveredGameServices_ExplosionSoundReady();
bool RecoveredGameServices_ExplosionParticlesReady();
bool RecoveredGameServices_ExplosionSmokeReady();
bool RecoveredGameServices_ExplosionPieceReady();
bool RecoveredGameServices_ExplosionTraceReady();
bool RecoveredGameServices_VehicleAttributesReady();
bool RecoveredGameServices_VehicleReferencesReady();
bool RecoveredGameServices_TaxiAttributesReady();
bool RecoveredGameServices_TaxiReferencesReady();
bool RecoveredGameServices_TaxiSubjectReady();
bool RecoveredGameServices_BulletAttributesReady();
bool RecoveredGameServices_BulletReferencesReady();
bool RecoveredGameServices_BulletSubjectRegistrationReady();
bool RecoveredGameServices_BulletSubjectReady();
bool RecoveredGameServices_BulletImpactEffectsReady();
bool RecoveredGameServices_BulletGroundSparkReady();
bool RecoveredGameServices_BulletBarrelSmokeReady();
bool RecoveredGameServices_FarterAttributesReady();
bool RecoveredGameServices_FarterReferencesReady();
bool RecoveredGameServices_FarterRuntimeReady();
bool RecoveredGameServices_FarterSubjectReady();
bool RecoveredGameServices_LampAttributesReady();
bool RecoveredGameServices_CorpseAttributesReady();
bool RecoveredGameServices_CorpseReferencesReady();
bool RecoveredGameServices_CorpseRuntimeReady();
bool RecoveredGameServices_CorpseSubjectReady();
bool RecoveredGameServices_SmokerAttributesReady();
bool RecoveredGameServices_SmokerReferencesReady();
bool RecoveredGameServices_SmokerRuntimeReady();
bool RecoveredGameServices_SmokerEmissionReady();
bool RecoveredGameServices_SmokerLightCoronaReady();
bool RecoveredGameServices_DynSmokerReady();
bool RecoveredGameServices_WavMetadataReady();
bool RecoveredGameServices_SoundObjectReady();
bool RecoveredGameServices_SkinResourcesReady();
bool RecoveredGameServices_SparkAttributesReady();
bool RecoveredGameServices_SparkSubjectReady();
bool RecoveredGameServices_SparkRenderingReady();
bool RecoveredGameServices_RouteReady();
bool RecoveredGameServices_VehicleReady();
double RecoveredGameServices_VehicleVesselMass();
bool RecoveredGameServices_VehicleMovementReady();
unsigned long long RecoveredGameServices_VehicleRuntimeFingerprint();
int RecoveredGameServices_VehicleVesselKind();
int RecoveredGameServices_VehicleProbeInvalidActivations();
int RecoveredGameServices_VehicleProbeActivations();
int RecoveredGameServices_VehicleProbeStationarySteps();
int RecoveredGameServices_VehicleProbeThrottleEvents();
int RecoveredGameServices_VehicleProbeMovementSteps();
int RecoveredGameServices_VehicleProbeTurnEvents();
int RecoveredGameServices_VehicleProbeCameraTransitions();
int RecoveredGameServices_VehicleProbeRollbacks();
double RecoveredGameServices_VehicleProbeHorizontalDistance();
bool RecoveredGameServices_TaxiVehicleTransitionReady();
int RecoveredGameServices_TaxiVehicleProbeAvailableTaxis();
int RecoveredGameServices_TaxiVehicleProbeInvalidTargets();
int RecoveredGameServices_TaxiVehicleProbeTransitions();
int RecoveredGameServices_TaxiVehicleProbeAttributeTransfers();
int RecoveredGameServices_TaxiVehicleProbePoseTransfers();
int RecoveredGameServices_TaxiVehicleProbePayloadTransfers();
int RecoveredGameServices_TaxiVehicleProbeRemovedTaxis();
int RecoveredGameServices_TaxiVehicleProbeRollbacks();
bool RecoveredGameServices_VehicleControlReady();
bool RecoveredGameServices_VehicleFallbackActive();
unsigned int RecoveredGameServices_VehicleInputEvents();
unsigned int RecoveredGameServices_VehicleForwardedEvents();
unsigned int RecoveredGameServices_VehicleHousekeepingEvents();
unsigned int RecoveredGameServices_VehicleIgnoredEvents();
bool RecoveredGameServices_SetApplicationActive(bool active);
bool RecoveredGameServices_VehicleApplicationActive();
unsigned int RecoveredGameServices_VehicleFocusLossCount();
unsigned int RecoveredGameServices_VehicleFocusGainCount();
unsigned int RecoveredGameServices_VehicleSyntheticReleaseCount();
unsigned int RecoveredGameServices_VehicleSuppressedInputCount();
unsigned int RecoveredGameServices_VehicleActiveActionCount();
int RecoveredGameServices_VehicleLastInputFailure();
bool RecoveredGameServices_VehicleDriveTelemetry(
    SRecoveredVehicleDriveTelemetry* telemetry);
unsigned int RecoveredGameServices_VehicleFrameCount();
unsigned int RecoveredGameServices_VehicleCameraFrameCount();
unsigned int RecoveredGameServices_VehicleDroppedTimeFrameCount();
unsigned int RecoveredGameServices_VehicleFallbackCount();
unsigned int RecoveredGameServices_VehicleFallbackReason();
bool RecoveredGameServices_QuitRequested();
bool RecoveredGameServices_IsReady();
unsigned int RecoveredGameServices_Issues();
const SRecoveredObserverState* RecoveredGameServices_ObserverState();
int RecoveredGameServices_RunFrame();
