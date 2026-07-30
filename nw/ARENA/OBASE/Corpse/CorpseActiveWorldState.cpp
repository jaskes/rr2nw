#include "CorpseActiveWorldState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "Corpse.h"
#include "CorpseAttributeState.h"
#include "CorpseSubjectState.h"
#include "kernel/h/context.h"
#include "message/corpsemsg.h"
#include "message/fountmsg.h"
#include "message/smokermsg.h"
#include "obase/smoke/SMOKER.H"
#include "obase/smoke/SmokeActiveWorldState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokerSubjectState.h"

namespace {

const std::uint32_t kMagic = 0x31524f43u;  // COR1
const std::uint32_t kVersion = 1u;
const std::size_t kMaximumCorpses = 4096;
const std::size_t kMaximumString = MAX_SYMBOLIC_LENGHT - 1;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

enum ChildRole
{
    kSmokeRole = 1,
    kFireRole = 2
};

struct StableSmoker
{
    int role;
    std::string name;
    std::string attribute;
    CFVector3 position;
    int count;
    double startTime;
    int visible;
    double brightness;
    int hasMove;
    double moveTime;
    int hasRemove;
    double removeTime;

    StableSmoker()
        : role(0), position(0.0, 0.0, 0.0), count(0), startTime(0.0),
          visible(0), brightness(0.0), hasMove(0), moveTime(0.0),
          hasRemove(0), removeTime(0.0) {}
};

struct StableCorpse
{
    std::string name;
    std::string attribute;
    CFVector3 position;
    int visible;
    int mustDieNow;
    int hasDeath;
    double deathTime;
    std::vector<StableSmoker> children;

    StableCorpse()
        : position(0.0, 0.0, 0.0), visible(0), mustDieNow(0),
          hasDeath(0), deathTime(0.0) {}
};

std::string g_failure;

bool Fail(const std::string &message)
{
    g_failure = message;
    return false;
}

bool IsNul(const KR_ObjectID &object)
{
    KR_ObjectID copy = object;
    return copy.isNUL() != 0;
}

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool NearlyEqual(double left, double right)
{
    const double scale = 1.0 + std::fabs(left) + std::fabs(right);
    return std::fabs(left - right) <= 1.0e-10 * scale;
}

std::string ObjectName(SimulationContext *context,
                       const KR_ObjectID &object)
{
    const char *name = context == NULL || IsNul(object)
        ? NULL : context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

void HashBytes(unsigned long long *hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int index = 0; index < size; ++index)
    {
        *hash ^= bytes[index];
        *hash *= kHashPrime;
    }
}

struct Writer
{
    std::vector<unsigned char> *bytes;

    void U32(std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            bytes->push_back(static_cast<unsigned char>(value >> shift));
    }

    void Double(double value)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        for (int shift = 0; shift < 64; shift += 8)
            bytes->push_back(static_cast<unsigned char>(bits >> shift));
    }

    bool String(const std::string &value)
    {
        if (value.empty() || value.size() > kMaximumString ||
            value.find('\0') != std::string::npos)
            return false;
        U32(static_cast<std::uint32_t>(value.size()));
        bytes->insert(bytes->end(), value.begin(), value.end());
        return true;
    }
};

struct Reader
{
    const std::vector<unsigned char> &bytes;
    std::size_t offset;

    explicit Reader(const std::vector<unsigned char> &source)
        : bytes(source), offset(0) {}

    bool U32(std::uint32_t *value)
    {
        if (value == NULL || offset > bytes.size() ||
            bytes.size() - offset < 4)
            return false;
        *value = 0;
        for (int shift = 0; shift < 32; shift += 8)
            *value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
        return true;
    }

    bool Double(double *value)
    {
        if (value == NULL || offset > bytes.size() ||
            bytes.size() - offset < 8)
            return false;
        std::uint64_t bits = 0;
        for (int shift = 0; shift < 64; shift += 8)
            bits |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
        std::memcpy(value, &bits, sizeof(bits));
        return true;
    }

    bool String(std::string *value)
    {
        std::uint32_t size = 0;
        if (value == NULL || !U32(&size) || size == 0 ||
            size > kMaximumString || offset > bytes.size() ||
            bytes.size() - offset < size)
            return false;
        value->assign(reinterpret_cast<const char *>(&bytes[offset]), size);
        offset += size;
        return value->find('\0') == std::string::npos;
    }
};

bool ValidFlag(int value)
{
    return value == 0 || value == 1;
}

bool ValidateChild(const StableSmoker &child)
{
    if ((child.role != kSmokeRole && child.role != kFireRole) ||
        child.name.empty() || child.attribute.empty() ||
        child.name.size() > kMaximumString ||
        child.attribute.size() > kMaximumString ||
        !FiniteVector(child.position) || child.count < 0 ||
        !std::isfinite(child.startTime) || child.startTime < 0.0 ||
        !ValidFlag(child.visible) || !std::isfinite(child.brightness) ||
        child.brightness < 0.0 || !ValidFlag(child.hasMove) ||
        !ValidFlag(child.hasRemove))
        return Fail("COR1 owned DynSmoker state is invalid");
    if ((child.hasMove && (!std::isfinite(child.moveTime) ||
                           child.moveTime < 0.1)) ||
        (!child.hasMove && child.moveTime != 0.0) ||
        (child.hasRemove && (!std::isfinite(child.removeTime) ||
                             child.removeTime < child.startTime)) ||
        (!child.hasRemove && child.removeTime != 0.0))
        return Fail("COR1 owned DynSmoker event state is invalid");
    return true;
}

bool ValidateRecord(const StableCorpse &record)
{
    if (record.name.empty() || record.attribute.empty() ||
        record.name.size() > kMaximumString ||
        record.attribute.size() > kMaximumString ||
        !FiniteVector(record.position) || !ValidFlag(record.visible) ||
        !ValidFlag(record.mustDieNow) || !ValidFlag(record.hasDeath) ||
        record.children.size() > 2)
        return Fail("COR1 Corpse state is invalid");
    if (record.hasDeath != (record.mustDieNow ? 0 : 1) ||
        (record.mustDieNow && !record.visible) ||
        (record.hasDeath && (!std::isfinite(record.deathTime) ||
                             record.deathTime < 0.1)) ||
        (!record.hasDeath && record.deathTime != 0.0))
        return Fail("COR1 Corpse death boundary is invalid");
    int previousRole = 0;
    for (std::size_t index = 0; index < record.children.size(); ++index)
    {
        if (!ValidateChild(record.children[index]) ||
            record.children[index].role <= previousRole)
            return Fail("COR1 owned DynSmoker roles are not canonical");
        previousRole = record.children[index].role;
    }
    return true;
}

bool Encode(const std::vector<StableCorpse> &records,
            std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumCorpses)
        return false;
    bytes->clear();
    Writer writer = {bytes};
    writer.U32(kMagic);
    writer.U32(kVersion);
    writer.U32(static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const StableCorpse &record = records[index];
        if (!ValidateRecord(record) ||
            (index != 0 && records[index - 1].name > record.name) ||
            !writer.String(record.name) || !writer.String(record.attribute))
            return Fail("COR1 record encoding failed");
        writer.Double(record.position.x);
        writer.Double(record.position.y);
        writer.Double(record.position.z);
        writer.U32(record.visible);
        writer.U32(record.mustDieNow);
        writer.U32(record.hasDeath);
        writer.Double(record.deathTime);
        writer.U32(static_cast<std::uint32_t>(record.children.size()));
        for (std::size_t childIndex = 0;
             childIndex < record.children.size(); ++childIndex)
        {
            const StableSmoker &child = record.children[childIndex];
            writer.U32(static_cast<std::uint32_t>(child.role));
            if (!writer.String(child.name) ||
                !writer.String(child.attribute))
                return Fail("COR1 child identity encoding failed");
            writer.Double(child.position.x);
            writer.Double(child.position.y);
            writer.Double(child.position.z);
            writer.U32(static_cast<std::uint32_t>(child.count));
            writer.Double(child.startTime);
            writer.U32(child.visible);
            writer.Double(child.brightness);
            writer.U32(child.hasMove);
            writer.Double(child.moveTime);
            writer.U32(child.hasRemove);
            writer.Double(child.removeTime);
        }
    }
    return true;
}

bool Decode(const std::vector<unsigned char> &bytes,
            std::vector<StableCorpse> *records)
{
    if (records == NULL)
        return false;
    Reader reader(bytes);
    std::uint32_t magic = 0, version = 0, count = 0;
    if (!reader.U32(&magic) || !reader.U32(&version) ||
        !reader.U32(&count) || magic != kMagic || version != kVersion ||
        count > kMaximumCorpses)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableCorpse record;
        std::uint32_t visible = 0, mustDie = 0, hasDeath = 0;
        std::uint32_t childCount = 0;
        if (!reader.String(&record.name) ||
            !reader.String(&record.attribute) ||
            !reader.Double(&record.position.x) ||
            !reader.Double(&record.position.y) ||
            !reader.Double(&record.position.z) || !reader.U32(&visible) ||
            !reader.U32(&mustDie) || !reader.U32(&hasDeath) ||
            !reader.Double(&record.deathTime) ||
            !reader.U32(&childCount) || childCount > 2)
            return false;
        record.visible = static_cast<int>(visible);
        record.mustDieNow = static_cast<int>(mustDie);
        record.hasDeath = static_cast<int>(hasDeath);
        for (std::uint32_t childIndex = 0;
             childIndex < childCount; ++childIndex)
        {
            StableSmoker child;
            std::uint32_t role = 0, countValue = 0, childVisible = 0;
            std::uint32_t hasMove = 0, hasRemove = 0;
            if (!reader.U32(&role) || !reader.String(&child.name) ||
                !reader.String(&child.attribute) ||
                !reader.Double(&child.position.x) ||
                !reader.Double(&child.position.y) ||
                !reader.Double(&child.position.z) ||
                !reader.U32(&countValue) ||
                !reader.Double(&child.startTime) ||
                !reader.U32(&childVisible) ||
                !reader.Double(&child.brightness) ||
                !reader.U32(&hasMove) ||
                !reader.Double(&child.moveTime) ||
                !reader.U32(&hasRemove) ||
                !reader.Double(&child.removeTime))
                return false;
            child.role = static_cast<int>(role);
            child.count = static_cast<int>(countValue);
            child.visible = static_cast<int>(childVisible);
            child.hasMove = static_cast<int>(hasMove);
            child.hasRemove = static_cast<int>(hasRemove);
            record.children.push_back(child);
        }
        if (!ValidateRecord(record) ||
            (!records->empty() && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return reader.offset == bytes.size();
}

bool CollectCorpseRoster(SimulationContext *context,
                         std::vector<Corpse *> *objects)
{
    std::vector<KR_ObjectID> ids;
    if (context == NULL || objects == NULL ||
        !CorpseSubjectState_CollectObjects(&ids))
        return false;
    objects->clear();
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        Corpse *object = CorpseSubjectState_Find(context, ids[index]);
        if (object == NULL)
            return false;
        objects->push_back(object);
    }
    std::sort(objects->begin(), objects->end(),
              [context](const Corpse *left, const Corpse *right)
              {
                  const std::string leftName =
                      ObjectName(context, left->getObjectID());
                  const std::string rightName =
                      ObjectName(context, right->getObjectID());
                  if (leftName != rightName)
                      return leftName < rightName;
                  return left->getObjectID().id < right->getObjectID().id;
              });
    return true;
}

bool CollectSmokerRoster(SimulationContext *context,
                         std::vector<Smoker *> *objects)
{
    std::vector<KR_ObjectID> ids;
    if (context == NULL || objects == NULL ||
        !SmokerSubjectState_CollectDynObjects(&ids))
        return false;
    objects->clear();
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        Smoker *object = SmokerSubjectState_FindDyn(ids[index]);
        if (object == NULL)
            return false;
        objects->push_back(object);
    }
    std::sort(objects->begin(), objects->end(),
              [context](const Smoker *left, const Smoker *right)
              {
                  const std::string leftName =
                      ObjectName(context, left->getObjectID());
                  const std::string rightName =
                      ObjectName(context, right->getObjectID());
                  if (leftName != rightName)
                      return leftName < rightName;
                  return left->getObjectID().id < right->getObjectID().id;
              });
    return true;
}

bool CaptureChild(SimulationContext *context, const KR_ObjectID &id,
                  int role, StableSmoker *record)
{
    Smoker *object = SmokerSubjectState_FindDyn(id);
    if (record == NULL || object == NULL || object->m_attr == NULL ||
        object->m_dynamicPublished)
        return Fail("live COR1 child is not at an open frame boundary");
    record->role = role;
    record->name = ObjectName(context, id);
    record->attribute = ObjectName(context, object->m_attr->getObjectID());
    record->position = object->m_position;
    record->count = object->m_cnt;
    record->startTime = object->m_startTime;
    record->visible = object->m_isVisible ? 1 : 0;
    record->brightness = object->m_brightness;
    KR_Event move[2];
    KR_Event remove[2];
    const int moveCount = context->copyEvents(sm_EV_MOVE, id, move, 2);
    const int removeCount = context->copyEvents(sm_EV_REMOVE, id, remove, 2);
    if (moveCount < 0 || moveCount > 1 || removeCount < 0 ||
        removeCount > 1 || (moveCount == 1) != object->m_sendedMove)
        return Fail("live COR1 child scheduler boundary is invalid");
    if (moveCount == 1)
    {
        if (move[0].source != id || move[0].destination != id ||
            move[0].data.size() != 0)
            return Fail("live COR1 MOVE edge is invalid");
        record->hasMove = 1;
        record->moveTime = move[0].timeStamp;
    }
    const bool expectsRemove = object->m_attr->m_maxTimeLife > 0.0;
    if ((removeCount == 1) != expectsRemove)
        return Fail("live COR1 REMOVE edge is invalid");
    if (removeCount == 1)
    {
        if (remove[0].source != id || remove[0].destination != id)
            return Fail("live COR1 REMOVE ownership is invalid");
        record->hasRemove = 1;
        record->removeTime = remove[0].timeStamp;
        if (!NearlyEqual(record->removeTime,
                         record->startTime + object->m_attr->m_maxTimeLife))
            return Fail("live COR1 REMOVE time chain is invalid");
    }
    return ValidateChild(*record);
}

bool CaptureRecords(SimulationContext *context,
                    std::vector<StableCorpse> *records)
{
    std::vector<Corpse *> objects;
    std::vector<Smoker *> smokers;
    if (records == NULL || !CollectCorpseRoster(context, &objects) ||
        !CollectSmokerRoster(context, &smokers))
        return false;
    records->clear();
    std::set<int> ownedChildren;
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        Corpse *object = objects[index];
        if (object->m_attr == NULL || object->m_dynamicPublished)
            return Fail("live Corpse is not at a stable open frame boundary");
        StableCorpse record;
        record.name = ObjectName(context, object->getObjectID());
        record.attribute = ObjectName(
            context, object->m_attr->getObjectID());
        record.position = object->getPosition();
        record.visible = object->m_isVisible ? 1 : 0;
        record.mustDieNow = object->m_mustDieNow ? 1 : 0;
        KR_Event death[2];
        const int deathCount = context->copyEvents(
            CORPSE_TIME_TO_DIE, object->getObjectID(), death, 2);
        if (deathCount < 0 || deathCount > 1 ||
            (deathCount == 1) != !record.mustDieNow)
            return Fail("live Corpse death event boundary is invalid");
        if (deathCount == 1)
        {
            if (death[0].source != object->getObjectID() ||
                death[0].destination != object->getObjectID())
                return Fail("live Corpse death event ownership is invalid");
            record.hasDeath = 1;
            record.deathTime = death[0].timeStamp;
        }
        const KR_ObjectID childIDs[2] = {object->m_smoke, object->m_fire};
        for (int roleIndex = 0; roleIndex < 2; ++roleIndex)
        {
            const KR_ObjectID child = childIDs[roleIndex];
            if (IsNul(child) || !context->isExist(child))
                continue;
            if (!ownedChildren.insert(child.id).second)
                return Fail("COR1 child is shared by more than one Corpse");
            StableSmoker childRecord;
            if (!CaptureChild(context, child, roleIndex + 1,
                              &childRecord))
                return false;
            record.children.push_back(childRecord);
        }
        if (!ValidateRecord(record))
            return false;
        records->push_back(record);
    }
    if (ownedChildren.size() != smokers.size())
        return Fail("live DynSmoker has no COR1 owning Corpse");
    return true;
}

void FlattenChildren(const std::vector<StableCorpse> &records,
                     std::vector<const StableSmoker *> *children)
{
    children->clear();
    for (std::size_t record = 0; record < records.size(); ++record)
        for (std::size_t child = 0;
             child < records[record].children.size(); ++child)
            children->push_back(&records[record].children[child]);
}

bool RosterMatches(SimulationContext *context,
                   const std::vector<StableCorpse> &records,
                   const std::vector<Corpse *> &corpses,
                   const std::vector<Smoker *> &smokers)
{
    std::vector<const StableSmoker *> children;
    FlattenChildren(records, &children);
    if (records.size() != corpses.size() ||
        children.size() != smokers.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (records[index].name !=
            ObjectName(context, corpses[index]->getObjectID()))
            return false;
    std::vector<std::string> childNames;
    for (std::size_t index = 0; index < children.size(); ++index)
        childNames.push_back(children[index]->name);
    std::sort(childNames.begin(), childNames.end());
    for (std::size_t index = 0; index < smokers.size(); ++index)
        if (childNames[index] !=
            ObjectName(context, smokers[index]->getObjectID()))
            return false;
    return true;
}

bool MatchSmokersToRecords(
    SimulationContext *context,
    const std::vector<const StableSmoker *> &records,
    const std::vector<Smoker *> &smokers,
    std::vector<Smoker *> *matched)
{
    if (matched == NULL || records.size() != smokers.size())
        return false;
    matched->clear();
    std::vector<bool> used(smokers.size(), false);
    for (std::size_t record = 0; record < records.size(); ++record)
    {
        std::size_t selected = smokers.size();
        for (std::size_t object = 0; object < smokers.size(); ++object)
            if (!used[object] && records[record]->name ==
                    ObjectName(context, smokers[object]->getObjectID()))
            {
                selected = object;
                break;
            }
        if (selected == smokers.size())
            return false;
        used[selected] = true;
        matched->push_back(smokers[selected]);
    }
    return true;
}

int DrainCorpseEvents(SimulationContext *context, const KR_ObjectID &object)
{
    int count = 0;
    while (context != NULL &&
           context->removeEvent(CORPSE_TIME_TO_DIE, object) == 1)
        ++count;
    return count;
}

int DrainSmokerEvents(SimulationContext *context, const KR_ObjectID &object)
{
    int count = 0;
    while (context != NULL && context->removeEvent(sm_EV_MOVE, object) == 1)
        ++count;
    while (context != NULL && context->removeEvent(sm_EV_REMOVE, object) == 1)
        ++count;
    return count;
}

AttributeCorpse *ResolveCorpseAttribute(SimulationContext *context,
                                       const std::string &name,
                                       int *attributeIndex)
{
    const KR_ObjectID id = context->searchObject(name.c_str());
    AttributeCorpse *attribute = IsNul(id) ? NULL :
        static_cast<AttributeCorpse *>(
            __attrCorpseTable.searchAttribute(id));
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("CorpseAttr");
    if (attribute == NULL || table == ct_NULLID ||
        attribute->m_cacheSkin == NULL)
        return NULL;
    if (attributeIndex != NULL)
        *attributeIndex = g_arena.getAttributeIndex(table, id);
    return attributeIndex == NULL || *attributeIndex >= 0
        ? attribute : NULL;
}

AttributeSmoker *ResolveSmokerAttribute(SimulationContext *context,
                                       const std::string &name)
{
    const KR_ObjectID id = context->searchObject(name.c_str());
    return IsNul(id) ? NULL : static_cast<AttributeSmoker *>(
        __attrSmokerTable.searchAttribute(id));
}

bool StartProbeCorpse(SimulationContext *context, int attributeIndex,
                      const CFVector3 &position, double timeStamp,
                      KR_ObjectID *object)
{
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Corpse");
    if (object == NULL || table == ct_NULLID)
        return false;
    *object = g_arena.newObject(table, "Corpse.ActiveWorld.Probe");
    if (IsNul(*object))
        return false;
    KR_Event event;
    event.label = CORPSE_START_ROTTING;
    event.source = g_arena.getObjectID();
    event.destination = *object;
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE)
        .putObjectID(g_arena.getObjectID())
        .putInt(attributeIndex)
        .putDouble(position.x)
        .putDouble(position.y)
        .putDouble(position.z)
        .close();
    context->sendEventNow(event);
    Corpse *corpse = CorpseSubjectState_Find(context, *object);
    return corpse != NULL && corpse->m_attr != NULL;
}

bool RemoveDetachedSmoke(SimulationContext *context, int *removed)
{
    std::vector<unsigned char> bytes;
    std::vector<KR_ObjectID> owners;
    if (!SmokeActiveWorldState_CaptureStable(context, &bytes) ||
        !SmokeActiveWorldState_CollectStableOwners(context, bytes, &owners))
        return false;
    if (removed != NULL)
        *removed = static_cast<int>(owners.size());
    SmokeActiveWorldState_RemoveStableOwners(context, &owners);
    return SmokeSubjectState_LiveCount() == 0;
}

}  // namespace

void CorpseActiveWorldState_Link()
{
    CorpseSubjectState_Link();
    SmokerSubjectState_Link();
}

const char *CorpseActiveWorldState_LastFailure()
{
    return g_failure.c_str();
}

int CorpseActiveWorldState_OwnedSmokerCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableCorpse> records;
    if (!Decode(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        count += static_cast<int>(records[index].children.size());
    return count;
}

int CorpseActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableCorpse> records;
    if (!Decode(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        count += records[index].hasDeath;
        for (std::size_t child = 0;
             child < records[index].children.size(); ++child)
            count += records[index].children[child].hasMove +
                     records[index].children[child].hasRemove;
    }
    return count;
}

unsigned long long CorpseActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!CorpseActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = kHashOffset;
    if (!bytes.empty())
        HashBytes(&hash, &bytes[0], static_cast<int>(bytes.size()));
    return hash;
}

bool CorpseActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_failure.clear();
    std::vector<StableCorpse> records;
    if (!CaptureRecords(context, &records))
    {
        if (g_failure.empty())
            Fail("Corpse stable graph collection failed");
        return false;
    }
    return Encode(records, bytes);
}

bool CorpseActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableCorpse> records;
    return Decode(bytes, &records);
}

bool CorpseActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return CorpseActiveWorldState_ValidateStable(bytes) &&
           CorpseActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool CorpseActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableCorpse> records;
    std::vector<Corpse *> corpses;
    std::vector<Smoker *> smokers;
    if (owners == NULL || !owners->empty() || !Decode(bytes, &records) ||
        !CollectCorpseRoster(context, &corpses) ||
        !CollectSmokerRoster(context, &smokers) ||
        !RosterMatches(context, records, corpses, smokers))
        return false;
    for (std::size_t index = 0; index < corpses.size(); ++index)
        owners->push_back(corpses[index]->getObjectID());
    for (std::size_t index = 0; index < smokers.size(); ++index)
        owners->push_back(smokers[index]->getObjectID());
    return true;
}

bool CorpseActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableCorpse> records;
    std::vector<Corpse *> corpses;
    std::vector<Smoker *> smokers;
    std::vector<const StableSmoker *> childRecords;
    if (context == NULL || created == NULL || !created->empty() ||
        !Decode(bytes, &records) ||
        !CollectCorpseRoster(context, &corpses) ||
        !CollectSmokerRoster(context, &smokers))
        return false;
    if (!corpses.empty() || !smokers.empty())
        return RosterMatches(context, records, corpses, smokers);
    FlattenChildren(records, &childRecords);
    if (static_cast<int>(records.size()) > CorpseSubjectState_Capacity() ||
        static_cast<int>(childRecords.size()) >
            SmokerSubjectState_DynCapacity())
        return Fail("COR1 owner tables have insufficient capacity");
    const ct_ClassTableID corpseTable =
        g_arena.searchSeanceClassTable("Corpse");
    const ct_ClassTableID smokerTable =
        g_arena.searchSeanceClassTable("DynSmoker");
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        KR_ObjectID object =
            g_arena.newObject(corpseTable, records[index].name.c_str());
        if (IsNul(object))
        {
            CorpseActiveWorldState_RemoveStableOwners(context, created);
            return Fail("COR1 Corpse owner allocation failed");
        }
        created->push_back(object);
    }
    for (std::size_t index = 0; index < childRecords.size(); ++index)
    {
        KR_ObjectID object =
            g_arena.newObject(smokerTable, childRecords[index]->name.c_str());
        if (IsNul(object))
        {
            CorpseActiveWorldState_RemoveStableOwners(context, created);
            return Fail("COR1 DynSmoker child allocation failed");
        }
        created->push_back(object);
    }
    corpses.clear();
    smokers.clear();
    if (!CollectCorpseRoster(context, &corpses) ||
        !CollectSmokerRoster(context, &smokers) ||
        !RosterMatches(context, records, corpses, smokers))
    {
        CorpseActiveWorldState_RemoveStableOwners(context, created);
        return Fail("COR1 allocated graph roster is not canonical");
    }
    return true;
}

bool CorpseActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableCorpse> records;
    std::vector<Corpse *> corpses;
    std::vector<Smoker *> smokers;
    std::vector<Smoker *> matchedSmokers;
    std::vector<const StableSmoker *> childRecords;
    if (context == NULL || !Decode(bytes, &records) ||
        !CollectCorpseRoster(context, &corpses) ||
        !CollectSmokerRoster(context, &smokers) ||
        !RosterMatches(context, records, corpses, smokers))
        return false;
    FlattenChildren(records, &childRecords);
    if (!MatchSmokersToRecords(context, childRecords, smokers,
                               &matchedSmokers))
        return false;
    std::vector<AttributeCorpse *> corpseAttributes(records.size(), NULL);
    std::vector<int> corpseAttributeIndices(records.size(), -1);
    std::vector<AttributeSmoker *> smokerAttributes(
        childRecords.size(), NULL);
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        corpseAttributes[index] = ResolveCorpseAttribute(
            context, records[index].attribute,
            &corpseAttributeIndices[index]);
        if (corpseAttributes[index] == NULL)
            return Fail("COR1 CorpseAttr/skin dependency is unresolved");
    }
    for (std::size_t index = 0; index < childRecords.size(); ++index)
    {
        smokerAttributes[index] = ResolveSmokerAttribute(
            context, childRecords[index]->attribute);
        if (smokerAttributes[index] == NULL)
            return Fail("COR1 SmokerAttr dependency is unresolved");
        if ((childRecords[index]->hasRemove != 0) !=
            (smokerAttributes[index]->m_maxTimeLife > 0.0))
            return Fail("COR1 SmokerAttr lifetime differs from the snapshot");
    }
    for (std::size_t index = 0; index < corpses.size(); ++index)
        if (corpses[index]->m_dynamicPublished)
            return Fail("COR1 cannot replace a frame-published Corpse");
    for (std::size_t index = 0; index < smokers.size(); ++index)
        if (smokers[index]->m_dynamicPublished)
            return Fail("COR1 cannot replace a frame-published DynSmoker");
    for (std::size_t index = 0; index < corpses.size(); ++index)
    {
        DrainCorpseEvents(context, corpses[index]->getObjectID());
        corpses[index]->resetState();
    }
    for (std::size_t index = 0; index < smokers.size(); ++index)
    {
        DrainSmokerEvents(context, smokers[index]->getObjectID());
        smokers[index]->resetState();
    }
    for (std::size_t index = 0; index < childRecords.size(); ++index)
    {
        const StableSmoker &record = *childRecords[index];
        Smoker *object = matchedSmokers[index];
        object->m_attr = smokerAttributes[index];
        object->m_position = record.position;
        object->m_cnt = record.count;
        object->m_startTime = record.startTime;
        object->m_isVisible = record.visible != 0;
        object->m_brightness = record.brightness;
        object->m_sendedMove = record.hasMove != 0;
        if (record.hasMove)
        {
            KR_Event event;
            event.label = sm_EV_MOVE;
            event.source = object->getObjectID();
            event.destination = object->getObjectID();
            event.timeStamp = record.moveTime;
            context->addEvent(event);
        }
        if (record.hasRemove)
        {
            KR_Event event;
            event.label = sm_EV_REMOVE;
            event.source = object->getObjectID();
            event.destination = object->getObjectID();
            event.timeStamp = record.removeTime;
            context->addEvent(event);
        }
    }
    std::size_t childOffset = 0;
    for (std::size_t index = 0; index < corpses.size(); ++index)
    {
        Corpse *object = corpses[index];
        const StableCorpse &record = records[index];
        object->m_attributeIndex = corpseAttributeIndices[index];
        object->m_attr = corpseAttributes[index];
        object->m_skin.Attach(object->m_attr->m_cacheSkin);
        object->setPosition(record.position);
        object->m_isVisible = record.visible != 0;
        object->m_mustDieNow = record.mustDieNow;
        for (std::size_t child = 0; child < record.children.size(); ++child)
        {
            const KR_ObjectID childID =
                matchedSmokers[childOffset++]->getObjectID();
            if (record.children[child].role == kSmokeRole)
                object->m_smoke = childID;
            else
                object->m_fire = childID;
        }
        if (record.hasDeath)
        {
            KR_Event event;
            event.label = CORPSE_TIME_TO_DIE;
            event.source = object->getObjectID();
            event.destination = object->getObjectID();
            event.timeStamp = record.deathTime;
            context->addEvent(event);
        }
    }
    std::vector<unsigned char> current;
    if (!CorpseActiveWorldState_CaptureStable(context, &current) ||
        current != bytes)
        return Fail("COR1 canonical recapture differs");
    return true;
}

void CorpseActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            DrainCorpseEvents(context, *object);
            DrainSmokerEvents(context, *object);
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    created->clear();
}

bool CorpseActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, double timeStamp,
    CorpseActiveWorldProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    g_failure.clear();
    if (context == NULL || CorpseSubjectState_LiveCount() != 0 ||
        SmokerSubjectState_DynLiveCount() != 0 ||
        SmokeSubjectState_LiveCount() != 0 ||
        !CorpseAttributeState_RuntimeReady(context))
        return Fail("Corpse active-world probe requires an empty ready graph");
    int attributeIndex = -1;
    AttributeCorpse *attribute = ResolveCorpseAttribute(
        context, "Corpse.Attr.Default", &attributeIndex);
    CViewScene *scene = CViewScene::Current();
    if (attribute == NULL || attributeIndex < 0 ||
        !attribute->m_isSmoking || !attribute->m_isBurning ||
        scene == NULL || scene->GetTerrain() == NULL)
        return Fail("Corpse active-world probe dependency is unavailable");

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    KR_ObjectID originalFirst = KR_ObjectID::NUL();
    KR_ObjectID originalSecond = KR_ObjectID::NUL();
    std::vector<KR_ObjectID> originalOwners;
    std::vector<KR_ObjectID> staged;
    std::vector<KR_ObjectID> restored;
    std::vector<KR_ObjectID> detachedSmoke;
    bool success = false;
    do
    {
        CFVector3 first(4096.0, 0.0, -4096.0);
        CFVector3 normal;
        double height = 0.0;
        scene->GetTerrain()->GetPlane(first, normal, height);
        first.y = height + 10.0;
        CFVector3 second = first + CFVector3(3.0, 0.0, 3.0);
        scene->GetTerrain()->GetPlane(second, normal, height);
        second.y = height + 10.0;
        if (!StartProbeCorpse(context, attributeIndex, first, ts,
                              &originalFirst) ||
            !StartProbeCorpse(context, attributeIndex, second, ts + 0.01,
                              &originalSecond) ||
            originalFirst == originalSecond)
        {
            Fail("Corpse active-world real START_ROTTING setup failed");
            break;
        }
        std::vector<Corpse *> corpses;
        if (!CollectCorpseRoster(context, &corpses) || corpses.size() != 2)
            break;
        for (std::size_t index = 0; index < corpses.size(); ++index)
            corpses[index]->m_isVisible = true;
        std::vector<Smoker *> smokers;
        if (!CollectSmokerRoster(context, &smokers) || smokers.size() != 4)
        {
            Fail("Corpse active-world owned DynSmoker setup failed");
            break;
        }
        for (std::size_t index = 0; index < smokers.size(); ++index)
        {
            smokers[index]->m_isVisible = true;
            smokers[index]->onView(ts + 1.0 + index * 0.01);
            if (!smokers[index]->m_sendedMove)
            {
                Fail("Corpse active-world child MOVE setup failed");
                break;
            }
        }
        if (!g_failure.empty())
            break;
        std::vector<unsigned char> bytes;
        if (!CorpseActiveWorldState_CaptureStable(context, &bytes) ||
            CorpseActiveWorldState_OwnedSmokerCount(bytes) != 4 ||
            !CorpseActiveWorldState_CollectStableOwners(
                context, bytes, &originalOwners) ||
            originalOwners.size() != 6)
            break;
        const int schedulerEvents =
            CorpseActiveWorldState_SchedulerEventCount(bytes);
        unsigned long long fingerprint = kHashOffset;
        HashBytes(&fingerprint, &bytes[0], static_cast<int>(bytes.size()));
        CorpseActiveWorldState_RemoveStableOwners(context, &originalOwners);
        if (CorpseSubjectState_LiveCount() != 0 ||
            SmokerSubjectState_DynLiveCount() != 0)
        {
            Fail("COR1 original graph teardown failed");
            break;
        }
        if (!CorpseActiveWorldState_CreateStableOwners(
                context, bytes, &staged) || staged.size() != 6 ||
            !CorpseActiveWorldState_ApplyStableReferences(context, bytes) ||
            !CorpseActiveWorldState_MatchesStable(context, bytes))
        {
            Fail("COR1 staged reconstruction failed");
            break;
        }
        CorpseActiveWorldState_RemoveStableOwners(context, &staged);
        if (CorpseSubjectState_LiveCount() != 0 ||
            SmokerSubjectState_DynLiveCount() != 0)
        {
            Fail("COR1 staged rollback retained graph state");
            break;
        }
        if (!CorpseActiveWorldState_CreateStableOwners(
                context, bytes, &restored) || restored.size() != 6 ||
            !CorpseActiveWorldState_ApplyStableReferences(context, bytes) ||
            !CorpseActiveWorldState_MatchesStable(context, bytes))
        {
            Fail("COR1 final reconstruction failed");
            break;
        }
        smokers.clear();
        if (!CollectSmokerRoster(context, &smokers) || smokers.empty())
            break;
        Smoker *resumedSmoker = smokers[0];
        KR_Event move[2];
        if (context->copyEvents(sm_EV_MOVE, resumedSmoker->getObjectID(),
                                move, 2) != 1 ||
            context->removeEvent(sm_EV_MOVE,
                                 resumedSmoker->getObjectID()) != 1)
        {
            Fail("COR1 restored child MOVE is unavailable");
            break;
        }
        context->sendEventNow(move[0]);
        int emitted = 0;
        if (SmokeSubjectState_LiveCount() != 1 ||
            context->copyEvents(sm_EV_MOVE,
                                resumedSmoker->getObjectID(), move, 2) != 1 ||
            !RemoveDetachedSmoke(context, &emitted) || emitted != 1)
        {
            Fail("COR1 restored child did not resume emission");
            break;
        }
        corpses.clear();
        if (!CollectCorpseRoster(context, &corpses) || corpses.empty())
            break;
        Corpse *resumedCorpse = corpses[0];
        KR_Event death[2];
        if (context->copyEvents(CORPSE_TIME_TO_DIE,
                                resumedCorpse->getObjectID(), death, 2) != 1 ||
            context->removeEvent(CORPSE_TIME_TO_DIE,
                                 resumedCorpse->getObjectID()) != 1)
        {
            Fail("COR1 restored death edge is unavailable");
            break;
        }
        context->sendEventNow(death[0]);
        if (!context->isExist(resumedCorpse->getObjectID()) ||
            !resumedCorpse->m_mustDieNow)
        {
            Fail("COR1 visible Corpse did not defer restored death");
            break;
        }
        resumedCorpse->onHide(death[0].timeStamp);
        summary->capturedCorpses = 2;
        summary->ownedSmokers = 4;
        summary->schedulerEvents = schedulerEvents;
        summary->stagedRollbacks = 1;
        summary->reconstructedObjects = 6;
        summary->stableRoundTrips = 2;
        summary->resumedEmissions = 1;
        summary->resumedDeaths = 1;
        summary->fingerprint = fingerprint;
        success = true;
    } while (false);

    CorpseActiveWorldState_RemoveStableOwners(context, &restored);
    CorpseActiveWorldState_RemoveStableOwners(context, &staged);
    CorpseActiveWorldState_RemoveStableOwners(context, &originalOwners);
    if (context != NULL)
    {
        if (!IsNul(originalSecond) && context->isExist(originalSecond))
            context->removeObject(originalSecond);
        if (!IsNul(originalFirst) && context->isExist(originalFirst))
            context->removeObject(originalFirst);
    }
    int ignored = 0;
    if (SmokeSubjectState_LiveCount() != 0)
        RemoveDetachedSmoke(context, &ignored);
    const bool clean = CorpseSubjectState_LiveCount() == 0 &&
                       SmokerSubjectState_DynLiveCount() == 0 &&
                       SmokeSubjectState_LiveCount() == 0;
    if (!success || !clean)
    {
        std::memset(summary, 0, sizeof(*summary));
        if (g_failure.empty())
            Fail("COR1 probe rollback was not clean");
        return false;
    }
    return true;
}
