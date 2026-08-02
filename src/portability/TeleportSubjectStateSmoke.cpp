#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "obase/teleport/TeleportSubjectState.h"
#include "obase/vehicle/VehicleActiveWorldState.h"
class CGRPanel;
#include "h/vehicle.h"
#include "storage/h/subject.h"

namespace {

bool g_seanceOpen = false;

class TemporaryVehicleConfig {
 public:
  TemporaryVehicleConfig() : ready_(false) {
    char temporaryRoot[MAX_PATH] = {};
    char uniquePath[MAX_PATH] = {};
    if (GetCurrentDirectoryA(MAX_PATH, original_) == 0 ||
        GetTempPathA(MAX_PATH, temporaryRoot) == 0 ||
        GetTempFileNameA(temporaryRoot, "rrt", 0, uniquePath) == 0 ||
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

int Fail(const char* message) {
  std::fprintf(stderr, "teleport-subject-state-smoke: %s (%s)\n", message,
               TeleportSubjectState_LastError());
  if (g_seanceOpen) {
    g_arena.closeSeance();
    g_seanceOpen = false;
  }
  g_vehicle = nullptr;
  return EXIT_FAILURE;
}

bool NearlyEqual(double left, double right) {
  const double scale = 1.0 + std::fabs(left) + std::fabs(right);
  return std::fabs(left - right) <= 1.0e-9 * scale;
}

bool NearlyEqual(const CFVector3& left, const CFVector3& right) {
  return NearlyEqual(left.x, right.x) && NearlyEqual(left.y, right.y) &&
         NearlyEqual(left.z, right.z);
}

bool NearlyEqual(const CFMatrix3x4& left, const CFMatrix3x4& right) {
  return NearlyEqual(left.Row(0), right.Row(0)) &&
         NearlyEqual(left.Row(1), right.Row(1)) &&
         NearlyEqual(left.Row(2), right.Row(2)) &&
         NearlyEqual(left.Offset(), right.Offset());
}

bool BuildVehicle(SimulationContext* context) {
  const ct_ClassTableID attributeTable =
      g_arena.addClassTable("VehicleAttr", 2);
  const ct_ClassTableID vehicleTable = g_arena.addClassTable("Vehicle", 1);
  if (attributeTable == ct_NULLID || vehicleTable == ct_NULLID) return false;

  KR_ObjectID defaultAttributeID =
      g_arena.newObject(attributeTable, "Vehicle.Attr.default");
  KR_ObjectID deadAttributeID =
      g_arena.newObject(attributeTable, "Vehicle.Attr.dead");
  KR_ObjectID vehicleID =
      g_arena.newObject(vehicleTable, "Vehicle.Default");
  if (defaultAttributeID.isNUL() || deadAttributeID.isNUL() ||
      vehicleID.isNUL())
    return false;

  AttributeVehicle* defaultAttribute = static_cast<AttributeVehicle*>(
      __attrVehicleTable.searchAttribute(defaultAttributeID));
  AttributeVehicle* deadAttribute = static_cast<AttributeVehicle*>(
      __attrVehicleTable.searchAttribute(deadAttributeID));
  if (defaultAttribute == nullptr || deadAttribute == nullptr) return false;
  std::strncpy(defaultAttribute->m_dynamic, "Emveshka",
               sizeof(defaultAttribute->m_dynamic) - 1);
  defaultAttribute->m_dynamic[sizeof(defaultAttribute->m_dynamic) - 1] = 0;
  std::strncpy(deadAttribute->m_dynamic, "Dead",
               sizeof(deadAttribute->m_dynamic) - 1);
  deadAttribute->m_dynamic[sizeof(deadAttribute->m_dynamic) - 1] = 0;

  KR_Event attributeEvent(KR_SET_ATTR, Session::m_moment,
                          g_arena.getObjectID(), vehicleID);
  attributeEvent.data.open(EDO_WRITE).putObjectID(defaultAttributeID).close();
  context->sendEventNow(attributeEvent);
  g_vehicle = static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  return g_vehicle != nullptr && g_vehicle->VesselMass() > 0.0;
}

}  // namespace

int main() {
  TemporaryVehicleConfig config;
  if (!config.ready())
    return Fail("could not create the temporary vessel config");

  Session::m_moment = 1.0;
  VehicleActiveWorldState_Link();
  TeleportSubjectState_Link();
  SimulationContext context(64, 128);
  g_arena.openSeance(&context, 5120.0, 5120.0);
  g_seanceOpen = true;
  if (!BuildVehicle(&context)) return Fail("could not publish Vehicle");

  CFMatrix3x4 direction;
  direction.LoadIdentity().RotateOyL(0.375);
  const CFVector3 position(125.5, 18.25, -74.75);
  g_vehicle->SetDir(direction);
  g_vehicle->SetPos(position);
  g_vehicle->setPosition(position);
  const CFVector3 speed = g_vehicle->Speed();

  TeleportDefinition definition;
  definition.source = CFVector3(100.0, 0.0, -100.0);
  definition.destination = CFVector3(200.0, 5.0, -200.0);
  definition.radius = 5.0;
  const std::vector<TeleportDefinition> definitions(1, definition);
  if (!TeleportSubjectState_Initialize(
          &context, 2, definitions, Session::m_moment) ||
      !TeleportSubjectState_TableReady(&context, 2) ||
      TeleportSubjectState_Capacity() != 2 ||
      TeleportSubjectState_LiveCount() != 1 ||
      TeleportSubjectState_Fingerprint(&context) == 0)
    return Fail("active route was not admitted");

  TeleportLifecycleProbeSummary probe = {};
  const bool probeAccepted = TeleportSubjectState_ProbeLifecycle(
      &context, Session::m_moment, &probe);
  if (!probeAccepted ||
      probe.rejectedNonPlayerCollisions != 1 ||
      probe.physicsCollisionEvents != 1 ||
      probe.appliedPlayerCollisions != 1 ||
      probe.vehiclePoseRollbacks != 1 ||
      !NearlyEqual(g_vehicle->Pos(), position) ||
      !NearlyEqual(g_vehicle->getPosition(), position) ||
      !NearlyEqual(g_vehicle->Speed(), speed) ||
      !NearlyEqual(g_vehicle->GetDir(), direction)) {
    std::fprintf(stderr,
                 "teleport probe: accepted=%d rejected=%d physics=%d "
                 "applied=%d rollback=%d radius=%.17g\n",
                 probeAccepted ? 1 : 0, probe.rejectedNonPlayerCollisions,
                 probe.physicsCollisionEvents, probe.appliedPlayerCollisions,
                 probe.vehiclePoseRollbacks, g_vehicle->getRadius());
    return Fail("collision proof did not preserve Vehicle pose");
  }

  const unsigned long long fingerprint =
      TeleportSubjectState_Fingerprint(&context);
  g_arena.closeSeance();
  g_seanceOpen = false;
  g_vehicle = nullptr;
  if (TeleportSubjectState_Capacity() != 0 ||
      TeleportSubjectState_LiveCount() != 0)
    return Fail("route table survived Arena teardown");

  std::printf(
      "teleport-subject-state-smoke: capacity=2 routes=1 rejected=1 "
      "physics=1 applied=1 rollback=1 fingerprint=%llu\n",
      fingerprint);
  return EXIT_SUCCESS;
}
