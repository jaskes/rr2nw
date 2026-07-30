#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

class CGRPanel;
#include "h/vehicle.h"
#include "i/dynobj.i"
#include "i/unit.i"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "sound.h"
#include "message/skinmsg.h"
#include "obase/artefact/ArtefactAttributeState.h"
#include "obase/bird/BirdAttributeState.h"
#include "obase/bullet/BulletAttributeState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/corpse/CorpseAttributeState.h"
#include "obase/corpse/CorpseSubjectState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/farter/FarterSubjectState.h"
#include "obase/lamp/LampAttributeState.h"
#include "obase/orphan/OrphanAttributeState.h"
#include "obase/orphan/OrphanSubjectState.h"
#include "obase/route/route.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokeVisualState.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/spark/SparkSubjectState.h"
#include "obase/taxi/TaxiAttributeState.h"
#include "obase/taxi/TaxiSubjectState.h"
#include "obase/vehicle/VehicleAttributeState.h"
#include "h/cachesmoke.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "storage/h/subject.h"

#include "RecoveredArenaSeanceRuntime.h"

extern SDeviceList _dL;

namespace {

unsigned long long g_explosionFixtureFingerprint = 0;
unsigned long long g_explosionSubjectFixtureFingerprint = 0;
unsigned long long g_explosionSoundFixtureFingerprint = 0;
unsigned long long g_explosionParticleFixtureFingerprint = 0;
unsigned long long g_explosionSmokeFixtureFingerprint = 0;
unsigned long long g_vehicleFixtureFingerprint = 0;
unsigned long long g_vehicleReferenceFixtureFingerprint = 0;
unsigned long long g_taxiFixtureFingerprint = 0;
unsigned long long g_bulletFixtureFingerprint = 0;
unsigned long long g_bulletReferenceFixtureFingerprint = 0;
unsigned long long g_bulletSubjectFixtureFingerprint = 0;
unsigned long long g_farterFixtureFingerprint = 0;
unsigned long long g_farterReferenceFixtureFingerprint = 0;
unsigned long long g_lampFixtureFingerprint = 0;
unsigned long long g_corpseFixtureFingerprint = 0;
unsigned long long g_corpseSubjectFixtureFingerprint = 0;
unsigned long long g_smokerFixtureFingerprint = 0;
unsigned long long g_smokerReferenceFixtureFingerprint = 0;
unsigned long long g_smokeSubjectFixtureFingerprint = 0;
unsigned long long g_smokeVisualFixtureFingerprint = 0;
unsigned long long g_dynSmokerFixtureFingerprint = 0;
unsigned long long g_wavFixtureFingerprint = 0;
unsigned long long g_soundObjectFixtureFingerprint = 0;
unsigned long long g_farterSubjectFixtureFingerprint = 0;
unsigned long long g_skinCatalogFixtureFingerprint = 0;

bool NearlyEqual(double left, double right) {
  const double magnitude = std::fmax(std::fabs(left), std::fabs(right));
  return std::fabs(left - right) <=
         1.0e-10 * std::fmax(1.0, magnitude);
}

class BulletDynamicProbe : public ct_Subject,
                           public IDynamicObject,
                           public IUnit {
 public:
  BulletDynamicProbe()
      : radius_(10.0), damageCount_(0), lastDamage_(0.0),
        lastDamagePosition_(0.0, 0.0, 0.0), lastDamageTime_(0.0),
        lastDamageOwner_(KR_ObjectID::NUL()),
        commander_(KR_ObjectID::NUL()) {
    direction_.LoadIdentity();
  }

  void addNotify() override {
    ct_Subject::addNotify();
    direction_.LoadIdentity();
    radius_ = 10.0;
    resetDamage();
    setPosition(CFVector3(2500.0, 100.0, 2500.0));
  }

  void removeNotify() override {
    ct_Subject::removeNotify();
    direction_.LoadIdentity();
    radius_ = 10.0;
    resetDamage();
    m_position = CFVector3(0.0, 0.0, 0.0);
  }

  int receiveEvent(KR_Event&) override { return 0; }
  CFVector3 realPosition() override { return m_position; }
  bool shouldDump() override { return false; }

  void* queryInterface(int iid) override {
    if (iid == IUnknownIID) return static_cast<KR_Object*>(this);
    if (iid == IDynamicObjectIID) return static_cast<IDynamicObject*>(this);
    if (iid == IUnitIID) return static_cast<IUnit*>(this);
    return nullptr;
  }

  CFVector3 getPos() override { return m_position; }
  double getHAngle() override { return 0.0; }
  CFVector3 getUpVector() override { return CFVector3(0.0, 1.0, 0.0); }
  CFVector3 getCenter() override { return CFVector3(0.0, 0.0, 0.0); }
  double getRadius() override { return radius_; }
  double getRadius0() override { return radius_; }
  CFVector3 getMoveDir() override { return CFVector3(1.0, 0.0, 0.0); }
  double getMoveSpeed() override { return 0.0; }
  void getMatrix(CFMatrix3x4& matrix) override { matrix = direction_; }
  double getMass() override { return 1.0; }
  TCCFMatrix3x4& GetDir() override { return direction_; }
  void SetDir(TCSFMatrix3x4& direction) override { direction_ = direction; }

  double getPower() override { return 1.0; }
  int isFriend(const KR_ObjectID&) override { return 0; }
  double getDamage() override { return lastDamage_; }
  void setDamage(double damage, const CFVector3& position, double timeStamp,
                 KR_ObjectID owner) override {
    ++damageCount_;
    lastDamage_ = damage;
    lastDamagePosition_ = position;
    lastDamageTime_ = timeStamp;
    lastDamageOwner_ = owner;
  }
  double desireShoot() override { return 0.0; }
  KR_ObjectID getCommander() override { return commander_; }
  void setCommander(KR_ObjectID commander) override {
    commander_ = commander;
  }

  void resetDamage() {
    damageCount_ = 0;
    lastDamage_ = 0.0;
    lastDamagePosition_ = CFVector3(0.0, 0.0, 0.0);
    lastDamageTime_ = 0.0;
    lastDamageOwner_ = KR_ObjectID::NUL();
    commander_ = KR_ObjectID::NUL();
  }
  int damageCount() const { return damageCount_; }
  const CFVector3& lastDamagePosition() const {
    return lastDamagePosition_;
  }
  double lastDamageTime() const { return lastDamageTime_; }
  KR_ObjectID lastDamageOwner() const { return lastDamageOwner_; }

 private:
  CFMatrix3x4 direction_;
  double radius_;
  int damageCount_;
  double lastDamage_;
  CFVector3 lastDamagePosition_;
  double lastDamageTime_;
  KR_ObjectID lastDamageOwner_;
  KR_ObjectID commander_;
};

class BulletDynamicProbeTable : public ct_SubjectTable {
 public:
  BulletDynamicProbeTable() : table_(nullptr) {
    registerClass("BulletDynamicProbe");
  }

  void allocObjects(int count) override {
    table_ = new (std::nothrow) BulletDynamicProbe[count];
    if (table_ == nullptr) m_maxObjectQnty = 0;
  }

  void freeObjects() override {
    delete[] table_;
    table_ = nullptr;
    m_maxObjectQnty = 0;
  }

  ct_Object* getObjectPTR(int index) override {
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "BulletDynamicProbeTable::getObjectPTR");
    return &table_[index];
  }

  bool isRendering() override { return false; }
  bool isAudible() override { return false; }

  BulletDynamicProbe* find(const KR_ObjectID& id) const {
    for (int index = 0; index < m_maxObjectQnty; ++index)
      if (table_[index].getObjectID() == id) return &table_[index];
    return nullptr;
  }

 private:
  BulletDynamicProbe* table_;
};

BulletDynamicProbeTable g_bulletDynamicProbeTable;

bool ProbeDynamicBulletCollision(SimulationContext& context) {
  const ct_ClassTableID table =
      g_arena.addClassTable("BulletDynamicProbe", 1);
  if (table == ct_NULLID) return false;
  KR_ObjectID target =
      g_arena.newObject(table, "Bullet.Dynamic.Target.Probe");
  BulletDynamicProbe* targetObject = g_bulletDynamicProbeTable.find(target);
  const char* attribute = BulletAttributeState_FirstAttributeName(&context);
  const bool bulletValid = !target.isNUL() && targetObject != nullptr &&
      attribute != nullptr &&
      BulletSubjectState_ProbeDynamicCollisionLifecycle(
          &context, attribute, target, Session::m_moment) &&
      BulletSubjectState_LiveCount() == 0;
  if (targetObject != nullptr) targetObject->resetDamage();
  const char* explosionAttribute =
      ExplosionAttributeState_FirstAttributeName(&context);
  ExplosionImpactProbeSummary explosionSummary = {};
  const KR_ObjectID damageOwner = g_arena.getObjectID();
  const double damageTime = Session::m_moment < 0.1
                                ? 0.1
                                : Session::m_moment;
  const bool explosionValid = targetObject != nullptr &&
      explosionAttribute != nullptr &&
      ExplosionSubjectState_ProbeDamageLifecycle(
          &context, explosionAttribute, target, damageOwner,
          Session::m_moment, &explosionSummary) &&
      explosionSummary.executedCommands == 1 &&
      explosionSummary.damageApplications == 1 &&
      explosionSummary.impulseApplications == 1 &&
      NearlyEqual(explosionSummary.expectedImpulseX,
                  explosionSummary.expectedDamage *
                      1234.0) &&
      NearlyEqual(explosionSummary.expectedImpulseY, 0.0) &&
      NearlyEqual(explosionSummary.expectedImpulseZ, 0.0) &&
      NearlyEqual(explosionSummary.impulseFactor, 5.0) &&
      targetObject->damageCount() == 1 &&
      NearlyEqual(targetObject->getDamage(),
                  explosionSummary.expectedDamage) &&
      targetObject->lastDamagePosition().x ==
          explosionSummary.impactPositionX &&
      targetObject->lastDamagePosition().y ==
          explosionSummary.impactPositionY &&
      targetObject->lastDamagePosition().z ==
          explosionSummary.impactPositionZ &&
      NearlyEqual(targetObject->lastDamageTime(), damageTime) &&
      targetObject->lastDamageOwner() == damageOwner &&
      ExplosionSubjectState_LiveCount() == 0;
  if (!target.isNUL() && context.isExist(target)) context.removeObject(target);
  return bulletValid && explosionValid &&
         !context.isExist("Bullet.Dynamic.Target.Probe");
}

int Fail(const char* message) {
  RecoveredArenaSeance_Release();
  std::fprintf(stderr,
               "recovered-arena-seance-runtime-smoke: %s "
               "(open=%d script=%d bird=%d portal=%d orphan=%d artefact=%d "
               "smoke=%d explosion=%d vehicle_attrs=%d vehicle_refs=%d "
               "taxi=%d taxi_refs=%d taxi_subject=%d "
               "bullet=%d bullet_refs=%d smoker=%d dyn_smoker=%d "
               "farter=%d lamp=%d corpse=%d corpse_subject=%d "
               "wav=%d sound=%d skin=%d "
               "spark=%d route=%d vehicle=%d "
               "issues=%llu extended_issues=%llu error=%s)\n",
               message, RecoveredArenaSeance_IsOpen() ? 1 : 0,
               RecoveredArenaSeance_ScriptCompleted() ? 1 : 0,
               RecoveredArenaSeance_BirdAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_PortalReady() ? 1 : 0,
               RecoveredArenaSeance_OrphanAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_ArtefactAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_SmokeAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_ExplosionAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_VehicleAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_VehicleReferencesReady() ? 1 : 0,
               RecoveredArenaSeance_TaxiAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_TaxiReferencesReady() ? 1 : 0,
               RecoveredArenaSeance_TaxiSubjectReady() ? 1 : 0,
               RecoveredArenaSeance_BulletAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_BulletReferencesReady() ? 1 : 0,
               RecoveredArenaSeance_SmokerAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_DynSmokerReady() ? 1 : 0,
               RecoveredArenaSeance_FarterAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_LampAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_CorpseAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_CorpseSubjectReady() ? 1 : 0,
               RecoveredArenaSeance_WavMetadataReady() ? 1 : 0,
               RecoveredArenaSeance_SoundObjectReady() ? 1 : 0,
               RecoveredArenaSeance_SkinResourcesReady() ? 1 : 0,
               RecoveredArenaSeance_SparkAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_RouteReady() ? 1 : 0,
               RecoveredArenaSeance_VehicleReady() ? 1 : 0,
               RecoveredArenaSeance_Issues(),
               RecoveredArenaSeance_ExtendedIssues(),
               RecoveredArenaSeance_LastError());
  return EXIT_FAILURE;
}

bool ValidateWavPayloadCompatibility() {
  WAVObj january;
  KR_Event januaryEvent;
  januaryEvent.label = sk_EV_LOAD;
  januaryEvent.data.open(EDO_WRITE)
      .putStr("january.wav")
      .putDouble(1.0)
      .putDouble(2.0)
      .putDouble(3.0)
      .putDouble(4.0)
      .putDouble(5.0)
      .close();
  const unsigned long cachedFlags =
      RSXEMITTERDESC_PREPROCESS | RSXEMITTERDESC_INMEMORY;
  if (!january.receiveEvent(januaryEvent) || !january.m_loaded ||
      january.m_flags != 0 ||
      std::strcmp(january.m_rsxCE.szFilename,
                  "..\\SOUND\\january.wav") != 0 ||
      (january.m_rsxCE.dwFlags & cachedFlags) != cachedFlags) {
    return false;
  }

  WAVObj may;
  KR_Event mayEvent;
  mayEvent.label = sk_EV_LOAD;
  mayEvent.data.open(EDO_WRITE)
      .putStr("may.wav")
      .putDouble(10.0)
      .putDouble(20.0)
      .putDouble(30.0)
      .putDouble(40.0)
      .putDouble(50.0)
      .putInt(1)
      .close();
  if (!may.receiveEvent(mayEvent) || !may.m_loaded || may.m_flags != 1 ||
      std::strcmp(may.m_rsxCE.szFilename, "..\\SOUND\\may.wav") != 0 ||
      (may.m_rsxCE.dwFlags & cachedFlags) != 0 ||
      may.m_rsxEModel.fMinFront != 10.0f ||
      may.m_rsxEModel.fIntensity != 50.0f) {
    return false;
  }

  WAVObj malformed;
  KR_Event malformedEvent;
  malformedEvent.label = sk_EV_LOAD;
  malformedEvent.data.open(EDO_WRITE)
      .putStr("malformed.wav")
      .putDouble(1.0)
      .putDouble(2.0)
      .putDouble(3.0)
      .putDouble(4.0)
      .putDouble(5.0)
      .putInt(1)
      .putInt(0)
      .close();
  return !malformed.receiveEvent(malformedEvent) && !malformed.m_loaded;
}

std::string JoinPath(const std::string& directory, const char* name) {
  return directory + "\\" + name;
}

bool EnsureDirectory(const std::string& path) {
  if (CreateDirectoryA(path.c_str(), nullptr) != FALSE) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool WriteFile(const std::string& path, const char* contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(contents, static_cast<std::streamsize>(std::strlen(contents)));
  return output.good();
}

bool ReadFile(const char* path, std::string& contents) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  contents.assign(std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>());
  return input.good() || input.eof();
}

bool WriteFile(const std::string& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(contents.data(),
               static_cast<std::streamsize>(contents.size()));
  return output.good();
}

bool WriteSprite(const std::string& path, unsigned char seed) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  const unsigned char header[5] = {0, 1, 0, 1, 0};
  output.write(reinterpret_cast<const char*>(header), sizeof(header));
  std::vector<unsigned char> pixels(256u * 256u);
  for (std::size_t index = 0; index < pixels.size(); ++index) {
    pixels[index] = static_cast<unsigned char>(seed + index % 251u);
  }
  output.write(reinterpret_cast<const char*>(pixels.data()),
               static_cast<std::streamsize>(pixels.size()));
  return output.good();
}

bool CurrentDirectory(std::string& result) {
  const DWORD required = GetCurrentDirectoryA(0, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied = GetCurrentDirectoryA(required, buffer.data());
  if (copied == 0 || copied >= required) return false;
  result.assign(buffer.data(), copied);
  return true;
}

bool FullPath(const char* path, std::string& result) {
  const DWORD required = GetFullPathNameA(path, 0, nullptr, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied =
      GetFullPathNameA(path, required, buffer.data(), nullptr);
  if (copied == 0 || copied >= required) return false;
  result.assign(buffer.data(), copied);
  return true;
}

bool IsReleased(SimulationContext& context) {
  return !RecoveredArenaSeance_IsOpen() &&
         !RecoveredArenaSeance_ScriptCompleted() &&
         !RecoveredArenaSeance_BirdAttributesReady() &&
         !RecoveredArenaSeance_PortalReady() &&
         !RecoveredArenaSeance_OrphanAttributesReady() &&
         !RecoveredArenaSeance_OrphanReferencesReady() &&
         RecoveredArenaSeance_OrphanReferenceFingerprint() == 0 &&
         !RecoveredArenaSeance_OrphanSubjectReady() &&
         RecoveredArenaSeance_OrphanSubjectCapacity() == 0 &&
         RecoveredArenaSeance_OrphanSubjectCount() == 0 &&
         RecoveredArenaSeance_OrphanSubjectFingerprint() == 0 &&
         OrphanSubjectState_LiveCount() == 0 &&
         !RecoveredArenaSeance_ArtefactAttributesReady() &&
         !RecoveredArenaSeance_SmokeAttributesReady() &&
         !RecoveredArenaSeance_SmokeSubjectReady() &&
         RecoveredArenaSeance_SmokeSubjectCapacity() == 0 &&
         RecoveredArenaSeance_SmokeSubjectFingerprint() == 0 &&
         !RecoveredArenaSeance_SmokeVisualResourcesReady() &&
         RecoveredArenaSeance_SmokeVisualResourceFingerprint() == 0 &&
         SmokeSubjectState_LiveCount() == 0 &&
         !RecoveredArenaSeance_ExplosionAttributesReady() &&
         !RecoveredArenaSeance_ExplosionSubjectReady() &&
         !RecoveredArenaSeance_ExplosionImpulseReady() &&
         !RecoveredArenaSeance_ExplosionLightReady() &&
         !RecoveredArenaSeance_ExplosionSoundReady() &&
         RecoveredArenaSeance_ExplosionSoundReferenceFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionSoundProbeStarted() == -1 &&
         RecoveredArenaSeance_ExplosionSoundProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionSoundProbeRollbacks() == -1 &&
         !RecoveredArenaSeance_ExplosionParticlesReady() &&
         RecoveredArenaSeance_ExplosionParticleVisualFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionParticleProbeStartedBranches() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeRays() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeMoveSteps() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeExpiredParents() == -1 &&
         RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches() == -1 &&
         !RecoveredArenaSeance_ExplosionSmokeReady() &&
         RecoveredArenaSeance_ExplosionSmokeVisualFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents() == -1 &&
         RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites() == -1 &&
         !RecoveredArenaSeance_ExplosionPieceReady() &&
         RecoveredArenaSeance_ExplosionPieceReferenceFingerprint() == 0 &&
         RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeDependencySkips() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeMoveSteps() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeExpiredParents() == -1 &&
         RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces() == -1 &&
         !RecoveredArenaSeance_ExplosionTraceReady() &&
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
         !RecoveredArenaSeance_ExplosionActiveWorldReady() &&
         RecoveredArenaSeance_ExplosionActiveWorldCapturedOwners() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldCapturedBranches() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldSchedulerEvents() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldSoundChildren() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldRollbacks() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldReconstructedIDs() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldStableRoundTrips() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldResumedMoves() == -1 &&
         RecoveredArenaSeance_ExplosionActiveWorldFingerprint() == 0 &&
         ExplosionSubjectState_LiveCount() == 0 &&
         ExplosionSubjectState_ParticleBranchLiveCount() == 0 &&
         ExplosionSubjectState_PieceDrawCount() == 0 &&
         !RecoveredArenaSeance_VehicleAttributesReady() &&
         !RecoveredArenaSeance_VehicleReferencesReady() &&
         RecoveredArenaSeance_VehicleAttributeCount() == -1 &&
         RecoveredArenaSeance_VehicleAttributeCapacity() == 0 &&
         RecoveredArenaSeance_VehicleAttributeFingerprint() == 0 &&
         RecoveredArenaSeance_VehicleReferenceFingerprint() == 0 &&
         !RecoveredArenaSeance_TaxiAttributesReady() &&
         !RecoveredArenaSeance_TaxiReferencesReady() &&
         RecoveredArenaSeance_TaxiAttributeCount() == -1 &&
         RecoveredArenaSeance_TaxiAttributeCapacity() == 0 &&
         RecoveredArenaSeance_TaxiAttributeFingerprint() == 0 &&
         RecoveredArenaSeance_TaxiReferenceFingerprint() == 0 &&
         !RecoveredArenaSeance_TaxiSubjectReady() &&
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
         !RecoveredArenaSeance_BulletAttributesReady() &&
         !RecoveredArenaSeance_BulletReferencesReady() &&
         !RecoveredArenaSeance_BulletSubjectRegistrationReady() &&
         !RecoveredArenaSeance_BulletSubjectReady() &&
         !RecoveredArenaSeance_BulletImpactEffectsReady() &&
         !RecoveredArenaSeance_BulletGroundSparkReady() &&
         !RecoveredArenaSeance_BulletBarrelSmokeReady() &&
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
         !RecoveredArenaSeance_BulletActiveWorldReady() &&
         RecoveredArenaSeance_BulletActiveWorldCapturedOwners() == -1 &&
         RecoveredArenaSeance_BulletActiveWorldSchedulerEvents() == -1 &&
         RecoveredArenaSeance_BulletActiveWorldRollbacks() == -1 &&
         RecoveredArenaSeance_BulletActiveWorldReconstructedIDs() == -1 &&
         RecoveredArenaSeance_BulletActiveWorldStableRoundTrips() == -1 &&
         RecoveredArenaSeance_BulletActiveWorldResumedMoves() == -1 &&
         RecoveredArenaSeance_BulletActiveWorldFingerprint() == 0 &&
         BulletSubjectState_LiveCount() == 0 &&
         !RecoveredArenaSeance_FarterAttributesReady() &&
         !RecoveredArenaSeance_FarterReferencesReady() &&
         !RecoveredArenaSeance_FarterRuntimeReady() &&
         !RecoveredArenaSeance_FarterSubjectReady() &&
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
         RecoveredArenaSeance_FarterReferenceFingerprint() == 0 &&
         !RecoveredArenaSeance_LampAttributesReady() &&
         !RecoveredArenaSeance_CorpseAttributesReady() &&
         !RecoveredArenaSeance_CorpseReferencesReady() &&
         !RecoveredArenaSeance_CorpseRuntimeReady() &&
         RecoveredArenaSeance_CorpseReferenceFingerprint() == 0 &&
         !RecoveredArenaSeance_CorpseSubjectReady() &&
         RecoveredArenaSeance_CorpseSubjectCapacity() == 0 &&
         RecoveredArenaSeance_CorpseSubjectFingerprint() == 0 &&
         CorpseSubjectState_LiveCount() == 0 &&
         !RecoveredArenaSeance_SmokerAttributesReady() &&
         !RecoveredArenaSeance_SmokerReferencesReady() &&
         !RecoveredArenaSeance_SmokerRuntimeReady() &&
         RecoveredArenaSeance_SmokerReferenceFingerprint() == 0 &&
         !RecoveredArenaSeance_DynSmokerReady() &&
         RecoveredArenaSeance_DynSmokerCapacity() == 0 &&
         RecoveredArenaSeance_DynSmokerFingerprint() == 0 &&
         SmokerSubjectState_DynLiveCount() == 0 &&
         !RecoveredArenaSeance_WavMetadataReady() &&
         !RecoveredArenaSeance_SoundObjectReady() &&
         RecoveredArenaSeance_SoundObjectCapacity() == 0 &&
         RecoveredArenaSeance_SoundObjectFingerprint() == 0 &&
         SoundObjectState_LiveCount() == 0 &&
         !RecoveredArenaSeance_SkinResourcesReady() &&
         !RecoveredArenaSeance_SparkAttributesReady() &&
         !RecoveredArenaSeance_SparkSubjectReady() &&
         !RecoveredArenaSeance_SparkVisualResourcesReady() &&
         RecoveredArenaSeance_SparkSubjectCapacity() == 0 &&
         RecoveredArenaSeance_SparkSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_SparkVisualResourceFingerprint() == 0 &&
         RecoveredArenaSeance_SparkProbeInvalidStarts() == -1 &&
         RecoveredArenaSeance_SparkProbeQueuedCreates() == -1 &&
         RecoveredArenaSeance_SparkProbeQueueRollbacks() == -1 &&
         RecoveredArenaSeance_SparkProbePhaseTransitions() == -1 &&
         RecoveredArenaSeance_SparkProbeExpirations() == -1 &&
         !RecoveredArenaSeance_SparkActiveWorldReady() &&
         RecoveredArenaSeance_SparkActiveWorldCapturedOwners() == -1 &&
         RecoveredArenaSeance_SparkActiveWorldSchedulerEvents() == -1 &&
         RecoveredArenaSeance_SparkActiveWorldRollbacks() == -1 &&
         RecoveredArenaSeance_SparkActiveWorldReconstructedIDs() == -1 &&
         RecoveredArenaSeance_SparkActiveWorldStableRoundTrips() == -1 &&
         RecoveredArenaSeance_SparkActiveWorldResumedPhases() == -1 &&
         RecoveredArenaSeance_SparkActiveWorldFingerprint() == 0 &&
         !RecoveredArenaSeance_SmokeActiveWorldReady() &&
         RecoveredArenaSeance_SmokeActiveWorldCapturedOwners() == -1 &&
         RecoveredArenaSeance_SmokeActiveWorldCapturedBlobs() == -1 &&
         RecoveredArenaSeance_SmokeActiveWorldSchedulerEvents() == -1 &&
         RecoveredArenaSeance_SmokeActiveWorldRollbacks() == -1 &&
         RecoveredArenaSeance_SmokeActiveWorldReconstructedIDs() == -1 &&
         RecoveredArenaSeance_SmokeActiveWorldStableRoundTrips() == -1 &&
         RecoveredArenaSeance_SmokeActiveWorldResumedMoves() == -1 &&
          RecoveredArenaSeance_SmokeActiveWorldFingerprint() == 0 &&
          !RecoveredArenaSeance_CorpseActiveWorldReady() &&
          RecoveredArenaSeance_CorpseActiveWorldCapturedOwners() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldOwnedSmokers() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldSchedulerEvents() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldRollbacks() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldReconstructedObjects() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldStableRoundTrips() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldResumedEmissions() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldResumedDeaths() == -1 &&
          RecoveredArenaSeance_CorpseActiveWorldFingerprint() == 0 &&
         SparkSubjectState_Capacity() == 0 &&
         SparkSubjectState_LiveCount() == 0 &&
         !RecoveredArenaSeance_RouteReady() &&
         !RecoveredArenaSeance_PeopleAttributesReady() &&
         !RecoveredArenaSeance_PeopleReferencesReady() &&
         !RecoveredArenaSeance_PeopleSubjectReady() &&
         RecoveredArenaSeance_PeopleAttributeCapacity() == -1 &&
         RecoveredArenaSeance_PeopleAttributeCount() == -1 &&
         RecoveredArenaSeance_PeopleSubjectCapacity() == -1 &&
         RecoveredArenaSeance_PeopleSubjectCount() == -1 &&
         RecoveredArenaSeance_PeopleAttributeFingerprint() == 0 &&
         RecoveredArenaSeance_PeopleSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_PeopleProbeScheduledMoves() == -1 &&
         RecoveredArenaSeance_PeopleProbeBulletDamageApplications() == -1 &&
         RecoveredArenaSeance_PeopleProbeDeathTransitions() == -1 &&
         RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips() == -1 &&
         RecoveredArenaSeance_PeopleProbeRollbacks() == -1 &&
         RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs() == -1 &&
         RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents() == -1 &&
         RecoveredArenaSeance_PeopleActiveWorldRollbacks() == -1 &&
         RecoveredArenaSeance_PeopleActiveWorldFingerprint() == 0 &&
         !RecoveredArenaSeance_TankCannonAttributesReady() &&
         !RecoveredArenaSeance_TankReferencesReady() &&
         !RecoveredArenaSeance_TankCannonSubjectTablesReady() &&
         RecoveredArenaSeance_CannonAttributeCapacity() == -1 &&
         RecoveredArenaSeance_CannonAttributeCount() == -1 &&
         RecoveredArenaSeance_CannonSubjectCapacity() == -1 &&
         RecoveredArenaSeance_CannonSubjectCount() == -1 &&
         RecoveredArenaSeance_CannonAttributeFingerprint() == 0 &&
         RecoveredArenaSeance_CannonSubjectFingerprint() == 0 &&
         RecoveredArenaSeance_TankAttributeCapacity() == -1 &&
         RecoveredArenaSeance_TankAttributeCount() == -1 &&
         RecoveredArenaSeance_TankSubjectCapacity() == -1 &&
         RecoveredArenaSeance_TankSubjectCount() == -1 &&
         RecoveredArenaSeance_TankAttributeFingerprint() == 0 &&
         RecoveredArenaSeance_TankSubjectFingerprint() == 0 &&
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
         !RecoveredArenaSeance_ActiveWorldPersistenceReady() &&
         RecoveredArenaSeance_ActiveWorldFormatVersion() == 0 &&
         RecoveredArenaSeance_ActiveWorldSections() == -1 &&
         RecoveredArenaSeance_ActiveWorldEvents() == -1 &&
         RecoveredArenaSeance_ActiveWorldOwnerPhases() == -1 &&
         RecoveredArenaSeance_ActiveWorldReferencePhases() == -1 &&
         RecoveredArenaSeance_ActiveWorldEventPhases() == -1 &&
         RecoveredArenaSeance_ActiveWorldCreatedOwners() == -1 &&
         RecoveredArenaSeance_ActiveWorldMissionRecords() == -1 &&
         RecoveredArenaSeance_ActiveWorldMissionConditionReferences() == -1 &&
         RecoveredArenaSeance_ActiveWorldMissionRouteReferences() == -1 &&
         RecoveredArenaSeance_ActiveWorldMissionCheckEvents() == -1 &&
         RecoveredArenaSeance_ActiveWorldCorruptionRejects() == -1 &&
         RecoveredArenaSeance_ActiveWorldRollbacks() == -1 &&
         RecoveredArenaSeance_ActiveWorldContainerBytes() == 0 &&
         RecoveredArenaSeance_ActiveWorldFingerprint() == 0 &&
         !RecoveredArenaSeance_VehicleReady() && g_vehicle == nullptr &&
         RecoveredArenaSeance_VehicleVesselMass() == 0.0 &&
         !context.isExist("Storage") && !context.isExist("Bird.Attr.0") &&
         !context.isExist("Orphan.Attr.Default") &&
         !context.isExist("Artefact.Attr.0") &&
         !context.isExist("Smoke.Attr.Small") &&
         !context.isExist("Smoke.Attr.Fire.Corpse") &&
         !context.isExist("Smoker.Attr") &&
         !context.isExist("wav.Ambient") &&
         !context.isExist("Expl.Test.0") &&
         !context.isExist("Bullet.Led") &&
         !context.isExist("Taxi.Attr.CorpseFinal") &&
         !context.isExist("Lamp.Attr.Default") &&
         !context.isExist("Corpse.Attr.Default") &&
         !context.isExist("snd.snd") &&
         !context.isExist("Spark.Flash") &&
         !context.isExist("Vehicle.Default") &&
         !context.isExist("Kingdom") && !context.isExist("Magician") &&
         Route::m_totalNodePos == 0 && g_cacheSmokeCnt == 0;
}

bool RunCycle(bool expectVisualResources) {
  const double previousSoundDistance = snd_distMax;
  const double previousSoundDistanceSquared = snd_distMax2;
  SimulationContext context(64, 128);
  if (!RecoveredArenaSeance_Initialize(&context, Session::m_moment) ||
      !RecoveredArenaSeance_IsOpen() ||
      !RecoveredArenaSeance_ScriptCompleted() ||
      !RecoveredArenaSeance_BirdAttributesReady() ||
      !RecoveredArenaSeance_PortalReady() ||
      !RecoveredArenaSeance_OrphanAttributesReady() ||
      RecoveredArenaSeance_OrphanReferencesReady() ||
      RecoveredArenaSeance_OrphanReferenceFingerprint() != 0 ||
      !RecoveredArenaSeance_OrphanSubjectReady() ||
      RecoveredArenaSeance_OrphanSubjectCapacity() != 5 ||
      RecoveredArenaSeance_OrphanSubjectCount() != 0 ||
      RecoveredArenaSeance_OrphanSubjectFingerprint() == 0 ||
      OrphanSubjectState_LiveCount() != 0 ||
      !RecoveredArenaSeance_ArtefactAttributesReady() ||
       !RecoveredArenaSeance_SmokeAttributesReady() ||
       !RecoveredArenaSeance_SmokeSubjectReady() ||
       RecoveredArenaSeance_SmokeSubjectCapacity() != 300 ||
       RecoveredArenaSeance_SmokeSubjectFingerprint() == 0 ||
       RecoveredArenaSeance_SmokeVisualResourcesReady() !=
           expectVisualResources ||
       (RecoveredArenaSeance_SmokeVisualResourceFingerprint() != 0) !=
           expectVisualResources ||
      SmokeSubjectState_LiveCount() != 0 ||
      !RecoveredArenaSeance_ExplosionAttributesReady() ||
      !RecoveredArenaSeance_ExplosionSubjectReady() ||
      !RecoveredArenaSeance_ExplosionImpulseReady() ||
      !RecoveredArenaSeance_ExplosionLightReady() ||
      !RecoveredArenaSeance_ExplosionSoundReady() ||
      RecoveredArenaSeance_ExplosionSoundReferenceFingerprint() == 0 ||
      RecoveredArenaSeance_ExplosionSoundProbeStarted() != 1 ||
      RecoveredArenaSeance_ExplosionSoundProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionSoundProbeRollbacks() != 1 ||
      !RecoveredArenaSeance_ExplosionParticlesReady() ||
      RecoveredArenaSeance_ExplosionParticleVisualFingerprint() == 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeStartedBranches() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeRays() < 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeDependencySkips() != 1 ||
      RecoveredArenaSeance_ExplosionParticleProbeMoveSteps() <= 0 ||
      RecoveredArenaSeance_ExplosionParticleProbeExpiredParents() != 1 ||
      RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches() !=
          RecoveredArenaSeance_ExplosionParticleProbeStartedBranches() ||
      RecoveredArenaSeance_ExplosionSmokeReady() != expectVisualResources ||
      (RecoveredArenaSeance_ExplosionSmokeVisualFingerprint() != 0) !=
          expectVisualResources ||
      (expectVisualResources &&
       (RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() <= 0 ||
        RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips() != 1 ||
        RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps() <= 0 ||
        RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents() != 1 ||
        RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites() !=
            RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites())) ||
       (!expectVisualResources &&
       (RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() != -1 ||
        RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips() != -1 ||
        RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps() != -1 ||
        RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents() != -1 ||
         RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites() != -1)) ||
      RecoveredArenaSeance_ExplosionPieceReady() ||
      RecoveredArenaSeance_ExplosionPieceReferenceFingerprint() != 0 ||
      RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() != -1 ||
      RecoveredArenaSeance_ExplosionPieceProbeDependencySkips() != -1 ||
      RecoveredArenaSeance_ExplosionPieceProbeMoveSteps() != -1 ||
      RecoveredArenaSeance_ExplosionPieceProbeExpiredParents() != -1 ||
      RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces() != -1 ||
      RecoveredArenaSeance_ExplosionTraceReady() ||
      RecoveredArenaSeance_ExplosionTraceReferenceFingerprint() != 0 ||
      RecoveredArenaSeance_ExplosionTraceProbeStartedPieces() != -1 ||
      RecoveredArenaSeance_ExplosionTraceProbeQuotaGateSkips() != -1 ||
      RecoveredArenaSeance_ExplosionTraceProbePuffEvents() != -1 ||
      RecoveredArenaSeance_ExplosionTraceProbeSmokeChildren() != -1 ||
      RecoveredArenaSeance_ExplosionTraceProbeMoveSteps() != -1 ||
      RecoveredArenaSeance_ExplosionTraceProbeExpiredParents() != -1 ||
      RecoveredArenaSeance_ExplosionTraceProbeRolledBackPieces() != -1 ||
      RecoveredArenaSeance_ExplosionSubjectCapacity() != 2 ||
      RecoveredArenaSeance_ExplosionSubjectFingerprint() == 0 ||
      RecoveredArenaSeance_ExplosionProbeInvalidStarts() != 2 ||
      RecoveredArenaSeance_ExplosionProbeAllocationRollbacks() != 1 ||
      RecoveredArenaSeance_ExplosionProbeQueuedCommands() != 1 ||
      RecoveredArenaSeance_ExplosionProbeQueueRollbacks() != 1 ||
      RecoveredArenaSeance_ExplosionProbeExecutedCommands() != 1 ||
      RecoveredArenaSeance_ExplosionProbeDamageApplications() != 0 ||
      !RecoveredArenaSeance_ExplosionActiveWorldReady() ||
      RecoveredArenaSeance_ExplosionActiveWorldCapturedOwners() != 1 ||
      RecoveredArenaSeance_ExplosionActiveWorldCapturedBranches() <= 0 ||
      RecoveredArenaSeance_ExplosionActiveWorldSchedulerEvents() < 1 ||
      RecoveredArenaSeance_ExplosionActiveWorldSchedulerEvents() > 2 ||
      RecoveredArenaSeance_ExplosionActiveWorldSoundChildren() != 1 ||
      RecoveredArenaSeance_ExplosionActiveWorldRollbacks() != 1 ||
      RecoveredArenaSeance_ExplosionActiveWorldReconstructedIDs() != 1 ||
      RecoveredArenaSeance_ExplosionActiveWorldStableRoundTrips() != 2 ||
      RecoveredArenaSeance_ExplosionActiveWorldResumedMoves() != 1 ||
      RecoveredArenaSeance_ExplosionActiveWorldFingerprint() == 0 ||
      ExplosionSubjectState_LiveCount() != 0 ||
      ExplosionSubjectState_ParticleBranchLiveCount() != 0 ||
      ExplosionSubjectState_ParticleBranchCapacity() != 500 ||
      !RecoveredArenaSeance_VehicleAttributesReady() ||
      !RecoveredArenaSeance_VehicleReferencesReady() ||
      RecoveredArenaSeance_VehicleAttributeCount() != 3 ||
      RecoveredArenaSeance_VehicleAttributeCapacity() != 8 ||
      RecoveredArenaSeance_VehicleAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_VehicleReferenceFingerprint() == 0 ||
      !RecoveredArenaSeance_TaxiAttributesReady() ||
      RecoveredArenaSeance_TaxiReferencesReady() ||
      RecoveredArenaSeance_TaxiAttributeCount() != 2 ||
      RecoveredArenaSeance_TaxiAttributeCapacity() != 7 ||
      RecoveredArenaSeance_TaxiAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_TaxiReferenceFingerprint() != 0 ||
      RecoveredArenaSeance_TaxiSubjectReady() ||
      RecoveredArenaSeance_TaxiSubjectCapacity() != 0 ||
      RecoveredArenaSeance_TaxiSubjectCount() != 0 ||
      RecoveredArenaSeance_TaxiSubjectSoundCount() != 0 ||
      RecoveredArenaSeance_TaxiSubjectFingerprint() != 0 ||
      RecoveredArenaSeance_TaxiProbeInvalidStarts() != 0 ||
      RecoveredArenaSeance_TaxiProbeValidStarts() != 0 ||
      RecoveredArenaSeance_TaxiProbeRenderReady() != 0 ||
      RecoveredArenaSeance_TaxiProbeSoundReady() != 0 ||
      RecoveredArenaSeance_TaxiProbeRollbacks() != 0 ||
      TaxiSubjectState_Capacity() != 0 ||
      TaxiSubjectState_LiveCount() != 0 ||
      TaxiSubjectState_SoundCount() != 0 ||
      !RecoveredArenaSeance_BulletAttributesReady() ||
      RecoveredArenaSeance_BulletReferencesReady() ||
      !RecoveredArenaSeance_BulletSubjectRegistrationReady() ||
      !RecoveredArenaSeance_BulletSubjectReady() ||
      RecoveredArenaSeance_BulletImpactEffectsReady() ||
      RecoveredArenaSeance_BulletGroundSparkReady() ||
      RecoveredArenaSeance_BulletBarrelSmokeReady() ||
      RecoveredArenaSeance_BulletAttributeCount() != 4 ||
      RecoveredArenaSeance_BulletAttributeCapacity() != 4 ||
      RecoveredArenaSeance_BulletSubjectCapacity() != 500 ||
      RecoveredArenaSeance_BulletSubjectFingerprint() == 0 ||
      RecoveredArenaSeance_BulletSubjectProbeMoveCount() != 2 ||
      RecoveredArenaSeance_BulletCollisionScheduledChecks() != 2 ||
      RecoveredArenaSeance_BulletCollisionExecutedChecks() != 1 ||
      RecoveredArenaSeance_BulletCollisionSphereCases() != 4 ||
      RecoveredArenaSeance_BulletCollisionEarliestHitCases() != 3 ||
      RecoveredArenaSeance_BulletCollisionWaterlineCases() != 4 ||
      RecoveredArenaSeance_BulletCollisionSceneQueries() != 0 ||
      RecoveredArenaSeance_BulletEffectQueuedBatches() != -1 ||
      RecoveredArenaSeance_BulletEffectQueuedChildren() != -1 ||
      RecoveredArenaSeance_BulletEffectSplashFirstCases() != -1 ||
      RecoveredArenaSeance_BulletEffectRolledBackChildren() != -1 ||
      RecoveredArenaSeance_BulletGroundSparkQueued() != -1 ||
      RecoveredArenaSeance_BulletGroundSparkRolledBack() != -1 ||
      RecoveredArenaSeance_BulletBarrelSmokeThresholdStarts() != -1 ||
      RecoveredArenaSeance_BulletBarrelSmokeFrameGateSkips() != -1 ||
      RecoveredArenaSeance_BulletBarrelSmokeAttributeGateSkips() != -1 ||
      RecoveredArenaSeance_BulletBarrelSmokeRollbacks() != -1 ||
      !RecoveredArenaSeance_BulletActiveWorldReady() ||
      RecoveredArenaSeance_BulletActiveWorldCapturedOwners() != 1 ||
      RecoveredArenaSeance_BulletActiveWorldSchedulerEvents() != 2 ||
      RecoveredArenaSeance_BulletActiveWorldRollbacks() != 1 ||
      RecoveredArenaSeance_BulletActiveWorldReconstructedIDs() != 1 ||
      RecoveredArenaSeance_BulletActiveWorldStableRoundTrips() != 2 ||
      RecoveredArenaSeance_BulletActiveWorldResumedMoves() != 1 ||
      RecoveredArenaSeance_BulletActiveWorldFingerprint() == 0 ||
      BulletSubjectState_LiveCount() != 0 ||
      RecoveredArenaSeance_BulletAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_BulletReferenceFingerprint() != 0 ||
      !RecoveredArenaSeance_SmokerAttributesReady() ||
      !RecoveredArenaSeance_SmokerReferencesReady() ||
       RecoveredArenaSeance_SmokerRuntimeReady() != expectVisualResources ||
      RecoveredArenaSeance_SmokerReferenceFingerprint() == 0 ||
      !RecoveredArenaSeance_DynSmokerReady() ||
      RecoveredArenaSeance_DynSmokerCapacity() != 62 ||
      RecoveredArenaSeance_DynSmokerFingerprint() == 0 ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      !RecoveredArenaSeance_FarterAttributesReady() ||
      !RecoveredArenaSeance_FarterSubjectReady() ||
      RecoveredArenaSeance_FarterSubjectCapacity() != 0 ||
      RecoveredArenaSeance_FarterSubjectFingerprint() !=
          FarterSubjectState_AbsentFingerprint() ||
      RecoveredArenaSeance_FarterScriptObjectCount() != 0 ||
      RecoveredArenaSeance_FarterLiveObjectCount() != 0 ||
      RecoveredArenaSeance_FarterSoundObjectCount() != 0 ||
      RecoveredArenaSeance_FarterNearFrameAudibleCount() != 0 ||
      RecoveredArenaSeance_FarterFarFrameAudibleCount() != 0 ||
      RecoveredArenaSeance_FarterAudibleFrameTransition() ||
      !RecoveredArenaSeance_SoundDistanceReady() ||
      RecoveredArenaSeance_SoundDistance() != 300.0 ||
      RecoveredArenaSeance_SoundDistanceSquared() != 90000.0 ||
      FarterSubjectState_LiveCount() != 0 ||
      !RecoveredArenaSeance_FarterReferencesReady() ||
      !RecoveredArenaSeance_FarterRuntimeReady() ||
      RecoveredArenaSeance_FarterReferenceFingerprint() == 0 ||
      !RecoveredArenaSeance_LampAttributesReady() ||
      !RecoveredArenaSeance_CorpseAttributesReady() ||
      !RecoveredArenaSeance_CorpseSubjectReady() ||
      RecoveredArenaSeance_CorpseSubjectCapacity() != 100 ||
      RecoveredArenaSeance_CorpseSubjectFingerprint() == 0 ||
      CorpseSubjectState_LiveCount() != 0 ||
      RecoveredArenaSeance_CorpseReferencesReady() ||
      RecoveredArenaSeance_CorpseRuntimeReady() ||
      RecoveredArenaSeance_CorpseReferenceFingerprint() != 0 ||
      !RecoveredArenaSeance_WavMetadataReady() ||
      !RecoveredArenaSeance_SoundObjectReady() ||
      RecoveredArenaSeance_SoundObjectCapacity() != 250 ||
      RecoveredArenaSeance_SoundObjectFingerprint() == 0 ||
      !SoundObjectState_DeviceFree() ||
      SoundObjectState_LiveCount() != 0 ||
      !RecoveredArenaSeance_SkinResourcesReady() ||
      !RecoveredArenaSeance_SparkAttributesReady() ||
      !RecoveredArenaSeance_SparkSubjectReady() ||
      RecoveredArenaSeance_SparkVisualResourcesReady() ||
      RecoveredArenaSeance_SparkSubjectCapacity() != 40 ||
      RecoveredArenaSeance_SparkSubjectFingerprint() != 0 ||
      RecoveredArenaSeance_SparkVisualResourceFingerprint() != 0 ||
      RecoveredArenaSeance_SparkProbeInvalidStarts() != -1 ||
      RecoveredArenaSeance_SparkProbeQueuedCreates() != -1 ||
      RecoveredArenaSeance_SparkProbeQueueRollbacks() != -1 ||
      RecoveredArenaSeance_SparkProbePhaseTransitions() != -1 ||
      RecoveredArenaSeance_SparkProbeExpirations() != -1 ||
      RecoveredArenaSeance_SparkActiveWorldReady() ||
      RecoveredArenaSeance_SparkActiveWorldCapturedOwners() != -1 ||
      RecoveredArenaSeance_SparkActiveWorldSchedulerEvents() != -1 ||
      RecoveredArenaSeance_SparkActiveWorldRollbacks() != -1 ||
      RecoveredArenaSeance_SparkActiveWorldReconstructedIDs() != -1 ||
      RecoveredArenaSeance_SparkActiveWorldStableRoundTrips() != -1 ||
      RecoveredArenaSeance_SparkActiveWorldResumedPhases() != -1 ||
      RecoveredArenaSeance_SparkActiveWorldFingerprint() != 0 ||
      RecoveredArenaSeance_SmokeActiveWorldReady() !=
          expectVisualResources ||
      (expectVisualResources &&
       (RecoveredArenaSeance_SmokeActiveWorldCapturedOwners() != 2 ||
        RecoveredArenaSeance_SmokeActiveWorldCapturedBlobs() != 2 ||
        RecoveredArenaSeance_SmokeActiveWorldSchedulerEvents() != 2 ||
        RecoveredArenaSeance_SmokeActiveWorldRollbacks() != 1 ||
        RecoveredArenaSeance_SmokeActiveWorldReconstructedIDs() != 2 ||
        RecoveredArenaSeance_SmokeActiveWorldStableRoundTrips() != 2 ||
        RecoveredArenaSeance_SmokeActiveWorldResumedMoves() != 1 ||
        RecoveredArenaSeance_SmokeActiveWorldFingerprint() == 0)) ||
      (!expectVisualResources &&
       (RecoveredArenaSeance_SmokeActiveWorldCapturedOwners() != -1 ||
        RecoveredArenaSeance_SmokeActiveWorldCapturedBlobs() != -1 ||
        RecoveredArenaSeance_SmokeActiveWorldSchedulerEvents() != -1 ||
        RecoveredArenaSeance_SmokeActiveWorldRollbacks() != -1 ||
        RecoveredArenaSeance_SmokeActiveWorldReconstructedIDs() != -1 ||
        RecoveredArenaSeance_SmokeActiveWorldStableRoundTrips() != -1 ||
        RecoveredArenaSeance_SmokeActiveWorldResumedMoves() != -1 ||
        RecoveredArenaSeance_SmokeActiveWorldFingerprint() != 0)) ||
      RecoveredArenaSeance_CorpseActiveWorldReady() ||
      RecoveredArenaSeance_CorpseActiveWorldCapturedOwners() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldOwnedSmokers() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldSchedulerEvents() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldRollbacks() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldReconstructedObjects() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldStableRoundTrips() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldResumedEmissions() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldResumedDeaths() != -1 ||
      RecoveredArenaSeance_CorpseActiveWorldFingerprint() != 0 ||
      SparkSubjectState_Capacity() != 40 ||
      SparkSubjectState_LiveCount() != 0 ||
      !RecoveredArenaSeance_RouteReady() ||
      !RecoveredArenaSeance_PeopleAttributesReady() ||
      !RecoveredArenaSeance_PeopleReferencesReady() ||
      !RecoveredArenaSeance_PeopleSubjectReady() ||
      RecoveredArenaSeance_PeopleAttributeCapacity() != 0 ||
      RecoveredArenaSeance_PeopleAttributeCount() != 0 ||
      RecoveredArenaSeance_PeopleSubjectCapacity() != 0 ||
      RecoveredArenaSeance_PeopleSubjectCount() != 0 ||
      RecoveredArenaSeance_PeopleAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_PeopleSubjectFingerprint() == 0 ||
      RecoveredArenaSeance_PeopleProbeScheduledMoves() != 0 ||
      RecoveredArenaSeance_PeopleProbeBulletDamageApplications() != 0 ||
      RecoveredArenaSeance_PeopleProbeDeathTransitions() != 0 ||
      RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips() != 0 ||
      RecoveredArenaSeance_PeopleProbeRollbacks() != 0 ||
      RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs() != 0 ||
      RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents() != 0 ||
      RecoveredArenaSeance_PeopleActiveWorldRollbacks() != 1 ||
      RecoveredArenaSeance_PeopleActiveWorldFingerprint() == 0 ||
      !RecoveredArenaSeance_TankCannonAttributesReady() ||
      !RecoveredArenaSeance_TankReferencesReady() ||
      !RecoveredArenaSeance_TankCannonSubjectTablesReady() ||
      RecoveredArenaSeance_CannonAttributeCapacity() != 2 ||
      RecoveredArenaSeance_CannonAttributeCount() != 2 ||
      RecoveredArenaSeance_CannonSubjectCapacity() != 140 ||
      RecoveredArenaSeance_CannonSubjectCount() != 0 ||
      RecoveredArenaSeance_CannonAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_CannonSubjectFingerprint() == 0 ||
      RecoveredArenaSeance_TankAttributeCapacity() != 2 ||
      RecoveredArenaSeance_TankAttributeCount() != 0 ||
      RecoveredArenaSeance_TankSubjectCapacity() != 64 ||
      RecoveredArenaSeance_TankSubjectCount() != 0 ||
      RecoveredArenaSeance_TankAttributeFingerprint() == 0 ||
      RecoveredArenaSeance_TankSubjectFingerprint() == 0 ||
      RecoveredArenaSeance_TankProbeAvailable() != 0 ||
      RecoveredArenaSeance_TankProbeValidStarts() != 0 ||
      RecoveredArenaSeance_TankProbeDynamicReady() != 0 ||
      RecoveredArenaSeance_TankProbeRenderReady() != 0 ||
      RecoveredArenaSeance_TankProbeCannonReady() != 0 ||
      RecoveredArenaSeance_TankProbeScheduledMoves() != 0 ||
      RecoveredArenaSeance_TankProbeBulletDamageApplications() != 0 ||
      RecoveredArenaSeance_TankProbeDeathTransitions() != 0 ||
      RecoveredArenaSeance_TankProbeDeathEffects() != 0 ||
      RecoveredArenaSeance_TankProbeSaveStateRoundTrips() != 0 ||
      RecoveredArenaSeance_TankProbeRollbacks() != 1 ||
      !RecoveredArenaSeance_CommanderReady() ||
      RecoveredArenaSeance_CommanderCapacity() != 2 ||
      RecoveredArenaSeance_CommanderCount() != 2 ||
      RecoveredArenaSeance_CommanderHostileLinks() != 0 ||
      RecoveredArenaSeance_CommanderFingerprint() == 0 ||
      !RecoveredArenaSeance_MissionTankLifecycleReady() ||
      RecoveredArenaSeance_TankGroupSubjectCapacity() != 30 ||
      RecoveredArenaSeance_MissionTankAvailable() != 0 ||
      RecoveredArenaSeance_MissionTankSpawns() != 0 ||
      RecoveredArenaSeance_MissionTankMembershipLinks() != 0 ||
      RecoveredArenaSeance_MissionTankFindEnemyCycles() != 0 ||
      RecoveredArenaSeance_MissionTankMovingCycles() != 0 ||
      RecoveredArenaSeance_MissionTankStableRoundTrips() != 0 ||
      RecoveredArenaSeance_MissionTankReconstructedIDs() != 0 ||
      RecoveredArenaSeance_MissionTankRollbacks() != 1 ||
      RecoveredArenaSeance_MissionTankFingerprint() != 0 ||
      !RecoveredArenaSeance_ActiveWorldPersistenceReady() ||
      RecoveredArenaSeance_ActiveWorldFormatVersion() != 1 ||
      RecoveredArenaSeance_ActiveWorldSections() != 11 ||
      RecoveredArenaSeance_ActiveWorldEvents() != 0 ||
      RecoveredArenaSeance_ActiveWorldOwnerPhases() != 11 ||
      RecoveredArenaSeance_ActiveWorldReferencePhases() != 11 ||
      RecoveredArenaSeance_ActiveWorldEventPhases() != 0 ||
      RecoveredArenaSeance_ActiveWorldCreatedOwners() != 0 ||
      RecoveredArenaSeance_ActiveWorldMissionRecords() != 0 ||
      RecoveredArenaSeance_ActiveWorldMissionConditionReferences() != 0 ||
      RecoveredArenaSeance_ActiveWorldMissionRouteReferences() != 0 ||
      RecoveredArenaSeance_ActiveWorldMissionCheckEvents() != 0 ||
      RecoveredArenaSeance_ActiveWorldCorruptionRejects() != 1 ||
      RecoveredArenaSeance_ActiveWorldRollbacks() != 1 ||
      RecoveredArenaSeance_ActiveWorldContainerBytes() == 0 ||
      RecoveredArenaSeance_ActiveWorldFingerprint() == 0 ||
      !RecoveredArenaSeance_VehicleReady() ||
      RecoveredArenaSeance_VehicleActiveWorldReconstructedIDs() != 1 ||
      RecoveredArenaSeance_VehicleActiveWorldRollbacks() != 1 ||
      RecoveredArenaSeance_VehicleActiveWorldFingerprint() == 0 ||
      RecoveredArenaSeance_VehicleVesselMass() != 1000.0 ||
      RecoveredArenaSeance_Issues() != 0 ||
      RecoveredArenaSeance_ExtendedIssues() != 0 ||
      g_arena.searchSeanceClassTable("BirdAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Portal") == ct_NULLID ||
      g_arena.searchSeanceClassTable("OrphanAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Orphan") == ct_NULLID ||
      g_arena.searchSeanceClassTable("ArtefactAttr") == ct_NULLID ||
       g_arena.searchSeanceClassTable("SmokeAttr") == ct_NULLID ||
       g_arena.searchSeanceClassTable("Smoke") == ct_NULLID ||
      g_arena.searchSeanceClassTable("ExplosionAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Explosion") == ct_NULLID ||
      g_arena.searchSeanceClassTable("BulletAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Bullet") == ct_NULLID ||
      g_arena.searchSeanceClassTable("TaxiAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Taxi") != ct_NULLID ||
      g_arena.searchSeanceClassTable("SmokerAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("DynSmoker") == ct_NULLID ||
      g_arena.searchSeanceClassTable("WAVObj") == ct_NULLID ||
      g_arena.searchSeanceClassTable("SoundObj") == ct_NULLID ||
      g_arena.searchSeanceClassTable("FarterAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Farter") != ct_NULLID ||
      g_arena.searchSeanceClassTable("LampAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("CorpseAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Corpse") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Skin") == ct_NULLID ||
      g_arena.searchSeanceClassTable("SkinSpr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("SparkAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Spark") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Route") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Commander") == ct_NULLID ||
      g_arena.searchSeanceClassTable("TankGroup") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Tank") == ct_NULLID ||
      g_arena.searchSeanceClassTable("VehicleAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Vehicle") == ct_NULLID ||
      !context.isExist("Kingdom") || !context.isExist("Magician")) {
    RecoveredArenaSeance_Release();
    return false;
  }

  KR_ObjectID storage = context.searchObject("Storage");
  KR_ObjectID bird = context.searchObject("Bird.Attr.0");
  KR_ObjectID orphan = context.searchObject("Orphan.Attr.Default");
  KR_ObjectID artefact = context.searchObject("Artefact.Attr.0");
  KR_ObjectID flash = context.searchObject("Spark.Flash");
  KR_ObjectID smoke = context.searchObject("Smoke.Attr.Small");
  const bool smokeSimulation =
      SmokeSubjectState_SimulationSupported(
          &context, "Smoke.Attr.Trace") &&
      SmokeSubjectState_ProbeSimulationLifecycle(
          &context, "Smoke.Attr.Trace", Session::m_moment);
  KR_ObjectID explosion = context.searchObject("Expl.Test.0");
  AttributeExplosion* explosionAttribute =
      explosion.isNUL()
          ? nullptr
          : static_cast<AttributeExplosion*>(
                __attrExplosionTable.searchAttribute(explosion));
  KR_ObjectID vehicle = context.searchObject("Vehicle.Default");
  const bool soundLifecycle =
      SoundObjectState_ProbeLifecycle(
          &context, "wav.Explosion", Session::m_moment) &&
      SoundObjectState_LiveCount() == 0;
  const bool vehiclePublished =
      !storage.isNUL() && !bird.isNUL() && !orphan.isNUL() &&
      !artefact.isNUL() && !flash.isNUL() && !vehicle.isNUL() &&
      !smoke.isNUL() && !explosion.isNUL() &&
      BirdAttributeState_IsRetailDefault(bird) &&
      OrphanAttributeState_IsRetailDefault(orphan) &&
      ArtefactAttributeState_IsRetailDefault(artefact) &&
      smokeSimulation && SmokeSubjectState_LiveCount() == 0 &&
      SmokeAttributeState_IsRetailRoster(&context) &&
      ExplosionAttributeState_IsKnownRoster(&context) &&
      ExplosionAttributeState_RosterSize(&context) == 10 &&
      ExplosionSubjectState_TableReady(&context, 2) &&
      ExplosionSubjectState_LiveCount() == 0 &&
      ExplosionSubjectState_Fingerprint(&context) ==
          RecoveredArenaSeance_ExplosionSubjectFingerprint() &&
      ExplosionAttributeState_SoundReferencesResolved(&context) &&
      ExplosionAttributeState_IsKnownSoundReferenceRoster(&context) &&
      ExplosionAttributeState_SoundReferenceFingerprint(&context) ==
          RecoveredArenaSeance_ExplosionSoundReferenceFingerprint() &&
      ExplosionAttributeState_ParticleVisualsResolved(&context) &&
      ExplosionAttributeState_ParticleVisualFingerprint(&context) ==
          RecoveredArenaSeance_ExplosionParticleVisualFingerprint() &&
      ExplosionAttributeState_IsKnownParticleVisualRoster(&context) &&
      (ExplosionAttributeState_SmokeVisualsResolved(&context) ==
       expectVisualResources) &&
      ((ExplosionAttributeState_SmokeVisualFingerprint(&context) != 0) ==
       expectVisualResources) &&
      (!expectVisualResources ||
       ExplosionAttributeState_IsKnownSmokeVisualRoster(&context)) &&
      ExplosionSubjectState_ParticleBranchLiveCount() == 0 &&
      ExplosionSubjectState_ImpulseTargetReady(&context, vehicle) &&
      VehicleAttributeState_IsKnownRoster(&context) &&
      VehicleAttributeState_RosterSize(&context) == 3 &&
      VehicleAttributeState_Capacity() == 8 &&
      VehicleAttributeState_ReferencesResolved(&context) &&
      VehicleAttributeState_IsKnownReferenceRoster(&context) &&
      VehicleAttributeState_ReferenceFingerprint(&context) ==
          RecoveredArenaSeance_VehicleReferenceFingerprint() &&
      TaxiAttributeState_IsKnownRoster(&context) &&
      TaxiAttributeState_RosterSize(&context) == 2 &&
      TaxiAttributeState_Capacity() == 7 &&
      TaxiAttributeState_CachesUnresolved(&context) &&
      !TaxiAttributeState_ResolveReferences(&context) &&
      TaxiAttributeState_CachesUnresolved(&context) &&
      !TaxiAttributeState_ReferencesResolved(&context) &&
      BulletAttributeState_IsKnownRoster(&context) &&
      BulletAttributeState_RosterSize(&context) == 4 &&
      BulletAttributeState_Capacity() == 4 &&
      BulletAttributeState_SubjectCapacity() == 500 &&
      BulletAttributeState_SubjectTableReady(&context) &&
      BulletSubjectState_TableReady(&context, 500) &&
      BulletSubjectState_LiveCount() == 0 &&
      BulletSubjectState_Fingerprint(&context) ==
          RecoveredArenaSeance_BulletSubjectFingerprint() &&
      BulletAttributeState_CachesUnresolved(&context) &&
      !BulletAttributeState_ResolveReferences(&context) &&
      BulletAttributeState_CachesUnresolved(&context) &&
      !BulletAttributeState_ReferencesResolved(&context) &&
      SmokerAttributeState_IsKnownRoster(&context) &&
      SmokerAttributeState_RosterSize(&context) == 11 &&
      SmokerAttributeState_Capacity() == 11 &&
       SmokerAttributeState_ReferencesResolved(&context) &&
       SmokerAttributeState_RuntimeReady(&context) == expectVisualResources &&
      SmokerAttributeState_ReferenceFingerprint(&context) != 0 &&
      SmokerSubjectState_DynTableReady(&context, 62) &&
      SmokerSubjectState_DynCapacity() == 62 &&
      SmokerSubjectState_DynLiveCount() == 0 &&
      SmokerSubjectState_DynFingerprint(&context) != 0 &&
      WAVResourceState_AllLoaded(&context) &&
      WAVResourceState_RosterSize(&context) == 5 &&
      WAVResourceState_Capacity() == 30 &&
      WAVResourceState_Fingerprint(&context) ==
          RecoveredArenaSeance_WavCatalogFingerprint() &&
      soundLifecycle &&
      SoundObjectState_TableReady(&context, 250) &&
      SoundObjectState_Fingerprint(&context) ==
          RecoveredArenaSeance_SoundObjectFingerprint() &&
      FarterAttributeState_IsKnownRoster(&context) &&
      FarterAttributeState_RosterSize(&context) == 0 &&
      FarterAttributeState_Capacity() == 10 &&
      FarterAttributeState_ReferencesResolved(&context) &&
      FarterAttributeState_RuntimeReady(&context) &&
      FarterAttributeState_ReferenceFingerprint(&context) != 0 &&
      LampAttributeState_IsKnownRoster(&context) &&
      LampAttributeState_RosterSize(&context) == 10 &&
      LampAttributeState_Capacity() == 10 &&
      CorpseAttributeState_IsKnownRoster(&context) &&
      CorpseAttributeState_RosterSize(&context) == 2 &&
      CorpseAttributeState_Capacity() == 3 &&
      CorpseAttributeState_CachesUnresolved(&context) &&
      !CorpseAttributeState_ReferencesResolved(&context) &&
      CorpseSubjectState_TableReady(&context, 100) &&
      CorpseSubjectState_LiveCount() == 0 &&
      explosionAttribute != nullptr &&
      explosionAttribute->m_useLight == 1 &&
      explosionAttribute->m_impulseCoeff == 1234 &&
      ((explosionAttribute->m_hTexture != nullptr) ==
       expectVisualResources) &&
      explosionAttribute->m_cacheSkin == nullptr &&
      ExplosionAttributeState_PieceCachesUnresolved(&context) &&
      !ExplosionAttributeState_PieceReferencesResolved(&context) &&
      WAVResourceState_IsLoadedPointer(explosionAttribute->m_wav) &&
      explosionAttribute->m_ctsndID ==
          g_arena.searchSeanceClassTable("SoundObj") &&
      g_vehicle != nullptr &&
       context.queryInterface(vehicle, IVehicleIID) == g_vehicle;
  const bool dynamicBulletCollision =
      ProbeDynamicBulletCollision(context);
  const unsigned long long smokeSubjectFingerprint =
      SmokeSubjectState_Fingerprint(&context);
  const unsigned long long smokeVisualFingerprint =
      RecoveredArenaSeance_SmokeVisualResourceFingerprint();
  const unsigned long long explosionFingerprint =
      ExplosionAttributeState_Fingerprint(&context);
  const unsigned long long explosionSubjectFingerprint =
      ExplosionSubjectState_Fingerprint(&context);
  const unsigned long long explosionSoundFingerprint =
      ExplosionAttributeState_SoundReferenceFingerprint(&context);
  const unsigned long long explosionParticleFingerprint =
      ExplosionAttributeState_ParticleVisualFingerprint(&context);
  const unsigned long long explosionSmokeFingerprint =
      ExplosionAttributeState_SmokeVisualFingerprint(&context);
  const unsigned long long vehicleFingerprint =
      VehicleAttributeState_Fingerprint(&context);
  const unsigned long long vehicleReferenceFingerprint =
      VehicleAttributeState_ReferenceFingerprint(&context);
  const unsigned long long taxiFingerprint =
      TaxiAttributeState_Fingerprint(&context);
  const unsigned long long bulletFingerprint =
      BulletAttributeState_Fingerprint(&context);
  const unsigned long long bulletReferenceFingerprint =
      BulletAttributeState_ReferenceFingerprint(&context);
  const unsigned long long bulletSubjectFingerprint =
      BulletSubjectState_Fingerprint(&context);
  const unsigned long long farterFingerprint =
      FarterAttributeState_Fingerprint(&context);
  const unsigned long long farterReferenceFingerprint =
      FarterAttributeState_ReferenceFingerprint(&context);
  const unsigned long long smokerFingerprint =
      SmokerAttributeState_Fingerprint(&context);
  const unsigned long long smokerReferenceFingerprint =
      SmokerAttributeState_ReferenceFingerprint(&context);
  const unsigned long long dynSmokerFingerprint =
      SmokerSubjectState_DynFingerprint(&context);
  const unsigned long long wavFingerprint =
      WAVResourceState_Fingerprint(&context);
  const unsigned long long soundObjectFingerprint =
      SoundObjectState_Fingerprint(&context);
  const unsigned long long farterSubjectFingerprint =
      RecoveredArenaSeance_FarterSubjectFingerprint();
  const unsigned long long lampFingerprint =
      LampAttributeState_Fingerprint(&context);
  const unsigned long long corpseFingerprint =
      CorpseAttributeState_Fingerprint(&context);
  const unsigned long long corpseSubjectFingerprint =
      CorpseSubjectState_Fingerprint(&context);
  const unsigned long long skinCatalogFingerprint =
      RecoveredArenaSeance_SkinCatalogFingerprint();
  const bool reconstructionStable =
      (g_explosionFixtureFingerprint == 0 ||
       g_explosionFixtureFingerprint == explosionFingerprint) &&
      (g_explosionSubjectFixtureFingerprint == 0 ||
       g_explosionSubjectFixtureFingerprint == explosionSubjectFingerprint) &&
      (g_explosionSoundFixtureFingerprint == 0 ||
       g_explosionSoundFixtureFingerprint == explosionSoundFingerprint) &&
      (g_explosionParticleFixtureFingerprint == 0 ||
       g_explosionParticleFixtureFingerprint ==
           explosionParticleFingerprint) &&
      (g_explosionSmokeFixtureFingerprint == 0 ||
       g_explosionSmokeFixtureFingerprint == explosionSmokeFingerprint) &&
      (g_vehicleFixtureFingerprint == 0 ||
       g_vehicleFixtureFingerprint == vehicleFingerprint) &&
      (g_vehicleReferenceFixtureFingerprint == 0 ||
       g_vehicleReferenceFixtureFingerprint ==
           vehicleReferenceFingerprint) &&
      (g_taxiFixtureFingerprint == 0 ||
       g_taxiFixtureFingerprint == taxiFingerprint) &&
      (g_bulletFixtureFingerprint == 0 ||
       g_bulletFixtureFingerprint == bulletFingerprint) &&
      (g_bulletReferenceFixtureFingerprint == 0 ||
       g_bulletReferenceFixtureFingerprint ==
           bulletReferenceFingerprint) &&
      (g_bulletSubjectFixtureFingerprint == 0 ||
       g_bulletSubjectFixtureFingerprint == bulletSubjectFingerprint) &&
      (g_farterFixtureFingerprint == 0 ||
       g_farterFixtureFingerprint == farterFingerprint) &&
      (g_farterReferenceFixtureFingerprint == 0 ||
       g_farterReferenceFixtureFingerprint == farterReferenceFingerprint) &&
      (g_smokerFixtureFingerprint == 0 ||
       g_smokerFixtureFingerprint == smokerFingerprint) &&
      (g_smokerReferenceFixtureFingerprint == 0 ||
       g_smokerReferenceFixtureFingerprint == smokerReferenceFingerprint) &&
      (g_smokeSubjectFixtureFingerprint == 0 ||
       g_smokeSubjectFixtureFingerprint == smokeSubjectFingerprint) &&
      (g_smokeVisualFixtureFingerprint == 0 ||
       g_smokeVisualFixtureFingerprint == smokeVisualFingerprint) &&
      (g_dynSmokerFixtureFingerprint == 0 ||
       g_dynSmokerFixtureFingerprint == dynSmokerFingerprint) &&
      (g_wavFixtureFingerprint == 0 ||
       g_wavFixtureFingerprint == wavFingerprint) &&
      (g_soundObjectFixtureFingerprint == 0 ||
       g_soundObjectFixtureFingerprint == soundObjectFingerprint) &&
      (g_farterSubjectFixtureFingerprint == 0 ||
       g_farterSubjectFixtureFingerprint == farterSubjectFingerprint) &&
      (g_lampFixtureFingerprint == 0 ||
       g_lampFixtureFingerprint == lampFingerprint) &&
      (g_corpseFixtureFingerprint == 0 ||
       g_corpseFixtureFingerprint == corpseFingerprint) &&
      (g_corpseSubjectFixtureFingerprint == 0 ||
       g_corpseSubjectFixtureFingerprint == corpseSubjectFingerprint) &&
      (g_skinCatalogFixtureFingerprint == 0 ||
       g_skinCatalogFixtureFingerprint == skinCatalogFingerprint);
  g_explosionFixtureFingerprint = explosionFingerprint;
  g_explosionSubjectFixtureFingerprint = explosionSubjectFingerprint;
  g_explosionSoundFixtureFingerprint = explosionSoundFingerprint;
  g_explosionParticleFixtureFingerprint = explosionParticleFingerprint;
  g_explosionSmokeFixtureFingerprint = explosionSmokeFingerprint;
  g_vehicleFixtureFingerprint = vehicleFingerprint;
  g_vehicleReferenceFixtureFingerprint = vehicleReferenceFingerprint;
  g_taxiFixtureFingerprint = taxiFingerprint;
  g_bulletFixtureFingerprint = bulletFingerprint;
  g_bulletReferenceFixtureFingerprint = bulletReferenceFingerprint;
  g_bulletSubjectFixtureFingerprint = bulletSubjectFingerprint;
  g_farterFixtureFingerprint = farterFingerprint;
  g_farterReferenceFixtureFingerprint = farterReferenceFingerprint;
  g_smokerFixtureFingerprint = smokerFingerprint;
  g_smokerReferenceFixtureFingerprint = smokerReferenceFingerprint;
  g_smokeSubjectFixtureFingerprint = smokeSubjectFingerprint;
  g_smokeVisualFixtureFingerprint = smokeVisualFingerprint;
  g_dynSmokerFixtureFingerprint = dynSmokerFingerprint;
  g_wavFixtureFingerprint = wavFingerprint;
  g_soundObjectFixtureFingerprint = soundObjectFingerprint;
  g_farterSubjectFixtureFingerprint = farterSubjectFingerprint;
  g_lampFixtureFingerprint = lampFingerprint;
  g_corpseFixtureFingerprint = corpseFingerprint;
  g_corpseSubjectFixtureFingerprint = corpseSubjectFingerprint;
  g_skinCatalogFixtureFingerprint = skinCatalogFingerprint;

  RecoveredArenaSeance_Release();
  RecoveredArenaSeance_Release();
  return vehiclePublished && dynamicBulletCollision && reconstructionStable &&
         IsReleased(context) &&
         snd_distMax == previousSoundDistance &&
         snd_distMax2 == previousSoundDistanceSquared;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 18) {
    return Fail("expected a fixture directory and thirteen retail-script sources");
  }

  RecoveredArenaSeance_Release();
  if (!ValidateWavPayloadCompatibility()) {
    return Fail("January/May WAV event payload compatibility failed");
  }
  if (RecoveredArenaSeance_Initialize(nullptr, 0.0) != FALSE ||
      RecoveredArenaSeance_Issues() !=
          RECOVERED_ARENA_SEANCE_INVALID_CONTEXT ||
      RecoveredArenaSeance_ExtendedIssues() != 0 ||
      RecoveredArenaSeance_IsOpen() || g_vehicle != nullptr) {
    return Fail("null context did not fail transactionally");
  }

  std::string originalDirectory;
  std::string fixtureDirectory;
  std::string levelDirectory;
  if (!CurrentDirectory(originalDirectory) ||
      !FullPath(argv[1], fixtureDirectory) ||
      !EnsureDirectory(fixtureDirectory)) {
    return Fail("could not establish the fixture directory");
  }
  levelDirectory = JoinPath(fixtureDirectory, "Level.Fixture");
  if (!EnsureDirectory(levelDirectory)) {
    return Fail("could not establish the fixture Level directory");
  }

  // These legacy single-byte comment characters reproduce the exact ctype
  // edge case present in retail vessels.cfg while the empty sections retain
  // the recovered vehicle dynamics defaults.
  const char fixture[] =
      "# legacy \xAC\xA8\xE0\r\n"
      "[Dragon]\r\n"
      "[Emv0]\r\n"
      "[Walk0]\r\n"
      "[Tank1]\r\n"
      "[Tank2]\r\n"
      "[Tank3]\r\n"
      "[Dead]\r\n";
  const char explosionRootFixture[] =
      "func void ExplosionFixtureRoot()\r\n"
      "{\r\n"
      "}\r\n";
  const char explosionLevelFixture[] =
      "func void main_CreateExplosionAttr()\r\n"
      " var int ctID, objectID, cachePos;\r\n"
      "{\r\n"
      "  s_AddClassTable(\"Explosion\", 2);\r\n"
      "  ctID := s_AddClassTable(\"ExplosionAttr\", 10);\r\n"
      "  New(ctID, \"Expl.Test.0\", objectID, cachePos);\r\n"
      "  SetAttribute_i(objectID, cachePos, \"m_useLight\", 1);\r\n"
      "  SetAttribute_f(objectID, cachePos, \"m_impulseCoeff\", 1234);\r\n"
      "  SetAttribute_s(objectID, cachePos, \"m_soundName\", \"wav.Explosion\");\r\n"
      "  New(ctID, \"Expl.Test.1\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.2\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.3\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.4\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.5\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.6\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.7\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.8\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.9\", objectID, cachePos);\r\n"
      "}\r\n";
  const char skinLevelFixture[] =
      "func void main_LoadSkin()\r\n"
      " var int ctIDSkin;\r\n"
      "{\r\n"
      " ctIDSkin := s_AddClassTable(\"Skin\",1);\r\n"
      " ctIDSkin := s_AddClassTable(\"SkinSpr\",1);\r\n"
      "}\r\n";
  const char invalidSkinLevelFixture[] =
      "func void main_LoadSkin()\r\n"
      " var int ctIDSkin;\r\n"
      "{\r\n"
      " ctIDSkin := s_AddClassTable(\"Skin\",2);\r\n"
      " ctIDSkin := s_AddClassTable(\"SkinSpr\",1);\r\n"
      "}\r\n";
  const char setTankFixture[] =
      "func void main_CreateTanks()\r\n"
      "{\r\n"
      " s_AddClassTable(\"TankGroup\",30);\r\n"
      " s_AddClassTable(\"Tank\",64);\r\n"
      "}\r\n";
  const char emptyPeopleSupportFixture[] =
      "// Level.03N source-only fixture has no populated People helpers\r\n";
  const std::string config = JoinPath(levelDirectory, "vessels.cfg");
  const std::string smokeSprite = JoinPath(levelDirectory, "SMOKE.SPR");
  const std::string flameSprite = JoinPath(levelDirectory, "FLAME.SPR");
  const std::string coronaSprite = JoinPath(levelDirectory, "CORONA.SPR");
  const std::string smokeCopy = JoinPath(fixtureDirectory, "SMOKE.SCI");
  const std::string explosionCopy =
      JoinPath(fixtureDirectory, "EXPLOSION.SCI");
  const std::string farterCopy = JoinPath(fixtureDirectory, "FARTER.SCI");
  const std::string lampCopy = JoinPath(fixtureDirectory, "LAMP.SCI");
  const std::string bulletCopy = JoinPath(fixtureDirectory, "BULLET.SCI");
  const std::string scincDirectory = JoinPath(levelDirectory, "SCINC");
  const std::string explosionLocalCopy =
      JoinPath(scincDirectory, "EXPLOSION_LOC.SCI");
  const std::string farterLocalCopy =
      JoinPath(scincDirectory, "FARTERATTR.SCI");
  const std::string taxiCopy = JoinPath(scincDirectory, "TAXI.SCI");
  const std::string vehicleCopy = JoinPath(scincDirectory, "VEHICLE.SCI");
  const std::string bulletLocalCopy =
      JoinPath(scincDirectory, "bullet_loc.sci");
  const std::string farterSetCopy =
      JoinPath(scincDirectory, "SET_FARTER.SCI");
  const std::string corpseCopy = JoinPath(scincDirectory, "CORPSE.SCI");
  const std::string skinCopy = JoinPath(scincDirectory, "SKIN.SCI");
  const std::string localMainCopy =
      JoinPath(scincDirectory, "LOCALMAIN.SCI");
  const std::string loadWavCopy = JoinPath(scincDirectory, "LOADWAV.SCI");
  const std::string routeCopy = JoinPath(scincDirectory, "load_route.sci");
  const std::string peopleCopy = JoinPath(scincDirectory, "PEOPLE.SCI");
  const std::string setPeopleCopy = JoinPath(scincDirectory, "set_people.sci");
  const std::string tankCopy = JoinPath(scincDirectory, "TANK.SCI");
  const std::string setTankCopy = JoinPath(scincDirectory, "set_tank.sci");
  const std::string unitsCopy = JoinPath(scincDirectory, "units.sci");
  const std::string sysfCopy = JoinPath(fixtureDirectory, "SYSF.SCI");
  DeleteFileA(smokeCopy.c_str());
  DeleteFileA(smokeSprite.c_str());
  DeleteFileA(flameSprite.c_str());
  DeleteFileA(coronaSprite.c_str());
  DeleteFileA(explosionCopy.c_str());
  DeleteFileA(farterCopy.c_str());
  DeleteFileA(lampCopy.c_str());
  DeleteFileA(bulletCopy.c_str());
  if (!EnsureDirectory(scincDirectory)) {
    return Fail("could not establish the fixture SCINC directory");
  }
  DeleteFileA(explosionLocalCopy.c_str());
  DeleteFileA(farterLocalCopy.c_str());
  DeleteFileA(taxiCopy.c_str());
  DeleteFileA(vehicleCopy.c_str());
  DeleteFileA(bulletLocalCopy.c_str());
  DeleteFileA(farterSetCopy.c_str());
  DeleteFileA(corpseCopy.c_str());
  DeleteFileA(skinCopy.c_str());
  DeleteFileA(localMainCopy.c_str());
  DeleteFileA(loadWavCopy.c_str());
  DeleteFileA(routeCopy.c_str());
  DeleteFileA(peopleCopy.c_str());
  DeleteFileA(setPeopleCopy.c_str());
  DeleteFileA(tankCopy.c_str());
  DeleteFileA(setTankCopy.c_str());
  DeleteFileA(unitsCopy.c_str());
  DeleteFileA(sysfCopy.c_str());
  if (!WriteFile(config, fixture) ||
      !WriteFile(unitsCopy, emptyPeopleSupportFixture) ||
      !WriteFile(sysfCopy, emptyPeopleSupportFixture) ||
      SetCurrentDirectoryA(levelDirectory.c_str()) == FALSE) {
    return Fail("could not prepare retail-script Arena fixture");
  }

  Session::m_moment = 0.0;
  SimulationContext missingWavSourceContext(64, 128);
  const bool missingWavSourceRejected =
      RecoveredArenaSeance_Initialize(&missingWavSourceContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_WAV_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingWavSourceContext);
  if (CopyFileA(argv[7], localMainCopy.c_str(), FALSE) == FALSE ||
      CopyFileA(argv[8], loadWavCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy WAV metadata sources into Arena fixture");
  }
  if (CopyFileA(argv[14], routeCopy.c_str(), FALSE) == FALSE ||
      CopyFileA(argv[15], peopleCopy.c_str(), FALSE) == FALSE ||
      CopyFileA(argv[16], setPeopleCopy.c_str(), FALSE) == FALSE ||
      CopyFileA(argv[17], tankCopy.c_str(), FALSE) == FALSE) {
    return Fail("could not copy Level-local Route/People/Tank sources");
  }

  SimulationContext missingSetTankContext(64, 128);
  const bool missingSetTankRejected =
      RecoveredArenaSeance_Initialize(&missingSetTankContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SOURCE_UNAVAILABLE) != 0 &&
      RecoveredArenaSeance_Issues() == 0 && IsReleased(missingSetTankContext);
  if (!WriteFile(setTankCopy, setTankFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write Level-local set_tank.sci fixture");
  }

  SimulationContext missingVehicleContext(64, 128);
  const bool missingVehicleRejected =
      RecoveredArenaSeance_Initialize(&missingVehicleContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      RecoveredArenaSeance_Issues() == 0 && IsReleased(missingVehicleContext);
  if (CopyFileA(argv[11], vehicleCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy VEHICLE.SCI into Arena fixture");
  }

  SimulationContext missingSourceContext(64, 128);
  const bool missingSourceRejected =
      RecoveredArenaSeance_Initialize(&missingSourceContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingSourceContext);
  if (CopyFileA(argv[2], smokeCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy retail SMOKE.SCI into Arena fixture");
  }

  SimulationContext missingExplosionRootContext(64, 128);
  const bool missingExplosionRootRejected =
      RecoveredArenaSeance_Initialize(&missingExplosionRootContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingExplosionRootContext);
  if (!WriteFile(explosionCopy, explosionRootFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write bounded Explosion root fixture");
  }

  SimulationContext missingExplosionLocalContext(64, 128);
  const bool missingExplosionLocalRejected =
      RecoveredArenaSeance_Initialize(&missingExplosionLocalContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingExplosionLocalContext);
  if (!WriteFile(explosionLocalCopy, explosionLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write bounded Explosion Level fixture");
  }

  SimulationContext missingTaxiContext(64, 128);
  const bool missingTaxiRejected =
      RecoveredArenaSeance_Initialize(&missingTaxiContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      RecoveredArenaSeance_Issues() == 0 && IsReleased(missingTaxiContext);
  if (CopyFileA(argv[10], taxiCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy TAXI.SCI into Arena fixture");
  }

  SimulationContext missingFarterRootContext(64, 128);
  const bool missingFarterRootRejected =
      RecoveredArenaSeance_Initialize(&missingFarterRootContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingFarterRootContext);
  if (CopyFileA(argv[3], farterCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy FARTER.SCI into Arena fixture");
  }

  SimulationContext missingFarterLocalContext(64, 128);
  const bool missingFarterLocalRejected =
      RecoveredArenaSeance_Initialize(&missingFarterLocalContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingFarterLocalContext);
  if (CopyFileA(argv[5], farterLocalCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy FARTERATTR.SCI into Arena fixture");
  }

  SimulationContext missingLampContext(64, 128);
  const bool missingLampRejected =
      RecoveredArenaSeance_Initialize(&missingLampContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingLampContext);
  if (CopyFileA(argv[4], lampCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy LAMP.SCI into Arena fixture");
  }

  SimulationContext missingCorpseContext(64, 128);
  const bool missingCorpseRejected =
      RecoveredArenaSeance_Initialize(&missingCorpseContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_CORPSE_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingCorpseContext);
  if (CopyFileA(argv[6], corpseCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy CORPSE.SCI into Arena fixture");
  }

  SimulationContext missingBulletRootContext(64, 128);
  const bool missingBulletRootRejected =
      RecoveredArenaSeance_Initialize(&missingBulletRootContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      RecoveredArenaSeance_Issues() == 0 &&
      IsReleased(missingBulletRootContext);
  if (CopyFileA(argv[12], bulletCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy BULLET.SCI into Arena fixture");
  }

  SimulationContext missingBulletLocalContext(64, 128);
  const bool missingBulletLocalRejected =
      RecoveredArenaSeance_Initialize(&missingBulletLocalContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      RecoveredArenaSeance_Issues() == 0 &&
      IsReleased(missingBulletLocalContext);
  if (CopyFileA(argv[13], bulletLocalCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy bullet_loc.sci into Arena fixture");
  }

  SimulationContext missingSkinCatalogContext(64, 128);
  const bool missingSkinCatalogRejected =
      RecoveredArenaSeance_Initialize(&missingSkinCatalogContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SKIN_CATALOG_INVALID) != 0 &&
      IsReleased(missingSkinCatalogContext);
  if (!WriteFile(skinCopy, invalidSkinLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Skin catalog fixture");
  }
  SimulationContext invalidSkinCatalogContext(64, 128);
  const bool invalidSkinCatalogRejected =
      RecoveredArenaSeance_Initialize(&invalidSkinCatalogContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SKIN_RESOURCE_ROSTER_INVALID) != 0 &&
      IsReleased(invalidSkinCatalogContext);
  if (!WriteFile(skinCopy, skinLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore bounded Skin catalog fixture");
  }

  SimulationContext missingFarterSubjectContext(64, 128);
  const bool missingFarterSubjectRejected =
      RecoveredArenaSeance_Initialize(&missingFarterSubjectContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE) != 0 &&
      IsReleased(missingFarterSubjectContext);
  if (CopyFileA(argv[9], farterSetCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy SET_FARTER.SCI into Arena fixture");
  }

  std::string invalidFarterSubjectFixture;
  if (!ReadFile(argv[9], invalidFarterSubjectFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not read Farter subject fixture for corruption");
  }
  invalidFarterSubjectFixture.append("\r\n/*");
  if (!WriteFile(farterSetCopy, invalidFarterSubjectFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Farter subject fixture");
  }
  const double preInvalidFarterDistance = snd_distMax;
  const double preInvalidFarterDistanceSquared = snd_distMax2;
  SimulationContext invalidFarterSubjectContext(64, 128);
  const bool invalidFarterSubjectRejected =
      RecoveredArenaSeance_Initialize(&invalidFarterSubjectContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE) != 0 &&
      IsReleased(invalidFarterSubjectContext) &&
      snd_distMax == preInvalidFarterDistance &&
      snd_distMax2 == preInvalidFarterDistanceSquared;
  if (CopyFileA(argv[9], farterSetCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid SET_FARTER.SCI fixture");
  }

  std::string invalidBulletFixture;
  if (!ReadFile(argv[13], invalidBulletFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not read Bullet fixture for deterministic corruption");
  }
  const std::string bulletNeedle =
      "s_NewObject( ctIDAttr, \"Bullet.Led\" );";
  const std::size_t bulletName = invalidBulletFixture.find(bulletNeedle);
  if (bulletName == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt Bullet fixture deterministically");
  }
  invalidBulletFixture.replace(
      bulletName, bulletNeedle.size(),
      "s_NewObject( ctIDAttr, \"Bullet.Led.Corrupt\" );");
  if (!WriteFile(bulletLocalCopy, invalidBulletFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Bullet fixture");
  }
  SimulationContext invalidBulletRosterContext(64, 128);
  const bool invalidBulletRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidBulletRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_ROSTER_INVALID) != 0 &&
      RecoveredArenaSeance_Issues() == 0 &&
      IsReleased(invalidBulletRosterContext);
  if (CopyFileA(argv[13], bulletLocalCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Bullet fixture");
  }

  const char tablelessBulletFixture[] =
      "func void main_CreateBullets()\r\n"
      "{\r\n"
      "}\r\n";
  if (!WriteFile(bulletLocalCopy, tablelessBulletFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write tableless Bullet fixture");
  }
  SimulationContext tablelessBulletContext(64, 128);
  const bool tablelessBulletRejected =
      RecoveredArenaSeance_Initialize(&tablelessBulletContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_TABLE_MISSING) != 0 &&
      RecoveredArenaSeance_Issues() == 0 &&
      IsReleased(tablelessBulletContext);
  if (CopyFileA(argv[13], bulletLocalCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore Bullet fixture after tableless probe");
  }

  std::string invalidExplosionLevelFixture = explosionLevelFixture;
  const std::size_t impulse = invalidExplosionLevelFixture.find("1234");
  if (impulse == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt Explosion fixture deterministically");
  }
  invalidExplosionLevelFixture.replace(impulse, 4, "1235");
  if (!WriteFile(explosionLocalCopy,
                 invalidExplosionLevelFixture.c_str())) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Explosion Level fixture");
  }
  SimulationContext invalidExplosionRosterContext(64, 128);
  const bool invalidExplosionRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidExplosionRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_ROSTER_INVALID) != 0 &&
      IsReleased(invalidExplosionRosterContext);
  if (!WriteFile(explosionLocalCopy, explosionLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Explosion Level fixture");
  }

  std::string invalidTaxiFixture;
  if (!ReadFile(argv[10], invalidTaxiFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not read Taxi fixture for deterministic corruption");
  }
  const std::string taxiNeedle = "sk.Taxi.akula";
  const std::size_t taxiSkin = invalidTaxiFixture.find(taxiNeedle);
  if (taxiSkin == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt Taxi fixture deterministically");
  }
  invalidTaxiFixture.replace(taxiSkin, taxiNeedle.size(), "sk.Taxi.akula2");
  if (!WriteFile(taxiCopy, invalidTaxiFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Taxi fixture");
  }
  SimulationContext invalidTaxiRosterContext(64, 128);
  const bool invalidTaxiRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidTaxiRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_ROSTER_INVALID) != 0 &&
      RecoveredArenaSeance_Issues() == 0 &&
      IsReleased(invalidTaxiRosterContext);
  if (CopyFileA(argv[10], taxiCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Taxi fixture");
  }

  const char tablelessTaxiFixture[] =
      "func void main_CreateTaxiAttr()\r\n"
      "{\r\n"
      "}\r\n";
  if (!WriteFile(taxiCopy, tablelessTaxiFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write tableless Taxi fixture");
  }
  SimulationContext tablelessTaxiContext(64, 128);
  const bool tablelessTaxiRejected =
      RecoveredArenaSeance_Initialize(&tablelessTaxiContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_TABLE_MISSING) != 0 &&
      RecoveredArenaSeance_Issues() == 0 && IsReleased(tablelessTaxiContext);
  if (CopyFileA(argv[10], taxiCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore Taxi fixture after tableless probe");
  }

  std::string invalidVehicleFixture;
  if (!ReadFile(argv[11], invalidVehicleFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not read Vehicle fixture for deterministic corruption");
  }
  const std::string vehicleNeedle = "Vehicle.Attr.Akula";
  const std::size_t vehicleName = invalidVehicleFixture.find(vehicleNeedle);
  if (vehicleName == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt Vehicle fixture deterministically");
  }
  invalidVehicleFixture.replace(vehicleName, vehicleNeedle.size(),
                                "Vehicle.Attr.Akula2");
  if (!WriteFile(vehicleCopy, invalidVehicleFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Vehicle fixture");
  }
  SimulationContext invalidVehicleRosterContext(64, 128);
  const bool invalidVehicleRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidVehicleRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_ROSTER_INVALID) != 0 &&
      RecoveredArenaSeance_Issues() == 0 &&
      IsReleased(invalidVehicleRosterContext);
  if (CopyFileA(argv[11], vehicleCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Vehicle fixture");
  }

  const char tablelessVehicleFixture[] =
      "func void main_CreateVehicleAttr()\r\n"
      "{\r\n"
      "}\r\n"
      "func void main_CreateVehicle()\r\n"
      "{\r\n"
      "}\r\n";
  if (!WriteFile(vehicleCopy, tablelessVehicleFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write tableless Vehicle fixture");
  }
  SimulationContext tablelessVehicleContext(64, 128);
  const bool tablelessVehicleRejected =
      RecoveredArenaSeance_Initialize(&tablelessVehicleContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_ExtendedIssues() &
       RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_TABLE_MISSING) != 0 &&
      RecoveredArenaSeance_Issues() == 0 &&
      IsReleased(tablelessVehicleContext);
  if (CopyFileA(argv[11], vehicleCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore Vehicle fixture after tableless probe");
  }

  std::string invalidLampFixture;
  if (!ReadFile(argv[4], invalidLampFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not read Lamp fixture for deterministic corruption");
  }
  const std::string lampNeedle = "\"m_particleWidth\",0.5";
  const std::size_t particleWidth = invalidLampFixture.find(lampNeedle);
  if (particleWidth == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt Lamp fixture deterministically");
  }
  invalidLampFixture.replace(particleWidth, lampNeedle.size(),
                             "\"m_particleWidth\",0.6");
  if (!WriteFile(lampCopy, invalidLampFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Lamp fixture");
  }
  SimulationContext invalidLampRosterContext(64, 128);
  const bool invalidLampRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidLampRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_ROSTER_INVALID) != 0 &&
      IsReleased(invalidLampRosterContext);
  if (CopyFileA(argv[4], lampCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Lamp fixture");
  }

  std::string invalidSmokerFixture;
  if (!ReadFile(argv[2], invalidSmokerFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not read Smoker fixture for deterministic corruption");
  }
  const std::string smokerNeedle =
      "0.1, 1.8, \"Smoke.Attr.Volcano\"";
  const std::size_t smokerRate = invalidSmokerFixture.find(smokerNeedle);
  if (smokerRate == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt Smoker fixture deterministically");
  }
  invalidSmokerFixture.replace(smokerRate, smokerNeedle.size(),
                               "0.2, 1.8, \"Smoke.Attr.Volcano\"");
  if (!WriteFile(smokeCopy, invalidSmokerFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Smoker fixture");
  }
  SimulationContext invalidSmokerRosterContext(64, 128);
  const bool invalidSmokerRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidSmokerRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SMOKER_ATTRIBUTE_ROSTER_INVALID) != 0 &&
      IsReleased(invalidSmokerRosterContext);
  if (CopyFileA(argv[2], smokeCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Smoker fixture");
  }

  std::string invalidWavFixture;
  if (!ReadFile(argv[8], invalidWavFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not read WAV fixture for deterministic corruption");
  }
  const std::string wavNeedle = "Ambient.wav";
  const std::size_t wavFile = invalidWavFixture.find(wavNeedle);
  if (wavFile == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt WAV fixture deterministically");
  }
  invalidWavFixture.replace(wavFile, wavNeedle.size(), "Ambient2.wav");
  if (!WriteFile(loadWavCopy, invalidWavFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid WAV metadata fixture");
  }
  SimulationContext invalidWavRosterContext(64, 128);
  const bool invalidWavRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidWavRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_WAV_ROSTER_INVALID) != 0 &&
      IsReleased(invalidWavRosterContext);
  if (CopyFileA(argv[8], loadWavCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid WAV metadata fixture");
  }

  const bool sourceOnlyCycle = RunCycle(false);
  if (!WriteSprite(smokeSprite, 3)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write partial Smoke visual fixture");
  }
  SimulationContext partialSmokeVisualContext(64, 128);
  const bool partialSmokeVisualRejected =
      RecoveredArenaSeance_Initialize(&partialSmokeVisualContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_INVALID) != 0 &&
      IsReleased(partialSmokeVisualContext);

  if (!WriteSprite(flameSprite, 7) || !WriteSprite(coronaSprite, 11) ||
      !WriteFile(coronaSprite, "invalid")) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Smoke visual fixture");
  }
  SimulationContext invalidSmokeVisualContext(64, 128);
  const bool invalidSmokeVisualRejected =
      RecoveredArenaSeance_Initialize(&invalidSmokeVisualContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_INVALID) != 0 &&
      IsReleased(invalidSmokeVisualContext);
  if (!WriteSprite(coronaSprite, 11)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Smoke visual fixture");
  }

  SDeviceDescr device = {};
  device.swHw = GR_HARDWARE;
  SDeviceDescr* const previousDevice = _dL.currDevice;
  _dL.currDevice = &device;
  auto previousTextureLoader = _pGRLoadTextureToDB;
  _pGRLoadTextureToDB = nullptr;
  SimulationContext failedSmokeVisualLoadContext(64, 128);
  const bool failedSmokeVisualLoadRejected =
      RecoveredArenaSeance_Initialize(&failedSmokeVisualLoadContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_LOAD_FAILURE) != 0 &&
      IsReleased(failedSmokeVisualLoadContext);
  _pGRLoadTextureToDB = previousTextureLoader;
  const bool firstCycle = RunCycle(true);
  const bool secondCycle = firstCycle && RunCycle(true);
  _dL.currDevice = previousDevice;
  const bool restored =
      SetCurrentDirectoryA(originalDirectory.c_str()) != FALSE;
  DeleteFileA(config.c_str());
  DeleteFileA(smokeCopy.c_str());
  DeleteFileA(explosionCopy.c_str());
  DeleteFileA(farterCopy.c_str());
  DeleteFileA(lampCopy.c_str());
  DeleteFileA(bulletCopy.c_str());
  DeleteFileA(explosionLocalCopy.c_str());
  DeleteFileA(farterLocalCopy.c_str());
  DeleteFileA(taxiCopy.c_str());
  DeleteFileA(vehicleCopy.c_str());
  DeleteFileA(bulletLocalCopy.c_str());
  DeleteFileA(farterSetCopy.c_str());
  DeleteFileA(corpseCopy.c_str());
  DeleteFileA(skinCopy.c_str());
  DeleteFileA(localMainCopy.c_str());
  DeleteFileA(loadWavCopy.c_str());
  DeleteFileA(routeCopy.c_str());
  DeleteFileA(peopleCopy.c_str());
  DeleteFileA(setPeopleCopy.c_str());
  DeleteFileA(setTankCopy.c_str());
  DeleteFileA(unitsCopy.c_str());
  DeleteFileA(smokeSprite.c_str());
  DeleteFileA(flameSprite.c_str());
  DeleteFileA(coronaSprite.c_str());
  RemoveDirectoryA(scincDirectory.c_str());
  RemoveDirectoryA(levelDirectory.c_str());
  DeleteFileA(sysfCopy.c_str());
  RemoveDirectoryA(fixtureDirectory.c_str());

  if (!missingSourceRejected) {
    return Fail("missing retail SMOKE.SCI was not rejected transactionally");
  }
  if (!missingWavSourceRejected) {
    return Fail("missing WAV metadata sources were not rejected transactionally");
  }
  if (!missingSetTankRejected) {
    return Fail("missing set_tank.sci was not rejected transactionally");
  }
  if (!missingExplosionRootRejected || !missingExplosionLocalRejected) {
    return Fail("missing retail Explosion fragments were not rejected "
                "transactionally");
  }
  if (!missingTaxiRejected) {
    return Fail("missing retail Taxi fragment was not rejected transactionally");
  }
  if (!missingVehicleRejected) {
    return Fail("missing retail Vehicle fragment was not rejected transactionally");
  }
  if (!missingFarterRootRejected || !missingFarterLocalRejected ||
      !missingLampRejected || !missingCorpseRejected) {
    return Fail("missing Farter/Lamp/Corpse fragments were not rejected "
                "transactionally");
  }
  if (!missingBulletRootRejected || !missingBulletLocalRejected) {
    return Fail("missing Bullet root/local fragments were not rejected "
                "transactionally");
  }
  if (!invalidBulletRosterRejected) {
    return Fail("invalid Bullet attribute roster was not rejected "
                "transactionally");
  }
  if (!tablelessBulletRejected) {
    return Fail("missing Bullet attribute table was not rejected "
                "transactionally");
  }
  if (!missingFarterSubjectRejected) {
    return Fail("missing Farter subject script was not rejected transactionally");
  }
  if (!invalidFarterSubjectRejected) {
    return Fail("invalid Farter subject script was not rejected transactionally");
  }
  if (!invalidExplosionRosterRejected) {
    return Fail("invalid Explosion attribute roster was not rejected "
                "transactionally");
  }
  if (!invalidTaxiRosterRejected) {
    return Fail("invalid Taxi attribute roster was not rejected transactionally");
  }
  if (!tablelessTaxiRejected) {
    return Fail("missing Taxi attribute table was not rejected transactionally");
  }
  if (!invalidVehicleRosterRejected) {
    return Fail("invalid Vehicle attribute roster was not rejected transactionally");
  }
  if (!tablelessVehicleRejected) {
    return Fail("missing Vehicle attribute table was not rejected transactionally");
  }
  if (!invalidLampRosterRejected) {
    return Fail("invalid Lamp attribute roster was not rejected "
                "transactionally");
  }
  if (!invalidSmokerRosterRejected) {
    return Fail("invalid Smoker attribute roster was not rejected "
                "transactionally");
  }
  if (!invalidWavRosterRejected) {
    return Fail("invalid WAV metadata roster was not rejected "
                "transactionally");
  }
  if (!partialSmokeVisualRejected || !invalidSmokeVisualRejected ||
      !failedSmokeVisualLoadRejected) {
    return Fail("invalid/failed Smoke visual resources were not rolled back");
  }
  if (!sourceOnlyCycle) {
    return Fail("source-only Smoke visual deferral was not preserved");
  }
  if (!missingSkinCatalogRejected) {
    return Fail("missing retail Skin catalog was not rejected transactionally");
  }
  if (!invalidSkinCatalogRejected) {
    return Fail("invalid Skin catalog was not rejected transactionally");
  }
  if (!firstCycle) return Fail("first real Vehicle seance failed");
  if (!secondCycle) return Fail("Vehicle seance reconstruction failed");
  if (!restored) return Fail("working directory was not restored");

  std::printf("bounded arena seance cycles=3 source-only-visual=deferred "
              "missing-smoke=rollback "
              "missing-wav=rollback "
              "missing-set-tank=rollback "
               "missing-explosion-root-local=rollback "
               "missing-vehicle=rollback "
               "missing-taxi=rollback tableless-taxi=rollback "
               "invalid-taxi-roster=rollback "
               "tableless-vehicle=rollback invalid-vehicle-roster=rollback "
              "missing-farter-root-local=rollback missing-lamp=rollback "
              "missing-farter-subject=rollback "
              "invalid-farter-subject=rollback "
              "missing-corpse=rollback "
              "invalid-bullet-roster=rollback tableless-bullet=rollback "
              "invalid-explosion-roster=rollback "
              "invalid-lamp-roster=rollback "
               "invalid-smoker-roster=rollback invalid-wav-roster=rollback "
               "partial-invalid-failed-smoke-visual=rollback "
              "missing-skin-catalog=rollback "
              "invalid-skin-catalog=rollback "
              "script=legacy-vm "
              "common_attrs=bird,orphan,artefact portal=table "
              "skin_resources=preflight-empty-fixture "
              "smoke_attrs=retail-18 explosion_attrs=level-aware-90-field "
              "explosion_impulse=local-vehicle-factor-5-offset-proof "
               "explosion_sound=1/1/1-device-free refs=%llu "
               "explosion_particles=bounded-simple-snake-ray visual=%llu "
               "explosion_smoke=standalone-alpha-sprite visual=%llu "
               "explosion_piece=source-only-deferred "
               "vehicle_attrs=3/8 vehicle_refs=Panel-Taxi-Bullet-atomic "
               "taxi_attrs=2/7-atomic-source-only "
               "bullet_attrs=4/4 "
               "bullet_subject=0/500-ballistic-collision-ground-waterline "
               "bullet_probe_moves=2 bullet_collision=2/1/4/3/4/0 "
               "bullet_dynamic_query=hit-remove-rollback "
               "smoke_subject=0/300 smoke_simulation=START-MOVE-remove "
               "smoke_visual=resolved "
               "smoker_attrs=11/11 dyn_smoker=0/62 wav_metadata=5/30 "
               "sound_object=0/250 device-free "
               "farter_subject=retail-absent script_objects=0 "
              "farter_attrs=0/10 lamp_attrs=10/10 corpse_attrs=2/3 "
              "corpse_subject=0/100 "
              "farter_refs=resolved smoker_refs=resolved "
               "smoker_runtime=ready corpse_refs=source-only "
              "spark=Spark.Flash route=table "
              "vehicle=Vehicle.Default "
              "explosion_fingerprint=%llu vehicle_fingerprint=%llu "
              "vehicle_reference_fingerprint=%llu "
              "bullet_fingerprint=%llu bullet_reference_fingerprint=%llu "
              "bullet_subject_fingerprint=%llu "
              "smoker_fingerprint=%llu "
               "taxi_fingerprint=%llu "
               "smoker_reference_fingerprint=%llu "
               "smoke_subject_fingerprint=%llu "
               "smoke_visual_fingerprint=%llu "
              "dyn_smoker_fingerprint=%llu "
              "wav_fingerprint=%llu sound_object_fingerprint=%llu "
              "farter_subject_fingerprint=%llu "
              "farter_fingerprint=%llu "
              "farter_reference_fingerprint=%llu "
              "lamp_fingerprint=%llu corpse_fingerprint=%llu "
              "corpse_subject_fingerprint=%llu "
              "skin_catalog_fingerprint=%llu "
              "rollback=idempotent\n",
              g_explosionSoundFixtureFingerprint,
              g_explosionParticleFixtureFingerprint,
              g_explosionSmokeFixtureFingerprint,
              g_explosionFixtureFingerprint,
              g_vehicleFixtureFingerprint,
              g_vehicleReferenceFixtureFingerprint,
              g_bulletFixtureFingerprint,
              g_bulletReferenceFixtureFingerprint,
              g_bulletSubjectFixtureFingerprint,
              g_smokerFixtureFingerprint,
              g_taxiFixtureFingerprint,
               g_smokerReferenceFixtureFingerprint,
               g_smokeSubjectFixtureFingerprint,
               g_smokeVisualFixtureFingerprint,
              g_dynSmokerFixtureFingerprint,
              g_wavFixtureFingerprint,
              g_soundObjectFixtureFingerprint,
              g_farterSubjectFixtureFingerprint,
              g_farterFixtureFingerprint,
              g_farterReferenceFixtureFingerprint,
              g_lampFixtureFingerprint,
              g_corpseFixtureFingerprint,
              g_corpseSubjectFixtureFingerprint,
              g_skinCatalogFixtureFingerprint);
  return EXIT_SUCCESS;
}
