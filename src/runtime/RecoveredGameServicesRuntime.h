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
  RECOVERED_GAME_SERVICES_SEANCE_FAILURE = 1u << 8
};

struct SRecoveredObserverState {
  double x;
  double y;
  double z;
  double yaw;
  double pitch;
  unsigned int inputEvents;
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
bool RecoveredGameServices_ExplosionAttributesReady();
bool RecoveredGameServices_FarterAttributesReady();
bool RecoveredGameServices_LampAttributesReady();
bool RecoveredGameServices_CorpseAttributesReady();
bool RecoveredGameServices_SmokerAttributesReady();
bool RecoveredGameServices_WavMetadataReady();
bool RecoveredGameServices_SkinResourcesReady();
bool RecoveredGameServices_SparkAttributesReady();
bool RecoveredGameServices_RouteReady();
bool RecoveredGameServices_VehicleReady();
bool RecoveredGameServices_QuitRequested();
bool RecoveredGameServices_IsReady();
unsigned int RecoveredGameServices_Issues();
const SRecoveredObserverState* RecoveredGameServices_ObserverState();
int RecoveredGameServices_RunFrame();
