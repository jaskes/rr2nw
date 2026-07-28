/*
 * File  : D:\GAME\OBASE\Spark\Spark.cpp
 * Autor :
 * Ver   1.0
 */
#include "Spark.h"

#include <cmath>
#include <cstring>
#include <new>

#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/sparkmsg.h"
#include "enum/spaceenum.h"
#include "scene.h"
#include "h/light.h"
#include "SparkSubjectState.h"

#ifdef __TRACE_NW__
#include "afxwin.h"
#else
class CDC {};
#endif

#ifndef RR2NW_SPARK_ATTRIBUTE_STATE_EXTERNAL
#include "SparkAttributeState.inl"
#endif

static s_ELN elnTable[] = {
    s_ELN(sp_EVC_LIFE, "sp_EVC_LIFE"),
    s_ELN(sp_EV_CREATE, "sp_EV_CREATE"),
    s_ELN(sp_EV_SET_PHASE_COUNT, "sp_EV_SET_PHASE_COUNT"),
    s_ELN(sp_EV_SET_PHASE, "sp_EV_SET_PHASE"),
    s_ELN(),
};

static s_ELNTable selnTable("Default", elnTable);

class SparkTable : public ct_SubjectTable
{
 private:
    Spark *m_table;

 public:
    SparkTable() : m_table(NULL) { registerClass("Spark"); }
    ~SparkTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    virtual void allocObjects(int objectQnty);
    virtual void freeObjects();
    virtual ct_Object *getObjectPTR(int index);
    virtual bool isRendering() { return true; }
    virtual bool isAudible() { return false; }
    int capacity() const { return m_maxObjectQnty; }
    int liveCount() const;
    Spark *find(const KR_ObjectID &object) const;
};

static SparkTable __classTable;

namespace {

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

bool IsNul(const KR_ObjectID &value)
{
    KR_ObjectID copy = value;
    return copy.isNUL() != 0;
}

void RemoveIfPresent(SimulationContext *context, const KR_ObjectID &object)
{
    if (context != NULL && !IsNul(object) && context->isExist(object))
        context->removeObject(object);
}

bool AttributeReady(AttributeSpark *attribute)
{
    if (attribute == NULL || attribute->m_cacheSkin == NULL ||
        attribute->m_cacheSkin->HImage() == NULL ||
        attribute->m_phaseCnt <= 0 ||
        attribute->m_phaseCnt > AttributeSpark::MAX_PHASE ||
        !std::isfinite(attribute->m_maxRadius) ||
        attribute->m_maxRadius <= 0.0)
        return false;
    for (int index = 0; index < attribute->m_phaseCnt; ++index)
    {
        const SparkPhase &phase = attribute->m_phase[index];
        if (!std::isfinite(phase.time) || phase.time < 0.0 ||
            (index + 1 < attribute->m_phaseCnt && phase.time <= 0.0) ||
            !std::isfinite(phase.radius) || phase.radius < 0.0 ||
            phase.brightness < 0 || phase.color < 0 ||
            phase.u0 < 0 || phase.v0 < 0 || phase.u1 <= phase.u0 ||
            phase.v1 <= phase.v0 ||
            phase.u1 > attribute->m_cacheSkin->Width() ||
            phase.v1 > attribute->m_cacheSkin->Height())
            return false;
    }
    return true;
}

bool ResolveRequest(SimulationContext *context,
                    const SparkCreateRequest &request,
                    AttributeSpark **attribute)
{
    if (attribute == NULL)
        return false;
    *attribute = NULL;
    if (context == NULL || g_arena.getContext() != context ||
        request.subjectTable == ct_NULLID ||
        request.subjectTable != __classTable.getClassTableID() ||
        request.objectName == NULL || request.objectName[0] == 0 ||
        std::strlen(request.objectName) >= MAX_SYMBOLIC_LENGHT ||
        !FiniteVector(request.position) ||
        !std::isfinite(request.timeStamp) || request.timeStamp < 0.1 ||
        !SparkAttributeState_ResolveEncodedIndex(
            context, request.attributeIndex, attribute) ||
        !AttributeReady(*attribute))
        return false;
    return true;
}

void BuildCreateEvent(KR_Event &event, const KR_ObjectID &child,
                      const SparkCreateRequest &request)
{
    event = KR_Event();
    event.label = sp_EV_CREATE;
    event.source = child;
    event.destination = child;
    event.timeStamp = request.timeStamp;
    event.data.open(EDO_WRITE)
        .descend(VECTOR3D_F, 0)
            .putDouble(request.position.x)
            .putDouble(request.position.y)
            .putDouble(request.position.z)
        .ascend()
        .putInt(request.attributeIndex)
        .close();
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

}  // namespace

Spark::Spark()
{
    resetState();
}

Spark::~Spark()
{
}

void Spark::resetState()
{
    m_attr = &__defaultSparkAttr;
    m_position = CFVector3(0.0, 0.0, 0.0);
    m_curPhase = 0;
    m_nextLifeTime = 0.0;
    m_started = false;
}

bool Spark::clean() const
{
    return !m_started && m_attr == &__defaultSparkAttr &&
           m_curPhase == 0 && m_nextLifeTime == 0.0 &&
           m_position.x == 0.0 && m_position.y == 0.0 &&
           m_position.z == 0.0;
}

int Spark::receiveEvent(KR_Event &event)
{
    switch (event.label)
    {
    case KR_WAKE_UP:
        return 1;

    case sp_EV_CREATE:
        {
            const int expectedSize =
                static_cast<int>(sizeof(double) * 3 + sizeof(int));
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
            CFVector3 position;
            int attrIndex = -1;
            data.descend(VECTOR3D_F, 0)
                    .getDouble(position.x)
                    .getDouble(position.y)
                    .getDouble(position.z)
                .ascend()
                .getInt(attrIndex)
                .close();
            AttributeSpark *attribute = NULL;
            if (!FiniteVector(position) ||
                !SparkAttributeState_ResolveEncodedIndex(
                    context, attrIndex, &attribute) ||
                !AttributeReady(attribute))
                return 0;
            const double next = event.timeStamp + attribute->m_phase[0].time;
            if (!std::isfinite(next) || next <= event.timeStamp)
                return 0;

            m_attr = attribute;
            m_position = position;
            m_curPhase = 0;
            m_nextLifeTime = next;
            m_started = true;
            setPosition(position);

            KR_Event life;
            life.label = sp_EVC_LIFE;
            life.source = getObjectID();
            life.destination = getObjectID();
            life.timeStamp = next;
            context->addEvent(life);
            return 1;
        }

    case sp_EVC_LIFE:
        {
            if (!m_started || !AttributeReady(m_attr) || context == NULL ||
                event.source != getObjectID() ||
                event.destination != getObjectID() || event.data.size() != 0 ||
                !std::isfinite(event.timeStamp) ||
                !NearlyEqual(event.timeStamp, m_nextLifeTime))
                return 0;
            if (m_curPhase >= m_attr->m_phaseCnt - 1)
            {
                const KR_ObjectID self = getObjectID();
                context->removeObject(self);
                return 1;
            }

            // May 1999 keeps the January timing order: the event that enters
            // the next phase is scheduled with the duration of the phase that
            // was visible before the transition.
            const double next =
                event.timeStamp + m_attr->m_phase[m_curPhase].time;
            if (!std::isfinite(next) || next <= event.timeStamp)
                return 0;
            ++m_curPhase;
            m_nextLifeTime = next;
            KR_Event life;
            life.label = sp_EVC_LIFE;
            life.source = getObjectID();
            life.destination = getObjectID();
            life.timeStamp = next;
            context->addEvent(life);
            return 1;
        }

    case KR_SET_ATTR:
        return 0;

    default:
        return 0;
    }
}

void Spark::addNotify()
{
    ct_Subject::addNotify();
    resetState();
}

void Spark::removeNotify()
{
    if (context != NULL)
    {
        context->removeEvent(sp_EV_CREATE, getObjectID());
        context->removeEvent(sp_EVC_LIFE, getObjectID());
    }
    ct_Subject::removeNotify();
    resetState();
}

#ifdef __TRACE_NW__
void Spark::draw(CDC &gc)
{
    CBrush brush(RGB(255, 255, 0));
    CBrush *oldBrush = gc.SelectObject(&brush);
    const double x = m_position.x;
    const double y = -m_position.z;
    const double radius = m_attr->m_maxRadius;
    gc.Ellipse(static_cast<int>(x - radius), static_cast<int>(y - radius),
               static_cast<int>(x + radius), static_cast<int>(y + radius));
    gc.SelectObject(oldBrush);
}
#else
void Spark::draw(CDC &) {}
#endif

CFVector3 Spark::realPosition()
{
    return m_position;
}

void SparkTable::allocObjects(int objectQnty)
{
    m_table = objectQnty <= 0
        ? NULL
        : new (std::nothrow) Spark[objectQnty];
    if (m_table == NULL)
        m_maxObjectQnty = 0;
}

void SparkTable::freeObjects()
{
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
}

ct_Object *SparkTable::getObjectPTR(int index)
{
    s_ASSERT(index >= 0 && index < m_maxObjectQnty,
             "SparkTable::getObjectPTR");
    return &(m_table[index]);
}

int SparkTable::liveCount() const
{
    int count = 0;
    for (ct_Subject *object = findFirstSubject(); object != NULL;
         object = findNextSubject(object))
        ++count;
    return count;
}

Spark *SparkTable::find(const KR_ObjectID &object) const
{
    for (ct_Subject *subject = findFirstSubject(); subject != NULL;
         subject = findNextSubject(subject))
        if (subject->getObjectID() == object)
            return static_cast<Spark *>(subject);
    return NULL;
}

void SparkSubjectState_Link()
{
}

bool SparkSubjectState_TableReady(SimulationContext *context,
                                  int expectedCapacity)
{
    if (context == NULL || expectedCapacity <= 0 ||
        g_arena.getContext() != context)
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Spark");
    return table != ct_NULLID &&
           table == __classTable.getClassTableID() &&
           __classTable.capacity() == expectedCapacity &&
           __classTable.liveCount() == 0 &&
           __classTable.isRendering() && !__classTable.isAudible();
}

int SparkSubjectState_Capacity()
{
    return __classTable.capacity();
}

int SparkSubjectState_LiveCount()
{
    return __classTable.liveCount();
}

unsigned long long SparkSubjectState_Fingerprint(SimulationContext *context)
{
    const int capacity = __classTable.capacity();
    if (!SparkSubjectState_TableReady(context, capacity) ||
        !SparkAttributeState_VisualResourcesResolved(context))
        return 0;
    unsigned long long hash = kHashOffset;
    HashString(hash, "Spark");
    HashBytes(hash, &capacity, sizeof(capacity));
    const int rendering = 1;
    const int audible = 0;
    const int boundedLifecycle = 1;
    const int selfOwnedEvents = 1;
    const int may1999PhaseTiming = 1;
    const int spriteAndLight = 1;
    const int duplicateSymbolicNames = 1;
    HashBytes(hash, &rendering, sizeof(rendering));
    HashBytes(hash, &audible, sizeof(audible));
    HashBytes(hash, &boundedLifecycle, sizeof(boundedLifecycle));
    HashBytes(hash, &selfOwnedEvents, sizeof(selfOwnedEvents));
    HashBytes(hash, &may1999PhaseTiming, sizeof(may1999PhaseTiming));
    HashBytes(hash, &spriteAndLight, sizeof(spriteAndLight));
    HashBytes(hash, &duplicateSymbolicNames,
              sizeof(duplicateSymbolicNames));
    return hash;
}

bool SparkSubjectState_QueueCreate(
    SimulationContext *context, const SparkCreateRequest &request,
    KR_ObjectID *child)
{
    if (child == NULL)
        return false;
    *child = KR_ObjectID::NUL();
    AttributeSpark *attribute = NULL;
    if (!ResolveRequest(context, request, &attribute) ||
        __classTable.liveCount() >= __classTable.capacity())
        return false;
    *child = g_arena.newObject(request.subjectTable, request.objectName);
    if (child->isNUL() || __classTable.find(*child) == NULL)
    {
        RemoveIfPresent(context, *child);
        *child = KR_ObjectID::NUL();
        return false;
    }
    KR_Event event;
    BuildCreateEvent(event, *child, request);
    context->addEvent(event);
    return true;
}

bool SparkSubjectState_RollbackQueued(
    SimulationContext *context, const KR_ObjectID &child)
{
    if (context == NULL || IsNul(child) || !context->isExist(child))
        return false;
    const bool eventRemoved =
        context->removeEvent(sp_EV_CREATE, child) != 0;
    context->removeObject(child);
    return eventRemoved && !context->isExist(child) &&
           context->removeEvent(sp_EVC_LIFE, child) == 0;
}

bool SparkSubjectState_ExecuteNow(
    SimulationContext *context, const SparkCreateRequest &request,
    KR_ObjectID *child)
{
    if (child == NULL)
        return false;
    *child = KR_ObjectID::NUL();
    AttributeSpark *attribute = NULL;
    if (!ResolveRequest(context, request, &attribute))
        return false;
    *child = g_arena.newObject(request.subjectTable, request.objectName);
    Spark *object = __classTable.find(*child);
    if (child->isNUL() || object == NULL)
    {
        RemoveIfPresent(context, *child);
        *child = KR_ObjectID::NUL();
        return false;
    }
    KR_Event event;
    BuildCreateEvent(event, *child, request);
    if (object->receiveEvent(event) != 1 || !context->isExist(*child) ||
        !object->m_started || object->m_attr != attribute)
    {
        RemoveIfPresent(context, *child);
        *child = KR_ObjectID::NUL();
        return false;
    }
    return true;
}

bool SparkSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    SparkLifecycleProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const int baseline = __classTable.liveCount();
    const int capacity = __classTable.capacity();
    if (context == NULL || baseline != 0 || capacity <= 0 ||
        !SparkSubjectState_TableReady(context, capacity) ||
        !SparkAttributeState_VisualResourcesResolved(context))
        return false;
    KR_ObjectID attributeID = context->searchObject("Spark.Flash");
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("SparkAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Spark");
    const int attributeIndex = attributeTable == ct_NULLID
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    AttributeSpark *attribute = attributeID.isNUL()
        ? NULL
        : static_cast<AttributeSpark *>(
              __attrSparkTable.searchAttribute(attributeID));
    if (subjectTable == ct_NULLID || attributeIndex == -1 ||
        !AttributeReady(attribute))
        return false;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;

    KR_ObjectID invalid = g_arena.newObject(
        subjectTable, "Spark.InvalidPayload.Probe");
    Spark *invalidObject = __classTable.find(invalid);
    KR_Event event;
    event.label = sp_EV_CREATE;
    event.source = invalid;
    event.destination = invalid;
    event.timeStamp = ts;
    event.data.open(EDO_WRITE).putInt(attributeIndex).close();
    const bool invalidPayload = !invalid.isNUL() && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 && invalidObject->clean();
    summary->invalidStarts += invalidPayload ? 1 : 0;
    RemoveIfPresent(context, invalid);

    invalid = g_arena.newObject(
        subjectTable, "Spark.InvalidAttribute.Probe");
    invalidObject = __classTable.find(invalid);
    SparkCreateRequest invalidRequest = {
        CFVector3(1.0, 2.0, 3.0), ts, subjectTable,
        attributeIndex ^ 0x40000000, "Spark.InvalidAttribute.Probe"};
    BuildCreateEvent(event, invalid, invalidRequest);
    const bool invalidAttribute = !invalid.isNUL() && invalidObject != NULL &&
        invalidObject->receiveEvent(event) == 0 && invalidObject->clean();
    summary->invalidStarts += invalidAttribute ? 1 : 0;
    RemoveIfPresent(context, invalid);
    if (!invalidPayload || !invalidAttribute ||
        __classTable.liveCount() != baseline)
        return false;

    SparkCreateRequest queuedRequest = {
        CFVector3(4.0, 5.0, 6.0), ts + 1.0, subjectTable,
        attributeIndex, "Spark.QueueRollback.Probe"};
    KR_ObjectID queued = KR_ObjectID::NUL();
    const bool queuedCreated =
        SparkSubjectState_QueueCreate(context, queuedRequest, &queued);
    summary->queuedCreates = queuedCreated ? 1 : 0;
    const bool queueRolledBack = queuedCreated &&
        SparkSubjectState_RollbackQueued(context, queued);
    summary->queueRollbacks = queueRolledBack ? 1 : 0;
    if (!queuedCreated || !queueRolledBack ||
        __classTable.liveCount() != baseline)
        return false;

    SparkCreateRequest concurrentRequest = {
        CFVector3(-4.0, 5.0, 6.0), ts + 1.25, subjectTable,
        attributeIndex, "Spark.DuplicateName.Probe"};
    KR_ObjectID concurrentFirst = KR_ObjectID::NUL();
    KR_ObjectID concurrentSecond = KR_ObjectID::NUL();
    const bool firstConcurrent = SparkSubjectState_QueueCreate(
        context, concurrentRequest, &concurrentFirst);
    concurrentRequest.timeStamp = ts + 1.5;
    const bool secondConcurrent = firstConcurrent &&
        SparkSubjectState_QueueCreate(
            context, concurrentRequest, &concurrentSecond);
    const bool distinctConcurrent = secondConcurrent &&
        concurrentFirst != concurrentSecond &&
        __classTable.liveCount() == baseline + 2;
    const bool secondConcurrentRolledBack = secondConcurrent &&
        SparkSubjectState_RollbackQueued(context, concurrentSecond);
    const bool firstConcurrentRolledBack = firstConcurrent &&
        SparkSubjectState_RollbackQueued(context, concurrentFirst);
    RemoveIfPresent(context, concurrentSecond);
    RemoveIfPresent(context, concurrentFirst);
    if (!distinctConcurrent || !secondConcurrentRolledBack ||
        !firstConcurrentRolledBack ||
        __classTable.liveCount() != baseline ||
        context->isExist("Spark.DuplicateName.Probe"))
        return false;

    SparkCreateRequest lifecycleRequest = {
        CFVector3(7.0, 8.0, -9.0), ts + 2.0, subjectTable,
        attributeIndex, "Spark.Lifecycle.Probe"};
    KR_ObjectID lifecycle = KR_ObjectID::NUL();
    const bool started = SparkSubjectState_ExecuteNow(
        context, lifecycleRequest, &lifecycle);
    Spark *object = __classTable.find(lifecycle);
    bool lifecycleValid = started && object != NULL &&
        object->m_curPhase == 0 &&
        NearlyEqual(object->m_nextLifeTime,
                    lifecycleRequest.timeStamp + attribute->m_phase[0].time);
    int phase = 0;
    while (lifecycleValid && context->isExist(lifecycle))
    {
        const double expectedTime = object->m_nextLifeTime;
        if (context->removeEvent(sp_EVC_LIFE, lifecycle) == 0)
        {
            lifecycleValid = false;
            break;
        }
        event = KR_Event();
        event.label = sp_EVC_LIFE;
        event.source = lifecycle;
        event.destination = lifecycle;
        event.timeStamp = expectedTime;
        if (object->receiveEvent(event) != 1)
        {
            lifecycleValid = false;
            break;
        }
        if (phase >= attribute->m_phaseCnt - 1)
        {
            lifecycleValid = !context->isExist(lifecycle);
            summary->expirations = lifecycleValid ? 1 : 0;
            break;
        }
        ++phase;
        const double wantedNext =
            expectedTime + attribute->m_phase[phase - 1].time;
        lifecycleValid = context->isExist(lifecycle) &&
            object->m_curPhase == phase &&
            NearlyEqual(object->m_nextLifeTime, wantedNext);
        if (lifecycleValid)
            ++summary->phaseTransitions;
    }
    const bool reusedClean = object != NULL && object->clean();
    RemoveIfPresent(context, lifecycle);
    return lifecycleValid && reusedClean &&
           summary->invalidStarts == 2 &&
           summary->queuedCreates == 1 &&
           summary->queueRollbacks == 1 &&
           summary->phaseTransitions == attribute->m_phaseCnt - 1 &&
           summary->expirations == 1 &&
           __classTable.liveCount() == baseline &&
           !context->isExist("Spark.InvalidPayload.Probe") &&
           !context->isExist("Spark.InvalidAttribute.Probe") &&
           !context->isExist("Spark.QueueRollback.Probe") &&
           !context->isExist("Spark.DuplicateName.Probe") &&
           !context->isExist("Spark.Lifecycle.Probe");
}

void Spark::render(CViewDynamicList &list, double)
{
    if (!m_started || !AttributeReady(m_attr) ||
        m_curPhase < 0 || m_curPhase >= m_attr->m_phaseCnt)
        return;
    const SparkPhase &current = m_attr->m_phase[m_curPhase];
    m_viewDynSpr.prepareToRender(
        m_position, m_attr->m_maxRadius,
        current.u0, current.v0, current.u1, current.v1,
        m_attr->m_cacheSkin);
    list.Load(&m_viewDynSpr);
    g_lightChain.add(m_position, current.color,
                     current.brightness, current.radius);
}

void Spark::endRender(CViewScene *scene)
{
    if (scene != NULL && m_started)
        scene->RemoveLandDynamic(&m_viewDynSpr);
}

/* End of file D:\GAME\OBASE\Spark\Spark.cpp */
