#ifndef RR2NW_SCENE_SOFTWARE_DRAW_H
#define RR2NW_SCENE_SOFTWARE_DRAW_H

struct SFMatrix3x4;
typedef const SFMatrix3x4 TCSFMatrix3x4;

class CViewDynamicList;
class CViewScene;

void SceneSoftwareDraw_LinkAnchor(CViewScene* scene,
                                  TCSFMatrix3x4* direction,
                                  CViewDynamicList* list);

#endif  // RR2NW_SCENE_SOFTWARE_DRAW_H
