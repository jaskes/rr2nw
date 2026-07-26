#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "filesys.h"
#include "graph.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ViewFigureLibraryState.h"
#include "ZavOverallInfoState.h"
#include "ZavShutdownState.h"

extern SDeviceList _dL;
extern double __terrainWaterline;

namespace {

struct SFontFixtureHeader {
  char id[4];
  long width;
  long height;
  unsigned char palette[256 * 3];
  long glyphTable[256 * 2];
  long spriteSize;
};

static_assert(sizeof(SFontFixtureHeader) == 2832,
              "fixed-font fixture layout changed");

int Fail(const char* message) {
  std::fprintf(stderr, "recovered-level-assets-smoke: %s\n", message);
  return EXIT_FAILURE;
}

std::string JoinPath(const std::string& directory, const char* name) {
  return directory + "\\" + name;
}

bool EnsureDirectory(const std::string& path) {
  if (CreateDirectoryA(path.c_str(), nullptr) != FALSE) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool WriteBytes(const std::string& path, const void* data,
                std::size_t size) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(static_cast<const char*>(data),
               static_cast<std::streamsize>(size));
  return output.good();
}

bool WriteText(const std::string& path, const char* contents) {
  return WriteBytes(path, contents, std::strlen(contents));
}

bool CurrentDirectory(std::string& result) {
  const DWORD required = GetCurrentDirectoryA(0, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied = GetCurrentDirectoryA(required, buffer.data());
  if (copied == 0 || copied >= required) return false;
  result.assign(buffer.data(), copied);
  return true;
}

bool FullPath(const char* path, std::string& result) {
  const DWORD required = GetFullPathNameA(path, 0, nullptr, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied =
      GetFullPathNameA(path, required, buffer.data(), nullptr);
  if (copied == 0 || copied >= required) return false;
  result.assign(buffer.data(), copied);
  return true;
}

bool SamePath(const std::string& left, const std::string& right) {
  return _stricmp(left.c_str(), right.c_str()) == 0;
}

bool AtDirectory(const std::string& expected) {
  std::string current;
  return CurrentDirectory(current) && SamePath(current, expected);
}

bool Near(double left, double right) {
  return std::fabs(left - right) < 0.000001;
}

bool WriteChunk(CTaggedFile& file, const void* data, long size) {
  return file.Write(data, size) == size;
}

bool WritePalettePack(const std::string& path) {
  std::vector<unsigned char> palette(256 * 3);
  for (int color = 0; color < 256; ++color) {
    palette[color * 3] = static_cast<unsigned char>(color);
    palette[color * 3 + 1] = static_cast<unsigned char>(color);
    palette[color * 3 + 2] = static_cast<unsigned char>(color);
  }

  std::vector<unsigned char> gouraud(16 * 256);
  std::vector<unsigned char> table(16 * 256);
  for (int shade = 0; shade < 16; ++shade) {
    for (int color = 0; color < 256; ++color) {
      const unsigned char value = static_cast<unsigned char>(color);
      gouraud[shade * 256 + color] = value;
      table[shade * 256 + color] = value;
    }
  }
  std::vector<unsigned char> lights(LIGHT_COLOR_COUNT * 32 * 256, 0);

  CTaggedFile file(false);
  if (!file.Create(path.c_str(), false)) return false;
  const unsigned char transparencyColor[3] = {32, 64, 96};
  const unsigned char hazeColors[CPaletteTranslator::NUM_HAZES][3] = {
      {96, 96, 96}, {16, 32, 64}};
  bool valid = file.Descend("PTP_", false, 2, false) == 2;
  valid = valid && file.Descend("PAL8", true, 0, false) == 0 &&
          WriteChunk(file, palette.data(),
                     static_cast<long>(palette.size())) &&
          file.Ascend();
  valid = valid && file.Descend("GOUR", true, 1, false) == 1 &&
          WriteChunk(file, gouraud.data(),
                     static_cast<long>(gouraud.size())) &&
          file.Ascend();
  valid = valid && file.Descend("TRAN", true, 1, false) == 1 &&
          file.WriteInt(1) &&
          WriteChunk(file, transparencyColor,
                     static_cast<long>(sizeof(transparencyColor))) &&
          WriteChunk(file, table.data(), static_cast<long>(table.size())) &&
          file.Ascend();
  for (int index = 0;
       valid && index < CPaletteTranslator::NUM_HAZES; ++index) {
    valid = file.Descend("HAZE", true, 0, false) == 0 &&
            WriteChunk(file, hazeColors[index], 3) && file.WriteInt(16) &&
            WriteChunk(file, table.data(), static_cast<long>(table.size())) &&
            file.Ascend();
  }
  valid = valid && file.Descend("LIG_", true, 0, false) == 0 &&
          file.WriteInt(LIGHT_COLOR_COUNT) &&
          WriteChunk(file, lights.data(), static_cast<long>(lights.size())) &&
          file.Ascend() && file.Ascend();
  const bool closed = file.Close(false);
  return valid && closed;
}

bool WriteFont(const std::string& path) {
  SFontFixtureHeader header = {};
  std::memcpy(header.id, "FIXF", sizeof(header.id));
  header.width = 1;
  header.height = 1;
  header.palette[255 * 3] = 255;
  header.palette[255 * 3 + 1] = 255;
  header.palette[255 * 3 + 2] = 255;
  for (int character = 0; character < 256; ++character) {
    header.glyphTable[character * 2] = 1;
    header.glyphTable[character * 2 + 1] = 0;
  }
  header.spriteSize = 1;
  const unsigned char sprite = 255;

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(&header), sizeof(header));
  output.write(reinterpret_cast<const char*>(&sprite), sizeof(sprite));
  return output.good();
}

bool WriteSceneHeader(const std::string& path) {
  CTaggedFile file(false);
  if (!file.Create(path.c_str(), false)) return false;
  const bool valid =
      file.Descend("SCEN", false, 0, false) == 0 &&
      file.Descend("SCEH", true, 0, false) == 0 && file.WriteInt(3) &&
      file.WriteInt(2) && file.WriteInt(17) && file.WriteDouble(4.5) &&
      file.WriteDouble(0.25) && file.WriteInt(4) && file.Ascend() &&
      file.Ascend();
  const bool closed = file.Close(false);
  return valid && closed;
}

bool WriteLevel(const std::string& directory, const char* config,
                bool validScene, bool validPalette) {
  if (!EnsureDirectory(directory) ||
      !WriteText(JoinPath(directory, "LEVEL.CFG"), config)) {
    return false;
  }
  if (validScene) {
    if (!WriteSceneHeader(JoinPath(directory, "world.sce"))) return false;
  } else if (!WriteText(JoinPath(directory, "world.sce"), "bad-scene")) {
    return false;
  }
  if (validPalette) {
    return WritePalettePack(JoinPath(directory, "default.ptp"));
  }
  return WriteText(JoinPath(directory, "default.ptp"), "bad-palette");
}

bool ExpectAssetFailure(const std::string& directory,
                        unsigned int expectedIssue,
                        const std::string& originalDirectory) {
  return RecoveredLevelRuntime_Prepare(directory.c_str()) != FALSE &&
         RecoveredLevelAssets_Initialize() == FALSE &&
         RecoveredLevelAssets_Issues() == expectedIssue &&
         !RecoveredLevelAssets_IsReady() &&
         !RecoveredLevelRuntime_IsPrepared() &&
         !ViewFigureLibrary_IsReady() && AtDirectory(originalDirectory);
}

const char kConfig[] =
    "[Scene]\r\n"
    "Load=world.sce\r\n"
    "\r\n"
    "[Visual]\r\n"
    "HazeMin=225\r\n"
    "HazeDist=75\r\n"
    "HazeMinW=2\r\n"
    "HazeDistW=35\r\n"
    "FogMode=2\r\n"
    "NearClip=0.3\r\n"
    "Waterline=31.5\r\n"
    "\r\n"
    "[Debug]\r\n"
    "BSPCheck=0\r\n";

const char kBspConfig[] =
    "[Scene]\r\n"
    "Load=world.sce\r\n"
    "[Debug]\r\n"
    "BSPCheck=1\r\n";

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    return Fail("expected a fixture root and optional retail level");
  }

  std::string originalDirectory;
  std::string fixtureRoot;
  if (!CurrentDirectory(originalDirectory) || !FullPath(argv[1], fixtureRoot) ||
      !EnsureDirectory(fixtureRoot)) {
    return Fail("could not establish fixture root");
  }

  const std::string validLevel = JoinPath(fixtureRoot, "valid-level");
  const std::string invalidScene = JoinPath(fixtureRoot, "invalid-scene");
  const std::string invalidPalette = JoinPath(fixtureRoot, "invalid-palette");
  const std::string bspLevel = JoinPath(fixtureRoot, "bsp-level");
  const std::string fontPath = JoinPath(fixtureRoot, "figs5x3c.fnt");
  if (!WriteFont(fontPath) ||
      !WriteLevel(validLevel, kConfig, true, true) ||
      !WriteLevel(invalidScene, kConfig, false, true) ||
      !WriteLevel(invalidPalette, kConfig, true, false) ||
      !WriteLevel(bspLevel, kBspConfig, true, true)) {
    return Fail("could not write asset fixtures");
  }

  if (!RecoveredSoftwareGraph_Initialize(nullptr)) {
    return Fail("headless graph initialization failed");
  }
  GameEntry_UseRecoveredRuntime();
  const unsigned int missingHooks = GameEntry_RuntimeMissingHooks();
  if ((missingHooks & GAME_ENTRY_MISSING_LEVEL_INIT) == 0 ||
      (missingHooks & GAME_ENTRY_MISSING_LEVEL_DEINIT) != 0 ||
      (missingHooks & GAME_ENTRY_MISSING_CONFIG) != 0) {
    return Fail("asset bootstrap was misreported as full Level init");
  }

  if (RecoveredLevelAssets_Initialize() != FALSE ||
      RecoveredLevelAssets_Issues() !=
          RECOVERED_LEVEL_ASSET_MISSING_LEVEL ||
      !AtDirectory(originalDirectory)) {
    return Fail("asset initialization did not require a prepared level");
  }
  if (!ExpectAssetFailure(invalidScene,
                          RECOVERED_LEVEL_ASSET_INVALID_SCENE,
                          originalDirectory)) {
    return Fail("invalid scene header did not roll back atomically");
  }
  if (!ExpectAssetFailure(invalidPalette,
                          RECOVERED_LEVEL_ASSET_INVALID_PALETTE,
                          originalDirectory)) {
    return Fail("invalid palette did not roll back atomically");
  }
  if (!ExpectAssetFailure(bspLevel,
                          RECOVERED_LEVEL_ASSET_BSP_CHECK_REQUESTED,
                          originalDirectory)) {
    return Fail("legacy fatal BSP check request was not rejected safely");
  }

  const std::string validPalettePath =
      JoinPath(validLevel, "default.ptp");
  if (DeleteFileA(validPalettePath.c_str()) == FALSE ||
      !ExpectAssetFailure(validLevel,
                          RECOVERED_LEVEL_ASSET_MISSING_PALETTE,
                          originalDirectory) ||
      !WritePalettePack(validPalettePath)) {
    return Fail("missing palette path did not roll back atomically");
  }

  const std::string validScenePath = JoinPath(validLevel, "world.sce");
  if (DeleteFileA(validScenePath.c_str()) == FALSE ||
      RecoveredLevelRuntime_Prepare(validLevel.c_str()) != FALSE ||
      RecoveredLevelRuntime_Issues() != RECOVERED_LEVEL_MISSING_SCENE ||
      !AtDirectory(originalDirectory) || !WriteSceneHeader(validScenePath)) {
    return Fail("missing scene did not remain a pre-asset failure");
  }

  if (DeleteFileA(fontPath.c_str()) == FALSE ||
      !ExpectAssetFailure(validLevel, RECOVERED_LEVEL_ASSET_MISSING_FONT,
                          originalDirectory) ||
      !WriteText(fontPath, "bad-font") ||
      !ExpectAssetFailure(validLevel, RECOVERED_LEVEL_ASSET_INVALID_FONT,
                          originalDirectory) ||
      !WriteFont(fontPath)) {
    return Fail("font failures did not roll back atomically");
  }

  if (!RecoveredLevelRuntime_Prepare(validLevel.c_str()) ||
      !RecoveredLevelAssets_Initialize() ||
      !RecoveredLevelAssets_IsReady() ||
      !RecoveredLevelRuntime_IsPrepared() ||
      !ViewFigureLibrary_IsReady() || !ZAV_IsLevelShutdownArmed() ||
      RecoveredLevelAssets_Issues() != 0) {
    return Fail("valid asset transaction did not commit");
  }

  const SRecoveredSceneHeader* header =
      RecoveredLevelAssets_SceneHeader();
  if (header == nullptr || header->namedBases != 3 ||
      header->directBases != 2 || header->totalReferences != 17 ||
      header->reductions != 4 || !Near(header->cellSize, 4.5) ||
      !Near(header->heightRatio, 0.25) ||
      !Near(__terrainWaterline, 31.5) || _dL.currDevice == nullptr ||
      _dL.currDevice->fogCurrent != 2 || _gr_pTransparency == nullptr ||
      _gr_nTranspCount != 1 || _gr_pGouraud == nullptr ||
      paletteTranslator.Haze(0).pTable == nullptr ||
      paletteTranslator.Haze(1).pTable == nullptr || nWhiteColor != 255) {
    return Fail("committed asset state diverged from the fixture");
  }

  std::memset(_gr_pScreen, 0,
              static_cast<std::size_t>(_gr_nScreenWidth) *
                  _gr_nScreenHeight);
  if (!font5.PrintAt(0, 0, "A") ||
      _gr_pScreen[static_cast<std::size_t>(_gr_nScreenOriginY) *
                      _gr_nScreenWidth +
                  _gr_nScreenOriginX] != 255) {
    return Fail("fixed font was not reconstructed for the software graph");
  }

  ZAV_DeInitLevel();
  if (RecoveredLevelAssets_IsReady() ||
      RecoveredLevelAssets_SceneHeader() != nullptr ||
      RecoveredLevelRuntime_IsPrepared() || ViewFigureLibrary_IsReady() ||
      ZAV_IsLevelShutdownArmed() || _gr_pTransparency != nullptr ||
      _gr_nTranspCount != 0 || _gr_pGouraud != nullptr ||
      !AtDirectory(originalDirectory)) {
    return Fail("level shutdown did not release the asset transaction");
  }

  if (argc == 3) {
    if (!RecoveredLevelRuntime_Prepare(argv[2]) ||
        !RecoveredLevelAssets_Initialize()) {
      return Fail("retail level assets did not validate");
    }
    const SRecoveredSceneHeader* retailHeader =
        RecoveredLevelAssets_SceneHeader();
    if (retailHeader == nullptr || retailHeader->cellSize <= 0.0 ||
        retailHeader->heightRatio <= 0.0) {
      return Fail("retail scene header was not published");
    }
    ZAV_DeInitLevel();
  }

  if (!RecoveredLevelRuntime_Prepare(validLevel.c_str()) ||
      !RecoveredLevelAssets_Initialize()) {
    return Fail("asset transaction did not recover after shutdown");
  }
  ZAV_Deinit();
  if (RecoveredSoftwareGraph_IsReady() ||
      RecoveredLevelAssets_IsReady() ||
      RecoveredLevelRuntime_IsPrepared() || ViewFigureLibrary_IsReady() ||
      !AtDirectory(originalDirectory)) {
    return Fail("graph shutdown did not cascade through level assets");
  }
  ZAV_Deinit();
  return EXIT_SUCCESS;
}
