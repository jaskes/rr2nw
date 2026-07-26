#ifndef RR2NW_FRAME_RUNTIME_STATE_H
#define RR2NW_FRAME_RUNTIME_STATE_H

struct SFMatrix3x4;
typedef const SFMatrix3x4 TCSFMatrix3x4;

class CViewDynamicList;
class CViewScene;

typedef void (*TFrameSuaBeginRender)(CViewScene*, CViewDynamicList&);
typedef void (*TFrameZavRender)(TCSFMatrix3x4*, CViewDynamicList&);
typedef void (*TFrameNoArgumentStep)();
typedef void (*TFrameSuaEndRender)(CViewScene*);

struct SFrameRuntimeHooks {
  TFrameSuaBeginRender beginArenaRender;
  TFrameZavRender renderScene;
  TFrameNoArgumentStep finishGraphicsScene;
  TFrameSuaEndRender endArenaRender;
  TFrameNoArgumentStep releaseFrameScene;
  TFrameNoArgumentStep flushHardwareZList;
};

enum EFrameRuntimeIssue {
  FRAME_RUNTIME_MISSING_SUA_BEGIN = 1u << 0,
  FRAME_RUNTIME_MISSING_ZAV_RENDER = 1u << 1,
  FRAME_RUNTIME_MISSING_GRAPHICS_FINISH = 1u << 2,
  FRAME_RUNTIME_MISSING_SUA_END = 1u << 3,
  FRAME_RUNTIME_MISSING_ZAV_END = 1u << 4,
  FRAME_RUNTIME_MISSING_D3D_FLUSH = 1u << 5,
  FRAME_RUNTIME_NULL_VIEW_DIRECTION = 1u << 6
};

void Frame_ConfigureRuntime(const SFrameRuntimeHooks& hooks);
bool Frame_RuntimeReady(bool hardware);
unsigned int Frame_RuntimeIssues();
void Frame_ClearRuntimeIssues();

void SUA_BeginRender(CViewScene* scene, CViewDynamicList& list);
void ZAV_RenderFrame(TCSFMatrix3x4* direction, CViewDynamicList& list);
void ZAV_PrintFrameInfo();
void SUA_EndRender(CViewScene* scene);
void ZAV_EndRenderFrame();
void D3D_DrawZList();

#endif  // RR2NW_FRAME_RUNTIME_STATE_H
