#ifndef RR2NW_FOUNTAIN_STATE_INL
#define RR2NW_FOUNTAIN_STATE_INL

#include <string.h>

bool FountBranch::dump(PIN_SaveFile &sf)
{
	PIN_SaveItemPrefix prefix;
	prefix.m_Type = PIN_SaveItemPrefix::IP_BRANCH;
	strcpy(prefix.m_Check, PIN_check);

	if (!sf.WriteData((char *)&prefix, sizeof(PIN_SaveItemPrefix)) ||
		!sf.WriteData((char *)&m_phase, sizeof(FountBranchData)))
		return false;

	return true;
}

bool FountBranch::load(PIN_SaveFile &sf)
{
	PIN_SaveItemPrefix prefix;
	if (!sf.GetData((char *)&prefix, sizeof(PIN_SaveItemPrefix)))
		return false;

	if (strcmp(prefix.m_Check, PIN_check) != 0 ||
		prefix.m_Type != PIN_SaveItemPrefix::IP_BRANCH)
		return false;

	if (!sf.GetData((char *)&m_phase, sizeof(FountBranchData)))
		return false;

	return true;
}

FountBranch Fountain::m_branch[MAX_BRANCH];
FountBranch *Fountain::m_freeList = 0;

void Fountain::createFreeList()
{
	for (int i = 0; i < MAX_BRANCH; ++i)
	{
		m_branch[i].m_next = &m_branch[i + 1];
		m_branch[i].m_prev = 0;
		m_branch[i].deleteCommand = 0;
	}
	m_branch[MAX_BRANCH - 1].m_next = 0;
	m_freeList = m_branch;
}

#endif
