#include "PortalActiveWorldState.h"

#include "portal.h"
#include "vehicle.h"
#include "console.h"
#include "obase/fountain/FountainClassTableState.h"
#include "kernel/h/session.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "kernel/h/context.h"
#include "i/carrier.i"
#include "message/artfmsg.h"
#include "message/bimsg.h"
#include "message/unitmsg.h"

void PortalCallback(CViewObjectBaseSet *baseSet, CViewObjectBase *base,
                    CViewObjectRef *reference);

namespace {

const std::uint32_t kPortalMagic = 0x31545250u;  // PRT1
const std::uint32_t kPortalVersion = 1u;
const std::size_t kMaximumPortals = 16u;
const std::size_t kMaximumNameBytes = MAX_SYMBOLIC_LENGHT - 1;
const char kPortalArabesk[] = "Portal.Arabesk";
// May 1999 stores these strings as CP866 bytes. Keep the byte contract exact:
// the retail font consumes the OEM text directly, and the original restored
// message deliberately contains a Latin 'C'.
const char kPortalRestored[] =
    "\x8f\x8e\x90\x92\x80\x8b \x82\x8e\x91\x43\x92\x80\x8d\x8e\x82\x8b\x85\x8d";
const char kOneArtefactRemaining[] =
    "\x8e\x91\x92\x80\x8b\x91\x9f 1 \x80\x90\x92\x85\x94\x80\x8a\x92";
const char kFewArtefactsRemaining[] =
    "\x8e\x91\x92\x80\x8b\x8e\x91\x9c %i \x80\x90\x92\x85\x94\x80\x8a\x92\x41";
const char kManyArtefactsRemaining[] =
    "\x8e\x91\x92\x80\x8b\x8e\x91\x9c %i \x80\x90\x92\x85\x94\x80\x8a\x92\x8e\x82";
std::string g_lastFailure;

struct LevelBinding {
  CViewObjectRef *reference;
  Portal *portal;
  KR_ObjectID object;

  LevelBinding() : reference(NULL), portal(NULL) {}
};

enum PortalStatusKind {
  kPortalStatusNone = 0,
  kPortalStatusSingular = 1,
  kPortalStatusFew = 2,
  kPortalStatusMany = 3,
  kPortalStatusRestored = 4
};

struct PortalPresentationEvidence {
  int statusKind;
  int remaining;
  int messagePublished;
  int arabeskPresent;
  int arabeskRemoved;
  char message[CON_MAX_MESSAGE_LEN];

  PortalPresentationEvidence()
      : statusKind(kPortalStatusNone), remaining(0), messagePublished(0),
        arabeskPresent(0), arabeskRemoved(0), message{} {}
};

SimulationContext *g_levelContext = NULL;
std::vector<LevelBinding> g_levelBindings;
bool g_transitionRequested = false;

bool Fail(const std::string &message) {
  g_lastFailure = message;
  return false;
}

bool IsNul(const KR_ObjectID &object) {
  KR_ObjectID copy = object;
  return copy.isNUL() != 0;
}

bool FiniteVector(const CFVector3 &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

bool SameVector(const CFVector3 &left, const CFVector3 &right) {
  return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool FormatPortalStatus(int remaining, char *message, std::size_t capacity,
                        int *statusKind) {
  if (message == NULL || capacity == 0 || statusKind == NULL || remaining < 0)
    return false;
  message[0] = 0;
  if (remaining == 0) {
    *statusKind = kPortalStatusRestored;
    return std::snprintf(message, capacity, "%s", kPortalRestored) > 0;
  }
  if (remaining == 1) {
    *statusKind = kPortalStatusSingular;
    return std::snprintf(message, capacity, "%s", kOneArtefactRemaining) > 0;
  }
  if (remaining <= 4) {
    *statusKind = kPortalStatusFew;
    return std::snprintf(message, capacity, kFewArtefactsRemaining,
                         remaining) > 0;
  }
  *statusKind = kPortalStatusMany;
  return std::snprintf(message, capacity, kManyArtefactsRemaining,
                       remaining) > 0;
}

bool PresentPortalStatus(SimulationContext *context, Portal *portal,
                         PortalPresentationEvidence *evidence) {
  PortalPresentationEvidence local;
  PortalPresentationEvidence *result = evidence == NULL ? &local : evidence;
  if (context == NULL || portal == NULL || portal->getContext() != context ||
      portal->portalGetSlotCnt() <= 0 ||
      portal->portalGetOccupiedSlot() < 0 ||
      portal->portalGetOccupiedSlot() > portal->portalGetSlotCnt())
    return Fail("Portal presentation has no valid live owner");

  result->remaining =
      portal->portalGetSlotCnt() - portal->portalGetOccupiedSlot();
  if (!FormatPortalStatus(result->remaining, result->message,
                          sizeof(result->message), &result->statusKind))
    return Fail("Portal status text could not be formatted");

  if (result->remaining == 0) {
    result->arabeskPresent = context->isExist(kPortalArabesk) ? 1 : 0;
    if (result->arabeskPresent) {
      const KR_ObjectID arabesk = context->searchObject(kPortalArabesk);
      context->removeObject(arabesk);
      result->arabeskRemoved = context->isExist(kPortalArabesk) ? 0 : 1;
      if (!result->arabeskRemoved)
        return Fail("restored Portal did not remove Portal.Arabesk");
    }
  }

  if (g_GameConsole.MessagesReady()) {
    g_GameConsole.PrintUrgent(result->message,
                              result->remaining == 0 ? 10.0 : 5.0,
                              GameConsole::CENTER);
    result->messagePublished = 1;
  }
  return true;
}

struct StablePortalRecord {
  std::string name;
  CFVector3 artefactPoint;
  CFVector3 triggerPoint;
  int slots;
  int occupied;

  StablePortalRecord()
      : artefactPoint(0.0, 0.0, 0.0), triggerPoint(0.0, 0.0, 0.0),
        slots(0), occupied(0) {}
};

struct RosterEntry {
  std::string name;
  Portal *portal;
};

struct Roster {
  SimulationContext *context;
  std::vector<RosterEntry> entries;
  bool valid;
};

bool ReconcilePortalArabesk(SimulationContext *context,
                            const Roster &roster) {
  bool full = false;
  for (std::size_t index = 0; index < roster.entries.size(); ++index) {
    Portal *portal = roster.entries[index].portal;
    if (portal->portalGetOccupiedSlot() == portal->portalGetSlotCnt()) {
      full = true;
      break;
    }
  }
  if (full) {
    if (context->isExist(kPortalArabesk))
      context->removeObject(context->searchObject(kPortalArabesk));
    return !context->isExist(kPortalArabesk) ||
           Fail("full restored Portal retained Portal.Arabesk");
  }
  if (!context->isExist("Fount.Attr.Arab")) return true;
  return FountainClassTable_EnsurePortalArabesk(context, Session::m_moment) ||
         Fail("partial restored Portal did not recreate Portal.Arabesk");
}

Portal *ResolvePortal(SimulationContext *context,
                      const KR_ObjectID &object) {
  if (context == NULL || IsNul(object) || !context->isExist(object))
    return NULL;
  IDynamicObject *dynamic = static_cast<IDynamicObject *>(
      context->queryInterface(object, IDynamicObjectIID));
  return dynamic == NULL ? NULL : dynamic_cast<Portal *>(dynamic);
}

bool PortalLess(const RosterEntry &left, const RosterEntry &right) {
  if (left.name != right.name) return left.name < right.name;
  const CFVector3 leftArtefact = left.portal->portalGetCoord();
  const CFVector3 rightArtefact = right.portal->portalGetCoord();
  if (leftArtefact.x != rightArtefact.x)
    return leftArtefact.x < rightArtefact.x;
  if (leftArtefact.y != rightArtefact.y)
    return leftArtefact.y < rightArtefact.y;
  if (leftArtefact.z != rightArtefact.z)
    return leftArtefact.z < rightArtefact.z;
  const CFVector3 leftTrigger = left.portal->getPos();
  const CFVector3 rightTrigger = right.portal->getPos();
  if (leftTrigger.x != rightTrigger.x) return leftTrigger.x < rightTrigger.x;
  if (leftTrigger.y != rightTrigger.y) return leftTrigger.y < rightTrigger.y;
  return leftTrigger.z < rightTrigger.z;
}

bool CollectPortal(KR_ObjectID object, void *user) {
  Roster *roster = static_cast<Roster *>(user);
  Portal *portal = roster == NULL
                       ? NULL
                       : ResolvePortal(roster->context, object);
  const char *name = roster == NULL || roster->context == NULL
                         ? NULL
                         : roster->context->searchObject(object);
  if (roster == NULL || portal == NULL || name == NULL || name[0] == 0 ||
      std::strlen(name) > kMaximumNameBytes ||
      !FiniteVector(portal->portalGetCoord()) ||
      !FiniteVector(portal->getPos()) || portal->portalGetSlotCnt() <= 0 ||
      portal->portalGetSlotCnt() > 1024 ||
      portal->portalGetOccupiedSlot() < 0 ||
      portal->portalGetOccupiedSlot() > portal->portalGetSlotCnt()) {
    if (roster != NULL) roster->valid = false;
    return Fail("Portal roster contains an unresolved owner");
  }
  RosterEntry entry;
  entry.name = name;
  entry.portal = portal;
  roster->entries.push_back(entry);
  return true;
}

bool CollectRoster(SimulationContext *context, Roster *roster) {
  if (context == NULL || roster == NULL)
    return Fail("Portal roster context/output is null");
  if (g_arena.getContext() != context)
    return Fail("Portal roster belongs to another Arena context");
  roster->context = context;
  roster->entries.clear();
  roster->valid = true;
  if (g_arena.searchSeanceClassTable("Portal") == ct_NULLID) return true;
  g_arena.userFind("Portal", CollectPortal, roster);
  if (!roster->valid || roster->entries.size() > kMaximumPortals)
    return false;
  std::sort(roster->entries.begin(), roster->entries.end(), PortalLess);
  return true;
}

bool ValidRecord(const StablePortalRecord &record) {
  return !record.name.empty() && record.name.size() <= kMaximumNameBytes &&
         record.name.find('\0') == std::string::npos &&
         FiniteVector(record.artefactPoint) &&
         FiniteVector(record.triggerPoint) && record.slots > 0 &&
         record.slots <= 1024 && record.occupied >= 0 &&
         record.occupied <= record.slots;
}

StablePortalRecord CaptureRecord(const RosterEntry &entry) {
  StablePortalRecord record;
  record.name = entry.name;
  record.artefactPoint = entry.portal->portalGetCoord();
  record.triggerPoint = entry.portal->getPos();
  record.slots = entry.portal->portalGetSlotCnt();
  record.occupied = entry.portal->portalGetOccupiedSlot();
  return record;
}

bool SameIdentity(const StablePortalRecord &left,
                  const StablePortalRecord &right) {
  return left.name == right.name &&
         SameVector(left.artefactPoint, right.artefactPoint) &&
         SameVector(left.triggerPoint, right.triggerPoint) &&
         left.slots == right.slots;
}

void PutU32(std::vector<unsigned char> *bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void PutDouble(std::vector<unsigned char> *bytes, double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  for (int shift = 0; shift < 64; shift += 8)
    bytes->push_back(static_cast<unsigned char>(bits >> shift));
}

void PutString(std::vector<unsigned char> *bytes, const std::string &value) {
  PutU32(bytes, static_cast<std::uint32_t>(value.size()));
  bytes->insert(bytes->end(), value.begin(), value.end());
}

void PutVector(std::vector<unsigned char> *bytes, const CFVector3 &value) {
  PutDouble(bytes, value.x);
  PutDouble(bytes, value.y);
  PutDouble(bytes, value.z);
}

bool GetU32(const std::vector<unsigned char> &bytes, std::size_t *offset,
            std::uint32_t *value) {
  if (offset == NULL || value == NULL || *offset > bytes.size() ||
      bytes.size() - *offset < 4) return false;
  *value = 0;
  for (int shift = 0; shift < 32; shift += 8)
    *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
  return true;
}

bool GetDouble(const std::vector<unsigned char> &bytes, std::size_t *offset,
               double *value) {
  if (offset == NULL || value == NULL || *offset > bytes.size() ||
      bytes.size() - *offset < 8) return false;
  std::uint64_t bits = 0;
  for (int shift = 0; shift < 64; shift += 8)
    bits |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
  std::memcpy(value, &bits, sizeof(bits));
  return true;
}

bool GetString(const std::vector<unsigned char> &bytes, std::size_t *offset,
               std::string *value) {
  std::uint32_t size = 0;
  if (value == NULL || !GetU32(bytes, offset, &size) ||
      size > kMaximumNameBytes || *offset > bytes.size() ||
      bytes.size() - *offset < size) return false;
  value->assign(size == 0 ? "" :
      reinterpret_cast<const char *>(&bytes[*offset]), size);
  *offset += size;
  return value->find('\0') == std::string::npos;
}

bool GetVector(const std::vector<unsigned char> &bytes, std::size_t *offset,
               CFVector3 *value) {
  return value != NULL && GetDouble(bytes, offset, &value->x) &&
         GetDouble(bytes, offset, &value->y) &&
         GetDouble(bytes, offset, &value->z);
}

bool Encode(const std::vector<StablePortalRecord> &records,
            std::vector<unsigned char> *bytes) {
  if (bytes == NULL || records.size() > kMaximumPortals) return false;
  bytes->clear();
  PutU32(bytes, kPortalMagic);
  PutU32(bytes, kPortalVersion);
  PutU32(bytes, static_cast<std::uint32_t>(records.size()));
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (!ValidRecord(records[index]) ||
        (index != 0 && SameIdentity(records[index - 1], records[index])))
      return false;
    PutString(bytes, records[index].name);
    PutVector(bytes, records[index].artefactPoint);
    PutVector(bytes, records[index].triggerPoint);
    PutU32(bytes, static_cast<std::uint32_t>(records[index].slots));
    PutU32(bytes, static_cast<std::uint32_t>(records[index].occupied));
  }
  return true;
}

bool Decode(const std::vector<unsigned char> &bytes,
            std::vector<StablePortalRecord> *records) {
  std::size_t offset = 0;
  std::uint32_t magic = 0, version = 0, count = 0;
  if (records == NULL || !GetU32(bytes, &offset, &magic) ||
      !GetU32(bytes, &offset, &version) ||
      !GetU32(bytes, &offset, &count) || magic != kPortalMagic ||
      version != kPortalVersion || count > kMaximumPortals) return false;
  records->clear();
  for (std::uint32_t index = 0; index < count; ++index) {
    StablePortalRecord record;
    std::uint32_t slots = 0, occupied = 0;
    if (!GetString(bytes, &offset, &record.name) ||
        !GetVector(bytes, &offset, &record.artefactPoint) ||
        !GetVector(bytes, &offset, &record.triggerPoint) ||
        !GetU32(bytes, &offset, &slots) ||
        !GetU32(bytes, &offset, &occupied)) return false;
    record.slots = static_cast<int>(slots);
    record.occupied = static_cast<int>(occupied);
    if (!ValidRecord(record) ||
        (index != 0 && SameIdentity(records->back(), record))) return false;
    records->push_back(record);
  }
  return offset == bytes.size();
}

bool RosterMatches(const Roster &roster,
                   const std::vector<StablePortalRecord> &records) {
  if (roster.entries.size() != records.size()) return false;
  for (std::size_t index = 0; index < records.size(); ++index)
    if (!SameIdentity(CaptureRecord(roster.entries[index]), records[index]))
      return false;
  return true;
}

Portal *FirstOpenPortal(Roster *roster) {
  if (roster == NULL) return NULL;
  for (std::size_t index = 0; index < roster->entries.size(); ++index)
    if (roster->entries[index].portal->portalGetOccupiedSlot() <
        roster->entries[index].portal->portalGetSlotCnt())
      return roster->entries[index].portal;
  return NULL;
}

void ClearLevelBindings(SimulationContext *context, bool removeObjects) {
  for (std::vector<LevelBinding>::reverse_iterator it =
           g_levelBindings.rbegin();
       it != g_levelBindings.rend(); ++it) {
    if (it->reference != NULL &&
        it->reference->GetUserAttrib() == it->portal) {
      it->reference->SetAnimationCallback(NULL);
      it->reference->SetUserAttrib(NULL);
    }
    if (removeObjects && context != NULL && !IsNul(it->object) &&
        context->isExist(it->object))
      context->removeObject(it->object);
  }
  g_levelBindings.clear();
  g_levelContext = NULL;
  g_transitionRequested = false;
}

}  // namespace

SPortalAdmissionProbeSummary::SPortalAdmissionProbeSummary()
    : portalCount(0), attachedRejected(0), fullRejected(0), consumed(0),
      occupiedAdvanced(0), eventResidueCleared(0) {}

SPortalTransitionProbeSummary::SPortalTransitionProbeSummary()
    : portalCount(0), fullPortal(0), collisionAccepted(0),
      transitionRequested(0) {}

SPortalPresentationProbeSummary::SPortalPresentationProbeSummary()
    : portalCount(0), singularStatus(0), fewStatus(0), manyStatus(0),
      restoredStatus(0), messagesPublished(0), arabeskPresent(0),
      arabeskRemoved(0), arabeskRecreated(0), arabeskRestoreRemoved(0) {}

void PortalActiveWorldState_Link() {}

const char *PortalActiveWorldState_LastFailure() {
  return g_lastFailure.c_str();
}

bool PortalActiveWorldState_InitializeLevelSubjects(
    SimulationContext *context) {
  g_lastFailure.clear();
  if (context == NULL || g_arena.getContext() != context ||
      CViewScene::Current() == NULL)
    return Fail("Portal Level binding dependencies are unavailable");
  if (g_levelContext != NULL || !g_levelBindings.empty())
    return Fail("Portal Level binding is already active");
  if (g_arena.searchSeanceClassTable("Portal") == ct_NULLID)
    return Fail("Portal subject table is unavailable");

  CNameDecl *declaration =
      CViewScene::Current()->ObjRefNames().Lookup("portal");
  if (declaration == NULL || declaration->Count() <= 0 ||
      static_cast<std::size_t>(declaration->Count()) > kMaximumPortals)
    return Fail("Level has no valid portal scene-reference roster");

  g_levelContext = context;
  for (int ordinal = 0; ordinal < declaration->Count(); ++ordinal) {
    CViewObjectRef *reference =
        static_cast<CViewObjectRef *>((*declaration)[ordinal]);
    if (reference == NULL || reference->GetAnimationCallback() != NULL ||
        reference->GetUserAttrib() != NULL) {
      ClearLevelBindings(context, true);
      return Fail("portal scene reference is invalid or already owned");
    }

    char objectName[MAX_SYMBOLIC_LENGHT] = {};
    if (ordinal == 0)
      std::strcpy(objectName, "PortalObj");
    else
      std::snprintf(objectName, sizeof(objectName), "PortalObj.%d", ordinal);
    const KR_ObjectID object = g_arena.newObject("Portal", objectName);
    Portal *portal = ResolvePortal(context, object);
    if (portal == NULL) {
      if (!IsNul(object) && context->isExist(object))
        context->removeObject(object);
      ClearLevelBindings(context, true);
      return Fail("Portal subject allocation failed");
    }

    const CFVector3 artefactPoint(reference->GetDir().Offset());
    CFVector3 triggerPoint(0.0, 2.6, 6.97);
    triggerPoint = reference->GetDir() * triggerPoint;
    portal->portalInit(artefactPoint, 4);
    portal->portalSetPortalPoint(triggerPoint);
    reference->SetUserAttrib(portal);
    reference->SetAnimationCallback(PortalCallback);

    LevelBinding binding;
    binding.reference = reference;
    binding.portal = portal;
    binding.object = object;
    g_levelBindings.push_back(binding);
  }

  Roster roster = {};
  if (!CollectRoster(context, &roster) ||
      roster.entries.size() != g_levelBindings.size()) {
    ClearLevelBindings(context, true);
    return Fail("Portal subject roster did not match scene references");
  }
  return true;
}

void PortalActiveWorldState_ReleaseLevelSubjects(
    SimulationContext *context) {
  if (context == NULL || g_levelContext != context) return;
  ClearLevelBindings(context, true);
}

bool PortalActiveWorldState_RequestTransition(
    SimulationContext *context, const KR_ObjectID &portal) {
  if (context == NULL || context != g_levelContext || IsNul(portal) ||
      ResolvePortal(context, portal) == NULL)
    return Fail("Portal transition request has no live Level owner");
  g_lastFailure.clear();
  g_transitionRequested = true;
  return true;
}

bool PortalActiveWorldState_PublishAdmissionStatus(
    SimulationContext *context, const KR_ObjectID &portal) {
  g_lastFailure.clear();
  if (context == NULL || context != g_levelContext)
    return Fail("Portal presentation belongs to another Level context");
  return PresentPortalStatus(context, ResolvePortal(context, portal), NULL);
}

bool PortalActiveWorldState_TransitionPending() {
  return g_transitionRequested;
}

bool PortalActiveWorldState_TakeTransitionRequest() {
  if (!g_transitionRequested) return false;
  g_transitionRequested = false;
  return true;
}

int PortalActiveWorldState_LiveCount(SimulationContext *context) {
  Roster roster = {};
  return CollectRoster(context, &roster)
             ? static_cast<int>(roster.entries.size()) : -1;
}

bool PortalActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes) {
  g_lastFailure.clear();
  Roster roster = {};
  if (bytes == NULL || !CollectRoster(context, &roster)) return false;
  std::vector<StablePortalRecord> records;
  for (std::size_t index = 0; index < roster.entries.size(); ++index)
    records.push_back(CaptureRecord(roster.entries[index]));
  return Encode(records, bytes) || Fail("Portal state encoding failed");
}

bool PortalActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes) {
  std::vector<StablePortalRecord> records;
  return Decode(bytes, &records);
}

bool PortalActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes) {
  std::vector<unsigned char> current;
  return PortalActiveWorldState_ValidateStable(bytes) &&
         PortalActiveWorldState_CaptureStable(context, &current) &&
         current == bytes;
}

bool PortalActiveWorldState_PrepareStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes) {
  std::vector<StablePortalRecord> records;
  Roster roster = {};
  return Decode(bytes, &records) && CollectRoster(context, &roster) &&
         RosterMatches(roster, records);
}

bool PortalActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes) {
  std::vector<StablePortalRecord> records;
  Roster roster = {};
  if (!Decode(bytes, &records) || !CollectRoster(context, &roster) ||
      !RosterMatches(roster, records))
    return Fail("Portal authored roster does not match the saved Level");
  for (std::size_t index = 0; index < records.size(); ++index)
    roster.entries[index].portal->m_occupiedSlotCnt = records[index].occupied;
  return (ReconcilePortalArabesk(context, roster) &&
          PortalActiveWorldState_MatchesStable(context, bytes)) ||
         Fail("Portal occupancy reconstruction failed");
}

bool PortalActiveWorldState_RejectAttachedProbe(
    SimulationContext *context, const KR_ObjectID &artefact,
    SPortalAdmissionProbeSummary *summary) {
  g_lastFailure.clear();
  Roster roster = {};
  if (summary == NULL || !CollectRoster(context, &roster)) return false;
  summary->portalCount = static_cast<int>(roster.entries.size());
  Portal *portal = FirstOpenPortal(&roster);
  IArtefact *attached = context == NULL ? NULL : static_cast<IArtefact *>(
      context->queryInterface(artefact, IArtefactIID));
  if (portal == NULL || attached == NULL || !attached->isAttached())
    return Fail("Portal attached-rejection probe has no eligible owners");
  const int before = portal->portalGetOccupiedSlot();
  portal->portalAddArtefact(artefact);
  summary->attachedRejected = context->isExist(artefact) &&
      attached->isAttached() && portal->portalGetOccupiedSlot() == before
          ? 1 : 0;
  return summary->attachedRejected == 1 ||
         Fail("Portal consumed an attached Artefact");
}

bool PortalActiveWorldState_AdmissionProbe(
    SimulationContext *context, const KR_ObjectID &artefact,
    double timeStamp, SPortalAdmissionProbeSummary *summary) {
  g_lastFailure.clear();
  Roster roster = {};
  if (summary == NULL || !std::isfinite(timeStamp) || timeStamp < 0.1 ||
      !CollectRoster(context, &roster)) return false;
  summary->portalCount = static_cast<int>(roster.entries.size());
  Portal *portal = FirstOpenPortal(&roster);
  IArtefact *freeArtefact = context == NULL ? NULL :
      static_cast<IArtefact *>(context->queryInterface(artefact,
                                                       IArtefactIID));
  if (portal == NULL || freeArtefact == NULL || freeArtefact->isAttached())
    return Fail("Portal admission probe has no free Artefact/open Portal");

  const int before = portal->portalGetOccupiedSlot();
  const int slots = portal->portalGetSlotCnt();
  portal->m_occupiedSlotCnt = slots;
  portal->portalAddArtefact(artefact);
  summary->fullRejected = context->isExist(artefact) &&
      portal->portalGetOccupiedSlot() == slots ? 1 : 0;
  portal->m_occupiedSlotCnt = before;
  if (!summary->fullRejected)
    return Fail("full Portal removed an extra Artefact");

  CFMatrix3x4 atPortal;
  atPortal.LoadIdentity().TranslateL(portal->portalGetCoord());
  freeArtefact->moveTo(atPortal);
  KR_Event event;
  event.label = ARTEFACT_MOVE;
  event.source = artefact;
  event.destination = artefact;
  event.timeStamp = timeStamp;
  context->sendEventNow(event);
  summary->consumed = context->isExist(artefact) ? 0 : 1;
  summary->occupiedAdvanced =
      portal->portalGetOccupiedSlot() == before + 1 ? 1 : 0;
  KR_Event residue[1];
  summary->eventResidueCleared =
      context->copyEvents(ARTEFACT_MOVE, artefact, residue, 1) == 0 &&
      context->copyEvents(ARTEFACT_CHANGEDIR, artefact, residue, 1) == 0
          ? 1 : 0;
  return (summary->consumed == 1 && summary->occupiedAdvanced == 1 &&
          summary->eventResidueCleared == 1) ||
         Fail("Portal admission did not close atomically");
}

bool PortalActiveWorldState_StageTransitionProbe(
    SimulationContext *context, double timeStamp,
    SPortalTransitionProbeSummary *summary) {
  g_lastFailure.clear();
  Roster roster = {};
  if (summary == NULL || !std::isfinite(timeStamp) || timeStamp < 0.1 ||
      !CollectRoster(context, &roster) || roster.entries.empty() ||
      g_vehicle == NULL || g_vehicle->getContext() != context)
    return Fail("Portal transition probe dependencies are unavailable");
  summary->portalCount = static_cast<int>(roster.entries.size());
  Portal *portal = roster.entries.front().portal;
  portal->m_occupiedSlotCnt = portal->m_slotCnt;
  summary->fullPortal = portal->m_occupiedSlotCnt == portal->m_slotCnt ? 1 : 0;

  KR_Event collision;
  collision.label = t_EV_ONCOLLISION;
  collision.source = g_vehicle->getObjectID();
  collision.destination = portal->getObjectID();
  collision.timeStamp = timeStamp;
  collision.data.open(EDO_WRITE)
      .putObjectID(g_vehicle->getObjectID())
      .close();
  summary->collisionAccepted = portal->receiveEvent(collision) == 1 ? 1 : 0;
  summary->transitionRequested = g_transitionRequested ? 1 : 0;
  return (summary->fullPortal == 1 && summary->collisionAccepted == 1 &&
          summary->transitionRequested == 1) ||
         Fail("full Portal collision did not request a Level transition");
}

bool PortalActiveWorldState_StagePresentationProbe(
    SimulationContext *context,
    SPortalPresentationProbeSummary *summary) {
  g_lastFailure.clear();
  Roster roster = {};
  if (summary == NULL || !CollectRoster(context, &roster) ||
      roster.entries.empty())
    return Fail("Portal presentation probe dependencies are unavailable");
  summary->portalCount = static_cast<int>(roster.entries.size());
  Portal *portal = roster.entries.front().portal;
  const int slots = portal->portalGetSlotCnt();
  if (slots < 4)
    return Fail("Portal presentation probe needs the retail four-slot owner");

  char formatted[CON_MAX_MESSAGE_LEN] = {};
  int kind = kPortalStatusNone;
  summary->singularStatus =
      FormatPortalStatus(1, formatted, sizeof(formatted), &kind) &&
      kind == kPortalStatusSingular &&
      std::strcmp(formatted, kOneArtefactRemaining) == 0 ? 1 : 0;
  summary->fewStatus =
      FormatPortalStatus(4, formatted, sizeof(formatted), &kind) &&
      kind == kPortalStatusFew &&
      std::strcmp(formatted,
                  "\x8e\x91\x92\x80\x8b\x8e\x91\x9c 4 \x80\x90\x92\x85\x94\x80\x8a\x92\x41") == 0 ? 1 : 0;
  summary->manyStatus =
      FormatPortalStatus(5, formatted, sizeof(formatted), &kind) &&
      kind == kPortalStatusMany &&
      std::strcmp(formatted,
                  "\x8e\x91\x92\x80\x8b\x8e\x91\x9c 5 \x80\x90\x92\x85\x94\x80\x8a\x92\x8e\x82") == 0 ? 1 : 0;

  PortalPresentationEvidence singular;
  portal->m_occupiedSlotCnt = slots - 1;
  const bool singularPresented =
      PresentPortalStatus(context, portal, &singular) &&
      singular.statusKind == kPortalStatusSingular &&
      singular.remaining == 1;
  PortalPresentationEvidence few;
  portal->m_occupiedSlotCnt = slots - 2;
  const bool fewPresented = PresentPortalStatus(context, portal, &few) &&
      few.statusKind == kPortalStatusFew && few.remaining == 2;
  PortalPresentationEvidence restored;
  portal->m_occupiedSlotCnt = slots;
  const bool restoredPresented =
      PresentPortalStatus(context, portal, &restored) &&
      restored.statusKind == kPortalStatusRestored &&
      restored.remaining == 0 &&
      std::strcmp(restored.message, kPortalRestored) == 0;
  summary->restoredStatus = restoredPresented ? 1 : 0;
  summary->messagesPublished = singular.messagePublished +
      few.messagePublished + restored.messagePublished;
  summary->arabeskPresent = restored.arabeskPresent;
  summary->arabeskRemoved = restored.arabeskRemoved;

  const bool ownsArabesk = context->isExist("Fount.Attr.Arab");
  std::vector<unsigned char> partialState;
  portal->m_occupiedSlotCnt = slots - 1;
  const bool partialCaptured =
      PortalActiveWorldState_CaptureStable(context, &partialState);
  const bool partialApplied = partialCaptured &&
      PortalActiveWorldState_ApplyStableReferences(context, partialState);
  summary->arabeskRecreated =
      ownsArabesk && partialApplied && context->isExist(kPortalArabesk) ? 1 : 0;

  std::vector<unsigned char> fullState;
  portal->m_occupiedSlotCnt = slots;
  const bool fullCaptured =
      PortalActiveWorldState_CaptureStable(context, &fullState);
  const bool fullApplied = fullCaptured &&
      PortalActiveWorldState_ApplyStableReferences(context, fullState);
  summary->arabeskRestoreRemoved =
      ownsArabesk && fullApplied && !context->isExist(kPortalArabesk) ? 1 : 0;

  return (summary->singularStatus == 1 && summary->fewStatus == 1 &&
          summary->manyStatus == 1 && singularPresented && fewPresented &&
          summary->restoredStatus == 1 && summary->messagesPublished == 3 &&
          (!ownsArabesk ||
           (summary->arabeskPresent == 1 && summary->arabeskRemoved == 1 &&
            summary->arabeskRecreated == 1 &&
            summary->arabeskRestoreRemoved == 1))) ||
         Fail("Portal May presentation contract diverged");
}
