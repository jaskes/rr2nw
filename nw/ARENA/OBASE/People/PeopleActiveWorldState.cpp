#include "PeopleActiveWorldState.h"

#include "PEOPLE.H"
#include "PeopleSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "i/commander.i"
#include "i/route.i"
#include "kernel/h/context.h"
#include "message/peopmsg.h"

namespace
{

const std::uint32_t kPeopleMagic = 0x314f4550u; // PEO1
const std::uint32_t kPeopleVersion = 1u;
const std::size_t kMaximumPeople = 4096;
const int kSchedulerLabels[] = {
    pe_EVC_MOVE,
    pe_EVC_NEXTNODE,
    pe_EVC_FIND_ENEMY,
    pe_EV_STARTSHOW,
    pe_EV_SETAUTOANIM,
    pe_EV_STARTMOVE};
std::string g_lastFailure;

bool Fail(const std::string &message)
{
    g_lastFailure = message;
    return false;
}

struct StablePeopleState
{
    int state;
    std::string enemy;
    int enemyPeopleOrdinal;
};

struct StablePeopleEvent
{
    int label;
    double timeStamp;
};

struct StablePeopleRecord
{
    std::string name;
    std::string attribute;
    std::string route;
    std::string commander;
    int audibleThisFrame;
    int visible;
    double lastMoveTimeStamp;
    CFVector3 position;
    int currentNode;
    double damage;
    CFVector3 direction;
    CFVector3 nextNode;
    double previousTime;
    double rotateOx;
    double rotateOy;
    double rotateOz;
    double positionIncrement;
    double horizontalAngle;
    double previousShootTime;
    double correctScale;
    int killed;
    double killStartPhase;
    double killDeltaVerticalAngle;
    double killTime;
    int deleted;
    double lastDamageTime;
    CFVector3 returnPoint;
    int notCreated;
    CFVector3 minimumPosition;
    CFVector3 maximumPosition;
    std::vector<StablePeopleState> states;
    CFVector3 lastMovePosition;
    double lastMoveDeltaTime;
    int previousStartShoot;
    int stopped;
    int closeCollision;
    int startBackSpaceNode;
    double startMoveDelay;
    std::vector<StablePeopleEvent> events;

    StablePeopleRecord()
        : audibleThisFrame(0), visible(0), lastMoveTimeStamp(0.0),
          position(0.0, 0.0, 0.0), currentNode(0), damage(0.0),
          direction(0.0, 0.0, 0.0), nextNode(0.0, 0.0, 0.0),
          previousTime(0.0), rotateOx(0.0), rotateOy(0.0), rotateOz(0.0),
          positionIncrement(0.0), horizontalAngle(0.0),
          previousShootTime(0.0), correctScale(1.0), killed(KILL_NONE),
          killStartPhase(0.0), killDeltaVerticalAngle(0.0), killTime(0.0),
          deleted(0), lastDamageTime(0.0), returnPoint(0.0, 0.0, 0.0),
          notCreated(0), minimumPosition(0.0, 0.0, 0.0),
          maximumPosition(0.0, 0.0, 0.0),
          lastMovePosition(0.0, 0.0, 0.0), lastMoveDeltaTime(0.0),
          previousStartShoot(0), stopped(0), closeCollision(0),
          startBackSpaceNode(-1), startMoveDelay(0.0)
    {
    }
};

struct PeopleRoster
{
    SimulationContext *context;
    std::vector<People *> people;
    bool valid;
};

struct ResolvedPeopleRecord
{
    People *people;
    KR_ObjectID attribute;
    KR_ObjectID route;
    KR_ObjectID commander;
    std::vector<KR_ObjectID> enemies;
};

bool IsNul(const KR_ObjectID &object)
{
    KR_ObjectID copy = object;
    return copy.isNUL() != FALSE;
}

bool FiniteVector(const SFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool IsBool(int value)
{
    return value == 0 || value == 1;
}

std::string ObjectName(SimulationContext *context, const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object))
        return std::string();
    const char *name = context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

People *ResolvePeople(SimulationContext *context, const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object) || !context->isExist(object))
        return NULL;
    IUnit *unit = static_cast<IUnit *>(
        context->queryInterface(object, IUnitIID));
    return unit == NULL ? NULL : dynamic_cast<People *>(unit);
}

bool CollectPeople(const KR_ObjectID object, void *user)
{
    PeopleRoster *roster = static_cast<PeopleRoster *>(user);
    People *people = roster == NULL ? NULL :
        ResolvePeople(roster->context, object);
    if (people == NULL || roster->context->searchObject(object) == NULL)
    {
        if (roster != NULL)
            roster->valid = false;
        return false;
    }
    roster->people.push_back(people);
    return true;
}

bool CollectRoster(SimulationContext *context, PeopleRoster *roster)
{
    if (context == NULL || roster == NULL)
        return false;
    roster->context = context;
    roster->valid = true;
    roster->people.clear();
    const ct_ClassTableID table = g_arena.searchSeanceClassTable("People");
    if (table == ct_NULLID)
        return true;
    g_arena.userFind(table, CollectPeople, roster);
    if (!roster->valid)
        return false;
    std::sort(roster->people.begin(), roster->people.end(),
              [context](People *left, People *right) {
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

int PeopleOrdinal(const PeopleRoster &roster, const People *target,
                  const std::string &name)
{
    int ordinal = 0;
    for (std::size_t index = 0; index < roster.people.size(); ++index)
    {
        People *candidate = roster.people[index];
        const std::string candidateName = ObjectName(
            roster.context, candidate->getObjectID());
        if (candidateName != name)
            continue;
        if (candidate == target)
            return ordinal;
        ++ordinal;
    }
    return -1;
}

People *PeopleAtOrdinal(const PeopleRoster &roster, const std::string &name,
                        int ordinal)
{
    if (ordinal < 0)
        return NULL;
    int current = 0;
    for (std::size_t index = 0; index < roster.people.size(); ++index)
    {
        People *candidate = roster.people[index];
        if (ObjectName(roster.context, candidate->getObjectID()) != name)
            continue;
        if (current == ordinal)
            return candidate;
        ++current;
    }
    return NULL;
}

bool RosterMatchesRecords(const PeopleRoster &roster,
                          const std::vector<StablePeopleRecord> &records)
{
    if (roster.people.size() != records.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (ObjectName(roster.context, roster.people[index]->getObjectID()) !=
            records[index].name)
            return false;
    return true;
}

bool IsSchedulerLabel(int label)
{
    for (std::size_t index = 0;
         index < sizeof(kSchedulerLabels) / sizeof(kSchedulerLabels[0]);
         ++index)
        if (kSchedulerLabels[index] == label)
            return true;
    return false;
}

bool CaptureEvents(SimulationContext *context, const KR_ObjectID &owner,
                   std::vector<StablePeopleEvent> *events)
{
    if (context == NULL || events == NULL)
        return false;
    events->clear();
    for (std::size_t index = 0;
         index < sizeof(kSchedulerLabels) / sizeof(kSchedulerLabels[0]);
         ++index)
    {
        KR_Event copied[2];
        const int count = context->copyEvents(
            kSchedulerLabels[index], owner, copied, 2);
        if (count < 0 || count > 1)
        {
            char message[128] = {};
            std::snprintf(message, sizeof(message),
                          "scheduler label %d count %d",
                          kSchedulerLabels[index], count);
            return Fail(message);
        }
        if (count == 1)
        {
            // The retail start handler reuses its incoming KR_Event when it
            // schedules STARTMOVE/STARTSHOW.  That leaves the already-read
            // start payload attached even though none of the six private
            // scheduler handlers consumes event.data.  PEO1 deliberately
            // canonicalizes this inert tail instead of persisting it.
            if (copied[0].destination != owner ||
                !std::isfinite(copied[0].timeStamp) ||
                copied[0].timeStamp < 0.1)
            {
                char message[160] = {};
                std::snprintf(message, sizeof(message),
                              "scheduler label %d destination/time invalid "
                              "(dest=%ld owner=%ld time=%.17g)",
                              copied[0].label, copied[0].destination.id,
                              owner.id, copied[0].timeStamp);
                return Fail(message);
            }
            StablePeopleEvent event = {};
            event.label = copied[0].label;
            event.timeStamp = copied[0].timeStamp;
            events->push_back(event);
        }
    }
    std::sort(events->begin(), events->end(),
              [](const StablePeopleEvent &left,
                 const StablePeopleEvent &right) {
                  return left.label < right.label;
              });
    return true;
}

void RemoveSchedulerEvents(SimulationContext *context,
                           const KR_ObjectID &owner)
{
    if (context == NULL)
        return;
    for (std::size_t index = 0;
         index < sizeof(kSchedulerLabels) / sizeof(kSchedulerLabels[0]);
         ++index)
        context->removeEvent(kSchedulerLabels[index], owner);
}

bool CaptureRecord(SimulationContext *context, const PeopleRoster &roster,
                   People *people, StablePeopleRecord *record)
{
    if (context == NULL || people == NULL || record == NULL ||
        !people->stableReferencesReady())
        return Fail("People runtime references are not ready");
    record->name = ObjectName(context, people->getObjectID());
    record->attribute = ObjectName(context, people->m_peopleAttrID);
    record->route = ObjectName(context, people->m_routeID);
    record->commander = ObjectName(context, people->m_commanderID);
    if (record->name.empty() || record->attribute.empty() ||
        record->route.empty())
        return Fail("People symbolic owner/attribute/route name is missing");
    record->audibleThisFrame = people->m_audibleThisFrame;
    record->visible = people->m_isVisible;
    record->lastMoveTimeStamp = people->m_lastMoveTimeStamp;
    record->position = people->getPosition();
    record->currentNode = people->m_curNode;
    record->damage = people->m_damage;
    record->direction = people->m_dir;
    record->nextNode = people->m_nextNode;
    record->previousTime = people->m_prevTime;
    record->rotateOx = people->m_rotateOx;
    record->rotateOy = people->m_rotateOy;
    record->rotateOz = people->m_rotateOz;
    record->positionIncrement = people->m_calcPosInc;
    record->horizontalAngle = people->m_hAngle;
    record->previousShootTime = people->m_prevShootTime;
    record->correctScale = people->m_correctScale;
    record->killed = people->m_killed;
    record->killStartPhase = people->m_killed == KILL_NONE
                                 ? 0.0 : people->m_kill_startPhase;
    record->killDeltaVerticalAngle = people->m_killed == KILL_NONE
                                         ? 0.0 : people->m_kill_deltaVAngle;
    record->killTime = people->m_killed == KILL_NONE
                           ? 0.0 : people->m_kill_time;
    record->deleted = people->m_deleted;
    record->lastDamageTime = people->m_lastDamageTime;
    record->returnPoint = people->m_stateSP <= 1
                              ? CFVector3(0.0, 0.0, 0.0)
                              : people->m_returnPoint;
    record->notCreated = people->m_isNotCreate;
    // These two January fields are never read by the released People code and
    // are not initialized by addNotify. Canonical zeroes keep PEO1 free from
    // allocator residue while preserving every state field with behaviour.
    record->minimumPosition = CFVector3(0.0, 0.0, 0.0);
    record->maximumPosition = CFVector3(0.0, 0.0, 0.0);
    if (people->m_stateSP <= 0 || people->m_stateSP > PeopleData::MAX_STATE)
        return Fail("People state stack depth is invalid");
    record->states.clear();
    for (int index = 0; index < people->m_stateSP; ++index)
    {
        StablePeopleState state = {};
        state.state = people->m_state[index];
        state.enemy = ObjectName(context, people->m_enemyID[index]);
        state.enemyPeopleOrdinal = -1;
        if (!IsNul(people->m_enemyID[index]) && state.enemy.empty())
            return Fail("People active enemy has no symbolic name");
        if (!IsNul(people->m_enemyID[index]))
        {
            People *enemy = ResolvePeople(context, people->m_enemyID[index]);
            if (enemy != NULL)
            {
                state.enemyPeopleOrdinal =
                    PeopleOrdinal(roster, enemy, state.enemy);
                if (state.enemyPeopleOrdinal < 0)
                    return Fail("People active enemy has no stable ordinal");
            }
        }
        record->states.push_back(state);
    }
    record->lastMovePosition = people->m_lastMovePos;
    record->lastMoveDeltaTime = people->m_lastMoveDeltaT;
    record->previousStartShoot = people->m_prevStartShoot;
    record->stopped = people->m_stoped;
    record->closeCollision = people->m_isClz;
    record->startBackSpaceNode = people->m_startBackSpaceNode;
    record->startMoveDelay = people->m_startMoveDelay;
    return CaptureEvents(context, people->getObjectID(), &record->events);
}

bool CollectRecords(SimulationContext *context,
                    std::vector<StablePeopleRecord> *records)
{
    PeopleRoster roster = {};
    if (records == NULL || !CollectRoster(context, &roster))
        return false;
    records->clear();
    for (std::size_t index = 0; index < roster.people.size(); ++index)
    {
        StablePeopleRecord record;
        if (!CaptureRecord(context, roster, roster.people[index], &record))
            return false;
        records->push_back(record);
    }
    return true;
}

void PutU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    bytes->push_back(static_cast<unsigned char>(value));
    bytes->push_back(static_cast<unsigned char>(value >> 8));
    bytes->push_back(static_cast<unsigned char>(value >> 16));
    bytes->push_back(static_cast<unsigned char>(value >> 24));
}

void PutI32(std::vector<unsigned char> *bytes, int value)
{
    PutU32(bytes, static_cast<std::uint32_t>(value));
}

void PutDouble(std::vector<unsigned char> *bytes, double value)
{
    std::uint64_t encoded = 0;
    std::memcpy(&encoded, &value, sizeof(encoded));
    for (int shift = 0; shift < 64; shift += 8)
        bytes->push_back(static_cast<unsigned char>(encoded >> shift));
}

bool GetU32(const std::vector<unsigned char> &bytes, std::size_t *offset,
            std::uint32_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 4)
        return false;
    *value = static_cast<std::uint32_t>(bytes[*offset]) |
             (static_cast<std::uint32_t>(bytes[*offset + 1]) << 8) |
             (static_cast<std::uint32_t>(bytes[*offset + 2]) << 16) |
             (static_cast<std::uint32_t>(bytes[*offset + 3]) << 24);
    *offset += 4;
    return true;
}

bool GetI32(const std::vector<unsigned char> &bytes, std::size_t *offset,
            int *value)
{
    std::uint32_t encoded = 0;
    if (value == NULL || !GetU32(bytes, offset, &encoded))
        return false;
    *value = static_cast<int>(encoded);
    return true;
}

bool GetDouble(const std::vector<unsigned char> &bytes, std::size_t *offset,
               double *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 8)
        return false;
    std::uint64_t encoded = 0;
    for (int shift = 0; shift < 64; shift += 8)
        encoded |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
    std::memcpy(value, &encoded, sizeof(encoded));
    return true;
}

bool PutString(std::vector<unsigned char> *bytes, const std::string &value)
{
    if (bytes == NULL || value.size() > MAX_SYMBOLIC_LENGHT)
        return false;
    PutU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
}

bool GetString(const std::vector<unsigned char> &bytes, std::size_t *offset,
               std::string *value)
{
    std::uint32_t size = 0;
    if (value == NULL || !GetU32(bytes, offset, &size) ||
        size > MAX_SYMBOLIC_LENGHT || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    value->assign(size == 0 ? "" :
                      reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return true;
}

void PutVector(std::vector<unsigned char> *bytes, const SFVector3 &value)
{
    PutDouble(bytes, value.x);
    PutDouble(bytes, value.y);
    PutDouble(bytes, value.z);
}

bool GetVector(const std::vector<unsigned char> &bytes, std::size_t *offset,
               SFVector3 *value)
{
    return value != NULL && GetDouble(bytes, offset, &value->x) &&
           GetDouble(bytes, offset, &value->y) &&
           GetDouble(bytes, offset, &value->z);
}

void PutBool(std::vector<unsigned char> *bytes, int value)
{
    PutU32(bytes, value != 0 ? 1u : 0u);
}

bool GetBool(const std::vector<unsigned char> &bytes, std::size_t *offset,
             int *value)
{
    std::uint32_t encoded = 0;
    if (value == NULL || !GetU32(bytes, offset, &encoded) || encoded > 1u)
        return false;
    *value = static_cast<int>(encoded);
    return true;
}

bool PutRecord(std::vector<unsigned char> *bytes,
               const StablePeopleRecord &record)
{
    if (!PutString(bytes, record.name) ||
        !PutString(bytes, record.attribute) ||
        !PutString(bytes, record.route) ||
        !PutString(bytes, record.commander))
        return false;
    PutBool(bytes, record.audibleThisFrame);
    PutBool(bytes, record.visible);
    PutDouble(bytes, record.lastMoveTimeStamp);
    PutVector(bytes, record.position);
    PutI32(bytes, record.currentNode);
    PutDouble(bytes, record.damage);
    PutVector(bytes, record.direction);
    PutVector(bytes, record.nextNode);
    PutDouble(bytes, record.previousTime);
    PutDouble(bytes, record.rotateOx);
    PutDouble(bytes, record.rotateOy);
    PutDouble(bytes, record.rotateOz);
    PutDouble(bytes, record.positionIncrement);
    PutDouble(bytes, record.horizontalAngle);
    PutDouble(bytes, record.previousShootTime);
    PutDouble(bytes, record.correctScale);
    PutI32(bytes, record.killed);
    PutDouble(bytes, record.killStartPhase);
    PutDouble(bytes, record.killDeltaVerticalAngle);
    PutDouble(bytes, record.killTime);
    PutBool(bytes, record.deleted);
    PutDouble(bytes, record.lastDamageTime);
    PutVector(bytes, record.returnPoint);
    PutBool(bytes, record.notCreated);
    PutVector(bytes, record.minimumPosition);
    PutVector(bytes, record.maximumPosition);
    PutU32(bytes, static_cast<std::uint32_t>(record.states.size()));
    for (std::size_t index = 0; index < record.states.size(); ++index)
    {
        PutI32(bytes, record.states[index].state);
        if (!PutString(bytes, record.states[index].enemy))
            return false;
        PutI32(bytes, record.states[index].enemyPeopleOrdinal);
    }
    PutVector(bytes, record.lastMovePosition);
    PutDouble(bytes, record.lastMoveDeltaTime);
    PutI32(bytes, record.previousStartShoot);
    PutBool(bytes, record.stopped);
    PutBool(bytes, record.closeCollision);
    PutI32(bytes, record.startBackSpaceNode);
    PutDouble(bytes, record.startMoveDelay);
    PutU32(bytes, static_cast<std::uint32_t>(record.events.size()));
    for (std::size_t index = 0; index < record.events.size(); ++index)
    {
        PutI32(bytes, record.events[index].label);
        PutDouble(bytes, record.events[index].timeStamp);
    }
    return true;
}

bool GetRecord(const std::vector<unsigned char> &bytes, std::size_t *offset,
               StablePeopleRecord *record)
{
    if (record == NULL || !GetString(bytes, offset, &record->name) ||
        !GetString(bytes, offset, &record->attribute) ||
        !GetString(bytes, offset, &record->route) ||
        !GetString(bytes, offset, &record->commander) ||
        !GetBool(bytes, offset, &record->audibleThisFrame) ||
        !GetBool(bytes, offset, &record->visible) ||
        !GetDouble(bytes, offset, &record->lastMoveTimeStamp) ||
        !GetVector(bytes, offset, &record->position) ||
        !GetI32(bytes, offset, &record->currentNode) ||
        !GetDouble(bytes, offset, &record->damage) ||
        !GetVector(bytes, offset, &record->direction) ||
        !GetVector(bytes, offset, &record->nextNode) ||
        !GetDouble(bytes, offset, &record->previousTime) ||
        !GetDouble(bytes, offset, &record->rotateOx) ||
        !GetDouble(bytes, offset, &record->rotateOy) ||
        !GetDouble(bytes, offset, &record->rotateOz) ||
        !GetDouble(bytes, offset, &record->positionIncrement) ||
        !GetDouble(bytes, offset, &record->horizontalAngle) ||
        !GetDouble(bytes, offset, &record->previousShootTime) ||
        !GetDouble(bytes, offset, &record->correctScale) ||
        !GetI32(bytes, offset, &record->killed) ||
        !GetDouble(bytes, offset, &record->killStartPhase) ||
        !GetDouble(bytes, offset, &record->killDeltaVerticalAngle) ||
        !GetDouble(bytes, offset, &record->killTime) ||
        !GetBool(bytes, offset, &record->deleted) ||
        !GetDouble(bytes, offset, &record->lastDamageTime) ||
        !GetVector(bytes, offset, &record->returnPoint) ||
        !GetBool(bytes, offset, &record->notCreated) ||
        !GetVector(bytes, offset, &record->minimumPosition) ||
        !GetVector(bytes, offset, &record->maximumPosition))
        return false;
    std::uint32_t stateCount = 0;
    if (!GetU32(bytes, offset, &stateCount) ||
        stateCount == 0 || stateCount > PeopleData::MAX_STATE)
        return false;
    record->states.clear();
    for (std::uint32_t index = 0; index < stateCount; ++index)
    {
        StablePeopleState state = {};
        state.enemyPeopleOrdinal = -1;
        if (!GetI32(bytes, offset, &state.state) ||
            !GetString(bytes, offset, &state.enemy) ||
            !GetI32(bytes, offset, &state.enemyPeopleOrdinal))
            return false;
        record->states.push_back(state);
    }
    if (!GetVector(bytes, offset, &record->lastMovePosition) ||
        !GetDouble(bytes, offset, &record->lastMoveDeltaTime) ||
        !GetI32(bytes, offset, &record->previousStartShoot) ||
        !GetBool(bytes, offset, &record->stopped) ||
        !GetBool(bytes, offset, &record->closeCollision) ||
        !GetI32(bytes, offset, &record->startBackSpaceNode) ||
        !GetDouble(bytes, offset, &record->startMoveDelay))
        return false;
    std::uint32_t eventCount = 0;
    const std::size_t maximumEvents =
        sizeof(kSchedulerLabels) / sizeof(kSchedulerLabels[0]);
    if (!GetU32(bytes, offset, &eventCount) || eventCount > maximumEvents)
        return false;
    record->events.clear();
    for (std::uint32_t index = 0; index < eventCount; ++index)
    {
        StablePeopleEvent event = {};
        if (!GetI32(bytes, offset, &event.label) ||
            !GetDouble(bytes, offset, &event.timeStamp))
            return false;
        record->events.push_back(event);
    }
    return true;
}

bool ValidateRecord(const StablePeopleRecord &record)
{
    if (record.name.empty() || record.attribute.empty() ||
        record.route.empty() || !IsBool(record.audibleThisFrame) ||
        !IsBool(record.visible) || !std::isfinite(record.lastMoveTimeStamp) ||
        !FiniteVector(record.position) || record.currentNode < 0 ||
        !std::isfinite(record.damage) || !FiniteVector(record.direction) ||
        !FiniteVector(record.nextNode) || !std::isfinite(record.previousTime) ||
        !std::isfinite(record.rotateOx) || !std::isfinite(record.rotateOy) ||
        !std::isfinite(record.rotateOz) ||
        !std::isfinite(record.positionIncrement) ||
        record.positionIncrement < 0.0 ||
        !std::isfinite(record.horizontalAngle) ||
        !std::isfinite(record.previousShootTime) ||
        !std::isfinite(record.correctScale) || record.correctScale <= 0.0 ||
        record.killed < KILL_NONE || record.killed > KILL_SINK ||
        !std::isfinite(record.killStartPhase) ||
        !std::isfinite(record.killDeltaVerticalAngle) ||
        !std::isfinite(record.killTime) || !IsBool(record.deleted) ||
        !std::isfinite(record.lastDamageTime) ||
        !FiniteVector(record.returnPoint) || !IsBool(record.notCreated) ||
        !FiniteVector(record.minimumPosition) ||
        !FiniteVector(record.maximumPosition) || record.states.empty() ||
        record.states.size() > PeopleData::MAX_STATE ||
        !FiniteVector(record.lastMovePosition) ||
        !std::isfinite(record.lastMoveDeltaTime) ||
        record.previousStartShoot < 0 || !IsBool(record.stopped) ||
        !IsBool(record.closeCollision) || record.startBackSpaceNode < -1 ||
        !std::isfinite(record.startMoveDelay) || record.startMoveDelay < 0.0)
        return false;
    for (std::size_t index = 0; index < record.states.size(); ++index)
    {
        const StablePeopleState &state = record.states[index];
        if (state.state < pe_STATE_DEFAULT || state.state > pe_STATE_BACK ||
            state.enemyPeopleOrdinal < -1 ||
            (state.state == pe_STATE_DEFAULT &&
             (!state.enemy.empty() || state.enemyPeopleOrdinal != -1)) ||
            (state.state != pe_STATE_DEFAULT && state.enemy.empty()))
            return false;
    }
    if (record.states[0].state != pe_STATE_DEFAULT)
        return false;
    for (std::size_t index = 0; index < record.events.size(); ++index)
    {
        const StablePeopleEvent &event = record.events[index];
        if (!IsSchedulerLabel(event.label) ||
            !std::isfinite(event.timeStamp) || event.timeStamp < 0.1 ||
            (index != 0 && record.events[index - 1].label >= event.label))
            return false;
    }
    return true;
}

bool EncodeRecords(const std::vector<StablePeopleRecord> &records,
                   std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumPeople)
        return false;
    bytes->clear();
    PutU32(bytes, kPeopleMagic);
    PutU32(bytes, kPeopleVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!ValidateRecord(records[index]))
        {
            char message[512] = {};
            const StablePeopleRecord &record = records[index];
            std::snprintf(
                message, sizeof(message),
                "record %.96s validation failed bool=%d/%d/%d/%d/%d/%d "
                "node=%d inc=%g scale=%g killed=%d states=%u prevShoot=%d "
                "back=%d delay=%g",
                record.name.c_str(), record.audibleThisFrame, record.visible,
                record.deleted, record.notCreated, record.stopped,
                record.closeCollision, record.currentNode,
                record.positionIncrement, record.correctScale, record.killed,
                static_cast<unsigned>(record.states.size()),
                record.previousStartShoot, record.startBackSpaceNode,
                record.startMoveDelay);
            return Fail(message);
        }
        if (index != 0 && records[index - 1].name > records[index].name)
            return Fail("People records are not in symbolic order");
        if (!PutRecord(bytes, records[index]))
            return Fail("People record encoding failed");
    }
    return true;
}

bool DecodeRecords(const std::vector<unsigned char> &bytes,
                   std::vector<StablePeopleRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) || magic != kPeopleMagic ||
        version != kPeopleVersion || count > kMaximumPeople)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StablePeopleRecord record;
        if (!GetRecord(bytes, &offset, &record) || !ValidateRecord(record) ||
            (index != 0 && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

bool ResolveRoute(SimulationContext *context, const std::string &name,
                  KR_ObjectID *object, IRouteObject **route)
{
    if (context == NULL || object == NULL || name.empty() ||
        !context->isExist(name.c_str()))
        return false;
    *object = context->searchObject(name.c_str());
    IRouteObject *resolved = static_cast<IRouteObject *>(
        context->queryInterface(*object, IRouteObjectIID));
    if (route != NULL)
        *route = resolved;
    return resolved != NULL && resolved->GetNodeCnt() >= 2;
}

bool ResolveOptionalInterface(SimulationContext *context,
                              const std::string &name, int interfaceID,
                              KR_ObjectID *object)
{
    if (object == NULL)
        return false;
    *object = KR_ObjectID::NUL();
    if (name.empty())
        return true;
    if (context == NULL || !context->isExist(name.c_str()))
        return false;
    *object = context->searchObject(name.c_str());
    return context->queryInterface(*object, interfaceID) != NULL;
}

bool ResolveRecord(SimulationContext *context, const PeopleRoster &roster,
                   People *owner,
                   const StablePeopleRecord &record,
                   ResolvedPeopleRecord *resolved)
{
    if (context == NULL || owner == NULL || resolved == NULL ||
        !context->isExist(record.attribute.c_str()))
        return false;
    resolved->people = owner;
    resolved->attribute = context->searchObject(record.attribute.c_str());
    IRouteObject *route = NULL;
    if (resolved->people == NULL ||
        !PeopleAttributeExists(resolved->attribute) ||
        !ResolveRoute(context, record.route, &resolved->route, &route) ||
        record.currentNode >= route->GetNodeCnt() ||
        !ResolveOptionalInterface(context, record.commander, ICommanderIID,
                                  &resolved->commander))
        return false;
    resolved->enemies.clear();
    for (std::size_t index = 0; index < record.states.size(); ++index)
    {
        const StablePeopleState &state = record.states[index];
        KR_ObjectID enemy = KR_ObjectID::NUL();
        if (state.enemyPeopleOrdinal >= 0)
        {
            People *people = PeopleAtOrdinal(
                roster, state.enemy, state.enemyPeopleOrdinal);
            if (people == NULL)
                return false;
            enemy = people->getObjectID();
        }
        else if (!ResolveOptionalInterface(context, state.enemy, IUnitIID,
                                           &enemy))
        {
            return false;
        }
        resolved->enemies.push_back(enemy);
    }
    return true;
}

bool ApplyRecord(SimulationContext *context,
                 const StablePeopleRecord &record,
                 const ResolvedPeopleRecord &resolved)
{
    People *people = resolved.people;
    if (context == NULL || people == NULL ||
        !people->restoreStableReferences(resolved.attribute, resolved.route))
        return false;
    people->m_audibleThisFrame = record.audibleThisFrame;
    people->m_isVisible = record.visible;
    people->m_lastMoveTimeStamp = record.lastMoveTimeStamp;
    people->ct_Subject::setPosition(record.position);
    people->m_curNode = record.currentNode;
    people->m_damage = record.damage;
    people->m_commanderID = resolved.commander;
    people->m_dir = record.direction;
    people->m_nextNode = record.nextNode;
    people->m_prevTime = record.previousTime;
    people->m_rotateOx = record.rotateOx;
    people->m_rotateOy = record.rotateOy;
    people->m_rotateOz = record.rotateOz;
    people->m_calcPosInc = record.positionIncrement;
    people->m_hAngle = record.horizontalAngle;
    people->m_prevShootTime = record.previousShootTime;
    people->m_correctScale = record.correctScale;
    people->m_killed = record.killed;
    people->m_kill_startPhase = record.killStartPhase;
    people->m_kill_deltaVAngle = record.killDeltaVerticalAngle;
    people->m_kill_time = record.killTime;
    people->m_deleted = record.deleted;
    people->m_lastDamageTime = record.lastDamageTime;
    people->m_returnPoint = record.returnPoint;
    people->m_isNotCreate = record.notCreated;
    people->m_minPos = record.minimumPosition;
    people->m_maxPos = record.maximumPosition;
    people->m_stateSP = static_cast<int>(record.states.size());
    for (int index = 0; index < PeopleData::MAX_STATE; ++index)
    {
        people->m_state[index] = pe_STATE_DEFAULT;
        people->m_enemyID[index] = KR_ObjectID::NUL();
    }
    for (std::size_t index = 0; index < record.states.size(); ++index)
    {
        people->m_state[index] = record.states[index].state;
        people->m_enemyID[index] = resolved.enemies[index];
    }
    people->m_lastMovePos = record.lastMovePosition;
    people->m_lastMoveDeltaT = record.lastMoveDeltaTime;
    people->m_prevStartShoot = record.previousStartShoot;
    people->m_stoped = record.stopped;
    people->m_isClz = record.closeCollision;
    people->m_startBackSpaceNode = record.startBackSpaceNode;
    people->m_startMoveDelay = record.startMoveDelay;

    RemoveSchedulerEvents(context, people->getObjectID());
    for (std::size_t index = 0; index < record.events.size(); ++index)
    {
        KR_Event event(record.events[index].label,
                       record.events[index].timeStamp,
                       people->getObjectID(), people->getObjectID());
        context->addEvent(event);
    }
    return people->stableReferencesReady();
}

void HashBytes(unsigned long long *hash, const void *data, std::size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        *hash ^= bytes[index];
        *hash *= 1099511628211ull;
    }
}

} // namespace

void PeopleActiveWorldState_Link()
{
    PeopleSubjectState_Link();
}

const char *PeopleActiveWorldState_LastFailure()
{
    return g_lastFailure.c_str();
}

int PeopleActiveWorldState_LiveCount(SimulationContext *context)
{
    PeopleRoster roster = {};
    return CollectRoster(context, &roster)
               ? static_cast<int>(roster.people.size()) : -1;
}

int PeopleActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StablePeopleRecord> records;
    if (!DecodeRecords(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        count += static_cast<int>(records[index].events.size());
    return count;
}

unsigned long long PeopleActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!PeopleActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = 14695981039346656037ull;
    if (!bytes.empty())
        HashBytes(&hash, &bytes[0], bytes.size());
    return hash;
}

bool PeopleActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_lastFailure.clear();
    std::vector<StablePeopleRecord> records;
    if (!CollectRecords(context, &records))
    {
        if (g_lastFailure.empty())
            Fail("People roster collection failed");
        return false;
    }
    if (!EncodeRecords(records, bytes))
    {
        if (g_lastFailure.empty())
            Fail("People roster encoding failed");
        return false;
    }
    return true;
}

bool PeopleActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StablePeopleRecord> records;
    return DecodeRecords(bytes, &records);
}

bool PeopleActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return PeopleActiveWorldState_ValidateStable(bytes) &&
           PeopleActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool PeopleActiveWorldState_ProbeDetailedCaptureFailure(
    SimulationContext *context)
{
    PeopleRoster roster = {};
    std::vector<unsigned char> baseline;
    if (!CollectRoster(context, &roster) ||
        !PeopleActiveWorldState_CaptureStable(context, &baseline))
        return false;
    // Some retail levels (notably Level.07N) legitimately contain no People.
    // Their empty roster still has to be capturable, but there is no live
    // state stack on which to inject the detailed failure probe.
    if (roster.people.empty())
        return true;
    People *people = roster.people.front();
    const int stateDepth = people->m_stateSP;
    people->m_stateSP = 0;
    std::vector<unsigned char> rejected;
    const bool rejectedWithDetail =
        !PeopleActiveWorldState_CaptureStable(context, &rejected) &&
        g_lastFailure == "People state stack depth is invalid";
    people->m_stateSP = stateDepth;
    std::vector<unsigned char> restored;
    return rejectedWithDetail &&
           PeopleActiveWorldState_CaptureStable(context, &restored) &&
           restored == baseline;
}

bool PeopleActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StablePeopleRecord> records;
    PeopleRoster roster = {};
    if (context == NULL || owners == NULL || !owners->empty() ||
        !DecodeRecords(bytes, &records) || !CollectRoster(context, &roster) ||
        !RosterMatchesRecords(roster, records))
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        owners->push_back(roster.people[index]->getObjectID());
    return true;
}

bool PeopleActiveWorldState_HoldRouteReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *heldRoutes)
{
    std::vector<StablePeopleRecord> records;
    if (context == NULL || heldRoutes == NULL || !heldRoutes->empty() ||
        !DecodeRecords(bytes, &records))
        return false;
    std::vector<std::string> routes;
    for (std::size_t index = 0; index < records.size(); ++index)
        routes.push_back(records[index].route);
    std::sort(routes.begin(), routes.end());
    routes.erase(std::unique(routes.begin(), routes.end()), routes.end());
    for (std::size_t index = 0; index < routes.size(); ++index)
    {
        KR_ObjectID object;
        IRouteObject *route = NULL;
        if (!ResolveRoute(context, routes[index], &object, &route))
        {
            PeopleActiveWorldState_ReleaseRouteReferences(context,
                                                          heldRoutes);
            return false;
        }
        route->AddRef();
        heldRoutes->push_back(object);
    }
    return true;
}

void PeopleActiveWorldState_ReleaseRouteReferences(
    SimulationContext *context, std::vector<KR_ObjectID> *heldRoutes)
{
    if (heldRoutes == NULL)
        return;
    if (context != NULL)
    {
        for (std::vector<KR_ObjectID>::reverse_iterator route =
                 heldRoutes->rbegin(); route != heldRoutes->rend(); ++route)
        {
            IRouteObject *resolved = static_cast<IRouteObject *>(
                context->queryInterface(*route, IRouteObjectIID));
            if (resolved != NULL)
                resolved->DelRef();
        }
    }
    heldRoutes->clear();
}

bool PeopleActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StablePeopleRecord> records;
    PeopleRoster roster = {};
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeRecords(bytes, &records) || !CollectRoster(context, &roster))
        return false;
    if (!roster.people.empty())
        return RosterMatchesRecords(roster, records);
    const int missing = static_cast<int>(records.size());
    if (missing != 0 &&
        (g_arena.searchSeanceClassTable("People") == ct_NULLID ||
         missing > PeopleFreeObjectCount()))
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!context->isExist(records[index].attribute.c_str()) ||
            !PeopleAttributeExists(
                context->searchObject(records[index].attribute.c_str())))
            return false;
        KR_ObjectID route;
        if (!ResolveRoute(context, records[index].route, &route, NULL))
            return false;
    }
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const char *name = records[index].name.c_str();
        KR_ObjectID object = g_arena.newObject("People", name);
        if (IsNul(object))
        {
            PeopleActiveWorldState_RemoveStableOwners(context, created);
            return false;
        }
        created->push_back(object);
    }
    PeopleRoster restored = {};
    if (!CollectRoster(context, &restored) ||
        !RosterMatchesRecords(restored, records))
    {
        PeopleActiveWorldState_RemoveStableOwners(context, created);
        return false;
    }
    return true;
}

bool PeopleActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StablePeopleRecord> records;
    PeopleRoster roster = {};
    if (context == NULL || !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, &roster) ||
        !RosterMatchesRecords(roster, records))
        return false;
    std::vector<ResolvedPeopleRecord> resolved(records.size());
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ResolveRecord(context, roster, roster.people[index],
                           records[index], &resolved[index]))
            return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ApplyRecord(context, records[index], resolved[index]))
            return false;
    return PeopleActiveWorldState_MatchesStable(context, bytes);
}

void PeopleActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
    {
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            RemoveSchedulerEvents(context, *object);
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    }
    created->clear();
}
