#include "PeopleSubjectState.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "PEOPLE.H"
#include "kernel/h/context.h"
#include "message/peopmsg.h"
#include "obase/bullet/BulletAttributeState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "storage/h/subject.h"
#include "storage/h/savefile.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const unsigned long long kAbsentAttributeFingerprint =
    0x50454f5041545452ull;
const unsigned long long kAbsentSubjectFingerprint =
    0x50454f505355424aull;

int g_attributeCapacity = 0;
int g_subjectCapacity = 0;
std::string g_firstNotReady;

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
    if (value == NULL)
        value = "";
    HashBytes(hash, value, static_cast<int>(std::strlen(value)) + 1);
}

struct ObjectRoster
{
    SimulationContext *context;
    std::vector<KR_ObjectID> ids;
    bool valid;
};

bool CollectObject(const KR_ObjectID object, void *user)
{
    ObjectRoster *roster = static_cast<ObjectRoster *>(user);
    if (roster == NULL || roster->context == NULL ||
        roster->context->searchObject(object) == NULL)
    {
        if (roster != NULL)
            roster->valid = false;
        return false;
    }
    roster->ids.push_back(object);
    return true;
}

bool ObjectNameLess(const KR_ObjectID &left, const KR_ObjectID &right,
                    SimulationContext *context)
{
    const char *leftName = context->searchObject(left);
    const char *rightName = context->searchObject(right);
    return std::strcmp(leftName == NULL ? "" : leftName,
                       rightName == NULL ? "" : rightName) < 0;
}

bool CollectTable(SimulationContext *context, const char *name,
                  ObjectRoster &roster)
{
    roster.context = context;
    roster.valid = context != NULL;
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID : g_arena.searchSeanceClassTable(name);
    if (table == ct_NULLID)
        return false;
    g_arena.userFind(table, CollectObject, &roster);
    if (!roster.valid)
        return false;
    std::sort(roster.ids.begin(), roster.ids.end(),
              [context](const KR_ObjectID &left, const KR_ObjectID &right) {
                  return ObjectNameLess(left, right, context);
              });
    return true;
}

People *ResolvePeople(SimulationContext *context, const KR_ObjectID &id)
{
    KR_ObjectID mutableID = id;
    if (context == NULL || mutableID.isNUL())
        return NULL;
    IUnit *unit = static_cast<IUnit *>(context->queryInterface(id, IUnitIID));
    return unit == NULL ? NULL : dynamic_cast<People *>(unit);
}

bool RuntimeReady(People *people)
{
    return people != NULL && people->m_attr != NULL &&
           people->m_skin.Model() != NULL && people->m_askin != NULL &&
           !people->m_peopleAttrID.isNUL() && !people->m_routeID.isNUL() &&
           people->m_stateSP > 0 &&
           people->m_stateSP <= PeopleData::MAX_STATE;
}

struct ProbeExemplar
{
    SimulationContext *context;
    KR_ObjectID id;
    double score;
};

bool CaptureProbeExemplar(const KR_ObjectID id, void *user)
{
    ProbeExemplar *result = static_cast<ProbeExemplar *>(user);
    People *people = result == NULL ? NULL :
        ResolvePeople(result->context, id);
    if (!RuntimeReady(people) || people->movementSpeed() <= 1e-3)
        return true;
    const CFVector3 center = people->getPos() - people->getPosition();
    const double radius = people->getRadius();
    const double score = radius + std::sqrt(center.x * center.x +
                                             center.z * center.z);
    if (std::isfinite(score) && radius > 0.0 && score < result->score)
    {
        result->id = id;
        result->score = score;
    }
    return true;
}

bool SendStart(People *people, const KR_ObjectID &attribute,
               const char *routeName, double timeStamp)
{
    KR_ObjectID mutableAttribute = attribute;
    if (people == NULL || mutableAttribute.isNUL() || routeName == NULL)
        return false;
    KR_Event event;
    event.label = pe_EVCMD_START_EX;
    event.source = g_arena.getObjectID();
    event.destination = people->getObjectID();
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(attribute)
              .putStr(routeName)
              .putDouble(0.0)
              .putInt(0)
              .putInt(7)
              .putDouble(1.25)
              .close();
    return people->receiveEvent(event) == 1 && RuntimeReady(people);
}

bool SamePersistentState(const PeopleData &left, const PeopleData &right)
{
    return std::memcmp(&left, &right, sizeof(PeopleData)) == 0;
}

bool RoundTripSerializedState(const PeopleData &saved)
{
    char temporaryDirectory[MAX_PATH] = {};
    char temporaryFile[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, temporaryDirectory) == 0 ||
        GetTempFileNameA(temporaryDirectory, "r2p", 0, temporaryFile) == 0)
        return false;

    PIN_SaveFile output;
    bool valid = output.OpenWrite(temporaryFile) &&
                 output.WriteData(
                     reinterpret_cast<char *>(const_cast<PeopleData *>(&saved)),
                     sizeof(PeopleData));
    output.Close();

    PeopleData restored = {};
    if (valid)
    {
        PIN_SaveFile input;
        valid = input.OpenRead(temporaryFile) &&
                input.GetData(reinterpret_cast<char *>(&restored),
                              sizeof(PeopleData)) &&
                input.GetCurrentData() == NULL;
        input.Close();
    }
    DeleteFileA(temporaryFile);
    return valid && SamePersistentState(saved, restored);
}

}  // namespace

void PeopleSubjectState_Link()
{
    // Pull PEOPLE.CPP (and its static People/PeopleAttr registrars) out of the
    // archive without publishing this temporary object to a context.
    People linkAnchor;
    (void)linkAnchor;
}

void PeopleSubjectState_SetExpectedCapacities(int attributeCapacity,
                                              int subjectCapacity)
{
    g_attributeCapacity = attributeCapacity > 0 ? attributeCapacity : 0;
    g_subjectCapacity = subjectCapacity > 0 ? subjectCapacity : 0;
}

int PeopleSubjectState_AttributeCapacity()
{
    return g_attributeCapacity;
}

int PeopleSubjectState_SubjectCapacity()
{
    return g_subjectCapacity;
}

int PeopleSubjectState_AttributeCount(SimulationContext *context)
{
    ObjectRoster roster = {};
    if (!CollectTable(context, "PeopleAttr", roster))
        return g_attributeCapacity == 0 ? 0 : -1;
    return static_cast<int>(roster.ids.size());
}

int PeopleSubjectState_LiveCount(SimulationContext *context)
{
    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return g_subjectCapacity == 0 ? 0 : -1;
    return static_cast<int>(roster.ids.size());
}

int PeopleSubjectState_SoundCount(SimulationContext *context)
{
    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return g_subjectCapacity == 0 ? 0 : -1;
    int count = 0;
    for (std::size_t i = 0; i < roster.ids.size(); ++i)
    {
        People *people = ResolvePeople(context, roster.ids[i]);
        if (people == NULL)
            return -1;
        if (!people->m_snd.isNUL())
        {
            if (!context->isExist(people->m_snd))
                return -1;
            ++count;
        }
    }
    return count;
}

bool PeopleSubjectState_UpdateAttributes(SimulationContext *context,
                                         double timeStamp)
{
    if (context == NULL || g_arena.getContext() != context ||
        !std::isfinite(timeStamp) ||
        g_arena.searchSeanceClassTable("PeopleAttr") == ct_NULLID)
        return false;
    ct_ClassTable *raw = ct_Storage::searchClassTable("PeopleAttr");
    ct_AttributeTable *attributes =
        dynamic_cast<ct_AttributeTable *>(raw);
    if (attributes == NULL || attributes->getClassTableID() == ct_NULLID)
        return false;
    attributes->update(timeStamp);
    return true;
}

bool PeopleSubjectState_TablesReady(SimulationContext *context,
                                    int expectedAttributeCapacity,
                                    int expectedSubjectCapacity)
{
    if (context == NULL || expectedAttributeCapacity < 0 ||
        expectedSubjectCapacity < 0 ||
        g_attributeCapacity != expectedAttributeCapacity ||
        g_subjectCapacity != expectedSubjectCapacity)
        return false;
    const bool attributePresent =
        g_arena.searchSeanceClassTable("PeopleAttr") != ct_NULLID;
    const bool subjectPresent =
        g_arena.searchSeanceClassTable("People") != ct_NULLID;
    return attributePresent == (expectedAttributeCapacity > 0) &&
           subjectPresent == (expectedSubjectCapacity > 0);
}

bool PeopleSubjectState_AllReady(SimulationContext *context)
{
    g_firstNotReady.clear();
    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return g_subjectCapacity == 0;
    for (std::size_t i = 0; i < roster.ids.size(); ++i)
    {
        People *people = ResolvePeople(context, roster.ids[i]);
        if (!RuntimeReady(people))
        {
            const char *name = context->searchObject(roster.ids[i]);
            g_firstNotReady = name == NULL ? "<unnamed>" : name;
            g_firstNotReady += ":";
            if (people == NULL) g_firstNotReady += "type";
            else if (people->m_attr == NULL) g_firstNotReady += "attribute";
            else if (people->m_skin.Model() == NULL) g_firstNotReady += "model";
            else if (people->m_askin == NULL) g_firstNotReady += "skin-interface";
            else if (people->m_peopleAttrID.isNUL()) g_firstNotReady += "attribute-id";
            else if (people->m_routeID.isNUL()) g_firstNotReady += "route";
            else g_firstNotReady += "state-stack";
            return false;
        }
    }
    return true;
}

const char *PeopleSubjectState_FirstNotReady()
{
    return g_firstNotReady.c_str();
}

unsigned long long PeopleSubjectState_AttributeFingerprint(
    SimulationContext *context)
{
    ObjectRoster roster = {};
    if (!CollectTable(context, "PeopleAttr", roster))
        return g_attributeCapacity == 0 ? kAbsentAttributeFingerprint : 0;
    unsigned long long hash = kHashOffset;
    HashBytes(hash, &g_attributeCapacity, sizeof(g_attributeCapacity));
    for (std::size_t i = 0; i < roster.ids.size(); ++i)
        HashString(hash, context->searchObject(roster.ids[i]));
    return hash;
}

unsigned long long PeopleSubjectState_SubjectFingerprint(
    SimulationContext *context)
{
    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return g_subjectCapacity == 0 ? kAbsentSubjectFingerprint : 0;
    unsigned long long hash = kHashOffset;
    HashBytes(hash, &g_subjectCapacity, sizeof(g_subjectCapacity));
    for (std::size_t i = 0; i < roster.ids.size(); ++i)
    {
        People *people = ResolvePeople(context, roster.ids[i]);
        if (!RuntimeReady(people))
            return 0;
        HashString(hash, context->searchObject(roster.ids[i]));
        HashString(hash, context->searchObject(people->m_peopleAttrID));
        HashString(hash, context->searchObject(people->m_routeID));
        const CFVector3 position = people->getPosition();
        HashBytes(hash, &position, sizeof(position));
        HashBytes(hash, &people->m_damage, sizeof(people->m_damage));
        HashBytes(hash, &people->m_killed, sizeof(people->m_killed));
        HashBytes(hash, &people->m_stateSP, sizeof(people->m_stateSP));
    }
    return hash;
}

unsigned long long PeopleSubjectState_AbsentAttributeFingerprint()
{
    return kAbsentAttributeFingerprint;
}

unsigned long long PeopleSubjectState_AbsentSubjectFingerprint()
{
    return kAbsentSubjectFingerprint;
}

bool PeopleSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    SPeopleLifecycleProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const int baselineCount = PeopleSubjectState_LiveCount(context);
    const int baselineSounds = SoundObjectState_LiveCount();
    const unsigned long long baselineFingerprint =
        PeopleSubjectState_SubjectFingerprint(context);
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID : g_arena.searchSeanceClassTable("People");
    if (context == NULL || table == ct_NULLID || baselineCount <= 0 ||
        baselineCount >= g_subjectCapacity || baselineFingerprint == 0)
        return false;

    ProbeExemplar selection = {context, KR_ObjectID::NUL(), DBL_MAX};
    g_arena.userFind(table, CaptureProbeExemplar, &selection);
    const KR_ObjectID exemplarID = selection.id;
    People *exemplar = ResolvePeople(context, exemplarID);
    if (!RuntimeReady(exemplar))
        return false;
    const KR_ObjectID attribute = exemplar->m_peopleAttrID;
    const char *routeName = context->searchObject(exemplar->m_routeID);

    KR_ObjectID probeID = g_arena.newObject(table, "People.Lifecycle.Probe");
    People *probe = ResolvePeople(context, probeID);
    bool valid = SendStart(probe, attribute, routeName, timeStamp);
    if (valid)
    {
        summary->validStarts =
            probe->m_startBackSpaceNode == 7 &&
            probe->m_startMoveDelay == 1.25 &&
            Abs2(probe->m_dir) == 0.0 ? 1 : 0;
        const bool showScheduled =
            context->removeEvent(pe_EV_STARTSHOW, probeID) == 1;
        KR_Event show;
        show.label = pe_EV_STARTSHOW;
        show.source = probeID;
        show.destination = probeID;
        show.timeStamp = timeStamp < 0.1 ? 0.1 : timeStamp;
        const bool shown = showScheduled && probe->receiveEvent(show) == 1;
        summary->dynamicReady =
            shown && context->queryInterface(probeID, IDynamicObjectIID) != NULL
                ? 1 : 0;
        summary->renderReady = probe->m_skin.Model() != NULL ? 1 : 0;

        const bool startMoveScheduled =
            context->removeEvent(pe_EV_STARTMOVE, probeID) == 1;
        KR_Event startMove;
        startMove.label = pe_EV_STARTMOVE;
        startMove.source = probeID;
        startMove.destination = probeID;
        startMove.timeStamp = timeStamp + 1.25;
        const bool movementStarted =
            startMoveScheduled && probe->receiveEvent(startMove) == 1 &&
            Abs2(probe->m_dir) > 0.0;
        const bool moveScheduled =
            context->removeEvent(pe_EVC_MOVE, probeID) == 1;
        const bool nextNodeScheduled =
            context->removeEvent(pe_EVC_NEXTNODE, probeID) == 1;
        summary->scheduledMoves =
            movementStarted && moveScheduled && nextNodeScheduled ? 1 : 0;

        const PeopleData saved = *static_cast<PeopleData *>(probe);
        const CFVector3 savedPosition = probe->getPosition();
        probe->m_damage = -123.0;
        probe->m_stateSP = 0;
        *static_cast<PeopleData *>(probe) = saved;
        probe->ct_Subject::setPosition(savedPosition);
        if (SamePersistentState(saved, *static_cast<PeopleData *>(probe)) &&
            probe->getPosition().x == savedPosition.x &&
            probe->getPosition().y == savedPosition.y &&
            probe->getPosition().z == savedPosition.z &&
            RoundTripSerializedState(saved))
            summary->saveStateRoundTrips = 1;

        const char *bulletAttribute =
            BulletAttributeState_FirstAttributeName(context);
        IDynamicObject *dynamic = static_cast<IDynamicObject *>(
            context->queryInterface(probeID, IDynamicObjectIID));
        if (dynamic != NULL)
        {
            const CFVector3 dynamicPosition = dynamic->getPos();
            const double dynamicRadius = dynamic->getRadius();
            if (std::isfinite(dynamicPosition.y) &&
                std::isfinite(dynamicRadius) && dynamicRadius > 0.0 &&
                dynamicPosition.y < 10000.0)
            {
                const CFVector3 centerOffset =
                    dynamicPosition - probe->getPosition();
                // Put the temporary subject in a sparse deterministic cache
                // cell and move its dynamic center far above the world.  This
                // avoids choosing a retail neighbour first on dense levels
                // while still exercising the real spatial lookup path.
                const CFVector3 desiredCenter(512.0, 10000.0, -512.0);
                probe->ct_Subject::setPosition(desiredCenter - centerOffset);
            }
        }
        const double damageBefore = probe->m_damage;
        if (bulletAttribute != NULL &&
            BulletSubjectState_ProbeDynamicCollisionLifecycle(
                context, bulletAttribute, probeID, timeStamp + 0.01) &&
            probe->m_damage < damageBefore)
            summary->bulletDamageApplications = 1;

        probe->setDamage(probe->m_damage + 1.0, probe->getPosition(),
                         timeStamp + 0.02, g_arena.getObjectID());
        if (probe->m_damage <= 0.0 && probe->m_killed != KILL_NONE)
            summary->deathTransitions = 1;
    }

    if (!probeID.isNUL() && context->isExist(probeID))
        context->removeObject(probeID);
    if (PeopleSubjectState_LiveCount(context) == baselineCount &&
        SoundObjectState_LiveCount() == baselineSounds &&
        PeopleSubjectState_SubjectFingerprint(context) == baselineFingerprint)
        summary->rollbacks = 1;

    return valid && summary->validStarts == 1 &&
           summary->dynamicReady == 1 && summary->renderReady == 1 &&
           summary->scheduledMoves == 1 &&
           summary->bulletDamageApplications == 1 &&
           summary->deathTransitions == 1 &&
           summary->saveStateRoundTrips == 1 && summary->rollbacks == 1;
}
