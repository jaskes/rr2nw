#ifndef RR2NW_VEHICLE_VESSEL_TELEMETRY_H
#define RR2NW_VEHICLE_VESSEL_TELEMETRY_H

int RecoveredVehicleVesselBumpFlags(const char *dynamic);
bool RecoveredVehicleVesselTouchesGround(const char *dynamic);

struct SRecoveredWheelsSurfaceTelemetry
{
    int ready;
    double groundX;
    double groundY;
    double groundZ;
    double groundLength;
    double forwardTangentLength;
    double rightTangentLength;
    double tangentDot;
    double suspensionTravel;
    double accelerationFactor;
    double throttle;
};

bool RecoveredVehicleVesselWheelsSurface(
    const char *dynamic, SRecoveredWheelsSurfaceTelemetry *telemetry);

#endif
