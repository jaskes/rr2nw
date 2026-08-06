#ifndef RR2NW_DEBUG_MAP_MISSION_STATE_INL
#define RR2NW_DEBUG_MAP_MISSION_STATE_INL

DebugMap::DebugMap() {
  m_vPort = NULL;
  m_mapW = 0;
  m_mapH = 0;
  m_active = FALSE;
  m_initialized = FALSE;
  m_enableDraw = FALSE;
  m_followMode = TRUE;
  m_drawFrames = 0;
  m_openTransitions = 0;
  m_closeTransitions = 0;
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

int DebugMap::MissionCount() const {
  int count = 0;
  for (int i = 0; i < MAX_MISSIONS; ++i) {
    if (m_mission[i].use != 0) {
      ++count;
    }
  }
  return count;
}

int DebugMap::MissionRouteCount() const {
  int count = 0;
  for (int i = 0; i < MAX_MISSIONS; ++i) {
    if (m_mission[i].use != 0) {
      count += m_mission[i].routesNum;
    }
  }
  return count;
}

int DebugMap::MissionTextCount() const {
  int count = 0;
  for (int i = 0; i < MAX_MISSIONS; ++i) {
    if (m_mission[i].use != 0 && m_mission[i].text.font != NULL) {
      ++count;
    }
  }
  return count;
}

bool DebugMap::MissionInUse(TMissionId mId) const {
  return mId >= 0 && mId < MAX_MISSIONS && m_mission[mId].use != 0;
}

int DebugMap::MissionRouteCount(TMissionId mId) const {
  return MissionInUse(mId) ? m_mission[mId].routesNum : 0;
}

bool DebugMap::MissionHasText(TMissionId mId) const {
  return MissionInUse(mId) && m_mission[mId].text.font != NULL;
}

int DebugMap::CurrentMissionTextLine() const {
  return MissionInUse(m_curMission)
             ? m_mission[m_curMission].text.textCurrLine
             : 0;
}

int DebugMap::CurrentMissionTextLineCount() const {
  return MissionInUse(m_curMission)
             ? m_mission[m_curMission].text.textLineQnty
             : 0;
}

bool DebugMap::ProbeMissionNavigation() {
  ClearMissions();
  const int first = CreateMission("first");
  const int second = CreateMission("second");
  if (first != 0 || second != 1) return false;

  m_initialized = TRUE;
  m_enableDraw = TRUE;
  m_active = TRUE;
  m_followMode = TRUE;
  m_mapW = 1000;
  m_mapH = 1000;
  m_winW = 640;
  m_winH = 480;
  m_winBaseX = 100;
  m_winBaseY = 120;
  m_step = 6;
  m_mapScrollL = 101;
  m_mapScrollR = 102;
  m_mapScrollU = 103;
  m_mapScrollD = 104;
  m_textBoxStrQnty = 5;
  m_mission[first].text.textLineQnty = 8;
  m_mission[first].text.textCurrLine = 0;

  const auto send = [this](int action, int code) {
    KR_Event event;
    event.label = CTRL_BUTTONS_MSG;
    event.timeStamp = 1.0;
    event.data.open(EDO_WRITE)
        .putInt(action)
        .putDouble(1.0)
        .putInt(code)
        .putInt(FALSE)
        .close();
    return receiveEvent(event) == 1;
  };

  if (!send(DMAP_TOGGLE_FOLLOW_MODE, 0) || m_followMode != FALSE ||
      !send(TURN_RIGHT, m_mapScrollR) || m_winBaseX != 106 ||
      !send(TURN_LEFT, m_mapScrollL) || m_winBaseX != 100 ||
      !send(LOOK_DOWN, m_mapScrollD) || m_winBaseY != 126 ||
      !send(LOOK_UP, m_mapScrollU) || m_winBaseY != 120 ||
      !send(DMAP_NEXT_MISSION, 0) || m_curMission != second ||
      !send(DMAP_PREVIOUS_MISSION, 0) || m_curMission != first ||
      !send(DMAP_TEXT_BOX_DOWN, 0) || CurrentMissionTextLine() != 1 ||
      !send(DMAP_TEXT_BOX_UP, 0) || CurrentMissionTextLine() != 0 ||
      !send(DMAP_TOGGLE_FOLLOW_MODE, 0) || m_followMode != TRUE ||
      !send(DMAP_TOGGLE, 0) || m_active != FALSE)
    return false;

  const int missionBefore = m_curMission;
  return send(DMAP_NEXT_MISSION, 0) && m_curMission == missionBefore &&
         m_closeTransitions == 1;
}

#endif
