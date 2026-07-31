#include "RecoveredLevelAssets.h"

#include <cmath>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "filesys.h"
#include "graph.h"

#include "RecoveredLevelRuntime.h"
#include "ViewFigureLibraryState.h"
#include "ZavOverallInfoState.h"
#include "ZavShutdownState.h"

extern SDeviceList _dL;
extern double __terrainWaterline;

CFixedColorFont font5;

namespace {

const char kPaletteFile[] = "default.ptp";
const char kFontFile[] = "..\\figs5x3c.fnt";
const std::size_t kMaximumFontSize = 16u * 1024u * 1024u;
const int kMaximumSceneObjects = 100000;

struct SFontHeader {
  char id[4];
  long width;
  long height;
  unsigned char palette[256 * 3];
  long glyphTable[256 * 2];
  long spriteSize;
};

static_assert(sizeof(long) == 4, "recovered font header requires Win32 long");
static_assert(sizeof(SFontHeader) == 2832,
              "recovered fixed-font header layout changed");

SRecoveredSceneHeader g_sceneHeader = {};
unsigned int g_issues = 0;
bool g_ready = false;
bool g_mutated = false;

bool IsRegularFile(const char* path) {
  const DWORD attributes = GetFileAttributesA(path);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool ReadBlock(CTaggedFile& file, long bytes) {
  if (bytes < 0 || file.BytesLeft() != bytes) return false;
  std::vector<unsigned char> block(static_cast<std::size_t>(bytes));
  return bytes == 0 || file.Read(block.data(), bytes) == bytes;
}

bool ValidatePalettePack(const char* path) {
  CTaggedFile file(false);
  const bool opened = file.Open(path, false);
  bool valid = opened;
  valid = valid && file.Descend("PTP_", false, 2, false) == 2;
  valid = valid && file.Descend("PAL8", true, 0, false) == 0;
  valid = valid && ReadBlock(file, 256 * 3) && file.Ascend();
  valid = valid && file.Descend("GOUR", true, 1, false) == 1;
  valid = valid && ReadBlock(file, 16 * 256) && file.Ascend();

  valid = valid && file.Descend("TRAN", true, 1, false) == 1;
  int transparencyCount = 0;
  valid = valid && file.ReadInt(transparencyCount) &&
          transparencyCount > 0 && transparencyCount <= 256;
  if (valid) {
    const long bytes = static_cast<long>(transparencyCount) *
                       (3 + 16 * 256);
    valid = ReadBlock(file, bytes);
  }
  valid = valid && file.Ascend();

  for (int index = 0; valid && index < CPaletteTranslator::NUM_HAZES;
       ++index) {
    valid = file.Descend("HAZE", true, 0, false) == 0;
    unsigned char color[3] = {};
    int colorCount = 0;
    valid = valid && file.Read(color, sizeof(color)) == sizeof(color) &&
            file.ReadInt(colorCount) && colorCount == 16 &&
            ReadBlock(file, 16 * 256) && file.Ascend();
  }

  valid = valid && file.Descend("LIG_", true, 0, false) == 0;
  int lightCount = 0;
  valid = valid && file.ReadInt(lightCount) &&
          lightCount == LIGHT_COLOR_COUNT &&
          ReadBlock(file, LIGHT_COLOR_COUNT * 32 * 256) && file.Ascend();
  valid = valid && file.Ascend();
  const bool closed = opened ? file.Close(false) : true;
  return valid && closed;
}

bool ValidateSceneHeader(const char* path, SRecoveredSceneHeader& result) {
  CTaggedFile file(false);
  const bool opened = file.Open(path, false);
  bool valid = opened;
  valid = valid && file.Descend("SCEN", false, 0, false) == 0;
  valid = valid && file.Descend("SCEH", true, 0, false) == 0;
  valid = valid && file.ReadInt(result.namedBases) &&
          file.ReadInt(result.directBases) &&
          file.ReadInt(result.totalReferences) &&
          file.ReadDouble(result.cellSize) &&
          file.ReadDouble(result.heightRatio) &&
          file.ReadInt(result.reductions);
  valid = valid && result.namedBases >= 0 && result.directBases >= 0 &&
          result.namedBases <= kMaximumSceneObjects &&
          result.directBases <= kMaximumSceneObjects - result.namedBases &&
          result.totalReferences >= 0 &&
          result.totalReferences <= kMaximumSceneObjects &&
          result.reductions >= 0 && std::isfinite(result.cellSize) &&
          result.cellSize > 0.0 && std::isfinite(result.heightRatio) &&
          result.heightRatio > 0.0;
  valid = valid && file.Ascend() && file.Ascend();
  const bool closed = opened ? file.Close(false) : true;
  return valid && closed;
}

bool ReadFont(const char* path, std::vector<unsigned char>& bytes) {
  long size = 0;
  FILE* input = CFileResource::FOpenCurrent(path, &size);
  if (input == nullptr) return false;
  if (size < static_cast<long>(sizeof(SFontHeader)) ||
      static_cast<std::size_t>(size) > kMaximumFontSize) {
    std::fclose(input);
    return false;
  }
  bytes.resize(static_cast<std::size_t>(size));
  const bool read =
      std::fread(bytes.data(), static_cast<std::size_t>(size), 1, input) == 1;
  return std::fclose(input) == 0 && read;
}

bool ValidateFont(const std::vector<unsigned char>& bytes) {
  SFontHeader header = {};
  std::memcpy(&header, bytes.data(), sizeof(header));
  if (std::memcmp(header.id, "FIXF", sizeof(header.id)) != 0 ||
      header.width <= 0 || header.width > 4096 || header.height <= 0 ||
      header.height > 4096 || header.spriteSize <= 0 ||
      static_cast<std::size_t>(header.spriteSize) >
          bytes.size() - sizeof(header)) {
    return false;
  }

  for (int character = 0; character < 256; ++character) {
    const long width = header.glyphTable[character * 2];
    const long offset = header.glyphTable[character * 2 + 1];
    if (width < 0 || width > header.width || offset < 0) return false;
    if (width == 0) continue;
    const long long end = static_cast<long long>(offset) +
                          static_cast<long long>(header.height - 1) *
                              header.width +
                          width;
    if (end > header.spriteSize) return false;
  }
  return true;
}

void ResetFont() {
  font5.~CFixedColorFont();
  new (&font5) CFixedColorFont();
}

int Fail(unsigned int issue) {
  g_issues = issue;
  RecoveredLevelRuntime_Release();
  return FALSE;
}

}  // namespace

int RecoveredLevelAssets_Initialize() {
  try {
    RecoveredLevelAssets_Release();
    g_issues = 0;
    if (!RecoveredLevelRuntime_IsPrepared()) {
      return Fail(RECOVERED_LEVEL_ASSET_MISSING_LEVEL);
    }

    const SRecoveredLevelSettings* settings =
        RecoveredLevelRuntime_Settings();
    if (settings == nullptr) {
      return Fail(RECOVERED_LEVEL_ASSET_MISSING_LEVEL);
    }
    if (settings->bspCheck != 0) {
      return Fail(RECOVERED_LEVEL_ASSET_BSP_CHECK_REQUESTED);
    }

    const char* sceneFile = RecoveredLevelRuntime_SceneFile();
    if (sceneFile == nullptr || !IsRegularFile(sceneFile)) {
      return Fail(RECOVERED_LEVEL_ASSET_MISSING_SCENE);
    }
    if (!ValidateSceneHeader(sceneFile, g_sceneHeader)) {
      return Fail(RECOVERED_LEVEL_ASSET_INVALID_SCENE);
    }
    if (!IsRegularFile(kPaletteFile)) {
      return Fail(RECOVERED_LEVEL_ASSET_MISSING_PALETTE);
    }
    if (!ValidatePalettePack(kPaletteFile)) {
      return Fail(RECOVERED_LEVEL_ASSET_INVALID_PALETTE);
    }
    if (!IsRegularFile(kFontFile)) {
      return Fail(RECOVERED_LEVEL_ASSET_MISSING_FONT);
    }

    std::vector<unsigned char> fontBytes;
    if (!ReadFont(kFontFile, fontBytes) || !ValidateFont(fontBytes)) {
      return Fail(RECOVERED_LEVEL_ASSET_INVALID_FONT);
    }

    g_mutated = true;
    if (!CPaletteTranslator::LoadPalettePack(kPaletteFile)) {
      return Fail(RECOVERED_LEVEL_ASSET_INVALID_PALETTE);
    }
    paletteTranslator.EnableMatch(FALSE);
    paletteTranslator.SetGRTables();
    if (!GRSetPalette(reinterpret_cast<byte*>(paletteTranslator.Palette()),
                      FALSE)) {
      return Fail(RECOVERED_LEVEL_ASSET_GRAPH_FAILURE);
    }

    ResetFont();
    if (!font5.ReadFromMemory(fontBytes.data())) {
      return Fail(RECOVERED_LEVEL_ASSET_INVALID_FONT);
    }

    nWhiteColor = GRFillColor(255, 255, 255);
    CPaletteTranslator::ComputeHazeColors();
    if (!ViewFigureLibrary_Initialize()) {
      return Fail(RECOVERED_LEVEL_ASSET_FIGURE_LIBRARY_FAILURE);
    }
    ZAV_ConfigureLevelShutdown(nullptr, ViewFigureLibrary_Release);
    ZAV_ArmLevelShutdown();
    GRReInitTextureDB();

    __terrainWaterline = settings->waterline;
    if (_dL.currDevice != nullptr) {
      _dL.currDevice->fogCurrent = settings->fogMode;
    }
    CViewObject::SetClipPlanes(
        settings->nearClip, settings->hazeMin + settings->hazeDistance);
    paletteTranslator.Haze(0).nHazeMin = settings->hazeMin;
    paletteTranslator.Haze(0).nHazeDist = settings->hazeDistance;
    paletteTranslator.Haze(1).nHazeMin = settings->hazeMinWater;
    paletteTranslator.Haze(1).nHazeDist = settings->hazeDistanceWater;
    CViewFigure::SetHaze(paletteTranslator.Haze(0));
    CViewObject::SetHaze(TRUE);

    g_ready = true;
    return TRUE;
  } catch (const std::bad_alloc&) {
    return Fail(RECOVERED_LEVEL_ASSET_ALLOCATION_FAILURE);
  }
}

void RecoveredLevelAssets_Release() {
  if (ZAV_IsLevelShutdownArmed()) {
    ZAV_DeinitLevelResources();
  } else if (ViewFigureLibrary_IsReady()) {
    ViewFigureLibrary_Release();
  }

  if (g_mutated) {
    GRSetPaletteTables(nullptr, 0, nullptr);
    CPaletteTranslator::CleanUp();
    paletteTranslator.EnableMatch(TRUE);
    ResetFont();
  }
  g_sceneHeader = SRecoveredSceneHeader{};
  g_ready = false;
  g_mutated = false;
}

bool RecoveredLevelAssets_IsReady() { return g_ready; }

unsigned int RecoveredLevelAssets_Issues() { return g_issues; }

const SRecoveredSceneHeader* RecoveredLevelAssets_SceneHeader() {
  return g_ready ? &g_sceneHeader : nullptr;
}
