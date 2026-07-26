#define LAST_H__VIEW
#include "game.h"

#include "FrameRuntimeState.h"
#include "RecoveredSoftwareFrame.h"
#include "SceneSoftwareDraw.h"
#include "ZavSceneState.h"
#include "h/light.h"
#include "kernel/h/session.h"
#include "scene.h"
#include "storage/h/subject.h"

extern int __fullCnt;
extern int __revCnt;

namespace {

void BeginArenaRender(CViewScene*, CViewDynamicList& list) {
  if (Session::m_realTimer == 0) {
    Frame_ReportRuntimeIssue(FRAME_RUNTIME_MISSING_REAL_TIMER);
    return;
  }

  g_arena.render(CViewObject::m_viewPointInvMx.Offset(),
                 CViewFigure::HazeMax(), list);
  g_lightChain.render();
}

void RenderScene(TCSFMatrix3x4* direction, CViewDynamicList& list) {
  SceneSoftwareDraw_Render(pScene, direction, &list);
}

void FinishGraphicsScene() {
  (void)GREndScene();
  __revCnt = 0;
  __fullCnt = 0;
}

void EndArenaRender(CViewScene* scene) {
  g_arena.endRender(scene);
}

void ReleaseFrameScene() {
  CViewDynamicList::WasteBox().Clear(FALSE);
  if (pScene != 0) pScene->CheckDynamicMap();
}

}  // namespace

void Frame_BindRecoveredSoftware() {
  const SFrameRuntimeHooks hooks = {
      BeginArenaRender,
      RenderScene,
      FinishGraphicsScene,
      EndArenaRender,
      ReleaseFrameScene,
      0
  };
  Frame_ConfigureRuntime(hooks);
}
