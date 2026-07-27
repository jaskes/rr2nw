#include "RecoveredLegacyScriptHost.h"

#include <cstdio>
#include <cstring>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
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
  SC_PARI(2) = object.id;
  SC_PARI(1) = object.getCachePos();
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
  SC_PARI(1) = object.id;
  SC_PARI(0) = object.getCachePos();
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

TLinkExtern g_bindings[] = {
    {"s_OpenEventData", ScriptOpenEventData, nullptr},
    {"s_CloseEventData", ScriptCloseEventData, nullptr},
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

TLinkConstExtern g_constants[] = {{nullptr, nullptr, 0}};

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
