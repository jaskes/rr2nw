#include "RecoveredLegacyScriptHost.h"

#include <cstdio>
#include <cstring>

#include "enum/spaceenum.h"
#include "graph.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/attrmsg.h"
#include "message/fartmsg.h"
#include "message/fountmsg.h"
#include "message/lampmsg.h"
#include "message/sparkmsg.h"
#include "message/routmsg.h"
#include "obase/route/route.h"
#include "storage/h/subject.h"

namespace {

RecoveredLegacyScriptHost* Host(void* userData) {
  return static_cast<RecoveredLegacyScriptHost*>(userData);
}

void ScriptOpenEventData(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARI(1) =
      host == nullptr
          ? -1
          : host->OpenEventData(static_cast<s_EventDataOpen>(SC_PARI(0)));
}

void ScriptCloseEventData(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->CloseEventData(SC_PARI(0));
}

void ScriptDescendEventData(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->DescendEventData(SC_PARI(2), SC_PARI(1), SC_PARI(0));
  }
}

void ScriptAscendEventData(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->AscendEventData(SC_PARI(0));
}

void ScriptWriteInt(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->WriteInt(SC_PARI(1), SC_PARI(0));
}

void ScriptWriteFloat(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->WriteFloat(SC_PARI(1), SC_PARF(0));
}

void ScriptWriteString(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->WriteString(SC_PARI(1), SC_PARS(0));
}

void ScriptWriteObjectID(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->WriteObjectID(SC_PARI(2),
                        KR_ObjectID(SC_PARI(1), SC_PARI(0)));
  }
}

void ScriptSendEventNow(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->SendEventNow(SC_PARI(3), SC_PARI(2),
                       KR_ObjectID(SC_PARI(1), SC_PARI(0)));
  }
}

void ScriptSearchObjectID(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  const KR_ObjectID object =
      host == nullptr ? KR_ObjectID::NUL() : host->SearchObject(SC_PARS(0));
  if (host != nullptr) {
    host->WriteScriptInteger(pc, SC_PARI(2), object.id);
    host->WriteScriptInteger(pc, SC_PARI(1), object.getCachePos());
  }
}

void ScriptAddClassTable(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARI(2) =
      host == nullptr ? ct_NULLID
                      : host->AddClassTable(SC_PARS(1), SC_PARI(0));
}

void ScriptNewObject(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  const KR_ObjectID object =
      host == nullptr
          ? KR_ObjectID::NUL()
          : host->NewObject(SC_PARI(3), SC_PARS(2));
  if (host != nullptr) {
    host->WriteScriptInteger(pc, SC_PARI(1), object.id);
    host->WriteScriptInteger(pc, SC_PARI(0), object.getCachePos());
  }
}

void ScriptNewObjectWithoutResult(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->NewObject(SC_PARI(1), SC_PARS(0));
}

void ScriptNewObjectByClass(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->NewObject(SC_PARS(1), SC_PARS(0));
}

void ScriptLoadRoute(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->LoadRoute(SC_PARI(2), SC_PARS(1), SC_PARS(0));
  }
}

void ConstSparkSetPhaseCount(TStackCell* cell) {
  cell->i = sp_EV_SET_PHASE_COUNT;
}

void ConstSparkSetPhase(TStackCell* cell) { cell->i = sp_EV_SET_PHASE; }

void ConstRect2DI(TStackCell* cell) { cell->i = RECT2D_I; }

void ConstLightColorYellow(TStackCell* cell) {
  cell->i = LIGHT_COLOR_YELLOW;
}

void ConstAttributeSetInt(TStackCell* cell) {
  cell->i = ATTR_MSG_SET_INT;
}

void ConstAttributeSetDouble(TStackCell* cell) {
  cell->i = ATTR_MSG_SET_DOUBLE;
}

void ConstAttributeSetString(TStackCell* cell) {
  cell->i = ATTR_MSG_SET_STR;
}

void ConstFountainStart(TStackCell* cell) { cell->i = fou_EVCMD_START; }

void ConstFarterStart(TStackCell* cell) { cell->i = START_FARTING; }

void ConstLampStart(TStackCell* cell) { cell->i = lmp_EV_START; }

void ConstLampSetEndPosition(TStackCell* cell) {
  cell->i = lmp_EV_SETENDPOS;
}

TLinkExtern g_bindings[] = {
    {"s_OpenEventData", ScriptOpenEventData, nullptr},
    {"s_CloseEventData", ScriptCloseEventData, nullptr},
    {"s_Descend", ScriptDescendEventData, nullptr},
    {"s_Ascend", ScriptAscendEventData, nullptr},
    {"s_WriteInt", ScriptWriteInt, nullptr},
    {"s_WriteFloat", ScriptWriteFloat, nullptr},
    {"s_WriteStr", ScriptWriteString, nullptr},
    {"s_WriteObjectID", ScriptWriteObjectID, nullptr},
    {"s_SendEventNow", ScriptSendEventNow, nullptr},
    {"s_SearchObjectID", ScriptSearchObjectID, nullptr},
    {"s_AddClassTable", ScriptAddClassTable, nullptr},
    {"s_New", ScriptNewObject, nullptr},
    {"s_NewObject", ScriptNewObjectWithoutResult, nullptr},
    {"s_NewObjectN", ScriptNewObjectByClass, nullptr},
    {"s_LoadRoute", ScriptLoadRoute, nullptr},
    {nullptr, nullptr, nullptr}};

TLinkConstExtern g_constants[] = {
    {"sp_EV_SET_PHASE_COUNT", ConstSparkSetPhaseCount, 0},
    {"sp_EV_SET_PHASE", ConstSparkSetPhase, 0},
    {"RECT2D_I", ConstRect2DI, 0},
    {"LIGHT_COLOR_YELLOW", ConstLightColorYellow, 0},
    {"s_ATTR_MSG_SET_INT", ConstAttributeSetInt, 0},
    {"s_ATTR_MSG_SET_DOUBLE", ConstAttributeSetDouble, 0},
    {"s_ATTR_MSG_SET_STR", ConstAttributeSetString, 0},
    {"fou_EVCMD_START", ConstFountainStart, 0},
    {"START_FARTING", ConstFarterStart, 0},
    {"lmp_EV_START", ConstLampStart, 0},
    {"lmp_EV_SETENDPOS", ConstLampSetEndPosition, 0},
    {nullptr, nullptr, 0}};

}  // namespace

RecoveredLegacyScriptHost::RecoveredLegacyScriptHost(ct_Arena* arena)
    : m_arena(arena), m_issues(0), m_lastError{} {
  Reset();
}

void RecoveredLegacyScriptHost::Reset() {
  m_issues = 0;
  m_lastError[0] = 0;
  for (ScriptEvent& event : m_events) event.inUse = false;
}

bool RecoveredLegacyScriptHost::IsHealthy() const { return m_issues == 0; }

unsigned int RecoveredLegacyScriptHost::Issues() const { return m_issues; }

const char* RecoveredLegacyScriptHost::LastError() const {
  return m_lastError;
}

int RecoveredLegacyScriptHost::OpenEventData(s_EventDataOpen style) {
  for (int index = 0; index < kEventCount; ++index) {
    if (!m_events[index].inUse) {
      m_events[index].inUse = true;
      m_events[index].data.open(style);
      return index;
    }
  }
  Report(RECOVERED_LEGACY_SCRIPT_HOST_EVENT_POOL_EXHAUSTED,
         "script event pool is exhausted");
  return -1;
}

void RecoveredLegacyScriptHost::CloseEventData(int eventIndex) {
  ScriptEvent* event = Event(eventIndex, "close event data");
  if (event != nullptr) event->data.close();
}

void RecoveredLegacyScriptHost::DescendEventData(int eventIndex, int tag,
                                                  int index) {
  ScriptEvent* event = Event(eventIndex, "descend event data");
  if (event != nullptr) event->data.descend(tag, index);
}

void RecoveredLegacyScriptHost::AscendEventData(int eventIndex) {
  ScriptEvent* event = Event(eventIndex, "ascend event data");
  if (event != nullptr) event->data.ascend();
}

void RecoveredLegacyScriptHost::WriteInt(int eventIndex, int value) {
  ScriptEvent* event = Event(eventIndex, "write event integer");
  if (event != nullptr) event->data.putInt(value);
}

void RecoveredLegacyScriptHost::WriteFloat(int eventIndex, double value) {
  ScriptEvent* event = Event(eventIndex, "write event float");
  if (event != nullptr) event->data.putDouble(value);
}

void RecoveredLegacyScriptHost::WriteString(int eventIndex,
                                            const char* value) {
  ScriptEvent* event = Event(eventIndex, "write event string");
  if (event != nullptr && value != nullptr) event->data.putStr(value);
}

void RecoveredLegacyScriptHost::WriteObjectID(
    int eventIndex, const KR_ObjectID& object) {
  ScriptEvent* event = Event(eventIndex, "write event object ID");
  if (event != nullptr) event->data.putObjectID(object);
}

void RecoveredLegacyScriptHost::SendEventNow(
    int eventIndex, int label, const KR_ObjectID& destination) {
  ScriptEvent* event = Event(eventIndex, "send immediate event");
  if (event == nullptr || !ArenaReady("send immediate event")) return;

  event->label = label;
  event->timeStamp = Session::m_moment;
  event->destination = destination;
  event->source = m_arena->getObjectID();
  event->inUse = false;
  m_arena->getContext()->sendEventNow(*event);
}

bool RecoveredLegacyScriptHost::WriteScriptInteger(TProcessContext* process,
                                                     int reference,
                                                     int value) {
  if (process == nullptr || process->m_stack == nullptr || reference < 0 ||
      reference >= process->m_stackSize) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_INVALID_STACK_REFERENCE,
           "script variable reference is outside the process stack");
    return false;
  }
  process->m_stack[reference].i = value;
  return true;
}

KR_ObjectID RecoveredLegacyScriptHost::SearchObject(const char* name) {
  if (!ArenaReady("search object") || name == nullptr) {
    return KR_ObjectID::NUL();
  }
  // A missing symbolic name is valid legacy data flow: the script receives
  // NUL and decides whether that is optional. Infrastructure errors above are
  // still fail-closed.
  return m_arena->getContext()->isExist(name)
             ? m_arena->getContext()->searchObject(name)
             : KR_ObjectID::NUL();
}

int RecoveredLegacyScriptHost::AddClassTable(const char* name, int capacity) {
  if (!ArenaReady("add class table") || name == nullptr) return ct_NULLID;
  const int table = m_arena->addClassTable(name, capacity);
  if (table == ct_NULLID) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_CLASS_TABLE_FAILURE,
           "script class table creation failed");
  }
  return table;
}

KR_ObjectID RecoveredLegacyScriptHost::NewObject(int classTable,
                                                  const char* name) {
  if (!ArenaReady("create object") || name == nullptr) {
    return KR_ObjectID::NUL();
  }
  KR_ObjectID object = m_arena->newObject(classTable, name);
  if (object.isNUL()) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_OBJECT_CREATION_FAILURE,
           "script object creation failed");
  }
  return object;
}

KR_ObjectID RecoveredLegacyScriptHost::NewObject(const char* className,
                                                  const char* name) {
  if (!ArenaReady("create object by class") || className == nullptr ||
      name == nullptr) {
    return KR_ObjectID::NUL();
  }
  KR_ObjectID object = m_arena->newObject(className, name);
  if (object.isNUL()) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_OBJECT_CREATION_FAILURE,
           "script named-class object creation failed");
  }
  return object;
}

KR_ObjectID RecoveredLegacyScriptHost::LoadRoute(
    int classTable, const char* fileName, const char* routeName) {
  if (!ArenaReady("load route") || fileName == nullptr ||
      routeName == nullptr) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE,
           "script route load received invalid input");
    return KR_ObjectID::NUL();
  }
  if (std::strlen(fileName) + 1 > s_EventData::BUFF_SIZE) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE,
           "script route path exceeds the event payload");
    return KR_ObjectID::NUL();
  }

  const int previousNodeCount = Route::m_totalNodePos;
  KR_ObjectID routeID = NewObject(classTable, routeName);
  if (routeID.isNUL()) return routeID;

  KR_Event event;
  event.label = ROUTE_LOAD;
  event.source = m_arena->getObjectID();
  event.destination = routeID;
  event.timeStamp = 0.1;
  event.data.open(EDO_WRITE).putStr(fileName).close();
  m_arena->getContext()->sendEventNow(event);

  IRouteObject* route = static_cast<IRouteObject*>(
      m_arena->getContext()->queryInterface(routeID, IRouteObjectIID));
  if (route == nullptr) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_INTERFACE_FAILURE,
           "script route object does not expose IRouteObjectIID");
  } else if (route->GetNodeCnt() <= 0 ||
             Route::m_totalNodePos <= previousNodeCount) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE,
           "script route file did not publish any nodes");
  }
  return routeID;
}

TLinkExtern* RecoveredLegacyScriptHost::Bindings() { return g_bindings; }

TLinkConstExtern* RecoveredLegacyScriptHost::Constants() {
  return g_constants;
}

void RecoveredLegacyScriptHost::ResetConstantLinks() {
  for (TLinkConstExtern* constant = g_constants;
       constant->m_name != nullptr; ++constant) {
    constant->m_offset = -1;
  }
}

int RecoveredLegacyScriptHost::CopyLinkedConstants(
    TLinkConstExtern* destination, int capacity) {
  if (destination == nullptr || capacity < kConstantCount + 1) return -1;

  int count = 0;
  for (TLinkConstExtern* constant = g_constants;
       constant->m_name != nullptr; ++constant) {
    if (constant->m_offset >= 0) destination[count++] = *constant;
  }
  destination[count] = {nullptr, nullptr, 0};
  return count;
}

RecoveredLegacyScriptHost::ScriptEvent* RecoveredLegacyScriptHost::Event(
    int eventIndex, const char* operation) {
  if (eventIndex >= 0 && eventIndex < kEventCount &&
      m_events[eventIndex].inUse) {
    return &m_events[eventIndex];
  }

  char message[256] = {};
  std::snprintf(message, sizeof(message), "%s received invalid event handle",
                operation == nullptr ? "script operation" : operation);
  Report(RECOVERED_LEGACY_SCRIPT_HOST_INVALID_EVENT, message);
  return nullptr;
}

bool RecoveredLegacyScriptHost::ArenaReady(const char* operation) {
  if (m_arena != nullptr && m_arena->getContext() != nullptr) return true;

  char message[256] = {};
  std::snprintf(message, sizeof(message), "%s has no active Arena",
                operation == nullptr ? "script operation" : operation);
  Report(RECOVERED_LEGACY_SCRIPT_HOST_ARENA_UNAVAILABLE, message);
  return false;
}

void RecoveredLegacyScriptHost::Report(unsigned int issue,
                                       const char* message) {
  m_issues |= issue;
  if (m_lastError[0] == 0) {
    std::snprintf(m_lastError, sizeof(m_lastError), "%s",
                  message == nullptr ? "legacy script host failed" : message);
  }
}
