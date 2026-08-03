#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

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
#include "obase/explosion/ExplosionActiveWorldState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/farter/FarterSubjectState.h"
#include "obase/lamp/LampAttributeState.h"
#include "obase/orphan/OrphanSubjectState.h"
#include "obase/orphan/OrphanActiveWorldState.h"
#include "obase/people/PeopleActiveWorldState.h"
#include "obase/spark/SparkAttributeState.h"
#include "obase/spark/SparkSubjectState.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/taxi/TaxiAttributeState.h"
#include "obase/taxi/Taxi.h"
#include "obase/taxi/TaxiActiveWorldState.h"
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
#include "RecoveredSavePreview.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavOverallInfoState.h"
#include "ZavSceneState.h"
#include "ZavShutdownState.h"

extern int g_godMode;

namespace {

const unsigned long long kBulletCacheHashOffset = 14695981039346656037ull;
const unsigned long long kBulletCacheHashPrime = 1099511628211ull;

struct STaxiDebugGroundingProbeSummary {
  int types = 0;
  int sweepHits = 0;
  int terrainFallbacks = 0;
  double maxBottomClearance = 0.0;
  double maxImmediateDrift = 0.0;
};

STaxiDebugGroundingProbeSummary g_taxiDebugGroundingProbe;

void (*g_originalAlphaSprite)(SGRAlphaSprite*) = nullptr;
void (*g_originalSprite)(int, int, int, int, int, int, int, int,
                         int, void*) = nullptr;
void (*g_originalParticle)(int, int, int, int, unsigned long) = nullptr;

bool ExerciseObserverAxisReducer() {
  SRecoveredObserverAxes axes = {};
  const auto exercisePair =
      [&axes](int positiveAction, int negativeAction,
              double SRecoveredObserverAxes::*member) {
        axes = {};
        if (!RecoveredObserverAxes_ApplyLegacyAction(
                &axes, positiveAction, 1.0) ||
            axes.*member != 1.0 ||
            !RecoveredObserverAxes_ApplyLegacyAction(
                &axes, negativeAction, 0.0) ||
            axes.*member != 0.0 ||
            !RecoveredObserverAxes_ApplyLegacyAction(
                &axes, positiveAction, -1.0) ||
            axes.*member != -1.0 ||
            !RecoveredObserverAxes_ApplyLegacyAction(
                &axes, negativeAction, 0.0) ||
            !RecoveredObserverAxes_IsNeutral(axes)) {
          return false;
        }

        if (!RecoveredObserverAxes_ApplyLegacyAction(
                &axes, negativeAction, 1.0) ||
            axes.*member != -1.0 ||
            !RecoveredObserverAxes_ApplyLegacyAction(
                &axes, positiveAction, 0.0) ||
            axes.*member != 0.0 ||
            !RecoveredObserverAxes_ApplyLegacyAction(
                &axes, negativeAction, -1.0) ||
            axes.*member != 1.0 ||
            !RecoveredObserverAxes_ApplyLegacyAction(
                &axes, positiveAction, 0.0) ||
            !RecoveredObserverAxes_IsNeutral(axes)) {
          return false;
        }
        return true;
      };

  if (!RecoveredObserverAxes_IsNeutral(axes) ||
      !exercisePair(MOVE_FORWARD, MOVE_BACKWARD,
                    &SRecoveredObserverAxes::forward) ||
      !exercisePair(STRAFE_RIGHT, STRAFE_LEFT,
                    &SRecoveredObserverAxes::strafe) ||
      !exercisePair(STRAFE_UP, STRAFE_DOWN,
                    &SRecoveredObserverAxes::vertical) ||
      !exercisePair(TURN_RIGHT, TURN_LEFT,
                    &SRecoveredObserverAxes::turn) ||
      !exercisePair(LOOK_UP, LOOK_DOWN,
                    &SRecoveredObserverAxes::look)) {
    return false;
  }

  axes.forward = 0.25;
  const SRecoveredObserverAxes beforeInvalid = axes;
  if (RecoveredObserverAxes_ApplyLegacyAction(&axes, -1337, 1.0) ||
      axes.forward != beforeInvalid.forward ||
      RecoveredObserverAxes_ApplyLegacyAction(
          &axes, MOVE_FORWARD,
          (std::numeric_limits<double>::quiet_NaN)()) ||
      axes.forward != beforeInvalid.forward ||
      RecoveredObserverAxes_ApplyLegacyAction(
          nullptr, MOVE_FORWARD, 1.0)) {
    return false;
  }
  return true;
}
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

void CleanupSaveSlotFixture(const std::wstring& directory) {
  for (std::uint32_t slot = 0;
       slot < LevelSaveSlot_Count(); ++slot) {
    const std::wstring path = LevelSaveSlot_Path(directory, slot);
    if (!path.empty()) DeleteFileW(path.c_str());
  }
  RemoveDirectoryW(directory.c_str());
}

bool PrepareSaveSlotFixture(std::wstring* directory) {
  if (directory == nullptr) return false;
  wchar_t temporaryRoot[MAX_PATH + 1] = {};
  const DWORD length =
      GetTempPathW(MAX_PATH, temporaryRoot);
  if (length == 0 || length > MAX_PATH) return false;
  *directory = temporaryRoot;
  *directory += L"rr2nw-level-slot-runtime-";
  *directory += std::to_wstring(GetCurrentProcessId());
  CleanupSaveSlotFixture(*directory);
  return true;
}

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
         !RecoveredGameServices_DebugMapReady() &&
         !RecoveredGameServices_DebugMapActive() &&
         RecoveredGameServices_DebugMapWidth() == 0 &&
         RecoveredGameServices_DebugMapHeight() == 0 &&
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
         !RecoveredGameServices_VehicleDeathCameraReady() &&
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
          RecoveredGameServices_VehicleProbeStabilityRecoveries() == -1 &&
          RecoveredGameServices_VehicleProbeRollbacks() == -1 &&
         RecoveredGameServices_VehicleProbeHorizontalDistance() == 0.0 &&
         RecoveredGameServices_VehicleDeathCameraProbeActivations() == -1 &&
         RecoveredGameServices_VehicleDeathCameraProbeAscentFrames() == -1 &&
         RecoveredGameServices_VehicleDeathCameraProbeTerminalFrames() == -1 &&
         RecoveredGameServices_VehicleDeathCameraProbeCompletionTransitions() ==
             -1 &&
         RecoveredGameServices_VehicleDeathCameraProbeFiniteCameras() == -1 &&
         RecoveredGameServices_VehicleDeathCameraProbeRollbacks() == -1 &&
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
         RecoveredGameServices_VehicleCameraMode() ==
             RECOVERED_VEHICLE_CAMERA_UNKNOWN &&
         RecoveredGameServices_VehicleCameraTransformFrameCount() == 0 &&
         RecoveredGameServices_VehicleDeathCameraFrameCount() == 0 &&
         RecoveredGameServices_VehicleDeathCameraCompletions() == 0 &&
         RecoveredGameServices_VehicleDeathCameraOffsetY() == 0.0 &&
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
         !RecoveredArenaSeance_SkinAnimationsReady() &&
         !RecoveredArenaSeance_TeleportRoutesReady() &&
         !RecoveredArenaSeance_TeleportTargetLevel() &&
         RecoveredArenaSeance_TeleportCapacity() == 0 &&
         RecoveredArenaSeance_TeleportRouteCount() == 0 &&
         RecoveredArenaSeance_TeleportProbeRejectedNonPlayer() == 0 &&
         RecoveredArenaSeance_TeleportProbePhysicsCollisions() == 0 &&
         RecoveredArenaSeance_TeleportProbeAppliedPlayer() == 0 &&
         RecoveredArenaSeance_TeleportProbeVehicleRollbacks() == 0 &&
         RecoveredArenaSeance_TeleportFingerprint() == 0 &&
         RecoveredArenaSeance_SkinAnimationEntryCallCount() == 0 &&
         RecoveredArenaSeance_SkinAnimatedModelCount() == 0 &&
         RecoveredArenaSeance_SkinAnimationCommandCount() == 0 &&
         RecoveredArenaSeance_SkinAnimationSourceFingerprint() == 0 &&
         RecoveredArenaSeance_SkinAnimationStateFingerprint() == 0 &&
         RecoveredArenaSeance_SkinAnimationPoseTemporalModelCount() == 0 &&
         RecoveredArenaSeance_SkinAnimationPoseChangedModelCount() == 0 &&
         RecoveredArenaSeance_SkinAnimationPoseSampleCount() == 0 &&
         RecoveredArenaSeance_SkinAnimationPoseRestoredModifierCount() == 0 &&
         RecoveredArenaSeance_SkinAnimationPoseFingerprint() == 0 &&
         !RecoveredArenaSeance_StaticMechanismsReady() &&
         !RecoveredArenaSeance_StaticMechanismTargetLevel() &&
         !RecoveredArenaSeance_StaticMechanismLevelOne() &&
         !RecoveredArenaSeance_StaticMechanismLevelFive() &&
         RecoveredArenaSeance_StaticMechanismBindingCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismWaterwheelCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismFlagCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismRotatingCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismDoorCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismPol16Count() == 0 &&
         RecoveredArenaSeance_StaticMechanismChangedBindingCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismPoseSampleCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismRestoredModifierCount() == 0 &&
         RecoveredArenaSeance_StaticMechanismFingerprint() == 0 &&
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

bool SendHardwareButton(const char* keyName, int buttonDown);
bool RunVehicleFrameAfter(double minimumDelta);

bool ExerciseCrossLevelLoad(const char* sourceDirectory,
                            const char* targetDirectory,
                            const std::wstring& saveDirectory) {
  constexpr std::uint32_t kTargetSlot = 4u;

  // Build one real target-Level RR2SLOT1 fixture. The source slot in index 3
  // was committed by the preceding same-Level continuation proof.
  ZAV_DeInitLevel();
  if (!StartServices(targetDirectory) ||
      !RecoveredGameServices_RunFrame()) {
    std::fprintf(stderr, "cross-Level target fixture did not start\n");
    return false;
  }
  if (!RecoveredGameServices_ConfigureDebugMenu(
          true, std::vector<std::string>{"Level.CrossLoad.Target"})) {
    std::fprintf(stderr, "cross-Level target debug catalog failed\n");
    return false;
  }
  std::size_t occupiedIndex = RecoveredGameServices_DebugVehicleTypeCount();
  SRecoveredDebugVehicleType occupiedType;
  for (std::size_t index = 0;
       index < RecoveredGameServices_DebugVehicleTypeCount(); ++index) {
    SRecoveredDebugVehicleType type;
    if (RecoveredGameServices_DebugVehicleType(index, &type) &&
        type.vehicleType == 1 &&
        type.vesselKind != RECOVERED_VEHICLE_VESSEL_UNKNOWN &&
        type.vesselProfile != RECOVERED_VEHICLE_PROFILE_UNKNOWN) {
      occupiedIndex = index;
      occupiedType = type;
      break;
    }
  }
  bool occupied = occupiedIndex <
      RecoveredGameServices_DebugVehicleTypeCount() &&
      RecoveredGameServices_RequestDebugVehicleSpawn(
          occupiedIndex, true) &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  for (int frame = 0; occupied && frame < 4; ++frame)
    occupied = RunVehicleFrameAfter(0.025);
  bool drove = occupied && SendHardwareButton("W", TRUE);
  for (int frame = 0; drove && frame < 8; ++frame)
    drove = RunVehicleFrameAfter(0.025);
  drove = drove && SendHardwareButton("W", FALSE) &&
      RunVehicleFrameAfter(0.01);
  const bool damaged = drove &&
      RecoveredGameServices_RequestDebugDamageOccupiedVehicle() &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  SRecoveredVehicleAuthorityState targetAuthority = {};
  SRecoveredVehicleRuntimeState targetVehicleState = {};
  SRecoveredVehicleControlJournalTelemetry targetJournal = {};
  KR_ObjectID targetVehicle = g_super.m_context == nullptr
      ? KR_ObjectID::NUL()
      : g_super.m_context->searchObject("Vehicle.Default");
  const bool targetAuthorityReady = damaged &&
      RecoveredGameServices_VehicleAuthorityState(&targetAuthority) &&
      VehicleRuntimeState_Inspect(
          g_super.m_context, targetVehicle, &targetVehicleState) &&
      RecoveredGameServices_VehicleControlJournalTelemetry(&targetJournal) &&
      targetAuthority.active == 1 && targetAuthority.frameBegun == 0 &&
      targetAuthority.dead == 0 && targetAuthority.takingTaxi == 0 &&
      targetAuthority.taxiChangeEnabled == 0 &&
      targetAuthority.vesselKind == occupiedType.vesselKind &&
      targetAuthority.vesselProfile == occupiedType.vesselProfile &&
      targetAuthority.damage > 0.0 &&
      targetAuthority.panelOpen == targetAuthority.panelReady &&
      RecoveredGameServices_VehicleCameraMode() ==
          RECOVERED_VEHICLE_CAMERA_LIVE &&
      std::sqrt(targetVehicleState.speed.x * targetVehicleState.speed.x +
                targetVehicleState.speed.y * targetVehicleState.speed.y +
                targetVehicleState.speed.z * targetVehicleState.speed.z) >
          1.0e-6 &&
      targetJournal.recording == 1 && targetJournal.actionRecords == 2u &&
      targetJournal.appendFailures == 0u &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u;

  SLevelSaveSlotSummary targetSlot;
  SLevelContinuationSummary targetContinuation;
  if (!targetAuthorityReady || !RecoveredGameServices_SaveLevelSlot(
          saveDirectory, kTargetSlot, "cross-Level target",
          "occupied moving/damaged coordinator regression", {}, &targetSlot,
          &targetContinuation) ||
      !targetSlot.ready || !targetContinuation.ready ||
      targetSlot.level.empty() || targetContinuation.actionRecords != 2u ||
      !RecoveredGameServices_ConfigureDebugMenu(
          false, std::vector<std::string>())) {
    std::fprintf(stderr, "cross-Level target fixture save failed: %s\n",
                 RecoveredGameServices_LastLevelSaveSlotError());
    return false;
  }

  ZAV_DeInitLevel();
  if (!StartServices(sourceDirectory) ||
      !RecoveredGameServices_RunFrame() ||
      !RecoveredGameServices_RequestLoadSlot(kTargetSlot) ||
      !RecoveredGameServices_ProcessPendingSaveCommand()) {
    std::fprintf(stderr, "cross-Level request staging failed: %s\n",
                 RecoveredGameServices_SaveMenuState()->lastError.c_str());
    return false;
  }
  const SRecoveredSaveMenuState* staged =
      RecoveredGameServices_SaveMenuState();
  if (staged == nullptr || !staged->crossLevelRestartPending ||
      staged->crossLevelRequests != 1u ||
      staged->completedCrossLevelLoads != 0u ||
      staged->crossLevelRollbacks != 0u ||
      !RecoveredGameServices_CrossLevelLoadPending()) {
    std::fprintf(stderr, "cross-Level staged telemetry is incorrect\n");
    return false;
  }
  SRecoveredCrossLevelLoadRequest request;
  if (!RecoveredGameServices_TakeCrossLevelLoadRequest(&request) ||
      !request.ready || request.slot != kTargetSlot ||
      request.sourceLevel.empty() ||
      request.targetLevel != targetSlot.level ||
      request.sourceContinuation.empty() ||
      request.targetContinuation.empty() ||
      !request.sourceContinuationSummary.ready ||
      request.targetSlot.archiveFingerprint !=
          targetSlot.archiveFingerprint) {
    std::fprintf(stderr, "cross-Level handoff contract is incomplete\n");
    return false;
  }

  ZAV_DeInitLevel();
  SLevelContinuationSummary restoredTarget;
  const bool targetApplied = StartServices(targetDirectory) &&
      RecoveredGameServices_ApplyCrossLevelLoad(request, &restoredTarget);
  SRecoveredVehicleAuthorityState restoredAuthority = {};
  SRecoveredVehicleRuntimeState restoredVehicleState = {};
  SRecoveredVehicleControlJournalTelemetry restoredJournal = {};
  KR_ObjectID restoredVehicle = g_super.m_context == nullptr
      ? KR_ObjectID::NUL()
      : g_super.m_context->searchObject("Vehicle.Default");
  const bool targetAuthorityRestored = targetApplied &&
      restoredTarget.restoredWorldFingerprint ==
          targetContinuation.worldFingerprint &&
      RecoveredGameServices_VehicleAuthorityState(&restoredAuthority) &&
      VehicleRuntimeState_Inspect(
          g_super.m_context, restoredVehicle, &restoredVehicleState) &&
      RecoveredGameServices_VehicleControlJournalTelemetry(
          &restoredJournal) &&
      restoredAuthority.identityFingerprint ==
          targetAuthority.identityFingerprint &&
      std::fabs(restoredAuthority.damage - targetAuthority.damage) <=
          1.0e-9 &&
      restoredAuthority.vesselKind == targetAuthority.vesselKind &&
      restoredAuthority.vesselProfile == targetAuthority.vesselProfile &&
      restoredAuthority.active == targetAuthority.active &&
      restoredAuthority.frameBegun == targetAuthority.frameBegun &&
      restoredAuthority.dead == targetAuthority.dead &&
      restoredAuthority.takingTaxi == targetAuthority.takingTaxi &&
      restoredAuthority.panelReady == targetAuthority.panelReady &&
      restoredAuthority.panelOpen == targetAuthority.panelOpen &&
      restoredAuthority.taxiChangeEnabled ==
          targetAuthority.taxiChangeEnabled &&
      std::fabs(restoredVehicleState.position.x -
                targetVehicleState.position.x) <= 1.0e-7 &&
      std::fabs(restoredVehicleState.position.y -
                targetVehicleState.position.y) <= 1.0e-7 &&
      std::fabs(restoredVehicleState.position.z -
                targetVehicleState.position.z) <= 1.0e-7 &&
      std::fabs(restoredVehicleState.speed.x -
                targetVehicleState.speed.x) <= 1.0e-7 &&
      std::fabs(restoredVehicleState.speed.y -
                targetVehicleState.speed.y) <= 1.0e-7 &&
      std::fabs(restoredVehicleState.speed.z -
                targetVehicleState.speed.z) <= 1.0e-7 &&
      RecoveredGameServices_VehicleCameraMode() ==
          RECOVERED_VEHICLE_CAMERA_LIVE &&
      restoredJournal.recording == 1 &&
      restoredJournal.actionRecords == targetJournal.actionRecords &&
      restoredJournal.appendFailures == 0u &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u;
  const bool targetResumed = targetAuthorityRestored &&
      SendHardwareButton("D", TRUE) && RunVehicleFrameAfter(0.025) &&
      RunVehicleFrameAfter(0.025) && SendHardwareButton("D", FALSE) &&
      RunVehicleFrameAfter(0.01) &&
      RecoveredGameServices_VehicleControlJournalTelemetry(
          &restoredJournal) &&
      restoredJournal.actionRecords == targetJournal.actionRecords + 2u &&
      restoredJournal.appendFailures == 0u &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u;
  if (!targetResumed) {
    std::fprintf(stderr, "cross-Level target commit failed: %s\n",
                 RecoveredGameServices_SaveMenuState()->lastError.c_str());
    return false;
  }
  const SRecoveredSaveMenuState* committed =
      RecoveredGameServices_SaveMenuState();
  if (committed == nullptr || committed->crossLevelRestartPending ||
      committed->crossLevelRequests != 1u ||
      committed->completedCrossLevelLoads != 1u ||
      committed->completedLoads != 1u || committed->loadRequests != 1u ||
      committed->crossLevelRollbacks != 0u ||
      committed->crossLevelRollbackFailures != 0u) {
    std::fprintf(stderr, "cross-Level commit telemetry is incorrect\n");
    return false;
  }

  // Stage a valid return to the source Level, then corrupt only the in-memory
  // handoff. The target Level must remain recoverable through the checkpoint
  // captured at the staging boundary.
  SRecoveredVehicleAuthorityState rollbackAuthority = {};
  if (!RecoveredGameServices_VehicleAuthorityState(&rollbackAuthority) ||
      !RecoveredGameServices_RequestLoadSlot(3u) ||
      !RecoveredGameServices_ProcessPendingSaveCommand()) {
    std::fprintf(stderr, "cross-Level rollback request failed: %s\n",
                 RecoveredGameServices_SaveMenuState()->lastError.c_str());
    return false;
  }
  SRecoveredCrossLevelLoadRequest rejected;
  if (!RecoveredGameServices_TakeCrossLevelLoadRequest(&rejected) ||
      rejected.targetContinuation.empty()) {
    std::fprintf(stderr, "cross-Level rollback handoff is unavailable\n");
    return false;
  }
  rejected.targetContinuation.back() ^= 0x5au;

  ZAV_DeInitLevel();
  SLevelContinuationSummary rejectedSummary;
  if (!StartServices(sourceDirectory) ||
      RecoveredGameServices_ApplyCrossLevelLoad(
          rejected, &rejectedSummary)) {
    std::fprintf(stderr, "corrupt cross-Level target was admitted\n");
    return false;
  }
  const std::string rejectedFailure =
      RecoveredGameServices_SaveMenuState()->lastError;
  ZAV_DeInitLevel();
  SLevelContinuationSummary rolledBackTarget;
  SRecoveredVehicleAuthorityState restoredRollbackAuthority = {};
  SRecoveredVehicleControlJournalTelemetry rollbackJournal = {};
  if (!StartServices(targetDirectory) ||
      !RecoveredGameServices_RestoreLevelContinuation(
          rejected.sourceContinuation, &rolledBackTarget) ||
      !RecoveredGameServices_VehicleAuthorityState(
          &restoredRollbackAuthority) ||
      !RecoveredGameServices_VehicleControlJournalTelemetry(
          &rollbackJournal)) {
    std::fprintf(stderr, "cross-Level source rollback restore failed: %s\n",
                 RecoveredGameServices_LastLevelContinuationError());
    return false;
  }
  RecoveredGameServices_RecordCrossLevelLoadFailure(
      rejected, rejectedFailure, true, true);
  const SRecoveredSaveMenuState* rolledBack =
      RecoveredGameServices_SaveMenuState();
  if (rolledBack == nullptr || rolledBack->crossLevelRestartPending ||
      rolledBack->crossLevelRequests != 2u ||
      rolledBack->completedCrossLevelLoads != 1u ||
      rolledBack->crossLevelRollbacks != 1u ||
      rolledBack->crossLevelRollbackFailures != 0u ||
      rolledBack->failedCommands != 1u ||
      rolledBackTarget.restoredWorldFingerprint !=
          rejected.sourceContinuationSummary.worldFingerprint ||
      restoredRollbackAuthority.identityFingerprint !=
          rollbackAuthority.identityFingerprint ||
      std::fabs(restoredRollbackAuthority.damage -
                rollbackAuthority.damage) > 1.0e-9 ||
      restoredRollbackAuthority.vesselProfile !=
          rollbackAuthority.vesselProfile ||
      restoredRollbackAuthority.panelReady !=
          rollbackAuthority.panelReady ||
      restoredRollbackAuthority.panelOpen != rollbackAuthority.panelOpen ||
      RecoveredGameServices_VehicleCameraMode() !=
          RECOVERED_VEHICLE_CAMERA_LIVE ||
      rollbackJournal.actionRecords !=
          rejected.sourceContinuationSummary.actionRecords ||
      rollbackJournal.appendFailures != 0u ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u ||
      !RecoveredGameServices_RunFrame()) {
    std::fprintf(stderr, "cross-Level rollback telemetry/proof failed\n");
    return false;
  }

  // The corrupt-container case above fails before reconstruction. Stage the
  // same return once more and reject it only after the destination world and
  // all gameplay authority have been reconstructed successfully. This must
  // unwind both transaction layers byte-for-byte: the destination-local
  // preflight checkpoint first, then the occupied coordinator source.
  std::vector<std::uint8_t> authoritySourceBefore;
  SLevelContinuationSummary authoritySourceBeforeSummary;
  SRecoveredVehicleAuthorityState authoritySourceBeforeState = {};
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &authoritySourceBefore, &authoritySourceBeforeSummary) ||
      authoritySourceBefore.empty() ||
      !RecoveredGameServices_VehicleAuthorityState(
          &authoritySourceBeforeState) ||
      !RecoveredGameServices_RequestLoadSlot(3u) ||
      !RecoveredGameServices_ProcessPendingSaveCommand()) {
    std::fprintf(stderr,
                 "post-restore authority rollback request failed\n");
    return false;
  }
  SRecoveredCrossLevelLoadRequest authorityRejected;
  if (!RecoveredGameServices_TakeCrossLevelLoadRequest(
          &authorityRejected) ||
      authorityRejected.targetContinuation.empty() ||
      authorityRejected.sourceContinuation != authoritySourceBefore) {
    std::fprintf(stderr,
                 "post-restore authority handoff changed source bytes\n");
    return false;
  }

  ZAV_DeInitLevel();
  std::vector<std::uint8_t> destinationBefore;
  SLevelContinuationSummary destinationBeforeSummary;
  if (!StartServices(sourceDirectory) ||
      !RecoveredGameServices_CaptureLevelContinuation(
          &destinationBefore, &destinationBeforeSummary) ||
      destinationBefore.empty()) {
    std::fprintf(stderr,
                 "post-restore authority destination preflight failed\n");
    return false;
  }
  RecoveredGameServices_FailNextRestoredGameplayAuthorityForTesting();
  SLevelContinuationSummary authorityRejectedSummary;
  if (RecoveredGameServices_ApplyCrossLevelLoad(
          authorityRejected, &authorityRejectedSummary)) {
    std::fprintf(stderr,
                 "post-restore authority failure was admitted\n");
    return false;
  }
  const std::string authorityFailure =
      RecoveredGameServices_SaveMenuState()->lastError;
  std::vector<std::uint8_t> destinationAfter;
  SLevelContinuationSummary destinationAfterSummary;
  if (authorityFailure !=
          "injected post-restore gameplay authority failure" ||
      !RecoveredGameServices_CaptureLevelContinuation(
          &destinationAfter, &destinationAfterSummary) ||
      destinationAfter != destinationBefore ||
      destinationAfterSummary.worldFingerprint !=
          destinationBeforeSummary.worldFingerprint) {
    std::fprintf(stderr,
                 "post-restore authority destination rollback changed "
                 "LCN1: %s\n",
                 authorityFailure.c_str());
    return false;
  }

  ZAV_DeInitLevel();
  SLevelContinuationSummary authorityRolledBackSummary;
  std::vector<std::uint8_t> authoritySourceAfter;
  SLevelContinuationSummary authoritySourceAfterSummary;
  SRecoveredVehicleAuthorityState authoritySourceAfterState = {};
  SRecoveredVehicleControlJournalTelemetry authorityRollbackJournal = {};
  if (!StartServices(targetDirectory) ||
      !RecoveredGameServices_RestoreLevelContinuation(
          authorityRejected.sourceContinuation,
          &authorityRolledBackSummary) ||
      !RecoveredGameServices_CaptureLevelContinuation(
          &authoritySourceAfter, &authoritySourceAfterSummary) ||
      !RecoveredGameServices_VehicleAuthorityState(
          &authoritySourceAfterState) ||
      !RecoveredGameServices_VehicleControlJournalTelemetry(
          &authorityRollbackJournal)) {
    std::fprintf(stderr,
                 "post-restore authority source rollback failed: %s\n",
                 RecoveredGameServices_LastLevelContinuationError());
    return false;
  }
  RecoveredGameServices_RecordCrossLevelLoadFailure(
      authorityRejected, authorityFailure, true, true);
  const SRecoveredSaveMenuState* authorityRolledBack =
      RecoveredGameServices_SaveMenuState();
  if (authorityRolledBack == nullptr ||
      authorityRolledBack->crossLevelRestartPending ||
      authorityRolledBack->crossLevelRequests != 3u ||
      authorityRolledBack->completedCrossLevelLoads != 1u ||
      authorityRolledBack->crossLevelRollbacks != 2u ||
      authorityRolledBack->crossLevelRollbackFailures != 0u ||
      authorityRolledBack->failedCommands != 1u ||
      authorityRolledBack->lastError != authorityFailure ||
      authoritySourceAfter != authoritySourceBefore ||
      authoritySourceAfterSummary.worldFingerprint !=
          authoritySourceBeforeSummary.worldFingerprint ||
      authorityRolledBackSummary.restoredWorldFingerprint !=
          authorityRejected.sourceContinuationSummary.worldFingerprint ||
      authoritySourceAfterState.identityFingerprint !=
          authoritySourceBeforeState.identityFingerprint ||
      std::fabs(authoritySourceAfterState.damage -
                authoritySourceBeforeState.damage) > 1.0e-9 ||
      authoritySourceAfterState.vesselProfile !=
          authoritySourceBeforeState.vesselProfile ||
      authoritySourceAfterState.panelReady !=
          authoritySourceBeforeState.panelReady ||
      authoritySourceAfterState.panelOpen !=
          authoritySourceBeforeState.panelOpen ||
      RecoveredGameServices_VehicleCameraMode() !=
          RECOVERED_VEHICLE_CAMERA_LIVE ||
      authorityRollbackJournal.actionRecords !=
          authorityRejected.sourceContinuationSummary.actionRecords ||
      authorityRollbackJournal.appendFailures != 0u ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u ||
      !RecoveredGameServices_RunFrame()) {
    std::fprintf(stderr,
                 "post-restore authority source rollback changed state "
                 "(state=%d pending=%d requests=%u completed=%u "
                 "rollbacks=%u/%u failed=%u error=%d bytes=%d "
                 "world=%llu/%llu/%llu identity=%llu/%llu "
                 "damage=%.9f/%.9f profile=%d/%d panel=%d/%d:%d/%d "
                 "camera=%d journal=%u/%u append=%u active=%u)\n",
                 authorityRolledBack != nullptr ? 1 : 0,
                 authorityRolledBack != nullptr
                     ? (authorityRolledBack->crossLevelRestartPending ? 1 : 0)
                     : -1,
                 authorityRolledBack != nullptr
                     ? authorityRolledBack->crossLevelRequests : 0u,
                 authorityRolledBack != nullptr
                     ? authorityRolledBack->completedCrossLevelLoads : 0u,
                 authorityRolledBack != nullptr
                     ? authorityRolledBack->crossLevelRollbacks : 0u,
                 authorityRolledBack != nullptr
                     ? authorityRolledBack->crossLevelRollbackFailures : 0u,
                 authorityRolledBack != nullptr
                     ? authorityRolledBack->failedCommands : 0u,
                 authorityRolledBack != nullptr &&
                         authorityRolledBack->lastError == authorityFailure
                     ? 1 : 0,
                 authoritySourceAfter == authoritySourceBefore ? 1 : 0,
                 static_cast<unsigned long long>(
                     authoritySourceAfterSummary.worldFingerprint),
                 static_cast<unsigned long long>(
                     authoritySourceBeforeSummary.worldFingerprint),
                 static_cast<unsigned long long>(
                     authorityRolledBackSummary.restoredWorldFingerprint),
                 authoritySourceAfterState.identityFingerprint,
                 authoritySourceBeforeState.identityFingerprint,
                 authoritySourceAfterState.damage,
                 authoritySourceBeforeState.damage,
                 authoritySourceAfterState.vesselProfile,
                 authoritySourceBeforeState.vesselProfile,
                 authoritySourceAfterState.panelReady,
                 authoritySourceBeforeState.panelReady,
                 authoritySourceAfterState.panelOpen,
                 authoritySourceBeforeState.panelOpen,
                 RecoveredGameServices_VehicleCameraMode(),
                 authorityRollbackJournal.actionRecords,
                 authorityRejected.sourceContinuationSummary.actionRecords,
                 authorityRollbackJournal.appendFailures,
                 RecoveredGameServices_VehicleActiveActionCount());
    return false;
  }
  std::printf(
      "occupied_cross_level_authority=commit-resume-rollback "
      "profile=%d damage=%.6f panel=%d/%d world=%llu "
      "actions=%u/%u rollback_world=%llu "
      "post_authority_failure=target/source-byte-exact\n",
      occupiedType.vesselProfile, targetAuthority.damage,
      targetAuthority.panelReady, targetAuthority.panelOpen,
      static_cast<unsigned long long>(targetContinuation.worldFingerprint),
      targetJournal.actionRecords, restoredJournal.actionRecords,
      static_cast<unsigned long long>(
          rolledBackTarget.restoredWorldFingerprint));
  return true;
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

bool ExerciseCurrentKeyTranslationAgainstStaleComplement() {
  const auto exercisePair = [](const char* currentName,
                               const char* complementName,
                               int expectedAction) {
    const int currentCode = g_hardware.SearchCode(currentName);
    const int complementCode = g_hardware.SearchCode(complementName);
    const int currentVirtualKey = currentCode >= CTRL_EXTENDED_KEY
        ? currentCode - CTRL_EXTENDED_KEY : currentCode;
    const int complementVirtualKey = complementCode >= CTRL_EXTENDED_KEY
        ? complementCode - CTRL_EXTENDED_KEY : complementCode;
    if (currentCode <= 0 || complementCode <= 0 ||
        currentVirtualKey < 0 || currentVirtualKey >= 256 ||
        complementVirtualKey < 0 || complementVirtualKey >= 256 ||
        (GetAsyncKeyState(currentVirtualKey) & 0x8000) != 0 ||
        (GetAsyncKeyState(complementVirtualKey) & 0x8000) != 0) {
      return false;
    }

    BYTE savedKeyboardState[256] = {};
    BYTE staleKeyboardState[256] = {};
    if (GetKeyboardState(savedKeyboardState) == FALSE) return false;
    std::memcpy(staleKeyboardState, savedKeyboardState,
                sizeof(staleKeyboardState));
    staleKeyboardState[currentVirtualKey] = 0;
    // Deliberately leave the queue-local complement stale. Translation must
    // consult its physical asynchronous state and report the current key.
    staleKeyboardState[complementVirtualKey] = 0x80;
    if (SetKeyboardState(staleKeyboardState) == FALSE) return false;

    int actions[MAX_ACTIONS_ON_KEY * 3] = {};
    double values[MAX_ACTIONS_ON_KEY * 3] = {};
    const int pressed = g_hardware.m_ctrlTranslator.Translate(
        CTRL_BUTTONS_MSG, currentCode, TRUE, values, actions);
    double pressedValue = 0.0;
    bool pressedFound = false;
    for (int index = 0; index < pressed; ++index) {
      if (actions[index] == expectedAction) {
        pressedValue = values[index];
        pressedFound = true;
      }
    }

    std::memset(actions, 0, sizeof(actions));
    std::memset(values, 0, sizeof(values));
    const int released = g_hardware.m_ctrlTranslator.Translate(
        CTRL_BUTTONS_MSG, currentCode, FALSE, values, actions);
    double releasedValue = -1.0;
    bool releasedFound = false;
    for (int index = 0; index < released; ++index) {
      if (actions[index] == expectedAction) {
        releasedValue = values[index];
        releasedFound = true;
      }
    }
    const bool restored = SetKeyboardState(savedKeyboardState) != FALSE;
    return restored && pressedFound && pressedValue > 0.0 &&
           pressedValue <= 1.0 && releasedFound && releasedValue == 0.0;
  };

  return exercisePair("Right", "Left", TURN_RIGHT) &&
         exercisePair("D", "A", STRAFE_RIGHT);
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

bool ExerciseOccupiedVehicleContinuation() {
  SimulationContext* context = g_super.m_context;
  if (context == nullptr ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u)
    return false;
  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  Vehicle* vehicle = vehicleID.isNUL() ? nullptr : static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  SRecoveredVehicleRuntimeState occupied = {};
  if (vehicle == nullptr ||
      !VehicleRuntimeState_Inspect(context, vehicleID, &occupied))
    return false;
  // Levels without a usable Taxi leave the default body active. Their empty
  // embodiment is covered by the ordinary continuation path below.
  if (vehicle->taxiChangeEnabled()) return true;

  const KR_ObjectID occupiedAttribute = occupied.attribute;
  const bool panelReady = vehicle->panelReady();
  const bool panelOpen = vehicle->panelOpen();
  const int taxiCount = TaxiSubjectState_LiveCount();
  std::vector<std::uint8_t> continuation;
  SLevelContinuationSummary captured;
  if ((panelReady && !panelOpen) || taxiCount < 0 ||
      !RecoveredGameServices_CaptureLevelContinuation(
          &continuation, &captured) || !captured.ready)
    return false;

  const bool exitRequested = SendHardwareButton("F1", TRUE) &&
      SendHardwareButton("F1", FALSE);
  bool exited = false;
  for (int frame = 0; exitRequested && frame < 120; ++frame) {
    if (!RunVehicleFrameAfter(0.025) ||
        !VehicleRuntimeState_Inspect(context, vehicleID, &occupied))
      break;
    if (vehicle->taxiChangeEnabled()) {
      exited = true;
      break;
    }
  }

  SLevelContinuationSummary restored;
  const bool restoreAccepted = exited &&
      RecoveredGameServices_RestoreLevelContinuation(
          continuation, &restored);
  vehicleID = context->searchObject("Vehicle.Default");
  vehicle = vehicleID.isNUL() ? nullptr : static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  SRecoveredVehicleRuntimeState recovered = {};
  std::vector<std::uint8_t> recapturedBytes;
  SLevelContinuationSummary recaptured;
  const bool recoveredInspected = vehicle != nullptr &&
      VehicleRuntimeState_Inspect(context, vehicleID, &recovered);
  const bool recapturedState = restoreAccepted &&
      RecoveredGameServices_CaptureLevelContinuation(
          &recapturedBytes, &recaptured);
  const bool result = restoreAccepted && restored.ready &&
         recoveredInspected &&
         !vehicle->taxiChangeEnabled() &&
         recovered.attribute == occupiedAttribute &&
         vehicle->panelReady() == panelReady &&
         vehicle->panelOpen() == panelOpen &&
         TaxiSubjectState_LiveCount() == taxiCount &&
         RecoveredGameServices_VehicleControlReady() &&
         RecoveredGameServices_VehicleActiveActionCount() == 0u &&
         RecoveredGameServices_VehicleCameraMode() ==
             RECOVERED_VEHICLE_CAMERA_LIVE &&
         restored.worldFingerprint == captured.worldFingerprint &&
         restored.containerFingerprint == captured.containerFingerprint &&
         recapturedState && recaptured.ready &&
         recaptured.worldFingerprint == captured.worldFingerprint &&
         recaptured.containerFingerprint == captured.containerFingerprint;
  if (!result) {
    std::fprintf(
        stderr,
        "occupied continuation exit=%d restore=%d error=%s "
        "restored=%d inspected=%d attr=%d/%d panel=%d/%d -> %d/%d "
        "taxi=%d/%d control=%d actions=%u camera=%d "
        "fingerprints=%llu/%llu %llu/%llu recapture=%d/%d "
        "%llu/%llu\n",
        exited ? 1 : 0, restoreAccepted ? 1 : 0,
        RecoveredGameServices_LastLevelContinuationError(),
        restored.ready ? 1 : 0, recoveredInspected ? 1 : 0,
        recovered.attribute == occupiedAttribute ? 1 : 0,
        vehicle != nullptr && !vehicle->taxiChangeEnabled() ? 1 : 0,
        panelReady ? 1 : 0, panelOpen ? 1 : 0,
        vehicle != nullptr && vehicle->panelReady() ? 1 : 0,
        vehicle != nullptr && vehicle->panelOpen() ? 1 : 0,
        TaxiSubjectState_LiveCount(), taxiCount,
        RecoveredGameServices_VehicleControlReady() ? 1 : 0,
        RecoveredGameServices_VehicleActiveActionCount(),
        RecoveredGameServices_VehicleCameraMode(),
        static_cast<unsigned long long>(restored.worldFingerprint),
        static_cast<unsigned long long>(captured.worldFingerprint),
        static_cast<unsigned long long>(restored.containerFingerprint),
        static_cast<unsigned long long>(captured.containerFingerprint),
        recapturedState ? 1 : 0, recaptured.ready ? 1 : 0,
        static_cast<unsigned long long>(recaptured.worldFingerprint),
        static_cast<unsigned long long>(recaptured.containerFingerprint));
  }
  return result;
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
  const int explosionCount = ExplosionSubjectState_LiveCount();
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

  // The unsafe exit owns a real, falling Orphan between the F1 transition and
  // its terminal impact.  This is exactly the save boundary that used to be
  // absent from LCN1: capture both the owner payload and the complete
  // continuation while the private moving event is pending.
  KR_ObjectID sourceOrphan = OrphanSubjectState_FirstObject(context);
  std::vector<unsigned char> orphanState;
  std::vector<std::uint8_t> continuation;
  SLevelContinuationSummary captured;
  unsigned long long sourceOrphanFingerprint = 0;
  bool continuationCaptured = false;
  // Other real Level-local owners may be between their own canonical states
  // on the exact F1 frame (Level.01D/01N commonly have a live Bullet). Keep
  // advancing complete frames while the Orphan is still falling and capture
  // the first boundary at which the entire world is serializable.
  for (int boundaryFrame = 0; boundaryFrame < 48; ++boundaryFrame) {
    orphanState.clear();
    continuation.clear();
    captured = SLevelContinuationSummary();
    sourceOrphanFingerprint =
        OrphanActiveWorldState_Fingerprint(context);
    continuationCaptured = sourceOrphanFingerprint != 0 &&
        OrphanActiveWorldState_CaptureStable(context, &orphanState) &&
        OrphanActiveWorldState_SchedulerEventCount(orphanState) == 1 &&
        OrphanActiveWorldState_MatchesStable(context, orphanState) &&
        RecoveredGameServices_CaptureLevelContinuation(
            &continuation, &captured) && captured.ready &&
        captured.sections == 14;
    if (continuationCaptured) break;
    if (OrphanSubjectState_LiveCount() != orphanCount + 1 ||
        !RunVehicleFrameAfter(0.025) ||
        !RecoveredGameServices_VehicleEmbodimentTelemetry(&dropped))
      break;
  }
  if (sourceOrphan.isNUL() || !continuationCaptured) {
    std::fprintf(stderr,
                 "ORP1 live capture failed owner=%d fingerprint=%llu "
                 "bytes=%zu events=%d matches=%d continuation=%d/%d "
                 "sections=%d orphan_error=%s level_error=%s\n",
                 sourceOrphan.isNUL() ? 0 : 1,
                 sourceOrphanFingerprint, orphanState.size(),
                 OrphanActiveWorldState_SchedulerEventCount(orphanState),
                 OrphanActiveWorldState_MatchesStable(context, orphanState)
                     ? 1 : 0,
                 continuation.empty() ? 0 : 1, captured.ready ? 1 : 0,
                 captured.sections,
                 OrphanActiveWorldState_LastFailure(),
                 RecoveredGameServices_LastLevelContinuationError());
    return false;
  }

  // Advance enough complete frames to make the falling body observably
  // divergent, while deliberately remaining before its impact.  This leaves
  // both the source and replacement worlds at admissible ORP1 boundaries.
  for (int frame = 0; frame < 8; ++frame)
    if (!RunVehicleFrameAfter(0.025) ||
        !RecoveredGameServices_VehicleEmbodimentTelemetry(&dropped))
      return false;
  if (OrphanSubjectState_LiveCount() != orphanCount + 1 ||
      dropped.orphanMoveEvents <= before.orphanMoveEvents ||
      OrphanActiveWorldState_Fingerprint(context) ==
          sourceOrphanFingerprint)
    return false;

  // Restore the whole post-exit boundary, not a synthetic Orphan-only
  // fixture.  The reconstructed object must carry byte-identical ORP1 state,
  // the exact LCN1 fingerprint, and resume its authentic impact lifecycle.
  SLevelContinuationSummary restored;
  const bool continuationRestored =
      RecoveredGameServices_RestoreLevelContinuation(
          continuation, &restored);
  const bool orphanRestored =
      OrphanActiveWorldState_MatchesStable(context, orphanState);
  if (!continuationRestored || !restored.ready ||
      restored.worldFingerprint != captured.worldFingerprint ||
      restored.containerFingerprint != captured.containerFingerprint ||
      OrphanSubjectState_LiveCount() != orphanCount + 1 ||
      OrphanActiveWorldState_Fingerprint(context) !=
          sourceOrphanFingerprint ||
      !orphanRestored) {
    std::fprintf(stderr,
                 "ORP1 continuation restore failed accepted=%d ready=%d "
                 "world=%llu/%llu container=%llu/%llu live=%d/%d "
                 "fingerprint=%llu/%llu matches=%d orphan_error=%s "
                 "level_error=%s\n",
                 continuationRestored ? 1 : 0, restored.ready ? 1 : 0,
                 static_cast<unsigned long long>(restored.worldFingerprint),
                 static_cast<unsigned long long>(captured.worldFingerprint),
                 static_cast<unsigned long long>(
                     restored.containerFingerprint),
                 static_cast<unsigned long long>(
                     captured.containerFingerprint),
                 OrphanSubjectState_LiveCount(), orphanCount + 1,
                 OrphanActiveWorldState_Fingerprint(context),
                 sourceOrphanFingerprint, orphanRestored ? 1 : 0,
                 OrphanActiveWorldState_LastFailure(),
                 RecoveredGameServices_LastLevelContinuationError());
    return false;
  }
  KR_ObjectID reconstructedOrphan =
      OrphanSubjectState_FirstObject(context);
  if (reconstructedOrphan.isNUL() || reconstructedOrphan == sourceOrphan) {
    std::fprintf(stderr,
                 "ORP1 reconstructed owner identity was not replaced\n");
    return false;
  }

  std::vector<std::uint8_t> recapturedBytes;
  SLevelContinuationSummary recaptured;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &recapturedBytes, &recaptured) || !recaptured.ready ||
      recaptured.worldFingerprint != captured.worldFingerprint ||
      recaptured.containerFingerprint != captured.containerFingerprint) {
    std::fprintf(stderr,
                 "ORP1 recapture failed ready=%d world=%llu/%llu "
                 "container=%llu/%llu error=%s\n",
                 recaptured.ready ? 1 : 0,
                 static_cast<unsigned long long>(recaptured.worldFingerprint),
                 static_cast<unsigned long long>(captured.worldFingerprint),
                 static_cast<unsigned long long>(
                     recaptured.containerFingerprint),
                 static_cast<unsigned long long>(
                     captured.containerFingerprint),
                 RecoveredGameServices_LastLevelContinuationError());
    return false;
  }

  bool resumedImpact = false;
  bool retailExplosionAccepted = false;
  const unsigned int restoredMoveBaseline = dropped.orphanMoveEvents;
  const unsigned int restoredImpactBaseline = dropped.orphanImpacts;
  const unsigned int restoredExplosionBaseline = dropped.orphanExplosions;
  for (int frame = 0; frame < 320; ++frame) {
    if (!RunVehicleFrameAfter(0.025) ||
        !RecoveredGameServices_VehicleEmbodimentTelemetry(&dropped))
      return false;
    if (OrphanSubjectState_LiveCount() == orphanCount &&
        dropped.orphanMoveEvents > restoredMoveBaseline &&
        dropped.orphanImpacts > restoredImpactBaseline) {
      resumedImpact = true;
      std::vector<unsigned char> impactExplosionState;
      retailExplosionAccepted =
          dropped.orphanExplosions == restoredExplosionBaseline + 1 &&
          ExplosionActiveWorldState_CaptureStable(
              context, &impactExplosionState) &&
          ExplosionActiveWorldState_SchedulerEventCount(
              impactExplosionState) >=
              ExplosionSubjectState_LiveCount() &&
          ExplosionActiveWorldState_Fingerprint(context) != 0;
      break;
    }
  }
  const bool result = resumedImpact && retailExplosionAccepted &&
         dropped.liveOrphans == 0 &&
         OrphanSubjectState_LiveCount() == orphanCount &&
         dropped.hardwareSubscriptionPreserved == 1;
  if (!result)
    std::fprintf(stderr,
                 "ORP1 resumed impact failed resumed=%d explosion=%d "
                 "explosion_live=%d/%d explosion_events=%u/%u live=%u/%d "
                 "moves=%u/%u impacts=%u/%u subscription=%d\n",
                 resumedImpact ? 1 : 0,
                 retailExplosionAccepted ? 1 : 0,
                 ExplosionSubjectState_LiveCount(), explosionCount,
                 dropped.orphanExplosions, restoredExplosionBaseline,
                 dropped.liveOrphans, orphanCount,
                 dropped.orphanMoveEvents, restoredMoveBaseline,
                 dropped.orphanImpacts, restoredImpactBaseline,
                 dropped.hardwareSubscriptionPreserved);
  return result;
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

bool VisibleProbeAboveViewTerrain(double clearance, CFVector3* position) {
  if (position == nullptr || !std::isfinite(clearance) || clearance <= 0.0)
    return false;
  *position = CViewObject::m_viewPointInvMx.Offset();
  if (!std::isfinite(position->x) || !std::isfinite(position->y) ||
      !std::isfinite(position->z))
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

bool OpenExplosionSaveBoundary(CViewDynamicList* dynamics,
                               KR_ObjectID* explosion) {
  if (dynamics == nullptr || explosion == nullptr ||
      g_super.m_context == nullptr) {
    return false;
  }
  *explosion = KR_ObjectID::NUL();
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
  const int attributeIndex =
      attributeTable == ct_NULLID || attributeID.isNUL()
          ? -1
          : g_arena.getAttributeIndex(attributeTable, attributeID);
  CFVector3 position;
  static const char kProbeName[] =
      "Explosion.SaveBoundary.Retry.Probe";
  const double clearance = attribute == nullptr
      ? 0.0
      : (attribute->m_radiusDamage > 0.0
             ? attribute->m_radiusDamage + 64.0
             : 64.0);
  if (attribute == nullptr || attributeIndex == -1 ||
      subjectTable == ct_NULLID ||
      !VisibleProbeAboveViewTerrain(clearance, &position) ||
      context->isExist(kProbeName)) {
    std::fprintf(stderr,
                 "explosion boundary setup attr=%s object=%d index=%d "
                 "table=%d clearance=%.3f haze=%.3f duplicate=%d\n",
                 attributeName == nullptr ? "<none>" : attributeName,
                 attribute != nullptr ? 1 : 0, attributeIndex,
                 subjectTable == ct_NULLID ? 0 : 1, clearance,
                 CViewFigure::HazeMax(), context->isExist(kProbeName) ? 1 : 0);
    return false;
  }

  const double currentTime = Session::m_moment;
  const double timeStamp =
      !std::isfinite(currentTime) || currentTime < 0.1
          ? 0.1
          : currentTime;
  const ExplosionImpactRequest request = {
      position, timeStamp, KR_ObjectID::NUL(), subjectTable,
      attributeIndex, kProbeName};
  int damageApplications = -1;
  const bool executed = ExplosionSubjectState_ExecuteNow(
      context, request, &damageApplications);
  if (!executed || damageApplications != 0) {
    const CFVector3 view = CViewObject::m_viewPointInvMx.Offset();
    std::fprintf(stderr,
                 "explosion boundary execute=%d damage=%d radius=%.3f "
                 "damage_radius=%.3f haze=%.3f distance=%.3f "
                 "position=%.3f/%.3f/%.3f view=%.3f/%.3f/%.3f\n",
                 executed ? 1 : 0, damageApplications, attribute->m_radius,
                 attribute->m_radiusDamage, CViewFigure::HazeMax(),
                 Abs(position - view), position.x, position.y, position.z,
                 view.x, view.y, view.z);
    return false;
  }
  *explosion = context->searchObject(kProbeName);
  std::vector<unsigned char> stable;
  if (explosion->isNUL() ||
      !ExplosionActiveWorldState_CaptureStable(context, &stable)) {
    std::fprintf(stderr,
                 "explosion boundary stable=0 owner=%d error=%s\n",
                 explosion->isNUL() ? 0 : 1,
                 ExplosionActiveWorldState_LastFailure());
    if (!explosion->isNUL() && context->isExist(*explosion))
      context->removeObject(*explosion);
    *explosion = KR_ObjectID::NUL();
    return false;
  }

  g_arena.render(CViewObject::m_viewPointInvMx.Offset(),
                 CViewFigure::HazeMax(), *dynamics);
  std::vector<unsigned char> rejected;
  const bool rejectedCapture =
      !ExplosionActiveWorldState_CaptureStable(context, &rejected);
  const std::string rejectedReason =
      ExplosionActiveWorldState_LastFailure();
  const bool rejectedAtOpenBoundary =
      rejectedCapture && rejectedReason.find("frame") != std::string::npos;
  if (!rejectedAtOpenBoundary) {
    std::fprintf(stderr,
                 "explosion boundary render did not open frame: distance=%.3f "
                 "haze=%.3f error=%s\n",
                 Abs(position - CViewObject::m_viewPointInvMx.Offset()),
                 CViewFigure::HazeMax(),
                 rejectedReason.c_str());
    g_arena.endRender(ZAV_Scene());
    dynamics->Clear(FALSE);
    ExplosionSubjectState_ReleaseLightFrame();
    if (context->isExist(*explosion)) context->removeObject(*explosion);
    *explosion = KR_ObjectID::NUL();
    return false;
  }
  return true;
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
  // This visual fixture owns its synthetic scheduler boundary. Remove both
  // records before the first host-timed frame so a slow Debug draw cannot
  // advance and expire the parent before its one required visible frame.
  ownedMove = context->removeEvent(EXPLOSION_MOVE, explosion) != 0;
  ownedPuff = context->removeEvent(EXPLOSION_NEWPUFF, explosion) != 0;
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
  // The draw proof owns this synthetic parent's event boundary. Detach both
  // scheduler records before yielding to a host-timed frame; a slow Debug
  // frame may otherwise execute either record and turn cleanup into a race
  // against wall-clock time.
  parentMoveDetached =
      context->removeEvent(EXPLOSION_MOVE, explosion) != 0;
  parentPuffDetached =
      context->removeEvent(EXPLOSION_NEWPUFF, explosion) != 0;
  {
    ScopedAlphaSpriteCapture capture(smokeAttribute->m_cacheImage);
    visibleFrame = RecoveredGameServices_RunFrame() != FALSE;
    visibleDraws = g_alphaSpriteDraws;
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

bool ExerciseTaxiDebugCatalogAndSpawn(SimulationContext* context,
                                      const KR_ObjectID& vehicleID) {
  g_taxiDebugGroundingProbe = {};
  SRecoveredVehicleRuntimeState vehicle = {};
  std::vector<STaxiDebugVehicleType> catalog;
  std::string failure;
  if (!VehicleRuntimeState_Inspect(context, vehicleID, &vehicle) ||
      !TaxiSubjectState_DebugVehicleCatalog(context, &catalog, &failure) ||
      catalog.empty()) {
    return false;
  }
  for (std::size_t index = 1; index < catalog.size(); ++index) {
    if (catalog[index - 1].taxiAttribute >= catalog[index].taxiAttribute)
      return false;
  }

  const int baselineCount = TaxiSubjectState_LiveCount();
  const int baselineSounds = TaxiSubjectState_SoundCount();
  const unsigned long long baselineFingerprint =
      TaxiSubjectState_Fingerprint(context);
  KR_ObjectID spawned = KR_ObjectID::NUL();
  STaxiDebugSpawnPlacement placement;
  if (TaxiSubjectState_DebugSpawn(
          context, "Taxi.Attr.Debug.Missing", "Debug.Taxi.Invalid",
          vehicle.position + CFVector3(0.0, 24.0, 16.0), 0.0,
          (std::max)(0.1, vehicle.lastTime), &spawned, &placement,
          &failure) ||
      !spawned.isNUL() || TaxiSubjectState_LiveCount() != baselineCount ||
      TaxiSubjectState_Fingerprint(context) != baselineFingerprint) {
    return false;
  }

  for (std::size_t index = 0; index < catalog.size(); ++index) {
    char objectName[64] = {};
    std::snprintf(objectName, sizeof(objectName),
                  "Debug.Taxi.Surface.%04u",
                  static_cast<unsigned int>(index + 1u));
    const CFVector3 requested =
        vehicle.position + CFVector3(0.0, 24.0, 16.0);
    spawned = KR_ObjectID::NUL();
    placement = STaxiDebugSpawnPlacement();
    if (!TaxiSubjectState_DebugSpawn(
            context, catalog[index].taxiAttribute.c_str(), objectName,
            requested, static_cast<double>(index) * 0.125,
            (std::max)(0.1, vehicle.lastTime), &spawned, &placement,
            &failure) ||
        spawned.isNUL() || !context->isExist(spawned) ||
        TaxiSubjectState_LiveCount() != baselineCount + 1 ||
        placement.ready == 0 ||
        placement.sweepHit + placement.terrainFallback != 1 ||
        placement.bumpKind == BF_NONE ||
        std::fabs(Abs(placement.surfaceNormal) - 1.0) > 1.0e-6 ||
        placement.surfaceNormal.y < 0.05 ||
        std::fabs(placement.modelBottomClearance) > 1.0e-6) {
      if (!spawned.isNUL() && context->isExist(spawned))
        context->removeObject(spawned);
      return false;
    }
    double drift = 0.0;
    if (!TaxiSubjectState_DebugPlacementDrift(
            context, objectName, placement.resolvedPosition, &drift) ||
        drift > 1.0e-6) {
      context->removeObject(spawned);
      return false;
    }
    ++g_taxiDebugGroundingProbe.types;
    g_taxiDebugGroundingProbe.sweepHits += placement.sweepHit;
    g_taxiDebugGroundingProbe.terrainFallbacks +=
        placement.terrainFallback;
    g_taxiDebugGroundingProbe.maxBottomClearance =
        (std::max)(g_taxiDebugGroundingProbe.maxBottomClearance,
                   std::fabs(placement.modelBottomClearance));
    g_taxiDebugGroundingProbe.maxImmediateDrift =
        (std::max)(g_taxiDebugGroundingProbe.maxImmediateDrift, drift);

    if (index == 0) {
      KR_ObjectID duplicate = KR_ObjectID::NUL();
      STaxiDebugSpawnPlacement duplicatePlacement;
      if (TaxiSubjectState_DebugSpawn(
              context, catalog[index].taxiAttribute.c_str(), objectName,
              requested + CFVector3(0.0, 0.0, 2.0), 0.0,
              (std::max)(0.1, vehicle.lastTime), &duplicate,
              &duplicatePlacement, &failure) ||
          !duplicate.isNUL() ||
          TaxiSubjectState_LiveCount() != baselineCount + 1) {
        context->removeObject(spawned);
        return false;
      }
    }
    context->removeObject(spawned);
    if (TaxiSubjectState_LiveCount() != baselineCount ||
        TaxiSubjectState_SoundCount() != baselineSounds ||
        TaxiSubjectState_Fingerprint(context) != baselineFingerprint)
      return false;
  }
  return g_taxiDebugGroundingProbe.types ==
             static_cast<int>(catalog.size()) &&
         g_taxiDebugGroundingProbe.sweepHits +
                 g_taxiDebugGroundingProbe.terrainFallbacks ==
             g_taxiDebugGroundingProbe.types;
}

bool ExerciseDebugMenuStableBoundaryRetry(SimulationContext* context) {
  const bool configured = context != nullptr &&
      RecoveredGameServices_ConfigureDebugMenu(
          true, std::vector<std::string>{"Level.Debug"});
  if (!configured) {
    std::fprintf(stderr, "debug menu retry: configuration failed: %s\n",
                 RecoveredGameServices_DebugMenuState() == nullptr
                     ? "state unavailable"
                     : RecoveredGameServices_DebugMenuState()
                           ->lastError.c_str());
    return false;
  }
  const int baselineTaxiCount = TaxiSubjectState_LiveCount();
  const int baselineTaxiSounds = TaxiSubjectState_SoundCount();
  const unsigned long long baselineTaxiFingerprint =
      TaxiSubjectState_Fingerprint(context);
  std::vector<unsigned char> baselineExplosionState;
  CViewDynamicList openFrameDynamics;
  KR_ObjectID openFrameExplosion = KR_ObjectID::NUL();
  const bool boundaryOpened = baselineTaxiFingerprint != 0 &&
      ExplosionActiveWorldState_CaptureStable(
          context, &baselineExplosionState) &&
      OpenExplosionSaveBoundary(&openFrameDynamics, &openFrameExplosion);
  const bool requested = boundaryOpened &&
      RecoveredGameServices_RequestDebugVehicleSpawn(0u, false);
  if (!boundaryOpened || !requested) {
    std::fprintf(stderr,
                 "debug menu retry: setup failed fingerprint=%llu "
                 "opened=%d requested=%d explosion_live=%d error=%s\n",
                 baselineTaxiFingerprint, boundaryOpened ? 1 : 0,
                 requested ? 1 : 0, ExplosionSubjectState_LiveCount(),
                 RecoveredGameServices_DebugMenuState() == nullptr
                     ? "state unavailable"
                     : RecoveredGameServices_DebugMenuState()
                           ->lastError.c_str());
    if (!openFrameExplosion.isNUL()) {
      g_arena.endRender(ZAV_Scene());
      openFrameDynamics.Clear(FALSE);
      ExplosionSubjectState_ReleaseLightFrame();
      if (context->isExist(openFrameExplosion))
        context->removeObject(openFrameExplosion);
    }
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }

  const bool rejectedAtOpenBoundary =
      !RecoveredGameServices_ProcessPendingDebugCommand();
  const SRecoveredDebugMenuState* deferred =
      RecoveredGameServices_DebugMenuState();
  const bool retainedForRetry =
      rejectedAtOpenBoundary && deferred != nullptr && deferred->pending &&
      deferred->pendingAction == RECOVERED_DEBUG_MENU_SPAWN_VEHICLE &&
      deferred->pendingIndex == 0u && deferred->pendingAttempts == 1u &&
      deferred->deferredCommands == 1u &&
      deferred->lastCommandAttempts == 0u &&
      deferred->requests == 1u && deferred->completedCommands == 0u &&
      deferred->failedCommands == 0u &&
      deferred->lastError.find("frame") != std::string::npos;

  g_arena.endRender(ZAV_Scene());
  openFrameDynamics.Clear(FALSE);
  ExplosionSubjectState_ReleaseLightFrame();
  std::vector<unsigned char> closedExplosionState;
  const bool boundaryClosed = ExplosionActiveWorldState_CaptureStable(
      context, &closedExplosionState);
  const unsigned long long closedTaxiFingerprint =
      TaxiSubjectState_Fingerprint(context);
  const bool completedAfterRetry =
      retainedForRetry && boundaryClosed && closedTaxiFingerprint != 0 &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  const SRecoveredDebugMenuState* completed =
      RecoveredGameServices_DebugMenuState();
  const std::string spawnedName =
      completed == nullptr ? std::string() : completed->lastObject;
  KR_ObjectID spawned = spawnedName.empty()
                            ? KR_ObjectID::NUL()
                            : context->searchObject(spawnedName.c_str());
  const bool completionState =
      completedAfterRetry && completed != nullptr && !completed->pending &&
      completed->pendingAttempts == 0u &&
      completed->deferredCommands == 1u &&
      completed->lastCommandAttempts == 2u &&
      completed->completedCommands == 1u &&
      completed->failedCommands == 0u &&
      completed->spawnedVehicles == 1u &&
      completed->rollbackAttempts == 0u && !spawned.isNUL() &&
      context->isExist(spawned);

  if (!spawned.isNUL() && context->isExist(spawned))
    context->removeObject(spawned);
  if (!openFrameExplosion.isNUL() && context->isExist(openFrameExplosion))
    context->removeObject(openFrameExplosion);

  const bool restored =
      TaxiSubjectState_LiveCount() == baselineTaxiCount &&
      TaxiSubjectState_SoundCount() == baselineTaxiSounds &&
      TaxiSubjectState_Fingerprint(context) == closedTaxiFingerprint &&
      ExplosionActiveWorldState_MatchesStable(
          context, baselineExplosionState);
  const bool disabled = RecoveredGameServices_ConfigureDebugMenu(
      false, std::vector<std::string>());
  if (!completionState || !restored || !disabled) {
    std::fprintf(
        stderr,
        "debug menu retry: retained=%d closed=%d completed=%d state=%d "
        "restored=%d disabled=%d name=%s taxi=%d/%d sound=%d/%d "
        "fingerprint=%llu/%llu explosion=%d\n",
        retainedForRetry ? 1 : 0, boundaryClosed ? 1 : 0,
        completedAfterRetry ? 1 : 0, completionState ? 1 : 0,
        restored ? 1 : 0, disabled ? 1 : 0, spawnedName.c_str(),
        TaxiSubjectState_LiveCount(), baselineTaxiCount,
        TaxiSubjectState_SoundCount(), baselineTaxiSounds,
        TaxiSubjectState_Fingerprint(context), closedTaxiFingerprint,
        ExplosionSubjectState_LiveCount());
  }
  return completionState && restored && disabled;
}

bool ExerciseDebugDeathLifecycle(SimulationContext* context) {
  if (context == nullptr ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u)
    return false;
  KR_ObjectID initialVehicle =
      context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState initialState = {};
  const int baselineCorpses = CorpseSubjectState_LiveCount();
  const unsigned int baselineSuppressed =
      RecoveredGameServices_VehicleSuppressedInputCount();
  std::vector<std::uint8_t> baselineContinuation;
  SLevelContinuationSummary baselineSummary;
  const bool baselineState = !initialVehicle.isNUL() &&
      baselineCorpses >= 0 && VehicleRuntimeState_Inspect(
          context, initialVehicle, &initialState) &&
      !initialState.dead && !initialState.takingTaxi;
  const bool baselineCaptured = baselineState &&
      RecoveredGameServices_CaptureLevelContinuation(
          &baselineContinuation, &baselineSummary) &&
      baselineSummary.ready && baselineSummary.worldFingerprint != 0 &&
      baselineSummary.containerFingerprint != 0;
  const bool configured = baselineCaptured &&
      RecoveredGameServices_ConfigureDebugMenu(
          true, std::vector<std::string>{"Level.Debug.Death"});
  const bool requested = configured &&
      RecoveredGameServices_RequestDebugKillPlayer();
  const bool processed = requested &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  if (!processed) {
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }
  KR_ObjectID deadVehicle =
      context->searchObject("Vehicle.Default");
  Vehicle* deadObject = deadVehicle.isNUL()
                            ? nullptr
                            : static_cast<Vehicle*>(
                                  context->queryInterface(
                                      deadVehicle, IVehicleIID));
  SRecoveredVehicleRuntimeState deadState = {};
  SRecoveredVehicleCameraTelemetry deadCamera = {};
  const SRecoveredDebugMenuState* killed =
      RecoveredGameServices_DebugMenuState();
  std::vector<std::uint8_t> deadContinuation;
  SLevelContinuationSummary deadSummary;
  const bool deadInspected = deadObject != nullptr &&
      VehicleRuntimeState_Inspect(context, deadVehicle, &deadState);
  KR_ObjectID control = context->searchObject("RecoveredVehicleControl");
  KR_Event deadInput;
  deadInput.source = context->searchObject("Hardware");
  deadInput.destination = control;
  deadInput.label = CTRL_BUTTONS_MSG;
  deadInput.timeStamp = deadState.lastTime;
  deadInput.data.open(EDO_WRITE)
      .putInt(MOVE_FORWARD)
      .putDouble(1.0)
      .putInt(0)
      .putInt(FALSE)
      .close();
  if (deadInspected && !control.isNUL() && !deadInput.source.isNUL())
    context->sendEventNow(deadInput);
  const bool deadInputSuppressed = deadInspected && !control.isNUL() &&
      !deadInput.source.isNUL() &&
      RecoveredGameServices_VehicleSuppressedInputCount() ==
          baselineSuppressed + 1u &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u;
  const bool killedState = deadInspected &&
      deadInputSuppressed &&
      deadState.dead && deadState.takingTaxi &&
      !deadObject->panelOpen() &&
      CorpseSubjectState_LiveCount() == baselineCorpses + 1 &&
      VehicleRuntimeState_InspectCamera(context, &deadCamera) &&
      deadCamera.mode == RECOVERED_VEHICLE_CAMERA_DEATH_ASCENT &&
      deadCamera.deathFrames == 1u &&
      killed != nullptr && killed->preDeathCheckpointAvailable &&
      killed->requests == 1u && killed->completedCommands == 1u &&
      killed->failedCommands == 0u && killed->forcedDeaths == 1u &&
      killed->deathCorpseCreations == 1u &&
      killed->deathCameraProofs == 1u &&
      killed->deathSaveProofs == 1u &&
      killed->restoredPreDeathCheckpoints == 0u &&
      killed->deathWorldFingerprint != 0 &&
      killed->deathContinuationFingerprint != 0 &&
      RecoveredGameServices_CaptureLevelContinuation(
          &deadContinuation, &deadSummary) &&
      deadSummary.ready &&
      deadSummary.worldFingerprint == killed->deathWorldFingerprint &&
      deadSummary.containerFingerprint ==
          killed->deathContinuationFingerprint;
  if (!killedState) {
    RecoveredGameServices_RequestDebugRestorePreDeath();
    RecoveredGameServices_ProcessPendingDebugCommand();
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }
  SLevelContinuationSummary reconstructedDead;
  const bool deadRoundTrip =
      RecoveredGameServices_RestoreLevelContinuation(
          deadContinuation, &reconstructedDead);
  KR_ObjectID reconstructedVehicle =
      context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState reconstructedState = {};
  if (!deadRoundTrip || !reconstructedDead.ready ||
      reconstructedVehicle.isNUL() ||
      !VehicleRuntimeState_Inspect(
          context, reconstructedVehicle, &reconstructedState) ||
      !reconstructedState.dead || !reconstructedState.takingTaxi ||
      CorpseSubjectState_LiveCount() != baselineCorpses + 1 ||
      !RecoveredGameServices_RequestDebugRestorePreDeath() ||
      !RecoveredGameServices_ProcessPendingDebugCommand()) {
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }
  KR_ObjectID restoredVehicle =
      context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState restoredState = {};
  std::vector<std::uint8_t> restoredContinuation;
  SLevelContinuationSummary restoredSummary;
  const SRecoveredDebugMenuState* restored =
      RecoveredGameServices_DebugMenuState();
  const bool restoredStateValid = restoredVehicle == initialVehicle &&
      VehicleRuntimeState_Inspect(
          context, restoredVehicle, &restoredState) &&
      !restoredState.dead && !restoredState.takingTaxi &&
      CorpseSubjectState_LiveCount() == baselineCorpses &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u &&
      RecoveredGameServices_CaptureLevelContinuation(
          &restoredContinuation, &restoredSummary) &&
      restoredSummary.ready &&
      restoredSummary.worldFingerprint == baselineSummary.worldFingerprint &&
      restoredSummary.containerFingerprint ==
          baselineSummary.containerFingerprint &&
      restored != nullptr && !restored->preDeathCheckpointAvailable &&
      restored->requests == 2u && restored->completedCommands == 2u &&
      restored->failedCommands == 0u && restored->forcedDeaths == 1u &&
      restored->restoredPreDeathCheckpoints == 1u &&
      restored->rollbackAttempts == 0u &&
      restored->lastAction == "restore-pre-death";
  const bool disabled = RecoveredGameServices_ConfigureDebugMenu(
      false, std::vector<std::string>());
  return restoredStateValid && disabled;
}

struct SDebugVehicleDestructionCoverage {
  unsigned int eligibleTypes = 0;
  unsigned int representativeProfiles = 0;
  unsigned int roundTrips = 0;
  unsigned int profileMask = 0;
  unsigned int gameplayProfiles = 0;
  unsigned int primaryProofs = 0;
  unsigned int secondaryProofs = 0;
  unsigned int armedPrimaryProfiles = 0;
  unsigned int armedSecondaryProfiles = 0;
  unsigned int damageProofs = 0;
  unsigned int hudProofs = 0;
  unsigned int hudProfiles = 0;
  unsigned int hudlessProfiles = 0;
  unsigned int gameplayRoundTrips = 0;
  unsigned int gameplayRestoreDeferrals = 0;
};

bool ExerciseDebugOccupiedVehicleDestructionOne(
    SimulationContext* context, std::size_t occupiedIndex,
    int expectedProfile, SDebugVehicleDestructionCoverage* coverage) {
  if (context == nullptr ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u ||
      !RecoveredGameServices_ConfigureDebugMenu(
          true, std::vector<std::string>{"Level.Debug.VehicleDeath"}) ||
      RecoveredGameServices_DebugVehicleTypeCount() == 0u) {
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }

  SRecoveredDebugVehicleType occupiedType;
  const bool selectionValid =
      occupiedIndex < RecoveredGameServices_DebugVehicleTypeCount() &&
      RecoveredGameServices_DebugVehicleType(occupiedIndex, &occupiedType) &&
      occupiedType.vehicleType == 1 &&
      occupiedType.vesselProfile == expectedProfile &&
      occupiedType.dynamic ==
          VehicleRuntimeState_VesselProfileName(expectedProfile);
  const bool spawnRequested = selectionValid &&
      RecoveredGameServices_RequestDebugVehicleSpawn(occupiedIndex, true);
  const bool spawnProcessed = spawnRequested &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  if (!selectionValid || !spawnRequested || !spawnProcessed) {
    const SRecoveredDebugMenuState* state =
        RecoveredGameServices_DebugMenuState();
    std::fprintf(stderr,
                 "debug Vehicle profile setup index=%zu count=%zu "
                 "selection=%d request=%d process=%d taxi=%s vehicle=%s "
                 "dynamic=%s profile=%d/%d action=%s error=%s\n",
                 occupiedIndex,
                 RecoveredGameServices_DebugVehicleTypeCount(),
                 selectionValid ? 1 : 0, spawnRequested ? 1 : 0,
                 spawnProcessed ? 1 : 0,
                 occupiedType.taxiAttribute.c_str(),
                 occupiedType.vehicleAttribute.c_str(),
                 occupiedType.dynamic.c_str(), occupiedType.vesselProfile,
                 expectedProfile,
                 state == nullptr ? "<none>" : state->lastAction.c_str(),
                 state == nullptr ? "<none>" : state->lastError.c_str());
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }

  KR_ObjectID occupiedVehicle = context->searchObject("Vehicle.Default");
  Vehicle* occupiedObject = occupiedVehicle.isNUL()
                                ? nullptr
                                : static_cast<Vehicle*>(
                                      context->queryInterface(
                                          occupiedVehicle, IVehicleIID));
  SRecoveredVehicleRuntimeState occupiedState = {};
  const bool occupiedPanelReady =
      occupiedObject != nullptr && occupiedObject->panelReady();
  const bool occupiedPanelOpen =
      occupiedObject != nullptr && occupiedObject->panelOpen();
  const int orphanBaseline = OrphanSubjectState_LiveCount();
  std::vector<std::uint8_t> occupiedContinuation;
  SLevelContinuationSummary occupiedSummary;
  const bool occupied = occupiedObject != nullptr &&
      VehicleRuntimeState_Inspect(
          context, occupiedVehicle, &occupiedState) &&
      !occupiedState.dead && !occupiedState.takingTaxi &&
      !occupiedObject->taxiChangeEnabled() &&
      occupiedPanelOpen == occupiedPanelReady &&
      orphanBaseline >= 0 &&
      RecoveredGameServices_CaptureLevelContinuation(
          &occupiedContinuation, &occupiedSummary) && occupiedSummary.ready;
  AttributeVehicle* occupiedAttribute = !occupied
      ? nullptr
      : static_cast<AttributeVehicle*>(
            __attrVehicleTable.searchAttribute(occupiedState.attribute));
  const bool expectsHud = occupiedAttribute != nullptr &&
      occupiedAttribute->m_panelName[0] != '\0';
  const bool hasPrimaryWeapon = occupiedAttribute != nullptr &&
      occupiedAttribute->m_bulletAttrName[0] != '\0';
  const bool hasSecondaryWeapon = occupiedAttribute != nullptr &&
      occupiedAttribute->m_bulletSecAttrName[0] != '\0';
  const bool weaponReferencesValid = occupiedAttribute != nullptr &&
      ((hasPrimaryWeapon && occupiedAttribute->m_bulletAttrIndex >= 0) ||
       (!hasPrimaryWeapon && occupiedAttribute->m_bulletAttrIndex < 0)) &&
      ((hasSecondaryWeapon &&
        occupiedAttribute->m_bulletSecAttrIndex >= 0) ||
       (!hasSecondaryWeapon &&
        occupiedAttribute->m_bulletSecAttrIndex < 0));
  BulletRuntimeTelemetry bulletsBefore = {};
  const double damageBefore =
      occupiedObject == nullptr ? 0.0 : occupiedObject->m_damage;
  const int secondaryAmmoBefore =
      occupiedObject == nullptr ? 0 : occupiedObject->m_secBulletCnt;
  bool gameplayValid = occupied && coverage != nullptr &&
      occupiedAttribute != nullptr && occupiedAttribute->m_type == 1 &&
      occupiedAttribute->m_dynamic[0] != '\0' &&
      weaponReferencesValid &&
      occupiedPanelReady == expectsHud && occupiedPanelOpen == expectsHud &&
      std::isfinite(damageBefore) && damageBefore > 0.0 &&
      (!hasSecondaryWeapon || secondaryAmmoBefore > 0) &&
      BulletSubjectState_OwnerRuntimeTelemetry(
          context, "Vehicle.Default", &bulletsBefore);
  const double damageAmount = damageBefore * 0.25;
  if (gameplayValid) {
    // God mode is a process global outside LCN1.  The damage/weapon proof owns
    // an explicit disabled precondition and the outer scope restores it.
    g_godMode = 0;
    occupiedObject->m_lastLeaveTime = -1.0e9;
    occupiedObject->setDamage(
        damageAmount, occupiedState.position,
        (std::max)(Session::m_moment, occupiedState.lastTime),
        KR_ObjectID::NUL());
    SRecoveredVehicleRuntimeState damagedState = {};
    gameplayValid = VehicleRuntimeState_Inspect(
                        context, occupiedVehicle, &damagedState) &&
        !damagedState.dead && !damagedState.takingTaxi &&
        occupiedObject->m_damage > 0.0 &&
        occupiedObject->m_damage < damageBefore;
  }

  BulletRuntimeTelemetry primaryAfter = {};
  if (gameplayValid && hasPrimaryWeapon) {
    gameplayValid = SendHardwareButton("MouseL", TRUE) &&
        RunVehicleFrameAfter(0.01) &&
        SendHardwareButton("MouseL", FALSE);
    for (int frame = 0; gameplayValid && frame < 24; ++frame) {
      gameplayValid = RunVehicleFrameAfter(0.025) &&
          BulletSubjectState_OwnerRuntimeTelemetry(
              context, "Vehicle.Default", &primaryAfter);
      if (gameplayValid &&
          primaryAfter.acceptedStarts > bulletsBefore.acceptedStarts)
        break;
    }
    gameplayValid = gameplayValid &&
        primaryAfter.acceptedStarts > bulletsBefore.acceptedStarts;
  } else if (gameplayValid) {
    primaryAfter = bulletsBefore;
  }

  BulletRuntimeTelemetry secondaryAfter = {};
  if (gameplayValid && hasSecondaryWeapon) {
    gameplayValid = SendHardwareButton("MouseR", TRUE) &&
        RunVehicleFrameAfter(0.01) &&
        SendHardwareButton("MouseR", FALSE);
    for (int frame = 0; gameplayValid && frame < 40; ++frame) {
      gameplayValid = RunVehicleFrameAfter(0.025) &&
          BulletSubjectState_OwnerRuntimeTelemetry(
              context, "Vehicle.Default", &secondaryAfter);
      if (gameplayValid &&
          secondaryAfter.acceptedStarts > primaryAfter.acceptedStarts &&
          occupiedObject->m_secBulletCnt < secondaryAmmoBefore)
        break;
    }
    gameplayValid = gameplayValid &&
        secondaryAfter.acceptedStarts > primaryAfter.acceptedStarts &&
        occupiedObject->m_secBulletCnt < secondaryAmmoBefore &&
        RecoveredGameServices_VehicleActiveActionCount() == 0u;
  } else if (gameplayValid) {
    secondaryAfter = primaryAfter;
    gameplayValid = occupiedObject->m_secBulletCnt == secondaryAmmoBefore &&
        RecoveredGameServices_VehicleActiveActionCount() == 0u;
  }
  const int secondaryAmmoAfter =
      occupiedObject == nullptr ? -1 : occupiedObject->m_secBulletCnt;
  const double damageAfter =
      occupiedObject == nullptr ? -1.0 : occupiedObject->m_damage;

  SLevelContinuationSummary gameplayRestoredSummary;
  std::vector<std::uint8_t> gameplayRecaptured;
  SLevelContinuationSummary gameplayRecapturedSummary;
  std::string gameplayRestoreError;
  if (gameplayValid) {
    bool gameplayRestored = false;
    for (unsigned int attempt = 0; attempt < 8u; ++attempt) {
      gameplayRestored = RecoveredGameServices_RestoreLevelContinuation(
          occupiedContinuation, &gameplayRestoredSummary);
      if (gameplayRestored) break;
      gameplayRestoreError =
          RecoveredGameServices_LastLevelContinuationError();
      const bool retryable =
          gameplayRestoreError.find("stable") != std::string::npos ||
          gameplayRestoreError.find("preflight capture failed") !=
              std::string::npos;
      if (!retryable || attempt + 1u >= 8u ||
          !RunVehicleFrameAfter(0.025))
        break;
      ++coverage->gameplayRestoreDeferrals;
    }
    gameplayValid = gameplayRestored &&
        RecoveredGameServices_CaptureLevelContinuation(
            &gameplayRecaptured, &gameplayRecapturedSummary) &&
        gameplayRestoredSummary.ready && gameplayRecapturedSummary.ready &&
        gameplayRestoredSummary.worldFingerprint ==
            occupiedSummary.worldFingerprint &&
        gameplayRestoredSummary.containerFingerprint ==
            occupiedSummary.containerFingerprint &&
        gameplayRecapturedSummary.worldFingerprint ==
            occupiedSummary.worldFingerprint &&
        gameplayRecapturedSummary.containerFingerprint ==
            occupiedSummary.containerFingerprint &&
        gameplayRecaptured == occupiedContinuation &&
        RecoveredGameServices_VehicleActiveActionCount() == 0u;
  }
  if (!gameplayValid) {
    std::fprintf(stderr,
                 "debug Vehicle gameplay profile=%d/%s attr=%d hud=%d/%d/%d "
                 "weapons=%s/%d,%s/%d "
                 "bullets=%u/%u/%u ammo=%d/%d damage=%.6f/%.6f "
                 "world=%llu/%llu container=%llu/%llu actions=%u "
                 "god=%d dead=%d leave=%.6f moment=%.6f vehicle_time=%.6f "
                 "immune=%.6f restore_error=%s\n",
                 expectedProfile,
                 VehicleRuntimeState_VesselProfileName(expectedProfile),
                 occupiedAttribute != nullptr ? 1 : 0,
                 expectsHud ? 1 : 0, occupiedPanelReady ? 1 : 0,
                 occupiedPanelOpen ? 1 : 0,
                 occupiedAttribute == nullptr ? "<none>" :
                     occupiedAttribute->m_bulletAttrName,
                 occupiedAttribute == nullptr ? -1 :
                     occupiedAttribute->m_bulletAttrIndex,
                 occupiedAttribute == nullptr ? "<none>" :
                     occupiedAttribute->m_bulletSecAttrName,
                 occupiedAttribute == nullptr ? -1 :
                     occupiedAttribute->m_bulletSecAttrIndex,
                 bulletsBefore.acceptedStarts,
                 primaryAfter.acceptedStarts, secondaryAfter.acceptedStarts,
                 secondaryAmmoBefore, secondaryAmmoAfter,
                 damageBefore, damageAfter,
                 static_cast<unsigned long long>(
                     gameplayRecapturedSummary.worldFingerprint),
                 static_cast<unsigned long long>(
                     occupiedSummary.worldFingerprint),
                 static_cast<unsigned long long>(
                     gameplayRecapturedSummary.containerFingerprint),
                 static_cast<unsigned long long>(
                     occupiedSummary.containerFingerprint),
                 RecoveredGameServices_VehicleActiveActionCount(),
                 g_godMode, Vehicle::m_dead ? 1 : 0,
                 occupiedObject == nullptr ? 0.0 :
                     occupiedObject->m_lastLeaveTime,
                 Session::m_moment, occupiedState.lastTime,
                 g_levelAttr.m_666Time,
                 gameplayRestoreError.c_str());
    RecoveredGameServices_RestoreLevelContinuation(
        occupiedContinuation, &gameplayRestoredSummary);
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }
  ++coverage->gameplayProfiles;
  ++coverage->primaryProofs;
  ++coverage->secondaryProofs;
  if (hasPrimaryWeapon) ++coverage->armedPrimaryProfiles;
  if (hasSecondaryWeapon) ++coverage->armedSecondaryProfiles;
  ++coverage->damageProofs;
  ++coverage->hudProofs;
  if (expectsHud)
    ++coverage->hudProfiles;
  else
    ++coverage->hudlessProfiles;
  ++coverage->gameplayRoundTrips;

  occupiedVehicle = context->searchObject("Vehicle.Default");
  occupiedObject = occupiedVehicle.isNUL()
                       ? nullptr
                       : static_cast<Vehicle*>(context->queryInterface(
                             occupiedVehicle, IVehicleIID));
  g_godMode = 0;
  const bool destructionRequested = occupiedObject != nullptr &&
      RecoveredGameServices_RequestDebugDestroyOccupiedVehicle();
  const bool destructionProcessed = destructionRequested &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  if (!occupied || !destructionRequested || !destructionProcessed) {
    const SRecoveredDebugMenuState* state =
        RecoveredGameServices_DebugMenuState();
    std::fprintf(stderr,
                 "debug Vehicle destruction setup occupied=%d object=%d "
                 "state=%d/%d/%d taxi_change=%d panel=%d/%d orphan=%d "
                 "capture=%d request=%d process=%d action=%s error=%s\n",
                 occupied ? 1 : 0, occupiedObject != nullptr ? 1 : 0,
                 occupiedState.dead ? 1 : 0,
                 occupiedState.takingTaxi ? 1 : 0,
                 occupiedState.active ? 1 : 0,
                 occupiedObject != nullptr &&
                         occupiedObject->taxiChangeEnabled()
                     ? 1 : 0,
                  occupiedObject != nullptr && occupiedObject->panelReady()
                      ? 1 : 0,
                  occupiedObject != nullptr && occupiedObject->panelOpen()
                      ? 1 : 0,
                  orphanBaseline, occupiedSummary.ready ? 1 : 0,
                 destructionRequested ? 1 : 0,
                 destructionProcessed ? 1 : 0,
                 state == nullptr ? "<none>" : state->lastAction.c_str(),
                 state == nullptr ? "<none>" : state->lastError.c_str());
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }

  KR_ObjectID defaultVehicle = context->searchObject("Vehicle.Default");
  Vehicle* defaultObject = defaultVehicle.isNUL()
                               ? nullptr
                               : static_cast<Vehicle*>(
                                     context->queryInterface(
                                         defaultVehicle, IVehicleIID));
  SRecoveredVehicleRuntimeState destroyedState = {};
  std::vector<unsigned char> orphanState;
  std::vector<std::uint8_t> destroyedContinuation;
  SLevelContinuationSummary destroyedSummary;
  const SRecoveredDebugMenuState* destroyed =
      RecoveredGameServices_DebugMenuState();
  const bool destroyedValid = defaultObject != nullptr &&
      VehicleRuntimeState_Inspect(
          context, defaultVehicle, &destroyedState) &&
      !destroyedState.dead && !destroyedState.takingTaxi &&
      defaultObject->taxiChangeEnabled() &&
      OrphanSubjectState_LiveCount() == orphanBaseline + 1 &&
      OrphanActiveWorldState_CaptureStable(context, &orphanState) &&
      OrphanActiveWorldState_SchedulerEventCount(orphanState) ==
          orphanBaseline + 1 &&
      RecoveredGameServices_CaptureLevelContinuation(
          &destroyedContinuation, &destroyedSummary) &&
      destroyedSummary.ready && destroyed != nullptr &&
      destroyed->preVehicleDestructionCheckpointAvailable &&
      destroyed->requests == 2u && destroyed->completedCommands == 2u &&
      destroyed->failedCommands == 0u &&
      destroyed->forcedVehicleDestructions == 1u &&
      destroyed->destructionOrphanCreations == 1u &&
      destroyed->destructionSaveProofs == 1u &&
      destroyed->restoredPreVehicleDestructionCheckpoints == 0u &&
      destroyed->destructionWorldFingerprint ==
          destroyedSummary.worldFingerprint &&
      destroyed->destructionContinuationFingerprint ==
          destroyedSummary.containerFingerprint &&
      destroyed->destructionOrphanFingerprint ==
          OrphanActiveWorldState_Fingerprint(context) &&
      destroyed->lastAction == "destroy-occupied-vehicle";
  if (!destroyedValid ||
      !RecoveredGameServices_RequestDebugRestorePreVehicleDestruction() ||
      !RecoveredGameServices_ProcessPendingDebugCommand()) {
    const SRecoveredDebugMenuState* state =
        RecoveredGameServices_DebugMenuState();
    std::fprintf(stderr,
                 "debug Vehicle destruction proof valid=%d live=%d/%d "
                 "orphan_bytes=%zu events=%d continuation=%d "
                 "requests=%u completed=%u failed=%u forced=%u "
                 "orphans=%u saves=%u checkpoint=%d action=%s error=%s\n",
                 destroyedValid ? 1 : 0,
                 OrphanSubjectState_LiveCount(), orphanBaseline + 1,
                 orphanState.size(),
                 OrphanActiveWorldState_SchedulerEventCount(orphanState),
                 destroyedSummary.ready ? 1 : 0,
                 state == nullptr ? 0u : state->requests,
                 state == nullptr ? 0u : state->completedCommands,
                 state == nullptr ? 0u : state->failedCommands,
                 state == nullptr ? 0u : state->forcedVehicleDestructions,
                 state == nullptr ? 0u : state->destructionOrphanCreations,
                 state == nullptr ? 0u : state->destructionSaveProofs,
                 state != nullptr &&
                         state->preVehicleDestructionCheckpointAvailable
                     ? 1 : 0,
                 state == nullptr ? "<none>" : state->lastAction.c_str(),
                 state == nullptr ? "<none>" : state->lastError.c_str());
    RecoveredGameServices_ConfigureDebugMenu(
        false, std::vector<std::string>());
    return false;
  }

  KR_ObjectID restoredVehicle = context->searchObject("Vehicle.Default");
  Vehicle* restoredObject = restoredVehicle.isNUL()
                                ? nullptr
                                : static_cast<Vehicle*>(
                                      context->queryInterface(
                                          restoredVehicle, IVehicleIID));
  SRecoveredVehicleRuntimeState restoredState = {};
  std::vector<std::uint8_t> restoredContinuation;
  SLevelContinuationSummary restoredSummary;
  const SRecoveredDebugMenuState* restored =
      RecoveredGameServices_DebugMenuState();
  const bool restoredValid = restoredObject != nullptr &&
      VehicleRuntimeState_Inspect(
          context, restoredVehicle, &restoredState) &&
      !restoredState.dead && !restoredState.takingTaxi &&
      !restoredObject->taxiChangeEnabled() &&
      restoredObject->panelReady() == occupiedPanelReady &&
      restoredObject->panelOpen() == occupiedPanelOpen &&
      OrphanSubjectState_LiveCount() == orphanBaseline &&
      RecoveredGameServices_VehicleCameraMode() ==
          RECOVERED_VEHICLE_CAMERA_LIVE &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u &&
      RecoveredGameServices_CaptureLevelContinuation(
          &restoredContinuation, &restoredSummary) &&
      restoredSummary.ready &&
      restoredSummary.worldFingerprint == occupiedSummary.worldFingerprint &&
      restoredSummary.containerFingerprint ==
          occupiedSummary.containerFingerprint &&
      restored != nullptr &&
      !restored->preVehicleDestructionCheckpointAvailable &&
      restored->requests == 3u && restored->completedCommands == 3u &&
      restored->failedCommands == 0u &&
      restored->forcedVehicleDestructions == 1u &&
      restored->restoredPreVehicleDestructionCheckpoints == 1u &&
      restored->rollbackAttempts == 0u &&
      restored->lastAction == "restore-pre-vehicle-destruction";
  const bool disabled = RecoveredGameServices_ConfigureDebugMenu(
      false, std::vector<std::string>());
  if (!restoredValid || !disabled) {
    std::fprintf(stderr,
                 "debug Vehicle restoration valid=%d disabled=%d "
                 "occupied=%d panel=%d/%d -> %d/%d live=%d/%d world=%llu/%llu "
                 "container=%llu/%llu action=%s error=%s\n",
                 restoredValid ? 1 : 0, disabled ? 1 : 0,
                  restoredObject != nullptr &&
                          !restoredObject->taxiChangeEnabled()
                      ? 1 : 0,
                  occupiedPanelReady ? 1 : 0,
                  occupiedPanelOpen ? 1 : 0,
                  restoredObject != nullptr && restoredObject->panelReady()
                      ? 1 : 0,
                  restoredObject != nullptr && restoredObject->panelOpen()
                      ? 1 : 0,
                  OrphanSubjectState_LiveCount(), orphanBaseline,
                 static_cast<unsigned long long>(
                     restoredSummary.worldFingerprint),
                 static_cast<unsigned long long>(
                     occupiedSummary.worldFingerprint),
                 static_cast<unsigned long long>(
                     restoredSummary.containerFingerprint),
                 static_cast<unsigned long long>(
                     occupiedSummary.containerFingerprint),
                 restored == nullptr ? "<none>" :
                     restored->lastAction.c_str(),
                 restored == nullptr ? "<none>" :
                     restored->lastError.c_str());
  }
  return restoredValid && disabled;
}

bool ExerciseDebugOccupiedVehicleDestruction(
    SimulationContext* context,
    SDebugVehicleDestructionCoverage* coverage) {
  if (context == nullptr || coverage == nullptr ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u)
    return false;
  *coverage = {};

  // Damage is a no-op while the legacy process-global briefing cheat is set.
  // Normalize only this proof and preserve the campaign-owned value for the
  // rest of the smoke.
  struct SScopedGodModeRestore {
    explicit SScopedGodModeRestore(int value) : saved(value) {}
    ~SScopedGodModeRestore() { g_godMode = saved; }
    int saved;
  } restoreGodMode(g_godMode);
  g_godMode = 0;

  std::vector<std::uint8_t> suiteBaseline;
  SLevelContinuationSummary suiteSummary;
  const int orphanBaseline = OrphanSubjectState_LiveCount();
  const bool suiteCaptured = orphanBaseline >= 0 &&
      RecoveredGameServices_CaptureLevelContinuation(
          &suiteBaseline, &suiteSummary);
  const bool menuConfigured = suiteCaptured && suiteSummary.ready &&
      suiteSummary.sections == 14 &&
      RecoveredGameServices_ConfigureDebugMenu(
          true, std::vector<std::string>{"Level.Debug.VehicleProfiles"});
  if (!suiteCaptured || !suiteSummary.ready || suiteSummary.sections != 14 ||
      !menuConfigured) {
    std::fprintf(stderr,
                 "debug Vehicle profile baseline capture=%d ready=%d "
                 "sections=%d orphan=%d bytes=%zu menu=%d error=%s\n",
                 suiteCaptured ? 1 : 0, suiteSummary.ready ? 1 : 0,
                 suiteSummary.sections, orphanBaseline, suiteBaseline.size(),
                 menuConfigured ? 1 : 0,
                 RecoveredGameServices_LastLevelContinuationError());
    return false;
  }

  const std::size_t catalogCount =
      RecoveredGameServices_DebugVehicleTypeCount();
  std::vector<std::size_t> representatives(
      RECOVERED_VEHICLE_PROFILE_TANK_GENN5 + 1, catalogCount);
  bool catalogValid = catalogCount > 0;
  bool unsupportedType = false;
  for (std::size_t index = 0; catalogValid && index < catalogCount; ++index) {
    SRecoveredDebugVehicleType type;
    catalogValid = RecoveredGameServices_DebugVehicleType(index, &type) &&
        !type.taxiAttribute.empty() && !type.vehicleAttribute.empty() &&
        context->isExist(type.vehicleAttribute.c_str()) &&
        type.vehicleType >= 0 &&
        type.vesselProfile >= RECOVERED_VEHICLE_PROFILE_UNKNOWN &&
        type.vesselProfile <= RECOVERED_VEHICLE_PROFILE_TANK_GENN5 &&
        (type.vesselProfile == RECOVERED_VEHICLE_PROFILE_UNKNOWN ||
         type.dynamic == VehicleRuntimeState_VesselProfileName(
                             type.vesselProfile));
    if (!catalogValid) {
      std::fprintf(stderr,
                   "debug Vehicle catalog entry invalid index=%zu taxi=%s "
                   "vehicle=%s type=%d dynamic=%s kind=%d profile=%d\n",
                   index, type.taxiAttribute.c_str(),
                   type.vehicleAttribute.c_str(), type.vehicleType,
                   type.dynamic.c_str(), type.vesselKind,
                   type.vesselProfile);
    }
    if (!catalogValid || type.vehicleType != 1)
      continue;
    if (type.vesselProfile == RECOVERED_VEHICLE_PROFILE_UNKNOWN ||
        type.vesselKind == RECOVERED_VEHICLE_VESSEL_UNKNOWN) {
      std::fprintf(stderr,
                   "debug Vehicle type-1 profile unknown index=%zu taxi=%s "
                   "vehicle=%s dynamic=%s kind=%d profile=%d\n",
                   index, type.taxiAttribute.c_str(),
                   type.vehicleAttribute.c_str(), type.dynamic.c_str(),
                   type.vesselKind, type.vesselProfile);
      unsupportedType = true;
      continue;
    }
    ++coverage->eligibleTypes;
    const unsigned int profileBit =
        1u << static_cast<unsigned int>(type.vesselProfile - 1);
    coverage->profileMask |= profileBit;
    if (representatives[type.vesselProfile] == catalogCount) {
      representatives[type.vesselProfile] = index;
      ++coverage->representativeProfiles;
    }
  }
  const bool catalogDisabled = RecoveredGameServices_ConfigureDebugMenu(
      false, std::vector<std::string>());
  catalogValid = catalogValid && !unsupportedType;
  if (!catalogValid || !catalogDisabled || coverage->eligibleTypes == 0u ||
      coverage->representativeProfiles == 0u || coverage->profileMask == 0u) {
    std::fprintf(stderr,
                 "debug Vehicle profile catalog valid=%d disabled=%d "
                 "types=%zu eligible=%u representatives=%u mask=%u\n",
                 catalogValid ? 1 : 0, catalogDisabled ? 1 : 0,
                 catalogCount, coverage->eligibleTypes,
                 coverage->representativeProfiles, coverage->profileMask);
    return false;
  }

  for (int profile = RECOVERED_VEHICLE_PROFILE_DRAGON;
       profile <= RECOVERED_VEHICLE_PROFILE_TANK_GENN5; ++profile) {
    const std::size_t representative = representatives[profile];
    if (representative == catalogCount)
      continue;
    if (!ExerciseDebugOccupiedVehicleDestructionOne(
            context, representative, profile, coverage)) {
      std::fprintf(stderr,
                   "debug Vehicle profile round-trip failed profile=%d/%s "
                   "index=%zu eligible=%u representatives=%u mask=%u\n",
                   profile, VehicleRuntimeState_VesselProfileName(profile),
                   representative, coverage->eligibleTypes,
                   coverage->representativeProfiles, coverage->profileMask);
      SLevelContinuationSummary ignored;
      RecoveredGameServices_RestoreLevelContinuation(suiteBaseline, &ignored);
      return false;
    }

    SLevelContinuationSummary restoredSummary;
    std::vector<std::uint8_t> recapturedBytes;
    SLevelContinuationSummary recapturedSummary;
    const bool baselineRestored =
        RecoveredGameServices_RestoreLevelContinuation(
            suiteBaseline, &restoredSummary);
    const bool baselineRecaptured = baselineRestored &&
        RecoveredGameServices_CaptureLevelContinuation(
            &recapturedBytes, &recapturedSummary);
    const bool baselineMatches = baselineRestored && baselineRecaptured &&
        restoredSummary.ready && recapturedSummary.ready &&
        restoredSummary.worldFingerprint == suiteSummary.worldFingerprint &&
        restoredSummary.containerFingerprint ==
            suiteSummary.containerFingerprint &&
        recapturedSummary.worldFingerprint == suiteSummary.worldFingerprint &&
        recapturedSummary.containerFingerprint ==
            suiteSummary.containerFingerprint &&
        recapturedBytes == suiteBaseline &&
        OrphanSubjectState_LiveCount() == orphanBaseline &&
        RecoveredGameServices_VehicleActiveActionCount() == 0u;
    if (!baselineMatches) {
      std::fprintf(stderr,
                   "debug Vehicle suite baseline failed profile=%d/%s "
                   "restore=%d recapture=%d ready=%d/%d orphan=%d/%d "
                   "actions=%u world=%llu/%llu/%llu "
                   "container=%llu/%llu/%llu bytes=%zu/%zu equal=%d "
                   "error=%s\n",
                   profile, VehicleRuntimeState_VesselProfileName(profile),
                   baselineRestored ? 1 : 0, baselineRecaptured ? 1 : 0,
                   restoredSummary.ready ? 1 : 0,
                   recapturedSummary.ready ? 1 : 0,
                   OrphanSubjectState_LiveCount(), orphanBaseline,
                   RecoveredGameServices_VehicleActiveActionCount(),
                   static_cast<unsigned long long>(
                       restoredSummary.worldFingerprint),
                   static_cast<unsigned long long>(
                       recapturedSummary.worldFingerprint),
                   static_cast<unsigned long long>(suiteSummary.worldFingerprint),
                   static_cast<unsigned long long>(
                       restoredSummary.containerFingerprint),
                   static_cast<unsigned long long>(
                       recapturedSummary.containerFingerprint),
                   static_cast<unsigned long long>(
                       suiteSummary.containerFingerprint),
                   recapturedBytes.size(), suiteBaseline.size(),
                   recapturedBytes == suiteBaseline ? 1 : 0,
                   RecoveredGameServices_LastLevelContinuationError());
      return false;
    }
    ++coverage->roundTrips;
  }
  return coverage->roundTrips == coverage->representativeProfiles &&
      coverage->gameplayProfiles == coverage->representativeProfiles &&
      coverage->primaryProofs == coverage->representativeProfiles &&
      coverage->secondaryProofs == coverage->representativeProfiles &&
      coverage->damageProofs == coverage->representativeProfiles &&
      coverage->hudProofs == coverage->representativeProfiles &&
      coverage->hudProfiles + coverage->hudlessProfiles ==
          coverage->representativeProfiles &&
      coverage->gameplayRoundTrips == coverage->representativeProfiles;
}

struct SOccupiedVehicleSaveLoadCoverage {
  bool ready = false;
  unsigned int eligibleTypes = 0;
  unsigned int representativeProfiles = 0;
  unsigned int restoredProfiles = 0;
  unsigned int profileMask = 0;
  unsigned int hudProfiles = 0;
  unsigned int hudlessProfiles = 0;
  int lastVesselProfile = RECOVERED_VEHICLE_PROFILE_UNKNOWN;
  int panelReady = 0;
  int panelOpen = 0;
  int cameraMode = RECOVERED_VEHICLE_CAMERA_UNKNOWN;
  int taxiCount = 0;
  int orphanCount = 0;
  double savedDamage = 0.0;
  double savedSpeed = 0.0;
  std::uint64_t worldFingerprint = 0;
  std::uint64_t taxiFingerprint = 0;
  std::uint64_t orphanFingerprint = 0;
  unsigned int resumedActions = 0;
};

bool SameVector(const CFVector3& left, const CFVector3& right,
                double epsilon = 1.0e-7) {
  return std::fabs(left.x - right.x) <= epsilon &&
      std::fabs(left.y - right.y) <= epsilon &&
      std::fabs(left.z - right.z) <= epsilon;
}

bool SameMatrix(const CFMatrix3x4& left, const CFMatrix3x4& right,
                double epsilon = 1.0e-7) {
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      if (std::fabs(left.m[row][column] -
                    right.m[row][column]) > epsilon)
        return false;
  return true;
}

bool ExerciseOccupiedVehicleSaveLoadOne(
    SimulationContext* context, const std::wstring& saveDirectory,
    std::size_t occupiedIndex, int expectedProfile,
    SOccupiedVehicleSaveLoadCoverage* coverage) {
  constexpr std::uint32_t kOccupiedSlot = 5u;
  if (context == nullptr || saveDirectory.empty() || coverage == nullptr ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u)
    return false;

  const SRecoveredSaveMenuState* menuBefore =
      RecoveredGameServices_SaveMenuState();
  const unsigned int saveRequestsBefore =
      menuBefore == nullptr ? 0u : menuBefore->saveRequests;
  const unsigned int completedSavesBefore =
      menuBefore == nullptr ? 0u : menuBefore->completedSaves;
  const unsigned int loadRequestsBefore =
      menuBefore == nullptr ? 0u : menuBefore->loadRequests;
  const unsigned int completedLoadsBefore =
      menuBefore == nullptr ? 0u : menuBefore->completedLoads;

  std::vector<std::uint8_t> suiteBaseline;
  SLevelContinuationSummary suiteSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &suiteBaseline, &suiteSummary) || !suiteSummary.ready ||
      !RecoveredGameServices_ConfigureDebugMenu(
          true, std::vector<std::string>{"Level.SaveLoad.Authority"}))
    return false;

  SRecoveredDebugVehicleType occupiedType;
  const std::size_t count = RecoveredGameServices_DebugVehicleTypeCount();
  const bool selectionValid = occupiedIndex < count &&
      RecoveredGameServices_DebugVehicleType(
          occupiedIndex, &occupiedType) &&
      occupiedType.vehicleType == 1 &&
      occupiedType.vesselProfile == expectedProfile &&
      occupiedType.dynamic ==
          VehicleRuntimeState_VesselProfileName(expectedProfile);
  const bool entered = selectionValid &&
      RecoveredGameServices_RequestDebugVehicleSpawn(
          occupiedIndex, true) &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  const bool spawnedWorldObject = entered &&
      RecoveredGameServices_RequestDebugVehicleSpawn(
          occupiedIndex, false) &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  bool settlementFrames = spawnedWorldObject;
  for (int frame = 0; settlementFrames && frame < 4; ++frame)
    settlementFrames = RunVehicleFrameAfter(0.025);

  KR_ObjectID vehicle = context->searchObject("Vehicle.Default");
  const bool drove = settlementFrames &&
      SendHardwareButton("W", TRUE);
  bool driveFrames = drove;
  for (int frame = 0; driveFrames && frame < 8; ++frame)
    driveFrames = RunVehicleFrameAfter(0.025);
  const bool released = driveFrames && SendHardwareButton("W", FALSE) &&
      RunVehicleFrameAfter(0.01);
  const bool damaged = released &&
      RecoveredGameServices_RequestDebugDamageOccupiedVehicle() &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  const SRecoveredDebugMenuState* savedDebugState =
      RecoveredGameServices_DebugMenuState();
  const std::string savedDebugObject = savedDebugState == nullptr
      ? std::string() : savedDebugState->lastObject;

  SRecoveredVehicleRuntimeState savedVehicle = {};
  SRecoveredVehicleCameraTelemetry savedCamera = {};
  const int savedTaxiCount = TaxiSubjectState_LiveCount();
  const int savedOrphanCount = OrphanSubjectState_LiveCount();
  const std::uint64_t savedTaxiFingerprint =
      TaxiActiveWorldState_Fingerprint(context);
  const std::uint64_t savedOrphanFingerprint =
      OrphanActiveWorldState_Fingerprint(context);
  const bool authorityReady = damaged &&
      VehicleRuntimeState_Inspect(context, vehicle, &savedVehicle) &&
      VehicleRuntimeState_InspectCamera(context, &savedCamera) &&
      savedVehicle.active && !savedVehicle.frameBegun &&
      !savedVehicle.dead && !savedVehicle.takingTaxi &&
      !savedVehicle.taxiChangeEnabled && savedVehicle.damage > 0.0 &&
      std::sqrt(savedVehicle.speed.x * savedVehicle.speed.x +
                savedVehicle.speed.y * savedVehicle.speed.y +
                savedVehicle.speed.z * savedVehicle.speed.z) > 1.0e-6 &&
      savedVehicle.panelOpen == savedVehicle.panelReady &&
      savedCamera.mode == RECOVERED_VEHICLE_CAMERA_LIVE &&
      savedTaxiCount > 0 && savedOrphanCount >= 0 &&
      savedTaxiFingerprint != 0 && savedOrphanFingerprint != 0 &&
      !savedDebugObject.empty() &&
      context->isExist(savedDebugObject.c_str()) &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u;

  SLevelSaveSlotSummary savedSlot;
  SLevelContinuationSummary savedContinuation;
  const bool saved = authorityReady &&
      RecoveredGameServices_RequestSaveSlotWithMetadata(
          kOccupiedSlot, true, "Occupied moving Vehicle",
          "damaged Vehicle, HUD/camera and debug-spawned world") &&
      RecoveredGameServices_ProcessPendingSaveCommand(
          &savedSlot, &savedContinuation) &&
      savedSlot.ready && savedContinuation.ready &&
      savedSlot.worldFingerprint == savedContinuation.worldFingerprint;

  bool mutationFrames = saved && SendHardwareButton("D", TRUE) &&
      SendHardwareButton("W", TRUE);
  for (int frame = 0; mutationFrames && frame < 8; ++frame)
    mutationFrames = RunVehicleFrameAfter(0.025);
  const bool mutationReleased = mutationFrames &&
      SendHardwareButton("W", FALSE) &&
      SendHardwareButton("D", FALSE) &&
      RunVehicleFrameAfter(0.01);
  const bool damagedAgain = mutationReleased &&
      RecoveredGameServices_RequestDebugDamageOccupiedVehicle() &&
      RecoveredGameServices_ProcessPendingDebugCommand();
  SRecoveredVehicleRuntimeState mutatedVehicle = {};
  const bool mutated = damagedAgain &&
      VehicleRuntimeState_Inspect(context, vehicle, &mutatedVehicle) &&
      (!SameVector(mutatedVehicle.position, savedVehicle.position) ||
       std::fabs(mutatedVehicle.damage - savedVehicle.damage) > 1.0e-7);

  SLevelSaveSlotSummary loadedSlot;
  SLevelContinuationSummary loadedContinuation;
  const bool loaded = mutated &&
      RecoveredGameServices_RequestLoadSlot(kOccupiedSlot) &&
      RecoveredGameServices_ProcessPendingSaveCommand(
          &loadedSlot, &loadedContinuation);
  vehicle = context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState restoredVehicle = {};
  SRecoveredVehicleCameraTelemetry restoredCamera = {};
  const bool restored = loaded &&
      VehicleRuntimeState_Inspect(context, vehicle, &restoredVehicle) &&
      VehicleRuntimeState_InspectCamera(context, &restoredCamera) &&
      loadedSlot.archiveFingerprint == savedSlot.archiveFingerprint &&
      loadedContinuation.ready && loadedContinuation.worldMatches &&
      loadedContinuation.boundaryMatches &&
      loadedContinuation.worldFingerprint ==
          savedContinuation.worldFingerprint &&
      loadedContinuation.restoredWorldFingerprint ==
          savedContinuation.worldFingerprint &&
      loadedContinuation.containerFingerprint ==
          savedContinuation.containerFingerprint &&
      restoredVehicle.attribute == savedVehicle.attribute &&
      SameVector(restoredVehicle.position, savedVehicle.position) &&
      SameVector(restoredVehicle.subjectPosition,
                 savedVehicle.subjectPosition) &&
      SameVector(restoredVehicle.speed, savedVehicle.speed) &&
      SameMatrix(restoredVehicle.direction, savedVehicle.direction) &&
      std::fabs(restoredVehicle.mass - savedVehicle.mass) <= 1.0e-7 &&
      std::fabs(restoredVehicle.damage - savedVehicle.damage) <= 1.0e-7 &&
      std::fabs(restoredVehicle.lastTime - savedVehicle.lastTime) <= 1.0e-7 &&
      restoredVehicle.secondaryBulletCount ==
          savedVehicle.secondaryBulletCount &&
      restoredVehicle.vesselKind == savedVehicle.vesselKind &&
      restoredVehicle.dead == savedVehicle.dead &&
      restoredVehicle.takingTaxi == savedVehicle.takingTaxi &&
      restoredVehicle.panelReady == savedVehicle.panelReady &&
      restoredVehicle.panelOpen == savedVehicle.panelOpen &&
      restoredVehicle.taxiChangeEnabled ==
          savedVehicle.taxiChangeEnabled &&
      restoredCamera.mode == savedCamera.mode &&
      TaxiSubjectState_LiveCount() == savedTaxiCount &&
      OrphanSubjectState_LiveCount() == savedOrphanCount &&
      TaxiActiveWorldState_Fingerprint(context) ==
          savedTaxiFingerprint &&
      OrphanActiveWorldState_Fingerprint(context) ==
          savedOrphanFingerprint &&
      context->isExist(savedDebugObject.c_str()) &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u;

  SRecoveredVehicleControlJournalTelemetry journalBefore = {};
  SRecoveredVehicleControlJournalTelemetry journalAfter = {};
  const bool resumed = restored &&
      RecoveredGameServices_VehicleControlJournalTelemetry(
          &journalBefore) &&
      SendHardwareButton("W", TRUE) && RunVehicleFrameAfter(0.025) &&
      RunVehicleFrameAfter(0.025) && SendHardwareButton("W", FALSE) &&
      RunVehicleFrameAfter(0.01) &&
      RecoveredGameServices_VehicleControlJournalTelemetry(
          &journalAfter) && journalAfter.recording == 1 &&
      journalAfter.appendFailures == 0 &&
      journalAfter.actionRecords == journalBefore.actionRecords + 2u &&
      RecoveredGameServices_VehicleActiveActionCount() == 0u;

  const SRecoveredSaveMenuState* menu =
      RecoveredGameServices_SaveMenuState();
  const SRecoveredDebugMenuState* debug =
      RecoveredGameServices_DebugMenuState();
  const bool telemetry = resumed && menu != nullptr && debug != nullptr &&
      menu->saveRequests == saveRequestsBefore + 1u &&
      menu->completedSaves == completedSavesBefore + 1u &&
      menu->loadRequests == loadRequestsBefore + 1u &&
      menu->completedLoads == completedLoadsBefore + 1u &&
      menu->failedCommands == 0u && !menu->pending &&
      debug->requests == 4u && debug->completedCommands == 4u &&
      debug->failedCommands == 0u &&
      debug->damagedOccupiedVehicles == 2u &&
      debug->lastVehicleDamageAfter < debug->lastVehicleDamageBefore;

  if (telemetry) {
    ++coverage->restoredProfiles;
    coverage->lastVesselProfile = occupiedType.vesselProfile;
    coverage->panelReady = savedVehicle.panelReady;
    coverage->panelOpen = savedVehicle.panelOpen;
    coverage->cameraMode = savedCamera.mode;
    coverage->taxiCount = savedTaxiCount;
    coverage->orphanCount = savedOrphanCount;
    const double speed = std::sqrt(
        savedVehicle.speed.x * savedVehicle.speed.x +
        savedVehicle.speed.y * savedVehicle.speed.y +
        savedVehicle.speed.z * savedVehicle.speed.z);
    if (coverage->restoredProfiles == 1u ||
        savedVehicle.damage < coverage->savedDamage)
      coverage->savedDamage = savedVehicle.damage;
    if (coverage->restoredProfiles == 1u || speed < coverage->savedSpeed)
      coverage->savedSpeed = speed;
    coverage->worldFingerprint = savedContinuation.worldFingerprint;
    coverage->taxiFingerprint = savedTaxiFingerprint;
    coverage->orphanFingerprint = savedOrphanFingerprint;
    coverage->resumedActions +=
        journalAfter.actionRecords - journalBefore.actionRecords;
    if (savedVehicle.panelReady)
      ++coverage->hudProfiles;
    else
      ++coverage->hudlessProfiles;
  } else {
    std::fprintf(
        stderr,
        "occupied save/load authority entered=%d spawned=%d settlement=%d "
        "drove=%d damage=%d authority=%d save=%d mutate=%d load=%d "
        "restore=%d resume=%d menu=%u/%u/%u/%u debug=%u/%u/%u "
        "world=%llu/%llu taxi=%d/%d orphan=%d/%d error=%s\n",
        entered ? 1 : 0, spawnedWorldObject ? 1 : 0,
        settlementFrames ? 1 : 0, drove ? 1 : 0, damaged ? 1 : 0,
        authorityReady ? 1 : 0, saved ? 1 : 0, mutated ? 1 : 0,
        loaded ? 1 : 0, restored ? 1 : 0, resumed ? 1 : 0,
        menu == nullptr ? 0u : menu->saveRequests,
        menu == nullptr ? 0u : menu->completedSaves,
        menu == nullptr ? 0u : menu->loadRequests,
        menu == nullptr ? 0u : menu->completedLoads,
        debug == nullptr ? 0u : debug->requests,
        debug == nullptr ? 0u : debug->completedCommands,
        debug == nullptr ? 0u : debug->damagedOccupiedVehicles,
        static_cast<unsigned long long>(savedContinuation.worldFingerprint),
        static_cast<unsigned long long>(loadedContinuation.worldFingerprint),
        savedTaxiCount, TaxiSubjectState_LiveCount(), savedOrphanCount,
        OrphanSubjectState_LiveCount(),
        menu == nullptr ? "<none>" : menu->lastError.c_str());
  }

  SLevelContinuationSummary restoredBaseline;
  const bool baselineRestored =
      RecoveredGameServices_RestoreLevelContinuation(
          suiteBaseline, &restoredBaseline) && restoredBaseline.ready &&
      restoredBaseline.worldFingerprint == suiteSummary.worldFingerprint &&
      restoredBaseline.containerFingerprint ==
          suiteSummary.containerFingerprint;
  const bool debugDisabled = RecoveredGameServices_ConfigureDebugMenu(
      false, std::vector<std::string>());
  return telemetry && baselineRestored && debugDisabled;
}

bool ExerciseOccupiedVehicleSaveLoad(
    SimulationContext* context, const std::wstring& saveDirectory,
    SOccupiedVehicleSaveLoadCoverage* coverage) {
  if (context == nullptr || saveDirectory.empty() || coverage == nullptr ||
      RecoveredGameServices_VehicleActiveActionCount() != 0u)
    return false;
  *coverage = {};

  std::vector<std::uint8_t> suiteBaseline;
  SLevelContinuationSummary suiteSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &suiteBaseline, &suiteSummary) || !suiteSummary.ready ||
      !RecoveredGameServices_ConfigureDebugMenu(
          true, std::vector<std::string>{"Level.SaveLoad.Profiles"}))
    return false;

  const std::size_t catalogCount =
      RecoveredGameServices_DebugVehicleTypeCount();
  std::vector<std::size_t> representatives(
      RECOVERED_VEHICLE_PROFILE_TANK_GENN5 + 1, catalogCount);
  bool catalogValid = catalogCount > 0;
  for (std::size_t index = 0; catalogValid && index < catalogCount; ++index) {
    SRecoveredDebugVehicleType type;
    catalogValid = RecoveredGameServices_DebugVehicleType(index, &type) &&
        !type.taxiAttribute.empty() && !type.vehicleAttribute.empty() &&
        type.vehicleType >= 0 &&
        type.vesselProfile >= RECOVERED_VEHICLE_PROFILE_UNKNOWN &&
        type.vesselProfile <= RECOVERED_VEHICLE_PROFILE_TANK_GENN5;
    if (!catalogValid || type.vehicleType != 1)
      continue;
    if (type.vesselProfile == RECOVERED_VEHICLE_PROFILE_UNKNOWN ||
        type.vesselKind == RECOVERED_VEHICLE_VESSEL_UNKNOWN ||
        type.dynamic !=
            VehicleRuntimeState_VesselProfileName(type.vesselProfile)) {
      catalogValid = false;
      break;
    }
    ++coverage->eligibleTypes;
    coverage->profileMask |=
        1u << static_cast<unsigned int>(type.vesselProfile - 1);
    if (representatives[type.vesselProfile] == catalogCount) {
      representatives[type.vesselProfile] = index;
      ++coverage->representativeProfiles;
    }
  }
  const bool catalogDisabled = RecoveredGameServices_ConfigureDebugMenu(
      false, std::vector<std::string>());
  if (!catalogValid || !catalogDisabled || coverage->eligibleTypes == 0u ||
      coverage->representativeProfiles == 0u || coverage->profileMask == 0u)
    return false;

  for (int profile = RECOVERED_VEHICLE_PROFILE_DRAGON;
       profile <= RECOVERED_VEHICLE_PROFILE_TANK_GENN5; ++profile) {
    if (representatives[profile] == catalogCount)
      continue;
    if (!ExerciseOccupiedVehicleSaveLoadOne(
            context, saveDirectory, representatives[profile], profile,
            coverage)) {
      SLevelContinuationSummary ignored;
      RecoveredGameServices_RestoreLevelContinuation(
          suiteBaseline, &ignored);
      RecoveredGameServices_ConfigureDebugMenu(
          false, std::vector<std::string>());
      return false;
    }
  }

  std::vector<std::uint8_t> recaptured;
  SLevelContinuationSummary recapturedSummary;
  const bool baselineMatches =
      RecoveredGameServices_CaptureLevelContinuation(
          &recaptured, &recapturedSummary) && recapturedSummary.ready &&
      recapturedSummary.worldFingerprint == suiteSummary.worldFingerprint &&
      recapturedSummary.containerFingerprint ==
          suiteSummary.containerFingerprint && recaptured == suiteBaseline;
  coverage->ready = baselineMatches &&
      coverage->restoredProfiles == coverage->representativeProfiles &&
      coverage->hudProfiles + coverage->hudlessProfiles ==
          coverage->representativeProfiles &&
      coverage->resumedActions == coverage->representativeProfiles * 2u;
  return coverage->ready;
}

bool ExerciseCampaignRestartStaging() {
  std::vector<std::uint8_t> baseline;
  SLevelContinuationSummary baselineSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &baseline, &baselineSummary) || !baselineSummary.ready ||
      !RecoveredGameServices_RequestCampaignRestart() ||
      !RecoveredGameServices_ProcessPendingCampaignRestart() ||
      !RecoveredGameServices_CampaignRestartPending())
    return false;
  SRecoveredCampaignRestartRequest request;
  if (!RecoveredGameServices_TakeCampaignRestartRequest(&request) ||
      !request.ready || request.requestOrdinal != 1u ||
      request.attempts != 1u || request.deferredCommands != 0u ||
      request.sourceDead ||
      request.level.empty() || request.sourceContinuation != baseline ||
      request.sourceContinuationSummary.worldFingerprint !=
          baselineSummary.worldFingerprint ||
      request.sourceContinuationSummary.containerFingerprint !=
          baselineSummary.containerFingerprint)
    return false;
  RecoveredGameServices_RecordCampaignRestartResult(
      request, true, false, false, std::string());
  const SRecoveredCampaignRestartState* state =
      RecoveredGameServices_CampaignRestartState();
  return state != nullptr && !state->pending &&
      !state->coordinatorPending && state->requests == 1u &&
      state->completedRestarts == 1u && state->failedRestarts == 0u &&
      state->deadSourceRestarts == 0u &&
      state->lastCommandAttempts == 1u && state->rollbacks == 0u &&
      state->rollbackFailures == 0u && state->lastError.empty();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 1 || argc > 3) {
    return Fail("expected optional source and target retail Level directories");
  }

  if (!ExerciseObserverAxisReducer()) {
    return Fail("observer axis overlap/release contract failed");
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
  if (!RecoveredGameServices_DebugMapReady() ||
      RecoveredGameServices_DebugMapActive() ||
      RecoveredGameServices_DebugMapWidth() != 1000 ||
      RecoveredGameServices_DebugMapHeight() != 1000 ||
      RecoveredGameServices_DebugMapDrawFrames() != 0 ||
      RecoveredGameServices_DebugMapOpenTransitions() != 0 ||
      RecoveredGameServices_DebugMapCloseTransitions() != 0) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("retail DebugMap initialization contract failed");
  }
  if (!ExerciseCurrentKeyTranslationAgainstStaleComplement()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("current-key translation trusted stale complement state");
  }
  std::wstring saveSlotDirectory;
  if (!PrepareSaveSlotFixture(&saveSlotDirectory)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("save slot fixture directory is unavailable");
  }
  if (!RecoveredGameServices_ConfigureSaveDirectory(
          saveSlotDirectory) ||
      RecoveredGameServices_SaveMenuState() == nullptr ||
      !RecoveredGameServices_SaveMenuState()->configured ||
      RecoveredGameServices_SaveMenuState()->directory !=
          saveSlotDirectory) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("save menu directory configuration failed");
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
  if (!ExerciseTaxiDebugCatalogAndSpawn(g_super.m_context, vehicleID)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Taxi debug catalog/spawn/rollback contract failed");
  }
  if (!PeopleActiveWorldState_ProbeDetailedCaptureFailure(
          g_super.m_context)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("People detailed stable-capture diagnostic failed");
  }
  std::vector<unsigned char> peopleState;
  std::vector<std::string> peopleRoutes;
  if (!PeopleActiveWorldState_CaptureStable(
          g_super.m_context, &peopleState) ||
      !PeopleActiveWorldState_RouteNames(peopleState, &peopleRoutes) ||
      std::adjacent_find(peopleRoutes.begin(), peopleRoutes.end()) !=
          peopleRoutes.end()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("People stable route manifest is invalid");
  }
  for (std::size_t index = 0; index < peopleRoutes.size(); ++index) {
    if (!g_super.m_context->isExist(peopleRoutes[index].c_str())) {
      ZAV_DeInitLevel();
      ZAV_Deinit();
      return Fail("People stable route manifest lost a live route");
    }
  }
  if (!RecoveredGameServices_HardwareReady() ||
      !RecoveredGameServices_SeanceReady() ||
      !RecoveredGameServices_BirdAttributesReady() ||
      !RecoveredGameServices_PortalReady() ||
      !RecoveredArenaSeance_TeleportRoutesReady() ||
      RecoveredArenaSeance_TeleportTargetLevel() ||
      RecoveredArenaSeance_TeleportCapacity() != 0 ||
      RecoveredArenaSeance_TeleportRouteCount() != 0 ||
      RecoveredArenaSeance_TeleportProbeRejectedNonPlayer() != 0 ||
      RecoveredArenaSeance_TeleportProbePhysicsCollisions() != 0 ||
      RecoveredArenaSeance_TeleportProbeAppliedPlayer() != 0 ||
      RecoveredArenaSeance_TeleportProbeVehicleRollbacks() != 0 ||
      RecoveredArenaSeance_TeleportFingerprint() != 0 ||
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
      !RecoveredArenaSeance_SkinAnimationsReady() ||
      RecoveredArenaSeance_SkinAnimationEntryCallCount() != 0 ||
      RecoveredArenaSeance_SkinAnimatedModelCount() != 0 ||
      RecoveredArenaSeance_SkinAnimationCommandCount() != 0 ||
      RecoveredArenaSeance_SkinAnimationSourceFingerprint() == 0 ||
      RecoveredArenaSeance_SkinAnimationStateFingerprint() == 0 ||
      RecoveredArenaSeance_SkinAnimationPoseTemporalModelCount() != 0 ||
      RecoveredArenaSeance_SkinAnimationPoseChangedModelCount() != 0 ||
      RecoveredArenaSeance_SkinAnimationPoseSampleCount() != 0 ||
      RecoveredArenaSeance_SkinAnimationPoseRestoredModifierCount() != 0 ||
      RecoveredArenaSeance_SkinAnimationPoseFingerprint() != 0 ||
      !RecoveredArenaSeance_StaticMechanismsReady() ||
      RecoveredArenaSeance_StaticMechanismTargetLevel() ||
      RecoveredArenaSeance_StaticMechanismLevelOne() ||
      RecoveredArenaSeance_StaticMechanismLevelFive() ||
      RecoveredArenaSeance_StaticMechanismBindingCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismWaterwheelCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismFlagCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismRotatingCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismDoorCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismPol16Count() != 0 ||
      RecoveredArenaSeance_StaticMechanismChangedBindingCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismPoseSampleCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismRestoredModifierCount() != 0 ||
      RecoveredArenaSeance_StaticMechanismFingerprint() != 0 ||
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
      !RecoveredGameServices_VehicleDeathCameraReady() ||
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
       RecoveredGameServices_VehicleProbeStabilityRecoveries() != 1 ||
       RecoveredGameServices_VehicleProbeRollbacks() != 1 ||
      RecoveredGameServices_VehicleProbeHorizontalDistance() <= 0.01 ||
      RecoveredGameServices_VehicleDeathCameraProbeActivations() != 1 ||
      RecoveredGameServices_VehicleDeathCameraProbeAscentFrames() != 1 ||
      RecoveredGameServices_VehicleDeathCameraProbeTerminalFrames() != 2 ||
      RecoveredGameServices_VehicleDeathCameraProbeCompletionTransitions() !=
          1 ||
      RecoveredGameServices_VehicleDeathCameraProbeFiniteCameras() != 3 ||
      RecoveredGameServices_VehicleDeathCameraProbeRollbacks() != 1 ||
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
  const int peopleProbeCadenceBounded =
      RecoveredArenaSeance_PeopleProbeCadenceBounded();
  const int peopleProbeRenderedPoseFrames =
      RecoveredArenaSeance_PeopleProbeRenderedPoseFrames();
  const int peopleProbeViewBoundaryResets =
      RecoveredArenaSeance_PeopleProbeViewBoundaryResets();
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
  const int tankProbeCadenceBounded =
      RecoveredArenaSeance_TankProbeCadenceBounded();
  const int tankProbeRenderedPoseFrames =
      RecoveredArenaSeance_TankProbeRenderedPoseFrames();
  const int tankProbeViewBoundaryResets =
      RecoveredArenaSeance_TankProbeViewBoundaryResets();
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
  const int vehicleProbeStabilityRecoveries =
      RecoveredGameServices_VehicleProbeStabilityRecoveries();
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
  SRecoveredFrameTimingTelemetry frameTiming = {};
  if (!RecoveredGameServices_FrameTimingTelemetry(&frameTiming) ||
      frameTiming.frames != 3 || frameTiming.totalMicroseconds == 0 ||
      frameTiming.maximumFrameMicroseconds == 0 ||
      frameTiming.renderMicroseconds == 0 ||
      frameTiming.maximumRenderMicroseconds == 0 ||
      frameTiming.totalMicroseconds < frameTiming.renderMicroseconds) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("successful frame timing telemetry is invalid");
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
      horizontalSpeedBeforeStop <= 1.0e-6 ||
      HorizontalSpeed(vehicleStopped) > 64.0 ||
      vehicleStopped.stabilityRecoveryCount != 0) {
    std::fprintf(stderr,
                 "vehicle-stop diagnostics before=%.12f immediate=%.12f "
                 "after_world=%.12f stability=%d/%d\n",
                 horizontalSpeedBeforeStop,
                 HorizontalSpeed(vehicleStopCommand),
                 HorizontalSpeed(vehicleStopped),
                 vehicleStopped.stabilityRecoveryCount,
                 vehicleStopped.lastStabilityReason);
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("X did not keep the partitioned Vehicle frame bounded");
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
  SLevelSaveSlotSummary savedSlot;
  SLevelContinuationSummary capturedContinuation;
  SRecoveredVehicleRuntimeState continuationVehicle = {};
  vehicleID = g_super.m_context->searchObject("Vehicle.Default");
  if (!VehicleRuntimeState_Inspect(
          g_super.m_context, vehicleID, &continuationVehicle) ||
      RecoveredGameServices_RequestSaveSlot(LevelSaveSlot_Count(), false) ||
      RecoveredGameServices_SaveMenuState() == nullptr ||
      RecoveredGameServices_SaveMenuState()->pending ||
      !RecoveredGameServices_RequestSaveSlotWithMetadata(
          3u, false, "Station approach",
          "Vehicle checkpoint after focus recovery") ||
      RecoveredGameServices_RequestLoadSlot(3u) ||
      !RecoveredGameServices_SaveMenuState()->pending ||
      RecoveredGameServices_SaveMenuState()->pendingAction !=
          RECOVERED_SAVE_MENU_SAVE ||
      RecoveredGameServices_SaveMenuState()->pendingSlot != 3u ||
      RecoveredGameServices_SaveMenuState()->pendingTitle !=
          "Station approach" ||
      RecoveredGameServices_SaveMenuState()->pendingDescription !=
          "Vehicle checkpoint after focus recovery" ||
      !RecoveredGameServices_ProcessPendingSaveCommand(
          &savedSlot, &capturedContinuation) ||
      !savedSlot.ready || savedSlot.slot != 3u ||
      savedSlot.title != "Station approach" ||
      savedSlot.description !=
          "Vehicle checkpoint after focus recovery" ||
      savedSlot.level.empty() ||
      savedSlot.archiveFingerprint == 0 ||
      savedSlot.continuationFingerprint == 0 ||
      savedSlot.continuationFingerprint !=
          capturedContinuation.containerFingerprint ||
      savedSlot.worldFingerprint !=
          capturedContinuation.worldFingerprint ||
      !capturedContinuation.ready || !capturedContinuation.sealedJournal ||
      !capturedContinuation.boundaryMatches ||
      !capturedContinuation.worldMatches ||
      capturedContinuation.sections != 14 ||
      capturedContinuation.worldFingerprint == 0 ||
      capturedContinuation.journalFingerprint == 0 ||
      capturedContinuation.containerFingerprint == 0 ||
      savedSlot.previewBytes == 0 ||
      savedSlot.archiveBytes == 0 ||
      RecoveredGameServices_SaveMenuState() == nullptr ||
      RecoveredGameServices_SaveMenuState()->saveRequests != 1u ||
      RecoveredGameServices_SaveMenuState()->completedSaves != 1u ||
      RecoveredGameServices_SaveMenuState()
              ->customMetadataSaveRequests != 1u ||
      RecoveredGameServices_SaveMenuState()->failedCommands != 0u ||
      !RecoveredGameServices_SaveMenuState()->lastPreview.ready ||
      RecoveredGameServices_SaveMenuState()->lastPreview.width != 640u ||
      RecoveredGameServices_SaveMenuState()->lastPreview.height != 480u ||
      RecoveredGameServices_SaveMenuState()->lastPreview.pngBytes !=
          savedSlot.previewBytes ||
      RecoveredGameServices_SaveMenuState()
              ->lastPreview.pngFingerprint == 0u) {
    std::fprintf(stderr, "save menu capture: %s\n",
                 RecoveredGameServices_SaveMenuState() == nullptr
                     ? "state unavailable"
                     : RecoveredGameServices_SaveMenuState()
                           ->lastError.c_str());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("live save menu command failed");
  }
  const std::uint64_t previewFingerprint =
      RecoveredGameServices_SaveMenuState()->lastPreview.pngFingerprint;
  const std::uint64_t committedSlotFingerprint =
      savedSlot.archiveFingerprint;
  if (RecoveredGameServices_RequestSaveSlot(3u, false) ||
      RecoveredGameServices_SaveMenuState()->pending ||
      RecoveredGameServices_SaveMenuState()->saveRequests != 1u) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("save menu overwrite guard failed");
  }
  SLevelSaveSlotSummary rejectedReplacement;
  SLevelContinuationSummary rejectedContinuation;
  SLevelSaveSlotStatus slotStatus;
  SLevelSaveSlot retainedSlot;
  if (RecoveredGameServices_SaveLevelSlot(
          saveSlotDirectory, 3u, std::string(),
          "invalid replacement", {}, &rejectedReplacement,
          &rejectedContinuation) ||
      !LevelSaveSlot_Read(saveSlotDirectory, 3u, &retainedSlot,
                          &slotStatus) ||
      retainedSlot.archiveFingerprint != committedSlotFingerprint) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("failed live slot replacement changed committed data");
  }
  SRecoveredSavePreviewImage decodedPreview;
  std::string decodedPreviewFailure;
  if (!RecoveredSavePreview_DecodePng(
          retainedSlot.previewPng, 320u, 240u, &decodedPreview,
          &decodedPreviewFailure) || !decodedPreview.ready ||
      decodedPreview.sourceWidth != 640u ||
      decodedPreview.sourceHeight != 480u ||
      decodedPreview.width != 320u || decodedPreview.height != 240u ||
      decodedPreview.bgra.size() != 320u * 240u * 4u ||
      decodedPreview.sourceFingerprint != previewFingerprint) {
    std::fprintf(stderr, "save preview decode: %s\n",
                 decodedPreviewFailure.c_str());
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("committed save preview did not decode for the Windows UI");
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
      vehicleDriveTelemetry.dynamicCollisionFrames > 42 ||
      vehicleDriveTelemetry.stabilityRecoveries != 0 ||
      vehicleDriveTelemetry.lastStabilityReason !=
          RECOVERED_VEHICLE_STABILITY_NONE) {
    SRecoveredVehicleStabilityTelemetry stability = {};
    const bool stabilityInspected =
        VehicleRuntimeState_InspectStability(g_super.m_context, &stability);
    std::fprintf(stderr,
                 "vehicle-visual-suite diagnostics ready=%d fallback=%d "
                 "reason=%u input=%u forwarded=%u housekeeping=%u ignored=%u "
                 "input_failure=%d frames=%u cameras=%u dropped=%u "
                 "fallbacks=%u dwFrames=%lu inspected=%d active=%d "
                 "frame_begun=%d advances=%u controls=%u "
                 "focus=%u/%u release=%u suppressed=%u active_actions=%u "
                 "telemetry=%d distance=%.9f speed=%.9f heading=%.9f "
                 "ground=%u static=%u land=%u dynamic=%u stability=%u/%d\n",
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
                 vehicleDriveTelemetry.dynamicCollisionFrames,
                 vehicleDriveTelemetry.stabilityRecoveries,
                 vehicleDriveTelemetry.lastStabilityReason);
    std::fprintf(stderr,
                 "vehicle-stability-frame inspected=%d count=%d reason=%d "
                 "kind=%d bump=%d ground=%d time=%.9f/%.9f/%.9f "
                 "start_pos=%.9f/%.9f/%.9f start_speed=%.9f/%.9f/%.9f "
                 "rejected_pos=%.9f/%.9f/%.9f "
                 "rejected_speed=%.9f/%.9f/%.9f "
                 "ground=%.9f/%.9f/%.9f len=%.9f "
                 "tangent=%.9f/%.9f dot=%.9f suspension=%.9f "
                 "accel_factor=%.9f throttle=%.9f\n",
                 stabilityInspected ? 1 : 0, stability.recoveryCount,
                 stability.lastReason, stability.vesselKind,
                 stability.bumpFlags, stability.touchingGround,
                 stability.frameStartTime, stability.rejectedTime,
                 stability.requestedTargetTime,
                 stability.frameStartPosition.x,
                 stability.frameStartPosition.y,
                 stability.frameStartPosition.z,
                 stability.frameStartSpeed.x,
                 stability.frameStartSpeed.y,
                 stability.frameStartSpeed.z,
                 stability.rejectedPosition.x,
                 stability.rejectedPosition.y,
                 stability.rejectedPosition.z,
                 stability.rejectedSpeed.x,
                 stability.rejectedSpeed.y,
                 stability.rejectedSpeed.z,
                 stability.groundX, stability.groundY, stability.groundZ,
                 stability.groundLength,
                 stability.forwardTangentLength,
                 stability.rightTangentLength, stability.tangentDot,
                 stability.suspensionTravel,
                 stability.accelerationFactor, stability.throttle);
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

  if (!ExerciseOccupiedVehicleContinuation()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("occupied Vehicle continuation/panel restore failed");
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
      RecoveredGameServices_VehicleProbeStabilityRecoveries() !=
          vehicleProbeStabilityRecoveries ||
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
      RecoveredArenaSeance_PeopleProbeCadenceBounded() !=
          peopleProbeCadenceBounded ||
      RecoveredArenaSeance_PeopleProbeRenderedPoseFrames() !=
          peopleProbeRenderedPoseFrames ||
      RecoveredArenaSeance_PeopleProbeViewBoundaryResets() !=
          peopleProbeViewBoundaryResets ||
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
      RecoveredArenaSeance_TankProbeCadenceBounded() !=
          tankProbeCadenceBounded ||
      RecoveredArenaSeance_TankProbeRenderedPoseFrames() !=
          tankProbeRenderedPoseFrames ||
      RecoveredArenaSeance_TankProbeViewBoundaryResets() !=
          tankProbeViewBoundaryResets ||
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
  SLevelSaveSlotSummary loadedSlot;
  SLevelContinuationSummary restoredContinuation;
  if (RecoveredGameServices_RequestLoadSlot(LevelSaveSlot_Count()) ||
      RecoveredGameServices_SaveMenuState() == nullptr ||
      RecoveredGameServices_SaveMenuState()->pending) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("fresh-context load request guard failed");
  }
  if (!ExerciseDebugMenuStableBoundaryRetry(g_super.m_context)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Debug menu stable-boundary retry contract failed");
  }
  if (!ExerciseDebugDeathLifecycle(g_super.m_context)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Debug death/save/recovery lifecycle failed");
  }
  SDebugVehicleDestructionCoverage destructionCoverage;
  if (!ExerciseDebugOccupiedVehicleDestruction(
          g_super.m_context, &destructionCoverage)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Debug occupied Vehicle destruction/ORP1 recovery failed");
  }
  if (!ExerciseCampaignRestartStaging()) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("campaign current-Level restart staging failed");
  }

  CViewDynamicList openFrameDynamics;
  KR_ObjectID openFrameExplosion = KR_ObjectID::NUL();
  if (!OpenExplosionSaveBoundary(
          &openFrameDynamics, &openFrameExplosion)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Explosion save-boundary fixture failed");
  }
  if (!RecoveredGameServices_RequestLoadSlot(3u)) {
    if (!openFrameExplosion.isNUL()) {
      g_arena.endRender(ZAV_Scene());
      openFrameDynamics.Clear(FALSE);
      ExplosionSubjectState_ReleaseLightFrame();
    }
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("Explosion-boundary slot load was not queued");
  }
  if (RecoveredGameServices_ProcessPendingSaveCommand(
          &loadedSlot, &restoredContinuation)) {
    // A successful restore has already removed the fixture owner, so the
    // deliberately open local draw list contains diagnostic-only stale
    // pointers and must not be traversed during failure cleanup.
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("open Explosion frame unexpectedly admitted slot load");
  }
  const SRecoveredSaveMenuState* deferredLoad =
      RecoveredGameServices_SaveMenuState();
  const bool loadWasDeferred =
      deferredLoad != nullptr && deferredLoad->pending &&
      deferredLoad->pendingAction == RECOVERED_SAVE_MENU_LOAD &&
      deferredLoad->pendingSlot == 3u &&
      deferredLoad->pendingAttempts == 1u &&
      deferredLoad->deferredCommands == 1u &&
      deferredLoad->lastCommandAttempts == 0u &&
      deferredLoad->loadRequests == 1u &&
      deferredLoad->completedLoads == 0u &&
      deferredLoad->failedCommands == 0u &&
      deferredLoad->lastError.find("frame") != std::string::npos;

  g_arena.endRender(ZAV_Scene());
  openFrameDynamics.Clear(FALSE);
  ExplosionSubjectState_ReleaseLightFrame();
  std::vector<unsigned char> closedExplosionState;
  const bool boundaryClosed =
      ExplosionActiveWorldState_CaptureStable(
          g_super.m_context, &closedExplosionState);
  if (!loadWasDeferred || !boundaryClosed ||
      !RecoveredGameServices_ProcessPendingSaveCommand(
          &loadedSlot, &restoredContinuation) ||
      !loadedSlot.ready ||
      loadedSlot.archiveFingerprint != committedSlotFingerprint ||
      loadedSlot.continuationFingerprint !=
          capturedContinuation.containerFingerprint ||
      !restoredContinuation.ready ||
      !restoredContinuation.sealedJournal ||
      !restoredContinuation.boundaryMatches ||
      !restoredContinuation.worldMatches ||
      restoredContinuation.sections != 14 ||
      restoredContinuation.ownerPhases != 14 ||
      restoredContinuation.referencePhases != 14 ||
      restoredContinuation.eventPhases != restoredContinuation.events ||
      restoredContinuation.worldFingerprint !=
          capturedContinuation.worldFingerprint ||
      restoredContinuation.restoredWorldFingerprint !=
          capturedContinuation.worldFingerprint ||
      restoredContinuation.journalFingerprint !=
          capturedContinuation.journalFingerprint ||
      restoredContinuation.containerFingerprint !=
          capturedContinuation.containerFingerprint ||
      RecoveredGameServices_SaveMenuState() == nullptr ||
      RecoveredGameServices_SaveMenuState()->loadRequests != 1u ||
      RecoveredGameServices_SaveMenuState()->completedLoads != 1u ||
      RecoveredGameServices_SaveMenuState()->failedCommands != 0u ||
      RecoveredGameServices_SaveMenuState()->pending ||
      RecoveredGameServices_SaveMenuState()->pendingAttempts != 0u ||
      RecoveredGameServices_SaveMenuState()->deferredCommands != 1u ||
      RecoveredGameServices_SaveMenuState()->lastCommandAttempts != 2u) {
    std::fprintf(stderr,
                 "fresh-context menu RR2SLOT1/LCN1 restore: %s "
                 "phases=%d/%d/%d fingerprints=%llu/%llu/%llu "
                 "retry=%u/%u/%u pending=%d\n",
                 RecoveredGameServices_SaveMenuState() == nullptr
                     ? "state unavailable"
                     : RecoveredGameServices_SaveMenuState()
                           ->lastError.c_str(),
                 restoredContinuation.ownerPhases,
                 restoredContinuation.referencePhases,
                 restoredContinuation.eventPhases,
                 static_cast<unsigned long long>(
                     restoredContinuation.worldFingerprint),
                 static_cast<unsigned long long>(
                     restoredContinuation.restoredWorldFingerprint),
                 static_cast<unsigned long long>(
                     restoredContinuation.containerFingerprint),
                 RecoveredGameServices_SaveMenuState() == nullptr
                     ? 0u
                     : RecoveredGameServices_SaveMenuState()
                           ->deferredCommands,
                 RecoveredGameServices_SaveMenuState() == nullptr
                     ? 0u
                     : RecoveredGameServices_SaveMenuState()
                           ->pendingAttempts,
                 RecoveredGameServices_SaveMenuState() == nullptr
                     ? 0u
                     : RecoveredGameServices_SaveMenuState()
                           ->lastCommandAttempts,
                 RecoveredGameServices_SaveMenuState() != nullptr &&
                         RecoveredGameServices_SaveMenuState()->pending
                     ? 1
                     : 0);
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return Fail("fresh-context LCN1 restore or whole-world proof failed");
  }

  vehicleID = g_super.m_context->searchObject("Vehicle.Default");
  SRecoveredVehicleRuntimeState restoredVehicle = {};
  SRecoveredVehicleAuthorityState restoredAuthority = {};
  SRecoveredVehicleControlJournalTelemetry resumedJournal = {};
  if (!VehicleRuntimeState_Inspect(
          g_super.m_context, vehicleID, &restoredVehicle) ||
      !RecoveredGameServices_VehicleAuthorityState(&restoredAuthority) ||
      restoredAuthority.identityFingerprint !=
          VehicleRuntimeState_IdentityFingerprint(
              g_super.m_context, vehicleID) ||
      std::fabs(restoredAuthority.damage -
                continuationVehicle.damage) > 1.0e-9 ||
      restoredAuthority.vesselKind != continuationVehicle.vesselKind ||
      restoredAuthority.vesselProfile != VehicleRuntimeState_VesselProfile(
          VehicleRuntimeState_DynamicName(g_super.m_context, vehicleID)) ||
      restoredAuthority.active != 1 || restoredAuthority.frameBegun != 0 ||
      restoredAuthority.dead != continuationVehicle.dead ||
      restoredAuthority.takingTaxi != continuationVehicle.takingTaxi ||
      restoredAuthority.panelReady != continuationVehicle.panelReady ||
      restoredAuthority.panelOpen != continuationVehicle.panelOpen ||
      restoredAuthority.taxiChangeEnabled !=
          continuationVehicle.taxiChangeEnabled ||
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
  SOccupiedVehicleSaveLoadCoverage occupiedSaveLoadCoverage;
  if (!ExerciseOccupiedVehicleSaveLoad(
          g_super.m_context, saveSlotDirectory,
          &occupiedSaveLoadCoverage)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    CleanupSaveSlotFixture(saveSlotDirectory);
    return Fail(
        "occupied moving/damaged Vehicle save/load authority failed");
  }
  if (argc == 3 &&
      !ExerciseCrossLevelLoad(argv[1], argv[2], saveSlotDirectory)) {
    ZAV_DeInitLevel();
    ZAV_Deinit();
    CleanupSaveSlotFixture(saveSlotDirectory);
    return Fail("transactional cross-Level load/rollback failed");
  }
  ZAV_DeInitLevel();
  ZAV_Deinit();
  if (!IsServiceReleased() || !IsLevelRolledBack() ||
      RecoveredSoftwareGraph_IsReady() ||
      snd_distMax != initialSoundDistance ||
      snd_distMax2 != initialSoundDistanceSquared) {
    return Fail("complete service shutdown failed");
  }
  CleanupSaveSlotFixture(saveSlotDirectory);

  std::printf(
      "save_gameplay_authority=occupied-moving-damaged-debug-world "
      "profiles=%u/%u restored=%u mask=%u hud=%u/%u/%u "
      "last_profile=%d panel=%d/%d camera=%d taxi=%d orphan=%d "
      "min_damage=%.6f min_speed=%.6f world=%llu taxi_world=%llu "
      "orphan_world=%llu resumed_actions=%u\n",
      occupiedSaveLoadCoverage.eligibleTypes,
      occupiedSaveLoadCoverage.representativeProfiles,
      occupiedSaveLoadCoverage.restoredProfiles,
      occupiedSaveLoadCoverage.profileMask,
      occupiedSaveLoadCoverage.restoredProfiles,
      occupiedSaveLoadCoverage.hudProfiles,
      occupiedSaveLoadCoverage.hudlessProfiles,
      occupiedSaveLoadCoverage.lastVesselProfile,
      occupiedSaveLoadCoverage.panelReady,
      occupiedSaveLoadCoverage.panelOpen,
      occupiedSaveLoadCoverage.cameraMode,
      occupiedSaveLoadCoverage.taxiCount,
      occupiedSaveLoadCoverage.orphanCount,
      occupiedSaveLoadCoverage.savedDamage,
      occupiedSaveLoadCoverage.savedSpeed,
      static_cast<unsigned long long>(
          occupiedSaveLoadCoverage.worldFingerprint),
      static_cast<unsigned long long>(
          occupiedSaveLoadCoverage.taxiFingerprint),
      static_cast<unsigned long long>(
          occupiedSaveLoadCoverage.orphanFingerprint),
      occupiedSaveLoadCoverage.resumedActions);
  std::printf(
      "vehicle_profile_gameplay=%u/%u primary=%u secondary=%u damage=%u "
      "hud=%u/%u/%u roundtrips=%u mask=%u armed=%u/%u "
      "restore_deferrals=%u\n",
      destructionCoverage.gameplayProfiles,
      destructionCoverage.representativeProfiles,
      destructionCoverage.primaryProofs,
      destructionCoverage.secondaryProofs,
      destructionCoverage.damageProofs,
      destructionCoverage.hudProofs,
      destructionCoverage.hudProfiles,
      destructionCoverage.hudlessProfiles,
      destructionCoverage.gameplayRoundTrips,
      destructionCoverage.profileMask,
      destructionCoverage.armedPrimaryProfiles,
      destructionCoverage.armedSecondaryProfiles,
      destructionCoverage.gameplayRestoreDeferrals);
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
              "probe=%d/%d/%d/%d/%d/%d/%d/%d/%d distance=%.6f "
              "vehicle_control=Hardware-exclusive-26/11/13/0 "
              "vehicle_focus=loss/gain-1/1 release=1 suppressed=2 stop=X "
              "vehicle_world=%u/%u/%u/%u "
               "vehicle_frames=42 dropped>=1 camera=Vehicle.Default fallback=0 "
               "taxi_attrs=%d/%d taxi_fingerprint=%llu taxi_refs=%llu "
               "taxi_subject=%d/%d sound=%d fingerprint=%llu "
               "taxi_lifecycle=1/1/1/1/2 "
               "taxi_vehicle=%d/%d/%d/%d/%d/%d/%d/%d "
               "taxi_debug_grounding=%d/%d/%d clearance=%.9f drift=%.9f "
               "taxi_handoff=F1-nearest-panel-drive-rollback "
               "vehicle_embodiment=F1-safe-Taxi-reentry-unsafe-Orphan-ORP1-continuation-resume-impact "
               "vehicle_destruction_profiles=%u/%u/%u mask=%u "
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
               "skin_resources=%llu skin_animations=empty-program-ready "
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
                "save_slot=RR2SLOT1-3-%llu bytes=%zu "
                "preview=PNG-%llu/%zu "
                "resumed_actions=%u "
                "load_retry=1/2 "
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
                 vehicleProbeCameraTransitions,
                 vehicleProbeStabilityRecoveries, vehicleProbeRollbacks,
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
                 g_taxiDebugGroundingProbe.types,
                 g_taxiDebugGroundingProbe.sweepHits,
                 g_taxiDebugGroundingProbe.terrainFallbacks,
                  g_taxiDebugGroundingProbe.maxBottomClearance,
                  g_taxiDebugGroundingProbe.maxImmediateDrift,
                  destructionCoverage.eligibleTypes,
                  destructionCoverage.representativeProfiles,
                  destructionCoverage.roundTrips,
                  destructionCoverage.profileMask,
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
               static_cast<unsigned long long>(
                   loadedSlot.archiveFingerprint),
               loadedSlot.archiveBytes,
               static_cast<unsigned long long>(previewFingerprint),
               loadedSlot.previewBytes,
               resumedJournal.actionRecords);
  return EXIT_SUCCESS;
}
