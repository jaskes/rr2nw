#include "graph.h"
#include "sd1_epal.h"

#include <iostream>
#include <vector>

extern SDeviceList _dL;
extern TExtendedPalette _EPal;
extern unsigned char _currPalette[768];

namespace {

int clip_updates = 0;

void CountClipUpdate()
{
  ++clip_updates;
}

bool Expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << '\n';
  return false;
}

}  // namespace

int main() {
  if (!Expect(_gr_nScreenWidth == 320 && _gr_nScreenHeight == 200,
              "graph defaults no longer match the recovered 320x200 state") ||
      !Expect(_gr_hWnd == NULL,
              "graph window handle is not zero-initialized") ||
      !Expect(GRGetViewport() == NULL,
              "graph viewport is not zero-initialized")) {
    return 1;
  }

  SDeviceDescr device = {};
  device.swHw = GR_SOFTWARE;
  device.textureFormat[NORMAL_TEXTURE_INDEX].rgbBitCount = 8;
  _dL.currDevice = &device;
  _pGRSetClipRect = CountClipUpdate;

  for (int color = 0; color < 256; ++color) {
    _currPalette[color * 3] = static_cast<unsigned char>(color);
    _currPalette[color * 3 + 1] = static_cast<unsigned char>(color);
    _currPalette[color * 3 + 2] = static_cast<unsigned char>(color);
  }
  epal_Load8BitPal(_EPal, _currPalette, 256);

  if (!Expect(GRCreateColor(42, 42, 42) == 0x2A2A2A2AUL,
              "8-bit graph color no longer returns palette RGBA bytes")) {
    return 1;
  }

  device.textureFormat[NORMAL_TEXTURE_INDEX].rgbBitCount = 16;
  const int palette_index = epal_Match(_EPal, RGB_i(210, 20, 30));
  const unsigned long expected_color =
      (210UL << 24) | (20UL << 16) | (30UL << 8) |
      static_cast<unsigned long>(palette_index);
  if (!Expect(GRCreateColor(210, 20, 30) == expected_color,
              "true-color graph color no longer preserves RGB and palette index")) {
    return 1;
  }

  std::vector<unsigned char> screen(
      static_cast<std::size_t>(_gr_nScreenWidth * _gr_nScreenHeight));
  _gr_pScreen = screen.data();
  CRect2 clip(12, 18, 92, 68);
  SGRViewport* viewport = GRCreateViewport(10, 15, clip);
  if (!Expect(viewport != NULL, "viewport allocation failed") ||
      !Expect(viewport->clipRect.left == 2 && viewport->clipRect.top == 3 &&
                  viewport->clipRect.right == 82 &&
                  viewport->clipRect.bottom == 53,
              "viewport clip coordinates changed") ||
      !Expect(viewport->pOrigin == screen.data() + 15 * 320 + 10,
              "software viewport origin changed") ||
      !Expect(viewport->pCache[0] == viewport->pOrigin,
              "software viewport row cache changed")) {
    GRReleaseViewport(viewport);
    return 1;
  }

  GRSetViewport(viewport);
  if (!Expect(GRGetViewport() == viewport,
              "active viewport was not retained") ||
      !Expect(_gr_nScreenOriginX == 10 && _gr_nScreenOriginY == 15,
              "active viewport origin was not published") ||
      !Expect(_gr_pOrigin == viewport->pOrigin &&
                  _gr_pYCache == viewport->pCache,
              "software viewport buffers were not published") ||
      !Expect(clip_updates == 1,
              "viewport activation did not update the clip rectangle once")) {
    GRReleaseViewport(viewport);
    return 1;
  }

  GRSetViewport(NULL);
  if (!Expect(GRGetViewport() == NULL,
              "null viewport did not clear the active pointer") ||
      !Expect(clip_updates == 1,
              "null viewport unexpectedly updated the clip rectangle")) {
    GRReleaseViewport(viewport);
    return 1;
  }

  GRReleaseViewport(viewport);
  if (!Expect(_gr_pYCache == NULL,
              "viewport release left the active row cache dangling")) {
    return 1;
  }

  _gr_pScreen = NULL;
  _dL.currDevice = NULL;
  return 0;
}
