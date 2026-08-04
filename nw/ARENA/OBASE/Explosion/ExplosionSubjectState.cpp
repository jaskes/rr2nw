#define LAST_H__SCENE
#include "game.h"

#include "ExplosionSubjectState.h"
#include "ExplosionActiveWorldState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "ExplosionAttributeState.h"
#include "h/light.h"
#include "h/olevel.h"
#include "i/dynobj.i"
#include "i/player.i"
#include "i/unit.i"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "kernel/h/session.h"
#include "message/explmsg.h"
#include "message/fountmsg.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "storage/h/subject.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const int kParticleBranchCapacity = 500;
const int kParticleBranchPerExplosion = 128;
const int kMaximumTracedExplosionParents = 4;
const double kRayTimeLife = 0.6;
const double kExplosionMaximumTimeLife = 15.0;
const double kPi = 3.14159265358979323846;
const std::uint32_t kExplosionActiveWorldMagic = 0x31505845u; // EXP1
const std::uint32_t kExplosionActiveWorldVersion = 1u;
const std::size_t kMaximumActiveWorldExplosions = 4096;
const std::size_t kMaximumActiveWorldString = MAX_SYMBOLIC_LENGHT - 1;

int g_executedCommands = 0;
int g_damageApplications = 0;
int g_impulseApplications = 0;
int g_allocationRollbacks = 0;
int g_queueRollbacks = 0;
int g_particleBranchesLive = 0;
int g_pieceDrawCalls = 0;
int g_tracedExplosionParents = 0;
int g_tracePuffsStarted = 0;
std::string g_activeWorldFailure;
SimulationContext *g_impulseContext = NULL;
KR_ObjectID g_impulseTarget = KR_ObjectID::NUL();
void *g_impulseUser = NULL;
ExplosionImpulseDispatch g_impulseDispatch = NULL;

enum ExplosionParticleBranchType
{
    EXPLOSION_PARTICLE_SIMPLE = 0,
    EXPLOSION_PARTICLE_SNAKE = 1,
    EXPLOSION_PARTICLE_PIECE = 2,
    EXPLOSION_PARTICLE_TRACED_PIECE = 3,
    EXPLOSION_PARTICLE_RAY = 4,
    EXPLOSION_PARTICLE_SMOKE = 5
};

class ExplosionPieceDrawable : public CViewSphericDynamic
{
 public:
    explicit ExplosionPieceDrawable(CViewObjectRef &skin) : m_skin(skin) {}

    void prepare()
    {
        m_dynBase = m_dynBase1 = m_bump.start = m_skin.Center();
        m_bump.vel = CFVector3(0.0, 0.0, 0.0);
        m_bump.fTime = 0.0;
        m_bump.fRadius = m_skin.Model()->Radius();
    }

    virtual void Draw()
    {
        if (m_skin.Model() == NULL)
            return;
        m_skin.LoadLights(m_dwLights);
        m_skin.Draw();
        ++g_pieceDrawCalls;
    }

 private:
    CViewObjectRef &m_skin;
};

struct ExplosionParticleBranch
{
    ExplosionParticleBranchType type;
    CFVector3 start;
    CFVector3 velocity;
    double radius;
    double timeOfLife;
    double rayAngle;
    double rayWidth;
    double rotationOySpeed;
    double rotationOxSpeed;
    double radiusA;
    double radiusB;
    double opacityA;
    double opacityB;
    double opacityC;
    double reciprocalMaximumTime;
    double multiplier;
    CFVector3 drift;
    int u0;
    int v0;
    int u1;
    int v1;
    int tailCount;
    unsigned long color;
    bool active;
    bool createPuffNow;
    bool landDynamicPublished;
    CViewObjectRef skin;
    ExplosionPieceDrawable drawable;

    ExplosionParticleBranch() : drawable(skin) { reset(); }

    void reset()
    {
        type = EXPLOSION_PARTICLE_SIMPLE;
        start = CFVector3(0.0, 0.0, 0.0);
        velocity = CFVector3(0.0, 0.0, 0.0);
        radius = 0.0;
        timeOfLife = 0.0;
        rayAngle = 0.0;
        rayWidth = 0.0;
        rotationOySpeed = 0.0;
        rotationOxSpeed = 0.0;
        radiusA = 0.0;
        radiusB = 0.0;
        opacityA = 0.0;
        opacityB = 0.0;
        opacityC = 0.0;
        reciprocalMaximumTime = 0.0;
        multiplier = 0.0;
        drift = CFVector3(0.0, 0.0, 0.0);
        u0 = v0 = u1 = v1 = 0;
        tailCount = 0;
        color = 0;
        active = false;
        createPuffNow = false;
        landDynamicPublished = false;
    }
};

struct StableExplosionBranch
{
    int type;
    CFVector3 start;
    CFVector3 velocity;
    double radius;
    double timeOfLife;
    double rayAngle;
    double rayWidth;
    double rotationOySpeed;
    double rotationOxSpeed;
    double radiusA;
    double radiusB;
    double opacityA;
    double opacityB;
    double opacityC;
    double reciprocalMaximumTime;
    double multiplier;
    CFVector3 drift;
    int u0;
    int v0;
    int u1;
    int v1;
    int tailCount;
    std::uint32_t color;
    int createPuffNow;

    StableExplosionBranch()
        : type(EXPLOSION_PARTICLE_SIMPLE),
          start(0.0, 0.0, 0.0), velocity(0.0, 0.0, 0.0),
          radius(0.0), timeOfLife(0.0), rayAngle(0.0), rayWidth(0.0),
          rotationOySpeed(0.0), rotationOxSpeed(0.0), radiusA(0.0),
          radiusB(0.0), opacityA(0.0), opacityB(0.0), opacityC(0.0),
          reciprocalMaximumTime(0.0), multiplier(0.0),
          drift(0.0, 0.0, 0.0), u0(0), v0(0), u1(0), v1(0),
          tailCount(0), color(0), createPuffNow(0) {}
};

struct StableExplosionRecord
{
    std::string name;
    std::string attribute;
    CFVector3 position;
    double startTime;
    double nextMoveTime;
    double previousMoveTime;
    double nextPuffTime;
    double landY;
    int lightActive;
    int landHeightReady;
    int traceQuotaHeld;
    int hasSound;
    double movingTimeStamp;
    int hasPuffEvent;
    double puffTimeStamp;
    std::vector<StableExplosionBranch> branches;

    StableExplosionRecord()
        : position(0.0, 0.0, 0.0), startTime(0.0),
          nextMoveTime(0.0), previousMoveTime(0.0), nextPuffTime(0.0),
          landY(0.0), lightActive(0), landHeightReady(0),
          traceQuotaHeld(0), hasSound(0), movingTimeStamp(0.0),
          hasPuffEvent(0), puffTimeStamp(0.0) {}
};

bool FailActiveWorld(const std::string &message)
{
    g_activeWorldFailure = message;
    return false;
}

class BoundedExplosion;
bool NearlyEqual(double actual, double expected);

double PositiveBranchRoot(double a, double b, double c)
{
    if (std::fabs(a) < 1.0e-5)
    {
        if (std::fabs(b) < 1.0e-5)
            return 1.0e10;
        const double result = -c / b;
        return result < 1.001 ? 1.0e10 : result;
    }
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
        return 1.0e10;
    const double root = std::sqrt(discriminant);
    const double first = (-b + root) / (2.0 * a);
    const double second = (-b - root) / (2.0 * a);
    const double result = first > second ? first : second;
    return result < 0.001 ? 1.0e10 : result;
}

class ExplosionParticleDrawable : public CViewSphericDynamic
{
 public:
    ExplosionParticleDrawable() : m_owner(NULL) {}
    void bind(BoundedExplosion *owner) { m_owner = owner; }
    void prepare(const CFVector3 &position, double radius);
    virtual void Draw();

 private:
    BoundedExplosion *m_owner;
};

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

std::string ObjectName(SimulationContext *context,
                       const KR_ObjectID &object)
{
    KR_ObjectID mutableObject = object;
    if (context == NULL || mutableObject.isNUL())
        return std::string();
    const char *name = context->searchObject(object);
    return name == NULL ? std::string() : std::string(name);
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

int ExpectedLightBrightness(int index)
{
    int brightness = index * 20;
    if (brightness > 255)
        brightness = 255;
    if (index > AttributeExplosion::MAX_BRIGHT / 3)
    {
        const int tailIndex =
            index - AttributeExplosion::MAX_BRIGHT / 3;
        brightness = static_cast<int>(255.0 / tailIndex);
    }
    return brightness;
}

bool LightBrightnessReady(const AttributeExplosion *attribute)
{
    if (attribute == NULL)
        return false;
    for (int index = 0; index < AttributeExplosion::MAX_BRIGHT; ++index)
        if (attribute->m_brightness[index] !=
            ExpectedLightBrightness(index))
            return false;
    return true;
}

bool LightAttributeReady(const AttributeExplosion *attribute)
{
    return attribute != NULL && attribute->m_useLight != 0 &&
           std::isfinite(attribute->m_lightOffset) &&
           std::isfinite(attribute->m_lightRadius) &&
           attribute->m_lightRadius > 0.0 &&
           attribute->m_lightColor >= 0 &&
           attribute->m_lightColor < LIGHT_COLOR_COUNT &&
           std::isfinite(attribute->m_lightTimeLife) &&
           attribute->m_lightTimeLife > 0.0 &&
           LightBrightnessReady(attribute);
}

struct LightRosterProbe
{
    bool ready;
};

struct LightProbeAttribute
{
    SimulationContext *context;
    const char *name;
    double timeLife;
};

struct SoundProbeAttribute
{
    SimulationContext *context;
    const char *name;
    double timeLife;
};

struct PieceProbeAttribute
{
    SimulationContext *context;
    const char *name;
    double timeLife;
};

struct TraceProbeAttribute
{
    SimulationContext *context;
    const char *name;
    double timeLife;
};

bool ValidateLightAttribute(const KR_ObjectID object, void *user)
{
    LightRosterProbe *probe = static_cast<LightRosterProbe *>(user);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (probe == NULL || attribute == NULL ||
        !LightBrightnessReady(attribute) ||
        (attribute->m_useLight != 0 && !LightAttributeReady(attribute)))
    {
        if (probe != NULL)
            probe->ready = false;
        return false;
    }
    return true;
}

bool SelectLightProbeAttribute(const KR_ObjectID object, void *user)
{
    LightProbeAttribute *probe =
        static_cast<LightProbeAttribute *>(user);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (probe == NULL || probe->context == NULL)
        return false;
    if (!LightAttributeReady(attribute))
        return true;
    const char *name = probe->context->searchObject(object);
    if (name != NULL &&
        (probe->name == NULL || attribute->m_lightTimeLife > probe->timeLife ||
         (attribute->m_lightTimeLife == probe->timeLife &&
          std::strcmp(name, probe->name) < 0)))
    {
        probe->name = name;
        probe->timeLife = attribute->m_lightTimeLife;
    }
    return true;
}

bool SelectSoundProbeAttribute(const KR_ObjectID object, void *user)
{
    SoundProbeAttribute *probe =
        static_cast<SoundProbeAttribute *>(user);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (probe == NULL || probe->context == NULL)
        return false;
    if (!LightAttributeReady(attribute) || attribute->m_soundName[0] == 0 ||
        attribute->m_wav == NULL || attribute->m_ctsndID == ct_NULLID)
        return true;
    const char *name = probe->context->searchObject(object);
    if (name != NULL &&
        (probe->name == NULL || attribute->m_lightTimeLife > probe->timeLife ||
         (attribute->m_lightTimeLife == probe->timeLife &&
          std::strcmp(name, probe->name) < 0)))
    {
        probe->name = name;
        probe->timeLife = attribute->m_lightTimeLife;
    }
    return true;
}

bool SelectPieceProbeAttribute(const KR_ObjectID object, void *user)
{
    PieceProbeAttribute *probe =
        static_cast<PieceProbeAttribute *>(user);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (probe == NULL || probe->context == NULL)
        return false;
    if (attribute == NULL || attribute->m_cacheSkin == NULL ||
        attribute->m_minPieceCnt <= 0 ||
        attribute->m_maxPieceCnt < attribute->m_minPieceCnt ||
        attribute->m_minPieceTimeLife <= 0.0)
        return true;
    const char *name = probe->context->searchObject(object);
    if (name != NULL &&
        (probe->name == NULL ||
         attribute->m_minPieceTimeLife > probe->timeLife ||
         (attribute->m_minPieceTimeLife == probe->timeLife &&
          std::strcmp(name, probe->name) < 0)))
    {
        probe->name = name;
        probe->timeLife = attribute->m_minPieceTimeLife;
    }
    return true;
}

bool SelectTraceProbeAttribute(const KR_ObjectID object, void *user)
{
    TraceProbeAttribute *probe =
        static_cast<TraceProbeAttribute *>(user);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(object));
    if (probe == NULL || probe->context == NULL)
        return false;
    if (attribute == NULL || attribute->m_cacheSkin == NULL ||
        attribute->m_smokeTableID == ct_NULLID ||
        attribute->m_smokeAttrID.isNUL() ||
        attribute->m_minPieceSmokeCnt <= 0 ||
        attribute->m_maxPieceSmokeCnt <
            attribute->m_minPieceSmokeCnt ||
        attribute->m_minPieceTimeLife <= 0.0 ||
        attribute->m_traceNewPuffTime <= 0.0)
        return true;
    const char *name = probe->context->searchObject(object);
    if (name != NULL &&
        (probe->name == NULL ||
         attribute->m_minPieceTimeLife > probe->timeLife ||
         (attribute->m_minPieceTimeLife == probe->timeLife &&
          std::strcmp(name, probe->name) < 0)))
    {
        probe->name = name;
        probe->timeLife = attribute->m_minPieceTimeLife;
    }
    return true;
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
    BoundedExplosion()
    {
        m_particleDrawable.bind(this);
        resetState();
    }

    virtual CFVector3 realPosition() { return m_position; }
    virtual bool shouldDump() { return false; }

    virtual void render(CViewDynamicList &list, double)
    {
        if (!m_started)
            return;
        if (m_lightActive && LightAttributeReady(m_attribute))
        {
            double elapsed = Session::m_moment - m_startTime;
            if (std::isfinite(elapsed) &&
                elapsed <= m_attribute->m_lightTimeLife)
            {
                if (elapsed < 0.0)
                    elapsed = 0.0;
                int index = static_cast<int>(
                    elapsed * AttributeExplosion::MAX_BRIGHT /
                    m_attribute->m_lightTimeLife);
                if (index < 0)
                    index = 0;
                else if (index >= AttributeExplosion::MAX_BRIGHT)
                    index = AttributeExplosion::MAX_BRIGHT - 1;
                g_lightChain.add(
                    m_position +
                        CFVector3(0.0, m_attribute->m_lightOffset, 0.0),
                    m_attribute->m_lightColor,
                    m_attribute->m_brightness[index],
                    m_attribute->m_lightRadius);
            }
        }
        const double pieceElapsed = m_previousMoveTime - m_startTime;
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
        {
            ExplosionParticleBranch &branch = m_particles[index];
            if (!branch.active ||
                (branch.type != EXPLOSION_PARTICLE_PIECE &&
                 branch.type != EXPLOSION_PARTICLE_TRACED_PIECE) ||
                branch.skin.Model() == NULL)
                continue;
            CFMatrix3x4 &matrix = branch.skin.GetDirModify();
            matrix.LoadIdentity();
            matrix.RotateOyL(branch.rotationOySpeed * pieceElapsed);
            matrix.RotateOxL(branch.rotationOxSpeed * pieceElapsed);
            matrix.TranslateL(branch.start + CFVector3(
                branch.velocity.x * pieceElapsed,
                branch.velocity.y * pieceElapsed -
                    4.9 * pieceElapsed * pieceElapsed,
                branch.velocity.z * pieceElapsed));
            branch.drawable.prepare();
            list.Load(&branch.drawable);
            branch.landDynamicPublished = true;
        }
        if (m_particleCount >
                particleCount(EXPLOSION_PARTICLE_PIECE) +
                    particleCount(EXPLOSION_PARTICLE_TRACED_PIECE) &&
            (_pGRDrawParticle != NULL || _pGRDrawAlphaSprite != NULL))
        {
            m_particleDrawable.prepare(m_position, m_attribute->m_radius);
            list.Load(&m_particleDrawable);
            m_particleDynamicPublished = true;
        }
    }

    virtual void endRender(CViewScene *scene)
    {
        if (scene == NULL || !m_started)
            return;
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
        {
            ExplosionParticleBranch &branch = m_particles[index];
            if (branch.landDynamicPublished)
            {
                scene->RemoveLandDynamic(&branch.drawable);
                branch.landDynamicPublished = false;
            }
        }
        if (m_particleDynamicPublished)
        {
            scene->RemoveLandDynamic(&m_particleDrawable);
            m_particleDynamicPublished = false;
        }
    }

    virtual void addNotify()
    {
        ct_Subject::addNotify();
        resetState();
    }

    virtual void removeNotify()
    {
        if (context != NULL)
        {
            context->removeEvent(EXPLOSION_START, getObjectID());
            context->removeEvent(EXPLOSION_MOVE, getObjectID());
            context->removeEvent(EXPLOSION_NEWPUFF, getObjectID());
            if (!IsNul(m_sound))
                SoundObjectState_RollbackOwned(context, &m_sound);
        }
        if (m_particleDynamicPublished)
        {
            CViewScene *scene = CViewScene::Current();
            if (scene != NULL)
                scene->RemoveLandDynamic(&m_particleDrawable);
            m_particleDynamicPublished = false;
        }
        releaseTraceQuota();
        releaseParticles();
        ct_Subject::removeNotify();
        resetState();
    }

    virtual int receiveEvent(KR_Event &event)
    {
        switch (event.label)
        {
        case EXPLOSION_START:
            return start(event);
        case EXPLOSION_MOVE:
            return expire(event);
        case EXPLOSION_NEWPUFF:
            return newPuff(event);
        case KR_WAKE_UP:
            return 1;
        default:
            return 0;
        }
    }

    bool clean()
    {
        return !m_started && !m_lightActive && m_attribute == NULL &&
               IsNul(m_sound) && m_particleCount == 0 &&
               m_nextMoveTime == 0.0 && m_previousMoveTime == 0.0 &&
               IsNul(m_damageOwner) && m_damageApplications == 0 &&
               m_position.x == 0.0 && m_position.y == 0.0 &&
               m_position.z == 0.0 && m_startTime == 0.0 &&
               m_landY == 0.0 && !m_landHeightReady &&
               !m_particleDynamicPublished && !m_traceQuotaHeld &&
               m_nextPuffTime == 0.0 && m_tracePuffsStarted == 0 &&
               IsNul(m_lastTracePuff);
    }

    bool lightActive() const { return m_lightActive; }
    bool started() const { return m_started; }
    bool retiring() const
    {
        return m_started && !m_lightActive && m_particleCount == 0 &&
               !m_traceQuotaHeld;
    }
    const KR_ObjectID &sound() const { return m_sound; }
    AttributeExplosion *attribute() const { return m_attribute; }
    double startTime() const { return m_startTime; }
    int particleCount() const { return m_particleCount; }
    double nextMoveTime() const { return m_nextMoveTime; }
    double nextPuffTime() const { return m_nextPuffTime; }
    bool traceQuotaHeld() const { return m_traceQuotaHeld; }
    int tracePuffsStarted() const { return m_tracePuffsStarted; }
    const KR_ObjectID &lastTracePuff() const { return m_lastTracePuff; }
    double previousMoveTime() const { return m_previousMoveTime; }

    int particleCount(ExplosionParticleBranchType type) const
    {
        int count = 0;
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
            if (m_particles[index].active &&
                m_particles[index].type == type)
                ++count;
        return count;
    }

    bool captureStable(SimulationContext *world,
                       StableExplosionRecord *record)
    {
        if (world == NULL || record == NULL || context != world)
            return FailActiveWorld(
                "live Explosion capture context is invalid");
        if (!m_started)
            return FailActiveWorld(
                "live Explosion has no committed START state");
        if (m_attribute == NULL)
            return FailActiveWorld(
                "live Explosion has no committed ExplosionAttr");
        if (m_particleDynamicPublished)
            return FailActiveWorld(
                "live Explosion particles are still published in a frame");
        record->name = ObjectName(world, getObjectID());
        record->attribute = ObjectName(world, m_attribute->getObjectID());
        if (record->name.empty() || record->attribute.empty())
            return FailActiveWorld(
                "live Explosion owner/attribute name is missing");

        KR_Event moving[2];
        KR_Event puff[2];
        const int movingCount = world->copyEvents(
            EXPLOSION_MOVE, getObjectID(), moving, 2);
        const int puffCount = world->copyEvents(
            EXPLOSION_NEWPUFF, getObjectID(), puff, 2);
        if (movingCount != 1 || moving[0].source != getObjectID() ||
            moving[0].destination != getObjectID() ||
            moving[0].data.size() != 0 ||
            (m_traceQuotaHeld ? puffCount != 1 : puffCount != 0) ||
            (puffCount == 1 &&
             (puff[0].source != getObjectID() ||
              puff[0].destination != getObjectID() ||
              puff[0].data.size() != 0)))
            return FailActiveWorld(
                "live Explosion private event boundary is invalid");

        record->hasSound = IsNul(m_sound) ? 0 : 1;
        if (record->hasSound &&
            (m_attribute->m_wav == NULL ||
             m_attribute->m_ctsndID == ct_NULLID ||
             !SoundObjectState_Matches(
                 m_sound, m_attribute->m_wav,
                 m_position.x, m_position.y, m_position.z,
                 true, true, 1)))
            return FailActiveWorld(
                "live Explosion owned Sound state is invalid");

        record->position = m_position;
        record->startTime = m_startTime;
        record->nextMoveTime = m_nextMoveTime;
        record->previousMoveTime = m_previousMoveTime;
        record->nextPuffTime = m_traceQuotaHeld ? m_nextPuffTime : 0.0;
        record->landY = m_landHeightReady ? m_landY : 0.0;
        record->lightActive = m_lightActive ? 1 : 0;
        record->landHeightReady = m_landHeightReady ? 1 : 0;
        record->traceQuotaHeld = m_traceQuotaHeld ? 1 : 0;
        record->movingTimeStamp = moving[0].timeStamp;
        record->hasPuffEvent = puffCount == 1 ? 1 : 0;
        record->puffTimeStamp = puffCount == 1 ? puff[0].timeStamp : 0.0;
        record->branches.clear();
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
        {
            const ExplosionParticleBranch &branch = m_particles[index];
            if (!branch.active)
                continue;
            if (branch.landDynamicPublished)
                return FailActiveWorld(
                    "live Explosion Piece is still published in a frame");
            StableExplosionBranch stable;
            stable.type = static_cast<int>(branch.type);
            stable.start = branch.start;
            stable.velocity = branch.velocity;
            stable.radius = branch.radius;
            stable.timeOfLife = branch.timeOfLife;
            stable.rayAngle = branch.rayAngle;
            stable.rayWidth = branch.rayWidth;
            stable.rotationOySpeed = branch.rotationOySpeed;
            stable.rotationOxSpeed = branch.rotationOxSpeed;
            stable.radiusA = branch.radiusA;
            stable.radiusB = branch.radiusB;
            stable.opacityA = branch.opacityA;
            stable.opacityB = branch.opacityB;
            stable.opacityC = branch.opacityC;
            stable.reciprocalMaximumTime = branch.reciprocalMaximumTime;
            stable.multiplier = branch.multiplier;
            stable.drift = branch.drift;
            stable.u0 = branch.u0;
            stable.v0 = branch.v0;
            stable.u1 = branch.u1;
            stable.v1 = branch.v1;
            stable.tailCount = branch.tailCount;
            stable.color = static_cast<std::uint32_t>(branch.color);
            stable.createPuffNow = branch.createPuffNow ? 1 : 0;
            record->branches.push_back(stable);
        }
        if (static_cast<int>(record->branches.size()) != m_particleCount)
            return FailActiveWorld(
                "live Explosion branch accounting is inconsistent");
        return true;
    }

    bool applyStable(const StableExplosionRecord &record,
                     AttributeExplosion *attribute)
    {
        if (context == NULL || attribute == NULL)
            return false;
        if (!clearStableRuntime())
            return false;

        m_attribute = attribute;
        m_position = record.position;
        m_startTime = record.startTime;
        m_nextMoveTime = record.nextMoveTime;
        m_previousMoveTime = record.previousMoveTime;
        m_nextPuffTime = record.nextPuffTime;
        m_landY = record.landY;
        m_lightActive = record.lightActive != 0;
        m_landHeightReady = record.landHeightReady != 0;
        m_traceQuotaHeld = record.traceQuotaHeld != 0;
        m_started = true;
        setPosition(record.position);

        for (std::size_t index = 0; index < record.branches.size(); ++index)
        {
            const StableExplosionBranch &stable = record.branches[index];
            ExplosionParticleBranch *branch = allocateParticle(
                static_cast<ExplosionParticleBranchType>(stable.type));
            if (branch == NULL)
                return false;
            branch->start = stable.start;
            branch->velocity = stable.velocity;
            branch->radius = stable.radius;
            branch->timeOfLife = stable.timeOfLife;
            branch->rayAngle = stable.rayAngle;
            branch->rayWidth = stable.rayWidth;
            branch->rotationOySpeed = stable.rotationOySpeed;
            branch->rotationOxSpeed = stable.rotationOxSpeed;
            branch->radiusA = stable.radiusA;
            branch->radiusB = stable.radiusB;
            branch->opacityA = stable.opacityA;
            branch->opacityB = stable.opacityB;
            branch->opacityC = stable.opacityC;
            branch->reciprocalMaximumTime =
                stable.reciprocalMaximumTime;
            branch->multiplier = stable.multiplier;
            branch->drift = stable.drift;
            branch->u0 = stable.u0;
            branch->v0 = stable.v0;
            branch->u1 = stable.u1;
            branch->v1 = stable.v1;
            branch->tailCount = stable.tailCount;
            branch->color = stable.color;
            branch->createPuffNow = stable.createPuffNow != 0;
            if (branch->type == EXPLOSION_PARTICLE_PIECE ||
                branch->type == EXPLOSION_PARTICLE_TRACED_PIECE)
                branch->skin.Attach(attribute->m_cacheSkin);
        }
        if (m_traceQuotaHeld)
            ++g_tracedExplosionParents;
        if (record.hasSound &&
            !SoundObjectState_StartOneShot(
                context, getObjectID(), attribute->m_ctsndID,
                attribute->m_wav, record.position, record.startTime,
                &m_sound))
            return false;

        KR_Event moving;
        moving.label = EXPLOSION_MOVE;
        moving.source = getObjectID();
        moving.destination = getObjectID();
        moving.timeStamp = record.movingTimeStamp;
        context->addEvent(moving);
        if (record.hasPuffEvent)
        {
            KR_Event puff;
            puff.label = EXPLOSION_NEWPUFF;
            puff.source = getObjectID();
            puff.destination = getObjectID();
            puff.timeStamp = record.puffTimeStamp;
            context->addEvent(puff);
        }
        return true;
    }

    bool clearStableRuntime()
    {
        if (context == NULL)
            return false;
        while (context->removeEvent(EXPLOSION_START, getObjectID()) == 1) {}
        while (context->removeEvent(EXPLOSION_MOVE, getObjectID()) == 1) {}
        while (context->removeEvent(
                   EXPLOSION_NEWPUFF, getObjectID()) == 1) {}
        if (!IsNul(m_sound) &&
            !SoundObjectState_RollbackOwned(context, &m_sound))
            return false;
        releaseTraceQuota();
        releaseParticles();
        resetState();
        return true;
    }

    void drawParticles();

 private:
    void resetState()
    {
        m_position = CFVector3(0.0, 0.0, 0.0);
        m_attribute = NULL;
        m_damageOwner = KR_ObjectID::NUL();
        m_sound = KR_ObjectID::NUL();
        m_startTime = 0.0;
        m_nextMoveTime = 0.0;
        m_previousMoveTime = 0.0;
        m_nextPuffTime = 0.0;
        m_damageApplications = 0;
        m_landY = 0.0;
        m_particleCount = 0;
        m_started = false;
        m_lightActive = false;
        m_landHeightReady = false;
        m_particleDynamicPublished = false;
        m_traceQuotaHeld = false;
        m_tracePuffsStarted = 0;
        m_lastTracePuff = KR_ObjectID::NUL();
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
            m_particles[index].reset();
    }

    ExplosionParticleBranch *allocateParticle(
        ExplosionParticleBranchType type)
    {
        if (m_particleCount >= kParticleBranchPerExplosion ||
            g_particleBranchesLive >= kParticleBranchCapacity)
            return NULL;
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
            if (!m_particles[index].active)
            {
                ExplosionParticleBranch *branch = &m_particles[index];
                branch->reset();
                branch->type = type;
                branch->active = true;
                ++m_particleCount;
                ++g_particleBranchesLive;
                return branch;
            }
        return NULL;
    }

    void deactivateParticle(ExplosionParticleBranch &branch)
    {
        if (!branch.active)
            return;
        if (branch.landDynamicPublished)
        {
            CViewScene *scene = CViewScene::Current();
            if (scene != NULL)
                scene->RemoveLandDynamic(&branch.drawable);
            branch.landDynamicPublished = false;
        }
        branch.reset();
        if (m_particleCount > 0)
            --m_particleCount;
        if (g_particleBranchesLive > 0)
            --g_particleBranchesLive;
    }

    void releaseParticles()
    {
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
            deactivateParticle(m_particles[index]);
    }

    void releaseTraceQuota()
    {
        if (context != NULL)
            context->removeEvent(EXPLOSION_NEWPUFF, getObjectID());
        m_nextPuffTime = 0.0;
        if (!m_traceQuotaHeld)
            return;
        m_traceQuotaHeld = false;
        if (g_tracedExplosionParents > 0)
            --g_tracedExplosionParents;
    }

    CFVector3 randomDirection(int seed, CFVector3 *spawnOffset) const
    {
        CFVector3 offset(
            context->rnd_f(-m_attribute->m_createRadius,
                           m_attribute->m_createRadius),
            context->rnd_f(-m_attribute->m_createRadius,
                           m_attribute->m_createRadius),
            context->rnd_f(-m_attribute->m_createRadius,
                           m_attribute->m_createRadius));
        if (spawnOffset != NULL)
            *spawnOffset = offset;
        if (Abs2(offset) > 1.0e-20)
            return offset;
        // Several retail presets permit a zero creation radius. The January
        // code normalizes that zero vector; choose a deterministic direction
        // while preserving the requested zero spawn offset.
        switch (seed % 3)
        {
        case 0: return CFVector3(1.0, 0.0, 0.0);
        case 1: return CFVector3(0.0, 1.0, 0.0);
        default: return CFVector3(0.0, 0.0, 1.0);
        }
    }

    int createParticles()
    {
        int created = 0;
        int count = 0;
        if (ExplosionAttributeState_ParticleVisualsResolved(context))
        {
            count = context->rnd_i(
                m_attribute->m_minRayCnt, m_attribute->m_maxRayCnt);
            for (int index = 0; index < count; ++index)
            {
                ExplosionParticleBranch *branch =
                    allocateParticle(EXPLOSION_PARTICLE_RAY);
                if (branch == NULL)
                    return created;
                branch->start = m_position;
                branch->timeOfLife = kRayTimeLife;
                branch->rayAngle = context->rnd_f(-kPi, kPi);
                branch->radius = context->rnd_f(
                    m_attribute->m_minRayLen, m_attribute->m_maxRayLen);
                branch->rayWidth = context->rnd_f(
                    m_attribute->m_minRayWidth,
                    m_attribute->m_maxRayWidth);
                branch->color = m_attribute->m_rayColor;
                ++created;
            }

            count = context->rnd_i(
                m_attribute->m_minPartCnt, m_attribute->m_maxPartCnt);
            // May 1999 preserves the January ordering bug: the later >0.08
            // quarter branch is unreachable after the >0.05 half branch.
            if (Session::m_frameSec > 0.05)
                count >>= 1;
            else if (Session::m_frameSec > 0.08)
                count >>= 2;
            for (int index = 0; index < count; ++index)
            {
                ExplosionParticleBranch *branch =
                    allocateParticle(EXPLOSION_PARTICLE_SIMPLE);
                if (branch == NULL)
                    return created;
                branch->radius = context->rnd_f(
                    m_attribute->m_minPartSize,
                    m_attribute->m_maxPartSize);
                switch (context->rnd_i() & 3)
                {
                case 0: branch->color = m_attribute->m_color0; break;
                case 1: branch->color = m_attribute->m_color1; break;
                case 2: branch->color = m_attribute->m_color2; break;
                default: branch->color = m_attribute->m_color3; break;
                }
                CFVector3 spawnOffset;
                const CFVector3 direction =
                    randomDirection(created, &spawnOffset);
                branch->start = m_position + spawnOffset;
                branch->velocity = Normal(direction) * context->rnd_f(
                    m_attribute->m_minPartSpeed,
                    m_attribute->m_maxPartSpeed);
                branch->timeOfLife = context->rnd_f(
                    m_attribute->m_minPartTimeLife,
                    m_attribute->m_maxPartTimeLife);
                ++created;
            }

            count = context->rnd_i(
                m_attribute->m_minPartSnCnt,
                m_attribute->m_maxPartSnCnt);
            if (Session::m_frameSec > 0.07)
                count >>= 2;
            for (int index = 0; index < count; ++index)
            {
                ExplosionParticleBranch *branch =
                    allocateParticle(EXPLOSION_PARTICLE_SNAKE);
                if (branch == NULL)
                    return created;
                branch->timeOfLife = context->rnd_f(
                    m_attribute->m_minPartSnTimeLife,
                    m_attribute->m_maxPartSnTimeLife);
                branch->radius = context->rnd_f(
                    m_attribute->m_minPartSnSize,
                    m_attribute->m_maxPartSnSize);
                CFVector3 spawnOffset;
                const CFVector3 direction =
                    randomDirection(created, &spawnOffset);
                branch->start = m_position + spawnOffset;
                branch->velocity = Normal(direction) * context->rnd_f(
                    m_attribute->m_minPartSpeed,
                    m_attribute->m_maxPartSpeed);
                branch->tailCount = m_attribute->m_snPartCnt;
                branch->color = m_attribute->m_colorSnHead;
                ++created;
            }
        }

        if (m_attribute->m_cacheSkin != NULL &&
            std::isfinite(m_attribute->m_cacheSkin->Radius()) &&
            m_attribute->m_cacheSkin->Radius() > 0.0)
        {
            count = context->rnd_i(
                m_attribute->m_minPieceCnt, m_attribute->m_maxPieceCnt);
            if (Session::m_frameSec > 0.1)
                count = 0;
            else if (Session::m_frameSec > 0.07)
                count >>= 2;
            for (int index = 0; index < count; ++index)
            {
                ExplosionParticleBranch *branch =
                    allocateParticle(EXPLOSION_PARTICLE_PIECE);
                if (branch == NULL)
                    return created;
                branch->timeOfLife = context->rnd_f(
                    m_attribute->m_minPieceTimeLife,
                    m_attribute->m_maxPieceTimeLife);
                branch->skin.Attach(m_attribute->m_cacheSkin);
                CFVector3 spawnOffset;
                const CFVector3 direction =
                    randomDirection(created, &spawnOffset);
                branch->start = m_position + spawnOffset;
                branch->velocity = Normal(direction) * context->rnd_f(
                    m_attribute->m_minPieceSpeed,
                    m_attribute->m_maxPieceSpeed);
                branch->rotationOySpeed = context->rnd_f(
                    m_attribute->m_minPieceOySpeed,
                    m_attribute->m_maxPieceOySpeed);
                branch->rotationOxSpeed = context->rnd_f(
                    m_attribute->m_minPieceOxSpeed,
                    m_attribute->m_maxPieceOxSpeed);
                ++created;
            }
        }

        if (ExplosionAttributeState_TraceReferencesResolved(context) &&
            m_attribute->m_cacheSkin != NULL &&
            std::isfinite(m_attribute->m_cacheSkin->Radius()) &&
            m_attribute->m_cacheSkin->Radius() > 0.0 &&
            g_tracedExplosionParents < kMaximumTracedExplosionParents)
        {
            count = context->rnd_i(
                m_attribute->m_minPieceSmokeCnt,
                m_attribute->m_maxPieceSmokeCnt);
            if (Session::m_frameSec > 0.1)
                count = 0;
            else if (Session::m_frameSec > 0.07)
                count >>= 2;
            int tracedCreated = 0;
            for (int index = 0; index < count; ++index)
            {
                ExplosionParticleBranch *branch =
                    allocateParticle(EXPLOSION_PARTICLE_TRACED_PIECE);
                if (branch == NULL)
                    break;
                branch->timeOfLife = context->rnd_f(
                    m_attribute->m_minPieceTimeLife,
                    m_attribute->m_maxPieceTimeLife);
                branch->skin.Attach(m_attribute->m_cacheSkin);
                CFVector3 spawnOffset;
                const CFVector3 direction =
                    randomDirection(created, &spawnOffset);
                branch->start = m_position + spawnOffset;
                branch->velocity = Normal(direction) * context->rnd_f(
                    m_attribute->m_minPieceSpeed * 2.0,
                    m_attribute->m_maxPieceSpeed * 2.0);
                branch->rotationOySpeed = context->rnd_f(
                    m_attribute->m_minPieceOySpeed,
                    m_attribute->m_maxPieceOySpeed);
                branch->rotationOxSpeed = context->rnd_f(
                    m_attribute->m_minPieceOxSpeed,
                    m_attribute->m_maxPieceOxSpeed);
                ++created;
                ++tracedCreated;
            }
            if (tracedCreated > 0)
            {
                m_traceQuotaHeld = true;
                ++g_tracedExplosionParents;
                if (!scheduleNextPuff(m_startTime))
                {
                    releaseTraceQuota();
                    for (int index = 0;
                         index < kParticleBranchPerExplosion; ++index)
                        if (m_particles[index].active &&
                            m_particles[index].type ==
                                EXPLOSION_PARTICLE_TRACED_PIECE)
                            deactivateParticle(m_particles[index]);
                }
            }
        }

        if (!ExplosionAttributeState_SmokeVisualsResolved(context))
            return created;
        count = context->rnd_i(
            m_attribute->m_minSmokeCnt, m_attribute->m_maxSmokeCnt);
        if (Session::m_frameSec > 0.1)
            count >>= 2;
        for (int index = 0; index < count; ++index)
        {
            ExplosionParticleBranch *branch =
                allocateParticle(EXPLOSION_PARTICLE_SMOKE);
            if (branch == NULL)
                return created;
            branch->timeOfLife = context->rnd_f(
                m_attribute->m_minSmokeTimeLife,
                m_attribute->m_maxSmokeTimeLife);
            branch->radiusA = context->rnd_f(
                m_attribute->m_minSmokeA, m_attribute->m_maxSmokeA);
            branch->radiusB = context->rnd_f(
                m_attribute->m_minSmokeB, m_attribute->m_maxSmokeB);
            branch->radius = context->rnd_f(
                m_attribute->m_minSmokeC, m_attribute->m_maxSmokeC);
            branch->opacityA = context->rnd_f(
                m_attribute->m_minSmokeTA, m_attribute->m_maxSmokeTA);
            branch->opacityB = context->rnd_f(
                m_attribute->m_minSmokeTB, m_attribute->m_maxSmokeTB);
            branch->opacityC = context->rnd_f(
                m_attribute->m_minSmokeTC, m_attribute->m_maxSmokeTC);
            CFVector3 spawnOffset;
            const CFVector3 direction =
                randomSmokeDirection(created, &spawnOffset);
            branch->start = m_position + spawnOffset;
            branch->velocity = Normal(direction) * context->rnd_f(
                m_attribute->m_minSmokeSpeed,
                m_attribute->m_maxSmokeSpeed);
            branch->multiplier = context->rnd_f(
                m_attribute->m_minMulSpeed, m_attribute->m_maxMulSpeed);
            int u0 = 2;
            int v0 = 2;
            int u1 = 126;
            int v1 = 126;
            const int morph = context->rnd_i();
            if ((morph & 1) != 0)
                std::swap(u0, u1);
            if ((morph & 2) != 0)
                std::swap(v0, v1);
            const int atlas = context->rnd_i() & 3;
            const int x = (atlas & 1) * 128;
            const int y = (atlas >> 1) * 128;
            branch->u0 = (x + u0) << 16;
            branch->v0 = (y + v0) << 16;
            branch->u1 = (x + u1) << 16;
            branch->v1 = (y + v1) << 16;
            double maximumTime = PositiveBranchRoot(
                branch->opacityA, branch->opacityB, branch->opacityC);
            const double radiusTime = PositiveBranchRoot(
                branch->radiusA, branch->radiusB, branch->radius);
            if (radiusTime < maximumTime)
                maximumTime = radiusTime;
            if (branch->timeOfLife < maximumTime)
                maximumTime = branch->timeOfLife;
            if (!std::isfinite(maximumTime) || maximumTime <= 0.0)
            {
                deactivateParticle(*branch);
                continue;
            }
            branch->reciprocalMaximumTime = 1.0 / maximumTime;
            CFMatrix3x4 rotation;
            rotation.LoadIdentity();
            rotation.RotateOzL(m_attribute->m_ofsVAngle);
            rotation.RotateOyL(m_attribute->m_ofsHAngle);
            branch->drift = (rotation * CFVector3(0.0, 1.0, 0.0)) *
                m_attribute->m_ofsSpeed;
            ++created;
        }
        return created;
    }

    CFVector3 randomSmokeDirection(int seed, CFVector3 *spawnOffset) const
    {
        CFVector3 offset(
            context->rnd_f(-m_attribute->m_createSmokeRadius,
                           m_attribute->m_createSmokeRadius),
            context->rnd_f(-m_attribute->m_createSmokeRadius,
                           m_attribute->m_createSmokeRadius),
            context->rnd_f(-m_attribute->m_createSmokeRadius,
                           m_attribute->m_createSmokeRadius));
        if (spawnOffset != NULL)
            *spawnOffset = offset;
        if (Abs2(offset) > 1.0e-20)
            return offset;
        switch (seed % 3)
        {
        case 0: return CFVector3(1.0, 0.0, 0.0);
        case 1: return CFVector3(0.0, 1.0, 0.0);
        default: return CFVector3(0.0, 0.0, 1.0);
        }
    }

    void updateParticles(double elapsed, double delta)
    {
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
        {
            ExplosionParticleBranch &branch = m_particles[index];
            if (!branch.active)
                continue;
            if (branch.type == EXPLOSION_PARTICLE_PIECE ||
                branch.type == EXPLOSION_PARTICLE_TRACED_PIECE)
            {
                const CFVector3 position = branch.start + CFVector3(
                    branch.velocity.x * elapsed,
                    branch.velocity.y * elapsed - 4.9 * elapsed * elapsed,
                    branch.velocity.z * elapsed);
                if (elapsed > branch.timeOfLife ||
                    (m_landHeightReady && position.y < m_landY))
                {
                    deactivateParticle(branch);
                    continue;
                }
                if (branch.type == EXPLOSION_PARTICLE_TRACED_PIECE &&
                    branch.createPuffNow)
                {
                    SmokeStaticStartRequest request = {
                        position, m_previousMoveTime, getObjectID(),
                        m_attribute->m_smokeTableID,
                        m_attribute->m_smokeAttrID,
                        m_attribute->m_traceSmokeName, "Smok.Static"};
                    KR_ObjectID child = KR_ObjectID::NUL();
                    if (SmokeSubjectState_StartAtPosition(
                            context, request, &child))
                    {
                        m_lastTracePuff = child;
                        ++m_tracePuffsStarted;
                        ++g_tracePuffsStarted;
                    }
                    branch.createPuffNow = false;
                }
                continue;
            }
            if (branch.type == EXPLOSION_PARTICLE_SMOKE)
            {
                if (elapsed > branch.timeOfLife)
                {
                    deactivateParticle(branch);
                    continue;
                }
                const double velocityScale =
                    branch.multiplier * (1.0 - delta);
                branch.velocity *= velocityScale;
                branch.start += branch.velocity * delta +
                                branch.drift * delta;
                branch.drift *= m_attribute->m_ofsSpeedMul;
                continue;
            }
            if (elapsed <= branch.timeOfLife)
                continue;
            if (branch.type == EXPLOSION_PARTICLE_SNAKE &&
                branch.tailCount > 0)
                --branch.tailCount;
            else
                deactivateParticle(branch);
        }
        if (m_traceQuotaHeld &&
            particleCount(EXPLOSION_PARTICLE_TRACED_PIECE) == 0)
            releaseTraceQuota();
    }

    bool scheduleNextMove(double currentTime)
    {
        double next = currentTime + m_attribute->m_moveTimeInc;
        const double deadline =
            m_startTime + kExplosionMaximumTimeLife;
        if (next > deadline)
            next = deadline;
        if (!std::isfinite(next) || next <= currentTime)
            return false;
        KR_Event move;
        move.label = EXPLOSION_MOVE;
        move.source = getObjectID();
        move.destination = getObjectID();
        move.timeStamp = next;
        context->addEvent(move);
        m_nextMoveTime = next;
        return true;
    }

    bool scheduleNextPuff(double currentTime)
    {
        if (!m_traceQuotaHeld || m_attribute == NULL ||
            particleCount(EXPLOSION_PARTICLE_TRACED_PIECE) == 0)
            return false;
        const double next =
            currentTime + m_attribute->m_traceNewPuffTime;
        const double deadline =
            m_startTime + kExplosionMaximumTimeLife;
        if (!std::isfinite(next) || next <= currentTime || next > deadline)
            return false;
        KR_Event puff;
        puff.label = EXPLOSION_NEWPUFF;
        puff.source = getObjectID();
        puff.destination = getObjectID();
        puff.timeStamp = next;
        context->addEvent(puff);
        m_nextPuffTime = next;
        return true;
    }

    int newPuff(KR_Event &event)
    {
        if (!m_started || m_attribute == NULL || context == NULL ||
            !m_traceQuotaHeld || event.source != getObjectID() ||
            event.destination != getObjectID() || event.data.size() != 0 ||
            !std::isfinite(event.timeStamp) ||
            !NearlyEqual(event.timeStamp, m_nextPuffTime))
            return 0;
        int tracedPieces = 0;
        for (int index = 0; index < kParticleBranchPerExplosion; ++index)
        {
            ExplosionParticleBranch &branch = m_particles[index];
            if (!branch.active ||
                branch.type != EXPLOSION_PARTICLE_TRACED_PIECE)
                continue;
            branch.createPuffNow = true;
            ++tracedPieces;
        }
        m_nextPuffTime = 0.0;
        if (tracedPieces == 0 || !scheduleNextPuff(event.timeStamp))
            releaseTraceQuota();
        return 1;
    }

    int start(KR_Event &event)
    {
        const int recoveredSize = static_cast<int>(
            sizeof(int) + sizeof(double) * 3 + sizeof(KR_ObjectID));
        const int retailSize = static_cast<int>(
            sizeof(int) + sizeof(double) * 3);
        if (m_started || context == NULL ||
            event.destination != getObjectID() ||
            !std::isfinite(event.timeStamp) || event.timeStamp < 0.1)
            return 0;
        s_EventData &data = event.data.open(EDO_READ);
        const int payloadSize = data.remaining();
        if (payloadSize != recoveredSize && payloadSize != retailSize)
        {
            data.close();
            return 0;
        }
        if (payloadSize == recoveredSize &&
            event.source != getObjectID())
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
            .getDouble(position.z);
        if (payloadSize == recoveredSize)
            data.getObjectID(damageOwner);
        else
            damageOwner = event.source;
        data.close();

        // Recovered queue/restore events use a self source and carry the
        // symbolic damage owner explicitly. Retail createExplosion() uses
        // the shorter packet and places that owner in event.source instead.
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
        m_previousMoveTime = event.timeStamp;
        setPosition(position);
        CViewScene *scene = CViewScene::Current();
        if (scene != NULL && scene->GetTerrain() != NULL)
        {
            CFVector3 normal;
            double landY = 0.0;
            scene->GetTerrain()->GetPlane(position, normal, landY);
            if (std::isfinite(landY) && FiniteVector(normal))
            {
                m_landY = landY;
                m_landHeightReady = true;
            }
        }
        m_damageApplications = ApplyRadialDamage(
            context, *attribute, position, event.timeStamp, damageOwner);
        ++g_executedCommands;
        g_damageApplications += m_damageApplications;

        if (attribute->m_soundName[0] != 0 && attribute->m_wav != NULL &&
            attribute->m_ctsndID != ct_NULLID)
            SoundObjectState_StartOneShot(
                context, getObjectID(), attribute->m_ctsndID,
                attribute->m_wav, position, event.timeStamp, &m_sound);

        createParticles();
        if (LightAttributeReady(attribute))
            m_lightActive = true;
        if ((m_lightActive || m_particleCount > 0) &&
            scheduleNextMove(event.timeStamp))
            return 1;
        const KR_ObjectID self = getObjectID();
        context->removeObject(self);
        return 1;
    }

    int expire(KR_Event &event)
    {
        if (!m_started || m_attribute == NULL ||
            context == NULL || event.source != getObjectID() ||
            event.destination != getObjectID() || event.data.size() != 0 ||
            !std::isfinite(event.timeStamp) ||
            !NearlyEqual(event.timeStamp, m_nextMoveTime))
            return 0;
        const double elapsed = event.timeStamp - m_startTime;
        const double delta = event.timeStamp - m_previousMoveTime;
        if (!std::isfinite(elapsed) || elapsed < 0.0 ||
            !std::isfinite(delta) || delta < 0.0)
            return 0;
        updateParticles(elapsed, delta);
        m_previousMoveTime = event.timeStamp;
        if (m_lightActive &&
            elapsed > m_attribute->m_lightTimeLife)
            m_lightActive = false;
        if (elapsed >= kExplosionMaximumTimeLife)
        {
            releaseParticles();
            m_lightActive = false;
        }
        if (m_particleCount == 0 && !m_lightActive)
        {
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
            return 1;
        }
        if (!scheduleNextMove(event.timeStamp))
        {
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
        }
        return 1;
    }

    AttributeExplosion *m_attribute;
    KR_ObjectID m_damageOwner;
    KR_ObjectID m_sound;
    double m_startTime;
    double m_nextMoveTime;
    double m_previousMoveTime;
    double m_nextPuffTime;
    int m_damageApplications;
    ExplosionParticleBranch
        m_particles[kParticleBranchPerExplosion];
    ExplosionParticleDrawable m_particleDrawable;
    int m_particleCount;
    bool m_started;
    bool m_lightActive;
    bool m_landHeightReady;
    bool m_particleDynamicPublished;
    bool m_traceQuotaHeld;
    int m_tracePuffsStarted;
    KR_ObjectID m_lastTracePuff;
    double m_landY;
};

void ExplosionParticleDrawable::prepare(
    const CFVector3 &position, double radius)
{
    m_dynBase = m_dynBase1 = m_bump.start = position;
    m_bump.vel = CFVector3(0.0, 0.0, 0.0);
    m_bump.fTime = 0.0;
    m_bump.fRadius = radius > 0.0 ? radius : 1.0;
}

void ExplosionParticleDrawable::Draw()
{
    if (m_owner != NULL)
        m_owner->drawParticles();
}

void BoundedExplosion::drawParticles()
{
    if (!m_started || m_attribute == NULL || m_particleCount <= 0)
        return;
    double elapsed = Session::m_moment - m_startTime;
    if (!std::isfinite(elapsed))
        return;
    if (elapsed < 0.0)
        elapsed = 0.0;

    const auto drawWorld = [](const CFVector3 &position, double radius,
                              unsigned long color) {
        const CFVector3 view = CViewObject::m_viewPointDirSMx * position;
        if (!std::isfinite(view.z) ||
            view.z < CViewObject::m_fFrontClip)
            return;
        const double inverse = 1.0 / view.z;
        int size = Round(radius * CViewObject::m_viewPointScale.x * inverse);
        if (size < 1)
            size = 1;
        const int inverseZ = static_cast<int>(65536.0 * inverse);
        if (inverseZ <= 0)
            return;
        GRDrawParticle(Round(view.x * inverse), Round(view.y * inverse),
                       size, inverseZ, color);
    };

    for (int index = 0; index < kParticleBranchPerExplosion; ++index)
    {
        ExplosionParticleBranch &branch = m_particles[index];
        if (!branch.active)
            continue;
        if (branch.type == EXPLOSION_PARTICLE_SMOKE)
        {
            if (_pGRDrawAlphaSprite == NULL ||
                m_attribute->m_hTexture == NULL)
                continue;
            const double smokeElapsed = m_previousMoveTime - m_startTime;
            const double smokeElapsed2 = smokeElapsed * smokeElapsed;
            const double radius = branch.radiusA * smokeElapsed2 +
                branch.radiusB * smokeElapsed + branch.radius;
            const double alpha = branch.opacityA * smokeElapsed2 +
                branch.opacityB * smokeElapsed + branch.opacityC;
            if (alpha < 0.0 || radius < 0.01)
            {
                deactivateParticle(branch);
                continue;
            }
            const CFVector3 view =
                CViewObject::m_viewPointDirSMx * branch.start;
            if (!std::isfinite(view.z) ||
                view.z < CViewObject::m_fFrontClip)
                continue;
            const double inverse = 1.0 / view.z;
            int screenWidth = Round(
                radius * CViewObject::m_viewPointScale.x * inverse);
            if (screenWidth < 1)
                screenWidth = 1;
            const int screenX = Round(view.x * inverse);
            const int screenY = Round(view.y * inverse);
            const int inverseZ = static_cast<int>(65536.0 * inverse);
            if (inverseZ <= 0)
                continue;
            SGRAlphaSprite sprite = {};
            sprite.x0 = screenX - screenWidth / 2;
            sprite.y0 = screenY - screenWidth / 2;
            sprite.x1 = sprite.x0 + screenWidth;
            sprite.y1 = sprite.y0 + screenWidth;
            sprite.u0 = branch.u0;
            sprite.v0 = branch.v0;
            sprite.u1 = branch.u1;
            sprite.v1 = branch.v1;
            int colorIndex = static_cast<int>(
                smokeElapsed * branch.reciprocalMaximumTime *
                (AttributeExplosion::COLLINE * 3 - 1));
            if (colorIndex < 0)
                colorIndex = 0;
            else if (colorIndex >= AttributeExplosion::COLLINE * 3)
                colorIndex = AttributeExplosion::COLLINE * 3 - 1;
            sprite.color = m_attribute->m_colBuf[colorIndex];
            sprite.opacity = static_cast<int>(alpha);
            if (sprite.opacity < 0)
                sprite.opacity = 0;
            else if (sprite.opacity > 255)
                sprite.opacity = 255;
            sprite.iz = inverseZ;
            sprite.hTexture = m_attribute->m_hTexture;
            GRDrawAlphaSprite(&sprite);
            continue;
        }
        if (branch.type == EXPLOSION_PARTICLE_PIECE ||
            branch.type == EXPLOSION_PARTICLE_TRACED_PIECE)
            continue;
        if (_pGRDrawParticle == NULL)
            continue;
        if (branch.type == EXPLOSION_PARTICLE_RAY)
        {
            const CFVector3 center =
                CViewObject::m_viewPointDirSMx * m_position;
            if (center.z < CViewObject::m_fFrontClip)
                continue;
            const double inverse = 1.0 / center.z;
            const double screenLength = elapsed * branch.radius *
                CViewObject::m_viewPointScale.x * inverse;
            int samples = static_cast<int>(std::fabs(screenLength) / 3.0);
            if (samples < 2)
                samples = 2;
            else if (samples > 24)
                samples = 24;
            const double dx = std::cos(branch.rayAngle) * screenLength;
            const double dy = std::sin(branch.rayAngle) * screenLength;
            const int inverseZ = static_cast<int>(65536.0 * inverse);
            for (int sample = 1; sample <= samples; ++sample)
            {
                const double fraction =
                    static_cast<double>(sample) / samples;
                int size = Round(std::fabs(screenLength) *
                                 branch.rayWidth * fraction);
                if (size < 1)
                    size = 1;
                GRDrawParticle(
                    Round(center.x * inverse + dx * fraction),
                    Round(center.y * inverse + dy * fraction),
                    size, inverseZ, branch.color);
            }
            continue;
        }

        if (branch.type == EXPLOSION_PARTICLE_SIMPLE)
        {
            const CFVector3 position = branch.start + CFVector3(
                branch.velocity.x * elapsed,
                branch.velocity.y * elapsed - 4.9 * elapsed * elapsed,
                branch.velocity.z * elapsed);
            drawWorld(position, branch.radius, branch.color);
            continue;
        }

        const int tailCount = branch.tailCount > 0
                                  ? branch.tailCount
                                  : 1;
        double tailStart = elapsed - m_attribute->m_snDeltaT;
        if (tailStart < 0.0)
            tailStart = 0.0;
        for (int tail = 0; tail < tailCount; ++tail)
        {
            const double fraction =
                static_cast<double>(tail + 1) / tailCount;
            const double sampleTime = tailStart +
                (elapsed - tailStart) * fraction;
            const CFVector3 position = branch.start + CFVector3(
                branch.velocity.x * sampleTime,
                branch.velocity.y * sampleTime -
                    4.9 * sampleTime * sampleTime,
                branch.velocity.z * sampleTime);
            drawWorld(position, branch.radius * fraction,
                      m_attribute->m_colorSnTail);
        }
        const CFVector3 head = branch.start + CFVector3(
            branch.velocity.x * elapsed,
            branch.velocity.y * elapsed - 4.9 * elapsed * elapsed,
            branch.velocity.z * elapsed);
        drawWorld(head, branch.radius, m_attribute->m_colorSnHead);
        drawWorld(head, branch.radius * 0.5,
                  m_attribute->m_colorSnCenter);
    }
}

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
        g_particleBranchesLive = 0;
        g_pieceDrawCalls = 0;
        g_tracedExplosionParents = 0;
        g_tracePuffsStarted = 0;
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
        g_particleBranchesLive = 0;
        g_pieceDrawCalls = 0;
        g_tracedExplosionParents = 0;
        g_tracePuffsStarted = 0;
        ResetImpulseBinding();
    }

    virtual ct_Object *getObjectPTR(int index)
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "BoundedExplosionTable::getObjectPTR");
        return &m_table[index];
    }

    virtual bool isRendering() { return true; }
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

    void collect(std::vector<BoundedExplosion *> *objects) const
    {
        if (objects == NULL)
            return;
        objects->clear();
        for (ct_Subject *object = findFirstSubject(); object != NULL;
             object = findNextSubject(object))
            objects->push_back(static_cast<BoundedExplosion *>(object));
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

bool IsBool(int value)
{
    return value == 0 || value == 1;
}

bool CollectStableRoster(SimulationContext *context,
                         std::vector<BoundedExplosion *> *objects)
{
    if (context == NULL || objects == NULL ||
        g_arena.getContext() != context)
        return false;
    g_explosionTable.collect(objects);
    objects->erase(
        std::remove_if(objects->begin(), objects->end(),
                       [context](BoundedExplosion *object)
                       {
                           return object == NULL ||
                               !context->isExist(object->getObjectID()) ||
                               ObjectName(context, object->getObjectID())
                                   .empty() ||
                               object->retiring() ||
                               (!object->started() &&
                                context->copyEventsTo(
                                    EXPLOSION_START,
                                    object->getObjectID(), NULL, 0) != 0);
                       }),
        objects->end());
    std::sort(objects->begin(), objects->end(),
              [context](BoundedExplosion *left,
                        BoundedExplosion *right) {
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

bool ValidateStableBranch(const StableExplosionBranch &branch)
{
    return branch.type >= EXPLOSION_PARTICLE_SIMPLE &&
           branch.type <= EXPLOSION_PARTICLE_SMOKE &&
           FiniteVector(branch.start) && FiniteVector(branch.velocity) &&
           FiniteVector(branch.drift) &&
           std::isfinite(branch.radius) &&
           std::isfinite(branch.timeOfLife) && branch.timeOfLife >= 0.0 &&
           std::isfinite(branch.rayAngle) &&
           std::isfinite(branch.rayWidth) &&
           std::isfinite(branch.rotationOySpeed) &&
           std::isfinite(branch.rotationOxSpeed) &&
           std::isfinite(branch.radiusA) &&
           std::isfinite(branch.radiusB) &&
           std::isfinite(branch.opacityA) &&
           std::isfinite(branch.opacityB) &&
           std::isfinite(branch.opacityC) &&
           std::isfinite(branch.reciprocalMaximumTime) &&
           std::isfinite(branch.multiplier) &&
           branch.tailCount >= 0 && branch.tailCount <= 4096 &&
           IsBool(branch.createPuffNow);
}

bool ValidateStableRecord(const StableExplosionRecord &record)
{
    if (record.name.empty() || record.attribute.empty() ||
        record.name.size() > kMaximumActiveWorldString ||
        record.attribute.size() > kMaximumActiveWorldString)
        return FailActiveWorld("EXP1 owner/attribute identity is invalid");
    if (!FiniteVector(record.position) ||
        !std::isfinite(record.startTime) || record.startTime < 0.1)
        return FailActiveWorld("EXP1 position/start time is invalid");
    if (
        !std::isfinite(record.nextMoveTime) ||
        record.nextMoveTime <= record.startTime ||
        !std::isfinite(record.previousMoveTime) ||
        record.previousMoveTime < record.startTime ||
        record.previousMoveTime >= record.nextMoveTime)
        return FailActiveWorld("EXP1 MOVE time chain is invalid");
    if (!std::isfinite(record.landY) ||
        !IsBool(record.lightActive) || !IsBool(record.landHeightReady) ||
        !IsBool(record.traceQuotaHeld) || !IsBool(record.hasSound) ||
        !std::isfinite(record.movingTimeStamp) ||
        record.movingTimeStamp != record.nextMoveTime)
        return FailActiveWorld("EXP1 parent flags/MOVE event are invalid");
    if (!IsBool(record.hasPuffEvent) ||
        record.hasPuffEvent != record.traceQuotaHeld)
        return FailActiveWorld("EXP1 NEWPUFF ownership is invalid");
    if (record.branches.size() > kParticleBranchPerExplosion ||
        (record.branches.empty() && !record.lightActive))
        return FailActiveWorld("EXP1 live branch roster is invalid");
    if (!record.landHeightReady && record.landY != 0.0)
        return FailActiveWorld("EXP1 absent terrain sample is not canonical");
    if (record.hasPuffEvent)
    {
        if (!std::isfinite(record.nextPuffTime) ||
            !std::isfinite(record.puffTimeStamp) ||
            record.nextPuffTime != record.puffTimeStamp ||
            record.nextPuffTime <= record.startTime)
            return FailActiveWorld("EXP1 NEWPUFF time chain is invalid");
    }
    else if (record.nextPuffTime != 0.0 || record.puffTimeStamp != 0.0)
        return FailActiveWorld("EXP1 absent NEWPUFF state is not canonical");
    int traced = 0;
    for (std::size_t index = 0; index < record.branches.size(); ++index)
    {
        if (!ValidateStableBranch(record.branches[index]))
            return FailActiveWorld("EXP1 particle branch state is invalid");
        if (record.branches[index].type ==
            EXPLOSION_PARTICLE_TRACED_PIECE)
            ++traced;
    }
    if (record.traceQuotaHeld && traced == 0)
        return FailActiveWorld("EXP1 traced-parent quota has no owner branch");
    return true;
}

bool CollectStableRecords(SimulationContext *context,
                          std::vector<StableExplosionRecord> *records)
{
    std::vector<BoundedExplosion *> objects;
    if (records == NULL || !CollectStableRoster(context, &objects))
        return false;
    records->clear();
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        StableExplosionRecord record;
        if (!objects[index]->captureStable(context, &record) ||
            !ValidateStableRecord(record))
            return false;
        records->push_back(record);
    }
    return true;
}

bool RosterMatches(const std::vector<BoundedExplosion *> &objects,
                   SimulationContext *context,
                   const std::vector<StableExplosionRecord> &records)
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

void PutVector(std::vector<unsigned char> *bytes, const CFVector3 &value)
{
    PutDouble(bytes, value.x);
    PutDouble(bytes, value.y);
    PutDouble(bytes, value.z);
}

bool PutString(std::vector<unsigned char> *bytes,
               const std::string &value)
{
    if (bytes == NULL || value.size() > kMaximumActiveWorldString ||
        value.find('\0') != std::string::npos)
        return false;
    PutU32(bytes, static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
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

bool GetDouble(const std::vector<unsigned char> &bytes,
               std::size_t *offset, double *value)
{
    if (offset == NULL || value == NULL || *offset > bytes.size() ||
        bytes.size() - *offset < 8)
        return false;
    std::uint64_t bits = 0;
    for (int shift = 0; shift < 64; shift += 8)
        bits |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
}

bool GetVector(const std::vector<unsigned char> &bytes,
               std::size_t *offset, CFVector3 *value)
{
    return value != NULL && GetDouble(bytes, offset, &value->x) &&
           GetDouble(bytes, offset, &value->y) &&
           GetDouble(bytes, offset, &value->z);
}

bool GetString(const std::vector<unsigned char> &bytes,
               std::size_t *offset, std::string *value)
{
    std::uint32_t size = 0;
    if (offset == NULL || value == NULL || !GetU32(bytes, offset, &size) ||
        size > kMaximumActiveWorldString || *offset > bytes.size() ||
        bytes.size() - *offset < size)
        return false;
    if (size == 0)
        value->clear();
    else
        value->assign(reinterpret_cast<const char *>(&bytes[*offset]), size);
    *offset += size;
    return value->find('\0') == std::string::npos;
}

void PutBranch(std::vector<unsigned char> *bytes,
               const StableExplosionBranch &branch)
{
    PutU32(bytes, static_cast<std::uint32_t>(branch.type));
    PutVector(bytes, branch.start);
    PutVector(bytes, branch.velocity);
    PutDouble(bytes, branch.radius);
    PutDouble(bytes, branch.timeOfLife);
    PutDouble(bytes, branch.rayAngle);
    PutDouble(bytes, branch.rayWidth);
    PutDouble(bytes, branch.rotationOySpeed);
    PutDouble(bytes, branch.rotationOxSpeed);
    PutDouble(bytes, branch.radiusA);
    PutDouble(bytes, branch.radiusB);
    PutDouble(bytes, branch.opacityA);
    PutDouble(bytes, branch.opacityB);
    PutDouble(bytes, branch.opacityC);
    PutDouble(bytes, branch.reciprocalMaximumTime);
    PutDouble(bytes, branch.multiplier);
    PutVector(bytes, branch.drift);
    PutU32(bytes, static_cast<std::uint32_t>(branch.u0));
    PutU32(bytes, static_cast<std::uint32_t>(branch.v0));
    PutU32(bytes, static_cast<std::uint32_t>(branch.u1));
    PutU32(bytes, static_cast<std::uint32_t>(branch.v1));
    PutU32(bytes, static_cast<std::uint32_t>(branch.tailCount));
    PutU32(bytes, branch.color);
    PutU32(bytes, static_cast<std::uint32_t>(branch.createPuffNow));
}

bool GetBranch(const std::vector<unsigned char> &bytes,
               std::size_t *offset, StableExplosionBranch *branch)
{
    std::uint32_t type = 0, u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    std::uint32_t tailCount = 0, color = 0, createPuffNow = 0;
    if (branch == NULL || !GetU32(bytes, offset, &type) ||
        !GetVector(bytes, offset, &branch->start) ||
        !GetVector(bytes, offset, &branch->velocity) ||
        !GetDouble(bytes, offset, &branch->radius) ||
        !GetDouble(bytes, offset, &branch->timeOfLife) ||
        !GetDouble(bytes, offset, &branch->rayAngle) ||
        !GetDouble(bytes, offset, &branch->rayWidth) ||
        !GetDouble(bytes, offset, &branch->rotationOySpeed) ||
        !GetDouble(bytes, offset, &branch->rotationOxSpeed) ||
        !GetDouble(bytes, offset, &branch->radiusA) ||
        !GetDouble(bytes, offset, &branch->radiusB) ||
        !GetDouble(bytes, offset, &branch->opacityA) ||
        !GetDouble(bytes, offset, &branch->opacityB) ||
        !GetDouble(bytes, offset, &branch->opacityC) ||
        !GetDouble(bytes, offset, &branch->reciprocalMaximumTime) ||
        !GetDouble(bytes, offset, &branch->multiplier) ||
        !GetVector(bytes, offset, &branch->drift) ||
        !GetU32(bytes, offset, &u0) || !GetU32(bytes, offset, &v0) ||
        !GetU32(bytes, offset, &u1) || !GetU32(bytes, offset, &v1) ||
        !GetU32(bytes, offset, &tailCount) ||
        !GetU32(bytes, offset, &color) ||
        !GetU32(bytes, offset, &createPuffNow))
        return false;
    branch->type = static_cast<int>(type);
    branch->u0 = static_cast<int>(static_cast<std::int32_t>(u0));
    branch->v0 = static_cast<int>(static_cast<std::int32_t>(v0));
    branch->u1 = static_cast<int>(static_cast<std::int32_t>(u1));
    branch->v1 = static_cast<int>(static_cast<std::int32_t>(v1));
    branch->tailCount = static_cast<int>(tailCount);
    branch->color = color;
    branch->createPuffNow = static_cast<int>(createPuffNow);
    return ValidateStableBranch(*branch);
}

bool PutRecord(std::vector<unsigned char> *bytes,
               const StableExplosionRecord &record)
{
    if (!PutString(bytes, record.name) ||
        !PutString(bytes, record.attribute))
        return false;
    PutVector(bytes, record.position);
    PutDouble(bytes, record.startTime);
    PutDouble(bytes, record.nextMoveTime);
    PutDouble(bytes, record.previousMoveTime);
    PutDouble(bytes, record.nextPuffTime);
    PutDouble(bytes, record.landY);
    PutU32(bytes, static_cast<std::uint32_t>(record.lightActive));
    PutU32(bytes, static_cast<std::uint32_t>(record.landHeightReady));
    PutU32(bytes, static_cast<std::uint32_t>(record.traceQuotaHeld));
    PutU32(bytes, static_cast<std::uint32_t>(record.hasSound));
    PutDouble(bytes, record.movingTimeStamp);
    PutU32(bytes, static_cast<std::uint32_t>(record.hasPuffEvent));
    PutDouble(bytes, record.puffTimeStamp);
    PutU32(bytes, static_cast<std::uint32_t>(record.branches.size()));
    for (std::size_t index = 0; index < record.branches.size(); ++index)
        PutBranch(bytes, record.branches[index]);
    return true;
}

bool GetRecord(const std::vector<unsigned char> &bytes,
               std::size_t *offset, StableExplosionRecord *record)
{
    std::uint32_t light = 0, land = 0, trace = 0, sound = 0;
    std::uint32_t hasPuff = 0, count = 0;
    if (record == NULL || !GetString(bytes, offset, &record->name) ||
        !GetString(bytes, offset, &record->attribute) ||
        !GetVector(bytes, offset, &record->position) ||
        !GetDouble(bytes, offset, &record->startTime) ||
        !GetDouble(bytes, offset, &record->nextMoveTime) ||
        !GetDouble(bytes, offset, &record->previousMoveTime) ||
        !GetDouble(bytes, offset, &record->nextPuffTime) ||
        !GetDouble(bytes, offset, &record->landY) ||
        !GetU32(bytes, offset, &light) || !GetU32(bytes, offset, &land) ||
        !GetU32(bytes, offset, &trace) || !GetU32(bytes, offset, &sound) ||
        !GetDouble(bytes, offset, &record->movingTimeStamp) ||
        !GetU32(bytes, offset, &hasPuff) ||
        !GetDouble(bytes, offset, &record->puffTimeStamp) ||
        !GetU32(bytes, offset, &count) ||
        count > kParticleBranchPerExplosion)
        return false;
    record->lightActive = static_cast<int>(light);
    record->landHeightReady = static_cast<int>(land);
    record->traceQuotaHeld = static_cast<int>(trace);
    record->hasSound = static_cast<int>(sound);
    record->hasPuffEvent = static_cast<int>(hasPuff);
    record->branches.clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableExplosionBranch branch;
        if (!GetBranch(bytes, offset, &branch))
            return false;
        record->branches.push_back(branch);
    }
    return ValidateStableRecord(*record);
}

bool EncodeStableRecords(
    const std::vector<StableExplosionRecord> &records,
    std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumActiveWorldExplosions)
        return false;
    bytes->clear();
    PutU32(bytes, kExplosionActiveWorldMagic);
    PutU32(bytes, kExplosionActiveWorldVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!ValidateStableRecord(records[index]) ||
            (index != 0 && records[index - 1].name > records[index].name) ||
            !PutRecord(bytes, records[index]))
            return FailActiveWorld("EXP1 record encoding failed");
    }
    return true;
}

bool DecodeStableRecords(const std::vector<unsigned char> &bytes,
                         std::vector<StableExplosionRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) ||
        magic != kExplosionActiveWorldMagic ||
        version != kExplosionActiveWorldVersion ||
        count > kMaximumActiveWorldExplosions)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableExplosionRecord record;
        if (!GetRecord(bytes, &offset, &record) ||
            (!records->empty() && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
}

int DrainPrivateExplosionEvents(SimulationContext *context,
                                const KR_ObjectID &object)
{
    KR_ObjectID mutableObject = object;
    if (context == NULL || mutableObject.isNUL())
        return 0;
    int removed = 0;
    while (context->removeEvent(EXPLOSION_START, object) == 1)
        ++removed;
    while (context->removeEvent(EXPLOSION_MOVE, object) == 1)
        ++removed;
    while (context->removeEvent(EXPLOSION_NEWPUFF, object) == 1)
        ++removed;
    return removed;
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
           g_explosionTable.liveCount() == 0 &&
           g_particleBranchesLive == 0;
}

int ExplosionSubjectState_Capacity()
{
    return g_explosionTable.capacity();
}

int ExplosionSubjectState_LiveCount()
{
    return g_explosionTable.liveCount();
}

bool ExplosionSubjectState_IsPending(
    SimulationContext *context, const KR_ObjectID &object)
{
    BoundedExplosion *explosion = context == NULL
        ? NULL : g_explosionTable.find(object);
    return explosion != NULL && context->isExist(object) &&
           explosion->context == context &&
           explosion->clean();
}

bool ExplosionSubjectState_CollectPending(
    SimulationContext *context, const char *objectName,
    std::vector<KR_ObjectID> *objects)
{
    if (context == NULL || objectName == NULL || objects == NULL ||
        g_arena.getContext() != context)
        return false;
    std::vector<BoundedExplosion *> roster;
    g_explosionTable.collect(&roster);
    objects->clear();
    for (std::size_t index = 0; index < roster.size(); ++index)
    {
        const char *name = context->searchObject(
            roster[index]->getObjectID());
        if (roster[index]->clean() && name != NULL &&
            std::strcmp(name, objectName) == 0)
            objects->push_back(roster[index]->getObjectID());
    }
    std::sort(objects->begin(), objects->end(),
              [](const KR_ObjectID &left, const KR_ObjectID &right)
              { return left.id < right.id; });
    return true;
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

bool ExplosionSubjectState_ParentSoundMatches(
    SimulationContext *context, const KR_ObjectID &parent,
    const WAVObj *wav, const CFVector3 &position,
    bool playing, int playCount)
{
    BoundedExplosion *object = g_explosionTable.find(parent);
    return context != NULL && g_arena.getContext() == context &&
           context->isExist(parent) && object != NULL &&
           !IsNul(object->sound()) &&
           SoundObjectState_Matches(
               object->sound(), wav, position.x, position.y, position.z,
               true, playing, playCount);
}

bool ExplosionSubjectState_ParentParticleCounts(
    SimulationContext *context, const KR_ObjectID &parent,
    int *simpleParticles, int *snakeParticles, int *rays,
    int *smokeSprites, int *pieces, int *tracedPieces)
{
    if (simpleParticles == NULL || snakeParticles == NULL || rays == NULL)
        return false;
    *simpleParticles = 0;
    *snakeParticles = 0;
    *rays = 0;
    if (smokeSprites != NULL)
        *smokeSprites = 0;
    if (pieces != NULL)
        *pieces = 0;
    if (tracedPieces != NULL)
        *tracedPieces = 0;
    BoundedExplosion *object = g_explosionTable.find(parent);
    if (context == NULL || g_arena.getContext() != context ||
        !context->isExist(parent) || object == NULL)
        return false;
    *simpleParticles =
        object->particleCount(EXPLOSION_PARTICLE_SIMPLE);
    *snakeParticles =
        object->particleCount(EXPLOSION_PARTICLE_SNAKE);
    *rays = object->particleCount(EXPLOSION_PARTICLE_RAY);
    const int smoke =
        object->particleCount(EXPLOSION_PARTICLE_SMOKE);
    const int piece =
        object->particleCount(EXPLOSION_PARTICLE_PIECE);
    const int traced =
        object->particleCount(EXPLOSION_PARTICLE_TRACED_PIECE);
    if (smokeSprites != NULL)
        *smokeSprites = smoke;
    if (pieces != NULL)
        *pieces = piece;
    if (tracedPieces != NULL)
        *tracedPieces = traced;
    return *simpleParticles + *snakeParticles + *rays + smoke + piece +
               traced ==
           object->particleCount();
}

bool ExplosionSubjectState_ParentTraceState(
    SimulationContext *context, const KR_ObjectID &parent,
    int *tracedPieces, bool *quotaHeld, double *nextPuffTime,
    int *startedPuffs, KR_ObjectID *lastPuff)
{
    if (tracedPieces == NULL || quotaHeld == NULL ||
        nextPuffTime == NULL || startedPuffs == NULL || lastPuff == NULL)
        return false;
    *tracedPieces = 0;
    *quotaHeld = false;
    *nextPuffTime = 0.0;
    *startedPuffs = 0;
    *lastPuff = KR_ObjectID::NUL();
    BoundedExplosion *object = g_explosionTable.find(parent);
    if (context == NULL || g_arena.getContext() != context ||
        !context->isExist(parent) || object == NULL)
        return false;
    *tracedPieces =
        object->particleCount(EXPLOSION_PARTICLE_TRACED_PIECE);
    *quotaHeld = object->traceQuotaHeld();
    *nextPuffTime = object->nextPuffTime();
    *startedPuffs = object->tracePuffsStarted();
    *lastPuff = object->lastTracePuff();
    return true;
}

int ExplosionSubjectState_ParticleBranchLiveCount()
{
    return g_particleBranchesLive;
}

int ExplosionSubjectState_ParticleBranchCapacity()
{
    return kParticleBranchCapacity;
}

int ExplosionSubjectState_PieceDrawCount()
{
    return g_pieceDrawCalls;
}

int ExplosionSubjectState_TracedParentCount()
{
    return g_tracedExplosionParents;
}

int ExplosionSubjectState_TracePuffCount()
{
    return g_tracePuffsStarted;
}

bool ExplosionSubjectState_LightRosterReady(SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context)
        return false;
    LightRosterProbe probe = {true};
    __attrExplosionTable.userFind(ValidateLightAttribute, &probe);
    return probe.ready &&
           ExplosionAttributeState_RosterSize(context) > 0;
}

const char *ExplosionSubjectState_LightProbeAttributeName(
    SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context)
        return NULL;
    LightProbeAttribute probe = {context, NULL, 0.0};
    __attrExplosionTable.userFind(SelectLightProbeAttribute, &probe);
    return probe.name;
}

const char *ExplosionSubjectState_SoundProbeAttributeName(
    SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context ||
        !ExplosionAttributeState_SoundReferencesResolved(context))
        return NULL;
    SoundProbeAttribute probe = {context, NULL, 0.0};
    __attrExplosionTable.userFind(SelectSoundProbeAttribute, &probe);
    return probe.name;
}

const char *ExplosionSubjectState_PieceProbeAttributeName(
    SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context ||
        !ExplosionAttributeState_PieceReferencesResolved(context))
        return NULL;
    PieceProbeAttribute probe = {context, NULL, 0.0};
    __attrExplosionTable.userFind(SelectPieceProbeAttribute, &probe);
    return probe.name;
}

const char *ExplosionSubjectState_TraceProbeAttributeName(
    SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context ||
        !ExplosionAttributeState_TraceReferencesResolved(context))
        return NULL;
    TraceProbeAttribute probe = {context, NULL, 0.0};
    __attrExplosionTable.userFind(SelectTraceProbeAttribute, &probe);
    return probe.name;
}

void ExplosionSubjectState_ReleaseLightFrame()
{
    CViewObject::EnableLights(0);
    g_lightChain.m_list = NULL;
    g_lightChain.m_count = 0;
}

unsigned long long ExplosionSubjectState_Fingerprint(
    SimulationContext *context)
{
    if (!ExplosionSubjectState_TableReady(
            context, g_explosionTable.capacity()))
        return 0;
    unsigned long long hash = kHashOffset;
    const int capacity = g_explosionTable.capacity();
    const int rendering = 1;
    const int audible = 0;
    const int boundedImpactCommand = 1;
    const int radialDamage = 1;
    const int friendlyPlayerAttribution = 1;
    const int selfOwnedQueuedEvent = 1;
    const int impulse = 1;
    const int light = 1;
    const int particles = 1;
    const int simpleSnakeRay = 1;
    const int standaloneSmokeSprite = 1;
    const int simplePiece = 1;
    const int pieceModelDynamicDraw = 1;
    const int pieceBallisticGroundGate = 1;
    const int tracedPiece = 1;
    const int traceSmokeChild = 1;
    const int boundedTraceParentQuota = kMaximumTracedExplosionParents;
    const int coalescedPuffEventChain = 1;
    const int globalParticleCapacity = kParticleBranchCapacity;
    const int perExplosionParticleCapacity =
        kParticleBranchPerExplosion;
    const int retailFrameGateOrdering = 1;
    const int zeroVectorGuard = 1;
    const int sound = 1;
    const int parentOwnedSound = 1;
    const int oneShotCount = 1;
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
    HashBytes(hash, &simpleSnakeRay, sizeof(simpleSnakeRay));
    HashBytes(hash, &standaloneSmokeSprite,
              sizeof(standaloneSmokeSprite));
    HashBytes(hash, &simplePiece, sizeof(simplePiece));
    HashBytes(hash, &pieceModelDynamicDraw,
              sizeof(pieceModelDynamicDraw));
    HashBytes(hash, &pieceBallisticGroundGate,
              sizeof(pieceBallisticGroundGate));
    HashBytes(hash, &tracedPiece, sizeof(tracedPiece));
    HashBytes(hash, &traceSmokeChild, sizeof(traceSmokeChild));
    HashBytes(hash, &boundedTraceParentQuota,
              sizeof(boundedTraceParentQuota));
    HashBytes(hash, &coalescedPuffEventChain,
              sizeof(coalescedPuffEventChain));
    HashBytes(hash, &globalParticleCapacity,
              sizeof(globalParticleCapacity));
    HashBytes(hash, &perExplosionParticleCapacity,
              sizeof(perExplosionParticleCapacity));
    HashBytes(hash, &retailFrameGateOrdering,
              sizeof(retailFrameGateOrdering));
    HashBytes(hash, &zeroVectorGuard, sizeof(zeroVectorGuard));
    HashBytes(hash, &sound, sizeof(sound));
    HashBytes(hash, &parentOwnedSound, sizeof(parentOwnedSound));
    HashBytes(hash, &oneShotCount, sizeof(oneShotCount));
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
    const bool retained = context->isExist(child) != 0;
    const bool complete = accepted == 1 &&
                          g_executedCommands == beforeCommands + 1 &&
                          ((retained && object->started()) ||
                           (!retained && object->clean()));
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
    const bool executed = ExplosionSubjectState_ExecuteNow(
        context, executeRequest, &damageApplications);
    const KR_ObjectID executedChild =
        context->searchObject("Explosion.Execute.Probe");
    RemoveIfPresent(context, executedChild);
    if (!executed || damageApplications != 0 ||
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
    const KR_ObjectID explosion =
        context->searchObject("Explosion.DynamicDamage.Probe");
    RemoveIfPresent(context, explosion);
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

bool ExplosionSubjectState_ProbeLightLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp)
{
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_explosionTable.liveCount() != 0 ||
        g_lightChain.m_count != 0 || g_lightChain.m_list != NULL ||
        CViewObject::EnabledLights() != 0 ||
        !ExplosionSubjectState_LightRosterReady(context))
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex = attributeTable == ct_NULLID
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || attribute == NULL ||
        !ImpactAttributeReady(attribute) || subjectTable == ct_NULLID ||
        attributeIndex == -1)
        return false;

    static const char kProbeName[] = "Explosion.Light.Probe";
    if (context->isExist(kProbeName))
        return false;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const CFVector3 position(23.0, 11.0, -29.0);
    ExplosionImpactRequest request = {
        position, ts, KR_ObjectID::NUL(), subjectTable, attributeIndex,
        kProbeName};
    int damageApplications = -1;
    const bool executed = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    const KR_ObjectID child = context->searchObject(kProbeName);

    if (attribute->m_useLight == 0)
        return executed && damageApplications == 0 && IsNul(child) &&
               g_explosionTable.liveCount() == 0 &&
               g_lightChain.m_count == 0 && g_lightChain.m_list == NULL &&
               CViewObject::EnabledLights() == 0;

    BoundedExplosion *object = g_explosionTable.find(child);
    const bool retained = executed && damageApplications == 0 &&
                          !IsNul(child) && object != NULL &&
                          object->lightActive() &&
                          object->attribute() == attribute &&
                          NearlyEqual(object->startTime(), ts) &&
                          g_explosionTable.liveCount() == 1;
    const bool expiryOwned = retained &&
        context->removeEvent(EXPLOSION_MOVE, child) != 0;

    const double savedMoment = Session::m_moment;
    Session::m_moment = ts + attribute->m_lightTimeLife * 0.5;
    CViewDynamicList list;
    if (expiryOwned)
        object->render(list, Session::m_moment);
    const int brightnessIndex =
        AttributeExplosion::MAX_BRIGHT / 2;
    const CFVector3 lightPosition =
        position + CFVector3(0.0, attribute->m_lightOffset, 0.0);
    const bool queued = expiryOwned && g_lightChain.m_count == 1 &&
                        g_lightChain.m_list == g_lightChain.m_dim &&
                        g_lightChain.m_list->m_pos == lightPosition &&
                        g_lightChain.m_list->m_color ==
                            attribute->m_lightColor &&
                        g_lightChain.m_list->m_brightness ==
                            attribute->m_brightness[brightnessIndex] &&
                        g_lightChain.m_list->m_radius ==
                            attribute->m_lightRadius;
    if (queued)
        g_lightChain.render();
    const bool published = queued && CViewObject::EnabledLights() == 1 &&
                           _gr_pLights[0].r == attribute->m_lightRadius &&
                           _gr_pLights[0].power0 ==
                               attribute->m_brightness[brightnessIndex] &&
                           _gr_pLights[0].color ==
                               attribute->m_lightColor;
    ExplosionSubjectState_ReleaseLightFrame();

    bool expired = false;
    int expirySteps = 0;
    if (retained && context->isExist(child))
    {
        while (context->isExist(child) && expirySteps < 4096)
        {
            object = g_explosionTable.find(child);
            if (object == NULL || object->nextMoveTime() <= 0.0)
                break;
            if (expirySteps > 0 &&
                context->removeEvent(EXPLOSION_MOVE, child) == 0)
                break;
            KR_Event expiry;
            expiry.label = EXPLOSION_MOVE;
            expiry.source = child;
            expiry.destination = child;
            expiry.timeStamp = object->nextMoveTime();
            if (object->receiveEvent(expiry) != 1)
                break;
            ++expirySteps;
        }
        expired = !context->isExist(child) && expirySteps > 0 &&
                  expirySteps < 4096;
    }
    Session::m_moment = savedMoment;
    RemoveIfPresent(context, child);
    list.Clear(false);
    return published && expired && g_explosionTable.liveCount() == 0 &&
           !context->isExist(kProbeName) &&
           (IsNul(child) ||
            context->removeEvent(EXPLOSION_MOVE, child) == 0) &&
           g_lightChain.m_count == 0 && g_lightChain.m_list == NULL &&
           CViewObject::EnabledLights() == 0;
}

bool ExplosionSubjectState_ProbeSoundLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionSoundProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_explosionTable.liveCount() != 0 ||
        SoundObjectState_LiveCount() != 0 ||
        !ExplosionAttributeState_SoundReferencesResolved(context))
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex = attributeTable == ct_NULLID
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || !ImpactAttributeReady(attribute) ||
        !LightAttributeReady(attribute) || attribute->m_soundName[0] == 0 ||
        attribute->m_wav == NULL || attribute->m_ctsndID == ct_NULLID ||
        subjectTable == ct_NULLID || attributeIndex == -1)
        return false;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const CFVector3 position(43.0, 17.0, -61.0);
    ExplosionImpactRequest request = {
        position, ts, KR_ObjectID::NUL(), subjectTable, attributeIndex,
        "Explosion.Sound.Parent.Probe"};
    int damageApplications = -1;
    const bool executed = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    const KR_ObjectID parent =
        context->searchObject("Explosion.Sound.Parent.Probe");
    BoundedExplosion *object = g_explosionTable.find(parent);
    const KR_ObjectID sound = object == NULL
        ? KR_ObjectID::NUL()
        : object->sound();
    const bool started = executed && damageApplications == 0 &&
        !IsNul(parent) && object != NULL && object->lightActive() &&
        !IsNul(sound) && SoundObjectState_LiveCount() == 1 &&
        SoundObjectState_Matches(
            sound, attribute->m_wav, position.x, position.y, position.z,
            true, true, 1);
    RemoveIfPresent(context, parent);
    const bool rolledBack = started && !context->isExist(parent) &&
        !context->isExist(sound) && g_explosionTable.liveCount() == 0 &&
        SoundObjectState_LiveCount() == 0;
    if (!rolledBack)
    {
        RemoveIfPresent(context, parent);
        RemoveIfPresent(context, sound);
        return false;
    }
    summary->startedSounds = 1;
    summary->rolledBackSounds = 1;

    WAVObj *savedWav = attribute->m_wav;
    const ct_ClassTableID savedTable = attribute->m_ctsndID;
    attribute->m_wav = NULL;
    attribute->m_ctsndID = ct_NULLID;
    request.timeStamp = ts + 1.0;
    request.objectName = "Explosion.Sound.DependencyGate.Probe";
    damageApplications = -1;
    const bool gatedExecuted = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    const KR_ObjectID gatedParent =
        context->searchObject("Explosion.Sound.DependencyGate.Probe");
    BoundedExplosion *gatedObject = g_explosionTable.find(gatedParent);
    const bool dependencySkipped = gatedExecuted &&
        damageApplications == 0 && !IsNul(gatedParent) &&
        gatedObject != NULL && gatedObject->lightActive() &&
        IsNul(gatedObject->sound()) && SoundObjectState_LiveCount() == 0;
    attribute->m_wav = savedWav;
    attribute->m_ctsndID = savedTable;
    RemoveIfPresent(context, gatedParent);
    if (!dependencySkipped || g_explosionTable.liveCount() != 0 ||
        SoundObjectState_LiveCount() != 0 ||
        !ExplosionAttributeState_SoundReferencesResolved(context))
        return false;
    summary->dependencyGateSkips = 1;
    return !context->isExist("Explosion.Sound.Parent.Probe") &&
           !context->isExist("Explosion.Sound.DependencyGate.Probe");
}

bool ExplosionSubjectState_ProbeParticleLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionParticleProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 ||
        !ExplosionAttributeState_ParticleVisualsResolved(context))
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex = attributeTable == ct_NULLID
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || !ImpactAttributeReady(attribute) ||
        subjectTable == ct_NULLID || attributeIndex == -1 ||
        attribute->m_minPartCnt <= 0 ||
        attribute->m_minPartSnCnt <= 0)
        return false;

    const double savedFrameSec = Session::m_frameSec;
    const double savedMoveTimeInc = attribute->m_moveTimeInc;
    Session::m_frameSec = 0.04;
    // Exercise the recurring owner event without making every seance
    // admission replay thousands of sub-frame retail increments.
    attribute->m_moveTimeInc = 0.25;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    ExplosionImpactRequest request = {
        CFVector3(31.0, 19.0, -47.0), ts, KR_ObjectID::NUL(),
        subjectTable, attributeIndex, "Explosion.Particle.Rollback.Probe"};
    int damageApplications = -1;
    const bool executed = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    KR_ObjectID parent =
        context->searchObject("Explosion.Particle.Rollback.Probe");
    int simple = 0;
    int snake = 0;
    int rays = 0;
    int pieces = 0;
    const bool countsReady = executed && damageApplications == 0 &&
        !IsNul(parent) && ExplosionSubjectState_ParentParticleCounts(
            context, parent, &simple, &snake, &rays, NULL, &pieces) &&
        simple >= attribute->m_minPartCnt &&
        simple <= attribute->m_maxPartCnt &&
        snake >= attribute->m_minPartSnCnt &&
        snake <= attribute->m_maxPartSnCnt &&
        rays >= attribute->m_minRayCnt &&
        rays <= attribute->m_maxRayCnt &&
        simple + snake + rays > 0 &&
        pieces >= 0 &&
        g_particleBranchesLive == simple + snake + rays + pieces;
    const int rollbackCount = simple + snake + rays;
    RemoveIfPresent(context, parent);
    const bool rollbackReady = countsReady &&
        g_explosionTable.liveCount() == 0 &&
        g_particleBranchesLive == 0;
    if (!rollbackReady)
    {
        Session::m_frameSec = savedFrameSec;
        attribute->m_moveTimeInc = savedMoveTimeInc;
        return false;
    }
    summary->startedBranches = simple + snake + rays;
    summary->simpleParticles = simple;
    summary->snakeParticles = snake;
    summary->rays = rays;
    summary->rolledBackBranches = rollbackCount;

    void (*savedDrawParticle)(int, int, int, int, unsigned long) =
        _pGRDrawParticle;
    _pGRDrawParticle = NULL;
    request.timeStamp = ts + 1.0;
    request.objectName = "Explosion.Particle.DependencyGate.Probe";
    damageApplications = -1;
    const bool gatedExecuted = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    _pGRDrawParticle = savedDrawParticle;
    parent = context->searchObject(
        "Explosion.Particle.DependencyGate.Probe");
    int gatedSimple = -1;
    int gatedSnake = -1;
    int gatedRays = -1;
    int gatedPieces = -1;
    const bool dependencySkipped = gatedExecuted &&
        damageApplications == 0 && !IsNul(parent) &&
        ExplosionSubjectState_ParentParticleCounts(
            context, parent, &gatedSimple, &gatedSnake, &gatedRays,
            NULL, &gatedPieces) &&
        gatedSimple == 0 && gatedSnake == 0 && gatedRays == 0 &&
        gatedPieces >= 0 && g_particleBranchesLive == gatedPieces;
    RemoveIfPresent(context, parent);
    if (!dependencySkipped || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 ||
        !ExplosionAttributeState_ParticleVisualsResolved(context))
    {
        Session::m_frameSec = savedFrameSec;
        attribute->m_moveTimeInc = savedMoveTimeInc;
        return false;
    }
    summary->dependencyGateSkips = 1;

    request.timeStamp = ts + 2.0;
    request.objectName = "Explosion.Particle.Expiry.Probe";
    damageApplications = -1;
    const bool lifecycleExecuted = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    parent = context->searchObject("Explosion.Particle.Expiry.Probe");
    BoundedExplosion *object = g_explosionTable.find(parent);
    bool lifecycleReady = lifecycleExecuted && damageApplications == 0 &&
        !IsNul(parent) && object != NULL && object->particleCount() > 0;
    int moveSteps = 0;
    while (lifecycleReady && context->isExist(parent) && moveSteps < 4096)
    {
        object = g_explosionTable.find(parent);
        if (object == NULL || object->nextMoveTime() <= 0.0 ||
            context->removeEvent(EXPLOSION_MOVE, parent) == 0)
        {
            lifecycleReady = false;
            break;
        }
        KR_Event move;
        move.label = EXPLOSION_MOVE;
        move.source = parent;
        move.destination = parent;
        move.timeStamp = object->nextMoveTime();
        if (object->receiveEvent(move) != 1)
        {
            lifecycleReady = false;
            break;
        }
        ++moveSteps;
    }
    const bool expired = lifecycleReady && !context->isExist(parent) &&
        moveSteps > 0 && moveSteps < 4096 &&
        g_explosionTable.liveCount() == 0 &&
        g_particleBranchesLive == 0;
    RemoveIfPresent(context, parent);
    Session::m_frameSec = savedFrameSec;
    attribute->m_moveTimeInc = savedMoveTimeInc;
    if (!expired)
        return false;
    summary->moveSteps = moveSteps;
    summary->expiredParents = 1;
    return !context->isExist("Explosion.Particle.Rollback.Probe") &&
           !context->isExist("Explosion.Particle.DependencyGate.Probe") &&
           !context->isExist("Explosion.Particle.Expiry.Probe") &&
           ExplosionAttributeState_ParticleVisualsResolved(context);
}

bool ExplosionSubjectState_ProbeSmokeLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionSmokeProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 ||
        !ExplosionAttributeState_SmokeVisualsResolved(context))
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex = attributeTable == ct_NULLID
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || !ImpactAttributeReady(attribute) ||
        subjectTable == ct_NULLID || attributeIndex == -1 ||
        attribute->m_minSmokeCnt <= 0 || attribute->m_hTexture == NULL)
        return false;

    const double savedFrameSec = Session::m_frameSec;
    const double savedMoveTimeInc = attribute->m_moveTimeInc;
    const int savedMinimumTracedPieces =
        attribute->m_minPieceSmokeCnt;
    const int savedMaximumTracedPieces =
        attribute->m_maxPieceSmokeCnt;
    Session::m_frameSec = 0.04;
    attribute->m_moveTimeInc = 0.25;
    // This probe owns only the aggregate Explosion smoke branch.  Once the
    // traced-Piece reference layer is active, isolate it from recurring
    // NEWPUFF/common-Smoke children so its expiry and rollback assertions do
    // not become dependent on the order in which admission probes run.
    attribute->m_minPieceSmokeCnt = 0;
    attribute->m_maxPieceSmokeCnt = 0;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    ExplosionImpactRequest request = {
        CFVector3(37.0, 23.0, -53.0), ts, KR_ObjectID::NUL(),
        subjectTable, attributeIndex, "Explosion.Smoke.Rollback.Probe"};
    int damageApplications = -1;
    const bool executed = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    KR_ObjectID parent =
        context->searchObject("Explosion.Smoke.Rollback.Probe");
    int simple = 0;
    int snake = 0;
    int rays = 0;
    int smoke = 0;
    int pieces = 0;
    const bool countsReady = executed && damageApplications == 0 &&
        !IsNul(parent) && ExplosionSubjectState_ParentParticleCounts(
            context, parent, &simple, &snake, &rays, &smoke, &pieces) &&
        smoke >= attribute->m_minSmokeCnt &&
        smoke <= attribute->m_maxSmokeCnt && smoke > 0 &&
        g_particleBranchesLive == simple + snake + rays + smoke + pieces;
    RemoveIfPresent(context, parent);
    const bool rollbackReady = countsReady &&
        g_explosionTable.liveCount() == 0 &&
        g_particleBranchesLive == 0;
    if (!rollbackReady)
    {
        Session::m_frameSec = savedFrameSec;
        attribute->m_moveTimeInc = savedMoveTimeInc;
        attribute->m_minPieceSmokeCnt = savedMinimumTracedPieces;
        attribute->m_maxPieceSmokeCnt = savedMaximumTracedPieces;
        return false;
    }
    summary->startedSprites = smoke;
    summary->rolledBackSprites = smoke;

    void (*savedDrawAlphaSprite)(SGRAlphaSprite *) =
        _pGRDrawAlphaSprite;
    _pGRDrawAlphaSprite = NULL;
    request.timeStamp = ts + 1.0;
    request.objectName = "Explosion.Smoke.DependencyGate.Probe";
    damageApplications = -1;
    const bool gatedExecuted = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    _pGRDrawAlphaSprite = savedDrawAlphaSprite;
    parent = context->searchObject(
        "Explosion.Smoke.DependencyGate.Probe");
    int gatedSmoke = -1;
    int gatedPieces = -1;
    const bool dependencySkipped = gatedExecuted &&
        damageApplications == 0 && !IsNul(parent) &&
        ExplosionSubjectState_ParentParticleCounts(
            context, parent, &simple, &snake, &rays, &gatedSmoke,
            &gatedPieces) &&
        gatedSmoke == 0 && gatedPieces >= 0;
    RemoveIfPresent(context, parent);
    if (!dependencySkipped || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 ||
        !ExplosionAttributeState_SmokeVisualsResolved(context))
    {
        Session::m_frameSec = savedFrameSec;
        attribute->m_moveTimeInc = savedMoveTimeInc;
        attribute->m_minPieceSmokeCnt = savedMinimumTracedPieces;
        attribute->m_maxPieceSmokeCnt = savedMaximumTracedPieces;
        return false;
    }
    summary->dependencyGateSkips = 1;

    request.timeStamp = ts + 2.0;
    request.objectName = "Explosion.Smoke.Expiry.Probe";
    damageApplications = -1;
    const bool lifecycleExecuted = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    parent = context->searchObject("Explosion.Smoke.Expiry.Probe");
    BoundedExplosion *object = g_explosionTable.find(parent);
    bool lifecycleReady = lifecycleExecuted && damageApplications == 0 &&
        !IsNul(parent) && object != NULL &&
        object->particleCount(EXPLOSION_PARTICLE_SMOKE) > 0;
    int moveSteps = 0;
    while (lifecycleReady && context->isExist(parent) && moveSteps < 4096)
    {
        object = g_explosionTable.find(parent);
        if (object == NULL || object->nextMoveTime() <= 0.0 ||
            context->removeEvent(EXPLOSION_MOVE, parent) == 0)
        {
            lifecycleReady = false;
            break;
        }
        KR_Event move;
        move.label = EXPLOSION_MOVE;
        move.source = parent;
        move.destination = parent;
        move.timeStamp = object->nextMoveTime();
        if (object->receiveEvent(move) != 1)
        {
            lifecycleReady = false;
            break;
        }
        ++moveSteps;
    }
    const bool expired = lifecycleReady && !context->isExist(parent) &&
        moveSteps > 0 && moveSteps < 4096 &&
        g_explosionTable.liveCount() == 0 &&
        g_particleBranchesLive == 0;
    RemoveIfPresent(context, parent);
    Session::m_frameSec = savedFrameSec;
    attribute->m_moveTimeInc = savedMoveTimeInc;
    attribute->m_minPieceSmokeCnt = savedMinimumTracedPieces;
    attribute->m_maxPieceSmokeCnt = savedMaximumTracedPieces;
    if (!expired)
        return false;
    summary->moveSteps = moveSteps;
    summary->expiredParents = 1;
    return !context->isExist("Explosion.Smoke.Rollback.Probe") &&
           !context->isExist("Explosion.Smoke.DependencyGate.Probe") &&
           !context->isExist("Explosion.Smoke.Expiry.Probe") &&
           ExplosionAttributeState_SmokeVisualsResolved(context);
}

bool ExplosionSubjectState_ProbePieceLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionPieceProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 ||
        !ExplosionAttributeState_PieceReferencesResolved(context))
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex = attributeTable == ct_NULLID
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || !ImpactAttributeReady(attribute) ||
        subjectTable == ct_NULLID || attributeIndex == -1 ||
        attribute->m_cacheSkin == NULL || attribute->m_minPieceCnt <= 0)
        return false;

    const double savedFrameSec = Session::m_frameSec;
    const double savedMoveTimeInc = attribute->m_moveTimeInc;
    Session::m_frameSec = 0.04;
    attribute->m_moveTimeInc = 0.25;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    ExplosionImpactRequest request = {
        CFVector3(43.0, 29.0, -59.0), ts, KR_ObjectID::NUL(),
        subjectTable, attributeIndex, "Explosion.Piece.Rollback.Probe"};
    int damageApplications = -1;
    const bool executed = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    KR_ObjectID parent =
        context->searchObject("Explosion.Piece.Rollback.Probe");
    int simple = 0;
    int snake = 0;
    int rays = 0;
    int smoke = 0;
    int pieces = 0;
    const bool countsReady = executed && damageApplications == 0 &&
        !IsNul(parent) && ExplosionSubjectState_ParentParticleCounts(
            context, parent, &simple, &snake, &rays, &smoke, &pieces) &&
        pieces >= attribute->m_minPieceCnt &&
        pieces <= attribute->m_maxPieceCnt && pieces > 0 &&
        g_particleBranchesLive ==
            simple + snake + rays + smoke + pieces;
    RemoveIfPresent(context, parent);
    if (!countsReady || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0)
    {
        Session::m_frameSec = savedFrameSec;
        attribute->m_moveTimeInc = savedMoveTimeInc;
        return false;
    }
    summary->startedPieces = pieces;
    summary->rolledBackPieces = pieces;

    CViewObjectModel *savedSkin = attribute->m_cacheSkin;
    attribute->m_cacheSkin = NULL;
    request.timeStamp = ts + 1.0;
    request.objectName = "Explosion.Piece.DependencyGate.Probe";
    damageApplications = -1;
    const bool gatedExecuted = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    attribute->m_cacheSkin = savedSkin;
    parent = context->isExist("Explosion.Piece.DependencyGate.Probe")
        ? context->searchObject("Explosion.Piece.DependencyGate.Probe")
        : KR_ObjectID::NUL();
    int gatedPieces = -1;
    const bool parentRemovedByEmptyGate = IsNul(parent) &&
        g_explosionTable.liveCount() == 0 && g_particleBranchesLive == 0;
    const bool parentRetainedWithoutPieces = !IsNul(parent) &&
        ExplosionSubjectState_ParentParticleCounts(
            context, parent, &simple, &snake, &rays, &smoke,
            &gatedPieces) && gatedPieces == 0;
    const bool dependencySkipped = damageApplications == 0 &&
        ((!gatedExecuted && parentRemovedByEmptyGate) ||
         (gatedExecuted &&
          (parentRemovedByEmptyGate || parentRetainedWithoutPieces)));
    RemoveIfPresent(context, parent);
    if (!dependencySkipped || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 ||
        !ExplosionAttributeState_PieceReferencesResolved(context))
    {
        Session::m_frameSec = savedFrameSec;
        attribute->m_moveTimeInc = savedMoveTimeInc;
        return false;
    }
    summary->dependencyGateSkips = 1;

    request.timeStamp = ts + 2.0;
    request.objectName = "Explosion.Piece.Expiry.Probe";
    damageApplications = -1;
    const bool lifecycleExecuted = ExplosionSubjectState_ExecuteNow(
        context, request, &damageApplications);
    parent = context->searchObject("Explosion.Piece.Expiry.Probe");
    BoundedExplosion *object = g_explosionTable.find(parent);
    bool lifecycleReady = lifecycleExecuted && damageApplications == 0 &&
        !IsNul(parent) && object != NULL &&
        object->particleCount(EXPLOSION_PARTICLE_PIECE) > 0;
    bool piecesExpired = false;
    int moveSteps = 0;
    while (lifecycleReady && context->isExist(parent) && moveSteps < 4096)
    {
        object = g_explosionTable.find(parent);
        if (object == NULL || object->nextMoveTime() <= 0.0 ||
            context->removeEvent(EXPLOSION_MOVE, parent) == 0)
        {
            lifecycleReady = false;
            break;
        }
        KR_Event move;
        move.label = EXPLOSION_MOVE;
        move.source = parent;
        move.destination = parent;
        move.timeStamp = object->nextMoveTime();
        if (object->receiveEvent(move) != 1)
        {
            lifecycleReady = false;
            break;
        }
        ++moveSteps;
        object = g_explosionTable.find(parent);
        if (object == NULL ||
            object->particleCount(EXPLOSION_PARTICLE_PIECE) == 0)
            piecesExpired = true;
    }
    const bool expired = lifecycleReady && piecesExpired &&
        !context->isExist(parent) && moveSteps > 0 && moveSteps < 4096 &&
        g_explosionTable.liveCount() == 0 && g_particleBranchesLive == 0;
    RemoveIfPresent(context, parent);
    Session::m_frameSec = savedFrameSec;
    attribute->m_moveTimeInc = savedMoveTimeInc;
    if (!expired)
        return false;
    summary->moveSteps = moveSteps;
    summary->expiredParents = 1;
    return !context->isExist("Explosion.Piece.Rollback.Probe") &&
           !context->isExist("Explosion.Piece.DependencyGate.Probe") &&
           !context->isExist("Explosion.Piece.Expiry.Probe") &&
           ExplosionAttributeState_PieceReferencesResolved(context);
}

bool ExplosionSubjectState_ProbeTraceLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionTraceProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 || g_tracedExplosionParents != 0 ||
        SmokeSubjectState_LiveCount() != 0 ||
        !ExplosionAttributeState_TraceReferencesResolved(context))
        return false;

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex = attributeTable == ct_NULLID
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (IsNul(attributeID) || !ImpactAttributeReady(attribute) ||
        subjectTable == ct_NULLID || attributeIndex == -1 ||
        attribute->m_cacheSkin == NULL ||
        attribute->m_smokeTableID == ct_NULLID ||
        attribute->m_smokeAttrID.isNUL() ||
        attribute->m_minPieceSmokeCnt <= 0 ||
        attribute->m_minPieceTimeLife <= 0.0)
        return false;

    const double savedFrameSec = Session::m_frameSec;
    const double savedMoveTimeInc = attribute->m_moveTimeInc;
    Session::m_frameSec = 0.04;
    attribute->m_moveTimeInc = 0.05;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    KR_ObjectID cleanupParents[7] = {};
    int cleanupParentCount = 0;
    bool succeeded = false;

    do
    {
        ExplosionImpactRequest request = {
            // Keep the admission trajectory far above any retail terrain so
            // the first NEWPUFF/MOVE pair proves Smoke creation instead of
            // nondeterministically losing every piece to a ground crossing.
            CFVector3(43.0, 10000.0, -59.0), ts,
            KR_ObjectID::NUL(), subjectTable, attributeIndex,
            "Explosion.Trace.Rollback.Probe"};
        int damageApplications = -1;
        if (!ExplosionSubjectState_ExecuteNow(
                context, request, &damageApplications) ||
            damageApplications != 0)
            break;
        KR_ObjectID parent =
            context->searchObject("Explosion.Trace.Rollback.Probe");
        cleanupParents[cleanupParentCount++] = parent;
        BoundedExplosion *object = g_explosionTable.find(parent);
        int tracedPieces = 0;
        bool quotaHeld = false;
        double nextPuffTime = 0.0;
        int startedPuffs = 0;
        KR_ObjectID lastPuff = KR_ObjectID::NUL();
        if (IsNul(parent) || object == NULL ||
            !ExplosionSubjectState_ParentTraceState(
                context, parent, &tracedPieces, &quotaHeld,
                &nextPuffTime, &startedPuffs, &lastPuff) ||
            tracedPieces < attribute->m_minPieceSmokeCnt ||
            tracedPieces > attribute->m_maxPieceSmokeCnt ||
            !quotaHeld || nextPuffTime <= ts || startedPuffs != 0 ||
            !IsNul(lastPuff) || g_tracedExplosionParents != 1)
            break;
        summary->startedPieces = tracedPieces;

        int moveSteps = 0;
        while (context->isExist(parent) &&
               object->nextMoveTime() <= nextPuffTime &&
               moveSteps < 4096)
        {
            const double moveTime = object->nextMoveTime();
            if (moveTime <= 0.0 ||
                context->removeEvent(EXPLOSION_MOVE, parent) == 0)
                break;
            KR_Event move;
            move.label = EXPLOSION_MOVE;
            move.source = parent;
            move.destination = parent;
            move.timeStamp = moveTime;
            if (object->receiveEvent(move) != 1)
                break;
            ++moveSteps;
            object = g_explosionTable.find(parent);
            if (object == NULL)
                break;
        }
        if (!context->isExist(parent) || object == NULL ||
            moveSteps >= 4096 ||
            context->removeEvent(EXPLOSION_NEWPUFF, parent) == 0)
            break;
        KR_Event puff;
        puff.label = EXPLOSION_NEWPUFF;
        puff.source = parent;
        puff.destination = parent;
        puff.timeStamp = nextPuffTime;
        if (object->receiveEvent(puff) != 1)
            break;
        ++summary->puffEvents;

        const double moveTime = object->nextMoveTime();
        // Context inserts a newly queued event before an older event with the
        // same timestamp.  The recurring MOVE at the first puff timestamp is
        // therefore still pending after NEWPUFF has armed the traced pieces;
        // executing that equal-time MOVE is what emits their first Smoke.
        if ((moveTime < nextPuffTime &&
             !NearlyEqual(moveTime, nextPuffTime)) ||
            context->removeEvent(EXPLOSION_MOVE, parent) == 0)
            break;
        KR_Event move;
        move.label = EXPLOSION_MOVE;
        move.source = parent;
        move.destination = parent;
        move.timeStamp = moveTime;
        if (object->receiveEvent(move) != 1)
            break;
        ++moveSteps;
        const bool traceStateReady =
            ExplosionSubjectState_ParentTraceState(
                context, parent, &tracedPieces, &quotaHeld,
                &nextPuffTime, &startedPuffs, &lastPuff);
        summary->smokeChildren = startedPuffs;
        summary->moveSteps = moveSteps;
        if (!traceStateReady ||
            startedPuffs <= 0 || IsNul(lastPuff) ||
            !context->isExist(lastPuff) ||
            SmokeSubjectState_LiveCount() != startedPuffs)
            break;

        context->removeObject(parent);
        cleanupParents[0] = KR_ObjectID::NUL();
        if (context->isExist(parent) ||
            context->removeEvent(EXPLOSION_MOVE, parent) != 0 ||
            context->removeEvent(EXPLOSION_NEWPUFF, parent) != 0 ||
            g_explosionTable.liveCount() != 0 ||
            g_particleBranchesLive != 0 ||
            g_tracedExplosionParents != 0 ||
            SmokeSubjectState_LiveCount() != startedPuffs)
            break;
        summary->rolledBackPieces = summary->startedPieces;
        int rolledBackPuffs = 0;
        while (SmokeSubjectState_LiveCount() > 0 &&
               rolledBackPuffs < startedPuffs)
        {
            const KR_ObjectID puff =
                context->searchObject("Smok.Static");
            if (IsNul(puff) ||
                !SmokeSubjectState_RollbackStarted(context, puff))
                break;
            ++rolledBackPuffs;
        }
        if (rolledBackPuffs != startedPuffs ||
            SmokeSubjectState_LiveCount() != 0)
            break;
        bool quotaReady = true;
        for (int index = 0; index < 5; ++index)
        {
            char name[64] = {};
            std::snprintf(name, sizeof(name),
                          "Explosion.Trace.Quota.%d", index);
            request.timeStamp = ts + 2.0 + index * 0.1;
            request.objectName = name;
            damageApplications = -1;
            if (!ExplosionSubjectState_ExecuteNow(
                    context, request, &damageApplications) ||
                damageApplications != 0)
            {
                quotaReady = false;
                break;
            }
            const KR_ObjectID quotaParent = context->searchObject(name);
            cleanupParents[cleanupParentCount++] = quotaParent;
            int quotaPieces = 0;
            bool held = false;
            double quotaPuffTime = 0.0;
            int quotaPuffs = 0;
            KR_ObjectID quotaLast = KR_ObjectID::NUL();
            if (IsNul(quotaParent) ||
                !ExplosionSubjectState_ParentTraceState(
                    context, quotaParent, &quotaPieces, &held,
                    &quotaPuffTime, &quotaPuffs, &quotaLast) ||
                (index < kMaximumTracedExplosionParents &&
                 (quotaPieces <= 0 || !held)) ||
                (index == kMaximumTracedExplosionParents &&
                 (quotaPieces != 0 || held)))
            {
                quotaReady = false;
                break;
            }
        }
        if (!quotaReady ||
            g_tracedExplosionParents != kMaximumTracedExplosionParents)
            break;
        summary->quotaGateSkips = 1;
        for (int index = 1; index < cleanupParentCount; ++index)
        {
            if (!IsNul(cleanupParents[index]) &&
                context->isExist(cleanupParents[index]))
                context->removeObject(cleanupParents[index]);
            cleanupParents[index] = KR_ObjectID::NUL();
        }
        cleanupParentCount = 0;
        if (g_explosionTable.liveCount() != 0 ||
            g_particleBranchesLive != 0 ||
            g_tracedExplosionParents != 0 ||
            SmokeSubjectState_LiveCount() != 0)
            break;

        request.timeStamp = ts + 4.0;
        request.objectName = "Explosion.Trace.Expiry.Probe";
        damageApplications = -1;
        if (!ExplosionSubjectState_ExecuteNow(
                context, request, &damageApplications) ||
            damageApplications != 0)
            break;
        parent = context->searchObject("Explosion.Trace.Expiry.Probe");
        cleanupParents[cleanupParentCount++] = parent;
        object = g_explosionTable.find(parent);
        if (IsNul(parent) || object == NULL ||
            object->particleCount(
                EXPLOSION_PARTICLE_TRACED_PIECE) <= 0 ||
            context->removeEvent(EXPLOSION_NEWPUFF, parent) == 0)
            break;
        int expiryMoves = 0;
        while (context->isExist(parent) && expiryMoves < 4096)
        {
            object = g_explosionTable.find(parent);
            if (object == NULL || object->nextMoveTime() <= 0.0 ||
                context->removeEvent(EXPLOSION_MOVE, parent) == 0)
                break;
            KR_Event expiryMove;
            expiryMove.label = EXPLOSION_MOVE;
            expiryMove.source = parent;
            expiryMove.destination = parent;
            expiryMove.timeStamp = object->nextMoveTime();
            if (object->receiveEvent(expiryMove) != 1)
                break;
            ++expiryMoves;
        }
        if (context->isExist(parent) || expiryMoves <= 0 ||
            expiryMoves >= 4096 || g_explosionTable.liveCount() != 0 ||
            g_particleBranchesLive != 0 ||
            g_tracedExplosionParents != 0 ||
            SmokeSubjectState_LiveCount() != 0 ||
            context->removeEvent(EXPLOSION_MOVE, parent) != 0 ||
            context->removeEvent(EXPLOSION_NEWPUFF, parent) != 0)
            break;
        cleanupParents[0] = KR_ObjectID::NUL();
        cleanupParentCount = 0;
        summary->moveSteps += expiryMoves;
        summary->expiredParents = 1;
        succeeded = true;
    }
    while (false);

    for (int index = 0; index < cleanupParentCount; ++index)
        if (!IsNul(cleanupParents[index]) &&
            context->isExist(cleanupParents[index]))
            context->removeObject(cleanupParents[index]);
    int cleanupPuffs = 0;
    while (SmokeSubjectState_LiveCount() > 0 && cleanupPuffs < 128)
    {
        const KR_ObjectID puff = context->searchObject("Smok.Static");
        if (IsNul(puff) ||
            !SmokeSubjectState_RollbackStarted(context, puff))
            break;
        ++cleanupPuffs;
    }
    Session::m_frameSec = savedFrameSec;
    attribute->m_moveTimeInc = savedMoveTimeInc;
    return succeeded && summary->startedPieces > 0 &&
           summary->quotaGateSkips == 1 && summary->puffEvents == 1 &&
           summary->smokeChildren > 0 && summary->moveSteps > 0 &&
           summary->expiredParents == 1 &&
           summary->rolledBackPieces == summary->startedPieces &&
           g_explosionTable.liveCount() == 0 &&
           g_particleBranchesLive == 0 &&
           g_tracedExplosionParents == 0 &&
           SmokeSubjectState_LiveCount() == 0 &&
           ExplosionAttributeState_TraceReferencesResolved(context);
}

void ExplosionActiveWorldState_Link()
{
    ExplosionSubjectState_Link();
}

const char *ExplosionActiveWorldState_LastFailure()
{
    return g_activeWorldFailure.c_str();
}

int ExplosionActiveWorldState_BranchCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableExplosionRecord> records;
    if (!DecodeStableRecords(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        count += static_cast<int>(records[index].branches.size());
    return count;
}

int ExplosionActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableExplosionRecord> records;
    if (!DecodeStableRecords(bytes, &records))
        return -1;
    int count = 0;
    for (std::size_t index = 0; index < records.size(); ++index)
        count += 1 + records[index].hasPuffEvent;
    return count;
}

unsigned long long ExplosionActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!ExplosionActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = kHashOffset;
    if (!bytes.empty())
        HashBytes(hash, &bytes[0], static_cast<int>(bytes.size()));
    return hash;
}

bool ExplosionActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_activeWorldFailure.clear();
    std::vector<StableExplosionRecord> records;
    if (!CollectStableRecords(context, &records))
    {
        if (g_activeWorldFailure.empty())
            FailActiveWorld("Explosion stable roster collection failed");
        return false;
    }
    return EncodeStableRecords(records, bytes);
}

bool ExplosionActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableExplosionRecord> records;
    return DecodeStableRecords(bytes, &records);
}

bool ExplosionActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return ExplosionActiveWorldState_ValidateStable(bytes) &&
           ExplosionActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool ExplosionActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableExplosionRecord> records;
    std::vector<BoundedExplosion *> objects;
    if (owners == NULL || !owners->empty() ||
        !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &objects) ||
        !RosterMatches(objects, context, records))
        return false;
    for (std::size_t index = 0; index < objects.size(); ++index)
        owners->push_back(objects[index]->getObjectID());
    return true;
}

bool ExplosionActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableExplosionRecord> records;
    std::vector<BoundedExplosion *> objects;
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &objects))
        return false;
    if (!objects.empty())
        return RosterMatches(objects, context, records);
    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Explosion");
    if ((!records.empty() && table == ct_NULLID) ||
        static_cast<int>(records.size()) >
            g_explosionTable.capacity() - g_explosionTable.liveCount())
        return FailActiveWorld(
            "Explosion owner table has insufficient capacity");
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        KR_ObjectID object =
            g_arena.newObject(table, records[index].name.c_str());
        if (object.isNUL() || g_explosionTable.find(object) == NULL)
        {
            ExplosionActiveWorldState_RemoveStableOwners(context, created);
            return FailActiveWorld("Explosion owner allocation failed");
        }
        created->push_back(object);
    }
    objects.clear();
    if (!CollectStableRoster(context, &objects) ||
        !RosterMatches(objects, context, records))
    {
        ExplosionActiveWorldState_RemoveStableOwners(context, created);
        return FailActiveWorld(
            "Explosion allocated roster is not canonical");
    }
    return true;
}

bool ExplosionActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableExplosionRecord> records;
    std::vector<BoundedExplosion *> objects;
    if (context == NULL || !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &objects) ||
        !RosterMatches(objects, context, records))
        return false;

    std::vector<AttributeExplosion *> attributes(records.size(), NULL);
    int targetBranches = 0;
    int targetTracedParents = 0;
    int targetSounds = 0;
    int currentOwnedSounds = 0;
    bool needsParticleVisuals = false;
    bool needsSmokeVisuals = false;
    bool needsPieceVisuals = false;
    bool needsTraceReferences = false;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        bool recordNeedsPieceVisual = false;
        if (!context->isExist(records[index].attribute.c_str()))
            return FailActiveWorld(
                "EXP1 ExplosionAttr dependency is missing");
        const KR_ObjectID attributeID =
            context->searchObject(records[index].attribute.c_str());
        attributes[index] = static_cast<AttributeExplosion *>(
            __attrExplosionTable.searchAttribute(attributeID));
        if (attributes[index] == NULL)
            return FailActiveWorld(
                "EXP1 ExplosionAttr dependency has wrong type");
        targetBranches +=
            static_cast<int>(records[index].branches.size());
        targetTracedParents += records[index].traceQuotaHeld;
        targetSounds += records[index].hasSound;
        if (!IsNul(objects[index]->sound()))
            ++currentOwnedSounds;
        if (records[index].hasSound &&
            (attributes[index]->m_wav == NULL ||
             attributes[index]->m_ctsndID == ct_NULLID))
            return FailActiveWorld(
                "EXP1 owned Sound dependency is unresolved");
        for (std::size_t branch = 0;
             branch < records[index].branches.size(); ++branch)
        {
            switch (records[index].branches[branch].type)
            {
            case EXPLOSION_PARTICLE_SIMPLE:
            case EXPLOSION_PARTICLE_SNAKE:
            case EXPLOSION_PARTICLE_RAY:
                needsParticleVisuals = true;
                break;
            case EXPLOSION_PARTICLE_SMOKE:
                needsSmokeVisuals = true;
                break;
            case EXPLOSION_PARTICLE_PIECE:
                needsPieceVisuals = true;
                recordNeedsPieceVisual = true;
                break;
            case EXPLOSION_PARTICLE_TRACED_PIECE:
                needsPieceVisuals = true;
                recordNeedsPieceVisual = true;
                needsTraceReferences = true;
                break;
            }
        }
        if (recordNeedsPieceVisual &&
            (attributes[index]->m_cacheSkin == NULL ||
             !std::isfinite(attributes[index]->m_cacheSkin->Radius()) ||
             attributes[index]->m_cacheSkin->Radius() <= 0.0))
            return FailActiveWorld(
                "EXP1 Piece skin dependency is unresolved");
    }
    const int foreignSounds =
        SoundObjectState_LiveCount() - currentOwnedSounds;
    if (targetBranches > kParticleBranchCapacity ||
        targetTracedParents > kMaximumTracedExplosionParents)
        return FailActiveWorld(
            "EXP1 particle or traced-parent capacity is exceeded");
    if (foreignSounds < 0 || targetSounds >
            SoundObjectState_Capacity() - foreignSounds)
        return FailActiveWorld("EXP1 SoundObj capacity is exceeded");
    if (needsParticleVisuals &&
        !ExplosionAttributeState_ParticleVisualsResolved(context))
        return FailActiveWorld(
            "EXP1 particle visual dependencies are unresolved");
    if (needsSmokeVisuals &&
        !ExplosionAttributeState_SmokeVisualsResolved(context))
        return FailActiveWorld(
            "EXP1 smoke visual dependencies are unresolved");
    if (needsPieceVisuals &&
        !ExplosionAttributeState_PieceReferencesResolved(context))
        return FailActiveWorld(
            "EXP1 Piece dependencies are unresolved");
    if (needsTraceReferences &&
        !ExplosionAttributeState_TraceReferencesResolved(context))
        return FailActiveWorld(
            "EXP1 traced-Piece dependencies are unresolved");

    // Release every old branch before allocating any new branch. This keeps
    // a valid 500-branch target from failing only because the roster changed
    // its per-owner distribution during the transaction.
    for (std::size_t index = 0; index < objects.size(); ++index)
        if (!objects[index]->clearStableRuntime())
            return FailActiveWorld(
                "EXP1 previous runtime graph release failed");
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!objects[index]->applyStable(records[index], attributes[index]))
        {
            for (std::size_t cleanup = 0; cleanup < objects.size(); ++cleanup)
                objects[cleanup]->clearStableRuntime();
            return FailActiveWorld("EXP1 runtime state application failed");
        }
    std::vector<unsigned char> current;
    if (!ExplosionActiveWorldState_CaptureStable(context, &current) ||
        current != bytes)
        return FailActiveWorld("EXP1 canonical recapture differs");
    return true;
}

void ExplosionActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            DrainPrivateExplosionEvents(context, *object);
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    created->clear();
}

bool ExplosionActiveWorldState_ProbeLiveRoundTrip(
    SimulationContext *context, const char *attributeName,
    double timeStamp, ExplosionActiveWorldProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    g_activeWorldFailure.clear();
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_explosionTable.liveCount() != 0 ||
        g_particleBranchesLive != 0 || g_tracedExplosionParents != 0)
        return FailActiveWorld(
            "Explosion active-world probe requires an empty Explosion graph");

    const KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeExplosion *attribute = static_cast<AttributeExplosion *>(
        __attrExplosionTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("ExplosionAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Explosion");
    const int attributeIndex = attributeTable == ct_NULLID ||
            IsNul(attributeID)
        ? -1 : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (attribute == NULL || attributeIndex < 0 ||
        subjectTable == ct_NULLID || attribute->m_wav == NULL ||
        attribute->m_ctsndID == ct_NULLID)
        return FailActiveWorld(
            "Explosion active-world probe attribute/table is unavailable");

    const int executedBefore = g_executedCommands;
    const int damageBefore = g_damageApplications;
    const int impulseBefore = g_impulseApplications;
    const int allocationBefore = g_allocationRollbacks;
    const int queueBefore = g_queueRollbacks;
    const int drawBefore = g_pieceDrawCalls;
    const int puffBefore = g_tracePuffsStarted;
    const int soundBefore = SoundObjectState_LiveCount();
    const double frameBefore = Session::m_frameSec;
    KR_ObjectID original = KR_ObjectID::NUL();
    KR_ObjectID stagedID = KR_ObjectID::NUL();
    KR_ObjectID restoredID = KR_ObjectID::NUL();
    KR_ObjectID firstSound = KR_ObjectID::NUL();
    KR_ObjectID stagedSound = KR_ObjectID::NUL();
    KR_ObjectID restoredSound = KR_ObjectID::NUL();
    std::vector<KR_ObjectID> originalOwners;
    std::vector<KR_ObjectID> staged;
    std::vector<KR_ObjectID> restored;
    bool success = false;

    do
    {
        const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
        ExplosionImpactRequest request;
        request.position = CFVector3(4096.0, 10000.0, -4096.0);
        request.timeStamp = ts;
        request.damageOwner = KR_ObjectID::NUL();
        request.subjectTable = subjectTable;
        request.attributeIndex = attributeIndex;
        request.objectName = "Explosion.ActiveWorld.Probe";
        int damageApplications = -1;
        Session::m_frameSec = 0.0;
        const bool executed = ExplosionSubjectState_ExecuteNow(
            context, request, &damageApplications);
        Session::m_frameSec = frameBefore;
        original = context->searchObject(request.objectName);
        BoundedExplosion *object = g_explosionTable.find(original);
        if (!executed || original.isNUL() || object == NULL ||
            object->particleCount() <= 0 || IsNul(object->sound()))
        {
            FailActiveWorld(
                "Explosion active-world probe start was not retained");
            break;
        }
        firstSound = object->sound();

        std::vector<unsigned char> bytes;
        const int branchCount = object->particleCount();
        if (!ExplosionActiveWorldState_CaptureStable(context, &bytes))
            break;
        const int decodedBranches =
            ExplosionActiveWorldState_BranchCount(bytes);
        const int decodedEvents =
            ExplosionActiveWorldState_SchedulerEventCount(bytes);
        const bool collected = ExplosionActiveWorldState_CollectStableOwners(
            context, bytes, &originalOwners);
        if (decodedBranches != branchCount || decodedEvents < 1 ||
            decodedEvents > 2 || !collected || originalOwners.size() != 1 ||
            (originalOwners.size() == 1 && originalOwners[0] != original))
        {
            char message[256] = {};
            std::snprintf(
                message, sizeof(message),
                "EXP1 live capture branches=%d/%d events=%d collected=%d "
                "owners=%u",
                decodedBranches, branchCount, decodedEvents,
                collected ? 1 : 0,
                static_cast<unsigned int>(originalOwners.size()));
            FailActiveWorld(message);
            break;
        }
        unsigned long long fingerprint = kHashOffset;
        HashBytes(fingerprint, &bytes[0], static_cast<int>(bytes.size()));
        ExplosionActiveWorldState_RemoveStableOwners(
            context, &originalOwners);
        if (g_explosionTable.liveCount() != 0 ||
            g_particleBranchesLive != 0 || g_tracedExplosionParents != 0 ||
            SoundObjectState_LiveCount() != soundBefore)
        {
            FailActiveWorld("EXP1 original graph teardown failed");
            break;
        }

        if (!ExplosionActiveWorldState_CreateStableOwners(
                context, bytes, &staged) || staged.size() != 1 ||
            staged[0] == original ||
            !ExplosionActiveWorldState_ApplyStableReferences(context, bytes) ||
            !ExplosionActiveWorldState_MatchesStable(context, bytes))
        {
            FailActiveWorld("EXP1 staged reconstruction failed");
            break;
        }
        stagedID = staged[0];
        object = g_explosionTable.find(stagedID);
        stagedSound = object == NULL
            ? KR_ObjectID::NUL() : object->sound();
        if (stagedSound.isNUL() || stagedSound == firstSound)
        {
            FailActiveWorld("EXP1 staged Sound identity was not fresh");
            break;
        }
        ExplosionActiveWorldState_RemoveStableOwners(context, &staged);
        if (g_explosionTable.liveCount() != 0 ||
            g_particleBranchesLive != 0 || g_tracedExplosionParents != 0 ||
            SoundObjectState_LiveCount() != soundBefore)
        {
            FailActiveWorld("EXP1 staged rollback retained graph state");
            break;
        }

        if (!ExplosionActiveWorldState_CreateStableOwners(
                context, bytes, &restored) || restored.size() != 1 ||
            restored[0] == original || restored[0] == stagedID ||
            !ExplosionActiveWorldState_ApplyStableReferences(context, bytes) ||
            !ExplosionActiveWorldState_MatchesStable(context, bytes))
        {
            FailActiveWorld("EXP1 final reconstruction failed");
            break;
        }
        restoredID = restored[0];
        BoundedExplosion *resumed = g_explosionTable.find(restoredID);
        restoredSound = resumed == NULL
            ? KR_ObjectID::NUL() : resumed->sound();
        KR_Event pendingMove[2];
        const int pendingMoveCount = context->copyEvents(
            EXPLOSION_MOVE, restoredID, pendingMove, 2);
        const double previousMove = resumed == NULL
            ? 0.0 : resumed->previousMoveTime();
        if (resumed == NULL || restoredSound.isNUL() ||
            restoredSound == firstSound || restoredSound == stagedSound ||
            pendingMoveCount != 1 ||
            context->removeEvent(EXPLOSION_MOVE, restoredID) != 1)
        {
            FailActiveWorld("EXP1 restored MOVE/Sound state is unavailable");
            break;
        }
        context->sendEventNow(pendingMove[0]);
        KR_Event nextMove[2];
        if (!context->isExist(restoredID) ||
            !NearlyEqual(resumed->previousMoveTime(),
                         pendingMove[0].timeStamp) ||
            NearlyEqual(resumed->previousMoveTime(), previousMove) ||
            context->copyEvents(
                EXPLOSION_MOVE, restoredID, nextMove, 2) != 1)
        {
            FailActiveWorld("EXP1 restored Explosion did not resume movement");
            break;
        }

        summary->capturedOwners = 1;
        summary->capturedBranches = branchCount;
        summary->schedulerEvents =
            ExplosionActiveWorldState_SchedulerEventCount(bytes);
        summary->soundChildren = 1;
        summary->stagedRollbacks = 1;
        summary->reconstructedOwners = 1;
        summary->stableRoundTrips = 2;
        summary->resumedMoves = 1;
        summary->fingerprint = fingerprint;
        success = true;
    } while (false);

    Session::m_frameSec = frameBefore;
    ExplosionActiveWorldState_RemoveStableOwners(context, &restored);
    ExplosionActiveWorldState_RemoveStableOwners(context, &staged);
    ExplosionActiveWorldState_RemoveStableOwners(context, &originalOwners);
    RemoveIfPresent(context, original);
    const int lateEvents =
        DrainPrivateExplosionEvents(context, original) +
        DrainPrivateExplosionEvents(context, stagedID) +
        DrainPrivateExplosionEvents(context, restoredID);
    const bool clean = g_explosionTable.liveCount() == 0 &&
        g_particleBranchesLive == 0 && g_tracedExplosionParents == 0 &&
        SoundObjectState_LiveCount() == soundBefore && lateEvents == 0;
    g_executedCommands = executedBefore;
    g_damageApplications = damageBefore;
    g_impulseApplications = impulseBefore;
    g_allocationRollbacks = allocationBefore;
    g_queueRollbacks = queueBefore;
    g_pieceDrawCalls = drawBefore;
    g_tracePuffsStarted = puffBefore;
    if (!success || !clean)
    {
        std::memset(summary, 0, sizeof(*summary));
        if (g_activeWorldFailure.empty())
            FailActiveWorld("EXP1 probe rollback was not clean");
        return false;
    }
    return true;
}
