#define LAST_H__VIEW
#include "game.h"

CViewOrdered* CViewOrdered::m_pCurrentTopOrdered = NULL;

CViewOrdered::CViewOrdered()
{
	m_bEmbedded = TRUE;
	m_bDynamics = FALSE;
	if( !m_pCurrentTopOrdered ) m_pCurrentTopOrdered = this;
	m_pTopOrder = m_pCurrentTopOrdered;
	m_fMaxHeight = 0.0;
	m_dwLights = 0;
}

void CViewOrdered::PromoteDynamic()
{
	ASSERT(!m_dynamicList.First() && !m_afterList.First() &&
		!m_shotList.First());
	m_bDynamics = FALSE;
}

void CViewOrdered::RemoveDynamics()
{
	ASSERT(!m_dynamicList.First() && !m_afterList.First() &&
		!m_shotList.First());
	m_bDynamics = FALSE;
}

#ifdef _DEBUG
void CViewOrdered::CheckNoDynamics()
{
	ASSERT(!m_dynamicList.First() && !m_afterList.First() &&
		!m_shotList.First());
}
#endif
