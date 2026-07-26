#include <cmath>
#include <cstdlib>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "h/vehicle.h"
#include "ZavSceneState.h"

extern double __terrainWaterline;

namespace {

constexpr double kTolerance = 1e-9;

bool Near(double left, double right) {
  return std::fabs(left - right) < kTolerance;
}

bool NearVector(const CFVector3& vector,
                double x,
                double y,
                double z) {
  return Near(vector.x, x) && Near(vector.y, y) && Near(vector.z, z);
}

int Fail(const char* message) {
  std::cerr << "legacy-view-state-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  static_assert(sizeof(TViewPoint) == sizeof(CFVector3),
                "script viewpoint layout changed");

  if (!NearVector(g_vp[0].vp, 0.0, 0.0, 0.0) ||
      !NearVector(g_vp[39].vp, 0.0, 0.0, 0.0)) {
    return Fail("script viewpoints are not zero-initialized");
  }
  g_vp[39].vp = CFVector3(12.5, -3.0, 77.25);
  if (!NearVector(g_vp[39].vp, 12.5, -3.0, 77.25)) {
    return Fail("script viewpoint write diverged");
  }

  if (!Near(CViewFigure::HazeMin(), 200.0) ||
      !Near(CViewFigure::HazeMax(), 228.0) ||
      !Near(CViewFigure::Waterline(), -1000.0)) {
    return Fail("figure visual defaults changed");
  }
  CViewFigure::SetWaterline(37.5);
  if (!Near(CViewFigure::Waterline(), 37.5)) {
    return Fail("figure waterline write diverged");
  }
  if (!Near(__terrainWaterline, 20.0)) {
    return Fail("terrain waterline default changed");
  }
  double (_CViewTerrain::*waterline_accessor)() = &_CViewTerrain::Waterline;
  if (waterline_accessor == nullptr) {
    return Fail("terrain waterline accessor is not available");
  }

  CFMatrix3x4 identity;
  identity.LoadIdentity();
  CViewObject::SetViewPoint(identity, CFVector3(200.0, -150.0, -1.0));
  if (!NearVector(CViewObject::m_viewPointScale, 200.0, -150.0, -1.0) ||
      !NearVector(CViewObject::m_viewPointScale_1,
                  0.005, -1.0 / 150.0, -1.0) ||
      !NearVector(CViewObject::m_viewPointScale2,
                  40000.0, 22500.0, 1.0) ||
      !Near(CViewObject::m_viewPointInvMx.m[0][0], 1.0) ||
      !Near(CViewObject::m_viewPointInvMx.m[1][1], 1.0) ||
      !Near(CViewObject::m_viewPointInvMx.m[2][2], 1.0) ||
      !Near(CViewObject::m_clipRect0.left, -0.8) ||
      !Near(CViewObject::m_clipRect0.right, 0.8) ||
      !Near(CViewObject::m_clipRect0.top, -2.0 / 3.0) ||
      !Near(CViewObject::m_clipRect0.bottom, 2.0 / 3.0) ||
      !NearVector(CViewObject::m_clipRays[0], -0.8, -2.0 / 3.0, -1.0)) {
    return Fail("viewpoint projection state diverged");
  }

  if (ZAV_Scene() != nullptr || ppViewports != nullptr) {
    return Fail("ZAV pointer state is not zero-initialized");
  }
  CViewScene* const sentinel = reinterpret_cast<CViewScene*>(0x1234);
  pScene = sentinel;
  if (ZAV_Scene() != sentinel) {
    return Fail("ZAV scene owner diverged");
  }
  pScene = nullptr;
  SGRViewport* const viewport_sentinel =
      reinterpret_cast<SGRViewport*>(0x5678);
  SGRViewport* viewport_slots[] = {viewport_sentinel};
  ppViewports = viewport_slots;
  if (ZAV_Viewport() != viewport_sentinel) {
    return Fail("ZAV viewport owner diverged");
  }
  ppViewports = nullptr;

  std::cout << "legacy-view-state-smoke: OK\n";
  return EXIT_SUCCESS;
}
