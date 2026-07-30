#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define LAST_H__SCENE
#include "game.h"
#include "dmap.h"
#include "enum/SpaceEnum.h"
#include "graph.h"
#include "hardware.h"
#include "h/cachesmoke.h"
#include "h/light.h"
#include "h/super.h"
#include "h/vehicle.h"
#include "kernel/h/session.h"
#include "message/bulmsg.h"
#include "message/fountmsg.h"
#include "message/hardmsg.h"
#include "message/explmsg.h"
#include "message/levelmsg.h"
#include "message/smokermsg.h"
#include "message/sparkmsg.h"
#include "obase/corpse/CorpseAttributeState.h"
#include "obase/corpse/CorpseSubjectState.h"
#include "obase/bullet/BulletAttributeState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/farter/FarterSubjectState.h"
#include "obase/lamp/LampAttributeState.h"
#include "obase/orphan/OrphanSubjectState.h"
#include "obase/spark/SparkAttributeState.h"
#include "obase/spark/SparkSubjectState.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/taxi/TaxiAttributeState.h"
#include "obase/taxi/Taxi.h"
#include "obase/taxi/TaxiSubjectState.h"
#include "obase/vehicle/VehicleAttributeState.h"
#include "obase/vehicle/VehicleRuntimeState.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "sound.h"
#include "suavik.h"

#include "FrameRuntimeState.h"
#include "GameEntryRuntimeState.h"
#include "RecoveredArenaSeanceRuntime.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredGameServicesRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredRetailScriptManifest.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

namespace {

const unsigned long long kBulletCacheHashOffset = 14695981039346656037ull;
const unsigned long long kBulletCacheHashPrime = 1099511628211ull;

void (*g_originalAlphaSprite)(SGRAlphaSprite*) = nullptr;
void (*g_originalSprite)(int, int, int, int, int, int, int, int,
                         int, void*) = nullptr;
void (*g_originalParticle)(int, int, int, int, unsigned long) = nullptr;
GR_HTEXTURE g_expectedAlphaTexture = nullptr;
GR_HTEXTURE g_expectedSpriteTexture = nullptr;
int g_alphaSpriteDraws = 0;
int g_spriteDraws = 0;
int g_particleDraws = 0;
bool g_alphaSpriteDrawValid = true;
bool g_spriteDrawValid = true;
bool g_particleDrawValid = true;
SGRAlphaSprite g_lastAlphaSprite = {};
int g_lastSpriteU0 = 0;
int g_lastSpriteV0 = 0;
int g_lastSpriteU1 = 0;
int g_lastSpriteV1 = 0;

void HashBulletCacheBytes(unsigned long long& hash, const void* data,
                          int size) {
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  for (int index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= kBulletCacheHashPrime;
  }
}

bool HashBulletCache(KR_ObjectID objectID, void* user) {
  unsigned long long* hash = static_cast<unsigned long long*>(user);
  AttributeBullet* attribute = static_cast<AttributeBullet*>(
      __bulletAttrTable.searchAttribute(objectID));
  if (attribute == nullptr) return false;
#define RR2NW_HASH_BULLET_CACHE(field) \
  HashBulletCacheBytes(*hash, &attribute->field, sizeof(attribute->field))
  RR2NW_HASH_BULLET_CACHE(m_cacheImage);
  RR2NW_HASH_BULLET_CACHE(m_cacheImageFront);
  RR2NW_HASH_BULLET_CACHE(m_wav);
  RR2NW_HASH_BULLET_CACHE(m_ctsndID);
  RR2NW_HASH_BULLET_CACHE(m_colorGrad);
  RR2NW_HASH_BULLET_CACHE(m_smokeTableID);
  RR2NW_HASH_BULLET_CACHE(m_smokeAttrID);
  RR2NW_HASH_BULLET_CACHE(m_cacheSparkAttrTable);
  RR2NW_HASH_BULLET_CACHE(m_cacheSparkTable);
  RR2NW_HASH_BULLET_CACHE(m_cacheColor);
  RR2NW_HASH_BULLET_CACHE(m_cacheSparkAttr);
  RR2NW_HASH_BULLET_CACHE(m_cacheOutSparkAttr);
  RR2NW_HASH_BULLET_CACHE(m_cacheSplashAttr);
  RR2NW_HASH_BULLET_CACHE(m_cacheExplAttr);
  RR2NW_HASH_BULLET_CACHE(m_cacheExplosionTable);
  RR2NW_HASH_BULLET_CACHE(m_cacheSkin);
#undef RR2NW_HASH_BULLET_CACHE
  return true;
}

unsigned long long BulletCacheFingerprint() {
  unsigned long long hash = kBulletCacheHashOffset;
  __bulletAttrTable.userFind(HashBulletCache, &hash);
  return hash;
}

bool FindFirstBullet(KR_ObjectID objectID, void* user) {
  AttributeBullet** result = static_cast<AttributeBullet**>(user);
  if (*result == nullptr) {
    *result = static_cast<AttributeBullet*>(
        __bulletAttrTable.searchAttribute(objectID));
  }
  return true;
}

void CaptureAlphaSprite(SGRAlphaSprite* sprite) {
  if (sprite != nullptr && sprite->hTexture == g_expectedAlphaTexture) {
    ++g_alphaSpriteDraws;
    g_lastAlphaSprite = *sprite;
    g_alphaSpriteDrawValid =
        g_alphaSpriteDrawValid && sprite->x1 > sprite->x0 &&
        sprite->y1 > sprite->y0 && sprite->opacity > 0 &&
        sprite->opacity <= 255 && sprite->iz > 0;
  }
  if (g_originalAlphaSprite != nullptr) g_originalAlphaSprite(sprite);
}

class ScopedAlphaSpriteCapture {
 public:
  explicit ScopedAlphaSpriteCapture(GR_HTEXTURE expected) {
    g_expectedAlphaTexture = expected;
    g_alphaSpriteDraws = 0;
    g_alphaSpriteDrawValid = true;
    std::memset(&g_lastAlphaSprite, 0, sizeof(g_lastAlphaSprite));
    g_originalAlphaSprite = _pGRDrawAlphaSprite;
    _pGRDrawAlphaSprite = CaptureAlphaSprite;
  }

  ~ScopedAlphaSpriteCapture() {
    _pGRDrawAlphaSprite = g_originalAlphaSprite;
    g_originalAlphaSprite = nullptr;
    g_expectedAlphaTexture = nullptr;
  }
};

void CaptureSprite(int x0, int y0, int x1, int y1,
                   int u0, int v0, int u1, int v1,
                   int inverseZ, void* texture) {
  if (texture == g_expectedSpriteTexture) {
    ++g_spriteDraws;
    g_lastSpriteU0 = u0;
    g_lastSpriteV0 = v0;
    g_lastSpriteU1 = u1;
    g_lastSpriteV1 = v1;
    g_spriteDrawValid = g_spriteDrawValid && x1 > x0 && y1 > y0 &&
                        u1 > u0 && v1 > v0 && inverseZ > 0;
  }
  if (g_originalSprite != nullptr) {
    g_originalSprite(x0, y0, x1, y1, u0, v0, u1, v1,
                     inverseZ, texture);
  }
}

class ScopedSpriteCapture {
 public:
  explicit ScopedSpriteCapture(GR_HTEXTURE expected) {
    g_expectedSpriteTexture = expected;
    g_spriteDraws = 0;
    g_spriteDrawValid = true;
    g_lastSpriteU0 = 0;
    g_lastSpriteV0 = 0;
    g_lastSpriteU1 = 0;
    g_lastSpriteV1 = 0;
    g_originalSprite = _pGRDrawSprite;
    _pGRDrawSprite = CaptureSprite;
  }

  ~ScopedSpriteCapture() {
    _pGRDrawSprite = g_originalSprite;
    g_originalSprite = nullptr;
    g_expectedSpriteTexture = nullptr;
  }
};

void CaptureParticle(int x, int y, int size, int inverseZ,
                     unsigned long color) {
  ++g_particleDraws;
  g_particleDrawValid = g_particleDrawValid && size > 0 &&
                        inverseZ > 0;
  if (g_originalParticle != nullptr) {
    g_originalParticle(x, y, size, inverseZ, color);
  }
}

class ScopedParticleCapture {
 public:
  ScopedParticleCapture() {
    g_particleDraws = 0;
    g_particleDrawValid = true;
    g_originalParticle = _pGRDrawParticle;
    _pGRDrawParticle = CaptureParticle;
  }

  ~ScopedParticleCapture() {
    _pGRDrawParticle = g_originalParticle;
    g_originalParticle = nullptr;
  }
};

int Fail(const char* message) {
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  std::fprintf(
      stderr,
      "game-services-runtime-smoke: %s (services=%u entry=%u missing=%u "
      "platform=%d session=%d loop=%d hardware=%d seance=%d bird=%d "
      "portal=%d orphan=%d artefact=%d smoke=%d explosion=%d "
      "explosion_piece=%d explosion_trace=%d "
      "vehicle_attrs=%d vehicle_refs=%d taxi=%d taxi_refs=%d "
      "bullet=%d bullet_refs=%d "
      "bullet_ground_spark=%d bullet_barrel_smoke=%d smoker=%d "
      "dyn_smoker=%d smoker_emission=%d smoker_light_corona=%d "
      "farter=%d lamp=%d corpse=%d corpse_subject=%d "
      "wav=%d sound=%d skin=%d spark=%d "
      "route=%d vehicle=%d vehicle_move=%d "
      "taxi_subject=%d taxi_shape=%d/%d/%d taxi_transition=%d/%d/%d/%d/%d/%d/%d/%d/%d "
      "arena_issues=%llu arena_extended_issues=%llu arena_error=%s "
      "level=%d graph=%d "
      "frame=%u context=%p publisher=%p timer=%p scene=%p current=%p "
      "bush=%d observer_events=%u observer_z=%lg)\n",
      message, RecoveredGameServices_Issues(), GameEntry_RuntimeIssues(),
      GameEntry_RuntimeMissingHooks(),
      RecoveredGameServices_PlatformReady() ? 1 : 0,
      RecoveredGameServices_SessionReady() ? 1 : 0,
      RecoveredGameServices_LoopReady() ? 1 : 0,
      RecoveredGameServices_HardwareReady() ? 1 : 0,
      RecoveredGameServices_SeanceReady() ? 1 : 0,
      RecoveredGameServices_BirdAttributesReady() ? 1 : 0,
      RecoveredGameServices_PortalReady() ? 1 : 0,
      RecoveredGameServices_OrphanAttributesReady() ? 1 : 0,
      RecoveredGameServices_ArtefactAttributesReady() ? 1 : 0,
      RecoveredGameServices_SmokeAttributesReady() ? 1 : 0,
      RecoveredGameServices_ExplosionAttributesReady() ? 1 : 0,
      RecoveredGameServices_ExplosionPieceReady() ? 1 : 0,
      RecoveredGameServices_ExplosionTraceReady() ? 1 : 0,
      RecoveredGameServices_VehicleAttributesReady() ? 1 : 0,
      RecoveredGameServices_VehicleReferencesReady() ? 1 : 0,
      RecoveredGameServices_TaxiAttributesReady() ? 1 : 0,
      RecoveredGameServices_TaxiReferencesReady() ? 1 : 0,
      RecoveredGameServices_BulletAttributesReady() ? 1 : 0,
      RecoveredGameServices_BulletReferencesReady() ? 1 : 0,
      RecoveredGameServices_BulletGroundSparkReady() ? 1 : 0,
      RecoveredGameServices_BulletBarrelSmokeReady() ? 1 : 0,
      RecoveredGameServices_SmokerAttributesReady() ? 1 : 0,
      RecoveredGameServices_DynSmokerReady() ? 1 : 0,
      RecoveredGameServices_SmokerEmissionReady() ? 1 : 0,
      RecoveredGameServices_SmokerLightCoronaReady() ? 1 : 0,
      RecoveredGameServices_FarterAttributesReady() ? 1 : 0,
      RecoveredGameServices_LampAttributesReady() ? 1 : 0,
      RecoveredGameServices_CorpseAttributesReady() ? 1 : 0,
      RecoveredGameServices_CorpseSubjectReady() ? 1 : 0,
      RecoveredGameServices_WavMetadataReady() ? 1 : 0,
      RecoveredGameServices_SoundObjectReady() ? 1 : 0,
      RecoveredGameServices_SkinResourcesReady() ? 1 : 0,
      RecoveredGameServices_SparkAttributesReady() ? 1 : 0,
      RecoveredGameServices_RouteReady() ? 1 : 0,
      RecoveredGameServices_VehicleReady() ? 1 : 0,
      RecoveredGameServices_VehicleMovementReady() ? 1 : 0,
      RecoveredGameServices_TaxiSubjectReady() ? 1 : 0,
      RecoveredArenaSeance_TaxiSubjectCount(),
      RecoveredArenaSeance_TaxiSubjectCapacity(),
      RecoveredArenaSeance_TaxiSubjectSoundCount(),
      RecoveredGameServices_TaxiVehicleTransitionReady() ? 1 : 0,
      RecoveredGameServices_TaxiVehicleProbeAvailableTaxis(),
      RecoveredGameServices_TaxiVehicleProbeInvalidTargets(),
      RecoveredGameServices_TaxiVehicleProbeTransitions(),
      RecoveredGameServices_TaxiVehicleProbeAttributeTransfers(),
      RecoveredGameServices_TaxiVehicleProbePoseTransfers(),
      RecoveredGameServices_TaxiVehicleProbePayloadTransfers(),
      RecoveredGameServices_TaxiVehicleProbeRemovedTaxis(),
      RecoveredGameServices_TaxiVehicleProbeRollbacks(),
      RecoveredArenaSeance_Issues(), RecoveredArenaSeance_ExtendedIssues(),
      RecoveredArenaSeance_LastError(),
      RecoveredGameLevel_IsReady() ? 1 : 0,
      RecoveredSoftwareGraph_IsReady() ? 1 : 0, Frame_RuntimeIssues(),
      static_cast<void*>(g_super.m_context),
      static_cast<void*>(g_super.m_publisher),
      static_cast<void*>(Session::m_realTimer), static_cast<void*>(pScene),
      static_cast<void*>(CViewScene::Current()),
      bush_IsInitialized() ? 1 : 0,
      observer != nullptr ? observer->inputEvents : 0,
      observer != nullptr ? observer->z : 0.0);
  return EXIT_FAILURE;
}

bool IsServiceReleased() {
  return !RecoveredGameServices_PlatformReady() &&
         !RecoveredGameServices_SessionReady() &&
         !RecoveredGameServices_LoopReady() &&
         !RecoveredGameServices_HardwareReady() &&
         !RecoveredGameServices_SeanceReady() &&
         !RecoveredGameServices_BirdAttributesReady() &&
         !RecoveredGameServices_PortalReady() &&
         !RecoveredGameServices_OrphanAttributesReady() &&
         !RecoveredGameServices_OrphanReferencesReady() &&
         RecoveredGameServices_OrphanReferenceFingerprint() == 0 &&
         !RecoveredGameServices_OrphanSubjectReady() &&
         RecoveredGameServices_OrphanSubjectCapacity() == 0 &&
         RecoveredGameServices_OrphanSubjectCount() == 0 &&
         RecoveredGameServices_OrphanSubjectFingerprint() == 0 &&
         !RecoveredGameServices_ArtefactAttributesReady() &&
         !RecoveredGameServices_SmokeAttributesReady() &&
         !RecoveredGameServices_SmokeSubjectReady() &&
         !RecoveredGameServices_SmokeTerrainReady() &&
         !RecoveredGameServices_SmokeRenderingReady() &&
         !RecoveredGameServices_SmokeVisualResourcesReady() &&
         RecoveredArenaSeance_SmokeSubjectCapacity() == 0 &&
         RecoveredArenaSeance_SmokeSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_SmokeVisualResourceFingerprint() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         !RecoveredGameServices_ExplosionAttributesReady() &&
         !RecoveredGameServices_ExplosionSubjectReady() &&
         !RecoveredGameServices_ExplosionImpulseReady() &&
         !RecoveredGameServices_ExplosionLightReady() &&
         !RecoveredGameServices_ExplosionSoundReady() &&
         !RecoveredGameServices_ExplosionParticlesReady() &&
         !RecoveredGameServices_ExplosionSmokeReady() &&
         !RecoveredGameServices_ExplosionPieceReady() &&
         !RecoveredGameServices_ExplosionTraceReady() &&
         RecoveredArenaSeance_ExplosionSoundReferenceFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionSoundProbeStarted() == -1 &&
         RecoveredArenaSeance_ExplosionSoundProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionSoundProbeRollbacks() == -1 &&
         RecoveredArenaSeance_ExplosionParticleVisualFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionParticleProbeStartedBranches() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeRays() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeMoveSteps() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeExpiredParents() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeVisualFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites() == -1 &&
         RecoveredArenaSeance_ExplosionPieceReferenceFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeMoveSteps() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeExpiredParents() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces() == -1 &&
         RecoveredArenaSeance_ExplosionTraceReferenceFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionTraceProbeStartedPieces() == -1 &&
         RecoveredArenaSeance_ExplosionTraceProbeQuotaGateSkips() == -1 &&
         RecoveredArenaSeance_ExplosionTraceProbePuffEvents() == -1 &&
         RecoveredArenaSeance_ExplosionTraceProbeSmokeChildren() == -1 &&
         RecoveredArenaSeance_ExplosionTraceProbeMoveSteps() == -1 &&
         RecoveredArenaSeance_ExplosionTraceProbeExpiredParents() == -1 &&
         RecoveredArenaSeance_ExplosionTraceProbeRolledBackPieces() == -1 &&
         RecoveredArenaSeance_ExplosionSubjectCapacity() == 0 &&
         RecoveredArenaSeance_ExplosionSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionProbeInvalidStarts() == -1 &&
         RecoveredArenaSeance_ExplosionProbeAllocationRollbacks() == -1 &&
         RecoveredArenaSeance_ExplosionProbeQueuedCommands() == -1 &&
         RecoveredArenaSeance_ExplosionProbeQueueRollbacks() == -1 &&
         RecoveredArenaSeance_ExplosionProbeExecutedCommands() == -1 &&
         RecoveredArenaSeance_ExplosionProbeDamageApplications() == -1 &&
         ExplosionSubjectState_LiveCount() == 0 &&
         ExplosionSubjectState_ParticleBranchLiveCount() == 0 &&
         ExplosionSubjectState_TracedParentCount() == 0 &&
         !RecoveredGameServices_VehicleAttributesReady() &&
         !RecoveredGameServices_VehicleReferencesReady() &&
         RecoveredGameServices_VehicleVesselMass() == 0.0 &&
         !RecoveredGameServices_VehicleMovementReady() &&
         RecoveredGameServices_VehicleRuntimeFingerprint() == 0 &&
         RecoveredGameServices_VehicleVesselKind() ==
             RECOVERED_VEHICLE_VESSEL_UNKNOWN &&
         RecoveredGameServices_VehicleProbeInvalidActivations() == -1 &&
         RecoveredGameServices_VehicleProbeActivations() == -1 &&
         RecoveredGameServices_VehicleProbeStationarySteps() == -1 &&
         RecoveredGameServices_VehicleProbeThrottleEvents() == -1 &&
         RecoveredGameServices_VehicleProbeMovementSteps() == -1 &&
         RecoveredGameServices_VehicleProbeTurnEvents() == -1 &&
         RecoveredGameServices_VehicleProbeCameraTransitions() == -1 &&
         RecoveredGameServices_VehicleProbeRollbacks() == -1 &&
         RecoveredGameServices_VehicleProbeHorizontalDistance() == 0.0 &&
         !RecoveredGameServices_VehicleControlReady() &&
         !RecoveredGameServices_VehicleControlReplayReady() &&
         !RecoveredGameServices_VehicleControlReplayTelemetry(nullptr) &&
         !RecoveredGameServices_VehicleControlJournalTelemetry(nullptr) &&
         !RecoveredGameServices_VehicleFallbackActive() &&
         RecoveredGameServices_VehicleInputEvents() == 0 &&
         RecoveredGameServices_VehicleForwardedEvents() == 0 &&
         RecoveredGameServices_VehicleHousekeepingEvents() == 0 &&
         RecoveredGameServices_VehicleIgnoredEvents() == 0 &&
         RecoveredGameServices_VehicleLastInputFailure() == 0 &&
         RecoveredGameServices_VehicleFrameCount() == 0 &&
         RecoveredGameServices_VehicleCameraFrameCount() == 0 &&
         RecoveredGameServices_VehicleDroppedTimeFrameCount() == 0 &&
         RecoveredGameServices_VehicleFallbackCount() == 0 &&
         RecoveredGameServices_VehicleFallbackReason() == 0 &&
         VehicleRuntimeState_IsClean(nullptr) &&
         RecoveredArenaSeance_VehicleAttributeCount() == -1 &&
         RecoveredArenaSeance_VehicleAttributeCapacity() == 0 &&
         RecoveredArenaSeance_VehicleAttributeFingerprint() == 0 &&
         RecoveredArenaSeance_VehicleReferenceFingerprint() == 0 &&
          !RecoveredGameServices_TaxiAttributesReady() &&
          !RecoveredGameServices_TaxiReferencesReady() &&
          !RecoveredGameServices_TaxiSubjectReady() &&
          RecoveredArenaSeance_TaxiAttributeCount() == -1 &&
          RecoveredArenaSeance_TaxiAttributeCapacity() == 0 &&
          RecoveredArenaSeance_TaxiAttributeFingerprint() == 0 &&
          RecoveredArenaSeance_TaxiReferenceFingerprint() == 0 &&
          RecoveredArenaSeance_TaxiSubjectCapacity() == 0 &&
          RecoveredArenaSeance_TaxiSubjectCount() == 0 &&
          RecoveredArenaSeance_TaxiSubjectSoundCount() == 0 &&
          RecoveredArenaSeance_TaxiSubjectFingerprint() == 0 &&
          RecoveredArenaSeance_TaxiProbeInvalidStarts() == 0 &&
          RecoveredArenaSeance_TaxiProbeValidStarts() == 0 &&
          RecoveredArenaSeance_TaxiProbeRenderReady() == 0 &&
          RecoveredArenaSeance_TaxiProbeSoundReady() == 0 &&
          RecoveredArenaSeance_TaxiProbeRollbacks() == 0 &&
          TaxiSubjectState_Capacity() == 0 &&
          TaxiSubjectState_LiveCount() == 0 &&
          TaxiSubjectState_SoundCount() == 0 &&
          !RecoveredGameServices_TaxiVehicleTransitionReady() &&
          RecoveredGameServices_TaxiVehicleProbeAvailableTaxis() == -1 &&
          RecoveredGameServices_TaxiVehicleProbeInvalidTargets() == -1 &&
          RecoveredGameServices_TaxiVehicleProbeTransitions() == -1 &&
          RecoveredGameServices_TaxiVehicleProbeAttributeTransfers() == -1 &&
          RecoveredGameServices_TaxiVehicleProbePoseTransfers() == -1 &&
          RecoveredGameServices_TaxiVehicleProbePayloadTransfers() == -1 &&
          RecoveredGameServices_TaxiVehicleProbeRemovedTaxis() == -1 &&
          RecoveredGameServices_TaxiVehicleProbeRollbacks() == -1 &&
         !RecoveredGameServices_BulletAttributesReady() &&
         !RecoveredGameServices_BulletReferencesReady() &&
         !RecoveredGameServices_BulletSubjectRegistrationReady() &&
         !RecoveredGameServices_BulletSubjectReady() &&
         !RecoveredGameServices_BulletImpactEffectsReady() &&
         !RecoveredGameServices_BulletGroundSparkReady() &&
         !RecoveredGameServices_BulletBarrelSmokeReady() &&
         RecoveredArenaSeance_BulletAttributeCount() == -1 &&
         RecoveredArenaSeance_BulletAttributeCapacity() == 0 &&
         RecoveredArenaSeance_BulletAttributeFingerprint() == 0 &&
         RecoveredArenaSeance_BulletReferenceFingerprint() == 0 &&
         RecoveredArenaSeance_BulletSubjectCapacity() == 0 &&
         RecoveredArenaSeance_BulletSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_BulletSubjectProbeMoveCount() == -1 &&
         RecoveredArenaSeance_BulletCollisionScheduledChecks() == -1 &&
         RecoveredArenaSeance_BulletCollisionExecutedChecks() == -1 &&
         RecoveredArenaSeance_BulletCollisionSphereCases() == -1 &&
         RecoveredArenaSeance_BulletCollisionEarliestHitCases() == -1 &&
         RecoveredArenaSeance_BulletCollisionWaterlineCases() == -1 &&
         RecoveredArenaSeance_BulletCollisionSceneQueries() == -1 &&
         RecoveredArenaSeance_BulletEffectQueuedBatches() == -1 &&
         RecoveredArenaSeance_BulletEffectQueuedChildren() == -1 &&
         RecoveredArenaSeance_BulletEffectSplashFirstCases() == -1 &&
         RecoveredArenaSeance_BulletEffectRolledBackChildren() == -1 &&
         RecoveredArenaSeance_BulletGroundSparkQueued() == -1 &&
         RecoveredArenaSeance_BulletGroundSparkRolledBack() == -1 &&
         RecoveredArenaSeance_BulletBarrelSmokeThresholdStarts() == -1 &&
         RecoveredArenaSeance_BulletBarrelSmokeFrameGateSkips() == -1 &&
         RecoveredArenaSeance_BulletBarrelSmokeAttributeGateSkips() == -1 &&
         RecoveredArenaSeance_BulletBarrelSmokeRollbacks() == -1 &&
         BulletSubjectState_LiveCount() == 0 &&
         !RecoveredGameServices_SmokerAttributesReady() &&
         !RecoveredGameServices_SmokerReferencesReady() &&
         !RecoveredGameServices_SmokerRuntimeReady() &&
         !RecoveredGameServices_SmokerEmissionReady() &&
         !RecoveredGameServices_SmokerLightCoronaReady() &&
         RecoveredArenaSeance_SmokerReferenceFingerprint() == 0 &&
         !RecoveredGameServices_FarterAttributesReady() &&
         !RecoveredGameServices_FarterReferencesReady() &&
         !RecoveredGameServices_FarterRuntimeReady() &&
         !RecoveredGameServices_FarterSubjectReady() &&
         RecoveredArenaSeance_FarterSubjectCapacity() == 0 &&
         RecoveredArenaSeance_FarterSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_FarterScriptObjectCount() == -1 &&
         RecoveredArenaSeance_FarterLiveObjectCount() == -1 &&
         RecoveredArenaSeance_FarterSoundObjectCount() == -1 &&
         RecoveredArenaSeance_FarterNearFrameAudibleCount() == -1 &&
         RecoveredArenaSeance_FarterFarFrameAudibleCount() == -1 &&
         !RecoveredArenaSeance_FarterAudibleFrameTransition() &&
         !RecoveredArenaSeance_SoundDistanceReady() &&
         RecoveredArenaSeance_SoundDistance() == 0.0 &&
         RecoveredArenaSeance_SoundDistanceSquared() == 0.0 &&
         FarterSubjectState_LiveCount() == 0 &&
         !RecoveredGameServices_LampAttributesReady() &&
         !RecoveredGameServices_CorpseAttributesReady() &&
         !RecoveredGameServices_CorpseReferencesReady() &&
         !RecoveredGameServices_CorpseRuntimeReady() &&
         !RecoveredGameServices_CorpseSubjectReady() &&
         RecoveredArenaSeance_CorpseSubjectCapacity() == 0 &&
         RecoveredArenaSeance_CorpseSubjectFingerprint() == 0 &&
         CorpseSubjectState_LiveCount() == 0 &&
         !RecoveredGameServices_DynSmokerReady() &&
         RecoveredArenaSeance_DynSmokerCapacity() == 0 &&
         RecoveredArenaSeance_DynSmokerFingerprint() == 0 &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         !RecoveredGameServices_WavMetadataReady() &&
         !RecoveredGameServices_SoundObjectReady() &&
         RecoveredArenaSeance_SoundObjectCapacity() == 0 &&
         RecoveredArenaSeance_SoundObjectFingerprint() == 0 &&
         SoundObjectState_LiveCount() == 0 &&
         !RecoveredGameServices_SkinResourcesReady() &&
         !RecoveredGameServices_SparkAttributesReady() &&
         !RecoveredGameServices_SparkSubjectReady() &&
         !RecoveredGameServices_SparkRenderingReady() &&
         !RecoveredArenaSeance_SparkVisualResourcesReady() &&
         RecoveredArenaSeance_SparkSubjectCapacity() == 0 &&
         RecoveredArenaSeance_SparkSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_SparkVisualResourceFingerprint() == 0 &&
         RecoveredArenaSeance_SparkProbeInvalidStarts() == -1 &&
         RecoveredArenaSeance_SparkProbeQueuedCreates() == -1 &&
         RecoveredArenaSeance_SparkProbeQueueRollbacks() == -1 &&
         RecoveredArenaSeance_SparkProbePhaseTransitions() == -1 &&
         RecoveredArenaSeance_SparkProbeExpirations() == -1 &&
         !RecoveredGameServices_RouteReady() &&
         !RecoveredGameServices_PeopleAttributesReady() &&
         !RecoveredGameServices_PeopleReferencesReady() &&
         !RecoveredGameServices_PeopleSubjectReady() &&
         RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs() == -1 &&
         RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents() == -1 &&
         RecoveredArenaSeance_PeopleActiveWorldRollbacks() == -1 &&
         RecoveredArenaSeance_PeopleActiveWorldFingerprint() == 0 &&
         !RecoveredGameServices_TankCannonAttributesReady() &&
         !RecoveredGameServices_TankReferencesReady() &&
         !RecoveredGameServices_TankCannonSubjectTablesReady() &&
         RecoveredArenaSeance_TankProbeAvailable() == -1 &&
         RecoveredArenaSeance_TankProbeValidStarts() == -1 &&
         RecoveredArenaSeance_TankProbeDynamicReady() == -1 &&
         RecoveredArenaSeance_TankProbeRenderReady() == -1 &&
         RecoveredArenaSeance_TankProbeCannonReady() == -1 &&
         RecoveredArenaSeance_TankProbeScheduledMoves() == -1 &&
         RecoveredArenaSeance_TankProbeBulletDamageApplications() == -1 &&
         RecoveredArenaSeance_TankProbeDeathTransitions() == -1 &&
         RecoveredArenaSeance_TankProbeDeathEffects() == -1 &&
         RecoveredArenaSeance_TankProbeSaveStateRoundTrips() == -1 &&
         RecoveredArenaSeance_TankProbeRollbacks() == -1 &&
         !RecoveredArenaSeance_CommanderReady() &&
         RecoveredArenaSeance_CommanderCapacity() == -1 &&
         RecoveredArenaSeance_CommanderCount() == -1 &&
         RecoveredArenaSeance_CommanderHostileLinks() == -1 &&
         RecoveredArenaSeance_CommanderFingerprint() == 0 &&
         !RecoveredArenaSeance_MissionTankLifecycleReady() &&
         RecoveredArenaSeance_TankGroupSubjectCapacity() == -1 &&
         RecoveredArenaSeance_MissionTankAvailable() == -1 &&
         RecoveredArenaSeance_MissionTankSpawns() == -1 &&
         RecoveredArenaSeance_MissionTankMembershipLinks() == -1 &&
         RecoveredArenaSeance_MissionTankFindEnemyCycles() == -1 &&
         RecoveredArenaSeance_MissionTankMovingCycles() == -1 &&
         RecoveredArenaSeance_MissionTankStableRoundTrips() == -1 &&
         RecoveredArenaSeance_MissionTankReconstructedIDs() == -1 &&
         RecoveredArenaSeance_MissionTankRollbacks() == -1 &&
         RecoveredArenaSeance_MissionTankFingerprint() == 0 &&
         !RecoveredGameServices_VehicleReady() &&
         !RecoveredArenaSeance_IsOpen() && g_vehicle == nullptr &&
         RecoveredGameServices_ObserverState() == nullptr &&
         g_super.m_context == nullptr && g_super.m_publisher == nullptr &&
         Session::m_realTimer == nullptr && Session::m_hardware == nullptr &&
         g_hardware.getContext() == nullptr;
}

bool IsLevelRolledBack() {
  return !RecoveredGameLevel_IsReady() &&
         !RecoveredLevelRuntime_IsPrepared() &&
         !RecoveredRetailScriptManifest_IsReady() &&
         !RecoveredLevelAssets_IsReady() &&
         !RecoveredDrawableScene_IsReady() && pScene == nullptr &&
         CViewScene::Current() == nullptr && !CViewScene::IsBuilding() &&
         CViewOrdered::GetCurrentTop() == nullptr && !bush_IsInitialized();
}

bool StartServices(const char* directory) {
  if (!ZAV_InitLevel(directory)) return false;
  PIN_InitEverything();
  SUA_InitEverything();
  ZAV_BeginLoop();
  return RecoveredGameServices_IsReady();
}

bool IsVehicleControlActive(KR_ObjectID vehicleID,
                            SRecoveredVehicleRuntimeState* state) {
  SRecoveredVehicleRuntimeState current = {};
  if (g_super.m_context == nullptr || vehicleID.isNUL() ||
      !RecoveredGameServices_VehicleControlReady() ||
      RecoveredGameServices_VehicleFallbackActive() ||
      RecoveredGameServices_VehicleIgnoredEvents() != 0 ||
      RecoveredGameServices_VehicleFallbackCount() != 0 ||
      !VehicleRuntimeState_Inspect(
          g_super.m_context, vehicleID, &current) ||
      !current.active || current.frameBegun) {
    return false;
  }
  if (state != nullptr) *state = current;
  return true;
}

bool SendHardwareButton(const char* keyName, int buttonDown) {
  if (g_super.m_context == nullptr || g_hardware.getContext() == nullptr) {
    return false;
  }
  const int code = g_hardware.SearchCode(keyName);
  if (code < 0) return false;

  BYTE savedKeyboardState[256] = {};
  bool restoreKeyboardState = false;
  if (code >= CTRL_EXTENDED_KEY) {
    const int virtualKey = code - CTRL_EXTENDED_KEY;
    if (virtualKey < 0 || virtualKey >= 256 ||
        GetKeyboardState(savedKeyboardState) == FALSE) {
      return false;
    }
    BYTE translatedKeyboardState[256] = {};
    std::memcpy(translatedKeyboardState, savedKeyboardState,
                sizeof(translatedKeyboardState));
    translatedKeyboardState[virtualKey] =
        buttonDown ? static_cast<BYTE>(0x80) : static_cast<BYTE>(0x00);
    if (SetKeyboardState(translatedKeyboardState) == FALSE) return false;
    restoreKeyboardState = true;
  }

  KR_Event event;
  event.source = g_hardware.getObjectID();
  event.destination = g_hardware.getObjectID();
  const double timerTime = Session::m_realTimer->GetTime();
  event.timeStamp = !std::isfinite(timerTime) || timerTime < 0.1
                        ? 0.1
                        : timerTime;
  event.label = CTRL_HARDWARE_EVENT;
  event.data.open(EDO_WRITE)
      .putInt(CTRL_BUTTONS_MSG)
      .putInt(code)
      .putInt(buttonDown)
      .putInt(FALSE)
      .close();
  g_super.m_context->sendEventNow(event);
  if (restoreKeyboardState) SetKeyboardState(savedKeyboardState);
  return true;
}

bool WaitForSessionTimeAdvance(double minimumDelta) {
  if (Session::m_realTimer == nullptr || !std::isfinite(minimumDelta) ||
      minimumDelta <= 0.0) {
    return false;
  }
  const double startTime = Session::m_realTimer->GetTime();
  if (!std::isfinite(startTime)) return false;
  const ULONGLONG deadline = GetTickCount64() + 2000;
  do {
    Sleep(1);
    const double currentTime = Session::m_realTimer->GetTime();
    if (std::isfinite(currentTime) &&
        currentTime - startTime >= minimumDelta) {
      return true;
    }
  } while (GetTickCount64() < deadline);
  return false;
}

double HorizontalSpeed(const SRecoveredVehicleRuntimeState& state) {
  return std::sqrt(state.speed.x * state.speed.x +
                   state.speed.z * state.speed.z);
}

double HorizontalHeadingDelta(const CFMatrix3x4& before,
                              const CFMatrix3x4& after) {
  const CFVector3 beforeForward = before.Row(2);
  const CFVector3 afterForward = after.Row(2);
  const double beforeLength = std::sqrt(
      beforeForward.x * beforeForward.x +
      beforeForward.z * beforeForward.z);
  const double afterLength = std::sqrt(
      afterForward.x * afterForward.x +
      afterForward.z * afterForward.z);
  if (beforeLength <= 1.0e-12 || afterLength <= 1.0e-12) return 0.0;
  const double dot = (std::max)(
      -1.0, (std::min)(1.0,
                      (beforeForward.x * afterForward.x +
                       beforeForward.z * afterForward.z) /
                          (beforeLength * afterLength)));
  return std::acos(dot);
}

bool RunVehicleFrameAfter(double minimumDelta) {
  return WaitForSessionTimeAdvance(minimumDelta) &&
         RecoveredGameServices_RunFrame() &&
         IsVehicleControlActive(
             g_super.m_context->searchObject("Vehicle.Default"), nullptr);
}

bool ExerciseInteractiveTaxiHandoff() {
  SimulationContext* context = g_super.m_context;
  if (context == nullptr) return false;
  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  Vehicle* vehicle = vehicleID.isNUL() ? nullptr : static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  if (vehicle == nullptr) return false;

  SRecoveredTaxiVehicleHandoffTelemetry before = {};
  if (!RecoveredGameServices_TaxiVehicleHandoffTelemetry(&before))
    return false;
  KR_ObjectID taxiID =
      TaxiSubjectState_FirstPanelVehicleObject(context);
  const bool expectsPanel = !taxiID.isNUL();
  if (taxiID.isNUL()) taxiID = TaxiSubjectState_FirstObject(context);
  const unsigned int inputBefore = RecoveredGameServices_VehicleInputEvents();
  const unsigned int forwardedBefore =
      RecoveredGameServices_VehicleForwardedEvents();
  const unsigned int housekeepingBefore =
      RecoveredGameServices_VehicleHousekeepingEvents();

  if (taxiID.isNUL()) {
    if (!SendHardwareButton("F1", TRUE) ||
        !SendHardwareButton("F1", FALSE) ||
        !RunVehicleFrameAfter(0.01))
      return false;
    SRecoveredTaxiVehicleHandoffTelemetry after = {};
    return RecoveredGameServices_TaxiVehicleHandoffTelemetry(&after) &&
           after.attempts == before.attempts + 1 &&
           after.noTargetAttempts == before.noTargetAttempts + 1 &&
           after.successfulTransitions == before.successfulTransitions &&
           after.hardwareSubscriptionPreserved == 1;
  }

  ITaxi* taxi = static_cast<ITaxi*>(
      context->queryInterface(taxiID, ITaxiIID));
  if (taxi == nullptr) return false;
  const KR_ObjectID targetAttribute = taxi->getAttributeForVehicle();
  const CFVector3 taxiPosition = taxi->taxiPos();
  vehicle->Stop();
  vehicle->SetPos(taxiPosition);
  vehicle->setPosition(taxiPosition);

  STaxiVehicleProximityState proximity = {};
  if (!TaxiSubjectState_InspectVehicleProximity(
          context, vehicleID, &proximity) ||
      proximity.nearestTaxi != taxiID || proximity.nearbyTaxis <= 0 ||
      proximity.nearestDistance > 1.0e-7)
    return false;

  SRecoveredVehicleRuntimeState transitionStart = {};
  if (!VehicleRuntimeState_Inspect(context, vehicleID, &transitionStart) ||
      !SendHardwareButton("F1", TRUE) ||
      !SendHardwareButton("F1", FALSE) ||
      !RunVehicleFrameAfter(0.01))
    return false;

  SRecoveredVehicleRuntimeState transitioned = {};
  SRecoveredTaxiVehicleHandoffTelemetry handoff = {};
  if (!VehicleRuntimeState_Inspect(context, vehicleID, &transitioned) ||
      !RecoveredGameServices_TaxiVehicleHandoffTelemetry(&handoff) ||
      transitioned.attribute != targetAttribute ||
      context->isExist(taxiID) ||
      handoff.attempts != before.attempts + 1 ||
      handoff.pendingTransitions != 0 ||
      handoff.successfulTransitions != before.successfulTransitions + 1 ||
      handoff.removedTaxis != before.removedTaxis + 1 ||
      handoff.hardwareSubscriptionPreserved != 1 ||
      (expectsPanel &&
       (!handoff.panelReady || !handoff.panelOpen ||
        handoff.panelOpenTransitions != before.panelOpenTransitions + 1 ||
        handoff.panelDraws == 0)))
    return false;

  if (!SendHardwareButton("W", TRUE))
    return false;
  for (int driveFrame = 0; driveFrame < 40; ++driveFrame)
    if (!RunVehicleFrameAfter(0.025)) return false;
  if (!SendHardwareButton("W", FALSE) ||
      !RunVehicleFrameAfter(0.01)) return false;
  SRecoveredVehicleRuntimeState driven = {};
  if (!VehicleRuntimeState_Inspect(context, vehicleID, &driven) ||
      !RecoveredGameServices_TaxiVehicleHandoffTelemetry(&handoff))
    return false;
  const double dx = driven.position.x - transitioned.position.x;
  const double dz = driven.position.z - transitioned.position.z;
  return std::sqrt(dx * dx + dz * dz) > 1.0e-6 &&
         handoff.postTransitionFrames >= 41 &&
         handoff.postTransitionDistance > 1.0e-6 &&
         handoff.hardwareSubscriptionPreserved == 1 &&
         RecoveredGameServices_VehicleInputEvents() == inputBefore + 8 &&
         RecoveredGameServices_VehicleForwardedEvents() == forwardedBefore + 4 &&
         RecoveredGameServices_VehicleHousekeepingEvents() ==
             housekeepingBefore + 4 &&
         RecoveredGameServices_VehicleIgnoredEvents() == 0;
}

bool ExerciseSafeVehicleExitAndReentry() {
  SimulationContext* context = g_super.m_context;
  if (context == nullptr) return false;
  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  Vehicle* vehicle = vehicleID.isNUL() ? nullptr : static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  SRecoveredVehicleRuntimeState beforeState = {};
  SRecoveredVehicleEmbodimentTelemetry before = {};
  if (vehicle == nullptr ||
      !VehicleRuntimeState_Inspect(context, vehicleID, &beforeState) ||
      !RecoveredGameServices_VehicleEmbodimentTelemetry(&before))
    return false;
  if (vehicle->taxiChangeEnabled())
    return TaxiSubjectState_LiveCount() == 0 &&
           OrphanSubjectState_LiveCount() == 0 &&
           before.exitAttempts == 0 && before.exitPending == 0 &&
           before.hardwareSubscriptionPreserved == 1;

  const int taxiCount = TaxiSubjectState_LiveCount();
  const int orphanCount = OrphanSubjectState_LiveCount();
  const bool panelWasOpen = vehicle->panelOpen();
  if (!SendHardwareButton("F1", TRUE) ||
      !SendHardwareButton("F1", FALSE) ||
      !RunVehicleFrameAfter(0.01))
    return false;

  SRecoveredVehicleRuntimeState exitedState = {};
  SRecoveredVehicleEmbodimentTelemetry exited = {};
  STaxiVehicleProximityState proximity = {};
  if (!VehicleRuntimeState_Inspect(context, vehicleID, &exitedState) ||
      !RecoveredGameServices_VehicleEmbodimentTelemetry(&exited) ||
      !TaxiSubjectState_InspectVehicleProximity(
          context, vehicleID, &proximity) ||
      exitedState.attribute == beforeState.attribute ||
      !vehicle->taxiChangeEnabled() ||
      TaxiSubjectState_LiveCount() != taxiCount + 1 ||
      OrphanSubjectState_LiveCount() != orphanCount ||
      proximity.nearbyTaxis <= 0 || proximity.nearestTaxi.isNUL() ||
      proximity.nearestDistance >= proximity.activationDistance ||
      exited.exitAttempts != before.exitAttempts + 1 ||
      exited.safeExitCompletions != before.safeExitCompletions + 1 ||
      exited.unsafeExitCompletions != before.unsafeExitCompletions ||
      exited.droppedTaxis != before.droppedTaxis + 1 ||
      exited.droppedOrphans != before.droppedOrphans ||
      exited.exitPending != 0 ||
      exited.hardwareSubscriptionPreserved != 1 ||
      (panelWasOpen &&
       (vehicle->panelOpen() ||
        exited.panelCloseTransitions != before.panelCloseTransitions + 1)))
    return false;

  if (!SendHardwareButton("F1", TRUE) ||
      !SendHardwareButton("F1", FALSE))
    return false;
  bool reentered = false;
  for (int frame = 0; frame < 120; ++frame) {
    if (!RunVehicleFrameAfter(0.025) ||
        !VehicleRuntimeState_Inspect(context, vehicleID, &exitedState))
      return false;
    if (exitedState.attribute == beforeState.attribute &&
        TaxiSubjectState_LiveCount() == taxiCount) {
      reentered = true;
      break;
    }
  }

  SRecoveredVehicleEmbodimentTelemetry complete = {};
  return reentered && !vehicle->taxiChangeEnabled() &&
         OrphanSubjectState_LiveCount() == orphanCount &&
         RecoveredGameServices_VehicleEmbodimentTelemetry(&complete) &&
         complete.reentryAttempts == before.reentryAttempts + 1 &&
         complete.reentryCompletions == before.reentryCompletions + 1 &&
         complete.hardwareSubscriptionPreserved == 1 &&
         (!panelWasOpen ||
          (vehicle->panelOpen() &&
           complete.panelReopenTransitions ==
               before.panelReopenTransitions + 1));
}

bool ExerciseUnsafeVehicleExitAndOrphanImpact() {
  SimulationContext* context = g_super.m_context;
  if (context == nullptr) return false;
  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  Vehicle* vehicle = vehicleID.isNUL() ? nullptr : static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  SRecoveredVehicleRuntimeState vehicleState = {};
  SRecoveredVehicleEmbodimentTelemetry before = {};
  if (vehicle == nullptr ||
      !VehicleRuntimeState_Inspect(context, vehicleID, &vehicleState) ||
      !RecoveredGameServices_VehicleEmbodimentTelemetry(&before))
    return false;
  if (vehicle->taxiChangeEnabled())
    return OrphanSubjectState_LiveCount() == 0 &&
           before.unsafeExitCompletions == 0 && before.exitPending == 0 &&
           before.hardwareSubscriptionPreserved == 1;

  const int taxiCount = TaxiSubjectState_LiveCount();
  const int orphanCount = OrphanSubjectState_LiveCount();
  vehicle->Stop();
  CFMatrix3x4 direction;
  direction.LoadIdentity();
  vehicle->SetDir(direction);
  const CFVector3 elevated =
      vehicleState.position + CFVector3(0.0, 40.0, 0.0);
  vehicle->SetPos(elevated);
  vehicle->setPosition(elevated);

  if (!SendHardwareButton("F1", TRUE) ||
      !SendHardwareButton("F1", FALSE) ||
      !RunVehicleFrameAfter(0.01))
    return false;

  SRecoveredVehicleEmbodimentTelemetry dropped = {};
  if (!RecoveredGameServices_VehicleEmbodimentTelemetry(&dropped) ||
      !vehicle->taxiChangeEnabled() ||
      TaxiSubjectState_LiveCount() != taxiCount ||
      OrphanSubjectState_LiveCount() != orphanCount + 1 ||
      dropped.exitAttempts != before.exitAttempts + 1 ||
      dropped.safeExitCompletions != before.safeExitCompletions ||
      dropped.unsafeExitCompletions != before.unsafeExitCompletions + 1 ||
      dropped.droppedOrphans != before.droppedOrphans + 1 ||
      dropped.exitPending != 0 ||
      dropped.hardwareSubscriptionPreserved != 1)
    return false;

  bool impacted = false;
  for (int frame = 0; frame < 320; ++frame) {
    if (!RunVehicleFrameAfter(0.025) ||
        !RecoveredGameServices_VehicleEmbodimentTelemetry(&dropped))
      return false;
    if (OrphanSubjectState_LiveCount() == orphanCount &&
        dropped.orphanMoveEvents > before.orphanMoveEvents &&
        dropped.orphanImpacts > before.orphanImpacts &&
        dropped.orphanExplosions > before.orphanExplosions &&
        dropped.orphanRenderFrames > before.orphanRenderFrames) {
      impacted = true;
      break;
    }
  }
  return impacted && dropped.liveOrphans == 0 &&
         dropped.hardwareSubscriptionPreserved == 1;
}

bool PrepareVehiclePrimaryFire(Vehicle* vehicle,
                               SRecoveredVehicleRuntimeState* armedState) {
  if (vehicle == nullptr || armedState == nullptr ||
      !IsVehicleControlActive(vehicle->getObjectID(), armedState))
    return false;
  vehicle->Stop();
  CFMatrix3x4 levelDirection;
  levelDirection.LoadIdentity();
  vehicle->SetDir(levelDirection);
  const CFVector3 firingPosition =
      armedState->position + CFVector3(0.0, 10.0, 0.0);
  vehicle->SetPos(firingPosition);
  CFMatrix3x4 observedDirection;
  vehicle->getMatrix(observedDirection);
  return IsVehicleControlActive(vehicle->getObjectID(), armedState) &&
         std::fabs((-observedDirection.Column(2)).y) <= 1.0e-7 &&
         std::fabs(armedState->position.y - firingPosition.y) <= 1.0e-7 &&
         std::isfinite(armedState->position.x) &&
         std::isfinite(armedState->position.y) &&
         std::isfinite(armedState->position.z);
}

bool ExerciseVehiclePrimaryFire() {
  SimulationContext* context = g_super.m_context;
  if (context == nullptr) return false;
  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  Vehicle* vehicle = vehicleID.isNUL() ? nullptr : static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  if (vehicle == nullptr ||
      !RecoveredGameServices_BeginVehiclePrimaryFireObservation())
    return false;

  SRecoveredVehiclePrimaryFireTelemetry before = {};
  if (!RecoveredGameServices_VehiclePrimaryFireTelemetry(&before))
    return false;
  const unsigned int inputBefore = RecoveredGameServices_VehicleInputEvents();
  const unsigned int forwardedBefore =
      RecoveredGameServices_VehicleForwardedEvents();
  const unsigned int housekeepingBefore =
      RecoveredGameServices_VehicleHousekeepingEvents();
  const unsigned int focusLossBefore =
      RecoveredGameServices_VehicleFocusLossCount();
  const unsigned int focusGainBefore =
      RecoveredGameServices_VehicleFocusGainCount();
  const unsigned int syntheticBefore =
      RecoveredGameServices_VehicleSyntheticReleaseCount();
  const unsigned int suppressedBefore =
      RecoveredGameServices_VehicleSuppressedInputCount();

  SRecoveredVehicleRuntimeState initialState = {};
  AttributeVehicle* initialAttribute =
      !IsVehicleControlActive(vehicleID, &initialState)
          ? nullptr
          : static_cast<AttributeVehicle*>(
                __attrVehicleTable.searchAttribute(initialState.attribute));
  if (initialAttribute == nullptr) return false;
  const bool primaryFireGated =
      vehicle->taxiChangeEnabled() ||
      initialAttribute->m_bulletAttrIndex == -1;
  if (primaryFireGated) {
    if (!SendHardwareButton("MouseL", TRUE) ||
        !RunVehicleFrameAfter(0.01) ||
        !SendHardwareButton("MouseL", FALSE) ||
        !RunVehicleFrameAfter(0.01))
      return false;
    SRecoveredVehiclePrimaryFireTelemetry gated = {};
    return RecoveredGameServices_VehiclePrimaryFireTelemetry(&gated) &&
           gated.triggerPresses == before.triggerPresses + 1 &&
           gated.acceptedShots == before.acceptedShots &&
           gated.hardwareSubscriptionPreserved == 1 &&
           RecoveredGameServices_VehicleInputEvents() == inputBefore + 4 &&
           RecoveredGameServices_VehicleForwardedEvents() ==
               forwardedBefore + 2 &&
           RecoveredGameServices_VehicleHousekeepingEvents() ==
               housekeepingBefore + 2;
  }

  SRecoveredVehicleRuntimeState armedState = {};
  if (!PrepareVehiclePrimaryFire(vehicle, &armedState) ||
      !SendHardwareButton("MouseL", TRUE) ||
      !RunVehicleFrameAfter(0.01) ||
      !SendHardwareButton("MouseL", FALSE))
    return false;

  // Let the retail 0.2-second repeat event observe the released first shot
  // before starting a second press.
  for (int cooldownFrame = 0; cooldownFrame < 10; ++cooldownFrame)
    if (!RunVehicleFrameAfter(0.025)) return false;

  // Fire once more and lose focus while the physical action is still held.
  // The recovered owner must synthesize the release, suppress inactive
  // clicks and require a fresh press after focus returns.
  if (!RunVehicleFrameAfter(0.01) ||
      !SendHardwareButton("MouseL", TRUE) ||
      !RunVehicleFrameAfter(0.01) ||
      !RecoveredGameServices_SetApplicationActive(false))
    return false;

  SRecoveredVehiclePrimaryFireTelemetry released = {};
  if (!RecoveredGameServices_VehiclePrimaryFireTelemetry(&released) ||
      released.triggerPresses != before.triggerPresses + 2 ||
      !SendHardwareButton("MouseL", TRUE) ||
      !SendHardwareButton("MouseL", FALSE))
    return false;

  SRecoveredVehiclePrimaryFireTelemetry fire = {};
  const double fireDrainTime = (std::max)(
      0.35, initialAttribute->m_bulletSlipTime + 0.05);
  bool complete = false;
  int stableShotFrames = 0;
  unsigned int previousShots = released.acceptedShots;
  for (int frame = 0; frame < 160; ++frame) {
    if (!RunVehicleFrameAfter(0.025) ||
        !RecoveredGameServices_VehiclePrimaryFireTelemetry(&fire))
      return false;
    if (fire.acceptedShots == previousShots) {
      ++stableShotFrames;
    } else {
      previousShots = fire.acceptedShots;
      stableShotFrames = 0;
    }
    if (fire.moveEvents > before.moveEvents &&
        fire.collisionChecks > before.collisionChecks &&
        fire.sceneImpacts + fire.dynamicImpacts >
            before.sceneImpacts + before.dynamicImpacts &&
        fire.impactEffectChildren > before.impactEffectChildren &&
        fire.maximumExplosionSubjects > before.maximumExplosionSubjects &&
        fire.maximumParticleBranches > before.maximumParticleBranches &&
        fire.maximumSoundObjects > before.maximumSoundObjects &&
        fire.effectRenderFrames > before.effectRenderFrames) {
      complete = true;
    }
  }
  if (!complete || fire.acceptedShots < before.acceptedShots + 2 ||
      stableShotFrames < 20)
    return false;

  const unsigned int quiescedShots = fire.acceptedShots;
  if (
      !RunVehicleFrameAfter(fireDrainTime) ||
      !RecoveredGameServices_VehiclePrimaryFireTelemetry(&fire) ||
      fire.acceptedShots != quiescedShots)
    return false;

  if (!RecoveredGameServices_SetApplicationActive(true))
    return false;

  const unsigned int inputDelta =
      RecoveredGameServices_VehicleInputEvents() - inputBefore;
  const unsigned int forwardedDelta =
      RecoveredGameServices_VehicleForwardedEvents() - forwardedBefore;
  const unsigned int housekeepingDelta =
      RecoveredGameServices_VehicleHousekeepingEvents() -
      housekeepingBefore;
  const unsigned int suppressedDelta =
      RecoveredGameServices_VehicleSuppressedInputCount() -
      suppressedBefore;

  const bool succeeded =
      complete && fire.triggerPresses == before.triggerPresses + 2 &&
      fire.acceptedShots == quiescedShots &&
      fire.rolledBackShots == before.rolledBackShots &&
      fire.hardwareSubscriptionPreserved == 1 && inputDelta >= 10 &&
      inputDelta == housekeepingDelta * 2 && forwardedDelta == 3 &&
      suppressedDelta >= 2 &&
      forwardedDelta + suppressedDelta == housekeepingDelta &&
      RecoveredGameServices_VehicleFocusLossCount() ==
          focusLossBefore + 1 &&
      RecoveredGameServices_VehicleFocusGainCount() == focusGainBefore + 1 &&
      RecoveredGameServices_VehicleSyntheticReleaseCount() ==
          syntheticBefore + 1 &&
      RecoveredGameServices_VehicleActiveActionCount() == 0 &&
      RecoveredGameServices_VehicleIgnoredEvents() == 0;
  if (!succeeded) {
    std::fprintf(stderr,
                 "vehicle-fire contract complete=%d shots="
                 "%u/%u/%u input=%u forwarded=%u housekeeping=%u "
                 "suppressed=%u focus=%u/%u synthetic=%u active=%u "
                 "ignored=%u\n",
                 complete ? 1 : 0, before.acceptedShots,
                 quiescedShots, fire.acceptedShots, inputDelta,
                 forwardedDelta, housekeepingDelta, suppressedDelta,
                 RecoveredGameServices_VehicleFocusLossCount() -
                     focusLossBefore,
                 RecoveredGameServices_VehicleFocusGainCount() -
                     focusGainBefore,
                 RecoveredGameServices_VehicleSyntheticReleaseCount() -
                     syntheticBefore,
                 RecoveredGameServices_VehicleActiveActionCount(),
                 RecoveredGameServices_VehicleIgnoredEvents());
  }
  return succeeded;
}

bool VisibleProbePosition(double verticalOffset, double forwardDistance,
                          CFVector3* position) {
  if (position == nullptr || g_super.m_context == nullptr) return false;

  SRecoveredVehicleRuntimeState vehicle = {};
  KR_ObjectID vehicleID =
      g_super.m_context->searchObject("Vehicle.Default");
  if (!vehicleID.isNUL() &&
      VehicleRuntimeState_Inspect(g_super.m_context, vehicleID, &vehicle) &&
      vehicle.active) {
    const CFVector3 forward = vehicle.direction.Row(2);
    position->x = vehicle.position.x - forward.x * forwardDistance;
    position->y = vehicle.position.y - forward.y * forwardDistance +
                  verticalOffset;
    position->z = vehicle.position.z - forward.z * forwardDistance;
  } else {
    const SRecoveredObserverState* observer =
        RecoveredGameServices_ObserverState();
    if (observer == nullptr) return false;
    position->x = observer->x;
    position->y = observer->y + verticalOffset;
    position->z = observer->z - forwardDistance;
  }
  return std::isfinite(position->x) && std::isfinite(position->y) &&
         std::isfinite(position->z);
}

bool VisibleProbeAboveTerrain(double clearance, double forwardDistance,
                              CFVector3* position) {
  if (position == nullptr || !std::isfinite(clearance) || clearance <= 0.0 ||
      !VisibleProbePosition(0.0, forwardDistance, position))
    return false;
  CViewScene* scene = CViewScene::Current();
  if (scene == nullptr || scene->GetTerrain() == nullptr) return false;
  CFVector3 normal;
  double landY = 0.0;
  scene->GetTerrain()->GetPlane(*position, normal, landY);
  if (!std::isfinite(landY) || !std::isfinite(normal.x) ||
      !std::isfinite(normal.y) || !std::isfinite(normal.z))
    return false;
  position->y = landY + clearance;
  return std::isfinite(position->y);
}

bool ExerciseVisibleSmoke() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_SmokeRenderingReady() ||
      SmokeSubjectState_LiveCount() != 0 ||
      SmokerSubjectState_DynLiveCount() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* attributeName = "Smoke.Attr.Trace";
  KR_ObjectID attributeID = context->searchObject(attributeName);
  AttributeSmoke* attribute = attributeID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoke*>(
            __attrSmokeTable.searchAttribute(attributeID));
  CFVector3 position;
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("Smoke");
  static const char kProbeName[] = "Smoke.Rendering.Probe";
  if (attribute == nullptr || attribute->m_cacheImage == nullptr ||
      attribute->m_maxBlob <= 0 ||
      !VisibleProbePosition(0.0, 64.0, &position) ||
      table == ct_NULLID || context->isExist(kProbeName)) {
    return false;
  }

  KR_ObjectID object = g_arena.newObject(table, kProbeName);
  if (object.isNUL()) return false;
  KR_Event event;
  event.label = fou_EVCMD_START;
  event.source = g_arena.getObjectID();
  event.destination = object;
  event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
  event.data.open(EDO_WRITE)
      .putObjectID(attributeID)
      .putDouble(position.x)
      .putDouble(position.y)
      .putDouble(position.z)
      .close();
  context->sendEventNow(event);
  const bool started = context->isExist(kProbeName) &&
                       SmokeSubjectState_LiveCount() == 1 &&
                       context->removeEvent(fou_EVC_MOVING, object) != 0;
  if (!started) {
    context->removeEvent(fou_EVC_MOVING, object);
    if (context->isExist(kProbeName)) context->removeObject(object);
    return false;
  }

  const dword framesBefore = dwFrames;
  bool firstFrame = false;
  bool secondFrame = false;
  int drawsAfterFirstFrame = 0;
  {
    ScopedAlphaSpriteCapture capture(attribute->m_cacheImage);
    firstFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterFirstFrame = g_alphaSpriteDraws;
    context->removeObject(object);
    secondFrame = RecoveredGameServices_RunFrame() != FALSE;
  }
  return firstFrame && secondFrame && g_alphaSpriteDrawValid &&
         drawsAfterFirstFrame == attribute->m_maxBlob &&
         g_alphaSpriteDraws == drawsAfterFirstFrame &&
         dwFrames == framesBefore + 2 &&
         !context->isExist(kProbeName) &&
         SmokeSubjectState_LiveCount() == 0 &&
         context->removeEvent(fou_EVC_MOVING, object) == 0;
}

bool ExerciseVisibleBulletBarrelSmoke() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_BulletBarrelSmokeReady() ||
      !RecoveredGameServices_SmokeRenderingReady() ||
      BulletSubjectState_LiveCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* bulletAttributeName =
      BulletAttributeState_FirstBarrelSmokeAttributeName(context);
  KR_ObjectID bulletAttributeID = bulletAttributeName == nullptr
      ? KR_ObjectID::NUL()
      : context->searchObject(bulletAttributeName);
  AttributeBullet* bulletAttribute = bulletAttributeID.isNUL()
      ? nullptr
      : static_cast<AttributeBullet*>(
            __bulletAttrTable.searchAttribute(bulletAttributeID));
  const ct_ClassTableID bulletAttributeTable =
      g_arena.searchSeanceClassTable("BulletAttr");
  const ct_ClassTableID bulletTable =
      g_arena.searchSeanceClassTable("Bullet");
  const int bulletAttributeIndex =
      bulletAttributeTable == ct_NULLID || bulletAttributeID.isNUL()
          ? -1
          : g_arena.getAttributeIndex(
                bulletAttributeTable, bulletAttributeID);
  AttributeSmoke* smokeAttribute = bulletAttribute == nullptr ||
          bulletAttribute->m_smokeAttrID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoke*>(__attrSmokeTable.searchAttribute(
            bulletAttribute->m_smokeAttrID));
  CFVector3 position;
  static const char kBulletName[] =
      "Bullet.BarrelSmoke.Rendering.Probe";
  if (bulletAttribute == nullptr || smokeAttribute == nullptr ||
      bulletAttribute->m_useBarellSmoke == 0 ||
      bulletAttributeIndex == -1 || bulletTable == ct_NULLID ||
      smokeAttribute->m_cacheImage == nullptr ||
      smokeAttribute->m_maxBlob <= 0 ||
      !VisibleProbePosition(0.0, 64.0, &position) ||
      context->isExist(kBulletName) || context->isExist("Smok.")) {
    return false;
  }

  KR_ObjectID bullet = g_arena.newObject(bulletTable, kBulletName);
  if (bullet.isNUL()) return false;
  const double currentTime = Session::m_moment;
  const double timeStamp = !std::isfinite(currentTime) || currentTime < 0.1
      ? 0.1 : currentTime;
  const CFVector3 direction(0.0, 0.0, -1.0);
  KR_Event event;
  event.label = b_EV_START;
  event.source = g_arena.getObjectID();
  event.destination = bullet;
  event.timeStamp = timeStamp;
  event.data.open(EDO_WRITE)
      .descend(VECTOR3D_F, 0)
        .putDouble(position.x)
        .putDouble(position.y)
        .putDouble(position.z)
      .ascend()
      .descend(VECTOR3D_F, 0)
        .putDouble(direction.x)
        .putDouble(direction.y)
        .putDouble(direction.z)
      .ascend()
      .putInt(bulletAttributeIndex)
      .putObjectID(g_arena.getObjectID())
      .close();
  const double previousFrameSec = Session::m_frameSec;
  Session::m_frameSec = 0.09;
  context->sendEventNow(event);
  Session::m_frameSec = previousFrameSec;

  KR_ObjectID smoke = context->searchObject("Smok.");
  const bool started = context->isExist(kBulletName) &&
      !smoke.isNUL() && context->isExist(smoke) &&
      BulletSubjectState_LiveCount() == 1 &&
      SmokeSubjectState_LiveCount() == 1;
  if (!started) {
    context->removeEvent(fou_EVC_MOVING, smoke);
    if (!smoke.isNUL() && context->isExist(smoke)) {
      context->removeObject(smoke);
    }
    if (context->isExist(bullet)) context->removeObject(bullet);
    return false;
  }
  context->removeObject(bullet);
  const bool childOwnsMoving =
      context->removeEvent(fou_EVC_MOVING, smoke) != 0;
  if (!childOwnsMoving || !context->isExist(smoke) ||
      BulletSubjectState_LiveCount() != 0) {
    if (context->isExist(smoke)) context->removeObject(smoke);
    return false;
  }

  const dword framesBefore = dwFrames;
  bool visibleFrame = false;
  bool detachedFrame = false;
  int drawsAfterVisibleFrame = 0;
  {
    ScopedAlphaSpriteCapture capture(smokeAttribute->m_cacheImage);
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterVisibleFrame = g_alphaSpriteDraws;
    context->removeObject(smoke);
    detachedFrame = RecoveredGameServices_RunFrame() != FALSE;
  }
  return visibleFrame && detachedFrame && g_alphaSpriteDrawValid &&
         drawsAfterVisibleFrame == smokeAttribute->m_maxBlob &&
         g_alphaSpriteDraws == drawsAfterVisibleFrame &&
         dwFrames == framesBefore + 2 &&
         !context->isExist(kBulletName) && !context->isExist("Smok.") &&
         BulletSubjectState_LiveCount() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         context->removeEvent(b_EVC_MOVING, bullet) == 0 &&
         context->removeEvent(b_EVC_CHECK_COLLISION, bullet) == 0 &&
         context->removeEvent(fou_EVC_MOVING, smoke) == 0;
}

bool ExerciseVisibleSmokerEmission() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_SmokerEmissionReady() ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* smokerAttributeName = "Smoker.Attr.Corpse";
  KR_ObjectID smokerAttributeID =
      context->searchObject(smokerAttributeName);
  AttributeSmoker* smokerAttribute = smokerAttributeID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoker*>(
            __attrSmokerTable.searchAttribute(smokerAttributeID));
  AttributeSmoke* smokeAttribute =
      smokerAttribute == nullptr || smokerAttribute->m_smokeAttrID.isNUL()
          ? nullptr
          : static_cast<AttributeSmoke*>(
                __attrSmokeTable.searchAttribute(
                    smokerAttribute->m_smokeAttrID));
  CFVector3 position;
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("DynSmoker");
  static const char kProbeName[] = "DynSmoker.Rendering.Probe";
  static const char kSmokeName[] = "Smok.Static";
  if (smokerAttribute == nullptr || smokeAttribute == nullptr ||
      smokeAttribute->m_cacheImage == nullptr ||
      smokeAttribute->m_maxBlob <= 0 ||
      !VisibleProbePosition(0.0, 64.0, &position) ||
      table == ct_NULLID || context->isExist(kProbeName) ||
      context->isExist(kSmokeName)) {
    return false;
  }

  KR_ObjectID smoker = g_arena.newObject(table, kProbeName);
  if (smoker.isNUL()) return false;
  KR_Event event;
  event.label = fou_EVCMD_START;
  event.source = g_arena.getObjectID();
  event.destination = smoker;
  event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
  event.data.open(EDO_WRITE)
      .putObjectID(smokerAttributeID)
      .putDouble(position.x)
      .putDouble(position.y)
      .putDouble(position.z)
      .close();
  context->sendEventNow(event);

  const dword framesBefore = dwFrames;
  const bool enteredView = RecoveredGameServices_RunFrame() != FALSE &&
      context->removeEvent(sm_EV_MOVE, smoker) != 0;
  if (enteredView) {
    event.label = sm_EV_MOVE;
    event.source = smoker;
    event.destination = smoker;
    context->sendEventNow(event);
  }
  KR_ObjectID smoke = context->searchObject(kSmokeName);
  const bool emitted = enteredView && !smoke.isNUL() &&
      SmokerSubjectState_DynLiveCount() == 1 &&
      SmokeSubjectState_LiveCount() == 1 &&
      context->removeEvent(sm_EV_MOVE, smoker) != 0 &&
      context->removeEvent(fou_EVC_MOVING, smoke) != 0;
  if (!emitted) {
    context->removeEvent(sm_EV_MOVE, smoker);
    if (context->isExist(smoker)) context->removeObject(smoker);
    if (!smoke.isNUL()) {
      context->removeEvent(fou_EVC_MOVING, smoke);
      if (context->isExist(smoke)) context->removeObject(smoke);
    }
    return false;
  }
  KR_Event pending;
  pending.label = sm_EV_MOVE;
  pending.source = smoker;
  pending.destination = smoker;
  pending.timeStamp = Session::m_moment + 1000.0;
  context->addEvent(pending);
  pending.label = fou_EVC_MOVING;
  pending.source = smoke;
  pending.destination = smoke;
  context->addEvent(pending);

  bool visibleFrame = false;
  bool detachedFrame = false;
  int drawsAfterVisibleFrame = 0;
  {
    ScopedAlphaSpriteCapture capture(smokeAttribute->m_cacheImage);
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterVisibleFrame = g_alphaSpriteDraws;
    context->removeObject(smoker);
    context->removeObject(smoke);
    detachedFrame = RecoveredGameServices_RunFrame() != FALSE;
  }
  return visibleFrame && detachedFrame && g_alphaSpriteDrawValid &&
         drawsAfterVisibleFrame == smokeAttribute->m_maxBlob &&
         g_alphaSpriteDraws == drawsAfterVisibleFrame &&
         dwFrames == framesBefore + 3 &&
         !context->isExist(kProbeName) &&
         !context->isExist(kSmokeName) &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         context->removeEvent(sm_EV_MOVE, smoker) == 0 &&
         context->removeEvent(sm_EV_REMOVE, smoker) == 0 &&
         context->removeEvent(fou_EVC_MOVING, smoke) == 0;
}

bool ExerciseVisibleSmokerLightCorona() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_SmokerLightCoronaReady() ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0 ||
      g_lightChain.m_count != 0 || g_lightChain.m_list != nullptr ||
      CViewObject::EnabledLights() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* attributeName = "Smoker.Attr.FireMd";
  KR_ObjectID attributeID = context->searchObject(attributeName);
  AttributeSmoker* attribute = attributeID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoker*>(
            __attrSmokerTable.searchAttribute(attributeID));
  CFVector3 position;
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("DynSmoker");
  static const char kProbeName[] = "DynSmoker.LightCorona.Rendering.Probe";
  static const char kSmokeName[] = "Smok.Static";
  if (attribute == nullptr || !attribute->m_useLight ||
      !attribute->m_useCorona || attribute->m_coronaHText == nullptr ||
      attribute->m_coronaColor == 0 ||
      !VisibleProbePosition(0.0, 64.0, &position) ||
      table == ct_NULLID || context->isExist(kProbeName) ||
      context->isExist(kSmokeName)) {
    return false;
  }

  KR_ObjectID smoker = g_arena.newObject(table, kProbeName);
  if (smoker.isNUL()) return false;
  KR_Event event;
  event.label = fou_EVCMD_START;
  event.source = g_arena.getObjectID();
  event.destination = smoker;
  event.timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
  event.data.open(EDO_WRITE)
      .putObjectID(attributeID)
      .putDouble(position.x)
      .putDouble(position.y)
      .putDouble(position.z)
      .close();
  context->sendEventNow(event);

  const dword framesBefore = dwFrames;
  const bool enteredView = RecoveredGameServices_RunFrame() != FALSE &&
      context->removeEvent(sm_EV_MOVE, smoker) != 0;
  if (enteredView) {
    event.label = sm_EV_MOVE;
    event.source = smoker;
    event.destination = smoker;
    context->sendEventNow(event);
  }
  KR_ObjectID smoke = context->searchObject(kSmokeName);
  const bool moved = enteredView && !smoke.isNUL() &&
      SmokerSubjectState_DynLiveCount() == 1 &&
      SmokeSubjectState_LiveCount() == 1 &&
      context->removeEvent(sm_EV_MOVE, smoker) != 0 &&
      context->removeEvent(fou_EVC_MOVING, smoke) != 0;
  if (!moved) {
    context->removeEvent(sm_EV_MOVE, smoker);
    if (context->isExist(smoker)) context->removeObject(smoker);
    if (!smoke.isNUL() && context->isExist(smoke)) {
      context->removeEvent(fou_EVC_MOVING, smoke);
      context->removeObject(smoke);
    }
    CViewObject::EnableLights(0);
    g_lightChain.m_list = nullptr;
    g_lightChain.m_count = 0;
    return false;
  }
  context->removeObject(smoke);
  KR_Event pending;
  pending.label = sm_EV_MOVE;
  pending.source = smoker;
  pending.destination = smoker;
  pending.timeStamp = Session::m_moment + 1000.0;
  context->addEvent(pending);

  bool visibleFrame = false;
  bool detachedFrame = false;
  bool lightPublished = false;
  int drawsAfterVisibleFrame = 0;
  {
    ScopedAlphaSpriteCapture capture(attribute->m_coronaHText);
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterVisibleFrame = g_alphaSpriteDraws;
    lightPublished = CViewObject::EnabledLights() == 1 &&
        g_lightChain.m_count == 0 && g_lightChain.m_list == nullptr &&
        _gr_pLights[0].r == attribute->m_lightRadius &&
        _gr_pLights[0].power0 >= attribute->m_minLightBright &&
        _gr_pLights[0].power0 <= attribute->m_maxLightBright &&
        _gr_pLights[0].color == attribute->m_lightColor;
    context->removeObject(smoker);
    detachedFrame = RecoveredGameServices_RunFrame() != FALSE;
  }
  return visibleFrame && detachedFrame && lightPublished &&
         g_alphaSpriteDrawValid && drawsAfterVisibleFrame == 1 &&
         g_alphaSpriteDraws == drawsAfterVisibleFrame &&
         g_lastAlphaSprite.opacity == attribute->m_coronaAlpha &&
         g_lastAlphaSprite.color == attribute->m_coronaColor &&
         g_lastAlphaSprite.hTexture == attribute->m_coronaHText &&
         dwFrames == framesBefore + 3 &&
         CViewObject::EnabledLights() == 0 &&
         g_lightChain.m_count == 0 && g_lightChain.m_list == nullptr &&
         !context->isExist(kProbeName) &&
         !context->isExist(kSmokeName) &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         context->removeEvent(sm_EV_MOVE, smoker) == 0 &&
         context->removeEvent(sm_EV_REMOVE, smoker) == 0 &&
         context->removeEvent(fou_EVC_MOVING, smoke) == 0;
}

bool ExerciseVisibleExplosionParticles() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_ExplosionLightReady() ||
       !RecoveredGameServices_ExplosionSoundReady() ||
       !RecoveredGameServices_ExplosionParticlesReady() ||
       !RecoveredGameServices_ExplosionSmokeReady() ||
      !RecoveredGameServices_ExplosionPieceReady() ||
      !RecoveredGameServices_ExplosionTraceReady() ||
      !ExplosionSubjectState_LightRosterReady(g_super.m_context) ||
      _pGRDrawParticle == nullptr || _pGRDrawAlphaSprite == nullptr ||
      ExplosionSubjectState_LiveCount() != 0 ||
      ExplosionSubjectState_ParticleBranchLiveCount() != 0 ||
      ExplosionSubjectState_TracedParentCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0 ||
      g_lightChain.m_count != 0 || g_lightChain.m_list != nullptr ||
      CViewObject::EnabledLights() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* attributeName =
      ExplosionSubjectState_SoundProbeAttributeName(context);
  KR_ObjectID attributeID = attributeName == nullptr
      ? KR_ObjectID::NUL()
      : context->searchObject(attributeName);
  AttributeExplosion* attribute = attributeID.isNUL()
      ? nullptr
      : static_cast<AttributeExplosion*>(
            __attrExplosionTable.searchAttribute(attributeID));
  const ct_ClassTableID attributeTable =
      g_arena.searchSeanceClassTable("ExplosionAttr");
  const ct_ClassTableID subjectTable =
      g_arena.searchSeanceClassTable("Explosion");
  const int attributeIndex = attributeTable == ct_NULLID ||
          attributeID.isNUL()
      ? -1
      : g_arena.getAttributeIndex(attributeTable, attributeID);
  CFVector3 position;
  static const char kProbeName[] = "Explosion.Light.Rendering.Probe";
  if (attribute == nullptr || !attribute->m_useLight ||
      attribute->m_wav == nullptr || attribute->m_ctsndID == ct_NULLID ||
      attribute->m_hTexture == nullptr || attribute->m_cacheSkin == nullptr ||
      !std::isfinite(attribute->m_cacheSkin->Radius()) ||
      attribute->m_cacheSkin->Radius() <= 0.0 ||
      attribute->m_lightTimeLife <= 0.0 ||
      attribute->m_lightRadius <= 0.0 || attributeIndex == -1 ||
      subjectTable == ct_NULLID ||
      !VisibleProbeAboveTerrain(32.0, 128.0, &position) ||
      context->isExist(kProbeName)) {
    return false;
  }

  const double currentTime = Session::m_moment;
  const double timeStamp = !std::isfinite(currentTime) || currentTime < 0.1
      ? 0.1 : currentTime;
  ExplosionImpactRequest request = {
      position, timeStamp, KR_ObjectID::NUL(), subjectTable,
      attributeIndex, kProbeName};
  int damageApplications = -1;
  const int soundsBefore = SoundObjectState_LiveCount();
  const int branchesBefore =
      ExplosionSubjectState_ParticleBranchLiveCount();
  const bool executed = ExplosionSubjectState_ExecuteNow(
      context, request, &damageApplications);
  KR_ObjectID explosion = context->searchObject(kProbeName);
  int simpleParticles = -1;
  int snakeParticles = -1;
  int rays = -1;
  int smokeSprites = -1;
  int pieces = -1;
  int tracedPieces = -1;
  const bool particlesOwned = !explosion.isNUL() &&
      ExplosionSubjectState_ParentParticleCounts(
          context, explosion, &simpleParticles, &snakeParticles, &rays,
          &smokeSprites, &pieces, &tracedPieces);
  const int ownedBranches =
      simpleParticles + snakeParticles + rays + smokeSprites + pieces +
      tracedPieces;
  if (!executed || damageApplications != 0 || explosion.isNUL() ||
      ExplosionSubjectState_LiveCount() != 1 ||
      !particlesOwned || simpleParticles <= 0 || snakeParticles <= 0 ||
      rays < 0 || smokeSprites <= 0 || pieces <= 0 || tracedPieces <= 0 ||
      ownedBranches <= 0 || ExplosionSubjectState_TracedParentCount() != 1 ||
      ExplosionSubjectState_ParticleBranchLiveCount() !=
          branchesBefore + ownedBranches ||
      SoundObjectState_LiveCount() != soundsBefore + 1 ||
      !ExplosionSubjectState_ParentSoundMatches(
          context, explosion, attribute->m_wav, position, true, 1)) {
    if (!explosion.isNUL() && context->isExist(explosion)) {
      context->removeObject(explosion);
    }
    return false;
  }

  const dword framesBefore = dwFrames;
  bool visibleFrame = false;
  bool detachedFrame = false;
  bool lightPublished = false;
  bool ownedMove = false;
  bool ownedPuff = false;
  int drawsAfterVisibleFrame = 0;
  int smokeDrawsAfterVisibleFrame = 0;
  const int pieceDrawsBefore = ExplosionSubjectState_PieceDrawCount();
  int pieceDrawsAfterVisibleFrame = pieceDrawsBefore;
  int pieceDrawsAfterDetachedFrame = pieceDrawsBefore;
  {
    ScopedParticleCapture capture;
    ScopedAlphaSpriteCapture smokeCapture(attribute->m_hTexture);
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterVisibleFrame = g_particleDraws;
    smokeDrawsAfterVisibleFrame = g_alphaSpriteDraws;
    pieceDrawsAfterVisibleFrame = ExplosionSubjectState_PieceDrawCount();
    double elapsed = Session::m_moment - timeStamp;
    if (elapsed < 0.0) elapsed = 0.0;
    int brightnessIndex = static_cast<int>(
        elapsed * AttributeExplosion::MAX_BRIGHT /
        attribute->m_lightTimeLife);
    if (brightnessIndex < 0) brightnessIndex = 0;
    if (brightnessIndex >= AttributeExplosion::MAX_BRIGHT) {
      brightnessIndex = AttributeExplosion::MAX_BRIGHT - 1;
    }
    lightPublished = visibleFrame &&
        CViewObject::EnabledLights() == 1 &&
        g_lightChain.m_count == 0 && g_lightChain.m_list == nullptr &&
        _gr_pLights[0].r == attribute->m_lightRadius &&
        _gr_pLights[0].power0 == attribute->m_brightness[brightnessIndex] &&
        _gr_pLights[0].color == attribute->m_lightColor;
    ownedMove = context->removeEvent(EXPLOSION_MOVE, explosion) != 0;
    ownedPuff = context->removeEvent(EXPLOSION_NEWPUFF, explosion) != 0;
    if (context->isExist(explosion)) context->removeObject(explosion);
    detachedFrame = RecoveredGameServices_RunFrame() != FALSE;
    pieceDrawsAfterDetachedFrame = ExplosionSubjectState_PieceDrawCount();
  }
  int removedTracePuffs = 0;
  while (SmokeSubjectState_LiveCount() > 0 && removedTracePuffs < 128) {
    KR_ObjectID smoke = context->searchObject("Smok.Static");
    if (smoke.isNUL() ||
        !SmokeSubjectState_RollbackStarted(context, smoke)) {
      break;
    }
    ++removedTracePuffs;
  }
  const int liveExplosions = ExplosionSubjectState_LiveCount();
  const int liveBranches =
      ExplosionSubjectState_ParticleBranchLiveCount();
  const int tracedParents = ExplosionSubjectState_TracedParentCount();
  const int liveSmoke = SmokeSubjectState_LiveCount();
  const int liveSounds = SoundObjectState_LiveCount();
  const bool rolledBack = !context->isExist(explosion) &&
      liveExplosions == 0 && liveBranches == branchesBefore &&
      tracedParents == 0 && liveSmoke == 0 && liveSounds == soundsBefore;
  if (context->isExist(explosion)) context->removeObject(explosion);
  const bool moveCleared =
      context->removeEvent(EXPLOSION_MOVE, explosion) == 0;
  const bool puffCleared =
      context->removeEvent(EXPLOSION_NEWPUFF, explosion) == 0;
  const bool result = lightPublished && ownedMove && ownedPuff && rolledBack &&
         detachedFrame &&
         g_particleDrawValid && drawsAfterVisibleFrame > 0 &&
         g_particleDraws == drawsAfterVisibleFrame &&
         g_alphaSpriteDrawValid && smokeDrawsAfterVisibleFrame > 0 &&
         g_alphaSpriteDraws == smokeDrawsAfterVisibleFrame &&
         pieceDrawsAfterVisibleFrame > pieceDrawsBefore &&
         pieceDrawsAfterDetachedFrame == pieceDrawsAfterVisibleFrame &&
         g_lastAlphaSprite.hTexture == attribute->m_hTexture &&
         dwFrames == framesBefore + 2 &&
         CViewObject::EnabledLights() == 0 &&
         g_lightChain.m_count == 0 && g_lightChain.m_list == nullptr &&
         SoundObjectState_LiveCount() == soundsBefore &&
         !context->isExist(kProbeName) &&
         moveCleared && puffCleared;
  if (!result) {
    std::fprintf(
        stderr,
        "explosion-render light=%d move=%d puff=%d rollback=%d detach=%d "
        "particle=%d draws=%d/%d alpha=%d smoke=%d/%d piece=%d/%d/%d "
        "frames=%lu/%lu lights=%d chain=%d/%d live=%d/%d/%d/%d/%d "
        "expected_branch_sound=%d/%d exists=%d "
        "events=%d/%d moment=%.9f stamp=%.9f life=%.9f\n",
        lightPublished ? 1 : 0, ownedMove ? 1 : 0, ownedPuff ? 1 : 0,
        rolledBack ? 1 : 0, detachedFrame ? 1 : 0,
        g_particleDrawValid ? 1 : 0, drawsAfterVisibleFrame,
        g_particleDraws, g_alphaSpriteDrawValid ? 1 : 0,
        smokeDrawsAfterVisibleFrame, g_alphaSpriteDraws,
        pieceDrawsBefore, pieceDrawsAfterVisibleFrame,
        pieceDrawsAfterDetachedFrame, dwFrames, framesBefore + 2,
        CViewObject::EnabledLights(), g_lightChain.m_count,
        g_lightChain.m_list == nullptr ? 0 : 1,
        liveExplosions, liveBranches, tracedParents, liveSmoke, liveSounds,
        branchesBefore, soundsBefore,
        context->isExist(kProbeName) ? 1 : 0,
        moveCleared ? 0 : 1, puffCleared ? 0 : 1,
        Session::m_moment, timeStamp, attribute->m_lightTimeLife);
  }
  return result;
}

bool ExerciseVisibleExplosionTrace() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_ExplosionTraceReady() ||
      !RecoveredGameServices_SmokeRenderingReady() ||
      ExplosionSubjectState_LiveCount() != 0 ||
      ExplosionSubjectState_ParticleBranchLiveCount() != 0 ||
      ExplosionSubjectState_TracedParentCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  const char* attributeName =
      ExplosionSubjectState_TraceProbeAttributeName(context);
  KR_ObjectID attributeID = attributeName == nullptr
      ? KR_ObjectID::NUL()
      : context->searchObject(attributeName);
  AttributeExplosion* attribute = attributeID.isNUL()
      ? nullptr
      : static_cast<AttributeExplosion*>(
            __attrExplosionTable.searchAttribute(attributeID));
  AttributeSmoke* smokeAttribute = attribute == nullptr ||
          attribute->m_smokeAttrID.isNUL()
      ? nullptr
      : static_cast<AttributeSmoke*>(
            __attrSmokeTable.searchAttribute(attribute->m_smokeAttrID));
  const ct_ClassTableID attributeTable =
      g_arena.searchSeanceClassTable("ExplosionAttr");
  const ct_ClassTableID subjectTable =
      g_arena.searchSeanceClassTable("Explosion");
  const int attributeIndex = attributeTable == ct_NULLID ||
          attributeID.isNUL()
      ? -1
      : g_arena.getAttributeIndex(attributeTable, attributeID);
  CFVector3 position;
  static const char kProbeName[] = "Explosion.Trace.Rendering.Probe";
  static const char kSmokeName[] = "Smok.Static";
  if (attribute == nullptr || smokeAttribute == nullptr ||
      smokeAttribute->m_cacheImage == nullptr ||
      smokeAttribute->m_maxBlob <= 0 ||
      attribute->m_moveTimeInc <= 0.0 ||
      attribute->m_traceNewPuffTime <= 0.0 ||
      attributeIndex == -1 || subjectTable == ct_NULLID ||
      !VisibleProbeAboveTerrain(32.0, 128.0, &position) ||
      context->isExist(kProbeName) ||
      context->isExist(kSmokeName)) {
    return false;
  }

  const double currentTime = Session::m_moment;
  const double timeStamp = !std::isfinite(currentTime) || currentTime < 0.1
      ? 0.1 : currentTime;
  ExplosionImpactRequest request = {
      position, timeStamp, KR_ObjectID::NUL(), subjectTable,
      attributeIndex, kProbeName};
  int damageApplications = -1;
  const bool executed = ExplosionSubjectState_ExecuteNow(
      context, request, &damageApplications);
  KR_ObjectID explosion = context->searchObject(kProbeName);

  auto rollback = [&]() {
    if (!explosion.isNUL() && context->isExist(explosion)) {
      while (context->removeEvent(EXPLOSION_MOVE, explosion) != 0) {}
      while (context->removeEvent(EXPLOSION_NEWPUFF, explosion) != 0) {}
      context->removeObject(explosion);
    }
    int removed = 0;
    while (SmokeSubjectState_LiveCount() > 0 && removed < 128) {
      KR_ObjectID smoke = context->searchObject(kSmokeName);
      if (smoke.isNUL() ||
          !SmokeSubjectState_RollbackStarted(context, smoke)) {
        break;
      }
      ++removed;
    }
  };

  int tracedPieces = 0;
  bool quotaHeld = false;
  double firstPuffTime = 0.0;
  int startedPuffs = 0;
  KR_ObjectID lastPuff = KR_ObjectID::NUL();
  if (!executed || damageApplications != 0 || explosion.isNUL() ||
      !ExplosionSubjectState_ParentTraceState(
          context, explosion, &tracedPieces, &quotaHeld,
          &firstPuffTime, &startedPuffs, &lastPuff) ||
      tracedPieces <= 0 || !quotaHeld || firstPuffTime <= timeStamp ||
      startedPuffs != 0 || !lastPuff.isNUL() ||
      ExplosionSubjectState_TracedParentCount() != 1) {
    std::fprintf(stderr,
                 "explosion-trace start executed=%d damage=%d explosion=%d "
                 "traced=%d quota=%d first=%.9f time=%.9f puffs=%d "
                 "last=%d parents=%d pos=%.3f/%.3f/%.3f\n",
                 executed ? 1 : 0, damageApplications,
                 explosion.isNUL() ? 0 : 1, tracedPieces,
                 quotaHeld ? 1 : 0, firstPuffTime, timeStamp,
                 startedPuffs, lastPuff.isNUL() ? 0 : 1,
                 ExplosionSubjectState_TracedParentCount(),
                 position.x, position.y, position.z);
    rollback();
    return false;
  }

  double moveTime = timeStamp + attribute->m_moveTimeInc;
  int moveSteps = 0;
  while (moveTime + 1.0e-9 < firstPuffTime && moveSteps < 4096) {
    if (context->removeEvent(EXPLOSION_MOVE, explosion) == 0) {
      std::fprintf(stderr, "explosion-trace missing MOVE at step=%d\n",
                   moveSteps);
      rollback();
      return false;
    }
    KR_Event move;
    move.label = EXPLOSION_MOVE;
    move.source = explosion;
    move.destination = explosion;
    move.timeStamp = moveTime;
    context->sendEventNow(move);
    if (!context->isExist(explosion)) {
      std::fprintf(stderr, "explosion-trace parent expired at step=%d\n",
                   moveSteps);
      rollback();
      return false;
    }
    moveTime += attribute->m_moveTimeInc;
    ++moveSteps;
  }
  const bool puffEventOwned =
      context->removeEvent(EXPLOSION_NEWPUFF, explosion) != 0;
  if (moveSteps >= 4096 || !puffEventOwned) {
    std::fprintf(stderr,
                 "explosion-trace pre-puff steps=%d event=%d\n",
                 moveSteps,
                 puffEventOwned ? 1 : 0);
    rollback();
    return false;
  }
  KR_Event puff;
  puff.label = EXPLOSION_NEWPUFF;
  puff.source = explosion;
  puff.destination = explosion;
  puff.timeStamp = firstPuffTime;
  context->sendEventNow(puff);
  if (context->removeEvent(EXPLOSION_MOVE, explosion) == 0) {
    std::fprintf(stderr, "explosion-trace missing post-puff MOVE\n");
    rollback();
    return false;
  }
  KR_Event move;
  move.label = EXPLOSION_MOVE;
  move.source = explosion;
  move.destination = explosion;
  move.timeStamp = moveTime;
  context->sendEventNow(move);
  ++moveSteps;

  if (!context->isExist(explosion) ||
      !ExplosionSubjectState_ParentTraceState(
          context, explosion, &tracedPieces, &quotaHeld,
          &firstPuffTime, &startedPuffs, &lastPuff) ||
      startedPuffs <= 0 || lastPuff.isNUL() ||
      !context->isExist(lastPuff) ||
      SmokeSubjectState_LiveCount() != startedPuffs) {
    std::fprintf(stderr,
                 "explosion-trace post-puff parent=%d state=%d puffs=%d "
                 "last=%d exists=%d live=%d\n",
                 context->isExist(explosion) ? 1 : 0,
                 ExplosionSubjectState_ParentTraceState(
                     context, explosion, &tracedPieces, &quotaHeld,
                     &firstPuffTime, &startedPuffs, &lastPuff) ? 1 : 0,
                 startedPuffs, lastPuff.isNUL() ? 0 : 1,
                 !lastPuff.isNUL() && context->isExist(lastPuff) ? 1 : 0,
                 SmokeSubjectState_LiveCount());
    rollback();
    return false;
  }

  const dword framesBefore = dwFrames;
  bool visibleFrame = false;
  bool detachedParentFrame = false;
  bool clearedFrame = false;
  int visibleDraws = 0;
  int detachedDraws = 0;
  int clearedDraws = 0;
  bool parentMoveDetached = false;
  bool parentPuffDetached = false;
  KR_ObjectID lastSmoke = lastPuff;
  {
    ScopedAlphaSpriteCapture capture(smokeAttribute->m_cacheImage);
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    visibleDraws = g_alphaSpriteDraws;
    parentMoveDetached =
        context->removeEvent(EXPLOSION_MOVE, explosion) != 0;
    parentPuffDetached =
        context->removeEvent(EXPLOSION_NEWPUFF, explosion) != 0;
    context->removeObject(explosion);
    detachedParentFrame = RecoveredGameServices_RunFrame() != FALSE;
    detachedDraws = g_alphaSpriteDraws;
    int removed = 0;
    while (SmokeSubjectState_LiveCount() > 0 &&
           removed < startedPuffs) {
      KR_ObjectID smoke = context->searchObject(kSmokeName);
      if (smoke.isNUL() ||
          !SmokeSubjectState_RollbackStarted(context, smoke)) {
        break;
      }
      ++removed;
    }
    clearedFrame = removed == startedPuffs &&
        RecoveredGameServices_RunFrame() != FALSE;
    clearedDraws = g_alphaSpriteDraws;
  }

  const bool clean = parentMoveDetached && parentPuffDetached &&
      !context->isExist(explosion) &&
      !context->isExist(lastSmoke) &&
      ExplosionSubjectState_LiveCount() == 0 &&
      ExplosionSubjectState_ParticleBranchLiveCount() == 0 &&
      ExplosionSubjectState_TracedParentCount() == 0 &&
      SmokeSubjectState_LiveCount() == 0 &&
      context->removeEvent(EXPLOSION_MOVE, explosion) == 0 &&
      context->removeEvent(EXPLOSION_NEWPUFF, explosion) == 0 &&
      context->removeEvent(fou_EVC_MOVING, lastSmoke) == 0;
  if (!clean) rollback();
  const bool result = visibleFrame && detachedParentFrame && clearedFrame && clean &&
      g_alphaSpriteDrawValid && visibleDraws > 0 &&
      detachedDraws == visibleDraws * 2 &&
      clearedDraws == detachedDraws &&
      visibleDraws == startedPuffs * smokeAttribute->m_maxBlob &&
      moveSteps > 0 && dwFrames == framesBefore + 3;
  if (!result)
    std::fprintf(stderr,
                 "explosion-trace render visible=%d detached=%d cleared=%d "
                 "clean=%d valid=%d draws=%d/%d/%d expected=%d puffs=%d "
                 "steps=%d frames=%lu/%lu moment=%.9f stamp=%.9f "
                 "first_puff=%.9f move=%.9f\n",
                 visibleFrame ? 1 : 0, detachedParentFrame ? 1 : 0,
                 clearedFrame ? 1 : 0, clean ? 1 : 0,
                 g_alphaSpriteDrawValid ? 1 : 0, visibleDraws,
                 detachedDraws, clearedDraws,
                 startedPuffs * smokeAttribute->m_maxBlob,
                 startedPuffs, moveSteps, dwFrames, framesBefore + 3,
                 Session::m_moment, timeStamp, firstPuffTime, moveTime);
  return result;
}

bool ExerciseVisibleSpark() {
  if (g_super.m_context == nullptr ||
      !RecoveredGameServices_SparkRenderingReady() ||
      SparkSubjectState_LiveCount() != 0 || _pGRDrawSprite == nullptr ||
      g_lightChain.m_count != 0 || g_lightChain.m_list != nullptr ||
      CViewObject::EnabledLights() != 0) {
    return false;
  }
  SimulationContext* context = g_super.m_context;
  KR_ObjectID attributeID = context->searchObject("Spark.Flash");
  AttributeSpark* attribute = attributeID.isNUL()
      ? nullptr
      : static_cast<AttributeSpark*>(
            __attrSparkTable.searchAttribute(attributeID));
  const ct_ClassTableID attributeTable =
      g_arena.searchSeanceClassTable("SparkAttr");
  const ct_ClassTableID subjectTable =
      g_arena.searchSeanceClassTable("Spark");
  const int attributeIndex = attributeTable == ct_NULLID ||
          attributeID.isNUL()
      ? -1
      : g_arena.getAttributeIndex(attributeTable, attributeID);
  CFVector3 position;
  static const char kProbeName[] = "Spark.Rendering.Probe";
  if (attribute == nullptr || attribute->m_cacheSkin == nullptr ||
      attribute->m_cacheSkin->HImage() == nullptr ||
      attribute->m_phaseCnt != 6 || attributeIndex == -1 ||
      subjectTable == ct_NULLID ||
      !VisibleProbePosition(0.0, 64.0, &position) ||
      context->isExist(kProbeName)) {
    return false;
  }

  const double currentTime = Session::m_moment;
  const double timeStamp = !std::isfinite(currentTime) || currentTime < 0.1
      ? 0.1 : currentTime;
  SparkCreateRequest request = {
      position, timeStamp, subjectTable, attributeIndex, kProbeName};
  KR_ObjectID spark = KR_ObjectID::NUL();
  const bool started = SparkSubjectState_ExecuteNow(
      context, request, &spark);
  const bool ownedLife = started &&
      context->removeEvent(sp_EVC_LIFE, spark) != 0;
  if (!started || !ownedLife || spark.isNUL() ||
      SparkSubjectState_LiveCount() != 1) {
    context->removeEvent(sp_EVC_LIFE, spark);
    if (!spark.isNUL() && context->isExist(spark)) {
      context->removeObject(spark);
    }
    return false;
  }

  const SparkPhase& phase = attribute->m_phase[0];
  const dword framesBefore = dwFrames;
  bool visibleFrame = false;
  bool detachedFrame = false;
  bool lightPublished = false;
  int drawsAfterVisibleFrame = 0;
  {
    ScopedSpriteCapture capture(attribute->m_cacheSkin->HImage());
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    drawsAfterVisibleFrame = g_spriteDraws;
    lightPublished = visibleFrame && CViewObject::EnabledLights() == 1 &&
        g_lightChain.m_count == 0 && g_lightChain.m_list == nullptr &&
        _gr_pLights[0].r == phase.radius &&
        _gr_pLights[0].power0 == phase.brightness &&
        _gr_pLights[0].color == phase.color;
    context->removeObject(spark);
    detachedFrame = RecoveredGameServices_RunFrame() != FALSE;
  }
  return visibleFrame && detachedFrame && lightPublished &&
         g_spriteDrawValid && drawsAfterVisibleFrame == 1 &&
         g_spriteDraws == drawsAfterVisibleFrame &&
         g_lastSpriteU0 == (phase.u0 << 16) &&
         g_lastSpriteV0 == (phase.v0 << 16) &&
         g_lastSpriteU1 == (phase.u1 << 16) &&
         g_lastSpriteV1 == (phase.v1 << 16) &&
         dwFrames == framesBefore + 2 &&
         CViewObject::EnabledLights() == 0 &&
         g_lightChain.m_count == 0 && g_lightChain.m_list == nullptr &&
         !context->isExist(kProbeName) &&
         SparkSubjectState_LiveCount() == 0 &&
         context->removeEvent(sp_EV_CREATE, spark) == 0 &&
         context->removeEvent(sp_EVC_LIFE, spark) == 0;
}

bool ValidateReferenceTransaction(
    unsigned long long taxiReferenceFingerprint,
    unsigned long long bulletReferenceFingerprint,
    unsigned long long smokerReferenceFingerprint,
    unsigned long long farterReferenceFingerprint,
    unsigned long long corpseReferenceFingerprint) {
  KR_ObjectID taxiID =
      g_super.m_context->searchObject("Taxi.Attr.CorpseFinal");
  AttributeTaxi* taxi = taxiID.isNUL()
      ? nullptr
      : static_cast<AttributeTaxi*>(__attrTaxiTable.searchAttribute(taxiID));
  if (taxi == nullptr || taxi->m_cacheSkin == nullptr ||
      taxi->m_attrForVehicle.isNUL())
    return false;
  char vehicleName[sizeof(taxi->m_attrForVehicleName)] = {};
  std::memcpy(vehicleName, taxi->m_attrForVehicleName, sizeof(vehicleName));
  CViewObjectModel* taxiSkin = taxi->m_cacheSkin;
  const ct_ClassTableID taxiCorpseTable = taxi->m_cacheCorpseTable;
  const int taxiCorpseAttr = taxi->m_cacheCorpseAttr;
  const KR_ObjectID taxiSkinID = taxi->m_skinID;
  const KR_ObjectID taxiVehicleID = taxi->m_attrForVehicle;
  std::strncpy(taxi->m_attrForVehicleName, "Vehicle.Attr.Missing",
               sizeof(taxi->m_attrForVehicleName) - 1);
  taxi->m_attrForVehicleName[sizeof(taxi->m_attrForVehicleName) - 1] = 0;
  const bool taxiRejected =
      !TaxiAttributeState_ResolveReferences(g_super.m_context) &&
      taxi->m_cacheSkin == taxiSkin &&
      taxi->m_cacheCorpseTable == taxiCorpseTable &&
      taxi->m_cacheCorpseAttr == taxiCorpseAttr &&
      taxi->m_skinID == taxiSkinID &&
      taxi->m_attrForVehicle == taxiVehicleID;
  std::memcpy(taxi->m_attrForVehicleName, vehicleName, sizeof(vehicleName));
  if (!taxiRejected ||
      !TaxiAttributeState_ResolveReferences(g_super.m_context) ||
      TaxiAttributeState_ReferenceFingerprint(g_super.m_context) !=
          taxiReferenceFingerprint)
    return false;

  AttributeBullet* bullet = nullptr;
  __bulletAttrTable.userFind(FindFirstBullet, &bullet);
  if (bullet == nullptr || bullet->m_smokeAttrID.isNUL()) return false;
  char bulletSmokeName[sizeof(bullet->m_smokeAttrName)] = {};
  std::memcpy(bulletSmokeName, bullet->m_smokeAttrName,
              sizeof(bulletSmokeName));
  const unsigned long long bulletCacheFingerprint =
      BulletCacheFingerprint();
  const int textureCheckpoint = SmokeTextureCache_Checkpoint();
  std::strncpy(bullet->m_smokeAttrName, "Smoke.Attr.Missing",
               sizeof(bullet->m_smokeAttrName) - 1);
  bullet->m_smokeAttrName[sizeof(bullet->m_smokeAttrName) - 1] = 0;
  const bool bulletRejected =
      !BulletAttributeState_ResolveReferences(g_super.m_context) &&
      BulletCacheFingerprint() == bulletCacheFingerprint &&
      SmokeTextureCache_Checkpoint() == textureCheckpoint;
  std::memcpy(bullet->m_smokeAttrName, bulletSmokeName,
              sizeof(bulletSmokeName));
  if (!bulletRejected ||
      !BulletAttributeState_ResolveReferences(g_super.m_context) ||
      !BulletAttributeState_IsKnownReferenceRoster(g_super.m_context) ||
      BulletAttributeState_ReferenceFingerprint(g_super.m_context) !=
          bulletReferenceFingerprint)
    return false;

  KR_ObjectID smokerObjectID =
      g_super.m_context->searchObject("Smoker.Attr.Corpse");
  AttributeSmoker* smoker =
      smokerObjectID.isNUL()
          ? nullptr
          : static_cast<AttributeSmoker*>(
                __attrSmokerTable.searchAttribute(smokerObjectID));
  if (smoker == nullptr || smoker->m_smokeAttrID.isNUL()) return false;
  char smokeName[sizeof(smoker->m_smokeAttrName)] = {};
  std::memcpy(smokeName, smoker->m_smokeAttrName, sizeof(smokeName));
  const KR_ObjectID smokeAttrID = smoker->m_smokeAttrID;
  const ct_ClassTableID smokeTableID = smoker->m_smokeTableID;
  const GR_HTEXTURE coronaHText = smoker->m_coronaHText;
  const unsigned long coronaColor = smoker->m_coronaColor;
  std::strncpy(smoker->m_smokeAttrName, "Smoke.Attr.Missing",
               sizeof(smoker->m_smokeAttrName) - 1);
  smoker->m_smokeAttrName[sizeof(smoker->m_smokeAttrName) - 1] = 0;
  const bool smokerRejected =
      !SmokerAttributeState_ResolveReferences(g_super.m_context) &&
      smoker->m_smokeAttrID == smokeAttrID &&
      smoker->m_smokeTableID == smokeTableID &&
      smoker->m_coronaHText == coronaHText &&
      smoker->m_coronaColor == coronaColor;
  std::memcpy(smoker->m_smokeAttrName, smokeName, sizeof(smokeName));
  if (!smokerRejected ||
      !SmokerAttributeState_ResolveReferences(g_super.m_context) ||
      SmokerAttributeState_ReferenceFingerprint(g_super.m_context) !=
          smokerReferenceFingerprint)
    return false;

  KR_ObjectID corpseID =
      g_super.m_context->searchObject("Corpse.Attr.Default");
  AttributeCorpse* corpse =
      corpseID.isNUL()
          ? nullptr
          : static_cast<AttributeCorpse*>(
                __attrCorpseTable.searchAttribute(corpseID));
  if (corpse == nullptr || !corpse->m_isSmoking) return false;
  char smokerName[sizeof(corpse->m_smokerAttr)] = {};
  std::memcpy(smokerName, corpse->m_smokerAttr, sizeof(smokerName));
  CViewObjectModel* skin = corpse->m_cacheSkin;
  const KR_ObjectID skinID = corpse->m_skinID;
  const KR_ObjectID smokerID = corpse->m_smokerAttrID;
  const KR_ObjectID fireID = corpse->m_fireAttrID;
  const ct_ClassTableID smokerTable = corpse->m_smokerTableID;
  std::strncpy(corpse->m_smokerAttr, "Smoker.Attr.Missing",
               sizeof(corpse->m_smokerAttr) - 1);
  corpse->m_smokerAttr[sizeof(corpse->m_smokerAttr) - 1] = 0;
  const bool corpseRejected =
      !CorpseAttributeState_ResolveReferences(g_super.m_context) &&
      corpse->m_cacheSkin == skin && corpse->m_skinID == skinID &&
      corpse->m_smokerAttrID == smokerID && corpse->m_fireAttrID == fireID &&
      corpse->m_smokerTableID == smokerTable;
  std::memcpy(corpse->m_smokerAttr, smokerName, sizeof(smokerName));
  if (!corpseRejected ||
      !CorpseAttributeState_ResolveReferences(g_super.m_context) ||
      CorpseAttributeState_ReferenceFingerprint(g_super.m_context) !=
          corpseReferenceFingerprint)
    return false;

  if (FarterAttributeState_RosterSize(g_super.m_context) == 0) return true;
  KR_ObjectID farterID =
      g_super.m_context->searchObject("Farter.Attr.Factory");
  AttributeFarter* farter =
      farterID.isNUL()
          ? nullptr
          : static_cast<AttributeFarter*>(
                __attrFarterTable.searchAttribute(farterID));
  if (farter == nullptr || farter->m_wav == nullptr) return false;
  char soundName[sizeof(farter->m_soundName)] = {};
  std::memcpy(soundName, farter->m_soundName, sizeof(soundName));
  WAVObj* wav = farter->m_wav;
  const ct_ClassTableID soundTable = farter->m_ctsndID;
  std::strncpy(farter->m_soundName, "wav.Missing",
               sizeof(farter->m_soundName) - 1);
  farter->m_soundName[sizeof(farter->m_soundName) - 1] = 0;
  const bool farterRejected =
      !FarterAttributeState_ResolveReferences(g_super.m_context) &&
      farter->m_wav == wav && farter->m_ctsndID == soundTable;
  std::memcpy(farter->m_soundName, soundName, sizeof(soundName));
  return farterRejected &&
         FarterAttributeState_ResolveReferences(g_super.m_context) &&
         FarterAttributeState_ReferenceFingerprint(g_super.m_context) ==
             farterReferenceFingerprint;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }

  RecoveredGameServices_UseRuntime();
  const double initialSoundDistance = snd_distMax;
  const double initialSoundDistanceSquared = snd_distMax2;
  if (!GameEntry_RuntimeReady() || GameEntry_RuntimeMissingHooks() != 0 ||
      GameEntry_BoundedStartupEnabled()) {
    return Fail("complete service hook inventory is incorrect");
  }
  if (!ZAV_InitGraph(nullptr)) {
    return Fail("public graph initialization failed");
  }

  if (argc == 1) {
    if (ZAV_InitLevel("rr2nw-missing-service-level") != FALSE ||
        !IsServiceReleased() || !IsLevelRolledBack()) {
      ZAV_Deinit();
      return Fail("missing-level service rollback failed");
    }
    ZAV_Deinit();
    return RecoveredSoftwareGraph_IsReady()
               ? Fail("graph survived complete shutdown")
               : EXIT_SUCCESS;
  }

  if (!StartServices(argv[1])) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("service initialization failed");
  }

  SRecoveredVehicleControlReplayTelemetry replayTelemetry = {};
  SRecoveredVehicleControlJournalTelemetry initialJournalTelemetry = {};
  if (!RecoveredGameServices_VehicleControlReplayReady() ||
      !RecoveredGameServices_VehicleControlReplayTelemetry(
          &replayTelemetry) ||
      replayTelemetry.recordings != 1 || replayTelemetry.replays != 1 ||
      replayTelemetry.codecRoundTrips != 1 ||
      replayTelemetry.actionRecords != 3 ||
      replayTelemetry.focusRecords != 2 ||
      replayTelemetry.syntheticReleases != 1 ||
      replayTelemetry.simulationFrames != 28 ||
      replayTelemetry.stateMatches != 1 ||
      replayTelemetry.clockMatches != 1 ||
      replayTelemetry.randomMatches != 1 ||
      replayTelemetry.rollbacks != 2 ||
      replayTelemetry.encodedBytes == 0 ||
      replayTelemetry.journalFingerprint == 0 ||
      replayTelemetry.recordedStateFingerprint == 0 ||
      replayTelemetry.recordedStateFingerprint !=
          replayTelemetry.replayedStateFingerprint ||
      !RecoveredGameServices_VehicleControlJournalTelemetry(
          &initialJournalTelemetry) ||
      initialJournalTelemetry.checkpointTick >
          initialJournalTelemetry.lastRecordTick ||
      initialJournalTelemetry.recordCount != 0 ||
      initialJournalTelemetry.actionRecords != 0 ||
      initialJournalTelemetry.focusRecords != 0 ||
      initialJournalTelemetry.encodedBytes == 0 ||
      initialJournalTelemetry.journalFingerprint == 0 ||
      initialJournalTelemetry.appendFailures != 0 ||
      initialJournalTelemetry.recording != 1 ||
      initialJournalTelemetry.applicationActive != 1) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Vehicle control journal/replay proof was not admitted");
  }

  KR_Event wake;
  wake.label = KR_WAKE_UP;
  if (g_super.m_level.receiveEvent(wake) != 1) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("bounded Level event was not dispatched");
  }
  g_debugMap.Draw();
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  KR_ObjectID vehicleID =
      g_super.m_context->searchObject("Vehicle.Default");
  KR_ObjectID birdID = g_super.m_context->searchObject("Bird.Attr.0");
  KR_ObjectID orphanID =
      g_super.m_context->searchObject("Orphan.Attr.Default");
  KR_ObjectID artefactID =
      g_super.m_context->searchObject("Artefact.Attr.0");
  KR_ObjectID sparkID = g_super.m_context->searchObject("Spark.Flash");
  KR_ObjectID smokeID =
      g_super.m_context->searchObject("Smoke.Attr.Small");
  if (!RecoveredGameServices_HardwareReady() ||
      !RecoveredGameServices_SeanceReady() ||
      !RecoveredGameServices_BirdAttributesReady() ||
      !RecoveredGameServices_PortalReady() ||
      !RecoveredGameServices_OrphanAttributesReady() ||
      !RecoveredGameServices_OrphanReferencesReady() ||
      !RecoveredGameServices_OrphanSubjectReady() ||
      RecoveredGameServices_OrphanSubjectCapacity() != 5 ||
      RecoveredGameServices_OrphanSubjectCount() != 0 ||
      RecoveredGameServices_OrphanReferenceFingerprint() == 0 ||
      RecoveredGameServices_OrphanSubjectFingerprint() == 0 ||
      !RecoveredGameServices_ArtefactAttributesReady() ||
      !RecoveredGameServices_SmokeAttributesReady() ||
      !RecoveredGameServices_SmokeSubjectReady() ||
      !RecoveredGameServices_SmokeTerrainReady() ||
      !RecoveredGameServices_SmokeRenderingReady() ||
      !RecoveredGameServices_SmokeVisualResourcesReady() ||
      !RecoveredGameServices_ExplosionAttributesReady() ||
      !RecoveredGameServices_ExplosionSubjectReady() ||
      !RecoveredGameServices_ExplosionImpulseReady() ||
      !RecoveredGameServices_ExplosionLightReady() ||
      !RecoveredGameServices_ExplosionSoundReady() ||
      !RecoveredGameServices_ExplosionParticlesReady() ||
      !RecoveredGameServices_ExplosionSmokeReady() ||
      !RecoveredGameServices_ExplosionPieceReady() ||
      !RecoveredGameServices_ExplosionTraceReady() ||
      !RecoveredGameServices_VehicleAttributesReady() ||
       !RecoveredGameServices_VehicleReferencesReady() ||
       !RecoveredGameServices_TaxiAttributesReady() ||
       !RecoveredGameServices_TaxiReferencesReady() ||
       !RecoveredGameServices_TaxiSubjectReady() ||
       !RecoveredGameServices_TaxiVehicleTransitionReady() ||
       !RecoveredGameServices_BulletAttributesReady() ||
       !RecoveredGameServices_BulletReferencesReady() ||
       !RecoveredGameServices_BulletSubjectRegistrationReady() ||
       !RecoveredGameServices_BulletSubjectReady() ||
       !RecoveredGameServices_BulletImpactEffectsReady() ||
       !RecoveredGameServices_BulletGroundSparkReady() ||
       !RecoveredGameServices_BulletBarrelSmokeReady() ||
      !RecoveredGameServices_SmokerAttributesReady() ||
      !RecoveredGameServices_SmokerReferencesReady() ||
      !RecoveredGameServices_SmokerRuntimeReady() ||
      !RecoveredGameServices_SmokerEmissionReady() ||
      !RecoveredGameServices_SmokerLightCoronaReady() ||
      !RecoveredGameServices_DynSmokerReady() ||
      !RecoveredGameServices_FarterAttributesReady() ||
      !RecoveredGameServices_FarterReferencesReady() ||
      !RecoveredGameServices_FarterRuntimeReady() ||
      !RecoveredGameServices_FarterSubjectReady() ||
      !RecoveredGameServices_LampAttributesReady() ||
       !RecoveredGameServices_CorpseAttributesReady() ||
       !RecoveredGameServices_CorpseSubjectReady() ||
      !RecoveredGameServices_WavMetadataReady() ||
      !RecoveredGameServices_SoundObjectReady() ||
      !RecoveredGameServices_SkinResourcesReady() ||
      !RecoveredGameServices_SparkAttributesReady() ||
      !RecoveredGameServices_SparkSubjectReady() ||
      !RecoveredGameServices_SparkRenderingReady() ||
      !RecoveredArenaSeance_SparkVisualResourcesReady() ||
      RecoveredArenaSeance_SparkSubjectCapacity() != 40 ||
      RecoveredArenaSeance_SparkSubjectFingerprint() == 0 ||
      RecoveredArenaSeance_SparkVisualResourceFingerprint() == 0 ||
      RecoveredArenaSeance_SparkProbeInvalidStarts() != 2 ||
      RecoveredArenaSeance_SparkProbeQueuedCreates() != 1 ||
      RecoveredArenaSeance_SparkProbeQueueRollbacks() != 1 ||
      RecoveredArenaSeance_SparkProbePhaseTransitions() != 5 ||
      RecoveredArenaSeance_SparkProbeExpirations() != 1 ||
      !RecoveredGameServices_RouteReady() ||
      !RecoveredGameServices_PeopleAttributesReady() ||
      !RecoveredGameServices_PeopleReferencesReady() ||
      !RecoveredGameServices_PeopleSubjectReady() ||
      !RecoveredGameServices_TankCannonAttributesReady() ||
      !RecoveredGameServices_TankReferencesReady() ||
      !RecoveredGameServices_TankCannonSubjectTablesReady() ||
      !RecoveredGameServices_VehicleReady() ||
      !RecoveredGameServices_VehicleMovementReady() ||
      RecoveredGameServices_VehicleRuntimeFingerprint() == 0 ||
      (RecoveredGameServices_VehicleVesselKind() !=
           RECOVERED_VEHICLE_VESSEL_EMV &&
       RecoveredGameServices_VehicleVesselKind() !=
           RECOVERED_VEHICLE_VESSEL_WHEELS) ||
      RecoveredGameServices_VehicleProbeInvalidActivations() != 2 ||
      RecoveredGameServices_VehicleProbeActivations() != 1 ||
      RecoveredGameServices_VehicleProbeStationarySteps() != 1 ||
      RecoveredGameServices_VehicleProbeThrottleEvents() != 2 ||
      RecoveredGameServices_VehicleProbeMovementSteps() != 172 ||
      RecoveredGameServices_VehicleProbeTurnEvents() != 2 ||
      RecoveredGameServices_VehicleProbeCameraTransitions() != 1 ||
      RecoveredGameServices_VehicleProbeRollbacks() != 1 ||
      RecoveredGameServices_VehicleProbeHorizontalDistance() <= 0.01 ||
      !IsVehicleControlActive(vehicleID, nullptr) ||
      RecoveredGameServices_VehicleVesselMass() <= 0.0 ||
      RecoveredArenaSeance_Issues() != 0 || birdID.isNUL() ||
      RecoveredArenaSeance_ExtendedIssues() != 0 ||
      orphanID.isNUL() || artefactID.isNUL() || smokeID.isNUL() ||
      sparkID.isNUL() ||
      vehicleID.isNUL() ||
      g_vehicle == nullptr ||
      g_super.m_context->queryInterface(vehicleID, IVehicleIID) != g_vehicle ||
      !ExplosionSubjectState_ImpulseTargetReady(
          g_super.m_context, vehicleID) ||
      observer == nullptr) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail(
        "recovered Hardware, Arena, common attributes, Portal, Spark, Route, "
        "Vehicle or observer was not published");
  }
  const bool smokeTraceSupported = SmokeSubjectState_SimulationSupported(
      g_super.m_context, "Smoke.Attr.Trace");
  const bool smokeTraceProbe = smokeTraceSupported &&
      SmokeSubjectState_ProbeSimulationLifecycle(
          g_super.m_context, "Smoke.Attr.Trace", Session::m_moment);
  const bool smokeFireSupported = SmokeSubjectState_SimulationSupported(
      g_super.m_context, "Smoke.Attr.FireArea");
  const bool smokeFireProbe = smokeFireSupported &&
      SmokeSubjectState_ProbeSimulationLifecycle(
          g_super.m_context, "Smoke.Attr.FireArea", Session::m_moment);
  const bool corpseSmokerSupported = SmokerSubjectState_EmissionSupported(
      g_super.m_context, "Smoker.Attr.Corpse");
  const bool corpseSmokerProbe = corpseSmokerSupported &&
      SmokerSubjectState_ProbeEmissionLifecycle(
          g_super.m_context, "Smoker.Attr.Corpse", Session::m_moment);
  const bool fireSmokerSupported = SmokerSubjectState_EmissionSupported(
      g_super.m_context, "Smoker.Attr.FireArea");
  const bool fireSmokerProbe = fireSmokerSupported &&
      SmokerSubjectState_ProbeEmissionLifecycle(
          g_super.m_context, "Smoker.Attr.FireArea", Session::m_moment);
  const bool coronaSupported = SmokerSubjectState_LightCoronaSupported(
      g_super.m_context, "Smoker.Attr.FireMd");
  const bool coronaProbe = coronaSupported &&
      SmokerSubjectState_ProbeLightCoronaLifecycle(
          g_super.m_context, "Smoker.Attr.FireMd", Session::m_moment);
  const bool soundProbe = SoundObjectState_ProbeLifecycle(
      g_super.m_context, "wav.Explosion", Session::m_moment);
  const int smokeLive = SmokeSubjectState_LiveCount();
  const int dynSmokerLive = SmokerSubjectState_DynLiveCount();
  const int soundLive = SoundObjectState_LiveCount();
  const int expectedSoundLive =
      RecoveredArenaSeance_FarterScriptObjectCount() +
      RecoveredArenaSeance_TaxiSubjectSoundCount() +
      RecoveredArenaSeance_PeopleSubjectSoundCount();
  if (!smokeTraceProbe || !smokeFireProbe || !corpseSmokerProbe ||
      !fireSmokerProbe || !coronaProbe || !soundProbe ||
      smokeLive != 0 || dynSmokerLive != 0 ||
      soundLive != expectedSoundLive) {
    std::fprintf(
        stderr,
        "effect-lifecycle diagnostics trace=%d/%d fire=%d/%d "
        "corpse=%d/%d fire_smoker=%d/%d corona=%d/%d sound=%d "
        "live=%d/%d/%d expected_sound=%d\n",
        smokeTraceSupported ? 1 : 0, smokeTraceProbe ? 1 : 0,
        smokeFireSupported ? 1 : 0, smokeFireProbe ? 1 : 0,
        corpseSmokerSupported ? 1 : 0, corpseSmokerProbe ? 1 : 0,
        fireSmokerSupported ? 1 : 0, fireSmokerProbe ? 1 : 0,
        coronaSupported ? 1 : 0, coronaProbe ? 1 : 0,
        soundProbe ? 1 : 0, smokeLive, dynSmokerLive, soundLive,
        expectedSoundLive);
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail(
        "retail Smoke/Smoker/SoundObj lifecycle failed");
  }
  const unsigned long long explosionFingerprint =
      ExplosionAttributeState_Fingerprint(g_super.m_context);
  const int explosionRosterSize =
      ExplosionAttributeState_RosterSize(g_super.m_context);
  const int explosionSubjectCapacity =
      ExplosionSubjectState_Capacity();
  const unsigned long long explosionSubjectFingerprint =
      ExplosionSubjectState_Fingerprint(g_super.m_context);
  const unsigned long long explosionSoundReferenceFingerprint =
      ExplosionAttributeState_SoundReferenceFingerprint(
          g_super.m_context);
  const unsigned long long explosionParticleVisualFingerprint =
      ExplosionAttributeState_ParticleVisualFingerprint(
          g_super.m_context);
  const unsigned long long explosionSmokeVisualFingerprint =
      ExplosionAttributeState_SmokeVisualFingerprint(
          g_super.m_context);
  const unsigned long long explosionPieceReferenceFingerprint =
      ExplosionAttributeState_PieceReferenceFingerprint(
          g_super.m_context);
  const unsigned long long explosionTraceReferenceFingerprint =
      ExplosionAttributeState_TraceReferenceFingerprint(
          g_super.m_context);
  const int explosionPieceStartedPieces =
      RecoveredArenaSeance_ExplosionPieceProbeStartedPieces();
  const int explosionPieceDependencySkips =
      RecoveredArenaSeance_ExplosionPieceProbeDependencySkips();
  const int explosionPieceMoveSteps =
      RecoveredArenaSeance_ExplosionPieceProbeMoveSteps();
  const int explosionPieceExpiredParents =
      RecoveredArenaSeance_ExplosionPieceProbeExpiredParents();
  const int explosionPieceRolledBackPieces =
      RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces();
  const int explosionTraceStartedPieces =
      RecoveredArenaSeance_ExplosionTraceProbeStartedPieces();
  const int explosionTraceQuotaSkips =
      RecoveredArenaSeance_ExplosionTraceProbeQuotaGateSkips();
  const int explosionTracePuffEvents =
      RecoveredArenaSeance_ExplosionTraceProbePuffEvents();
  const int explosionTraceSmokeChildren =
      RecoveredArenaSeance_ExplosionTraceProbeSmokeChildren();
  const int explosionTraceMoveSteps =
      RecoveredArenaSeance_ExplosionTraceProbeMoveSteps();
  const int explosionTraceExpiredParents =
      RecoveredArenaSeance_ExplosionTraceProbeExpiredParents();
  const int explosionTraceRolledBackPieces =
      RecoveredArenaSeance_ExplosionTraceProbeRolledBackPieces();
  const int explosionSmokeStartedSprites =
      RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites();
  const int explosionSmokeDependencySkips =
      RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips();
  const int explosionSmokeMoveSteps =
      RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps();
  const int explosionSmokeExpiredParents =
      RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents();
  const int explosionSmokeRolledBackSprites =
      RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites();
  const int explosionParticleStartedBranches =
      RecoveredArenaSeance_ExplosionParticleProbeStartedBranches();
  const int explosionParticleSimple =
      RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles();
  const int explosionParticleSnake =
      RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles();
  const int explosionParticleRays =
      RecoveredArenaSeance_ExplosionParticleProbeRays();
  const int explosionParticleDependencySkips =
      RecoveredArenaSeance_ExplosionParticleProbeDependencySkips();
  const int explosionParticleMoveSteps =
      RecoveredArenaSeance_ExplosionParticleProbeMoveSteps();
  const int explosionParticleExpiredParents =
      RecoveredArenaSeance_ExplosionParticleProbeExpiredParents();
  const int explosionParticleRolledBackBranches =
      RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches();
  const unsigned long long taxiFingerprint =
      TaxiAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long taxiReferenceFingerprint =
      TaxiAttributeState_ReferenceFingerprint(g_super.m_context);
  const int taxiRosterSize =
      TaxiAttributeState_RosterSize(g_super.m_context);
  const int taxiCapacity = TaxiAttributeState_Capacity();
  const int taxiSubjectCapacity =
      RecoveredArenaSeance_TaxiSubjectCapacity();
  const int taxiSubjectCount = RecoveredArenaSeance_TaxiSubjectCount();
  const int taxiSubjectSoundCount =
      RecoveredArenaSeance_TaxiSubjectSoundCount();
  const unsigned long long taxiSubjectFingerprint =
      RecoveredArenaSeance_TaxiSubjectFingerprint();
  const int taxiVehicleAvailableTaxis =
      RecoveredGameServices_TaxiVehicleProbeAvailableTaxis();
  const int taxiVehicleInvalidTargets =
      RecoveredGameServices_TaxiVehicleProbeInvalidTargets();
  const int taxiVehicleTransitions =
      RecoveredGameServices_TaxiVehicleProbeTransitions();
  const int taxiVehicleAttributeTransfers =
      RecoveredGameServices_TaxiVehicleProbeAttributeTransfers();
  const int taxiVehiclePoseTransfers =
      RecoveredGameServices_TaxiVehicleProbePoseTransfers();
  const int taxiVehiclePayloadTransfers =
      RecoveredGameServices_TaxiVehicleProbePayloadTransfers();
  const int taxiVehicleRemovedTaxis =
      RecoveredGameServices_TaxiVehicleProbeRemovedTaxis();
  const int taxiVehicleRollbacks =
      RecoveredGameServices_TaxiVehicleProbeRollbacks();
  const unsigned long long bulletFingerprint =
      BulletAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long bulletReferenceFingerprint =
      BulletAttributeState_ReferenceFingerprint(g_super.m_context);
  const int bulletRosterSize =
      BulletAttributeState_RosterSize(g_super.m_context);
  const int bulletCapacity = BulletAttributeState_Capacity();
  const int bulletSubjectCapacity =
      BulletAttributeState_SubjectCapacity();
  const unsigned long long bulletSubjectFingerprint =
      BulletSubjectState_Fingerprint(g_super.m_context);
  const int peopleAttributeCount =
      RecoveredArenaSeance_PeopleAttributeCount();
  const int peopleAttributeCapacity =
      RecoveredArenaSeance_PeopleAttributeCapacity();
  const int peopleSubjectCount =
      RecoveredArenaSeance_PeopleSubjectCount();
  const int peopleSubjectCapacity =
      RecoveredArenaSeance_PeopleSubjectCapacity();
  const int peopleSubjectSoundCount =
      RecoveredArenaSeance_PeopleSubjectSoundCount();
  const unsigned long long peopleAttributeFingerprint =
      RecoveredArenaSeance_PeopleAttributeFingerprint();
  const unsigned long long peopleSubjectFingerprint =
      RecoveredArenaSeance_PeopleSubjectFingerprint();
  const int peopleProbeScheduledMoves =
      RecoveredArenaSeance_PeopleProbeScheduledMoves();
  const int peopleProbeBulletDamage =
      RecoveredArenaSeance_PeopleProbeBulletDamageApplications();
  const int peopleProbeDeathTransitions =
      RecoveredArenaSeance_PeopleProbeDeathTransitions();
  const int peopleProbeSaveRoundTrips =
      RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips();
  const int peopleProbeRollbacks =
      RecoveredArenaSeance_PeopleProbeRollbacks();
  const int peopleActiveWorldReconstructedIDs =
      RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs();
  const int peopleActiveWorldSchedulerEvents =
      RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents();
  const int peopleActiveWorldRollbacks =
      RecoveredArenaSeance_PeopleActiveWorldRollbacks();
  const unsigned long long peopleActiveWorldFingerprint =
      RecoveredArenaSeance_PeopleActiveWorldFingerprint();
  const int tankAttributeCount =
      RecoveredArenaSeance_TankAttributeCount();
  const int tankAttributeCapacity =
      RecoveredArenaSeance_TankAttributeCapacity();
  const int tankSubjectCount = RecoveredArenaSeance_TankSubjectCount();
  const int tankSubjectCapacity =
      RecoveredArenaSeance_TankSubjectCapacity();
  const unsigned long long tankAttributeFingerprint =
      RecoveredArenaSeance_TankAttributeFingerprint();
  const unsigned long long tankSubjectFingerprint =
      RecoveredArenaSeance_TankSubjectFingerprint();
  const int tankProbeAvailable =
      RecoveredArenaSeance_TankProbeAvailable();
  const int tankProbeValidStarts =
      RecoveredArenaSeance_TankProbeValidStarts();
  const int tankProbeDynamicReady =
      RecoveredArenaSeance_TankProbeDynamicReady();
  const int tankProbeRenderReady =
      RecoveredArenaSeance_TankProbeRenderReady();
  const int tankProbeCannonReady =
      RecoveredArenaSeance_TankProbeCannonReady();
  const int tankProbeScheduledMoves =
      RecoveredArenaSeance_TankProbeScheduledMoves();
  const int tankProbeBulletDamage =
      RecoveredArenaSeance_TankProbeBulletDamageApplications();
  const int tankProbeDeathTransitions =
      RecoveredArenaSeance_TankProbeDeathTransitions();
  const int tankProbeDeathEffects =
      RecoveredArenaSeance_TankProbeDeathEffects();
  const int tankProbeSaveRoundTrips =
      RecoveredArenaSeance_TankProbeSaveStateRoundTrips();
  const int tankProbeRollbacks =
      RecoveredArenaSeance_TankProbeRollbacks();
  const int commanderCapacity =
      RecoveredArenaSeance_CommanderCapacity();
  const int commanderCount = RecoveredArenaSeance_CommanderCount();
  const int commanderHostileLinks =
      RecoveredArenaSeance_CommanderHostileLinks();
  const unsigned long long commanderFingerprint =
      RecoveredArenaSeance_CommanderFingerprint();
  const int tankGroupSubjectCapacity =
      RecoveredArenaSeance_TankGroupSubjectCapacity();
  const int missionTankAvailable =
      RecoveredArenaSeance_MissionTankAvailable();
  const int missionTankSpawns =
      RecoveredArenaSeance_MissionTankSpawns();
  const int missionTankMembershipLinks =
      RecoveredArenaSeance_MissionTankMembershipLinks();
  const int missionTankFindEnemyCycles =
      RecoveredArenaSeance_MissionTankFindEnemyCycles();
  const int missionTankMovingCycles =
      RecoveredArenaSeance_MissionTankMovingCycles();
  const int missionTankStableRoundTrips =
      RecoveredArenaSeance_MissionTankStableRoundTrips();
  const int missionTankReconstructedIDs =
      RecoveredArenaSeance_MissionTankReconstructedIDs();
  const int missionTankRollbacks =
      RecoveredArenaSeance_MissionTankRollbacks();
  const unsigned long long missionTankFingerprint =
      RecoveredArenaSeance_MissionTankFingerprint();
  const int cannonAttributeCount =
      RecoveredArenaSeance_CannonAttributeCount();
  const int cannonAttributeCapacity =
      RecoveredArenaSeance_CannonAttributeCapacity();
  const int cannonSubjectCount =
      RecoveredArenaSeance_CannonSubjectCount();
  const int cannonSubjectCapacity =
      RecoveredArenaSeance_CannonSubjectCapacity();
  const unsigned long long cannonAttributeFingerprint =
      RecoveredArenaSeance_CannonAttributeFingerprint();
  const unsigned long long cannonSubjectFingerprint =
      RecoveredArenaSeance_CannonSubjectFingerprint();
  const unsigned long long vehicleAttributeFingerprint =
      VehicleAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long vehicleReferenceFingerprint =
      VehicleAttributeState_ReferenceFingerprint(g_super.m_context);
  const int vehicleAttributeRosterSize =
      VehicleAttributeState_RosterSize(g_super.m_context);
  const int vehicleAttributeCapacity = VehicleAttributeState_Capacity();
  const double vehicleVesselMass =
      RecoveredGameServices_VehicleVesselMass();
  const unsigned long long vehicleRuntimeFingerprint =
      RecoveredGameServices_VehicleRuntimeFingerprint();
  const int vehicleVesselKind =
      RecoveredGameServices_VehicleVesselKind();
  const int vehicleProbeInvalidActivations =
      RecoveredGameServices_VehicleProbeInvalidActivations();
  const int vehicleProbeActivations =
      RecoveredGameServices_VehicleProbeActivations();
  const int vehicleProbeStationarySteps =
      RecoveredGameServices_VehicleProbeStationarySteps();
  const int vehicleProbeThrottleEvents =
      RecoveredGameServices_VehicleProbeThrottleEvents();
  const int vehicleProbeMovementSteps =
      RecoveredGameServices_VehicleProbeMovementSteps();
  const int vehicleProbeTurnEvents =
      RecoveredGameServices_VehicleProbeTurnEvents();
  const int vehicleProbeCameraTransitions =
      RecoveredGameServices_VehicleProbeCameraTransitions();
  const int vehicleProbeRollbacks =
      RecoveredGameServices_VehicleProbeRollbacks();
  const double vehicleProbeHorizontalDistance =
      RecoveredGameServices_VehicleProbeHorizontalDistance();
  const unsigned long long smokerFingerprint =
      SmokerAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long smokerReferenceFingerprint =
      SmokerAttributeState_ReferenceFingerprint(g_super.m_context);
  const bool smokerRuntimeReady =
      RecoveredArenaSeance_SmokerRuntimeReady();
  const int smokeSubjectCapacity =
      RecoveredArenaSeance_SmokeSubjectCapacity();
  const unsigned long long smokeSubjectFingerprint =
      RecoveredArenaSeance_SmokeSubjectFingerprint();
  const unsigned long long smokeVisualResourceFingerprint =
      RecoveredArenaSeance_SmokeVisualResourceFingerprint();
  const int smokerRosterSize =
      SmokerAttributeState_RosterSize(g_super.m_context);
  const int smokerCapacity = SmokerAttributeState_Capacity();
  const int dynSmokerCapacity =
      RecoveredArenaSeance_DynSmokerCapacity();
  const unsigned long long dynSmokerFingerprint =
      RecoveredArenaSeance_DynSmokerFingerprint();
  const unsigned long long wavFingerprint =
      WAVResourceState_Fingerprint(g_super.m_context);
  const int wavRosterSize =
      WAVResourceState_RosterSize(g_super.m_context);
  const int wavCapacity = WAVResourceState_Capacity();
  const int soundObjectCapacity =
      RecoveredArenaSeance_SoundObjectCapacity();
  const unsigned long long soundObjectFingerprint =
      RecoveredArenaSeance_SoundObjectFingerprint();
  const unsigned long long farterFingerprint =
      FarterAttributeState_Fingerprint(g_super.m_context);
  const int farterSubjectCapacity =
      RecoveredArenaSeance_FarterSubjectCapacity();
  const unsigned long long farterSubjectFingerprint =
      RecoveredArenaSeance_FarterSubjectFingerprint();
  const int farterScriptObjectCount =
      RecoveredArenaSeance_FarterScriptObjectCount();
  const int farterLiveObjectCount =
      RecoveredArenaSeance_FarterLiveObjectCount();
  const int farterSoundObjectCount =
      RecoveredArenaSeance_FarterSoundObjectCount();
  const int farterNearFrameAudibleCount =
      RecoveredArenaSeance_FarterNearFrameAudibleCount();
  const int farterFarFrameAudibleCount =
      RecoveredArenaSeance_FarterFarFrameAudibleCount();
  const bool farterAudibleFrameTransition =
      RecoveredArenaSeance_FarterAudibleFrameTransition();
  const double soundDistance = RecoveredArenaSeance_SoundDistance();
  const double soundDistanceSquared =
      RecoveredArenaSeance_SoundDistanceSquared();
  const unsigned long long farterReferenceFingerprint =
      FarterAttributeState_ReferenceFingerprint(g_super.m_context);
  const bool farterRuntimeReady =
      RecoveredArenaSeance_FarterRuntimeReady();
  const int farterRosterSize =
      FarterAttributeState_RosterSize(g_super.m_context);
  const int farterCapacity = FarterAttributeState_Capacity();
  const unsigned long long lampFingerprint =
      LampAttributeState_Fingerprint(g_super.m_context);
  const int lampRosterSize = LampAttributeState_RosterSize(g_super.m_context);
  const int lampCapacity = LampAttributeState_Capacity();
  const unsigned long long corpseFingerprint =
      CorpseAttributeState_Fingerprint(g_super.m_context);
  const unsigned long long corpseReferenceFingerprint =
      CorpseAttributeState_ReferenceFingerprint(g_super.m_context);
  const bool corpseRuntimeReady =
      RecoveredArenaSeance_CorpseRuntimeReady();
  const int corpseSubjectCapacity =
      RecoveredArenaSeance_CorpseSubjectCapacity();
  const unsigned long long corpseSubjectFingerprint =
      RecoveredArenaSeance_CorpseSubjectFingerprint();
  const int corpseRosterSize =
      CorpseAttributeState_RosterSize(g_super.m_context);
  const int corpseCapacity = CorpseAttributeState_Capacity();
  const int skinModelCount = RecoveredArenaSeance_SkinModelCount();
  const int skinSpriteCount = RecoveredArenaSeance_SkinSpriteCount();
  const unsigned long long skinCatalogFingerprint =
      RecoveredArenaSeance_SkinCatalogFingerprint();
  const unsigned long long skinResourceFingerprint =
      RecoveredArenaSeance_SkinResourceFingerprint();
  const int sparkSubjectCapacity =
      RecoveredArenaSeance_SparkSubjectCapacity();
  const unsigned long long sparkSubjectFingerprint =
      RecoveredArenaSeance_SparkSubjectFingerprint();
  const unsigned long long sparkVisualResourceFingerprint =
      RecoveredArenaSeance_SparkVisualResourceFingerprint();
  if (explosionFingerprint == 0 || explosionRosterSize < 10 ||
      explosionRosterSize > 14 ||
      (explosionSubjectCapacity != 40 &&
       explosionSubjectCapacity != 50 &&
       explosionSubjectCapacity != 60) ||
      !ExplosionSubjectState_TableReady(g_super.m_context,
                                        explosionSubjectCapacity) ||
      ExplosionSubjectState_LiveCount() != 0 ||
      explosionSubjectFingerprint == 0 ||
      RecoveredArenaSeance_ExplosionSubjectCapacity() !=
          explosionSubjectCapacity ||
      RecoveredArenaSeance_ExplosionSubjectFingerprint() !=
          explosionSubjectFingerprint ||
      !RecoveredGameServices_ExplosionImpulseReady() ||
      !RecoveredGameServices_ExplosionLightReady() ||
      !RecoveredGameServices_ExplosionSoundReady() ||
      !RecoveredGameServices_ExplosionParticlesReady() ||
      !RecoveredGameServices_ExplosionSmokeReady() ||
      !ExplosionAttributeState_ParticleVisualsResolved(
          g_super.m_context) ||
      !ExplosionAttributeState_IsKnownParticleVisualRoster(
          g_super.m_context) ||
      RecoveredArenaSeance_ExplosionParticleVisualFingerprint() == 0 ||
      RecoveredArenaSeance_ExplosionParticleVisualFingerprint() !=
          explosionParticleVisualFingerprint ||
      RecoveredArenaSeance_ExplosionParticleProbeStartedBranches() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeRays() < 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionParticleProbeMoveSteps() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeExpiredParents() != 1 ||
      RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches() !=
          RecoveredArenaSeance_ExplosionParticleProbeStartedBranches() ||
      !ExplosionAttributeState_SmokeVisualsResolved(g_super.m_context) ||
      !ExplosionAttributeState_IsKnownSmokeVisualRoster(g_super.m_context) ||
      explosionSmokeVisualFingerprint == 0 ||
      RecoveredArenaSeance_ExplosionSmokeVisualFingerprint() !=
          explosionSmokeVisualFingerprint ||
      RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() <= 0 ||
      RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps() <= 0 ||
      RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents() != 1 ||
      RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites() !=
          RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() ||
      !RecoveredGameServices_ExplosionPieceReady() ||
      !ExplosionAttributeState_PieceReferencesResolved(g_super.m_context) ||
      !ExplosionAttributeState_IsKnownPieceReferenceRoster(
          g_super.m_context) ||
      explosionPieceReferenceFingerprint == 0 ||
      RecoveredArenaSeance_ExplosionPieceReferenceFingerprint() !=
          explosionPieceReferenceFingerprint ||
      RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() <= 0 ||
      RecoveredArenaSeance_ExplosionPieceProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionPieceProbeMoveSteps() <= 0 ||
      RecoveredArenaSeance_ExplosionPieceProbeExpiredParents() != 1 ||
      RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces() !=
          RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() ||
      !RecoveredGameServices_ExplosionTraceReady() ||
      !ExplosionAttributeState_TraceReferencesResolved(g_super.m_context) ||
      !ExplosionAttributeState_IsKnownTraceReferenceRoster(
          g_super.m_context) ||
      explosionTraceReferenceFingerprint == 0 ||
      RecoveredArenaSeance_ExplosionTraceReferenceFingerprint() !=
          explosionTraceReferenceFingerprint ||
      explosionTraceStartedPieces <= 0 || explosionTraceQuotaSkips != 1 ||
      explosionTracePuffEvents != 1 ||
      explosionTraceSmokeChildren <= 0 || explosionTraceMoveSteps <= 0 ||
      explosionTraceExpiredParents != 1 ||
      explosionTraceRolledBackPieces != explosionTraceStartedPieces ||
      ExplosionSubjectState_ParticleBranchLiveCount() != 0 ||
      ExplosionSubjectState_TracedParentCount() != 0 ||
      ExplosionSubjectState_ParticleBranchCapacity() != 500 ||
      !ExplosionAttributeState_SoundReferencesResolved(g_super.m_context) ||
      !ExplosionAttributeState_IsKnownSoundReferenceRoster(
          g_super.m_context) ||
      explosionSoundReferenceFingerprint == 0 ||
      RecoveredArenaSeance_ExplosionSoundReferenceFingerprint() !=
          explosionSoundReferenceFingerprint ||
      RecoveredArenaSeance_ExplosionSoundProbeStarted() != 1 ||
      RecoveredArenaSeance_ExplosionSoundProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionSoundProbeRollbacks() != 1 ||
      RecoveredArenaSeance_ExplosionProbeInvalidStarts() != 2 ||
      RecoveredArenaSeance_ExplosionProbeAllocationRollbacks() != 1 ||
      RecoveredArenaSeance_ExplosionProbeQueuedCommands() != 1 ||
      RecoveredArenaSeance_ExplosionProbeQueueRollbacks() != 1 ||
      RecoveredArenaSeance_ExplosionProbeExecutedCommands() != 1 ||
      RecoveredArenaSeance_ExplosionProbeDamageApplications() != 0 ||
      taxiFingerprint == 0 ||
      taxiRosterSize < 2 || taxiRosterSize > 10 ||
      taxiCapacity < taxiRosterSize || taxiCapacity > 10 ||
      !TaxiAttributeState_IsKnownRoster(g_super.m_context) ||
      !TaxiAttributeState_ReferencesResolved(g_super.m_context) ||
      !TaxiAttributeState_IsKnownReferenceRoster(g_super.m_context) ||
      taxiReferenceFingerprint == 0 ||
      RecoveredArenaSeance_TaxiAttributeCount() != taxiRosterSize ||
      RecoveredArenaSeance_TaxiAttributeCapacity() != taxiCapacity ||
      RecoveredArenaSeance_TaxiAttributeFingerprint() != taxiFingerprint ||
      RecoveredArenaSeance_TaxiReferenceFingerprint() !=
          taxiReferenceFingerprint ||
      !RecoveredGameServices_TaxiSubjectReady() ||
      TaxiSubjectState_Capacity() != taxiSubjectCapacity ||
      TaxiSubjectState_LiveCount() != taxiSubjectCount ||
      TaxiSubjectState_SoundCount() != taxiSubjectSoundCount ||
      TaxiSubjectState_Fingerprint(g_super.m_context) !=
          taxiSubjectFingerprint ||
      RecoveredArenaSeance_TaxiSubjectCapacity() != taxiSubjectCapacity ||
      RecoveredArenaSeance_TaxiSubjectCount() != taxiSubjectCount ||
      RecoveredArenaSeance_TaxiSubjectSoundCount() != taxiSubjectSoundCount ||
      RecoveredArenaSeance_TaxiSubjectFingerprint() !=
          taxiSubjectFingerprint ||
      !RecoveredGameServices_TaxiVehicleTransitionReady() ||
      RecoveredGameServices_TaxiVehicleProbeAvailableTaxis() !=
          taxiVehicleAvailableTaxis ||
      RecoveredGameServices_TaxiVehicleProbeInvalidTargets() !=
          taxiVehicleInvalidTargets ||
      RecoveredGameServices_TaxiVehicleProbeTransitions() !=
          taxiVehicleTransitions ||
      RecoveredGameServices_TaxiVehicleProbeAttributeTransfers() !=
          taxiVehicleAttributeTransfers ||
      RecoveredGameServices_TaxiVehicleProbePoseTransfers() !=
          taxiVehiclePoseTransfers ||
      RecoveredGameServices_TaxiVehicleProbePayloadTransfers() !=
          taxiVehiclePayloadTransfers ||
      RecoveredGameServices_TaxiVehicleProbeRemovedTaxis() !=
          taxiVehicleRemovedTaxis ||
      RecoveredGameServices_TaxiVehicleProbeRollbacks() !=
          taxiVehicleRollbacks ||
      !TaxiSubjectState_IsKnownRetailRoster(g_super.m_context) ||
      TaxiSubjectState_Capacity() != taxiSubjectCapacity ||
      TaxiSubjectState_LiveCount() != taxiSubjectCount ||
      TaxiSubjectState_SoundCount() != taxiSubjectSoundCount ||
      TaxiSubjectState_Fingerprint(g_super.m_context) !=
          taxiSubjectFingerprint ||
      taxiSubjectFingerprint == 0 ||
      RecoveredArenaSeance_TaxiProbeInvalidStarts() != 1 ||
      RecoveredArenaSeance_TaxiProbeValidStarts() != 1 ||
      RecoveredArenaSeance_TaxiProbeRenderReady() != 1 ||
      RecoveredArenaSeance_TaxiProbeSoundReady() != 1 ||
      RecoveredArenaSeance_TaxiProbeRollbacks() != 2 ||
      taxiVehicleInvalidTargets != 1 || taxiVehicleRollbacks != 1 ||
      taxiVehicleAvailableTaxis != (taxiSubjectCount == 0 ? 0 : 1) ||
      taxiVehicleTransitions != (taxiSubjectCount == 0 ? 0 : 1) ||
      taxiVehicleAttributeTransfers != (taxiSubjectCount == 0 ? 0 : 1) ||
      taxiVehiclePoseTransfers != (taxiSubjectCount == 0 ? 0 : 1) ||
      taxiVehiclePayloadTransfers != (taxiSubjectCount == 0 ? 0 : 1) ||
      taxiVehicleRemovedTaxis != (taxiSubjectCount == 0 ? 0 : 1) ||
      bulletFingerprint == 0 || bulletReferenceFingerprint == 0 ||
      bulletRosterSize < 3 || bulletRosterSize > 15 ||
      bulletCapacity != bulletRosterSize ||
      (bulletSubjectCapacity != 50 && bulletSubjectCapacity != 100 &&
       bulletSubjectCapacity != 250) ||
      !BulletAttributeState_SubjectTableReady(g_super.m_context) ||
      !BulletSubjectState_TableReady(g_super.m_context,
                                     bulletSubjectCapacity) ||
      BulletSubjectState_LiveCount() != 0 ||
      bulletSubjectFingerprint == 0 ||
      RecoveredArenaSeance_BulletSubjectFingerprint() !=
          bulletSubjectFingerprint ||
      RecoveredArenaSeance_BulletSubjectProbeMoveCount() != 2 ||
      RecoveredArenaSeance_BulletCollisionScheduledChecks() != 2 ||
      RecoveredArenaSeance_BulletCollisionExecutedChecks() != 1 ||
      RecoveredArenaSeance_BulletCollisionSphereCases() != 4 ||
      RecoveredArenaSeance_BulletCollisionEarliestHitCases() != 3 ||
      RecoveredArenaSeance_BulletCollisionWaterlineCases() != 4 ||
      RecoveredArenaSeance_BulletCollisionSceneQueries() != 1 ||
      RecoveredArenaSeance_BulletEffectQueuedBatches() != 2 ||
      RecoveredArenaSeance_BulletEffectQueuedChildren() != 3 ||
      RecoveredArenaSeance_BulletEffectSplashFirstCases() != 1 ||
      RecoveredArenaSeance_BulletEffectRolledBackChildren() != 3 ||
      RecoveredArenaSeance_BulletGroundSparkQueued() != 1 ||
      RecoveredArenaSeance_BulletGroundSparkRolledBack() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeThresholdStarts() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeFrameGateSkips() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeAttributeGateSkips() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeRollbacks() != 1 ||
      !BulletAttributeState_IsKnownRoster(g_super.m_context) ||
      !BulletAttributeState_ReferencesResolved(g_super.m_context) ||
      !BulletAttributeState_IsKnownReferenceRoster(g_super.m_context) ||
      RecoveredArenaSeance_BulletAttributeCount() != bulletRosterSize ||
      RecoveredArenaSeance_BulletAttributeCapacity() != bulletCapacity ||
      RecoveredArenaSeance_BulletAttributeFingerprint() !=
          bulletFingerprint ||
      RecoveredArenaSeance_BulletReferenceFingerprint() !=
          bulletReferenceFingerprint ||
      RecoveredArenaSeance_BulletSubjectCapacity() !=
          bulletSubjectCapacity ||
      vehicleAttributeFingerprint == 0 ||
      vehicleReferenceFingerprint == 0 ||
      vehicleAttributeRosterSize < 3 || vehicleAttributeRosterSize > 9 ||
      vehicleAttributeCapacity < vehicleAttributeRosterSize ||
      vehicleAttributeCapacity > 10 ||
      !VehicleAttributeState_IsKnownRoster(g_super.m_context) ||
      !VehicleAttributeState_ReferencesResolved(g_super.m_context) ||
      !VehicleAttributeState_IsKnownReferenceRoster(g_super.m_context) ||
      RecoveredArenaSeance_VehicleAttributeCount() !=
          vehicleAttributeRosterSize ||
      RecoveredArenaSeance_VehicleAttributeCapacity() !=
          vehicleAttributeCapacity ||
      RecoveredArenaSeance_VehicleAttributeFingerprint() !=
          vehicleAttributeFingerprint ||
      RecoveredArenaSeance_VehicleReferenceFingerprint() !=
          vehicleReferenceFingerprint ||
      !RecoveredGameServices_VehicleMovementReady() ||
      vehicleRuntimeFingerprint == 0 ||
      !VehicleRuntimeState_IsKnownRetailIdentity(
          g_super.m_context, vehicleID) ||
      (vehicleVesselKind != RECOVERED_VEHICLE_VESSEL_EMV &&
       vehicleVesselKind != RECOVERED_VEHICLE_VESSEL_WHEELS) ||
      vehicleProbeInvalidActivations != 2 ||
      vehicleProbeActivations != 1 ||
      vehicleProbeStationarySteps != 1 ||
      vehicleProbeThrottleEvents != 2 ||
      vehicleProbeMovementSteps != 172 ||
      vehicleProbeTurnEvents != 2 ||
      vehicleProbeCameraTransitions != 1 ||
      vehicleProbeRollbacks != 1 ||
      vehicleProbeHorizontalDistance <= 0.01 ||
      !IsVehicleControlActive(vehicleID, nullptr) ||
      smokerFingerprint == 0 ||
      smokerRosterSize != 12 || smokerCapacity != 12 ||
      smokerReferenceFingerprint == 0 || !smokerRuntimeReady ||
      smokeSubjectCapacity != 300 || smokeSubjectFingerprint == 0 ||
      smokeVisualResourceFingerprint == 0 ||
      SmokeSubjectState_LiveCount() != 0 ||
      dynSmokerCapacity != 62 || dynSmokerFingerprint == 0 ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      wavFingerprint == 0 || wavRosterSize < 22 || wavRosterSize > 33 ||
      wavCapacity < 30 || wavCapacity > 35 ||
      RecoveredArenaSeance_WavCatalogFingerprint() != wavFingerprint ||
      soundObjectCapacity != 250 || soundObjectFingerprint == 0 ||
      !SoundObjectState_DeviceFree() ||
      !RecoveredArenaSeance_SoundDistanceReady() ||
      RecoveredArenaSeance_SoundDistance() != 300.0 ||
      RecoveredArenaSeance_SoundDistanceSquared() != 90000.0 ||
      SoundObjectState_LiveCount() !=
          farterScriptObjectCount + taxiSubjectSoundCount +
              RecoveredArenaSeance_PeopleSubjectSoundCount() ||
      skinModelCount < 26 ||
      skinModelCount > 52 || skinSpriteCount != 1 ||
      skinCatalogFingerprint == 0 || skinResourceFingerprint == 0 ||
      sparkSubjectCapacity != 40 || sparkSubjectFingerprint == 0 ||
      sparkVisualResourceFingerprint == 0 ||
      farterFingerprint == 0 || farterRosterSize < 0 ||
      farterRosterSize > 4 || farterCapacity != 10 ||
      !RecoveredGameServices_FarterReferencesReady() ||
      !RecoveredGameServices_FarterRuntimeReady() ||
      farterReferenceFingerprint == 0 ||
      (farterSubjectCapacity != 0 && farterSubjectCapacity != 25) ||
      farterSubjectFingerprint == 0 ||
      (farterSubjectCapacity == 25 && farterScriptObjectCount == 0 &&
       farterSubjectFingerprint != 4111324552562250482ull) ||
      (farterSubjectCapacity == 0 &&
       farterSubjectFingerprint !=
           FarterSubjectState_AbsentFingerprint()) ||
      (farterScriptObjectCount != 0 && farterScriptObjectCount != 23) ||
      (farterScriptObjectCount == 23 &&
       (farterSubjectCapacity != 25 || farterRosterSize != 4 ||
        farterLiveObjectCount != 23 || farterSoundObjectCount != 23 ||
        farterNearFrameAudibleCount <= 0 ||
        farterFarFrameAudibleCount != 0 ||
        !farterAudibleFrameTransition)) ||
      (farterScriptObjectCount == 0 && farterRosterSize != 0) ||
      (farterScriptObjectCount == 0 &&
       (farterLiveObjectCount != 0 || farterSoundObjectCount != 0 ||
        farterNearFrameAudibleCount != 0 ||
        farterFarFrameAudibleCount != 0 ||
        farterAudibleFrameTransition)) ||
      FarterSubjectState_LiveCount() != farterScriptObjectCount ||
      lampFingerprint == 0 || lampRosterSize != 12 || lampCapacity != 12 ||
      corpseFingerprint == 0 || corpseRosterSize < 3 ||
      corpseRosterSize > 7 || corpseCapacity < corpseRosterSize ||
      corpseCapacity > 7 ||
      !RecoveredGameServices_CorpseReferencesReady() ||
      corpseReferenceFingerprint == 0 ||
       !corpseRuntimeReady || corpseSubjectCapacity != 100 ||
       corpseSubjectFingerprint == 0 ||
       CorpseSubjectState_LiveCount() != 0 ||
      RecoveredArenaSeance_PeopleAttributeCapacity() < 0 ||
      RecoveredArenaSeance_PeopleAttributeCount() < 0 ||
      RecoveredArenaSeance_PeopleAttributeCount() >
          RecoveredArenaSeance_PeopleAttributeCapacity() ||
      RecoveredArenaSeance_PeopleSubjectCapacity() < 0 ||
      RecoveredArenaSeance_PeopleSubjectCount() < 0 ||
      RecoveredArenaSeance_PeopleSubjectCount() >
          RecoveredArenaSeance_PeopleSubjectCapacity() ||
      RecoveredArenaSeance_PeopleSubjectSoundCount() < 0 ||
      RecoveredArenaSeance_PeopleSubjectSoundCount() >
          RecoveredArenaSeance_PeopleSubjectCount() ||
      RecoveredArenaSeance_PeopleAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_PeopleSubjectFingerprint() == 0 ||
      (RecoveredArenaSeance_PeopleSubjectCount() > 0 &&
       (RecoveredArenaSeance_PeopleProbeScheduledMoves() != 1 ||
        RecoveredArenaSeance_PeopleProbeBulletDamageApplications() != 1 ||
        RecoveredArenaSeance_PeopleProbeDeathTransitions() != 1 ||
        RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips() != 1 ||
        RecoveredArenaSeance_PeopleProbeRollbacks() != 1)) ||
      (RecoveredArenaSeance_PeopleSubjectCount() == 0 &&
       (RecoveredArenaSeance_PeopleProbeScheduledMoves() != 0 ||
        RecoveredArenaSeance_PeopleProbeBulletDamageApplications() != 0 ||
        RecoveredArenaSeance_PeopleProbeDeathTransitions() != 0 ||
        RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips() != 0 ||
        RecoveredArenaSeance_PeopleProbeRollbacks() != 0)) ||
      RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs() !=
          RecoveredArenaSeance_PeopleSubjectCount() ||
      RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents() < 0 ||
      RecoveredArenaSeance_PeopleActiveWorldRollbacks() != 1 ||
      RecoveredArenaSeance_PeopleActiveWorldFingerprint() == 0 ||
      RecoveredArenaSeance_CannonAttributeCapacity() < 0 ||
      RecoveredArenaSeance_CannonAttributeCount() < 0 ||
      RecoveredArenaSeance_CannonAttributeCount() >
          RecoveredArenaSeance_CannonAttributeCapacity() ||
      RecoveredArenaSeance_CannonSubjectCapacity() <= 0 ||
      RecoveredArenaSeance_CannonSubjectCount() != 0 ||
      RecoveredArenaSeance_CannonAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_CannonSubjectFingerprint() == 0 ||
      RecoveredArenaSeance_TankAttributeCapacity() < 0 ||
      RecoveredArenaSeance_TankAttributeCount() < 0 ||
      RecoveredArenaSeance_TankAttributeCount() >
          RecoveredArenaSeance_TankAttributeCapacity() ||
      RecoveredArenaSeance_TankSubjectCapacity() <= 0 ||
      RecoveredArenaSeance_TankSubjectCount() != 0 ||
      RecoveredArenaSeance_TankAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_TankSubjectFingerprint() == 0 ||
      (RecoveredArenaSeance_TankAttributeCount() > 0 &&
       (RecoveredArenaSeance_TankProbeAvailable() != 1 ||
        RecoveredArenaSeance_TankProbeValidStarts() != 1 ||
        RecoveredArenaSeance_TankProbeDynamicReady() != 1 ||
        RecoveredArenaSeance_TankProbeRenderReady() != 1 ||
        RecoveredArenaSeance_TankProbeCannonReady() != 1 ||
        RecoveredArenaSeance_TankProbeScheduledMoves() != 1 ||
        RecoveredArenaSeance_TankProbeBulletDamageApplications() != 1 ||
        RecoveredArenaSeance_TankProbeDeathTransitions() != 1 ||
        RecoveredArenaSeance_TankProbeDeathEffects() != 1 ||
        RecoveredArenaSeance_TankProbeSaveStateRoundTrips() != 1 ||
        RecoveredArenaSeance_TankProbeRollbacks() != 1)) ||
      (RecoveredArenaSeance_TankAttributeCount() == 0 &&
       (RecoveredArenaSeance_TankProbeAvailable() != 0 ||
        RecoveredArenaSeance_TankProbeValidStarts() != 0 ||
        RecoveredArenaSeance_TankProbeDynamicReady() != 0 ||
        RecoveredArenaSeance_TankProbeRenderReady() != 0 ||
        RecoveredArenaSeance_TankProbeCannonReady() != 0 ||
        RecoveredArenaSeance_TankProbeScheduledMoves() != 0 ||
        RecoveredArenaSeance_TankProbeBulletDamageApplications() != 0 ||
        RecoveredArenaSeance_TankProbeDeathTransitions() != 0 ||
        RecoveredArenaSeance_TankProbeDeathEffects() != 0 ||
        RecoveredArenaSeance_TankProbeSaveStateRoundTrips() != 0 ||
        RecoveredArenaSeance_TankProbeRollbacks() != 1)) ||
      !RecoveredArenaSeance_CommanderReady() ||
      RecoveredArenaSeance_CommanderCapacity() <= 0 ||
      RecoveredArenaSeance_CommanderCount() <= 0 ||
      RecoveredArenaSeance_CommanderCount() >
          RecoveredArenaSeance_CommanderCapacity() ||
      RecoveredArenaSeance_CommanderHostileLinks() < 0 ||
      RecoveredArenaSeance_CommanderFingerprint() == 0 ||
      !RecoveredArenaSeance_MissionTankLifecycleReady() ||
      RecoveredArenaSeance_TankGroupSubjectCapacity() <= 0 ||
      (RecoveredArenaSeance_MissionTankAvailable() != 0 &&
       RecoveredArenaSeance_MissionTankAvailable() != 1) ||
      (RecoveredArenaSeance_MissionTankAvailable() == 1 &&
       (RecoveredArenaSeance_MissionTankSpawns() != 1 ||
        RecoveredArenaSeance_MissionTankMembershipLinks() != 4 ||
        RecoveredArenaSeance_MissionTankFindEnemyCycles() != 1 ||
        RecoveredArenaSeance_MissionTankMovingCycles() != 1 ||
        RecoveredArenaSeance_MissionTankStableRoundTrips() != 3 ||
        RecoveredArenaSeance_MissionTankReconstructedIDs() != 1 ||
        RecoveredArenaSeance_MissionTankRollbacks() != 1 ||
        RecoveredArenaSeance_MissionTankFingerprint() == 0)) ||
      (RecoveredArenaSeance_MissionTankAvailable() == 0 &&
       (RecoveredArenaSeance_MissionTankSpawns() != 0 ||
        RecoveredArenaSeance_MissionTankMembershipLinks() != 0 ||
        RecoveredArenaSeance_MissionTankFindEnemyCycles() != 0 ||
        RecoveredArenaSeance_MissionTankMovingCycles() != 0 ||
        RecoveredArenaSeance_MissionTankStableRoundTrips() != 0 ||
        RecoveredArenaSeance_MissionTankReconstructedIDs() != 0 ||
        RecoveredArenaSeance_MissionTankRollbacks() != 1 ||
        RecoveredArenaSeance_MissionTankFingerprint() != 0))) {
    const int result =
        Fail("level-aware Arena subject/attribute roster is invalid");
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return result;
  }
  if (!ValidateReferenceTransaction(taxiReferenceFingerprint,
                                    bulletReferenceFingerprint,
                                    smokerReferenceFingerprint,
                                    farterReferenceFingerprint,
                                    corpseReferenceFingerprint)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Taxi/Bullet/Smoker/Farter/Corpse reference transaction "
                "was not atomic");
  }
  const SRecoveredObserverState observerBefore = *observer;
  SRecoveredVehicleRuntimeState vehicleBefore = {};
  if (!IsVehicleControlActive(vehicleID, &vehicleBefore) ||
      RecoveredGameServices_VehicleInputEvents() != 0 ||
      RecoveredGameServices_VehicleForwardedEvents() != 0 ||
      RecoveredGameServices_VehicleHousekeepingEvents() != 0 ||
      RecoveredGameServices_VehicleFrameCount() != 0 ||
      RecoveredGameServices_VehicleCameraFrameCount() != 0 ||
      !SendHardwareButton("W", TRUE)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Vehicle control handoff was not clean");
  }
  if (!WaitForSessionTimeAdvance(0.01)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("legacy Session timer did not expose the W press");
  }
  if (RecoveredGameServices_Issues() != 0 ||
      !RecoveredGameServices_RunFrame()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("bounded software press frame failed");
  }
  // Wait on the same timer that drives Session rather than assuming a wall
  // clock Sleep maps one-to-one onto a legacy timer tick.
  if (!WaitForSessionTimeAdvance(0.04)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("legacy Session timer did not advance while W was held");
  }
  if (!RecoveredGameServices_RunFrame() ||
      !SendHardwareButton("W", FALSE)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("bounded software held-input frame failed");
  }
  if (!WaitForSessionTimeAdvance(0.01)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("legacy Session timer did not expose the W release");
  }
  if (!RecoveredGameServices_RunFrame() || dwFrames != 3) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("bounded software frames failed");
  }
  observer = RecoveredGameServices_ObserverState();
  SRecoveredVehicleRuntimeState vehicleAfter = {};
  if (observer == nullptr ||
      !IsVehicleControlActive(vehicleID, &vehicleAfter)) {
    std::fprintf(stderr,
                 "vehicle-control diagnostics ready=%d fallback=%d "
                 "reason=%u input=%u forwarded=%u housekeeping=%u ignored=%u "
                 "frames=%u cameras=%u dropped=%u input_failure=%d\n",
                 RecoveredGameServices_VehicleControlReady() ? 1 : 0,
                 RecoveredGameServices_VehicleFallbackActive() ? 1 : 0,
                 RecoveredGameServices_VehicleFallbackReason(),
                 RecoveredGameServices_VehicleInputEvents(),
                 RecoveredGameServices_VehicleForwardedEvents(),
                 RecoveredGameServices_VehicleHousekeepingEvents(),
                 RecoveredGameServices_VehicleIgnoredEvents(),
                 RecoveredGameServices_VehicleFrameCount(),
                 RecoveredGameServices_VehicleCameraFrameCount(),
                 RecoveredGameServices_VehicleDroppedTimeFrameCount(),
                 RecoveredGameServices_VehicleLastInputFailure());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Vehicle control was lost after three frames");
  }
  const double vehicleDx = vehicleAfter.position.x - vehicleBefore.position.x;
  const double vehicleDz = vehicleAfter.position.z - vehicleBefore.position.z;
  if (RecoveredGameServices_VehicleInputEvents() != 4 ||
      RecoveredGameServices_VehicleForwardedEvents() != 2 ||
      RecoveredGameServices_VehicleHousekeepingEvents() != 2 ||
      RecoveredGameServices_VehicleIgnoredEvents() != 0 ||
      RecoveredGameServices_VehicleFrameCount() != 3 ||
      RecoveredGameServices_VehicleCameraFrameCount() != 3 ||
      vehicleAfter.controlEventCount - vehicleBefore.controlEventCount != 2 ||
      std::sqrt(vehicleDx * vehicleDx + vehicleDz * vehicleDz) <= 1.0e-6 ||
      observer->inputEvents != observerBefore.inputEvents ||
      std::fabs(observer->x - observerBefore.x) > 1.0e-9 ||
      std::fabs(observer->y - observerBefore.y) > 1.0e-9 ||
      std::fabs(observer->z - observerBefore.z) > 1.0e-9) {
    std::fprintf(stderr,
                 "vehicle-motion diagnostics input=%u forwarded=%u "
                 "housekeeping=%u ignored=%u frames=%u cameras=%u "
                 "control_delta=%u dx=%.12f dz=%.12f observer_events=%u/%u "
                 "observer_xyz=%.12f/%.12f/%.12f\n",
                 RecoveredGameServices_VehicleInputEvents(),
                 RecoveredGameServices_VehicleForwardedEvents(),
                 RecoveredGameServices_VehicleHousekeepingEvents(),
                 RecoveredGameServices_VehicleIgnoredEvents(),
                 RecoveredGameServices_VehicleFrameCount(),
                 RecoveredGameServices_VehicleCameraFrameCount(),
                 vehicleAfter.controlEventCount -
                     vehicleBefore.controlEventCount,
                 vehicleDx, vehicleDz,
                 observer->inputEvents, observerBefore.inputEvents,
                 observer->x - observerBefore.x,
                 observer->y - observerBefore.y,
                 observer->z - observerBefore.z);
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Hardware actions did not advance the live Vehicle camera");
  }

  if (!SendHardwareButton("Right", TRUE) ||
      !SendHardwareButton("W", TRUE)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Vehicle right-turn sequence failed");
  }
  for (int turnFrame = 0; turnFrame < 12; ++turnFrame) {
    if (!RunVehicleFrameAfter(0.025)) {
      ZAV_DeInitLevel();
      ZAV_Deinit();
      return Fail("live Vehicle right-turn frame failed");
    }
  }
  if (!SendHardwareButton("Right", FALSE) ||
      !RunVehicleFrameAfter(0.01)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Vehicle right-turn release failed");
  }
  SRecoveredVehicleRuntimeState vehicleTurned = {};
  if (!IsVehicleControlActive(vehicleID, &vehicleTurned) ||
      HorizontalHeadingDelta(vehicleAfter.direction,
                             vehicleTurned.direction) <= 1.0e-6) {
    std::fprintf(stderr,
                 "vehicle-turn diagnostics heading=%.12f "
                 "before0=%.9f/%.9f/%.9f before1=%.9f/%.9f/%.9f "
                 "before2=%.9f/%.9f/%.9f after0=%.9f/%.9f/%.9f "
                 "after1=%.9f/%.9f/%.9f after2=%.9f/%.9f/%.9f\n",
                 HorizontalHeadingDelta(vehicleAfter.direction,
                                        vehicleTurned.direction),
                 vehicleAfter.direction.Row(0).x,
                 vehicleAfter.direction.Row(0).y,
                 vehicleAfter.direction.Row(0).z,
                 vehicleAfter.direction.Row(1).x,
                 vehicleAfter.direction.Row(1).y,
                 vehicleAfter.direction.Row(1).z,
                 vehicleAfter.direction.Row(2).x,
                 vehicleAfter.direction.Row(2).y,
                 vehicleAfter.direction.Row(2).z,
                 vehicleTurned.direction.Row(0).x,
                 vehicleTurned.direction.Row(0).y,
                 vehicleTurned.direction.Row(0).z,
                 vehicleTurned.direction.Row(1).x,
                 vehicleTurned.direction.Row(1).y,
                 vehicleTurned.direction.Row(1).z,
                 vehicleTurned.direction.Row(2).x,
                 vehicleTurned.direction.Row(2).y,
                 vehicleTurned.direction.Row(2).z);
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("right arrow did not rotate the live Vehicle heading");
  }

  const double horizontalSpeedBeforeStop = HorizontalSpeed(vehicleTurned);
  SRecoveredVehicleRuntimeState vehicleStopCommand = {};
  const bool stopPressed = SendHardwareButton("X", TRUE);
  const bool stopInspected = stopPressed &&
      IsVehicleControlActive(vehicleID, &vehicleStopCommand);
  const bool stopThrottleReleased = stopInspected &&
      SendHardwareButton("W", FALSE);
  const bool stopPressFrame = stopThrottleReleased &&
      RunVehicleFrameAfter(0.01);
  const bool stopReleased = stopPressFrame && SendHardwareButton("X", FALSE);
  const bool stopReleaseFrame = stopReleased && RunVehicleFrameAfter(0.01);
  if (!stopReleaseFrame) {
    std::fprintf(stderr,
                 "vehicle-stop sequence pressed=%d inspected=%d "
                 "throttle_released=%d press_frame=%d released=%d "
                 "release_frame=%d "
                 "speed_before=%.12f speed_immediate=%.12f failure=%d\n",
                 stopPressed ? 1 : 0, stopInspected ? 1 : 0,
                 stopThrottleReleased ? 1 : 0,
                 stopPressFrame ? 1 : 0, stopReleased ? 1 : 0,
                 stopReleaseFrame ? 1 : 0,
                 horizontalSpeedBeforeStop,
                 HorizontalSpeed(vehicleStopCommand),
                 RecoveredGameServices_VehicleLastInputFailure());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Vehicle stop sequence failed");
  }
  SRecoveredVehicleRuntimeState vehicleStopped = {};
  if (!IsVehicleControlActive(vehicleID, &vehicleStopped) ||
      horizontalSpeedBeforeStop <= 1.0e-6) {
    std::fprintf(stderr,
                 "vehicle-stop diagnostics before=%.12f immediate=%.12f "
                 "after_world=%.12f\n",
                 horizontalSpeedBeforeStop,
                 HorizontalSpeed(vehicleStopCommand),
                 HorizontalSpeed(vehicleStopped));
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("X did not stop the live Vehicle");
  }

  if (!SendHardwareButton("W", TRUE) ||
      !RunVehicleFrameAfter(0.01) ||
      RecoveredGameServices_VehicleActiveActionCount() != 1 ||
      !RecoveredGameServices_SetApplicationActive(false) ||
      RecoveredGameServices_VehicleApplicationActive() ||
      RecoveredGameServices_VehicleFocusLossCount() != 1 ||
      RecoveredGameServices_VehicleSyntheticReleaseCount() != 1 ||
      RecoveredGameServices_VehicleActiveActionCount() != 0 ||
      !RunVehicleFrameAfter(0.01)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("focus loss did not release the held Vehicle throttle");
  }
  SRecoveredVehicleRuntimeState vehicleFocusReleased = {};
  if (!IsVehicleControlActive(vehicleID, &vehicleFocusReleased) ||
      !SendHardwareButton("W", TRUE) ||
      !SendHardwareButton("W", FALSE) ||
      !RunVehicleFrameAfter(0.01) ||
      RecoveredGameServices_VehicleSuppressedInputCount() != 2 ||
      RecoveredGameServices_VehicleActiveActionCount() != 0 ||
      !RecoveredGameServices_SetApplicationActive(true) ||
      !RecoveredGameServices_VehicleApplicationActive() ||
      RecoveredGameServices_VehicleFocusGainCount() != 1) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("inactive Vehicle input was not suppressed and rearmed");
  }

  if (!SendHardwareButton("W", TRUE) ||
      !RunVehicleFrameAfter(0.01) ||
      !RunVehicleFrameAfter(0.04) ||
      !SendHardwareButton("W", FALSE) ||
      !RunVehicleFrameAfter(0.01)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Vehicle throttle did not resume after focus recovery");
  }
  SRecoveredVehicleRuntimeState vehicleFocusResumed = {};
  if (!IsVehicleControlActive(vehicleID, &vehicleFocusResumed)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Vehicle control was lost after focus recovery");
  }
  const double resumedDx =
      vehicleFocusResumed.position.x - vehicleFocusReleased.position.x;
  const double resumedDz =
      vehicleFocusResumed.position.z - vehicleFocusReleased.position.z;
  if (RecoveredGameServices_VehicleInputEvents() != 26 ||
      RecoveredGameServices_VehicleForwardedEvents() != 11 ||
      RecoveredGameServices_VehicleHousekeepingEvents() != 13 ||
      RecoveredGameServices_VehicleIgnoredEvents() != 0 ||
      RecoveredGameServices_VehicleSuppressedInputCount() != 2 ||
      RecoveredGameServices_VehicleActiveActionCount() != 0 ||
      vehicleFocusResumed.controlEventCount -
              vehicleBefore.controlEventCount != 12 ||
      std::sqrt(resumedDx * resumedDx + resumedDz * resumedDz) <= 1.0e-6 ||
      dwFrames != 24 ||
      RecoveredGameServices_VehicleFrameCount() != 24 ||
      RecoveredGameServices_VehicleCameraFrameCount() != 24) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("focus-safe Vehicle input accounting or recovery failed");
  }

  // Capture after a normal completed frame, at a stable drawable and clock
  // boundary.  The following stall probe deliberately leaves a diagnostic
  // frame delta above the serializable clock ceiling, while the later effect
  // suite intentionally opens transient Explosion/Smoke frames.
  std::vector<std::uint8_t> levelContinuationBytes;
  SLevelContinuationSummary capturedContinuation;
  SRecoveredVehicleRuntimeState continuationVehicle = {};
  vehicleID = g_super.m_context->searchObject("Vehicle.Default");
  if (!VehicleRuntimeState_Inspect(
          g_super.m_context, vehicleID, &continuationVehicle) ||
      !RecoveredGameServices_CaptureLevelContinuation(
          &levelContinuationBytes, &capturedContinuation) ||
      !capturedContinuation.ready || !capturedContinuation.sealedJournal ||
      !capturedContinuation.boundaryMatches ||
      !capturedContinuation.worldMatches ||
      capturedContinuation.sections != 12 ||
      capturedContinuation.worldFingerprint == 0 ||
      capturedContinuation.journalFingerprint == 0 ||
      capturedContinuation.containerFingerprint == 0 ||
      levelContinuationBytes.empty()) {
    std::fprintf(stderr, "level continuation capture: %s\n",
                 RecoveredGameServices_LastLevelContinuationError());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Level continuation capture failed");
  }

  const unsigned int droppedFramesBeforeStall =
      RecoveredGameServices_VehicleDroppedTimeFrameCount();
  if (!WaitForSessionTimeAdvance(0.06) ||
      !RecoveredGameServices_RunFrame() ||
      !IsVehicleControlActive(vehicleID, nullptr) || dwFrames != 25 ||
      RecoveredGameServices_VehicleFrameCount() != 25 ||
      RecoveredGameServices_VehicleCameraFrameCount() != 25 ||
      RecoveredGameServices_VehicleDroppedTimeFrameCount() !=
          droppedFramesBeforeStall + 1 ||
      RecoveredGameServices_VehicleFallbackCount() != 0) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("long Vehicle frame was not capped without fallback");
  }
  if (!ExerciseVisibleSmoke()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible Smoke scene/draw/detach rollback failed");
  }
  if (!ExerciseVisibleBulletBarrelSmoke()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible Bullet barrel Smoke threshold/detach failed");
  }
  if (!ExerciseVisibleSmokerEmission()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible DynSmoker MOVE/emission/detach rollback failed");
  }
  if (!ExerciseVisibleSmokerLightCorona()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible DynSmoker light/corona/detach rollback failed");
  }
  if (!ExerciseVisibleExplosionParticles()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible Explosion particle/light/detach rollback failed");
  }
  if (!ExerciseVisibleExplosionTrace()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible Explosion traced-Piece/Smoke/detach rollback failed");
  }
  if (!ExerciseVisibleSpark()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("visible Spark sprite/light/detach rollback failed");
  }
  SRecoveredVehicleRuntimeState vehicleVisualState = {};
  const bool vehicleVisualStateInspected = VehicleRuntimeState_Inspect(
      g_super.m_context, vehicleID, &vehicleVisualState);
  const bool vehicleVisualControlActive =
      IsVehicleControlActive(vehicleID, nullptr);
  SRecoveredVehicleDriveTelemetry vehicleDriveTelemetry = {};
  const bool vehicleDriveTelemetryInspected =
      RecoveredGameServices_VehicleDriveTelemetry(&vehicleDriveTelemetry);
  SRecoveredVehicleControlJournalTelemetry controlJournalTelemetry = {};
  const bool controlJournalInspected =
      RecoveredGameServices_VehicleControlJournalTelemetry(
          &controlJournalTelemetry);
  if (!vehicleVisualControlActive || dwFrames != 42 ||
      RecoveredGameServices_VehicleInputEvents() != 26 ||
      RecoveredGameServices_VehicleForwardedEvents() != 11 ||
      RecoveredGameServices_VehicleHousekeepingEvents() != 13 ||
      RecoveredGameServices_VehicleIgnoredEvents() != 0 ||
      RecoveredGameServices_VehicleSuppressedInputCount() != 2 ||
      RecoveredGameServices_VehicleSyntheticReleaseCount() != 1 ||
      RecoveredGameServices_VehicleFocusLossCount() != 1 ||
      RecoveredGameServices_VehicleFocusGainCount() != 1 ||
      RecoveredGameServices_VehicleActiveActionCount() != 0 ||
      !RecoveredGameServices_VehicleApplicationActive() ||
      RecoveredGameServices_VehicleLastInputFailure() != 0 ||
      RecoveredGameServices_VehicleFrameCount() != 42 ||
      RecoveredGameServices_VehicleCameraFrameCount() != 42 ||
      RecoveredGameServices_VehicleDroppedTimeFrameCount() < 1 ||
      RecoveredGameServices_VehicleFallbackCount() != 0 ||
      RecoveredGameServices_VehicleFallbackReason() != 0 ||
      !controlJournalInspected ||
      controlJournalTelemetry.recordCount != 13 ||
      controlJournalTelemetry.actionRecords != 11 ||
      controlJournalTelemetry.focusRecords != 2 ||
      controlJournalTelemetry.lastRecordTick <
          controlJournalTelemetry.checkpointTick ||
      controlJournalTelemetry.encodedBytes == 0 ||
      controlJournalTelemetry.journalFingerprint == 0 ||
      controlJournalTelemetry.appendFailures != 0 ||
      controlJournalTelemetry.recording != 1 ||
      controlJournalTelemetry.applicationActive != 1 ||
      !vehicleDriveTelemetryInspected ||
      vehicleDriveTelemetry.maximumHorizontalDistance <= 1.0e-6 ||
      vehicleDriveTelemetry.maximumSpeedMagnitude <= 1.0e-6 ||
      vehicleDriveTelemetry.maximumHeadingDelta <= 1.0e-6 ||
      vehicleDriveTelemetry.lastBumpFlags < 0 ||
      vehicleDriveTelemetry.lastBumpFlags > 5 ||
      vehicleDriveTelemetry.groundContactFrames > 42 ||
      vehicleDriveTelemetry.staticCollisionFrames > 42 ||
      vehicleDriveTelemetry.landCollisionFrames > 42 ||
      vehicleDriveTelemetry.dynamicCollisionFrames > 42) {
    std::fprintf(stderr,
                 "vehicle-visual-suite diagnostics ready=%d fallback=%d "
                 "reason=%u input=%u forwarded=%u housekeeping=%u ignored=%u "
                 "input_failure=%d frames=%u cameras=%u dropped=%u "
                 "fallbacks=%u dwFrames=%lu inspected=%d active=%d "
                 "frame_begun=%d advances=%u controls=%u "
                 "focus=%u/%u release=%u suppressed=%u active_actions=%u "
                 "telemetry=%d distance=%.9f speed=%.9f heading=%.9f "
                 "ground=%u static=%u land=%u dynamic=%u\n",
                 RecoveredGameServices_VehicleControlReady() ? 1 : 0,
                 RecoveredGameServices_VehicleFallbackActive() ? 1 : 0,
                 RecoveredGameServices_VehicleFallbackReason(),
                 RecoveredGameServices_VehicleInputEvents(),
                 RecoveredGameServices_VehicleForwardedEvents(),
                 RecoveredGameServices_VehicleHousekeepingEvents(),
                 RecoveredGameServices_VehicleIgnoredEvents(),
                 RecoveredGameServices_VehicleLastInputFailure(),
                 RecoveredGameServices_VehicleFrameCount(),
                 RecoveredGameServices_VehicleCameraFrameCount(),
                 RecoveredGameServices_VehicleDroppedTimeFrameCount(),
                 RecoveredGameServices_VehicleFallbackCount(), dwFrames,
                 vehicleVisualStateInspected ? 1 : 0,
                 vehicleVisualState.active ? 1 : 0,
                 vehicleVisualState.frameBegun ? 1 : 0,
                 vehicleVisualState.advanceCount,
                 vehicleVisualState.controlEventCount,
                 RecoveredGameServices_VehicleFocusLossCount(),
                 RecoveredGameServices_VehicleFocusGainCount(),
                 RecoveredGameServices_VehicleSyntheticReleaseCount(),
                 RecoveredGameServices_VehicleSuppressedInputCount(),
                 RecoveredGameServices_VehicleActiveActionCount(),
                 vehicleDriveTelemetryInspected ? 1 : 0,
                 vehicleDriveTelemetry.maximumHorizontalDistance,
                 vehicleDriveTelemetry.maximumSpeedMagnitude,
                 vehicleDriveTelemetry.maximumHeadingDelta,
                 vehicleDriveTelemetry.groundContactFrames,
                 vehicleDriveTelemetry.staticCollisionFrames,
                 vehicleDriveTelemetry.landCollisionFrames,
                 vehicleDriveTelemetry.dynamicCollisionFrames);
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live Vehicle control did not survive the visual frame suite");
  }

  if (!ExerciseInteractiveTaxiHandoff()) {
    SRecoveredTaxiVehicleHandoffTelemetry handoff = {};
    const bool inspected =
        RecoveredGameServices_TaxiVehicleHandoffTelemetry(&handoff);
    std::fprintf(stderr,
                 "taxi-handoff diagnostics inspected=%d nearest=%.9f "
                 "radius=%.9f available=%u nearby=%u attempts=%u "
                 "pending=%u success=%u no_target=%u removed=%u "
                 "panel_ready=%d panel_open=%d panel_transitions=%u "
                 "panel_draws=%u subscription=%d post_frames=%u "
                 "post_distance=%.9f input=%u forwarded=%u "
                 "housekeeping=%u ignored=%u\n",
                 inspected ? 1 : 0, handoff.nearestTaxiDistance,
                 handoff.activationDistance, handoff.availableTaxis,
                 handoff.nearbyTaxis, handoff.attempts,
                 handoff.pendingTransitions, handoff.successfulTransitions,
                 handoff.noTargetAttempts, handoff.removedTaxis,
                 handoff.panelReady, handoff.panelOpen,
                 handoff.panelOpenTransitions, handoff.panelDraws,
                 handoff.hardwareSubscriptionPreserved,
                 handoff.postTransitionFrames,
                 handoff.postTransitionDistance,
                 RecoveredGameServices_VehicleInputEvents(),
                 RecoveredGameServices_VehicleForwardedEvents(),
                 RecoveredGameServices_VehicleHousekeepingEvents(),
                 RecoveredGameServices_VehicleIgnoredEvents());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("F1 Taxi handoff/panel/post-transition drive failed");
  }

  if (!ExerciseSafeVehicleExitAndReentry()) {
    SRecoveredVehicleEmbodimentTelemetry embodiment = {};
    const bool inspected =
        RecoveredGameServices_VehicleEmbodimentTelemetry(&embodiment);
    std::fprintf(stderr,
                 "safe embodiment inspected=%d exit=%u safe=%u unsafe=%u "
                 "taxi=%u orphan=%u reentry=%u/%u panel=%u/%u "
                 "pending=%d live=%u subscription=%d "
                 "control=%d/%d/%u/%d/%d/%d "
                 "timer=%u/%.6f\n",
                 inspected ? 1 : 0, embodiment.exitAttempts,
                 embodiment.safeExitCompletions,
                 embodiment.unsafeExitCompletions,
                 embodiment.droppedTaxis, embodiment.droppedOrphans,
                 embodiment.reentryAttempts, embodiment.reentryCompletions,
                 embodiment.panelCloseTransitions,
                 embodiment.panelReopenTransitions, embodiment.exitPending,
                 embodiment.liveOrphans,
                 embodiment.hardwareSubscriptionPreserved,
                 RecoveredGameServices_VehicleControlReady() ? 1 : 0,
                 RecoveredGameServices_VehicleFallbackActive() ? 1 : 0,
                 RecoveredGameServices_VehicleFallbackReason(),
                 RecoveredGameServices_VehicleLastInputFailure(),
                 VehicleRuntimeState_LastFrameFailure(),
                 VehicleRuntimeState_LastFrameReadinessIssue(),
                 SUA_ClampedTimerSampleCount(), SUA_ClampedTimerSeconds());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("safe F1 Vehicle exit/Taxi re-entry failed");
  }

  if (!ExerciseVehiclePrimaryFire()) {
    SRecoveredVehiclePrimaryFireTelemetry fire = {};
    const bool inspected =
        RecoveredGameServices_VehiclePrimaryFireTelemetry(&fire);
    std::fprintf(stderr,
                 "vehicle-fire diagnostics inspected=%d trigger=%u "
                 "shots=%u rollback=%u moves=%u checks=%u scene=%u "
                 "dynamic=%u water=%u children=%u ground=%u barrel=%u "
                 "live=%u peak=%u explosions=%u particles=%u smokes=%u "
                 "sparks=%u sounds=%u render=%u effect_render=%u "
                 "subscription=%d input=%u forwarded=%u housekeeping=%u "
                 "focus=%u/%u releases=%u suppressed=%u active=%u "
                 "ignored=%u\n",
                 inspected ? 1 : 0, fire.triggerPresses,
                 fire.acceptedShots, fire.rolledBackShots, fire.moveEvents,
                 fire.collisionChecks, fire.sceneImpacts,
                 fire.dynamicImpacts, fire.waterlineSplashes,
                 fire.impactEffectChildren, fire.groundRemovals,
                 fire.barrelSmokeStarts, fire.liveBullets,
                 fire.tablePeakLiveBullets,
                 fire.maximumExplosionSubjects,
                 fire.maximumParticleBranches, fire.maximumSmokeSubjects,
                 fire.maximumSparkSubjects, fire.maximumSoundObjects,
                 fire.renderedFramesAfterShot, fire.effectRenderFrames,
                 fire.hardwareSubscriptionPreserved,
                 RecoveredGameServices_VehicleInputEvents(),
                 RecoveredGameServices_VehicleForwardedEvents(),
                 RecoveredGameServices_VehicleHousekeepingEvents(),
                 RecoveredGameServices_VehicleFocusLossCount(),
                 RecoveredGameServices_VehicleFocusGainCount(),
                 RecoveredGameServices_VehicleSyntheticReleaseCount(),
                 RecoveredGameServices_VehicleSuppressedInputCount(),
                 RecoveredGameServices_VehicleActiveActionCount(),
                 RecoveredGameServices_VehicleIgnoredEvents());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("MouseL Vehicle primary-fire/effect chain failed");
  }

  if (!ExerciseUnsafeVehicleExitAndOrphanImpact()) {
    SRecoveredVehicleEmbodimentTelemetry embodiment = {};
    const bool inspected =
        RecoveredGameServices_VehicleEmbodimentTelemetry(&embodiment);
    std::fprintf(stderr,
                 "unsafe embodiment inspected=%d exit=%u safe=%u unsafe=%u "
                 "taxi=%u orphan=%u moves=%u impacts=%u explosions=%u "
                 "smoke=%u renders=%u pending=%d live=%u subscription=%d\n",
                 inspected ? 1 : 0, embodiment.exitAttempts,
                 embodiment.safeExitCompletions,
                 embodiment.unsafeExitCompletions,
                 embodiment.droppedTaxis, embodiment.droppedOrphans,
                 embodiment.orphanMoveEvents, embodiment.orphanImpacts,
                 embodiment.orphanExplosions,
                 embodiment.orphanSmokeStarts,
                 embodiment.orphanRenderFrames, embodiment.exitPending,
                 embodiment.liveOrphans,
                 embodiment.hardwareSubscriptionPreserved);
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("unsafe F1 Vehicle exit/Orphan impact failed");
  }

  KR_Event unsupported;
  unsupported.label = lev_SAVE;
  if (g_super.m_level.receiveEvent(unsupported) != 0 ||
      RecoveredGameServices_Issues() !=
          RECOVERED_GAME_SERVICES_UNSUPPORTED_LEVEL_EVENT) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("unsupported Level event was not diagnosed");
  }

  ZAV_DeInitLevel();
  ZAV_DeInitLevel();
  if (!IsServiceReleased() || !IsLevelRolledBack() ||
      !RecoveredSoftwareGraph_IsReady() ||
      snd_distMax != initialSoundDistance ||
      snd_distMax2 != initialSoundDistanceSquared) {
    ZAV_Deinit();
    return Fail("public service shutdown was not idempotent");
  }

  if (!StartServices(argv[1]) || RecoveredGameServices_Issues() != 0 ||
      RecoveredArenaSeance_ExtendedIssues() != 0 ||
      ExplosionAttributeState_Fingerprint(g_super.m_context) !=
          explosionFingerprint ||
      ExplosionAttributeState_ParticleVisualFingerprint(
          g_super.m_context) != explosionParticleVisualFingerprint ||
      ExplosionAttributeState_SmokeVisualFingerprint(
          g_super.m_context) != explosionSmokeVisualFingerprint ||
      !RecoveredGameServices_ExplosionSmokeReady() ||
      RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() <= 0 ||
      RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps() <= 0 ||
      RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents() != 1 ||
      RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites() !=
          RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() ||
      ExplosionAttributeState_PieceReferenceFingerprint(
          g_super.m_context) != explosionPieceReferenceFingerprint ||
      !RecoveredGameServices_ExplosionPieceReady() ||
      RecoveredArenaSeance_ExplosionPieceReferenceFingerprint() !=
          explosionPieceReferenceFingerprint ||
      RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() <= 0 ||
      RecoveredArenaSeance_ExplosionPieceProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionPieceProbeMoveSteps() <= 0 ||
      RecoveredArenaSeance_ExplosionPieceProbeExpiredParents() != 1 ||
      RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces() !=
          RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() ||
      ExplosionAttributeState_TraceReferenceFingerprint(
          g_super.m_context) != explosionTraceReferenceFingerprint ||
      !RecoveredGameServices_ExplosionTraceReady() ||
      RecoveredArenaSeance_ExplosionTraceReferenceFingerprint() !=
          explosionTraceReferenceFingerprint ||
      RecoveredArenaSeance_ExplosionTraceProbeStartedPieces() <= 0 ||
      RecoveredArenaSeance_ExplosionTraceProbeQuotaGateSkips() != 1 ||
      RecoveredArenaSeance_ExplosionTraceProbePuffEvents() != 1 ||
      RecoveredArenaSeance_ExplosionTraceProbeSmokeChildren() <= 0 ||
      RecoveredArenaSeance_ExplosionTraceProbeMoveSteps() <= 0 ||
      RecoveredArenaSeance_ExplosionTraceProbeExpiredParents() != 1 ||
      RecoveredArenaSeance_ExplosionTraceProbeRolledBackPieces() !=
          RecoveredArenaSeance_ExplosionTraceProbeStartedPieces() ||
      ExplosionSubjectState_TracedParentCount() != 0 ||
      ExplosionSubjectState_Capacity() != explosionSubjectCapacity ||
      ExplosionSubjectState_LiveCount() != 0 ||
      ExplosionSubjectState_Fingerprint(g_super.m_context) !=
          explosionSubjectFingerprint ||
      RecoveredArenaSeance_ExplosionSubjectCapacity() !=
          explosionSubjectCapacity ||
      RecoveredArenaSeance_ExplosionSubjectFingerprint() !=
          explosionSubjectFingerprint ||
      TaxiAttributeState_Fingerprint(g_super.m_context) != taxiFingerprint ||
      TaxiAttributeState_RosterSize(g_super.m_context) != taxiRosterSize ||
      TaxiAttributeState_Capacity() != taxiCapacity ||
      !TaxiAttributeState_ReferencesResolved(g_super.m_context) ||
      TaxiAttributeState_ReferenceFingerprint(g_super.m_context) !=
          taxiReferenceFingerprint ||
      RecoveredArenaSeance_TaxiAttributeCount() != taxiRosterSize ||
      RecoveredArenaSeance_TaxiAttributeCapacity() != taxiCapacity ||
      RecoveredArenaSeance_TaxiAttributeFingerprint() != taxiFingerprint ||
      RecoveredArenaSeance_TaxiReferenceFingerprint() !=
          taxiReferenceFingerprint ||
      !RecoveredGameServices_TaxiSubjectReady() ||
      TaxiSubjectState_Capacity() != taxiSubjectCapacity ||
      TaxiSubjectState_LiveCount() != taxiSubjectCount ||
      TaxiSubjectState_SoundCount() != taxiSubjectSoundCount ||
      TaxiSubjectState_Fingerprint(g_super.m_context) !=
          taxiSubjectFingerprint ||
      RecoveredArenaSeance_TaxiSubjectCapacity() != taxiSubjectCapacity ||
      RecoveredArenaSeance_TaxiSubjectCount() != taxiSubjectCount ||
      RecoveredArenaSeance_TaxiSubjectSoundCount() != taxiSubjectSoundCount ||
      RecoveredArenaSeance_TaxiSubjectFingerprint() !=
          taxiSubjectFingerprint ||
      !RecoveredGameServices_TaxiVehicleTransitionReady() ||
      RecoveredGameServices_TaxiVehicleProbeAvailableTaxis() !=
          taxiVehicleAvailableTaxis ||
      RecoveredGameServices_TaxiVehicleProbeInvalidTargets() !=
          taxiVehicleInvalidTargets ||
      RecoveredGameServices_TaxiVehicleProbeTransitions() !=
          taxiVehicleTransitions ||
      RecoveredGameServices_TaxiVehicleProbeAttributeTransfers() !=
          taxiVehicleAttributeTransfers ||
      RecoveredGameServices_TaxiVehicleProbePoseTransfers() !=
          taxiVehiclePoseTransfers ||
      RecoveredGameServices_TaxiVehicleProbePayloadTransfers() !=
          taxiVehiclePayloadTransfers ||
      RecoveredGameServices_TaxiVehicleProbeRemovedTaxis() !=
          taxiVehicleRemovedTaxis ||
      RecoveredGameServices_TaxiVehicleProbeRollbacks() !=
          taxiVehicleRollbacks ||
      BulletAttributeState_Fingerprint(g_super.m_context) !=
          bulletFingerprint ||
      BulletAttributeState_RosterSize(g_super.m_context) !=
          bulletRosterSize ||
      BulletAttributeState_Capacity() != bulletCapacity ||
      BulletAttributeState_SubjectCapacity() != bulletSubjectCapacity ||
      !BulletAttributeState_ReferencesResolved(g_super.m_context) ||
      BulletAttributeState_ReferenceFingerprint(g_super.m_context) !=
          bulletReferenceFingerprint ||
      RecoveredArenaSeance_BulletAttributeFingerprint() !=
          bulletFingerprint ||
      RecoveredArenaSeance_BulletReferenceFingerprint() !=
          bulletReferenceFingerprint ||
      RecoveredArenaSeance_BulletSubjectCapacity() !=
          bulletSubjectCapacity ||
      BulletSubjectState_LiveCount() != 0 ||
      BulletSubjectState_Fingerprint(g_super.m_context) !=
          bulletSubjectFingerprint ||
      RecoveredArenaSeance_BulletSubjectFingerprint() !=
          bulletSubjectFingerprint ||
      RecoveredArenaSeance_BulletSubjectProbeMoveCount() != 2 ||
      RecoveredArenaSeance_BulletCollisionScheduledChecks() != 2 ||
      RecoveredArenaSeance_BulletCollisionExecutedChecks() != 1 ||
      RecoveredArenaSeance_BulletCollisionSphereCases() != 4 ||
      RecoveredArenaSeance_BulletCollisionEarliestHitCases() != 3 ||
      RecoveredArenaSeance_BulletCollisionWaterlineCases() != 4 ||
      RecoveredArenaSeance_BulletCollisionSceneQueries() != 1 ||
      RecoveredArenaSeance_BulletEffectQueuedBatches() != 2 ||
      RecoveredArenaSeance_BulletEffectQueuedChildren() != 3 ||
      RecoveredArenaSeance_BulletEffectSplashFirstCases() != 1 ||
      RecoveredArenaSeance_BulletEffectRolledBackChildren() != 3 ||
      RecoveredArenaSeance_BulletGroundSparkQueued() != 1 ||
      RecoveredArenaSeance_BulletGroundSparkRolledBack() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeThresholdStarts() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeFrameGateSkips() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeAttributeGateSkips() != 1 ||
      RecoveredArenaSeance_BulletBarrelSmokeRollbacks() != 1 ||
      VehicleAttributeState_Fingerprint(g_super.m_context) !=
          vehicleAttributeFingerprint ||
      VehicleAttributeState_RosterSize(g_super.m_context) !=
          vehicleAttributeRosterSize ||
      VehicleAttributeState_Capacity() != vehicleAttributeCapacity ||
      !VehicleAttributeState_ReferencesResolved(g_super.m_context) ||
      RecoveredArenaSeance_VehicleAttributeFingerprint() !=
          vehicleAttributeFingerprint ||
      RecoveredArenaSeance_VehicleReferenceFingerprint() !=
          vehicleReferenceFingerprint ||
      !RecoveredGameServices_VehicleMovementReady() ||
      RecoveredGameServices_VehicleRuntimeFingerprint() !=
          vehicleRuntimeFingerprint ||
      RecoveredGameServices_VehicleVesselKind() != vehicleVesselKind ||
      RecoveredGameServices_VehicleProbeInvalidActivations() !=
          vehicleProbeInvalidActivations ||
      RecoveredGameServices_VehicleProbeActivations() !=
          vehicleProbeActivations ||
      RecoveredGameServices_VehicleProbeStationarySteps() !=
          vehicleProbeStationarySteps ||
      RecoveredGameServices_VehicleProbeThrottleEvents() !=
          vehicleProbeThrottleEvents ||
      RecoveredGameServices_VehicleProbeMovementSteps() !=
          vehicleProbeMovementSteps ||
      RecoveredGameServices_VehicleProbeTurnEvents() !=
          vehicleProbeTurnEvents ||
      RecoveredGameServices_VehicleProbeCameraTransitions() !=
          vehicleProbeCameraTransitions ||
      RecoveredGameServices_VehicleProbeRollbacks() !=
          vehicleProbeRollbacks ||
      std::fabs(RecoveredGameServices_VehicleProbeHorizontalDistance() -
                vehicleProbeHorizontalDistance) > 1.0e-9 ||
      !IsVehicleControlActive(vehicleID, nullptr) ||
      SmokerAttributeState_Fingerprint(g_super.m_context) !=
          smokerFingerprint ||
      SmokerAttributeState_RosterSize(g_super.m_context) !=
          smokerRosterSize ||
      SmokerAttributeState_Capacity() != smokerCapacity ||
      SmokerAttributeState_ReferenceFingerprint(g_super.m_context) !=
          smokerReferenceFingerprint ||
      RecoveredArenaSeance_SmokerRuntimeReady() != smokerRuntimeReady ||
      RecoveredArenaSeance_SmokeSubjectCapacity() != smokeSubjectCapacity ||
      RecoveredArenaSeance_SmokeSubjectFingerprint() !=
          smokeSubjectFingerprint ||
      RecoveredArenaSeance_SmokeVisualResourceFingerprint() !=
          smokeVisualResourceFingerprint ||
      SmokeSubjectState_LiveCount() != 0 ||
      RecoveredArenaSeance_DynSmokerCapacity() != dynSmokerCapacity ||
      RecoveredArenaSeance_DynSmokerFingerprint() !=
          dynSmokerFingerprint ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      WAVResourceState_Fingerprint(g_super.m_context) != wavFingerprint ||
      WAVResourceState_RosterSize(g_super.m_context) != wavRosterSize ||
      WAVResourceState_Capacity() != wavCapacity ||
      RecoveredArenaSeance_SoundObjectCapacity() != soundObjectCapacity ||
      RecoveredArenaSeance_SoundObjectFingerprint() !=
          soundObjectFingerprint ||
      SoundObjectState_LiveCount() !=
          farterScriptObjectCount + taxiSubjectSoundCount +
              RecoveredArenaSeance_PeopleSubjectSoundCount() ||
      FarterAttributeState_Fingerprint(g_super.m_context) !=
          farterFingerprint ||
      FarterAttributeState_RosterSize(g_super.m_context) !=
          farterRosterSize ||
      FarterAttributeState_Capacity() != farterCapacity ||
      FarterAttributeState_ReferenceFingerprint(g_super.m_context) !=
          farterReferenceFingerprint ||
      RecoveredArenaSeance_FarterRuntimeReady() != farterRuntimeReady ||
      !RecoveredGameServices_FarterRuntimeReady() ||
      RecoveredArenaSeance_FarterSubjectCapacity() !=
          farterSubjectCapacity ||
      RecoveredArenaSeance_FarterSubjectFingerprint() !=
          farterSubjectFingerprint ||
      RecoveredArenaSeance_FarterScriptObjectCount() !=
          farterScriptObjectCount ||
      RecoveredArenaSeance_FarterLiveObjectCount() !=
          farterLiveObjectCount ||
      RecoveredArenaSeance_FarterSoundObjectCount() !=
          farterSoundObjectCount ||
      RecoveredArenaSeance_FarterNearFrameAudibleCount() !=
          farterNearFrameAudibleCount ||
      RecoveredArenaSeance_FarterFarFrameAudibleCount() !=
          farterFarFrameAudibleCount ||
      RecoveredArenaSeance_FarterAudibleFrameTransition() !=
          farterAudibleFrameTransition ||
      RecoveredArenaSeance_SoundDistance() != soundDistance ||
      RecoveredArenaSeance_SoundDistanceSquared() !=
          soundDistanceSquared ||
      FarterSubjectState_LiveCount() != farterScriptObjectCount ||
      LampAttributeState_Fingerprint(g_super.m_context) != lampFingerprint ||
      LampAttributeState_RosterSize(g_super.m_context) != lampRosterSize ||
      LampAttributeState_Capacity() != lampCapacity ||
      CorpseAttributeState_Fingerprint(g_super.m_context) !=
          corpseFingerprint ||
      CorpseAttributeState_RosterSize(g_super.m_context) !=
          corpseRosterSize ||
      CorpseAttributeState_Capacity() != corpseCapacity ||
      CorpseAttributeState_ReferenceFingerprint(g_super.m_context) !=
          corpseReferenceFingerprint ||
      RecoveredArenaSeance_CorpseRuntimeReady() != corpseRuntimeReady ||
      RecoveredArenaSeance_CorpseSubjectCapacity() !=
          corpseSubjectCapacity ||
      RecoveredArenaSeance_CorpseSubjectFingerprint() !=
          corpseSubjectFingerprint ||
      CorpseSubjectState_LiveCount() != 0 ||
      RecoveredArenaSeance_PeopleAttributeCount() != peopleAttributeCount ||
      RecoveredArenaSeance_PeopleAttributeCapacity() !=
          peopleAttributeCapacity ||
      RecoveredArenaSeance_PeopleSubjectCount() != peopleSubjectCount ||
      RecoveredArenaSeance_PeopleSubjectCapacity() !=
          peopleSubjectCapacity ||
      RecoveredArenaSeance_PeopleSubjectSoundCount() !=
          peopleSubjectSoundCount ||
      RecoveredArenaSeance_PeopleAttributeFingerprint() !=
          peopleAttributeFingerprint ||
      RecoveredArenaSeance_PeopleSubjectFingerprint() !=
          peopleSubjectFingerprint ||
      RecoveredArenaSeance_PeopleProbeScheduledMoves() !=
          peopleProbeScheduledMoves ||
      RecoveredArenaSeance_PeopleProbeBulletDamageApplications() !=
          peopleProbeBulletDamage ||
      RecoveredArenaSeance_PeopleProbeDeathTransitions() !=
          peopleProbeDeathTransitions ||
      RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips() !=
          peopleProbeSaveRoundTrips ||
      RecoveredArenaSeance_PeopleProbeRollbacks() !=
          peopleProbeRollbacks ||
      RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs() !=
          peopleActiveWorldReconstructedIDs ||
      RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents() !=
          peopleActiveWorldSchedulerEvents ||
      RecoveredArenaSeance_PeopleActiveWorldRollbacks() !=
          peopleActiveWorldRollbacks ||
      RecoveredArenaSeance_PeopleActiveWorldFingerprint() !=
          peopleActiveWorldFingerprint ||
      RecoveredArenaSeance_TankAttributeCount() != tankAttributeCount ||
      RecoveredArenaSeance_TankAttributeCapacity() !=
          tankAttributeCapacity ||
      RecoveredArenaSeance_TankSubjectCount() != tankSubjectCount ||
      RecoveredArenaSeance_TankSubjectCapacity() != tankSubjectCapacity ||
      RecoveredArenaSeance_TankAttributeFingerprint() !=
          tankAttributeFingerprint ||
      RecoveredArenaSeance_TankSubjectFingerprint() !=
          tankSubjectFingerprint ||
      RecoveredArenaSeance_TankProbeAvailable() != tankProbeAvailable ||
      RecoveredArenaSeance_TankProbeValidStarts() != tankProbeValidStarts ||
      RecoveredArenaSeance_TankProbeDynamicReady() != tankProbeDynamicReady ||
      RecoveredArenaSeance_TankProbeRenderReady() != tankProbeRenderReady ||
      RecoveredArenaSeance_TankProbeCannonReady() != tankProbeCannonReady ||
      RecoveredArenaSeance_TankProbeScheduledMoves() !=
          tankProbeScheduledMoves ||
      RecoveredArenaSeance_TankProbeBulletDamageApplications() !=
          tankProbeBulletDamage ||
      RecoveredArenaSeance_TankProbeDeathTransitions() !=
          tankProbeDeathTransitions ||
      RecoveredArenaSeance_TankProbeDeathEffects() !=
          tankProbeDeathEffects ||
      RecoveredArenaSeance_TankProbeSaveStateRoundTrips() !=
          tankProbeSaveRoundTrips ||
      RecoveredArenaSeance_TankProbeRollbacks() != tankProbeRollbacks ||
      RecoveredArenaSeance_CommanderCapacity() != commanderCapacity ||
      RecoveredArenaSeance_CommanderCount() != commanderCount ||
      RecoveredArenaSeance_CommanderHostileLinks() !=
          commanderHostileLinks ||
      RecoveredArenaSeance_CommanderFingerprint() !=
          commanderFingerprint ||
      RecoveredArenaSeance_TankGroupSubjectCapacity() !=
          tankGroupSubjectCapacity ||
      RecoveredArenaSeance_MissionTankAvailable() !=
          missionTankAvailable ||
      RecoveredArenaSeance_MissionTankSpawns() != missionTankSpawns ||
      RecoveredArenaSeance_MissionTankMembershipLinks() !=
          missionTankMembershipLinks ||
      RecoveredArenaSeance_MissionTankFindEnemyCycles() !=
          missionTankFindEnemyCycles ||
      RecoveredArenaSeance_MissionTankMovingCycles() !=
          missionTankMovingCycles ||
      RecoveredArenaSeance_MissionTankStableRoundTrips() !=
          missionTankStableRoundTrips ||
      RecoveredArenaSeance_MissionTankReconstructedIDs() !=
          missionTankReconstructedIDs ||
      RecoveredArenaSeance_MissionTankRollbacks() != missionTankRollbacks ||
      RecoveredArenaSeance_MissionTankFingerprint() !=
          missionTankFingerprint ||
      RecoveredArenaSeance_CannonAttributeCount() !=
          cannonAttributeCount ||
      RecoveredArenaSeance_CannonAttributeCapacity() !=
          cannonAttributeCapacity ||
      RecoveredArenaSeance_CannonSubjectCount() != cannonSubjectCount ||
      RecoveredArenaSeance_CannonSubjectCapacity() !=
          cannonSubjectCapacity ||
      RecoveredArenaSeance_CannonAttributeFingerprint() !=
          cannonAttributeFingerprint ||
      RecoveredArenaSeance_CannonSubjectFingerprint() !=
          cannonSubjectFingerprint ||
      RecoveredArenaSeance_SkinCatalogFingerprint() !=
          skinCatalogFingerprint ||
      RecoveredArenaSeance_SkinResourceFingerprint() !=
          skinResourceFingerprint ||
      RecoveredArenaSeance_SparkSubjectCapacity() !=
          sparkSubjectCapacity ||
      RecoveredArenaSeance_SparkSubjectFingerprint() !=
          sparkSubjectFingerprint ||
      RecoveredArenaSeance_SparkVisualResourceFingerprint() !=
          sparkVisualResourceFingerprint ||
      RecoveredArenaSeance_SparkProbeInvalidStarts() != 2 ||
      RecoveredArenaSeance_SparkProbeQueuedCreates() != 1 ||
      RecoveredArenaSeance_SparkProbeQueueRollbacks() != 1 ||
      RecoveredArenaSeance_SparkProbePhaseTransitions() != 5 ||
      RecoveredArenaSeance_SparkProbeExpirations() != 1 ||
      !RecoveredGameServices_RunFrame() || dwFrames != 1) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("service reconstruction failed");
  }
  SLevelContinuationSummary restoredContinuation;
  if (!RecoveredGameServices_RestoreLevelContinuation(
          levelContinuationBytes, &restoredContinuation) ||
      !restoredContinuation.ready ||
      !restoredContinuation.sealedJournal ||
      !restoredContinuation.boundaryMatches ||
      !restoredContinuation.worldMatches ||
      restoredContinuation.sections != 12 ||
      restoredContinuation.ownerPhases != 12 ||
      restoredContinuation.referencePhases != 12 ||
      restoredContinuation.eventPhases != restoredContinuation.events ||
      restoredContinuation.worldFingerprint !=
          capturedContinuation.worldFingerprint ||
      restoredContinuation.restoredWorldFingerprint !=
          capturedContinuation.worldFingerprint ||
      restoredContinuation.journalFingerprint !=
          capturedContinuation.journalFingerprint ||
      restoredContinuation.containerFingerprint !=
          capturedContinuation.containerFingerprint) {
    std::fprintf(stderr,
                 "fresh-context LCN1 restore: %s "
                 "phases=%d/%d/%d fingerprints=%llu/%llu/%llu\n",
                 RecoveredGameServices_LastLevelContinuationError(),
                 restoredContinuation.ownerPhases,
                 restoredContinuation.referencePhases,
                 restoredContinuation.eventPhases,
                 static_cast<unsigned long long>(
                     restoredContinuation.worldFingerprint),
                 static_cast<unsigned long long>(
                     restoredContinuation.restoredWorldFingerprint),
                 static_cast<unsigned long long>(
                     restoredContinuation.containerFingerprint));
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("fresh-context LCN1 restore or whole-world proof failed");
  }

  vehicleID = g_super.m_context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState restoredVehicle = {};
  SRecoveredVehicleControlJournalTelemetry resumedJournal = {};
  if (!VehicleRuntimeState_Inspect(
          g_super.m_context, vehicleID, &restoredVehicle) ||
      std::fabs(restoredVehicle.position.x -
                continuationVehicle.position.x) > 1.0e-7 ||
      std::fabs(restoredVehicle.position.y -
                continuationVehicle.position.y) > 1.0e-7 ||
      std::fabs(restoredVehicle.position.z -
                continuationVehicle.position.z) > 1.0e-7 ||
      !RecoveredGameServices_VehicleControlJournalTelemetry(
          &resumedJournal) || resumedJournal.recording != 1 ||
      resumedJournal.appendFailures != 0 ||
      resumedJournal.actionRecords != capturedContinuation.actionRecords ||
      resumedJournal.focusRecords != capturedContinuation.focusRecords ||
      !SendHardwareButton("W", TRUE)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("restored Vehicle/control lifecycle was not resumed");
  }
  for (int frame = 0; frame < 4; ++frame)
    if (!RunVehicleFrameAfter(0.025)) {
      ZAV_DeInitLevel();
      ZAV_Deinit();
      return Fail("post-restore Vehicle frame failed");
    }
  if (!SendHardwareButton("W", FALSE) ||
      !RunVehicleFrameAfter(0.01)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("post-restore Vehicle release frame failed");
  }
  SRecoveredVehicleRuntimeState continuedVehicle = {};
  if (!VehicleRuntimeState_Inspect(
          g_super.m_context, vehicleID, &continuedVehicle)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("post-restore Vehicle state was unavailable");
  }
  const double continuationDx =
      continuedVehicle.position.x - restoredVehicle.position.x;
  const double continuationDz =
      continuedVehicle.position.z - restoredVehicle.position.z;
  if (std::sqrt(continuationDx * continuationDx +
                continuationDz * continuationDz) <= 1.0e-6 ||
      !RecoveredGameServices_VehicleControlJournalTelemetry(
          &resumedJournal) ||
      resumedJournal.actionRecords !=
          capturedContinuation.actionRecords + 2u ||
      resumedJournal.appendFailures != 0) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("restored Vehicle did not continue through real frames");
  }
  ZAV_DeInitLevel();
  ZAV_Deinit();
  if (!IsServiceReleased() || !IsLevelRolledBack() ||
      RecoveredSoftwareGraph_IsReady() ||
      snd_distMax != initialSoundDistance ||
      snd_distMax2 != initialSoundDistanceSquared) {
    return Fail("complete service shutdown failed");
  }

  std::printf("bounded services frames=42 hooks=12 hardware=legacy "
               "arena=1 script=bounded common_attrs=3 smoke_attrs=18 "
               "smoke_subject=%d fingerprint=%llu "
               "smoke_simulation=START-MOVE-remove "
               "smoke_terrain=FireArea-directed-snap "
               "smoke_render=scene-alpha-sprite-detach smoke_visual=%llu "
              "explosion_attrs=%d explosion_fingerprint=%llu "
              "explosion_subject=0/%d-impact-damage-impulse-light "
              "fingerprint=%llu "
              "explosion_probe=2/1/1/1/1/0 "
              "explosion_light=useLight-brightness-frame-expiry "
              "explosion_sound=1/1/1-device-free refs=%llu "
              "explosion_particles=%d/%d/%d/%d/%d/%d/%d/%d-cap500 "
              "visual=%llu frame=draw-detach "
              "explosion_smoke=%d/%d/%d/%d/%d visual=%llu "
              "frame=alpha-draw-detach "
              "explosion_piece=%d/%d/%d/%d/%d refs=%llu "
              "frame=model-draw-detach "
              "explosion_trace=%d/%d/%d/%d/%d/%d/%d refs=%llu "
              "frame=NEWPUFF-common-Smoke-detach quota=4 "
              "vehicle_attrs=%d/%d vehicle_fingerprint=%llu "
              "vehicle_refs=%llu mass=%.0f "
              "vehicle_runtime=live-BeginPreStep-UpdatePos kind=%d fingerprint=%llu "
              "probe=%d/%d/%d/%d/%d/%d/%d/%d distance=%.6f "
              "vehicle_control=Hardware-exclusive-26/11/13/0 "
              "vehicle_focus=loss/gain-1/1 release=1 suppressed=2 stop=X "
              "vehicle_world=%u/%u/%u/%u "
               "vehicle_frames=42 dropped>=1 camera=Vehicle.Default fallback=0 "
               "taxi_attrs=%d/%d taxi_fingerprint=%llu taxi_refs=%llu "
               "taxi_subject=%d/%d sound=%d fingerprint=%llu "
               "taxi_lifecycle=1/1/1/1/2 "
               "taxi_vehicle=%d/%d/%d/%d/%d/%d/%d/%d "
               "taxi_handoff=F1-nearest-panel-drive-rollback "
               "vehicle_embodiment=F1-safe-Taxi-reentry-unsafe-Orphan-impact-rollback "
               "vehicle_fire=MouseL-Bullet-impact-visual-sound-focus-rollback "
               "bullet_attrs=%d/%d bullet_fingerprint=%llu "
              "bullet_refs=%llu "
              "bullet_subject=0/%d-ballistic-collision-impact-ground-waterline-barrel-smoke "
               "bullet_subject_fingerprint=%llu bullet_probe_moves=2 "
               "bullet_collision=2/1/4/3/4/1 bullet_effects=2/3/1/3 "
               "bullet_ground_spark=1/1 "
               "bullet_barrel_smoke=1/1/1/1-frameSec<=0.09 "
              "smoker_attrs=%d/%d smoker_fingerprint=%llu "
              "smoker_refs=%llu smoker_runtime=%d "
              "dyn_smoker=%d fingerprint=%llu "
              "smoker_emission=visible-MOVE-Smoke-draw-detach "
              "smoker_light_corona=visible-light-corona-draw-detach "
              "wav_metadata=%d/%d wav_fingerprint=%llu "
              "sound_object=%d fingerprint=%llu backend=device-free "
              "farter_attrs=%d/%d farter_fingerprint=%llu "
              "farter_refs=%llu farter_runtime=%d "
               "farter_subject=%d fingerprint=%llu live=%d sound=%d "
               "script_objects=%d dist=%.0f/%.0f "
               "near_frame=%d far_frame=%d transition=%d "
              "lamp_attrs=%d/%d lamp_fingerprint=%llu "
              "corpse_attrs=%d/%d corpse_fingerprint=%llu portal=table "
              "corpse_refs=%llu corpse_runtime=%d "
              "corpse_subject=%d fingerprint=%llu live=0 "
               "skin_models=%d skin_sprites=%d skin_catalog=%llu "
               "skin_resources=%llu "
                "spark=0/%d-sprite-light-May-phase spark_subject=%llu "
                "spark_visual=%llu spark_probe=2/1/1/5/1 "
                "people=%d/%d sound=%d attrs=%d/%d probe=%d/%d/%d/%d/%d "
                "tank_attrs=%d/%d cannon_attrs=%d/%d "
                "tank_subject=%d/%d cannon_subject=%d/%d "
                "tank_probe=%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d "
                "commander=%d/%d hostile=%d fingerprint=%llu "
                "tank_group=%d mission_tank=%d/%d/%d/%d/%d/%d/%d/%d "
                "fingerprint=%llu "
                "level_continuation=LCN1-%d/%d/%d events=%d/%d "
                "tick=%llu time=%.6f world=%llu journal=%llu container=%llu "
                "resumed_actions=%u "
                "route=table vehicle=real observer=fallback-suspended\n",
               smokeSubjectCapacity, smokeSubjectFingerprint,
               smokeVisualResourceFingerprint,
               explosionRosterSize, explosionFingerprint,
                explosionSubjectCapacity, explosionSubjectFingerprint,
                explosionSoundReferenceFingerprint,
                explosionParticleStartedBranches,
                explosionParticleSimple,
                explosionParticleSnake,
                explosionParticleRays,
                explosionParticleDependencySkips,
                explosionParticleMoveSteps,
                explosionParticleExpiredParents,
                explosionParticleRolledBackBranches,
                explosionParticleVisualFingerprint,
                explosionSmokeStartedSprites,
                explosionSmokeDependencySkips,
                explosionSmokeMoveSteps,
                explosionSmokeExpiredParents,
               explosionSmokeRolledBackSprites,
               explosionSmokeVisualFingerprint,
               explosionPieceStartedPieces,
               explosionPieceDependencySkips,
               explosionPieceMoveSteps,
               explosionPieceExpiredParents,
               explosionPieceRolledBackPieces,
               explosionPieceReferenceFingerprint,
               explosionTraceStartedPieces,
               explosionTraceQuotaSkips,
               explosionTracePuffEvents,
               explosionTraceSmokeChildren,
               explosionTraceMoveSteps,
               explosionTraceExpiredParents,
               explosionTraceRolledBackPieces,
               explosionTraceReferenceFingerprint,
                vehicleAttributeRosterSize, vehicleAttributeCapacity,
                vehicleAttributeFingerprint, vehicleReferenceFingerprint,
                vehicleVesselMass,
                vehicleVesselKind, vehicleRuntimeFingerprint,
                vehicleProbeInvalidActivations, vehicleProbeActivations,
                vehicleProbeStationarySteps, vehicleProbeThrottleEvents,
                vehicleProbeMovementSteps, vehicleProbeTurnEvents,
                vehicleProbeCameraTransitions, vehicleProbeRollbacks,
                vehicleProbeHorizontalDistance,
                vehicleDriveTelemetry.groundContactFrames,
                vehicleDriveTelemetry.staticCollisionFrames,
                vehicleDriveTelemetry.landCollisionFrames,
                vehicleDriveTelemetry.dynamicCollisionFrames,
                 taxiRosterSize, taxiCapacity, taxiFingerprint,
                 taxiReferenceFingerprint,
                 taxiSubjectCount, taxiSubjectCapacity,
                 taxiSubjectSoundCount, taxiSubjectFingerprint,
                 taxiVehicleAvailableTaxis, taxiVehicleInvalidTargets,
                 taxiVehicleTransitions, taxiVehicleAttributeTransfers,
                 taxiVehiclePoseTransfers, taxiVehiclePayloadTransfers,
                 taxiVehicleRemovedTaxis, taxiVehicleRollbacks,
                 bulletRosterSize, bulletCapacity, bulletFingerprint,
                bulletReferenceFingerprint, bulletSubjectCapacity,
                bulletSubjectFingerprint,
              smokerRosterSize, smokerCapacity, smokerFingerprint,
              smokerReferenceFingerprint, smokerRuntimeReady ? 1 : 0,
              dynSmokerCapacity, dynSmokerFingerprint,
              wavRosterSize, wavCapacity, wavFingerprint,
              soundObjectCapacity, soundObjectFingerprint,
              farterRosterSize, farterCapacity, farterFingerprint,
              farterReferenceFingerprint, farterRuntimeReady ? 1 : 0,
               farterSubjectCapacity, farterSubjectFingerprint,
               farterLiveObjectCount, farterSoundObjectCount,
               farterScriptObjectCount,
               soundDistance, soundDistanceSquared,
               farterNearFrameAudibleCount, farterFarFrameAudibleCount,
               farterAudibleFrameTransition ? 1 : 0,
              lampRosterSize, lampCapacity, lampFingerprint,
              corpseRosterSize, corpseCapacity, corpseFingerprint,
              corpseReferenceFingerprint, corpseRuntimeReady ? 1 : 0,
              corpseSubjectCapacity, corpseSubjectFingerprint,
               skinModelCount,
               skinSpriteCount, skinCatalogFingerprint,
               skinResourceFingerprint, sparkSubjectCapacity,
               sparkSubjectFingerprint, sparkVisualResourceFingerprint,
               peopleSubjectCount, peopleSubjectCapacity,
               peopleSubjectSoundCount,
               peopleAttributeCount, peopleAttributeCapacity,
               peopleProbeScheduledMoves, peopleProbeBulletDamage,
               peopleProbeDeathTransitions, peopleProbeSaveRoundTrips,
               peopleProbeRollbacks,
               tankAttributeCount, tankAttributeCapacity,
               cannonAttributeCount, cannonAttributeCapacity,
               tankSubjectCount, tankSubjectCapacity,
               cannonSubjectCount, cannonSubjectCapacity,
               tankProbeAvailable, tankProbeValidStarts,
               tankProbeDynamicReady, tankProbeRenderReady,
               tankProbeCannonReady, tankProbeScheduledMoves,
               tankProbeBulletDamage, tankProbeDeathTransitions,
               tankProbeDeathEffects, tankProbeSaveRoundTrips,
               tankProbeRollbacks,
               commanderCount, commanderCapacity, commanderHostileLinks,
               commanderFingerprint, tankGroupSubjectCapacity,
               missionTankAvailable, missionTankSpawns,
               missionTankMembershipLinks, missionTankFindEnemyCycles,
               missionTankMovingCycles, missionTankStableRoundTrips,
               missionTankReconstructedIDs, missionTankRollbacks,
               missionTankFingerprint,
               restoredContinuation.sections,
               restoredContinuation.ownerPhases,
               restoredContinuation.referencePhases,
               restoredContinuation.events,
               restoredContinuation.eventPhases,
               static_cast<unsigned long long>(
                   restoredContinuation.simulationTick),
               restoredContinuation.simulationTime,
               static_cast<unsigned long long>(
                   restoredContinuation.worldFingerprint),
               static_cast<unsigned long long>(
                   restoredContinuation.journalFingerprint),
               static_cast<unsigned long long>(
                   restoredContinuation.containerFingerprint),
               resumedJournal.actionRecords);
  return EXIT_SUCCESS;
}
