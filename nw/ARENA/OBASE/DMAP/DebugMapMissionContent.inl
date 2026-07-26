#ifndef RR2NW_DEBUG_MAP_MISSION_CONTENT_INL
#define RR2NW_DEBUG_MAP_MISSION_CONTENT_INL

void DebugMap::AddMissionRoute(TMissionId mId,
                               IRouteObject* route,
                               unsigned long rgb,
                               float widthS,
                               float widthE) {
  if (mId < 0 || mId >= MAX_MISSIONS || m_mission[mId].use == 0 ||
      route == NULL || m_mission[mId].routesNum >= MAX_ROUTES) {
    return;
  }

  SMapRoute* r = &(m_mission[mId].route[m_mission[mId].routesNum++]);

  r->pointsNum = route->GetNodeCnt();
  if (r->pointsNum > MAX_ROUTE_POINTS) {
    r->pointsNum = MAX_ROUTE_POINTS;
  }
  for (int i = 0; i < r->pointsNum; ++i) {
    CFVector3 node = route->GetNode(i);

    r->point[i * 2 + 0] = m_winX + (int)(node.x * m_mapScaleX);
    r->point[i * 2 + 1] = m_winY + m_mapH + (int)(node.z * m_mapScaleY);
  }

  r->widthS = widthS;
  r->widthE = widthE;
  r->color = GRCreateColor(rgb >> 16, (rgb >> 8) & 0xff, rgb & 0xff);
}

void DebugMap::AddMissionText(TMissionId mId,
                              const char* str,
                              const char* fntObjName) {
  if (mId < 0 || mId >= MAX_MISSIONS || m_mission[mId].use == 0 ||
      str == NULL || fntObjName == NULL || g_arena.getContext() == NULL) {
    return;
  }

  SMapText* txt = &m_mission[mId].text;
  KR_ObjectID fontID = g_arena.getContext()->searchObject(fntObjName);
  txt->font = (IFixedFont*)(
      g_arena.getContext()->queryInterface(fontID, IFixedFontIID));
  if (txt->font == NULL) {
    return;
  }

  txt->textCurrLine = 0;
  strncpy(txt->str, str, sizeof(txt->str) - 1);
  txt->str[sizeof(txt->str) - 1] = 0;

  txt->textLineQnty = 1;
  for (int i = 0; txt->str[i] != 0; ++i) {
    if (txt->str[i] == '$') {
      txt->str[i] = 0;
      ++txt->textLineQnty;
    }
  }

  txt->dy = txt->font->Height();
  txt->x = m_textBoxX + m_textBoxOffX;
  txt->y = m_textBoxY + m_textBoxOffY + txt->dy;
  txt->boxDy = m_textBoxOffY * 2 + txt->dy * (m_textBoxStrQnty + 1);

  txt->boxDx = 0;
  char* line = txt->str;
  for (int i = 0; i < txt->textLineQnty; ++i) {
    const int len = txt->font->StringWidth(line);
    if (len > txt->boxDx) {
      txt->boxDx = len;
    }
    while (*line++ != 0) {
    }
  }

  const int name_len = txt->font->StringWidth(txt->name);
  if (name_len > txt->boxDx) {
    txt->boxDx = name_len;
  }

  txt->boxDx += m_textBoxOffX * 2 + m_textBoxSliderDx;
  txt->sliderUpDownOffX =
      txt->boxDx - m_textBoxOffX - m_textBoxSliderDx;
  txt->sliderUpOffY = txt->dy + m_textBoxOffY;
  txt->sliderDownOffY = txt->boxDy - m_textBoxOffY - m_textBoxSliderDy;
}

#endif
