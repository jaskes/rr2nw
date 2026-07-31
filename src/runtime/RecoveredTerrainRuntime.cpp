#include "RecoveredTerrainRuntime.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <new>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "graph.h"

#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredModRuntime.h"

extern CFixedColorFont terrFnt;

namespace {

struct SSpriteResource {
  const char* name;
  std::uint16_t width;
  std::uint16_t height;
  unsigned int bytesPerPixel;
};

struct SBitmapResource {
  const char* name;
  LONG dimension;
};

const SSpriteResource kSprites[] = {
    {"covh7.spr", 512, 512, 1},
    {"mapc7.spr", 512, 512, 2},
    {"hrange.spr", 64, 64, 2},
    {"red.spr", 512, 512, 1},
    {"maskflag.spr", 512, 512, 1},
};

const SBitmapResource kBitmaps[] = {
    {"Mask9.bmp", 512},
    {"Mask19.bmp", 512},
    {"bump.bmp", 256},
    {"maskw.bmp", 256},
    {"bumpw.bmp", 256},
};

_CViewTerrain* g_terrain = nullptr;
unsigned int g_issues = 0;

std::string JoinPath(const char* directory, const char* name) {
  std::string result(directory);
  if (!result.empty() && result.back() != '\\' && result.back() != '/') {
    result.push_back('\\');
  }
  result += name;
  return result;
}

bool IsDirectory(const char* path) {
  if (path == nullptr || *path == '\0') return false;
  const DWORD attributes = GetFileAttributesA(path);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsRegularFile(const std::string& path) {
  char resolved[4096] = {};
  if (!RecoveredModRuntime_ResolveReadPath(path.c_str(), resolved,
                                           sizeof(resolved)))
    return false;
  const DWORD attributes = GetFileAttributesA(resolved);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool FileSize(std::ifstream& input, std::uint64_t& result) {
  input.seekg(0, std::ios::end);
  const std::streamoff end = input.tellg();
  if (end < 0) return false;
  result = static_cast<std::uint64_t>(end);
  input.seekg(0, std::ios::beg);
  return input.good();
}

bool ValidateSprite(const std::string& path,
                    const SSpriteResource& expected) {
  char resolved[4096] = {};
  if (!RecoveredModRuntime_ResolveReadPath(path.c_str(), resolved,
                                           sizeof(resolved)))
    return false;
  std::ifstream input(resolved, std::ios::binary);
  std::uint64_t size = 0;
  if (!input || !FileSize(input, size)) return false;

  unsigned char header[5] = {};
  if (!input.read(reinterpret_cast<char*>(header), sizeof(header))) {
    return false;
  }
  const std::uint16_t width = static_cast<std::uint16_t>(
      header[0] | static_cast<unsigned int>(header[1]) << 8);
  const std::uint16_t height = static_cast<std::uint16_t>(
      header[2] | static_cast<unsigned int>(header[3]) << 8);
  const std::uint64_t expectedSize =
      sizeof(header) + static_cast<std::uint64_t>(expected.width) *
                           expected.height * expected.bytesPerPixel;
  return width == expected.width && height == expected.height &&
         size == expectedSize;
}

bool ValidateBitmap(const std::string& path,
                    const SBitmapResource& expected) {
  char resolved[4096] = {};
  if (!RecoveredModRuntime_ResolveReadPath(path.c_str(), resolved,
                                           sizeof(resolved)))
    return false;
  std::ifstream input(resolved, std::ios::binary);
  std::uint64_t size = 0;
  if (!input || !FileSize(input, size)) return false;

  BITMAPFILEHEADER fileHeader = {};
  BITMAPINFOHEADER infoHeader = {};
  if (!input.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader)) ||
      !input.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader))) {
    return false;
  }

  const DWORD paletteEntries =
      infoHeader.biClrUsed == 0 ? 256 : infoHeader.biClrUsed;
  const std::uint64_t imageBytes =
      static_cast<std::uint64_t>(expected.dimension) * expected.dimension;
  const std::uint64_t expectedOffset =
      sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
      static_cast<std::uint64_t>(paletteEntries) * sizeof(RGBQUAD);
  const std::uint64_t expectedSize = expectedOffset + imageBytes;

  return fileHeader.bfType == 0x4D42 &&
         fileHeader.bfReserved1 == 0 && fileHeader.bfReserved2 == 0 &&
         fileHeader.bfOffBits == expectedOffset &&
         fileHeader.bfSize == expectedSize && size == expectedSize &&
         infoHeader.biSize == sizeof(BITMAPINFOHEADER) &&
         infoHeader.biWidth == expected.dimension &&
         infoHeader.biHeight == expected.dimension &&
         infoHeader.biPlanes == 1 && infoHeader.biBitCount == 8 &&
         infoHeader.biCompression == BI_RGB &&
         infoHeader.biSizeImage == imageBytes &&
         infoHeader.biClrUsed <= 256;
}

void ResetTerrainFont() {
  terrFnt.~CFixedColorFont();
  new (&terrFnt) CFixedColorFont();
}

int Fail(unsigned int issue) {
  g_issues = issue;
  RecoveredTerrain_Release();
  return FALSE;
}

}  // namespace

unsigned int RecoveredTerrain_ValidateDirectory(const char* directory) {
  try {
    if (!IsDirectory(directory)) return RECOVERED_TERRAIN_MISSING_LEVEL;

    unsigned int issues = 0;
    for (const SSpriteResource& resource : kSprites) {
      const std::string path = JoinPath(directory, resource.name);
      if (!IsRegularFile(path)) {
        issues |= RECOVERED_TERRAIN_MISSING_RESOURCE;
      } else if (!ValidateSprite(path, resource)) {
        issues |= RECOVERED_TERRAIN_INVALID_RESOURCE;
      }
    }
    for (const SBitmapResource& resource : kBitmaps) {
      const std::string path = JoinPath(directory, resource.name);
      if (!IsRegularFile(path)) {
        issues |= RECOVERED_TERRAIN_MISSING_RESOURCE;
      } else if (!ValidateBitmap(path, resource)) {
        issues |= RECOVERED_TERRAIN_INVALID_RESOURCE;
      }
    }
    return issues;
  } catch (const std::bad_alloc&) {
    return RECOVERED_TERRAIN_ALLOCATION_FAILURE;
  }
}

int RecoveredTerrain_Initialize() {
  RecoveredTerrain_Release();
  g_issues = 0;
  if (!RecoveredLevelRuntime_IsPrepared()) {
    return Fail(RECOVERED_TERRAIN_MISSING_LEVEL);
  }
  if (!RecoveredLevelAssets_IsReady()) {
    return Fail(RECOVERED_TERRAIN_MISSING_LEVEL_ASSETS);
  }

  const unsigned int validation = RecoveredTerrain_ValidateDirectory(
      RecoveredLevelRuntime_Directory());
  if (validation != 0) return Fail(validation);

  const SRecoveredSceneHeader* header = RecoveredLevelAssets_SceneHeader();
  if (header == nullptr) {
    return Fail(RECOVERED_TERRAIN_MISSING_LEVEL_ASSETS);
  }
  const double maximumEdges = std::ceil(11000.0 / header->cellSize);
  if (!std::isfinite(maximumEdges) || maximumEdges < 1.0 ||
      maximumEdges > 1048576.0) {
    return Fail(RECOVERED_TERRAIN_INVALID_RESOURCE);
  }

  try {
    g_terrain =
        new _CViewTerrain(header->cellSize, header->heightRatio, 11000.0);
  } catch (const std::bad_alloc&) {
    return Fail(RECOVERED_TERRAIN_ALLOCATION_FAILURE);
  } catch (...) {
    return Fail(RECOVERED_TERRAIN_CONSTRUCTION_FAILURE);
  }

  if (g_terrain->GetHeightMap() == nullptr) {
    return Fail(RECOVERED_TERRAIN_CONSTRUCTION_FAILURE);
  }
  return TRUE;
}

void RecoveredTerrain_Release() {
  delete g_terrain;
  g_terrain = nullptr;
  ResetTerrainFont();
}

bool RecoveredTerrain_IsReady() { return g_terrain != nullptr; }

unsigned int RecoveredTerrain_Issues() { return g_issues; }

_CViewTerrain* RecoveredTerrain_Get() { return g_terrain; }
