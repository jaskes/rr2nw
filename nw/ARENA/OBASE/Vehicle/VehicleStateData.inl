#ifndef RR2NW_VEHICLE_STATE_DATA_INL
#define RR2NW_VEHICLE_STATE_DATA_INL

#ifdef _MSC_VER
#define RR2NW_VEHICLE_MEMBER_STORAGE
#else
#define RR2NW_VEHICLE_MEMBER_STORAGE static
#endif

RR2NW_VEHICLE_MEMBER_STORAGE int Vehicle::m_isTakingTaxiNow = 0;
RR2NW_VEHICLE_MEMBER_STORAGE double Vehicle::m_takingTaxiCurrentAngle;
RR2NW_VEHICLE_MEMBER_STORAGE double Vehicle::m_takingTaxiFinalAngle;

RR2NW_VEHICLE_MEMBER_STORAGE double Vehicle::m_lastEventTime;
RR2NW_VEHICLE_MEMBER_STORAGE double Vehicle::m_taxiRotateSpeed;

RR2NW_VEHICLE_MEMBER_STORAGE double Vehicle::m_spZ;
RR2NW_VEHICLE_MEMBER_STORAGE double Vehicle::m_spX;
RR2NW_VEHICLE_MEMBER_STORAGE double Vehicle::m_spY;
RR2NW_VEHICLE_MEMBER_STORAGE CFVector3 Vehicle::m_currentTaxiPos;
RR2NW_VEHICLE_MEMBER_STORAGE CFVector3 Vehicle::m_currentTaxiOurPos;
RR2NW_VEHICLE_MEMBER_STORAGE bool Vehicle::m_dead = 0;

#undef RR2NW_VEHICLE_MEMBER_STORAGE

double Vehicle::s_curTime = 0;

#endif
