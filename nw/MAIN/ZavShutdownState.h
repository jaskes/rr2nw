#ifndef RR2NW_ZAV_SHUTDOWN_STATE_H
#define RR2NW_ZAV_SHUTDOWN_STATE_H

#include "ZavSceneState.h"

typedef void (*TZavReleaseScene)(CViewScene* scene);
typedef void (*TZavClearFigureLibrary)();
typedef void (*TZavReleaseViewport)(SGRViewport* viewport);
typedef void (*TZavFinishGraph)();

struct SZavShutdownHooks {
  TZavReleaseScene releaseScene;
  TZavClearFigureLibrary clearFigureLibrary;
  TZavReleaseViewport releaseViewport;
  TZavFinishGraph endProfile;
  TZavFinishGraph finishGraph;
};

extern unsigned char* pVirtualScreen;

void ZAV_ConfigureShutdown(const SZavShutdownHooks& hooks);
void ZAV_ArmGraphShutdown(int viewportCount);
void ZAV_ArmLevelShutdown();
void ZAV_DeinitLevelResources();
bool ZAV_IsGraphShutdownArmed();
bool ZAV_IsLevelShutdownArmed();
void ZAV_Deinit();

#endif  // RR2NW_ZAV_SHUTDOWN_STATE_H
