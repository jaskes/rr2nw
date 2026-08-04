#include "graph.h"

#include <algorithm>
#include <cmath>

extern int __HazeStartInt;

namespace {

void SetVertex(int index, int x, int y, int inverseZ) {
  _gr_vertices[index].any.x = x;
  _gr_vertices[index].any.y = y;
  _gr_vertices[index].any.iz = inverseZ;
}

void ConfigureRayPolygon(int vertices, unsigned long color, int opacity,
                         int inverseZ) {
  _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
  _gr_polygon.nVertices = vertices;
  _gr_polygon.hTexture = NULL;
  _gr_polygon.dwColor.color = static_cast<long>(color);
  _gr_polygon.dwOpacity = (std::max)(0, (std::min)(255, opacity));
  _gr_polygon.dwAddType =
      inverseZ < __HazeStartInt ? GR_POLY_ADD_HAZE : GR_POLY_ADD_NONE;
}

}  // namespace

// Recovered software counterpart of the retail ray primitive.  The old
// implementation also owned a Direct3D texture and therefore could not be
// linked into the modern software-only runtime.  Artefact coronas already
// provide the soft halo; this keeps the two tapered transparent beam segments
// and the legacy palette blend table without reviving the removed D3D path.
extern "C" void GRDrawRay(int x0, int y0, int x1, int y1,
                          unsigned long color, int opacity, int inverseZ,
                          float width, float center) {
  const double dx = static_cast<double>(x1 - x0);
  const double dy = static_cast<double>(y1 - y0);
  const double lengthSquared = dx * dx + dy * dy;
  if (color == 0 || !std::isfinite(lengthSquared) ||
      lengthSquared <= 1.0e-12 || !std::isfinite(width) ||
      !std::isfinite(center))
    return;

  center = (std::max)(0.0f, (std::min)(1.0f, center));
  width = std::fabs(width);
  const double inverseLength = 1.0 / std::sqrt(lengthSquared);
  const double sine = dy * inverseLength;
  const double cosine = dx * inverseLength;
  const double middleX = static_cast<double>(x0) + dx * center;
  const double middleY = static_cast<double>(y0) + dy * center;
  const double middleWidth = static_cast<double>(width) * center;
  const int middleLeftX = static_cast<int>(middleX - middleWidth * sine);
  const int middleLeftY = static_cast<int>(middleY + middleWidth * cosine);
  const int middleRightX = static_cast<int>(middleX + middleWidth * sine);
  const int middleRightY = static_cast<int>(middleY - middleWidth * cosine);

  GRSetZPrecision(0);
  ConfigureRayPolygon(3, color, opacity, inverseZ);
  SetVertex(0, x0, y0, inverseZ);
  SetVertex(1, middleLeftX, middleLeftY, inverseZ);
  SetVertex(2, middleRightX, middleRightY, inverseZ);
  GRDrawPolygonPCCW();

  const double endWidthX = static_cast<double>(width) * sine;
  const double endWidthY = static_cast<double>(width) * cosine;
  ConfigureRayPolygon(4, color, opacity, inverseZ);
  SetVertex(0, middleRightX, middleRightY, inverseZ);
  SetVertex(1, middleLeftX, middleLeftY, inverseZ);
  SetVertex(2, static_cast<int>(static_cast<double>(x1) - endWidthX),
            static_cast<int>(static_cast<double>(y1) + endWidthY), inverseZ);
  SetVertex(3, static_cast<int>(static_cast<double>(x1) + endWidthX),
            static_cast<int>(static_cast<double>(y1) - endWidthY), inverseZ);
  GRDrawPolygonPCCW();
}
