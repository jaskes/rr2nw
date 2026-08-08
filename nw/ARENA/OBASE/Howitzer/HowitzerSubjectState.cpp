#include "HowitzerSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "zav.h"
#include "i/dynobj.i"
#include "i/unit.i"
#include "storage/h/savefile.h"
#include "storage/h/strgdefs.h"
#include "storage/h/subject.h"
#include "../DynObj/DynObj.h"
#include "Howitzer.h"
#include "RecoveredModRuntime.h"
#include "kernel/h/context.h"
#include "message/peopmsg.h"
#include "obase/skin/SkinResourceState.h"
#include "storage/h/subject.h"
#include "super.h"

namespace {

constexpr unsigned long long kHashOffset = 14695981039346656037ull;
constexpr unsigned long long kHashPrime = 1099511628211ull;

int g_attributeCapacity = 0;
int g_subjectCapacity = 0;
char g_lastError[256] = {};
std::vector<HowitzerPlace> g_holderCatalog;

// This reference makes the static-library linker retain Howitzer.cpp, whose
// static class/attribute table objects own the actual registrations.
// Method pointers inherited from the old interfaces can be represented only
// as vtable slots and therefore create no COFF relocation to Howitzer.cpp.
// This volatile false gate emits a real constructor reference without ever
// constructing a dummy runtime object; that retains the translation unit and
// its static Howitzer/HowitzerAttr registrations in every final executable.
volatile int g_forceHowitzerTranslationUnit = 0;

struct ImmediateStartContext {
  SimulationContext* context;
  double boundary;
  bool valid;
};

bool ActivateImmediateStart(KR_ObjectID object, void* user) {
  ImmediateStartContext* activation =
      static_cast<ImmediateStartContext*>(user);
  if (activation == nullptr || activation->context == nullptr)
    return false;
  Howitzer* howitzer = static_cast<Howitzer*>(
      activation->context->queryInterface(object, IUnknownIID));
  if (howitzer == nullptr) {
    activation->valid = false;
    return false;
  }
  if (howitzer->m_attr != nullptr && !howitzer->m_HowitzerAttrID.isNUL())
    return true;

  KR_Event events[2];
  const int count = activation->context->copyEventsTo(
      pe_EVCMD_START, object, events, 2);
  if (count != 1 || !std::isfinite(events[0].timeStamp)) {
    activation->valid = false;
    return false;
  }
  // Only the immediate marker belongs to the completed admission boundary.
  // A real positive authored future start remains queued for the scheduler.
  if (events[0].timeStamp > activation->boundary) return true;
  if (activation->context->removeEventsTo(pe_EVCMD_START, object) != 1) {
    activation->valid = false;
    return false;
  }
  activation->context->sendEventNow(events[0]);
  const char* holder = HowitzerSubjectState_HolderName(
      howitzer->m_HolderIndex);
  if (howitzer->m_attr == nullptr || howitzer->m_HowitzerAttrID.isNUL() ||
      holder == nullptr ||
      HowitzerSubjectState_HolderOccupant(holder) != object) {
    activation->valid = false;
    return false;
  }
  return true;
}

void SetError(const char* message) {
  std::snprintf(g_lastError, sizeof(g_lastError), "%s",
                message == nullptr ? "Howitzer lifecycle failed" : message);
}

void HashBytes(unsigned long long* hash, const void* data, std::size_t size) {
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

void HashString(unsigned long long* hash, const char* value) {
  HashBytes(hash, value, std::strlen(value) + 1u);
}

bool CountObject(KR_ObjectID, void* user) {
  ++*static_cast<int*>(user);
  return true;
}

bool CollectName(KR_ObjectID object, void* user) {
  std::vector<KR_ObjectID>* objects =
      static_cast<std::vector<KR_ObjectID>*>(user);
  objects->push_back(object);
  return true;
}

struct ReadyCountContext {
  SimulationContext* context;
  int count;
};

bool CountReadyHowitzer(KR_ObjectID object, void* user) {
  ReadyCountContext* probe = static_cast<ReadyCountContext*>(user);
  Howitzer* howitzer = static_cast<Howitzer*>(
      probe->context->queryInterface(object, IUnknownIID));
  if (howitzer != nullptr && howitzer->m_attr != nullptr &&
      howitzer->m_HolderIndex >= 0 &&
      howitzer->m_HolderIndex < static_cast<int>(g_holderCatalog.size()) &&
      g_holderCatalog[howitzer->m_HolderIndex].occupant == object)
    ++probe->count;
  return true;
}

bool IsTransactionObject(const KR_ObjectID& object,
                         const std::vector<KR_ObjectID>& objects) {
  return std::find(objects.begin(), objects.end(), object) != objects.end();
}

}  // namespace

void HowitzerSubjectState_Link() {
  if (g_forceHowitzerTranslationUnit != 0) {
    Howitzer* anchor = new Howitzer;
    delete anchor;
  }
}

void HowitzerSubjectState_SetExpectedCapacities(int attributeCapacity,
                                                int subjectCapacity) {
  g_attributeCapacity = attributeCapacity > 0 ? attributeCapacity : 0;
  g_subjectCapacity = subjectCapacity > 0 ? subjectCapacity : 0;
}

bool HowitzerSubjectState_TableReady(SimulationContext* context) {
  if (context == nullptr || g_arena.getContext() != context)
    return false;
  const bool attributeTable =
      g_arena.searchSeanceClassTable("HowitzerAttr") != ct_NULLID;
  const bool subjectTable =
      g_arena.searchSeanceClassTable("Howitzer") != ct_NULLID;
  if (g_attributeCapacity == 0 && g_subjectCapacity == 0)
    return !attributeTable && !subjectTable;
  return g_attributeCapacity > 0 && g_subjectCapacity > 0 &&
         attributeTable && subjectTable;
}

int HowitzerSubjectState_AttributeCapacity() { return g_attributeCapacity; }
int HowitzerSubjectState_SubjectCapacity() { return g_subjectCapacity; }

int HowitzerSubjectState_AttributeCount() {
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("HowitzerAttr");
  if (table == ct_NULLID) return 0;
  int count = 0;
  g_arena.userFind(table, CountObject, &count);
  return count;
}

int HowitzerSubjectState_LiveCount() {
  const ct_ClassTableID table = g_arena.searchSeanceClassTable("Howitzer");
  if (table == ct_NULLID) return 0;
  int count = 0;
  g_arena.userFind(table, CountObject, &count);
  return count;
}

int HowitzerSubjectState_ReadyLiveCount(SimulationContext* context) {
  const ct_ClassTableID table = g_arena.searchSeanceClassTable("Howitzer");
  if (context == nullptr || g_arena.getContext() != context ||
      table == ct_NULLID)
    return 0;
  ReadyCountContext probe = {context, 0};
  g_arena.userFind(table, CountReadyHowitzer, &probe);
  return probe.count;
}

int HowitzerSubjectState_OccupiedHolderCount() {
  int count = 0;
  for (HowitzerPlace& holder : g_holderCatalog)
    if (!holder.occupant.isNUL()) ++count;
  return count;
}

int HowitzerSubjectState_SupportedHolderCount() {
  return static_cast<int>(g_holderCatalog.size());
}

bool HowitzerSubjectState_ActivateImmediateStarts(
    SimulationContext* context, double boundary) {
  const ct_ClassTableID table = g_arena.searchSeanceClassTable("Howitzer");
  if (context == nullptr || g_arena.getContext() != context ||
      table == ct_NULLID || !std::isfinite(boundary))
    return g_subjectCapacity == 0 && table == ct_NULLID;
  ImmediateStartContext activation = {context, boundary, true};
  g_arena.userFind(table, ActivateImmediateStart, &activation);
  return activation.valid;
}

bool HowitzerSubjectState_ResolveReferences(SimulationContext* context,
                                            double timeStamp) {
  if (!HowitzerSubjectState_TableReady(context)) return false;
  if (g_attributeCapacity == 0) return true;
  // Source-only hermetic fixtures intentionally publish an empty Skin table.
  // There is no model graph to resolve in that mode; real retail Levels all
  // have models and therefore take the exact archival update path below.
  if (SkinResourceState_ModelCount(context) == 0) return true;
  ct_ClassTable* table = ct_Storage::searchClassTable("HowitzerAttr");
  if (table == nullptr || table->objectsType() != OBJECT_ATTRIBUTE)
    return false;
  static_cast<ct_AttributeTable*>(table)->update(timeStamp);
  return true;
}

bool HowitzerSubjectState_PrepareNewObject(SimulationContext* context,
                                           const KR_ObjectID& object) {
  if (context == nullptr || !context->isExist(object)) return false;
  Howitzer* howitzer = static_cast<Howitzer*>(
      context->queryInterface(object, IUnknownIID));
  if (howitzer == nullptr) return false;
  howitzer->m_HowitzerAttrID = KR_ObjectID::NUL();
  // The January destructor unconditionally releases this index. Slot zero is
  // a safe unattached sentinel until the start event installs the real holder;
  // the mod-aware holder catalog always initializes every occupant to NUL.
  howitzer->m_HolderIndex = 0;
  howitzer->m_damage = 0.0;
  howitzer->m_commanderID = KR_ObjectID::NUL();
  howitzer->m_rotateOy = 0.0;
  howitzer->m_hAngle = 0.0;
  howitzer->m_enemyID = KR_ObjectID::NUL();
  howitzer->m_lastActionTime = 0.0;
  howitzer->m_lastEnemyScanTime = 0.0;
  howitzer->m_shootThisBastard = false;
  howitzer->m_lastShootTime = 0.0;
  howitzer->m_attr = nullptr;
  howitzer->m_audibleThisFrame = 0;
  howitzer->m_isVisible = 0;
  howitzer->m_lastMoveTimeStamp = 0.0;
  return true;
}

bool HowitzerSubjectState_AttributeExists(SimulationContext* context,
                                          const KR_ObjectID& object) {
  KR_ObjectID candidate = object;
  if (context == nullptr || g_arena.getContext() != context ||
      candidate.isNUL() || !context->isExist(object))
    return false;
  ct_ClassTable* table = ct_Storage::searchClassTable("HowitzerAttr");
  return table != nullptr && table->objectsType() == OBJECT_ATTRIBUTE &&
         static_cast<ct_AttributeTable*>(table)->searchAttribute(object) !=
             nullptr;
}

unsigned long long HowitzerSubjectState_Fingerprint(
    SimulationContext* context) {
  if (!HowitzerSubjectState_TableReady(context)) return 0;
  std::vector<KR_ObjectID> attributes;
  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("HowitzerAttr");
  if (table != ct_NULLID)
    g_arena.userFind(table, CollectName, &attributes);
  std::sort(attributes.begin(), attributes.end(),
            [](const KR_ObjectID& left, const KR_ObjectID& right) {
              return left.id < right.id;
            });
  unsigned long long hash = kHashOffset;
  HashString(&hash, "Howitzer");
  HashBytes(&hash, &g_attributeCapacity, sizeof(g_attributeCapacity));
  HashBytes(&hash, &g_subjectCapacity, sizeof(g_subjectCapacity));
  for (const KR_ObjectID& object : attributes) {
    const char* name = context->searchObject(object);
    if (name == nullptr || name[0] == '\0') return 0;
    HashString(&hash, name);
  }
  for (const HowitzerPlace& holder : g_holderCatalog) {
    HashString(&hash, holder.symbolic);
    HashBytes(&hash, &holder.pos.x, sizeof(holder.pos.x));
    HashBytes(&hash, &holder.pos.y, sizeof(holder.pos.y));
    HashBytes(&hash, &holder.pos.z, sizeof(holder.pos.z));
  }
  return hash;
}

bool HowitzerSubjectState_LoadHolders() {
  g_lastError[0] = '\0';
  FILE* file = RecoveredModRuntime_OpenRead("Howitzers.hwz", nullptr);
  if (file == nullptr) {
    SetError("Howitzers.hwz is unavailable");
    return false;
  }
  std::vector<HowitzerPlace> holders;
  char line[512] = {};
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    char* current = line;
    while (*current == ' ' || *current == '\t') ++current;
    if (*current == '\0' || *current == '\r' || *current == '\n' ||
        *current == '#' || (*current == '/' && current[1] == '*'))
      continue;
    char name[HOWITZER_MAX_NAME] = {};
    CFVector3 position;
    // The retail loader counted the three coordinates as a complete record
    // before matching the closing bracket.  Level.01D/N intentionally ship
    // one such line without a trailing ']'; retaining that tolerance is part
    // of the data format, while the bounded name and finite checks below keep
    // the modern loader safe.
    const int parsed = std::sscanf(current,
        "%39s [ %lf , %lf , %lf", name, &position.x, &position.y,
        &position.z);
    if (parsed != 4 || !std::isfinite(position.x) ||
        !std::isfinite(position.y) || !std::isfinite(position.z) ||
        holders.size() >= 1024u) {
      std::fclose(file);
      SetError("Howitzers.hwz contains an invalid holder record");
      return false;
    }
    for (const HowitzerPlace& holder : holders) {
      if (_stricmp(holder.symbolic, name) == 0) {
        std::fclose(file);
        SetError("Howitzers.hwz contains a duplicate holder name");
        return false;
      }
    }
    HowitzerPlace holder = {};
    std::snprintf(holder.symbolic, sizeof(holder.symbolic), "%s", name);
    holder.pos = position;
    holder.occupant = KR_ObjectID::NUL();
    holders.push_back(holder);
  }
  const bool readError = std::ferror(file) != 0;
  std::fclose(file);
  if (readError) {
    SetError("Howitzers.hwz could not be read completely");
    return false;
  }
  g_holderCatalog.swap(holders);
  g_super.m_level.m_howitzersLoaded = static_cast<int>(
      std::min<std::size_t>(g_holderCatalog.size(), HOWITZER_MAX_LEVEL));
  for (int index = 0; index < g_super.m_level.m_howitzersLoaded; ++index)
    g_super.m_level.m_howitzerPool[index] = g_holderCatalog[index];
  return true;
}

void HowitzerSubjectState_ReleaseHolders() {
  for (int index = 0; index < g_super.m_level.m_howitzersLoaded; ++index)
    g_super.m_level.m_howitzerPool[index].occupant = KR_ObjectID::NUL();
  g_super.m_level.m_howitzersLoaded = 0;
  g_holderCatalog.clear();
  g_attributeCapacity = 0;
  g_subjectCapacity = 0;
  g_lastError[0] = '\0';
}

int HowitzerSubjectState_HolderCount() {
  return static_cast<int>(g_holderCatalog.size());
}

int HowitzerSubjectState_FindHolder(const char* name) {
  if (name == nullptr || name[0] == '\0') return -1;
  for (std::size_t index = 0; index < g_holderCatalog.size(); ++index) {
    if (std::strcmp(g_holderCatalog[index].symbolic, name) == 0)
      return index;
  }
  return -1;
}

const char* HowitzerSubjectState_HolderName(int index) {
  return index < 0 || index >= static_cast<int>(g_holderCatalog.size())
             ? nullptr : g_holderCatalog[index].symbolic;
}

bool HowitzerSubjectState_HolderPosition(int index, double* x, double* y,
                                         double* z) {
  if (index < 0 || index >= static_cast<int>(g_holderCatalog.size()) ||
      x == nullptr || y == nullptr || z == nullptr)
    return false;
  *x = g_holderCatalog[index].pos.x;
  *y = g_holderCatalog[index].pos.y;
  *z = g_holderCatalog[index].pos.z;
  return true;
}

KR_ObjectID HowitzerSubjectState_HolderOccupant(const char* name) {
  const int index = HowitzerSubjectState_FindHolder(name);
  return index < 0 ? KR_ObjectID::NUL() : g_holderCatalog[index].occupant;
}

bool HowitzerSubjectState_DeleteHolderOccupant(
    SimulationContext* context, const char* name,
    const std::vector<KR_ObjectID>& transactionObjects,
    bool transactionActive) {
  g_lastError[0] = '\0';
  if (context == nullptr || g_arena.getContext() != context) {
    SetError("Howitzer holder deletion has no active context");
    return false;
  }
  const int index = HowitzerSubjectState_FindHolder(name);
  if (index < 0) {
    SetError("Howitzer holder name is not present in Howitzers.hwz");
    return false;
  }
  KR_ObjectID occupant = g_holderCatalog[index].occupant;
  if (occupant.isNUL()) return true;
  if (!context->isExist(occupant)) {
    g_holderCatalog[index].occupant = KR_ObjectID::NUL();
    if (index < g_super.m_level.m_howitzersLoaded)
      g_super.m_level.m_howitzerPool[index].occupant = KR_ObjectID::NUL();
    return true;
  }
  if (transactionActive &&
      !IsTransactionObject(occupant, transactionObjects)) {
    SetError("mission attempted to replace a pre-existing Howitzer");
    return false;
  }
  context->removeObject(occupant);
  if (!g_holderCatalog[index].occupant.isNUL()) {
    SetError("Howitzer removal did not release its holder");
    return false;
  }
  return true;
}

const char* HowitzerSubjectState_LastError() { return g_lastError; }

void ol_Level::ReleaseHolder(int index, KR_ObjectID occupant) {
  if (index < 0 || index >= static_cast<int>(g_holderCatalog.size())) return;
  if (g_holderCatalog[index].occupant != occupant) return;
  g_holderCatalog[index].occupant = KR_ObjectID::NUL();
  if (index < m_howitzersLoaded)
    m_howitzerPool[index].occupant = KR_ObjectID::NUL();
}

int ol_Level::AttachToHowitzerHolder(int index, KR_ObjectID occupant) {
  if (index < 0 || index >= static_cast<int>(g_holderCatalog.size()))
    return -1;
  if (!g_holderCatalog[index].occupant.isNUL() &&
      g_holderCatalog[index].occupant != occupant)
    return -1;
  g_holderCatalog[index].occupant = occupant;
  if (index < m_howitzersLoaded)
    m_howitzerPool[index].occupant = occupant;
  return index;
}

int ol_Level::AttachToHowitzerHolder(const char* holderName,
                                     KR_ObjectID occupant) {
  const int index = HowitzerSubjectState_FindHolder(holderName);
  if (index < 0) return -1;
  KR_ObjectID previous = g_holderCatalog[index].occupant;
  if (!previous.isNUL() && previous != occupant && context != nullptr &&
      context->isExist(previous))
    context->removeObject(previous);
  g_holderCatalog[index].occupant = occupant;
  if (index < m_howitzersLoaded)
    m_howitzerPool[index].occupant = occupant;
  return index;
}
