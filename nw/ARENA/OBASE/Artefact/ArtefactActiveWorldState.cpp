#include "ArtefactActiveWorldState.h"

#include "Artefact.h"
#include "ArtefactAttributeState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "kernel/h/context.h"
#include "message/artfmsg.h"
#include "message/unitmsg.h"

namespace {

const std::uint32_t kArtefactMagic = 0x31545241u;  // ART1
const std::uint32_t kArtefactVersion = 1u;
const std::size_t kMaximumArtefacts = 64u;
const std::size_t kMaximumNameBytes = MAX_SYMBOLIC_LENGHT - 1;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
std::string g_lastFailure;

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

bool FiniteMatrix(const CFMatrix3x4 &value) {
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      if (!std::isfinite(value.m[row][column])) return false;
  return true;
}

struct StableArtefactRecord {
  std::string name;
  std::string attribute;
  std::string commander;
  std::string carrier;
  CFMatrix3x4 orientation;
  CFVector3 direction;
  double attributeStartTime;
  int hasMoveEvent;
  double moveEventTime;
  int hasChangeDirectionEvent;
  double changeDirectionEventTime;

  StableArtefactRecord()
      : direction(0.0, 0.0, 0.0), attributeStartTime(0.0),
        hasMoveEvent(0), moveEventTime(0.0),
        hasChangeDirectionEvent(0), changeDirectionEventTime(0.0) {
    orientation.LoadIdentity();
  }
};

struct RosterEntry {
  std::string name;
  Artefact *artefact;
};

struct Roster {
  SimulationContext *context;
  std::vector<RosterEntry> entries;
  bool valid;
  bool requireReady;
};

Artefact *ResolveArtefact(SimulationContext *context,
                          const KR_ObjectID &object) {
  if (context == NULL || IsNul(object) || !context->isExist(object))
    return NULL;
  IDynamicObject *dynamic = static_cast<IDynamicObject *>(
      context->queryInterface(object, IDynamicObjectIID));
  return dynamic == NULL ? NULL : dynamic_cast<Artefact *>(dynamic);
}

bool CollectArtefact(KR_ObjectID object, void *user) {
  Roster *roster = static_cast<Roster *>(user);
  Artefact *artefact = roster == NULL
                           ? NULL
                           : ResolveArtefact(roster->context, object);
  const char *name = roster == NULL || roster->context == NULL
                         ? NULL
                         : roster->context->searchObject(object);
  if (roster == NULL || artefact == NULL || name == NULL || name[0] == 0 ||
      std::strlen(name) > kMaximumNameBytes ||
      (roster->requireReady &&
       (artefact->m_attr == NULL ||
        IsNul(artefact->m_artefactAttrID)))) {
    if (roster != NULL) roster->valid = false;
    return Fail("Artefact roster contains an unresolved owner");
  }
  RosterEntry entry;
  entry.name = name;
  entry.artefact = artefact;
  roster->entries.push_back(entry);
  return true;
}

bool CollectRoster(SimulationContext *context, bool requireReady,
                   Roster *roster) {
  if (context == NULL || roster == NULL)
    return Fail("Artefact roster context/output is null");
  if (g_arena.getContext() != context)
    return Fail("Artefact roster belongs to another Arena context");
  roster->context = context;
  roster->entries.clear();
  roster->valid = true;
  roster->requireReady = requireReady;
  if (g_arena.searchSeanceClassTable("Artefact") == ct_NULLID) return true;
  g_arena.userFind("Artefact", CollectArtefact, roster);
  if (!roster->valid) return false;
  std::sort(roster->entries.begin(), roster->entries.end(),
            [](const RosterEntry &left, const RosterEntry &right) {
              return left.name < right.name;
            });
  return roster->entries.size() <= kMaximumArtefacts;
}

std::string SymbolicName(SimulationContext *context,
                         const KR_ObjectID &object) {
  const char *name = context == NULL || IsNul(object) ||
                             !context->isExist(object)
                         ? NULL
                         : context->searchObject(object);
  return name == NULL ? std::string() : std::string(name);
}

bool CapturePrivateEvent(SimulationContext *context, int label,
                         const KR_ObjectID &object, int *present,
                         double *timeStamp) {
  KR_Event events[2];
  const int count = context->copyEvents(label, object, events, 2);
  if (count < 0 || count > 1) return false;
  *present = count;
  *timeStamp = count == 0 ? 0.0 : events[0].timeStamp;
  return count == 0 ||
         (events[0].source == object && events[0].destination == object &&
          std::isfinite(events[0].timeStamp));
}

bool CaptureRecord(SimulationContext *context, const RosterEntry &entry,
                   StableArtefactRecord *record) {
  if (record == NULL || entry.artefact == NULL) return false;
  Artefact *artefact = entry.artefact;
  record->name = entry.name;
  record->attribute = SymbolicName(context, artefact->m_artefactAttrID);
  record->commander = SymbolicName(context, artefact->m_commander);
  record->carrier = SymbolicName(context, artefact->m_carrierID);
  record->orientation = artefact->m_orient;
  record->direction = artefact->m_dir;
  record->attributeStartTime = artefact->m_viewDynObj.m_startTime;
  if (record->attribute.empty() ||
      __attrArtefactTable.searchAttribute(artefact->m_artefactAttrID) == NULL)
    return Fail("Artefact attribute dependency is unresolved");
  if (!IsNul(artefact->m_commander) && record->commander.empty())
    return Fail("Artefact commander dependency is unresolved");
  if (!IsNul(artefact->m_carrierID) && record->carrier.empty())
    return Fail("Artefact carrier dependency is unresolved");
  if (!CapturePrivateEvent(context, ARTEFACT_MOVE,
                           artefact->getObjectID(),
                           &record->hasMoveEvent,
                           &record->moveEventTime))
    return Fail("Artefact move event boundary is unstable");
  if (!CapturePrivateEvent(context, ARTEFACT_CHANGEDIR,
                           artefact->getObjectID(),
                           &record->hasChangeDirectionEvent,
                           &record->changeDirectionEventTime))
    return Fail("Artefact direction event boundary is unstable");
  return true;
}

bool ValidName(const std::string &value, bool allowEmpty) {
  return (allowEmpty || !value.empty()) &&
         value.size() <= kMaximumNameBytes &&
         value.find('\0') == std::string::npos;
}

bool ValidateRecord(const StableArtefactRecord &record) {
  return ValidName(record.name, false) &&
         ValidName(record.attribute, false) &&
         ValidName(record.commander, true) && ValidName(record.carrier, true) &&
         FiniteMatrix(record.orientation) && FiniteVector(record.direction) &&
         std::isfinite(record.attributeStartTime) &&
         (record.hasMoveEvent == 0 || record.hasMoveEvent == 1) &&
         (record.hasChangeDirectionEvent == 0 ||
          record.hasChangeDirectionEvent == 1) &&
         ((!record.hasMoveEvent && record.moveEventTime == 0.0) ||
          (record.hasMoveEvent && std::isfinite(record.moveEventTime))) &&
         ((!record.hasChangeDirectionEvent &&
           record.changeDirectionEventTime == 0.0) ||
          (record.hasChangeDirectionEvent &&
           std::isfinite(record.changeDirectionEventTime)));
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

void PutMatrix(std::vector<unsigned char> *bytes,
               const CFMatrix3x4 &value) {
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      PutDouble(bytes, value.m[row][column]);
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
  if (value == NULL || offset == NULL || *offset > bytes.size() ||
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

bool GetMatrix(const std::vector<unsigned char> &bytes, std::size_t *offset,
               CFMatrix3x4 *value) {
  if (value == NULL) return false;
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      if (!GetDouble(bytes, offset, &value->m[row][column])) return false;
  return true;
}

bool EncodeRecords(const std::vector<StableArtefactRecord> &records,
                   std::vector<unsigned char> *bytes) {
  if (bytes == NULL || records.size() > kMaximumArtefacts) return false;
  bytes->clear();
  PutU32(bytes, kArtefactMagic);
  PutU32(bytes, kArtefactVersion);
  PutU32(bytes, static_cast<std::uint32_t>(records.size()));
  for (std::size_t index = 0; index < records.size(); ++index) {
    const StableArtefactRecord &record = records[index];
    if (!ValidateRecord(record) ||
        (index != 0 && records[index - 1].name >= record.name)) return false;
    PutString(bytes, record.name);
    PutString(bytes, record.attribute);
    PutString(bytes, record.commander);
    PutString(bytes, record.carrier);
    PutMatrix(bytes, record.orientation);
    PutVector(bytes, record.direction);
    PutDouble(bytes, record.attributeStartTime);
    PutU32(bytes, static_cast<std::uint32_t>(record.hasMoveEvent));
    PutDouble(bytes, record.moveEventTime);
    PutU32(bytes,
           static_cast<std::uint32_t>(record.hasChangeDirectionEvent));
    PutDouble(bytes, record.changeDirectionEventTime);
  }
  return true;
}

bool DecodeRecords(const std::vector<unsigned char> &bytes,
                   std::vector<StableArtefactRecord> *records) {
  std::size_t offset = 0;
  std::uint32_t magic = 0, version = 0, count = 0;
  if (records == NULL || !GetU32(bytes, &offset, &magic) ||
      !GetU32(bytes, &offset, &version) ||
      !GetU32(bytes, &offset, &count) || magic != kArtefactMagic ||
      version != kArtefactVersion || count > kMaximumArtefacts) return false;
  records->clear();
  for (std::uint32_t index = 0; index < count; ++index) {
    StableArtefactRecord record;
    std::uint32_t move = 0, changed = 0;
    if (!GetString(bytes, &offset, &record.name) ||
        !GetString(bytes, &offset, &record.attribute) ||
        !GetString(bytes, &offset, &record.commander) ||
        !GetString(bytes, &offset, &record.carrier) ||
        !GetMatrix(bytes, &offset, &record.orientation) ||
        !GetVector(bytes, &offset, &record.direction) ||
        !GetDouble(bytes, &offset, &record.attributeStartTime) ||
        !GetU32(bytes, &offset, &move) ||
        !GetDouble(bytes, &offset, &record.moveEventTime) ||
        !GetU32(bytes, &offset, &changed) ||
        !GetDouble(bytes, &offset, &record.changeDirectionEventTime) ||
        move > 1 || changed > 1) return false;
    record.hasMoveEvent = static_cast<int>(move);
    record.hasChangeDirectionEvent = static_cast<int>(changed);
    if (!ValidateRecord(record) ||
        (index != 0 && records->back().name >= record.name)) return false;
    records->push_back(record);
  }
  return offset == bytes.size();
}

bool RosterMatches(const Roster &roster,
                   const std::vector<StableArtefactRecord> &records) {
  if (roster.entries.size() != records.size()) return false;
  for (std::size_t index = 0; index < records.size(); ++index)
    if (roster.entries[index].name != records[index].name) return false;
  return true;
}

void ClearPrivateEvents(SimulationContext *context,
                        const KR_ObjectID &object) {
  while (context != NULL && context->removeEvent(ARTEFACT_MOVE, object) == 1) {}
  while (context != NULL &&
         context->removeEvent(ARTEFACT_CHANGEDIR, object) == 1) {}
}

bool ApplyRecord(SimulationContext *context, Artefact *artefact,
                 const StableArtefactRecord &record) {
  if (context == NULL || artefact == NULL ||
      !context->isExist(record.attribute.c_str())) return false;
  const KR_ObjectID attribute = context->searchObject(record.attribute.c_str());
  if (__attrArtefactTable.searchAttribute(attribute) == NULL) return false;
  KR_Event setAttribute;
  setAttribute.label = KR_SET_ATTR;
  setAttribute.source = artefact->getObjectID();
  setAttribute.destination = artefact->getObjectID();
  setAttribute.timeStamp = record.attributeStartTime;
  setAttribute.data.open(EDO_WRITE).putObjectID(attribute).close();
  if (artefact->receiveEvent(setAttribute) != 1) return false;

  ClearPrivateEvents(context, artefact->getObjectID());
  CFMatrix3x4 orientation = record.orientation;
  artefact->moveTo(orientation);
  artefact->m_dir = record.direction;
  artefact->m_commander = record.commander.empty()
      ? KR_ObjectID::NUL() : context->searchObject(record.commander.c_str());
  artefact->m_carrierID = KR_ObjectID::NUL();
  artefact->m_carrier = NULL;
  if (!record.carrier.empty()) {
    if (!context->isExist(record.carrier.c_str())) return false;
    const KR_ObjectID carrierID = context->searchObject(record.carrier.c_str());
    ICarrier *carrier = static_cast<ICarrier *>(
        context->queryInterface(carrierID, ICarrierIID));
    if (carrier == NULL || carrier->m_artefact != NULL ||
        !artefact->attachTo(carrierID, carrier)) return false;
    carrier->carrierTakeArtefact(artefact->getObjectID(), artefact);
  }
  if (record.hasMoveEvent) {
    KR_Event event;
    event.label = ARTEFACT_MOVE;
    event.source = artefact->getObjectID();
    event.destination = artefact->getObjectID();
    event.timeStamp = record.moveEventTime;
    context->addEvent(event);
  }
  if (record.hasChangeDirectionEvent) {
    KR_Event event;
    event.label = ARTEFACT_CHANGEDIR;
    event.source = artefact->getObjectID();
    event.destination = artefact->getObjectID();
    event.timeStamp = record.changeDirectionEventTime;
    context->addEvent(event);
  }
  return true;
}

void DetachAndRemove(SimulationContext *context, const KR_ObjectID &object) {
  if (context == NULL || !context->isExist(object)) return;
  Artefact *artefact = ResolveArtefact(context, object);
  ClearPrivateEvents(context, object);
  if (artefact != NULL && artefact->m_carrier != NULL)
    artefact->m_carrier->carrierOnRemoveArtefact();
  if (artefact != NULL) {
    artefact->m_carrier = NULL;
    artefact->m_carrierID = KR_ObjectID::NUL();
  }
  context->removeObject(object);
  // Artefact::removeNotify() can schedule a move event while dropping from a
  // carrier.  A removed owner must leave no private queue residue.
  ClearPrivateEvents(context, object);
}

}  // namespace

void ArtefactActiveWorldState_Link() {}

const char *ArtefactActiveWorldState_LastFailure() {
  return g_lastFailure.c_str();
}

int ArtefactActiveWorldState_LiveCount(SimulationContext *context) {
  Roster roster = {};
  return CollectRoster(context, false, &roster)
             ? static_cast<int>(roster.entries.size()) : -1;
}

bool ArtefactActiveWorldState_IsReady(SimulationContext *context,
                                      const KR_ObjectID &object) {
  Artefact *artefact = ResolveArtefact(context, object);
  return artefact != NULL && artefact->m_attr != NULL &&
         !IsNul(artefact->m_artefactAttrID) &&
         __attrArtefactTable.searchAttribute(artefact->m_artefactAttrID) ==
             artefact->m_attr;
}

unsigned long long ArtefactActiveWorldState_Fingerprint(
    SimulationContext *context) {
  std::vector<unsigned char> bytes;
  if (!ArtefactActiveWorldState_CaptureStable(context, &bytes)) return 0;
  unsigned long long hash = kHashOffset;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    hash ^= bytes[index];
    hash *= kHashPrime;
  }
  return hash;
}

bool ArtefactActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes) {
  g_lastFailure.clear();
  Roster roster = {};
  if (bytes == NULL || !CollectRoster(context, true, &roster)) return false;
  std::vector<StableArtefactRecord> records;
  for (std::size_t index = 0; index < roster.entries.size(); ++index) {
    StableArtefactRecord record;
    if (!CaptureRecord(context, roster.entries[index], &record) ||
        !ValidateRecord(record)) return false;
    records.push_back(record);
  }
  return EncodeRecords(records, bytes);
}

bool ArtefactActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes) {
  std::vector<StableArtefactRecord> records;
  return DecodeRecords(bytes, &records);
}

bool ArtefactActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes) {
  std::vector<unsigned char> current;
  return ArtefactActiveWorldState_ValidateStable(bytes) &&
         ArtefactActiveWorldState_CaptureStable(context, &current) &&
         current == bytes;
}

bool ArtefactActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners) {
  std::vector<StableArtefactRecord> records;
  Roster roster = {};
  if (owners == NULL || !owners->empty() || !DecodeRecords(bytes, &records) ||
      !CollectRoster(context, true, &roster) ||
      !RosterMatches(roster, records)) return false;
  for (std::size_t index = 0; index < roster.entries.size(); ++index)
    owners->push_back(roster.entries[index].artefact->getObjectID());
  return true;
}

bool ArtefactActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created) {
  std::vector<StableArtefactRecord> records;
  Roster roster = {};
  if (context == NULL || created == NULL || !created->empty() ||
      !DecodeRecords(bytes, &records) ||
      !CollectRoster(context, false, &roster)) return false;
  if (!roster.entries.empty()) return RosterMatches(roster, records);
  for (std::size_t index = 0; index < records.size(); ++index)
    if (!context->isExist(records[index].attribute.c_str()) ||
        __attrArtefactTable.searchAttribute(context->searchObject(
            records[index].attribute.c_str())) == NULL)
      return Fail("Artefact attribute dependency is unavailable");
  for (std::size_t index = 0; index < records.size(); ++index) {
    KR_ObjectID object = g_arena.newObject("Artefact",
                                            records[index].name.c_str());
    if (IsNul(object)) {
      ArtefactActiveWorldState_RemoveStableOwners(context, created);
      return Fail("Artefact owner allocation failed");
    }
    created->push_back(object);
  }
  Roster restored = {};
  if (!CollectRoster(context, false, &restored) ||
      !RosterMatches(restored, records)) {
    ArtefactActiveWorldState_RemoveStableOwners(context, created);
    return Fail("Artefact symbolic owner allocation did not match");
  }
  return true;
}

bool ArtefactActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes) {
  std::vector<StableArtefactRecord> records;
  Roster roster = {};
  if (!DecodeRecords(bytes, &records) ||
      !CollectRoster(context, false, &roster) ||
      !RosterMatches(roster, records)) return false;
  for (std::size_t index = 0; index < records.size(); ++index)
    if (!ApplyRecord(context, roster.entries[index].artefact,
                     records[index]))
      return Fail("Artefact symbolic reconstruction failed");
  return ArtefactActiveWorldState_MatchesStable(context, bytes);
}

void ArtefactActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created) {
  if (created == NULL) return;
  if (context != NULL)
    for (std::vector<KR_ObjectID>::reverse_iterator object = created->rbegin();
         object != created->rend(); ++object)
      DetachAndRemove(context, *object);
  created->clear();
}
