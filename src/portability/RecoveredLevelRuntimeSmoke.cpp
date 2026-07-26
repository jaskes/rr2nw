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

#include "filesys.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavShutdownState.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "recovered-level-runtime-smoke: %s\n", message);
  return EXIT_FAILURE;
}

std::string JoinPath(const std::string& directory, const char* name) {
  return directory + "\\" + name;
}

bool EnsureDirectory(const std::string& path) {
  if (CreateDirectoryA(path.c_str(), nullptr) != FALSE) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool WriteFile(const std::string& path, const char* contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  return output.good();
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

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    return Fail("expected a fixture root and optional retail level");
  }

  std::string originalDirectory;
  std::string fixtureRoot;
  if (!CurrentDirectory(originalDirectory) ||
      !FullPath(argv[1], fixtureRoot) || !EnsureDirectory(fixtureRoot)) {
    return Fail("could not establish fixture root");
  }

  const std::string valid = JoinPath(fixtureRoot, "valid-level");
  const std::string defaultScene =
      JoinPath(fixtureRoot, "default-scene-level");
  const std::string missingConfig =
      JoinPath(fixtureRoot, "missing-config-level");
  const std::string invalidConfig =
      JoinPath(fixtureRoot, "invalid-config-level");
  const std::string emptyConfig =
      JoinPath(fixtureRoot, "empty-config-level");
  const std::string missingScene =
      JoinPath(fixtureRoot, "missing-scene-level");
  if (!EnsureDirectory(valid) || !EnsureDirectory(defaultScene) ||
      !EnsureDirectory(missingConfig) || !EnsureDirectory(invalidConfig) ||
      !EnsureDirectory(emptyConfig) || !EnsureDirectory(missingScene)) {
    return Fail("could not create level fixtures");
  }

  DeleteFileA(JoinPath(missingConfig, "LEVEL.CFG").c_str());
  if (!WriteFile(JoinPath(valid, "LEVEL.CFG"),
                 "[Scene]\r\n"
                 "Load=world.sce\r\n"
                 "\r\n"
                 "[Visual]\r\n"
                 "HazeMin=225\r\n"
                 "HazeDist=75\r\n"
                 "HazeMinW=2\r\n"
                 "HazeDistW=35\r\n"
                 "FrameInfo=1\r\n"
                 "LoadTextures=0\r\n"
                 "FogMode=2\r\n"
                 "NearClip=0.3\r\n"
                 "Waterline=31.5\r\n"
                 "Focus=1.25\r\n"
                 "Terrain0=3.5\r\n"
                 "\r\n"
                 "[Debug]\r\n"
                 "RandomPos=1\r\n"
                 "BSPCheck=1\r\n"
                 "Log=1\r\n") ||
      !WriteFile(JoinPath(valid, "world.sce"), "scene-fixture") ||
      !WriteFile(JoinPath(defaultScene, "LEVEL.CFG"),
                 "[Visual]\r\nFocus=1\r\n") ||
      !WriteFile(JoinPath(defaultScene, "1.sce"), "scene-fixture") ||
      !WriteFile(JoinPath(invalidConfig, "LEVEL.CFG"),
                 " [Visual]\r\nFocus=1\r\n") ||
      !WriteFile(JoinPath(invalidConfig, "1.sce"), "scene-fixture") ||
      !WriteFile(JoinPath(emptyConfig, "LEVEL.CFG"), "") ||
      !WriteFile(JoinPath(emptyConfig, "1.sce"), "scene-fixture") ||
      !WriteFile(JoinPath(missingScene, "LEVEL.CFG"),
                 "[Scene]\r\nLoad=absent.sce\r\n")) {
    return Fail("could not write level fixtures");
  }

  if (RecoveredLevelRuntime_Prepare(valid.c_str()) != FALSE ||
      RecoveredLevelRuntime_Issues() != RECOVERED_LEVEL_MISSING_GRAPH ||
      !AtDirectory(originalDirectory)) {
    return Fail("level preparation did not require a graph");
  }

  if (!RecoveredSoftwareGraph_Initialize(nullptr)) {
    return Fail("headless graph initialization failed");
  }
  GameEntry_UseRecoveredRuntime();
  const unsigned int missingHooks = GameEntry_RuntimeMissingHooks();
  if ((missingHooks & GAME_ENTRY_MISSING_LEVEL_DEINIT) != 0 ||
      (missingHooks & GAME_ENTRY_MISSING_CONFIG) != 0 ||
      (missingHooks & GAME_ENTRY_MISSING_LEVEL_INIT) == 0) {
    return Fail("recovered level hooks were not published conservatively");
  }

  if (RecoveredLevelRuntime_Prepare("missing-level") != FALSE ||
      RecoveredLevelRuntime_Issues() !=
          RECOVERED_LEVEL_INVALID_DIRECTORY ||
      !AtDirectory(originalDirectory)) {
    return Fail("missing directory did not fail without a cwd change");
  }
  if (RecoveredLevelRuntime_Prepare(missingConfig.c_str()) != FALSE ||
      RecoveredLevelRuntime_Issues() != RECOVERED_LEVEL_MISSING_CONFIG ||
      !AtDirectory(originalDirectory)) {
    return Fail("missing config did not fail without a cwd change");
  }
  if (RecoveredLevelRuntime_Prepare(invalidConfig.c_str()) != FALSE ||
      RecoveredLevelRuntime_Issues() != RECOVERED_LEVEL_INVALID_CONFIG ||
      !AtDirectory(originalDirectory)) {
    return Fail("invalid config reached the fatal legacy parser");
  }
  if (RecoveredLevelRuntime_Prepare(emptyConfig.c_str()) != FALSE ||
      RecoveredLevelRuntime_Issues() != RECOVERED_LEVEL_INVALID_CONFIG ||
      !AtDirectory(originalDirectory)) {
    return Fail("empty config reached the fatal legacy parser");
  }
  if (RecoveredLevelRuntime_Prepare(missingScene.c_str()) != FALSE ||
      RecoveredLevelRuntime_Issues() != RECOVERED_LEVEL_MISSING_SCENE ||
      RecoveredLevelRuntime_Config() != nullptr ||
      !AtDirectory(originalDirectory)) {
    return Fail("missing scene did not roll back level state");
  }

  if (!RecoveredLevelRuntime_Prepare(valid.c_str()) ||
      !RecoveredLevelRuntime_IsPrepared() ||
      RecoveredLevelRuntime_Config() == nullptr ||
      RecoveredLevelRuntime_Settings() == nullptr ||
      std::strcmp(RecoveredLevelRuntime_SceneFile(), "world.sce") != 0 ||
      !SamePath(RecoveredLevelRuntime_Directory(), valid) ||
      !AtDirectory(valid)) {
    return Fail("valid uppercase LEVEL.CFG was not prepared");
  }

  const SRecoveredLevelSettings& settings =
      *RecoveredLevelRuntime_Settings();
  if (settings.hazeMin != 225 || settings.hazeDistance != 75 ||
      settings.hazeMinWater != 2 || settings.hazeDistanceWater != 35 ||
      settings.frameInfo != 1 || settings.loadTextures != 0 ||
      settings.randomPosition != 1 || settings.fogMode != 2 ||
      settings.bspCheck != 1 || settings.debugLog != 1 ||
      !Near(settings.nearClip, 0.3) ||
      !Near(settings.waterline, 31.5) || !Near(settings.focus, 500.0) ||
      !Near(settings.terrainReduction, 3.5) ||
      &ZAV_Config() != RecoveredLevelRuntime_Config()) {
    return Fail("legacy pre-scene settings diverged");
  }

  ZAV_DeInitLevel();
  if (RecoveredLevelRuntime_IsPrepared() ||
      RecoveredLevelRuntime_Config() != nullptr ||
      !AtDirectory(originalDirectory)) {
    return Fail("level deinit did not release config and restore cwd");
  }
  ZAV_DeInitLevel();
  if (!AtDirectory(originalDirectory)) {
    return Fail("repeated level deinit was not idempotent");
  }

  if (!RecoveredLevelRuntime_Prepare(defaultScene.c_str()) ||
      std::strcmp(RecoveredLevelRuntime_SceneFile(), "1.sce") != 0 ||
      RecoveredLevelRuntime_Settings()->hazeMin != 400 ||
      RecoveredLevelRuntime_Settings()->hazeDistance != 128) {
    return Fail("legacy scene and haze defaults changed");
  }

  if (argc == 3) {
    ZAV_DeInitLevel();
    if (!RecoveredLevelRuntime_Prepare(argv[2]) ||
        RecoveredLevelRuntime_Config() == nullptr ||
        std::strcmp(RecoveredLevelRuntime_SceneFile(), "1.sce") != 0) {
      return Fail("retail level fixture did not prepare");
    }
    ZAV_DeInitLevel();
    if (!RecoveredLevelRuntime_Prepare(defaultScene.c_str())) {
      return Fail("synthetic level did not recover after retail validation");
    }
  }

  ZAV_Deinit();
  if (RecoveredSoftwareGraph_IsReady() ||
      RecoveredLevelRuntime_IsPrepared() ||
      RecoveredLevelRuntime_Config() != nullptr ||
      !AtDirectory(originalDirectory)) {
    return Fail("graph shutdown did not roll back prepared level state");
  }
  ZAV_Deinit();
  return EXIT_SUCCESS;
}
