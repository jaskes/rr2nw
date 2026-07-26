#ifndef RR2NW_VEHICLE_STATE_IO_INL
#define RR2NW_VEHICLE_STATE_IO_INL

bool Vehicle::LoadStaticData(PIN_SaveFile &sf)
{
	if (!sf.GetData((char *)&m_dead, sizeof(bool)) ||
		!sf.GetData((char *)&s_curTime, sizeof(double)) ||
		!sf.GetData((char *)&m_isTakingTaxiNow, sizeof(int)) ||
		!sf.GetData((char *)&m_takingTaxiCurrentAngle, sizeof(double)) ||
		!sf.GetData((char *)&m_takingTaxiFinalAngle, sizeof(double)) ||
		!sf.GetData((char *)&m_lastEventTime, sizeof(double)) ||
		!sf.GetData((char *)&m_taxiRotateSpeed, sizeof(double)) ||
		!sf.GetData((char *)&m_spX, sizeof(double)) ||
		!sf.GetData((char *)&m_spZ, sizeof(double)) ||
		!sf.GetData((char *)&m_currentTaxiPos, sizeof(CFVector3)) ||
		!sf.GetData((char *)&m_currentTaxiOurPos, sizeof(CFVector3)))
		return false;

	return true;
}

bool Vehicle::SaveStaticData(PIN_SaveFile &sf)
{
	if (!sf.WriteData((char *)&m_dead, sizeof(bool)) ||
		!sf.WriteData((char *)&s_curTime, sizeof(double)) ||
		!sf.WriteData((char *)&m_isTakingTaxiNow, sizeof(int)) ||
		!sf.WriteData((char *)&m_takingTaxiCurrentAngle, sizeof(double)) ||
		!sf.WriteData((char *)&m_takingTaxiFinalAngle, sizeof(double)) ||
		!sf.WriteData((char *)&m_lastEventTime, sizeof(double)) ||
		!sf.WriteData((char *)&m_taxiRotateSpeed, sizeof(double)) ||
		!sf.WriteData((char *)&m_spX, sizeof(double)) ||
		!sf.WriteData((char *)&m_spZ, sizeof(double)) ||
		!sf.WriteData((char *)&m_currentTaxiPos, sizeof(CFVector3)) ||
		!sf.WriteData((char *)&m_currentTaxiOurPos, sizeof(CFVector3)))
		return false;

	return true;
}

#endif
