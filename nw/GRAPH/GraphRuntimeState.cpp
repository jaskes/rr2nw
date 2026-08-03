#include "graph.h"
#include "GraphSoftwareTextureInternal.h"
#include "sd1_epal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

// A retail scene sends roughly one million pixels per frame through this
// recovered software scanline rasterizer. MSVC Debug's translation-unit-wide
// /Od and /RTC1 turn that isolated inner loop into a frame-pacing bottleneck
// and repeatedly trip the engine's 50 ms anti-stall clock clamp. Keep ordinary
// Debug semantics throughout the game, but let this leaf renderer retain its
// symbols while using speed-oriented code. Release already receives its
// optimization policy from CMake.
#if defined(_MSC_VER) && defined(_DEBUG)
#pragma runtime_checks("", off)
#pragma optimize("gty", on)
#define RR2NW_HOT_INLINE __forceinline
#else
#define RR2NW_HOT_INLINE inline
#endif

extern "C" {
unsigned char *_gr_pScreen = NULL;
unsigned char *_gr_pOrigin = NULL;
unsigned char **_gr_pYCache = NULL;
unsigned char *_gr_pHaze = NULL;
SGRColorDef *_gr_pTransparency = NULL;
int _gr_nTranspCount = 0;
unsigned char *_gr_pGouraud = NULL;
long _gr_pDiserTable[64*64] = {};

int _gr_nScreenWidth = 320;
int _gr_nScreenHeight = 200;
int _gr_nScreenOriginX = 0;
int _gr_nScreenOriginY = 0;
CRect2 _gr_clipRect;
SGRViewport *_Viewport = NULL;

float _gr_fFrontClip = 1.0f;
volatile int _gr_bRestoreSurf = 0;

HWND _gr_hWnd = NULL;
HDC _gr_hDC = NULL;
HPALETTE _gr_hPal = NULL;
GR_BITMAPINFO _gr_DIBInfo = {};
GR_LOGPALETTE _gr_logPal = {};
POINTS _gr_windowPos = {0, 0};

UGRVertex _gr_vertices[GR_MAX_VERTEX] = {};
SGRPolygon _gr_polygon = {};
SGRLight _gr_pLights[LIGHT_SOURCE_COUNT] = {};
}

TExtendedPalette _EPal;
TExtendedPalette _ETransparencyPal;
unsigned char _currPalette[768];
SDeviceList _dL = {NULL, -1, NULL, 0, 0, NULL};

int _rScale = 0;
int _rShift = 0;
int _gScale = 0;
int _gShift = 0;
int _bScale = 0;
int _bShift = 0;

float _kX = 0.01f;
float _kY = 0.01f;
float _kXX = 0.0001f;
float _kYY = 0.0001f;
float _kXY = 0.0001f;
float _ikX = 100.0f;
float _ikY = 100.0f;

int __HazeStartInt = 0;
float __HazeLen = 0.0f;

void (*_pGRSetClipRect)(void) = NULL;

namespace {

using rr2nw_software::SoftwareTexture;
using rr2nw_software::TextureFromHandle;

constexpr double kFixed16Scale = 65536.0;
constexpr double kSpanEpsilon = 1.0e-9;

struct ClipBounds {
  int left;
  int top;
  int right;
  int bottom;
};

struct RasterVertex {
  double x;
  double y;
  double inverseZ;
  double u;
  double v;
  double uInverseZ;
  double vInverseZ;
  double shade;
  double red;
  double green;
  double blue;
};

struct EdgeSample {
  double x;
  double inverseZ;
  double u;
  double v;
  double uInverseZ;
  double vInverseZ;
  double shade;
  double red;
  double green;
  double blue;
};

SGRSoftwareRasterStats g_frameRasterStats = {};
SGRSoftwareRasterStats g_totalRasterStats = {};
int g_zPrecision = 0;
int g_hazeStart = 0;
int g_hazeLength = 0;
unsigned char g_frameClearColor = 0;
bool g_ditherTableReady = false;
int g_ditherTextureStride = 0;
unsigned char g_lightMixTable[LIGHT_COLOR_COUNT*32*256] = {};
bool g_lightMixTableReady = false;

void AddStat(unsigned long long SGRSoftwareRasterStats::*member,
             unsigned long long value = 1) {
  g_frameRasterStats.*member += value;
  g_totalRasterStats.*member += value;
}

int PolygonTypeIndex(long fullType) {
  if (fullType < 0 || fullType % ADD_TYPE_SIZE != 0) return -1;
  const long index = fullType / ADD_TYPE_SIZE;
  return index >= 0 && index < TYPE_COUNT ? static_cast<int>(index) : -1;
}

void AddSubmittedType(int type) {
  if (type < 0) return;
  ++g_frameRasterStats.submittedByType[type];
  ++g_totalRasterStats.submittedByType[type];
}

void AddAcceptedType(int type) {
  ++g_frameRasterStats.acceptedByType[type];
  ++g_totalRasterStats.acceptedByType[type];
}

void AddRasterizedType(int type) {
  ++g_frameRasterStats.rasterizedByType[type];
  ++g_totalRasterStats.rasterizedByType[type];
}

void PopulateFramebufferFingerprint(SGRSoftwareRasterStats* stats) {
  stats->framebufferHash = 0;
  stats->framebufferNonClearPixels = 0;
  if (_gr_pScreen == NULL || _gr_nScreenWidth <= 0 ||
      _gr_nScreenHeight <= 0) {
    return;
  }

  constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
  constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);
  std::uint64_t hash = kFnvOffset;
  const std::size_t size = static_cast<std::size_t>(_gr_nScreenWidth) *
                           _gr_nScreenHeight;
  for (std::size_t index = 0; index < size; ++index) {
    const unsigned char value = _gr_pScreen[index];
    hash ^= value;
    hash *= kFnvPrime;
    if (value != g_frameClearColor) {
      ++stats->framebufferNonClearPixels;
    }
  }
  stats->framebufferHash = hash;
}

bool GetClipBounds(ClipBounds* bounds) {
  bounds->left = static_cast<int>(_gr_clipRect.left);
  bounds->top = static_cast<int>(_gr_clipRect.top);
  bounds->right = static_cast<int>(_gr_clipRect.right);
  bounds->bottom = static_cast<int>(_gr_clipRect.bottom);
  if (bounds->right <= bounds->left || bounds->bottom <= bounds->top) {
    bounds->left = -_gr_nScreenOriginX;
    bounds->top = -_gr_nScreenOriginY;
    bounds->right = _gr_nScreenWidth - _gr_nScreenOriginX;
    bounds->bottom = _gr_nScreenHeight - _gr_nScreenOriginY;
  }
  bounds->left = (std::max)(bounds->left, -_gr_nScreenOriginX);
  bounds->top = (std::max)(bounds->top, -_gr_nScreenOriginY);
  bounds->right = (std::min)(
      bounds->right, _gr_nScreenWidth - _gr_nScreenOriginX);
  bounds->bottom = (std::min)(
      bounds->bottom, _gr_nScreenHeight - _gr_nScreenOriginY);
  return bounds->right > bounds->left && bounds->bottom > bounds->top;
}

bool IsTexturedType(int type) {
  return type == GR_POLY_TEXTURE_PERSP / ADD_TYPE_SIZE ||
         type == GR_POLY_TEXTURE_LIN / ADD_TYPE_SIZE ||
         type == GR_POLY_SPRITE_PERSP / ADD_TYPE_SIZE ||
         type == GR_POLY_TEXTURE_ALPHA / ADD_TYPE_SIZE ||
         type == GR_POLY_SPRITE_LIN / ADD_TYPE_SIZE ||
         type == GR_POLY_TEXTURE_SMP / ADD_TYPE_SIZE ||
         type == GR_POLY_SPRITE_MIP / ADD_TYPE_SIZE ||
         type == GR_POLY_TEXTURE_GOURAUD / ADD_TYPE_SIZE;
}

bool IsPerspectiveType(int type) {
  return type == GR_POLY_TEXTURE_PERSP / ADD_TYPE_SIZE ||
         type == GR_POLY_SPRITE_PERSP / ADD_TYPE_SIZE ||
         type == GR_POLY_TEXTURE_ALPHA / ADD_TYPE_SIZE ||
         type == GR_POLY_TEXTURE_SMP / ADD_TYPE_SIZE ||
         type == GR_POLY_SPRITE_MIP / ADD_TYPE_SIZE ||
         type == GR_POLY_TEXTURE_GOURAUD / ADD_TYPE_SIZE;
}

bool IsSpriteType(int type) {
  return type == GR_POLY_SPRITE_PERSP / ADD_TYPE_SIZE ||
         type == GR_POLY_SPRITE_LIN / ADD_TYPE_SIZE ||
         type == GR_POLY_SPRITE_MIP / ADD_TYPE_SIZE;
}

double FixedGouraudShade(const UGRVertex& vertex) {
  return static_cast<double>(vertex.gouraud.b) / kFixed16Scale;
}

double TextureGouraudShade(const UGRVertex& vertex) {
  const std::uint32_t raw = static_cast<std::uint32_t>(vertex.gouraud.b);
  return static_cast<double>((raw >> 8) & 0xffU) * 15.0 / 255.0;
}

RasterVertex BuildRasterVertex(const UGRVertex& vertex, int type) {
  RasterVertex result = {};
  result.x = static_cast<double>(vertex.any.x);
  result.y = static_cast<double>(vertex.any.y);
  result.inverseZ = static_cast<double>(vertex.any.iz);
  result.u = static_cast<double>(vertex.texture.u) / kFixed16Scale;
  result.v = static_cast<double>(vertex.texture.v) / kFixed16Scale;
  result.uInverseZ = result.u * result.inverseZ;
  result.vInverseZ = result.v * result.inverseZ;
  if (type == GR_POLY_GOURAUD / ADD_TYPE_SIZE) {
    result.shade = FixedGouraudShade(vertex);
  } else if (type == GR_POLY_TEXTURE_GOURAUD / ADD_TYPE_SIZE) {
    result.shade = TextureGouraudShade(vertex);
  }
  if (type == GR_POLY_GOURAUD_RGB / ADD_TYPE_SIZE) {
    const std::uint32_t color =
        static_cast<std::uint32_t>(vertex.gouraud.b);
    result.red = static_cast<double>((color >> 16) & 0xffU);
    result.green = static_cast<double>((color >> 8) & 0xffU);
    result.blue = static_cast<double>(color & 0xffU);
  }
  return result;
}

RR2NW_HOT_INLINE double Lerp(double first, double second, double ratio) {
  return first + (second - first) * ratio;
}

EdgeSample InterpolateEdge(const RasterVertex& first,
                           const RasterVertex& second, double ratio) {
  EdgeSample result = {};
  result.x = Lerp(first.x, second.x, ratio);
  result.inverseZ = Lerp(first.inverseZ, second.inverseZ, ratio);
  result.u = Lerp(first.u, second.u, ratio);
  result.v = Lerp(first.v, second.v, ratio);
  result.uInverseZ =
      Lerp(first.uInverseZ, second.uInverseZ, ratio);
  result.vInverseZ =
      Lerp(first.vInverseZ, second.vInverseZ, ratio);
  result.shade = Lerp(first.shade, second.shade, ratio);
  result.red = Lerp(first.red, second.red, ratio);
  result.green = Lerp(first.green, second.green, ratio);
  result.blue = Lerp(first.blue, second.blue, ratio);
  return result;
}

RR2NW_HOT_INLINE int ClampInt(int value, int minimum, int maximum) {
  return (std::max)(minimum, (std::min)(value, maximum));
}

RR2NW_HOT_INLINE unsigned char SampleTexture(
    const SoftwareTexture& texture, double u, double v, bool dither) {
  const auto coordinate = [](double value, long dimension) {
    if (!std::isfinite(value) || value <= 0.0) return 0;
    const double maximum = static_cast<double>(dimension - 1);
    if (value >= maximum) return static_cast<int>(dimension - 1);
    return static_cast<int>(std::floor(value));
  };
  const int x = coordinate(u, texture.w);
  const int y = coordinate(v, texture.h);
  const std::size_t size = static_cast<std::size_t>(texture.w) * texture.h;
  std::size_t index = static_cast<std::size_t>(y) * texture.w + x;
  if (dither && g_ditherTableReady) {
    const double uFraction = u - std::floor(u);
    const double vFraction = v - std::floor(v);
    const int uIndex = ClampInt(
        static_cast<int>(std::floor(uFraction * 64.0)), 0, 63);
    const int vIndex = ClampInt(
        static_cast<int>(std::floor(vFraction * 64.0)), 0, 63);
    const long sourceOffset = _gr_pDiserTable[uIndex * 64 + vIndex];
    // DITH.DTH stores a neighbouring texel as dy * sourcePitch + dx.  The
    // recovered texture owner keeps each image tightly packed, so translate
    // that vector to the texture's actual row pitch instead of requiring the
    // retail table pitch (512) as the texture width.
    const long rowOffset = sourceOffset / g_ditherTextureStride;
    const long columnOffset =
        sourceOffset - rowOffset * g_ditherTextureStride;
    const long long shifted = static_cast<long long>(index) +
        static_cast<long long>(rowOffset) * texture.w + columnOffset;
    if (shifted >= 0 && static_cast<unsigned long long>(shifted) < size) {
      index = static_cast<std::size_t>(shifted);
    }
  }
  return texture.dataPtr[index];
}

RR2NW_HOT_INLINE unsigned char ApplyGouraud(
    unsigned char color, double shade) {
  if (_gr_pGouraud == NULL) return color;
  const int layer = ClampInt(static_cast<int>(std::floor(shade + 0.5)),
                             0, 15);
  return _gr_pGouraud[static_cast<unsigned int>(color) * 16U + layer];
}

RR2NW_HOT_INLINE unsigned char ApplyHaze(
    unsigned char color, double inverseZ) {
  if (_gr_pHaze == NULL || g_hazeStart <= 0 || g_hazeLength <= 0 ||
      inverseZ <= 0.0) {
    return color;
  }
  const double depthScale = std::ldexp(kFixed16Scale, g_zPrecision);
  const double distance = depthScale / inverseZ;
  if (distance <= g_hazeStart) return color;

  const double hazeEnd = static_cast<double>(g_hazeStart + g_hazeLength);
  int layer = 0;
  if (distance < hazeEnd) {
    layer = ClampInt(static_cast<int>(std::floor(
                         (hazeEnd - distance) * 16.0 / g_hazeLength)),
                     0, 15);
  }
  return _gr_pHaze[layer * 256 + color];
}

struct PreparedLight {
  const unsigned char* mixTable;
  double fixedBase;
  double numeratorA;
  double numeratorB0;
  double numeratorB1;
  double numeratorC2;
  double numeratorC1;
  double numeratorC0;
  double denominatorA;
  double denominatorB0;
  double denominatorB1;
  double denominatorC2;
  double denominatorC1;
  double denominatorC0;
};

int PrepareDynamicLights(bool lightThrough, PreparedLight* prepared) {
  if (!g_lightMixTableReady || _gr_polygon.nLights == 0) return 0;

  int count = 0;
  unsigned long mask = _gr_polygon.nLights;
  for (int index = 0; index < LIGHT_SOURCE_COUNT && mask != 0;
       ++index, mask >>= 1) {
    if ((mask & 1UL) == 0) continue;
    const SGRLight& light = _gr_pLights[index];
    if (light.type != GR_LIGHT || !std::isfinite(light.x) ||
        !std::isfinite(light.y) || !std::isfinite(light.z) ||
        !std::isfinite(light.r) || light.r <= 0.0f ||
        light.color < 0 || light.color >= LIGHT_COLOR_COUNT) {
      continue;
    }

    double planeDistance =
        light.x * _gr_polygon.a + light.y * _gr_polygon.b +
        light.z * _gr_polygon.c - _gr_polygon.d;
    if (lightThrough) planeDistance = std::fabs(planeDistance);
    if (planeDistance < 0.0 || planeDistance > light.r) continue;

    const int maximum = ClampInt(light.power0 >> 3, 0, 31);
    if (maximum == 0) continue;
    const double sourceLengthSquared =
        static_cast<double>(light.x) * light.x +
        static_cast<double>(light.y) * light.y +
        static_cast<double>(light.z) * light.z;
    PreparedLight& result = prepared[count++];
    result.mixTable = g_lightMixTable + light.color * 32 * 256;
    result.fixedBase = -maximum * kFixed16Scale;
    const double lightScale = result.fixedBase /
                              (static_cast<double>(light.r) * light.r);
    const double a = _gr_polygon.a;
    const double b = _gr_polygon.b;
    const double c = _gr_polygon.c;
    const double d = _gr_polygon.d;
    result.numeratorA =
        (a * a * sourceLengthSquared + d * (d - 2.0 * a * light.x)) *
        _kXX * lightScale;
    result.numeratorB0 =
        -2.0 * (a * b * sourceLengthSquared -
                d * (b * light.x + a * light.y)) *
        _kXY * lightScale;
    result.numeratorB1 =
        -2.0 * (a * c * sourceLengthSquared -
                d * (c * light.x + a * light.z)) *
        _kX * lightScale;
    result.numeratorC2 =
        (b * b * sourceLengthSquared + d * (d - 2.0 * b * light.y)) *
        _kYY * lightScale;
    result.numeratorC1 =
        2.0 * (c * b * sourceLengthSquared -
               d * (c * light.y + b * light.z)) *
        _kY * lightScale;
    result.numeratorC0 =
        (c * c * sourceLengthSquared + d * (d - 2.0 * c * light.z)) *
        lightScale;
    result.denominatorA = a * a * _kXX;
    result.denominatorB0 = -2.0 * a * b * _kXY;
    result.denominatorB1 = -2.0 * a * c * _kX;
    result.denominatorC2 = b * b * _kYY;
    result.denominatorC1 = 2.0 * b * c * _kY;
    result.denominatorC0 = c * c;
  }
  return count;
}

RR2NW_HOT_INLINE unsigned char ApplyDynamicLights(
    unsigned char color, int x, int y, const PreparedLight* lights,
    int lightCount, unsigned long long* applications) {
  const double screenX = static_cast<double>(x);
  const double screenY = static_cast<double>(y);
  for (int index = 0; index < lightCount; ++index) {
    const PreparedLight& light = lights[index];
    const double numerator =
        light.numeratorA * screenX * screenX +
        (light.numeratorB0 * screenY + light.numeratorB1) * screenX +
        light.numeratorC2 * screenY * screenY +
        light.numeratorC1 * screenY + light.numeratorC0;
    const double denominator =
        light.denominatorA * screenX * screenX +
        (light.denominatorB0 * screenY + light.denominatorB1) * screenX +
        light.denominatorC2 * screenY * screenY +
        light.denominatorC1 * screenY + light.denominatorC0;
    if (!std::isfinite(numerator) || !std::isfinite(denominator) ||
        std::fabs(denominator) <= kSpanEpsilon) {
      continue;
    }
    const double fixedLayer = numerator / denominator - light.fixedBase;
    if (!std::isfinite(fixedLayer) || fixedLayer <= 0.0) continue;
    const int layer = ClampInt(static_cast<int>(std::floor(
                                   fixedLayer / kFixed16Scale + 0.5)),
                               0, 31);
    if (layer == 0) continue;
    color = light.mixTable[layer * 256 + color];
    ++(*applications);
  }
  return color;
}

void SetSoftwareZPrecision(int precision) {
  g_zPrecision = ClampInt(precision, -16, 14);
}

int DrawSoftwarePolygon() {
  AddStat(&SGRSoftwareRasterStats::submitted);
  const int type = PolygonTypeIndex(_gr_polygon.dwFullType);
  AddSubmittedType(type);

  if (_dL.currDevice == NULL ||
      _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL ||
      _gr_nScreenWidth <= 0 || _gr_nScreenHeight <= 0 ||
      _gr_polygon.nVertices < 3 ||
      _gr_polygon.nVertices > GR_MAX_VERTEX) {
    AddStat(&SGRSoftwareRasterStats::rejectedInvalid);
    return FALSE;
  }
  if (type < 0) {
    AddStat(&SGRSoftwareRasterStats::rejectedUnsupported);
    return FALSE;
  }

  ClipBounds clip = {};
  if (!GetClipBounds(&clip)) {
    AddStat(&SGRSoftwareRasterStats::rejectedInvalid);
    return FALSE;
  }

  const bool textured = IsTexturedType(type);
  const bool perspective = IsPerspectiveType(type);
  const bool sprite = IsSpriteType(type);
  const bool alpha = type == GR_POLY_TEXTURE_ALPHA / ADD_TYPE_SIZE;
  const bool gouraud = type == GR_POLY_GOURAUD / ADD_TYPE_SIZE;
  const bool gouraudRgb = type == GR_POLY_GOURAUD_RGB / ADD_TYPE_SIZE;
  const bool textureGouraud =
      type == GR_POLY_TEXTURE_GOURAUD / ADD_TYPE_SIZE;
  const bool flatTransparent =
      type == GR_POLY_TRANSPARENT / ADD_TYPE_SIZE;
  SoftwareTexture* texture = NULL;
  if (textured) {
    texture = TextureFromHandle(_gr_polygon.hTexture);
    if (texture == NULL || texture->dataPtr == NULL || texture->w <= 0 ||
        texture->h <= 0) {
      AddStat(&SGRSoftwareRasterStats::rejectedTexture);
      return FALSE;
    }
  }
  const bool bumpRequested =
      (_gr_polygon.dwAddType & GR_POLY_ADD_BUMP) != 0;
  // The original draw dispatch routes BUMP only for TEXTURE_P_TYPE to
  // ADrawDiserTexture32.  Every other polygon type keeps its ordinary draw
  // routine even when the add-type bit is present.
  const bool bumpDitherEligible =
      bumpRequested && type == GR_POLY_TEXTURE_PERSP / ADD_TYPE_SIZE;
  const bool ditheredBump =
      bumpDitherEligible &&
      g_ditherTableReady && g_ditherTextureStride > 0 && texture != NULL;
  PreparedLight preparedLights[LIGHT_SOURCE_COUNT] = {};
  const int preparedLightCount = PrepareDynamicLights(
      (_gr_polygon.dwAddType & GR_POLY_ADD_LIGHTTHROUGH) != 0,
      preparedLights);
  const bool dynamicLighting = preparedLightCount != 0;

  const unsigned char* blendTable = NULL;
  if (flatTransparent || alpha) {
    const std::uintptr_t address = static_cast<std::uintptr_t>(
        static_cast<unsigned long>(_gr_polygon.dwColor.color));
    if (address == 0) {
      AddStat(&SGRSoftwareRasterStats::rejectedInvalid);
      return FALSE;
    }
    blendTable = reinterpret_cast<const unsigned char*>(address);
    if (flatTransparent) {
      const int opacity = ClampInt(_gr_polygon.dwOpacity >> 4, 0, 15);
      blendTable += opacity * 256;
    }
  }

  RasterVertex vertices[GR_MAX_VERTEX] = {};
  double minimumX = static_cast<double>(_gr_vertices[0].any.x);
  double maximumX = minimumX;
  double minimumY = static_cast<double>(_gr_vertices[0].any.y);
  double maximumY = minimumY;
  for (int index = 0; index < _gr_polygon.nVertices; ++index) {
    vertices[index] = BuildRasterVertex(_gr_vertices[index], type);
    minimumX = (std::min)(minimumX, vertices[index].x);
    maximumX = (std::max)(maximumX, vertices[index].x);
    minimumY = (std::min)(minimumY, vertices[index].y);
    maximumY = (std::max)(maximumY, vertices[index].y);
    if ((perspective || (_gr_polygon.dwAddType & GR_POLY_ADD_HAZE)) &&
        vertices[index].inverseZ <= 0.0) {
      AddStat(&SGRSoftwareRasterStats::rejectedInvalid);
      return FALSE;
    }
  }

  if (maximumX <= clip.left || minimumX >= clip.right ||
      maximumY <= clip.top || minimumY >= clip.bottom) {
    AddStat(&SGRSoftwareRasterStats::rejectedOutside);
    return TRUE;
  }

  AddStat(&SGRSoftwareRasterStats::accepted);
  AddAcceptedType(type);
  if (bumpRequested) {
    if (ditheredBump) {
      AddStat(&SGRSoftwareRasterStats::ditheredBumpPolygons);
    } else if (bumpDitherEligible) {
      AddStat(&SGRSoftwareRasterStats::approximatedBumpPolygons);
    } else {
      AddStat(
          &SGRSoftwareRasterStats::ignoredNonPerspectiveBumpPolygons);
    }
  }
  if ((_gr_polygon.dwAddType & GR_POLY_ADD_LIGHTTHROUGH) != 0) {
    AddStat(&SGRSoftwareRasterStats::lightThroughPolygons);
  }
  if (dynamicLighting) {
    AddStat(&SGRSoftwareRasterStats::litPolygons);
  } else if (_gr_polygon.nLights != 0 && !g_lightMixTableReady) {
    AddStat(&SGRSoftwareRasterStats::approximatedLightPolygons);
  }

  int firstY = static_cast<int>(std::ceil(minimumY - 0.5));
  int endY = static_cast<int>(std::ceil(maximumY - 0.5));
  firstY = (std::max)(firstY, clip.top);
  endY = (std::min)(endY, clip.bottom);

  const bool useHaze =
      (_gr_polygon.dwAddType & GR_POLY_ADD_HAZE) != 0;
  const unsigned char flatColor =
      static_cast<unsigned char>(_gr_polygon.dwColor.color & 0xff);
  const int polygonOpacity = ClampInt(_gr_polygon.dwOpacity >> 4, 0, 15);
  unsigned long long covered = 0;
  unsigned long long written = 0;
  unsigned long long hazed = 0;
  unsigned long long transparentWrites = 0;
  unsigned long long lightApplications = 0;

  EdgeSample intersections[GR_MAX_VERTEX] = {};
  for (int y = firstY; y < endY; ++y) {
    const double scanY = static_cast<double>(y) + 0.5;
    int intersectionCount = 0;
    for (int index = 0; index < _gr_polygon.nVertices; ++index) {
      const RasterVertex& first = vertices[index];
      const RasterVertex& second =
          vertices[(index + 1) % _gr_polygon.nVertices];
      if ((first.y <= scanY && scanY < second.y) ||
          (second.y <= scanY && scanY < first.y)) {
        const double ratio = (scanY - first.y) / (second.y - first.y);
        intersections[intersectionCount++] =
            InterpolateEdge(first, second, ratio);
      }
    }
    std::sort(intersections, intersections + intersectionCount,
              [](const EdgeSample& first, const EdgeSample& second) {
                return first.x < second.x;
              });

    for (int edge = 0; edge + 1 < intersectionCount; edge += 2) {
      const EdgeSample& left = intersections[edge];
      const EdgeSample& right = intersections[edge + 1];
      const double span = right.x - left.x;
      if (span <= kSpanEpsilon) continue;

      int firstX = static_cast<int>(std::ceil(left.x - 0.5));
      int endX = static_cast<int>(std::ceil(right.x - 0.5));
      firstX = (std::max)(firstX, clip.left);
      endX = (std::min)(endX, clip.right);
      if (endX <= firstX) continue;

      const double firstRatio =
          (static_cast<double>(firstX) + 0.5 - left.x) / span;
      const double ratioStep = 1.0 / span;
      double inverseZ = Lerp(left.inverseZ, right.inverseZ, firstRatio);
      double u = Lerp(left.u, right.u, firstRatio);
      double v = Lerp(left.v, right.v, firstRatio);
      double uInverseZ =
          Lerp(left.uInverseZ, right.uInverseZ, firstRatio);
      double vInverseZ =
          Lerp(left.vInverseZ, right.vInverseZ, firstRatio);
      double shade = Lerp(left.shade, right.shade, firstRatio);
      double red = Lerp(left.red, right.red, firstRatio);
      double green = Lerp(left.green, right.green, firstRatio);
      double blue = Lerp(left.blue, right.blue, firstRatio);

      const double inverseZStep =
          (right.inverseZ - left.inverseZ) * ratioStep;
      const double uStep = (right.u - left.u) * ratioStep;
      const double vStep = (right.v - left.v) * ratioStep;
      const double uInverseZStep =
          (right.uInverseZ - left.uInverseZ) * ratioStep;
      const double vInverseZStep =
          (right.vInverseZ - left.vInverseZ) * ratioStep;
      const double shadeStep = (right.shade - left.shade) * ratioStep;
      const double redStep = (right.red - left.red) * ratioStep;
      const double greenStep = (right.green - left.green) * ratioStep;
      const double blueStep = (right.blue - left.blue) * ratioStep;

      unsigned char* destination = _gr_pScreen +
          static_cast<std::size_t>(y + _gr_nScreenOriginY) *
              _gr_nScreenWidth +
          firstX + _gr_nScreenOriginX;
      for (int x = firstX; x < endX; ++x, ++destination) {
        ++covered;
        unsigned char color = flatColor;
        unsigned char textureValue = 0;
        if (textured) {
          double textureU = u;
          double textureV = v;
          if (perspective) {
            if (inverseZ <= kSpanEpsilon) {
              inverseZ += inverseZStep;
              u += uStep;
              v += vStep;
              uInverseZ += uInverseZStep;
              vInverseZ += vInverseZStep;
              shade += shadeStep;
              red += redStep;
              green += greenStep;
              blue += blueStep;
              continue;
            }
            textureU = uInverseZ / inverseZ;
            textureV = vInverseZ / inverseZ;
          }
          textureValue =
              SampleTexture(*texture, textureU, textureV, ditheredBump);
          color = textureValue;
          if (sprite && textureValue == 0) {
            inverseZ += inverseZStep;
            u += uStep;
            v += vStep;
            uInverseZ += uInverseZStep;
            vInverseZ += vInverseZStep;
            shade += shadeStep;
            red += redStep;
            green += greenStep;
            blue += blueStep;
            continue;
          }
        }

        if (gouraud || textureGouraud) {
          color = ApplyGouraud(color, shade);
        } else if (gouraudRgb) {
          const int r = ClampInt(static_cast<int>(std::floor(red + 0.5)),
                                 0, 255);
          const int g = ClampInt(static_cast<int>(std::floor(green + 0.5)),
                                 0, 255);
          const int b = ClampInt(static_cast<int>(std::floor(blue + 0.5)),
                                 0, 255);
          color = static_cast<unsigned char>(epal_Match(_EPal, RGB_i(r, g, b)));
        }

        if (flatTransparent) {
          color = blendTable[*destination];
          ++transparentWrites;
        } else if (alpha) {
          int textureAlpha = ClampInt(textureValue, 0, 15);
          if (polygonOpacity < 15) {
            textureAlpha = polygonOpacity * (textureAlpha + 1) >> 4;
          }
          color = blendTable[textureAlpha * 256 + *destination];
          ++transparentWrites;
        }

        if (dynamicLighting) {
          color = ApplyDynamicLights(color, x, y, preparedLights,
                                     preparedLightCount,
                                     &lightApplications);
        }
        if (useHaze) {
          color = ApplyHaze(color, inverseZ);
          ++hazed;
        }
        *destination = color;
        ++written;

        inverseZ += inverseZStep;
        u += uStep;
        v += vStep;
        uInverseZ += uInverseZStep;
        vInverseZ += vInverseZStep;
        shade += shadeStep;
        red += redStep;
        green += greenStep;
        blue += blueStep;
      }
    }
  }

  AddStat(&SGRSoftwareRasterStats::coveredPixels, covered);
  AddStat(&SGRSoftwareRasterStats::writtenPixels, written);
  AddStat(&SGRSoftwareRasterStats::hazePixels, hazed);
  AddStat(&SGRSoftwareRasterStats::transparentPixels, transparentWrites);
  AddStat(&SGRSoftwareRasterStats::litPixels, lightApplications);
  if (covered != 0) {
    AddStat(&SGRSoftwareRasterStats::rasterized);
    AddRasterizedType(type);
  }
  return TRUE;
}

}  // namespace

int (*_pGRDrawPolygonPCCW)(void) = DrawSoftwarePolygon;
void (*_pGRSetZPrecision)(int) = SetSoftwareZPrecision;

void GRSetViewport(SGRViewport *pViewport)
{
    _Viewport = pViewport;

    if( pViewport == NULL ) return;

    _gr_nScreenOriginY = pViewport->y;
    _gr_nScreenOriginX = pViewport->x;
    _gr_clipRect = pViewport->clipRect;

    if( _dL.currDevice != NULL &&
        _dL.currDevice->swHw == GR_SOFTWARE ) {
        _gr_pOrigin = pViewport->pOrigin;
        _gr_pYCache = pViewport->pCache;
    }

    GRSetClipRect();
}

SGRViewport *GRGetViewport()
{
    return _Viewport;
}

SGRViewport *GRCreateViewport(int originX,int originY,TCSRect2 &clipRect)
{
    if( _dL.currDevice == NULL || originX < 0 || originY < 0 ||
        originX > _gr_nScreenWidth || originY > _gr_nScreenHeight ||
        (_dL.currDevice->swHw == GR_SOFTWARE && _gr_pScreen == NULL) )
        return NULL;

    SGRViewport *pViewport = new SGRViewport;
    int clipH = _gr_nScreenHeight;

    pViewport->x = originX;
    pViewport->y = originY;
    pViewport->clipRect.left = clipRect.left-originX;
    pViewport->clipRect.right = clipRect.right-originX;
    pViewport->clipRect.top = clipRect.top-originY;
    pViewport->clipRect.bottom = clipRect.bottom-originY;

    if( _dL.currDevice->swHw == GR_SOFTWARE ) {
        pViewport->pCache0 = new byte *[clipH];
        pViewport->pCache = pViewport->pCache0+originY;
        pViewport->pOrigin = _gr_pScreen+originY*_gr_nScreenWidth+originX;
        for( int i = -originY; i < _gr_nScreenHeight-originY; ++i )
            pViewport->pCache[i] = pViewport->pOrigin+i*_gr_nScreenWidth;
    } else {
        pViewport->pCache0 = NULL;
    }

    return pViewport;
}

void GRReleaseViewport(SGRViewport *pViewport)
{
    if( !pViewport ) return;
    if( pViewport->pCache0 != NULL ) {
        if( _gr_pYCache == pViewport->pCache ) _gr_pYCache = NULL;
        if( _gr_pOrigin == pViewport->pOrigin ) _gr_pOrigin = NULL;
        delete [] pViewport->pCache0;
    }
    if( _Viewport == pViewport ) _Viewport = NULL;

    delete pViewport;
}

unsigned long GRFillColor(int r,int g,int b)
{
    if( _dL.currDevice == NULL ) return 0;
    if( _dL.currDevice->swHw == GR_HARDWARE )
        return (((unsigned int)r>>_rScale)<<_rShift) |
               (((unsigned int)g>>_gScale)<<_gShift) |
               (((unsigned int)b>>_bScale)<<_bShift);

    return epal_Match(_EPal,RGB_i(r,g,b));
}

unsigned long GRCreateColor(int r,int g,int b)
{
    if( _dL.currDevice == NULL ) return 0;
    STextureFormat *tf =
        &_dL.currDevice->textureFormat[NORMAL_TEXTURE_INDEX];

    if( tf->rgbBitCount == 8 ) {
        unsigned int col = epal_Match(_EPal,RGB_i(r,g,b));
        unsigned char *pal = &_currPalette[col*3];

        return (((unsigned int)pal[0])<<24) |
               (((unsigned int)pal[1])<<16) |
               (((unsigned int)pal[2])<<8) | col;
    }

    return (((unsigned int)r)<<24) |
           (((unsigned int)g)<<16) |
           (((unsigned int)b)<<8) |
           epal_Match(_EPal,RGB_i(r,g,b));
}

int GRIsHardware()
{
    return _dL.currDevice != NULL &&
           _dL.currDevice->swHw == GR_HARDWARE;
}

void GRSetScale(float scaleX,float scaleY)
{
    if( scaleX == 0.0f || scaleY == 0.0f ) return;
    _kX = scaleX;
    _kY = scaleY;
    _kXY = scaleX*scaleY;
    _ikX = 1.0f/scaleX;
    _ikY = 1.0f/scaleY;
    _kXX = scaleX*scaleX;
    _kYY = scaleY*scaleY;
}

int GRSetHaze(int start,int length,SGRColorDef *definition)
{
    if( start <= 0 || length <= 0 || definition == NULL ||
        definition->pTable == NULL || GRIsHardware() ) return FALSE;

    __HazeStartInt = static_cast<int>(65536.0/start);
    __HazeLen = static_cast<float>(start+length);
    g_hazeStart = start;
    g_hazeLength = length;
    _gr_pHaze = definition->pTable;
    return TRUE;
}

void GRSetPaletteTables(SGRColorDef *pTransparency,int nTranspCount,
                        TCbyte *pGouraud)
{
    _gr_pTransparency = pTransparency;
    _gr_nTranspCount = pTransparency != NULL && nTranspCount > 0 ?
                       (std::min)(nTranspCount,256) : 0;
    _gr_pGouraud = const_cast<unsigned char *>(pGouraud);

    _ETransparencyPal.Clear();
    if( _gr_nTranspCount == 0 ) return;

    unsigned char palette[768] = {};
    for( int i = 0; i < _gr_nTranspCount; ++i ) {
        palette[i*3] = static_cast<unsigned char>(pTransparency[i].r);
        palette[i*3+1] = static_cast<unsigned char>(pTransparency[i].g);
        palette[i*3+2] = static_cast<unsigned char>(pTransparency[i].b);
    }
    epal_Load8BitPal(_ETransparencyPal,palette,_gr_nTranspCount);
}

void SetMixLightTable(unsigned char *table)
{
    if( table == NULL ) {
        std::memset(g_lightMixTable,0,sizeof(g_lightMixTable));
        g_lightMixTableReady = false;
        return;
    }
    std::memcpy(g_lightMixTable,table,sizeof(g_lightMixTable));
    g_lightMixTableReady = true;
}

unsigned long GRTransparentColor(int r,int g,int b)
{
    if( _dL.currDevice == NULL ) return 0;
    if( _dL.currDevice->swHw == GR_HARDWARE )
        return (static_cast<unsigned long>(r)<<16) |
               (static_cast<unsigned long>(g)<<8) |
               static_cast<unsigned long>(b);
    if( _gr_pTransparency == NULL || _gr_nTranspCount <= 0 ) return 0;

    const int color = epal_Match(_ETransparencyPal,RGB_i(r,g,b));
    return static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(
        _gr_pTransparency[color].pTable));
}

int GRSetPalette(const unsigned char *pal8,int setScr)
{
    (void)setScr;
    if( _dL.currDevice == NULL || pal8 == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE ) return FALSE;

    std::memcpy(_currPalette,pal8,sizeof(_currPalette));
    _currPalette[0] = 0;
    _currPalette[1] = 0;
    _currPalette[2] = 0;
    for( int i = 3; i < 256*3; i += 3 )
        if( _currPalette[i] == 0 && _currPalette[i+1] == 0 &&
            _currPalette[i+2] == 0 ) _currPalette[i+2] = 2;

    _EPal.Clear();
    epal_Load8BitPal(_EPal,_currPalette,256);
    _gr_logPal.palVersion = 0x300;
    _gr_logPal.palNumEntries = 256;
    for( int i = 0; i < 256; ++i ) {
        const unsigned char r = _currPalette[i*3];
        const unsigned char g = _currPalette[i*3+1];
        const unsigned char b = _currPalette[i*3+2];
        _gr_logPal.palPalEntry[i].peRed = r;
        _gr_logPal.palPalEntry[i].peGreen = g;
        _gr_logPal.palPalEntry[i].peBlue = b;
        _gr_logPal.palPalEntry[i].peFlags = PC_NOCOLLAPSE;
        _gr_DIBInfo.bmiColors[i].rgbRed = r;
        _gr_DIBInfo.bmiColors[i].rgbGreen = g;
        _gr_DIBInfo.bmiColors[i].rgbBlue = b;
        _gr_DIBInfo.bmiColors[i].rgbReserved = 0;
        _gr_DIBInfo.bmiIndex[i] = static_cast<WORD>(i);
    }
    return TRUE;
}

int GRClearScreen(BOOL fClr,long fColor)
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE ) return FALSE;
    if( !fClr ) return TRUE;
    if( _gr_pScreen == NULL || _gr_nScreenWidth <= 0 ||
        _gr_nScreenHeight <= 0 ) return FALSE;

    int left = static_cast<int>(_gr_clipRect.left)+_gr_nScreenOriginX;
    int top = static_cast<int>(_gr_clipRect.top)+_gr_nScreenOriginY;
    int right = static_cast<int>(_gr_clipRect.right)+_gr_nScreenOriginX;
    int bottom = static_cast<int>(_gr_clipRect.bottom)+_gr_nScreenOriginY;
    if( right <= left || bottom <= top ) {
        left = 0;
        top = 0;
        right = _gr_nScreenWidth;
        bottom = _gr_nScreenHeight;
    }
    left = (std::max)(left,0);
    top = (std::max)(top,0);
    right = (std::min)(right,_gr_nScreenWidth);
    bottom = (std::min)(bottom,_gr_nScreenHeight);
    if( right <= left || bottom <= top ) return FALSE;

    unsigned char *row = _gr_pScreen+
        static_cast<std::size_t>(top)*_gr_nScreenWidth+left;
    for( int y = top; y < bottom; ++y,row += _gr_nScreenWidth )
        std::memset(row,static_cast<unsigned char>(fColor),
                    static_cast<std::size_t>(right-left));
    return TRUE;
}

int GRSoftwareBeginFrame(long fColor)
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL ||
        _gr_nScreenWidth <= 0 || _gr_nScreenHeight <= 0 ) return FALSE;

    std::memset(&g_frameRasterStats,0,sizeof(g_frameRasterStats));
    g_frameRasterStats.frames = 1;
    ++g_totalRasterStats.frames;
    g_frameClearColor = static_cast<unsigned char>(fColor);
    std::memset(_gr_pScreen,g_frameClearColor,
                static_cast<std::size_t>(_gr_nScreenWidth)*
                    _gr_nScreenHeight);
    return TRUE;
}

int GRSoftwareLoadDitherTable(const char *path)
{
    std::memset(_gr_pDiserTable,0,sizeof(_gr_pDiserTable));
    g_ditherTextureStride = 0;
    g_ditherTableReady = false;
    if( path == NULL || path[0] == '\0' ) return FALSE;
    std::FILE *file = std::fopen(path,"rb");
    if( file == NULL ) return FALSE;

    unsigned char header[6] = {};
    std::int32_t table[64*64] = {};
    const bool read = std::fread(header,1,sizeof(header),file) == sizeof(header) &&
        std::fread(table,sizeof(table[0]),64*64,file) == 64*64;
    std::fclose(file);
    const unsigned int width =
        static_cast<unsigned int>(header[0]) |
        (static_cast<unsigned int>(header[1]) << 8);
    const unsigned int height =
        static_cast<unsigned int>(header[2]) |
        (static_cast<unsigned int>(header[3]) << 8);
    const unsigned int stride =
        static_cast<unsigned int>(header[4]) |
        (static_cast<unsigned int>(header[5]) << 8);
    if( !read || width != 64 || height != 64 || stride == 0 ||
        stride > 4096 ) return FALSE;

    for( int index = 0; index < 64*64; ++index )
        _gr_pDiserTable[index] = static_cast<long>(table[index]);
    g_ditherTextureStride = static_cast<int>(stride);
    g_ditherTableReady = true;
    return TRUE;
}

void GRSoftwareClearDitherTable()
{
    std::memset(_gr_pDiserTable,0,sizeof(_gr_pDiserTable));
    g_ditherTextureStride = 0;
    g_ditherTableReady = false;
}

int GRSoftwareDitherTableReady()
{
    return g_ditherTableReady ? TRUE : FALSE;
}

void GRSoftwareGetFrameStats(SGRSoftwareRasterStats *stats)
{
    if( stats != NULL ) {
        *stats = g_frameRasterStats;
        PopulateFramebufferFingerprint(stats);
    }
}

void GRSoftwareGetTotalStats(SGRSoftwareRasterStats *stats)
{
    if( stats != NULL ) {
        *stats = g_totalRasterStats;
        PopulateFramebufferFingerprint(stats);
    }
}

void GRSoftwareResetTotalStats()
{
    std::memset(&g_totalRasterStats,0,sizeof(g_totalRasterStats));
}

void GRZBufferEnable(int enable)
{
    (void)enable;
}

int GREndScene()
{
    return _dL.currDevice != NULL &&
           _dL.currDevice->swHw == GR_SOFTWARE;
}

int GRStartScene()
{
    return _dL.currDevice != NULL &&
           _dL.currDevice->swHw == GR_SOFTWARE;
}

int GRDumpScreen()
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL )
        return FALSE;
    if( _gr_hDC == NULL ) return TRUE;

    return SetDIBitsToDevice(
        _gr_hDC,0,0,_gr_nScreenWidth,_gr_nScreenHeight,0,0,0,
        _gr_nScreenHeight,_gr_pScreen,
        reinterpret_cast<BITMAPINFO *>(&_gr_DIBInfo),DIB_RGB_COLORS) != 0;
}

#if defined(_MSC_VER) && defined(_DEBUG)
#pragma optimize("", off)
#pragma runtime_checks("", restore)
#endif

#undef RR2NW_HOT_INLINE
