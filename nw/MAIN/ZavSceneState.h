#ifndef RR2NW_ZAV_SCENE_STATE_H
#define RR2NW_ZAV_SCENE_STATE_H

struct SGRViewport_;
typedef SGRViewport_ SGRViewport;
class CViewScene;

extern CViewScene* pScene;
extern SGRViewport** ppViewports;

SGRViewport* ZAV_Viewport();
CViewScene* ZAV_Scene();

#endif  // RR2NW_ZAV_SCENE_STATE_H
