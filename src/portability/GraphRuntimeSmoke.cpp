#include "graph.h"
#include "sd1_epal.h"

#include <cstring>
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

  unsigned char palette[768];
  std::memcpy(palette, _currPalette, sizeof(palette));
  if (!Expect(GRSetPalette(palette, FALSE) == TRUE,
              "software palette update failed") ||
      !Expect(_gr_logPal.palVersion == 0x300 &&
                  _gr_logPal.palNumEntries == 256,
              "software logical palette metadata changed") ||
      !Expect(GRIsHardware() == FALSE,
              "software device was reported as hardware") ||
      !Expect(GRStartScene() == TRUE && GREndScene() == TRUE &&
                  GRDumpScreen() == TRUE,
              "software scene lifecycle no longer succeeds offscreen")) {
    GRReleaseViewport(viewport);
    return 1;
  }

  if (!Expect(GRClearScreen(TRUE, 9) == TRUE,
              "clipped software clear failed") ||
      !Expect(screen[18 * 320 + 12] == 9 &&
                  screen[17 * 320 + 12] == 0 &&
                  screen[18 * 320 + 11] == 0,
              "software clear escaped the active viewport clip")) {
    GRReleaseViewport(viewport);
    return 1;
  }

  CGRImage image(2, 2);
  image.SetPalette(palette);
  unsigned char imagePixels[] = {1, 2, 3, 4};
  image.LoadPalImage(imagePixels, 0, 0, 0, 0, 1, 1, 2);
  if (!Expect(image.Draw(20, 20) == 1 &&
                  screen[20 * 320 + 20] == 1 &&
                  screen[20 * 320 + 21] == 2 &&
                  screen[21 * 320 + 20] == 3 &&
                  screen[21 * 320 + 21] == 4,
              "opaque software image blit changed") ||
      !Expect(image.DrawX2(30, 20) == 1 &&
                  screen[20 * 320 + 30] == 1 &&
                  screen[20 * 320 + 31] == 1 &&
                  screen[21 * 320 + 30] == 1 &&
                  screen[22 * 320 + 32] == 4 &&
                  screen[23 * 320 + 33] == 4,
              "2x software image blit changed")) {
    GRReleaseViewport(viewport);
    return 1;
  }

  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwAddType = 0;
  _gr_polygon.nVertices = 4;
  _gr_polygon.dwColor.color = 77;
  _gr_polygon.nLights = 0;
  _gr_vertices[0].any.x = 4;
  _gr_vertices[0].any.y = 5;
  _gr_vertices[1].any.x = 4;
  _gr_vertices[1].any.y = 9;
  _gr_vertices[2].any.x = 8;
  _gr_vertices[2].any.y = 9;
  _gr_vertices[3].any.x = 8;
  _gr_vertices[3].any.y = 5;
  if (!Expect(GRDrawPolygonPCCW() == TRUE &&
                  screen[21 * 320 + 15] == 77,
              "flat software shell polygon changed")) {
    GRReleaseViewport(viewport);
    return 1;
  }

  unsigned char transparency[16 * 256];
  for (int layer = 0; layer < 16; ++layer)
    for (int color = 0; color < 256; ++color)
      transparency[layer * 256 + color] =
          static_cast<unsigned char>((color + 1) & 0xFF);
  SGRColorDef transparencyDef = {transparency, 0, 0, 0};
  GRSetPaletteTables(&transparencyDef, 1, NULL);
  _gr_polygon.dwFullType = GR_POLY_TRANSPARENT;
  _gr_polygon.dwOpacity = 128;
  _gr_polygon.dwColor.color = static_cast<long>(
      GRTransparentColor(0, 0, 0));
  screen[26 * 320 + 21] = 7;
  _gr_vertices[0].any.x = 10;
  _gr_vertices[0].any.y = 10;
  _gr_vertices[1].any.x = 10;
  _gr_vertices[1].any.y = 14;
  _gr_vertices[2].any.x = 14;
  _gr_vertices[2].any.y = 14;
  _gr_vertices[3].any.x = 14;
  _gr_vertices[3].any.y = 10;
  if (!Expect(_gr_polygon.dwColor.color != 0 &&
                  GRDrawPolygonPCCW() == TRUE &&
                  screen[26 * 320 + 21] == 8,
              "transparent software shell polygon changed")) {
    GRReleaseViewport(viewport);
    return 1;
  }
  GRSetPaletteTables(NULL, 0, NULL);

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
