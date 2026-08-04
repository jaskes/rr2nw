#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "ActiveWorldRuntimeProbe.h"
#include "SimulationRandom.h"
#include "TimeRuntimeState.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/comanmsg.h"
#include "message/groupmsg.h"
#include "obase/comander/CommanderState.h"
#include "obase/group/TankGroupState.h"
#include "obase/vehicle/VehicleActiveWorldState.h"
class CGRPanel;
#include "h/vehicle.h"
#include "storage/h/subject.h"

namespace {

class TemporaryVehicleConfig {
 public:
  TemporaryVehicleConfig() : ready_(false) {
    char temporaryRoot[MAX_PATH] = {};
    char uniquePath[MAX_PATH] = {};
    if (GetCurrentDirectoryA(MAX_PATH, original_) == 0 ||
        GetTempPathA(MAX_PATH, temporaryRoot) == 0 ||
        GetTempFileNameA(temporaryRoot, "rrv", 0, uniquePath) == 0 ||
        !DeleteFileA(uniquePath) || !CreateDirectoryA(uniquePath, nullptr))
      return;
    directory_ = uniquePath;
    const std::string config = directory_ + "\\vessels.cfg";
    std::ofstream output(config.c_str(), std::ios::binary | std::ios::trunc);
    output << "\r\n";
    output.close();
    if (!output || !SetCurrentDirectoryA(directory_.c_str())) {
      DeleteFileA(config.c_str());
      RemoveDirectoryA(directory_.c_str());
      directory_.clear();
      return;
    }
    ready_ = true;
  }

  ~TemporaryVehicleConfig() {
    if (!ready_) return;
    SetCurrentDirectoryA(original_);
    const std::string config = directory_ + "\\vessels.cfg";
    DeleteFileA(config.c_str());
    RemoveDirectoryA(directory_.c_str());
  }

  bool ready() const { return ready_; }

 private:
  char original_[MAX_PATH] = {};
  std::string directory_;
  bool ready_;
};

class FreshMember : public ct_Object {
 public:
  int receiveEvent(KR_Event&) override { return 1; }
  bool shouldDump() override { return false; }
};

class FreshMemberTable : public ct_ClassTable {
 public:
  FreshMemberTable() : table_(nullptr) { registerClass("FreshMember"); }

  void allocObjects(int count) override {
    table_ = new (std::nothrow) FreshMember[count];
    if (table_ == nullptr) m_maxObjectQnty = 0;
  }
  void freeObjects() override {
    delete[] table_;
    table_ = nullptr;
    m_maxObjectQnty = 0;
  }
  ct_Object* getObjectPTR(int index) override {
    return index >= 0 && index < m_maxObjectQnty ? &table_[index] : nullptr;
  }

 private:
  FreshMember* table_;
};

FreshMemberTable g_freshMemberTable;

int Fail(const char* message) {
  std::fprintf(stderr, "active-world-fresh-restore-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool SendObjectID(SimulationContext* context, const KR_ObjectID& destination,
                  int label, const KR_ObjectID& value) {
  KR_Event event;
  event.label = label;
  event.source = g_arena.getObjectID();
  event.destination = destination;
  event.timeStamp = Session::m_moment;
  event.data.open(EDO_WRITE).putObjectID(value).close();
  context->sendEventNow(event);
  return true;
}

bool SendName(SimulationContext* context, const KR_ObjectID& destination,
              int label, const char* value) {
  KR_Event event;
  event.label = label;
  event.source = g_arena.getObjectID();
  event.destination = destination;
  event.timeStamp = Session::m_moment;
  event.data.open(EDO_WRITE).putStr(value).close();
  context->sendEventNow(event);
  return true;
}

void RemoveIfPresent(SimulationContext* context, const char* name) {
  if (context->isExist(name))
    context->removeObject(context->searchObject(name));
}

bool BuildSourceGraph(SimulationContext* context, KR_ObjectID* alpha,
                      KR_ObjectID* beta, KR_ObjectID* group,
                      KR_ObjectID* member, KR_ObjectID* vehicle) {
  const ct_ClassTableID commanderTable =
      g_arena.addClassTable("Commander", 2);
  const ct_ClassTableID groupAttributeTable =
      g_arena.addClassTable("TankGroupTable", 1);
  const ct_ClassTableID groupTable =
      g_arena.addClassTable("TankGroup", 2);
  const ct_ClassTableID memberTable =
      g_arena.addClassTable("FreshMember", 2);
  const ct_ClassTableID vehicleAttributeTable =
      g_arena.addClassTable("VehicleAttr", 3);
  const ct_ClassTableID vehicleTable =
      g_arena.addClassTable("Vehicle", 2);
  if (commanderTable == ct_NULLID || groupAttributeTable == ct_NULLID ||
      groupTable == ct_NULLID || memberTable == ct_NULLID ||
      vehicleAttributeTable == ct_NULLID || vehicleTable == ct_NULLID)
    return false;
  KR_ObjectID attribute =
      g_arena.newObject(groupAttributeTable, "Group.Attr.Fresh");
  *alpha = g_arena.newObject(commanderTable, "Commander.Alpha");
  *beta = g_arena.newObject(commanderTable, "Commander.Beta");
  *group = g_arena.newObject(groupTable, "Group.Fresh");
  *member = g_arena.newObject(memberTable, "Tank.Dependency");
  KR_ObjectID defaultVehicleAttribute =
      g_arena.newObject(vehicleAttributeTable, "Vehicle.Attr.default");
  KR_ObjectID deadVehicleAttribute =
      g_arena.newObject(vehicleAttributeTable, "Vehicle.Attr.dead");
  *vehicle = g_arena.newObject(vehicleTable, "Vehicle.Default");
  if (attribute.isNUL() || alpha->isNUL() || beta->isNUL() ||
      group->isNUL() || member->isNUL() ||
      defaultVehicleAttribute.isNUL() || deadVehicleAttribute.isNUL() ||
      vehicle->isNUL())
    return false;

  AttributeVehicle* defaultAttribute = static_cast<AttributeVehicle*>(
      __attrVehicleTable.searchAttribute(defaultVehicleAttribute));
  AttributeVehicle* deadAttribute = static_cast<AttributeVehicle*>(
      __attrVehicleTable.searchAttribute(deadVehicleAttribute));
  if (defaultAttribute == nullptr || deadAttribute == nullptr)
    return false;
  std::strncpy(defaultAttribute->m_dynamic, "Emveshka",
               sizeof(defaultAttribute->m_dynamic) - 1);
  defaultAttribute->m_dynamic[sizeof(defaultAttribute->m_dynamic) - 1] = 0;
  std::strncpy(deadAttribute->m_dynamic, "Dead",
               sizeof(deadAttribute->m_dynamic) - 1);
  deadAttribute->m_dynamic[sizeof(deadAttribute->m_dynamic) - 1] = 0;

  SendObjectID(context, *group, KR_SET_ATTR, attribute);
  SendName(context, *alpha, COMMANDER_ADD_MEMBER_N, "Group.Fresh");
  SendObjectID(context, *alpha, com_EV_SETHOSTILECOMMANDER, *beta);
  SendObjectID(context, *beta, com_EV_SETFRIENDLYCOMMANDER, *alpha);
  SendObjectID(context, *group, GROUP_ADD_MEMBER, *member);
  SendObjectID(context, *vehicle, KR_SET_ATTR, defaultVehicleAttribute);
  g_vehicle = static_cast<Vehicle*>(
      context->queryInterface(*vehicle, IVehicleIID));
  if (g_vehicle == nullptr || g_vehicle->VesselMass() <= 0.0)
    return false;
  CFMatrix3x4 direction;
  direction.LoadIdentity().RotateOyL(0.375);
  g_vehicle->SetDir(direction);
  g_vehicle->SetPos(CFVector3(125.5, 18.25, -74.75));
  if (!g_vehicle->ApplyExplosionImpulse(CFVector3(90.0, 12.0, -30.0),
                                        1.0))
    return false;
  g_vehicle->m_damage = 0.625;
  g_vehicle->m_secBulletCnt = 17;
  g_vehicle->m_skipPos = CFVector3(10.0, 20.0, 30.0);
  g_vehicle->m_skipTime = 4.5;
  g_vehicle->m_lastLeaveTime = 2.25;
  g_vehicle->m_lastTime = 7.75;
  Vehicle::s_curTime = 7.75;
  Vehicle::m_spX = -11.0;
  Vehicle::m_spY = 22.0;
  Vehicle::m_spZ = -33.0;
  Player& player = static_cast<Player&>(g_vehicle->player());
  player.findCommander();
  player.m_damage = 0.875;
  if (player.m_sideQnty != 2)
    return false;
  for (int index = 0; index < player.m_sideQnty; ++index) {
    const char* commander =
        context->searchObject(player.m_playerStatus[index].m_masterID);
    if (commander == nullptr)
      return false;
    player.m_playerStatus[index].m_damageDiff =
        std::strcmp(commander, "Commander.Alpha") == 0 ? -0.25 : 0.5;
    player.m_playerStatus[index].m_isRenegat =
        std::strcmp(commander, "Commander.Alpha") == 0 ? 1 : 0;
  }
  return CommanderState_MemberCount(context, *alpha) == 1 &&
         CommanderState_IsHostile(context, *alpha, *beta) &&
         TankGroupState_Commander(context, *group) == *alpha &&
         TankGroupState_HasMember(context, *group, *member) &&
         VehicleActiveWorldState_LiveCount(context) == 1 &&
         VehicleActiveWorldState_Fingerprint(context) != 0;
}

}  // namespace

int main() {
  TemporaryVehicleConfig config;
  if (!config.ready())
    return Fail("could not create the temporary vessel config");
  Session::m_moment = 1.0;
  CommanderState_Link();
  TankGroupState_Link();
  VehicleActiveWorldState_Link();
  SimulationContext context(64, 128);
  g_arena.openSeance(&context, 1024.0, 1024.0);

  KR_ObjectID alpha, beta, group, member, vehicle;
  if (!BuildSourceGraph(&context, &alpha, &beta, &group, &member, &vehicle)) {
    g_arena.closeSeance();
    return Fail("could not build the symbolic source graph");
  }

  std::vector<std::uint8_t> bytes;
  SActiveWorldRuntimeProbeSummary capture;
  std::string failure;
  if (!ActiveWorldRuntime_CaptureProbe(
          &context, 0x4652455348435458ull, "fresh-context", &bytes,
          &capture, &failure)) {
    g_arena.closeSeance();
    return Fail(failure.c_str());
  }
  SSimulationClockState capturedClock;
  std::vector<std::uint8_t> capturedRandom;
  if (!SUA_CaptureSimulationClock(&capturedClock) ||
      !SimulationRandom_Capture(&capturedRandom)) {
    g_arena.closeSeance();
    return Fail("could not retain the captured continuation boundary");
  }
  const KR_ObjectID oldAlpha = alpha;
  const KR_ObjectID oldBeta = beta;
  const KR_ObjectID oldGroup = group;
  const KR_ObjectID oldVehicle = vehicle;
  std::vector<KR_ObjectID> removedVehicles(1, vehicle);
  VehicleActiveWorldState_RemoveStableOwners(&context, &removedVehicles);
  RemoveIfPresent(&context, "Group.Fresh");
  RemoveIfPresent(&context, "Commander.Beta");
  RemoveIfPresent(&context, "Commander.Alpha");
  if (context.isExist("Group.Fresh") || context.isExist("Commander.Alpha") ||
      context.isExist("Commander.Beta") ||
      context.isExist("Vehicle.Default") || g_vehicle != nullptr) {
    g_arena.closeSeance();
    return Fail("source owners survived teardown");
  }

  SSimulationClockState divergentClock = capturedClock;
  divergentClock.tick += 100u;
  divergentClock.eventMoment += 5.0;
  divergentClock.viewTime += 5.0;
  if (!SUA_ApplySimulationClock(divergentClock)) {
    g_arena.closeSeance();
    return Fail("could not stage divergent continuation state");
  }
  SimulationRandom_Reset(0x12345678u);
  SimulationRandom_Next();

  SActiveWorldRuntimeProbeSummary restored = capture;
  if (!ActiveWorldRuntime_RestoreProbe(
          &context, bytes, &restored, &failure)) {
    g_arena.closeSeance();
    return Fail(failure.c_str());
  }
  alpha = context.searchObject("Commander.Alpha");
  beta = context.searchObject("Commander.Beta");
  group = context.searchObject("Group.Fresh");
  vehicle = context.searchObject("Vehicle.Default");
  Vehicle* restoredVehicle = static_cast<Vehicle*>(
      context.queryInterface(vehicle, IVehicleIID));
  Player* restoredPlayer = restoredVehicle == nullptr ? nullptr :
      &static_cast<Player&>(restoredVehicle->player());
  const bool fresh = restored.ready && restored.createdOwners == 4 &&
      restored.ownerPhases == 16 && restored.referencePhases == 16 &&
      restored.clockRecords == 1 && restored.rngAlgorithm == 1 &&
      restored.rngStateBytes == 12 &&
      SUA_SimulationClockMatches(capturedClock) &&
      SimulationRandom_Matches(SimulationRandom_Algorithm(), capturedRandom) &&
      alpha != oldAlpha && beta != oldBeta && group != oldGroup &&
      vehicle != oldVehicle && restoredVehicle != nullptr &&
      g_vehicle == restoredVehicle && restoredPlayer != nullptr &&
      restoredPlayer->m_sideQnty == 2 &&
      restoredPlayer->m_damage == 0.875 &&
      restoredVehicle->m_damage == 0.625 &&
      restoredVehicle->m_secBulletCnt == 17 &&
      restoredVehicle->Speed() != CFVector3(0.0, 0.0, 0.0) &&
      CommanderState_MemberCount(&context, alpha) == 1 &&
      CommanderState_IsHostile(&context, alpha, beta) &&
      TankGroupState_Commander(&context, group) == alpha &&
      TankGroupState_HasMember(&context, group, member);
  if (!fresh) {
    g_arena.closeSeance();
    return Fail("decoded owner allocation or symbolic linking diverged");
  }

  removedVehicles.push_back(vehicle);
  VehicleActiveWorldState_RemoveStableOwners(&context, &removedVehicles);
  RemoveIfPresent(&context, "Group.Fresh");
  RemoveIfPresent(&context, "Commander.Beta");
  RemoveIfPresent(&context, "Commander.Alpha");
  RemoveIfPresent(&context, "Tank.Dependency");
  SActiveWorldRuntimeProbeSummary rejected = capture;
  failure.clear();
  const bool missingRejected =
      !ActiveWorldRuntime_RestoreProbe(&context, bytes, &rejected, &failure) &&
      !failure.empty() && !context.isExist("Group.Fresh") &&
      !context.isExist("Commander.Alpha") &&
      !context.isExist("Commander.Beta") &&
      !context.isExist("Vehicle.Default") && g_vehicle == nullptr;

  KR_ObjectID conflicting =
      g_arena.newObject("FreshMember", "Commander.Alpha");
  SActiveWorldRuntimeProbeSummary collision = capture;
  failure.clear();
  const bool collisionRejected = !conflicting.isNUL() &&
      !ActiveWorldRuntime_RestoreProbe(
          &context, bytes, &collision, &failure) &&
      context.isExist(conflicting) &&
      context.searchObject("Commander.Alpha") == conflicting &&
      !context.isExist("Commander.Beta") && !context.isExist("Group.Fresh") &&
      !context.isExist("Vehicle.Default") && g_vehicle == nullptr;

  g_arena.closeSeance();
  if (!missingRejected)
    return Fail("missing symbolic dependency did not roll owners back");
  if (!collisionRejected)
    return Fail("wrong-class symbolic collision mutated the live graph");
  std::printf(
      "active world fresh restore sections=15 created=4 refs=15 vehicle=state "
      "ids=reallocated missing-dependency=rollback collision=rejected "
      "fingerprint=%llu\n",
      static_cast<unsigned long long>(restored.worldFingerprint));
  return EXIT_SUCCESS;
}
