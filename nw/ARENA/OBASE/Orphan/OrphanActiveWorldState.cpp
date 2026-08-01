#include "Orphan.h"
#include "OrphanActiveWorldState.h"
#include "OrphanSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "kernel/h/context.h"
#include "message/Unitmsg.h"
#include "message/vehiclemsg.h"
#include "obase/taxi/Taxi.h"

namespace
{

const std::uint32_t kOrphanMagic = 0x3150524fu; // ORP1
const std::uint32_t kOrphanVersion = 1u;
const std::size_t kMaximumOrphans = 4096u;
const std::size_t kMaximumNameBytes = 1024u;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
std::string g_lastFailure;

bool Fail(const std::string &message)
{
    g_lastFailure = message;
    return false;
}

bool IsNul(const KR_ObjectID &object)
{
    KR_ObjectID copy = object;
    return copy.isNUL();
}

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool FiniteMatrix(const CFMatrix3x4 &value)
{
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            if (!std::isfinite(value.m[row][column]))
                return false;
    return true;
}

struct StableOrphanRecord
{
    std::string name;
    std::string taxiAttribute;
    int audibleThisFrame;
    int visible;
    double lastMoveTimeStamp;
    CFVector3 position;
    CFVector3 speed;
    double damage;
    double lastEventTime;
    CFMatrix3x4 direction;
    CFMatrix3x4 storedDirection;
    CFVector3 lastMovePosition;
    double lastMoveDeltaT;
    double movingTimeStamp;

    StableOrphanRecord()
        : audibleThisFrame(0), visible(0), lastMoveTimeStamp(0.0),
          position(0.0, 0.0, 0.0), speed(0.0, 0.0, 0.0),
          damage(0.0), lastEventTime(0.0),
          lastMovePosition(0.0, 0.0, 0.0), lastMoveDeltaT(0.0),
          movingTimeStamp(0.0)
    {
        direction.LoadIdentity();
        storedDirection.LoadIdentity();
    }
};

struct OrphanRosterEntry
{
    std::string name;
    Orphan *orphan;
};

struct OrphanRoster
{
    SimulationContext *context;
    std::vector<OrphanRosterEntry> entries;
    bool valid;
    bool requireReady;
};

Orphan *ResolveOrphan(SimulationContext *context,
                      const KR_ObjectID &object)
{
    KR_ObjectID candidate = object;
    if (context == NULL || candidate.isNUL() || !context->isExist(object))
        return NULL;
    IDynamicObject *dynamicObject = static_cast<IDynamicObject *>(
        context->queryInterface(object, IDynamicObjectIID));
    return dynamicObject == NULL
               ? NULL
               : dynamic_cast<Orphan *>(dynamicObject);
}

bool CollectOrphan(const KR_ObjectID object, void *user)
{
    OrphanRoster *roster = static_cast<OrphanRoster *>(user);
    Orphan *orphan = ResolveOrphan(roster->context, object);
    const char *name = roster->context == NULL
                           ? NULL
                           : roster->context->searchObject(object);
    if (orphan == NULL || name == NULL || name[0] == '\0' ||
        std::strlen(name) > kMaximumNameBytes ||
        (roster->requireReady && !orphan->runtimeReady()))
    {
        roster->valid = false;
        if (orphan == NULL)
            Fail("Orphan roster contains an unresolved owner");
        else if (name == NULL || name[0] == '\0')
            Fail("Orphan roster owner has no symbolic name");
        else if (std::strlen(name) > kMaximumNameBytes)
            Fail("Orphan roster owner name exceeds the codec limit");
        else
            Fail(std::string("Orphan owner is not runtime-ready: ") + name);
        return false;
    }
    OrphanRosterEntry entry;
    entry.name = name;
    entry.orphan = orphan;
    roster->entries.push_back(entry);
    return true;
}

bool OrphanEntryLess(const OrphanRosterEntry &left,
                     const OrphanRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectRoster(SimulationContext *context, bool requireReady,
                   OrphanRoster *roster)
{
    if (context == NULL || roster == NULL)
        return Fail("Orphan roster context/output is null");
    if (g_arena.getContext() != context)
        return Fail("Orphan roster belongs to another Arena context");
    roster->context = context;
    roster->entries.clear();
    roster->valid = true;
    roster->requireReady = requireReady;
    if (g_arena.searchSeanceClassTable("Orphan") == ct_NULLID)
        return true;
    g_arena.userFind("Orphan", CollectOrphan, roster);
    if (!roster->valid)
        return false;
    // Retail and runtime-created Orphans may share one symbolic name. The
    // stable class-table order inside an equal-name group is their ordinal.
    std::stable_sort(roster->entries.begin(), roster->entries.end(),
                     OrphanEntryLess);
    return true;
}

bool CaptureRecord(SimulationContext *context,
                   const OrphanRosterEntry &entry,
                   StableOrphanRecord *record)
{
    Orphan *orphan = entry.orphan;
    const char *attribute = orphan == NULL
                                ? NULL
                                : context->searchObject(
                                      orphan->m_orphanAttrID);
    if (record == NULL || orphan == NULL || attribute == NULL ||
        attribute[0] == '\0' || std::strlen(attribute) > kMaximumNameBytes)
        return Fail("Orphan TaxiAttr identity is unavailable");
    KR_Event moving[2];
    const int movingCount = context->copyEvents(
        t_EVC_MOVING, orphan->getObjectID(), moving, 2);
    if (movingCount != 1 || moving[0].source != orphan->getObjectID() ||
        moving[0].destination != orphan->getObjectID() ||
        !std::isfinite(moving[0].timeStamp))
        return Fail("Orphan private moving event is not stable");

    record->name = entry.name;
    record->taxiAttribute = attribute;
    record->audibleThisFrame = orphan->m_audibleThisFrame != 0 ? 1 : 0;
    record->visible = orphan->m_isVisible != 0 ? 1 : 0;
    record->lastMoveTimeStamp = orphan->m_lastMoveTimeStamp;
    record->position = orphan->getPosition();
    record->speed = orphan->m_speed;
    record->damage = orphan->m_damage;
    record->lastEventTime = orphan->m_lastEventTime;
    record->direction = orphan->GetDir();
    record->storedDirection = orphan->m_dir;
    record->lastMovePosition = orphan->m_lastMovePos;
    record->lastMoveDeltaT = orphan->m_lastMoveDeltaT;
    record->movingTimeStamp = moving[0].timeStamp;
    return true;
}

bool ValidateRecord(const StableOrphanRecord &record)
{
    return !record.name.empty() && record.name.size() <= kMaximumNameBytes &&
           !record.taxiAttribute.empty() &&
           record.taxiAttribute.size() <= kMaximumNameBytes &&
           record.name.find('\0') == std::string::npos &&
           record.taxiAttribute.find('\0') == std::string::npos &&
           (record.audibleThisFrame == 0 || record.audibleThisFrame == 1) &&
           (record.visible == 0 || record.visible == 1) &&
           std::isfinite(record.lastMoveTimeStamp) &&
           record.lastMoveTimeStamp >= 0.0 && FiniteVector(record.position) &&
           FiniteVector(record.speed) && std::isfinite(record.damage) &&
           std::isfinite(record.lastEventTime) &&
           record.lastEventTime >= 0.1 && FiniteMatrix(record.direction) &&
           FiniteMatrix(record.storedDirection) &&
           FiniteVector(record.lastMovePosition) &&
           std::isfinite(record.lastMoveDeltaT) &&
           record.lastMoveDeltaT >= 0.0 &&
           std::isfinite(record.movingTimeStamp) &&
           record.movingTimeStamp > record.lastEventTime;
}

void PutU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void PutU64(std::vector<unsigned char> *bytes, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void PutDouble(std::vector<unsigned char> *bytes, double value)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    PutU64(bytes, bits);
}

void PutString(std::vector<unsigned char> *bytes,
               const std::string &value)
{
    PutU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
}

void PutVector(std::vector<unsigned char> *bytes, const CFVector3 &value)
{
    PutDouble(bytes, value.x);
    PutDouble(bytes, value.y);
    PutDouble(bytes, value.z);
}

void PutMatrix(std::vector<unsigned char> *bytes,
               const CFMatrix3x4 &value)
{
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            PutDouble(bytes, value.m[row][column]);
}

bool GetU32(const std::vector<unsigned char> &bytes, std::size_t *offset,
            std::uint32_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 4)
        return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
    return true;
}

bool GetU64(const std::vector<unsigned char> &bytes, std::size_t *offset,
            std::uint64_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 8)
        return false;
    *value = 0;
    for (int shift = 0; shift < 64; shift += 8)
        *value |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
    return true;
}

bool GetDouble(const std::vector<unsigned char> &bytes, std::size_t *offset,
               double *value)
{
    std::uint64_t bits = 0;
    if (value == NULL || !GetU64(bytes, offset, &bits))
        return false;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
}

bool GetString(const std::vector<unsigned char> &bytes, std::size_t *offset,
               std::string *value)
{
    std::uint32_t size = 0;
    if (value == NULL || !GetU32(bytes, offset, &size) ||
        size > kMaximumNameBytes || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    value->assign(size == 0
                      ? ""
                      : reinterpret_cast<const char *>(&bytes[*offset]),
                  size);
    *offset += size;
    return value->find('\0') == std::string::npos;
}

bool GetVector(const std::vector<unsigned char> &bytes, std::size_t *offset,
               CFVector3 *value)
{
    return value != NULL && GetDouble(bytes, offset, &value->x) &&
           GetDouble(bytes, offset, &value->y) &&
           GetDouble(bytes, offset, &value->z);
}

bool GetMatrix(const std::vector<unsigned char> &bytes, std::size_t *offset,
               CFMatrix3x4 *value)
{
    if (value == NULL)
        return false;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            if (!GetDouble(bytes, offset, &value->m[row][column]))
                return false;
    return true;
}

bool PutRecord(std::vector<unsigned char> *bytes,
               const StableOrphanRecord &record)
{
    if (!ValidateRecord(record))
        return false;
    PutString(bytes, record.name);
    PutString(bytes, record.taxiAttribute);
    PutU32(bytes, static_cast<std::uint32_t>(record.audibleThisFrame));
    PutU32(bytes, static_cast<std::uint32_t>(record.visible));
    PutDouble(bytes, record.lastMoveTimeStamp);
    PutVector(bytes, record.position);
    PutVector(bytes, record.speed);
    PutDouble(bytes, record.damage);
    PutDouble(bytes, record.lastEventTime);
    PutMatrix(bytes, record.direction);
    PutMatrix(bytes, record.storedDirection);
    PutVector(bytes, record.lastMovePosition);
    PutDouble(bytes, record.lastMoveDeltaT);
    PutDouble(bytes, record.movingTimeStamp);
    return true;
}

bool GetRecord(const std::vector<unsigned char> &bytes, std::size_t *offset,
               StableOrphanRecord *record)
{
    std::uint32_t audible = 0, visible = 0;
    if (record == NULL || !GetString(bytes, offset, &record->name) ||
        !GetString(bytes, offset, &record->taxiAttribute) ||
        !GetU32(bytes, offset, &audible) ||
        !GetU32(bytes, offset, &visible) ||
        !GetDouble(bytes, offset, &record->lastMoveTimeStamp) ||
        !GetVector(bytes, offset, &record->position) ||
        !GetVector(bytes, offset, &record->speed) ||
        !GetDouble(bytes, offset, &record->damage) ||
        !GetDouble(bytes, offset, &record->lastEventTime) ||
        !GetMatrix(bytes, offset, &record->direction) ||
        !GetMatrix(bytes, offset, &record->storedDirection) ||
        !GetVector(bytes, offset, &record->lastMovePosition) ||
        !GetDouble(bytes, offset, &record->lastMoveDeltaT) ||
        !GetDouble(bytes, offset, &record->movingTimeStamp) ||
        audible > 1u || visible > 1u)
        return false;
    record->audibleThisFrame = static_cast<int>(audible);
    record->visible = static_cast<int>(visible);
    return ValidateRecord(*record);
}

bool EncodeRecords(const std::vector<StableOrphanRecord> &records,
                   std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumOrphans)
        return false;
    bytes->clear();
    PutU32(bytes, kOrphanMagic);
    PutU32(bytes, kOrphanVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (index != 0 && records[index - 1].name > records[index].name)
            return Fail("Orphan records are not in symbolic order");
        if (!PutRecord(bytes, records[index]))
            return Fail("Orphan record validation failed");
    }
    return true;
}

bool DecodeRecords(const std::vector<unsigned char> &bytes,
                   std::vector<StableOrphanRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) || magic != kOrphanMagic ||
        version != kOrphanVersion || count > kMaximumOrphans)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableOrphanRecord record;
        if (!GetRecord(bytes, &offset, &record) ||
            (index != 0 && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

bool RosterMatches(const OrphanRoster &roster,
                   const std::vector<StableOrphanRecord> &records)
{
    if (roster.entries.size() != records.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (roster.entries[index].name != records[index].name)
            return false;
    return true;
}

bool SendOrphanStart(Orphan *orphan, const KR_ObjectID &taxiAttribute,
                     const StableOrphanRecord &record)
{
    if (orphan == NULL)
        return false;
    KR_Event event;
    event.label = EV_VEHICLE_DROP_TAXI;
    event.destination = orphan->getObjectID();
    event.source = orphan->getObjectID();
    event.timeStamp = record.lastEventTime;
    event.data.open(EDO_WRITE)
              .putObjectID(taxiAttribute)
              .putDouble(record.position.x)
              .putDouble(record.position.y)
              .putDouble(record.position.z)
              .putDouble(record.damage)
              .putInt(0)
              .close();
    return orphan->receiveEvent(event) == 1;
}

void RemoveMovingEvents(SimulationContext *context,
                        const KR_ObjectID &object)
{
    while (context != NULL &&
           context->removeEvent(t_EVC_MOVING, object) == 1)
    {
    }
}

bool ApplyRecord(SimulationContext *context, Orphan *orphan,
                 const StableOrphanRecord &record)
{
    if (context == NULL || orphan == NULL ||
        !context->isExist(record.taxiAttribute.c_str()))
        return false;
    KR_ObjectID attribute = context->searchObject(
        record.taxiAttribute.c_str());
    if (__attrTaxiTable.searchAttribute(attribute) == NULL ||
        !SendOrphanStart(orphan, attribute, record))
        return false;
    RemoveMovingEvents(context, orphan->getObjectID());
    orphan->m_speed = record.speed;
    orphan->m_damage = record.damage;
    orphan->m_lastEventTime = record.lastEventTime;
    orphan->m_dir = record.storedDirection;
    orphan->SetDir(record.direction);
    orphan->setPosition(record.position);
    orphan->m_lastMovePos = record.lastMovePosition;
    orphan->m_lastMoveDeltaT = record.lastMoveDeltaT;
    orphan->m_lastMoveTimeStamp = record.lastMoveTimeStamp;
    orphan->m_audibleThisFrame = record.audibleThisFrame;
    orphan->m_isVisible = record.visible;
    KR_Event moving;
    moving.label = t_EVC_MOVING;
    moving.source = orphan->getObjectID();
    moving.destination = orphan->getObjectID();
    moving.timeStamp = record.movingTimeStamp;
    context->addEvent(moving);
    return orphan->runtimeReady();
}

} // namespace

void OrphanActiveWorldState_Link()
{
}

const char *OrphanActiveWorldState_LastFailure()
{
    return g_lastFailure.c_str();
}

int OrphanActiveWorldState_LiveCount(SimulationContext *context)
{
    OrphanRoster roster = {};
    return CollectRoster(context, false, &roster)
               ? static_cast<int>(roster.entries.size())
               : -1;
}

int OrphanActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableOrphanRecord> records;
    return DecodeRecords(bytes, &records)
               ? static_cast<int>(records.size())
               : -1;
}

unsigned long long OrphanActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!OrphanActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = kHashOffset;
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        hash ^= bytes[index];
        hash *= kHashPrime;
    }
    return hash;
}

bool OrphanActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_lastFailure.clear();
    OrphanRoster roster = {};
    if (bytes == NULL)
        return Fail("Orphan capture output is null");
    if (!CollectRoster(context, true, &roster))
        return false;
    std::vector<StableOrphanRecord> records;
    records.reserve(roster.entries.size());
    for (std::size_t index = 0; index < roster.entries.size(); ++index)
    {
        StableOrphanRecord record;
        if (!CaptureRecord(context, roster.entries[index], &record))
            return false;
        if (!ValidateRecord(record))
            return Fail("Orphan live record is outside a stable boundary");
        records.push_back(record);
    }
    return EncodeRecords(records, bytes);
}

bool OrphanActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableOrphanRecord> records;
    return DecodeRecords(bytes, &records);
}

bool OrphanActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return OrphanActiveWorldState_ValidateStable(bytes) &&
           OrphanActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool OrphanActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableOrphanRecord> records;
    OrphanRoster roster = {};
    if (context == NULL || owners == NULL || !owners->empty() ||
        !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, true, &roster) ||
        !RosterMatches(roster, records))
        return false;
    for (std::size_t index = 0; index < roster.entries.size(); ++index)
        owners->push_back(roster.entries[index].orphan->getObjectID());
    return true;
}

bool OrphanActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableOrphanRecord> records;
    OrphanRoster roster = {};
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, false, &roster))
        return false;
    if (!roster.entries.empty())
        return RosterMatches(roster, records);
    if (records.size() > static_cast<std::size_t>(
                             OrphanSubjectState_Capacity()))
        return Fail("Orphan subject table has insufficient capacity");
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!context->isExist(records[index].taxiAttribute.c_str()) ||
            __attrTaxiTable.searchAttribute(context->searchObject(
                records[index].taxiAttribute.c_str())) == NULL)
            return Fail("Orphan TaxiAttr dependency is unavailable");
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        KR_ObjectID object = g_arena.newObject(
            "Orphan", records[index].name.c_str());
        if (IsNul(object))
        {
            OrphanActiveWorldState_RemoveStableOwners(context, created);
            return Fail("Orphan owner allocation failed");
        }
        created->push_back(object);
    }
    OrphanRoster restored = {};
    if (!CollectRoster(context, false, &restored) ||
        !RosterMatches(restored, records))
    {
        OrphanActiveWorldState_RemoveStableOwners(context, created);
        return Fail("Orphan symbolic owner allocation did not match");
    }
    return true;
}

bool OrphanActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableOrphanRecord> records;
    OrphanRoster roster = {};
    if (context == NULL || !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, false, &roster) ||
        !RosterMatches(roster, records))
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ApplyRecord(context, roster.entries[index].orphan,
                         records[index]))
            return Fail("Orphan symbolic reconstruction failed");
    return OrphanActiveWorldState_MatchesStable(context, bytes);
}

void OrphanActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            RemoveMovingEvents(context, *object);
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    created->clear();
}
