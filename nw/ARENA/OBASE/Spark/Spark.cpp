/*
 * File  : D:\GAME\OBASE\Spark\Spark.cpp
 * Autor :
 * Ver   1.0
 */
#include "Spark.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "kernel/h/context.h"
#include "kernel/h/echo.h"
#include "kernel/h/s_debug.h"
#include "message/sparkmsg.h"
#include "enum/spaceenum.h"
#include "scene.h"
#include "h/light.h"
#include "SparkActiveWorldState.h"
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
const std::uint32_t kSparkActiveWorldMagic = 0x314b5053u; // SPK1
const std::uint32_t kSparkActiveWorldVersion = 1u;
const std::size_t kMaximumActiveWorldSparks = 4096;
const std::size_t kMaximumActiveWorldString = MAX_SYMBOLIC_LENGHT - 1;

std::string g_activeWorldFailure;

struct StableSparkRecord
{
    std::string name;
    std::string attribute;
    CFVector3 position;
    int currentPhase;
    double nextLifeTime;
    double lifeEventTime;

    StableSparkRecord()
        : position(0.0, 0.0, 0.0), currentPhase(0),
          nextLifeTime(0.0), lifeEventTime(0.0) {}
};

bool FailActiveWorld(const std::string &message)
{
    g_activeWorldFailure = message;
    return false;
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

std::string ObjectName(SimulationContext *context,
                       const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object))
        return std::string();
    const char *name = context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

bool CollectStableRoster(SimulationContext *context,
                         std::vector<Spark *> *objects)
{
    if (context == NULL || objects == NULL ||
        g_arena.getContext() != context)
        return false;
    objects->clear();
    for (ct_Subject *subject = __classTable.findFirstSubject();
         subject != NULL; subject = __classTable.findNextSubject(subject))
        objects->push_back(static_cast<Spark *>(subject));
    std::sort(objects->begin(), objects->end(),
              [context](const Spark *left, const Spark *right)
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

bool ValidateStableRecord(const StableSparkRecord &record)
{
    if (record.name.empty() || record.attribute.empty() ||
        record.name.size() > kMaximumActiveWorldString ||
        record.attribute.size() > kMaximumActiveWorldString)
        return FailActiveWorld("SPK1 owner/attribute identity is invalid");
    if (!FiniteVector(record.position) ||
        record.currentPhase < 0 ||
        record.currentPhase >= AttributeSpark::MAX_PHASE ||
        !std::isfinite(record.nextLifeTime) ||
        record.nextLifeTime < 0.1 ||
        !std::isfinite(record.lifeEventTime) ||
        record.lifeEventTime != record.nextLifeTime)
        return FailActiveWorld("SPK1 phase or LIFE time is invalid");
    return true;
}

bool CaptureStableRecord(SimulationContext *context, Spark *object,
                         StableSparkRecord *record)
{
    if (context == NULL || object == NULL || record == NULL ||
        !object->m_started || object->m_dynamicPublished ||
        !AttributeReady(object->m_attr))
        return FailActiveWorld(
            "live Spark is not at a stable started frame boundary");
    record->name = ObjectName(context, object->getObjectID());
    record->attribute = ObjectName(
        context, object->m_attr->getObjectID());
    record->position = object->m_position;
    record->currentPhase = object->m_curPhase;
    record->nextLifeTime = object->m_nextLifeTime;
    KR_Event life[2];
    KR_Event create[1];
    const int lifeCount = context->copyEvents(
        sp_EVC_LIFE, object->getObjectID(), life, 2);
    const int createCount = context->copyEvents(
        sp_EV_CREATE, object->getObjectID(), create, 1);
    if (lifeCount != 1 || createCount != 0 ||
        life[0].source != object->getObjectID() ||
        life[0].destination != object->getObjectID() ||
        life[0].data.size() != 0)
        return FailActiveWorld(
            "live Spark private event boundary is invalid");
    record->lifeEventTime = life[0].timeStamp;
    if (record->currentPhase >= object->m_attr->m_phaseCnt)
        return FailActiveWorld("live Spark phase exceeds its attribute");
    return ValidateStableRecord(*record);
}

bool CollectStableRecords(SimulationContext *context,
                          std::vector<StableSparkRecord> *records)
{
    std::vector<Spark *> objects;
    if (records == NULL || !CollectStableRoster(context, &objects))
        return false;
    records->clear();
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        StableSparkRecord record;
        if (!CaptureStableRecord(context, objects[index], &record))
            return false;
        records->push_back(record);
    }
    return true;
}

bool RosterMatches(const std::vector<Spark *> &objects,
                   SimulationContext *context,
                   const std::vector<StableSparkRecord> &records)
{
    if (objects.size() != records.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (ObjectName(context, objects[index]->getObjectID()) !=
            records[index].name)
            return false;
    return true;
}

void PutU32(std::vector<unsigned char> *bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes->push_back(static_cast<unsigned char>(value >> shift));
}

void PutDouble(std::vector<unsigned char> *bytes, double value)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 0; shift < 64; shift += 8)
        bytes->push_back(static_cast<unsigned char>(bits >> shift));
}

bool PutString(std::vector<unsigned char> *bytes,
               const std::string &value)
{
    if (bytes == NULL || value.empty() ||
        value.size() > kMaximumActiveWorldString ||
        value.find('\0') != std::string::npos)
        return false;
    PutU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
}

bool GetU32(const std::vector<unsigned char> &bytes,
            std::size_t *offset, std::uint32_t *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 4)
        return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        *value |= static_cast<std::uint32_t>(
            bytes[(*offset)++]) << shift;
    return true;
}

bool GetDouble(const std::vector<unsigned char> &bytes,
               std::size_t *offset, double *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 8)
        return false;
    std::uint64_t bits = 0;
    for (int shift = 0; shift < 64; shift += 8)
        bits |= static_cast<std::uint64_t>(
            bytes[(*offset)++]) << shift;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
}

bool GetString(const std::vector<unsigned char> &bytes,
               std::size_t *offset, std::string *value)
{
    std::uint32_t size = 0;
    if (offset == NULL || value == NULL ||
        !GetU32(bytes, offset, &size) || size == 0 ||
        size > kMaximumActiveWorldString || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    value->assign(reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return value->find('\0') == std::string::npos;
}

bool EncodeStableRecords(const std::vector<StableSparkRecord> &records,
                         std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumActiveWorldSparks)
        return false;
    bytes->clear();
    PutU32(bytes, kSparkActiveWorldMagic);
    PutU32(bytes, kSparkActiveWorldVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const StableSparkRecord &record = records[index];
        if (!ValidateStableRecord(record) ||
            (index != 0 && records[index - 1].name > record.name) ||
            !PutString(bytes, record.name) ||
            !PutString(bytes, record.attribute))
            return FailActiveWorld("SPK1 record encoding failed");
        PutDouble(bytes, record.position.x);
        PutDouble(bytes, record.position.y);
        PutDouble(bytes, record.position.z);
        PutU32(bytes, static_cast<std::uint32_t>(record.currentPhase));
        PutDouble(bytes, record.nextLifeTime);
        PutDouble(bytes, record.lifeEventTime);
    }
    return true;
}

bool DecodeStableRecords(const std::vector<unsigned char> &bytes,
                         std::vector<StableSparkRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) ||
        magic != kSparkActiveWorldMagic ||
        version != kSparkActiveWorldVersion ||
        count > kMaximumActiveWorldSparks)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableSparkRecord record;
        std::uint32_t phase = 0;
        if (!GetString(bytes, &offset, &record.name) ||
            !GetString(bytes, &offset, &record.attribute) ||
            !GetDouble(bytes, &offset, &record.position.x) ||
            !GetDouble(bytes, &offset, &record.position.y) ||
            !GetDouble(bytes, &offset, &record.position.z) ||
            !GetU32(bytes, &offset, &phase) ||
            !GetDouble(bytes, &offset, &record.nextLifeTime) ||
            !GetDouble(bytes, &offset, &record.lifeEventTime))
            return false;
        record.currentPhase = static_cast<int>(phase);
        if (!ValidateStableRecord(record) ||
            (!records->empty() && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

int DrainPrivateSparkEvents(SimulationContext *context,
                            const KR_ObjectID &object)
{
    if (context == NULL || IsNul(object))
        return 0;
    int removed = 0;
    while (context->removeEvent(sp_EV_CREATE, object) == 1)
        ++removed;
    while (context->removeEvent(sp_EVC_LIFE, object) == 1)
        ++removed;
    return removed;
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
    m_dynamicPublished = false;
}

bool Spark::clean() const
{
    return !m_started && !m_dynamicPublished &&
           m_attr == &__defaultSparkAttr &&
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
    if (m_dynamicPublished)
    {
        CViewScene *scene = CViewScene::Current();
        if (scene != NULL)
            scene->RemoveLandDynamic(&m_viewDynSpr);
        m_dynamicPublished = false;
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
    m_dynamicPublished = true;
    g_lightChain.add(m_position, current.color,
                     current.brightness, current.radius);
}

void Spark::endRender(CViewScene *scene)
{
    if (scene != NULL && m_dynamicPublished)
        scene->RemoveLandDynamic(&m_viewDynSpr);
    m_dynamicPublished = false;
}

void SparkActiveWorldState_Link()
{
    SparkSubjectState_Link();
}

const char *SparkActiveWorldState_LastFailure()
{
    return g_activeWorldFailure.c_str();
}

int SparkActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableSparkRecord> records;
    return DecodeStableRecords(bytes, &records)
        ? static_cast<int>(records.size()) : -1;
}

unsigned long long SparkActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!SparkActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = kHashOffset;
    if (!bytes.empty())
        HashBytes(hash, &bytes[0], static_cast<int>(bytes.size()));
    return hash;
}

bool SparkActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_activeWorldFailure.clear();
    std::vector<StableSparkRecord> records;
    if (!CollectStableRecords(context, &records))
    {
        if (g_activeWorldFailure.empty())
            FailActiveWorld("Spark stable roster collection failed");
        return false;
    }
    return EncodeStableRecords(records, bytes);
}

bool SparkActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableSparkRecord> records;
    return DecodeStableRecords(bytes, &records);
}

bool SparkActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return SparkActiveWorldState_ValidateStable(bytes) &&
           SparkActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool SparkActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableSparkRecord> records;
    std::vector<Spark *> objects;
    if (owners == NULL || !owners->empty() ||
        !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &objects) ||
        !RosterMatches(objects, context, records))
        return false;
    for (std::size_t index = 0; index < objects.size(); ++index)
        owners->push_back(objects[index]->getObjectID());
    return true;
}

bool SparkActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableSparkRecord> records;
    std::vector<Spark *> objects;
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &objects))
        return false;
    if (!objects.empty())
        return RosterMatches(objects, context, records);
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Spark");
    if ((!records.empty() && table == ct_NULLID) ||
        static_cast<int>(records.size()) >
            __classTable.capacity() - __classTable.liveCount())
        return FailActiveWorld("Spark owner table has insufficient capacity");
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        KR_ObjectID object =
            g_arena.newObject(table, records[index].name.c_str());
        if (object.isNUL() || __classTable.find(object) == NULL)
        {
            SparkActiveWorldState_RemoveStableOwners(context, created);
            return FailActiveWorld("Spark owner allocation failed");
        }
        created->push_back(object);
    }
    objects.clear();
    if (!CollectStableRoster(context, &objects) ||
        !RosterMatches(objects, context, records))
    {
        SparkActiveWorldState_RemoveStableOwners(context, created);
        return FailActiveWorld("Spark allocated roster is not canonical");
    }
    return true;
}

bool SparkActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableSparkRecord> records;
    std::vector<Spark *> objects;
    if (context == NULL || !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &objects) ||
        !RosterMatches(objects, context, records))
        return false;
    std::vector<AttributeSpark *> attributes(records.size(), NULL);
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const KR_ObjectID attributeID =
            context->searchObject(records[index].attribute.c_str());
        attributes[index] = IsNul(attributeID) ? NULL :
            static_cast<AttributeSpark *>(
                __attrSparkTable.searchAttribute(attributeID));
        if (!AttributeReady(attributes[index]))
            return FailActiveWorld(
                "SPK1 SparkAttr/visual dependency is unresolved");
        if (records[index].currentPhase >=
            attributes[index]->m_phaseCnt)
            return FailActiveWorld(
                "SPK1 phase exceeds the resolved SparkAttr");
    }
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        if (objects[index]->m_dynamicPublished)
            return FailActiveWorld(
                "SPK1 cannot replace a frame-published Spark");
        DrainPrivateSparkEvents(context, objects[index]->getObjectID());
        objects[index]->resetState();
    }
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        Spark *object = objects[index];
        const StableSparkRecord &record = records[index];
        object->m_attr = attributes[index];
        object->m_position = record.position;
        object->m_curPhase = record.currentPhase;
        object->m_nextLifeTime = record.nextLifeTime;
        object->m_started = true;
        object->setPosition(record.position);
        KR_Event life;
        life.label = sp_EVC_LIFE;
        life.source = object->getObjectID();
        life.destination = object->getObjectID();
        life.timeStamp = record.lifeEventTime;
        context->addEvent(life);
    }
    std::vector<unsigned char> current;
    if (!SparkActiveWorldState_CaptureStable(context, &current) ||
        current != bytes)
        return FailActiveWorld("SPK1 canonical recapture differs");
    return true;
}

void SparkActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            DrainPrivateSparkEvents(context, *object);
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    created->clear();
}

bool SparkActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, double timeStamp,
    SparkActiveWorldProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    g_activeWorldFailure.clear();
    if (context == NULL || __classTable.liveCount() != 0 ||
        !SparkAttributeState_VisualResourcesResolved(context))
        return FailActiveWorld(
            "Spark active-world probe requires an empty ready table");
    const KR_ObjectID attributeID = context->searchObject("Spark.Flash");
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("SparkAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Spark");
    const int attributeIndex = attributeTable == ct_NULLID ||
            IsNul(attributeID)
        ? -1 : g_arena.getAttributeIndex(attributeTable, attributeID);
    AttributeSpark *attribute = IsNul(attributeID) ? NULL :
        static_cast<AttributeSpark *>(
            __attrSparkTable.searchAttribute(attributeID));
    if (subjectTable == ct_NULLID || attributeIndex < 0 ||
        !AttributeReady(attribute) || attribute->m_phaseCnt < 3)
        return FailActiveWorld(
            "Spark active-world probe dependency is unavailable");

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    SparkCreateRequest request = {
        CFVector3(4096.0, 10000.0, -4096.0), ts,
        subjectTable, attributeIndex, "Spark.ActiveWorld.Probe"};
    KR_ObjectID originalFirst = KR_ObjectID::NUL();
    KR_ObjectID originalSecond = KR_ObjectID::NUL();
    KR_ObjectID stagedFirst = KR_ObjectID::NUL();
    KR_ObjectID stagedSecond = KR_ObjectID::NUL();
    KR_ObjectID restoredFirst = KR_ObjectID::NUL();
    KR_ObjectID restoredSecond = KR_ObjectID::NUL();
    std::vector<KR_ObjectID> originalOwners;
    std::vector<KR_ObjectID> staged;
    std::vector<KR_ObjectID> restored;
    bool success = false;
    do
    {
        if (!SparkSubjectState_ExecuteNow(
                context, request, &originalFirst))
        {
            FailActiveWorld("Spark active-world probe start failed");
            break;
        }
        request.position = CFVector3(4104.0, 10008.0, -4104.0);
        request.timeStamp = ts + 0.01;
        if (!SparkSubjectState_ExecuteNow(
                context, request, &originalSecond) ||
            originalSecond == originalFirst)
        {
            FailActiveWorld(
                "Spark active-world duplicate-name start failed");
            break;
        }
        const auto advancePhase =
            [context](const KR_ObjectID &owner, int expectedPhase)
            {
                Spark *object = __classTable.find(owner);
                KR_Event life[2];
                if (object == NULL || context->copyEvents(
                        sp_EVC_LIFE, owner, life, 2) != 1 ||
                    context->removeEvent(sp_EVC_LIFE, owner) != 1)
                    return false;
                context->sendEventNow(life[0]);
                return context->isExist(owner) &&
                       object->m_curPhase == expectedPhase;
            };
        if (!advancePhase(originalFirst, 1) ||
            !advancePhase(originalSecond, 1) ||
            !advancePhase(originalSecond, 2))
        {
            FailActiveWorld("Spark active-world phase setup failed");
            break;
        }

        std::vector<unsigned char> bytes;
        if (!SparkActiveWorldState_CaptureStable(context, &bytes) ||
            SparkActiveWorldState_SchedulerEventCount(bytes) != 2 ||
            !SparkActiveWorldState_CollectStableOwners(
                context, bytes, &originalOwners) ||
            originalOwners.size() != 2 ||
            originalOwners[0] != originalFirst ||
            originalOwners[1] != originalSecond)
            break;
        unsigned long long fingerprint = kHashOffset;
        HashBytes(fingerprint, &bytes[0], static_cast<int>(bytes.size()));
        SparkActiveWorldState_RemoveStableOwners(
            context, &originalOwners);
        if (__classTable.liveCount() != 0)
        {
            FailActiveWorld("SPK1 original teardown failed");
            break;
        }

        if (!SparkActiveWorldState_CreateStableOwners(
                context, bytes, &staged) || staged.size() != 2 ||
            staged[0] == originalFirst || staged[0] == originalSecond ||
            staged[1] == originalFirst || staged[1] == originalSecond ||
            !SparkActiveWorldState_ApplyStableReferences(context, bytes) ||
            !SparkActiveWorldState_MatchesStable(context, bytes))
        {
            FailActiveWorld("SPK1 staged reconstruction failed");
            break;
        }
        stagedFirst = staged[0];
        stagedSecond = staged[1];
        SparkActiveWorldState_RemoveStableOwners(context, &staged);
        if (__classTable.liveCount() != 0)
        {
            FailActiveWorld("SPK1 staged rollback retained state");
            break;
        }

        if (!SparkActiveWorldState_CreateStableOwners(
                context, bytes, &restored) || restored.size() != 2 ||
            restored[0] == originalFirst ||
            restored[0] == originalSecond ||
            restored[0] == stagedFirst || restored[0] == stagedSecond ||
            restored[1] == originalFirst ||
            restored[1] == originalSecond ||
            restored[1] == stagedFirst || restored[1] == stagedSecond ||
            !SparkActiveWorldState_ApplyStableReferences(context, bytes) ||
            !SparkActiveWorldState_MatchesStable(context, bytes))
        {
            FailActiveWorld("SPK1 final reconstruction failed");
            break;
        }
        restoredFirst = restored[0];
        restoredSecond = restored[1];
        Spark *resumed = __classTable.find(restoredFirst);
        Spark *unmoved = __classTable.find(restoredSecond);
        KR_Event pending[2];
        if (resumed == NULL || unmoved == NULL ||
            resumed->m_curPhase != 1 || unmoved->m_curPhase != 2 ||
            context->copyEvents(
                sp_EVC_LIFE, restoredFirst, pending, 2) != 1 ||
            context->removeEvent(sp_EVC_LIFE, restoredFirst) != 1)
        {
            FailActiveWorld("SPK1 restored LIFE is unavailable");
            break;
        }
        context->sendEventNow(pending[0]);
        KR_Event next[2];
        if (!context->isExist(restoredFirst) ||
            !context->isExist(restoredSecond) ||
            resumed->m_curPhase != 2 || unmoved->m_curPhase != 2 ||
            context->copyEvents(
                sp_EVC_LIFE, restoredFirst, next, 2) != 1 ||
            !NearlyEqual(next[0].timeStamp, resumed->m_nextLifeTime))
        {
            FailActiveWorld("SPK1 restored Spark did not resume its phase");
            break;
        }
        summary->capturedOwners = 2;
        summary->schedulerEvents = 2;
        summary->stagedRollbacks = 1;
        summary->reconstructedOwners = 2;
        summary->stableRoundTrips = 2;
        summary->resumedPhases = 1;
        summary->fingerprint = fingerprint;
        success = true;
    } while (false);

    SparkActiveWorldState_RemoveStableOwners(context, &restored);
    SparkActiveWorldState_RemoveStableOwners(context, &staged);
    SparkActiveWorldState_RemoveStableOwners(context, &originalOwners);
    RemoveIfPresent(context, originalSecond);
    RemoveIfPresent(context, originalFirst);
    const int lateEvents =
        DrainPrivateSparkEvents(context, originalFirst) +
        DrainPrivateSparkEvents(context, originalSecond) +
        DrainPrivateSparkEvents(context, stagedFirst) +
        DrainPrivateSparkEvents(context, stagedSecond) +
        DrainPrivateSparkEvents(context, restoredFirst) +
        DrainPrivateSparkEvents(context, restoredSecond);
    const bool clean = __classTable.liveCount() == 0 && lateEvents == 0;
    if (!success || !clean)
    {
        std::memset(summary, 0, sizeof(*summary));
        if (g_activeWorldFailure.empty())
            FailActiveWorld("SPK1 probe rollback was not clean");
        return false;
    }
    return true;
}

/* End of file D:\GAME\OBASE\Spark\Spark.cpp */
