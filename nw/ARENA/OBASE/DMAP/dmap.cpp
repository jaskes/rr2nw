#include "dmap.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "kernel/h/s_debug.h"
#include "kernel/h/echo.h"
#include "Storage/h/subject.h"
#include "vehicle.h"
#include "filesys.h"
#include "i/dynobj.i"
#include "d3d.h"

extern SDeviceList _dL;
extern float __HazeLen;

#ifndef RR2NW_DMAP_MISSION_EXTERNAL
#include "DebugMapMissionState.inl"
#include "DebugMapMissionContent.inl"
#include "DebugMapMissionEvents.inl"
#endif

//---------------------------------------------
bool DebugMap::Init(const char * mapName)
{
   DeInit();
   m_drawFrames = 0;
   m_openTransitions = 0;
   m_closeTransitions = 0;
   if (mapName == NULL || mapName[0] == 0 ||
       !m_levelMap->LoadFromBMPFile(mapName, 0, 0))
      return false;

   m_mapW = m_levelMap->Width();
   m_mapH = m_levelMap->Height();
   if (m_mapW <= 0 || m_mapH <= 0) {
      m_levelMap->Delete();
      return false;
   }
   m_mapScaleX  = (float)m_mapW / (512.0f*10.0f);
   m_mapScaleY  = (float)m_mapH / (512.0f*10.0f);
   m_mapiScaleX = (512.0f*10.0f) / (float)m_mapW;
   m_mapiScaleY = (512.0f*10.0f) / (float)m_mapH;

   m_mapScrollL = g_hardware.SearchCode("Left");
   m_mapScrollR = g_hardware.SearchCode("Right");
   m_mapScrollU = g_hardware.SearchCode("Up");
   m_mapScrollD = g_hardware.SearchCode("Down");

   m_selfColor     = GRTransparentColor(255, 255, 255);
   m_selfRColor    = GRFillColor(255, 255, 255);
   m_friendColor   = GRTransparentColor(0, 255, 0);
   m_enemyColor    = GRTransparentColor(255, 0, 0);
   m_objectColor   = GRTransparentColor(128, 128, 128);
   m_portalColor   = GRCreateColor(255, 0, 255);
   m_artefactColor = GRFillColor(0, 0, 255);

   m_artefactR = 10.0;

   m_textBoxX          = 10;
   m_textBoxY          = 10;
   m_textBoxOffX       = 4;
   m_textBoxOffY       = 4;
   m_textBoxStrQnty    = 5;
   m_textBoxSliderDx   = 14;
   m_textBoxSliderDy   = 14;
   m_textBoxSlOnColor  = GRTransparentColor(0,128,0);
   m_textBoxSlOffColor = GRTransparentColor(0,0,0);
   m_textBoxColor      = GRTransparentColor(0,0,0);


   m_active            = FALSE;
   m_drawRoutes        = TRUE;
   m_drawFriendObjects = TRUE;
   m_drawEnemyObjects  = TRUE;
   m_drawPortal        = FALSE;
   m_drawArtefact      = TRUE;

   m_enableDraw        = TRUE;

   m_followMode        = TRUE;

   m_winBaseX = 0;
   m_winBaseY = 0;
   m_step     = 6;

   m_winW    = Min(_gr_nScreenWidth,  m_mapW);
   m_winH    = Min(_gr_nScreenHeight, m_mapH);
   m_winX    = Max(0, (_gr_nScreenWidth  - m_mapW) / 2);
   m_winY    = Max(0, (_gr_nScreenHeight - m_mapH) / 2);

   m_panelW       = 320 - m_textBoxX*2;
   m_panelH       = 32;
   m_panelX       = (m_winW - m_panelW)/2;
   m_panelY       = m_winH - m_panelH - m_textBoxY;
   m_panelDamageX = m_panelX+4;
   m_panelDamageY = m_panelY+8;

   m_panelColor = GRTransparentColor(0,0,0);

   m_curMission = 0;
   for(int i = 0;i < MAX_MISSIONS;i++)
      DeleteMission(i);


   m_winDx = m_winW/4;
   m_winDy = m_winH/4;

   CRect2      viewRect(m_winX + (m_winW*3)/4 - m_textBoxX,
                        m_winY + m_textBoxY,
                        m_winX + m_winW - m_textBoxX,
                        m_winY + m_winH/4 + m_textBoxY
                       );

   m_vPort = GRCreateViewport(m_winX + (m_winW*7)/8 - m_textBoxX,
                              m_winY + m_winH/8 + m_textBoxY,
                              viewRect
                             );
   if (m_vPort == NULL) {
      m_levelMap->Delete();
      m_enableDraw = FALSE;
      return false;
   }
   m_initialized = TRUE;
   return true;
}

//---------------------------------------------
void DebugMap::DeInit()
{
   m_active = FALSE;
   m_initialized = FALSE;
   if (m_vPort != NULL) {
      GRReleaseViewport(m_vPort);
      m_vPort = NULL;
   }
   m_levelMap->Delete();

   m_enableDraw = FALSE;
   m_mapW = 0;
   m_mapH = 0;
}

//---------------------------------------------
//---------------------------------------------
bool DebugMap::DrawRecovered() {
    int i,j;
    SMapMission *mM = &m_mission[m_curMission];
    IPlayer        *player;

    if (!m_initialized || !m_enableDraw || !m_active || m_vPort == NULL ||
        getContext() == NULL || g_vehicle == NULL)
       return false;

    //patch clipRect
    _gr_clipRect.left   = m_winX -_gr_nScreenOriginX;
    _gr_clipRect.right  = m_winX + m_winW - _gr_nScreenOriginX;
    _gr_clipRect.top    = m_winY -_gr_nScreenOriginY;
    _gr_clipRect.bottom = m_winY + m_winH - _gr_nScreenOriginY;
    GRSetClipRect();

    m_self = (IDynamicObject *)(g_vehicle->queryInterface(IDynamicObjectIID));
    player = (IPlayer *)(g_vehicle->queryInterface(IPlayerIID));
    if (m_self == NULL || player == NULL)
       return false;

    //draw Map
    GREndScene();

    if (m_followMode) {
       CFVector3 pos = m_self->getPos();
       int x = (int)(pos.x*m_mapScaleX);
       int y = m_mapH + (int)(pos.z*m_mapScaleY);

       if (x + m_winW/2 > m_mapW)
          m_winBaseX = m_mapW - m_winW;
       else {
          if (x - m_winW/2 < 0)
             m_winBaseX = 0;
          else
             m_winBaseX = x - m_winW/2;
       }

       if (y + m_winH/2 > m_mapH)
          m_winBaseY = m_mapH - m_winH;
       else {
          if (y - m_winH/2 < 0)
             m_winBaseY = 0;
          else
             m_winBaseY = y - m_winH/2;
       }
    }

    if (!m_levelMap->Draw(m_winX, m_winY, m_winBaseX, m_winBaseY,
                          m_winBaseX+m_winW, m_winBaseY+m_winH))
       return false;

    GRStartScene();

    GRZBufferEnable(0);

    //draw Routes
    if (m_drawRoutes) {
       int points[MAX_ROUTE_POINTS*2];
       SMapRoute *rt = mM->route;

       for(i = 0;i < mM->routesNum;i++,rt++) {
          for(j = 0;j < rt->pointsNum*2;j += 2) {
             points[j+0] = rt->point[j+0] - m_winBaseX;
             points[j+1] = rt->point[j+1] - m_winBaseY;
          }

          if (rt->pointsNum >= 2)
             GRLUDrawArrow(points, rt->pointsNum, rt->widthS, rt->widthE, rt->color);
       }
    }

    //draw Portal
    if (m_drawPortal) {
    }


    //draw Friend ,Enemy  and Artefact Objects
    if (m_drawFriendObjects || m_drawEnemyObjects || m_drawArtefact) {
       float xMin, yMin, xMax, yMax;
       ct_SubjectFindData fsd;
       IDynamicObject *dyn;
       IUnit          *unit;
       IArtefact      *art;


       xMin =  m_winBaseX * m_mapiScaleX;
       yMin =  (m_winBaseY - m_mapH) * m_mapiScaleY;
       xMax = xMin + m_winW*m_mapiScaleX;
       yMax = yMin + m_winH*m_mapiScaleY;
       ct_Arena::findFirstSubject( fsd, xMin, yMin, xMax, yMax);

       for(i = 0; i < fsd.getCount(); ++i ) {
          KR_ObjectID id = fsd[i];

          if (id == KR_ObjectID::NUL()) continue;

          dyn = (IDynamicObject *)(getContext()->queryInterface(id, IDynamicObjectIID));
          if (dyn == NULL) continue;

          art = (IArtefact *)(getContext()->queryInterface(id, IArtefactIID));

          if (art != NULL) {
             if (m_drawArtefact)
                if (!art->isAttached() || art->m_carrierID != g_vehicle->getObjectID())
                   DrawArtefact(dyn, m_artefactColor, TRUE);
          }
          else {
             unit = (IUnit*) (getContext()->queryInterface(id, IUnitIID));

             if (unit != NULL) {
                if (dyn != m_self) {
                   KR_ObjectID com = unit->getCommander();

                   if (com.isNUL())
                      DrawDynamicObject(dyn, m_objectColor, FALSE);
                   else {
                      if (player->isRenegat(com)) {
                         if (m_drawEnemyObjects)
                            DrawDynamicObject(dyn, m_enemyColor, TRUE);
                      }
                      else {
                         if (m_drawFriendObjects)
                            DrawDynamicObject(dyn, m_friendColor, FALSE);
                      }
                   }
                }
             }
          }
       }

    }

    //draw Self
    DrawSelf(m_self, m_selfColor);

    //draw Mission Text
    if (mM->text.font != NULL)
       DrawMissionText(&mM->text);

    //draw Panel
    GRStartScene();

    if (mM->text.font != NULL)
       DrawPanel(player);

    GRZBufferEnable(1);
    //restore clipRect
    _gr_clipRect = GRGetViewport()->clipRect;
    GRSetClipRect();

    GRStartScene();
    ++m_drawFrames;
    return true;
}
//---------------------------------------------
void DebugMap::DrawSelf(IDynamicObject * obj, unsigned long col)
{  CFVector3 objPos = obj->getPos();
   float     x, y, r;

   DrawDynamicObject(obj, col, FALSE);

   //GREndScene();
   GREnable2D();

   x  = (float)(m_winX - m_winBaseX + (int)(objPos.x*m_mapScaleX));
   y  = (float)(m_winY + m_mapH - m_winBaseY + (int)(objPos.z*m_mapScaleY));
   r  = __HazeLen*m_mapScaleX;

   Circle((int)x, (int)y, (int)r, m_selfRColor, FALSE);

   GRDisable2D();
   //GRStartScene();
}
//---------------------------------------------

#define PROC 0.7

void DebugMap::DrawDynamicObject(IDynamicObject * obj, unsigned long col, int checkR)
{  CFVector3 objPos = obj->getPos();
   float     objR   = (float)obj->getRadius();
   float     hAng   = (float)obj->getHAngle();
   float     x, y;
   float     r,r1;
   float     dx,dy;

   if (checkR) {
      CFVector3 sPos = m_self->getPos();
      if (Abs2(objPos - sPos) >= __HazeLen*__HazeLen)
         return;
   }

   x  = (float)(m_winX - m_winBaseX + (int)(objPos.x*m_mapScaleX) - _gr_nScreenOriginX);
   y  = (float)(m_winY + m_mapH - m_winBaseY + (int)(objPos.z*m_mapScaleY) - _gr_nScreenOriginY);
   r  = (float)Max(objR*m_mapScaleX, 8.0f);
   r1 = r*(float)PROC;

   _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
   _gr_polygon.dwAddType = 0;
   _gr_polygon.nVertices = 3;
   _gr_polygon.dwColor.color = col;
   _gr_polygon.dwOpacity = 200;
   _gr_polygon.nLights = 0;

   dx = (float)cos(hAng);
   dy = (float)sin(hAng);

   _gr_vertices[0].any.x = (int)(x + dx*r);
   _gr_vertices[0].any.y = (int)(y + dy*r);
   _gr_vertices[0].any.iz = 6550;

   x -= (dx*r);
   y -= (dy*r);

   _gr_vertices[1].any.x = (int)(x + dy*r1);
   _gr_vertices[1].any.y = (int)(y - dx*r1);
   _gr_vertices[1].any.iz = 6550;

   _gr_vertices[2].any.x = (int)(x - dy*r1);
   _gr_vertices[2].any.y = (int)(y + dx*r1);
   _gr_vertices[2].any.iz = 6550;

   GRDrawPolygonPCCW();

}
//---------------------------------------------
void DebugMap::DrawArtefact(IDynamicObject * obj, unsigned long col, int checkR)
{  CFVector3 objPos = obj->getPos();
   float     x, y, time;

   if (checkR) {
      CFVector3 sPos = m_self->getPos();
      if (Abs2(objPos - sPos) >= __HazeLen*__HazeLen)
         return;
   }

   //GREndScene();
   GREnable2D();

   x  = (float)(m_winX - m_winBaseX + (int)(objPos.x*m_mapScaleX));
   y  = (float)(m_winY + m_mapH - m_winBaseY + (int)(objPos.z*m_mapScaleY));

   Circle((int)x, (int)y, 3, col, TRUE);

   time = (float)Session::m_realTimer->GetTime();

   Circle((int)x, (int)y, (int)(m_artefactR*(time - ((int)time))), col, FALSE);

   GRDisable2D();
   //GRStartScene();
}
//---------------------------------------------
void DebugMap::DrawPanel(IPlayer *player)
{  char str[40];

   (void)player;

   _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
   _gr_polygon.dwAddType = 0;
   _gr_polygon.nVertices = 4;
   _gr_polygon.dwOpacity = 128;
   _gr_polygon.dwColor.color = m_panelColor;
   _gr_polygon.nLights = 0;

   _gr_vertices[0].any.x = m_panelX - _gr_nScreenOriginX;
   _gr_vertices[0].any.y = m_panelY - _gr_nScreenOriginY;
   _gr_vertices[0].any.iz = 6550;

   _gr_vertices[1].any.x = m_panelX - _gr_nScreenOriginX;
   _gr_vertices[1].any.y = m_panelY - _gr_nScreenOriginY + m_panelH;
   _gr_vertices[1].any.iz = 6550;

   _gr_vertices[2].any.x = m_panelX - _gr_nScreenOriginX + m_panelW;
   _gr_vertices[2].any.y = m_panelY - _gr_nScreenOriginY + m_panelH;
   _gr_vertices[2].any.iz = 6550;

   _gr_vertices[3].any.x = m_panelX - _gr_nScreenOriginX + m_panelW;
   _gr_vertices[3].any.y = m_panelY - _gr_nScreenOriginY;
   _gr_vertices[3].any.iz = 6550;

   GRDrawPolygonPCCW();

   GREndScene();

   sprintf(str,"Health  %d%%", (int)(g_vehicle->getDamage()*100.));

   m_mission[m_curMission].text.font->LUPrintAt(m_panelDamageX, m_panelDamageY, str);
}

//---------------------------------------------
void DebugMap::DrawMissionText(SMapText * mT)
{  char *tmp;
   int i, drL, y;

   //GREnableZBuffer(0);

   _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
   _gr_polygon.dwAddType = 0;
   _gr_polygon.nVertices = 4;
   _gr_polygon.dwOpacity = 128;
   _gr_polygon.dwColor.color = m_textBoxColor;
   _gr_polygon.nLights = 0;

   _gr_vertices[0].any.x = m_textBoxX - _gr_nScreenOriginX;
   _gr_vertices[0].any.y = m_textBoxY - _gr_nScreenOriginY;
   _gr_vertices[0].any.iz = 6550;

   _gr_vertices[1].any.x = m_textBoxX - _gr_nScreenOriginX;
   _gr_vertices[1].any.y = m_textBoxY - _gr_nScreenOriginY + mT->boxDy;
   _gr_vertices[1].any.iz = 6550;

   _gr_vertices[2].any.x = m_textBoxX - _gr_nScreenOriginX + mT->boxDx;
   _gr_vertices[2].any.y = m_textBoxY - _gr_nScreenOriginY + mT->boxDy;
   _gr_vertices[2].any.iz = 6550;

   _gr_vertices[3].any.x = m_textBoxX - _gr_nScreenOriginX + mT->boxDx;
   _gr_vertices[3].any.y = m_textBoxY - _gr_nScreenOriginY;
   _gr_vertices[3].any.iz = 6550;

   GRDrawPolygonPCCW();

   //draw Sliders
   //up
   _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
   _gr_polygon.dwAddType = 0;
   _gr_polygon.nVertices = 3;
   _gr_polygon.dwOpacity = 200;
   if (mT->textCurrLine > 0)
      _gr_polygon.dwColor.color = m_textBoxSlOnColor;
   else
      _gr_polygon.dwColor.color = m_textBoxSlOffColor;
   _gr_polygon.nLights = 0;

   _gr_vertices[0].any.x = m_textBoxX - _gr_nScreenOriginX +
                           mT->sliderUpDownOffX + m_textBoxSliderDx/2;
   _gr_vertices[0].any.y = m_textBoxY - _gr_nScreenOriginY +
                           mT->sliderUpOffY;
   _gr_vertices[0].any.iz = 6550;

   _gr_vertices[1].any.x = m_textBoxX - _gr_nScreenOriginX +
                           mT->sliderUpDownOffX;
   _gr_vertices[1].any.y = m_textBoxY - _gr_nScreenOriginY +
                           mT->sliderUpOffY + m_textBoxSliderDy;
   _gr_vertices[1].any.iz = 6550;

   _gr_vertices[2].any.x = m_textBoxX - _gr_nScreenOriginX +
                           mT->sliderUpDownOffX + m_textBoxSliderDx;
   _gr_vertices[2].any.y = m_textBoxY - _gr_nScreenOriginY +
                           mT->sliderUpOffY + m_textBoxSliderDy;
   _gr_vertices[2].any.iz = 6550;

   GRDrawPolygonPCCW();

   //down
   _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
   _gr_polygon.dwAddType = 0;
   _gr_polygon.nVertices = 3;
   _gr_polygon.dwOpacity = 200;
   if (mT->textLineQnty - mT->textCurrLine > m_textBoxStrQnty)
      _gr_polygon.dwColor.color = m_textBoxSlOnColor;
   else
      _gr_polygon.dwColor.color = m_textBoxSlOffColor;
   _gr_polygon.nLights = 0;

   _gr_vertices[0].any.x = m_textBoxX - _gr_nScreenOriginX +
                           mT->sliderUpDownOffX + m_textBoxSliderDx/2;
   _gr_vertices[0].any.y = m_textBoxY - _gr_nScreenOriginY +
                           mT->sliderDownOffY + m_textBoxSliderDy;
   _gr_vertices[0].any.iz = 6550;

   _gr_vertices[1].any.x = m_textBoxX - _gr_nScreenOriginX +
                           mT->sliderUpDownOffX + m_textBoxSliderDx;
   _gr_vertices[1].any.y = m_textBoxY - _gr_nScreenOriginY +
                           mT->sliderDownOffY;
   _gr_vertices[1].any.iz = 6550;

   _gr_vertices[2].any.x = m_textBoxX - _gr_nScreenOriginX +
                           mT->sliderUpDownOffX;
   _gr_vertices[2].any.y = m_textBoxY - _gr_nScreenOriginY +
                           mT->sliderDownOffY;
   _gr_vertices[2].any.iz = 6550;

   GRDrawPolygonPCCW();

   //draw Mission Sliders
   y = (mT->boxDx - m_textBoxOffX*2 - m_textBoxSliderDx*2 - mT->font->StringWidth(mT->name))/2;
   int nameX0 = mT->x + m_textBoxSliderDx + y;
   int nameX1 = mT->x + m_textBoxSliderDx + y + mT->font->StringWidth(mT->name);
   //draw left Slider
   _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
   _gr_polygon.dwAddType = 0;
   _gr_polygon.nVertices = 3;
   _gr_polygon.dwOpacity = 200;

   drL = 0;
   for(i = m_curMission - 1;i >= 0;i--)
      if (m_mission[i].use) {
         drL = 1;
         break;
      }
   if (drL)
      _gr_polygon.dwColor.color = m_textBoxSlOnColor;
   else
      _gr_polygon.dwColor.color = m_textBoxSlOffColor;

   _gr_polygon.nLights = 0;

   _gr_vertices[0].any.x = -2 - _gr_nScreenOriginX +
                           nameX0 - m_textBoxSliderDx;
   _gr_vertices[0].any.y = m_textBoxY - _gr_nScreenOriginY +
                           m_textBoxOffY + mT->dy/2;
   _gr_vertices[0].any.iz = 6550;

   _gr_vertices[1].any.x = -2 - _gr_nScreenOriginX +
                           nameX0;
   _gr_vertices[1].any.y = m_textBoxY - _gr_nScreenOriginY +
                           m_textBoxOffY + mT->dy;
   _gr_vertices[1].any.iz = 6550;

   _gr_vertices[2].any.x = -2 - _gr_nScreenOriginX +
                           nameX0;
   _gr_vertices[2].any.y = m_textBoxY - _gr_nScreenOriginY +
                           m_textBoxOffY;
   _gr_vertices[2].any.iz = 6550;

   GRDrawPolygonPCCW();

   //draw right Slider
   _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
   _gr_polygon.dwAddType = 0;
   _gr_polygon.nVertices = 3;
   _gr_polygon.dwOpacity = 200;

   drL = 0;
   for(i = m_curMission + 1;i < MAX_MISSIONS;i++)
      if (m_mission[i].use) {
         drL = 1;
         break;
      }
   if (drL)
      _gr_polygon.dwColor.color = m_textBoxSlOnColor;
   else
      _gr_polygon.dwColor.color = m_textBoxSlOffColor;

   _gr_polygon.nLights = 0;

   _gr_vertices[0].any.x =  - _gr_nScreenOriginX +
                           nameX1 + m_textBoxSliderDx;
   _gr_vertices[0].any.y = m_textBoxY - _gr_nScreenOriginY +
                           m_textBoxOffY + mT->dy/2;
   _gr_vertices[0].any.iz = 6550;

   _gr_vertices[1].any.x =  - _gr_nScreenOriginX +
                           nameX1;
   _gr_vertices[1].any.y = m_textBoxY - _gr_nScreenOriginY +
                           m_textBoxOffY;
   _gr_vertices[1].any.iz = 6550;

   _gr_vertices[2].any.x =  - _gr_nScreenOriginX +
                           nameX1;
   _gr_vertices[2].any.y = m_textBoxY - _gr_nScreenOriginY +
                           m_textBoxOffY + mT->dy;
   _gr_vertices[2].any.iz = 6550;

   GRDrawPolygonPCCW();

   //draw Text
   GREndScene();

   //mission Name

   mT->font->LUPrintAt(nameX0, m_textBoxY + m_textBoxOffY, mT->name);

   tmp = mT->str;
   for(i = 0;i < mT->textCurrLine;i++) {
      while(*tmp++ != 0);
   }

   drL = Min(m_textBoxStrQnty, mT->textLineQnty - mT->textCurrLine);

   y = mT->y;
   for(i = 0;i < drL;i++) {
      mT->font->LUPrintAt(mT->x, y, tmp);
      y += mT->dy;
      while(*tmp++ != 0);
   }

   //GREnableZBuffer(1);
}
//---------------------------------------------
void DebugMap::Pixel(int x, int y, int color) const{
    GRPset(x, y, color);
}
//---------------------------------------------
void DebugMap::Line(int x0, int y0, int x1, int y1, int color) const{
    GRLine(x0, y0, x1, y1, color);
}
//---------------------------------------------
void DebugMap::Rectangle(int x0, int y0, int x1, int y1, int color) const{
    GRRect(x0, y0, x1, y1, color);
}
//---------------------------------------------
void DebugMap::Circle(int x, int y, int r, int color, int fill)
{
    GRCircle(x, y, r, color, fill);
}

//---------------------------------------------
//---------------------------------------------
