#include "TeleportSubjectState.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

class CGRPanel;
#include "h/vehicle.h"
#include "h/phisics.h"
#include "bumpdef.h"
#include "i/dynobj.i"
#include "kernel/h/context.h"
#include "message/unitmsg.h"
#include "storage/h/savefile.h"
#include "storage/h/strgdefs.h"
#include "storage/h/subject.h"

namespace {

const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;
const int kMaximumTeleportCapacity = 64;

char g_lastError[192] = {};

void SetError(const char *message)
{
    std::snprintf(g_lastError, sizeof(g_lastError), "%s",
                  message == NULL ? "unknown Teleport failure" : message);
}

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool NearlyEqual(double left, double right)
{
    const double scale = 1.0 + std::fabs(left) + std::fabs(right);
    return std::fabs(left - right) <= 1.0e-9 * scale;
}

bool NearlyEqual(const CFVector3 &left, const CFVector3 &right)
{
    return NearlyEqual(left.x, right.x) && NearlyEqual(left.y, right.y) &&
           NearlyEqual(left.z, right.z);
}

bool NearlyEqual(const CFMatrix3x4 &left, const CFMatrix3x4 &right)
{
    return NearlyEqual(left.Row(0), right.Row(0)) &&
           NearlyEqual(left.Row(1), right.Row(1)) &&
           NearlyEqual(left.Row(2), right.Row(2)) &&
           NearlyEqual(left.Offset(), right.Offset());
}

void HashBytes(unsigned long long &hash, const void *bytes,
               unsigned int count)
{
    const unsigned char *data = static_cast<const unsigned char *>(bytes);
    for (unsigned int index = 0; index < count; ++index)
    {
        hash ^= data[index];
        hash *= kHashPrime;
    }
}

class Teleport : public ct_Subject, public IDynamicObject
{
 public:
    Teleport()
        : m_source(0.0, 0.0, 0.0), m_destination(0.0, 0.0, 0.0),
          m_radius(0.0), m_configured(false), m_rejectedCollisions(0),
          m_appliedCollisions(0)
    {
        m_direction.LoadIdentity();
    }

    int receiveEvent(KR_Event &event) override
    {
        if (event.label == KR_SET_ATTR)
        {
            if (event.data.remaining() != 7 * static_cast<int>(sizeof(double)))
                return 0;
            TeleportDefinition definition;
            event.data.open(EDO_READ)
                .getDouble(definition.source.x)
                .getDouble(definition.source.y)
                .getDouble(definition.source.z)
                .getDouble(definition.radius)
                .getDouble(definition.destination.x)
                .getDouble(definition.destination.y)
                .getDouble(definition.destination.z)
                .close();
            if (!validDefinition(definition)) return 0;
            m_source = definition.source;
            m_destination = definition.destination;
            m_radius = definition.radius;
            m_direction.LoadIdentity().TranslateL(m_source);
            setPosition(m_source);
            m_configured = true;
            return 1;
        }

        if (event.label == t_EV_ONCOLLISION)
        {
            if (!m_configured ||
                event.data.remaining() !=
                    static_cast<int>(sizeof(KR_ObjectID)))
                return 0;
            KR_ObjectID collided;
            event.data.open(EDO_READ).getObjectID(collided).close();
            if (g_vehicle == NULL || g_vehicle->getObjectID() != collided)
            {
                ++m_rejectedCollisions;
                return 1;
            }

            // The May 1999 executable calls the complete Vehicle position
            // setter here.  The recovered Vehicle keeps its vessel and
            // ct_Subject cache positions separately, so both owners must be
            // moved at this one intentional discontinuity.
            g_vehicle->SetPos(m_destination);
            g_vehicle->setPosition(m_destination);
            ++m_appliedCollisions;
            return 1;
        }
        return 0;
    }

    void addNotify() override
    {
        ct_Subject::addNotify();
        reset();
    }

    void removeNotify() override
    {
        ct_Subject::removeNotify();
        reset();
    }

    void *queryInterface(int iid) override
    {
        if (iid == IUnknownIID) return static_cast<KR_Object *>(this);
        if (iid == IDynamicObjectIID)
            return static_cast<IDynamicObject *>(this);
        return NULL;
    }

    bool shouldDump() override { return true; }

    bool dump(PIN_SaveFile &save) override
    {
        if (!ct_Subject::dump(save)) return false;
        const double values[7] = {
            m_source.x, m_source.y, m_source.z, m_radius,
            m_destination.x, m_destination.y, m_destination.z};
        return save.WriteData(
            const_cast<char *>(reinterpret_cast<const char *>(values)),
            static_cast<int>(sizeof(values)));
    }

    bool load(PIN_SaveFile &save) override
    {
        if (!ct_Subject::load(save)) return false;
        double values[7] = {};
        if (!save.GetData(reinterpret_cast<char *>(values),
                          static_cast<int>(sizeof(values))))
            return false;
        TeleportDefinition definition;
        definition.source = CFVector3(values[0], values[1], values[2]);
        definition.radius = values[3];
        definition.destination = CFVector3(values[4], values[5], values[6]);
        if (!validDefinition(definition)) return false;
        m_source = definition.source;
        m_radius = definition.radius;
        m_destination = definition.destination;
        m_configured = true;
        return true;
    }

    void loadNotify() override
    {
        ct_Subject::loadNotify();
        m_direction.LoadIdentity().TranslateL(m_source);
        setPosition(m_source);
    }

    CFVector3 realPosition() override { return m_source; }
    CFVector3 getPos() override { return m_source; }
    double getHAngle() override { return 0.0; }
    CFVector3 getUpVector() override { return CFVector3(0.0, 1.0, 0.0); }
    CFVector3 getCenter() override { return CFVector3(0.0, 0.0, 0.0); }
    double getRadius() override { return m_radius; }
    double getRadius0() override { return m_radius; }
    CFVector3 getMoveDir() override { return CFVector3(1.0, 0.0, 0.0); }
    double getMoveSpeed() override { return 0.0; }
    void getMatrix(CFMatrix3x4 &matrix) override { matrix = m_direction; }
    double getMass() override { return 1.0; }
    TCCFMatrix3x4 &GetDir() override { return m_direction; }
    void SetDir(TCSFMatrix3x4 &direction) override
    {
        m_direction = direction;
        m_source = direction.Offset();
        setPosition(m_source);
    }

    bool configured() const { return m_configured; }
    const CFVector3 &source() const { return m_source; }
    const CFVector3 &destination() const { return m_destination; }
    double radius() const { return m_radius; }
    int rejectedCollisions() const { return m_rejectedCollisions; }
    int appliedCollisions() const { return m_appliedCollisions; }
    void setCollisionCounts(int rejected, int applied)
    {
        m_rejectedCollisions = rejected;
        m_appliedCollisions = applied;
    }

 private:
    static bool validDefinition(const TeleportDefinition &definition)
    {
        return FiniteVector(definition.source) &&
               FiniteVector(definition.destination) &&
               std::isfinite(definition.radius) && definition.radius > 0.0 &&
               definition.radius <= 1024.0;
    }

    void reset()
    {
        m_source = CFVector3(0.0, 0.0, 0.0);
        m_destination = CFVector3(0.0, 0.0, 0.0);
        m_radius = 0.0;
        m_configured = false;
        m_rejectedCollisions = 0;
        m_appliedCollisions = 0;
        m_direction.LoadIdentity();
    }

    CFVector3 m_source;
    CFVector3 m_destination;
    double m_radius;
    bool m_configured;
    int m_rejectedCollisions;
    int m_appliedCollisions;
    CFMatrix3x4 m_direction;
};

class TeleportTable : public ct_SubjectTable
{
 private:
    Teleport *m_table;

 public:
    TeleportTable() : m_table(NULL) { registerClass("Teleport"); }
    ~TeleportTable()
    {
        delete [] m_table;
        m_table = NULL;
    }

    void allocObjects(int count) override
    {
        m_table = count <= 0 ? NULL : new (std::nothrow) Teleport[count];
        if (m_table == NULL) m_maxObjectQnty = 0;
    }

    void freeObjects() override
    {
        delete [] m_table;
        m_table = NULL;
        m_maxObjectQnty = 0;
    }

    ct_Object *getObjectPTR(int index) override
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "TeleportTable::getObjectPTR");
        return &m_table[index];
    }

    bool isRendering() override { return false; }
    bool isAudible() override { return false; }
    int capacity() const { return m_maxObjectQnty; }

    int liveCount() const
    {
        int count = 0;
        for (ct_Subject *subject = findFirstSubject(); subject != NULL;
             subject = findNextSubject(subject))
            ++count;
        return count;
    }

    Teleport *first() const
    {
        return static_cast<Teleport *>(findFirstSubject());
    }
};

TeleportTable g_teleportTable;

bool RestoreVehiclePose(const CFVector3 &position,
                        const CFVector3 &subjectPosition,
                        const CFMatrix3x4 &direction)
{
    if (g_vehicle == NULL) return false;
    g_vehicle->SetDir(direction);
    g_vehicle->SetPos(position);
    g_vehicle->setPosition(subjectPosition);
    return NearlyEqual(g_vehicle->Pos(), position) &&
           NearlyEqual(g_vehicle->getPosition(), subjectPosition) &&
           NearlyEqual(g_vehicle->GetDir(), direction);
}

}  // namespace

void TeleportSubjectState_Link()
{
}

bool TeleportSubjectState_Initialize(
    SimulationContext *context, int capacity,
    const std::vector<TeleportDefinition> &definitions,
    double timeStamp)
{
    g_lastError[0] = 0;
    if (context == NULL || g_arena.getContext() != context ||
        !std::isfinite(timeStamp) || timeStamp < 0.0 || capacity <= 0 ||
        capacity > kMaximumTeleportCapacity || definitions.empty() ||
        definitions.size() > static_cast<std::size_t>(capacity) ||
        g_arena.searchSeanceClassTable("Teleport") != ct_NULLID)
    {
        SetError("invalid Teleport table request");
        return false;
    }
    const ct_ClassTableID table =
        g_arena.addClassTable("Teleport", capacity);
    if (table == ct_NULLID ||
        table != g_teleportTable.getClassTableID() ||
        g_teleportTable.capacity() != capacity)
    {
        SetError("Teleport table allocation failed");
        return false;
    }

    for (std::size_t index = 0; index < definitions.size(); ++index)
    {
        const TeleportDefinition &definition = definitions[index];
        if (!FiniteVector(definition.source) ||
            !FiniteVector(definition.destination) ||
            !std::isfinite(definition.radius) || definition.radius <= 0.0 ||
            definition.radius > 1024.0)
        {
            SetError("Teleport script contains an invalid route");
            return false;
        }
        KR_ObjectID object = g_arena.newObject(table, "Teleport");
        if (object.isNUL())
        {
            SetError("Teleport object allocation failed");
            return false;
        }
        KR_Event event(KR_SET_ATTR, timeStamp, g_arena.getObjectID(), object);
        event.data.open(EDO_WRITE)
            .putDouble(definition.source.x)
            .putDouble(definition.source.y)
            .putDouble(definition.source.z)
            .putDouble(definition.radius)
            .putDouble(definition.destination.x)
            .putDouble(definition.destination.y)
            .putDouble(definition.destination.z)
            .close();
        context->sendEventNow(event);
        if (event.label != KR_SET_ATTR)
        {
            SetError("Teleport route event was rejected");
            return false;
        }
    }

    if (g_teleportTable.liveCount() !=
        static_cast<int>(definitions.size()))
    {
        SetError("Teleport live roster is incomplete");
        return false;
    }
    for (ct_Subject *subject = g_teleportTable.findFirstSubject();
         subject != NULL; subject = g_teleportTable.findNextSubject(subject))
        if (!static_cast<Teleport *>(subject)->configured())
        {
            SetError("Teleport route did not become collision-ready");
            return false;
        }
    return true;
}

bool TeleportSubjectState_TableReady(SimulationContext *context,
                                     int expectedCapacity)
{
    return context != NULL && g_arena.getContext() == context &&
           expectedCapacity > 0 &&
           g_arena.searchSeanceClassTable("Teleport") ==
               g_teleportTable.getClassTableID() &&
           g_teleportTable.capacity() == expectedCapacity &&
           g_teleportTable.liveCount() > 0;
}

int TeleportSubjectState_Capacity()
{
    return g_teleportTable.capacity();
}

int TeleportSubjectState_LiveCount()
{
    return g_teleportTable.liveCount();
}

unsigned long long TeleportSubjectState_Fingerprint(
    SimulationContext *context)
{
    if (context == NULL || g_arena.getContext() != context ||
        g_teleportTable.liveCount() <= 0)
        return 0;
    unsigned long long hash = kHashOffset;
    const int capacity = g_teleportTable.capacity();
    HashBytes(hash, &capacity, sizeof(capacity));
    for (ct_Subject *subject = g_teleportTable.findFirstSubject();
         subject != NULL; subject = g_teleportTable.findNextSubject(subject))
    {
        const Teleport *teleport = static_cast<const Teleport *>(subject);
        const CFVector3 source = teleport->source();
        const CFVector3 destination = teleport->destination();
        const double radius = teleport->radius();
        HashBytes(hash, &source.x, sizeof(source.x));
        HashBytes(hash, &source.y, sizeof(source.y));
        HashBytes(hash, &source.z, sizeof(source.z));
        HashBytes(hash, &radius, sizeof(radius));
        HashBytes(hash, &destination.x, sizeof(destination.x));
        HashBytes(hash, &destination.y, sizeof(destination.y));
        HashBytes(hash, &destination.z, sizeof(destination.z));
    }
    return hash;
}

bool TeleportSubjectState_ProbeLifecycle(
    SimulationContext *context, double timeStamp,
    TeleportLifecycleProbeSummary *summary)
{
    if (summary == NULL) return false;
    std::memset(summary, 0, sizeof(*summary));
    Teleport *teleport = g_teleportTable.first();
    if (context == NULL || g_arena.getContext() != context ||
        teleport == NULL || !teleport->configured() || g_vehicle == NULL ||
        g_vehicle->getContext() != context ||
        !std::isfinite(timeStamp) || timeStamp < 0.0)
        return false;

    const CFVector3 originalPosition = g_vehicle->Pos();
    const CFVector3 originalSubjectPosition = g_vehicle->getPosition();
    const CFVector3 originalSpeed = g_vehicle->Speed();
    const CFMatrix3x4 originalDirection = g_vehicle->GetDir();
    const int originalRejected = teleport->rejectedCollisions();
    const int originalApplied = teleport->appliedCollisions();
    bool success = FiniteVector(originalPosition) &&
                   FiniteVector(originalSubjectPosition) &&
                   FiniteVector(originalSpeed);

    if (success)
    {
        KR_Event rejected(t_EV_ONCOLLISION, timeStamp,
                          g_arena.getObjectID(), teleport->getObjectID());
        rejected.data.open(EDO_WRITE)
            .putObjectID(teleport->getObjectID())
            .close();
        context->sendEventNow(rejected);
        success = teleport->rejectedCollisions() == originalRejected + 1 &&
                  teleport->appliedCollisions() == originalApplied &&
                  NearlyEqual(g_vehicle->Pos(), originalPosition) &&
                  NearlyEqual(g_vehicle->getPosition(),
                              originalSubjectPosition);
        if (success) summary->rejectedNonPlayerCollisions = 1;
    }

    if (success)
    {
        const double vehicleRadius = g_vehicle->getRadius();
        SBumpDef collision;
        collision.start = teleport->source() -
            CFVector3(teleport->radius() + vehicleRadius + 2.0, 0.0, 0.0);
        collision.vel = CFVector3(
            2.0 * (teleport->radius() + vehicleRadius + 2.0), 0.0, 0.0);
        collision.fRadius = vehicleRadius;
        collision.fTime = 1.0;
        collision.nBumpFlags = 0;
        collision.fMass = 1.0;
        KR_ObjectID hit;
        CFVector3 hitPosition;
        CFVector3 hitSpeed;
        double hitRadius = 0.0;
        const int collided = checkDynamicCollision(
            collision, g_vehicle->getObjectID(), hit, hitPosition,
            hitRadius, hitSpeed, timeStamp);
        KR_Event queued[2];
        const int queuedCount = context->copyEventsTo(
            t_EV_ONCOLLISION, teleport->getObjectID(), queued, 2);
        KR_ObjectID queuedMaster = KR_ObjectID::NUL();
        if (queuedCount == 1)
            queued[0].data.open(EDO_READ).getObjectID(queuedMaster).close();
        const int removed = context->removeEventsTo(
            t_EV_ONCOLLISION, teleport->getObjectID());
        success = collided != 0 && queuedCount == 1 && removed == 1 &&
                  queuedMaster == g_vehicle->getObjectID();
        if (success)
        {
            queued[0].data.open(EDO_READ);
            context->sendEventNow(queued[0]);
            summary->physicsCollisionEvents = 1;
        }
        success = success &&
                  teleport->appliedCollisions() == originalApplied + 1 &&
                  NearlyEqual(g_vehicle->Pos(), teleport->destination()) &&
                  NearlyEqual(g_vehicle->getPosition(),
                              teleport->destination()) &&
                  NearlyEqual(g_vehicle->Speed(), originalSpeed) &&
                  NearlyEqual(g_vehicle->GetDir(), originalDirection);
        if (success) summary->appliedPlayerCollisions = 1;
    }

    const bool restored = RestoreVehiclePose(
        originalPosition, originalSubjectPosition, originalDirection) &&
        NearlyEqual(g_vehicle->Speed(), originalSpeed);
    teleport->setCollisionCounts(originalRejected, originalApplied);
    if (restored) summary->vehiclePoseRollbacks = 1;
    if (!success || !restored)
        SetError("Teleport collision probe did not roll back cleanly");
    return success && restored &&
           summary->rejectedNonPlayerCollisions == 1 &&
           summary->physicsCollisionEvents == 1 &&
           summary->appliedPlayerCollisions == 1 &&
           summary->vehiclePoseRollbacks == 1;
}

const char *TeleportSubjectState_LastError()
{
    return g_lastError;
}
