#include "ActiveWorldSemanticEvents.h"

#include "MissionActiveWorldState.h"

#include "kernel/h/context.h"
#include "message/corpsemsg.h"
#include "message/explmsg.h"
#include "message/recrcenmsg.h"
#include "message/SPARKMSG.H"
#include "obase/corpse/CorpseAttributeState.h"
#include "obase/corpse/CorpseSubjectState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/spark/SparkAttributeState.h"
#include "obase/spark/SparkSubjectState.h"
#include "storage/h/subject.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <utility>

namespace {

const std::uint32_t kPayloadMagic = 0x31545645u;  // EVT1
const std::uint32_t kPayloadVersion = 1u;
const std::size_t kMaximumString = MAX_SYMBOLIC_LENGHT - 1;

const char kProbeExplosion[] = "Expl.ActiveWorld.Event.Probe";
const char kProbeSpark[] = "Spark.ActiveWorld.Event.Probe";
const char kProbeCorpse[] = "Corpse.ActiveWorld.Event.Probe";

enum EffectKind
{
    kEffectExplosion = 1,
    kEffectSpark = 2,
    kEffectCorpse = 3,
    kMissionCheck = 4
};

enum ReferenceKind
{
    kReferenceTombstone = 0,
    kReferenceSelf = 1,
    kReferenceSymbolic = 2
};

struct SemanticRecord
{
    EffectKind kind;
    std::uint32_t ordinal;
    ReferenceKind sourceKind;
    std::string attribute;
    CFVector3 position;
    ReferenceKind relationKind;
    std::string relation;
    int missionIndex;

    SemanticRecord()
        : kind(kEffectExplosion), ordinal(0),
          sourceKind(kReferenceTombstone), position(0.0, 0.0, 0.0),
          relationKind(kReferenceTombstone), missionIndex(-1) {}
};

struct Writer
{
    std::vector<std::uint8_t> *bytes;

    void U32(std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            bytes->push_back(static_cast<std::uint8_t>(value >> shift));
    }

    void Double(double value)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        for (int shift = 0; shift < 64; shift += 8)
            bytes->push_back(static_cast<std::uint8_t>(bits >> shift));
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
    const std::vector<std::uint8_t> &bytes;
    std::size_t offset;

    explicit Reader(const std::vector<std::uint8_t> &source)
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

void SetFailure(std::string *failure, const std::string &message)
{
    if (failure != NULL)
        *failure = message;
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

std::string ObjectName(SimulationContext *context,
                       const KR_ObjectID &object)
{
    const char *name = context == NULL || IsNul(object)
        ? NULL : context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
}

bool KindFromLabel(int label, EffectKind *kind)
{
    if (kind == NULL)
        return false;
    if (label == EXPLOSION_START)
        *kind = kEffectExplosion;
    else if (label == sp_EV_CREATE)
        *kind = kEffectSpark;
    else if (label == CORPSE_START_ROTTING)
        *kind = kEffectCorpse;
    else if (label == rc_CHECK_MISSION)
        *kind = kMissionCheck;
    else
        return false;
    return true;
}

int LabelForKind(EffectKind kind)
{
    switch (kind)
    {
    case kEffectExplosion: return EXPLOSION_START;
    case kEffectSpark: return sp_EV_CREATE;
    case kEffectCorpse: return CORPSE_START_ROTTING;
    case kMissionCheck: return rc_CHECK_MISSION;
    }
    return -1;
}

const char *TableForKind(EffectKind kind)
{
    switch (kind)
    {
    case kEffectExplosion: return "Explosion";
    case kEffectSpark: return "Spark";
    case kEffectCorpse: return "Corpse";
    case kMissionCheck: break;
    }
    return "";
}

bool IsPending(SimulationContext *context, EffectKind kind,
               const KR_ObjectID &object)
{
    switch (kind)
    {
    case kEffectExplosion:
        return ExplosionSubjectState_IsPending(context, object);
    case kEffectSpark:
        return SparkSubjectState_IsPending(context, object);
    case kEffectCorpse:
        return CorpseSubjectState_IsPending(context, object);
    case kMissionCheck:
        return false;
    }
    return false;
}

bool CollectPending(SimulationContext *context, EffectKind kind,
                    const std::string &name,
                    std::vector<KR_ObjectID> *objects)
{
    switch (kind)
    {
    case kEffectExplosion:
        return ExplosionSubjectState_CollectPending(
            context, name.c_str(), objects);
    case kEffectSpark:
        return SparkSubjectState_CollectPending(
            context, name.c_str(), objects);
    case kEffectCorpse:
        return CorpseSubjectState_CollectPending(
            context, name.c_str(), objects);
    case kMissionCheck:
        return false;
    }
    return false;
}

bool PendingOrdinal(SimulationContext *context, EffectKind kind,
                    const std::string &name, const KR_ObjectID &object,
                    std::uint32_t *ordinal)
{
    std::vector<KR_ObjectID> objects;
    if (ordinal == NULL || !CollectPending(context, kind, name, &objects))
        return false;
    for (std::size_t index = 0; index < objects.size(); ++index)
        if (objects[index] == object)
        {
            *ordinal = static_cast<std::uint32_t>(index);
            return true;
        }
    return false;
}

bool SymbolicReference(SimulationContext *context,
                       const KR_ObjectID &object,
                       ReferenceKind *kind, std::string *name)
{
    if (context == NULL || kind == NULL || name == NULL)
        return false;
    name->clear();
    if (IsNul(object) || !context->isExist(object))
    {
        *kind = kReferenceTombstone;
        return true;
    }
    *name = ObjectName(context, object);
    if (name->empty() || !context->isExist(name->c_str()) ||
        context->searchObject(name->c_str()) != object)
        return false;
    *kind = kReferenceSymbolic;
    return true;
}

bool AttributeName(SimulationContext *context, EffectKind kind,
                   int encodedIndex, std::string *name)
{
    if (context == NULL || name == NULL)
        return false;
    KR_ObjectID id = KR_ObjectID::NUL();
    if (kind == kEffectExplosion)
    {
        AttributeExplosion *attribute = NULL;
        if (!ExplosionAttributeState_ResolveEncodedIndex(
                context, encodedIndex, &attribute))
            return false;
        id = attribute->getObjectID();
    }
    else if (kind == kEffectSpark)
    {
        AttributeSpark *attribute = NULL;
        if (!SparkAttributeState_ResolveEncodedIndex(
                context, encodedIndex, &attribute))
            return false;
        id = attribute->getObjectID();
    }
    else
    {
        AttributeCorpse *attribute = NULL;
        if (!CorpseAttributeState_ResolveEncodedIndex(
                context, encodedIndex, &attribute))
            return false;
        id = attribute->getObjectID();
    }
    *name = ObjectName(context, id);
    return !name->empty() && context->searchObject(name->c_str()) == id;
}

bool AttributeIndex(SimulationContext *context, EffectKind kind,
                    const std::string &name, int *encodedIndex)
{
    if (context == NULL || encodedIndex == NULL || name.empty() ||
        !context->isExist(name.c_str()))
        return false;
    const KR_ObjectID id = context->searchObject(name.c_str());
    const char *tableName = kind == kEffectExplosion ? "ExplosionAttr" :
        (kind == kEffectSpark ? "SparkAttr" : "CorpseAttr");
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable(tableName);
    if (table == ct_NULLID)
        return false;
    *encodedIndex = g_arena.getAttributeIndex(table, id);
    std::string resolved;
    return *encodedIndex >= 0 &&
           AttributeName(context, kind, *encodedIndex, &resolved) &&
           resolved == name;
}

bool EncodePayload(const SemanticRecord &record,
                   std::vector<std::uint8_t> *payload)
{
    if (payload == NULL)
        return false;
    payload->clear();
    Writer writer = {payload};
    writer.U32(kPayloadMagic);
    writer.U32(static_cast<std::uint32_t>(record.kind));
    writer.U32(record.ordinal);
    writer.U32(static_cast<std::uint32_t>(record.sourceKind));
    if (record.kind == kMissionCheck)
    {
        if (record.ordinal != 0 || record.missionIndex < 0 ||
            record.missionIndex >= 6)
            return false;
        writer.U32(static_cast<std::uint32_t>(record.missionIndex));
        return true;
    }
    if (record.attribute.empty() || !FiniteVector(record.position))
        return false;
    if (!writer.String(record.attribute))
        return false;
    writer.Double(record.position.x);
    writer.Double(record.position.y);
    writer.Double(record.position.z);
    writer.U32(static_cast<std::uint32_t>(record.relationKind));
    return record.relationKind != kReferenceSymbolic ||
           writer.String(record.relation);
}

bool DecodePayload(const SActiveWorldEvent &event, SemanticRecord *record)
{
    if (record == NULL || event.payloadVersion != kPayloadVersion)
        return false;
    Reader reader(event.payload);
    std::uint32_t magic = 0, kind = 0, sourceKind = 0, relationKind = 0;
    if (!reader.U32(&magic) || !reader.U32(&kind) ||
        !reader.U32(&record->ordinal) || !reader.U32(&sourceKind) ||
        magic != kPayloadMagic || kind < kEffectExplosion ||
        kind > kMissionCheck || sourceKind > kReferenceSymbolic)
        return false;
    record->kind = static_cast<EffectKind>(kind);
    record->sourceKind = static_cast<ReferenceKind>(sourceKind);
    if (record->kind == kMissionCheck)
    {
        std::uint32_t missionIndex = 0;
        if (!reader.U32(&missionIndex) ||
            missionIndex >= 6 || record->ordinal != 0)
            return false;
        record->missionIndex = static_cast<int>(missionIndex);
        record->attribute.clear();
        record->position = CFVector3(0.0, 0.0, 0.0);
        record->relationKind = kReferenceTombstone;
        record->relation.clear();
        return reader.offset == event.payload.size() &&
               event.label == rc_CHECK_MISSION &&
               !event.destination.empty() &&
               ((record->sourceKind == kReferenceTombstone &&
                 event.source.empty()) ||
                (record->sourceKind == kReferenceSelf &&
                 event.source == event.destination) ||
                (record->sourceKind == kReferenceSymbolic &&
                 !event.source.empty()));
    }
    if (!reader.String(&record->attribute) ||
        !reader.Double(&record->position.x) ||
        !reader.Double(&record->position.y) ||
        !reader.Double(&record->position.z) ||
        !reader.U32(&relationKind) ||
        relationKind > kReferenceSymbolic)
        return false;
    record->relationKind = static_cast<ReferenceKind>(relationKind);
    record->relation.clear();
    if (record->relationKind == kReferenceSymbolic &&
        !reader.String(&record->relation))
        return false;
    return reader.offset == event.payload.size() &&
           FiniteVector(record->position) &&
           LabelForKind(record->kind) == event.label &&
           !event.destination.empty() &&
           ((record->sourceKind == kReferenceTombstone &&
             event.source.empty()) ||
            (record->sourceKind == kReferenceSelf &&
             event.source == event.destination) ||
            (record->sourceKind == kReferenceSymbolic &&
             !event.source.empty())) &&
           (record->kind == kEffectSpark
                ? record->relationKind == kReferenceTombstone
                : record->relationKind != kReferenceSelf);
}

bool DecodeQueuedEvent(SimulationContext *context, KR_Event event,
                       EffectKind kind, SemanticRecord *record)
{
    if (context == NULL || record == NULL ||
        !std::isfinite(event.timeStamp) || event.timeStamp < 0.1)
        return false;
    const std::string destination = ObjectName(context, event.destination);
    if (destination.empty() || !context->isExist(destination.c_str()) ||
        context->searchObject(destination.c_str()) != event.destination)
        return false;
    if (kind == kMissionCheck)
    {
        int missionIndex = -1;
        s_EventData &data = event.data.open(EDO_READ);
        if (data.remaining() != static_cast<int>(sizeof(int)))
        {
            data.close();
            return false;
        }
        data.getInt(missionIndex).close();
        record->kind = kind;
        record->ordinal = 0;
        record->missionIndex = missionIndex;
        record->sourceKind = event.source == event.destination
            ? kReferenceSelf : kReferenceTombstone;
        std::string source;
        if (event.source != event.destination &&
            !SymbolicReference(context, event.source,
                               &record->sourceKind, &source))
            return false;
        record->attribute.clear();
        record->position = CFVector3(0.0, 0.0, 0.0);
        record->relationKind = kReferenceTombstone;
        record->relation.clear();
        return MissionActiveWorldState_MissionIndexValid(
            context, missionIndex);
    }
    if (!IsPending(context, kind, event.destination) ||
        !PendingOrdinal(context, kind, destination, event.destination,
                        &record->ordinal))
        return false;
    int attributeIndex = -1;
    KR_ObjectID relation = KR_ObjectID::NUL();
    s_EventData &data = event.data.open(EDO_READ);
    if (kind == kEffectExplosion)
    {
        const int expected = static_cast<int>(
            sizeof(int) + sizeof(double) * 3 + sizeof(KR_ObjectID));
        if (data.remaining() != expected)
        {
            data.close();
            return false;
        }
        data.getInt(attributeIndex)
            .getDouble(record->position.x)
            .getDouble(record->position.y)
            .getDouble(record->position.z)
            .getObjectID(relation).close();
    }
    else if (kind == kEffectSpark)
    {
        const int expected = static_cast<int>(sizeof(double) * 3 + sizeof(int));
        if (data.remaining() != expected)
        {
            data.close();
            return false;
        }
        data.getDouble(record->position.x)
            .getDouble(record->position.y)
            .getDouble(record->position.z)
            .getInt(attributeIndex).close();
    }
    else
    {
        const int expected = static_cast<int>(
            sizeof(KR_ObjectID) + sizeof(int) + sizeof(double) * 3);
        if (data.remaining() != expected)
        {
            data.close();
            return false;
        }
        data.getObjectID(relation)
            .getInt(attributeIndex)
            .getDouble(record->position.x)
            .getDouble(record->position.y)
            .getDouble(record->position.z).close();
    }
    record->kind = kind;
    record->sourceKind = event.source == event.destination
        ? kReferenceSelf : kReferenceTombstone;
    std::string source;
    if (event.source != event.destination &&
        !SymbolicReference(context, event.source,
                           &record->sourceKind, &source))
        return false;
    if (!AttributeName(context, kind, attributeIndex,
                       &record->attribute) ||
        !FiniteVector(record->position))
        return false;
    record->relationKind = kReferenceTombstone;
    record->relation.clear();
    return kind == kEffectSpark ||
           SymbolicReference(context, relation,
                             &record->relationKind, &record->relation);
}

bool SameEvent(const SActiveWorldEvent &left,
               const SActiveWorldEvent &right)
{
    return left.sequence == right.sequence && left.tick == right.tick &&
           left.timeStamp == right.timeStamp && left.label == right.label &&
           left.source == right.source &&
           left.destination == right.destination &&
           left.payloadVersion == right.payloadVersion &&
           left.payload == right.payload;
}

bool ResolveReference(SimulationContext *context, ReferenceKind kind,
                      const std::string &name,
                      const KR_ObjectID &self, KR_ObjectID *object)
{
    if (object == NULL)
        return false;
    if (kind == kReferenceTombstone)
    {
        *object = KR_ObjectID::NUL();
        return true;
    }
    if (kind == kReferenceSelf)
    {
        *object = self;
        return true;
    }
    if (name.empty() || !context->isExist(name.c_str()))
        return false;
    *object = context->searchObject(name.c_str());
    return ObjectName(context, *object) == name;
}

bool BuildRuntimeEvent(SimulationContext *context,
                       const SActiveWorldEvent &saved,
                       const SemanticRecord &record,
                       const KR_ObjectID &destination, KR_Event *event)
{
    if (context == NULL || event == NULL)
        return false;
    int attributeIndex = -1;
    KR_ObjectID source = KR_ObjectID::NUL();
    KR_ObjectID relation = KR_ObjectID::NUL();
    if (!ResolveReference(context, record.sourceKind, saved.source,
                          destination, &source))
        return false;
    event->label = saved.label;
    event->source = source;
    event->destination = destination;
    event->timeStamp = saved.timeStamp;
    if (record.kind == kMissionCheck)
    {
        if (!MissionActiveWorldState_MissionIndexValid(
                context, record.missionIndex))
            return false;
        event->data.open(EDO_WRITE).putInt(record.missionIndex).close();
        return true;
    }
    if (!AttributeIndex(context, record.kind, record.attribute,
                        &attributeIndex) ||
        !ResolveReference(context, record.relationKind, record.relation,
                          destination, &relation))
        return false;
    if (record.kind == kEffectExplosion)
        event->data.open(EDO_WRITE)
            .putInt(attributeIndex)
            .putDouble(record.position.x)
            .putDouble(record.position.y)
            .putDouble(record.position.z)
            .putObjectID(relation).close();
    else if (record.kind == kEffectSpark)
        event->data.open(EDO_WRITE)
            .putDouble(record.position.x)
            .putDouble(record.position.y)
            .putDouble(record.position.z)
            .putInt(attributeIndex).close();
    else
        event->data.open(EDO_WRITE)
            .putObjectID(relation)
            .putInt(attributeIndex)
            .putDouble(record.position.x)
            .putDouble(record.position.y)
            .putDouble(record.position.z).close();
    return true;
}

bool RemovePendingObjects(SimulationContext *context,
                          const std::vector<SActiveWorldEvent> &events,
                          const std::vector<SemanticRecord> &records)
{
    std::vector<KR_ObjectID> remove;
    for (std::size_t index = 0; index < events.size(); ++index)
    {
        if (records[index].kind == kMissionCheck)
        {
            if (!context->isExist(events[index].destination.c_str()))
                return false;
            const KR_ObjectID destination =
                context->searchObject(events[index].destination.c_str());
            context->removeEventsTo(events[index].label, destination);
            continue;
        }
        std::vector<KR_ObjectID> objects;
        if (!CollectPending(context, records[index].kind,
                            events[index].destination, &objects) ||
            records[index].ordinal >= objects.size())
            return false;
        const KR_ObjectID destination = objects[records[index].ordinal];
        context->removeEventsTo(events[index].label, destination);
        if (std::find(remove.begin(), remove.end(), destination) == remove.end())
            remove.push_back(destination);
    }
    for (std::vector<KR_ObjectID>::reverse_iterator object = remove.rbegin();
         object != remove.rend(); ++object)
        if (context->isExist(*object))
            context->removeObject(*object);
    return true;
}

void RemoveCreatedObjects(SimulationContext *context,
                          std::vector<KR_ObjectID> *created)
{
    if (context == NULL || created == NULL)
        return;
    for (std::vector<KR_ObjectID>::reverse_iterator object = created->rbegin();
         object != created->rend(); ++object)
        if (context->isExist(*object))
            context->removeObject(*object);
    created->clear();
}

bool ClearProbeKind(SimulationContext *context, EffectKind kind,
                    const char *name)
{
    std::vector<KR_ObjectID> objects;
    if (!CollectPending(context, kind, name, &objects))
        return false;
    for (std::vector<KR_ObjectID>::reverse_iterator object = objects.rbegin();
         object != objects.rend(); ++object)
    {
        context->removeEventsTo(LabelForKind(kind), *object);
        if (context->isExist(*object))
            context->removeObject(*object);
    }
    return true;
}

}  // namespace

bool ActiveWorldSemanticEvents_Capture(
    SimulationContext *context, std::vector<SActiveWorldEvent> *events,
    std::string *failure)
{
    if (context == NULL || events == NULL)
    {
        SetFailure(failure, "EVT1 capture arguments are invalid");
        return false;
    }
    const int count = context->copyAllEvents(NULL, 0);
    if (count < 0)
        return false;
    std::vector<KR_Event> queued(static_cast<std::size_t>(count));
    if (context->copyAllEvents(count == 0 ? NULL : &queued[0], count) != count)
    {
        SetFailure(failure, "EVT1 queue changed during capture");
        return false;
    }
    events->clear();
    for (int index = 0; index < count; ++index)
    {
        EffectKind kind;
        if (!KindFromLabel(queued[index].label, &kind) ||
            (kind != kMissionCheck &&
             !IsPending(context, kind, queued[index].destination)))
            continue;
        SemanticRecord record;
        if (!DecodeQueuedEvent(context, queued[index], kind, &record))
        {
            SetFailure(failure, std::string("EVT1 queued effect payload is ") +
                "invalid: label=" + std::to_string(queued[index].label) +
                " bytes=" + std::to_string(queued[index].data.size()));
            return false;
        }
        SActiveWorldEvent saved = {};
        saved.sequence = static_cast<std::uint32_t>(events->size());
        saved.tick = saved.sequence;
        saved.timeStamp = queued[index].timeStamp;
        saved.label = queued[index].label;
        saved.destination = ObjectName(context, queued[index].destination);
        if (record.sourceKind == kReferenceSelf)
            saved.source = saved.destination;
        else if (record.sourceKind == kReferenceSymbolic)
            saved.source = ObjectName(context, queued[index].source);
        saved.payloadVersion = kPayloadVersion;
        if (!EncodePayload(record, &saved.payload))
        {
            SetFailure(failure, "EVT1 payload encoding failed");
            return false;
        }
        events->push_back(saved);
    }
    return ActiveWorldSemanticEvents_Validate(*events, failure);
}

bool ActiveWorldSemanticEvents_Validate(
    const std::vector<SActiveWorldEvent> &events, std::string *failure)
{
    typedef std::pair<int, std::string> Group;
    std::map<Group, std::set<std::uint32_t> > ordinals;
    for (std::size_t index = 0; index < events.size(); ++index)
    {
        SemanticRecord record;
        if (events[index].sequence != index ||
            events[index].tick != index ||
            !std::isfinite(events[index].timeStamp) ||
            events[index].timeStamp < 0.1 ||
            !DecodePayload(events[index], &record) ||
            (index != 0 && events[index - 1].timeStamp >
                               events[index].timeStamp))
        {
            SetFailure(failure, "EVT1 semantic event is invalid");
            return false;
        }
        if (record.kind != kMissionCheck)
        {
            Group group(static_cast<int>(record.kind),
                        events[index].destination);
            if (!ordinals[group].insert(record.ordinal).second)
            {
                SetFailure(failure,
                           "EVT1 destination identity is duplicated");
                return false;
            }
        }
    }
    for (std::map<Group, std::set<std::uint32_t> >::const_iterator group =
             ordinals.begin(); group != ordinals.end(); ++group)
    {
        std::uint32_t expected = 0;
        for (std::set<std::uint32_t>::const_iterator ordinal =
                 group->second.begin(); ordinal != group->second.end();
             ++ordinal, ++expected)
            if (*ordinal != expected)
            {
                SetFailure(failure,
                           "EVT1 destination ordinals are not contiguous");
                return false;
            }
    }
    return true;
}

bool ActiveWorldSemanticEvents_Replace(
    SimulationContext *context,
    const std::vector<SActiveWorldEvent> &events,
    std::vector<KR_ObjectID> *created, std::string *failure)
{
    if (context == NULL || created == NULL || !created->empty() ||
        !ActiveWorldSemanticEvents_Validate(events, failure))
        return false;

    std::vector<SemanticRecord> records(events.size());
    std::vector<int> attributeIndices(events.size(), -1);
    for (std::size_t index = 0; index < events.size(); ++index)
    {
        if (!DecodePayload(events[index], &records[index]))
        {
            SetFailure(failure, "EVT1 payload dependency is invalid");
            return false;
        }
        if (records[index].kind != kMissionCheck &&
            !AttributeIndex(context, records[index].kind,
                            records[index].attribute,
                            &attributeIndices[index]))
        {
            SetFailure(failure, "EVT1 attribute dependency is unresolved");
            return false;
        }
        KR_ObjectID resolved = KR_ObjectID::NUL();
        if ((records[index].sourceKind == kReferenceSymbolic &&
             !ResolveReference(context, kReferenceSymbolic,
                               events[index].source, KR_ObjectID::NUL(),
                               &resolved)) ||
            (records[index].relationKind == kReferenceSymbolic &&
             !ResolveReference(context, kReferenceSymbolic,
                               records[index].relation,
                               KR_ObjectID::NUL(), &resolved)))
        {
            SetFailure(failure, "EVT1 symbolic dependency is unresolved");
            return false;
        }
        if (records[index].kind == kMissionCheck &&
            (!context->isExist(events[index].destination.c_str()) ||
             ObjectName(context,
                        context->searchObject(
                            events[index].destination.c_str())) !=
                 events[index].destination ||
             !MissionActiveWorldState_MissionIndexValid(
                 context, records[index].missionIndex)))
        {
            SetFailure(failure, "EVT1 mission dependency is unresolved");
            return false;
        }
    }

    std::vector<SActiveWorldEvent> current;
    if (!ActiveWorldSemanticEvents_Capture(context, &current, failure) ||
        context->eventFreeCount() + static_cast<int>(current.size()) <
            static_cast<int>(events.size()))
    {
        SetFailure(failure, "EVT1 event pool has insufficient capacity");
        return false;
    }
    int required[5] = {0, 0, 0, 0, 0};
    int released[5] = {0, 0, 0, 0, 0};
    int requiredPending = 0;
    int releasedPending = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (records[index].kind != kMissionCheck)
        {
            ++required[records[index].kind];
            ++requiredPending;
        }
    for (std::size_t index = 0; index < current.size(); ++index)
    {
        SemanticRecord record;
        if (!DecodePayload(current[index], &record))
            return false;
        if (record.kind != kMissionCheck)
        {
            ++released[record.kind];
            ++releasedPending;
        }
    }
    if (required[kEffectExplosion] >
            ExplosionSubjectState_Capacity() -
                ExplosionSubjectState_LiveCount() +
                released[kEffectExplosion] ||
        required[kEffectSpark] >
            SparkSubjectState_Capacity() - SparkSubjectState_LiveCount() +
                released[kEffectSpark] ||
        required[kEffectCorpse] >
            CorpseSubjectState_Capacity() - CorpseSubjectState_LiveCount() +
                released[kEffectCorpse] ||
        requiredPending > context->objectFreeCount() + releasedPending)
    {
        SetFailure(failure, "EVT1 pending owner pool has insufficient capacity");
        return false;
    }
    if (!ActiveWorldSemanticEvents_Clear(context, failure))
        return false;

    std::vector<KR_ObjectID> destinations(events.size(), KR_ObjectID::NUL());
    for (std::size_t index = 0; index < events.size(); ++index)
    {
        if (records[index].kind == kMissionCheck)
        {
            destinations[index] =
                context->searchObject(events[index].destination.c_str());
            if (ObjectName(context, destinations[index]) !=
                events[index].destination)
                break;
            continue;
        }
        std::vector<KR_ObjectID> objects;
        if (!CollectPending(context, records[index].kind,
                            events[index].destination, &objects))
            break;
        while (objects.size() <= records[index].ordinal)
        {
            const ct_ClassTableID table = g_arena.searchSeanceClassTable(
                TableForKind(records[index].kind));
            KR_ObjectID object = table == ct_NULLID ? KR_ObjectID::NUL() :
                g_arena.newObject(table, events[index].destination.c_str());
            if (IsNul(object) ||
                !IsPending(context, records[index].kind, object))
                break;
            created->push_back(object);
            if (!CollectPending(context, records[index].kind,
                                events[index].destination, &objects))
                break;
        }
        if (objects.size() <= records[index].ordinal)
            break;
        destinations[index] = objects[records[index].ordinal];
    }
    bool complete = true;
    for (std::size_t index = 0; index < destinations.size(); ++index)
        complete = complete && !IsNul(destinations[index]);
    if (!complete)
    {
        RemoveCreatedObjects(context, created);
        SetFailure(failure, "EVT1 pending owner allocation failed");
        return false;
    }

    std::vector<KR_Event> runtime(events.size());
    for (std::size_t index = 0; index < events.size(); ++index)
        if (!BuildRuntimeEvent(context, events[index], records[index],
                               destinations[index], &runtime[index]))
        {
            RemoveCreatedObjects(context, created);
            SetFailure(failure, "EVT1 runtime event reconstruction failed");
            return false;
        }
    // Legacy addEvent prepends equal timestamps. Reverse insertion preserves
    // the exact captured order for equal-time effects and mission checks.
    for (std::vector<KR_Event>::reverse_iterator event = runtime.rbegin();
         event != runtime.rend(); ++event)
        context->addEvent(*event);
    if (!ActiveWorldSemanticEvents_Matches(context, events))
    {
        ActiveWorldSemanticEvents_Clear(context, NULL);
        RemoveCreatedObjects(context, created);
        SetFailure(failure, "EVT1 canonical recapture differs");
        return false;
    }
    return true;
}

bool ActiveWorldSemanticEvents_Matches(
    SimulationContext *context,
    const std::vector<SActiveWorldEvent> &events)
{
    std::vector<SActiveWorldEvent> current;
    if (!ActiveWorldSemanticEvents_Capture(context, &current, NULL) ||
        current.size() != events.size())
        return false;
    for (std::size_t index = 0; index < events.size(); ++index)
        if (!SameEvent(current[index], events[index]))
            return false;
    return true;
}

bool ActiveWorldSemanticEvents_Clear(
    SimulationContext *context, std::string *failure)
{
    std::vector<SActiveWorldEvent> current;
    if (!ActiveWorldSemanticEvents_Capture(context, &current, failure))
        return false;
    std::vector<SemanticRecord> records(current.size());
    for (std::size_t index = 0; index < current.size(); ++index)
        if (!DecodePayload(current[index], &records[index]))
            return false;
    if (!RemovePendingObjects(context, current, records))
    {
        SetFailure(failure, "EVT1 pending owner removal failed");
        return false;
    }
    std::vector<SActiveWorldEvent> remaining;
    if (!ActiveWorldSemanticEvents_Capture(context, &remaining, failure) ||
        !remaining.empty())
    {
        SetFailure(failure, "EVT1 queue did not clear canonically");
        return false;
    }
    return true;
}

bool ActiveWorldSemanticEvents_StageProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    std::string *failure)
{
    if (context == NULL || staged == NULL)
        return false;
    *staged = false;
    const char *explosionAttribute =
        ExplosionSubjectState_SoundProbeAttributeName(context);
    const KR_ObjectID sparkAttribute = context->searchObject("Spark.Flash");
    const KR_ObjectID corpseAttribute =
        context->searchObject("Corpse.Attr.Default");
    if (explosionAttribute == NULL || IsNul(sparkAttribute) ||
        IsNul(corpseAttribute) ||
        !SparkAttributeState_VisualResourcesResolved(context) ||
        !CorpseAttributeState_RuntimeReady(context))
        return true;
    if (context->eventFreeCount() < 3 ||
        !ActiveWorldSemanticEvents_ClearProbe(context, failure))
        return false;

    const ct_ClassTableID explosionTable =
        g_arena.searchSeanceClassTable("Explosion");
    const ct_ClassTableID explosionAttributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID sparkTable =
        g_arena.searchSeanceClassTable("Spark");
    const ct_ClassTableID sparkAttributeTable =
        g_arena.searchSeanceClassTable("SparkAttr");
    const ct_ClassTableID corpseTable =
        g_arena.searchSeanceClassTable("Corpse");
    const ct_ClassTableID corpseAttributeTable =
        g_arena.searchSeanceClassTable("CorpseAttr");
    const KR_ObjectID explosionAttributeID =
        context->searchObject(explosionAttribute);
    const int explosionIndex = explosionAttributeTable == ct_NULLID
        ? -1 : g_arena.getAttributeIndex(explosionAttributeTable,
                                          explosionAttributeID);
    const int sparkIndex = sparkAttributeTable == ct_NULLID
        ? -1 : g_arena.getAttributeIndex(sparkAttributeTable,
                                          sparkAttribute);
    const int corpseIndex = corpseAttributeTable == ct_NULLID
        ? -1 : g_arena.getAttributeIndex(corpseAttributeTable,
                                          corpseAttribute);
    if (explosionTable == ct_NULLID || sparkTable == ct_NULLID ||
        corpseTable == ct_NULLID || explosionIndex < 0 || sparkIndex < 0 ||
        corpseIndex < 0)
        return true;

    const double ts = (std::isfinite(timeStamp) && timeStamp >= 0.1)
        ? timeStamp : 0.1;
    const KR_ObjectID vehicle = context->searchObject("Vehicle.Default");
    std::string stageFailure = "Explosion queue rejected the probe";
    ExplosionImpactRequest explosion = {
        CFVector3(4096.0, 10000.0, -4096.0), ts, vehicle,
        explosionTable, explosionIndex, kProbeExplosion};
    KR_ObjectID explosionObject = KR_ObjectID::NUL();
    if (!ExplosionSubjectState_QueueBatch(
            context, &explosion, 1, &explosionObject))
        goto failed;

    {
        SparkCreateRequest spark = {
            CFVector3(4099.0, 10000.0, -4099.0), ts, sparkTable,
            sparkIndex, kProbeSpark};
        KR_ObjectID sparkObject = KR_ObjectID::NUL();
        if (!SparkSubjectState_QueueCreate(context, spark, &sparkObject)) {
            stageFailure = "Spark queue rejected the probe";
            goto failed;
        }
    }
    {
        KR_ObjectID corpseObject = g_arena.newObject(
            corpseTable, kProbeCorpse);
        if (IsNul(corpseObject) ||
            !CorpseSubjectState_IsPending(context, corpseObject)) {
            stageFailure = "Corpse pending owner allocation failed";
            goto failed;
        }
        KR_Event event;
        event.label = CORPSE_START_ROTTING;
        event.source = KR_ObjectID::NUL();
        event.destination = corpseObject;
        event.timeStamp = ts;
        event.data.open(EDO_WRITE)
            .putObjectID(KR_ObjectID::NUL())
            .putInt(corpseIndex)
            .putDouble(4102.0)
            .putDouble(10000.0)
            .putDouble(-4102.0).close();
        context->addEvent(event);
    }
    {
        std::vector<SActiveWorldEvent> events;
        if (!ActiveWorldSemanticEvents_Capture(context, &events, failure)) {
            stageFailure = failure == NULL || failure->empty()
                ? "queued effects did not capture as EVT1"
                : *failure;
            goto failed;
        }
        if (events.size() < 3) {
            stageFailure = std::string("EVT1 captured only ") +
                std::to_string(events.size()) + " of three probe effects";
            goto failed;
        }
    }
    *staged = true;
    return true;

failed:
    ActiveWorldSemanticEvents_ClearProbe(context, NULL);
    SetFailure(failure, std::string("EVT1 retail staging failed: ") +
                            stageFailure);
    return false;
}

bool ActiveWorldSemanticEvents_ClearProbe(
    SimulationContext *context, std::string *failure)
{
    if (context == NULL ||
        !ClearProbeKind(context, kEffectCorpse, kProbeCorpse) ||
        !ClearProbeKind(context, kEffectSpark, kProbeSpark) ||
        !ClearProbeKind(context, kEffectExplosion, kProbeExplosion))
    {
        SetFailure(failure, "EVT1 probe cleanup failed");
        return false;
    }
    return true;
}
