#pragma once

#include "LevelContinuation.h"
#include "LevelSaveSlot.h"
#include "RecoveredFramePreview.h"

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
  RECOVERED_GAME_SERVICES_TAXI_VEHICLE_TRANSITION_FAILURE = 1u << 11,
  RECOVERED_GAME_SERVICES_VEHICLE_CONTROL_REPLAY_FAILURE = 1u << 12,
  RECOVERED_GAME_SERVICES_SAVE_MENU_FAILURE = 1u << 13
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

struct SRecoveredTaxiVehicleHandoffTelemetry {
  double nearestTaxiDistance;
  double activationDistance;
  double postTransitionDistance;
  unsigned int availableTaxis;
  unsigned int nearbyTaxis;
  unsigned int attempts;
  unsigned int pendingTransitions;
  unsigned int successfulTransitions;
  unsigned int noTargetAttempts;
  unsigned int removedTaxis;
  unsigned int panelOpenTransitions;
  unsigned int panelDraws;
  unsigned int postTransitionFrames;
  int panelReady;
  int panelOpen;
  int hardwareSubscriptionPreserved;
};

struct SRecoveredVehiclePrimaryFireTelemetry {
  unsigned int triggerPresses;
  unsigned int acceptedShots;
  unsigned int rolledBackShots;
  unsigned int moveEvents;
  unsigned int collisionChecks;
  unsigned int sceneImpacts;
  unsigned int dynamicImpacts;
  unsigned int waterlineSplashes;
  unsigned int impactEffectChildren;
  unsigned int groundRemovals;
  unsigned int barrelSmokeStarts;
  unsigned int liveBullets;
  unsigned int tablePeakLiveBullets;
  unsigned int maximumExplosionSubjects;
  unsigned int maximumParticleBranches;
  unsigned int maximumSmokeSubjects;
  unsigned int maximumSparkSubjects;
  unsigned int maximumSoundObjects;
  unsigned int renderedFramesAfterShot;
  unsigned int effectRenderFrames;
  int hardwareSubscriptionPreserved;
};

struct SRecoveredVehicleEmbodimentTelemetry {
  unsigned int exitAttempts;
  unsigned int safeExitCompletions;
  unsigned int unsafeExitCompletions;
  unsigned int droppedTaxis;
  unsigned int droppedOrphans;
  unsigned int reentryAttempts;
  unsigned int reentryCompletions;
  unsigned int panelCloseTransitions;
  unsigned int panelReopenTransitions;
  unsigned int orphanMoveEvents;
  unsigned int orphanImpacts;
  unsigned int orphanExplosions;
  unsigned int orphanSmokeStarts;
  unsigned int orphanRenderFrames;
  unsigned int liveOrphans;
  int exitPending;
  int hardwareSubscriptionPreserved;
};

struct SRecoveredVehicleControlJournalTelemetry {
  unsigned long long checkpointTick;
  unsigned long long lastRecordTick;
  unsigned long long journalFingerprint;
  unsigned int recordCount;
  unsigned int actionRecords;
  unsigned int focusRecords;
  unsigned int encodedBytes;
  unsigned int appendFailures;
  int recording;
  int applicationActive;
};

struct SRecoveredVehicleControlReplayTelemetry {
  unsigned long long journalFingerprint;
  unsigned long long recordedStateFingerprint;
  unsigned long long replayedStateFingerprint;
  unsigned int encodedBytes;
  int recordings;
  int replays;
  int codecRoundTrips;
  int actionRecords;
  int focusRecords;
  int syntheticReleases;
  int simulationFrames;
  int stateMatches;
  int clockMatches;
  int randomMatches;
  int rollbacks;
};

enum ERecoveredSaveMenuAction {
  RECOVERED_SAVE_MENU_NONE = 0,
  RECOVERED_SAVE_MENU_SAVE = 1,
  RECOVERED_SAVE_MENU_LOAD = 2
};

struct SRecoveredSaveMenuState {
  bool configured = false;
  bool nativeMenuInstalled = false;
  bool pending = false;
  ERecoveredSaveMenuAction pendingAction = RECOVERED_SAVE_MENU_NONE;
  std::uint32_t pendingSlot = 0;
  unsigned int saveRequests = 0;
  unsigned int loadRequests = 0;
  unsigned int completedSaves = 0;
  unsigned int completedLoads = 0;
  unsigned int failedCommands = 0;
  std::wstring directory;
  std::string lastError;
  SRecoveredFramePreviewSummary lastPreview;
  SLevelSaveSlotSummary lastSlot;
  SLevelContinuationSummary lastContinuation;
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
bool RecoveredGameServices_OrphanReferencesReady();
unsigned long long RecoveredGameServices_OrphanReferenceFingerprint();
bool RecoveredGameServices_OrphanSubjectReady();
int RecoveredGameServices_OrphanSubjectCapacity();
int RecoveredGameServices_OrphanSubjectCount();
unsigned long long RecoveredGameServices_OrphanSubjectFingerprint();
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
bool RecoveredGameServices_PeopleAttributesReady();
bool RecoveredGameServices_PeopleReferencesReady();
bool RecoveredGameServices_PeopleSubjectReady();
bool RecoveredGameServices_TankCannonAttributesReady();
bool RecoveredGameServices_TankReferencesReady();
bool RecoveredGameServices_TankCannonSubjectTablesReady();
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
bool RecoveredGameServices_TaxiVehicleHandoffTelemetry(
    SRecoveredTaxiVehicleHandoffTelemetry* telemetry);
bool RecoveredGameServices_VehicleEmbodimentTelemetry(
    SRecoveredVehicleEmbodimentTelemetry* telemetry);
bool RecoveredGameServices_BeginVehiclePrimaryFireObservation();
bool RecoveredGameServices_VehiclePrimaryFireTelemetry(
    SRecoveredVehiclePrimaryFireTelemetry* telemetry);
bool RecoveredGameServices_VehicleControlReady();
bool RecoveredGameServices_VehicleControlReplayReady();
bool RecoveredGameServices_VehicleControlReplayTelemetry(
    SRecoveredVehicleControlReplayTelemetry* telemetry);
bool RecoveredGameServices_VehicleControlJournalTelemetry(
    SRecoveredVehicleControlJournalTelemetry* telemetry);
bool RecoveredGameServices_CaptureLevelContinuation(
    std::vector<std::uint8_t>* bytes,
    SLevelContinuationSummary* summary);
bool RecoveredGameServices_RestoreLevelContinuation(
    const std::vector<std::uint8_t>& bytes,
    SLevelContinuationSummary* summary);
const char* RecoveredGameServices_LastLevelContinuationError();
bool RecoveredGameServices_SaveLevelSlot(
    const std::wstring& directory, std::uint32_t slot,
    const std::string& title, const std::string& description,
    const std::vector<std::uint8_t>& previewPng,
    SLevelSaveSlotSummary* slotSummary,
    SLevelContinuationSummary* continuationSummary);
bool RecoveredGameServices_LoadLevelSlot(
    const std::wstring& directory, std::uint32_t slot,
    SLevelSaveSlotSummary* slotSummary,
    SLevelContinuationSummary* continuationSummary);
const char* RecoveredGameServices_LastLevelSaveSlotError();
bool RecoveredGameServices_ConfigureSaveDirectory(
    const std::wstring& directory);
bool RecoveredGameServices_RequestSaveSlot(
    std::uint32_t slot, bool allowOverwrite);
bool RecoveredGameServices_RequestLoadSlot(std::uint32_t slot);
bool RecoveredGameServices_ProcessPendingSaveCommand(
    SLevelSaveSlotSummary* slotSummary = nullptr,
    SLevelContinuationSummary* continuationSummary = nullptr);
const SRecoveredSaveMenuState* RecoveredGameServices_SaveMenuState();
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
int RecoveredGameServices_VehicleLastFrameFailure();
int RecoveredGameServices_VehicleLastFrameReadinessIssue();
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
