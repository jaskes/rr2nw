#ifndef RR2NW_PEOPLE_OBSTACLE_RECOVERY_H
#define RR2NW_PEOPLE_OBSTACLE_RECOVERY_H

struct SPeopleObstacleRecoveryRequest
{
    double objectRadius;
    double maximumRadius;
    double recoveryTime;
    double deltaTime;
    double collisionTime;
    int visible;
    int collisionDetected;
    int contactCode;
    int detectedContactCode;
    int stateDepth;
};

struct SPeopleObstacleRecoveryResult
{
    double sweepRadius;
    double recoveryTime;
    double travelTime;
    int sweepEnabled;
    int popState;
    int contactCode;
};

// Deterministic policy recovered from the May 1999 ON_OBJ branch. Scene
// collision detection remains at the call site; this kernel owns the shrinking
// sweep radius, contact travel fraction and recovery timer transitions. The
// modern compatibility boundary also performs one bounded opposite-side retry
// for a persistent ownerless horizontal obstruction.
bool PeopleObstacleRecovery_Advance(
    const SPeopleObstacleRecoveryRequest &request,
    SPeopleObstacleRecoveryResult *result);

bool PeopleObstacleRecovery_Probe();

#endif
