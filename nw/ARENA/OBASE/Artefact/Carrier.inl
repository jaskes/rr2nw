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
		IArtefact *artefact = m_artefact;
		artefact->drop(m, ts);
		artefact->m_carrier = 0;
		artefact->m_carrierID = KR_ObjectID::NUL();

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
	if (m_artefact == 0 && artefact != 0 && !oID.isNUL())
	{
		m_artefactID = oID;
		m_artefact = artefact;
	}
}

bool ICarrier::carrierOnCollision(KR_ObjectID carrierID,
	KR_ObjectID objectID)
{
	SimulationContext *context = g_arena.getContext();
	if (context == 0 || m_artefact != 0 || carrierID.isNUL() ||
		objectID.isNUL() ||
		context->queryInterface(carrierID, ICarrierIID) != this)
		return false;

	IArtefact *artefact = (IArtefact *)context->queryInterface(
		objectID, IArtefactIID);
	if (artefact == 0 || artefact->isAttached() ||
		!artefact->attachTo(carrierID, this))
		return false;

	carrierTakeArtefact(objectID, artefact);
	if (m_artefact != artefact || m_artefactID != objectID)
		return false;
	carrierOnMove();
	return true;
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
