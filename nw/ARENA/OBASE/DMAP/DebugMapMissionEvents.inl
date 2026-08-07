#ifndef RR2NW_DEBUG_MAP_MISSION_EVENTS_INL
#define RR2NW_DEBUG_MAP_MISSION_EVENTS_INL

namespace {

KR_ObjectID DebugMapHardwareID(DebugMap* map) {
  if (map->getContext() == NULL) {
    return KR_ObjectID::NUL();
  }
  return map->getContext()->searchObject("Hardware");
}

void DebugMapSetHardwareMode(DebugMap* map, int label, double timeStamp) {
  KR_ObjectID hardware_id = DebugMapHardwareID(map);
  if (hardware_id.isNUL()) {
    return;
  }

  KR_Event event;
  event.timeStamp = timeStamp;
  event.source = map->getObjectID();
  event.destination = hardware_id;
  event.label = label;
  map->getContext()->sendEventNow(event);
}

}  // namespace

void DebugMap::EnableRender3D() {}

void DebugMap::DisableRender3D() {}

void DebugMap::addNotify() {
  KR_ObjectID hardware_id = DebugMapHardwareID(this);
  if (!hardware_id.isNUL()) {
    KR_Event event;
    event.source = getObjectID();
    event.destination = hardware_id;
    event.label = CTRL_SUBSCRIBE;
    event.timeStamp = Session::m_moment;
    event.data.open(EDO_WRITE)
        .putObjectID(getObjectID())
        .putInt(EXCLUSIVE)
        .close();
    getContext()->addEvent(event);
  }

  KR_Object::addNotify();
}

void DebugMap::removeNotify() {
  KR_Object::removeNotify();

  KR_ObjectID hardware_id = DebugMapHardwareID(this);
  if (hardware_id.isNUL()) {
    return;
  }

  KR_Event event;
  event.timeStamp = Session::m_moment;
  event.source = getObjectID();
  event.destination = hardware_id;
  event.label = CTRL_UNSUBSCRIBE;
  event.data.open(EDO_WRITE).putObjectID(getObjectID()).close();
  getContext()->sendEventNow(event);
}

int DebugMap::receiveEvent(KR_Event& event) {
  int code;
  int ctrlEvent;
  double down;
  int repeat;

  switch (event.label) {
    case KR_WAKE_UP:
    case CTRL_CHAR:
    case CTRL_MOUSE_MOVE_MSG:
    case CTRL_JOYSTICK_MOVE_MSG:
      break;

    case CTRL_BUTTONS_MSG: {
      event.data.open(EDO_READ)
          .getInt(ctrlEvent)
          .getDouble(down)
          .getInt(code)
          .getInt(repeat)
          .close();

      if (down == 0) {
        break;
      }

      if (ctrlEvent == DMAP_TOGGLE) {
        if (m_active) {
          m_active = FALSE;
          ++m_closeTransitions;
          EnableRender3D();
          if (!m_followMode) {
            DebugMapSetHardwareMode(this, CTRL_SET_NORMAL, event.timeStamp);
          }
        } else {
          if (!m_initialized || !m_enableDraw) {
            break;
          }
          m_active = TRUE;
          ++m_openTransitions;
          DisableRender3D();
          if (!m_followMode) {
            DebugMapSetHardwareMode(this, CTRL_SET_EXCLUSIVE,
                                    event.timeStamp);
          }
        }
      }

      if (!m_active) {
        break;
      }

      const bool explicitMapScroll =
          ctrlEvent == DMAP_SCROLL_LEFT ||
          ctrlEvent == DMAP_SCROLL_RIGHT ||
          ctrlEvent == DMAP_SCROLL_UP ||
          ctrlEvent == DMAP_SCROLL_DOWN;
      if (!m_followMode && !explicitMapScroll) {
        if (code == m_mapScrollL) {
          m_winBaseX -= m_step;
          m_winBaseX = Max(0, m_winBaseX);
        } else if (code == m_mapScrollR) {
          m_winBaseX += m_step;
          m_winBaseX = Min(m_mapW - m_winW, m_winBaseX);
        } else if (code == m_mapScrollU) {
          m_winBaseY -= m_step;
          m_winBaseY = Max(0, m_winBaseY);
        } else if (code == m_mapScrollD) {
          m_winBaseY += m_step;
          m_winBaseY = Min(m_mapH - m_winH, m_winBaseY);
        }
      }

      switch (ctrlEvent) {
        case DMAP_SCROLL_LEFT:
          if (!m_followMode) {
            m_winBaseX -= m_step;
            m_winBaseX = Max(0, m_winBaseX);
          }
          break;

        case DMAP_SCROLL_RIGHT:
          if (!m_followMode) {
            m_winBaseX += m_step;
            m_winBaseX = Min(m_mapW - m_winW, m_winBaseX);
          }
          break;

        case DMAP_SCROLL_UP:
          if (!m_followMode) {
            m_winBaseY -= m_step;
            m_winBaseY = Max(0, m_winBaseY);
          }
          break;

        case DMAP_SCROLL_DOWN:
          if (!m_followMode) {
            m_winBaseY += m_step;
            m_winBaseY = Min(m_mapH - m_winH, m_winBaseY);
          }
          break;

        case DMAP_TOGGLE_FOLLOW_MODE:
          m_followMode = !m_followMode;
          DebugMapSetHardwareMode(this,
                                  m_followMode ? CTRL_SET_NORMAL
                                               : CTRL_SET_EXCLUSIVE,
                                  event.timeStamp);
          break;

        case DMAP_NEXT_MISSION:
          for (int i = m_curMission + 1; i < MAX_MISSIONS; ++i) {
            if (m_mission[i].use) {
              m_curMission = i;
              break;
            }
          }
          break;

        case DMAP_PREVIOUS_MISSION:
          for (int i = m_curMission - 1; i >= 0; --i) {
            if (m_mission[i].use) {
              m_curMission = i;
              break;
            }
          }
          break;

        case DMAP_TEXT_BOX_UP:
          if (m_mission[m_curMission].text.textCurrLine > 0) {
            --m_mission[m_curMission].text.textCurrLine;
          }
          break;

        case DMAP_TEXT_BOX_DOWN: {
          SMapText* text = &m_mission[m_curMission].text;
          if (text->textCurrLine <
              text->textLineQnty - m_textBoxStrQnty) {
            ++text->textCurrLine;
          }
          break;
        }

        default:
          break;
      }
      break;
    }

    default:
      return 0;
  }
  return 1;
}

#endif
