#include <time.h>
#include <windows.h>

#include "h/super.h"
#include "kernel/h/echo.h"
#include "kernel/h/session.h"
#include "message/comanmsg.h"
#include "sound.h"
#include "h/cnststr.h"

#define LAST_H__VIEW
#include "game.h"
#include "scene.h"


#include "suavik.h"
#include "hardware.h" // Green
#include "dmap.h"
#include "h/light.h"
#include "h/vehicle.h" // Green
#include "briefing.h"
#include "menu.h"
#include "zav.h"

// Green Globals BEGIN

#ifndef RR2NW_SHELL_GLOBALS_EXTERNAL
#include "ShellGlobals.inl"
#endif
#ifndef RR2NW_DEBUG_MAP_GLOBAL_EXTERNAL
#include "obase/dmap/DebugMapGlobal.inl"
#endif
Menu g_menu;

// Green Globals END 

typedef int TR_RecruitType;

extern CViewScene *pScene;
static HRESULT f_RSXOK =0;

// Green nearAI BEGIN
#include "cmap.h"

//CMap<double> hMap;
//CMap<byte>  g_oMap;



//-------------------------------------------------------------
inline CViewScene *GetScene(){ 
	return(pScene); 
}
//-------------------------------------------------------------
void FilterNearAI_Data(double ){
/*
	int x, y;
	int n;
	double h;
	
	for(y = 0; y < hMap.GetH(); ++y)
	for(x = 0; x < hMap.GetW(); ++x){
		h = hMap(x, y);
		if(fabs(h - hMap(x-1, y-1)) > ht) { g_oMap(x, y) = 1; break; }
		if(fabs(h - hMap(x  , y-1)) > ht) { g_oMap(x, y) = 1; break; }
		if(fabs(h - hMap(x+1, y-1)) > ht) { g_oMap(x, y) = 1; break; }
		if(fabs(h - hMap(x-1, y  )) > ht) { g_oMap(x, y) = 1; break; }
		if(fabs(h - hMap(x+1, y  )) > ht) { g_oMap(x, y) = 1; break; }
		if(fabs(h - hMap(x-1, y+1)) > ht) { g_oMap(x, y) = 1; break; }
		if(fabs(h - hMap(x  , y+1)) > ht) { g_oMap(x, y) = 1; break; }
		if(fabs(h - hMap(x+1, y+1)) > ht) { g_oMap(x, y) = 1; break; }
	}
    for(y = 0; y < g_oMap.GetH(); ++y)
	for(x = 0; x < g_oMap.GetW(); ++x){
		if(g_oMap(x, y)) continue;
		n = 0;
		if(g_oMap(x-1, y-1)) ++n;
		if(g_oMap(x  , y-1)) ++n;
		if(g_oMap(x+1, y-1)) ++n;
		if(g_oMap(x-1, y  )) ++n;
		if(g_oMap(x+1, y  )) ++n;
		if(g_oMap(x-1, y+1)) ++n;
		if(g_oMap(x  , y+1)) ++n;
		if(g_oMap(x+1, y+1)) ++n;
		if(n >= 5) g_oMap(x, y) = 1;
	}
*/
}

void InitNearAI_Data(){
/*
    const double step = 10.0;
    const double barrierH = 5.0;
    const double maxStaticH = 500.0;
    const double vel  = 100.0;
	const double cellInHeightThreshold = 2;
	const double cellHeightThreshold   = 1;
	const int 	 cellBumps = 4;
    int x, y, i, j, l[cellBumps+1];
	double minH, maxH;
    SBumpDef bumpData;
    bool isMapExist;
    const char fName[] = "omap.spr";
    FILE *fh = fopen(fName, "rb");
	double wLine = GetScene()->GetTerrain()->Waterline();

    isMapExist = !(fh == NULL);
    if(isMapExist) fclose(fh);

    if(isMapExist) {
       g_oMap.Read(fName);
       echo("Land obstacles map exist - READED.");
       return;
    }
    else{
       g_oMap.Create(512, 512);
       echo("Creating land map.");
    }
    hMap.Create(512, 512);

    for(y = 0; y < 512; ++y){   // heights evaluate
		for(x = 0; x < 512; ++x){
		   g_oMap(x, y)     = 0;
		   for(j = 0; j < sqrt(cellBumps); ++j)
		   for(i = 0; i < sqrt(cellBumps); ++i){
			   bumpData.start   = CFVector3(x*step + i*step/cellBumps, maxStaticH, -(y*step + j*step/cellBumps));
			   bumpData.fTime   = 10.0;
			   bumpData.fRadius = step/cellBumps;
			   bumpData.vel     = CFVector3(0, -vel, 0);
			   bumpData.fMass   = 1;
			   bumpData.nBumpFlags = 0;
			   GetScene()->Bump(bumpData);
			   l[j*int(sqrt(cellBumps))+i] = vel*bumpData.fTime;
		   }
		   for(i = 0, minH = 1e10, maxH = 0; i < cellBumps; ++i){
			   if(l[i] > maxH) maxH = l[i];
			   if(l[i] < minH) minH = l[i];
		   }
		   ASSERT(maxH >= minH);
		   hMap(x, y) = (maxH+minH)/2;
		   if(maxH-minH > cellInHeightThreshold)	g_oMap(x, y) = 1;  // 
		   if(hMap(x, y) <= wLine+2.01) g_oMap(x, y) = 1;  // water part	!!!!BUG!!!!
		   ASSERT(hMap(x, y) > 0);
		   //if(GetScene()->GetTerrain()->GetRed(x, y) >> 6 < 0x02) g_oMap(x, y) = 1;
		}
		echo("Y == %i", y);
    }
    echo("Land obstacles map created.");
	FilterNearAI_Data(cellHeightThreshold);
	echo("Land obstacles map filtered");
	hMap.Destroy();

    if(!isMapExist) g_oMap.Write(fName);
    echo("Land map saved.");
*/
}
// Green nearAI END

KR_ActiveObject::StateTransitionTableElem a_TObserver::m_table[1] =
         {
          {KR_WAKE_UP, EVENT_IS_IGNORED}
         };

KR_ActiveObject::StateElem a_TObserver::m_state[1] =
         {
          KR_ActiveObject::StateElem( 1, &(a_TObserver::m_table[0]) )
         };



#ifndef RR2NW_TIME_RUNTIME_EXTERNAL
#include "TimeRuntimeState.inl"
#endif

 //-----------------------------------------
void a_TObserver::draw( CDC &gc )
 {
    for( 
         ct_SubjectTable *st = m_arena->findFirstSubjectTable(); 
                          st != NULL; 
                          st = m_arena->findNextSubjectTable( st ) 
       )
         for(
              ct_Subject *subject = st->findFirstSubject(); 
                          subject != NULL ; 
                          subject = st->findNextSubject(subject) 
            )
              subject->draw(gc);
 }

void a_TObserver::loadStateTransitionTable()
 {
    printf("Observer::loadStateTransitionTable");

    m_stateQnty = 1;

    m_stateTable = m_state;
    m_currentState = 0;
 }

void g_enablePortal(const char *portalName);
void Supervisor::startSeance()
 {
   s_ENTRY(CWinGameDlg::startSeance)

//   ::createConsole(this);

   if( SUACRIPT_REGISTER_ERROR_HANDLE(m_suacript) )
   {
        RTCHECK(0,m_suacript.m_heap.m_error.m_msg);
        return;
   }

   g_timer.Start();
   Session::m_realTimer = &g_timer;
   Session::m_hardware  = &g_hardware;
   m_publisher          = new Publisher();

   double e=1;
   str_ConstStr str;
   const char  * cfgStr;

#define GETCFG(n,v) cfgStr = ZAV_Config()("Timer",n); \
    if(  cfgStr!=NULL  )                               \
    {    str.assign(cfgStr);                           \
         if(  str.to(e,0)  )                           \
              v = e;                                   \
    }

   double aspect = 1;
   GETCFG("Speed", g_timer.m_aspect)

   m_context = new SimulationContext(
                                     4000,
                                     5000
                                     );
   m_observer.init( g_arena );

   /*
    * System init
    */
   
   
   m_context->addObject("Hardware", &g_hardware);
   m_context->addObject("Observer", &m_observer );
   m_context->addObject("Publisher",m_publisher);

   m_session.AddObserver(&m_observer);
   m_session.Add(m_context);
   SUA_BindSession(&m_session);

   g_arena.openSeance( m_context, 5120, 5120 );
   
   m_context->addObject("MainMenu", &g_menu);
   m_context->addObject("DebugMap", &g_debugMap);
   m_context->addObject("LevelAttr", &g_levelAttr);
   m_context->addObject("LEVEL",  &m_level);

   if(!m_level.openLevel(m_suacript, g_arena,0))return;
   g_enablePortal("portal");

   
   m_context->addObject("GameConsole", &g_GameConsole);   
   m_context->addObject("Briefing", &g_briefing);
   g_debugMap.Init("level04s.bmp");
   g_GameConsole.Init("Font.fig8x8.fnt", "Font.fnt16x16.fnt", GRTransparentColor(150, 150, 150), 128, 4, NULL);

   //g_menu.Init(GRTransparentColor(50, 50, 50));
   g_menu.Init("../menu.cfg");
   
   KR_ObjectID
   VehicleID = g_arena.getContext()->searchObject("Vehicle.Default");
   g_vehicle = (Vehicle*)(g_arena.getContext()->queryInterface(VehicleID,IVehicleIID));
   
   //m_context->addObject("LEVEL",  &m_level);
   
   g_vehicle->player().findCommander();


   //............................................
//   ct_ClassTableID id = g_arena.findFirstSubjectTableID();
//   while( id!=ct_NULLID )
//   {
//          echo("Founded ClassTable [%s].OK",g_arena.searchSeanceClassTable(id));
//          id = g_arena.findNextSubjectTableID(id);
//   }

   //g_arena.updateAttributes();
   ////////// Tests ////////////////
/*   KR_Event event;
   event.label       = com_EV_PLANE0;
   event.timeStamp   = Session::m_moment;
   event.destination = m_context->searchObject("Mechwarriors");
   event.source      = KR_ObjectID::NUL();
   m_context->sendEventNow( event );

   event.label       = com_EV_PLANE1;
   event.timeStamp   = Session::m_moment;
   event.destination = m_context->searchObject("Tankwarriors");
   m_context->sendEventNow( event );*/


   echo("Supervisor::Seance started");
 }

extern int g_cacheSmokeCnt;

void Supervisor::endSeance()
 {
   SUA_BindSession(NULL);

   m_context->clearObjects();
   m_session.Remove(m_context);
   m_session.RemoveObserver(&m_observer);
   g_cacheSmokeCnt = 0;

   Session::m_realTimer = NULL;

   Session::m_hardware = NULL;

   m_level.closeLevel();
   g_arena.closeSeance();
   g_vehicle = 0;
 }

Supervisor g_super;

void PIN_InitEverything()
{
    CoInitialize(NULL);
    f_RSXOK = InitializeRSX();
}

void SUA_InitEverything()
 {

   InitNearAI_Data(); // Green
   g_super.startSeance();

   //g_hardware.Test(); // fixme Green
 }

void SUA_DeinitEverything()
 {
   g_vehicle->closePanel(Session::m_moment);
   g_super.endSeance();
 }

void PIN_DeInitEverything()
{
   DeInitializeRSX();
   CoUninitialize();
}

void SUA_BeginRender(CViewScene *,CViewDynamicList &list)
 {
/*   if (SUCCEEDED(f_RSXOK))
	PositionListener();*/

    g_arena.render( CViewObject::m_viewPointInvMx.Offset(), 
                    CViewFigure::HazeMax(), list );
    g_lightChain.render();
 }

void SUA_EndRender(CViewScene *pScene)
 {
    g_arena.endRender( pScene );
 }

