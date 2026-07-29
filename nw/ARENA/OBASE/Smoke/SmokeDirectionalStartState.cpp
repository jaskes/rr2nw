#include "SmokeSubjectState.h"

#include <cmath>
#include <cstring>

#define LAST_H__VIEW
#include "game.h"

#include "kernel/h/context.h"
#include "message/fountmsg.h"
#include "storage/h/subject.h"

namespace {

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool IsNul(const KR_ObjectID &value)
{
    KR_ObjectID copy = value;
    return copy.isNUL() != 0;
}

}  // namespace

bool SmokeSubjectState_StartWithDirection(
    SimulationContext *context,
    const SmokeDirectionalStartRequest &request,
    KR_ObjectID *child)
{
    if (child == NULL)
        return false;
    *child = KR_ObjectID::NUL();
    const double directionLength2 = Abs2(request.direction);
    const ct_ClassTableID table = context == NULL
        ? ct_NULLID
        : g_arena.searchSeanceClassTable("Smoke");
    if (context == NULL || g_arena.getContext() != context ||
        request.subjectTable == ct_NULLID ||
        request.subjectTable != table ||
        request.objectName == NULL || request.objectName[0] == 0 ||
        std::strlen(request.objectName) >= MAX_SYMBOLIC_LENGHT ||
        request.attributeName == NULL || request.attributeName[0] == 0 ||
        IsNul(request.source) || !context->isExist(request.source) ||
        IsNul(request.attribute) ||
        context->searchObject(request.attributeName) != request.attribute ||
        !FiniteVector(request.position) || !FiniteVector(request.direction) ||
        !std::isfinite(directionLength2) || directionLength2 <= 0.0 ||
        !std::isfinite(request.timeStamp) || request.timeStamp < 0.1 ||
        !SmokeSubjectState_SimulationSupported(
            context, request.attributeName))
        return false;

    const int liveBefore = SmokeSubjectState_LiveCount();
    const int capacity = SmokeSubjectState_Capacity();
    if (liveBefore < 0 || capacity <= 0 || liveBefore >= capacity)
        return false;
    *child = g_arena.newObject(request.subjectTable,
                               request.objectName);
    if (IsNul(*child) || !context->isExist(*child))
    {
        *child = KR_ObjectID::NUL();
        return false;
    }

    KR_Event event;
    event.label = fou_EVCMD_START_WITHDIR;
    event.source = request.source;
    event.destination = *child;
    event.timeStamp = request.timeStamp;
    event.data.open(EDO_WRITE)
              .putObjectID(request.attribute)
              .putDouble(request.position.x)
              .putDouble(request.position.y)
              .putDouble(request.position.z)
              .putDouble(request.direction.x)
              .putDouble(request.direction.y)
              .putDouble(request.direction.z)
              .close();
    context->sendEventNow(event);

    if (!context->isExist(*child) ||
        SmokeSubjectState_LiveCount() != liveBefore + 1)
    {
        if (context->isExist(*child))
            context->removeObject(*child);
        *child = KR_ObjectID::NUL();
        return false;
    }
    return true;
}

bool SmokeSubjectState_RollbackStarted(
    SimulationContext *context, const KR_ObjectID &child)
{
    if (context == NULL || IsNul(child) || !context->isExist(child))
        return false;
    const bool movingRemoved =
        context->removeEvent(fou_EVC_MOVING, child) != 0;
    context->removeObject(child);
    return movingRemoved && !context->isExist(child) &&
           context->removeEvent(fou_EVC_MOVING, child) == 0;
}
