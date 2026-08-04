#ifndef RR2NW_PEOPLE_ROUTE_MOTION_H
#define RR2NW_PEOPLE_ROUTE_MOTION_H

#include "PEOPLE.H"

class IPeopleRouteNodeSource
{
public:
    virtual ~IPeopleRouteNodeSource() {}
    virtual int NodeCount() = 0;
    virtual CFVector3 Node(int index) = 0;
};

struct SPeopleRouteMotionRequest
{
    CFVector3 candidate;
    int previousNode;
    int currentNode;
    int backSpaceNode;
    int horizontal;
    int maximumSegments;
    double maximumCorridorDistance;
    double movementDistance;
    int centerToRoute;
};

struct SPeopleRouteMotionResult
{
    CFVector3 position;
    int previousNode;
    int currentNode;
    int segmentsConsumed;
    int stopped;
    int traversalLimitReached;
    int degenerateSegmentSeen;
    double corridorDistance;
    int outsideCorridor;
    int centeredToRoute;
};

bool PeopleRouteMotion_Advance(IPeopleRouteNodeSource *source,
                               const SPeopleRouteMotionRequest &request,
                               SPeopleRouteMotionResult *result);
bool PeopleRouteMotion_AllowsHorizontalStep(
    const CFVector3 &direction, const CFVector3 &position,
    const CFVector3 &target, double minimumAlignment);
// Diagnostic mirror of the replaced legacy predicate. Keeping it next to the
// XZ policy lets live telemetry count frames rescued from slope-sensitive 3D
// alignment without duplicating normalization math in the sampler.
bool PeopleRouteMotion_AllowsSpatialStep(
    const CFVector3 &direction, const CFVector3 &position,
    const CFVector3 &target, double minimumAlignment);
bool PeopleRouteMotion_FarAttackTarget(
    const CFVector3 &enemyPosition, const CFVector3 &enemyVelocity,
    const CFVector3 &previousTarget, double maximumExcursion,
    CFVector3 *target);
bool PeopleRouteMotion_NearAttackTarget(
    const CFVector3 &actorPosition, const CFVector3 &candidate,
    double maximumExcursion, CFVector3 *target);

// Deterministic synthetic proof for multi-segment travel, all three terminal
// policies, degenerate nodes, hard corridor bounds, smooth recentering and the
// May ten-node cap.
bool PeopleRouteMotion_Probe();

#endif
