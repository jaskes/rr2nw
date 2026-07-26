#include <cmath>
#include <cstdlib>
#include <iostream>

#define LAST_H__VIEW
#include "game.h"

extern SDeviceList _dL;
extern float _ikX;
extern float _ikY;
extern float _kX;
extern float _kY;
extern unsigned char* _gr_pHaze;

namespace {

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << "scene-software-state-smoke: " << message << '\n';
  return false;
}

bool Near(double actual, double expected) {
  return std::fabs(actual - expected) < 1e-6;
}

}  // namespace

int main() {
  SDeviceDescr device = {};
  device.swHw = GR_SOFTWARE;
  _dL.currDevice = &device;

  GRSetScale(0.5f, -0.25f);
  if (!Expect(Near(_kX, 0.5) && Near(_kY, -0.25) &&
                  Near(_ikX, 2.0) && Near(_ikY, -4.0),
              "software projection scale was not published")) {
    return EXIT_FAILURE;
  }

  CRect2 clip;
  clip.left = -160;
  clip.top = -100;
  clip.right = 160;
  clip.bottom = 100;
  CViewObject::SetClipRect(clip);
  CViewObject::SetClipPlanes(1.0, 256.0);
  if (!Expect(Near(_gr_fFrontClip, 1.0),
              "front clip plane was not published")) {
    return EXIT_FAILURE;
  }

  unsigned char hazeTable[16 * 256] = {};
  SHazeDef& haze = CPaletteTranslator::Haze(0);
  haze.nHazeMin = 64;
  haze.nHazeDist = 128;
  haze.pTable = hazeTable;
  CViewFigure::SetHaze(haze);
  if (!Expect(Near(CViewFigure::HazeMin(), 64.0) &&
                  Near(CViewFigure::HazeMax(), 192.0),
              "recovered haze range changed") ||
      !Expect(_gr_pHaze == hazeTable,
              "software haze table was not published")) {
    return EXIT_FAILURE;
  }

  CViewObject::SetTerrainCellSize(10.0);
  CFVector3 lightPosition(0.0, 0.0, 0.0);
  CViewObject::SetLight(0, 7, lightPosition, 12.0, 200);
  CLandDynamicMap map;
  map.Create(CVector2(8, 8), 10.0);
  map.SetupLights(1);
  if (!Expect(!map.IsEmpty(),
              "land light mask remained empty after setup")) {
    return EXIT_FAILURE;
  }
  map.RemoveLights(1);
  if (!Expect(map.IsEmpty(),
              "land light mask was not cleared after frame")) {
    return EXIT_FAILURE;
  }

  CViewObject::EnableLights(0);
  CViewObject::BeginDraw();
  CViewDynamicList emptyList;
  emptyList.SortZOrder();

  haze.pTable = 0;
  _dL.currDevice = 0;
  return EXIT_SUCCESS;
}
