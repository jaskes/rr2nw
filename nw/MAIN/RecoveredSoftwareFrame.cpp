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
#include "sound.h"

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

bool Frame_PublishAudioListener() {
  const CFVector3 position = CViewObject::m_viewPointInvMx.Offset();
  const CFVector3 front = CViewObject::m_viewPointInvMx.Column(2);
  const CFVector3 up = CViewObject::m_viewPointInvMx.Column(1);
  const SSoundStateListenerPose listener = {
      static_cast<float>(position.x), static_cast<float>(position.y),
      static_cast<float>(position.z), static_cast<float>(front.x),
      static_cast<float>(front.y), static_cast<float>(front.z),
      static_cast<float>(up.x), static_cast<float>(up.y),
      static_cast<float>(up.z)};
  return SoundState_SetListener(&listener);
}
