#include "BulletSubjectState.h"

#include <cmath>
#include <cstring>
#include <new>

#define LAST_H__SCENE
#include "game.h"

#include "BulletAttributeState.h"
#include "enum/SpaceEnum.h"
#include "i/dynobj.i"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "message/a_msg.h"
#include "message/bulmsg.h"
#include "message/funitmsg.h"
#include "storage/h/subject.h"

namespace {

const double kGravity = -9.8;
const double kBulletCollisionRadius = 0.01;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

enum BulletImpactKind
{
    BULLET_IMPACT_NONE = 0,
    BULLET_IMPACT_DYNAMIC = 1,
    BULLET_IMPACT_SCENE = 2
};

struct BulletImpact
{
    BulletImpactKind kind;
    double time;
    KR_ObjectID object;
};

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

bool SphereImpactTime(const CFVector3 &centerOffset,
                      const CFVector3 &relativeVelocity,
                      double radius, double horizon, double *impactTime)
{
    if (impactTime == NULL || !FiniteVector(centerOffset) ||
        !FiniteVector(relativeVelocity) || !std::isfinite(radius) ||
        !std::isfinite(horizon) || radius < 0.0 || horizon < 0.0)
        return false;
    *impactTime = 0.0;
    const double radius2 = radius * radius;
    const double distance2 = Abs2(centerOffset);
    if (!std::isfinite(radius2) || !std::isfinite(distance2))
        return false;
    if (distance2 <= radius2)
        return true;

    const double speed2 = Abs2(relativeVelocity);
    const double toward = relativeVelocity * centerOffset;
    if (!std::isfinite(speed2) || !std::isfinite(toward) ||
        speed2 <= 1.0e-4 || toward < 0.0)
        return false;
    const double discriminant =
        4.0 * toward * toward -
        4.0 * (distance2 - radius2) * speed2;
    if (!std::isfinite(discriminant) || discriminant < 0.0)
        return false;
    const double result =
        (2.0 * toward - std::sqrt(discriminant)) / (2.0 * speed2);
    if (!std::isfinite(result) || result < 0.0 || result > horizon)
        return false;
    *impactTime = result;
    return true;
}

bool WaterlineImpactTime(const CFVector3 &position,
                         const CFVector3 &velocity,
                         double waterline, double horizon,
                         double *impactTime)
{
    if (impactTime == NULL || !FiniteVector(position) ||
        !FiniteVector(velocity) || !std::isfinite(waterline) ||
        !std::isfinite(horizon) || horizon < 0.0 ||
        velocity.y >= 0.0 || position.y <= waterline)
        return false;
    const double result = (waterline - position.y) / velocity.y;
    if (!std::isfinite(result) || result < 0.0 || result > horizon)
        return false;
    *impactTime = result;
    return true;
}

BulletImpact SelectEarliestImpact(bool dynamicHit, double dynamicTime,
                                  KR_ObjectID dynamicObject,
                                  bool sceneHit, double sceneTime)
{
    BulletImpact result = {BULLET_IMPACT_NONE, 0.0, KR_ObjectID::NUL()};
    // Preserve the retail tie rule: scene/order wins equal-time hits.
    if (dynamicHit && (!sceneHit || dynamicTime < sceneTime))
    {
        result.kind = BULLET_IMPACT_DYNAMIC;
        result.time = dynamicTime;
        result.object = dynamicObject;
    }
    else if (sceneHit)
    {
        result.kind = BULLET_IMPACT_SCENE;
        result.time = sceneTime;
    }
    return result;
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
            return checkCollision(event);
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
               m_lastCollisionTimeStamp == 0.0 &&
               m_collisionCheckCount == 0 && m_sceneQueryCount == 0 &&
               !m_hasWaterline && !m_crossedWaterline &&
               m_position.x == 0.0 && m_position.y == 0.0 &&
               m_position.z == 0.0 && m_velocity.x == 0.0 &&
               m_velocity.y == 0.0 && m_velocity.z == 0.0;
    }

    bool started() const { return m_started; }
    int moveCount() const { return m_moveCount; }
    AttributeBullet *attribute() const { return m_attribute; }
    const CFVector3 &velocity() const { return m_velocity; }
    int collisionCheckCount() const { return m_collisionCheckCount; }
    int sceneQueryCount() const { return m_sceneQueryCount; }

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
        m_lastCollisionTimeStamp = 0.0;
        m_waterline = 0.0;
        m_hasWaterline = false;
        m_crossedWaterline = false;
        m_collisionCheckCount = 0;
        m_sceneQueryCount = 0;
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

    bool scheduleCollision(double timeStamp)
    {
        if (context == NULL || !std::isfinite(timeStamp) ||
            timeStamp < 0.1)
            return false;
        KR_Event collision;
        collision.label = b_EVC_CHECK_COLLISION;
        collision.source = getObjectID();
        collision.destination = getObjectID();
        collision.timeStamp = timeStamp;
        m_lastCollisionTimeStamp = timeStamp;
        issueEvent(collision);
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
            attribute->m_moveTimeIncrement <= 0.0 ||
            !std::isfinite(attribute->m_chkClzTimeIncrement) ||
            attribute->m_chkClzTimeIncrement <= 0.0)
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
        CViewScene *scene = CViewScene::Current();
        if (scene != NULL && scene->GetTerrain() != NULL)
        {
            m_waterline = scene->GetTerrain()->Waterline();
            m_hasWaterline = std::isfinite(m_waterline) != 0;
        }
        setPosition(position);
        if (position.y <= 0.0)
        {
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
            return 1;
        }
        if (!scheduleMove(event.timeStamp,
                          event.timeStamp +
                              attribute->m_moveTimeIncrement) ||
            !scheduleCollision(event.timeStamp))
        {
            context->removeEvent(b_EVC_MOVING, getObjectID());
            context->removeEvent(b_EVC_CHECK_COLLISION, getObjectID());
            setPosition(CFVector3(0.0, 0.0, 0.0));
            resetState();
            return 0;
        }
        return 1;
    }

    BulletImpact findImpact(double horizon)
    {
        bool dynamicHit = false;
        double dynamicTime = horizon;
        KR_ObjectID dynamicObject = KR_ObjectID::NUL();
        const CFVector3 end = m_position + m_velocity * horizon;
        if (FiniteVector(end))
        {
            ct_SubjectFindData found;
            g_arena.findFirstSubject(found, m_position.x, m_position.z,
                                     end.x, end.z);
            for (int index = 0; index < found.getCount(); ++index)
            {
                const KR_ObjectID candidate(found[index]);
                if (candidate == m_master || candidate == getObjectID())
                    continue;
                IDynamicObject *object = static_cast<IDynamicObject *>(
                    context->queryInterface(candidate, IDynamicObjectIID));
                if (object == NULL)
                    continue;
                const CFVector3 targetPosition = object->getPos();
                const CFVector3 targetVelocity =
                    object->getMoveDir() * object->getMoveSpeed();
                const double targetRadius = object->getRadius();
                double candidateTime = 0.0;
                if (!FiniteVector(targetPosition) ||
                    !FiniteVector(targetVelocity) ||
                    !std::isfinite(targetRadius) || targetRadius < 0.0 ||
                    !SphereImpactTime(targetPosition - m_position,
                                      m_velocity - targetVelocity,
                                      targetRadius, horizon,
                                      &candidateTime))
                    continue;
                if (!dynamicHit || candidateTime < dynamicTime)
                {
                    dynamicHit = true;
                    dynamicTime = candidateTime;
                    dynamicObject = candidate;
                }
            }
        }

        bool sceneHit = false;
        double sceneTime = horizon;
        CViewScene *scene = CViewScene::Current();
        if (scene != NULL && scene->Order() != NULL)
        {
            ++m_sceneQueryCount;
            SBumpDef bump;
            bump.start = m_position;
            bump.vel = m_velocity;
            bump.vel1 = CFVector3(0.0, 0.0, 0.0);
            bump.fTime = horizon;
            bump.fRadius = kBulletCollisionRadius;
            bump.dwFlags1 = 0;
            bump.fMass = 0.001;
            bump.nBumpFlags = 0;
            bump.pBonus = NULL;
            bump.pBumpRef = NULL;
            if (scene->Order()->Bump(bump) &&
                std::isfinite(bump.fTime) && bump.fTime >= 0.0 &&
                bump.fTime <= horizon)
            {
                sceneHit = true;
                sceneTime = bump.fTime;
            }
        }
        return SelectEarliestImpact(dynamicHit, dynamicTime, dynamicObject,
                                    sceneHit, sceneTime);
    }

    int checkCollision(KR_Event &event)
    {
        if (!m_started || m_attribute == NULL || context == NULL ||
            event.source != getObjectID() ||
            event.destination != getObjectID() || event.data.size() != 0 ||
            !std::isfinite(event.timeStamp) || event.timeStamp < 0.1 ||
            !NearlyEqual(event.timeStamp, m_lastCollisionTimeStamp) ||
            !std::isfinite(m_attribute->m_chkClzTimeIncrement) ||
            m_attribute->m_chkClzTimeIncrement <= 0.0)
            return 0;

        const double horizon = m_attribute->m_chkClzTimeIncrement;
        double waterTime = 0.0;
        const bool waterHit = m_hasWaterline && !m_crossedWaterline &&
            WaterlineImpactTime(m_position, m_velocity, m_waterline,
                                horizon, &waterTime);
        const BulletImpact impact = findImpact(horizon);
        ++m_collisionCheckCount;
        if (waterHit &&
            (impact.kind == BULLET_IMPACT_NONE || waterTime < impact.time))
            m_crossedWaterline = true;

        if (impact.kind != BULLET_IMPACT_NONE)
        {
            context->removeEvent(b_EVC_MOVING, getObjectID());
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
            return 1;
        }
        if (!scheduleCollision(event.timeStamp + horizon))
        {
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
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
    double m_lastCollisionTimeStamp;
    double m_waterline;
    bool m_hasWaterline;
    bool m_crossedWaterline;
    int m_collisionCheckCount;
    int m_sceneQueryCount;
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
    const int sceneOrderCollision = 1;
    const int dynamicSphereCollision = 1;
    const int waterlineIntersection = 1;
    const int effects = 0;
    HashString(hash, "Bullet");
    HashBytes(hash, &capacity, sizeof(capacity));
    HashBytes(hash, &rendering, sizeof(rendering));
    HashBytes(hash, &audible, sizeof(audible));
    HashBytes(hash, &ballisticFreeFlight, sizeof(ballisticFreeFlight));
    HashBytes(hash, &groundRemoval, sizeof(groundRemoval));
    HashBytes(hash, &sceneOrderCollision, sizeof(sceneOrderCollision));
    HashBytes(hash, &dynamicSphereCollision,
              sizeof(dynamicSphereCollision));
    HashBytes(hash, &waterlineIntersection,
              sizeof(waterlineIntersection));
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
        context->removeEvent(b_EVC_MOVING, invalid) == 0 &&
        context->removeEvent(b_EVC_CHECK_COLLISION, invalid) == 0;
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
        context->removeEvent(b_EVC_MOVING, invalid) == 0 &&
        context->removeEvent(b_EVC_CHECK_COLLISION, invalid) == 0;
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
        context->removeEvent(b_EVC_MOVING, invalid) == 0 &&
        context->removeEvent(b_EVC_CHECK_COLLISION, invalid) == 0;
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
    const bool firstCollisionScheduled =
        context->removeEvent(b_EVC_CHECK_COLLISION, flight) != 0;
    if (!started || !speedAnswered ||
        !NearlyEqual(queriedSpeed, attribute->m_startSpeed) ||
        !firstScheduled || !firstCollisionScheduled)
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

bool BulletSubjectState_ProbeCollisionLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletCollisionProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const int baseline = g_bulletTable.liveCount();
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        baseline != 0 ||
        !BulletSubjectState_TableReady(context, g_bulletTable.capacity()))
        return false;

    double impactTime = 0.0;
    const bool sphereInside =
        SphereImpactTime(CFVector3(1.0, 0.0, 0.0),
                         CFVector3(0.0, 0.0, 0.0), 2.0, 1.0,
                         &impactTime) && NearlyEqual(impactTime, 0.0);
    const bool sphereApproach =
        SphereImpactTime(CFVector3(10.0, 0.0, 0.0),
                         CFVector3(2.0, 0.0, 0.0), 2.0, 5.0,
                         &impactTime) && NearlyEqual(impactTime, 4.0);
    const bool sphereDeparture =
        !SphereImpactTime(CFVector3(10.0, 0.0, 0.0),
                          CFVector3(-2.0, 0.0, 0.0), 2.0, 5.0,
                          &impactTime);
    const bool sphereMiss =
        !SphereImpactTime(CFVector3(10.0, 10.0, 0.0),
                          CFVector3(2.0, 0.0, 0.0), 1.0, 5.0,
                          &impactTime);
    if (!sphereInside || !sphereApproach || !sphereDeparture ||
        !sphereMiss)
        return false;
    summary->sphereCases = 4;

    const KR_ObjectID target = g_arena.getObjectID();
    const BulletImpact dynamicFirst =
        SelectEarliestImpact(true, 0.25, target, true, 0.5);
    const BulletImpact sceneFirst =
        SelectEarliestImpact(true, 0.75, target, true, 0.5);
    const BulletImpact sceneTie =
        SelectEarliestImpact(true, 0.5, target, true, 0.5);
    if (dynamicFirst.kind != BULLET_IMPACT_DYNAMIC ||
        dynamicFirst.object != target ||
        !NearlyEqual(dynamicFirst.time, 0.25) ||
        sceneFirst.kind != BULLET_IMPACT_SCENE ||
        !NearlyEqual(sceneFirst.time, 0.5) ||
        sceneTie.kind != BULLET_IMPACT_SCENE ||
        !NearlyEqual(sceneTie.time, 0.5))
        return false;
    summary->earliestHitCases = 3;

    const bool waterCrossing =
        WaterlineImpactTime(CFVector3(0.0, 10.0, 0.0),
                            CFVector3(0.0, -20.0, 0.0), 5.0, 1.0,
                            &impactTime) && NearlyEqual(impactTime, 0.25);
    const bool waterBeyondHorizon =
        !WaterlineImpactTime(CFVector3(0.0, 10.0, 0.0),
                             CFVector3(0.0, -2.0, 0.0), 5.0, 1.0,
                             &impactTime);
    const bool waterMovingUp =
        !WaterlineImpactTime(CFVector3(0.0, 10.0, 0.0),
                             CFVector3(0.0, 2.0, 0.0), 5.0, 1.0,
                             &impactTime);
    const bool waterAlreadyBelow =
        !WaterlineImpactTime(CFVector3(0.0, 4.0, 0.0),
                             CFVector3(0.0, -2.0, 0.0), 5.0, 1.0,
                             &impactTime);
    if (!waterCrossing || !waterBeyondHorizon || !waterMovingUp ||
        !waterAlreadyBelow)
        return false;
    summary->waterlineCases = 4;

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
    if (attributeIndex == -1)
        return false;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    KR_ObjectID collision = g_arena.newObject(
        subjectTable, "Bullet.Subject.Collision.Probe");
    BoundedBullet *collisionObject = g_bulletTable.find(collision);
    KR_Event event;
    BuildStartEvent(event, collision, target, ts,
                    CFVector3(1000000.0, 1000000.0, 1000000.0),
                    CFVector3(1.0, 0.0, 0.0), attributeIndex, target);
    context->sendEventNow(event);
    const bool started = !collision.isNUL() && collisionObject != NULL &&
        collisionObject->started();
    const bool firstCollisionScheduled = started &&
        context->removeEvent(b_EVC_CHECK_COLLISION, collision) != 0;
    if (!started || !firstCollisionScheduled)
    {
        RemoveIfPresent(context, collision);
        return false;
    }
    summary->scheduledChecks = 1;

    KR_Event invalid;
    invalid.label = b_EVC_CHECK_COLLISION;
    invalid.source = collision;
    invalid.destination = collision;
    invalid.timeStamp = ts;
    invalid.data.open(EDO_WRITE).putInt(1).close();
    const bool malformedRejected =
        collisionObject->receiveEvent(invalid) == 0 &&
        collisionObject->collisionCheckCount() == 0;

    KR_Event stale;
    stale.label = b_EVC_CHECK_COLLISION;
    stale.source = collision;
    stale.destination = collision;
    stale.timeStamp = ts + attribute->m_chkClzTimeIncrement;
    const bool staleRejected =
        collisionObject->receiveEvent(stale) == 0 &&
        collisionObject->collisionCheckCount() == 0;

    event = KR_Event();
    event.label = b_EVC_CHECK_COLLISION;
    event.source = collision;
    event.destination = collision;
    event.timeStamp = ts;
    const bool executed = collisionObject->receiveEvent(event) == 1 &&
        context->isExist(collision) &&
        collisionObject->collisionCheckCount() == 1;
    const bool secondCollisionScheduled = executed &&
        context->removeEvent(b_EVC_CHECK_COLLISION, collision) != 0;
    summary->executedChecks = executed ? 1 : 0;
    summary->scheduledChecks += secondCollisionScheduled ? 1 : 0;
    summary->sceneQueries = collisionObject->sceneQueryCount();
    RemoveIfPresent(context, collision);
    const bool rolledBack =
        context->removeEvent(b_EVC_MOVING, collision) == 0 &&
        context->removeEvent(b_EVC_CHECK_COLLISION, collision) == 0;

    return malformedRejected && staleRejected && executed &&
           secondCollisionScheduled &&
           rolledBack && summary->scheduledChecks == 2 &&
           summary->executedChecks == 1 &&
           summary->sceneQueries ==
               (CViewScene::Current() == NULL ? 0 : 1) &&
           g_bulletTable.liveCount() == baseline &&
           !context->isExist("Bullet.Subject.Collision.Probe");
}

bool BulletSubjectState_ProbeDynamicCollisionLifecycle(
    SimulationContext *context, const char *attributeName,
    const KR_ObjectID &target, double timeStamp)
{
    const int baseline = g_bulletTable.liveCount();
    KR_ObjectID targetID = target;
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        targetID.isNUL() || !context->isExist(targetID) || baseline != 0 ||
        !BulletSubjectState_TableReady(context, g_bulletTable.capacity()))
        return false;
    IDynamicObject *dynamic = static_cast<IDynamicObject *>(
        context->queryInterface(targetID, IDynamicObjectIID));
    if (dynamic == NULL)
        return false;
    const CFVector3 targetPosition = dynamic->getPos();
    const double targetRadius = dynamic->getRadius();
    if (!FiniteVector(targetPosition) || targetPosition.y <= 0.0 ||
        !std::isfinite(targetRadius) || targetRadius <= 0.0)
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
    if (attributeIndex == -1)
        return false;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    KR_ObjectID bullet = g_arena.newObject(
        subjectTable, "Bullet.Subject.DynamicCollision.Probe");
    BoundedBullet *bulletObject = g_bulletTable.find(bullet);
    KR_Event event;
    BuildStartEvent(event, bullet, g_arena.getObjectID(), ts,
                    targetPosition, CFVector3(1.0, 0.0, 0.0),
                    attributeIndex, g_arena.getObjectID());
    context->sendEventNow(event);
    const bool started = !bullet.isNUL() && bulletObject != NULL &&
        bulletObject->started() &&
        context->removeEvent(b_EVC_CHECK_COLLISION, bullet) != 0;
    if (!started)
    {
        RemoveIfPresent(context, bullet);
        return false;
    }

    event = KR_Event();
    event.label = b_EVC_CHECK_COLLISION;
    event.source = bullet;
    event.destination = bullet;
    event.timeStamp = ts;
    const bool hit = bulletObject->receiveEvent(event) == 1 &&
                     !context->isExist(bullet);
    RemoveIfPresent(context, bullet);
    return hit && context->removeEvent(b_EVC_MOVING, bullet) == 0 &&
           context->removeEvent(b_EVC_CHECK_COLLISION, bullet) == 0 &&
           g_bulletTable.liveCount() == baseline &&
           !context->isExist("Bullet.Subject.DynamicCollision.Probe");
}
