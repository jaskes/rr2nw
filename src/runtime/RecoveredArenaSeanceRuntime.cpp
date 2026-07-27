#include "RecoveredArenaSeanceRuntime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

class CGRPanel;
#include "h/vehicle.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "sc.h"
#include "storage/h/subject.h"

namespace {

constexpr double kSceneWidth = 5120.0;
constexpr double kSceneDepth = 5120.0;
constexpr int kScriptEventCount = 8;
constexpr int kMaximumVmSlices = 1024;
constexpr const char kBootstrapProgramName[] =
    "recovered_vehicle_bootstrap";

// This deliberately uses the original script-facing storage and event
// protocol. It is a bounded bridge to the real Vehicle tables, not a second
// gameplay implementation. The complete retail LEVEL0.SC will replace it as
// the remaining OBASE class-table archives are connected.
const char kVehicleBootstrapScript[] = R"RR2NW_SCRIPT(const int EDO_WRITE = 1;
const int KR_SET_ATTR = 1;
const int ATTR_MSG_SET_STR = 16010;

func int s_OpenEventData(int style) extern;
func void s_CloseEventData(int event) extern;
func void s_WriteStr(int event, str value) extern;
func void s_WriteObjectID(int event, int objectID, int cachePos) extern;
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;
func int s_AddClassTable(str className, int maxTableSize) extern;
func void s_New(int classTableID, str name, var int objectID, var int cachePos) extern;

func void SetAttributeStr(int objectID, int cachePos, str name, str value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteStr(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, ATTR_MSG_SET_STR, objectID, cachePos);
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
var int attrTable, vehicleTable, objectID, cachePos;
{
  attrTable := s_AddClassTable("VehicleAttr", 2);
  s_New(attrTable, "Vehicle.Attr.default", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_dynamic", "TankGenn0");
  s_New(attrTable, "Vehicle.Attr.dead", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_dynamic", "Dead");

  vehicleTable := s_AddClassTable("Vehicle", 1);
  s_New(vehicleTable, "Vehicle.Default", objectID, cachePos);
  ChangeObjectAttrN("Vehicle.Attr.default", "Vehicle.Default");
}
)RR2NW_SCRIPT";

struct RecoveredScriptEvent : public KR_Event {
  bool inUse = false;
};

struct ScriptAttempt {
  TSuaCript compiler;
  TSetOfProcess processes;
  char* source;
  bool compilerInitialized;
  bool processInitialized;
  int stage;
};

enum EScriptStage {
  SCRIPT_STAGE_ALLOCATION = 0,
  SCRIPT_STAGE_INITIALIZATION,
  SCRIPT_STAGE_COMPILE,
  SCRIPT_STAGE_PROCESS,
  SCRIPT_STAGE_EXECUTION
};

unsigned int g_issues = 0;
bool g_arenaOpen = false;
bool g_scriptCompleted = false;
bool g_vehicleReady = false;
RecoveredScriptEvent g_scriptEvents[kScriptEventCount];
char g_lastError[256] = {};

void SetError(const char* message) {
  std::snprintf(g_lastError, sizeof(g_lastError), "%s",
                message == nullptr ? "unknown seance failure" : message);
}

void ResetScriptEvents() {
  for (RecoveredScriptEvent& event : g_scriptEvents) {
    event.inUse = false;
  }
}

void RollBackPartiallyOpenedArena(SimulationContext* context) {
  if (context == nullptr) return;
  if (g_arena.getContext() == context || context->isExist("Storage")) {
    g_arenaOpen = true;
    RecoveredArenaSeance_Release();
  }
}

RecoveredScriptEvent* ScriptEvent(int index) {
  if (index < 0 || index >= kScriptEventCount ||
      !g_scriptEvents[index].inUse) {
    return nullptr;
  }
  return &g_scriptEvents[index];
}

void ScriptOpenEventData(TProcessContext* pc, void*) {
  for (int index = 0; index < kScriptEventCount; ++index) {
    if (!g_scriptEvents[index].inUse) {
      g_scriptEvents[index].inUse = true;
      g_scriptEvents[index].data.open(
          static_cast<s_EventDataOpen>(SC_PARI(0)));
      SC_PARI(1) = index;
      return;
    }
  }
  SC_PARI(1) = -1;
}

void ScriptCloseEventData(TProcessContext* pc, void*) {
  RecoveredScriptEvent* event = ScriptEvent(SC_PARI(0));
  if (event != nullptr) event->data.close();
}

void ScriptWriteStr(TProcessContext* pc, void*) {
  RecoveredScriptEvent* event = ScriptEvent(SC_PARI(1));
  if (event != nullptr) event->data.putStr(SC_PARS(0));
}

void ScriptWriteObjectID(TProcessContext* pc, void*) {
  RecoveredScriptEvent* event = ScriptEvent(SC_PARI(2));
  if (event != nullptr) {
    event->data.putObjectID(KR_ObjectID(SC_PARI(1), SC_PARI(0)));
  }
}

void ScriptSendEventNow(TProcessContext* pc, void* arena) {
  RecoveredScriptEvent* event = ScriptEvent(SC_PARI(3));
  ct_Arena* storage = static_cast<ct_Arena*>(arena);
  if (event == nullptr || storage == nullptr ||
      storage->getContext() == nullptr) {
    return;
  }

  event->label = SC_PARI(2);
  event->timeStamp = Session::m_moment;
  event->destination.Init(SC_PARI(1), SC_PARI(0));
  event->source = storage->getObjectID();
  event->inUse = false;
  storage->getContext()->sendEventNow(*event);
}

void ScriptSearchObjectID(TProcessContext* pc, void* arena) {
  ct_Arena* storage = static_cast<ct_Arena*>(arena);
  const KR_ObjectID object =
      storage->getContext()->searchObject(SC_PARS(0));
  SC_PARI(2) = object.id;
  SC_PARI(1) = object.getCachePos();
}

void ScriptAddClassTable(TProcessContext* pc, void* arena) {
  ct_Arena* storage = static_cast<ct_Arena*>(arena);
  SC_PARI(2) = storage->addClassTable(SC_PARS(1), SC_PARI(0));
}

void ScriptNewObject(TProcessContext* pc, void* arena) {
  ct_Arena* storage = static_cast<ct_Arena*>(arena);
  const KR_ObjectID object = storage->newObject(SC_PARI(3), SC_PARS(2));
  SC_PARI(1) = object.id;
  SC_PARI(0) = object.getCachePos();
}

TLinkExtern g_bootstrapFunctions[] = {
    {"s_OpenEventData", ScriptOpenEventData, nullptr},
    {"s_CloseEventData", ScriptCloseEventData, nullptr},
    {"s_WriteStr", ScriptWriteStr, nullptr},
    {"s_WriteObjectID", ScriptWriteObjectID, nullptr},
    {"s_SendEventNow", ScriptSendEventNow, nullptr},
    {"s_SearchObjectID", ScriptSearchObjectID, nullptr},
    {"s_AddClassTable", ScriptAddClassTable, nullptr},
    {"s_New", ScriptNewObject, nullptr},
    {nullptr, nullptr, nullptr}};

TLinkConstExtern g_bootstrapConstants[] = {{nullptr, nullptr, 0}};

void CleanupScriptAttempt(ScriptAttempt* attempt) {
  if (attempt == nullptr) return;
  if (attempt->processInitialized) {
    std::free(attempt->processes.m_stacks);
    std::free(attempt->processes.m_process);
  }
  if (attempt->compilerInitialized) {
    sc_DeleteTSuaCript(&attempt->compiler);
  }
  std::free(attempt->source);
  std::free(attempt);
}

#ifdef _MSC_VER
// The recovered compiler reports errors through setjmp/longjmp. All mutable
// state in this scope lives in a POD heap block and is cleaned explicitly.
#pragma warning(push)
#pragma warning(disable : 4611)
#endif
bool CompileAndRunBootstrap(SimulationContext* context, double startTime) {
  ScriptAttempt* attempt =
      static_cast<ScriptAttempt*>(std::calloc(1, sizeof(ScriptAttempt)));
  if (attempt == nullptr) {
    g_issues |= RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE;
    SetError("could not allocate script bootstrap state");
    return false;
  }

  attempt->stage = SCRIPT_STAGE_ALLOCATION;
  const std::size_t sourceLength = std::strlen(kVehicleBootstrapScript);
  std::size_t normalizedLength = sourceLength;
  for (std::size_t index = 0; index < sourceLength; ++index) {
    if (kVehicleBootstrapScript[index] == '\n' &&
        (index == 0 || kVehicleBootstrapScript[index - 1] != '\r')) {
      ++normalizedLength;
    }
  }
  attempt->source =
      static_cast<char*>(std::malloc(normalizedLength + 1));
  if (attempt->source == nullptr) {
    CleanupScriptAttempt(attempt);
    g_issues |= RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE;
    SetError("could not allocate script bootstrap source");
    return false;
  }
  std::size_t output = 0;
  for (std::size_t index = 0; index < sourceLength; ++index) {
    if (kVehicleBootstrapScript[index] == '\n' &&
        (index == 0 || kVehicleBootstrapScript[index - 1] != '\r')) {
      attempt->source[output++] = '\r';
    }
    attempt->source[output++] = kVehicleBootstrapScript[index];
  }
  attempt->source[output] = 0;

  attempt->stage = SCRIPT_STAGE_INITIALIZATION;
  if (!sc_InitTSuaCript(&attempt->compiler, 8 * 1024, 8 * 1024, 512,
                        32 * 1024, 32 * 1024, 8 * 1024, 256, 1)) {
    CleanupScriptAttempt(attempt);
    g_issues |= RECOVERED_ARENA_SEANCE_SCRIPT_INITIALIZATION_FAILURE;
    SetError("could not initialize script compiler buffers");
    return false;
  }
  attempt->compilerInitialized = true;

  if (SUACRIPT_REGISTER_ERROR_HANDLE(attempt->compiler)) {
    const int failedStage = attempt->stage;
    SetError(attempt->compiler.m_heap.m_error.m_msg);
    CleanupScriptAttempt(attempt);
    g_issues |= failedStage <= SCRIPT_STAGE_COMPILE
                    ? RECOVERED_ARENA_SEANCE_SCRIPT_COMPILE_FAILURE
                    : RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE;
    return false;
  }

  attempt->stage = SCRIPT_STAGE_COMPILE;
  sc_InitScannerFromMem(&attempt->compiler, attempt->source);
  sc_Compile(&attempt->compiler, kBootstrapProgramName,
             g_bootstrapFunctions, g_bootstrapConstants);
  const int program =
      sc_ProgrammId(&attempt->compiler, kBootstrapProgramName);
  if (program < 0 || program >= attempt->compiler.m_programmCnt) {
    CleanupScriptAttempt(attempt);
    g_issues |= RECOVERED_ARENA_SEANCE_SCRIPT_COMPILE_FAILURE;
    SetError("script compiler did not publish the bootstrap program");
    return false;
  }

  attempt->stage = SCRIPT_STAGE_PROCESS;
  if (!sc_InitTSetOfProcess(&attempt->processes, 512, 1)) {
    CleanupScriptAttempt(attempt);
    g_issues |= RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE;
    SetError("could not initialize script process storage");
    return false;
  }
  attempt->processInitialized = true;
  if (!sc_CreateProcess(&attempt->processes, &attempt->compiler, program, 512,
                        4096, g_bootstrapConstants)) {
    CleanupScriptAttempt(attempt);
    g_issues |= RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE;
    SetError("could not create script bootstrap process");
    return false;
  }

  attempt->stage = SCRIPT_STAGE_EXECUTION;
  context->start(startTime);
  bool completed = false;
  for (int slice = 0; slice < kMaximumVmSlices; ++slice) {
    if (sc_RunProcess(&attempt->processes, 1, &g_arena) != 0) {
      completed = true;
      break;
    }
  }
  CleanupScriptAttempt(attempt);
  if (!completed) {
    g_issues |= RECOVERED_ARENA_SEANCE_SCRIPT_TIMEOUT;
    SetError("script bootstrap exceeded its bounded VM slice budget");
    return false;
  }
  return true;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace

int RecoveredArenaSeance_Initialize(SimulationContext* context,
                                    double startTime) {
  RecoveredArenaSeance_Release();
  g_issues = 0;
  g_lastError[0] = 0;
  ResetScriptEvents();

  if (context == nullptr) {
    g_issues |= RECOVERED_ARENA_SEANCE_INVALID_CONTEXT;
    SetError("seance requires a SimulationContext");
    return FALSE;
  }

  try {
    g_arena.openSeance(context, kSceneWidth, kSceneDepth);
    g_arenaOpen = g_arena.getContext() == context &&
                  !context->searchObject("Storage").isNUL();
  } catch (const std::bad_alloc&) {
    RollBackPartiallyOpenedArena(context);
    g_issues |= RECOVERED_ARENA_SEANCE_OPEN_FAILURE;
    SetError("arena allocation failed");
    return FALSE;
  } catch (...) {
    RollBackPartiallyOpenedArena(context);
    g_issues |= RECOVERED_ARENA_SEANCE_OPEN_FAILURE;
    SetError("arena open failed");
    return FALSE;
  }
  if (!g_arenaOpen) {
    g_issues |= RECOVERED_ARENA_SEANCE_OPEN_FAILURE;
    SetError("arena did not publish Storage");
    RecoveredArenaSeance_Release();
    return FALSE;
  }

  if (!CompileAndRunBootstrap(context, startTime)) {
    RecoveredArenaSeance_Release();
    return FALSE;
  }
  g_scriptCompleted = true;

  if (g_arena.searchSeanceClassTable("VehicleAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Vehicle") == ct_NULLID) {
    g_issues |= RECOVERED_ARENA_SEANCE_VEHICLE_TABLE_MISSING;
    SetError("script did not create the Vehicle tables");
    RecoveredArenaSeance_Release();
    return FALSE;
  }

  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  if (vehicleID.isNUL()) {
    g_issues |= RECOVERED_ARENA_SEANCE_VEHICLE_OBJECT_MISSING;
    SetError("script did not create Vehicle.Default");
    RecoveredArenaSeance_Release();
    return FALSE;
  }
  g_vehicle = static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  if (g_vehicle == nullptr) {
    g_issues |= RECOVERED_ARENA_SEANCE_VEHICLE_INTERFACE_MISSING;
    SetError("Vehicle.Default does not expose IVehicleIID");
    RecoveredArenaSeance_Release();
    return FALSE;
  }

  g_vehicleReady = true;
  return TRUE;
}

void RecoveredArenaSeance_Release() {
  g_vehicleReady = false;
  g_scriptCompleted = false;
  g_vehicle = nullptr;
  ResetScriptEvents();
  if (!g_arenaOpen) return;
  g_arena.closeSeance();
  g_arenaOpen = false;
}

bool RecoveredArenaSeance_IsOpen() { return g_arenaOpen; }

bool RecoveredArenaSeance_ScriptCompleted() { return g_scriptCompleted; }

bool RecoveredArenaSeance_VehicleReady() { return g_vehicleReady; }

unsigned int RecoveredArenaSeance_Issues() { return g_issues; }

const char* RecoveredArenaSeance_LastError() { return g_lastError; }
