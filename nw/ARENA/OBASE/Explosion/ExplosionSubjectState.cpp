#include "ExplosionSubjectState.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

#include "ExplosionAttributeState.h"
#include "h/olevel.h"
#include "i/dynobj.i"
#include "i/player.i"
#include "i/unit.i"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "message/explmsg.h"
#include "storage/h/subject.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

int g_executedCommands = 0;
int g_damageApplications = 0;
int g_impulseApplications = 0;
int g_allocationRollbacks = 0;
int g_queueRollbacks = 0;
SimulationContext *g_impulseContext = NULL;
KR_ObjectID g_impulseTarget = KR_ObjectID::NUL();
void *g_impulseUser = NULL;
ExplosionImpulseDispatch g_impulseDispatch = NULL;

void ResetImpulseBinding()
{
    g_impulseContext = NULL;
    g_impulseTarget = KR_ObjectID::NUL();
    g_impulseUser = NULL;
    g_impulseDispatch = NULL;
}

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool IsNul(KR_ObjectID value)
{
    return value.isNUL() != 0;
}

void HashBytes(unsigned long long &hash, const void *data, int size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (int index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= kHashPrime;
    }
}

void HashString(unsigned long long &hash, const char *value)
{
    HashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

bool ImpactAttributeReady(const AttributeExplosion *attribute)
{
    return attribute != NULL && std::isfinite(attribute->m_radius) &&
           attribute->m_radius >= 0.0 &&
           std::isfinite(attribute->m_radiusDamage) &&
           attribute->m_radiusDamage > 0.0 &&
           std::isfinite(attribute->m_power) &&
           attribute->m_power >= 0.0 &&
           std::isfinite(attribute->m_impulseCoeff) &&
           attribute->m_impulseCoeff >= 0.0;
}

bool DispatchImpulse(SimulationContext *context,
                     const KR_ObjectID &target,
                     const CFVector3 &targetPosition,
                     const CFVector3 &explosionPosition,
                     double distance, double damage,
                     double impulseCoefficient)
{
    if (context == NULL || context != g_impulseContext ||
        target != g_impulseTarget || g_impulseDispatch == NULL ||
        !std::isfinite(distance) || distance < 0.0 ||
        !std::isfinite(damage) || damage < 0.0 ||
        !std::isfinite(impulseCoefficient) || impulseCoefficient < 0.0)
        return false;
    CFVector3 direction(0.0, 0.0, 0.0);
    if (distance > 1e-12)
        direction = (targetPosition - explosionPosition) / distance;
    const CFVector3 impulse = direction * (damage * impulseCoefficient);
    if (!FiniteVector(impulse) ||
        !g_impulseDispatch(g_impulseUser, impulse, 5.0))
        return false;
    ++g_impulseApplications;
    return true;
}

int ApplyRadialDamage(SimulationContext *context,
                      const AttributeExplosion &attribute,
                      const CFVector3 &position, double timeStamp,
                      const KR_ObjectID &damageOwner)
{
    ct_SubjectFindData found;
    g_arena.findFirstSubject(found,
                             position.x - attribute.m_radius,
                             position.z - attribute.m_radius,
                             position.x + attribute.m_radius,
                             position.z + attribute.m_radius);

    IPlayer *player = NULL;
    IUnit *ownerUnit = NULL;
    if (!IsNul(damageOwner))
    {
        player = static_cast<IPlayer *>(
            context->queryInterface(damageOwner, IPlayerIID));
        ownerUnit = static_cast<IUnit *>(
            context->queryInterface(damageOwner, IUnitIID));
    }

    int applications = 0;
    for (int index = 0; index < found.getCount(); ++index)
    {
        const KR_ObjectID target(found[index]);
        IDynamicObject *dynamic = static_cast<IDynamicObject *>(
            context->queryInterface(target, IDynamicObjectIID));
        IUnit *unit = static_cast<IUnit *>(
            context->queryInterface(target, IUnitIID));
        if (dynamic == NULL || unit == NULL)
            continue;

        const CFVector3 targetPosition = dynamic->getPos();
        const double targetRadius = dynamic->getRadius();
        if (!FiniteVector(targetPosition) || !std::isfinite(targetRadius) ||
            targetRadius < 0.0)
            continue;
        const double distance = Abs(targetPosition - position);
        if (!std::isfinite(distance) ||
            distance >= targetRadius + attribute.m_radiusDamage)
            continue;

        double damage = attribute.m_power *
            (1.0 - (distance - targetRadius) /
                       attribute.m_radiusDamage);
        if (player == NULL && ownerUnit != NULL &&
            unit->isFriend(ownerUnit->getCommander()))
            damage *= g_levelAttr.m_fromFriendDamageScale;
        if (!std::isfinite(damage) || damage < 0.0)
            continue;

        unit->setDamage(damage, position, timeStamp, damageOwner);
        ++applications;
        if (player != NULL)
        {
            const KR_ObjectID commander = unit->getCommander();
            if (!IsNul(commander))
                player->attackUnit(commander, damage);
        }
        DispatchImpulse(context, target, targetPosition, position,
                        distance, damage, attribute.m_impulseCoeff);
    }
    return applications;
}

class BoundedExplosion : public ct_Subject
{
 public:
    BoundedExplosion() { resetState(); }

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
            context->removeEvent(EXPLOSION_START, getObjectID());
        ct_Subject::removeNotify();
        resetState();
    }

    virtual int receiveEvent(KR_Event &event)
    {
        if (event.label != EXPLOSION_START)
            return event.label == KR_WAKE_UP ? 1 : 0;
        return start(event);
    }

    bool clean()
    {
        return !m_started && m_attribute == NULL &&
               IsNul(m_damageOwner) && m_damageApplications == 0 &&
               m_position.x == 0.0 && m_position.y == 0.0 &&
               m_position.z == 0.0 && m_startTime == 0.0;
    }

 private:
    void resetState()
    {
        m_position = CFVector3(0.0, 0.0, 0.0);
        m_attribute = NULL;
        m_damageOwner = KR_ObjectID::NUL();
        m_startTime = 0.0;
        m_damageApplications = 0;
        m_started = false;
    }

    int start(KR_Event &event)
    {
        const int expectedSize = static_cast<int>(
            sizeof(int) + sizeof(double) * 3 + sizeof(KR_ObjectID));
        if (m_started || context == NULL ||
            event.source != getObjectID() ||
            event.destination != getObjectID() ||
            !std::isfinite(event.timeStamp) || event.timeStamp < 0.1)
            return 0;
        s_EventData &data = event.data.open(EDO_READ);
        if (data.remaining() != expectedSize)
        {
            data.close();
            return 0;
        }

        int attributeIndex = -1;
        CFVector3 position;
        KR_ObjectID damageOwner = KR_ObjectID::NUL();
        data.getInt(attributeIndex)
            .getDouble(position.x)
            .getDouble(position.y)
            .getDouble(position.z)
            .getObjectID(damageOwner)
            .close();

        AttributeExplosion *attribute = NULL;
        if (!FiniteVector(position) ||
            !ExplosionAttributeState_ResolveEncodedIndex(
                context, attributeIndex, &attribute) ||
            !ImpactAttributeReady(attribute))
            return 0;

        m_started = true;
        m_attribute = attribute;
        m_damageOwner = damageOwner;
        m_startTime = event.timeStamp;
        setPosition(position);
        m_damageApplications = ApplyRadialDamage(
            context, *attribute, position, event.timeStamp, damageOwner);
        ++g_executedCommands;
        g_damageApplications += m_damageApplications;

        const KR_ObjectID self = getObjectID();
        context->removeObject(self);
        return 1;
    }

    AttributeExplosion *m_attribute;
    KR_ObjectID m_damageOwner;
    double m_startTime;
    int m_damageApplications;
    bool m_started;
};

class BoundedExplosionTable : public ct_SubjectTable
{
 public:
    BoundedExplosionTable() : m_table(NULL)
    {
        registerClass("Explosion");
    }

    virtual void allocObjects(int objectQnty)
    {
        m_table = new (std::nothrow) BoundedExplosion[objectQnty];
        if (m_table == NULL)
            m_maxObjectQnty = 0;
        g_executedCommands = 0;
        g_damageApplications = 0;
        g_impulseApplications = 0;
        g_allocationRollbacks = 0;
        g_queueRollbacks = 0;
        ResetImpulseBinding();
    }

    virtual void freeObjects()
    {
        delete [] m_table;
        m_table = NULL;
        m_maxObjectQnty = 0;
        g_executedCommands = 0;
        g_damageApplications = 0;
        g_impulseApplications = 0;
        g_allocationRollbacks = 0;
        g_queueRollbacks = 0;
        ResetImpulseBinding();
    }

    virtual ct_Object *getObjectPTR(int index)
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "BoundedExplosionTable::getObjectPTR");
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

    BoundedExplosion *find(const KR_ObjectID &id) const
    {
        for (ct_Subject *object = findFirstSubject(); object != NULL;
             object = findNextSubject(object))
            if (object->getObjectID() == id)
                return static_cast<BoundedExplosion *>(object);
        return NULL;
    }

 private:
    BoundedExplosion *m_table;
};

BoundedExplosionTable g_explosionTable;

bool ResolveRequest(SimulationContext *context,
                    const ExplosionImpactRequest &request,
                    AttributeExplosion **attribute)
{
    if (attribute == NULL)
        return false;
    *attribute = NULL;
    if (context == NULL || g_arena.getContext() != context ||
        !FiniteVector(request.position) ||
        !std::isfinite(request.timeStamp) || request.timeStamp < 0.1 ||
        request.objectName == NULL || request.objectName[0] == 0 ||
        request.subjectTable == ct_NULLID ||
        request.subjectTable !=
            g_arena.searchSeanceClassTable("Explosion") ||
        !ExplosionAttributeState_ResolveEncodedIndex(
            context, request.attributeIndex, attribute) ||
        !ImpactAttributeReady(*attribute))
        return false;
    return true;
}

void BuildStartEvent(KR_Event &event, const KR_ObjectID &child,
                     const ExplosionImpactRequest &request)
{
    event.label = EXPLOSION_START;
    event.source = child;
    event.destination = child;
    event.timeStamp = request.timeStamp;
    event.data.open(EDO_WRITE)
        .putInt(request.attributeIndex)
        .putDouble(request.position.x)
        .putDouble(request.position.y)
        .putDouble(request.position.z)
        .putObjectID(request.damageOwner)
        .close();
}

void RemoveIfPresent(SimulationContext *context, const KR_ObjectID &object)
{
    if (context != NULL && !IsNul(object) && context->isExist(object))
        context->removeObject(object);
}

struct ImpulseProbeCapture
{
    int applications;
    CFVector3 impulse;
    double factor;
};

bool CaptureImpulse(void *user, const CFVector3 &impulse, double factor)
{
    ImpulseProbeCapture *capture =
        static_cast<ImpulseProbeCapture *>(user);
    if (capture == NULL || !FiniteVector(impulse) ||
        !std::isfinite(factor))
        return false;
    ++capture->applications;
    capture->impulse = impulse;
    capture->factor = factor;
    return true;
}

bool NearlyEqual(double actual, double expected)
{
    const double scale = 1.0 + std::fabs(expected);
    return std::fabs(actual - expected) <= 1e-10 * scale;
}

}  // namespace

void ExplosionSubjectState_Link()
{
}

bool ExplosionSubjectState_TableReady(SimulationContext *context,
                                      int expectedCapacity)
{
    return context != NULL && g_arena.getContext() == context &&
           expectedCapacity > 0 &&
           g_explosionTable.capacity() == expectedCapacity &&
           g_arena.searchSeanceClassTable("Explosion") != ct_NULLID &&
           g_explosionTable.liveCount() == 0;
}

int ExplosionSubjectState_Capacity()
{
    return g_explosionTable.capacity();
}

int ExplosionSubjectState_LiveCount()
{
    return g_explosionTable.liveCount();
}

bool ExplosionSubjectState_BindImpulseTarget(
    SimulationContext *context, const KR_ObjectID &target,
    void *user, ExplosionImpulseDispatch dispatch)
{
    IDynamicObject *dynamic = context == NULL || IsNul(target)
        ? NULL
        : static_cast<IDynamicObject *>(
              context->queryInterface(target, IDynamicObjectIID));
    IUnit *unit = context == NULL || IsNul(target)
        ? NULL
        : static_cast<IUnit *>(context->queryInterface(target, IUnitIID));
    if (context == NULL || g_arena.getContext() != context ||
        IsNul(target) || !context->isExist(target) || dispatch == NULL ||
        dynamic == NULL || unit == NULL)
        return false;
    g_impulseContext = context;
    g_impulseTarget = target;
    g_impulseUser = user;
    g_impulseDispatch = dispatch;
    return true;
}

void ExplosionSubjectState_UnbindImpulseTarget(
    SimulationContext *context)
{
    if (context == NULL || context == g_impulseContext)
        ResetImpulseBinding();
}

bool ExplosionSubjectState_ImpulseTargetReady(
    SimulationContext *context, const KR_ObjectID &target)
{
    return context != NULL && context == g_impulseContext &&
           !IsNul(target) && target == g_impulseTarget &&
           context->isExist(target) && g_impulseDispatch != NULL;
}

unsigned long long ExplosionSubjectState_Fingerprint(
    SimulationContext *context)
{
    if (!ExplosionSubjectState_TableReady(
            context, g_explosionTable.capacity()))
        return 0;
    unsigned long long hash = kHashOffset;
    const int capacity = g_explosionTable.capacity();
    const int rendering = 0;
    const int audible = 0;
    const int boundedImpactCommand = 1;
    const int radialDamage = 1;
    const int friendlyPlayerAttribution = 1;
    const int selfOwnedQueuedEvent = 1;
    const int impulse = 1;
    const int light = 0;
    const int particles = 0;
    const int sound = 0;
    HashString(hash, "Explosion");
    HashBytes(hash, &capacity, sizeof(capacity));
    HashBytes(hash, &rendering, sizeof(rendering));
    HashBytes(hash, &audible, sizeof(audible));
    HashBytes(hash, &boundedImpactCommand, sizeof(boundedImpactCommand));
    HashBytes(hash, &radialDamage, sizeof(radialDamage));
    HashBytes(hash, &friendlyPlayerAttribution,
              sizeof(friendlyPlayerAttribution));
    HashBytes(hash, &selfOwnedQueuedEvent,
              sizeof(selfOwnedQueuedEvent));
    HashBytes(hash, &impulse, sizeof(impulse));
    HashBytes(hash, &light, sizeof(light));
    HashBytes(hash, &particles, sizeof(particles));
    HashBytes(hash, &sound, sizeof(sound));
    return hash;
}

bool ExplosionSubjectState_QueueBatch(
    SimulationContext *context, const ExplosionImpactRequest *requests,
    int requestCount, KR_ObjectID *children)
{
    if (requests == NULL || children == NULL || requestCount <= 0 ||
        requestCount > 2)
        return false;
    for (int index = 0; index < requestCount; ++index)
        children[index] = KR_ObjectID::NUL();

    AttributeExplosion *attributes[2] = {NULL, NULL};
    for (int index = 0; index < requestCount; ++index)
    {
        if (!ResolveRequest(context, requests[index], &attributes[index]) ||
            (index > 0 &&
             requests[index].timeStamp < requests[index - 1].timeStamp))
            return false;
    }
    if (g_explosionTable.capacity() - g_explosionTable.liveCount() <
        requestCount)
    {
        ++g_allocationRollbacks;
        return false;
    }

    int created = 0;
    for (; created < requestCount; ++created)
    {
        children[created] = g_arena.newObject(
            requests[created].subjectTable,
            requests[created].objectName);
        if (IsNul(children[created]))
            break;
    }
    if (created != requestCount)
    {
        for (int index = created - 1; index >= 0; --index)
        {
            RemoveIfPresent(context, children[index]);
            children[index] = KR_ObjectID::NUL();
        }
        ++g_allocationRollbacks;
        return false;
    }

    for (int index = 0; index < requestCount; ++index)
    {
        KR_Event event;
        BuildStartEvent(event, children[index], requests[index]);
        context->addEvent(event);
    }
    return true;
}

bool ExplosionSubjectState_RollbackQueued(
    SimulationContext *context, const KR_ObjectID *children,
    int childCount)
{
    if (context == NULL || children == NULL || childCount <= 0 ||
        childCount > 2)
        return false;
    bool clean = true;
    int rolledBack = 0;
    for (int index = childCount - 1; index >= 0; --index)
    {
        if (IsNul(children[index]) || !context->isExist(children[index]))
        {
            clean = false;
            continue;
        }
        if (context->removeEvent(EXPLOSION_START, children[index]) == 0)
            clean = false;
        context->removeObject(children[index]);
        if (context->isExist(children[index]))
            clean = false;
        else
            ++rolledBack;
    }
    g_queueRollbacks += rolledBack;
    return clean;
}

bool ExplosionSubjectState_ExecuteNow(
    SimulationContext *context, const ExplosionImpactRequest &request,
    int *damageApplications)
{
    if (damageApplications == NULL)
        return false;
    *damageApplications = 0;
    AttributeExplosion *attribute = NULL;
    if (!ResolveRequest(context, request, &attribute))
        return false;
    const KR_ObjectID child = g_arena.newObject(
        request.subjectTable, request.objectName);
    BoundedExplosion *object = g_explosionTable.find(child);
    if (IsNul(child) || object == NULL)
    {
        RemoveIfPresent(context, child);
        return false;
    }
    KR_Event event;
    BuildStartEvent(event, child, request);
    const int beforeCommands = g_executedCommands;
    const int beforeDamage = g_damageApplications;
    const int accepted = object->receiveEvent(event);
    const bool complete = accepted == 1 && !context->isExist(child) &&
                          g_executedCommands == beforeCommands + 1;
    if (!complete)
        RemoveIfPresent(context, child);
    *damageApplications = g_damageApplications - beforeDamage;
    return complete;
}

bool ExplosionSubjectState_ProbeLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionImpactProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const int baseline = g_explosionTable.liveCount();
    const int capacity = g_explosionTable.capacity();
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        baseline != 0 || capacity < 2 ||
        !ExplosionSubjectState_TableReady(context, capacity))
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex =
        g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || attributeTable == ct_NULLID ||
        subjectTable == ct_NULLID || attributeIndex == -1)
        return false;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const CFVector3 farPosition(1000000.0, 1000000.0, 1000000.0);

    KR_ObjectID invalid = g_arena.newObject(
        subjectTable, "Explosion.InvalidPayload.Probe");
    BoundedExplosion *invalidObject = g_explosionTable.find(invalid);
    KR_Event event;
    event.label = EXPLOSION_START;
    event.source = invalid;
    event.destination = invalid;
    event.timeStamp = ts;
    event.data.open(EDO_WRITE).putInt(attributeIndex).close();
    const bool invalidPayload = !IsNul(invalid) && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 && invalidObject->clean();
    RemoveIfPresent(context, invalid);

    invalid = g_arena.newObject(
        subjectTable, "Explosion.InvalidAttribute.Probe");
    invalidObject = g_explosionTable.find(invalid);
    ExplosionImpactRequest invalidRequest = {
        farPosition, ts, g_arena.getObjectID(), subjectTable,
        0x7fffffff, "Explosion.InvalidAttribute.Probe"};
    BuildStartEvent(event, invalid, invalidRequest);
    const bool invalidAttribute = !IsNul(invalid) && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 && invalidObject->clean();
    RemoveIfPresent(context, invalid);
    if (!invalidPayload || !invalidAttribute)
        return false;
    summary->invalidStarts = 2;

    std::vector<KR_ObjectID> fillers;
    fillers.reserve(static_cast<std::size_t>(capacity - 1));
    for (int index = 0; index < capacity - 1; ++index)
    {
        char name[64] = {};
        std::snprintf(name, sizeof(name),
                      "Explosion.Capacity.Probe.%d", index);
        const KR_ObjectID filler = g_arena.newObject(subjectTable, name);
        if (IsNul(filler))
            break;
        fillers.push_back(filler);
    }
    ExplosionImpactRequest atomicRequests[2] = {
        {farPosition, ts + 1.0, g_arena.getObjectID(), subjectTable,
         attributeIndex, "Explosion.Atomic.Splash.Probe"},
        {farPosition, ts + 2.0, g_arena.getObjectID(), subjectTable,
         attributeIndex, "Explosion.Atomic.Impact.Probe"}};
    KR_ObjectID atomicChildren[2] = {
        KR_ObjectID::NUL(), KR_ObjectID::NUL()};
    const int beforeAllocationRollbacks = g_allocationRollbacks;
    const bool allocationRolledBack =
        static_cast<int>(fillers.size()) == capacity - 1 &&
        !ExplosionSubjectState_QueueBatch(
            context, atomicRequests, 2, atomicChildren) &&
        IsNul(atomicChildren[0]) && IsNul(atomicChildren[1]) &&
        g_explosionTable.liveCount() == capacity - 1 &&
        g_allocationRollbacks == beforeAllocationRollbacks + 1;
    for (std::size_t index = 0; index < fillers.size(); ++index)
        RemoveIfPresent(context, fillers[index]);
    if (!allocationRolledBack || g_explosionTable.liveCount() != baseline)
        return false;
    summary->allocationRollbacks = 1;

    KR_ObjectID queuedChild = KR_ObjectID::NUL();
    ExplosionImpactRequest queuedRequest = {
        farPosition, ts + 3.0, g_arena.getObjectID(), subjectTable,
        attributeIndex, "Explosion.QueueRollback.Probe"};
    const int beforeQueueRollbacks = g_queueRollbacks;
    const bool queued = ExplosionSubjectState_QueueBatch(
        context, &queuedRequest, 1, &queuedChild);
    const bool queueRolledBack = queued && !IsNul(queuedChild) &&
        g_explosionTable.liveCount() == baseline + 1 &&
        ExplosionSubjectState_RollbackQueued(
            context, &queuedChild, 1) &&
        g_queueRollbacks == beforeQueueRollbacks + 1 &&
        g_explosionTable.liveCount() == baseline;
    if (!queueRolledBack)
        return false;
    summary->queuedCommands = 1;
    summary->queueRollbacks = 1;

    ExplosionImpactRequest executeRequest = {
        farPosition, ts + 4.0, g_arena.getObjectID(), subjectTable,
        attributeIndex, "Explosion.Execute.Probe"};
    int damageApplications = -1;
    if (!ExplosionSubjectState_ExecuteNow(
            context, executeRequest, &damageApplications) ||
        damageApplications != 0 ||
        g_explosionTable.liveCount() != baseline)
        return false;
    summary->executedCommands = 1;
    summary->damageApplications = 0;

    const KR_ObjectID reused = g_arena.newObject(
        subjectTable, "Explosion.Reuse.Probe");
    BoundedExplosion *reusedObject = g_explosionTable.find(reused);
    const bool reusedClean = !IsNul(reused) && reusedObject != NULL &&
                             reusedObject->clean();
    RemoveIfPresent(context, reused);
    return reusedClean && g_explosionTable.liveCount() == baseline &&
           !context->isExist("Explosion.InvalidPayload.Probe") &&
           !context->isExist("Explosion.InvalidAttribute.Probe") &&
           !context->isExist("Explosion.Atomic.Splash.Probe") &&
           !context->isExist("Explosion.Atomic.Impact.Probe") &&
           !context->isExist("Explosion.QueueRollback.Probe") &&
           !context->isExist("Explosion.Execute.Probe") &&
           !context->isExist("Explosion.Reuse.Probe");
}

bool ExplosionSubjectState_ProbeDamageLifecycle(
    SimulationContext *context, const char *attributeName,
    const KR_ObjectID &target, const KR_ObjectID &damageOwner,
    double timeStamp, ExplosionImpactProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        IsNul(target) || !context->isExist(target) ||
        g_explosionTable.liveCount() != 0)
        return false;
    IDynamicObject *dynamic = static_cast<IDynamicObject *>(
        context->queryInterface(target, IDynamicObjectIID));
    IUnit *unit = static_cast<IUnit *>(
        context->queryInterface(target, IUnitIID));
    if (dynamic == NULL || unit == NULL)
        return false;
    const CFVector3 targetPosition = dynamic->getPos();
    const double targetRadius = dynamic->getRadius();
    if (!FiniteVector(targetPosition) || targetPosition.y <= 0.0 ||
        !std::isfinite(targetRadius) || targetRadius < 0.0)
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex =
        g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || !ImpactAttributeReady(attribute) ||
        attributeTable == ct_NULLID || subjectTable == ct_NULLID ||
        attributeIndex == -1)
        return false;

    const double offset = std::fmin(attribute->m_radius * 0.25,
                                    attribute->m_radiusDamage * 0.25);
    if (!std::isfinite(offset) || offset <= 0.0)
        return false;
    const CFVector3 impactPosition =
        targetPosition - CFVector3(offset, 0.0, 0.0);
    if (!FiniteVector(impactPosition))
        return false;

    SimulationContext *savedContext = g_impulseContext;
    const KR_ObjectID savedTarget = g_impulseTarget;
    void *savedUser = g_impulseUser;
    ExplosionImpulseDispatch savedDispatch = g_impulseDispatch;
    ImpulseProbeCapture impulseCapture = {
        0, CFVector3(0.0, 0.0, 0.0), 0.0};
    if (!ExplosionSubjectState_BindImpulseTarget(
            context, target, &impulseCapture, CaptureImpulse))
        return false;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    ExplosionImpactRequest request = {
        impactPosition, ts, damageOwner, subjectTable, attributeIndex,
        "Explosion.DynamicDamage.Probe"};
    int damageApplications = 0;
    const int beforeImpulseApplications = g_impulseApplications;
    const bool executed = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    g_impulseContext = savedContext;
    g_impulseTarget = savedTarget;
    g_impulseUser = savedUser;
    g_impulseDispatch = savedDispatch;
    const double expectedDamage = attribute->m_power *
        (1.0 - (offset - targetRadius) / attribute->m_radiusDamage);
    const double expectedImpulse =
        expectedDamage * attribute->m_impulseCoeff;
    summary->executedCommands = executed ? 1 : 0;
    summary->damageApplications = damageApplications;
    summary->impulseApplications = impulseCapture.applications;
    summary->expectedDamage = expectedDamage;
    summary->impactPositionX = impactPosition.x;
    summary->impactPositionY = impactPosition.y;
    summary->impactPositionZ = impactPosition.z;
    summary->expectedImpulseX = expectedImpulse;
    summary->expectedImpulseY = 0.0;
    summary->expectedImpulseZ = 0.0;
    summary->impulseFactor = 5.0;
    return executed && damageApplications == 1 &&
           std::isfinite(expectedDamage) && expectedDamage >= 0.0 &&
           impulseCapture.applications == 1 &&
           g_impulseApplications == beforeImpulseApplications + 1 &&
           NearlyEqual(impulseCapture.impulse.x, expectedImpulse) &&
           NearlyEqual(impulseCapture.impulse.y, 0.0) &&
           NearlyEqual(impulseCapture.impulse.z, 0.0) &&
           NearlyEqual(impulseCapture.factor, 5.0) &&
           g_explosionTable.liveCount() == 0 &&
           !context->isExist("Explosion.DynamicDamage.Probe");
}
