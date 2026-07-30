#ifndef RR2NW_VEHICLE_VESSEL_SAVE_STATE_H
#define RR2NW_VEHICLE_VESSEL_SAVE_STATE_H

#include <cstdint>

#include "mathlib.h"

// Field-level snapshots used by the original vessel SaveGame/LoadGame
// methods.  Keeping these layouts named lets the active-world codec encode
// each scalar explicitly instead of persisting compiler padding or pointers.
struct SVehicleEmvSaveState
{
    std::uint32_t size;
    SFMatrix3x4 direction;
    SFVector3 position;
    SFVector3 speed;
    double fuelSpeed;
    double throttle;
    double verticalRotation;
    double axisRotation;
    double inclination;
    double inclinationRotation;
    double turnRotation;
    double strafeInclination;
    double strafeInclinationRotation;
    double verticalStrafe;
    double mouseTurnRotation;
    double mouseVerticalRotation;
};

struct SVehicleWheelsSaveState
{
    std::uint32_t size;
    SFMatrix3x4 direction;
    SFMatrix3x4 base;
    SFVector3 position;
    SFVector3 speed;
    SFVector3 moment;
    SFVector3 groundNormal;
    SFVector3 lastAcceleration;
    double fuelAcceleration;
    double throttle;
    int forward;
    bool brakingDone;
    bool touchingGround;
    int braking;
    double raise;
    double raiseSpeed;
    double turnSpeed;
    SFVector3 maximumHorizontalSpeed;
    double accelerationFactor;
    double jumpOffsetFactor;
    double jumpTimeout;
    double suspensionTravel;
    double walkPendulumArgument;
    double walkPendulumCoefficient;
    double walkPendulumSpeedCoefficient;
    double mouseTurnSpeed;
    double mouseRaise;
    double mouseRaiseSpeed;
    double jumpTime;
    double jumpAccumulation;
    double jumpDownVelocity;
    double lastFallVelocity;
    SFVector3 jumpOffset;
    SFVector3 jumpSpeed;
    SFVector3 walkPendulumTravel;
    int jumpPhase;
};

#endif
