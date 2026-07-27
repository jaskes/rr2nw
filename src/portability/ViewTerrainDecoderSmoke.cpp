#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "RecoveredTerrainRuntime.h"
#include "ZavShutdownState.h"

namespace {

const char* const kSyntheticFiles[] = {
    "covh7.spr", "mapc7.spr", "hrange.spr", "red.spr",
    "maskflag.spr", "Mask9.bmp", "Mask19.bmp", "bump.bmp",
    "maskw.bmp", "bumpw.bmp",
};

int Fail(const char* message) {
  std::fprintf(stderr, "view-terrain-decoder-smoke: %s\n", message);
  return EXIT_FAILURE;
}

std::string JoinPath(const std::string& directory, const char* name) {
  return directory + "\\" + name;
}

bool WriteZeros(std::ofstream& output, std::uint64_t bytes) {
  const char zeros[4096] = {};
  while (bytes != 0) {
    const std::uint64_t count64 =
        (std::min)(bytes, static_cast<std::uint64_t>(sizeof(zeros)));
    const std::streamsize count = static_cast<std::streamsize>(count64);
    if (!output.write(zeros, count)) return false;
    bytes -= count64;
  }
  return true;
}

bool WriteSprite(const std::string& path, std::uint16_t width,
                 std::uint16_t height, unsigned int bytesPerPixel,
                 bool includePixels = true) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  const unsigned char header[5] = {
      static_cast<unsigned char>(width & 0xff),
      static_cast<unsigned char>(width >> 8),
      static_cast<unsigned char>(height & 0xff),
      static_cast<unsigned char>(height >> 8), 0};
  if (!output.write(reinterpret_cast<const char*>(header), sizeof(header))) {
    return false;
  }
  if (includePixels &&
      !WriteZeros(output, static_cast<std::uint64_t>(width) * height *
                              bytesPerPixel)) {
    return false;
  }
  output.close();
  return output.good();
}

bool WriteBitmap(const std::string& path, LONG dimension) {
  const DWORD imageBytes = static_cast<DWORD>(dimension * dimension);
  const DWORD pixelOffset = static_cast<DWORD>(
      sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
      256 * sizeof(RGBQUAD));

  BITMAPFILEHEADER fileHeader = {};
  fileHeader.bfType = 0x4D42;
  fileHeader.bfSize = pixelOffset + imageBytes;
  fileHeader.bfOffBits = pixelOffset;

  BITMAPINFOHEADER infoHeader = {};
  infoHeader.biSize = sizeof(infoHeader);
  infoHeader.biWidth = dimension;
  infoHeader.biHeight = dimension;
  infoHeader.biPlanes = 1;
  infoHeader.biBitCount = 8;
  infoHeader.biCompression = BI_RGB;
  infoHeader.biSizeImage = imageBytes;
  infoHeader.biClrUsed = 256;

  const RGBQUAD palette[256] = {};
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output.write(reinterpret_cast<const char*>(&fileHeader),
                    sizeof(fileHeader)) ||
      !output.write(reinterpret_cast<const char*>(&infoHeader),
                    sizeof(infoHeader)) ||
      !output.write(reinterpret_cast<const char*>(palette), sizeof(palette)) ||
      !WriteZeros(output, imageBytes)) {
    return false;
  }
  output.close();
  return output.good();
}

class CTemporaryTerrainDirectory {
 public:
  bool Create() {
    char temporaryRoot[MAX_PATH] = {};
    char uniquePath[MAX_PATH] = {};
    const DWORD rootLength = GetTempPathA(MAX_PATH, temporaryRoot);
    if (rootLength == 0 || rootLength >= MAX_PATH ||
        GetTempFileNameA(temporaryRoot, "r2t", 0, uniquePath) == 0 ||
        DeleteFileA(uniquePath) == FALSE ||
        CreateDirectoryA(uniquePath, nullptr) == FALSE) {
      return false;
    }
    path_ = uniquePath;
    return true;
  }

  ~CTemporaryTerrainDirectory() {
    if (path_.empty()) return;
    for (const char* name : kSyntheticFiles) {
      DeleteFileA(JoinPath(path_, name).c_str());
    }
    RemoveDirectoryA(path_.c_str());
  }

  const std::string& Path() const { return path_; }

 private:
  std::string path_;
};

bool WriteSyntheticTerrain(const std::string& directory) {
  return WriteSprite(JoinPath(directory, "covh7.spr"), 512, 512, 1) &&
         WriteSprite(JoinPath(directory, "mapc7.spr"), 512, 512, 2) &&
         WriteSprite(JoinPath(directory, "hrange.spr"), 64, 64, 2) &&
         WriteSprite(JoinPath(directory, "red.spr"), 512, 512, 1) &&
         WriteSprite(JoinPath(directory, "maskflag.spr"), 512, 512, 1) &&
         WriteBitmap(JoinPath(directory, "Mask9.bmp"), 512) &&
         WriteBitmap(JoinPath(directory, "Mask19.bmp"), 512) &&
         WriteBitmap(JoinPath(directory, "bump.bmp"), 256) &&
         WriteBitmap(JoinPath(directory, "maskw.bmp"), 256) &&
         WriteBitmap(JoinPath(directory, "bumpw.bmp"), 256);
}

bool RunSyntheticPreflight() {
  RecoveredTerrain_Release();
  RecoveredTerrain_Release();
  if (RecoveredTerrain_Initialize() != FALSE ||
      RecoveredTerrain_IsReady() ||
      RecoveredTerrain_Issues() != RECOVERED_TERRAIN_MISSING_LEVEL) {
    return false;
  }

  CTemporaryTerrainDirectory directory;
  if (!directory.Create() || !WriteSyntheticTerrain(directory.Path())) {
    return false;
  }
  if (RecoveredTerrain_ValidateDirectory(directory.Path().c_str()) != 0) {
    return false;
  }

  const std::string red = JoinPath(directory.Path(), "red.spr");
  if (!WriteSprite(red, 512, 512, 1, false) ||
      RecoveredTerrain_ValidateDirectory(directory.Path().c_str()) !=
          RECOVERED_TERRAIN_INVALID_RESOURCE ||
      !WriteSprite(red, 512, 512, 1)) {
    return false;
  }

  const std::string bump = JoinPath(directory.Path(), "bump.bmp");
  if (DeleteFileA(bump.c_str()) == FALSE ||
      RecoveredTerrain_ValidateDirectory(directory.Path().c_str()) !=
          RECOVERED_TERRAIN_MISSING_RESOURCE ||
      !WriteBitmap(bump, 256)) {
    return false;
  }

  return RecoveredTerrain_ValidateDirectory(nullptr) ==
             RECOVERED_TERRAIN_MISSING_LEVEL &&
         RecoveredTerrain_ValidateDirectory(directory.Path().c_str()) == 0;
}

struct STerrainSummary {
  unsigned int minimum;
  unsigned int maximum;
  std::uint64_t checksum;
};

STerrainSummary Summarize(const byte* heights) {
  STerrainSummary summary = {255, 0, 1469598103934665603ULL};
  for (std::size_t index = 0; index < 512U * 512U; ++index) {
    const unsigned int value = heights[index];
    summary.minimum = (std::min)(summary.minimum, value);
    summary.maximum = (std::max)(summary.maximum, value);
    summary.checksum ^= value;
    summary.checksum *= 1099511628211ULL;
  }
  return summary;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }
  if (!RunSyntheticPreflight()) {
    return Fail("synthetic terrain preflight contract failed");
  }
  if (argc == 1) return EXIT_SUCCESS;

  if (!RecoveredSoftwareGraph_Initialize(nullptr)) {
    return Fail("headless graph initialization failed");
  }
  GameEntry_UseRecoveredRuntime();
  if (!RecoveredLevelRuntime_Prepare(argv[1]) ||
      !RecoveredLevelAssets_Initialize() || !RecoveredTerrain_Initialize()) {
    std::fprintf(stderr, "terrain issues=%u\n", RecoveredTerrain_Issues());
    RecoveredTerrain_Release();
    ZAV_Deinit();
    return Fail("retail terrain assets did not initialize");
  }

  const SRecoveredSceneHeader* header = RecoveredLevelAssets_SceneHeader();
  const SRecoveredLevelSettings* settings = RecoveredLevelRuntime_Settings();
  _CViewTerrain* terrain = RecoveredTerrain_Get();
  bool valid = header != nullptr && settings != nullptr && terrain != nullptr &&
               RecoveredTerrain_IsReady();
  STerrainSummary summary = {};
  if (valid) {
    const byte* heights = terrain->GetHeightMap();
    const double expectedWaterline =
        settings->waterline * header->cellSize * header->heightRatio;
    valid = heights != nullptr && header->cellSize > 0.0 &&
            header->heightRatio > 0.0 &&
            std::fabs(terrain->CellSize() - header->cellSize) < 1e-9 &&
            std::fabs(terrain->Waterline() - expectedWaterline) < 1e-9;
    if (!valid) {
      std::fprintf(stderr,
                   "terrain validation: heights=%p cell=%lg/%lg "
                   "waterline=%lg/%lg\n",
                   static_cast<const void*>(heights), terrain->CellSize(),
                   header->cellSize, terrain->Waterline(), expectedWaterline);
    }
    if (valid) {
      summary = Summarize(heights);
      valid = summary.minimum <= summary.maximum;
    }
  }

  RecoveredTerrain_Release();
  valid = valid && !RecoveredTerrain_IsReady() &&
          RecoveredSoftwareGraph_IsReady() &&
          RecoveredLevelAssets_IsReady() &&
          RecoveredLevelRuntime_IsPrepared();
  ZAV_Deinit();
  if (!valid || RecoveredSoftwareGraph_IsReady() ||
      RecoveredLevelAssets_IsReady() ||
      RecoveredLevelRuntime_IsPrepared()) {
    return Fail("retail terrain did not load and release cleanly");
  }

  std::printf("terrain min=%u max=%u checksum=%016llx\n", summary.minimum,
              summary.maximum,
              static_cast<unsigned long long>(summary.checksum));
  return EXIT_SUCCESS;
}
