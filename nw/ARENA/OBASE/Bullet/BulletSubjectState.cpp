#include "BulletSubjectState.h"
#include "BulletActiveWorldState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#define LAST_H__SCENE
#include "game.h"

#include "BulletAttributeState.h"
#include "enum/SpaceEnum.h"
#include "h/light.h"
#include "i/dynobj.i"
#include "kernel/h/context.h"
#include "kernel/h/s_debug.h"
#include "kernel/h/session.h"
#include "message/a_msg.h"
#include "message/bulmsg.h"
#include "message/fountmsg.h"
#include "message/funitmsg.h"
#include "message/sparkmsg.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/spark/SparkSubjectState.h"
#include "storage/h/subject.h"

namespace {

const double kGravity = -9.8;
const double kBulletCollisionRadius = 0.01;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const std::uint32_t kBulletActiveWorldMagic = 0x314c5542u; // BUL1
const std::uint32_t kBulletActiveWorldVersion = 1u;
const std::size_t kMaximumActiveWorldBullets = 4096;
const std::size_t kMaximumActiveWorldString = MAX_SYMBOLIC_LENGHT - 1;

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

BulletRuntimeTelemetry g_runtimeTelemetry = {};
unsigned int g_activeBullets = 0;
std::string g_activeWorldFailure;

struct StableBulletRecord
{
    std::string name;
    std::string attribute;
    std::string master;
    CFVector3 position;
    CFVector3 previousPosition;
    CFVector3 velocity;
    CFVector3 initialPosition;
    CFVector3 initialDirection;
    double lastMoveTimeStamp;
    double lastCollisionTimeStamp;
    double waterline;
    int hasWaterline;
    int crossedWaterline;
    double movingTimeStamp;
    double movingPreviousTimeStamp;
    double collisionTimeStamp;

    StableBulletRecord()
        : position(0.0, 0.0, 0.0),
          previousPosition(0.0, 0.0, 0.0), velocity(0.0, 0.0, 0.0),
          initialPosition(0.0, 0.0, 0.0),
          initialDirection(0.0, 0.0, 0.0), lastMoveTimeStamp(0.0),
          lastCollisionTimeStamp(0.0), waterline(0.0), hasWaterline(0),
          crossedWaterline(0), movingTimeStamp(0.0),
          movingPreviousTimeStamp(0.0), collisionTimeStamp(0.0) {}
};

bool FailActiveWorld(const std::string &message)
{
    g_activeWorldFailure = message;
    return false;
}

struct BulletOwnerRuntimeEntry
{
    char ownerName[MAX_SYMBOLIC_LENGHT + 1];
    BulletRuntimeTelemetry telemetry;
};

std::vector<BulletOwnerRuntimeEntry> g_ownerRuntimeTelemetry;

int FindOwnerRuntimeEntry(const char *ownerName, bool create)
{
    if (ownerName == NULL || ownerName[0] == 0)
        return -1;
    for (std::size_t index = 0; index < g_ownerRuntimeTelemetry.size();
         ++index)
        if (std::strcmp(g_ownerRuntimeTelemetry[index].ownerName,
                        ownerName) == 0)
            return static_cast<int>(index);
    if (!create || std::strlen(ownerName) > MAX_SYMBOLIC_LENGHT)
        return -1;
    BulletOwnerRuntimeEntry entry = {};
    std::strcpy(entry.ownerName, ownerName);
    g_ownerRuntimeTelemetry.push_back(entry);
    return static_cast<int>(g_ownerRuntimeTelemetry.size() - 1);
}

BulletRuntimeTelemetry *OwnerRuntimeTelemetry(int index)
{
    if (index < 0 ||
        static_cast<std::size_t>(index) >= g_ownerRuntimeTelemetry.size())
        return NULL;
    return &g_ownerRuntimeTelemetry[static_cast<std::size_t>(index)].telemetry;
}

void ResetRuntimeTelemetry()
{
    std::memset(&g_runtimeTelemetry, 0, sizeof(g_runtimeTelemetry));
    g_activeBullets = 0;
    g_ownerRuntimeTelemetry.clear();
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

bool QueueImpactEffects(SimulationContext *context,
                        AttributeBullet *attribute,
                        const KR_ObjectID &damageOwner,
                        const CFVector3 &position,
                        const CFVector3 &velocity,
                        double eventTimeStamp,
                        bool splashBeforeImpact, double waterTime,
                        bool hasImpact, double impactTime,
                        KR_ObjectID *children, int *childCount)
{
    if (children == NULL || childCount == NULL)
        return false;
    children[0] = KR_ObjectID::NUL();
    children[1] = KR_ObjectID::NUL();
    *childCount = 0;
    if (context == NULL || attribute == NULL ||
        !FiniteVector(position) || !FiniteVector(velocity) ||
        !std::isfinite(eventTimeStamp) || eventTimeStamp < 0.1)
        return false;

    // The source-only fixture deliberately leaves the dependency transaction
    // unresolved. Collision remains valid there without manufacturing effects.
    if (attribute->m_cacheExplosionTable == ct_NULLID)
        return true;

    ExplosionImpactRequest requests[2] = {};
    int requestCount = 0;
    if (splashBeforeImpact && attribute->m_cacheSplashAttr != ct_NULLID)
    {
        const CFVector3 splashPosition = position + velocity * waterTime;
        if (!FiniteVector(splashPosition) || !std::isfinite(waterTime) ||
            waterTime < 0.0)
            return false;
        requests[requestCount].position = splashPosition;
        requests[requestCount].timeStamp = eventTimeStamp + waterTime;
        requests[requestCount].damageOwner = damageOwner;
        requests[requestCount].subjectTable =
            attribute->m_cacheExplosionTable;
        requests[requestCount].attributeIndex =
            attribute->m_cacheSplashAttr;
        requests[requestCount].objectName = "Expl.Bullet.Splash";
        ++requestCount;
    }
    if (hasImpact)
    {
        const CFVector3 impactPosition = position + velocity * impactTime;
        if (attribute->m_cacheExplAttr == ct_NULLID ||
            !FiniteVector(impactPosition) || !std::isfinite(impactTime) ||
            impactTime < 0.0)
            return false;
        requests[requestCount].position = impactPosition;
        requests[requestCount].timeStamp = eventTimeStamp + impactTime;
        requests[requestCount].damageOwner = damageOwner;
        requests[requestCount].subjectTable =
            attribute->m_cacheExplosionTable;
        requests[requestCount].attributeIndex = attribute->m_cacheExplAttr;
        requests[requestCount].objectName = "Expl.Bullet.Impact";
        ++requestCount;
    }
    if (requestCount == 0)
        return true;
    if (!ExplosionSubjectState_QueueBatch(
            context, requests, requestCount, children))
        return false;
    *childCount = requestCount;
    return true;
}

bool QueueGroundSpark(SimulationContext *context,
                      AttributeBullet *attribute,
                      const CFVector3 &position,
                      double timeStamp, KR_ObjectID *child)
{
    if (child == NULL)
        return false;
    *child = KR_ObjectID::NUL();
    if (context == NULL || attribute == NULL ||
        !FiniteVector(position) || !std::isfinite(timeStamp) ||
        timeStamp < 0.1)
        return false;
    // The January source-only fixture has no resolved dependencies. Ground
    // removal remains valid there without manufacturing a visual child.
    if (attribute->m_cacheSparkTable == ct_NULLID ||
        attribute->m_cacheSparkAttr == ct_NULLID)
        return true;
    SparkCreateRequest request = {
        position, timeStamp, attribute->m_cacheSparkTable,
        attribute->m_cacheSparkAttr, "S"};
    return SparkSubjectState_QueueCreate(context, request, child);
}

bool StartBarrelSmoke(SimulationContext *context,
                      AttributeBullet *attribute,
                      const KR_ObjectID &source,
                      const CFVector3 &position,
                      const CFVector3 &direction,
                      double timeStamp, KR_ObjectID *child)
{
    if (child == NULL)
        return false;
    *child = KR_ObjectID::NUL();
    if (context == NULL || attribute == NULL ||
        !FiniteVector(position) || !FiniteVector(direction) ||
        !std::isfinite(timeStamp) || timeStamp < 0.1 ||
        !std::isfinite(Session::m_frameSec))
        return false;
    if (attribute->m_useBarellSmoke == 0 ||
        Session::m_frameSec > 0.09)
        return true;
    // The source-only fixture deliberately leaves the dependency transaction
    // unresolved. The presentation branch remains a valid no-op there.
    if (attribute->m_smokeTableID == ct_NULLID ||
        attribute->m_smokeAttrID.isNUL())
        return true;
    if (!SmokeSubjectState_RenderingSupported(
            context, attribute->m_smokeAttrName))
        return false;
    SmokeDirectionalStartRequest request = {
        position, direction, timeStamp, source,
        attribute->m_smokeTableID, attribute->m_smokeAttrID,
        attribute->m_smokeAttrName, "Smok."};
    return SmokeSubjectState_StartWithDirection(
        context, request, child);
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

class BulletParticleDrawable : public CViewSphericDynamic
{
 public:
    BulletParticleDrawable()
        : m_radius0(0.0), m_radius1(0.0), m_step0(0.0), m_step(0.0),
          m_colors(NULL)
    {
        m_start = CFVector3(0.0, 0.0, 0.0);
        m_end = CFVector3(0.0, 0.0, 0.0);
    }

    void prepare(const CFVector3 &start, const CFVector3 &end,
                 double radius0, double radius1, double step0,
                 double step, unsigned long *colors)
    {
        m_start = start;
        m_end = end;
        m_radius0 = radius0;
        m_radius1 = radius1;
        m_step0 = step0;
        m_step = step;
        m_colors = colors;
        m_dynBase = m_dynBase1 = m_bump.start = (start + end) * 0.5;
        m_bump.vel = CFVector3(0.0, 0.0, 0.0);
        m_bump.fTime = 0.0;
        m_bump.fRadius = Abs(end - start) * 0.5 +
            (std::max)(radius0, radius1);
    }

    virtual void Draw()
    {
        if (_pGRDrawParticle == NULL || m_colors == NULL)
            return;
        const CFVector3 segment = m_end - m_start;
        const double length = Abs(segment);
        if (!std::isfinite(length) || length <= 1.0e-8)
            return;
        const CFVector3 direction = segment / length;
        double offset = 0.0;
        double step = m_step0;
        if (!std::isfinite(step) || step <= 1.0e-5)
            step = length;
        const double acceleration =
            std::isfinite(m_step) && m_step > 0.0 ? m_step : 0.0;
        for (int sample = 0; sample < 256 && offset <= length; ++sample)
        {
            const double fraction = offset / length;
            const CFVector3 position = m_start + direction * offset;
            const CFVector3 view =
                CViewObject::m_viewPointDirSMx * position;
            if (std::isfinite(view.z) &&
                view.z >= CViewObject::m_fFrontClip)
            {
                const double inverse = 1.0 / view.z;
                const double radius = m_radius0 +
                    (m_radius1 - m_radius0) * fraction;
                int size = Round(radius *
                                 CViewObject::m_viewPointScale.x * inverse);
                if (size < 1)
                    size = 1;
                const int inverseZ =
                    static_cast<int>(65536.0 * inverse);
                int color = static_cast<int>(fraction *
                    (RR2NW_BULLET_COLOR_GRAD - 1));
                if (color < 0)
                    color = 0;
                else if (color >= RR2NW_BULLET_COLOR_GRAD)
                    color = RR2NW_BULLET_COLOR_GRAD - 1;
                if (inverseZ > 0)
                    GRDrawParticle(Round(view.x * inverse),
                                   Round(view.y * inverse), size,
                                   inverseZ, m_colors[color]);
            }
            offset += step;
            step += acceleration;
        }
    }

 private:
    CFVector3 m_start;
    CFVector3 m_end;
    double m_radius0;
    double m_radius1;
    double m_step0;
    double m_step;
    unsigned long *m_colors;
};

class BulletSkinDrawable : public CViewSphericDynamic
{
 public:
    explicit BulletSkinDrawable(CViewObjectRef &skin) : m_skin(skin) {}

    void prepare()
    {
        if (m_skin.Model() == NULL)
            return;
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
    }

 private:
    CViewObjectRef &m_skin;
};

class BoundedBullet : public ct_Subject
{
 public:
    BoundedBullet() : m_skinDrawable(m_skin)
    {
        resetState();
    }

    virtual CFVector3 realPosition() { return m_position; }
    virtual bool shouldDump() { return false; }

    virtual void render(CViewDynamicList &list, double timeStamp)
    {
        if (!m_started || m_attribute == NULL)
            return;
        const CFVector3 interpolated = presentationPosition(timeStamp);
        if (m_attribute->m_useSkin != 0)
        {
            if (!m_skinAttached || m_skin.Model() == NULL)
            {
                ++g_runtimeTelemetry.skippedSkinRenderSubmissions;
                BulletRuntimeTelemetry *owner =
                    OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
                if (owner != NULL)
                    ++owner->skippedSkinRenderSubmissions;
                return;
            }
            prepareSkin(interpolated, timeStamp);
            m_skinDrawable.prepare();
            list.Load(&m_skinDrawable);
            m_publishedDrawable = &m_skinDrawable;
            ++g_runtimeTelemetry.skinRenderSubmissions;
            BulletRuntimeTelemetry *owner =
                OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
            if (owner != NULL)
                ++owner->skinRenderSubmissions;
        }
        else
        {
            CFVector3 start = m_previousPosition;
            const CFVector3 segment = interpolated - start;
            const double length = Abs(segment);
            if (std::isfinite(length) && length > m_attribute->m_length &&
                m_attribute->m_length > 0.0)
                start = interpolated -
                    segment * (m_attribute->m_length / length);
            m_particleDrawable.prepare(
                start, interpolated, m_attribute->m_radius0,
                m_attribute->m_radius1, m_attribute->m_step0,
                m_attribute->m_step, m_attribute->m_colorGrad);
            list.Load(&m_particleDrawable);
            m_publishedDrawable = &m_particleDrawable;
            ++g_runtimeTelemetry.particleRenderSubmissions;
            BulletRuntimeTelemetry *owner =
                OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
            if (owner != NULL)
                ++owner->particleRenderSubmissions;
        }
        ++g_runtimeTelemetry.renderSubmissions;
        BulletRuntimeTelemetry *owner =
            OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
        if (owner != NULL)
            ++owner->renderSubmissions;
        if (m_attribute->m_useLight != 0 &&
            m_attribute->m_lightColor >= 0 &&
            m_attribute->m_lightColor < LIGHT_COLOR_COUNT &&
            std::isfinite(m_attribute->m_lightRadius) &&
            m_attribute->m_lightRadius > 0.0)
            g_lightChain.add(interpolated, m_attribute->m_lightColor,
                             m_attribute->m_lightBrightness,
                             m_attribute->m_lightRadius);
    }

    virtual void endRender(CViewScene *scene)
    {
        if (m_publishedDrawable == NULL)
            return;
        if (scene != NULL)
            scene->RemoveLandDynamic(m_publishedDrawable);
        m_publishedDrawable = NULL;
    }

    virtual void addNotify()
    {
        ct_Subject::addNotify();
        resetState();
    }

    virtual void removeNotify()
    {
        if (m_publishedDrawable != NULL)
            endRender(CViewScene::Current());
        releaseActiveCount();
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

    bool skinAttached() const { return m_skinAttached; }
    bool presentationPublished() const
    {
        return m_publishedDrawable != NULL;
    }

    bool captureStable(SimulationContext *world,
                       StableBulletRecord *record)
    {
        if (world == NULL || record == NULL || context != world)
            return FailActiveWorld(
                "live Bullet capture has an invalid world/record context");
        if (!m_started || m_attribute == NULL)
        {
            char failure[256];
            const int startEvents = world->copyEventsTo(
                b_EV_START, getObjectID(), NULL, 0);
            const int movingEvents = world->copyEvents(
                b_EVC_MOVING, getObjectID(), NULL, 0);
            const int collisionEvents = world->copyEvents(
                b_EVC_CHECK_COLLISION, getObjectID(), NULL, 0);
            std::snprintf(
                failure, sizeof(failure),
                "live Bullet is not ready for canonical capture "
                "(started=%d attr=%d clean=%d start=%d moving=%d "
                "collision=%d moves=%d)",
                m_started ? 1 : 0, m_attribute != NULL ? 1 : 0,
                clean() ? 1 : 0, startEvents, movingEvents,
                collisionEvents, m_moveCount);
            return FailActiveWorld(failure);
        }
        record->name = ObjectName(world, getObjectID());
        record->attribute =
            ObjectName(world, m_attribute->getObjectID());
        record->master = ObjectName(world, m_master);
        if (record->name.empty() || record->attribute.empty())
            return FailActiveWorld(
                "live Bullet symbolic owner/attribute is missing");
        if (!record->master.empty() &&
            (!world->isExist(record->master.c_str()) ||
             world->searchObject(record->master.c_str()) != m_master))
            return FailActiveWorld(
                "live Bullet master name is absent or ambiguous");

        KR_Event moving[2];
        KR_Event collision[2];
        const int movingCount = world->copyEvents(
            b_EVC_MOVING, getObjectID(), moving, 2);
        const int collisionCount = world->copyEvents(
            b_EVC_CHECK_COLLISION, getObjectID(), collision, 2);
        if (movingCount != 1 || collisionCount != 1 ||
            moving[0].source != getObjectID() ||
            moving[0].destination != getObjectID() ||
            collision[0].source != getObjectID() ||
            collision[0].destination != getObjectID() ||
            collision[0].data.size() != 0)
            return FailActiveWorld(
                "live Bullet is outside the quiescent two-event boundary");
        s_EventData &movingData = moving[0].data.open(EDO_READ);
        if (movingData.remaining() != static_cast<int>(sizeof(double)))
        {
            movingData.close();
            return FailActiveWorld(
                "live Bullet moving event payload is invalid");
        }
        movingData.getDouble(record->movingPreviousTimeStamp).close();

        record->position = m_position;
        record->previousPosition = m_previousPosition;
        record->velocity = m_velocity;
        record->initialPosition = m_initialPosition;
        record->initialDirection = m_initialDirection;
        record->lastMoveTimeStamp = m_lastMoveTimeStamp;
        record->lastCollisionTimeStamp = m_lastCollisionTimeStamp;
        record->waterline = m_hasWaterline ? m_waterline : 0.0;
        record->hasWaterline = m_hasWaterline ? 1 : 0;
        record->crossedWaterline = m_crossedWaterline ? 1 : 0;
        record->movingTimeStamp = moving[0].timeStamp;
        record->collisionTimeStamp = collision[0].timeStamp;
        return true;
    }

    bool applyStable(const StableBulletRecord &record,
                     AttributeBullet *attribute,
                     const KR_ObjectID &master)
    {
        if (context == NULL || attribute == NULL)
            return false;
        releaseActiveCount();
        context->removeEvent(b_EVC_MOVING, getObjectID());
        context->removeEvent(b_EVC_CHECK_COLLISION, getObjectID());
        resetState();

        m_previousPosition = record.previousPosition;
        m_velocity = record.velocity;
        m_initialPosition = record.initialPosition;
        m_initialDirection = record.initialDirection;
        m_attribute = attribute;
        m_master = master;
        m_started = true;
        m_lastMoveTimeStamp = record.lastMoveTimeStamp;
        m_lastCollisionTimeStamp = record.lastCollisionTimeStamp;
        m_waterline = record.waterline;
        m_hasWaterline = record.hasWaterline != 0;
        m_crossedWaterline = record.crossedWaterline != 0;
        m_countedActive = true;
        m_ownerTelemetryIndex = FindOwnerRuntimeEntry(
            record.master.empty() ? NULL : record.master.c_str(), true);
        attachPresentation();
        ++g_activeBullets;
        g_runtimeTelemetry.peakLiveBullets = (std::max)(
            g_runtimeTelemetry.peakLiveBullets, g_activeBullets);
        BulletRuntimeTelemetry *owner =
            OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
        if (owner != NULL)
        {
            ++owner->liveBullets;
            owner->peakLiveBullets = (std::max)(
                owner->peakLiveBullets, owner->liveBullets);
        }
        setPosition(record.position);

        KR_Event moving;
        moving.label = b_EVC_MOVING;
        moving.source = getObjectID();
        moving.destination = getObjectID();
        moving.timeStamp = record.movingTimeStamp;
        moving.data.open(EDO_WRITE)
                   .putDouble(record.movingPreviousTimeStamp)
                   .close();
        context->addEvent(moving);
        KR_Event collision;
        collision.label = b_EVC_CHECK_COLLISION;
        collision.source = getObjectID();
        collision.destination = getObjectID();
        collision.timeStamp = record.collisionTimeStamp;
        context->addEvent(collision);
        return true;
    }

 private:
    void releaseActiveCount()
    {
        if (!m_countedActive)
            return;
        if (g_activeBullets > 0)
            --g_activeBullets;
        BulletRuntimeTelemetry *owner =
            OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
        if (owner != NULL && owner->liveBullets > 0)
            --owner->liveBullets;
        m_countedActive = false;
    }

    int rejectStart()
    {
        ++g_runtimeTelemetry.rejectedStarts;
        return 0;
    }

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
        m_countedActive = false;
        m_ownerTelemetryIndex = -1;
        m_skinAttached = false;
        m_publishedDrawable = NULL;
    }

    void attachPresentation()
    {
        m_skinAttached = false;
        if (m_attribute != NULL && m_attribute->m_useSkin != 0 &&
            m_attribute->m_cacheSkin != NULL)
        {
            m_skin.Attach(m_attribute->m_cacheSkin);
            m_skinAttached = m_skin.Model() != NULL;
        }
    }

    CFVector3 presentationPosition(double timeStamp) const
    {
        double delta = timeStamp - m_lastMoveTimeStamp;
        if (!std::isfinite(delta) || delta < 0.0)
            delta = 0.0;
        const double maximum = m_attribute != NULL &&
                std::isfinite(m_attribute->m_moveTimeIncrement) &&
                m_attribute->m_moveTimeIncrement > 0.0
            ? m_attribute->m_moveTimeIncrement
            : 0.02;
        if (delta > maximum)
            delta = maximum;
        return m_position +
            (m_velocity + CFVector3(0.0, kGravity, 0.0) *
                (0.5 * delta)) * delta;
    }

    void prepareSkin(const CFVector3 &position, double timeStamp)
    {
        CFMatrix3x4 &matrix = m_skin.GetDirModify();
        matrix.LoadIdentity();
        const double heading = std::atan2(-m_velocity.x, -m_velocity.z);
        const double elevation = std::atan2(
            m_velocity.y, std::hypot(m_velocity.x, m_velocity.z));
        matrix.RotateOzL(m_attribute->m_rotSpeedOz * timeStamp);
        matrix.RotateOxL(elevation +
                         m_attribute->m_rotSpeedOx * timeStamp);
        matrix.RotateOyL(heading +
                         m_attribute->m_rotSpeedOy * timeStamp);
        matrix.TranslateL(position);
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
            return rejectStart();
        s_EventData &data = event.data.open(EDO_READ);
        if (data.remaining() != expectedSize)
        {
            data.close();
            return rejectStart();
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
            return rejectStart();

        const double inverseLength = 1.0 / std::sqrt(directionLength2);
        m_attribute = attribute;
        m_master = master;
        const char *ownerName = context->searchObject(master);
        m_ownerTelemetryIndex = FindOwnerRuntimeEntry(ownerName, true);
        m_initialPosition = position;
        m_previousPosition = position;
        m_initialDirection = direction * inverseLength;
        m_velocity = m_initialDirection * attribute->m_startSpeed;
        m_started = true;
        attachPresentation();
        m_countedActive = true;
        ++g_activeBullets;
        ++g_runtimeTelemetry.acceptedStarts;
        g_runtimeTelemetry.peakLiveBullets = (std::max)(
            g_runtimeTelemetry.peakLiveBullets, g_activeBullets);
        BulletRuntimeTelemetry *owner =
            OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
        if (owner != NULL)
        {
            ++owner->acceptedStarts;
            ++owner->liveBullets;
            owner->peakLiveBullets = (std::max)(
                owner->peakLiveBullets, owner->liveBullets);
        }
        m_moveCount = 0;
        m_lastMoveTimeStamp = event.timeStamp;
        CViewScene *scene = CViewScene::Current();
        if (scene != NULL && scene->GetTerrain() != NULL)
        {
            m_waterline = scene->GetTerrain()->Waterline();
            m_hasWaterline = std::isfinite(m_waterline) != 0;
        }
        setPosition(position);
        KR_ObjectID barrelSmoke = KR_ObjectID::NUL();
        StartBarrelSmoke(context, attribute, getObjectID(), position,
                         direction, event.timeStamp, &barrelSmoke);
        if (!barrelSmoke.isNUL())
        {
            ++g_runtimeTelemetry.barrelSmokeStarts;
            if (owner != NULL)
                ++owner->barrelSmokeStarts;
        }
        if (position.y <= 0.0)
        {
            KR_ObjectID spark = KR_ObjectID::NUL();
            QueueGroundSpark(context, m_attribute, m_position,
                             event.timeStamp, &spark);
            ++g_runtimeTelemetry.groundRemovals;
            if (owner != NULL)
                ++owner->groundRemovals;
            const KR_ObjectID self = getObjectID();
            context->removeObject(self);
            return 1;
        }
        if (!scheduleMove(event.timeStamp,
                          event.timeStamp +
                              attribute->m_moveTimeIncrement) ||
            !scheduleCollision(event.timeStamp))
        {
            if (!barrelSmoke.isNUL() && context->isExist(barrelSmoke))
                SmokeSubjectState_RollbackStarted(
                    context, barrelSmoke);
            context->removeEvent(b_EVC_MOVING, getObjectID());
            context->removeEvent(b_EVC_CHECK_COLLISION, getObjectID());
            setPosition(CFVector3(0.0, 0.0, 0.0));
            ++g_runtimeTelemetry.rolledBackStarts;
            if (owner != NULL)
                ++owner->rolledBackStarts;
            releaseActiveCount();
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
                const double targetSpeed = object->getMoveSpeed();
                CFVector3 targetVelocity(0.0, 0.0, 0.0);
                if (std::isfinite(targetSpeed) && targetSpeed > 1.0e-8)
                {
                    const CFVector3 targetDirection = object->getMoveDir();
                    if (!FiniteVector(targetDirection))
                        targetVelocity = CFVector3(0.0, 0.0, 0.0);
                    else
                        targetVelocity = targetDirection * targetSpeed;
                }
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
        ++g_runtimeTelemetry.collisionChecks;
        BulletRuntimeTelemetry *owner =
            OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
        if (owner != NULL)
            ++owner->collisionChecks;
        const bool splashBeforeImpact = waterHit &&
            (impact.kind == BULLET_IMPACT_NONE || waterTime < impact.time);
        if (splashBeforeImpact)
        {
            m_crossedWaterline = true;
            ++g_runtimeTelemetry.waterlineSplashes;
            if (owner != NULL)
                ++owner->waterlineSplashes;
        }

        KR_ObjectID effectChildren[2] = {
            KR_ObjectID::NUL(), KR_ObjectID::NUL()};
        int effectChildCount = 0;
        QueueImpactEffects(context, m_attribute, m_master, m_position,
                           m_velocity, event.timeStamp,
                           splashBeforeImpact, waterTime,
                           impact.kind != BULLET_IMPACT_NONE, impact.time,
                           effectChildren, &effectChildCount);
        g_runtimeTelemetry.impactEffectChildren +=
            static_cast<unsigned int>((std::max)(effectChildCount, 0));
        if (owner != NULL)
            owner->impactEffectChildren +=
                static_cast<unsigned int>((std::max)(effectChildCount, 0));

        if (impact.kind != BULLET_IMPACT_NONE)
        {
            if (impact.kind == BULLET_IMPACT_SCENE)
            {
                ++g_runtimeTelemetry.sceneImpacts;
                if (owner != NULL)
                    ++owner->sceneImpacts;
            }
            else if (impact.kind == BULLET_IMPACT_DYNAMIC)
            {
                ++g_runtimeTelemetry.dynamicImpacts;
                if (owner != NULL)
                    ++owner->dynamicImpacts;
            }
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
        ++g_runtimeTelemetry.moveEvents;
        BulletRuntimeTelemetry *owner =
            OwnerRuntimeTelemetry(m_ownerTelemetryIndex);
        if (owner != NULL)
            ++owner->moveEvents;
        setPosition(nextPosition);
        if (m_position.y <= 0.0)
        {
            KR_ObjectID spark = KR_ObjectID::NUL();
            QueueGroundSpark(context, m_attribute, m_position,
                             event.timeStamp, &spark);
            ++g_runtimeTelemetry.groundRemovals;
            if (owner != NULL)
                ++owner->groundRemovals;
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
    bool m_countedActive;
    int m_ownerTelemetryIndex;
    CViewObjectRef m_skin;
    BulletSkinDrawable m_skinDrawable;
    BulletParticleDrawable m_particleDrawable;
    bool m_skinAttached;
    CViewDynamic *m_publishedDrawable;
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
        ResetRuntimeTelemetry();
        m_table = new (std::nothrow) BoundedBullet[objectQnty];
        if (m_table == NULL)
            m_maxObjectQnty = 0;
    }

    virtual void freeObjects()
    {
        delete [] m_table;
        m_table = NULL;
        m_maxObjectQnty = 0;
        ResetRuntimeTelemetry();
    }

    virtual ct_Object *getObjectPTR(int index)
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "BoundedBulletTable::getObjectPTR");
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

    BoundedBullet *find(const KR_ObjectID &objectID) const
    {
        for (ct_Subject *object = findFirstSubject(); object != NULL;
             object = findNextSubject(object))
            if (object->getObjectID() == objectID)
                return static_cast<BoundedBullet *>(object);
        return NULL;
    }

    void collect(std::vector<BoundedBullet *> *objects) const
    {
        if (objects == NULL)
            return;
        objects->clear();
        for (ct_Subject *object = findFirstSubject(); object != NULL;
             object = findNextSubject(object))
            objects->push_back(static_cast<BoundedBullet *>(object));
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

bool RollbackBallisticProbeEffects(SimulationContext *context)
{
    if (context == NULL)
        return false;
    bool clean = true;
    KR_ObjectID smoke = context->searchObject("Smok.");
    if (!smoke.isNUL())
    {
        if (!SmokeSubjectState_RollbackStarted(context, smoke))
            clean = false;
        RemoveIfPresent(context, smoke);
    }
    KR_ObjectID spark = context->searchObject("S");
    if (!spark.isNUL())
    {
        if (!SparkSubjectState_RollbackQueued(context, spark))
            clean = false;
        RemoveIfPresent(context, spark);
    }
    return clean && SmokeSubjectState_LiveCount() == 0 &&
           SparkSubjectState_LiveCount() == 0 &&
           !context->isExist("Smok.") && !context->isExist("S");
}

int DrainPrivateBulletEvents(SimulationContext *context,
                             const KR_ObjectID &object)
{
    KR_ObjectID mutableObject = object;
    if (context == NULL || mutableObject.isNUL())
        return 0;
    int removed = 0;
    while (context->removeEvent(b_EVC_MOVING, object) == 1)
        ++removed;
    while (context->removeEvent(b_EVC_CHECK_COLLISION, object) == 1)
        ++removed;
    return removed;
}

bool CollectObjectID(KR_ObjectID object, void *user)
{
    std::vector<KR_ObjectID> *objects =
        static_cast<std::vector<KR_ObjectID> *>(user);
    if (objects == NULL)
        return false;
    objects->push_back(object);
    return true;
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

bool IsBool(int value)
{
    return value == 0 || value == 1;
}

bool NearlySame(double left, double right)
{
    return left == right;
}

bool CollectRuntimeRoster(SimulationContext *context,
                          std::vector<BoundedBullet *> *objects)
{
    if (context == NULL || objects == NULL ||
        g_arena.getContext() != context)
        return false;
    g_bulletTable.collect(objects);
    objects->erase(
        std::remove_if(objects->begin(), objects->end(),
                       [context](BoundedBullet *object) {
                           return object == NULL ||
                               !context->isExist(object->getObjectID()) ||
                               ObjectName(context, object->getObjectID())
                                   .empty();
                       }),
        objects->end());
    std::sort(objects->begin(), objects->end(),
              [context](BoundedBullet *left, BoundedBullet *right) {
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

bool IsDiscardableIdleBullet(SimulationContext *context,
                             BoundedBullet *object)
{
    return context != NULL && object != NULL && object->clean() &&
        context->copyEventsTo(
            b_EV_START, object->getObjectID(), NULL, 0) == 0 &&
        context->copyEvents(
            b_EVC_MOVING, object->getObjectID(), NULL, 0) == 0 &&
        context->copyEvents(
            b_EVC_CHECK_COLLISION, object->getObjectID(), NULL, 0) == 0;
}

bool CollectStableRoster(SimulationContext *context,
                         std::vector<BoundedBullet *> *objects)
{
    if (!CollectRuntimeRoster(context, objects))
        return false;
    objects->erase(
        std::remove_if(objects->begin(), objects->end(),
                       [context](BoundedBullet *object) {
                           return IsDiscardableIdleBullet(context, object);
                       }),
        objects->end());
    return true;
}

bool ValidateStableRecord(const StableBulletRecord &record)
{
    return !record.name.empty() && !record.attribute.empty() &&
           record.name.size() <= kMaximumActiveWorldString &&
           record.attribute.size() <= kMaximumActiveWorldString &&
           record.master.size() <= kMaximumActiveWorldString &&
           FiniteVector(record.position) &&
           FiniteVector(record.previousPosition) &&
           FiniteVector(record.velocity) &&
           FiniteVector(record.initialPosition) &&
           FiniteVector(record.initialDirection) &&
           std::isfinite(record.lastMoveTimeStamp) &&
           record.lastMoveTimeStamp >= 0.1 &&
           std::isfinite(record.lastCollisionTimeStamp) &&
           record.lastCollisionTimeStamp >= 0.1 &&
           std::isfinite(record.waterline) &&
           IsBool(record.hasWaterline) &&
           IsBool(record.crossedWaterline) &&
           (!record.crossedWaterline || record.hasWaterline) &&
           (record.hasWaterline || record.waterline == 0.0) &&
           std::isfinite(record.movingTimeStamp) &&
           record.movingTimeStamp >= 0.1 &&
           std::isfinite(record.movingPreviousTimeStamp) &&
           record.movingPreviousTimeStamp >= 0.1 &&
           std::isfinite(record.collisionTimeStamp) &&
           record.collisionTimeStamp >= 0.1 &&
           NearlySame(record.movingPreviousTimeStamp,
                      record.lastMoveTimeStamp) &&
           NearlySame(record.collisionTimeStamp,
                      record.lastCollisionTimeStamp);
}

bool CollectStableRecords(SimulationContext *context,
                          std::vector<StableBulletRecord> *records)
{
    std::vector<BoundedBullet *> objects;
    if (records == NULL || !CollectStableRoster(context, &objects))
        return false;
    records->clear();
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        StableBulletRecord record;
        if (!objects[index]->captureStable(context, &record) ||
            !ValidateStableRecord(record))
            return false;
        records->push_back(record);
    }
    return true;
}

bool RosterMatches(const std::vector<BoundedBullet *> &objects,
                   SimulationContext *context,
                   const std::vector<StableBulletRecord> &records)
{
    if (objects.size() != records.size())
        return false;
    for (std::size_t index = 0; index < records.size(); ++index)
        if (ObjectName(context, objects[index]->getObjectID()) !=
            records[index].name)
            return false;
    return true;
}

bool AllOwnersStarted(const std::vector<BoundedBullet *> &objects)
{
    for (std::size_t index = 0; index < objects.size(); ++index)
        if (objects[index] == NULL || !objects[index]->started() ||
            objects[index]->attribute() == NULL)
            return false;
    return true;
}

bool SelectIdleRestoreRoster(
    SimulationContext *context,
    const std::vector<StableBulletRecord> &records,
    std::vector<BoundedBullet *> *objects)
{
    std::vector<BoundedBullet *> runtime;
    if (objects == NULL || !CollectRuntimeRoster(context, &runtime))
        return false;
    objects->clear();
    std::vector<unsigned char> selected(runtime.size(), 0);
    for (std::size_t record = 0; record < records.size(); ++record)
    {
        std::size_t match = runtime.size();
        for (std::size_t candidate = 0; candidate < runtime.size();
             ++candidate)
            if (!selected[candidate] &&
                IsDiscardableIdleBullet(context, runtime[candidate]) &&
                ObjectName(context, runtime[candidate]->getObjectID()) ==
                    records[record].name)
            {
                match = candidate;
                break;
            }
        if (match == runtime.size())
        {
            objects->clear();
            return false;
        }
        selected[match] = 1;
        objects->push_back(runtime[match]);
    }
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

bool PutString(std::vector<unsigned char> *bytes, const std::string &value)
{
    if (value.size() > kMaximumActiveWorldString ||
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

bool PutStableRecord(std::vector<unsigned char> *bytes,
                     const StableBulletRecord &record)
{
    if (!PutString(bytes, record.name) ||
        !PutString(bytes, record.attribute) ||
        !PutString(bytes, record.master))
        return false;
    PutVector(bytes, record.position);
    PutVector(bytes, record.previousPosition);
    PutVector(bytes, record.velocity);
    PutVector(bytes, record.initialPosition);
    PutVector(bytes, record.initialDirection);
    PutDouble(bytes, record.lastMoveTimeStamp);
    PutDouble(bytes, record.lastCollisionTimeStamp);
    PutDouble(bytes, record.waterline);
    PutU32(bytes, static_cast<std::uint32_t>(record.hasWaterline));
    PutU32(bytes, static_cast<std::uint32_t>(record.crossedWaterline));
    PutDouble(bytes, record.movingTimeStamp);
    PutDouble(bytes, record.movingPreviousTimeStamp);
    PutDouble(bytes, record.collisionTimeStamp);
    return true;
}

bool GetStableRecord(const std::vector<unsigned char> &bytes,
                     std::size_t *offset, StableBulletRecord *record)
{
    std::uint32_t hasWaterline = 0, crossedWaterline = 0;
    if (record == NULL || !GetString(bytes, offset, &record->name) ||
        !GetString(bytes, offset, &record->attribute) ||
        !GetString(bytes, offset, &record->master) ||
        !GetVector(bytes, offset, &record->position) ||
        !GetVector(bytes, offset, &record->previousPosition) ||
        !GetVector(bytes, offset, &record->velocity) ||
        !GetVector(bytes, offset, &record->initialPosition) ||
        !GetVector(bytes, offset, &record->initialDirection) ||
        !GetDouble(bytes, offset, &record->lastMoveTimeStamp) ||
        !GetDouble(bytes, offset, &record->lastCollisionTimeStamp) ||
        !GetDouble(bytes, offset, &record->waterline) ||
        !GetU32(bytes, offset, &hasWaterline) ||
        !GetU32(bytes, offset, &crossedWaterline) ||
        !GetDouble(bytes, offset, &record->movingTimeStamp) ||
        !GetDouble(bytes, offset, &record->movingPreviousTimeStamp) ||
        !GetDouble(bytes, offset, &record->collisionTimeStamp))
        return false;
    record->hasWaterline = static_cast<int>(hasWaterline);
    record->crossedWaterline = static_cast<int>(crossedWaterline);
    return ValidateStableRecord(*record);
}

bool EncodeStableRecords(const std::vector<StableBulletRecord> &records,
                         std::vector<unsigned char> *bytes)
{
    if (bytes == NULL || records.size() > kMaximumActiveWorldBullets)
        return false;
    bytes->clear();
    PutU32(bytes, kBulletActiveWorldMagic);
    PutU32(bytes, kBulletActiveWorldVersion);
    PutU32(bytes, static_cast<std::uint32_t>(records.size()));
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!ValidateStableRecord(records[index]) ||
            (index != 0 && records[index - 1].name > records[index].name) ||
            !PutStableRecord(bytes, records[index]))
            return FailActiveWorld(
                "BUL1 record validation/encoding failed");
    }
    return true;
}

bool DecodeStableRecords(const std::vector<unsigned char> &bytes,
                         std::vector<StableBulletRecord> *records)
{
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, count = 0;
    if (records == NULL || !GetU32(bytes, &offset, &magic) ||
        !GetU32(bytes, &offset, &version) ||
        !GetU32(bytes, &offset, &count) ||
        magic != kBulletActiveWorldMagic ||
        version != kBulletActiveWorldVersion ||
        count > kMaximumActiveWorldBullets)
        return false;
    records->clear();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        StableBulletRecord record;
        if (!GetStableRecord(bytes, &offset, &record) ||
            (!records->empty() && records->back().name > record.name))
            return false;
        records->push_back(record);
    }
    return offset == bytes.size();
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
    const int impactCommands = 1;
    const int splashCommands = 1;
    const int visualEffects = 1;
    const int groundSpark = 1;
    const int barrelSmoke = 1;
    const int strictFrameGate = 1;
    const int directionalSmokeStart = 1;
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
    HashBytes(hash, &impactCommands, sizeof(impactCommands));
    HashBytes(hash, &splashCommands, sizeof(splashCommands));
    HashBytes(hash, &visualEffects, sizeof(visualEffects));
    HashBytes(hash, &groundSpark, sizeof(groundSpark));
    HashBytes(hash, &barrelSmoke, sizeof(barrelSmoke));
    HashBytes(hash, &strictFrameGate, sizeof(strictFrameGate));
    HashBytes(hash, &directionalSmokeStart,
              sizeof(directionalSmokeStart));
    return hash;
}

bool BulletSubjectState_RuntimeTelemetry(
    SimulationContext *context, BulletRuntimeTelemetry *telemetry)
{
    const int capacity = g_bulletTable.capacity();
    if (telemetry == NULL ||
        !BulletSubjectState_TableReady(context, capacity))
        return false;
    *telemetry = g_runtimeTelemetry;
    telemetry->liveBullets = static_cast<unsigned int>(
        (std::max)(g_bulletTable.liveCount(), 0));
    return true;
}

bool BulletSubjectState_OwnerRuntimeTelemetry(
    SimulationContext *context, const char *ownerName,
    BulletRuntimeTelemetry *telemetry)
{
    const int capacity = g_bulletTable.capacity();
    if (telemetry == NULL || ownerName == NULL || ownerName[0] == 0 ||
        !BulletSubjectState_TableReady(context, capacity))
        return false;
    std::memset(telemetry, 0, sizeof(*telemetry));
    const int index = FindOwnerRuntimeEntry(ownerName, false);
    BulletRuntimeTelemetry *owner = OwnerRuntimeTelemetry(index);
    if (owner != NULL)
        *telemetry = *owner;
    return true;
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
        SmokeSubjectState_LiveCount() != 0 ||
        SparkSubjectState_LiveCount() != 0 ||
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
        RollbackBallisticProbeEffects(context);
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
    const bool flightEffectsClean = RollbackBallisticProbeEffects(context);
    if (!moved || !secondScheduled || !flightEffectsClean ||
        g_bulletTable.liveCount() != baseline)
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
    const bool groundEffectsClean = RollbackBallisticProbeEffects(context);

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
    const bool rollbackEffectsClean = RollbackBallisticProbeEffects(context);
    const bool pendingEventsRolledBack = rollbackStarted &&
        context->removeEvent(b_EVC_MOVING, rollback) == 0 &&
        context->removeEvent(b_EVC_CHECK_COLLISION, rollback) == 0;

    KR_ObjectID reused =
        g_arena.newObject(subjectTable, "Bullet.Subject.Reuse.Probe");
    BoundedBullet *reusedObject = g_bulletTable.find(reused);
    const bool reusedClean = !reused.isNUL() && reusedObject != NULL &&
                             reusedObject->clean();
    RemoveIfPresent(context, reused);

    return removedAtGround && groundEffectsClean &&
           pendingEventsRolledBack && rollbackEffectsClean && reusedClean &&
           *moveCount == 2 &&
           g_bulletTable.liveCount() == baseline &&
           SmokeSubjectState_LiveCount() == 0 &&
           SparkSubjectState_LiveCount() == 0 &&
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
    const int baselineExplosions = ExplosionSubjectState_LiveCount();
    const int baselineSmokes = SmokeSubjectState_LiveCount();
    KR_ObjectID targetID = target;
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        targetID.isNUL() || !context->isExist(targetID) || baseline != 0 ||
        baselineExplosions != 0 || baselineSmokes != 0 ||
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
    const unsigned int dynamicImpactsBefore =
        g_runtimeTelemetry.dynamicImpacts;
    const int collisionAccepted = bulletObject->receiveEvent(event);
    const bool hit = collisionAccepted == 1 &&
                     !context->isExist(bullet) &&
                     g_runtimeTelemetry.dynamicImpacts ==
                         dynamicImpactsBefore + 1;
    RemoveIfPresent(context, bullet);

    bool impactClean = hit;
    if (context->isExist("Expl.Bullet.Impact"))
    {
        KR_ObjectID impact = context->searchObject("Expl.Bullet.Impact");
        impactClean = !impact.isNUL() &&
            ExplosionSubjectState_RollbackQueued(context, &impact, 1);
    }
    ExplosionImpactRequest damageRequest = {
        targetPosition, ts, g_arena.getObjectID(),
        attribute->m_cacheExplosionTable, attribute->m_cacheExplAttr,
        "Explosion.Bullet.DynamicDamage.Probe"};
    int damageApplications = 0;
    const bool damageGraphPublished =
        attribute->m_cacheExplosionTable != ct_NULLID &&
        attribute->m_cacheExplAttr != ct_NULLID;
    const bool damageExecuted = !damageGraphPublished ||
        (impactClean && ExplosionSubjectState_ExecuteNow(
             context, damageRequest, &damageApplications) &&
         damageApplications == 1);
    if (context->isExist("Explosion.Bullet.DynamicDamage.Probe"))
    {
        const KR_ObjectID damageExplosion =
            context->searchObject("Explosion.Bullet.DynamicDamage.Probe");
        RemoveIfPresent(context, damageExplosion);
    }
    // A traced Explosion branch can publish its first real smoke puff during
    // the immediate damage step.  The Explosion owner deliberately does not
    // own that independently scheduled child, so the bounded probe must roll
    // it back explicitly just like the queued impact/explosion subjects.
    bool smokeClean = true;
    std::vector<KR_ObjectID> spawnedSmokes;
    const ct_ClassTableID smokeTable =
        g_arena.searchSeanceClassTable("Smoke");
    if (smokeTable != ct_NULLID)
        g_arena.userFind(smokeTable, CollectObjectID, &spawnedSmokes);
    for (std::vector<KR_ObjectID>::reverse_iterator smoke =
             spawnedSmokes.rbegin();
         smoke != spawnedSmokes.rend(); ++smoke)
    {
        smokeClean = SmokeSubjectState_RollbackStarted(context, *smoke) &&
                     smokeClean;
    }
    smokeClean = smokeClean &&
                 SmokeSubjectState_LiveCount() == baselineSmokes;

    const bool result = hit && impactClean && damageExecuted && smokeClean &&
           context->removeEvent(b_EVC_MOVING, bullet) == 0 &&
           context->removeEvent(b_EVC_CHECK_COLLISION, bullet) == 0 &&
           g_bulletTable.liveCount() == baseline &&
           ExplosionSubjectState_LiveCount() == baselineExplosions &&
           SmokeSubjectState_LiveCount() == baselineSmokes &&
           !context->isExist("Bullet.Subject.DynamicCollision.Probe") &&
           !context->isExist("Expl.Bullet.Impact") &&
           !context->isExist("Explosion.Bullet.DynamicDamage.Probe");
    if (!result)
        std::fprintf(stderr,
             "Bullet dynamic probe rollback: hit=%i impact=%i damage=%i/%i "
             "smoke=%i live=%i/%i/%i\n",
             hit ? 1 : 0, impactClean ? 1 : 0,
             damageExecuted ? 1 : 0, damageApplications,
             smokeClean ? 1 : 0, g_bulletTable.liveCount(),
             ExplosionSubjectState_LiveCount(),
             SmokeSubjectState_LiveCount());
    return result;
}

bool BulletSubjectState_ProbeImpactEffectLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletEffectProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        ExplosionSubjectState_LiveCount() != 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeBullet *attribute = static_cast<AttributeBullet *>(
        __bulletAttrTable.searchAttribute(attributeID));
    if (attributeID.isNUL() || attribute == NULL ||
        attribute->m_cacheExplosionTable == ct_NULLID ||
        attribute->m_cacheSplashAttr == ct_NULLID ||
        attribute->m_cacheExplAttr == ct_NULLID)
        return false;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const CFVector3 position(1000000.0, 1000000.0, 1000000.0);
    const CFVector3 velocity(10.0, -10.0, 0.0);
    KR_ObjectID children[2] = {
        KR_ObjectID::NUL(), KR_ObjectID::NUL()};
    int childCount = 0;
    const bool splashAndImpact = QueueImpactEffects(
        context, attribute, g_arena.getObjectID(), position, velocity,
        ts, true, 0.25, true, 0.5, children, &childCount);
    if (!splashAndImpact || childCount != 2 ||
        ExplosionSubjectState_LiveCount() != 2 ||
        !ExplosionSubjectState_RollbackQueued(context, children, 2) ||
        ExplosionSubjectState_LiveCount() != 0)
        return false;
    summary->queuedBatches = 1;
    summary->queuedChildren = 2;
    summary->splashFirstCases = 1;
    summary->rolledBackChildren = 2;

    children[0] = KR_ObjectID::NUL();
    children[1] = KR_ObjectID::NUL();
    childCount = 0;
    const bool impactOnly = QueueImpactEffects(
        context, attribute, g_arena.getObjectID(), position, velocity,
        ts + 1.0, false, 0.0, true, 0.5, children, &childCount);
    if (!impactOnly || childCount != 1 ||
        ExplosionSubjectState_LiveCount() != 1 ||
        !ExplosionSubjectState_RollbackQueued(context, children, 1) ||
        ExplosionSubjectState_LiveCount() != 0)
        return false;
    ++summary->queuedBatches;
    ++summary->queuedChildren;
    ++summary->rolledBackChildren;
    return summary->queuedBatches == 2 &&
           summary->queuedChildren == 3 &&
           summary->splashFirstCases == 1 &&
           summary->rolledBackChildren == 3;
}

bool BulletSubjectState_ProbeGroundSparkLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletGroundSparkProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_bulletTable.liveCount() != 0 ||
        SparkSubjectState_LiveCount() != 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeBullet *attribute = static_cast<AttributeBullet *>(
        __bulletAttrTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("BulletAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Bullet");
    const int attributeIndex = attributeTable == ct_NULLID ||
            attributeID.isNUL()
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (attribute == NULL || attributeIndex == -1 ||
        subjectTable == ct_NULLID ||
        attribute->m_cacheSparkTable == ct_NULLID ||
        attribute->m_cacheSparkAttr == ct_NULLID)
        return false;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const KR_ObjectID source = g_arena.getObjectID();
    KR_ObjectID bullet = g_arena.newObject(
        subjectTable, "Bullet.GroundSpark.Probe");
    BoundedBullet *object = g_bulletTable.find(bullet);
    KR_Event event;
    BuildStartEvent(event, bullet, source, ts,
                    CFVector3(0.0, 0.001, 0.0),
                    CFVector3(0.0, -1.0, 0.0),
                    attributeIndex, source);
    const double previousFrameSec = Session::m_frameSec;
    Session::m_frameSec = 0.090001;
    context->sendEventNow(event);
    Session::m_frameSec = previousFrameSec;
    const bool started = !bullet.isNUL() && object != NULL &&
        object->started() &&
        context->removeEvent(b_EVC_MOVING, bullet) != 0;
    if (!started)
    {
        RemoveIfPresent(context, bullet);
        return false;
    }

    event = KR_Event();
    event.label = b_EVC_MOVING;
    event.source = bullet;
    event.destination = bullet;
    event.timeStamp = ts + attribute->m_moveTimeIncrement;
    event.data.open(EDO_WRITE).putDouble(ts).close();
    context->sendEventNow(event);
    KR_ObjectID spark = context->searchObject("S");
    const bool queued = !context->isExist(bullet) && !spark.isNUL() &&
        SparkSubjectState_LiveCount() == 1;
    summary->queuedSparks = queued ? 1 : 0;
    const bool rolledBack = queued &&
        SparkSubjectState_RollbackQueued(context, spark);
    summary->rolledBackSparks = rolledBack ? 1 : 0;
    RemoveIfPresent(context, bullet);
    RemoveIfPresent(context, spark);
    return queued && rolledBack && summary->queuedSparks == 1 &&
           summary->rolledBackSparks == 1 &&
           g_bulletTable.liveCount() == 0 &&
           SparkSubjectState_LiveCount() == 0 &&
           context->removeEvent(b_EVC_MOVING, bullet) == 0 &&
           context->removeEvent(b_EVC_CHECK_COLLISION, bullet) == 0 &&
           context->removeEvent(sp_EV_CREATE, spark) == 0 &&
           context->removeEvent(sp_EVC_LIFE, spark) == 0 &&
           !context->isExist("Bullet.GroundSpark.Probe") &&
           !context->isExist("S");
}

bool BulletSubjectState_ProbeBarrelSmokeLifecycle(
    SimulationContext *context, const char *attributeName,
    double timeStamp, BulletBarrelSmokeProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || attributeName == NULL ||
        attributeName[0] == 0 || g_bulletTable.liveCount() != 0 ||
        SmokeSubjectState_LiveCount() != 0)
        return false;
    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeBullet *attribute = static_cast<AttributeBullet *>(
        __bulletAttrTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("BulletAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Bullet");
    const int attributeIndex = attributeTable == ct_NULLID ||
            attributeID.isNUL()
        ? -1
        : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (attribute == NULL || attributeIndex == -1 ||
        subjectTable == ct_NULLID || attribute->m_useBarellSmoke == 0 ||
        attribute->m_smokeTableID == ct_NULLID ||
        attribute->m_smokeAttrID.isNUL() ||
        !SmokeSubjectState_RenderingSupported(
            context, attribute->m_smokeAttrName))
        return false;

    const double previousFrameSec = Session::m_frameSec;
    const int previousUseBarrelSmoke = attribute->m_useBarellSmoke;
    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const KR_ObjectID source = g_arena.getObjectID();
    const CFVector3 position(0.0, 10.0, 0.0);
    const CFVector3 direction(0.0, 0.0, -1.0);
    KR_Event event;

    Session::m_frameSec = 0.09;
    KR_ObjectID thresholdBullet = g_arena.newObject(
        subjectTable, "Bullet.BarrelSmoke.Threshold.Probe");
    BoundedBullet *thresholdObject = g_bulletTable.find(thresholdBullet);
    BuildStartEvent(event, thresholdBullet, source, ts, position,
                    direction, attributeIndex, source);
    context->sendEventNow(event);
    KR_ObjectID thresholdSmoke = context->searchObject("Smok.");
    const bool thresholdStarted = !thresholdBullet.isNUL() &&
        thresholdObject != NULL && thresholdObject->started() &&
        !thresholdSmoke.isNUL() &&
        SmokeSubjectState_LiveCount() == 1;
    summary->thresholdStarts = thresholdStarted ? 1 : 0;
    RemoveIfPresent(context, thresholdBullet);
    const bool thresholdRolledBack = thresholdStarted &&
        SmokeSubjectState_RollbackStarted(context, thresholdSmoke);
    summary->rolledBackSmokes = thresholdRolledBack ? 1 : 0;

    Session::m_frameSec = 0.090001;
    KR_ObjectID frameGateBullet = g_arena.newObject(
        subjectTable, "Bullet.BarrelSmoke.FrameGate.Probe");
    BoundedBullet *frameGateObject = g_bulletTable.find(frameGateBullet);
    BuildStartEvent(event, frameGateBullet, source, ts + 1.0,
                    position, direction, attributeIndex, source);
    context->sendEventNow(event);
    KR_ObjectID frameGateSmoke = KR_ObjectID::NUL();
    const bool frameGateSkipped = !frameGateBullet.isNUL() &&
        frameGateObject != NULL && frameGateObject->started() &&
        frameGateSmoke.isNUL() &&
        SmokeSubjectState_LiveCount() == 0;
    summary->frameGateSkips = frameGateSkipped ? 1 : 0;
    RemoveIfPresent(context, frameGateBullet);

    Session::m_frameSec = 0.09;
    attribute->m_useBarellSmoke = 0;
    KR_ObjectID attributeGateBullet = g_arena.newObject(
        subjectTable, "Bullet.BarrelSmoke.AttributeGate.Probe");
    BoundedBullet *attributeGateObject =
        g_bulletTable.find(attributeGateBullet);
    BuildStartEvent(event, attributeGateBullet, source, ts + 2.0,
                    position, direction, attributeIndex, source);
    context->sendEventNow(event);
    KR_ObjectID attributeGateSmoke = KR_ObjectID::NUL();
    const bool attributeGateSkipped = !attributeGateBullet.isNUL() &&
        attributeGateObject != NULL && attributeGateObject->started() &&
        attributeGateSmoke.isNUL() &&
        SmokeSubjectState_LiveCount() == 0;
    summary->attributeGateSkips = attributeGateSkipped ? 1 : 0;
    RemoveIfPresent(context, attributeGateBullet);

    attribute->m_useBarellSmoke = previousUseBarrelSmoke;
    Session::m_frameSec = previousFrameSec;
    RemoveIfPresent(context, thresholdBullet);
    RemoveIfPresent(context, thresholdSmoke);
    RemoveIfPresent(context, frameGateBullet);
    RemoveIfPresent(context, frameGateSmoke);
    RemoveIfPresent(context, attributeGateBullet);
    RemoveIfPresent(context, attributeGateSmoke);
    return thresholdStarted && thresholdRolledBack &&
           frameGateSkipped && attributeGateSkipped &&
           summary->thresholdStarts == 1 &&
           summary->frameGateSkips == 1 &&
           summary->attributeGateSkips == 1 &&
           summary->rolledBackSmokes == 1 &&
           g_bulletTable.liveCount() == 0 &&
           SmokeSubjectState_LiveCount() == 0 &&
           !context->isExist("Bullet.BarrelSmoke.Threshold.Probe") &&
           !context->isExist("Bullet.BarrelSmoke.FrameGate.Probe") &&
           !context->isExist("Bullet.BarrelSmoke.AttributeGate.Probe") &&
           !context->isExist("Smok.");
}

bool BulletSubjectState_ProbePresentationLifecycle(
    SimulationContext *context, const char *particleAttributeName,
    const char *skinAttributeName, double timeStamp,
    BulletPresentationProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    const bool hasParticle = particleAttributeName != NULL &&
        particleAttributeName[0] != 0;
    const bool hasSkin = skinAttributeName != NULL && skinAttributeName[0] != 0;
    if (context == NULL || (!hasParticle && !hasSkin) ||
        g_bulletTable.liveCount() != 0 || g_lightChain.m_count != 0 ||
        g_lightChain.m_list != NULL || CViewObject::EnabledLights() != 0)
        return false;

    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("BulletAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Bullet");
    if (attributeTable == ct_NULLID || subjectTable == ct_NULLID)
        return false;
    summary->tableRenders = g_bulletTable.isRendering() ? 1 : 0;

    const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
    const KR_ObjectID source = g_arena.getObjectID();
    const CFVector3 position(1000000.0, 1000000.0, 1000000.0);
    const CFVector3 direction(0.0, 0.0, -1.0);
    const char *names[2] = {particleAttributeName, skinAttributeName};
    const char *owners[2] = {
        "Bullet.Presentation.Particle.Probe",
        "Bullet.Presentation.Skin.Probe"};
    bool valid = summary->tableRenders == 1;
    const BulletRuntimeTelemetry before = g_runtimeTelemetry;
    const std::vector<BulletOwnerRuntimeEntry> ownerTelemetryBefore =
        g_ownerRuntimeTelemetry;
    const unsigned int activeBefore = g_activeBullets;
    const int smokeLiveBefore = SmokeSubjectState_LiveCount();
    const int sparkLiveBefore = SparkSubjectState_LiveCount();
    int expectedSubmissions = 0;
    for (int index = 0; index < 2 && valid; ++index)
    {
        if (names[index] == NULL || names[index][0] == 0)
            continue;
        ++expectedSubmissions;
        KR_ObjectID attributeID = context->searchObject(names[index]);
        AttributeBullet *attribute = static_cast<AttributeBullet *>(
            __bulletAttrTable.searchAttribute(attributeID));
        const int attributeIndex = attributeID.isNUL()
            ? -1
            : g_arena.getAttributeIndex(attributeTable, attributeID);
        const bool expectsSkin = index == 1;
        if (attribute == NULL || attributeIndex < 0 ||
            (!expectsSkin && attribute->m_useSkin != 0) ||
            (expectsSkin && (attribute->m_useSkin == 0 ||
                             attribute->m_cacheSkin == NULL)))
        {
            valid = false;
            break;
        }

        KR_ObjectID bullet =
            g_arena.newObject(subjectTable, owners[index]);
        BoundedBullet *object = g_bulletTable.find(bullet);
        KR_Event event;
        BuildStartEvent(event, bullet, source, ts + index, position,
                        direction, attributeIndex, source);
        const int previousBarrelSmoke = attribute->m_useBarellSmoke;
        attribute->m_useBarellSmoke = 0;
        context->sendEventNow(event);
        attribute->m_useBarellSmoke = previousBarrelSmoke;
        const bool started = !bullet.isNUL() && object != NULL &&
            object->started() &&
            (!expectsSkin || object->skinAttached());
        CViewDynamicList list;
        if (started)
            object->render(list, ts + index +
                attribute->m_moveTimeIncrement * 0.5);
        const bool expectsLight = attribute->m_useLight != 0 &&
            attribute->m_lightColor >= 0 &&
            attribute->m_lightColor < LIGHT_COLOR_COUNT &&
            std::isfinite(attribute->m_lightRadius) &&
            attribute->m_lightRadius > 0.0;
        const bool lightSubmissionValid = expectsLight
            ? g_lightChain.m_count == 1 &&
                g_lightChain.m_list == g_lightChain.m_dim
            : g_lightChain.m_count == 0 && g_lightChain.m_list == NULL;
        const bool submitted = started && list.First() != NULL &&
            object->presentationPublished() && lightSubmissionValid;
        list.Clear(false);
        if (object != NULL)
            object->endRender(NULL);
        CViewObject::EnableLights(0);
        g_lightChain.m_list = NULL;
        g_lightChain.m_count = 0;
        const bool detached = object != NULL &&
            !object->presentationPublished();
        if (!expectsSkin)
            summary->particleSubmissions = submitted ? 1 : 0;
        else
            summary->skinSubmissions = submitted ? 1 : 0;
        summary->detachedSubmissions += detached ? 1 : 0;
        RemoveIfPresent(context, bullet);
        valid = started && submitted && detached &&
            SmokeSubjectState_LiveCount() == smokeLiveBefore &&
            SparkSubjectState_LiveCount() == sparkLiveBefore;
    }

    summary->skippedSkinSubmissions = static_cast<int>(
        g_runtimeTelemetry.skippedSkinRenderSubmissions -
        before.skippedSkinRenderSubmissions);
    const int expectedParticleSubmissions = hasParticle ? 1 : 0;
    const int expectedSkinSubmissions = hasSkin ? 1 : 0;
    const bool result = valid && summary->tableRenders == 1 &&
           summary->particleSubmissions == expectedParticleSubmissions &&
           summary->skinSubmissions == expectedSkinSubmissions &&
           summary->skippedSkinSubmissions == 0 &&
           summary->detachedSubmissions == expectedSubmissions &&
           g_runtimeTelemetry.particleRenderSubmissions ==
               before.particleRenderSubmissions +
                   expectedParticleSubmissions &&
           g_runtimeTelemetry.skinRenderSubmissions ==
               before.skinRenderSubmissions + expectedSkinSubmissions &&
           g_runtimeTelemetry.renderSubmissions ==
               before.renderSubmissions + expectedSubmissions &&
           g_lightChain.m_count == 0 && g_lightChain.m_list == NULL &&
           CViewObject::EnabledLights() == 0 &&
           g_bulletTable.liveCount() == 0 &&
           SmokeSubjectState_LiveCount() == smokeLiveBefore &&
           SparkSubjectState_LiveCount() == sparkLiveBefore &&
           !context->isExist(owners[0]) &&
           !context->isExist(owners[1]);
    g_runtimeTelemetry = before;
    g_ownerRuntimeTelemetry = ownerTelemetryBefore;
    g_activeBullets = activeBefore;
    return result;
}

void BulletActiveWorldState_Link()
{
    BulletSubjectState_Link();
}

const char *BulletActiveWorldState_LastFailure()
{
    return g_activeWorldFailure.c_str();
}

int BulletActiveWorldState_SchedulerEventCount(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableBulletRecord> records;
    return DecodeStableRecords(bytes, &records)
               ? static_cast<int>(records.size() * 2) : -1;
}

unsigned long long BulletActiveWorldState_Fingerprint(
    SimulationContext *context)
{
    std::vector<unsigned char> bytes;
    if (!BulletActiveWorldState_CaptureStable(context, &bytes))
        return 0;
    unsigned long long hash = kHashOffset;
    if (!bytes.empty())
        HashBytes(hash, &bytes[0], static_cast<int>(bytes.size()));
    return hash;
}

bool BulletActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes)
{
    g_activeWorldFailure.clear();
    std::vector<StableBulletRecord> records;
    if (!CollectStableRecords(context, &records))
    {
        if (g_activeWorldFailure.empty())
            FailActiveWorld("Bullet stable roster collection failed");
        return false;
    }
    return EncodeStableRecords(records, bytes);
}

bool BulletActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes)
{
    std::vector<StableBulletRecord> records;
    return DecodeStableRecords(bytes, &records);
}

bool BulletActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<unsigned char> current;
    return BulletActiveWorldState_ValidateStable(bytes) &&
           BulletActiveWorldState_CaptureStable(context, &current) &&
           current == bytes;
}

bool BulletActiveWorldState_CollectStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *owners)
{
    std::vector<StableBulletRecord> records;
    std::vector<BoundedBullet *> objects;
    if (owners == NULL || !owners->empty() ||
        !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &objects) ||
        !RosterMatches(objects, context, records))
        return false;
    for (std::size_t index = 0; index < objects.size(); ++index)
        owners->push_back(objects[index]->getObjectID());
    return true;
}

bool BulletActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created)
{
    std::vector<StableBulletRecord> records;
    std::vector<BoundedBullet *> active;
    if (context == NULL || created == NULL || !created->empty() ||
        !DecodeStableRecords(bytes, &records) ||
        !CollectStableRoster(context, &active))
        return false;
    if (!active.empty())
        return AllOwnersStarted(active) &&
            RosterMatches(active, context, records);

    const ct_ClassTableID table =
        g_arena.searchSeanceClassTable("Bullet");
    if (!records.empty() && table == ct_NULLID)
        return FailActiveWorld("Bullet owner table has insufficient capacity");

    std::vector<BoundedBullet *> objects;
    std::vector<KR_ObjectID> allocated;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        std::vector<StableBulletRecord> prefix(
            records.begin(), records.begin() + index + 1);
        objects.clear();
        if (!SelectIdleRestoreRoster(context, prefix, &objects))
        {
            if (g_bulletTable.liveCount() >= g_bulletTable.capacity())
            {
                BulletActiveWorldState_RemoveStableOwners(context, &allocated);
                return FailActiveWorld(
                    "Bullet owner table has insufficient capacity");
            }
            KR_ObjectID object =
                g_arena.newObject(table, records[index].name.c_str());
            if (object.isNUL() || g_bulletTable.find(object) == NULL)
            {
                BulletActiveWorldState_RemoveStableOwners(context, &allocated);
                return FailActiveWorld("Bullet owner allocation failed");
            }
            allocated.push_back(object);
        }
    }
    objects.clear();
    if (!SelectIdleRestoreRoster(context, records, &objects) ||
        !RosterMatches(objects, context, records))
    {
        BulletActiveWorldState_RemoveStableOwners(context, &allocated);
        return FailActiveWorld("Bullet allocated roster is not canonical");
    }
    for (std::size_t index = 0; index < objects.size(); ++index)
        created->push_back(objects[index]->getObjectID());
    return true;
}

bool BulletActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes)
{
    std::vector<StableBulletRecord> records;
    std::vector<BoundedBullet *> objects;
    if (context == NULL || !DecodeStableRecords(bytes, &records))
        return false;
    if (!CollectStableRoster(context, &objects))
        return false;
    if (objects.empty())
    {
        if (!SelectIdleRestoreRoster(context, records, &objects))
            return FailActiveWorld(
                "BUL1 restore roster is unavailable");
    }
    else if (!AllOwnersStarted(objects))
        return FailActiveWorld(
            "BUL1 restore roster contains a pending noncanonical owner");
    if (!RosterMatches(objects, context, records))
        return FailActiveWorld("BUL1 restore roster differs");
    std::vector<AttributeBullet *> attributes(records.size(), NULL);
    std::vector<KR_ObjectID> masters(records.size(), KR_ObjectID::NUL());
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (!context->isExist(records[index].attribute.c_str()))
            return FailActiveWorld("BUL1 BulletAttr dependency is missing");
        const KR_ObjectID attribute =
            context->searchObject(records[index].attribute.c_str());
        attributes[index] = static_cast<AttributeBullet *>(
            __bulletAttrTable.searchAttribute(attribute));
        if (attributes[index] == NULL)
            return FailActiveWorld("BUL1 BulletAttr dependency has wrong type");
        if (!records[index].master.empty())
        {
            if (!context->isExist(records[index].master.c_str()))
                return FailActiveWorld("BUL1 master dependency is missing");
            masters[index] =
                context->searchObject(records[index].master.c_str());
            if (ObjectName(context, masters[index]) != records[index].master)
                return FailActiveWorld("BUL1 master dependency is ambiguous");
        }
    }
    for (std::size_t index = 0; index < records.size(); ++index)
        if (!objects[index]->applyStable(records[index], attributes[index],
                                         masters[index]))
            return FailActiveWorld("BUL1 runtime state application failed");
    std::vector<unsigned char> current;
    if (!BulletActiveWorldState_CaptureStable(context, &current) ||
        current != bytes)
        return FailActiveWorld("BUL1 canonical recapture differs");
    return true;
}

void BulletActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created)
{
    if (created == NULL)
        return;
    if (context != NULL)
        for (std::vector<KR_ObjectID>::reverse_iterator object =
                 created->rbegin(); object != created->rend(); ++object)
        {
            DrainPrivateBulletEvents(context, *object);
            if (context->isExist(*object))
                context->removeObject(*object);
        }
    created->clear();
}

bool BulletActiveWorldState_ProbeFlightRoundTrip(
    SimulationContext *context, const char *attributeName,
    const KR_ObjectID &master, double timeStamp,
    BulletActiveWorldProbeSummary *summary)
{
    if (summary == NULL)
        return false;
    std::memset(summary, 0, sizeof(*summary));
    g_activeWorldFailure.clear();
    KR_ObjectID mutableMaster = master;
    if (context == NULL || attributeName == NULL || attributeName[0] == 0 ||
        mutableMaster.isNUL() || !context->isExist(master) ||
        ObjectName(context, master).empty() ||
        context->searchObject(ObjectName(context, master).c_str()) != master ||
        g_bulletTable.liveCount() != 0)
        return FailActiveWorld(
            "Bullet active-world probe requires one unique live master and "
            "an empty Bullet pool");

    KR_ObjectID attributeID = context->searchObject(attributeName);
    AttributeBullet *attribute = static_cast<AttributeBullet *>(
        __bulletAttrTable.searchAttribute(attributeID));
    const ct_ClassTableID attributeTable =
        g_arena.searchSeanceClassTable("BulletAttr");
    const ct_ClassTableID subjectTable =
        g_arena.searchSeanceClassTable("Bullet");
    const int attributeIndex = attributeTable == ct_NULLID ||
            attributeID.isNUL()
        ? -1 : g_arena.getAttributeIndex(attributeTable, attributeID);
    if (attribute == NULL || attributeIndex < 0 ||
        subjectTable == ct_NULLID)
        return FailActiveWorld(
            "Bullet active-world probe attribute/table is unavailable");

    const BulletRuntimeTelemetry telemetryBefore = g_runtimeTelemetry;
    const unsigned int activeBefore = g_activeBullets;
    const std::vector<BulletOwnerRuntimeEntry> ownerTelemetryBefore =
        g_ownerRuntimeTelemetry;
    const double frameSecBefore = Session::m_frameSec;
    KR_ObjectID idle = KR_ObjectID::NUL();
    KR_ObjectID original = KR_ObjectID::NUL();
    KR_ObjectID stagedID = KR_ObjectID::NUL();
    KR_ObjectID restoredID = KR_ObjectID::NUL();
    std::vector<KR_ObjectID> originalOwners;
    std::vector<KR_ObjectID> staged;
    std::vector<KR_ObjectID> restored;
    bool success = false;

    do
    {
        // Retail effects may leave a context-backed, completely reset Bullet
        // subject in the fixed class table.  It is capacity, not active-world
        // state: BUL1 must ignore it during capture and may reuse it when the
        // saved flight is reconstructed.
        idle = g_arena.newObject(subjectTable, "B");
        BoundedBullet *idleBullet = g_bulletTable.find(idle);
        if (idle.isNUL() || idleBullet == NULL || !idleBullet->clean())
        {
            FailActiveWorld(
                "Bullet active-world probe idle-slot allocation failed");
            break;
        }
        original = g_arena.newObject(subjectTable, "B");
        BoundedBullet *bullet = g_bulletTable.find(original);
        if (original.isNUL() || bullet == NULL)
        {
            FailActiveWorld("Bullet active-world probe allocation failed");
            break;
        }
        CFVector3 position(10.0, 20.0, -5.0);
        IDynamicObject *dynamic = static_cast<IDynamicObject *>(
            context->queryInterface(master, IDynamicObjectIID));
        if (dynamic != NULL && FiniteVector(dynamic->getPos()))
            position = dynamic->getPos() + CFVector3(0.0, 10.0, 0.0);
        const double ts = timeStamp < 0.1 ? 0.1 : timeStamp;
        // Use a generation-mismatched ID in the live master's former slot.
        // It models a projectile whose shooter was removed before the save.
        const KR_ObjectID staleMaster(master.id ^ 0x40000000L,
                                      master.getCachePos());
        KR_Event start;
        Session::m_frameSec = 0.090001;
        if (!BuildStartEvent(start, original, master, ts, position,
                             CFVector3(0.0, 1.0, 0.0), attributeIndex,
                             staleMaster))
        {
            FailActiveWorld("Bullet active-world probe start encoding failed");
            break;
        }
        context->sendEventNow(start);
        Session::m_frameSec = frameSecBefore;
        if (!bullet->started())
        {
            FailActiveWorld("Bullet active-world probe start was rejected");
            break;
        }

        std::vector<unsigned char> bytes;
        if (!BulletActiveWorldState_CaptureStable(context, &bytes) ||
            BulletActiveWorldState_SchedulerEventCount(bytes) != 2 ||
            !BulletActiveWorldState_CollectStableOwners(
                context, bytes, &originalOwners) ||
            originalOwners.size() != 1 || originalOwners[0] != original)
        {
            FailActiveWorld("BUL1 live flight capture failed");
            break;
        }
        const KR_ObjectID first = originalOwners[0];
        unsigned long long fingerprint = kHashOffset;
        HashBytes(fingerprint, &bytes[0], static_cast<int>(bytes.size()));
        BulletActiveWorldState_RemoveStableOwners(context, &originalOwners);
        if (g_bulletTable.liveCount() != 1 || !context->isExist(idle) ||
            g_bulletTable.find(idle) == NULL ||
            !g_bulletTable.find(idle)->clean())
        {
            FailActiveWorld(
                "BUL1 original teardown did not retain one idle slot");
            break;
        }

        if (!BulletActiveWorldState_CreateStableOwners(
                context, bytes, &staged) || staged.size() != 1 ||
            staged[0] == first || staged[0] != idle ||
            !BulletActiveWorldState_ApplyStableReferences(context, bytes) ||
            !BulletActiveWorldState_MatchesStable(context, bytes))
        {
            FailActiveWorld("BUL1 staged reconstruction failed");
            break;
        }
        stagedID = staged[0];
        BulletActiveWorldState_RemoveStableOwners(context, &staged);
        if (g_bulletTable.liveCount() != 0)
        {
            FailActiveWorld("BUL1 staged rollback retained an owner");
            break;
        }

        if (!BulletActiveWorldState_CreateStableOwners(
                context, bytes, &restored) || restored.size() != 1 ||
            restored[0] == first || restored[0] == stagedID ||
            !BulletActiveWorldState_ApplyStableReferences(context, bytes) ||
            !BulletActiveWorldState_MatchesStable(context, bytes))
        {
            FailActiveWorld("BUL1 final reconstruction failed");
            break;
        }
        restoredID = restored[0];
        BoundedBullet *resumed = g_bulletTable.find(restored[0]);
        KR_Event pendingMove[2];
        const int pendingMoveCount = context->copyEvents(
            b_EVC_MOVING, restored[0], pendingMove, 2);
        const CFVector3 positionBefore = resumed == NULL
            ? CFVector3(0.0, 0.0, 0.0) : resumed->getPosition();
        if (resumed == NULL || pendingMoveCount != 1 ||
            context->removeEvent(b_EVC_MOVING, restored[0]) != 1)
        {
            FailActiveWorld("BUL1 restored moving event is unavailable");
            break;
        }
        context->sendEventNow(pendingMove[0]);
        KR_Event nextMove[2];
        const CFVector3 positionAfter = resumed->getPosition();
        if (!context->isExist(restored[0]) || resumed->moveCount() != 1 ||
            (positionAfter.x == positionBefore.x &&
             positionAfter.y == positionBefore.y &&
             positionAfter.z == positionBefore.z) ||
            context->copyEvents(b_EVC_MOVING, restored[0], nextMove, 2) != 1)
        {
            FailActiveWorld("BUL1 restored flight did not resume movement");
            break;
        }
        summary->capturedOwners = 1;
        summary->schedulerEvents = 2;
        summary->stagedRollbacks = 1;
        summary->reconstructedOwners = 1;
        summary->stableRoundTrips = 2;
        summary->resumedMoves = 1;
        summary->tombstonedMasters = 1;
        summary->fingerprint = fingerprint;
        success = true;
    } while (false);

    Session::m_frameSec = frameSecBefore;
    BulletActiveWorldState_RemoveStableOwners(context, &restored);
    BulletActiveWorldState_RemoveStableOwners(context, &staged);
    BulletActiveWorldState_RemoveStableOwners(context, &originalOwners);
    RemoveIfPresent(context, original);
    RemoveIfPresent(context, idle);
    const int lateEvents =
        DrainPrivateBulletEvents(context, original) +
        DrainPrivateBulletEvents(context, stagedID) +
        DrainPrivateBulletEvents(context, restoredID);
    const bool clean = g_bulletTable.liveCount() == 0 && lateEvents == 0;
    g_runtimeTelemetry = telemetryBefore;
    g_activeBullets = activeBefore;
    g_ownerRuntimeTelemetry = ownerTelemetryBefore;
    if (!success || !clean)
    {
        std::memset(summary, 0, sizeof(*summary));
        if (g_activeWorldFailure.empty())
            FailActiveWorld("BUL1 probe rollback was not clean");
        return false;
    }
    return true;
}
