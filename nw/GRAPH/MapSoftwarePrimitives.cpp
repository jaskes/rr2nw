#include "graph.h"

#include <algorithm>

namespace {

bool g_enabled = false;

void PutPixel(int x, int y, unsigned long color) {
  if (_gr_pScreen == nullptr || x < 0 || y < 0 ||
      x >= _gr_nScreenWidth || y >= _gr_nScreenHeight)
    return;
  _gr_pScreen[y * _gr_nScreenWidth + x] =
      static_cast<unsigned char>(color);
}

void HorizontalLine(int x0, int y, int x1, unsigned long color) {
  if (y < 0 || y >= _gr_nScreenHeight || x0 > x1) return;
  x0 = (std::max)(x0, 0);
  x1 = (std::min)(x1, _gr_nScreenWidth - 1);
  if (x0 > x1) return;
  unsigned char* destination = _gr_pScreen + y * _gr_nScreenWidth + x0;
  std::fill(destination, destination + (x1 - x0 + 1),
            static_cast<unsigned char>(color));
}

void DrawLine(int x0, int y0, int x1, int y1, unsigned long color) {
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  for (;;) {
    PutPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    const int doubled = error * 2;
    if (doubled >= dy) {
      error += dy;
      x0 += sx;
    }
    if (doubled <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

}  // namespace

int GREnable2D() {
  if (_gr_pScreen == nullptr || _gr_nScreenWidth <= 0 ||
      _gr_nScreenHeight <= 0)
    return 0;
  g_enabled = true;
  return 1;
}

int GRDisable2D() {
  g_enabled = false;
  return 1;
}

void GRPset(int x0, int y0, unsigned long color) {
  if (g_enabled) PutPixel(x0, y0, color);
}

void GRLine(int x0, int y0, int x1, int y1, unsigned long color) {
  if (g_enabled) DrawLine(x0, y0, x1, y1, color);
}

void GRRect(int x0, int y0, int x1, int y1, unsigned long color) {
  if (!g_enabled) return;
  DrawLine(x0, y0, x1, y0, color);
  DrawLine(x1, y0, x1, y1, color);
  DrawLine(x1, y1, x0, y1, color);
  DrawLine(x0, y1, x0, y0, color);
}

void GRBar(int x0, int y0, int x1, int y1, unsigned long color) {
  if (!g_enabled) return;
  if (y0 > y1) std::swap(y0, y1);
  if (x0 > x1) std::swap(x0, x1);
  for (int y = y0; y <= y1; ++y) HorizontalLine(x0, y, x1, color);
}

void GRCircle(int x0, int y0, int radius, unsigned long color, int fill) {
  if (!g_enabled || radius < 0) return;
  int x = radius;
  int y = 0;
  int error = 1 - radius;
  while (x >= y) {
    if (fill) {
      HorizontalLine(x0 - x, y0 + y, x0 + x, color);
      HorizontalLine(x0 - x, y0 - y, x0 + x, color);
      HorizontalLine(x0 - y, y0 + x, x0 + y, color);
      HorizontalLine(x0 - y, y0 - x, x0 + y, color);
    } else {
      PutPixel(x0 + x, y0 + y, color);
      PutPixel(x0 + y, y0 + x, color);
      PutPixel(x0 - y, y0 + x, color);
      PutPixel(x0 - x, y0 + y, color);
      PutPixel(x0 - x, y0 - y, color);
      PutPixel(x0 - y, y0 - x, color);
      PutPixel(x0 + y, y0 - x, color);
      PutPixel(x0 + x, y0 - y, color);
    }
    ++y;
    if (error < 0) {
      error += 2 * y + 1;
    } else {
      --x;
      error += 2 * (y - x) + 1;
    }
  }
}
