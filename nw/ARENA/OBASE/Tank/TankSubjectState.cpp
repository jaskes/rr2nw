#include "TankSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "TANK.H"
#include "message/unitmsg.h"
#include "obase/bullet/BulletAttributeState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/cannon/Cannon.h"
#include "obase/cannon/CannonSubjectState.h"
#include "obase/corpse/CorpseSubjectState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "kernel/h/context.h"
#include "storage/h/savefile.h"
#include "storage/h/subject.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const unsigned long long kAbsentAttributeFingerprint =
    0x54414e4b41545452ull;
const unsigned long long kAbsentSubjectFingerprint =
    0x54414e4b5355424aull;
int g_attributeCapacity = 0;
int g_subjectCapacity = 0;
std::string g_firstUnresolvedReference;

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

void HashAttribute(unsigned long long &hash, const AttributeTank &attribute)
{
#define RR2NW_TANK_HASH(field) \
    HashBytes(hash, &attribute.field, sizeof(attribute.field))
    RR2NW_TANK_HASH(m_type);
    RR2NW_TANK_HASH(m_power);
    RR2NW_TANK_HASH(m_armor);
    RR2NW_TANK_HASH(m_radius);
    RR2NW_TANK_HASH(m_viewDist);
    RR2NW_TANK_HASH(massa);
    RR2NW_TANK_HASH(maxPower);
    RR2NW_TANK_HASH(refriction);
    RR2NW_TANK_HASH(maxSpeed);
    RR2NW_TANK_HASH(minSpeed);
    RR2NW_TANK_HASH(rotSpeed);
    RR2NW_TANK_HASH(stopAngle);
    RR2NW_TANK_HASH(maxRotSpeed);
    RR2NW_TANK_HASH(stopRadius2);
    RR2NW_TANK_HASH(stopRadius);
    RR2NW_TANK_HASH(deaxelerate);
    RR2NW_TANK_HASH(rotRefrict);
    RR2NW_TANK_HASH(stopSpeed);
    RR2NW_TANK_HASH(stopPower);
    RR2NW_TANK_HASH(m_maxDistScale);
    HashString(hash, attribute.m_cannonTable);
    HashString(hash, attribute.m_bulletAttrTable);
    HashString(hash, attribute.m_bulletAttr);
    RR2NW_TANK_HASH(m_movingAndShootingIncrement);
    // AttributeTank::wakeUp replaces m_bulletSpeed with the authoritative
    // fu_EV_QUERY_SPEED response from BulletAttr.  Treat it as resolved
    // runtime state rather than immutable script identity.
    HashString(hash, attribute.m_skin);
    RR2NW_TANK_HASH(m_blobCreateTime);
    HashString(hash, attribute.m_smokeTableName);
    HashString(hash, attribute.m_smokeAttrName);
    RR2NW_TANK_HASH(m_mvTimeInc);
    RR2NW_TANK_HASH(m_maxViewDist);
    RR2NW_TANK_HASH(m_turnAngleTreshold);
    RR2NW_TANK_HASH(m_obstH);
    RR2NW_TANK_HASH(m_staticTresh);
    RR2NW_TANK_HASH(m_dynamicTresh);
    RR2NW_TANK_HASH(m_checkRotateTime);
    RR2NW_TANK_HASH(m_checkRotateDelta);
    HashString(hash, attribute.m_soundName);
    RR2NW_TANK_HASH(m_attackDelay);
    RR2NW_TANK_HASH(m_cannonCnt);
    HashString(hash, attribute.m_cannon0);
    HashString(hash, attribute.m_cannon1);
    HashString(hash, attribute.m_cannon2);
    RR2NW_TANK_HASH(powerDeltaTime);
    HashString(hash, attribute.m_corpseAttrName);
    HashString(hash, attribute.m_explAttrName);
    RR2NW_TANK_HASH(m_isCannon);
#undef RR2NW_TANK_HASH
}

struct RosterEntry
{
    std::string name;
    KR_ObjectID id;
    AttributeTank *attribute;
};

struct Roster
{
    SimulationContext *context;
    std::vector<RosterEntry> entries;
    bool attributes;
    bool valid;
};

bool CollectEntry(const KR_ObjectID object, void *user)
{
    Roster *roster = static_cast<Roster *>(user);
    const char *name = roster->context->searchObject(object);
    AttributeTank *attribute = roster->attributes
        ? static_cast<AttributeTank *>(g_tankAttrTable.searchAttribute(object))
        : NULL;
    if (name == NULL || (roster->attributes && attribute == NULL))
    {
        roster->valid = false;
        return false;
    }
    RosterEntry entry = {name, object, attribute};
    roster->entries.push_back(entry);
    return true;
}

bool EntryLess(const RosterEntry &left, const RosterEntry &right)
{
    return left.name < right.name;
}

bool CollectTable(SimulationContext *context, const char *name,
                  bool attributes, Roster &roster)
{
    roster.context = context;
    roster.attributes = attributes;
    roster.valid = context != NULL;
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID : g_arena.searchSeanceClassTable(name);
    if (table == ct_NULLID)
        return false;
    g_arena.userFind(table, CollectEntry, &roster);
    std::sort(roster.entries.begin(), roster.entries.end(), EntryLess);
    return roster.valid;
}

AttributeTank *ResolveTankAttribute(SimulationContext *context,
                                    const char *name)
{
    if (context == NULL || name == NULL || name[0] == 0)
        return NULL;
    KR_ObjectID id = context->searchObject(name);
    return id.isNUL() ? NULL : static_cast<AttributeTank *>(
        g_tankAttrTable.searchAttribute(id));
}

bool ReferenceResolves(SimulationContext *context, const char *owner,
                       const char *field, const char *value)
{
    if (value != NULL && value[0] != '\0' && context->isExist(value))
        return true;
    g_firstUnresolvedReference = owner == NULL ? "<unnamed>" : owner;
    g_firstUnresolvedReference += ".";
    g_firstUnresolvedReference += field;
    g_firstUnresolvedReference += "=";
    g_firstUnresolvedReference += value == NULL ? "<null>" : value;
    return false;
}

bool ObjectNamesResolve(SimulationContext *context, const char *owner,
                        const AttributeTank &attribute)
{
    if (context == NULL || attribute.m_cannonCnt < 0 ||
        attribute.m_cannonCnt > MAX_CANNON)
    {
        g_firstUnresolvedReference = owner == NULL ? "<unnamed>" : owner;
        g_firstUnresolvedReference += ".m_cannonCnt=out-of-range";
        return false;
    }
    const char *cannons[MAX_CANNON] = {
        attribute.m_cannon0, attribute.m_cannon1, attribute.m_cannon2};
    for (int i = 0; i < attribute.m_cannonCnt; ++i)
    {
        const char *fields[MAX_CANNON] = {
            "m_cannon0", "m_cannon1", "m_cannon2"};
        if (!ReferenceResolves(context, owner, fields[i], cannons[i]))
            return false;
    }
    return ReferenceResolves(context, owner, "m_bulletAttr",
                             attribute.m_bulletAttr) &&
           ReferenceResolves(context, owner, "m_skin", attribute.m_skin) &&
           ReferenceResolves(context, owner, "m_smokeAttrName",
                             attribute.m_smokeAttrName) &&
           ReferenceResolves(context, owner, "m_corpseAttrName",
                             attribute.m_corpseAttrName) &&
           ReferenceResolves(context, owner, "m_explAttrName",
                             attribute.m_explAttrName);
}

Tank *ResolveTank(SimulationContext *context, const KR_ObjectID &id)
{
    KR_ObjectID mutableID = id;
    if (context == NULL || mutableID.isNUL())
        return NULL;
    IUnit *unit = static_cast<IUnit *>(
        context->queryInterface(id, IUnitIID));
    return unit == NULL ? NULL : dynamic_cast<Tank *>(unit);
}

bool AttributeRuntimeReady(const AttributeTank *attribute)
{
    KR_ObjectID skin = attribute == NULL
        ? KR_ObjectID::NUL() : attribute->m_skinID;
    return attribute != NULL && attribute->m_cacheSkin != NULL &&
           !skin.isNUL() &&
           attribute->m_cacheCannonTable != ct_NULLID &&
           attribute->m_cacheBulletAttrTable != ct_NULLID &&
           attribute->m_cacheBulletAttr >= 0 &&
           attribute->m_cacheExplosionTable != ct_NULLID &&
           attribute->m_cacheExplAttr >= 0 &&
           attribute->m_cacheCorpseTable != ct_NULLID &&
           attribute->m_cacheCorpseAttr >= 0 &&
           attribute->m_cannonCnt >= 0 &&
           attribute->m_cannonCnt <= MAX_CANNON;
}

bool TankRuntimeReady(SimulationContext *context, Tank *tank)
{
    return tank != NULL && AttributeRuntimeReady(tank->m_attr) &&
           !tank->m_tankAttrID.isNUL() && tank->m_skin.Model() != NULL &&
           tank->m_askin != NULL &&
           context->queryInterface(tank->getObjectID(),
                                   IDynamicObjectIID) != NULL &&
           context->queryInterface(tank->getObjectID(), IUnitIID) != NULL;
}

bool CannonsReady(SimulationContext *context, const Tank &tank)
{
    if (context == NULL || tank.m_attr == NULL ||
        tank.m_cannons.getCount() != tank.m_attr->m_cannonCnt)
        return false;
    for (int i = 0; i < tank.m_cannons.getCount(); ++i)
    {
        const KR_ObjectID &cannon = tank.m_cannons[i];
        if (!context->isExist(cannon) ||
            context->queryInterface(cannon, ICannonIID) == NULL)
            return false;
    }
    return true;
}

bool SameTankData(const TankData &left, const TankData &right)
{
    return std::memcmp(&left, &right, sizeof(TankData)) == 0;
}

bool RoundTripSerializedState(const TankData &saved,
                              const KR_SetOfID &savedCannons)
{
    char temporaryDirectory[MAX_PATH] = {};
    char temporaryFile[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, temporaryDirectory) == 0 ||
        GetTempFileNameA(temporaryDirectory, "r2t", 0, temporaryFile) == 0)
        return false;

    PIN_SaveFile output;
    KR_SetOfID cannonCopy = savedCannons;
    bool valid = output.OpenWrite(temporaryFile) &&
                 output.WriteData(
                     reinterpret_cast<char *>(const_cast<TankData *>(&saved)),
                     sizeof(TankData)) &&
                 cannonCopy.dump(output);
    output.Close();

    TankData restored = {};
    KR_SetOfID restoredCannons;
    if (valid)
    {
        PIN_SaveFile input;
        valid = input.OpenRead(temporaryFile) &&
                input.GetData(reinterpret_cast<char *>(&restored),
                              sizeof(TankData)) &&
                restoredCannons.load(input) &&
                input.GetCurrentData() == NULL;
        input.Close();
    }
    DeleteFileA(temporaryFile);
    if (!valid || !SameTankData(saved, restored) ||
        savedCannons.getCount() != restoredCannons.getCount())
        return false;
    for (int i = 0; i < savedCannons.getCount(); ++i)
        if (savedCannons[i] != restoredCannons[i])
            return false;
    return true;
}

bool SendAttribute(Tank *tank, const KR_ObjectID &attribute,
                   double timeStamp)
{
    KR_ObjectID mutableAttribute = attribute;
    if (tank == NULL || mutableAttribute.isNUL())
        return false;
    KR_Event event;
    event.label = KR_SET_ATTR;
    event.source = g_arena.getObjectID();
    event.destination = tank->getObjectID();
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE).putObjectID(attribute).close();
    return tank->receiveEvent(event) == 1;
}

bool StartMovement(Tank *tank, double timeStamp)
{
    if (tank == NULL)
        return false;
    KR_Event event;
    event.label = t_EV_MOVE;
    event.source = g_arena.getObjectID();
    event.destination = tank->getObjectID();
    event.timeStamp = timeStamp;
    event.data.open(EDO_WRITE)
              .putDouble(0.5)
              .descend(VECTOR_LAND, 0)
                .putDouble(1.0)
                .putDouble(0.0)
              .ascend()
              .putDouble(50.0)
              .close();
    return tank->receiveEvent(event) == 1;
}

int RemoveAllSubjects(SimulationContext *context, const char *tableName)
{
    Roster roster = {};
    if (!CollectTable(context, tableName, false, roster))
        return 0;
    int removed = 0;
    for (std::vector<RosterEntry>::reverse_iterator entry =
             roster.entries.rbegin();
         entry != roster.entries.rend(); ++entry)
    {
        if (context->isExist(entry->id))
        {
            context->removeObject(entry->id);
            ++removed;
        }
    }
    return removed;
}

}  // namespace

void TankSubjectState_Link()
{
    Tank linkAnchor;
    (void)linkAnchor;
}

void TankSubjectState_SetExpectedCapacities(int attributeCapacity,
                                            int subjectCapacity)
{
    g_attributeCapacity = attributeCapacity > 0 ? attributeCapacity : 0;
    g_subjectCapacity = subjectCapacity > 0 ? subjectCapacity : 0;
}

int TankSubjectState_AttributeCapacity() { return g_attributeCapacity; }
int TankSubjectState_SubjectCapacity() { return g_subjectCapacity; }

int TankSubjectState_AttributeCount(SimulationContext *context)
{
    Roster roster = {};
    if (!CollectTable(context, "TankAttr", true, roster))
        return g_attributeCapacity == 0 ? 0 : -1;
    return static_cast<int>(roster.entries.size());
}

int TankSubjectState_LiveCount(SimulationContext *context)
{
    Roster roster = {};
    if (!CollectTable(context, "Tank", false, roster))
        return g_subjectCapacity == 0 ? 0 : -1;
    return static_cast<int>(roster.entries.size());
}

unsigned long long TankSubjectState_AttributeFingerprint(
    SimulationContext *context)
{
    Roster roster = {};
    if (!CollectTable(context, "TankAttr", true, roster))
        return g_attributeCapacity == 0 ? kAbsentAttributeFingerprint : 0;
    unsigned long long hash = kHashOffset;
    HashBytes(hash, &g_attributeCapacity, sizeof(g_attributeCapacity));
    for (std::size_t i = 0; i < roster.entries.size(); ++i)
    {
        HashString(hash, roster.entries[i].name.c_str());
        HashAttribute(hash, *roster.entries[i].attribute);
    }
    return hash;
}

unsigned long long TankSubjectState_SubjectFingerprint(
    SimulationContext *context)
{
    Roster roster = {};
    if (!CollectTable(context, "Tank", false, roster))
        return g_subjectCapacity == 0 ? kAbsentSubjectFingerprint : 0;
    unsigned long long hash = kHashOffset;
    HashBytes(hash, &g_subjectCapacity, sizeof(g_subjectCapacity));
    for (std::size_t i = 0; i < roster.entries.size(); ++i)
        HashString(hash, roster.entries[i].name.c_str());
    return hash;
}

bool TankSubjectState_AttributeReferencesResolved(SimulationContext *context)
{
    g_firstUnresolvedReference.clear();
    Roster roster = {};
    if (!CollectTable(context, "TankAttr", true, roster))
        return g_attributeCapacity == 0;
    for (std::size_t i = 0; i < roster.entries.size(); ++i)
        if (!ObjectNamesResolve(context, roster.entries[i].name.c_str(),
                                *roster.entries[i].attribute))
            return false;
    return true;
}

const char *TankSubjectState_FirstUnresolvedReference()
{
    return g_firstUnresolvedReference.c_str();
}

bool TankSubjectState_UpdateAttributes(SimulationContext *context,
                                       double timeStamp)
{
    if (context == NULL || g_arena.getContext() != context ||
        !std::isfinite(timeStamp))
        return false;
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("TankAttr");
    if (table == ct_NULLID)
        return g_attributeCapacity == 0;
    ct_ClassTable *raw = ct_Storage::searchClassTable("TankAttr");
    ct_AttributeTable *attributes = dynamic_cast<ct_AttributeTable *>(raw);
    if (attributes == NULL || attributes->getClassTableID() == ct_NULLID)
        return false;
    attributes->update(timeStamp);
    return true;
}

bool TankSubjectState_CaptureGameplayTuning(
    SimulationContext *context, const char *id,
    STankGameplayTuningState *state)
{
    if (state == NULL)
        return false;
    std::memset(state, 0, sizeof(*state));
    AttributeTank *attribute = ResolveTankAttribute(context, id);
    if (attribute == NULL || std::strlen(id) >= sizeof(state->id))
        return false;
    state->owner = attribute;
    std::strncpy(state->id, id, sizeof(state->id) - 1);
    state->maxSpeed = attribute->maxSpeed;
    state->attackPower = attribute->m_power;
    state->attackDelay = attribute->m_attackDelay;
    state->mass = attribute->massa;
    if (std::strlen(attribute->m_bulletAttr) >= sizeof(state->projectile))
        return false;
    std::strncpy(state->projectile, attribute->m_bulletAttr,
                 sizeof(state->projectile) - 1);
    return std::isfinite(state->maxSpeed) &&
           std::isfinite(state->attackPower) &&
           std::isfinite(state->attackDelay) && std::isfinite(state->mass) &&
           state->maxSpeed >= 0.0 && state->mass > 0.0 &&
           state->attackPower > 0.0 && state->attackDelay >= 0.0;
}

bool TankSubjectState_ApplyGameplayTuning(
    SimulationContext *context, const STankGameplayTuningState *state,
    const STankGameplayTuningPatch *patch)
{
    if (state == NULL || patch == NULL || state->owner == NULL)
        return false;
    AttributeTank *attribute = ResolveTankAttribute(context, state->id);
    if (attribute == NULL || attribute != state->owner)
        return false;
    if (patch->hasMaxSpeed) attribute->maxSpeed = patch->maxSpeed;
    if (patch->hasAttackPower) attribute->m_power = patch->attackPower;
    if (patch->hasAttackDelay) attribute->m_attackDelay = patch->attackDelay;
    if (patch->hasMass) attribute->massa = patch->mass;
    if (patch->hasProjectile)
        std::snprintf(attribute->m_bulletAttr,
                      sizeof(attribute->m_bulletAttr), "%s",
                      patch->projectile);
    return true;
}

bool TankSubjectState_RestoreGameplayTuning(
    SimulationContext *context, const STankGameplayTuningState *state)
{
    if (state == NULL || state->owner == NULL)
        return false;
    AttributeTank *attribute = ResolveTankAttribute(context, state->id);
    if (attribute == NULL || attribute != state->owner)
        return false;
    attribute->maxSpeed = state->maxSpeed;
    attribute->m_power = state->attackPower;
    attribute->m_attackDelay = state->attackDelay;
    attribute->massa = state->mass;
    std::snprintf(attribute->m_bulletAttr,
                  sizeof(attribute->m_bulletAttr), "%s",
                  state->projectile);
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

static bool RenderTankPose(Tank *tank, double timeStamp, CFVector3 *offset)
{
    if (tank == NULL || offset == NULL || !std::isfinite(timeStamp))
        return false;
    CViewDynamicList frame;
    tank->render(frame, timeStamp);
    const bool linked = frame.Contains(&tank->m_viewDynObj);
    *offset = tank->m_skin.GetDir().Offset();
    frame.Clear(FALSE);
    return linked && FiniteVector(*offset);
}

static bool ProbeTankPresentation(Tank *tank, double timeStamp,
                                  int *renderedFrames,
                                  int *boundaryResets)
{
    if (tank == NULL || renderedFrames == NULL || boundaryResets == NULL ||
        !std::isfinite(timeStamp))
        return false;
    *renderedFrames = 0;
    *boundaryResets = 0;

    const TankData saved = *static_cast<TankData *>(tank);
    const CFVector3 savedPosition = tank->getPosition();
    const double savedMoveTime = tank->m_lastMoveTimeStamp;
    const int savedVisible = tank->m_isVisible;
    const CFMatrix3x4 savedMatrix = tank->m_skin.GetDir();
    const CFVector3 displacement(4.0, 1.0, -2.0);
    const double sampleInterval = 0.1;
    CFVector3 baseline;
    CFVector3 interpolated;
    CFVector3 stale;
    CFVector3 reentered;

    tank->m_lastMovePos = savedPosition;
    tank->m_lastMoveTimeStamp = timeStamp;
    tank->m_lastDeltaT = 0.0;
    bool valid = RenderTankPose(tank, timeStamp, &baseline);
    if (valid) ++*renderedFrames;

    tank->m_lastMovePos = savedPosition - displacement;
    tank->m_lastMoveTimeStamp = timeStamp;
    tank->m_lastDeltaT = sampleInterval;
    valid = valid && RenderTankPose(
        tank, timeStamp + sampleInterval * 0.5, &interpolated);
    if (valid) ++*renderedFrames;
    valid = valid && SameVector(interpolated - baseline,
                                displacement * -0.5);

    valid = valid && RenderTankPose(
        tank, timeStamp + sampleInterval * 10.0, &stale);
    if (valid) ++*renderedFrames;
    valid = valid && SameVector(stale, baseline);

    tank->m_isVisible = 0;
    tank->onHide(timeStamp + sampleInterval * 11.0);
    tank->m_runSmoke = 1;
    tank->m_isVisible = 1;
    tank->onView(timeStamp + sampleInterval * 12.0);
    valid = valid && tank->m_lastDeltaT == 0.0 &&
            SameVector(tank->m_lastMovePos, savedPosition);
    if (valid) *boundaryResets = 1;
    valid = valid && RenderTankPose(
        tank, timeStamp + sampleInterval * 20.0, &reentered);
    if (valid) ++*renderedFrames;
    valid = valid && SameVector(reentered, baseline) &&
            SameVector(tank->getPosition(), savedPosition);

    *static_cast<TankData *>(tank) = saved;
    tank->ct_Subject::setPosition(savedPosition);
    tank->m_lastMoveTimeStamp = savedMoveTime;
    tank->m_isVisible = savedVisible;
    tank->m_skin.GetDirModify() = savedMatrix;
    return valid && *renderedFrames == 4 && *boundaryResets == 1;
}

static bool ProbeTankLifecycle(
    SimulationContext *context, const char *requestedAttribute,
    bool requireMassConsumer, const char *expectedProjectile,
    double timeStamp,
    STankLifecycleProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const int baselineTanks = TankSubjectState_LiveCount(context);
    const int baselineCannons = CannonSubjectState_LiveCount(context);
    const int baselineSounds = SoundObjectState_LiveCount();
    const int baselineExplosions = ExplosionSubjectState_LiveCount();
    const int baselineCorpses = CorpseSubjectState_LiveCount();
    const int baselineBullets = BulletSubjectState_LiveCount();
    const unsigned long long baselineTankFingerprint =
        TankSubjectState_SubjectFingerprint(context);
    const unsigned long long baselineCannonFingerprint =
        CannonSubjectState_SubjectFingerprint(context);
    Roster attributes = {};
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID : g_arena.searchSeanceClassTable("Tank");
    if (context == NULL || table == ct_NULLID || baselineTanks != 0 ||
        (expectedProjectile != NULL && baselineBullets != 0) ||
        baselineCannons != 0 || baselineExplosions != 0 ||
        baselineCorpses != 0 || baselineTankFingerprint == 0 ||
        baselineCannonFingerprint == 0 ||
        !CollectTable(context, "TankAttr", true, attributes))
        return false;

    const RosterEntry *selection = NULL;
    for (std::size_t i = 0; i < attributes.entries.size(); ++i)
        if ((requestedAttribute == NULL ||
             attributes.entries[i].name == requestedAttribute) &&
            AttributeRuntimeReady(attributes.entries[i].attribute) &&
            attributes.entries[i].attribute->m_cannonCnt > 0)
        {
            selection = &attributes.entries[i];
            break;
        }
    if (selection == NULL)
    {
        // Level.06 has a deliberately empty TankAttr roster and Level.07 has
        // no Tank tables.  That is a valid retail state, not a failed probe.
        summary->rollbacks = 1;
        return requestedAttribute == NULL && attributes.entries.empty();
    }
    summary->available = 1;

    KR_ObjectID probeID =
        g_arena.newObject(table, "Tank.Lifecycle.Probe");
    Tank *tank = ResolveTank(context, probeID);
    bool valid = SendAttribute(tank, selection->id, timeStamp);
    if (valid && requestedAttribute != NULL)
        valid = tank->m_tankAttrID == selection->id &&
                tank->m_attr == selection->attribute &&
                std::fabs(tank->getPower() -
                          selection->attribute->m_power) <= 1e-9;
    std::vector<KR_ObjectID> ownedCannons;
    if (valid && TankRuntimeReady(context, tank))
    {
        summary->validStarts = 1;
        summary->dynamicReady = 1;
        summary->renderReady = tank->m_skin.Model() != NULL ? 1 : 0;
        summary->cannonReady = CannonsReady(context, *tank) ? 1 : 0;
        const double expectedInverseMass = 1.0 / selection->attribute->massa;
        if (std::isfinite(expectedInverseMass) &&
            std::fabs(tank->massa_D - expectedInverseMass) <=
                1e-12 * (std::max)(1.0, std::fabs(expectedInverseMass)))
            summary->massConsumerReady = 1;
        if (expectedProjectile != NULL)
        {
            const ct_ClassTableID bulletAttributes =
                g_arena.searchSeanceClassTable("BulletAttr");
            KR_ObjectID expected =
                context->searchObject(expectedProjectile);
            const int expectedIndex = expected.isNUL() ||
                bulletAttributes == ct_NULLID ? -1 :
                g_arena.getAttributeIndex(bulletAttributes, expected);
            if (std::strcmp(selection->attribute->m_bulletAttr,
                            expectedProjectile) == 0 &&
                selection->attribute->m_cacheBulletAttrTable ==
                    bulletAttributes &&
                selection->attribute->m_cacheBulletAttr == expectedIndex &&
                expectedIndex >= 0)
                summary->projectileReferenceReady = 1;
        }
        for (int i = 0; i < tank->m_cannons.getCount(); ++i)
            ownedCannons.push_back(tank->m_cannons[i]);

        if (expectedProjectile != NULL &&
            summary->projectileReferenceReady == 1 &&
            !ownedCannons.empty())
        {
            ICannon *cannonInterface = static_cast<ICannon *>(
                context->queryInterface(ownedCannons.front(), ICannonIID));
            Cannon *cannon = dynamic_cast<Cannon *>(cannonInterface);
            if (cannon != NULL)
            {
                cannon->shoot(selection->attribute->m_cacheBulletAttr,
                              timeStamp + 0.005);
                if (BulletSubjectState_LiveCount() == baselineBullets + 1)
                    summary->outgoingProjectileStarts = 1;
                RemoveAllSubjects(context, "Bullet");
            }
        }

        const TankData saved = *static_cast<TankData *>(tank);
        const KR_SetOfID savedCannons = tank->m_cannons;
        const CFVector3 savedPosition = tank->getPosition();
        tank->m_damage = -123.0;
        *static_cast<TankData *>(tank) = saved;
        tank->ct_Subject::setPosition(savedPosition);
        if (SameTankData(saved, *static_cast<TankData *>(tank)) &&
            RoundTripSerializedState(saved, savedCannons))
            summary->saveStateRoundTrips = 1;

        tank->ct_Subject::setPosition(CFVector3(512.0, 10000.0, -512.0));
        const bool moveAccepted = StartMovement(tank, timeStamp + 0.1);
        const bool firstMoveScheduled =
            context->removeEvent(t_EVC_MOVING, probeID) == 1;
        KR_Event move;
        move.label = t_EVC_MOVING;
        move.source = probeID;
        move.destination = probeID;
        move.timeStamp = timeStamp + 0.2;
        move.data.open(EDO_WRITE).putDouble(timeStamp + 0.1).close();
        const double moveExecutionTime = move.timeStamp;
        const bool moveExecuted = firstMoveScheduled &&
            tank->receiveEvent(move) == 1;
        const bool nextMoveScheduled =
            context->removeEvent(t_EVC_MOVING, probeID) == 1;
        KR_Event drive[2];
        const int driveCount =
            context->copyEvents(UNIT_I_DRIVE, probeID, drive, 2);
        const double driveInterval = driveCount == 1
            ? drive[0].timeStamp - moveExecutionTime : 0.0;
        context->removeEvent(UNIT_I_DRIVE, probeID);
        summary->cadenceBounded = driveCount == 1 &&
            std::isfinite(driveInterval) &&
            driveInterval >= MODEL_TIME_DELTA_FORWARD * 0.2 - 1e-9 &&
            driveInterval <= MODEL_TIME_DELTA_FORWARD * 2.0 + 1e-9 ? 1 : 0;
        summary->scheduledMoves =
            moveAccepted && moveExecuted && nextMoveScheduled ? 1 : 0;

        ProbeTankPresentation(tank, timeStamp + 2.0,
                              &summary->renderedPoseFrames,
                              &summary->viewBoundaryResets);

        const char *bulletAttribute =
            BulletAttributeState_FirstAttributeName(context);
        tank->m_damage = 1000.0;
        const double damageBefore = tank->m_damage;
        const bool bulletLifecycle = bulletAttribute != NULL &&
            BulletSubjectState_ProbeDynamicCollisionLifecycle(
                context, bulletAttribute, probeID, timeStamp + 0.3);
        if (bulletLifecycle && tank->m_damage < damageBefore &&
            context->isExist(probeID))
            summary->bulletDamageApplications = 1;
        else
            std::fprintf(stderr,
                         "Tank bullet probe attr=%s bullet=%s radius=%g "
                         "lifecycle=%i damage=%g/%g scale=%g exists=%i\n",
                         selection->name.c_str(),
                         bulletAttribute == NULL ? "<none>" : bulletAttribute,
                         tank->getRadius(), bulletLifecycle ? 1 : 0,
                         damageBefore, tank->m_damage,
                         g_levelAttr.m_unitDamageScale,
                         context->isExist(probeID) ? 1 : 0);

        tank->m_isVisible = 1;
        // com_EV_I_AM_DEAD in the original handler uses this exact sentinel
        // path, independent of the current Level damage scaling constants.
        tank->m_damage = -1.0;
        tank->setDamage(0.0, tank->getPosition(), timeStamp + 0.4,
                        KR_ObjectID::NUL());
        const int deathExplosions =
            ExplosionSubjectState_LiveCount() - baselineExplosions;
        const int deathCorpses =
            CorpseSubjectState_LiveCount() - baselineCorpses;
        bool childrenRemoved = !context->isExist(probeID);
        for (std::size_t i = 0; i < ownedCannons.size(); ++i)
            childrenRemoved = childrenRemoved &&
                              !context->isExist(ownedCannons[i]);
        if (childrenRemoved)
            summary->deathTransitions = 1;
        if (deathExplosions == 1 && deathCorpses == 1)
            summary->deathEffects = 1;
        RemoveAllSubjects(context, "Explosion");
        RemoveAllSubjects(context, "Corpse");
    }

    if (!probeID.isNUL() && context->isExist(probeID))
        context->removeObject(probeID);
    if (TankSubjectState_LiveCount(context) == baselineTanks &&
        CannonSubjectState_LiveCount(context) == baselineCannons &&
        SoundObjectState_LiveCount() == baselineSounds &&
        ExplosionSubjectState_LiveCount() == baselineExplosions &&
        CorpseSubjectState_LiveCount() == baselineCorpses &&
        TankSubjectState_SubjectFingerprint(context) ==
            baselineTankFingerprint &&
        CannonSubjectState_SubjectFingerprint(context) ==
            baselineCannonFingerprint)
        summary->rollbacks = 1;

    const bool massProof = !requireMassConsumer ||
        summary->massConsumerReady == 1;
    const bool projectileProof = expectedProjectile == NULL ||
        (summary->projectileReferenceReady == 1 &&
         summary->outgoingProjectileStarts == 1 &&
         BulletSubjectState_LiveCount() == baselineBullets);
    return valid && massProof && projectileProof && summary->available == 1 &&
           summary->validStarts == 1 && summary->dynamicReady == 1 &&
           summary->renderReady == 1 && summary->cannonReady == 1 &&
           summary->scheduledMoves == 1 &&
           summary->cadenceBounded == 1 &&
           summary->renderedPoseFrames == 4 &&
           summary->viewBoundaryResets == 1 &&
           summary->bulletDamageApplications == 1 &&
           summary->deathTransitions == 1 &&
           summary->deathEffects == 1 &&
           summary->saveStateRoundTrips == 1 && summary->rollbacks == 1;
}

bool TankSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    STankLifecycleProbeSummary *summary)
{
    return ProbeTankLifecycle(context, NULL, false, NULL, timeStamp, summary);
}

bool TankSubjectState_ProbeAttributeLifecycle(
    SimulationContext *context, const char *attributeName, double timeStamp,
    STankLifecycleProbeSummary *summary)
{
    return attributeName != NULL && attributeName[0] != 0 &&
           ProbeTankLifecycle(context, attributeName, false, NULL, timeStamp,
                              summary);
}

bool TankSubjectState_ProbeTunedAttributeLifecycle(
    SimulationContext *context, const char *attributeName,
    bool requireMassConsumer, const char *expectedProjectile,
    double timeStamp, STankLifecycleProbeSummary *summary)
{
    return attributeName != NULL && attributeName[0] != 0 &&
           (expectedProjectile == NULL || expectedProjectile[0] != 0) &&
           ProbeTankLifecycle(context, attributeName, requireMassConsumer,
                              expectedProjectile, timeStamp, summary);
}
