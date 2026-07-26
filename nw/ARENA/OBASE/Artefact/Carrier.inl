#ifndef RR2NW_CARRIER_INL
#define RR2NW_CARRIER_INL

bool ICarrier::dump(PIN_SaveFile &sf)
{
	if (!sf.WriteData((char *)&m_artefactID, sizeof(KR_ObjectID)))
		return false;

	return true;
}

bool ICarrier::load(PIN_SaveFile &sf)
{
	if (!sf.GetData((char *)&m_artefactID, sizeof(KR_ObjectID)))
		return false;

	return true;
}

void ICarrier::loadNotify()
{
	if (!m_artefactID.isNUL())
		m_artefact = (IArtefact *)g_arena.getContext()->queryInterface(
			m_artefactID, IArtefactIID);
	else
		m_artefact = NULL;
}

void ICarrier::carrierDropArtefact(double ts)
{
	if (m_artefact != 0)
	{
		CFMatrix3x4 m;
		carrierLoadMatrix(m);
		m_artefact->drop(m, ts);

		m_artefact = 0;
		m_artefactID = KR_ObjectID::NUL();
	}
}

void ICarrier::carrierOnRemoveArtefact()
{
	if (m_artefact != 0)
	{
		m_artefact = 0;
		m_artefactID = KR_ObjectID::NUL();
	}
}

void ICarrier::carrierTakeArtefact(KR_ObjectID oID, IArtefact *artefact)
{
	if (m_artefact == 0)
	{
		m_artefactID = oID;
		m_artefact = artefact;
	}
}

void ICarrier::carrierOnCollizion(KR_ObjectID oID)
{
	if (m_artefact == 0 && !oID.isNUL())
	{
		IArtefact *artefact = (IArtefact *)g_arena.getContext()->queryInterface(
			oID, IArtefactIID);

		if (artefact != 0)
			carrierTakeArtefact(oID, artefact);
	}
}

int ICarrier::carrierReceiveEvent(KR_Event &event)
{
	switch (event.label)
	{
	case t_EV_ONCOLLISION:
		return 0;

	default:
		return 0;
	}
}

void ICarrier::carrierAddNotify(SimulationContext *, double)
{
	m_artefactID = KR_ObjectID::NUL();
	m_artefact = 0;
}

void ICarrier::carrierRemoveNotify(SimulationContext *, double ts)
{
	carrierDropArtefact(ts);
}

void ICarrier::carrierOnMove()
{
	if (m_artefact != 0)
	{
		CFMatrix3x4 m;
		carrierLoadMatrix(m);
		m_artefact->moveTo(m);
	}
}

bool IArtefact::dump(PIN_SaveFile &sf)
{
	if (!sf.WriteData((char *)&m_carrierID, sizeof(KR_ObjectID)))
		return false;

	return true;
}

bool IArtefact::load(PIN_SaveFile &sf)
{
	if (!sf.GetData((char *)&m_carrierID, sizeof(KR_ObjectID)))
		return false;

	return true;
}

void IArtefact::loadNotify()
{
	if (!m_carrierID.isNUL())
		m_carrier = (ICarrier *)g_arena.getContext()->queryInterface(
			m_carrierID, ICarrierIID);
	else
		m_carrier = NULL;
}

#endif
