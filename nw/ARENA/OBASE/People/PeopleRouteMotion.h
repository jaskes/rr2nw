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

// Deterministic synthetic proof for multi-segment travel, all three terminal
// policies, degenerate nodes, hard corridor bounds, smooth recentering and the
// May ten-node cap.
bool PeopleRouteMotion_Probe();

#endif
