#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "zav.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "i/dynobj.i"
#include "i/unit.i"
#include "../DynObj/DynObj.h"
#include "Howitzer.h"
#include "HowitzerActiveWorldState.h"
#include "HowitzerSubjectState.h"
#include "kernel/h/context.h"
#include "message/howitzermsg.h"
#include "message/peopmsg.h"
#include "storage/h/subject.h"
#include "super.h"

namespace {

const std::uint32_t kHowitzerMagic = 0x315A5748u;  // HWZ1
const std::uint32_t kHowitzerLegacyVersion = 1u;
const std::uint32_t kHowitzerSourceKindVersion = 2u;
const std::uint32_t kHowitzerVersion = 3u;
const std::size_t kMaximumHowitzers = 4096u;
const std::size_t kMaximumNameBytes = 1024u;
const int kMaximumEventsPerKind = 32;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
std::string g_lastFailure;

bool Fail(const std::string& message) {
  g_lastFailure = message;
  return false;
}

bool IsNul(const KR_ObjectID& object) {
  KR_ObjectID copy = object;
  return copy.isNUL();
}

bool FiniteVector(const CFVector3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

struct StableHowitzerRecord {
  enum Lifecycle {
    kReady = 0,
    kPendingStart = 1
  };

  struct Event {
    enum SourceKind {
      kOptionalSymbolic = 0,
      kDestinationSelf = 1
    };

    int sourceKind;
    std::string source;
    double timeStamp;

    Event() : sourceKind(kOptionalSymbolic), timeStamp(0.0) {}
  };

  std::string holder;
  std::string name;
  std::string attribute;
  std::string commander;
  std::string enemy;
  int lifecycle;
  int startMode;
  Event startEvent;
  int audibleThisFrame;
  int visible;
  double lastMoveTimeStamp;
  CFVector3 position;
  double damage;
  double rotateOy;
  double horizontalAngle;
  double lastActionTime;
  double lastEnemyScanTime;
  int shootThisBastard;
  double lastShootTime;
  std::vector<Event> findEnemyEvents;
  std::vector<Event> actionEvents;

  StableHowitzerRecord()
      : lifecycle(kReady), startMode(0), audibleThisFrame(0), visible(0),
        lastMoveTimeStamp(0.0),
        position(0.0, 0.0, 0.0), damage(0.0), rotateOy(0.0),
        horizontalAngle(0.0), lastActionTime(0.0),
        lastEnemyScanTime(0.0), shootThisBastard(0), lastShootTime(0.0) {}
};

struct HowitzerRosterEntry {
  std::string holder;
  std::string name;
  Howitzer* howitzer;
  bool ready;
  KR_ObjectID pendingAttribute;
  KR_ObjectID pendingSource;
  double pendingTimeStamp;
  int pendingMode;

  HowitzerRosterEntry()
      : howitzer(nullptr), ready(false),
        pendingAttribute(KR_ObjectID::NUL()),
        pendingSource(KR_ObjectID::NUL()), pendingTimeStamp(0.0),
        pendingMode(0) {}
};

struct HowitzerRoster {
  SimulationContext* context;
  std::vector<HowitzerRosterEntry> entries;
  bool valid;
  bool requireReady;
};

Howitzer* ResolveHowitzer(SimulationContext* context,
                          const KR_ObjectID& object) {
  KR_ObjectID candidate = object;
  if (context == nullptr || candidate.isNUL() ||
      !context->isExist(object))
    return nullptr;
  return static_cast<Howitzer*>(
      context->queryInterface(object, IUnknownIID));
}

bool InspectPendingStart(SimulationContext* context,
                         const KR_ObjectID& object,
                         HowitzerRosterEntry* entry) {
  if (context == nullptr || entry == nullptr) return false;
  KR_Event events[2];
  const int count = context->copyEventsTo(pe_EVCMD_START, object, events, 2);
  if (count != 1 || events[0].destination != object ||
      !std::isfinite(events[0].timeStamp) || events[0].timeStamp < 0.1)
    return false;
  KR_ObjectID attribute = KR_ObjectID::NUL();
  char holder[HOWITZER_MAX_NAME + 1] = {};
  int mode = 0;
  s_EventData& data = events[0].data.open(EDO_READ);
  data.getObjectID(attribute).getStr(holder, HOWITZER_MAX_NAME);
  if (data.remaining() != static_cast<int>(sizeof(int))) {
    data.close();
    return false;
  }
  data.getInt(mode).close();
  const int holderIndex = HowitzerSubjectState_FindHolder(holder);
  if ((mode != 0 && mode != 1) || holderIndex < 0 ||
      !HowitzerSubjectState_AttributeExists(context, attribute) ||
      !IsNul(HowitzerSubjectState_HolderOccupant(holder)))
    return false;
  entry->holder = holder;
  entry->pendingAttribute = attribute;
  entry->pendingSource = events[0].source;
  entry->pendingTimeStamp = events[0].timeStamp;
  entry->pendingMode = mode;
  return true;
}

bool CollectHowitzer(const KR_ObjectID object, void* user) {
  HowitzerRoster* roster = static_cast<HowitzerRoster*>(user);
  Howitzer* howitzer = ResolveHowitzer(roster->context, object);
  const char* name = roster->context == nullptr
                         ? nullptr
                         : roster->context->searchObject(object);
  const char* holder = howitzer == nullptr
                           ? nullptr
                           : HowitzerSubjectState_HolderName(
                                 howitzer->m_HolderIndex);
  const bool holderOwnsObject =
      holder != nullptr &&
      HowitzerSubjectState_HolderOccupant(holder) == object;
  const bool ready = howitzer != nullptr && howitzer->m_attr != nullptr &&
                     !IsNul(howitzer->m_HowitzerAttrID);
  const bool allocatedReady = !ready && howitzer != nullptr &&
      howitzer->m_attr == nullptr && IsNul(howitzer->m_HowitzerAttrID) &&
      holderOwnsObject;
  HowitzerRosterEntry entry;
  entry.name = name == nullptr ? "" : name;
  entry.howitzer = howitzer;
  entry.ready = ready || allocatedReady;
  const bool pending = !entry.ready && howitzer != nullptr &&
      howitzer->m_attr == nullptr && IsNul(howitzer->m_HowitzerAttrID) &&
      InspectPendingStart(roster->context, object, &entry);
  if (howitzer == nullptr || name == nullptr || name[0] == '\0' ||
      std::strlen(name) > kMaximumNameBytes ||
      (entry.ready && (holder == nullptr || holder[0] == '\0' ||
                 std::strlen(holder) > kMaximumNameBytes ||
                 !holderOwnsObject)) || (!entry.ready && !pending) ||
      (roster->requireReady && !ready)) {
    if (howitzer == nullptr)
      Fail("Howitzer roster contains an unresolved owner");
    else if (name == nullptr || name[0] == '\0')
      Fail("Howitzer roster owner has no symbolic name");
    else if (entry.ready &&
             (holder == nullptr || holder[0] == '\0' || !holderOwnsObject))
      Fail(std::string("Howitzer holder ownership is invalid: ") + name);
    else if (!entry.ready && !pending)
      Fail(std::string("Howitzer pending START is invalid: ") + name);
    else if (!ready)
      Fail(std::string("Howitzer owner is not runtime-ready: ") + name);
    else
      Fail("Howitzer symbolic identity exceeds the codec limit");
    roster->valid = false;
    return false;
  }
  if (entry.ready) entry.holder = holder;
  roster->entries.push_back(entry);
  return true;
}

bool RosterEntryLess(const HowitzerRosterEntry& left,
                     const HowitzerRosterEntry& right) {
  return left.holder < right.holder;
}

bool CollectRoster(SimulationContext* context, bool requireReady,
                   HowitzerRoster* roster) {
  if (context == nullptr || roster == nullptr)
    return Fail("Howitzer roster arguments are invalid");
  if (g_arena.getContext() != context)
    return Fail("Howitzer roster belongs to another Arena context");
  roster->context = context;
  roster->entries.clear();
  roster->valid = true;
  roster->requireReady = requireReady;
  if (g_arena.searchSeanceClassTable("Howitzer") == ct_NULLID) return true;
  g_arena.userFind("Howitzer", CollectHowitzer, roster);
  if (!roster->valid) return false;
  std::sort(roster->entries.begin(), roster->entries.end(), RosterEntryLess);
  for (std::size_t index = 1; index < roster->entries.size(); ++index)
    if (roster->entries[index - 1].holder == roster->entries[index].holder)
      return Fail("Howitzer roster contains duplicate holder ownership");
  return true;
}

bool CaptureOptionalReference(SimulationContext* context,
                              const KR_ObjectID& object,
                              const char* role,
                              bool canonicalizeAmbiguous,
                              std::string* name) {
  if (context == nullptr || name == nullptr) return false;
  name->clear();
  KR_ObjectID candidate = object;
  if (candidate.isNUL() || !context->isExist(object)) return true;
  const char* symbolic = context->searchObject(object);
  if (symbolic == nullptr || symbolic[0] == '\0' ||
      std::strlen(symbolic) > kMaximumNameBytes)
    return Fail("Howitzer reference has no bounded symbolic identity");
  const KR_ObjectID canonical = context->searchObject(symbolic);
  if (canonical != object) {
    // Installed missions can deliberately publish several Howitzers with the
    // same symbolic owner name.  Their currently selected enemy is a
    // transient AI target, not an ownership dependency: persist that one
    // ambiguous link as NUL and let the restored scanner reacquire it.  Stable
    // Commander and private scheduler references remain fail-closed.
    if (canonicalizeAmbiguous) return true;
    return Fail(std::string("Howitzer ") +
                (role == nullptr ? "reference" : role) +
                " is symbolically ambiguous: " + symbolic);
  }
  *name = symbolic;
  return true;
}

bool CaptureEvents(SimulationContext* context, Howitzer* howitzer,
                   int label,
                   bool allowEmpty,
                   std::vector<StableHowitzerRecord::Event>* stable) {
  if (context == nullptr || howitzer == nullptr || stable == nullptr)
    return false;
  KR_Event events[kMaximumEventsPerKind + 1];
  const int count = context->copyEventsTo(
      label, howitzer->getObjectID(), events, kMaximumEventsPerKind + 1);
  if (count < (allowEmpty ? 0 : 1) || count > kMaximumEventsPerKind)
    return Fail("Howitzer private scheduler cardinality is invalid (label=" +
                std::to_string(label) + ", count=" +
                std::to_string(count) + ")");
  stable->clear();
  for (int index = 0; index < count; ++index) {
    if (events[index].destination != howitzer->getObjectID() ||
        !std::isfinite(events[index].timeStamp) ||
        events[index].timeStamp < 0.1)
      return Fail("Howitzer private scheduler event is invalid");
    StableHowitzerRecord::Event entry;
    if (events[index].source == howitzer->getObjectID()) {
      entry.sourceKind = StableHowitzerRecord::Event::kDestinationSelf;
      entry.source.clear();
    } else if (!CaptureOptionalReference(context, events[index].source,
                                         "private-event source",
                                         false,
                                         &entry.source)) {
      return false;
    }
    entry.timeStamp = events[index].timeStamp;
    stable->push_back(entry);
  }
  return true;
}

bool CaptureRecord(SimulationContext* context,
                   const HowitzerRosterEntry& entry,
                   StableHowitzerRecord* record) {
  Howitzer* howitzer = entry.howitzer;
  const KR_ObjectID attributeID = entry.ready
      ? howitzer->m_HowitzerAttrID : entry.pendingAttribute;
  const char* attribute = howitzer == nullptr
                              ? nullptr
                              : context->searchObject(attributeID);
  if (record == nullptr || howitzer == nullptr || attribute == nullptr ||
      attribute[0] == '\0' || std::strlen(attribute) > kMaximumNameBytes ||
      !HowitzerSubjectState_AttributeExists(context, attributeID))
    return Fail("Howitzer symbolic attribute is unavailable");
  record->holder = entry.holder;
  record->name = entry.name;
  record->attribute = attribute;
  record->lifecycle = entry.ready ? StableHowitzerRecord::kReady
                                  : StableHowitzerRecord::kPendingStart;
  if (!CaptureOptionalReference(context, howitzer->m_commanderID,
                                "commander",
                                false,
                                &record->commander) ||
      !CaptureOptionalReference(context, howitzer->m_enemyID,
                                "enemy",
                                true,
                                &record->enemy))
    return false;
  if (!entry.ready) {
    record->startMode = entry.pendingMode;
    record->startEvent.timeStamp = entry.pendingTimeStamp;
    if (entry.pendingSource == howitzer->getObjectID()) {
      record->startEvent.sourceKind =
          StableHowitzerRecord::Event::kDestinationSelf;
      record->startEvent.source.clear();
    } else if (!CaptureOptionalReference(context, entry.pendingSource,
                                         "START source", false,
                                         &record->startEvent.source)) {
      return false;
    }
  }
  record->audibleThisFrame = howitzer->m_audibleThisFrame != 0 ? 1 : 0;
  record->visible = howitzer->m_isVisible != 0 ? 1 : 0;
  record->lastMoveTimeStamp = howitzer->m_lastMoveTimeStamp;
  record->position = howitzer->getPosition();
  record->damage = howitzer->m_damage;
  record->rotateOy = howitzer->m_rotateOy;
  record->horizontalAngle = howitzer->m_hAngle;
  record->lastActionTime = howitzer->m_lastActionTime;
  record->lastEnemyScanTime = howitzer->m_lastEnemyScanTime;
  record->shootThisBastard = howitzer->m_shootThisBastard ? 1 : 0;
  record->lastShootTime = howitzer->m_lastShootTime;
  return CaptureEvents(context, howitzer, HOWITZER_FIND_ENEMY,
                       !entry.ready, &record->findEnemyEvents) &&
         CaptureEvents(context, howitzer, HOWITZER_ACTION,
                       !entry.ready, &record->actionEvents);
}

bool ValidName(const std::string& value, bool allowEmpty) {
  return (allowEmpty || !value.empty()) &&
         value.size() <= kMaximumNameBytes &&
         value.find('\0') == std::string::npos;
}

bool ValidEvents(const std::vector<StableHowitzerRecord::Event>& events,
                 bool allowEmpty = false) {
  if ((!allowEmpty && events.empty()) || events.size() >
                           static_cast<std::size_t>(kMaximumEventsPerKind))
    return false;
  for (std::size_t index = 0; index < events.size(); ++index)
    if ((events[index].sourceKind !=
             StableHowitzerRecord::Event::kOptionalSymbolic &&
         events[index].sourceKind !=
             StableHowitzerRecord::Event::kDestinationSelf) ||
        (events[index].sourceKind ==
             StableHowitzerRecord::Event::kDestinationSelf &&
         !events[index].source.empty()) ||
        !ValidName(events[index].source, true) ||
        !std::isfinite(events[index].timeStamp) ||
        events[index].timeStamp < 0.1 ||
        (index != 0 &&
         events[index - 1].timeStamp > events[index].timeStamp))
      return false;
  return true;
}

bool ValidateRecord(const StableHowitzerRecord& record) {
  const bool common =
      ValidName(record.holder, false) && ValidName(record.name, false) &&
         ValidName(record.attribute, false) &&
         ValidName(record.commander, true) && ValidName(record.enemy, true) &&
         (record.audibleThisFrame == 0 || record.audibleThisFrame == 1) &&
         (record.visible == 0 || record.visible == 1) &&
         (record.shootThisBastard == 0 || record.shootThisBastard == 1) &&
         std::isfinite(record.lastMoveTimeStamp) &&
         record.lastMoveTimeStamp >= 0.0 && FiniteVector(record.position) &&
         std::isfinite(record.damage) && std::isfinite(record.rotateOy) &&
         std::isfinite(record.horizontalAngle) &&
         std::isfinite(record.lastActionTime) &&
         record.lastActionTime >= 0.0 &&
         std::isfinite(record.lastEnemyScanTime) &&
         record.lastEnemyScanTime >= 0.0 &&
         std::isfinite(record.lastShootTime) && record.lastShootTime >= 0.0;
  if (!common) return false;
  if (record.lifecycle == StableHowitzerRecord::kReady)
    return record.startMode == 0 &&
           record.startEvent.timeStamp == 0.0 &&
           record.startEvent.source.empty() &&
           ValidEvents(record.findEnemyEvents) &&
           ValidEvents(record.actionEvents);
  return record.lifecycle == StableHowitzerRecord::kPendingStart &&
         (record.startMode == 0 || record.startMode == 1) &&
         (record.startEvent.sourceKind ==
              StableHowitzerRecord::Event::kOptionalSymbolic ||
          record.startEvent.sourceKind ==
              StableHowitzerRecord::Event::kDestinationSelf) &&
         ValidName(record.startEvent.source, true) &&
         std::isfinite(record.startEvent.timeStamp) &&
         record.startEvent.timeStamp >= 0.1 &&
         ValidEvents(record.findEnemyEvents, true) &&
         record.actionEvents.empty();
}

void PutU32(std::vector<unsigned char>* bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void PutU64(std::vector<unsigned char>* bytes, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8)
    bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void PutDouble(std::vector<unsigned char>* bytes, double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  PutU64(bytes, bits);
}

void PutString(std::vector<unsigned char>* bytes, const std::string& value) {
  PutU32(bytes, static_cast<std::uint32_t>(value.size()));
  bytes->insert(bytes->end(), value.begin(), value.end());
}

void PutVector(std::vector<unsigned char>* bytes, const CFVector3& value) {
  PutDouble(bytes, value.x);
  PutDouble(bytes, value.y);
  PutDouble(bytes, value.z);
}

void PutEvents(
    std::vector<unsigned char>* bytes,
    const std::vector<StableHowitzerRecord::Event>& events,
    std::uint32_t version, const std::string& ownerName) {
  PutU32(bytes, static_cast<std::uint32_t>(events.size()));
  for (const StableHowitzerRecord::Event& event : events) {
    if (version >= kHowitzerSourceKindVersion)
      PutU32(bytes, static_cast<std::uint32_t>(event.sourceKind));
    PutString(bytes,
              version == kHowitzerLegacyVersion &&
                      event.sourceKind ==
                          StableHowitzerRecord::Event::kDestinationSelf
                  ? ownerName
                  : event.source);
    PutDouble(bytes, event.timeStamp);
  }
}

bool GetU32(const std::vector<unsigned char>& bytes, std::size_t* offset,
            std::uint32_t* value) {
  if (offset == nullptr || value == nullptr || *offset > bytes.size() ||
      bytes.size() - *offset < 4)
    return false;
  *value = 0;
  for (int shift = 0; shift < 32; shift += 8)
    *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
  return true;
}

bool GetU64(const std::vector<unsigned char>& bytes, std::size_t* offset,
            std::uint64_t* value) {
  if (offset == nullptr || value == nullptr || *offset > bytes.size() ||
      bytes.size() - *offset < 8)
    return false;
  *value = 0;
  for (int shift = 0; shift < 64; shift += 8)
    *value |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
  return true;
}

bool GetDouble(const std::vector<unsigned char>& bytes, std::size_t* offset,
               double* value) {
  std::uint64_t bits = 0;
  if (value == nullptr || !GetU64(bytes, offset, &bits)) return false;
  std::memcpy(value, &bits, sizeof(bits));
  return true;
}

bool GetString(const std::vector<unsigned char>& bytes, std::size_t* offset,
               std::string* value) {
  std::uint32_t size = 0;
  if (value == nullptr || !GetU32(bytes, offset, &size) ||
      size > kMaximumNameBytes || *offset > bytes.size() ||
      bytes.size() - *offset < size)
    return false;
  value->assign(size == 0
                    ? ""
                    : reinterpret_cast<const char*>(&bytes[*offset]),
                size);
  *offset += size;
  return value->find('\0') == std::string::npos;
}

bool GetVector(const std::vector<unsigned char>& bytes, std::size_t* offset,
               CFVector3* value) {
  return value != nullptr && GetDouble(bytes, offset, &value->x) &&
         GetDouble(bytes, offset, &value->y) &&
         GetDouble(bytes, offset, &value->z);
}

bool GetEvents(
    const std::vector<unsigned char>& bytes, std::size_t* offset,
    std::uint32_t version, bool allowEmpty,
    std::vector<StableHowitzerRecord::Event>* events) {
  std::uint32_t count = 0;
  if (events == nullptr || !GetU32(bytes, offset, &count) ||
      (!allowEmpty && count == 0) ||
      count > static_cast<std::uint32_t>(kMaximumEventsPerKind))
    return false;
  events->clear();
  for (std::uint32_t index = 0; index < count; ++index) {
    StableHowitzerRecord::Event event;
    std::uint32_t sourceKind = 0;
    if ((version >= kHowitzerSourceKindVersion &&
         !GetU32(bytes, offset, &sourceKind)) ||
        sourceKind > static_cast<std::uint32_t>(
                         StableHowitzerRecord::Event::kDestinationSelf) ||
        !GetString(bytes, offset, &event.source) ||
        !GetDouble(bytes, offset, &event.timeStamp))
      return false;
    event.sourceKind = static_cast<int>(sourceKind);
    events->push_back(event);
  }
  return ValidEvents(*events, allowEmpty);
}

bool PutRecord(std::vector<unsigned char>* bytes,
               const StableHowitzerRecord& record, std::uint32_t version) {
  if (!ValidateRecord(record) ||
      (version < kHowitzerVersion &&
       record.lifecycle != StableHowitzerRecord::kReady))
    return false;
  if (version >= kHowitzerVersion)
    PutU32(bytes, static_cast<std::uint32_t>(record.lifecycle));
  PutString(bytes, record.holder);
  PutString(bytes, record.name);
  PutString(bytes, record.attribute);
  PutString(bytes, record.commander);
  PutString(bytes, record.enemy);
  PutU32(bytes, static_cast<std::uint32_t>(record.audibleThisFrame));
  PutU32(bytes, static_cast<std::uint32_t>(record.visible));
  PutDouble(bytes, record.lastMoveTimeStamp);
  PutVector(bytes, record.position);
  PutDouble(bytes, record.damage);
  PutDouble(bytes, record.rotateOy);
  PutDouble(bytes, record.horizontalAngle);
  PutDouble(bytes, record.lastActionTime);
  PutDouble(bytes, record.lastEnemyScanTime);
  PutU32(bytes, static_cast<std::uint32_t>(record.shootThisBastard));
  PutDouble(bytes, record.lastShootTime);
  PutEvents(bytes, record.findEnemyEvents, version, record.name);
  PutEvents(bytes, record.actionEvents, version, record.name);
  if (version >= kHowitzerVersion) {
    PutU32(bytes, static_cast<std::uint32_t>(record.startEvent.sourceKind));
    PutString(bytes, record.startEvent.source);
    PutDouble(bytes, record.startEvent.timeStamp);
    PutU32(bytes, static_cast<std::uint32_t>(record.startMode));
  }
  return true;
}

bool GetRecord(const std::vector<unsigned char>& bytes, std::size_t* offset,
               std::uint32_t version, StableHowitzerRecord* record) {
  std::uint32_t lifecycle = 0, audible = 0, visible = 0, shooting = 0;
  if (record == nullptr ||
      (version >= kHowitzerVersion &&
       !GetU32(bytes, offset, &lifecycle)) ||
      lifecycle > static_cast<std::uint32_t>(
                      StableHowitzerRecord::kPendingStart) ||
      !GetString(bytes, offset, &record->holder) ||
      !GetString(bytes, offset, &record->name) ||
      !GetString(bytes, offset, &record->attribute) ||
      !GetString(bytes, offset, &record->commander) ||
      !GetString(bytes, offset, &record->enemy) ||
      !GetU32(bytes, offset, &audible) ||
      !GetU32(bytes, offset, &visible) ||
      !GetDouble(bytes, offset, &record->lastMoveTimeStamp) ||
      !GetVector(bytes, offset, &record->position) ||
      !GetDouble(bytes, offset, &record->damage) ||
      !GetDouble(bytes, offset, &record->rotateOy) ||
      !GetDouble(bytes, offset, &record->horizontalAngle) ||
      !GetDouble(bytes, offset, &record->lastActionTime) ||
      !GetDouble(bytes, offset, &record->lastEnemyScanTime) ||
      !GetU32(bytes, offset, &shooting) ||
      !GetDouble(bytes, offset, &record->lastShootTime) ||
      !GetEvents(bytes, offset, version,
                 lifecycle == StableHowitzerRecord::kPendingStart,
                 &record->findEnemyEvents) ||
      !GetEvents(bytes, offset, version,
                 lifecycle == StableHowitzerRecord::kPendingStart,
                 &record->actionEvents) || audible > 1u ||
      visible > 1u || shooting > 1u)
    return false;
  record->lifecycle = static_cast<int>(lifecycle);
  record->audibleThisFrame = static_cast<int>(audible);
  record->visible = static_cast<int>(visible);
  record->shootThisBastard = static_cast<int>(shooting);
  if (version == kHowitzerLegacyVersion) {
    for (StableHowitzerRecord::Event& event : record->findEnemyEvents)
      if (event.source == record->name) {
        event.sourceKind = StableHowitzerRecord::Event::kDestinationSelf;
        event.source.clear();
      }
    for (StableHowitzerRecord::Event& event : record->actionEvents)
      if (event.source == record->name) {
        event.sourceKind = StableHowitzerRecord::Event::kDestinationSelf;
        event.source.clear();
      }
  }
  if (version >= kHowitzerVersion) {
    std::uint32_t sourceKind = 0, startMode = 0;
    if (!GetU32(bytes, offset, &sourceKind) ||
        sourceKind > static_cast<std::uint32_t>(
                         StableHowitzerRecord::Event::kDestinationSelf) ||
        !GetString(bytes, offset, &record->startEvent.source) ||
        !GetDouble(bytes, offset, &record->startEvent.timeStamp) ||
        !GetU32(bytes, offset, &startMode) || startMode > 1u)
      return false;
    record->startEvent.sourceKind = static_cast<int>(sourceKind);
    record->startMode = static_cast<int>(startMode);
  }
  return ValidateRecord(*record);
}

bool EncodeRecordsVersion(const std::vector<StableHowitzerRecord>& records,
                          std::uint32_t version,
                          std::vector<unsigned char>* bytes) {
  if (version != kHowitzerLegacyVersion &&
      version != kHowitzerSourceKindVersion && version != kHowitzerVersion)
    return false;
  if (bytes == nullptr || records.size() > kMaximumHowitzers) return false;
  bytes->clear();
  PutU32(bytes, kHowitzerMagic);
  PutU32(bytes, version);
  PutU32(bytes, static_cast<std::uint32_t>(records.size()));
  for (std::size_t index = 0; index < records.size(); ++index) {
    if ((index != 0 &&
         records[index - 1].holder >= records[index].holder) ||
        !PutRecord(bytes, records[index], version))
      return Fail("Howitzer records are invalid or not in holder order");
  }
  return true;
}

bool EncodeRecords(const std::vector<StableHowitzerRecord>& records,
                   std::vector<unsigned char>* bytes) {
  return EncodeRecordsVersion(records, kHowitzerVersion, bytes);
}

bool DecodeRecords(const std::vector<unsigned char>& bytes,
                   std::vector<StableHowitzerRecord>* records) {
  std::size_t offset = 0;
  std::uint32_t magic = 0, version = 0, count = 0;
  if (records == nullptr || !GetU32(bytes, &offset, &magic) ||
      !GetU32(bytes, &offset, &version) ||
      !GetU32(bytes, &offset, &count) || magic != kHowitzerMagic ||
      (version != kHowitzerLegacyVersion &&
       version != kHowitzerSourceKindVersion &&
       version != kHowitzerVersion) ||
      count > kMaximumHowitzers)
    return false;
  records->clear();
  for (std::uint32_t index = 0; index < count; ++index) {
    StableHowitzerRecord record;
    if (!GetRecord(bytes, &offset, version, &record) ||
        (index != 0 && records->back().holder >= record.holder))
      return false;
    records->push_back(record);
  }
  return offset == bytes.size();
}

bool RosterMatches(const HowitzerRoster& roster,
                   const std::vector<StableHowitzerRecord>& records) {
  if (roster.entries.size() != records.size()) return false;
  for (std::size_t index = 0; index < records.size(); ++index)
    if (roster.entries[index].holder != records[index].holder ||
        roster.entries[index].name != records[index].name ||
        roster.entries[index].ready !=
            (records[index].lifecycle == StableHowitzerRecord::kReady))
      return false;
  return true;
}

std::string FirstEventDifference(
    const std::vector<StableHowitzerRecord::Event>& left,
    const std::vector<StableHowitzerRecord::Event>& right) {
  if (left.size() != right.size())
    return "event count " + std::to_string(left.size()) + "/" +
           std::to_string(right.size());
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (left[index].sourceKind != right[index].sourceKind)
      return "event source kind " +
             std::to_string(left[index].sourceKind) + "/" +
             std::to_string(right[index].sourceKind);
    if (left[index].source != right[index].source)
      return "event source " + left[index].source + "/" +
             right[index].source;
    if (left[index].timeStamp != right[index].timeStamp)
      return "event timestamp " + std::to_string(left[index].timeStamp) +
             "/" + std::to_string(right[index].timeStamp);
  }
  return std::string();
}

std::string FirstRecordDifference(const StableHowitzerRecord& left,
                                  const StableHowitzerRecord& right) {
  if (left.lifecycle != right.lifecycle) return "lifecycle";
  if (left.holder != right.holder) return "holder";
  if (left.name != right.name) return "name";
  if (left.attribute != right.attribute) return "attribute";
  if (left.commander != right.commander) return "commander";
  if (left.enemy != right.enemy) return "enemy";
  if (left.audibleThisFrame != right.audibleThisFrame)
    return "audible flag";
  if (left.visible != right.visible) return "visible flag";
  if (left.lastMoveTimeStamp != right.lastMoveTimeStamp)
    return "last move timestamp";
  if (left.position.x != right.position.x ||
      left.position.y != right.position.y ||
      left.position.z != right.position.z)
    return "position";
  if (left.damage != right.damage) return "damage";
  if (left.rotateOy != right.rotateOy) return "rotation";
  if (left.horizontalAngle != right.horizontalAngle)
    return "horizontal angle";
  if (left.lastActionTime != right.lastActionTime)
    return "last action timestamp";
  if (left.lastEnemyScanTime != right.lastEnemyScanTime)
    return "last enemy scan timestamp";
  if (left.shootThisBastard != right.shootThisBastard)
    return "shoot decision";
  if (left.lastShootTime != right.lastShootTime)
    return "last shot timestamp";
  const std::string event = FirstEventDifference(left.findEnemyEvents,
                                                 right.findEnemyEvents);
  if (!event.empty()) return event;
  const std::string action = FirstEventDifference(left.actionEvents,
                                                  right.actionEvents);
  if (!action.empty()) return action;
  if (left.startMode != right.startMode) return "START mode";
  if (left.startEvent.sourceKind != right.startEvent.sourceKind ||
      left.startEvent.source != right.startEvent.source ||
      left.startEvent.timeStamp != right.startEvent.timeStamp)
    return "START event";
  return std::string();
}

bool ResolveOptionalReference(SimulationContext* context,
                              const std::string& name, int interfaceId,
                              KR_ObjectID* object) {
  if (context == nullptr || object == nullptr) return false;
  if (name.empty()) {
    *object = KR_ObjectID::NUL();
    return true;
  }
  if (!context->isExist(name.c_str())) return false;
  *object = context->searchObject(name.c_str());
  return interfaceId == IUnknownIID ||
         context->queryInterface(*object, interfaceId) != nullptr;
}

void RemovePrivateEvents(SimulationContext* context,
                         const KR_ObjectID& object) {
  if (context == nullptr) return;
  context->removeEventsTo(pe_EVCMD_START, object);
  context->removeEventsTo(HOWITZER_FIND_ENEMY, object);
  context->removeEventsTo(HOWITZER_ACTION, object);
}

bool RestoreEvents(
    SimulationContext* context, const KR_ObjectID& object, int label,
    const std::vector<StableHowitzerRecord::Event>& stable) {
  // SimulationContext inserts an event before the first event with an equal
  // timestamp.  Replaying the captured queue forwards would therefore reverse
  // equal-time events (retail Level.01N has such a Howitzer pair).  Rebuild it
  // backwards to preserve both chronological and equal-time ordering.
  for (std::vector<StableHowitzerRecord::Event>::const_reverse_iterator it =
           stable.rbegin();
       it != stable.rend(); ++it) {
    const StableHowitzerRecord::Event& entry = *it;
    KR_ObjectID source;
    if (entry.sourceKind ==
        StableHowitzerRecord::Event::kDestinationSelf) {
      source = object;
    } else if (!ResolveOptionalReference(context, entry.source, IUnknownIID,
                                         &source)) {
      return false;
    }
    KR_Event event;
    event.label = label;
    event.source = source;
    event.destination = object;
    event.timeStamp = entry.timeStamp;
    context->addEvent(event);
  }
  return true;
}

bool ApplyRecord(SimulationContext* context, Howitzer* howitzer,
                 const StableHowitzerRecord& record) {
  if (context == nullptr || howitzer == nullptr ||
      !context->isExist(record.attribute.c_str()))
    return false;
  const KR_ObjectID attribute =
      context->searchObject(record.attribute.c_str());
  const int holder =
      HowitzerSubjectState_FindHolder(record.holder.c_str());
  KR_ObjectID commander;
  KR_ObjectID enemy;
  if (!HowitzerSubjectState_AttributeExists(context, attribute) ||
      holder < 0 || howitzer->m_HolderIndex != holder ||
      HowitzerSubjectState_HolderOccupant(record.holder.c_str()) !=
          howitzer->getObjectID() ||
      !ResolveOptionalReference(context, record.commander, IUnknownIID,
                                &commander) ||
      !ResolveOptionalReference(context, record.enemy, IUnitIID, &enemy))
    return false;
  // Restore identity and visual/grounding state without replaying
  // pe_EVCMD_START.  The ordinary start handler immediately executes AI and
  // can fire a new Bullet, which would violate transactional save/load.
  howitzer->restoreHowitzerIdentity(attribute, holder);
  if (howitzer->m_attr == nullptr) return false;
  RemovePrivateEvents(context, howitzer->getObjectID());
  howitzer->m_audibleThisFrame = record.audibleThisFrame;
  howitzer->m_isVisible = record.visible;
  howitzer->m_lastMoveTimeStamp = record.lastMoveTimeStamp;
  howitzer->ct_Subject::setPosition(record.position);
  howitzer->m_damage = record.damage;
  howitzer->m_commanderID = commander;
  howitzer->m_rotateOy = record.rotateOy;
  howitzer->m_hAngle = record.horizontalAngle;
  howitzer->m_enemyID = enemy;
  howitzer->m_lastActionTime = record.lastActionTime;
  howitzer->m_lastEnemyScanTime = record.lastEnemyScanTime;
  howitzer->m_shootThisBastard = record.shootThisBastard != 0;
  howitzer->m_lastShootTime = record.lastShootTime;
  if (!RestoreEvents(context, howitzer->getObjectID(), HOWITZER_FIND_ENEMY,
                     record.findEnemyEvents) ||
      !RestoreEvents(context, howitzer->getObjectID(), HOWITZER_ACTION,
                     record.actionEvents))
    return false;
  return howitzer->m_attr != nullptr &&
         HowitzerSubjectState_HolderOccupant(record.holder.c_str()) ==
             howitzer->getObjectID();
}

bool RestorePendingStartEvent(SimulationContext* context,
                              Howitzer* howitzer,
                              const StableHowitzerRecord& record) {
  if (context == nullptr || howitzer == nullptr ||
      !context->isExist(record.attribute.c_str()))
    return false;
  const KR_ObjectID attribute =
      context->searchObject(record.attribute.c_str());
  KR_ObjectID source;
  if (!HowitzerSubjectState_AttributeExists(context, attribute)) return false;
  if (record.startEvent.sourceKind ==
      StableHowitzerRecord::Event::kDestinationSelf)
    source = howitzer->getObjectID();
  else if (!ResolveOptionalReference(context, record.startEvent.source,
                                     IUnknownIID, &source))
    return false;
  KR_Event start;
  start.label = pe_EVCMD_START;
  start.source = source;
  start.destination = howitzer->getObjectID();
  start.timeStamp = record.startEvent.timeStamp;
  start.data.open(EDO_WRITE)
      .putObjectID(attribute)
      .putStr(record.holder.c_str())
      .putInt(record.startMode)
      .close();
  context->addEvent(start);
  return true;
}

bool ApplyPendingRecord(SimulationContext* context, Howitzer* howitzer,
                        const StableHowitzerRecord& record) {
  if (context == nullptr || howitzer == nullptr ||
      record.lifecycle != StableHowitzerRecord::kPendingStart ||
      howitzer->m_attr != nullptr || !IsNul(howitzer->m_HowitzerAttrID) ||
      !context->isExist(record.attribute.c_str()))
    return false;
  const KR_ObjectID attribute =
      context->searchObject(record.attribute.c_str());
  const int holder = HowitzerSubjectState_FindHolder(record.holder.c_str());
  KR_ObjectID commander;
  if (!HowitzerSubjectState_AttributeExists(context, attribute) || holder < 0 ||
      !IsNul(HowitzerSubjectState_HolderOccupant(record.holder.c_str())) ||
      !ResolveOptionalReference(context, record.commander, IUnknownIID,
                                &commander))
    return false;
  RemovePrivateEvents(context, howitzer->getObjectID());
  howitzer->m_commanderID = commander;
  if (!RestoreEvents(context, howitzer->getObjectID(), HOWITZER_FIND_ENEMY,
                     record.findEnemyEvents) ||
      !RestoreEvents(context, howitzer->getObjectID(), HOWITZER_ACTION,
                     record.actionEvents))
    return false;
  return RestorePendingStartEvent(context, howitzer, record);
}

}  // namespace

void HowitzerActiveWorldState_Link() { HowitzerSubjectState_Link(); }

const char* HowitzerActiveWorldState_LastFailure() {
  return g_lastFailure.c_str();
}

int HowitzerActiveWorldState_LiveCount(SimulationContext* context) {
  HowitzerRoster roster = {};
  return CollectRoster(context, false, &roster)
             ? static_cast<int>(roster.entries.size())
             : -1;
}

int HowitzerActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char>& bytes) {
  std::vector<StableHowitzerRecord> records;
  if (!DecodeRecords(bytes, &records)) return -1;
  int count = 0;
  for (const StableHowitzerRecord& record : records)
    count += static_cast<int>(record.findEnemyEvents.size() +
                              record.actionEvents.size() +
                              (record.lifecycle ==
                                       StableHowitzerRecord::kPendingStart
                                   ? 1u : 0u));
  return count;
}

unsigned long long HowitzerActiveWorldState_Fingerprint(
    SimulationContext* context) {
  std::vector<unsigned char> bytes;
  if (!HowitzerActiveWorldState_CaptureStable(context, &bytes)) return 0;
  unsigned long long hash = kHashOffset;
  for (unsigned char byte : bytes) {
    hash ^= byte;
    hash *= kHashPrime;
  }
  return hash;
}

bool HowitzerActiveWorldState_CaptureStable(
    SimulationContext* context, std::vector<unsigned char>* bytes) {
  g_lastFailure.clear();
  if (bytes == nullptr) return Fail("Howitzer capture output is null");
  HowitzerRoster roster = {};
  if (!CollectRoster(context, false, &roster)) return false;
  std::vector<StableHowitzerRecord> records;
  records.reserve(roster.entries.size());
  for (const HowitzerRosterEntry& entry : roster.entries) {
    StableHowitzerRecord record;
    if (!CaptureRecord(context, entry, &record)) return false;
    records.push_back(record);
  }
  return EncodeRecords(records, bytes);
}

bool HowitzerActiveWorldState_CaptureHolder(
    SimulationContext* context, const char* holderName,
    std::vector<unsigned char>* bytes) {
  g_lastFailure.clear();
  if (context == nullptr || holderName == nullptr || holderName[0] == '\0' ||
      bytes == nullptr)
    return Fail("Howitzer holder capture arguments are invalid");
  const int holderIndex = HowitzerSubjectState_FindHolder(holderName);
  KR_ObjectID occupant = HowitzerSubjectState_HolderOccupant(holderName);
  if (holderIndex < 0 || occupant.isNUL() || !context->isExist(occupant))
    return Fail(std::string("Howitzer holder is not occupied: ") + holderName);
  Howitzer* howitzer = ResolveHowitzer(context, occupant);
  const char* name = context->searchObject(occupant);
  if (howitzer == nullptr || name == nullptr || name[0] == '\0' ||
      howitzer->m_HolderIndex != holderIndex)
    return Fail(std::string("Howitzer holder ownership is invalid: ") +
                holderName);

  // Mission scripts replace holders one by one. Earlier replacement owners in
  // the same transaction can still be between s_New and pe_EVCMD_START, so a
  // local rollback record must not enumerate or validate unrelated owners.
  HowitzerRosterEntry entry;
  entry.holder = holderName;
  entry.name = name;
  entry.howitzer = howitzer;
  entry.ready = true;
  StableHowitzerRecord record;
  if (!CaptureRecord(context, entry, &record)) return false;
  std::vector<StableHowitzerRecord> records(1, record);
  return EncodeRecords(records, bytes);
}

bool HowitzerActiveWorldState_ValidateStable(
    const std::vector<unsigned char>& bytes) {
  std::vector<StableHowitzerRecord> records;
  return DecodeRecords(bytes, &records);
}

bool HowitzerActiveWorldState_ProbeLegacyVersionCompatibility(
    SimulationContext* context) {
  g_lastFailure.clear();
  HowitzerRoster roster = {};
  std::vector<StableHowitzerRecord> records;
  if (!CollectRoster(context, true, &roster)) return false;
  records.reserve(roster.entries.size());
  for (const HowitzerRosterEntry& entry : roster.entries) {
    StableHowitzerRecord record;
    if (!CaptureRecord(context, entry, &record)) return false;
    records.push_back(record);
  }
  std::vector<unsigned char> version1;
  std::vector<unsigned char> version2;
  std::vector<unsigned char> version3;
  return EncodeRecordsVersion(records, kHowitzerLegacyVersion, &version1) &&
         EncodeRecordsVersion(records, kHowitzerSourceKindVersion,
                              &version2) &&
         EncodeRecordsVersion(records, kHowitzerVersion, &version3) &&
         version1 != version2 &&
         version2 != version3 &&
         HowitzerActiveWorldState_ValidateStable(version1) &&
         HowitzerActiveWorldState_ValidateStable(version2) &&
         HowitzerActiveWorldState_ValidateStable(version3) &&
         HowitzerActiveWorldState_MatchesStable(context, version1) &&
         HowitzerActiveWorldState_MatchesStable(context, version2) &&
         HowitzerActiveWorldState_MatchesStable(context, version3);
}

bool HowitzerActiveWorldState_MatchesStable(
    SimulationContext* context, const std::vector<unsigned char>& bytes) {
  std::vector<unsigned char> current;
  if (!HowitzerActiveWorldState_ValidateStable(bytes) ||
      !HowitzerActiveWorldState_CaptureStable(context, &current))
    return false;
  if (current == bytes) return true;
  std::vector<StableHowitzerRecord> expectedRecords;
  std::vector<StableHowitzerRecord> currentRecords;
  if (!DecodeRecords(bytes, &expectedRecords) ||
      !DecodeRecords(current, &currentRecords))
    return Fail("Howitzer stable payload changed encoding");
  if (expectedRecords.size() != currentRecords.size())
    return Fail("Howitzer stable roster count changed");
  for (std::size_t index = 0; index < expectedRecords.size(); ++index) {
    const std::string difference =
        FirstRecordDifference(expectedRecords[index], currentRecords[index]);
    if (!difference.empty())
      return Fail(std::string("Howitzer stable mismatch for ") +
                  expectedRecords[index].holder + ": " + difference);
  }
  // HWZ1 v1 stores a self-scheduled event by the owner's symbolic name;
  // v2 stores the destination-self relation explicitly.  A freshly captured
  // v2 payload is therefore byte-different after restoring a legacy save even
  // when every decoded runtime field is identical.
  return true;
}

bool HowitzerActiveWorldState_CollectStableOwners(
    SimulationContext* context, const std::vector<unsigned char>& bytes,
    std::vector<KR_ObjectID>* owners) {
  std::vector<StableHowitzerRecord> records;
  HowitzerRoster roster = {};
  if (context == nullptr || owners == nullptr || !owners->empty() ||
      !DecodeRecords(bytes, &records) ||
      !CollectRoster(context, false, &roster) ||
      !RosterMatches(roster, records))
    return false;
  for (const HowitzerRosterEntry& entry : roster.entries)
    owners->push_back(entry.howitzer->getObjectID());
  return true;
}

bool HowitzerActiveWorldState_CreateStableOwners(
    SimulationContext* context, const std::vector<unsigned char>& bytes,
    std::vector<KR_ObjectID>* created) {
  g_lastFailure.clear();
  std::vector<StableHowitzerRecord> records;
  HowitzerRoster roster = {};
  if (context == nullptr || created == nullptr || !created->empty() ||
      !DecodeRecords(bytes, &records) ||
      !CollectRoster(context, false, &roster))
    return Fail("Howitzer owner allocation preflight failed");
  if (!roster.entries.empty())
    return RosterMatches(roster, records) ||
           Fail("Howitzer live roster differs from the saved roster");
  if (records.size() >
      static_cast<std::size_t>(HowitzerSubjectState_SubjectCapacity()))
    return Fail("Howitzer subject table has insufficient capacity");
  for (const StableHowitzerRecord& record : records) {
    if (!context->isExist(record.attribute.c_str()) ||
        !HowitzerSubjectState_AttributeExists(
            context, context->searchObject(record.attribute.c_str())))
      return Fail("HowitzerAttr symbolic dependency is unavailable: " +
                  record.attribute);
    const int holder = HowitzerSubjectState_FindHolder(record.holder.c_str());
    if (holder < 0 ||
        holder >= HowitzerSubjectState_SupportedHolderCount() ||
        !IsNul(HowitzerSubjectState_HolderOccupant(
            record.holder.c_str())))
      return Fail("Howitzer holder is unavailable: " + record.holder);
  }
  for (const StableHowitzerRecord& record : records) {
    KR_ObjectID object =
        g_arena.newObject("Howitzer", record.name.c_str());
    if (IsNul(object) ||
        !HowitzerSubjectState_PrepareNewObject(context, object)) {
      if (!IsNul(object) && context->isExist(object))
        context->removeObject(object);
      HowitzerActiveWorldState_RemoveStableOwners(context, created);
      return Fail("Howitzer owner allocation failed: " + record.name);
    }
    Howitzer* howitzer = ResolveHowitzer(context, object);
    const int holder = HowitzerSubjectState_FindHolder(record.holder.c_str());
    if (howitzer == nullptr ||
        (record.lifecycle == StableHowitzerRecord::kReady &&
         g_super.m_level.AttachToHowitzerHolder(holder, object) != holder)) {
      context->removeObject(object);
      HowitzerActiveWorldState_RemoveStableOwners(context, created);
      return Fail("Howitzer owner preparation failed: " + record.holder);
    }
    if (record.lifecycle == StableHowitzerRecord::kReady)
      howitzer->m_HolderIndex = holder;
    created->push_back(object);
    if (record.lifecycle == StableHowitzerRecord::kPendingStart &&
        !RestorePendingStartEvent(context, howitzer, record)) {
      HowitzerActiveWorldState_RemoveStableOwners(context, created);
      return Fail("Howitzer pending START reconstruction failed: " +
                  record.name);
    }
  }
  return true;
}

bool HowitzerActiveWorldState_ApplyStableReferences(
    SimulationContext* context, const std::vector<unsigned char>& bytes) {
  std::vector<StableHowitzerRecord> records;
  HowitzerRoster roster = {};
  if (context == nullptr || !DecodeRecords(bytes, &records) ||
      !CollectRoster(context, false, &roster) ||
      !RosterMatches(roster, records))
    return false;
  for (std::size_t index = 0; index < records.size(); ++index)
    if (!(records[index].lifecycle == StableHowitzerRecord::kReady
              ? ApplyRecord(context, roster.entries[index].howitzer,
                            records[index])
              : ApplyPendingRecord(context, roster.entries[index].howitzer,
                                   records[index])))
      return Fail("Howitzer symbolic reconstruction failed: " +
                  records[index].holder);
  return HowitzerActiveWorldState_MatchesStable(context, bytes);
}

bool HowitzerActiveWorldState_RestoreHolder(
    SimulationContext* context, const std::vector<unsigned char>& bytes) {
  g_lastFailure.clear();
  std::vector<StableHowitzerRecord> records;
  if (context == nullptr || !DecodeRecords(bytes, &records) ||
      records.size() != 1u)
    return Fail("Howitzer holder restore payload is invalid");
  const StableHowitzerRecord& record = records[0];
  if (context->isExist(record.name.c_str()))
    return Fail("Howitzer holder restore name is already occupied: " +
                record.name);
  const int holder =
      HowitzerSubjectState_FindHolder(record.holder.c_str());
  if (holder < 0 ||
      holder >= HowitzerSubjectState_SupportedHolderCount() ||
      !IsNul(HowitzerSubjectState_HolderOccupant(record.holder.c_str())))
    return Fail("Howitzer holder restore target is unavailable: " +
                record.holder);
  if (!context->isExist(record.attribute.c_str()) ||
      !HowitzerSubjectState_AttributeExists(
          context, context->searchObject(record.attribute.c_str())))
    return Fail("Howitzer holder restore attribute is unavailable: " +
                record.attribute);

  KR_ObjectID object = g_arena.newObject("Howitzer", record.name.c_str());
  if (IsNul(object) ||
      !HowitzerSubjectState_PrepareNewObject(context, object)) {
    if (!IsNul(object) && context->isExist(object))
      context->removeObject(object);
    return Fail("Howitzer holder restore allocation failed: " + record.name);
  }
  Howitzer* howitzer = ResolveHowitzer(context, object);
  if (howitzer == nullptr ||
      g_super.m_level.AttachToHowitzerHolder(holder, object) != holder) {
    context->removeObject(object);
    return Fail("Howitzer holder restore reservation failed: " +
                record.holder);
  }
  howitzer->m_HolderIndex = holder;
  if (!ApplyRecord(context, howitzer, record)) {
    RemovePrivateEvents(context, object);
    if (context->isExist(object)) context->removeObject(object);
    return Fail("Howitzer holder restore references failed: " +
                record.holder);
  }
  return true;
}

void HowitzerActiveWorldState_RemoveStableOwners(
    SimulationContext* context, std::vector<KR_ObjectID>* created) {
  if (created == nullptr) return;
  if (context != nullptr)
    for (std::vector<KR_ObjectID>::reverse_iterator object =
             created->rbegin();
         object != created->rend(); ++object) {
      RemovePrivateEvents(context, *object);
      if (context->isExist(*object)) context->removeObject(*object);
    }
  created->clear();
}
