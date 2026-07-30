#include "graph.h"
#include "sd1_epal.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

extern SDeviceList _dL;
extern TExtendedPalette _EPal;
extern unsigned char _currPalette[768];

namespace {

void IgnoreClipUpdate() {}

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << "software-polygon-rasterizer-smoke: " << message << '\n';
  return false;
}

void SetQuad(int left, int top, int right, int bottom, int inverseZ) {
  std::memset(_gr_vertices, 0, sizeof(_gr_vertices));
  _gr_polygon.nVertices = 4;
  _gr_polygon.dwAddType = GR_POLY_ADD_NONE;
  _gr_polygon.dwOpacity = 255;
  _gr_polygon.hTexture = nullptr;
  _gr_polygon.hBump = nullptr;
  _gr_polygon.nLights = 0;

  _gr_vertices[0].any.x = left;
  _gr_vertices[0].any.y = top;
  _gr_vertices[1].any.x = left;
  _gr_vertices[1].any.y = bottom;
  _gr_vertices[2].any.x = right;
  _gr_vertices[2].any.y = bottom;
  _gr_vertices[3].any.x = right;
  _gr_vertices[3].any.y = top;
  for (int index = 0; index < 4; ++index) {
    _gr_vertices[index].any.iz = inverseZ;
  }
}

void SetTextureCoordinates(int maximum) {
  _gr_vertices[0].texture.u = 0;
  _gr_vertices[0].texture.v = 0;
  _gr_vertices[1].texture.u = 0;
  _gr_vertices[1].texture.v = maximum << 16;
  _gr_vertices[2].texture.u = maximum << 16;
  _gr_vertices[2].texture.v = maximum << 16;
  _gr_vertices[3].texture.u = maximum << 16;
  _gr_vertices[3].texture.v = 0;
}

unsigned char& Pixel(std::vector<unsigned char>& screen,
                     int x, int y) {
  return screen[static_cast<std::size_t>(y + 32) * 64 + x + 32];
}

GR_HTEXTURE CreateTexture(unsigned long flags,
                          const unsigned char* pixels) {
  std::vector<unsigned long> storage(3 + 4, 0);
  storage[0] = flags;
  storage[1] = 4;
  storage[2] = 4;
  unsigned char* source = reinterpret_cast<unsigned char*>(storage.data() + 3);
  std::memcpy(source, pixels, 16);
  return GRLoadTextureToDB(nullptr, nullptr, 0, source);
}

bool WriteDitherFixture(const char* path) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  const std::uint16_t header[3] = {64, 64, 512};
  const std::int32_t offset = 513;
  stream.write(reinterpret_cast<const char*>(header), sizeof(header));
  for (int index = 0; index < 64 * 64; ++index) {
    stream.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
  }
  return stream.good();
}

}  // namespace

int main() {
  SDeviceDescr device = {};
  device.swHw = GR_SOFTWARE;
  device.textureFormat[NORMAL_TEXTURE_INDEX].rgbBitCount = 8;
  _dL.currDevice = &device;
  _gr_nScreenWidth = 64;
  _gr_nScreenHeight = 64;
  std::vector<unsigned char> screen(64 * 64, 0);
  _gr_pScreen = screen.data();
  _pGRSetClipRect = IgnoreClipUpdate;

  for (int color = 0; color < 256; ++color) {
    _currPalette[color * 3] = static_cast<unsigned char>(color);
    _currPalette[color * 3 + 1] = static_cast<unsigned char>(color);
    _currPalette[color * 3 + 2] = static_cast<unsigned char>(color);
  }
  epal_Load8BitPal(_EPal, _currPalette, 256);

  CRect2 clip(0, 0, 64, 64);
  SGRViewport* viewport = GRCreateViewport(32, 32, clip);
  if (!Expect(viewport != nullptr, "viewport allocation failed")) {
    return EXIT_FAILURE;
  }
  GRSetViewport(viewport);
  GRSoftwareResetTotalStats();
  if (!Expect(GRSoftwareBeginFrame(5) == TRUE,
              "full-frame clear failed") ||
      !Expect(screen.front() == 5 && screen.back() == 5,
              "full-frame clear retained old pixels")) {
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  SetQuad(-28, -28, -20, -20, 65536);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwColor.color = 77;
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, -24, -24) == 77,
              "flat polygon did not rasterize")) {
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  unsigned char gouraudTable[256 * 16] = {};
  for (int color = 0; color < 256; ++color) {
    for (int layer = 0; layer < 16; ++layer) {
      gouraudTable[color * 16 + layer] =
          static_cast<unsigned char>((color + layer) & 0xff);
    }
  }
  GRSetPaletteTables(nullptr, 0, gouraudTable);
  SetQuad(-18, -28, -10, -20, 65536);
  _gr_polygon.dwFullType = GR_POLY_GOURAUD;
  _gr_polygon.dwColor.color = 20;
  for (int index = 0; index < 4; ++index) {
    _gr_vertices[index].gouraud.b = 3 << 16;
  }
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, -14, -24) == 23,
              "Gouraud palette lookup did not rasterize")) {
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  const unsigned char opaquePixels[16] = {
      1, 2, 3, 4, 5, 6, 7, 8,
      9, 10, 11, 12, 13, 14, 15, 16};
  GR_HTEXTURE opaqueTexture =
      CreateTexture(TEXTURE_MEM_FORMAT, opaquePixels);
  if (!Expect(opaqueTexture != nullptr, "opaque texture allocation failed")) {
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  SetQuad(-8, -28, 0, -20, 65536);
  SetTextureCoordinates(3);
  _gr_polygon.dwFullType = GR_POLY_TEXTURE_LIN;
  _gr_polygon.hTexture = opaqueTexture;
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, -4, -24) == 6,
              "linear textured scanline did not rasterize")) {
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  SetQuad(2, -28, 10, -20, 65536);
  SetTextureCoordinates(3);
  _gr_polygon.dwFullType = GR_POLY_TEXTURE_PERSP;
  _gr_polygon.hTexture = opaqueTexture;
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, 6, -24) == 6,
              "perspective textured scanline did not rasterize")) {
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  const char* ditherPath = "rr2nw-software-dither-smoke.dth";
  if (!Expect(WriteDitherFixture(ditherPath) &&
                  GRSoftwareLoadDitherTable(ditherPath) == TRUE &&
                  GRSoftwareDitherTableReady() == TRUE,
              "software dither table did not load")) {
    std::remove(ditherPath);
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }
  SetQuad(-28, -6, -20, 2, 65536);
  SetTextureCoordinates(3);
  _gr_polygon.dwFullType = GR_POLY_TEXTURE_PERSP;
  _gr_polygon.dwAddType = GR_POLY_ADD_BUMP;
  _gr_polygon.hTexture = opaqueTexture;
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, -24, -2) == 11,
              "dithered perspective texture did not translate source pitch")) {
    GRSoftwareClearDitherTable();
    std::remove(ditherPath);
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }
  GRSoftwareClearDitherTable();
  std::remove(ditherPath);
  _gr_polygon.dwAddType = GR_POLY_ADD_NONE;

  SetQuad(6, -6, 14, 2, 65536);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwAddType = GR_POLY_ADD_BUMP;
  _gr_polygon.dwColor.color = 33;
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, 10, -2) == 33,
              "non-perspective BUMP flag changed the legacy draw dispatch")) {
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }
  _gr_polygon.dwAddType = GR_POLY_ADD_NONE;

  std::vector<unsigned char> lightTable(LIGHT_COLOR_COUNT * 32 * 256, 0);
  for (int lightColor = 0; lightColor < LIGHT_COLOR_COUNT; ++lightColor) {
    for (int layer = 0; layer < 32; ++layer) {
      for (int color = 0; color < 256; ++color) {
        lightTable[(lightColor * 32 + layer) * 256 + color] =
            static_cast<unsigned char>((std::min)(255, color + layer));
      }
    }
  }
  SetMixLightTable(lightTable.data());
  std::memset(&_gr_pLights[0], 0, sizeof(_gr_pLights[0]));
  _gr_pLights[0].type = GR_LIGHT;
  _gr_pLights[0].z = -1.0f;
  _gr_pLights[0].r = 10.0f;
  _gr_pLights[0].power0 = 248;
  _gr_pLights[0].color = LIGHT_COLOR_WHITE;
  SetQuad(-4, -4, 4, 4, 65536);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwAddType = GR_POLY_ADD_LIGHTTHROUGH;
  _gr_polygon.dwColor.color = 10;
  _gr_polygon.a = 0.0f;
  _gr_polygon.b = 0.0f;
  _gr_polygon.c = 1.0f;
  _gr_polygon.d = -1.0f;
  _gr_polygon.nLights = 1;
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, 0, 0) == 41,
              "dynamic light did not use the retail palette mix table")) {
    SetMixLightTable(nullptr);
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }
  SetMixLightTable(nullptr);
  _gr_polygon.nLights = 0;
  _gr_polygon.dwAddType = GR_POLY_ADD_NONE;

  const unsigned char spritePixels[16] = {
      0, 0, 0, 0, 0, 9, 9, 0,
      0, 9, 9, 0, 0, 0, 0, 0};
  GR_HTEXTURE spriteTexture =
      CreateTexture(TEXTURE_MEM_FORMAT | TEXTURE_SPRITE, spritePixels);
  SetQuad(12, -28, 20, -20, 65536);
  SetTextureCoordinates(3);
  _gr_polygon.dwFullType = GR_POLY_SPRITE_PERSP;
  _gr_polygon.hTexture = spriteTexture;
  if (!Expect(spriteTexture != nullptr && GRDrawPolygonPCCW() == TRUE &&
                  Pixel(screen, 12, -28) == 5 &&
                  Pixel(screen, 16, -24) == 9,
              "color-keyed sprite did not preserve transparent pixels")) {
    GRDeleteTextureFromDB(spriteTexture);
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  unsigned char blendTable[16 * 256] = {};
  for (int layer = 0; layer < 16; ++layer) {
    for (int color = 0; color < 256; ++color) {
      blendTable[layer * 256 + color] =
          static_cast<unsigned char>((color + layer) & 0xff);
    }
  }
  const unsigned char alphaPixels[16] = {
      15, 15, 15, 15, 15, 15, 15, 15,
      15, 15, 15, 15, 15, 15, 15, 15};
  GR_HTEXTURE alphaTexture =
      CreateTexture(TEXTURE_MEM_FORMAT | TEXTURE_ALPHA, alphaPixels);
  SetQuad(22, -28, 30, -20, 65536);
  SetTextureCoordinates(3);
  _gr_polygon.dwFullType = GR_POLY_TEXTURE_ALPHA;
  _gr_polygon.hTexture = alphaTexture;
  _gr_polygon.dwColor.color = static_cast<long>(
      reinterpret_cast<std::uintptr_t>(blendTable));
  if (!Expect(alphaTexture != nullptr && GRDrawPolygonPCCW() == TRUE &&
                  Pixel(screen, 26, -24) == 20,
              "alpha texture did not use the transparency table")) {
    GRDeleteTextureFromDB(alphaTexture);
    GRDeleteTextureFromDB(spriteTexture);
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  unsigned char hazeTable[16 * 256] = {};
  for (int layer = 0; layer < 16; ++layer) {
    std::memset(hazeTable + layer * 256, 200 + layer, 256);
  }
  SGRColorDef haze = {hazeTable, 0, 0, 0};
  GRSetHaze(10, 10, &haze);
  GRSetZPrecision(0);
  SetQuad(-28, -16, -20, -8, 3277);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwAddType = GR_POLY_ADD_HAZE;
  _gr_polygon.dwColor.color = 42;
  if (!Expect(GRDrawPolygonPCCW() == TRUE && Pixel(screen, -24, -12) == 200,
              "far polygon did not use the haze table")) {
    GRDeleteTextureFromDB(alphaTexture);
    GRDeleteTextureFromDB(spriteTexture);
    GRDeleteTextureFromDB(opaqueTexture);
    GRReleaseViewport(viewport);
    return EXIT_FAILURE;
  }

  SetQuad(40, 40, 50, 50, 65536);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  if (!Expect(GRDrawPolygonPCCW() == TRUE,
              "fully clipped polygon changed the legacy success contract")) {
    return EXIT_FAILURE;
  }
  SetQuad(-8, -16, 0, -8, 65536);
  _gr_polygon.dwFullType = GR_POLY_TEXTURE_PERSP;
  _gr_polygon.hTexture = nullptr;
  if (!Expect(GRDrawPolygonPCCW() == FALSE,
              "missing texture was not rejected")) {
    return EXIT_FAILURE;
  }
  _gr_polygon.dwFullType = 17;
  if (!Expect(GRDrawPolygonPCCW() == FALSE,
              "unknown polygon type was not rejected")) {
    return EXIT_FAILURE;
  }

  SGRSoftwareRasterStats frame = {};
  SGRSoftwareRasterStats total = {};
  GRSoftwareGetFrameStats(&frame);
  GRSoftwareGetTotalStats(&total);
  const bool telemetryValid =
      frame.frames == 1 && total.frames == 1 && frame.submitted == 13 &&
      frame.accepted == 10 && frame.rasterized == 10 &&
      frame.rejectedOutside == 1 && frame.rejectedTexture == 1 &&
      frame.rejectedUnsupported == 1 && frame.writtenPixels != 0 &&
      frame.hazePixels != 0 && frame.transparentPixels != 0 &&
      frame.ditheredBumpPolygons == 1 &&
      frame.approximatedBumpPolygons == 0 &&
      frame.ignoredNonPerspectiveBumpPolygons == 1 &&
      frame.lightThroughPolygons == 1 &&
      frame.litPolygons == 1 && frame.litPixels != 0 &&
      frame.approximatedLightPolygons == 0 &&
      frame.framebufferHash != 0 && frame.framebufferNonClearPixels != 0 &&
      frame.rasterizedByType[GR_POLY_TEXTURE_PERSP / ADD_TYPE_SIZE] == 2 &&
      frame.rasterizedByType[GR_POLY_TEXTURE_LIN / ADD_TYPE_SIZE] == 1 &&
      frame.rasterizedByType[GR_POLY_SPRITE_PERSP / ADD_TYPE_SIZE] == 1 &&
      frame.rasterizedByType[GR_POLY_TEXTURE_ALPHA / ADD_TYPE_SIZE] == 1;
  if (!Expect(telemetryValid,
              "accepted/rejected polygon telemetry changed")) {
    return EXIT_FAILURE;
  }

  const unsigned long long firstFrameHash = frame.framebufferHash;
  if (!Expect(GRSoftwareBeginFrame(5) == TRUE,
              "second full-frame clear failed")) {
    return EXIT_FAILURE;
  }
  SetQuad(10, 10, 18, 18, 65536);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwColor.color = 91;
  if (!Expect(GRDrawPolygonPCCW() == TRUE,
              "second frame polygon did not rasterize")) {
    return EXIT_FAILURE;
  }
  SetQuad(-16, 10, -8, 18, 65536);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwColor.color = 92;
  if (!Expect(GRDrawPolygonPCCW() == TRUE,
              "left adjacent polygon did not rasterize")) {
    return EXIT_FAILURE;
  }
  SetQuad(-8, 10, 0, 18, 65536);
  _gr_polygon.dwFullType = GR_POLY_FLAT;
  _gr_polygon.dwColor.color = 92;
  if (!Expect(GRDrawPolygonPCCW() == TRUE,
              "right adjacent polygon did not rasterize")) {
    return EXIT_FAILURE;
  }
  bool seamFree = true;
  for (int y = 10; y < 18; ++y) {
    for (int x = -16; x < 0; ++x) {
      seamFree = seamFree && Pixel(screen, x, y) == 92;
    }
  }
  SGRSoftwareRasterStats secondFrame = {};
  GRSoftwareGetFrameStats(&secondFrame);
  if (!Expect(Pixel(screen, -24, -24) == 5,
              "second frame retained a polygon from the previous frame") ||
      !Expect(secondFrame.framebufferHash != firstFrameHash,
              "distinct frames produced the same framebuffer hash") ||
      !Expect(secondFrame.framebufferNonClearPixels != 0,
              "second frame fingerprint reported an empty framebuffer") ||
      !Expect(seamFree,
              "adjacent top-left-rule polygons left a clipping seam")) {
    return EXIT_FAILURE;
  }

  GRDeleteTextureFromDB(alphaTexture);
  GRDeleteTextureFromDB(spriteTexture);
  GRDeleteTextureFromDB(opaqueTexture);
  GRSetPaletteTables(nullptr, 0, nullptr);
  GRSetViewport(nullptr);
  GRReleaseViewport(viewport);
  _gr_pScreen = nullptr;
  _dL.currDevice = nullptr;
  return EXIT_SUCCESS;
}
