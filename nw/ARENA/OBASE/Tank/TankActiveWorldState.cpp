#include "TankActiveWorldState.h"

#include "TANK.H"
#include "TankSubjectState.h"
#include "obase/cannon/Cannon.h"
#include "obase/cannon/CannonSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "i/commander.i"
#include "i/dynobj.i"
#include "kernel/h/context.h"
#include "message/cnmsg.h"
#include "message/unitmsg.h"

namespace
{

const std::uint32_t kTankMagic = 0x314e4154u; // TAN1
const std::uint32_t kTankVersion = 1u;
const std::size_t kMaximumTanks = 1024;
const std::size_t kMaximumString = 512;

const int kTankSchedulerLabels[] = {
    t_EVC_MOVING, t_EV_CREATE_SMOKE, t_EVC_STAY_AND_SHOOTING,
    t_EVC_MOVING_AND_SHOOTING, t_EVC_RETREATING, t_EVC_RETURN,
    UNIT_I_DRIVE, t_EVC_CHECK_ROTATE};
const int kCannonSchedulerLabels[] = {
    cn_EV_SHOOT, cn_EV_IDLEOK, cn_EV_END_SHOOTING, cn_EV_SINGLE_SHOOT,
    cn_EVC_IDLE, cn_EV_ENDOFSHOOT, cn_EVC_ROTATE,
    cn_EVCMD_ROTATE_AND_SHOOT, cn_EVCMD_DIRECT_SHOOT};

std::string g_lastFailure;

bool Fail(const std::string &message)
{
    g_lastFailure = message;
    return false;
}

struct StableReference
{
    std::string name;
    int tankOrdinal;

    StableReference() : tankOrdinal(-1) {}
};

struct StableEvent
{
    int label;
    double timeStamp;
    double first;
    double second;
    double third;
    int count;
    std::string bulletAttribute;

    StableEvent()
        : label(0), timeStamp(0.0), first(0.0), second(0.0), third(0.0),
          count(0) {}
};

struct StableCannonRecord
{
    std::string name;
    std::string attribute;
    int audibleThisFrame;
    int visible;
    double lastMoveTimeStamp;
    CFVector3 position;
    int currentState;
    CFVector3 vector;
    double horizontalAngle;
    double verticalAngle;
    double localHorizontalAngle;
    double localVerticalAngle;
    std::vector<StableEvent> events;

    StableCannonRecord()
        : audibleThisFrame(0), visible(0), lastMoveTimeStamp(0.0),
          position(0.0, 0.0, 0.0), currentState(0),
          vector(0.0, 0.0, -1.0), horizontalAngle(0.0),
          verticalAngle(0.0), localHorizontalAngle(0.0),
          localVerticalAngle(0.0) {}
};

struct StableTankRecord
{
    std::string name;
    std::string attribute;
    StableReference group;
    StableReference commander;
    StableReference attackedEnemy;
    std::string artefact;
    int audibleThisFrame;
    int visible;
    double lastMoveTimeStamp;
    CFVector3 position;
    int currentState;
    int directionCount;
    std::vector<CFVector2> directions;
    std::vector<double> directionAngles;
    CFVector2 destinationDirection;
    CFVector2 previousDirection;
    int stopMode;
    double neededPower;
    double neededAngle;
    double phase;
    int runSmoke;
    double lastAttackTime;
    int maximumDistanceScale;
    CFVector3 towerAxis;
    CFVector3 destinationPosition;
    double previousTime;
    int haveDriveEvent;
    CFVector3 normal;
    double speed;
    double power;
    double angle;
    double damage;
    double desireShoot;
    double maximumMoveDistance;
    CFVector3 previousPosition;
    int attack;
    int checkRotateCount;
    double savedHorizontalAngle;
    double savedVerticalAngle;
    int savedCount;
    int savedCannonOrdinal;
    CFVector3 lastMovePosition;
    double lastMoveDeltaTime;
    std::vector<StableEvent> events;
    std::vector<StableCannonRecord> cannons;

    StableTankRecord()
        : audibleThisFrame(0), visible(0), lastMoveTimeStamp(0.0),
          position(0.0, 0.0, 0.0), currentState(0), directionCount(0),
          destinationDirection(0.0, 1.0), previousDirection(0.0, 1.0),
          stopMode(0), neededPower(0.0), neededAngle(0.0), phase(0.0),
          runSmoke(0), lastAttackTime(0.0), maximumDistanceScale(0),
          towerAxis(0.0, 0.0, 0.0),
          destinationPosition(0.0, 0.0, 0.0), previousTime(0.0),
          haveDriveEvent(0), normal(0.0, 1.0, 0.0), speed(0.0),
          power(0.0), angle(0.0), damage(0.0), desireShoot(0.0),
          maximumMoveDistance(0.0), previousPosition(0.0, 0.0, 0.0),
          attack(0), checkRotateCount(0), savedHorizontalAngle(0.0),
          savedVerticalAngle(0.0), savedCount(0), savedCannonOrdinal(-1),
          lastMovePosition(0.0, 0.0, 0.0), lastMoveDeltaTime(0.0) {}
};

struct TankRoster
{
    SimulationContext *context;
    std::vector<Tank *> tanks;
    bool valid;
};

bool IsNul(const KR_ObjectID &object)
{
    KR_ObjectID copy = object;
    return copy.isNUL() != FALSE;
}

bool IsBool(int value) { return value == 0 || value == 1; }

bool Finite(const CFVector2 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool Finite(const SFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

std::string ObjectName(SimulationContext *context, const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object))
        return std::string();
    const char *name = context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

Tank *ResolveTank(SimulationContext *context, const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object) || !context->isExist(object))
        return NULL;
    IUnit *unit = static_cast<IUnit *>(
        context->queryInterface(object, IUnitIID));
    return unit == NULL ? NULL : dynamic_cast<Tank *>(unit);
}

Cannon *ResolveCannon(SimulationContext *context, const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object) || !context->isExist(object))
        return NULL;
    ICannon *cannon = static_cast<ICannon *>(
        context->queryInterface(object, ICannonIID));
    return cannon == NULL ? NULL : dynamic_cast<Cannon *>(cannon);
}

bool CollectTank(const KR_ObjectID object, void *user)
{
    TankRoster *roster = static_cast<TankRoster *>(user);
    Tank *tank = roster == NULL ? NULL : ResolveTank(roster->context, object);
    if (tank == NULL || roster->context->searchObject(object) == NULL)
    {
        if (roster != NULL)
            roster->valid = false;
        return false;
    }
    roster->tanks.push_back(tank);
    return true;
}

bool CollectRoster(SimulationContext *context, TankRoster *roster)
{
    if (context == NULL || roster == NULL)
        return false;
    roster->context = context;
    roster->valid = true;
    roster->tanks.clear();
    const ct_ClassTableID table = g_arena.searchSeanceClassTable("Tank");
    if (table == ct_NULLID)
        return true;
    g_arena.userFind(table, CollectTank, roster);
    if (!roster->valid)
        return false;
    std::sort(roster->tanks.begin(), roster->tanks.end(),
              [context](Tank *left, Tank *right) {
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

int TankOrdinal(const TankRoster &roster, const Tank *target,
                const std::string &name)
{
    int ordinal = 0;
    for (std::size_t index = 0; index < roster.tanks.size(); ++index)
    {
        Tank *candidate = roster.tanks[index];
        if (ObjectName(roster.context, candidate->getObjectID()) != name)
            continue;
        if (candidate == target)
            return ordinal;
        ++ordinal;
    }
    return -1;
}

Tank *TankAtOrdinal(const TankRoster &roster, const std::string &name,
                    int ordinal)
{
    int current = 0;
    if (ordinal < 0)
        return NULL;
    for (std::size_t index = 0; index < roster.tanks.size(); ++index)
    {
        Tank *candidate = roster.tanks[index];
        if (ObjectName(roster.context, candidate->getObjectID()) != name)
            continue;
        if (current++ == ordinal)
            return candidate;
    }
    return NULL;
}

StableReference CaptureReference(SimulationContext *context,
                                 const TankRoster &roster,
                                 const KR_ObjectID &object)
{
    StableReference reference;
    reference.name = ObjectName(context, object);
    Tank *tank = ResolveTank(context, object);
    if (tank != NULL)
        reference.tankOrdinal = TankOrdinal(roster, tank, reference.name);
    return reference;
}

bool RosterMatches(const TankRoster &roster,
                   const std::vector<StableTankRecord> &records)
{
    if (roster.tanks.size() != records.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (ObjectName(roster.context, roster.tanks[index]->getObjectID()) !=
                records[index].name ||
            roster.tanks[index]->m_cannons.getCount() !=
                static_cast<int>(records[index].cannons.size()))
            return false;
    return true;
}

bool IsTankSchedulerLabel(int label)
{
    for (std::size_t index = 0;
         index < sizeof(kTankSchedulerLabels) / sizeof(kTankSchedulerLabels[0]);
         ++index)
        if (kTankSchedulerLabels[index] == label)
            return true;
    return false;
}

bool IsCannonSchedulerLabel(int label)
{
    for (std::size_t index = 0;
         index < sizeof(kCannonSchedulerLabels) /
                     sizeof(kCannonSchedulerLabels[0]);
         ++index)
        if (kCannonSchedulerLabels[index] == label)
            return true;
    return false;
}

bool EventUsesBulletAttribute(int label)
{
    return label == cn_EV_SHOOT || label == cn_EV_IDLEOK ||
           label == cn_EV_SINGLE_SHOOT ||
           label == cn_EVCMD_ROTATE_AND_SHOOT ||
           label == cn_EVCMD_DIRECT_SHOOT;
}

bool ReadTankEvent(KR_Event &source, StableEvent *event)
{
    if (event == NULL)
        return false;
    event->label = source.label;
    event->timeStamp = source.timeStamp;
    if (source.label == t_EVC_MOVING ||
        source.label == t_EVC_MOVING_AND_SHOOTING ||
        source.label == t_EVC_RETREATING || source.label == t_EVC_RETURN)
        source.data.open(EDO_READ).getDouble(event->first).close();
    return true;
}

bool ReadCannonEvent(KR_Event &source, const AttributeTank &tankAttribute,
                     StableEvent *event)
{
    if (event == NULL)
        return false;
    event->label = source.label;
    event->timeStamp = source.timeStamp;
    int bulletIndex = -1;
    switch (source.label)
    {
    case cn_EV_SHOOT:
        source.data.open(EDO_READ).getInt(event->count)
            .getInt(bulletIndex).close();
        break;
    case cn_EV_IDLEOK:
        source.data.open(EDO_READ).getInt(bulletIndex)
            .getInt(event->count).close();
        break;
    case cn_EV_SINGLE_SHOOT:
        source.data.open(EDO_READ).getInt(bulletIndex).close();
        event->count = 1;
        break;
    case cn_EVC_ROTATE:
        source.data.open(EDO_READ).getDouble(event->first)
            .getDouble(event->second).getDouble(event->third).close();
        break;
    case cn_EVCMD_ROTATE_AND_SHOOT:
        source.data.open(EDO_READ)
            .descend(cn_ROTATE, 0)
              .getDouble(event->first).getDouble(event->second)
              .getDouble(event->third)
            .ascend()
            .descend(cn_SHOOT, 0)
              .getInt(event->count).getInt(bulletIndex)
            .ascend().close();
        break;
    case cn_EVCMD_DIRECT_SHOOT:
        source.data.open(EDO_READ).getDouble(event->second)
            .getDouble(event->third).getInt(bulletIndex)
            .getInt(event->count).close();
        break;
    default:
        break;
    }
    if (EventUsesBulletAttribute(source.label))
    {
        if (bulletIndex != tankAttribute.m_cacheBulletAttr)
            return Fail("Cannon scheduler uses a foreign BulletAttr index");
        event->bulletAttribute = tankAttribute.m_bulletAttr;
    }
    return true;
}

template <std::size_t N, typename Reader>
bool CaptureEvents(SimulationContext *context, const KR_ObjectID &owner,
                   const int (&labels)[N], Reader reader,
                   std::vector<StableEvent> *events)
{
    if (context == NULL || events == NULL)
        return false;
    events->clear();
    for (std::size_t index = 0; index < N; ++index)
    {
        KR_Event copied[2];
        const int count = context->copyEvents(labels[index], owner, copied, 2);
        if (count < 0 || count > 1)
            return Fail("duplicate private Tank/Cannon scheduler label");
        if (count == 1)
        {
            if (copied[0].destination != owner ||
                !std::isfinite(copied[0].timeStamp) ||
                copied[0].timeStamp < 0.0)
                return Fail("private Tank/Cannon scheduler endpoint is invalid");
            StableEvent event;
            if (!reader(copied[0], &event))
                return false;
            events->push_back(event);
        }
    }
    std::sort(events->begin(), events->end(),
              [](const StableEvent &left, const StableEvent &right) {
                  return left.label < right.label;
              });
    return true;
}

template <std::size_t N>
void RemoveEvents(SimulationContext *context, const KR_ObjectID &owner,
                  const int (&labels)[N])
{
    if (context == NULL)
        return;
    for (std::size_t index = 0; index < N; ++index)
        while (context->removeEvent(labels[index], owner) == 1) {}
}

bool CaptureRecord(SimulationContext *context, const TankRoster &roster,
                   Tank *tank, StableTankRecord *record)
{
    if (context == NULL || tank == NULL || record == NULL ||
        tank->m_attr == NULL || IsNul(tank->m_tankAttrID))
        return Fail("Tank runtime references are not ready");
    record->name = ObjectName(context, tank->getObjectID());
    record->attribute = ObjectName(context, tank->m_tankAttrID);
    record->group = CaptureReference(context, roster, tank->m_group);
    record->commander = CaptureReference(context, roster, tank->m_commander);
    record->attackedEnemy =
        CaptureReference(context, roster, tank->m_attackedEnemy);
    record->artefact = ObjectName(context, tank->m_artefactID);
    if (record->name.empty() || record->attribute.empty() ||
        (!IsNul(tank->m_group) && record->group.name.empty()) ||
        (!IsNul(tank->m_commander) && record->commander.name.empty()) ||
        (!IsNul(tank->m_attackedEnemy) && record->attackedEnemy.name.empty()) ||
        (!IsNul(tank->m_artefactID) && record->artefact.empty()))
        return Fail("Tank symbolic reference is missing");
    record->audibleThisFrame = tank->m_audibleThisFrame;
    record->visible = tank->m_isVisible;
    record->lastMoveTimeStamp = tank->m_lastMoveTimeStamp;
    record->position = tank->getPosition();
    record->currentState = tank->m_currentState;
    record->directionCount = tank->m_dirCnt;
    record->directions.clear();
    record->directionAngles.clear();
    for (int index = 1; index < tank->m_dirCnt; ++index)
    {
        record->directions.push_back(tank->m_dir[index]);
        record->directionAngles.push_back(tank->m_dirAngle[index]);
    }
    record->destinationDirection = tank->m_destDir;
    record->previousDirection = tank->m_prevDir;
    record->stopMode = tank->m_stopMode;
    record->neededPower = tank->m_needPow;
    record->neededAngle = tank->m_needAng;
    record->phase = tank->m_phase;
    record->runSmoke = tank->m_runSmoke;
    record->lastAttackTime = tank->m_lastAttackTime;
    record->maximumDistanceScale = tank->m_maxDistScale;
    record->towerAxis = tank->m_towerAxis;
    record->destinationPosition = tank->m_destPos;
    record->previousTime = tank->m_prevTime;
    record->haveDriveEvent = tank->m_have_UNIT_I_DRIVE;
    record->normal = tank->m_normal;
    record->speed = tank->m_speed;
    record->power = tank->m_power;
    record->angle = tank->m_angle;
    record->damage = tank->m_damage;
    record->desireShoot = tank->m_desireShoot;
    record->maximumMoveDistance = tank->m_maxMoveDist;
    record->previousPosition = tank->m_prevPos;
    record->attack = tank->m_attack ? 1 : 0;
    if (tank->m_currentState == 4)
    {
        record->checkRotateCount = tank->m_checkRotateCount;
        record->savedHorizontalAngle = tank->save_hAngle;
        record->savedVerticalAngle = tank->save_vAngle;
        record->savedCount = tank->save_count;
        for (int index = 0; index < tank->m_cannons.getCount(); ++index)
            if (tank->m_cannons[index] == tank->save_cannonID)
                record->savedCannonOrdinal = index;
        if (record->savedCannonOrdinal < 0)
            return Fail("Tank rotate-and-shoot Cannon reference is invalid");
    }
    if (tank->m_lastDeltaT != 0.0)
    {
        record->lastMovePosition = tank->m_lastMovePos;
        record->lastMoveDeltaTime = tank->m_lastDeltaT;
    }
    if (!CaptureEvents(context, tank->getObjectID(), kTankSchedulerLabels,
                       [](KR_Event &event, StableEvent *stable) {
                           return ReadTankEvent(event, stable);
                       }, &record->events))
        return false;

    record->cannons.clear();
    const char *attributes[MAX_CANNON] = {
        tank->m_attr->m_cannon0, tank->m_attr->m_cannon1,
        tank->m_attr->m_cannon2};
    if (tank->m_cannons.getCount() != tank->m_attr->m_cannonCnt)
        return Fail("Tank owned Cannon count does not match TankAttr");
    const ct_ClassTableID bulletTable =
        g_arena.searchSeanceClassTable("Bullet");
    for (int index = 0; index < tank->m_cannons.getCount(); ++index)
    {
        Cannon *cannon = ResolveCannon(context, tank->m_cannons[index]);
        if (cannon == NULL || cannon->m_cannonMaster != tank->getObjectID() ||
            cannon->m_attrIndex != tank->m_attr->m_cannonIndex[index] ||
            cannon->m_bulletTable != bulletTable)
            return Fail("Tank owned Cannon runtime reference is invalid");
        StableCannonRecord child;
        child.name = ObjectName(context, cannon->getObjectID());
        child.attribute = attributes[index];
        child.audibleThisFrame = cannon->m_audibleThisFrame;
        child.visible = cannon->m_isVisible;
        child.lastMoveTimeStamp = cannon->m_lastMoveTimeStamp;
        // Cannon::realPosition always follows its Tank master. The inherited
        // Subject position remains the addNotify sentinel and is never read by
        // Cannon behavior; canonical zero avoids turning setPosition's scene
        // clamping into persistent state.
        child.position = CFVector3(0.0, 0.0, 0.0);
        child.currentState = cannon->m_currentState;
        child.vector = cannon->m_vector;
        child.horizontalAngle = cannon->m_hAngle;
        child.verticalAngle = cannon->m_vAngle;
        child.localHorizontalAngle = cannon->m_localHAngle;
        child.localVerticalAngle = cannon->m_localVAngle;
        if (child.name.empty() || child.attribute.empty() ||
            !CaptureEvents(
                context, cannon->getObjectID(), kCannonSchedulerLabels,
                [tank](KR_Event &event, StableEvent *stable) {
                    return ReadCannonEvent(event, *tank->m_attr, stable);
                }, &child.events))
            return false;
        record->cannons.push_back(child);
    }
    return true;
}

bool CollectRecords(SimulationContext *context,
                    std::vector<StableTankRecord> *records)
{
    TankRoster roster = {};
    if (records == NULL || !CollectRoster(context, &roster))
        return false;
    records->clear();
    for (std::size_t index = 0; index < roster.tanks.size(); ++index)
    {
        StableTankRecord record;
        if (!CaptureRecord(context, roster, roster.tanks[index], &record))
            return false;
        records->push_back(record);
    }
    return true;
}

void PutU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
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
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
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
    if (bytes == NULL || value.size() > kMaximumString)
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
        size > kMaximumString || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    value->assign(size == 0 ? "" :
        reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return true;
}

void PutBool(std::vector<unsigned char> *bytes, int value)
{
    PutU32(bytes, value == 0 ? 0u : 1u);
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

void PutVector2(std::vector<unsigned char> *bytes, const CFVector2 &value)
{
    PutDouble(bytes, value.x);
    PutDouble(bytes, value.y);
}

bool GetVector2(const std::vector<unsigned char> &bytes, std::size_t *offset,
                CFVector2 *value)
{
    return value != NULL && GetDouble(bytes, offset, &value->x) &&
           GetDouble(bytes, offset, &value->y);
}

void PutVector3(std::vector<unsigned char> *bytes, const SFVector3 &value)
{
    PutDouble(bytes, value.x);
    PutDouble(bytes, value.y);
    PutDouble(bytes, value.z);
}

bool GetVector3(const std::vector<unsigned char> &bytes, std::size_t *offset,
                SFVector3 *value)
{
    return value != NULL && GetDouble(bytes, offset, &value->x) &&
           GetDouble(bytes, offset, &value->y) &&
           GetDouble(bytes, offset, &value->z);
}

bool PutReference(std::vector<unsigned char> *bytes,
                  const StableReference &reference)
{
    if (!PutString(bytes, reference.name))
        return false;
    PutI32(bytes, reference.tankOrdinal);
    return true;
}

bool GetReference(const std::vector<unsigned char> &bytes,
                  std::size_t *offset, StableReference *reference)
{
    return reference != NULL && GetString(bytes, offset, &reference->name) &&
           GetI32(bytes, offset, &reference->tankOrdinal);
}

bool PutEvent(std::vector<unsigned char> *bytes, const StableEvent &event)
{
    PutI32(bytes, event.label);
    PutDouble(bytes, event.timeStamp);
    PutDouble(bytes, event.first);
    PutDouble(bytes, event.second);
    PutDouble(bytes, event.third);
    PutI32(bytes, event.count);
    return PutString(bytes, event.bulletAttribute);
}

bool GetEvent(const std::vector<unsigned char> &bytes, std::size_t *offset,
              StableEvent *event)
{
    return event != NULL && GetI32(bytes, offset, &event->label) &&
           GetDouble(bytes, offset, &event->timeStamp) &&
           GetDouble(bytes, offset, &event->first) &&
           GetDouble(bytes, offset, &event->second) &&
           GetDouble(bytes, offset, &event->third) &&
           GetI32(bytes, offset, &event->count) &&
           GetString(bytes, offset, &event->bulletAttribute);
}

bool PutEvents(std::vector<unsigned char> *bytes,
               const std::vector<StableEvent> &events)
{
    PutU32(bytes, static_cast<std::uint32_t>(events.size()));
    for (std::size_t index = 0; index < events.size(); ++index)
        if (!PutEvent(bytes, events[index]))
            return false;
    return true;
}

bool GetEvents(const std::vector<unsigned char> &bytes, std::size_t *offset,
               std::vector<StableEvent> *events)
{
    std::uint32_t count = 0;
    if (events == NULL || !GetU32(bytes, offset, &count) || count > 32)
        return false;
    events->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableEvent event;
        if (!GetEvent(bytes, offset, &event))
            return false;
        events->push_back(event);
    }
    return true;
}

bool PutCannon(std::vector<unsigned char> *bytes,
               const StableCannonRecord &record)
{
    if (!PutString(bytes, record.name) ||
        !PutString(bytes, record.attribute))
        return false;
    PutBool(bytes, record.audibleThisFrame);
    PutBool(bytes, record.visible);
    PutDouble(bytes, record.lastMoveTimeStamp);
    PutVector3(bytes, record.position);
    PutI32(bytes, record.currentState);
    PutVector3(bytes, record.vector);
    PutDouble(bytes, record.horizontalAngle);
    PutDouble(bytes, record.verticalAngle);
    PutDouble(bytes, record.localHorizontalAngle);
    PutDouble(bytes, record.localVerticalAngle);
    return PutEvents(bytes, record.events);
}

bool GetCannon(const std::vector<unsigned char> &bytes, std::size_t *offset,
               StableCannonRecord *record)
{
    return record != NULL && GetString(bytes, offset, &record->name) &&
           GetString(bytes, offset, &record->attribute) &&
           GetBool(bytes, offset, &record->audibleThisFrame) &&
           GetBool(bytes, offset, &record->visible) &&
           GetDouble(bytes, offset, &record->lastMoveTimeStamp) &&
           GetVector3(bytes, offset, &record->position) &&
           GetI32(bytes, offset, &record->currentState) &&
           GetVector3(bytes, offset, &record->vector) &&
           GetDouble(bytes, offset, &record->horizontalAngle) &&
           GetDouble(bytes, offset, &record->verticalAngle) &&
           GetDouble(bytes, offset, &record->localHorizontalAngle) &&
           GetDouble(bytes, offset, &record->localVerticalAngle) &&
           GetEvents(bytes, offset, &record->events);
}

bool PutRecord(std::vector<unsigned char> *bytes,
               const StableTankRecord &record)
{
    if (!PutString(bytes, record.name) ||
        !PutString(bytes, record.attribute) ||
        !PutReference(bytes, record.group) ||
        !PutReference(bytes, record.commander) ||
        !PutReference(bytes, record.attackedEnemy) ||
        !PutString(bytes, record.artefact))
        return false;
    PutBool(bytes, record.audibleThisFrame);
    PutBool(bytes, record.visible);
    PutDouble(bytes, record.lastMoveTimeStamp);
    PutVector3(bytes, record.position);
    PutI32(bytes, record.currentState);
    PutI32(bytes, record.directionCount);
    PutU32(bytes, static_cast<std::uint32_t>(record.directions.size()));
    for (std::size_t index = 0; index < record.directions.size(); ++index)
    {
        PutVector2(bytes, record.directions[index]);
        PutDouble(bytes, record.directionAngles[index]);
    }
    PutVector2(bytes, record.destinationDirection);
    PutVector2(bytes, record.previousDirection);
    PutBool(bytes, record.stopMode);
    PutDouble(bytes, record.neededPower);
    PutDouble(bytes, record.neededAngle);
    PutDouble(bytes, record.phase);
    PutBool(bytes, record.runSmoke);
    PutDouble(bytes, record.lastAttackTime);
    PutI32(bytes, record.maximumDistanceScale);
    PutVector3(bytes, record.towerAxis);
    PutVector3(bytes, record.destinationPosition);
    PutDouble(bytes, record.previousTime);
    PutBool(bytes, record.haveDriveEvent);
    PutVector3(bytes, record.normal);
    PutDouble(bytes, record.speed);
    PutDouble(bytes, record.power);
    PutDouble(bytes, record.angle);
    PutDouble(bytes, record.damage);
    PutDouble(bytes, record.desireShoot);
    PutDouble(bytes, record.maximumMoveDistance);
    PutVector3(bytes, record.previousPosition);
    PutBool(bytes, record.attack);
    PutI32(bytes, record.checkRotateCount);
    PutDouble(bytes, record.savedHorizontalAngle);
    PutDouble(bytes, record.savedVerticalAngle);
    PutI32(bytes, record.savedCount);
    PutI32(bytes, record.savedCannonOrdinal);
    PutVector3(bytes, record.lastMovePosition);
    PutDouble(bytes, record.lastMoveDeltaTime);
    if (!PutEvents(bytes, record.events))
        return false;
    PutU32(bytes, static_cast<std::uint32_t>(record.cannons.size()));
    for (std::size_t index = 0; index < record.cannons.size(); ++index)
        if (!PutCannon(bytes, record.cannons[index]))
            return false;
    return true;
}

bool GetRecord(const std::vector<unsigned char> &bytes, std::size_t *offset,
               StableTankRecord *record)
{
    std::uint32_t directionCount = 0, cannonCount = 0;
    if (record == NULL || !GetString(bytes, offset, &record->name) ||
        !GetString(bytes, offset, &record->attribute) ||
        !GetReference(bytes, offset, &record->group) ||
        !GetReference(bytes, offset, &record->commander) ||
        !GetReference(bytes, offset, &record->attackedEnemy) ||
        !GetString(bytes, offset, &record->artefact) ||
        !GetBool(bytes, offset, &record->audibleThisFrame) ||
        !GetBool(bytes, offset, &record->visible) ||
        !GetDouble(bytes, offset, &record->lastMoveTimeStamp) ||
        !GetVector3(bytes, offset, &record->position) ||
        !GetI32(bytes, offset, &record->currentState) ||
        !GetI32(bytes, offset, &record->directionCount) ||
        !GetU32(bytes, offset, &directionCount) ||
        directionCount > MAX_DIRECTIONS)
        return false;
    record->directions.clear();
    record->directionAngles.clear();
    for (std::uint32_t index = 0; index < directionCount; ++index)
    {
        CFVector2 direction;
        double angle = 0.0;
        if (!GetVector2(bytes, offset, &direction) ||
            !GetDouble(bytes, offset, &angle))
            return false;
        record->directions.push_back(direction);
        record->directionAngles.push_back(angle);
    }
    if (!GetVector2(bytes, offset, &record->destinationDirection) ||
        !GetVector2(bytes, offset, &record->previousDirection) ||
        !GetBool(bytes, offset, &record->stopMode) ||
        !GetDouble(bytes, offset, &record->neededPower) ||
        !GetDouble(bytes, offset, &record->neededAngle) ||
        !GetDouble(bytes, offset, &record->phase) ||
        !GetBool(bytes, offset, &record->runSmoke) ||
        !GetDouble(bytes, offset, &record->lastAttackTime) ||
        !GetI32(bytes, offset, &record->maximumDistanceScale) ||
        !GetVector3(bytes, offset, &record->towerAxis) ||
        !GetVector3(bytes, offset, &record->destinationPosition) ||
        !GetDouble(bytes, offset, &record->previousTime) ||
        !GetBool(bytes, offset, &record->haveDriveEvent) ||
        !GetVector3(bytes, offset, &record->normal) ||
        !GetDouble(bytes, offset, &record->speed) ||
        !GetDouble(bytes, offset, &record->power) ||
        !GetDouble(bytes, offset, &record->angle) ||
        !GetDouble(bytes, offset, &record->damage) ||
        !GetDouble(bytes, offset, &record->desireShoot) ||
        !GetDouble(bytes, offset, &record->maximumMoveDistance) ||
        !GetVector3(bytes, offset, &record->previousPosition) ||
        !GetBool(bytes, offset, &record->attack) ||
        !GetI32(bytes, offset, &record->checkRotateCount) ||
        !GetDouble(bytes, offset, &record->savedHorizontalAngle) ||
        !GetDouble(bytes, offset, &record->savedVerticalAngle) ||
        !GetI32(bytes, offset, &record->savedCount) ||
        !GetI32(bytes, offset, &record->savedCannonOrdinal) ||
        !GetVector3(bytes, offset, &record->lastMovePosition) ||
        !GetDouble(bytes, offset, &record->lastMoveDeltaTime) ||
        !GetEvents(bytes, offset, &record->events) ||
        !GetU32(bytes, offset, &cannonCount) || cannonCount > MAX_CANNON)
        return false;
    record->cannons.clear();
    for (std::uint32_t index = 0; index < cannonCount; ++index)
    {
        StableCannonRecord cannon;
        if (!GetCannon(bytes, offset, &cannon))
            return false;
        record->cannons.push_back(cannon);
    }
    return true;
}

bool ValidateEvent(const StableEvent &event, bool cannon)
{
    if (!std::isfinite(event.timeStamp) || event.timeStamp < 0.0 ||
        !std::isfinite(event.first) || !std::isfinite(event.second) ||
        !std::isfinite(event.third) || event.count < 0 ||
        (cannon ? !IsCannonSchedulerLabel(event.label)
                : !IsTankSchedulerLabel(event.label)))
        return false;
    return EventUsesBulletAttribute(event.label)
               ? !event.bulletAttribute.empty()
               : event.bulletAttribute.empty();
}

bool ValidateEvents(const std::vector<StableEvent> &events, bool cannon)
{
    for (std::size_t index = 0; index < events.size(); ++index)
        if (!ValidateEvent(events[index], cannon) ||
            (index != 0 && events[index - 1].label >= events[index].label))
            return false;
    return true;
}

bool ValidateReference(const StableReference &reference)
{
    return reference.tankOrdinal >= -1 &&
           (reference.name.empty() ? reference.tankOrdinal == -1 : true);
}

bool ValidateRecord(const StableTankRecord &record)
{
    if (record.name.empty() || record.attribute.empty() ||
        !ValidateReference(record.group) ||
        !ValidateReference(record.commander) ||
        !ValidateReference(record.attackedEnemy) ||
        !IsBool(record.audibleThisFrame) || !IsBool(record.visible) ||
        !std::isfinite(record.lastMoveTimeStamp) || !Finite(record.position) ||
        record.currentState < 0 || record.currentState > 4 ||
        record.directionCount < 1 ||
        record.directionCount > MAX_DIRECTIONS + 1 ||
        record.directions.size() !=
            static_cast<std::size_t>(record.directionCount - 1) ||
        record.directionAngles.size() != record.directions.size() ||
        !Finite(record.destinationDirection) ||
        !Finite(record.previousDirection) || !IsBool(record.stopMode) ||
        !std::isfinite(record.neededPower) ||
        !std::isfinite(record.neededAngle) || !std::isfinite(record.phase) ||
        !IsBool(record.runSmoke) || !std::isfinite(record.lastAttackTime) ||
        record.maximumDistanceScale < 0 || !Finite(record.towerAxis) ||
        !Finite(record.destinationPosition) ||
        !std::isfinite(record.previousTime) ||
        !IsBool(record.haveDriveEvent) || !Finite(record.normal) ||
        !std::isfinite(record.speed) || !std::isfinite(record.power) ||
        !std::isfinite(record.angle) || !std::isfinite(record.damage) ||
        !std::isfinite(record.desireShoot) ||
        !std::isfinite(record.maximumMoveDistance) ||
        !Finite(record.previousPosition) || !IsBool(record.attack) ||
        record.checkRotateCount < 0 ||
        !std::isfinite(record.savedHorizontalAngle) ||
        !std::isfinite(record.savedVerticalAngle) || record.savedCount < 0 ||
        record.savedCannonOrdinal < -1 ||
        record.savedCannonOrdinal >=
            static_cast<int>(record.cannons.size()) ||
        !Finite(record.lastMovePosition) ||
        !std::isfinite(record.lastMoveDeltaTime) ||
        record.lastMoveDeltaTime < 0.0 ||
        record.cannons.size() > MAX_CANNON ||
        !ValidateEvents(record.events, false))
        return false;
    if ((record.currentState == 4) != (record.savedCannonOrdinal >= 0))
        return false;
    for (std::size_t index = 0; index < record.directions.size(); ++index)
        if (!Finite(record.directions[index]) ||
            !std::isfinite(record.directionAngles[index]))
            return false;
    for (std::size_t index = 0; index < record.cannons.size(); ++index)
    {
        const StableCannonRecord &cannon = record.cannons[index];
        if (cannon.name.empty() || cannon.attribute.empty() ||
            !IsBool(cannon.audibleThisFrame) || !IsBool(cannon.visible) ||
            !std::isfinite(cannon.lastMoveTimeStamp) ||
            !Finite(cannon.position) || cannon.currentState < 0 ||
            cannon.currentState > 4 || !Finite(cannon.vector) ||
            !std::isfinite(cannon.horizontalAngle) ||
            !std::isfinite(cannon.verticalAngle) ||
            !std::isfinite(cannon.localHorizontalAngle) ||
            !std::isfinite(cannon.localVerticalAngle) ||
            !ValidateEvents(cannon.events, true))
            return false;
    }
    return true;
}

bool EncodeRecords(const std::vector<StableTankRecord> &records,
                   std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumTanks)
        return false;
    bytes->clear();
    PutU32(bytes, kTankMagic);
    PutU32(bytes, kTankVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!ValidateRecord(records[index]) ||
            (index != 0 && records[index - 1].name > records[index].name) ||
            !PutRecord(bytes, records[index]))
            return Fail("TAN1 Tank record validation/encoding failed");
    }
    return true;
}

bool DecodeRecords(const std::vector<unsigned char> &bytes,
                   std::vector<StableTankRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) || magic != kTankMagic ||
        version != kTankVersion || count > kMaximumTanks)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableTankRecord record;
        if (!GetRecord(bytes, &offset, &record) || !ValidateRecord(record) ||
            (index != 0 && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

bool ResolveReference(SimulationContext *context, const TankRoster &roster,
                      const StableReference &reference, int interfaceID,
                      KR_ObjectID *object)
{
    if (object == NULL)
        return false;
    *object = KR_ObjectID::NUL();
    if (reference.name.empty())
        return true;
    if (reference.tankOrdinal >= 0)
    {
        Tank *tank = TankAtOrdinal(roster, reference.name,
                                   reference.tankOrdinal);
        if (tank == NULL)
            return false;
        *object = tank->getObjectID();
    }
    else
    {
        if (context == NULL || !context->isExist(reference.name.c_str()))
            return false;
        *object = context->searchObject(reference.name.c_str());
    }
    return interfaceID == 0 ||
           context->queryInterface(*object, interfaceID) != NULL;
}

bool AddEvent(SimulationContext *context, const KR_ObjectID &owner,
              const StableEvent &stable, int bulletIndex)
{
    KR_Event event(stable.label, stable.timeStamp, owner, owner);
    switch (stable.label)
    {
    case t_EVC_MOVING:
    case t_EVC_MOVING_AND_SHOOTING:
    case t_EVC_RETREATING:
    case t_EVC_RETURN:
        event.data.open(EDO_WRITE).putDouble(stable.first).close();
        break;
    case cn_EV_SHOOT:
        event.data.open(EDO_WRITE).putInt(stable.count)
            .putInt(bulletIndex).close();
        break;
    case cn_EV_IDLEOK:
        event.data.open(EDO_WRITE).putInt(bulletIndex)
            .putInt(stable.count).close();
        break;
    case cn_EV_SINGLE_SHOOT:
        event.data.open(EDO_WRITE).putInt(bulletIndex).close();
        break;
    case cn_EVC_ROTATE:
        event.data.open(EDO_WRITE).putDouble(stable.first)
            .putDouble(stable.second).putDouble(stable.third).close();
        break;
    case cn_EVCMD_ROTATE_AND_SHOOT:
        event.data.open(EDO_WRITE)
            .descend(cn_ROTATE, 0)
              .putDouble(stable.first).putDouble(stable.second)
              .putDouble(stable.third)
            .ascend()
            .descend(cn_SHOOT, 0)
              .putInt(stable.count).putInt(bulletIndex)
            .ascend().close();
        break;
    case cn_EVCMD_DIRECT_SHOOT:
        event.data.open(EDO_WRITE).putDouble(stable.second)
            .putDouble(stable.third).putInt(bulletIndex)
            .putInt(stable.count).close();
        break;
    default:
        break;
    }
    context->addEvent(event);
    return true;
}

bool ApplyRecord(SimulationContext *context, const TankRoster &roster,
                 Tank *tank, const StableTankRecord &record)
{
    if (tank == NULL || !context->isExist(record.attribute.c_str()))
        return false;
    const KR_ObjectID attribute =
        context->searchObject(record.attribute.c_str());
    if (g_tankAttrTable.searchAttribute(attribute) == NULL ||
        tank->m_tankAttrID != attribute || tank->m_attr == NULL ||
        tank->m_cannons.getCount() !=
            static_cast<int>(record.cannons.size()))
        return false;
    KR_ObjectID group, commander, attackedEnemy;
    if (!ResolveReference(context, roster, record.group, 0, &group) ||
        !ResolveReference(context, roster, record.commander, ICommanderIID,
                          &commander) ||
        !ResolveReference(context, roster, record.attackedEnemy,
                          IDynamicObjectIID, &attackedEnemy))
        return false;
    KR_ObjectID artefact = KR_ObjectID::NUL();
    IArtefact *artefactInterface = NULL;
    if (!record.artefact.empty())
    {
        if (!context->isExist(record.artefact.c_str()))
            return false;
        artefact = context->searchObject(record.artefact.c_str());
        artefactInterface = static_cast<IArtefact *>(
            context->queryInterface(artefact, IArtefactIID));
        if (artefactInterface == NULL)
            return false;
    }

    tank->m_audibleThisFrame = record.audibleThisFrame;
    tank->m_isVisible = record.visible;
    tank->m_lastMoveTimeStamp = record.lastMoveTimeStamp;
    tank->ct_Subject::setPosition(record.position);
    tank->m_currentState = record.currentState;
    tank->m_dirCnt = record.directionCount;
    tank->m_destDir = record.destinationDirection;
    tank->m_prevDir = record.previousDirection;
    tank->m_dir[0] = record.destinationDirection;
    tank->m_dirAngle[0] =
        std::atan2(record.destinationDirection.y,
                   record.destinationDirection.x);
    for (int index = 1; index <= MAX_DIRECTIONS; ++index)
    {
        tank->m_dir[index] = CFVector2(0.0, 0.0);
        tank->m_dirAngle[index] = 0.0;
    }
    for (std::size_t index = 0; index < record.directions.size(); ++index)
    {
        tank->m_dir[index + 1] = record.directions[index];
        tank->m_dirAngle[index + 1] = record.directionAngles[index];
    }
    tank->m_maxViewDist = 0.0;
    tank->m_stopMode = record.stopMode;
    tank->m_needPow = record.neededPower;
    tank->m_needAng = record.neededAngle;
    tank->m_phase = record.phase;
    tank->m_runSmoke = record.runSmoke;
    tank->m_lastAttackTime = record.lastAttackTime;
    tank->m_group = group;
    tank->m_maxDistScale = record.maximumDistanceScale;
    tank->m_towerAxis = record.towerAxis;
    tank->m_destPos = record.destinationPosition;
    tank->m_prevTime = record.previousTime;
    tank->m_have_UNIT_I_DRIVE = record.haveDriveEvent;
    tank->m_normal = record.normal;
    tank->m_speed = record.speed;
    tank->m_power = record.power;
    tank->m_angle = record.angle;
    tank->m_damage = record.damage;
    tank->m_desireShoot = record.desireShoot;
    tank->m_maxMoveDist = record.maximumMoveDistance;
    tank->m_prevPos = record.previousPosition;
    tank->m_attackedEnemy = attackedEnemy;
    tank->m_commander = commander;
    tank->massa_D = 1.0 / tank->m_attr->massa;
    tank->maxPower_D = 1.0 / tank->m_attr->maxPower;
    tank->m_attack = record.attack != 0;
    tank->m_checkRotateCount = record.checkRotateCount;
    tank->save_hAngle = record.savedHorizontalAngle;
    tank->save_vAngle = record.savedVerticalAngle;
    tank->save_count = record.savedCount;
    tank->save_attr = tank->m_attr->m_cacheBulletAttr;
    tank->save_cannonID = record.savedCannonOrdinal < 0
        ? KR_ObjectID::NUL()
        : tank->m_cannons[record.savedCannonOrdinal];
    tank->m_lastMovePos = record.lastMovePosition;
    tank->m_lastDeltaT = record.lastMoveDeltaTime;
    tank->m_artefactID = artefact;
    tank->m_artefact = artefactInterface;
    if (artefactInterface != NULL)
    {
        artefactInterface->m_carrierID = tank->getObjectID();
        artefactInterface->m_carrier = tank;
    }

    RemoveEvents(context, tank->getObjectID(), kTankSchedulerLabels);
    for (std::size_t index = 0; index < record.events.size(); ++index)
        AddEvent(context, tank->getObjectID(), record.events[index],
                 tank->m_attr->m_cacheBulletAttr);

    const ct_ClassTableID cannonAttributes =
        g_arena.searchSeanceClassTable("CannonAttr");
    const ct_ClassTableID bullets = g_arena.searchSeanceClassTable("Bullet");
    for (std::size_t index = 0; index < record.cannons.size(); ++index)
    {
        Cannon *cannon = ResolveCannon(context, tank->m_cannons[index]);
        const StableCannonRecord &child = record.cannons[index];
        if (cannon == NULL || !context->isExist(child.attribute.c_str()))
            return false;
        const KR_ObjectID cannonAttribute =
            context->searchObject(child.attribute.c_str());
        const int attributeIndex =
            g_arena.getAttributeIndex(cannonAttributes, cannonAttribute);
        if (attributeIndex < 0 || cannon->m_attrIndex != attributeIndex)
            return false;
        cannon->m_cannonMaster = tank->getObjectID();
        cannon->m_bulletTable = bullets;
        cannon->m_audibleThisFrame = child.audibleThisFrame;
        cannon->m_isVisible = child.visible;
        cannon->m_lastMoveTimeStamp = child.lastMoveTimeStamp;
        cannon->m_currentState = child.currentState;
        cannon->m_vector = child.vector;
        cannon->m_hAngle = child.horizontalAngle;
        cannon->m_vAngle = child.verticalAngle;
        cannon->m_localHAngle = child.localHorizontalAngle;
        cannon->m_localVAngle = child.localVerticalAngle;
        RemoveEvents(context, cannon->getObjectID(), kCannonSchedulerLabels);
        for (std::size_t eventIndex = 0;
             eventIndex < child.events.size(); ++eventIndex)
        {
            if (EventUsesBulletAttribute(child.events[eventIndex].label) &&
                child.events[eventIndex].bulletAttribute !=
                    tank->m_attr->m_bulletAttr)
                return false;
            AddEvent(context, cannon->getObjectID(), child.events[eventIndex],
                     tank->m_attr->m_cacheBulletAttr);
        }
    }
    return true;
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

void TankActiveWorldState_Link()
{
    TankSubjectState_Link();
    CannonSubjectState_Link();
}

const char *TankActiveWorldState_LastFailure()
{
    return g_lastFailure.c_str();
}

int TankActiveWorldState_LiveCount(SimulationContext *context)
{
    TankRoster roster = {};
    return CollectRoster(context, &roster)
               ? static_cast<int>(roster.tanks.size()) : -1;
}

int TankActiveWorldState_OwnedCannonCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableTankRecord> records;
    if (!DecodeRecords(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        count += static_cast<int>(records[index].cannons.size());
    return count;
}

int TankActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableTankRecord> records;
    if (!DecodeRecords(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        count += static_cast<int>(records[index].events.size());
        for (std::size_t child = 0;
             child < records[index].cannons.size(); ++child)
            count += static_cast<int>(
                records[index].cannons[child].events.size());
    }
    return count;
}

unsigned long long TankActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!TankActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = 14695981039346656037ull;
    if (!bytes.empty())
        HashBytes(&hash, &bytes[0], bytes.size());
    return hash;
}

bool TankActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_lastFailure.clear();
    std::vector<StableTankRecord> records;
    if (!CollectRecords(context, &records))
    {
        if (g_lastFailure.empty())
            Fail("Tank/Cannon roster collection failed");
        return false;
    }
    return EncodeRecords(records, bytes);
}

bool TankActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableTankRecord> records;
    return DecodeRecords(bytes, &records);
}

bool TankActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return TankActiveWorldState_ValidateStable(bytes) &&
           TankActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool TankActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableTankRecord> records;
    TankRoster roster = {};
    if (context == NULL || owners == NULL || !owners->empty() ||
        !DecodeRecords(bytes, &records) || !CollectRoster(context, &roster) ||
        !RosterMatches(roster, records))
        return false;
    for (std::size_t index = 0; index < roster.tanks.size(); ++index)
        owners->push_back(roster.tanks[index]->getObjectID());
    return true;
}

bool TankActiveWorldState_CollectOwnedCannons(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *cannons)
{
    std::vector<StableTankRecord> records;
    TankRoster roster = {};
    if (context == NULL || cannons == NULL || !cannons->empty() ||
        !DecodeRecords(bytes, &records) || !CollectRoster(context, &roster) ||
        !RosterMatches(roster, records))
        return false;
    for (std::size_t owner = 0; owner < roster.tanks.size(); ++owner)
        for (int child = 0;
             child < roster.tanks[owner]->m_cannons.getCount(); ++child)
            cannons->push_back(roster.tanks[owner]->m_cannons[child]);
    return true;
}

bool TankActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableTankRecord> records;
    TankRoster roster = {};
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeRecords(bytes, &records) || !CollectRoster(context, &roster))
        return false;
    if (!roster.tanks.empty())
        return RosterMatches(roster, records);
    int cannonCount = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        cannonCount += static_cast<int>(records[index].cannons.size());
    const int tankFree = TankSubjectState_SubjectCapacity() -
                         TankSubjectState_LiveCount(context);
    const int cannonFree = CannonSubjectState_SubjectCapacity() -
                           CannonSubjectState_LiveCount(context);
    if ((!records.empty() &&
         g_arena.searchSeanceClassTable("Tank") == ct_NULLID) ||
        static_cast<int>(records.size()) > tankFree ||
        cannonCount > cannonFree)
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!context->isExist(records[index].attribute.c_str()))
            return false;
        const KR_ObjectID attribute =
            context->searchObject(records[index].attribute.c_str());
        AttributeTank *tankAttribute = static_cast<AttributeTank *>(
            g_tankAttrTable.searchAttribute(attribute));
        if (tankAttribute == NULL || tankAttribute->m_cannonCnt !=
                static_cast<int>(records[index].cannons.size()))
            return false;
    }
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const KR_ObjectID object =
            g_arena.newObject("Tank", records[index].name.c_str());
        Tank *tank = ResolveTank(context, object);
        if (tank == NULL)
        {
            TankActiveWorldState_RemoveStableOwners(context, created);
            return false;
        }
        created->push_back(object);
        KR_Event event(KR_SET_ATTR, 0.0, g_arena.getObjectID(), object);
        const KR_ObjectID attribute =
            context->searchObject(records[index].attribute.c_str());
        event.data.open(EDO_WRITE).putObjectID(attribute).close();
        context->sendEventNow(event);
        if (tank->m_tankAttrID != attribute || tank->m_attr == NULL ||
            tank->m_cannons.getCount() !=
                static_cast<int>(records[index].cannons.size()))
        {
            TankActiveWorldState_RemoveStableOwners(context, created);
            return false;
        }
    }
    TankRoster restored = {};
    if (!CollectRoster(context, &restored) || !RosterMatches(restored, records))
    {
        TankActiveWorldState_RemoveStableOwners(context, created);
        return false;
    }
    return true;
}

bool TankActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableTankRecord> records;
    TankRoster roster = {};
    if (context == NULL || !DecodeRecords(bytes, &records) ||
        !CollectRoster(context, &roster) || !RosterMatches(roster, records))
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!ApplyRecord(context, roster, roster.tanks[index], records[index]))
            return Fail(std::string("Tank stable record apply failed: ") +
                        records[index].name);
    std::vector<unsigned char> current;
    if (!TankActiveWorldState_CaptureStable(context, &current))
        return false;
    if (current != bytes)
    {
        std::size_t difference = 0;
        while (difference < current.size() && difference < bytes.size() &&
               current[difference] == bytes[difference])
            ++difference;
        char message[192] = {};
        const unsigned expectedByte = difference < bytes.size()
            ? bytes[difference] : 0u;
        const unsigned currentByte = difference < current.size()
            ? current[difference] : 0u;
        std::snprintf(message, sizeof(message),
                      "Tank stable recapture differs at byte %u=%u/%u (%u/%u)",
                      static_cast<unsigned>(difference),
                      expectedByte, currentByte,
                      static_cast<unsigned>(current.size()),
                      static_cast<unsigned>(bytes.size()));
        return Fail(message);
    }
    return true;
}

void TankActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
    {
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            Tank *tank = ResolveTank(context, *object);
            if (tank != NULL)
            {
                RemoveEvents(context, *object, kTankSchedulerLabels);
                for (int index = 0; index < tank->m_cannons.getCount(); ++index)
                    RemoveEvents(context, tank->m_cannons[index],
                                 kCannonSchedulerLabels);
            }
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    }
    created->clear();
}
