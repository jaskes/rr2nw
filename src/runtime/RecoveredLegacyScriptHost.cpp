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
#include "message/groupmsg.h"
#include "message/comanmsg.h"
#include "message/skinmsg.h"
#include "message/lampmsg.h"
#include "message/sparkmsg.h"
#include "message/routmsg.h"
#include "message/peopmsg.h"
#include "message/recrcenmsg.h"
#include "message/unitmsg.h"
#include "mproj/h/mproj.h"
#include "obase/route/route.h"
#include "i/unit.i"
#include "i/commander.i"
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

void ScriptForceRemoveObject(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->ForceRemoveObject(SC_PARS(0));
}

void ScriptRemoveObject(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->RemoveObject(SC_PARS(1), SC_PARF(0));
}

void ScriptSearchSeanceClassTable(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARI(1) =
      host == nullptr ? ct_NULLID : host->SearchClassTable(SC_PARS(0));
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

void ScriptSetCommander(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARI(2) = host == nullptr
                   ? 0
                   : host->SetCommander(SC_PARS(1), SC_PARS(0));
}

void ScriptSetHostileCommander(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->SetCommanderRelation(KR_ObjectID(SC_PARI(3), SC_PARI(2)),
                               KR_ObjectID(SC_PARI(1), SC_PARI(0)), true);
  }
}

void ScriptSetFriendlyCommander(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->SetCommanderRelation(KR_ObjectID(SC_PARI(3), SC_PARI(2)),
                               KR_ObjectID(SC_PARI(1), SC_PARI(0)), false);
  }
}

void ScriptCreateProjectTable(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->CreateProjectTable(SC_PARI(2), SC_PARI(1), SC_PARI(0));
  }
}

void ScriptNewProjectNode(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARI(3) = host == nullptr
                   ? mp_NodeNULL()
                   : host->NewProjectNode(SC_PARI(2), SC_PARI(1),
                                          SC_PARI(0));
}

void ScriptOpenProjectData(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->OpenProjectData(SC_PARI(0));
}

void ScriptCloseProjectData(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->CloseProjectData(SC_PARI(0));
}

void ScriptProjectWriteInt(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->ProjectWriteInt(SC_PARI(1), SC_PARI(0));
}

void ScriptProjectWriteFloat(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->ProjectWriteFloat(SC_PARI(1), SC_PARF(0));
}

void ScriptProjectWriteString(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->ProjectWriteString(SC_PARI(1), SC_PARS(0));
}

void ScriptProjectNodeSetLink(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->ProjectNodeSetLink(SC_PARI(2), SC_PARI(1), SC_PARI(0));
  }
}

void ScriptProjectNodeNull(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARI(0) = host == nullptr ? mp_NodeNULL() : host->ProjectNodeNull();
}

void ScriptNewProject(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->NewProject(SC_PARS(2), SC_PARI(1), SC_PARI(0) != 0);
  }
}

void ScriptDeferMissionHowitzer(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->DeferMissionHowitzer(SC_PARI(4), SC_PARS(3), SC_PARS(2),
                               SC_PARF(1), SC_PARS(0));
  }
}

void ScriptDeferMissionDestroyable(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->DeferMissionDestroyable(SC_PARS(2), SC_PARS(1), SC_PARS(0));
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

void ConstSkinLoad(TStackCell* cell) { cell->i = sk_EV_LOAD; }

void ConstSkinProgram(TStackCell* cell) { cell->i = sk_EV_PROG; }

void ConstPeopleSetAnimation(TStackCell* cell) {
  cell->i = pe_EV_SETANIM;
}

void ConstTaxiSetToPosition(TStackCell* cell) {
  // Preserved May retail nw.exe constant callback at 0x00590FC0.
  cell->i = 0x139A;
}

void ConstPeopleStart(TStackCell* cell) { cell->i = pe_EVCMD_START; }

void ConstPeopleStartExtended(TStackCell* cell) {
  cell->i = pe_EVCMD_START_EX;
}

void ConstGroupAddMemberByName(TStackCell* cell) {
  cell->i = GROUP_ADD_MEMBER_N;
}

void ConstCommanderAddMemberByName(TStackCell* cell) {
  cell->i = COMMANDER_ADD_MEMBER_N;
}

void ConstUnitSetAttributePosition(TStackCell* cell) {
  cell->i = t_EV_SET_ATTR_POS;
}

void ConstRecruitCenterSetEject(TStackCell* cell) {
  cell->i = rc_SET_EJECT;
}

void ConstRecruitCenterSetVideo(TStackCell* cell) {
  cell->i = rc_SET_VIDEO;
}

void ConstRecruitCenterSetDefaultTaxi(TStackCell* cell) {
  cell->i = rc_SET_DEFTAXI;
}

void ConstRecruitCenterSetDictionary(TStackCell* cell) {
  cell->i = rc_SET_DICTIONARY;
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
    {"s_SearchObjectIDNoWarning", ScriptSearchObjectID, nullptr},
    {"s_RemoveObject", ScriptRemoveObject, nullptr},
    {"s_ForceRemoveObject", ScriptForceRemoveObject, nullptr},
    {"s_SearchSeanceClassTable", ScriptSearchSeanceClassTable, nullptr},
    {"s_AddClassTable", ScriptAddClassTable, nullptr},
    {"s_New", ScriptNewObject, nullptr},
    {"s_NewObject", ScriptNewObjectWithoutResult, nullptr},
    {"s_NewObjectN", ScriptNewObjectByClass, nullptr},
    {"s_LoadRoute", ScriptLoadRoute, nullptr},
    {"s_SetCommander", ScriptSetCommander, nullptr},
    {"s_SetHostileCommander", ScriptSetHostileCommander, nullptr},
    {"s_SetFriendlyCommander", ScriptSetFriendlyCommander, nullptr},
    {"s_CreateProjectTable", ScriptCreateProjectTable, nullptr},
    {"s_NewPNode", ScriptNewProjectNode, nullptr},
    {"s_OpenProjectData", ScriptOpenProjectData, nullptr},
    {"s_CloseProjectData", ScriptCloseProjectData, nullptr},
    {"s_ProjectWriteInt", ScriptProjectWriteInt, nullptr},
    {"s_ProjectWriteFloat", ScriptProjectWriteFloat, nullptr},
    {"s_ProjectWriteStr", ScriptProjectWriteString, nullptr},
    {"s_ProjectNodeSetLink", ScriptProjectNodeSetLink, nullptr},
    {"s_PNodeNULL", ScriptProjectNodeNull, nullptr},
    {"s_NewProjectEx", ScriptNewProject, nullptr},
    {"s_DeferMissionHowitzer", ScriptDeferMissionHowitzer, nullptr},
    {"s_DeferMissionDestroyable", ScriptDeferMissionDestroyable, nullptr},
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
    {"sk_EV_LOAD", ConstSkinLoad, 0},
    {"sk_EV_PROG", ConstSkinProgram, 0},
    {"pe_EV_SETANIM", ConstPeopleSetAnimation, 0},
    {"taxi_SET_TO_POS", ConstTaxiSetToPosition, 0},
    {"pe_EVCMD_START", ConstPeopleStart, 0},
    {"pe_EVCMD_START_EX", ConstPeopleStartExtended, 0},
    {"s_GROUP_ADD_MEMBER_N", ConstGroupAddMemberByName, 0},
    {"s_COMMANDER_ADD_MEMBER_N", ConstCommanderAddMemberByName, 0},
    {"t_EV_SET_ATTR_POS", ConstUnitSetAttributePosition, 0},
    {"rc_SET_EJECT", ConstRecruitCenterSetEject, 0},
    {"rc_SET_VIDEO", ConstRecruitCenterSetVideo, 0},
    {"rc_SET_DEFTAXI", ConstRecruitCenterSetDefaultTaxi, 0},
    {"rc_SET_DICTIONARY", ConstRecruitCenterSetDictionary, 0},
    {nullptr, nullptr, 0}};

}  // namespace

RecoveredLegacyScriptHost::RecoveredLegacyScriptHost(ct_Arena* arena)
    : m_arena(arena), m_issues(0), m_lastError{},
      m_projectTableCreated(false), m_projectDataOpen(false),
      m_projectCapacity(0), m_projectNodeCapacity(0),
      m_projectHeapCapacity(0), m_projectNodeCount(0), m_projectCount(0),
      m_projectDataBytes(0), m_openProjectNode(mp_NodeNULL()),
      m_deferredMissionHowitzerCount(0),
      m_deferredMissionDestroyableCount(0) {
  Reset();
}

void RecoveredLegacyScriptHost::Reset() {
  m_issues = 0;
  m_lastError[0] = 0;
  for (ScriptEvent& event : m_events) event.inUse = false;
  m_projectTableCreated = false;
  m_projectDataOpen = false;
  m_projectCapacity = 0;
  m_projectNodeCapacity = 0;
  m_projectHeapCapacity = 0;
  m_projectNodeCount = 0;
  m_projectCount = 0;
  m_projectDataBytes = 0;
  m_openProjectNode = mp_NodeNULL();
  m_deferredMissionHowitzerCount = 0;
  m_deferredMissionDestroyableCount = 0;
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

int RecoveredLegacyScriptHost::SearchClassTable(const char* name) {
  if (!ArenaReady("search class table") || name == nullptr) return ct_NULLID;
  return m_arena->searchSeanceClassTable(name);
}

bool RecoveredLegacyScriptHost::RemoveObject(const char* name, double from) {
  if (!ArenaReady("remove object") || name == nullptr) return false;
  SimulationContext* context = m_arena->getContext();
  if (!context->isExist(name)) return true;

  const KR_ObjectID object = context->searchObject(name);
  // Retail scripts pass a level-local absolute moment (and use zero for an
  // immediate removal).  Reuse ct_Storage's original delayed-delete event so
  // the operation participates in the same scheduler and saveable event
  // queue as native subjects.
  if (from <= Session::m_moment) {
    m_arena->delObject(object);
    return !context->isExist(name);
  }
  m_arena->delObject(object, from);
  return true;
}

bool RecoveredLegacyScriptHost::ForceRemoveObject(const char* name) {
  if (!ArenaReady("force remove object") || name == nullptr) return false;
  SimulationContext* context = m_arena->getContext();
  if (!context->isExist(name)) return true;
  context->removeObject(context->searchObject(name));
  return !context->isExist(name);
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

  // Many retail LEVEL0 rosters request the same symbolic route once per
  // subject.  The original context allowed duplicate names, but every later
  // search returned the first object, leaving the duplicate route nodes
  // unreachable and eventually exhausting Route's fixed 3000-node arena.
  // Reuse the already published route: this preserves the identity scripts
  // actually observe and keeps the legacy save payload/layout unchanged.
  SimulationContext* context = m_arena->getContext();
  if (context->isExist(routeName)) {
    const KR_ObjectID existingID = context->searchObject(routeName);
    IRouteObject* existing = static_cast<IRouteObject*>(
        context->queryInterface(existingID, IRouteObjectIID));
    if (existing == nullptr) {
      Report(RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_INTERFACE_FAILURE,
             "existing symbolic route does not expose IRouteObjectIID");
    } else if (existing->GetNodeCnt() <= 0) {
      Report(RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE,
             "existing symbolic route did not publish any nodes");
    }
    return existingID;
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

int RecoveredLegacyScriptHost::SetCommander(const char* objectName,
                                             const char* commanderName) {
  if (!ArenaReady("set commander") || objectName == nullptr ||
      commanderName == nullptr)
    return 0;
  KR_ObjectID object = SearchObject(objectName);
  KR_ObjectID commander = SearchObject(commanderName);
  if (object.isNUL() || commander.isNUL()) return 0;
  IUnit* unit = static_cast<IUnit*>(
      m_arena->getContext()->queryInterface(object, IUnitIID));
  if (unit == nullptr) return 0;
  unit->setCommander(commander);
  return 1;
}

bool RecoveredLegacyScriptHost::SetCommanderRelation(
    const KR_ObjectID& commander, const KR_ObjectID& relativeCommander,
    bool hostile) {
  KR_ObjectID mutableLeft = commander;
  KR_ObjectID mutableRight = relativeCommander;
  if (!ArenaReady("set commander relation") || mutableLeft.isNUL() ||
      mutableRight.isNUL())
    return false;
  ICommander* left = static_cast<ICommander*>(
      m_arena->getContext()->queryInterface(commander, ICommanderIID));
  ICommander* right = static_cast<ICommander*>(m_arena->getContext()->
      queryInterface(relativeCommander, ICommanderIID));
  if (left == nullptr || right == nullptr) return false;
  if (hostile) {
    left->setHostile(mutableRight);
    right->setHostile(mutableLeft);
  } else {
    left->setFriendly(mutableRight);
    right->setFriendly(mutableLeft);
  }
  return true;
}

bool RecoveredLegacyScriptHost::CreateProjectTable(int projectCapacity,
                                                    int nodeCapacity,
                                                    int heapCapacity) {
  if (!ArenaReady("create project table")) return false;
  if (m_projectTableCreated || projectCapacity <= 0 || projectCapacity > 200 ||
      nodeCapacity <= 0 || nodeCapacity > 1024 || heapCapacity <= 0 ||
      heapCapacity > 10240) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_TABLE_FAILURE,
           "script requested an invalid or duplicate project table");
    return false;
  }
  projectTable.create(projectCapacity, m_arena->getContext(), *m_arena,
                      nodeCapacity, heapCapacity);
  if (projectTable.getClassTableID() == ct_NULLID) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_TABLE_FAILURE,
           "project table registration failed");
    return false;
  }
  m_projectTableCreated = true;
  m_projectCapacity = projectCapacity;
  m_projectNodeCapacity = nodeCapacity;
  m_projectHeapCapacity = heapCapacity;
  return true;
}

bool RecoveredLegacyScriptHost::ProjectNodeValid(int node,
                                                  bool allowNull) const {
  const int decoded = mp_Code2Int(node);
  return (allowNull && decoded == -1) ||
         (decoded >= 0 && decoded < m_projectNodeCount);
}

int RecoveredLegacyScriptHost::NewProjectNode(int command, int left,
                                               int right) {
  if (!m_projectTableCreated || m_projectNodeCount >= m_projectNodeCapacity ||
      command < 0 || command > 35 || !ProjectNodeValid(left, true) ||
      !ProjectNodeValid(right, true)) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_NODE_FAILURE,
           "script requested an invalid or overflowing project node");
    return mp_NodeNULL();
  }
  const int node = mp_New(projectTable, command, left, right);
  if (node == mp_NodeNULL()) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_NODE_FAILURE,
           "project node allocation failed");
    return node;
  }
  ++m_projectNodeCount;
  return node;
}

bool RecoveredLegacyScriptHost::OpenProjectData(int node) {
  if (!m_projectTableCreated || m_projectDataOpen ||
      !ProjectNodeValid(node)) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_DATA_FAILURE,
           "script opened invalid or nested project data");
    return false;
  }
  mp_OpenData(projectTable, node, EDO_WRITE);
  m_projectDataOpen = true;
  m_openProjectNode = node;
  return true;
}

bool RecoveredLegacyScriptHost::CloseProjectData(int node) {
  if (!m_projectDataOpen || node != m_openProjectNode ||
      !ProjectNodeValid(node)) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_DATA_FAILURE,
           "script closed mismatched project data");
    return false;
  }
  mp_CloseData(projectTable, node);
  m_projectDataOpen = false;
  m_openProjectNode = mp_NodeNULL();
  return true;
}

bool RecoveredLegacyScriptHost::ReserveProjectData(int node, int bytes) {
  if (!m_projectDataOpen || node != m_openProjectNode || bytes <= 0 ||
      m_projectDataBytes > m_projectHeapCapacity - bytes) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_DATA_FAILURE,
           "script project data write is invalid or exceeds the heap");
    return false;
  }
  m_projectDataBytes += bytes;
  return true;
}

bool RecoveredLegacyScriptHost::ProjectWriteInt(int node, int value) {
  if (!ReserveProjectData(node, 1 + static_cast<int>(sizeof(value))))
    return false;
  mp_WriteInt(projectTable, node, value);
  return true;
}

bool RecoveredLegacyScriptHost::ProjectWriteFloat(int node, double value) {
  if (!ReserveProjectData(node, 1 + static_cast<int>(sizeof(value))))
    return false;
  mp_WriteFloat(projectTable, node, value);
  return true;
}

bool RecoveredLegacyScriptHost::ProjectWriteString(int node,
                                                    const char* value) {
  if (value == nullptr || std::strlen(value) > 4095u) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_DATA_FAILURE,
           "script project string is invalid or unreasonably large");
    return false;
  }
  if (!ReserveProjectData(node,
                          2 + static_cast<int>(std::strlen(value))))
    return false;
  mp_WriteStr(projectTable, node, value);
  return true;
}

bool RecoveredLegacyScriptHost::ProjectNodeSetLink(int node, int left,
                                                    int right) {
  if (!m_projectDataOpen || node != m_openProjectNode ||
      !ProjectNodeValid(node) || !ProjectNodeValid(left, true) ||
      !ProjectNodeValid(right, true)) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_NODE_FAILURE,
           "script requested invalid project node links");
    return false;
  }
  mp_SetLink(projectTable, node, left, right);
  return true;
}

int RecoveredLegacyScriptHost::ProjectNodeNull() const {
  return mp_NodeNULL();
}

KR_ObjectID RecoveredLegacyScriptHost::NewProject(const char* name, int node,
                                                   bool permanent) {
  if (!m_projectTableCreated || m_projectDataOpen || name == nullptr ||
      name[0] == 0 || std::strlen(name) > MAX_SYMBOLIC_LENGHT ||
      !ProjectNodeValid(node) || m_projectCount >= m_projectCapacity ||
      m_arena->getContext()->isExist(name)) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_CREATION_FAILURE,
           "script requested an invalid or duplicate project");
    return KR_ObjectID::NUL();
  }
  KR_ObjectID object = projectTable.newProject(name, node, permanent ? 1 : 0);
  if (object.isNUL()) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_CREATION_FAILURE,
           "project creation failed");
    return object;
  }
  ++m_projectCount;
  return object;
}

void RecoveredLegacyScriptHost::DeferMissionHowitzer(
    int classTable, const char* attributeName, const char* holderName,
    double startTime, const char* objectName) {
  (void)classTable;
  (void)startTime;
  if (attributeName == nullptr || attributeName[0] == 0 ||
      holderName == nullptr || holderName[0] == 0 || objectName == nullptr ||
      objectName[0] == 0) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_CREATION_FAILURE,
           "mission Howitzer deferral received invalid authored data");
    return;
  }
  ++m_deferredMissionHowitzerCount;
}

void RecoveredLegacyScriptHost::DeferMissionDestroyable(
    const char* attributeName, const char* scriptName,
    const char* objectName) {
  if (attributeName == nullptr || attributeName[0] == 0 ||
      scriptName == nullptr || scriptName[0] == 0 || objectName == nullptr ||
      objectName[0] == 0) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_CREATION_FAILURE,
           "mission Destroyable deferral received invalid authored data");
    return;
  }
  ++m_deferredMissionDestroyableCount;
}

bool RecoveredLegacyScriptHost::ProjectTableCreated() const {
  return m_projectTableCreated;
}

int RecoveredLegacyScriptHost::ProjectNodeCount() const {
  return m_projectNodeCount;
}

int RecoveredLegacyScriptHost::ProjectCount() const { return m_projectCount; }

int RecoveredLegacyScriptHost::ProjectDataBytes() const {
  return m_projectDataBytes;
}

int RecoveredLegacyScriptHost::DeferredMissionHowitzerCount() const {
  return m_deferredMissionHowitzerCount;
}

int RecoveredLegacyScriptHost::DeferredMissionDestroyableCount() const {
  return m_deferredMissionDestroyableCount;
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
