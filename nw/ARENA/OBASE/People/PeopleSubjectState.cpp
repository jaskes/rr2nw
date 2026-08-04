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
#include "PeopleContactResponse.h"
#include "PeopleObstacleRecovery.h"
#include "PeopleRouteMotion.h"
#include "i/route.i"
#include "i/commander.i"
#include "kernel/h/context.h"
#include "message/peopmsg.h"
#include "obase/bullet/BulletAttributeState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/corpse/CorpseSubjectState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "storage/h/subject.h"
#include "storage/h/savefile.h"

// Exact legacy spawn path used by People::onShoot.  The recovery probe calls
// it with the resolved symbolic attribute index without exposing the private
// AttributePeople class.
void shoot(KR_ObjectID fromID, int bulletTable, int attrIndex,
           const CFVector3 &pos, const CFVector3 &dir,
           SimulationContext *context, double ts);

static bool FiniteVector(const CFVector3 &value);

// Encoding-preserved PEOPLE.CPP owns these route geometry helpers. Keep the
// probe declarations here so the legacy header does not need another edit.
double g_distToSeg(const CFVector3 &value, const CFVector3 &start,
                   const CFVector3 &end);
void g_toSeg(CFVector3 &value, const CFVector3 &start,
             const CFVector3 &end, double maximumDistance);

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const unsigned long long kAbsentAttributeFingerprint =
    0x50454f5041545452ull;
const unsigned long long kAbsentSubjectFingerprint =
    0x50454f505355424aull;
const int kPeopleOnObjects = 3;

int g_attributeCapacity = 0;
int g_subjectCapacity = 0;
std::string g_firstNotReady;
SPeopleLiveCombatTelemetry g_liveCombatTelemetry = {};
struct LivePeopleSample
{
    KR_ObjectID id;
    KR_ObjectID enemy;
    std::string name;
    CFVector3 position;
    double damage;
    double lastMoveTime;
    double lastShootTime;
    double findDeadline;
    int state;
    int killed;

    LivePeopleSample()
        : id(KR_ObjectID::NUL()), enemy(KR_ObjectID::NUL()), damage(0.0),
          lastMoveTime(0.0), lastShootTime(0.0), findDeadline(-1.0),
          state(pe_STATE_DEFAULT), killed(KILL_NONE)
    {
    }
};
std::vector<LivePeopleSample> g_livePeopleSamples;
int g_liveExplosionCount = -1;
int g_liveCorpseCount = -1;

class LivePeopleRouteSource : public IPeopleRouteNodeSource
{
public:
    explicit LivePeopleRouteSource(IRouteObject *route) : route_(route) {}
    int NodeCount() { return route_ == NULL ? 0 : route_->GetNodeCnt(); }
    CFVector3 Node(int index) { return route_->GetNode(index); }

private:
    IRouteObject *route_;
};

void CopyTelemetryName(char *destination, std::size_t capacity,
                       const char *source)
{
    if (destination == NULL || capacity == 0)
        return;
    destination[0] = 0;
    if (source != NULL)
        std::strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = 0;
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
    const int comparison = std::strcmp(leftName == NULL ? "" : leftName,
                                       rightName == NULL ? "" : rightName);
    return comparison != 0 ? comparison < 0 : left.id < right.id;
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

ct_Attribute *ResolvePeopleAttribute(SimulationContext *context,
                                     const char *name)
{
    if (context == NULL || name == NULL || name[0] == 0)
        return NULL;
    ct_ClassTable *raw = ct_Storage::searchClassTable("PeopleAttr");
    ct_AttributeTable *attributes =
        dynamic_cast<ct_AttributeTable *>(raw);
    KR_ObjectID id = context->searchObject(name);
    return attributes == NULL || id.isNUL()
        ? NULL : attributes->searchAttribute(id);
}

bool RuntimeReady(People *people)
{
    return people != NULL && people->m_attr != NULL &&
           people->m_skin.Model() != NULL && people->m_askin != NULL &&
           !people->m_peopleAttrID.isNUL() && !people->m_routeID.isNUL() &&
           people->m_stateSP >= 0 &&
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

int RemoveAllSubjects(SimulationContext *context, const char *tableName)
{
    ObjectRoster roster = {};
    if (!CollectTable(context, tableName, roster))
        return 0;
    int removed = 0;
    for (std::vector<KR_ObjectID>::reverse_iterator id = roster.ids.rbegin();
         id != roster.ids.rend(); ++id)
    {
        if (context->isExist(*id))
        {
            context->removeObject(*id);
            ++removed;
        }
    }
    return removed;
}

void RemovePeopleSchedulerEvents(SimulationContext *context,
                                 const KR_ObjectID &owner)
{
    KR_ObjectID mutableOwner = owner;
    if (context == NULL || mutableOwner.isNUL())
        return;
    const int labels[] = {
        pe_EVC_MOVE, pe_EVC_NEXTNODE, pe_EVC_GROUNDED_NEXTNODE,
        pe_EVC_FIND_ENEMY, pe_EV_STARTSHOW, pe_EV_SETAUTOANIM,
        pe_EV_STARTMOVE};
    for (std::size_t index = 0;
         index < sizeof(labels) / sizeof(labels[0]); ++index)
        while (context->removeEvent(labels[index], owner) == 1) {}
}

bool TakePeopleEvent(SimulationContext *context, int label,
                     const KR_ObjectID &owner, KR_Event *event)
{
    KR_ObjectID mutableOwner = owner;
    if (context == NULL || mutableOwner.isNUL() || event == NULL)
        return false;
    KR_Event copied[2];
    if (context->copyEvents(label, owner, copied, 2) != 1 ||
        context->removeEvent(label, owner) != 1)
        return false;
    event->getCopy(copied[0]);
    return true;
}

}  // namespace

void PeopleSubjectState_Link()
{
    // Pull PEOPLE.CPP (and its static People/PeopleAttr registrars) out of the
    // archive without publishing this temporary object to a context.
    People linkAnchor;
    (void)linkAnchor;
}

void PeopleSubjectState_ResetLiveCombatTelemetry()
{
    std::memset(&g_liveCombatTelemetry, 0, sizeof(g_liveCombatTelemetry));
    g_livePeopleSamples.clear();
    g_liveExplosionCount = -1;
    g_liveCorpseCount = -1;
}

bool PeopleSubjectState_LiveCombatTelemetry(
    SPeopleLiveCombatTelemetry *telemetry)
{
    if (telemetry == NULL)
        return false;
    *telemetry = g_liveCombatTelemetry;
    return true;
}

bool PeopleSubjectState_SampleLiveCombat(SimulationContext *context)
{
    if (context == NULL)
        return false;
    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return false;

    ++g_liveCombatTelemetry.sampleFrames;
    g_liveCombatTelemetry.rosterSamples += roster.ids.size();
    std::vector<LivePeopleSample> current;
    current.reserve(roster.ids.size());
    for (std::size_t index = 0; index < roster.ids.size(); ++index)
    {
        People *people = ResolvePeople(context, roster.ids[index]);
        if (!RuntimeReady(people))
            return false;
        LivePeopleSample sample;
        sample.id = roster.ids[index];
        sample.enemy = people->getEnemyID();
        const char *name = context->searchObject(sample.id);
        sample.name = name == NULL ? "" : name;
        sample.position = people->getPosition();
        sample.damage = people->m_damage;
        sample.lastMoveTime = people->m_lastMoveTimeStamp;
        sample.lastShootTime = people->m_prevShootTime;
        sample.state = people->getState();
        sample.killed = people->m_killed;
        KR_Event findEvent[1];
        if (context->copyEvents(pe_EVC_FIND_ENEMY, sample.id,
                                findEvent, 1) == 1)
            sample.findDeadline = findEvent[0].timeStamp;

        const std::vector<LivePeopleSample>::const_iterator previous =
            std::find_if(g_livePeopleSamples.begin(),
                         g_livePeopleSamples.end(),
                         [&sample](const LivePeopleSample &value) {
                             return value.id == sample.id &&
                                    value.name == sample.name;
                         });
        if (previous != g_livePeopleSamples.end())
        {
            const bool attack = sample.state == pe_STATE_ATTACK;
            if (attack)
                ++g_liveCombatTelemetry.attackStateSamples;
            if (sample.lastMoveTime > previous->lastMoveTime + 1e-9)
            {
                ++g_liveCombatTelemetry.moveEvents;
                if (attack)
                    ++g_liveCombatTelemetry.attackMoveEvents;
                if (people->m_isClz != 0)
                    ++g_liveCombatTelemetry.contactMoveEvents;
                const double deltaTime = people->m_lastMoveDeltaT;
                const double displacement = hypot(
                    sample.position.x - people->m_lastMovePos.x,
                    sample.position.z - people->m_lastMovePos.z);
                const bool suppressed = people->m_stoped && attack;
                if (std::isfinite(deltaTime) && deltaTime > 0.0 &&
                    std::isfinite(displacement) &&
                    people->getMoveSpeed() > 1e-6 && !suppressed)
                {
                    ++g_liveCombatTelemetry.eligibleMoveEvents;
                    g_liveCombatTelemetry.maximumHorizontalDisplacement =
                        (std::max)(
                            g_liveCombatTelemetry.maximumHorizontalDisplacement,
                            displacement);
                    const char *attributeName =
                        context->searchObject(people->m_peopleAttrID);
                    ct_Attribute *movementAttribute =
                        ResolvePeopleAttribute(context, attributeName);
                    if (movementAttribute != NULL &&
                        movementAttribute->get_int("m_onLand") ==
                            kPeopleOnObjects &&
                        movementAttribute->get_int("m_stopIfAttack") != 0)
                    {
                        const double minimumAlignment =
                            movementAttribute->get_double(
                                "m_deltaZeroSpeed");
                        const CFVector3 moveDirection = people->getMoveDir();
                        if (PeopleRouteMotion_AllowsHorizontalStep(
                                moveDirection, people->m_lastMovePos,
                                people->m_nextNode, minimumAlignment) &&
                            !PeopleRouteMotion_AllowsSpatialStep(
                                moveDirection, people->m_lastMovePos,
                                people->m_nextNode, minimumAlignment))
                        {
                            ++g_liveCombatTelemetry.
                                legacySlopeReleaseOpportunities;
                            if (displacement > 1e-6)
                            {
                                ++g_liveCombatTelemetry.
                                    legacySlopeReleasedMoves;
                                CopyTelemetryName(
                                    g_liveCombatTelemetry.
                                        lastLegacySlopeReleasedOwner,
                                    sizeof(g_liveCombatTelemetry.
                                               lastLegacySlopeReleasedOwner),
                                    sample.name.c_str());
                            }
                        }
                    }
                    if (displacement > 1e-6)
                        ++g_liveCombatTelemetry.displacedMoveEvents;
                    else
                    {
                        ++g_liveCombatTelemetry.stationaryMoveEvents;
                        g_liveCombatTelemetry.lastStationaryDeltaTime =
                            deltaTime;
                        g_liveCombatTelemetry.lastStationaryMoveSpeed =
                            people->getMoveSpeed();
                        g_liveCombatTelemetry.lastStationaryX =
                            sample.position.x;
                        g_liveCombatTelemetry.lastStationaryY =
                            sample.position.y;
                        g_liveCombatTelemetry.lastStationaryZ =
                            sample.position.z;
                        g_liveCombatTelemetry.lastStationaryMoveStartX =
                            people->m_lastMovePos.x;
                        g_liveCombatTelemetry.lastStationaryMoveStartZ =
                            people->m_lastMovePos.z;
                        const CFVector3 moveDirection = people->getMoveDir();
                        g_liveCombatTelemetry.lastStationaryDirectionX =
                            moveDirection.x;
                        g_liveCombatTelemetry.lastStationaryDirectionZ =
                            moveDirection.z;
                        g_liveCombatTelemetry.lastStationaryTargetX =
                            people->m_nextNode.x;
                        g_liveCombatTelemetry.lastStationaryTargetZ =
                            people->m_nextNode.z;
                        g_liveCombatTelemetry.
                            lastStationaryObstacleRecoveryTime =
                                people->m_obstacleRecoveryTime;
                        g_liveCombatTelemetry.lastStationaryContactCode =
                            people->m_isClz;
                        g_liveCombatTelemetry.lastStationaryState =
                            sample.state;
                        g_liveCombatTelemetry.lastStationaryStopped =
                            people->m_stoped;
                        g_liveCombatTelemetry.lastStationaryPreviousNode =
                            people->m_previousRouteNode;
                        g_liveCombatTelemetry.lastStationaryCurrentNode =
                            people->m_curNode;
                        CopyTelemetryName(
                            g_liveCombatTelemetry.lastStationaryOwner,
                            sizeof(
                                g_liveCombatTelemetry.lastStationaryOwner),
                            sample.name.c_str());
                        CopyTelemetryName(
                            g_liveCombatTelemetry.lastStationaryAttribute,
                            sizeof(g_liveCombatTelemetry.
                                       lastStationaryAttribute),
                            context->searchObject(people->m_peopleAttrID));
                        ct_Attribute *peopleAttribute =
                            ResolvePeopleAttribute(
                                context,
                                g_liveCombatTelemetry.
                                    lastStationaryAttribute);
                        if (peopleAttribute != NULL)
                        {
                            g_liveCombatTelemetry.lastStationaryOnLand =
                                peopleAttribute->get_int("m_onLand");
                            g_liveCombatTelemetry.
                                lastStationaryStopIfAttack =
                                    peopleAttribute->get_int(
                                        "m_stopIfAttack");
                            g_liveCombatTelemetry.
                                lastStationaryDeltaZeroSpeed =
                                    peopleAttribute->get_double(
                                        "m_deltaZeroSpeed");
                            IRouteObject *route =
                                static_cast<IRouteObject *>(
                                    context->queryInterface(
                                        people->m_routeID,
                                        IRouteObjectIID));
                            if (route != NULL)
                            {
                                LivePeopleRouteSource source(route);
                                SPeopleRouteMotionRequest request = {};
                                request.candidate = sample.position +
                                    moveDirection * people->getMoveSpeed() *
                                        deltaTime;
                                request.previousNode =
                                    people->m_previousRouteNode;
                                request.currentNode = people->m_curNode;
                                request.backSpaceNode =
                                    people->m_startBackSpaceNode;
                                request.horizontal =
                                    peopleAttribute->get_int("m_onLand") == 0
                                        ? 0 : 1;
                                request.maximumSegments =
                                    sample.state == pe_STATE_DEFAULT ? 10 : 0;
                                request.maximumCorridorDistance =
                                    peopleAttribute->get_double(
                                        "m_maxOutDist");
                                request.movementDistance =
                                    people->getMoveSpeed() * deltaTime;
                                request.centerToRoute =
                                    people->m_isClz == 0 ? 1 : 0;
                                SPeopleRouteMotionResult result = {};
                                if (PeopleRouteMotion_Advance(
                                        &source, request, &result))
                                {
                                    const double predictedDisplacement = hypot(
                                        result.position.x - sample.position.x,
                                        result.position.z - sample.position.z);
                                    g_liveCombatTelemetry.
                                        lastStationaryPredictedDisplacement =
                                            predictedDisplacement;
                                }
                            }
                        }
                        CopyTelemetryName(
                            g_liveCombatTelemetry.lastStationaryRoute,
                            sizeof(g_liveCombatTelemetry.lastStationaryRoute),
                            context->searchObject(people->m_routeID));
                    }
                }
            }
            if (sample.findDeadline >= 0.0 &&
                previous->findDeadline >= 0.0 &&
                sample.findDeadline > previous->findDeadline + 1e-9)
            {
                ++g_liveCombatTelemetry.findEvents;
                if (previous->state == pe_STATE_DEFAULT)
                {
                    ++g_liveCombatTelemetry.eligibleFindEvents;
                    if (attack && !sample.enemy.isNUL())
                    {
                        ++g_liveCombatTelemetry.targetAcquisitions;
                        CopyTelemetryName(
                            g_liveCombatTelemetry.lastAcquiringOwner,
                            sizeof(
                                g_liveCombatTelemetry.lastAcquiringOwner),
                            sample.name.c_str());
                    }
                    else ++g_liveCombatTelemetry.targetMisses;
                }
            }
            if (sample.lastShootTime > previous->lastShootTime + 1e-9)
            {
                ++g_liveCombatTelemetry.shotsStarted;
                CopyTelemetryName(
                    g_liveCombatTelemetry.lastShootingOwner,
                    sizeof(g_liveCombatTelemetry.lastShootingOwner),
                    sample.name.c_str());
            }
            if (sample.damage < previous->damage - 1e-9)
            {
                ++g_liveCombatTelemetry.damageApplications;
                CopyTelemetryName(
                    g_liveCombatTelemetry.lastDamagedOwner,
                    sizeof(g_liveCombatTelemetry.lastDamagedOwner),
                    sample.name.c_str());
            }
            if (previous->killed == KILL_NONE &&
                sample.killed != KILL_NONE)
            {
                ++g_liveCombatTelemetry.killTransitions;
                CopyTelemetryName(
                    g_liveCombatTelemetry.lastKilledOwner,
                    sizeof(g_liveCombatTelemetry.lastKilledOwner),
                    sample.name.c_str());
            }
        }
        current.push_back(sample);
    }

    const int explosions = ExplosionSubjectState_LiveCount();
    const int corpses = CorpseSubjectState_LiveCount();
    if (g_liveExplosionCount >= 0 && explosions > g_liveExplosionCount)
        g_liveCombatTelemetry.explosionEffects +=
            explosions - g_liveExplosionCount;
    if (g_liveCorpseCount >= 0 && corpses > g_liveCorpseCount)
        g_liveCombatTelemetry.corpseEffects += corpses - g_liveCorpseCount;
    g_liveExplosionCount = explosions;
    g_liveCorpseCount = corpses;
    g_livePeopleSamples.swap(current);
    return true;
}

bool PeopleSubjectState_ObjectIDs(
    SimulationContext *context, std::vector<KR_ObjectID> *objects)
{
    if (objects == NULL)
        return false;
    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return false;
    *objects = roster.ids;
    return true;
}

bool PeopleSubjectState_SelectNaturalMissionCombat(
    SimulationContext *context,
    const std::vector<KR_ObjectID> &baselineObjects,
    SPeopleNaturalCombatSummary *summary)
{
    if (context == NULL || summary == NULL)
        return false;
    std::vector<SPeopleNaturalCombatSummary> cohort;
    if (!PeopleSubjectState_SelectNaturalMissionCombatCohort(
            context, baselineObjects, &cohort) || cohort.empty())
        return false;
    *summary = cohort[0];
    return true;
}

bool PeopleSubjectState_SelectNaturalMissionCombatCohort(
    SimulationContext *context,
    const std::vector<KR_ObjectID> &baselineObjects,
    std::vector<SPeopleNaturalCombatSummary> *summaries)
{
    if (context == NULL || summaries == NULL)
        return false;
    summaries->clear();

    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return false;

    struct Candidate
    {
        int score;
        SPeopleNaturalCombatSummary summary;
    };
    std::vector<Candidate> candidates;
    int missionPeople = 0;
    int missionShooters = 0;
    for (std::size_t index = 0; index < roster.ids.size(); ++index)
    {
        if (std::find(baselineObjects.begin(), baselineObjects.end(),
                      roster.ids[index]) != baselineObjects.end())
            continue;
        ++missionPeople;
        People *people = ResolvePeople(context, roster.ids[index]);
        if (!RuntimeReady(people) || !people->isShooter())
            continue;
        ++missionShooters;
        const char *name = context->searchObject(roster.ids[index]);
        int score = 0;
        if (name != NULL &&
            std::strstr(name, "Enemy.Flyer.Falcon.01") != NULL)
            score += 1000;
        if (name != NULL && std::strstr(name, "Enemy") != NULL)
            score += 100;
        if (people->m_isNotCreate == 0)
            score += 20;
        if (people->m_isVisible != 0)
            score += 10;
        const CFVector3 position = people->getPosition();
        if (!FiniteVector(position))
            return false;
        Candidate candidate = {};
        candidate.score = score;
        candidate.summary.baselinePeople =
            static_cast<int>(baselineObjects.size());
        candidate.summary.livePeople = static_cast<int>(roster.ids.size());
        candidate.summary.available = 1;
        candidate.summary.visible = people->m_isVisible != 0 ? 1 : 0;
        candidate.summary.targetDistance = -1.0;
        candidate.summary.initialX = candidate.summary.currentX = position.x;
        candidate.summary.initialY = candidate.summary.currentY = position.y;
        candidate.summary.initialZ = candidate.summary.currentZ = position.z;
        candidate.summary.initialShootTime =
            candidate.summary.currentShootTime = people->m_prevShootTime;
        candidate.summary.actorID = roster.ids[index];
        CopyTelemetryName(candidate.summary.actor,
                          sizeof(candidate.summary.actor), name);
        CopyTelemetryName(candidate.summary.commander,
                          sizeof(candidate.summary.commander),
                          context->searchObject(people->m_commanderID));
        CopyTelemetryName(candidate.summary.attribute,
                          sizeof(candidate.summary.attribute),
                          context->searchObject(people->m_peopleAttrID));
        CopyTelemetryName(candidate.summary.route,
                          sizeof(candidate.summary.route),
                          context->searchObject(people->m_routeID));
        candidates.push_back(candidate);
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate &left, const Candidate &right) {
                         return left.score > right.score;
                     });
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        candidates[index].summary.missionPeople = missionPeople;
        candidates[index].summary.missionShooters = missionShooters;
        summaries->push_back(candidates[index].summary);
    }
    return !summaries->empty();
}

bool PeopleSubjectState_InspectNaturalMissionCombat(
    SimulationContext *context,
    const SPeopleNaturalCombatSummary *selection,
    SPeopleNaturalCombatSummary *summary)
{
    if (context == NULL || selection == NULL || summary == NULL ||
        selection->available == 0)
        return false;
    *summary = *selection;
    People *actor = ResolvePeople(context, selection->actorID);
    if (!RuntimeReady(actor))
    {
        summary->available = 0;
        return true;
    }

    const CFVector3 position = actor->getPosition();
    if (!FiniteVector(position))
        return false;
    summary->visible = actor->m_isVisible != 0 ? 1 : 0;
    summary->attackState =
        actor->getState() == pe_STATE_ATTACK ? 1 : 0;
    summary->currentX = position.x;
    summary->currentY = position.y;
    summary->currentZ = position.z;
    summary->horizontalDisplacement = hypot(
        position.x - selection->initialX,
        position.z - selection->initialZ);
    summary->currentShootTime = actor->m_prevShootTime;
    summary->shot = actor->m_prevShootTime >
        selection->initialShootTime + 1e-9 ? 1 : 0;

    KR_ObjectID targetID = actor->getEnemyID();
    summary->hasTarget = targetID.isNUL() ? 0 : 1;
    CopyTelemetryName(summary->target, sizeof(summary->target),
                      context->searchObject(targetID));
    IDynamicObject *target = targetID.isNUL() ? NULL :
        static_cast<IDynamicObject *>(
            context->queryInterface(targetID, IDynamicObjectIID));
    if (target != NULL)
    {
        const CFVector3 targetPosition = target->getPos();
        if (!FiniteVector(targetPosition))
            return false;
        summary->targetIsDynamic = 1;
        summary->targetDistance = Abs(targetPosition - position);
    }
    else summary->targetDistance = -1.0;
    return true;
}

bool PeopleSubjectState_StageMissionCombat(
    SimulationContext *context,
    const std::vector<KR_ObjectID> &baselineObjects,
    double timeStamp, SPeopleMissionCombatStageSummary *summary)
{
    if (context == NULL || summary == NULL || !std::isfinite(timeStamp))
        return false;
    std::memset(summary, 0, sizeof(*summary));
    summary->baselinePeople = static_cast<int>(baselineObjects.size());

    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return false;
    summary->livePeople = static_cast<int>(roster.ids.size());

    std::vector<KR_ObjectID> missionObjects;
    for (std::size_t index = 0; index < roster.ids.size(); ++index)
        if (std::find(baselineObjects.begin(), baselineObjects.end(),
                      roster.ids[index]) == baselineObjects.end())
            missionObjects.push_back(roster.ids[index]);
    summary->missionPeople = static_cast<int>(missionObjects.size());

    People *selectedAttacker = NULL;
    People *selectedTarget = NULL;
    KR_ObjectID selectedAttackerID = KR_ObjectID::NUL();
    KR_ObjectID selectedTargetID = KR_ObjectID::NUL();
    int selectedScore = -1;
    for (std::size_t attackerIndex = 0;
         attackerIndex < missionObjects.size(); ++attackerIndex)
    {
        People *attacker = ResolvePeople(context, missionObjects[attackerIndex]);
        if (!RuntimeReady(attacker) || !attacker->isShooter() ||
            attacker->m_commanderID.isNUL())
            continue;
        const char *attackerAttributeName =
            context->searchObject(attacker->m_peopleAttrID);
        ct_Attribute *attackerAttribute =
            ResolvePeopleAttribute(context, attackerAttributeName);
        SPeopleGameplayTuningState attackerTuning = {};
        if (attackerAttribute == NULL || attackerAttributeName == NULL ||
            !PeopleSubjectState_CaptureGameplayTuning(
                context, attackerAttributeName, &attackerTuning) ||
            attackerTuning.projectile[0] == 0)
            continue;

        for (std::size_t targetIndex = 0;
             targetIndex < missionObjects.size(); ++targetIndex)
        {
            if (targetIndex == attackerIndex)
                continue;
            People *target = ResolvePeople(context, missionObjects[targetIndex]);
            if (!RuntimeReady(target) || target->m_commanderID.isNUL() ||
                target->m_commanderID == attacker->m_commanderID ||
                target->m_damage <= 0.0)
                continue;
            ++summary->hostilePairs;

            const char *targetName =
                context->searchObject(missionObjects[targetIndex]);
            const char *attackerName =
                context->searchObject(missionObjects[attackerIndex]);
            const char *targetAttributeName =
                context->searchObject(target->m_peopleAttrID);
            ct_Attribute *targetAttribute =
                ResolvePeopleAttribute(context, targetAttributeName);
            if (targetAttribute == NULL)
                continue;

            // Recruit.Robots creates hostile Flyers and allied Robots.  Prefer
            // that authored first-mission pairing while remaining usable for
            // another public mission with the same commander topology.
            int score = 0;
            if (targetName != NULL && std::strstr(targetName, "Robot") != NULL)
                score += 1000;
            if (targetName != NULL &&
                std::strstr(targetName, "R01.Friend") != NULL)
                score += 500;
            if (attackerName != NULL &&
                std::strstr(attackerName, "Enemy") != NULL)
                score += 100;
            const int attackerOnLand =
                attackerAttribute->get_int("m_onLand");
            const int targetOnLand = targetAttribute->get_int("m_onLand");
            if (attackerOnLand == 0)
                score += 20;
            if (targetOnLand != 0)
                score += 10;
            if (score > selectedScore)
            {
                selectedScore = score;
                selectedAttacker = attacker;
                selectedTarget = target;
                selectedAttackerID = missionObjects[attackerIndex];
                selectedTargetID = missionObjects[targetIndex];
            }
        }
    }
    if (selectedAttacker == NULL || selectedTarget == NULL)
        return false;

    const char *attackerName = context->searchObject(selectedAttackerID);
    const char *targetName = context->searchObject(selectedTargetID);
    CopyTelemetryName(summary->attacker, sizeof(summary->attacker),
                      attackerName);
    CopyTelemetryName(summary->target, sizeof(summary->target), targetName);
    CopyTelemetryName(summary->attackerCommander,
                      sizeof(summary->attackerCommander),
                      context->searchObject(selectedAttacker->m_commanderID));
    CopyTelemetryName(summary->targetCommander,
                      sizeof(summary->targetCommander),
                      context->searchObject(selectedTarget->m_commanderID));

    // The first Robot contract authors its hostile Flyers with a long start
    // delay.  Acceptance deliberately advances one of those real owners into
    // the bounded encounter before asking for IDynamicObject.
    selectedAttacker->m_isNotCreate = 0;
    selectedTarget->m_isNotCreate = 0;

    IDynamicObject *attackerDynamic = static_cast<IDynamicObject *>(
        context->queryInterface(selectedAttackerID, IDynamicObjectIID));
    IDynamicObject *targetDynamic = static_cast<IDynamicObject *>(
        context->queryInterface(selectedTargetID, IDynamicObjectIID));
    const char *attackerAttributeName =
        context->searchObject(selectedAttacker->m_peopleAttrID);
    const char *targetAttributeName =
        context->searchObject(selectedTarget->m_peopleAttrID);
    ct_Attribute *attackerAttribute =
        ResolvePeopleAttribute(context, attackerAttributeName);
    ct_Attribute *targetAttribute =
        ResolvePeopleAttribute(context, targetAttributeName);
    if (attackerDynamic == NULL || targetDynamic == NULL ||
        attackerAttribute == NULL || targetAttribute == NULL)
        return false;

    const CFVector3 targetCenter = targetDynamic->getPos();
    const CFVector3 attackerOffset =
        attackerDynamic->getPos() - selectedAttacker->getPosition();
    if (!FiniteVector(targetCenter) || !FiniteVector(attackerOffset))
        return false;

    // Keep the real mission Robot at its authored ground location and bring
    // the hostile mission Flyer into a short, unobstructed firing lane.  A
    // continuation checkpoint owned by the caller rolls this setup back.
    const CFVector3 desiredAttackerCenter =
        targetCenter + CFVector3(-48.0, 12.0, 0.0);
    selectedAttacker->ct_Subject::setPosition(
        desiredAttackerCenter - attackerOffset);
    const CFVector3 stagedAttackerCenter = attackerDynamic->getPos();
    const CFVector3 toTarget = targetCenter - stagedAttackerCenter;
    const double separation = Abs(toTarget);
    if (!FiniteVector(stagedAttackerCenter) || !std::isfinite(separation) ||
        separation <= 1e-6 || separation >= 190.0)
        return false;

    RemovePeopleSchedulerEvents(context, selectedAttackerID);
    RemovePeopleSchedulerEvents(context, selectedTargetID);
    selectedAttacker->initState();
    selectedTarget->initState();
    selectedAttacker->m_isNotCreate = 0;
    selectedTarget->m_isNotCreate = 0;
    selectedAttacker->m_isVisible = 1;
    selectedTarget->m_isVisible = 1;
    selectedAttacker->m_killed = KILL_NONE;
    selectedTarget->m_killed = KILL_NONE;
    selectedAttacker->m_deleted = 0;
    selectedTarget->m_deleted = 0;
    selectedAttacker->m_stoped = 0;
    selectedTarget->m_stoped = 1;
    selectedAttacker->m_isClz = 0;
    selectedTarget->m_isClz = 0;
    selectedAttacker->m_prevTime = timeStamp;
    selectedTarget->m_prevTime = timeStamp;
    selectedAttacker->m_prevShootTime = timeStamp - 1000.0;
    selectedTarget->m_prevShootTime = timeStamp + 1000.0;
    selectedAttacker->m_lastMoveTimeStamp = timeStamp;
    selectedTarget->m_lastMoveTimeStamp = timeStamp;
    selectedTarget->m_nextNode = stagedAttackerCenter;

    CFMatrix3x4 muzzleMatrix;
    selectedAttacker->getMatrix(muzzleMatrix);
    muzzleMatrix.TranslateR(CFVector3(
        attackerAttribute->get_double("m_cannonX"),
        attackerAttribute->get_double("m_cannonY"),
        attackerAttribute->get_double("m_cannonZ")))
        .TranslateL(selectedAttacker->getPosition());
    const CFVector3 muzzle = muzzleMatrix.Offset();
    const CFVector3 aim = Normal(targetCenter - muzzle);
    const double attackerSpeed = selectedAttacker->movementSpeed();
    selectedAttacker->m_dir = aim * attackerSpeed;
    // NO_LAND recalculates its direction from nextNode immediately before
    // onShoot.  Aim that node along the muzzle-to-target ray so the authored
    // cannon offset cannot turn a threshold-valid shot into a clean miss.
    selectedAttacker->m_nextNode =
        selectedAttacker->getPosition() + aim * 1000.0;
    selectedAttacker->m_hAngle = atan2(aim.z, aim.x);
    selectedAttacker->m_rotateOy =
        attackerAttribute->get_double("m_addRoll") -
        1.57079632679489661923 -
        selectedAttacker->m_hAngle;
    selectedAttacker->m_rotateOx =
        atan2(aim.y, hypot(aim.x, aim.z));
    selectedTarget->m_dir = CFVector3(0.0, 0.0, 0.0);

    summary->targetDamageBefore = selectedTarget->m_damage;
    selectedTarget->m_damage = (std::min)(selectedTarget->m_damage, 0.01);
    if (selectedTarget->m_damage <= 0.0)
        selectedTarget->m_damage = 0.01;
    summary->targetDamageStaged = selectedTarget->m_damage;
    summary->attackerViewDistanceBefore =
        attackerAttribute->get_double("m_viewDist");
    summary->attackerViewDistanceStaged = separation + 3.0;
    attackerAttribute->set_double(
        "m_viewDist", summary->attackerViewDistanceStaged);
    CopyTelemetryName(summary->attackerAttribute,
                      sizeof(summary->attackerAttribute),
                      attackerAttributeName);

    const double findTime = timeStamp + 0.05;
    const double moveTime = timeStamp + 0.06;
    context->addEvent(KR_Event(pe_EVC_FIND_ENEMY, findTime,
                               selectedAttackerID, selectedAttackerID));
    context->addEvent(KR_Event(pe_EVC_MOVE, moveTime,
                               selectedAttackerID, selectedAttackerID));

    summary->attackerOnLand = attackerAttribute->get_int("m_onLand");
    summary->targetOnLand = targetAttribute->get_int("m_onLand");
    summary->attackerID = selectedAttackerID;
    summary->targetID = selectedTargetID;
    summary->separation = separation;
    summary->timeStamp = timeStamp;
    summary->staged = 1;
    return true;
}

bool PeopleSubjectState_InspectMissionCombat(
    SimulationContext *context,
    const SPeopleMissionCombatStageSummary *stage,
    SPeopleMissionCombatLiveState *state)
{
    if (context == NULL || stage == NULL || state == NULL ||
        stage->staged == 0)
        return false;
    std::memset(state, 0, sizeof(*state));
    People *attacker = ResolvePeople(context, stage->attackerID);
    People *target = ResolvePeople(context, stage->targetID);
    state->attackerExists = attacker != NULL ? 1 : 0;
    state->targetExists = target != NULL ? 1 : 0;
    if (attacker != NULL)
    {
        state->attackerAttackState =
            attacker->getState() == pe_STATE_ATTACK ? 1 : 0;
        state->attackerHasExactTarget =
            attacker->getEnemyID() == stage->targetID ? 1 : 0;
        state->attackerShot =
            attacker->m_prevShootTime > stage->timeStamp + 0.01 ? 1 : 0;
        CopyTelemetryName(state->attackerTarget,
                          sizeof(state->attackerTarget),
                          context->searchObject(attacker->getEnemyID()));
    }
    if (target != NULL)
    {
        state->targetAttackState =
            target->getState() == pe_STATE_ATTACK ? 1 : 0;
        state->targetDamageSourceAttacker =
            target->getEnemyID() == stage->attackerID ? 1 : 0;
        state->targetKilled = target->m_killed != KILL_NONE ? 1 : 0;
        state->targetDamage = target->m_damage;
    }
    state->bullets = BulletSubjectState_LiveCount();
    state->explosions = ExplosionSubjectState_LiveCount();
    state->corpses = CorpseSubjectState_LiveCount();
    return state->bullets >= 0 && state->explosions >= 0 &&
           state->corpses >= 0;
}

bool PeopleSubjectState_RestoreMissionCombatTuning(
    SimulationContext *context,
    const SPeopleMissionCombatStageSummary *stage)
{
    if (context == NULL || stage == NULL || stage->staged == 0 ||
        stage->attackerAttribute[0] == 0 ||
        !std::isfinite(stage->attackerViewDistanceBefore))
        return false;
    ct_Attribute *attribute = ResolvePeopleAttribute(
        context, stage->attackerAttribute);
    if (attribute == NULL)
        return false;
    attribute->set_double("m_viewDist",
                          stage->attackerViewDistanceBefore);
    return std::fabs(attribute->get_double("m_viewDist") -
                     stage->attackerViewDistanceBefore) <= 1e-9;
}

bool PeopleSubjectState_ScheduleMissionCombatDeath(
    SimulationContext *context,
    const SPeopleMissionCombatStageSummary *stage,
    double timeStamp)
{
    if (context == NULL || stage == NULL || stage->staged == 0 ||
        !std::isfinite(timeStamp))
        return false;
    People *target = ResolvePeople(context, stage->targetID);
    if (target == NULL || target->m_killed == KILL_NONE ||
        target->getEnemyID() != stage->attackerID)
        return false;
    while (context->removeEvent(pe_EVC_MOVE, stage->targetID) == 1) {}
    target->m_prevTime = timeStamp;
    context->addEvent(KR_Event(pe_EVC_MOVE, timeStamp + 0.01,
                               stage->targetID, stage->targetID));
    return true;
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

unsigned long long PeopleSubjectState_GameplayFingerprint(
    SimulationContext *context)
{
    ObjectRoster roster = {};
    if (!CollectTable(context, "PeopleAttr", roster))
        return 0;
    unsigned long long hash = kHashOffset;
    HashBytes(hash, &g_attributeCapacity, sizeof(g_attributeCapacity));
    for (std::size_t index = 0; index < roster.ids.size(); ++index)
    {
        const char *name = context->searchObject(roster.ids[index]);
        ct_Attribute *attribute = ResolvePeopleAttribute(context, name);
        if (name == NULL || attribute == NULL)
            return 0;
        const double movementSpeed = attribute->get_double("m_speed");
        const double initialHealth = attribute->get_double("m_initialDamage");
        const double fireInterval = attribute->get_double("m_cannonSpeed");
        const int burstCount = attribute->get_int("m_burstCount");
        const char *projectile = attribute->get_str("m_bulletAttrName");
        HashString(hash, name);
        HashBytes(hash, &movementSpeed, sizeof(movementSpeed));
        HashBytes(hash, &initialHealth, sizeof(initialHealth));
        HashBytes(hash, &fireInterval, sizeof(fireInterval));
        HashBytes(hash, &burstCount, sizeof(burstCount));
        HashString(hash, projectile);
    }
    return hash == 0 ? 1 : hash;
}

bool PeopleSubjectState_CaptureGameplayTuning(
    SimulationContext *context, const char *id,
    SPeopleGameplayTuningState *state)
{
    if (state == NULL)
        return false;
    std::memset(state, 0, sizeof(*state));
    ct_Attribute *attribute = ResolvePeopleAttribute(context, id);
    if (attribute == NULL || std::strlen(id) >= sizeof(state->id))
        return false;
    state->owner = attribute;
    std::strncpy(state->id, id, sizeof(state->id) - 1);
    state->movementSpeed = attribute->get_double("m_speed");
    state->initialHealth = attribute->get_double("m_initialDamage");
    state->fireInterval = attribute->get_double("m_cannonSpeed");
    state->burstCount = attribute->get_int("m_burstCount");
    const char *projectile = attribute->get_str("m_bulletAttrName");
    if (projectile == NULL ||
        std::strlen(projectile) >= sizeof(state->projectile))
        return false;
    std::strncpy(state->projectile, projectile,
                 sizeof(state->projectile) - 1);
    return std::isfinite(state->movementSpeed) &&
           std::isfinite(state->initialHealth) &&
           std::isfinite(state->fireInterval) && state->movementSpeed > 0.0 &&
           state->initialHealth > 0.0 && state->fireInterval >= 0.0 &&
           state->burstCount >= 0;
}

bool PeopleSubjectState_ApplyGameplayTuning(
    SimulationContext *context, const SPeopleGameplayTuningState *state,
    const SPeopleGameplayTuningPatch *patch)
{
    if (state == NULL || patch == NULL || state->owner == NULL)
        return false;
    ct_Attribute *attribute = ResolvePeopleAttribute(context, state->id);
    if (attribute == NULL || attribute != state->owner)
        return false;
    if (patch->hasMovementSpeed)
        attribute->set_double("m_speed", patch->movementSpeed);
    if (patch->hasInitialHealth)
        attribute->set_double("m_initialDamage", patch->initialHealth);
    if (patch->hasFireInterval)
        attribute->set_double("m_cannonSpeed", patch->fireInterval);
    if (patch->hasBurstCount)
        attribute->set_int("m_burstCount", patch->burstCount);
    if (patch->hasProjectile)
        attribute->set_str("m_bulletAttrName", patch->projectile);
    return true;
}

bool PeopleSubjectState_RestoreGameplayTuning(
    SimulationContext *context, const SPeopleGameplayTuningState *state)
{
    if (state == NULL || state->owner == NULL)
        return false;
    ct_Attribute *attribute = ResolvePeopleAttribute(context, state->id);
    if (attribute == NULL || attribute != state->owner)
        return false;
    attribute->set_double("m_speed", state->movementSpeed);
    attribute->set_double("m_initialDamage", state->initialHealth);
    attribute->set_double("m_cannonSpeed", state->fireInterval);
    attribute->set_int("m_burstCount", state->burstCount);
    attribute->set_str("m_bulletAttrName", state->projectile);
    return true;
}

static bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

static bool SameVector(const CFVector3 &left, const CFVector3 &right,
                double tolerance = 1e-8)
{
    return FiniteVector(left) && FiniteVector(right) &&
           Abs2(left - right) <= tolerance * tolerance;
}

static bool RenderPeoplePose(People *people, double timeStamp,
                             CFVector3 *offset)
{
    if (people == NULL || offset == NULL || !std::isfinite(timeStamp))
        return false;
    CViewDynamicList frame;
    people->render(frame, timeStamp);
    const bool linked = frame.Contains(&people->m_viewDynObj);
    *offset = people->m_skin.GetDir().Offset();
    frame.Clear(FALSE);
    return linked && FiniteVector(*offset);
}

static bool ProbePeoplePresentation(People *people, double timeStamp,
                                    int *renderedFrames,
                                    int *boundaryResets)
{
    if (people == NULL || renderedFrames == NULL || boundaryResets == NULL ||
        !std::isfinite(timeStamp))
        return false;
    *renderedFrames = 0;
    *boundaryResets = 0;

    const PeopleData saved = *static_cast<PeopleData *>(people);
    const CFVector3 savedPosition = people->getPosition();
    const double savedMoveTime = people->m_lastMoveTimeStamp;
    const int savedVisible = people->m_isVisible;
    const CFMatrix3x4 savedMatrix = people->m_skin.GetDir();
    const CFVector3 displacement(4.0, 1.0, -2.0);
    const double sampleInterval = 0.1;
    CFVector3 baseline;
    CFVector3 interpolated;
    CFVector3 stale;
    CFVector3 reentered;

    people->m_lastMovePos = savedPosition;
    people->m_lastMoveTimeStamp = timeStamp;
    people->m_lastMoveDeltaT = 0.0;
    bool valid = RenderPeoplePose(people, timeStamp, &baseline);
    if (valid) ++*renderedFrames;

    people->m_lastMovePos = savedPosition - displacement;
    people->m_lastMoveTimeStamp = timeStamp;
    people->m_lastMoveDeltaT = sampleInterval;
    valid = valid && RenderPeoplePose(
        people, timeStamp + sampleInterval * 0.5, &interpolated);
    if (valid) ++*renderedFrames;
    valid = valid && SameVector(interpolated - baseline,
                                displacement * 0.5);

    valid = valid && RenderPeoplePose(
        people, timeStamp + sampleInterval * 10.0, &stale);
    if (valid) ++*renderedFrames;
    valid = valid && SameVector(stale - baseline, displacement);

    people->m_isVisible = 0;
    people->onHide(timeStamp + sampleInterval * 11.0);
    people->m_isVisible = 1;
    people->onView(timeStamp + sampleInterval * 12.0);
    valid = valid && people->m_lastMoveDeltaT == 0.0 &&
            SameVector(people->m_lastMovePos, savedPosition);
    if (valid) *boundaryResets = 1;
    valid = valid && RenderPeoplePose(
        people, timeStamp + sampleInterval * 20.0, &reentered);
    if (valid) ++*renderedFrames;
    valid = valid && SameVector(reentered, baseline) &&
            SameVector(people->getPosition(), savedPosition);

    *static_cast<PeopleData *>(people) = saved;
    people->ct_Subject::setPosition(savedPosition);
    people->m_lastMoveTimeStamp = savedMoveTime;
    people->m_isVisible = savedVisible;
    people->m_skin.GetDirModify() = savedMatrix;
    return valid && *renderedFrames == 4 && *boundaryResets == 1;
}

static bool ProbePeopleLifecycle(
    SimulationContext *context, const char *requestedAttribute,
    const char *expectedProjectile,
    double timeStamp,
    SPeopleLifecycleProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const int baselineCount = PeopleSubjectState_LiveCount(context);
    const int baselineSounds = SoundObjectState_LiveCount();
    const int baselineBullets = BulletSubjectState_LiveCount();
    const unsigned long long baselineFingerprint =
        PeopleSubjectState_SubjectFingerprint(context);
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID : g_arena.searchSeanceClassTable("People");
    if (context == NULL || table == ct_NULLID || baselineCount <= 0 ||
        (expectedProjectile != NULL && baselineBullets != 0) ||
        baselineCount >= g_subjectCapacity || baselineFingerprint == 0)
        return false;

    ProbeExemplar selection = {context, KR_ObjectID::NUL(), DBL_MAX};
    g_arena.userFind(table, CaptureProbeExemplar, &selection);
    const KR_ObjectID exemplarID = selection.id;
    People *exemplar = ResolvePeople(context, exemplarID);
    if (!RuntimeReady(exemplar))
        return false;
    KR_ObjectID attribute = exemplar->m_peopleAttrID;
    SPeopleGameplayTuningState requestedState = {};
    if (requestedAttribute != NULL)
    {
        if (!PeopleSubjectState_CaptureGameplayTuning(
                context, requestedAttribute, &requestedState))
            return false;
        attribute = context->searchObject(requestedAttribute);
    }
    const char *routeName = context->searchObject(exemplar->m_routeID);

    KR_ObjectID probeID = g_arena.newObject(table, "People.Lifecycle.Probe");
    People *probe = ResolvePeople(context, probeID);
    bool valid = SendStart(probe, attribute, routeName, timeStamp);
    if (valid && requestedAttribute != NULL)
        valid = probe->m_peopleAttrID == attribute &&
                std::fabs(probe->movementSpeed() -
                          requestedState.movementSpeed) <= 1e-9 &&
                std::fabs(probe->m_damage - requestedState.initialHealth) <=
                    1e-9;
    if (valid && expectedProjectile != NULL)
    {
        const ct_ClassTableID bulletAttributes =
            g_arena.searchSeanceClassTable("BulletAttr");
        const ct_ClassTableID bullets =
            g_arena.searchSeanceClassTable("Bullet");
        KR_ObjectID expected = context->searchObject(expectedProjectile);
        const int expectedIndex = expected.isNUL() ||
            bulletAttributes == ct_NULLID ? -1 :
            g_arena.getAttributeIndex(bulletAttributes, expected);
        valid = requestedState.projectile[0] != 0 &&
                std::strcmp(requestedState.projectile,
                            expectedProjectile) == 0 &&
                bullets != ct_NULLID && expectedIndex >= 0 &&
                probe->isShooter();
        if (valid)
        {
            summary->projectileReferenceReady = 1;
            shoot(probeID, bullets, expectedIndex, probe->getPos(),
                  CFVector3(1.0, 0.0, 0.0), context,
                  timeStamp + 0.005);
            if (BulletSubjectState_LiveCount() == baselineBullets + 1)
                summary->outgoingProjectileStarts = 1;
            RemoveAllSubjects(context, "Bullet");
        }
    }
    if (valid)
    {
        IRouteObject *probeRoute = static_cast<IRouteObject *>(
            context->queryInterface(probe->m_routeID, IRouteObjectIID));
        const int routeNodeCount = probeRoute == NULL
            ? 0 : probeRoute->GetNodeCnt();
        summary->validStarts =
            probe->m_startBackSpaceNode == 7 &&
            probe->m_startMoveDelay == 1.25 &&
            Abs2(probe->m_dir) == 0.0 ? 1 : 0;
        summary->routePhaseExact = probeRoute != NULL &&
            routeNodeCount >= 2 && probe->m_previousRouteNode == 0 &&
            probe->m_curNode == 1 &&
            SameVector(probe->getPosition(), probeRoute->GetNode(0)) &&
            SameVector(probe->m_nextNode, probeRoute->GetNode(1)) ? 1 : 0;

        CFVector3 projected(500.0, 0.0, 1000.0);
        g_toSeg(projected, CFVector3(0.0, 0.0, 0.0),
                CFVector3(1000.0, 0.0, 0.0), 10.0);
        const double projectedDistance = g_distToSeg(
            projected, CFVector3(0.0, 0.0, 0.0),
            CFVector3(1000.0, 0.0, 0.0));
        const PeopleData routeMotionState =
            *static_cast<PeopleData *>(probe);
        const CFVector3 routeMotionPosition = probe->getPosition();
        const int routeMotionPrevious = probe->m_previousRouteNode;
        bool routeMotionIntegrated = false;
        bool routeDeviationRecovery = false;
        if (routeNodeCount >= 2 && routeMotionPrevious >= 0 &&
            routeMotionPrevious < routeNodeCount && probe->m_curNode >= 0 &&
            probe->m_curNode < routeNodeCount)
        {
            const CFVector3 start = probeRoute->GetNode(routeMotionPrevious);
            const CFVector3 target = probeRoute->GetNode(probe->m_curNode);
            CFVector3 overshoot = target + (target - start);
            if (Abs2(target - start) <= 1e-12)
                overshoot = target + CFVector3(1.0, 0.0, 0.0);
            probe->setPosition(overshoot);
            routeMotionIntegrated =
                probe->m_previousRouteNode != routeMotionPrevious &&
                probe->m_curNode >= 0 && probe->m_curNode < routeNodeCount &&
                SameVector(probe->m_nextNode,
                           probeRoute->GetNode(probe->m_curNode)) &&
                SameVector(probe->m_stateNextNode[0], probe->m_nextNode) &&
                FiniteVector(probe->getPosition());
            context->removeEvent(pe_EVC_NEXTNODE, probeID);
            *static_cast<PeopleData *>(probe) = routeMotionState;
            probe->ct_Subject::setPosition(routeMotionPosition);

            const CFVector3 segment = target - start;
            CFVector3 perpendicular;
            const double horizontalLength = hypot(segment.x, segment.z);
            if (horizontalLength > 1e-9)
                perpendicular = CFVector3(-segment.z / horizontalLength,
                                          0.0,
                                          segment.x / horizontalLength);
            else
                perpendicular = CFVector3(1.0, 0.0, 0.0);
            if (probe->m_stateSP < PeopleData::MAX_STATE)
            {
                const int stateDepth = probe->m_stateSP;
                probe->pushState(pe_STATE_ATTACK, probeID);
                probe->m_routeDeviationTime = 2.4;
                probe->setMovingPosition(
                    routeMotionPosition + perpendicular * 1000000.0, 0.2);
                routeDeviationRecovery =
                    probe->m_stateSP == stateDepth &&
                    std::fabs(probe->m_routeDeviationTime) <= 1e-9 &&
                    FiniteVector(probe->getPosition());
                while (context->removeEvent(pe_EVC_NEXTNODE, probeID) == 1) {}
                *static_cast<PeopleData *>(probe) = routeMotionState;
                probe->ct_Subject::setPosition(routeMotionPosition);
            }
        }
        summary->corridorProjection = PeopleRouteMotion_Probe() &&
            routeMotionIntegrated &&
            routeDeviationRecovery &&
            FiniteVector(projected) &&
            std::isfinite(projectedDistance) &&
            std::fabs(projected.x - 500.0) <= 1e-9 &&
            std::fabs(projected.z - 10.0) <= 1e-9 &&
            std::fabs(projectedDistance - 10.0) <= 1e-9 ? 1 : 0;
        summary->obstacleRecovery = PeopleObstacleRecovery_Probe() ? 1 : 0;
        summary->contactResponse = PeopleContactResponse_Probe() ? 1 : 0;
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
        KR_Event initialMove[2];
        const int initialMoveCount =
            context->copyEvents(pe_EVC_MOVE, probeID, initialMove, 2);
        const double initialMoveTime = initialMoveCount == 1
            ? initialMove[0].timeStamp : 0.0;
        const bool moveScheduled = initialMoveCount == 1 &&
            context->removeEvent(pe_EVC_MOVE, probeID) == 1;
        KR_Event routeEvents[2];
        const int groundedRouteCount = context->copyEvents(
            pe_EVC_GROUNDED_NEXTNODE, probeID, routeEvents, 2);
        KR_Event airRouteEvents[2];
        const int airRouteCount = context->copyEvents(
            pe_EVC_NEXTNODE, probeID, airRouteEvents, 2);
        const bool groundedCadenceScheduled =
            groundedRouteCount == 0 ||
            (groundedRouteCount == 1 &&
             std::fabs(routeEvents[0].timeStamp - startMove.timeStamp) <= 1e-9);
        bool groundedCadenceExact = groundedRouteCount == 0;
        if (groundedRouteCount == 1)
        {
            const PeopleData cadenceState =
                *static_cast<PeopleData *>(probe);
            const CFVector3 cadencePosition = probe->getPosition();
            const int stateDepth = probe->m_stateSP;
            const double cadenceTime = routeEvents[0].timeStamp;
            context->removeEvent(pe_EVC_GROUNDED_NEXTNODE, probeID);
            probe->m_isVisible = 1;
            const bool executed = probe->receiveEvent(routeEvents[0]) == 1;
            KR_Event repeated[2];
            const int repeatedCount = context->copyEvents(
                pe_EVC_GROUNDED_NEXTNODE, probeID, repeated, 2);
            groundedCadenceExact = executed && stateDepth > 0 &&
                probe->m_stateSP == stateDepth - 1 && repeatedCount == 1 &&
                std::fabs(repeated[0].timeStamp -
                          cadenceTime - 0.3) <= 1e-9;
            context->removeEvent(pe_EVC_GROUNDED_NEXTNODE, probeID);
            *static_cast<PeopleData *>(probe) = cadenceState;
            probe->ct_Subject::setPosition(cadencePosition);
        }
        // NEXTNODE remains the normal-cadence bridge while the recovered May
        // route kernel handles true movement overshoot.  The two speculative
        // MOVE branches below must each start from the same empty route-event
        // queue or their generated replacement deadlines would accumulate.
        const bool nextNodeScheduled =
            airRouteCount == 1 && groundedCadenceScheduled &&
            groundedCadenceExact;
        const int routeEventLabel = pe_EVC_NEXTNODE;
        if (groundedRouteCount != 0)
            context->removeEvent(pe_EVC_GROUNDED_NEXTNODE, probeID);
        if (airRouteCount != 0)
            context->removeEvent(pe_EVC_NEXTNODE, probeID);
        bool hiddenMove = false;
        bool visibleMove = false;
        double hiddenNextTime = 0.0;
        double visibleNextTime = 0.0;
        if (movementStarted && moveScheduled)
        {
            const PeopleData cadenceState =
                *static_cast<PeopleData *>(probe);
            const CFVector3 cadencePosition = probe->getPosition();
            const double cadenceMoveTime = probe->m_lastMoveTimeStamp;
            const int cadenceVisible = probe->m_isVisible;
            KR_Event execute;
            execute.getCopy(initialMove[0]);
            probe->m_isVisible = 0;
            hiddenMove = probe->receiveEvent(execute) == 1;
            KR_Event hiddenNext[2];
            hiddenMove = hiddenMove &&
                context->copyEvents(pe_EVC_MOVE, probeID, hiddenNext, 2) == 1;
            if (hiddenMove) hiddenNextTime = hiddenNext[0].timeStamp;
            context->removeEvent(pe_EVC_MOVE, probeID);
            while (context->removeEvent(pe_EVC_NEXTNODE, probeID) == 1) {}

            *static_cast<PeopleData *>(probe) = cadenceState;
            probe->ct_Subject::setPosition(cadencePosition);
            probe->m_lastMoveTimeStamp = cadenceMoveTime;
            probe->m_isVisible = 1;
            execute.getCopy(initialMove[0]);
            visibleMove = probe->receiveEvent(execute) == 1;
            KR_Event visibleNext[2];
            visibleMove = visibleMove &&
                context->copyEvents(pe_EVC_MOVE, probeID, visibleNext, 2) == 1;
            if (visibleMove) visibleNextTime = visibleNext[0].timeStamp;
            context->removeEvent(pe_EVC_MOVE, probeID);
            while (context->removeEvent(pe_EVC_NEXTNODE, probeID) == 1) {}

            *static_cast<PeopleData *>(probe) = cadenceState;
            probe->ct_Subject::setPosition(cadencePosition);
            probe->m_lastMoveTimeStamp = cadenceMoveTime;
            probe->m_isVisible = cadenceVisible;
        }
        const double cadenceInterval = hiddenNextTime - initialMoveTime;
        summary->cadenceBounded = hiddenMove && visibleMove &&
            std::isfinite(cadenceInterval) && probe->m_calcPosInc > 0.0 &&
            cadenceInterval >= probe->m_calcPosInc * 0.2 - 1e-9 &&
            cadenceInterval <= probe->m_calcPosInc * 2.0 + 1e-9 &&
            std::fabs(hiddenNextTime - visibleNextTime) <= 1e-9 ? 1 : 0;
        summary->scheduledMoves = movementStarted && moveScheduled &&
            nextNodeScheduled && hiddenMove && visibleMove ? 1 : 0;

        if (movementStarted && nextNodeScheduled && routeNodeCount >= 2)
        {
            const PeopleData routeState =
                *static_cast<PeopleData *>(probe);
            const CFVector3 routePosition = probe->getPosition();
            const int finalNode = routeNodeCount - 1;
            const auto probeEndPolicy = [&](int backSpace,
                                            int expectedNode,
                                            bool expectEvent,
                                            bool expectStopped) {
                while (context->removeEvent(routeEventLabel, probeID) == 1) {}
                *static_cast<PeopleData *>(probe) = routeState;
                probe->ct_Subject::setPosition(probeRoute->GetNode(finalNode));
                probe->m_previousRouteNode = finalNode - 1;
                probe->m_curNode = finalNode;
                probe->m_nextNode = probeRoute->GetNode(finalNode);
                probe->m_startBackSpaceNode = backSpace;
                probe->m_dir = CFVector3(1.0, 0.0, 0.0);
                KR_Event advance(routeEventLabel, timeStamp + 4.0,
                                 probeID, probeID);
                probe->nexNodeDefault(advance);
                KR_Event queued[2];
                const int queuedCount = context->copyEvents(
                    routeEventLabel, probeID, queued, 2);
                const bool result = probe->m_curNode == expectedNode &&
                    (queuedCount == 1) == expectEvent &&
                    (Abs2(probe->m_dir) == 0.0) == expectStopped;
                while (context->removeEvent(routeEventLabel, probeID) == 1) {}
                return result;
            };
            const bool loops = probeEndPolicy(-1, 0, true, false);
            const bool stops = probeEndPolicy(0, finalNode, false, true);
            const bool rewinds = probeEndPolicy(
                1, routeNodeCount > 2 ? routeNodeCount - 2 : 0,
                true, false);
            summary->routeEndPolicies = loops && stops && rewinds ? 1 : 0;
            *static_cast<PeopleData *>(probe) = routeState;
            probe->ct_Subject::setPosition(routePosition);
        }

        ProbePeoplePresentation(probe, timeStamp + 2.0,
                                &summary->renderedPoseFrames,
                                &summary->viewBoundaryResets);

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

    const bool projectileProof = expectedProjectile == NULL ||
        (summary->projectileReferenceReady == 1 &&
         summary->outgoingProjectileStarts == 1 &&
         BulletSubjectState_LiveCount() == baselineBullets);
    return valid && projectileProof && summary->validStarts == 1 &&
           summary->routePhaseExact == 1 &&
           summary->routeEndPolicies == 1 &&
           summary->corridorProjection == 1 &&
           summary->obstacleRecovery == 1 &&
           summary->contactResponse == 1 &&
           summary->dynamicReady == 1 && summary->renderReady == 1 &&
           summary->scheduledMoves == 1 &&
           summary->cadenceBounded == 1 &&
           summary->renderedPoseFrames == 4 &&
           summary->viewBoundaryResets == 1 &&
           summary->bulletDamageApplications == 1 &&
           summary->deathTransitions == 1 &&
           summary->saveStateRoundTrips == 1 && summary->rollbacks == 1;
}

bool PeopleSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    SPeopleLifecycleProbeSummary *summary)
{
    return ProbePeopleLifecycle(context, NULL, NULL, timeStamp, summary);
}

bool PeopleSubjectState_ProbeAttributeLifecycle(
    SimulationContext *context, const char *attributeName, double timeStamp,
    SPeopleLifecycleProbeSummary *summary)
{
    return attributeName != NULL && attributeName[0] != 0 &&
           ProbePeopleLifecycle(context, attributeName, NULL, timeStamp,
                                summary);
}

bool PeopleSubjectState_ProbeTunedAttributeLifecycle(
    SimulationContext *context, const char *attributeName,
    const char *expectedProjectile, double timeStamp,
    SPeopleLifecycleProbeSummary *summary)
{
    return attributeName != NULL && attributeName[0] != 0 &&
           expectedProjectile != NULL && expectedProjectile[0] != 0 &&
           ProbePeopleLifecycle(context, attributeName, expectedProjectile,
                                timeStamp, summary);
}

bool PeopleSubjectState_ProbeNewestDelayedRoute(
    SimulationContext *context,
    SPeopleRouteMotionProbeSummary *summary)
{
    if (context == NULL || summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));

    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return false;
    People *selected = NULL;
    KR_ObjectID selectedID = KR_ObjectID::NUL();
    for (std::size_t index = 0; index < roster.ids.size(); ++index)
    {
        People *candidate = ResolvePeople(context, roster.ids[index]);
        KR_Event pending[2];
        if (RuntimeReady(candidate) && candidate->m_startMoveDelay > 0.0 &&
            context->copyEvents(pe_EV_STARTMOVE, roster.ids[index],
                                pending, 2) == 1 &&
            (selected == NULL || roster.ids[index].id > selectedID.id))
        {
            selected = candidate;
            selectedID = roster.ids[index];
        }
    }
    if (selected == NULL)
        return true;

    const char *ownerName = context->searchObject(selectedID);
    std::strncpy(summary->owner, ownerName == NULL ? "" : ownerName,
                 sizeof(summary->owner) - 1);
    summary->owner[sizeof(summary->owner) - 1] = 0;
    summary->available = 1;
    summary->startNode = selected->m_previousRouteNode;
    summary->targetNode = selected->m_curNode;
    summary->backSpaceNode = selected->m_startBackSpaceNode;
    summary->startMoveDelay = selected->m_startMoveDelay;

    IRouteObject *route = static_cast<IRouteObject *>(
        context->queryInterface(selected->m_routeID, IRouteObjectIID));
    if (route == NULL || route->GetNodeCnt() < 2 ||
        summary->startNode < 0 || summary->startNode >= route->GetNodeCnt() ||
        summary->targetNode < 0 || summary->targetNode >= route->GetNodeCnt())
        return false;

    const PeopleData saved = *static_cast<PeopleData *>(selected);
    const CFVector3 savedPosition = selected->getPosition();
    const int schedulerLabels[] = {
        pe_EVC_MOVE, pe_EVC_NEXTNODE, pe_EVC_GROUNDED_NEXTNODE,
        pe_EVC_FIND_ENEMY, pe_EV_STARTSHOW, pe_EV_SETAUTOANIM,
        pe_EV_STARTMOVE};
    struct SavedSchedulerEvent
    {
        int label;
        double timeStamp;
    };
    std::vector<SavedSchedulerEvent> originalEvents;
    KR_Event startMove;
    bool foundStartMove = false;
    bool schedulerShapeValid = true;
    for (std::size_t index = 0;
         index < sizeof(schedulerLabels) / sizeof(schedulerLabels[0]);
         ++index)
    {
        KR_Event copied[2];
        const int count = context->copyEvents(
            schedulerLabels[index], selectedID, copied, 2);
        if (count < 0 || count > 1)
        {
            schedulerShapeValid = false;
            break;
        }
        if (count == 1)
        {
            SavedSchedulerEvent savedEvent = {
                schedulerLabels[index], copied[0].timeStamp};
            originalEvents.push_back(savedEvent);
            if (schedulerLabels[index] == pe_EV_STARTMOVE)
            {
                startMove.getCopy(copied[0]);
                foundStartMove = true;
            }
        }
    }

    if (!schedulerShapeValid)
        return false;
    for (std::size_t index = 0;
         index < sizeof(schedulerLabels) / sizeof(schedulerLabels[0]);
         ++index)
        context->removeEvent(schedulerLabels[index], selectedID);

    bool valid = foundStartMove;
    const CFVector3 segmentStart = route->GetNode(summary->startNode);
    const CFVector3 target = route->GetNode(summary->targetNode);
    summary->phaseExact = SameVector(savedPosition, segmentStart) &&
        SameVector(saved.m_nextNode, target) ? 1 : 0;
    const double distanceBefore = hypot(target.x - savedPosition.x,
                                        target.z - savedPosition.z);

    if (valid)
        valid = selected->receiveEvent(startMove) == 1;
    KR_Event grounded[2];
    KR_Event airborne[2];
    const int groundedCount = valid ? context->copyEvents(
        pe_EVC_GROUNDED_NEXTNODE, selectedID, grounded, 2) : 0;
    const int airborneCount = valid ? context->copyEvents(
        pe_EVC_NEXTNODE, selectedID, airborne, 2) : 0;
    summary->groundedRouteEvent = groundedCount == 1 &&
        airborneCount == 1 &&
        std::fabs(grounded[0].timeStamp - startMove.timeStamp) <= 1e-9
            ? 1 : 0;
    context->removeEvent(pe_EVC_GROUNDED_NEXTNODE, selectedID);
    context->removeEvent(pe_EVC_NEXTNODE, selectedID);

    KR_Event firstMove[2];
    int moveCount = valid ? context->copyEvents(
        pe_EVC_MOVE, selectedID, firstMove, 2) : 0;
    const double firstMoveTime = moveCount == 1
        ? firstMove[0].timeStamp : 0.0;
    valid = valid && moveCount == 1 &&
        context->removeEvent(pe_EVC_MOVE, selectedID) == 1 &&
        selected->receiveEvent(firstMove[0]) == 1;
    KR_Event secondMove[2];
    moveCount = valid ? context->copyEvents(
        pe_EVC_MOVE, selectedID, secondMove, 2) : 0;
    if (valid && moveCount == 1)
    {
        context->removeEvent(pe_EVC_MOVE, selectedID);
        summary->elapsed = secondMove[0].timeStamp - firstMoveTime;
        valid = summary->elapsed > 0.0 &&
            selected->receiveEvent(secondMove[0]) == 1;
    }
    else valid = false;

    const CFVector3 movedPosition = selected->getPosition();
    const double distanceAfter = hypot(target.x - movedPosition.x,
                                       target.z - movedPosition.z);
    const double horizontalStep = hypot(movedPosition.x - savedPosition.x,
                                        movedPosition.z - savedPosition.z);
    summary->displacement = horizontalStep;
    summary->finiteMotion = FiniteVector(movedPosition) &&
        std::isfinite(distanceAfter) && std::isfinite(horizontalStep) ? 1 : 0;
    summary->movedTowardTarget = summary->finiteMotion &&
        horizontalStep > 1e-6 && distanceAfter < distanceBefore ? 1 : 0;
    summary->boundedStep = summary->finiteMotion &&
        std::isfinite(summary->elapsed) && summary->elapsed > 0.0 &&
        horizontalStep <= selected->movementSpeed() * summary->elapsed + 1e-5
            ? 1 : 0;

    for (std::size_t index = 0;
         index < sizeof(schedulerLabels) / sizeof(schedulerLabels[0]);
         ++index)
        context->removeEvent(schedulerLabels[index], selectedID);
    *static_cast<PeopleData *>(selected) = saved;
    selected->ct_Subject::setPosition(savedPosition);
    for (std::size_t index = 0; index < originalEvents.size(); ++index)
    {
        KR_Event event(originalEvents[index].label,
                       originalEvents[index].timeStamp,
                       selectedID, selectedID);
        context->addEvent(event);
    }

    return valid && summary->phaseExact == 1 &&
           summary->groundedRouteEvent == 1 &&
           summary->finiteMotion == 1 &&
           summary->movedTowardTarget == 1 && summary->boundedStep == 1;
}

bool PeopleSubjectState_ProbeCombatLifecycle(
    SimulationContext *context, double timeStamp,
    SPeopleCombatProbeSummary *summary)
{
    if (context == NULL || summary == NULL || !std::isfinite(timeStamp))
        return false;
    std::memset(summary, 0, sizeof(*summary));

    const int baselinePeople = PeopleSubjectState_LiveCount(context);
    const int baselineSounds = SoundObjectState_LiveCount();
    const int baselineBullets = BulletSubjectState_LiveCount();
    const int baselineExplosions = ExplosionSubjectState_LiveCount();
    const int baselineCorpses = CorpseSubjectState_LiveCount();
    const int baselineSmokes = SmokeSubjectState_LiveCount();
    const unsigned long long baselineFingerprint =
        PeopleSubjectState_SubjectFingerprint(context);
    const ct_ClassTableID peopleTable =
        g_arena.searchSeanceClassTable("People");
    if (peopleTable == ct_NULLID || baselinePeople <= 0 ||
        baselineFingerprint == 0 || baselineBullets != 0 ||
        baselineExplosions != 0 || baselineCorpses != 0 ||
        baselineSmokes != 0)
        return false;

    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return false;

    People *exemplar = NULL;
    KR_ObjectID exemplarID = KR_ObjectID::NUL();
    SPeopleGameplayTuningState tuning = {};
    double selectedRouteSegment = 0.0;
    for (std::size_t index = 0; index < roster.ids.size(); ++index)
    {
        People *candidate = ResolvePeople(context, roster.ids[index]);
        const char *attributeName = candidate == NULL ? NULL :
            context->searchObject(candidate->m_peopleAttrID);
        SPeopleGameplayTuningState candidateTuning = {};
        IRouteObject *candidateRoute = candidate == NULL ? NULL :
            static_cast<IRouteObject *>(context->queryInterface(
                candidate->m_routeID, IRouteObjectIID));
        const bool routePhaseReady = candidateRoute != NULL &&
            candidate->m_previousRouteNode >= 0 &&
            candidate->m_previousRouteNode < candidateRoute->GetNodeCnt() &&
            candidate->m_curNode >= 0 &&
            candidate->m_curNode < candidateRoute->GetNodeCnt();
        const double routeSegment = routePhaseReady ? Abs(
            candidateRoute->GetNode(candidate->m_curNode) -
            candidateRoute->GetNode(candidate->m_previousRouteNode)) : 0.0;
        if (RuntimeReady(candidate) && candidate->isShooter() &&
            candidate->movementSpeed() > 1e-3 && attributeName != NULL &&
            PeopleSubjectState_CaptureGameplayTuning(
                context, attributeName, &candidateTuning) &&
            candidateTuning.projectile[0] != 0 &&
            std::isfinite(routeSegment) &&
            routeSegment > selectedRouteSegment)
        {
            exemplar = candidate;
            exemplarID = roster.ids[index];
            tuning = candidateTuning;
            selectedRouteSegment = routeSegment;
        }
    }

    // Some retail levels intentionally contain no armed People.  They are a
    // valid skip; shooter-bearing levels must execute the complete chain.
    if (exemplar == NULL)
    {
        summary->rollbacks = 1;
        return true;
    }
    summary->available = 1;
    const char *exemplarName = context->searchObject(exemplarID);
    std::strncpy(summary->attacker,
                 exemplarName == NULL ? "" : exemplarName,
                 sizeof(summary->attacker) - 1);
    std::strncpy(summary->projectile, tuning.projectile,
                 sizeof(summary->projectile) - 1);

    if (baselinePeople + 2 > g_subjectCapacity)
        return false;
    const char *routeName = context->searchObject(exemplar->m_routeID);
    if (routeName == NULL || routeName[0] == 0)
        return false;

    KR_ObjectID attackerID = g_arena.newObject(
        peopleTable, "People.Combat.Attacker.Probe");
    KR_ObjectID targetID = g_arena.newObject(
        peopleTable, "People.Combat.Target.Probe");
    People *attacker = ResolvePeople(context, attackerID);
    People *target = ResolvePeople(context, targetID);
    bool valid = SendStart(attacker, exemplar->m_peopleAttrID,
                           routeName, timeStamp) &&
                 SendStart(target, exemplar->m_peopleAttrID,
                           routeName, timeStamp);
    summary->attackerReady = valid && RuntimeReady(attacker) &&
        attacker->isShooter() ? 1 : 0;
    summary->targetReady = valid && RuntimeReady(target) ? 1 : 0;

    KR_Event showAttacker;
    KR_Event showTarget;
    if (valid)
        valid = TakePeopleEvent(context, pe_EV_STARTSHOW, attackerID,
                                &showAttacker) &&
                attacker->receiveEvent(showAttacker) == 1 &&
                TakePeopleEvent(context, pe_EV_STARTSHOW, targetID,
                                &showTarget) &&
                target->receiveEvent(showTarget) == 1;

    const CFVector3 movementStart = valid
        ? attacker->getPosition() : CFVector3();
    KR_Event startMove;
    KR_Event firstMove;
    KR_Event secondMove;
    if (valid)
        valid = TakePeopleEvent(context, pe_EV_STARTMOVE, attackerID,
                                &startMove) &&
                attacker->receiveEvent(startMove) == 1 &&
                TakePeopleEvent(context, pe_EVC_MOVE, attackerID,
                                &firstMove) &&
                attacker->receiveEvent(firstMove) == 1 &&
                TakePeopleEvent(context, pe_EVC_MOVE, attackerID,
                                &secondMove) &&
                attacker->receiveEvent(secondMove) == 1;
    if (valid)
    {
        const CFVector3 displacement = attacker->getPosition() - movementStart;
        const double distance = Abs(displacement);
        const double elapsed = secondMove.timeStamp - firstMove.timeStamp;
        summary->routeDisplacement = FiniteVector(displacement) &&
            std::isfinite(distance) && std::isfinite(elapsed) &&
            distance > 1e-6 && elapsed > 0.0 &&
            distance <= attacker->movementSpeed() * elapsed + 1e-5 ? 1 : 0;
    }

    RemovePeopleSchedulerEvents(context, attackerID);
    RemovePeopleSchedulerEvents(context, targetID);

    if (valid)
    {
        IDynamicObject *attackerDynamic = static_cast<IDynamicObject *>(
            context->queryInterface(attackerID, IDynamicObjectIID));
        IDynamicObject *targetDynamic = static_cast<IDynamicObject *>(
            context->queryInterface(targetID, IDynamicObjectIID));
        valid = attackerDynamic != NULL && targetDynamic != NULL;
        if (valid)
        {
            const CFVector3 attackerOffset =
                attackerDynamic->getPos() - attacker->getPosition();
            const CFVector3 targetOffset =
                targetDynamic->getPos() - target->getPosition();
            const CFVector3 attackerCenter(512.0, 10000.0, -512.0);
            // Keep the target well inside the retail 200-unit weapon range,
            // but far enough from the muzzle that aircraft cannon offsets do
            // not dominate the aim vector and reject an otherwise exact shot.
            const CFVector3 targetCenter(576.0, 10000.0, -512.0);
            attacker->ct_Subject::setPosition(
                attackerCenter - attackerOffset);
            target->ct_Subject::setPosition(targetCenter - targetOffset);
            attacker->m_commanderID = attackerID;
            target->m_commanderID = targetID;
            attacker->m_isVisible = 1;
            target->m_isVisible = 1;
            attacker->m_nextNode = targetDynamic->getPos();
            attacker->setHAngleFixDir(atan2(
                targetDynamic->getPos().z - attackerDynamic->getPos().z,
                targetDynamic->getPos().x - attackerDynamic->getPos().x));

            KR_Event find(pe_EVC_FIND_ENEMY, timeStamp + 10.0,
                          attackerID, attackerID);
            valid = attacker->receiveEvent(find) == 1;
            while (context->removeEvent(pe_EVC_FIND_ENEMY,
                                        attackerID) == 1) {}
            summary->targetAcquired = valid &&
                attacker->getState() == pe_STATE_ATTACK &&
                attacker->getEnemyID() == targetID ? 1 : 0;

            KR_Event cadence(pe_EVC_GROUNDED_NEXTNODE,
                             timeStamp + 10.1, attackerID, attackerID);
            if (valid && summary->targetAcquired == 1)
                valid = attacker->receiveEvent(cadence) == 1;
            KR_Event nextCadence[2];
            const int cadenceCount = valid ? context->copyEvents(
                pe_EVC_GROUNDED_NEXTNODE, attackerID,
                nextCadence, 2) : 0;
            summary->targetCadence = cadenceCount == 1 &&
                attacker->getEnemyID() == targetID &&
                SameVector(attacker->m_nextNode, targetDynamic->getPos()) &&
                std::fabs(nextCadence[0].timeStamp -
                          (timeStamp + 10.3)) <= 1e-9 ? 1 : 0;
            while (context->removeEvent(pe_EVC_GROUNDED_NEXTNODE,
                                        attackerID) == 1) {}

            attacker->setHAngleFixDir(atan2(
                targetDynamic->getPos().z - attackerDynamic->getPos().z,
                targetDynamic->getPos().x - attackerDynamic->getPos().x));
            attacker->m_prevShootTime = timeStamp - 1000.0;
            if (valid && summary->targetCadence == 1)
                attacker->onShoot(timeStamp + 10.2);
            if (BulletSubjectState_LiveCount() == baselineBullets + 1)
                summary->projectileStarted = 1;
            RemoveAllSubjects(context, "Bullet");
            RemoveAllSubjects(context, "Smoke");

            // Keep the acquired-target proof coupled to the real shooter,
            // then isolate the bounded impact execution.  Otherwise the
            // retail splash radius legitimately damages both temporary
            // People and the one-target Bullet probe rejects two consumers.
            const CFVector3 isolatedAttackerCenter(
                800.0, 10000.0, -512.0);
            attacker->ct_Subject::setPosition(
                isolatedAttackerCenter -
                (attackerDynamic->getPos() - attacker->getPosition()));

            target->m_damage = 1000.0;
            const double damageBefore = target->m_damage;
            if (summary->projectileStarted == 1 &&
                BulletSubjectState_ProbeDynamicCollisionLifecycle(
                    context, tuning.projectile, targetID,
                    timeStamp + 10.3) &&
                target->m_damage < damageBefore)
                summary->damageDelivered = 1;

            target->setDamage(target->m_damage + 1.0,
                              targetDynamic->getPos(), timeStamp + 10.4,
                              attackerID);
            if (target->m_damage <= 0.0 &&
                target->m_killed != KILL_NONE)
                summary->deathTransition = 1;
            target->crashExpl(targetDynamic->getPos(),
                              timeStamp + 10.5);
            target->crashCreateCorpse(targetDynamic->getPos(),
                                      timeStamp + 10.5);
            if (ExplosionSubjectState_LiveCount() ==
                    baselineExplosions + 1 &&
                CorpseSubjectState_LiveCount() == baselineCorpses + 1)
                summary->deathEffects = 1;
        }
    }

    RemoveAllSubjects(context, "Bullet");
    RemoveAllSubjects(context, "Explosion");
    RemoveAllSubjects(context, "Corpse");
    RemoveAllSubjects(context, "Smoke");
    RemovePeopleSchedulerEvents(context, attackerID);
    RemovePeopleSchedulerEvents(context, targetID);
    if (!targetID.isNUL() && context->isExist(targetID))
        context->removeObject(targetID);
    if (!attackerID.isNUL() && context->isExist(attackerID))
        context->removeObject(attackerID);

    if (PeopleSubjectState_LiveCount(context) == baselinePeople &&
        SoundObjectState_LiveCount() == baselineSounds &&
        BulletSubjectState_LiveCount() == baselineBullets &&
        ExplosionSubjectState_LiveCount() == baselineExplosions &&
        CorpseSubjectState_LiveCount() == baselineCorpses &&
        SmokeSubjectState_LiveCount() == baselineSmokes &&
        PeopleSubjectState_SubjectFingerprint(context) == baselineFingerprint)
        summary->rollbacks = 1;

    return valid && summary->attackerReady == 1 &&
           summary->targetReady == 1 &&
           summary->routeDisplacement == 1 &&
           summary->targetAcquired == 1 &&
           summary->targetCadence == 1 &&
           summary->projectileStarted == 1 &&
           summary->damageDelivered == 1 &&
           summary->deathTransition == 1 &&
           summary->deathEffects == 1 && summary->rollbacks == 1;
}

bool PeopleSubjectState_AuditCombatScheduling(
    SimulationContext *context,
    SPeopleCombatScheduleSummary *summary)
{
    if (context == NULL || summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    ObjectRoster roster = {};
    if (!CollectTable(context, "People", roster))
        return g_subjectCapacity == 0;
    summary->livePeople = static_cast<int>(roster.ids.size());
    for (std::size_t index = 0; index < roster.ids.size(); ++index)
    {
        People *people = ResolvePeople(context, roster.ids[index]);
        if (!RuntimeReady(people))
            return false;
        if (!people->isShooter())
            continue;
        ++summary->shooters;
        if (people->getState() == pe_STATE_ATTACK)
            ++summary->attackStates;
        if (people->m_commanderID.isNUL())
            continue;
        ++summary->commandedShooters;
        if (context->queryInterface(people->m_commanderID,
                                    ICommanderIID) != NULL)
            ++summary->commanderInterfaces;

        KR_Event findEvents[2];
        const int findCount = context->copyEvents(
            pe_EVC_FIND_ENEMY, roster.ids[index], findEvents, 2);
        if (findCount == 1)
            ++summary->scheduledFindEnemy;
        else
            ++summary->malformedQueues;

        KR_Event moveEvents[2];
        KR_Event startEvents[2];
        const int moveCount = context->copyEvents(
            pe_EVC_MOVE, roster.ids[index], moveEvents, 2);
        const int startCount = context->copyEvents(
            pe_EV_STARTMOVE, roster.ids[index], startEvents, 2);
        if ((moveCount == 1 && startCount == 0) ||
            (moveCount == 0 && startCount == 1))
            ++summary->scheduledMotion;
        else
            ++summary->malformedQueues;
    }
    // Armed ambience and scripted set pieces can deliberately have no
    // commander.  Once retail assigns a commander, however, all four pieces
    // of the live combat schedule are mandatory.
    return summary->commanderInterfaces == summary->commandedShooters &&
           summary->scheduledFindEnemy == summary->commandedShooters &&
           summary->scheduledMotion == summary->commandedShooters &&
           summary->malformedQueues == 0;
}
