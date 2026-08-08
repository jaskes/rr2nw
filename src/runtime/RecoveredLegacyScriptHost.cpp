#include "RecoveredLegacyScriptHost.h"

#include "HowitzerActiveWorldState.h"
#include "HowitzerSubjectState.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "enum/spaceenum.h"
#include "graph.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/attrmsg.h"
#include "message/artfmsg.h"
#include "message/bimsg.h"
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
#include "i/carrier.i"
#include "obase/route/route.h"
#include "i/unit.i"
#include "i/commander.i"
#include "storage/h/subject.h"

namespace {

int g_routeCapacityFloor = 0;
int g_peopleCapacityFloor = 0;

bool CollectObjectID(KR_ObjectID object, void* parameter) {
  std::vector<KR_ObjectID>* objects =
      static_cast<std::vector<KR_ObjectID>*>(parameter);
  if (objects == nullptr) return false;
  objects->push_back(object);
  return true;
}

bool ContainsObject(const std::vector<KR_ObjectID>& objects,
                    const KR_ObjectID& object) {
  for (const KR_ObjectID& candidate : objects)
    if (candidate == object) return true;
  return false;
}

bool PrepareNewArtefact(SimulationContext* context,
                        const KR_ObjectID& object) {
  KR_ObjectID mutableObject = object;
  if (context == nullptr || mutableObject.isNUL()) return false;
  IArtefact* artefact = static_cast<IArtefact*>(
      context->queryInterface(object, IArtefactIID));
  IUnit* unit = static_cast<IUnit*>(
      context->queryInterface(object, IUnitIID));
  if (artefact == nullptr || unit == nullptr) return false;

  // The archival constructor leaves the raw-dumped ArtefactData block
  // untouched. A script publishes the attribute and authored transform next,
  // but a neutral world artefact intentionally never receives a commander.
  // Establish that deterministic pre-event state through public interfaces,
  // keeping the OEM archival translation unit byte-for-byte unchanged.
  unit->setCommander(KR_ObjectID::NUL());
  artefact->artefactMove(CFVector3(0.0, 0.0, 0.0));
  CFMatrix3x4 orientation;
  orientation.LoadIdentity();
  artefact->moveTo(orientation);
  return true;
}

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

void ScriptWriteVector(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->WriteVector(SC_PARI(3), SC_PARF(2), SC_PARF(1), SC_PARF(0));
  }
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

void ScriptIssueEvent(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) {
    host->IssueEvent(SC_PARI(4), SC_PARI(3), SC_PARF(2),
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

void ScriptGetTime(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARF(0) = host == nullptr ? 0.0 : host->CurrentTime();
}

void ScriptUpdateAttributes(TProcessContext* pc, void* userData) {
  (void)pc;
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->UpdateAttributes();
}

void ScriptAttachObject(TProcessContext* pc, void* userData) {
  (void)pc;
  RecoveredLegacyScriptHost* host = Host(userData);
  // The declaration is part of every retail DEFINES.SCI include, so the
  // legacy linker requires a symbol even when a mission never calls it.
  // Fail closed if a mission does call it until scene-reference attachment
  // participates in the same object transaction as NewObject/LoadRoute.
  if (host != nullptr) host->Unsupported("s_AttachObject");
}

void ScriptSetDamage(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARI(2) = host == nullptr ? 0
                               : host->SetDamage(SC_PARS(1), SC_PARF(0));
}

void ScriptSetLevel(TProcessContext* pc, void* userData) {
  (void)pc;
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->Unsupported("s_SetLevel");
}

void ScriptSetViewPoint(TProcessContext* pc, void* userData) {
  (void)pc;
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->Unsupported("s_SetViewPoint");
}

void ScriptGetBriefingTime(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  SC_PARF(1) = 0.0;
  if (host != nullptr) host->Unsupported("s_GetBriefingTime");
}

void ScriptDeleteHowitzer(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->DeleteHowitzer(SC_PARS(0));
}

void ScriptRestartLevel(TProcessContext* pc, void* userData) {
  RecoveredLegacyScriptHost* host = Host(userData);
  if (host != nullptr) host->RequestRestartLevel(SC_PARI(0));
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

void ConstGroupAddMember(TStackCell* cell) {
  cell->i = GROUP_ADD_MEMBER;
}

void ConstBuildingBeginMove(TStackCell* cell) {
  cell->i = bi_EV_BEGIN_MOVE;
}

void ConstCommanderSetRoute(TStackCell* cell) {
  cell->i = com_EV_SET_ROUTE;
}

void ConstArtefactMoveTo(TStackCell* cell) {
  cell->i = ARTEFACT_MOVETO;
}

void ConstUnsupported(TStackCell* cell) { cell->i = -1; }

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
    {"s_WriteVector", ScriptWriteVector, nullptr},
    {"s_WriteStr", ScriptWriteString, nullptr},
    {"s_WriteObjectID", ScriptWriteObjectID, nullptr},
    {"s_IssueEvent", ScriptIssueEvent, nullptr},
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
    {"s_GetTime", ScriptGetTime, nullptr},
    {"s_UpdateAttributes", ScriptUpdateAttributes, nullptr},
    {"s_AttachObject", ScriptAttachObject, nullptr},
    {"s_SetDamage", ScriptSetDamage, nullptr},
    {"s_SetLevel", ScriptSetLevel, nullptr},
    {"s_SetViewPoint", ScriptSetViewPoint, nullptr},
    {"s_GetBriefingTime", ScriptGetBriefingTime, nullptr},
    {"s_DeleteHowitzer", ScriptDeleteHowitzer, nullptr},
    {"s_RestartLevel", ScriptRestartLevel, nullptr},
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
    {"s_GROUP_ADD_MEMBER", ConstGroupAddMember, 0},
    {"s_COMMANDER_ADD_MEMBER_N", ConstCommanderAddMemberByName, 0},
    {"bi_EV_BEGIN_MOVE", ConstBuildingBeginMove, 0},
    {"com_EV_SET_ROUTE", ConstCommanderSetRoute, 0},
    {"ARTEFACT_MOVETO", ConstArtefactMoveTo, 0},
    {"DESTROYABLE_IMMORTAL_STATE", ConstUnsupported, 0},
    {"EV_VEHICLE_PRINTMESSAGE", ConstUnsupported, 0},
    {"EV_VEHICLE_SOUNDEVENT", ConstUnsupported, 0},
    {"EV_VEHICLE_PANELEVENT", ConstUnsupported, 0},
    {"train_EV_SETPAUSE", ConstUnsupported, 0},
    {"train_EV_ATTACH", ConstUnsupported, 0},
    {"train_EV_ATTACH_SMOKER", ConstUnsupported, 0},
    {"train_EV_ADDCANNON", ConstUnsupported, 0},
    {"t_EV_SET_ATTR_POS", ConstUnitSetAttributePosition, 0},
    {"rc_SET_EJECT", ConstRecruitCenterSetEject, 0},
    {"rc_SET_VIDEO", ConstRecruitCenterSetVideo, 0},
    {"rc_SET_DEFTAXI", ConstRecruitCenterSetDefaultTaxi, 0},
    {"rc_SET_DICTIONARY", ConstRecruitCenterSetDictionary, 0},
    {nullptr, nullptr, 0}};

}  // namespace

void RecoveredLegacyScriptHost_SetRouteCapacityFloor(int capacity) {
  g_routeCapacityFloor = capacity > 0 ? capacity : 0;
}

void RecoveredLegacyScriptHost_SetPeopleCapacityFloor(int capacity) {
  g_peopleCapacityFloor = capacity > 0 ? capacity : 0;
}

RecoveredLegacyScriptHost::RecoveredLegacyScriptHost(ct_Arena* arena)
    : m_arena(arena), m_issues(0), m_lastError{},
      m_projectTableCreated(false), m_projectDataOpen(false),
      m_projectCapacity(0), m_projectNodeCapacity(0),
      m_projectHeapCapacity(0), m_projectNodeCount(0), m_projectCount(0),
      m_projectDataBytes(0), m_openProjectNode(mp_NodeNULL()),
      m_deferredMissionHowitzerCount(0),
      m_deferredMissionDestroyableCount(0),
      m_discardNextMissingHolderHowitzer(false),
      m_restartLevelRequested(false), m_restartLevelIndex(-1),
      m_objectTransactionActive(false),
      m_transactionCreatedObjects(), m_transactionDestroyedCreatedObjectNames(),
      m_transactionExistingRoutes(), m_transactionExistingCorpses(),
      m_transactionPinnedRoutes(), m_transactionReclaimedRoutes(),
      m_transactionReplacedHowitzers() {
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
  m_discardNextMissingHolderHowitzer = false;
  m_restartLevelRequested = false;
  m_restartLevelIndex = -1;
  if (!m_objectTransactionActive) {
    m_transactionCreatedObjects.clear();
    m_transactionDestroyedCreatedObjectNames.clear();
    m_transactionExistingRoutes.clear();
    m_transactionExistingCorpses.clear();
    m_transactionPinnedRoutes.clear();
    m_transactionReclaimedRoutes.clear();
    m_transactionReplacedHowitzers.clear();
  }
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

void RecoveredLegacyScriptHost::WriteVector(int eventIndex, double x,
                                             double y, double z) {
  ScriptEvent* event = Event(eventIndex, "write event vector");
  if (event != nullptr && std::isfinite(x) && std::isfinite(y) &&
      std::isfinite(z)) {
    event->data.putDouble(x).putDouble(y).putDouble(z);
  }
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

void RecoveredLegacyScriptHost::IssueEvent(
    int eventIndex, int label, double timeStamp,
    const KR_ObjectID& destination) {
  ScriptEvent* event = Event(eventIndex, "issue scheduled event");
  if (event == nullptr || !ArenaReady("issue scheduled event")) return;
  SimulationContext* context = m_arena->getContext();
  // Match the retail host contract: scheduled events may legitimately target
  // an object that is not published yet, but the kernel requires a usable
  // cache slot and a timestamp past its initialization sentinel. Retail
  // mission helpers intentionally pass zero for an immediate start.  Resolve
  // that marker against the current simulation clock: pinning it to absolute
  // 0.1 leaves newly admitted Howitzers pending forever once a long campaign
  // has advanced beyond the first legacy boundary.  Authored positive values
  // remain absolute timestamps.
  // During initial graph construction the kernel has not reached its first
  // valid scheduler boundary. Preserve the original 0.1 enqueue there. In a
  // running session, enqueue zero-time commands at the current moment so
  // poll() consumes them in the same scheduler boundary. Mission admission
  // closes pending Howitzer STARTs only after the complete script graph has
  // returned; event submission itself never re-enters a partially authored
  // object graph.
  if (timeStamp == 0.0)
    timeStamp = Session::m_moment < 0.1 ? 0.1 : Session::m_moment;
  if (!std::isfinite(timeStamp) || timeStamp < 0.1 ||
      destination.getCachePos() < 0 || context->eventFreeCount() <= 0) {
    event->inUse = false;
    char message[224] = {};
    std::snprintf(message, sizeof(message),
                  "script scheduled event is invalid label=%d time=%.17g "
                  "target=%ld/%ld free=%d",
                  label, timeStamp, static_cast<long>(destination.id),
                  static_cast<long>(destination.getCachePos()),
                  context->eventFreeCount());
    Report(RECOVERED_LEGACY_SCRIPT_HOST_INVALID_EVENT_DESTINATION,
           message);
    return;
  }
  event->label = label;
  event->timeStamp = timeStamp;
  event->destination = destination;
  event->source = m_arena->getObjectID();
  event->inUse = false;
  context->addEvent(*event);
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
  // The original s_SendEventNow deliberately delegated destination
  // validation to SimulationContext. Missing or stale targets are a safe
  // no-op there and are used by several retail attribute fragments.
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
  int effectiveCapacity = capacity;
  if (std::strcmp(name, "Route") == 0 &&
      g_routeCapacityFloor > effectiveCapacity)
    effectiveCapacity = g_routeCapacityFloor;
  else if (std::strcmp(name, "People") == 0 &&
           g_peopleCapacityFloor > effectiveCapacity)
    effectiveCapacity = g_peopleCapacityFloor;
  const int table = m_arena->addClassTable(name, effectiveCapacity);
  if (table == ct_NULLID) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "script class table creation failed for %.100s/%d",
                  name, effectiveCapacity);
    Report(RECOVERED_LEGACY_SCRIPT_HOST_CLASS_TABLE_FAILURE, message);
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
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "script object creation failed for %.120s in table %d",
                  name, classTable);
    Report(RECOVERED_LEGACY_SCRIPT_HOST_OBJECT_CREATION_FAILURE, message);
  }
  const bool discardMissingHolderHowitzer =
      m_discardNextMissingHolderHowitzer &&
      classTable == m_arena->searchSeanceClassTable("Howitzer");
  if (discardMissingHolderHowitzer) {
    m_discardNextMissingHolderHowitzer = false;
    // Installed ProjectG14 asks the archival CreateHowitzerName helper to
    // place one owner into absent HwzAct77.  The original delete call is a
    // silent no-op and the owner cannot acquire a valid placement.  Return its
    // now-stale identity so the remaining helper calls retain their original
    // harmless no-op behavior, but do not publish an unserializable owner.
    if (!object.isNUL()) m_arena->getContext()->removeObject(object);
    return object;
  }
  if (!object.isNUL() &&
      classTable == m_arena->searchSeanceClassTable("Howitzer") &&
      !HowitzerSubjectState_PrepareNewObject(m_arena->getContext(), object)) {
    m_arena->getContext()->removeObject(object);
    Report(RECOVERED_LEGACY_SCRIPT_HOST_HOWITZER_HOLDER_FAILURE,
           "new Howitzer could not enter a safe pending state");
    return KR_ObjectID::NUL();
  }
  if (!object.isNUL() &&
      classTable == m_arena->searchSeanceClassTable("Artefact") &&
      !PrepareNewArtefact(m_arena->getContext(), object)) {
    m_arena->getContext()->removeObject(object);
    Report(RECOVERED_LEGACY_SCRIPT_HOST_ARTEFACT_PENDING_FAILURE,
           "new Artefact could not enter a deterministic pending state");
    return KR_ObjectID::NUL();
  }
  if (!object.isNUL() && m_objectTransactionActive)
    m_transactionCreatedObjects.push_back(object);
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
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "script named-class object creation failed for %.100s in "
                  "%.100s", name, className);
    Report(RECOVERED_LEGACY_SCRIPT_HOST_OBJECT_CREATION_FAILURE, message);
  }
  if (!object.isNUL() && std::strcmp(className, "Howitzer") == 0 &&
      !HowitzerSubjectState_PrepareNewObject(m_arena->getContext(), object)) {
    m_arena->getContext()->removeObject(object);
    Report(RECOVERED_LEGACY_SCRIPT_HOST_HOWITZER_HOLDER_FAILURE,
           "new named Howitzer could not enter a safe pending state");
    return KR_ObjectID::NUL();
  }
  if (!object.isNUL() && std::strcmp(className, "Artefact") == 0 &&
      !PrepareNewArtefact(m_arena->getContext(), object)) {
    m_arena->getContext()->removeObject(object);
    Report(RECOVERED_LEGACY_SCRIPT_HOST_ARTEFACT_PENDING_FAILURE,
           "new named Artefact could not enter a deterministic pending state");
    return KR_ObjectID::NUL();
  }
  if (!object.isNUL() && m_objectTransactionActive)
    m_transactionCreatedObjects.push_back(object);
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
    } else if (m_objectTransactionActive && existing->GetRefCount() > 0 &&
               !ContainsObject(m_transactionPinnedRoutes, existingID)) {
      // CreateManName removes an already named People after its wrapper has
      // loaded the replacement route.  Retail S04 does exactly this by
      // creating a.unit.ms04.ap00 twice.  Keep the reused route alive across
      // that removal; the replacement People acquires its own reference when
      // the script sends pe_EVCMD_START.
      existing->AddRef();
      m_transactionPinnedRoutes.push_back(existingID);
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

double RecoveredLegacyScriptHost::CurrentTime() const {
  return Session::m_moment;
}

bool RecoveredLegacyScriptHost::UpdateAttributes() {
  if (!ArenaReady("update attributes")) return false;
  m_arena->updateAttributes(Session::m_moment);
  return true;
}

int RecoveredLegacyScriptHost::SetDamage(const char* objectName,
                                          double damage) {
  if (!ArenaReady("set unit damage") || objectName == nullptr ||
      !std::isfinite(damage))
    return 0;
  const KR_ObjectID object = SearchObject(objectName);
  IUnit* unit = static_cast<IUnit*>(
      m_arena->getContext()->queryInterface(object, IUnitIID));
  if (unit == nullptr) return 0;
  const bool transactionCreated = m_objectTransactionActive &&
      ContainsObject(m_transactionCreatedObjects, object);
  unit->setDamage(damage, CFVector3(0.0, 0.0, 0.0), Session::m_moment,
                  m_arena->getObjectID());
  if (transactionCreated && !m_arena->getContext()->isExist(object)) {
    bool recorded = false;
    for (const std::string& name : m_transactionDestroyedCreatedObjectNames)
      if (name == objectName) {
        recorded = true;
        break;
      }
    if (!recorded)
      m_transactionDestroyedCreatedObjectNames.push_back(objectName);
  }
  return 1;
}

bool RecoveredLegacyScriptHost::DeleteHowitzer(const char* holderName) {
  if (!ArenaReady("delete Howitzer holder occupant") ||
      holderName == nullptr) {
    return false;
  }
  SimulationContext* context = m_arena->getContext();
  if (HowitzerSubjectState_FindHolder(holderName) < 0) {
    // Archival s_DeleteHowitzer delegates to AttachToHowitzerHolder, whose
    // missing-name branch returns -1 without reporting an error.  Remember the
    // immediately following Howitzer allocation so it can be discarded at
    // the same authored boundary instead of poisoning stable save capture.
    m_discardNextMissingHolderHowitzer = true;
    return true;
  }
  KR_ObjectID occupant =
      HowitzerSubjectState_HolderOccupant(holderName);
  const bool replacingExisting = m_objectTransactionActive &&
      !occupant.isNUL() && context->isExist(occupant) &&
      !ContainsObject(m_transactionCreatedObjects, occupant);
  ReplacedHowitzer replacement;
  if (replacingExisting) {
    replacement.holder = holderName;
    if (!HowitzerActiveWorldState_CaptureHolder(
            context, holderName, &replacement.stable)) {
      char message[256] = {};
      std::snprintf(message, sizeof(message),
                    "Howitzer holder %.80s capture failed: %.140s",
                    holderName, HowitzerActiveWorldState_LastFailure());
      Report(RECOVERED_LEGACY_SCRIPT_HOST_HOWITZER_HOLDER_FAILURE, message);
      return false;
    }
  }
  if (!HowitzerSubjectState_DeleteHolderOccupant(
          context, holderName, m_transactionCreatedObjects,
          m_objectTransactionActive && !replacingExisting)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "Howitzer holder %.80s deletion failed: %.140s",
                  holderName, HowitzerSubjectState_LastError());
    Report(RECOVERED_LEGACY_SCRIPT_HOST_HOWITZER_HOLDER_FAILURE, message);
    return false;
  }
  if (replacingExisting)
    m_transactionReplacedHowitzers.push_back(replacement);
  return true;
}

void RecoveredLegacyScriptHost::Unsupported(const char* operation) {
  char message[256] = {};
  std::snprintf(message, sizeof(message),
                "retail mission script called unsupported operation %s",
                operation == nullptr ? "<unknown>" : operation);
  Report(RECOVERED_LEGACY_SCRIPT_HOST_UNSUPPORTED_OPERATION, message);
}

bool RecoveredLegacyScriptHost::RequestRestartLevel(int levelIndex) {
  // Retail game.cfg is the authority for translating this authored ordinal
  // into a Level identity. The process coordinator performs that lookup;
  // the script host only admits one bounded request and never tears down the
  // active Context from inside the VM callback.
  if (levelIndex < 0 || levelIndex > 255 ||
      (m_restartLevelRequested && m_restartLevelIndex != levelIndex)) {
    Report(RECOVERED_LEGACY_SCRIPT_HOST_LEVEL_TRANSITION_FAILURE,
           "script Level transition request is invalid or conflicting");
    return false;
  }
  m_restartLevelRequested = true;
  m_restartLevelIndex = levelIndex;
  return true;
}

bool RecoveredLegacyScriptHost::RestartLevelRequested() const {
  return m_restartLevelRequested;
}

int RecoveredLegacyScriptHost::RestartLevelIndex() const {
  return m_restartLevelRequested ? m_restartLevelIndex : -1;
}

void RecoveredLegacyScriptHost::BeginObjectTransaction() {
  m_objectTransactionActive = true;
  m_discardNextMissingHolderHowitzer = false;
  m_transactionCreatedObjects.clear();
  m_transactionDestroyedCreatedObjectNames.clear();
  m_transactionExistingRoutes.clear();
  m_transactionExistingCorpses.clear();
  m_transactionPinnedRoutes.clear();
  m_transactionReclaimedRoutes.clear();
  m_transactionReplacedHowitzers.clear();
  if (ArenaReady("capture Route transaction baseline") &&
      m_arena->searchSeanceClassTable("Route") != ct_NULLID)
    m_arena->userFind("Route", CollectObjectID,
                      &m_transactionExistingRoutes);
  if (m_arena->searchSeanceClassTable("Corpse") != ct_NULLID)
    m_arena->userFind("Corpse", CollectObjectID,
                      &m_transactionExistingCorpses);
}

int RecoveredLegacyScriptHost::ReclaimUnreferencedRoutes(
    const KR_ObjectID* preserved, int preservedCount) {
  if (!m_objectTransactionActive || !ArenaReady("reclaim Route capacity") ||
      preservedCount < 0 || (preservedCount > 0 && preserved == nullptr) ||
      m_arena->searchSeanceClassTable("Route") == ct_NULLID)
    return -1;

  SimulationContext* context = m_arena->getContext();
  std::vector<KR_ObjectID> routes;
  m_arena->userFind("Route", CollectObjectID, &routes);
  int reclaimed = 0;
  for (const KR_ObjectID& routeID : routes) {
    bool keep = false;
    for (int index = 0; index < preservedCount; ++index)
      if (preserved[index] == routeID) {
        keep = true;
        break;
      }
    if (keep) continue;

    IRouteObject* route = static_cast<IRouteObject*>(
        context->queryInterface(routeID, IRouteObjectIID));
    const char* name = context->searchObject(routeID);
    if (route == nullptr || name == nullptr || route->GetRefCount() != 0 ||
        route->GetNodeCnt() < 2)
      continue;

    ReclaimedRoute record;
    record.name = name;
    record.coordinates.reserve(
        static_cast<std::size_t>(route->GetNodeCnt()) * 3u);
    for (int node = 0; node < route->GetNodeCnt(); ++node) {
      const CFVector3 position = route->GetNode(node);
      record.coordinates.push_back(position.x);
      record.coordinates.push_back(position.y);
      record.coordinates.push_back(position.z);
    }
    m_transactionReclaimedRoutes.push_back(record);
    context->removeObject(routeID);
    if (context->isExist(routeID)) return -1;
    ++reclaimed;
  }
  return reclaimed;
}

bool RecoveredLegacyScriptHost::RollbackObjectTransaction() {
  if (!m_objectTransactionActive) return true;
  if (!ArenaReady("rollback object transaction")) return false;
  SimulationContext* context = m_arena->getContext();
  for (const KR_ObjectID& routeID : m_transactionPinnedRoutes) {
    if (!context->isExist(routeID)) continue;
    IRouteObject* route = static_cast<IRouteObject*>(
        context->queryInterface(routeID, IRouteObjectIID));
    if (route != nullptr) route->DelRef();
  }
  m_transactionPinnedRoutes.clear();
  const int eventCount = context->eventCount();
  std::vector<KR_Event> events(
      eventCount > 0 ? static_cast<std::size_t>(eventCount) : 0u);
  if (eventCount > 0 &&
      context->copyAllEvents(events.data(), eventCount) != eventCount)
    return false;
  for (const KR_ObjectID& object : m_transactionCreatedObjects) {
    for (const KR_Event& event : events) {
      if (event.source == object)
        while (context->removeEvent(event.label, object) == 1) {}
      if (event.destination == object)
        context->removeEventsTo(event.label, object);
    }
  }
  for (std::vector<KR_ObjectID>::reverse_iterator object =
           m_transactionCreatedObjects.rbegin();
       object != m_transactionCreatedObjects.rend(); ++object) {
    if (context->isExist(*object)) context->removeObject(*object);
  }

  // A retail mission can create a Taxi and destroy it immediately with
  // s_SetDamage, which creates a Corpse outside the script host. Keep that
  // native side effect inside the same admission transaction.
  std::vector<KR_ObjectID> currentCorpses;
  if (m_arena->searchSeanceClassTable("Corpse") != ct_NULLID)
    m_arena->userFind("Corpse", CollectObjectID, &currentCorpses);
  for (const KR_ObjectID& corpse : currentCorpses)
    if (!ContainsObject(m_transactionExistingCorpses, corpse) &&
        context->isExist(corpse))
      context->removeObject(corpse);

  // Commander::com_EV_SET_ROUTE allocates Route objects natively, outside
  // s_NewObject/s_LoadRoute. Remove every Route that was not present at the
  // transaction boundary so a failed script cannot consume the fixed retail
  // table a little further on every retry.
  std::vector<KR_ObjectID> currentRoutes;
  if (m_arena->searchSeanceClassTable("Route") != ct_NULLID)
    m_arena->userFind("Route", CollectObjectID, &currentRoutes);
  for (const KR_ObjectID& route : currentRoutes)
    if (!ContainsObject(m_transactionExistingRoutes, route) &&
        context->isExist(route))
      context->removeObject(route);

  // Recreate only zero-reference routes reclaimed by this transaction. Their
  // old IDs had no live owner; symbolic identity and exact geometry are the
  // observable rollback contract.
  for (const ReclaimedRoute& record : m_transactionReclaimedRoutes) {
    if (context->isExist(record.name.c_str())) continue;
    KR_ObjectID routeID = m_arena->newObject("Route", record.name.c_str());
    IRouteObject* route = routeID.isNUL()
        ? nullptr
        : static_cast<IRouteObject*>(
              context->queryInterface(routeID, IRouteObjectIID));
    if (route == nullptr || record.coordinates.empty() ||
        !route->RestoreGeometry(&record.coordinates[0],
                                static_cast<int>(record.coordinates.size())))
      return false;
  }
  bool howitzersRestored = true;
  for (std::vector<ReplacedHowitzer>::reverse_iterator replacement =
           m_transactionReplacedHowitzers.rbegin();
       replacement != m_transactionReplacedHowitzers.rend(); ++replacement)
    if (!HowitzerActiveWorldState_RestoreHolder(
            context, replacement->stable))
      howitzersRestored = false;
  m_transactionCreatedObjects.clear();
  m_transactionDestroyedCreatedObjectNames.clear();
  m_transactionExistingRoutes.clear();
  m_transactionExistingCorpses.clear();
  m_transactionPinnedRoutes.clear();
  m_transactionReclaimedRoutes.clear();
  m_transactionReplacedHowitzers.clear();
  m_discardNextMissingHolderHowitzer = false;
  m_objectTransactionActive = false;
  return howitzersRestored;
}

void RecoveredLegacyScriptHost::CommitObjectTransaction() {
  if (m_arena != nullptr) {
    SimulationContext* context = m_arena->getContext();
    for (const KR_ObjectID& routeID : m_transactionPinnedRoutes) {
      if (!context->isExist(routeID)) continue;
      IRouteObject* route = static_cast<IRouteObject*>(
          context->queryInterface(routeID, IRouteObjectIID));
      if (route != nullptr) route->DelRef();
    }
  }
  m_transactionCreatedObjects.clear();
  m_transactionDestroyedCreatedObjectNames.clear();
  m_transactionExistingRoutes.clear();
  m_transactionExistingCorpses.clear();
  m_transactionPinnedRoutes.clear();
  m_transactionReclaimedRoutes.clear();
  m_transactionReplacedHowitzers.clear();
  m_discardNextMissingHolderHowitzer = false;
  m_objectTransactionActive = false;
}

int RecoveredLegacyScriptHost::TransactionCreatedObjectCount() const {
  return static_cast<int>(m_transactionCreatedObjects.size());
}

int RecoveredLegacyScriptHost::TransactionReplacedHowitzerCount() const {
  return static_cast<int>(m_transactionReplacedHowitzers.size());
}

bool RecoveredLegacyScriptHost::TransactionDestroyedCreatedObject(
    const char* name) const {
  if (name == nullptr || !m_objectTransactionActive) return false;
  for (const std::string& candidate :
       m_transactionDestroyedCreatedObjectNames)
    if (candidate == name) return true;
  return false;
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
