#ifndef RR2NW_DEBUG_MAP_MISSION_STATE_INL
#define RR2NW_DEBUG_MAP_MISSION_STATE_INL

DebugMap::DebugMap() {
  m_vPort = NULL;
  m_active = FALSE;
  m_enableDraw = FALSE;
  m_followMode = TRUE;
  m_levelMap = new CGRImage;
  ClearMissions();
}

DebugMap::~DebugMap() {
  delete m_levelMap;
}

TMissionId DebugMap::CreateMission(const char* name) {
  for (int i = 0; i < MAX_MISSIONS; ++i) {
    if (m_mission[i].use == FALSE) {
      m_mission[i].use = TRUE;
      const char* mission_name = name == NULL ? "" : name;
      strncpy(m_mission[i].text.name, mission_name,
              sizeof(m_mission[i].text.name) - 1);
      m_mission[i].text.name[sizeof(m_mission[i].text.name) - 1] = 0;
      return i;
    }
  }

  return -1;
}

void DebugMap::DeleteMission(TMissionId mId) {
  if (mId < 0 || mId >= MAX_MISSIONS) {
    return;
  }

  m_mission[mId].use = 0;
  ClearMission(mId);
}

void DebugMap::ClearMission(TMissionId mId) {
  if (mId < 0 || mId >= MAX_MISSIONS) {
    return;
  }

  m_mission[mId].text.str[0] = 0;
  m_mission[mId].text.name[0] = 0;
  m_mission[mId].text.textLineQnty = 0;
  m_mission[mId].text.font = NULL;
  m_mission[mId].routesNum = 0;
}

void DebugMap::ClearMissions() {
  m_curMission = 0;
  for (int i = 0; i < MAX_MISSIONS; ++i) {
    DeleteMission(i);
  }
}

#endif
