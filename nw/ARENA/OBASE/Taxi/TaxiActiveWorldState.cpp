#define LAST_H__VIEW
#include "game.h"
#include "scene.h"
#include "Taxi.h"
#include "TaxiActiveWorldState.h"
#include "TaxiSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "kernel/h/context.h"
#include "message/Unitmsg.h"

namespace
{

const std::uint32_t kTaxiMagic = 0x31495854u; // TXI1
const std::uint32_t kTaxiVersion = 1u;
const std::size_t kMaximumTaxis = 4096u;
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

struct StableTaxiEvent
{
    bool present;
    double timeStamp;

    StableTaxiEvent() : present(false), timeStamp(0.0) {}
};

struct StableTaxiRecord
{
    std::string name;
    std::string attribute;
    int audibleThisFrame;
    int visible;
    double lastMoveTimeStamp;
    CFVector3 position;
    double damage;
    int bullets;
    CFMatrix3x4 direction;
    CFMatrix3x4 storedDirection;
    StableTaxiEvent moving;

    StableTaxiRecord()
        : audibleThisFrame(0), visible(0), lastMoveTimeStamp(0.0),
          position(0.0, 0.0, 0.0), damage(0.0), bullets(0)
    {
        direction.LoadIdentity();
        storedDirection.LoadIdentity();
    }
};

struct TaxiRosterEntry
{
    std::string name;
    Taxi *taxi;
};

struct TaxiRoster
{
    SimulationContext *context;
    std::vector<TaxiRosterEntry> entries;
    bool valid;
    bool requireReady;
};

Taxi *ResolveTaxi(SimulationContext *context, const KR_ObjectID &object)
{
    KR_ObjectID candidate = object;
    if (context == NULL || candidate.isNUL() || !context->isExist(object))
        return NULL;
    ITaxi *taxi = static_cast<ITaxi *>(
        context->queryInterface(object, ITaxiIID));
    return taxi == NULL ? NULL : dynamic_cast<Taxi *>(taxi);
}

bool CollectTaxi(const KR_ObjectID object, void *user)
{
    TaxiRoster *roster = static_cast<TaxiRoster *>(user);
    Taxi *taxi = ResolveTaxi(roster->context, object);
    const char *name = roster->context == NULL
                           ? NULL
                           : roster->context->searchObject(object);
    if (taxi == NULL || name == NULL || name[0] == '\0' ||
        std::strlen(name) > kMaximumNameBytes ||
        (roster->requireReady && !taxi->runtimeReady()))
    {
        if (taxi == NULL)
            Fail("Taxi roster contains an unresolved owner");
        else if (name == NULL || name[0] == '\0')
            Fail("Taxi roster owner has no symbolic name");
        else if (std::strlen(name) > kMaximumNameBytes)
            Fail("Taxi roster owner name exceeds the codec limit");
        else
            Fail(std::string("Taxi owner is not runtime-ready: ") + name);
        roster->valid = false;
        return false;
    }
    TaxiRosterEntry entry;
    entry.name = name;
    entry.taxi = taxi;
    roster->entries.push_back(entry);
    return true;
}

bool TaxiEntryLess(const TaxiRosterEntry &left,
                   const TaxiRosterEntry &right)
{
    return left.name < right.name;
}

bool CollectRoster(SimulationContext *context, bool requireReady,
                   TaxiRoster *roster)
{
    if (context == NULL)
        return Fail("Taxi roster context is null");
    if (roster == NULL)
        return Fail("Taxi roster output is null");
    if (g_arena.getContext() != context)
        return Fail("Taxi roster belongs to another Arena context");
    roster->context = context;
    roster->entries.clear();
    roster->valid = true;
    roster->requireReady = requireReady;
    // Small persistence-only fixtures intentionally omit the Taxi table.  An
    // absent table is a valid empty roster; a non-empty TXI1 cannot be created
    // there because its later capacity/dependency preflight still fails shut.
    if (g_arena.searchSeanceClassTable("Taxi") == ct_NULLID)
        return true;
    g_arena.userFind("Taxi", CollectTaxi, roster);
    if (!roster->valid)
        return false;
    // Retail scripts reuse symbolic Taxi names. Preserve the class-table slot
    // order inside each equal-name group so that an occurrence ordinal, rather
    // than an invalid uniqueness assumption, identifies the owner.
    std::stable_sort(roster->entries.begin(), roster->entries.end(),
                     TaxiEntryLess);
    return true;
}

bool CaptureMovingEvent(SimulationContext *context, Taxi *taxi,
                        StableTaxiEvent *stable)
{
    if (context == NULL || taxi == NULL || stable == NULL)
        return false;
    KR_Event events[2];
    const int count = context->copyEvents(
        t_EVC_MOVING, taxi->getObjectID(), events, 2);
    if (count < 0 || count > 1)
        return Fail("Taxi owns duplicate private moving events");
    stable->present = count == 1;
    stable->timeStamp = count == 1 ? events[0].timeStamp : 0.0;
    if (count == 1 &&
        (events[0].destination != taxi->getObjectID() ||
         !std::isfinite(events[0].timeStamp) || events[0].timeStamp < 0.1))
        return Fail("Taxi private moving event is invalid");
    return true;
}

bool CaptureRecord(SimulationContext *context,
                   const TaxiRosterEntry &entry,
                   StableTaxiRecord *record)
{
    Taxi *taxi = entry.taxi;
    const char *attribute = taxi == NULL
                                ? NULL
                                : context->searchObject(
                                      taxi->taxiAttributeID());
    if (taxi == NULL || record == NULL || attribute == NULL ||
        attribute[0] == '\0' || std::strlen(attribute) > kMaximumNameBytes)
        return Fail("Taxi symbolic attribute is unavailable");
    record->name = entry.name;
    record->attribute = attribute;
    record->audibleThisFrame = taxi->m_audibleThisFrame != 0 ? 1 : 0;
    record->visible = taxi->m_isVisible != 0 ? 1 : 0;
    record->lastMoveTimeStamp = taxi->m_lastMoveTimeStamp;
    record->position = taxi->taxiPos();
    record->damage = taxi->m_damage;
    record->bullets = taxi->m_bulletCnt;
    record->direction = taxi->GetDir();
    record->storedDirection = taxi->m_taxiDir;
    return CaptureMovingEvent(context, taxi, &record->moving);
}

bool ValidateRecord(const StableTaxiRecord &record)
{
    return !record.name.empty() && record.name.size() <= kMaximumNameBytes &&
           !record.attribute.empty() &&
           record.attribute.size() <= kMaximumNameBytes &&
           record.name.find('\0') == std::string::npos &&
           record.attribute.find('\0') == std::string::npos &&
           (record.audibleThisFrame == 0 ||
            record.audibleThisFrame == 1) &&
           (record.visible == 0 || record.visible == 1) &&
           std::isfinite(record.lastMoveTimeStamp) &&
           record.lastMoveTimeStamp >= 0.0 && FiniteVector(record.position) &&
           std::isfinite(record.damage) && record.bullets >= 0 &&
           FiniteMatrix(record.direction) &&
           FiniteMatrix(record.storedDirection) &&
           (!record.moving.present ||
            (std::isfinite(record.moving.timeStamp) &&
             record.moving.timeStamp >= 0.1));
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

void PutString(std::vector<unsigned char> *bytes, const std::string &value)
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

void PutMatrix(std::vector<unsigned char> *bytes, const CFMatrix3x4 &value)
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
    if (size == 0)
        value->clear();
    else
        value->assign(reinterpret_cast<const char *>(&bytes[*offset]), size);
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
               const StableTaxiRecord &record)
{
    if (!ValidateRecord(record))
        return false;
    PutString(bytes, record.name);
    PutString(bytes, record.attribute);
    PutU32(bytes, static_cast<std::uint32_t>(record.audibleThisFrame));
    PutU32(bytes, static_cast<std::uint32_t>(record.visible));
    PutDouble(bytes, record.lastMoveTimeStamp);
    PutVector(bytes, record.position);
    PutDouble(bytes, record.damage);
    PutU32(bytes, static_cast<std::uint32_t>(record.bullets));
    PutMatrix(bytes, record.direction);
    PutMatrix(bytes, record.storedDirection);
    PutU32(bytes, record.moving.present ? 1u : 0u);
    PutDouble(bytes, record.moving.timeStamp);
    return true;
}

bool GetRecord(const std::vector<unsigned char> &bytes, std::size_t *offset,
               StableTaxiRecord *record)
{
    std::uint32_t audible = 0, visible = 0, bullets = 0, moving = 0;
    if (record == NULL || !GetString(bytes, offset, &record->name) ||
        !GetString(bytes, offset, &record->attribute) ||
        !GetU32(bytes, offset, &audible) ||
        !GetU32(bytes, offset, &visible) ||
        !GetDouble(bytes, offset, &record->lastMoveTimeStamp) ||
        !GetVector(bytes, offset, &record->position) ||
        !GetDouble(bytes, offset, &record->damage) ||
        !GetU32(bytes, offset, &bullets) ||
        !GetMatrix(bytes, offset, &record->direction) ||
        !GetMatrix(bytes, offset, &record->storedDirection) ||
        !GetU32(bytes, offset, &moving) ||
        !GetDouble(bytes, offset, &record->moving.timeStamp) ||
        bullets > 0x7fffffffu || moving > 1u)
        return false;
    record->audibleThisFrame = static_cast<int>(audible);
    record->visible = static_cast<int>(visible);
    record->bullets = static_cast<int>(bullets);
    record->moving.present = moving != 0;
    return ValidateRecord(*record);
}

bool EncodeRecords(const std::vector<StableTaxiRecord> &records,
                   std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumTaxis)
        return false;
    bytes->clear();
    PutU32(bytes, kTaxiMagic);
    PutU32(bytes, kTaxiVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (index != 0 && records[index - 1].name > records[index].name)
            return Fail("Taxi records are not in symbolic order");
        if (!PutRecord(bytes, records[index]))
        {
            const StableTaxiRecord &record = records[index];
            char detail[384] = {};
            std::snprintf(detail, sizeof(detail),
                          "Taxi record validation failed: %.96s "
                          "audible=%d visible=%d last=%g damage=%g "
                          "bullets=%d moving=%d/%g",
                          record.name.c_str(), record.audibleThisFrame,
                          record.visible, record.lastMoveTimeStamp,
                          record.damage, record.bullets,
                          record.moving.present ? 1 : 0,
                          record.moving.timeStamp);
            return Fail(detail);
        }
    }
    return true;
}

bool DecodeRecords(const std::vector<unsigned char> &bytes,
                   std::vector<StableTaxiRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) || magic != kTaxiMagic ||
        version != kTaxiVersion || count > kMaximumTaxis)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableTaxiRecord record;
        if (!GetRecord(bytes, &offset, &record) ||
            (index != 0 && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

bool RosterMatches(const TaxiRoster &roster,
                   const std::vector<StableTaxiRecord> &records)
{
    if (roster.entries.size() != records.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (roster.entries[index].name != records[index].name)
            return false;
    return true;
}

bool SendTaxiStart(Taxi *taxi, const KR_ObjectID &attribute,
                   const CFVector3 &position)
{
    if (taxi == NULL)
        return false;
    KR_Event event;
    event.label = 0x139A;
    event.destination = taxi->getObjectID();
    event.source = taxi->getObjectID();
    event.timeStamp = 0.1;
    event.data.open(EDO_WRITE)
              .putObjectID(attribute)
              .putDouble(position.x)
              .putDouble(position.y)
              .putDouble(position.z)
              .putDouble(0.0)
              .close();
    return taxi->receiveEvent(event) == 1;
}

bool ApplyRecord(SimulationContext *context, Taxi *taxi,
                 const StableTaxiRecord &record)
{
    if (context == NULL || taxi == NULL ||
        !context->isExist(record.attribute.c_str()))
        return false;
    KR_ObjectID attribute = context->searchObject(record.attribute.c_str());
    if (__attrTaxiTable.searchAttribute(attribute) == NULL ||
        !SendTaxiStart(taxi, attribute, record.position))
        return false;
    while (context->removeEvent(t_EVC_MOVING, taxi->getObjectID()) == 1)
    {
    }
    taxi->m_damage = record.damage;
    taxi->m_bulletCnt = record.bullets;
    taxi->m_taxiDir = record.storedDirection;
    taxi->SetDir(record.direction);
    taxi->setPosition(record.position);
    taxi->m_audibleThisFrame = record.audibleThisFrame;
    taxi->m_isVisible = record.visible;
    taxi->m_lastMoveTimeStamp = record.lastMoveTimeStamp;
    if (record.moving.present)
    {
        KR_Event moving;
        moving.label = t_EVC_MOVING;
        moving.source = taxi->getObjectID();
        moving.destination = taxi->getObjectID();
        moving.timeStamp = record.moving.timeStamp;
        context->addEvent(moving);
    }
    return taxi->runtimeReady();
}

void RemoveMovingEvents(SimulationContext *context,
                        const KR_ObjectID &object)
{
    while (context != NULL &&
           context->removeEvent(t_EVC_MOVING, object) == 1)
    {
    }
}

} // namespace

void TaxiActiveWorldState_Link()
{
}

const char *TaxiActiveWorldState_LastFailure()
{
    return g_lastFailure.c_str();
}

int TaxiActiveWorldState_LiveCount(SimulationContext *context)
{
    TaxiRoster roster = {};
    return CollectRoster(context, false, &roster)
               ? static_cast<int>(roster.entries.size())
               : -1;
}

int TaxiActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableTaxiRecord> records;
    if (!DecodeRecords(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        count += records[index].moving.present ? 1 : 0;
    return count;
}

unsigned long long TaxiActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!TaxiActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = kHashOffset;
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        hash ^= bytes[index];
        hash *= kHashPrime;
    }
    return hash;
}

bool TaxiActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_lastFailure.clear();
    TaxiRoster roster = {};
    if (bytes == NULL)
        return Fail("Taxi capture output is null");
    if (!CollectRoster(context, true, &roster))
        return g_lastFailure.empty()
                   ? Fail("Taxi roster collection failed")
                   : false;
    std::vector<StableTaxiRecord> records;
    records.reserve(roster.entries.size());
    for (std::size_t index = 0; index < roster.entries.size(); ++index)
    {
        StableTaxiRecord record;
        if (!CaptureRecord(context, roster.entries[index], &record))
            return false;
        records.push_back(record);
    }
    if (!EncodeRecords(records, bytes))
        return g_lastFailure.empty()
                   ? Fail("Taxi roster encoding failed")
                   : false;
    return true;
}

bool TaxiActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableTaxiRecord> records;
    return DecodeRecords(bytes, &records);
}

bool TaxiActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return TaxiActiveWorldState_ValidateStable(bytes) &&
           TaxiActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool TaxiActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableTaxiRecord> records;
    TaxiRoster roster = {};
    if (context == NULL || owners == NULL || !owners->empty() ||
        !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, true, &roster) ||
        !RosterMatches(roster, records))
        return false;
    for (std::size_t index = 0; index < roster.entries.size(); ++index)
        owners->push_back(roster.entries[index].taxi->getObjectID());
    return true;
}

bool TaxiActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableTaxiRecord> records;
    TaxiRoster roster = {};
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, false, &roster))
        return false;
    if (!roster.entries.empty())
        return RosterMatches(roster, records);
    if (records.size() > static_cast<std::size_t>(
                             TaxiSubjectState_Capacity()))
        return Fail("Taxi subject table has insufficient capacity");
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!context->isExist(records[index].attribute.c_str()) ||
            __attrTaxiTable.searchAttribute(context->searchObject(
                records[index].attribute.c_str())) == NULL)
            return Fail("TaxiAttr symbolic dependency is unavailable");
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        KR_ObjectID object = g_arena.newObject(
            "Taxi", records[index].name.c_str());
        if (IsNul(object))
        {
            TaxiActiveWorldState_RemoveStableOwners(context, created);
            return Fail("Taxi owner allocation failed");
        }
        created->push_back(object);
    }
    TaxiRoster restored = {};
    if (!CollectRoster(context, false, &restored) ||
        !RosterMatches(restored, records))
    {
        TaxiActiveWorldState_RemoveStableOwners(context, created);
        return Fail("Taxi symbolic owner allocation did not match");
    }
    return true;
}

bool TaxiActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableTaxiRecord> records;
    TaxiRoster roster = {};
    if (context == NULL || !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, false, &roster) ||
        !RosterMatches(roster, records))
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ApplyRecord(context, roster.entries[index].taxi,
                         records[index]))
            return Fail("Taxi symbolic reconstruction failed");
    return TaxiActiveWorldState_MatchesStable(context, bytes);
}

void TaxiActiveWorldState_RemoveStableOwners(
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
