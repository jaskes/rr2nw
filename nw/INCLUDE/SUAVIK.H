class	CViewScene;
class	CViewDynamicList;
class	Session;

void SUA_InitEverything();
void SUA_DeinitEverything();
void PIN_InitEverything();
void PIN_DeInitEverything();
void SUA_BeginRender(CViewScene *,CViewDynamicList &);
void SUA_EndRender(CViewScene *);
void SUA_BindSession( Session *session );
void SUA_ProcessEvents();
void SUA_SkipTime( double t );
unsigned int SUA_ClampedTimerSampleCount();
double SUA_ClampedTimerSeconds();

