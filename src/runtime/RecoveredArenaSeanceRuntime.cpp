#include "RecoveredArenaSeanceRuntime.h"

#include <cstdio>
#include <new>

class CGRPanel;
#include "h/vehicle.h"
#include "kernel/h/context.h"
#include "obase/artefact/ArtefactAttributeState.h"
#include "obase/bird/BirdAttributeState.h"
#include "obase/orphan/OrphanAttributeState.h"
#include "obase/portal/PortalClassTableState.h"
#include "obase/spark/SparkAttributeState.h"
#include "storage/h/subject.h"

#include "RecoveredLegacyScriptHost.h"
#include "RecoveredLegacyScriptRunner.h"

namespace {

constexpr double kSceneWidth = 5120.0;
constexpr double kSceneDepth = 5120.0;
constexpr const char kBootstrapProgramName[] =
    "recovered_common_attribute_vehicle_bootstrap";

// This deliberately uses the original script-facing storage and event
// protocol. It is a bounded bridge to the real Vehicle tables, not a second
// gameplay implementation. The complete retail LEVEL0.SC will replace it as
// the remaining OBASE class-table archives are connected.
const char kAttributeVehicleBootstrapScript[] = R"RR2NW_SCRIPT(const int EDO_WRITE = 1;
const int KR_SET_ATTR = 1;
const int sp_EV_SET_PHASE_COUNT extern;
const int sp_EV_SET_PHASE extern;
const int RECT2D_I extern;
const int LIGHT_COLOR_YELLOW extern;
const int s_ATTR_MSG_SET_INT extern;
const int s_ATTR_MSG_SET_DOUBLE extern;
const int s_ATTR_MSG_SET_STR extern;

func int s_OpenEventData(int style) extern;
func void s_CloseEventData(int event) extern;
func void s_Descend(int event, int tag, int index) extern;
func void s_Ascend(int event) extern;
func void s_WriteInt(int event, int value) extern;
func void s_WriteFloat(int event, float value) extern;
func void s_WriteStr(int event, str value) extern;
func void s_WriteObjectID(int event, int objectID, int cachePos) extern;
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;
func int s_AddClassTable(str className, int maxTableSize) extern;
func void s_New(int classTableID, str name, var int objectID, var int cachePos) extern;
func void s_NewObject(int classTableID, str name) extern;

func void SetSparkPhase(int objectID, int cachePos,
                        int phase, int x, int y, int w, int h, float time,
                        int brightness, int color, float radius)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, phase);
  s_Descend(event, RECT2D_I, 0);
  s_WriteInt(event, x-w);
  s_WriteInt(event, y-h);
  s_WriteInt(event, x+w);
  s_WriteInt(event, y+h);
  s_Ascend(event);
  s_WriteFloat(event, time);
  s_WriteInt(event, brightness);
  s_WriteInt(event, color);
  s_WriteFloat(event, radius);
  s_CloseEventData(event);
  s_SendEventNow(event, sp_EV_SET_PHASE, objectID, cachePos);
}

func void SetSpark0()
var int objectID, cachePos, event;
{
  s_SearchObjectID(objectID, cachePos, "Spark.Flash");
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, 6);
  s_CloseEventData(event);
  s_SendEventNow(event, sp_EV_SET_PHASE_COUNT, objectID, cachePos);

  SetSparkPhase(objectID,cachePos,0, 25,45,20,40,0.04,100,LIGHT_COLOR_YELLOW, 7);
  SetSparkPhase(objectID,cachePos,1, 78,46,20,40,0.03,200,LIGHT_COLOR_YELLOW,14);
  SetSparkPhase(objectID,cachePos,2,126,47,20,40,0.03,100,LIGHT_COLOR_YELLOW,10);
  SetSparkPhase(objectID,cachePos,3,180,44,20,40,0.10, 20,LIGHT_COLOR_YELLOW, 7);
  SetSparkPhase(objectID,cachePos,4,230,44,20,40,0.10,  0,LIGHT_COLOR_YELLOW, 4);
  SetSparkPhase(objectID,cachePos,5,230,44,20,40,0.00,  0,LIGHT_COLOR_YELLOW, 0);
}

func void SetAttributeFloat(int objectID, int cachePos, str name, float value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteFloat(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_DOUBLE, objectID, cachePos);
}

func void SetAttributeInt(int objectID, int cachePos, str name, int value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteInt(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_INT, objectID, cachePos);
}

func void SetAttributeStr(int objectID, int cachePos, str name, str value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteStr(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_STR, objectID, cachePos);
}

func void ChangeObjectAttrN(str attrName, str objectName)
var int event, attrID, attrCachePos, objectID, cachePos;
{
  s_SearchObjectID(attrID, attrCachePos, attrName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteObjectID(event, attrID, attrCachePos);
  s_CloseEventData(event);
  s_SearchObjectID(objectID, cachePos, objectName);
  s_SendEventNow(event, KR_SET_ATTR, objectID, cachePos);
}

func void main()
var int sparkAttrTable, routeTable, attrTable, birdAttrTable, portalTable;
var int orphanAttrTable, artefactAttrTable, vehicleTable, objectID, cachePos;
{
  sparkAttrTable := s_AddClassTable("SparkAttr", 3);
  routeTable := s_AddClassTable("Route", 100);

  attrTable := s_AddClassTable("VehicleAttr", 2);
  s_New(attrTable, "Vehicle.Attr.default", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_dynamic", "TankGenn0");
  s_New(attrTable, "Vehicle.Attr.dead", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_dynamic", "Dead");

  birdAttrTable := s_AddClassTable("BirdAttr", 1);
  s_New(birdAttrTable, "Bird.Attr.0", objectID, cachePos);
  SetAttributeFloat(objectID, cachePos, "m_speed", 2.0);
  SetAttributeStr(objectID, cachePos, "m_skinName", "sk.Bird.0");
  SetAttributeFloat(objectID, cachePos, "m_calcPosIncrement", 0.2);

  portalTable := s_AddClassTable("Portal", 2);

  s_NewObject(sparkAttrTable, "Spark.Flash");
  SetSpark0();

  orphanAttrTable := s_AddClassTable("OrphanAttr", 3);
  s_New(orphanAttrTable, "Orphan.Attr.Default", objectID, cachePos);

  artefactAttrTable := s_AddClassTable("ArtefactAttr", 2);
  s_New(artefactAttrTable, "Artefact.Attr.0", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_skinName", "sk.Artefact.0");
  SetAttributeFloat(objectID, cachePos, "m_maxCoronaR", 20.0);
  SetAttributeFloat(objectID, cachePos, "m_coronaR", 0.4);
  SetAttributeInt(objectID, cachePos, "m_coronaRGB", 16711935);
  SetAttributeInt(objectID, cachePos, "m_coronaAlpha", 150);

  vehicleTable := s_AddClassTable("Vehicle", 1);
  s_New(vehicleTable, "Vehicle.Default", objectID, cachePos);
  ChangeObjectAttrN("Vehicle.Attr.default", "Vehicle.Default");
}
)RR2NW_SCRIPT";

struct RecoveredArenaSeanceState {
  unsigned int issues;
  bool arenaOpen;
  bool scriptCompleted;
  bool birdAttributesReady;
  bool portalReady;
  bool orphanAttributesReady;
  bool artefactAttributesReady;
  bool sparkAttributesReady;
  bool routeReady;
  bool vehicleReady;
  char lastError[256];
};

RecoveredArenaSeanceState g_state = {};

void SetError(const char* message) {
  std::snprintf(g_state.lastError, sizeof(g_state.lastError), "%s",
                message == nullptr ? "unknown seance failure" : message);
}

void Report(unsigned int issue, const char* message) {
  g_state.issues |= issue;
  SetError(message);
}

void RollBackPartiallyOpenedArena(SimulationContext* context) {
  if (context == nullptr) return;
  if (g_arena.getContext() == context || context->isExist("Storage")) {
    g_state.arenaOpen = true;
    RecoveredArenaSeance_Release();
  }
}

unsigned int IssueForScriptStatus(ERecoveredLegacyScriptRunStatus status) {
  switch (status) {
    case RECOVERED_LEGACY_SCRIPT_RUN_ALLOCATION_FAILURE:
      return RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_INITIALIZATION_FAILURE:
    case RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT:
      return RECOVERED_ARENA_SEANCE_SCRIPT_INITIALIZATION_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_COMPILE_FAILURE:
      return RECOVERED_ARENA_SEANCE_SCRIPT_COMPILE_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_TIMEOUT:
      return RECOVERED_ARENA_SEANCE_SCRIPT_TIMEOUT;
    case RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE:
      return RECOVERED_ARENA_SEANCE_SCRIPT_HOST_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_PROCESS_FAILURE:
    case RECOVERED_LEGACY_SCRIPT_RUN_EXCEPTION:
      return RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS:
      break;
  }
  return RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE;
}

bool RunAttributeVehicleBootstrap(SimulationContext* context,
                                  double startTime) {
  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_BootstrapProfile();
  if (!RecoveredLegacyScript_RunMemory(
          kAttributeVehicleBootstrapScript, kBootstrapProgramName, profile,
          context, startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool OpenArena(SimulationContext* context) {
  try {
    g_arena.openSeance(context, kSceneWidth, kSceneDepth);
    g_state.arenaOpen = g_arena.getContext() == context &&
                        context->isExist("Storage");
  } catch (const std::bad_alloc&) {
    RollBackPartiallyOpenedArena(context);
    Report(RECOVERED_ARENA_SEANCE_OPEN_FAILURE,
           "Arena allocation failed");
    return false;
  } catch (...) {
    RollBackPartiallyOpenedArena(context);
    Report(RECOVERED_ARENA_SEANCE_OPEN_FAILURE, "Arena open failed");
    return false;
  }

  if (!g_state.arenaOpen) {
    Report(RECOVERED_ARENA_SEANCE_OPEN_FAILURE,
           "Arena did not publish Storage");
    RecoveredArenaSeance_Release();
    return false;
  }
  return true;
}

bool PublishVehicle(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("VehicleAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Vehicle") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_VEHICLE_TABLE_MISSING,
           "script did not create the Vehicle tables");
    return false;
  }

  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  if (vehicleID.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_VEHICLE_OBJECT_MISSING,
           "script did not create Vehicle.Default");
    return false;
  }

  g_vehicle = static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  if (g_vehicle == nullptr) {
    Report(RECOVERED_ARENA_SEANCE_VEHICLE_INTERFACE_MISSING,
           "Vehicle.Default does not expose IVehicleIID");
    return false;
  }

  g_state.vehicleReady = true;
  return true;
}

bool PublishRouteTable() {
  if (g_arena.searchSeanceClassTable("Route") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_ROUTE_TABLE_MISSING,
           "script did not create the Route table");
    return false;
  }
  g_state.routeReady = true;
  return true;
}

bool PublishSparkAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("SparkAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_SPARK_TABLE_MISSING,
           "script did not create the SparkAttr table");
    return false;
  }

  KR_ObjectID flash = context->searchObject("Spark.Flash");
  if (flash.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_SPARK_OBJECT_MISSING,
           "script did not create Spark.Flash");
    return false;
  }
  if (!SparkAttributeState_IsRetailFlash(flash)) {
    Report(RECOVERED_ARENA_SEANCE_SPARK_DEFAULT_INVALID,
           "Spark.Flash does not match the retail phase table");
    return false;
  }

  g_state.sparkAttributesReady = true;
  return true;
}

bool PublishBirdAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("BirdAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_BIRD_TABLE_MISSING,
           "script did not create the BirdAttr table");
    return false;
  }

  KR_ObjectID bird = context->searchObject("Bird.Attr.0");
  if (bird.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_BIRD_OBJECT_MISSING,
           "script did not create Bird.Attr.0");
    return false;
  }
  if (!BirdAttributeState_IsRetailDefault(bird)) {
    Report(RECOVERED_ARENA_SEANCE_BIRD_DEFAULT_INVALID,
           "Bird.Attr.0 does not match the retail common attribute");
    return false;
  }

  g_state.birdAttributesReady = true;
  return true;
}

bool PublishPortalTable() {
  if (g_arena.searchSeanceClassTable("Portal") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_PORTAL_TABLE_MISSING,
           "script did not create the Portal table");
    return false;
  }
  g_state.portalReady = true;
  return true;
}

bool PublishOrphanAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("OrphanAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_ORPHAN_TABLE_MISSING,
           "script did not create the OrphanAttr table");
    return false;
  }

  KR_ObjectID orphan = context->searchObject("Orphan.Attr.Default");
  if (orphan.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_ORPHAN_OBJECT_MISSING,
           "script did not create Orphan.Attr.Default");
    return false;
  }
  if (!OrphanAttributeState_IsRetailDefault(orphan)) {
    Report(RECOVERED_ARENA_SEANCE_ORPHAN_DEFAULT_INVALID,
           "Orphan.Attr.Default does not match the retail common attribute");
    return false;
  }

  g_state.orphanAttributesReady = true;
  return true;
}

bool PublishArtefactAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("ArtefactAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_ARTEFACT_TABLE_MISSING,
           "script did not create the ArtefactAttr table");
    return false;
  }

  KR_ObjectID artefact = context->searchObject("Artefact.Attr.0");
  if (artefact.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_ARTEFACT_OBJECT_MISSING,
           "script did not create Artefact.Attr.0");
    return false;
  }
  if (!ArtefactAttributeState_IsRetailDefault(artefact)) {
    Report(RECOVERED_ARENA_SEANCE_ARTEFACT_DEFAULT_INVALID,
           "Artefact.Attr.0 does not match the retail common attribute");
    return false;
  }

  g_state.artefactAttributesReady = true;
  return true;
}

}  // namespace

int RecoveredArenaSeance_Initialize(SimulationContext* context,
                                    double startTime) {
  RecoveredArenaSeance_Release();
  g_state = {};

  if (context == nullptr) {
    Report(RECOVERED_ARENA_SEANCE_INVALID_CONTEXT,
           "seance requires a SimulationContext");
    return FALSE;
  }

  BirdAttributeState_Link();
  ArtefactAttributeState_Link();
  OrphanAttributeState_Link();
  PortalClassTable_Link();
  SparkAttributeState_Link();
  if (!OpenArena(context)) return FALSE;

  try {
    if (!RunAttributeVehicleBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    g_state.scriptCompleted = true;

    if (!PublishSparkAttributes(context)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishBirdAttributes(context) || !PublishPortalTable() ||
        !PublishOrphanAttributes(context) ||
        !PublishArtefactAttributes(context)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishRouteTable()) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishVehicle(context)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
  } catch (const std::bad_alloc&) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "seance bootstrap allocation failed");
    RecoveredArenaSeance_Release();
    return FALSE;
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "seance bootstrap raised an exception");
    RecoveredArenaSeance_Release();
    return FALSE;
  }

  return TRUE;
}

void RecoveredArenaSeance_Release() {
  g_state.vehicleReady = false;
  g_state.routeReady = false;
  g_state.sparkAttributesReady = false;
  g_state.artefactAttributesReady = false;
  g_state.orphanAttributesReady = false;
  g_state.portalReady = false;
  g_state.birdAttributesReady = false;
  g_state.scriptCompleted = false;
  g_vehicle = nullptr;
  if (!g_state.arenaOpen) return;
  g_arena.closeSeance();
  g_state.arenaOpen = false;
}

bool RecoveredArenaSeance_IsOpen() { return g_state.arenaOpen; }

bool RecoveredArenaSeance_ScriptCompleted() {
  return g_state.scriptCompleted;
}

bool RecoveredArenaSeance_RouteReady() { return g_state.routeReady; }

bool RecoveredArenaSeance_BirdAttributesReady() {
  return g_state.birdAttributesReady;
}

bool RecoveredArenaSeance_PortalReady() { return g_state.portalReady; }

bool RecoveredArenaSeance_OrphanAttributesReady() {
  return g_state.orphanAttributesReady;
}

bool RecoveredArenaSeance_ArtefactAttributesReady() {
  return g_state.artefactAttributesReady;
}

bool RecoveredArenaSeance_SparkAttributesReady() {
  return g_state.sparkAttributesReady;
}

bool RecoveredArenaSeance_VehicleReady() { return g_state.vehicleReady; }

unsigned int RecoveredArenaSeance_Issues() { return g_state.issues; }

const char* RecoveredArenaSeance_LastError() { return g_state.lastError; }
