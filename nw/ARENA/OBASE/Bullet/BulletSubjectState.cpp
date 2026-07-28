#include "BulletSubjectState.h"

#include <cmath>
#include <cstring>
#include <new>

#include "BulletAttributeState.h"
#include "enum/SpaceEnum.h"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "message/a_msg.h"
#include "message/bulmsg.h"
#include "message/funitmsg.h"
#include "storage/h/subject.h"

namespace {

const double kGravity = -9.8;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

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

void HashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= kHashPrime;
    }
}

void HashString(unsigned long long &hash, const char *value)
{
    HashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

class BoundedBullet : public ct_Subject
{
 public:
    BoundedBullet()
    {
        resetState();
    }

    virtual CFVector3 realPosition() { return m_position; }
    virtual bool shouldDump() { return false; }

    virtual void addNotify()
    {
        ct_Subject::addNotify();
        resetState();
    }

    virtual void removeNotify()
    {
        if (context != NULL)
        {
            context->removeEvent(b_EVC_MOVING, getObjectID());
            context->removeEvent(b_EVC_CHECK_COLLISION, getObjectID());
        }
        ct_Subject::removeNotify();
        resetState();
    }

    virtual int receiveEvent(KR_Event &event)
    {
        switch (event.label)
        {
        case KR_WAKE_UP:
            return 1;
        case KR_SET_ATTR:
            return 0;
        case b_EV_START:
            return start(event);
        case b_EVC_MOVING:
            return move(event);
        case b_EVC_CHECK_COLLISION:
            // The spatial collision/effect graph is the next admitted slice.
            return 0;
        case fu_EV_QUERY_SPEED:
            if (m_attribute == NULL)
                return 0;
            event.label = fu_EV_QUERY_SPEED_OK;
            event.data.open(EDO_WRITE)
                      .putDouble(m_attribute->m_startSpeed)
                      .close();
            return 1;
        default:
            return 0;
        }
    }

    bool clean()
    {
        return !m_started && m_attribute == NULL && m_master.isNUL() &&
               m_moveCount == 0 && m_lastMoveTimeStamp == 0.0 &&
               m_position.x == 0.0 && m_position.y == 0.0 &&
               m_position.z == 0.0 && m_velocity.x == 0.0 &&
               m_velocity.y == 0.0 && m_velocity.z == 0.0;
    }

    bool started() const { return m_started; }
    int moveCount() const { return m_moveCount; }
    AttributeBullet *attribute() const { return m_attribute; }
    const CFVector3 &velocity() const { return m_velocity; }

 private:
    void resetState()
    {
        m_position = CFVector3(0.0, 0.0, 0.0);
        m_previousPosition = CFVector3(0.0, 0.0, 0.0);
        m_velocity = CFVector3(0.0, 0.0, 0.0);
        m_initialPosition = CFVector3(0.0, 0.0, 0.0);
        m_initialDirection = CFVector3(0.0, 0.0, 0.0);
        m_attribute = NULL;
        m_master = KR_ObjectID::NUL();
        m_started = false;
        m_moveCount = 0;
        m_lastMoveTimeStamp = 0.0;
    }

    bool scheduleMove(double previousTimeStamp, double timeStamp)
    {
        if (context == NULL || !std::isfinite(previousTimeStamp) ||
            !std::isfinite(timeStamp) || timeStamp < 0.1)
            return false;
        KR_Event moving;
        moving.label = b_EVC_MOVING;
        moving.source = getObjectID();
        moving.destination = getObjectID();
        moving.timeStamp = timeStamp;
        moving.data.open(EDO_WRITE)
                   .putDouble(previousTimeStamp)
                   .close();
        issueEvent(moving);
        return true;
    }

    int start(KR_Event &event)
    {
        const int expectedSize =
            static_cast<int>(sizeof(double) * 6 + sizeof(int) +
                             sizeof(KR_ObjectID));
        if (m_started || context == NULL || !std::isfinite(event.timeStamp) ||
            event.timeStamp < 0.1)
            return 0;
        s_EventData &data = event.data.open(EDO_READ);
        if (data.remaining() != expectedSize)
        {
            data.close();
            return 0;
        }
        CFVector3 position;
        CFVector3 direction;
        int attributeIndex = -1;
        KR_ObjectID master = KR_ObjectID::NUL();
        data.descend(VECTOR3D_F, 0)
                .getDouble(position.x)
                .getDouble(position.y)
                .getDouble(position.z)
            .ascend()
            .descend(VECTOR3D_F, 0)
                .getDouble(direction.x)
                .getDouble(direction.y)
                .getDouble(direction.z)
            .ascend()
            .getInt(attributeIndex)
            .getObjectID(master)
            .close();

        AttributeBullet *attribute = NULL;
        const double directionLength2 = Abs2(direction);
        if (!FiniteVector(position) || !FiniteVector(direction) ||
            !std::isfinite(directionLength2) || directionLength2 <= 0.0 ||
            !BulletAttributeState_ResolveEncodedIndex(
                context, attributeIndex, &attribute) ||
            attribute == NULL || !std::isfinite(attribute->m_startSpeed) ||
            attribute->m_startSpeed <= 0.0 ||
            !std::isfinite(attribute->m_moveTimeIncrement) ||
            attribute->m_moveTimeIncrement <= 0.0)
            return 0;

        const double inverseLength = 1.0 / std::sqrt(directionLength2);
        m_attribute = attribute;
        m_master = master;
        m_initialPosition = position;
        m_previousPosition = position;
        m_initialDirection = direction * inverseLength;
        m_velocity = m_initialDirection * attribute->m_startSpeed;
        m_started = true;
        m_moveCount = 0;
        m_lastMoveTimeStamp = event.timeStamp;
        setPosition(position);
        if (position.y <= 0.0)
        {
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
            return 1;
        }
        if (!scheduleMove(event.timeStamp,
                          event.timeStamp +
                              attribute->m_moveTimeIncrement))
        {
            setPosition(CFVector3(0.0, 0.0, 0.0));
            resetState();
            return 0;
        }
        return 1;
    }

    int move(KR_Event &event)
    {
        if (!m_started || m_attribute == NULL || context == NULL ||
            !std::isfinite(event.timeStamp) || event.timeStamp < 0.1)
            return 0;
        s_EventData &data = event.data.open(EDO_READ);
        if (data.remaining() != static_cast<int>(sizeof(double)))
        {
            data.close();
            return 0;
        }
        double previousTimeStamp = 0.0;
        data.getDouble(previousTimeStamp).close();
        const double delta = event.timeStamp - previousTimeStamp;
        if (!std::isfinite(previousTimeStamp) || !std::isfinite(delta) ||
            delta < 0.0 || previousTimeStamp < 0.1 ||
            !NearlyEqual(previousTimeStamp, m_lastMoveTimeStamp) ||
            event.source != getObjectID())
            return 0;

        const CFVector3 gravity(0.0, kGravity, 0.0);
        const CFVector3 nextPosition =
            m_position + (m_velocity + gravity * (0.5 * delta)) * delta;
        const CFVector3 nextVelocity = m_velocity + gravity * delta;
        if (!FiniteVector(nextPosition) || !FiniteVector(nextVelocity))
            return 0;

        m_previousPosition = m_position;
        m_velocity = nextVelocity;
        m_lastMoveTimeStamp = event.timeStamp;
        ++m_moveCount;
        setPosition(nextPosition);
        if (m_position.y <= 0.0)
        {
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
            return 1;
        }
        if (!scheduleMove(
                event.timeStamp,
                event.timeStamp + m_attribute->m_moveTimeIncrement))
        {
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
            return 0;
        }
        return 1;
    }

    CFVector3 m_previousPosition;
    CFVector3 m_velocity;
    CFVector3 m_initialPosition;
    CFVector3 m_initialDirection;
    AttributeBullet *m_attribute;
    KR_ObjectID m_master;
    bool m_started;
    int m_moveCount;
};

class BoundedBulletTable : public ct_SubjectTable
{
 public:
    BoundedBulletTable() : m_table(NULL)
    {
        registerClass("Bullet");
    }

    virtual void allocObjects(int objectQnty)
    {
        m_table = new (std::nothrow) BoundedBullet[objectQnty];
        if (m_table == NULL)
            m_maxObjectQnty = 0;
    }

    virtual void freeObjects()
    {
        delete [] m_table;
        m_table = NULL;
        m_maxObjectQnty = 0;
    }

    virtual ct_Object *getObjectPTR(int index)
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "BoundedBulletTable::getObjectPTR");
        return &m_table[index];
    }

    virtual bool isRendering() { return false; }
    virtual bool isAudible() { return false; }

    int capacity() const { return m_maxObjectQnty; }

    int liveCount() const
    {
        int count = 0;
        for (ct_Subject *object = findFirstSubject(); object != NULL;
             object = findNextSubject(object))
            ++count;
        return count;
    }

    BoundedBullet *find(const KR_ObjectID &objectID) const
    {
        for (ct_Subject *object = findFirstSubject(); object != NULL;
             object = findNextSubject(object))
            if (object->getObjectID() == objectID)
                return static_cast<BoundedBullet *>(object);
        return NULL;
    }

 private:
    BoundedBullet *m_table;
};

BoundedBulletTable g_bulletTable;

bool RemoveIfPresent(SimulationContext *context, KR_ObjectID object)
{
    if (!object.isNUL() && context->isExist(object))
        context->removeObject(object);
    return object.isNUL() || !context->isExist(object);
}

bool BuildStartEvent(KR_Event &event, KR_ObjectID destination,
                     KR_ObjectID source, double timeStamp,
                     const CFVector3 &position, const CFVector3 &direction,
                     int attributeIndex, KR_ObjectID master)
{
    if (timeStamp < 0.1 || !std::isfinite(timeStamp))
        return false;
    event = KR_Event();
    event.label = b_EV_START;
    event.source = source;
    event.destination = destination;
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE)
              .descend(VECTOR3D_F, 0)
                .putDouble(position.x)
                .putDouble(position.y)
                .putDouble(position.z)
              .ascend()
              .descend(VECTOR3D_F, 0)
                .putDouble(direction.x)
                .putDouble(direction.y)
                .putDouble(direction.z)
              .ascend()
              .putInt(attributeIndex)
              .putObjectID(master)
              .close();
    return true;
}

}  // namespace

void BulletSubjectState_Link()
{
}

bool BulletSubjectState_TableReady(SimulationContext *context, int capacity)
{
    return context != NULL && capacity > 0 &&
           g_arena.getContext() == context &&
           g_arena.searchSeanceClassTable("Bullet") != ct_NULLID &&
           g_bulletTable.capacity() == capacity;
}

int BulletSubjectState_Capacity()
{
    return g_bulletTable.capacity();
}

int BulletSubjectState_LiveCount()
{
    return g_bulletTable.liveCount();
}

unsigned long long BulletSubjectState_Fingerprint(
    SimulationContext *context)
{
    const int capacity = g_bulletTable.capacity();
    if (!BulletSubjectState_TableReady(context, capacity) ||
        g_bulletTable.liveCount() != 0)
        return 0;
    unsigned long long hash = kHashOffset;
    const int rendering = 0;
    const int audible = 0;
    const int ballisticFreeFlight = 1;
    const int groundRemoval = 1;
    const int collision = 0;
    const int effects = 0;
    HashString(hash, "Bullet");
    HashBytes(hash, &capacity, sizeof(capacity));
    HashBytes(hash, &rendering, sizeof(rendering));
    HashBytes(hash, &audible, sizeof(audible));
    HashBytes(hash, &ballisticFreeFlight, sizeof(ballisticFreeFlight));
    HashBytes(hash, &groundRemoval, sizeof(groundRemoval));
    HashBytes(hash, &collision, sizeof(collision));
    HashBytes(hash, &effects, sizeof(effects));
    return hash;
}

bool BulletSubjectState_ProbeBallisticLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, int *moveCount)
{
    if (moveCount == NULL)
        return false;
    *moveCount = 0;
    const int baseline = g_bulletTable.liveCount();
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        baseline != 0 ||
        !BulletSubjectState_TableReady(context, g_bulletTable.capacity()))
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeBullet *attribute = static_cast<AttributeBullet *>(
        __bulletAttrTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("BulletAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Bullet");
    if (attributeID.isNUL() || attribute == NULL ||
        attributeTable == ct_NULLID || subjectTable == ct_NULLID)
        return false;
    const int attributeIndex =
        g_arena.getAttributeIndex(attributeTable, attributeID);
    AttributeBullet *resolved = NULL;
    if (attributeIndex == -1 ||
        !BulletAttributeState_ResolveEncodedIndex(
            context, attributeIndex, &resolved) || resolved != attribute)
        return false;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const KR_ObjectID source = g_arena.getObjectID();
    KR_Event event;

    KR_ObjectID invalid = g_arena.newObject(
        subjectTable, "Bullet.Subject.InvalidPayload.Probe");
    BoundedBullet *invalidObject = g_bulletTable.find(invalid);
    event.label = b_EV_START;
    event.timeStamp = ts;
    event.data.open(EDO_WRITE).putInt(1).close();
    const bool invalidPayloadRejected =
        !invalid.isNUL() && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 && invalidObject->clean() &&
        context->removeEvent(b_EVC_MOVING, invalid) == 0;
    RemoveIfPresent(context, invalid);

    invalid = g_arena.newObject(
        subjectTable, "Bullet.Subject.InvalidAttribute.Probe");
    invalidObject = g_bulletTable.find(invalid);
    BuildStartEvent(event, invalid, source, ts,
                    CFVector3(1.0, 2.0, 3.0),
                    CFVector3(1.0, 0.0, 0.0),
                    attributeIndex ^ 0x40000000, source);
    const bool invalidAttributeRejected =
        !invalid.isNUL() && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 && invalidObject->clean() &&
        context->removeEvent(b_EVC_MOVING, invalid) == 0;
    RemoveIfPresent(context, invalid);

    invalid = g_arena.newObject(
        subjectTable, "Bullet.Subject.InvalidDirection.Probe");
    invalidObject = g_bulletTable.find(invalid);
    BuildStartEvent(event, invalid, source, ts,
                    CFVector3(1.0, 2.0, 3.0),
                    CFVector3(0.0, 0.0, 0.0), attributeIndex, source);
    const bool invalidDirectionRejected =
        !invalid.isNUL() && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 && invalidObject->clean() &&
        context->removeEvent(b_EVC_MOVING, invalid) == 0;
    RemoveIfPresent(context, invalid);
    if (!invalidPayloadRejected || !invalidAttributeRejected ||
        !invalidDirectionRejected ||
        g_bulletTable.liveCount() != baseline)
        return false;

    KR_ObjectID flight =
        g_arena.newObject(subjectTable, "Bullet.Subject.Flight.Probe");
    BoundedBullet *flightObject = g_bulletTable.find(flight);
    const CFVector3 startPosition(10.0, 5.0, -5.0);
    const CFVector3 startDirection(1.0, 0.0, 0.0);
    BuildStartEvent(event, flight, source, ts, startPosition,
                    startDirection, attributeIndex, source);
    context->sendEventNow(event);
    const bool started = !flight.isNUL() && flightObject != NULL &&
        flightObject->started() && flightObject->attribute() == attribute &&
        flightObject->getPosition().x == startPosition.x &&
        flightObject->getPosition().y == startPosition.y &&
        flightObject->getPosition().z == startPosition.z &&
        NearlyEqual(flightObject->velocity().x, attribute->m_startSpeed) &&
        NearlyEqual(flightObject->velocity().y, 0.0) &&
        NearlyEqual(flightObject->velocity().z, 0.0);
    KR_Event speedQuery;
    speedQuery.label = fu_EV_QUERY_SPEED;
    speedQuery.source = source;
    speedQuery.destination = flight;
    speedQuery.timeStamp = ts;
    const bool speedAnswered = flightObject != NULL &&
        flightObject->receiveEvent(speedQuery) == 1 &&
        speedQuery.label == fu_EV_QUERY_SPEED_OK &&
        speedQuery.data.size() == static_cast<int>(sizeof(double));
    double queriedSpeed = 0.0;
    if (speedAnswered)
        speedQuery.data.open(EDO_READ).getDouble(queriedSpeed).close();
    const bool firstScheduled =
        context->removeEvent(b_EVC_MOVING, flight) != 0;
    if (!started || !speedAnswered ||
        !NearlyEqual(queriedSpeed, attribute->m_startSpeed) ||
        !firstScheduled)
    {
        context->removeEvent(b_EVC_MOVING, flight);
        RemoveIfPresent(context, flight);
        return false;
    }

    const double delta = attribute->m_moveTimeIncrement;
    event = KR_Event();
    event.label = b_EVC_MOVING;
    event.source = flight;
    event.destination = flight;
    event.timeStamp = ts + delta;
    event.data.open(EDO_WRITE).putDouble(ts).close();
    context->sendEventNow(event);
    const CFVector3 expectedPosition(
        startPosition.x + attribute->m_startSpeed * delta,
        startPosition.y + 0.5 * kGravity * delta * delta,
        startPosition.z);
    const bool moved = context->isExist(flight) &&
        flightObject->moveCount() == 1 &&
        NearlyEqual(flightObject->getPosition().x, expectedPosition.x) &&
        NearlyEqual(flightObject->getPosition().y, expectedPosition.y) &&
        NearlyEqual(flightObject->getPosition().z, expectedPosition.z) &&
        NearlyEqual(flightObject->velocity().x, attribute->m_startSpeed) &&
        NearlyEqual(flightObject->velocity().y, kGravity * delta) &&
        NearlyEqual(flightObject->velocity().z, 0.0);
    const bool secondScheduled =
        context->removeEvent(b_EVC_MOVING, flight) != 0;
    *moveCount += flightObject == NULL ? 0 : flightObject->moveCount();
    RemoveIfPresent(context, flight);
    if (!moved || !secondScheduled || g_bulletTable.liveCount() != baseline)
        return false;

    KR_ObjectID ground =
        g_arena.newObject(subjectTable, "Bullet.Subject.Ground.Probe");
    BoundedBullet *groundObject = g_bulletTable.find(ground);
    BuildStartEvent(event, ground, source, ts + 1.0,
                    CFVector3(0.0, 0.001, 0.0),
                    CFVector3(0.0, -1.0, 0.0), attributeIndex, source);
    context->sendEventNow(event);
    const bool groundStarted = !ground.isNUL() && groundObject != NULL &&
        groundObject->started() &&
        context->removeEvent(b_EVC_MOVING, ground) != 0;
    event = KR_Event();
    event.label = b_EVC_MOVING;
    event.source = ground;
    event.destination = ground;
    event.timeStamp = ts + 1.0 + delta;
    event.data.open(EDO_WRITE).putDouble(ts + 1.0).close();
    context->sendEventNow(event);
    const bool removedAtGround = groundStarted && !context->isExist(ground) &&
        context->removeEvent(b_EVC_MOVING, ground) == 0 &&
        context->removeEvent(b_EVC_CHECK_COLLISION, ground) == 0;
    if (groundStarted)
        ++(*moveCount);
    RemoveIfPresent(context, ground);

    KR_ObjectID rollback =
        g_arena.newObject(subjectTable, "Bullet.Subject.Rollback.Probe");
    BoundedBullet *rollbackObject = g_bulletTable.find(rollback);
    BuildStartEvent(event, rollback, source, ts + 2.0,
                    CFVector3(4.0, 5.0, 6.0),
                    CFVector3(1.0, 0.0, 0.0), attributeIndex, source);
    context->sendEventNow(event);
    const bool rollbackStarted = !rollback.isNUL() &&
        rollbackObject != NULL && rollbackObject->started();
    KR_Event pendingCollision;
    pendingCollision.label = b_EVC_CHECK_COLLISION;
    pendingCollision.source = rollback;
    pendingCollision.destination = rollback;
    pendingCollision.timeStamp = ts + 2.0 + delta;
    context->addEvent(pendingCollision);
    RemoveIfPresent(context, rollback);
    const bool pendingEventsRolledBack = rollbackStarted &&
        context->removeEvent(b_EVC_MOVING, rollback) == 0 &&
        context->removeEvent(b_EVC_CHECK_COLLISION, rollback) == 0;

    KR_ObjectID reused =
        g_arena.newObject(subjectTable, "Bullet.Subject.Reuse.Probe");
    BoundedBullet *reusedObject = g_bulletTable.find(reused);
    const bool reusedClean = !reused.isNUL() && reusedObject != NULL &&
                             reusedObject->clean();
    RemoveIfPresent(context, reused);

    return removedAtGround && pendingEventsRolledBack && reusedClean &&
           *moveCount == 2 &&
           g_bulletTable.liveCount() == baseline &&
           !context->isExist("Bullet.Subject.InvalidPayload.Probe") &&
           !context->isExist("Bullet.Subject.InvalidAttribute.Probe") &&
           !context->isExist("Bullet.Subject.InvalidDirection.Probe") &&
           !context->isExist("Bullet.Subject.Flight.Probe") &&
           !context->isExist("Bullet.Subject.Ground.Probe") &&
           !context->isExist("Bullet.Subject.Rollback.Probe") &&
           !context->isExist("Bullet.Subject.Reuse.Probe");
}
