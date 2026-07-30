#define LAST_H__SCENE
#include "game.h"

#include "SceneSoftwareDraw.h"

void CViewScene::PromoteDynamic(CViewDynamicList& list, bool willDraw) {
  if (willDraw) {
    CViewFigure::SetWaterline(m_pTerrain->Waterline());
    m_pTerrain->SetViewPoint();
    ASSERT(m_landDynamic.IsEmpty());
    m_landDynamic.SetOrigin(m_pTerrain->ViewpointL());
    m_landDynamic.SetupLights(CViewObject::EnabledLights());
    for (CViewDynamic* dynamic = list.First(); dynamic;
         dynamic = dynamic->Next()) {
      m_landDynamic.ApplyLights(dynamic);
    }
    m_landDynamic.SetupLandSticks(list, *m_pTerrain);
  }
  m_pOrder->LoadDyns(list);
  m_pOrder->PromoteDynamic();
}

void CViewScene::Draw(TCSFMatrix3x4& viewDirection,
                      CViewDynamicList& list) {
  CViewOrdered* viewNode = m_pOrder;
  CViewObject::SetClipRect(_gr_clipRect);

  CViewObject::SetBelowWater(
      -viewDirection.Column(1) * viewDirection.Offset() <
      m_pTerrain->Waterline());

  if (!CViewObject::IsBelowWater()) {
    // The retail software path relied on the sky to touch every pixel.  That
    // assumption turns a single rejected sky polygon into persistent trails
    // in a DIB-backed modern window, so establish a deterministic background
    // before drawing the real sky.
    GRClearScreen(TRUE, CPaletteTranslator::Haze(0).nColor);
    const dword lights = CViewObject::EnabledLights();
    CViewObject::EnableLights(0);
    CFMatrix3x4 skyDirection = viewDirection;
    skyDirection.LoadOffset(CFVector3(0, 0, 0));
    CViewObject::SetViewPoint(skyDirection, m_scale);
    CViewFigure::SetWaterline(1000);
    CViewObject::SetHaze(FALSE);
    CViewObject::SetClipPlanes(CViewObject::m_fFrontClip, 10000);
    CViewObject::m_eWaterView = WATER_VIEW_ALL;
    m_pSkyref->Draw();
    CViewObject::m_eWaterView = WATER_VIEW_ABOVE;
    CViewObject::EnableLights(lights);
  } else {
    GRClearScreen(TRUE, CPaletteTranslator::Haze(1).nColor);
  }

  CViewObject::SetViewPoint(viewDirection, m_scale);
  GRSetScale(static_cast<float>(CViewObject::m_viewPointScale_1.x),
             static_cast<float>(-CViewObject::m_viewPointScale_1.y));

  CViewObject::SetHaze(TRUE);
  CViewFigure::SetHaze(
      CPaletteTranslator::Haze(MAKEBOOL(CViewObject::IsBelowWater())));
  CViewObject::SetClipPlanes(CViewObject::m_fFrontClip,
                             CViewFigure::HazeMax());
  PromoteDynamic(list, TRUE);

  m_pOrder->LoadLights(CViewObject::EnabledLights());
  CViewObject::BeginDraw();
  viewNode->DrawView();
  m_landDynamic.AfterList().ZSortDrawNDrop();
  m_landDynamic.RemoveLights(CViewObject::EnabledLights());
  m_pTerrain->EndDrawTerrain();
}

void SceneSoftwareDraw_Render(CViewScene* scene,
                              TCSFMatrix3x4* direction,
                              CViewDynamicList* list) {
  if (scene != 0 && direction != 0 && list != 0) {
    scene->Draw(*direction, *list);
  }
}
