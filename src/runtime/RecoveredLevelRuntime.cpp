#include "RecoveredLevelRuntime.h"

#include <cctype>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "filesys.h"

#include "RecoveredSoftwareGraph.h"
#include "ZavShutdownState.h"

namespace {

const char kLevelConfigName[] = "level.cfg";
const char kDefaultSceneName[] = "1.sce";
const std::streamoff kMaximumConfigSize = 16 * 1024 * 1024;

std::unique_ptr<CConfigFile> g_config;
SRecoveredLevelSettings g_settings = {};
std::string g_directory;
std::string g_restoreDirectory;
std::string g_sceneFile;
unsigned int g_issues = 0;
bool g_prepared = false;

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

std::string JoinPath(const std::string& directory, const char* name) {
  if (directory.empty() || directory.back() == '\\' ||
      directory.back() == '/') {
    return directory + name;
  }
  return directory + "\\" + name;
}

bool IsDirectory(const std::string& path) {
  const DWORD attributes = GetFileAttributesA(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsRegularFile(const std::string& path) {
  const DWORD attributes = GetFileAttributesA(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsWhitespaceOnly(const std::string& text, std::size_t begin,
                      std::size_t end) {
  for (std::size_t index = begin; index < end; ++index) {
    if (std::isspace(static_cast<unsigned char>(text[index])) == 0) {
      return false;
    }
  }
  return true;
}

bool ValidateLegacyConfig(const std::string& text) {
  bool hasSection = false;
  std::size_t position = 0;
  const std::size_t controlZ = text.find('\x1a');
  const std::size_t end =
      controlZ == std::string::npos ? text.size() : controlZ;

  while (position < end) {
    std::size_t lineEnd = position;
    while (lineEnd < end && text[lineEnd] != '\r' &&
           text[lineEnd] != '\n') {
      ++lineEnd;
    }

    if (lineEnd != position) {
      const char first = text[position];
      if (first == '#') {
        // Legacy comments must begin in column zero.
      } else if (first == '[') {
        const std::size_t close = text.find(']', position + 1);
        if (close == std::string::npos || close >= lineEnd ||
            !IsWhitespaceOnly(text, close + 1, lineEnd)) {
          return false;
        }
        hasSection = true;
      } else if (std::isspace(static_cast<unsigned char>(first)) != 0) {
        if (!IsWhitespaceOnly(text, position, lineEnd)) return false;
      } else {
        if (!hasSection || text.find('=', position) >= lineEnd) return false;
      }
    }

    position = lineEnd;
    if (position < end && text[position] == '\r') ++position;
    if (position < end && text[position] == '\n') ++position;
  }
  return true;
}

bool ReadAndValidateConfig(const std::string& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) return false;

  const std::streamoff size = input.tellg();
  if (size <= 0 || size > kMaximumConfigSize) return false;
  input.seekg(0, std::ios::beg);

  std::string text(static_cast<std::size_t>(size), '\0');
  if (size != 0 &&
      !input.read(&text[0], static_cast<std::streamsize>(size))) {
    return false;
  }
  return ValidateLegacyConfig(text);
}

SRecoveredLevelSettings ReadSettings(CConfigFile& config) {
  SRecoveredLevelSettings settings = {};
  settings.hazeMin = 400;
  settings.hazeDistance = 128;
  config("Visual", "HazeMin", "%d", &settings.hazeMin);
  config("Visual", "HazeDist", "%d", &settings.hazeDistance);
  settings.hazeMinWater = config.GetInt("Visual", "HazeMinW", 10);
  settings.hazeDistanceWater =
      config.GetInt("Visual", "HazeDistW", settings.hazeDistance);
  settings.frameInfo = config.GetInt("Visual", "FrameInfo", 0);
  settings.loadTextures = config.GetInt("Visual", "LoadTextures", 1);
  settings.randomPosition = config.GetInt("Debug", "RandomPos", 0);
  settings.fogMode = config.GetInt("Visual", "FogMode", 0);
  settings.bspCheck = config.GetInt("Debug", "BSPCheck", 0);
  settings.debugLog = config.GetInt("Debug", "Log", 0);
  settings.nearClip = config.GetDouble("Visual", "NearClip", 1.0);
  settings.waterline = config.GetDouble("Visual", "Waterline", 200.0);
  settings.focus = config.GetDouble("Visual", "Focus", 1.0) *
                   RecoveredSoftwareGraph_Width() * 5.0 / 8.0;
  settings.terrainReduction = config.GetDouble("Visual", "Terrain0", 4.0);
  return settings;
}

bool ResetState() {
  if (g_prepared || g_config != nullptr || !g_restoreDirectory.empty()) {
    ZAV_DeinitLevelResources();
  }
  g_config.reset();

  bool restored = true;
  if (!g_restoreDirectory.empty()) {
    restored = SetCurrentDirectoryA(g_restoreDirectory.c_str()) != FALSE;
  }

  g_settings = SRecoveredLevelSettings{};
  g_directory.clear();
  g_restoreDirectory.clear();
  g_sceneFile.clear();
  g_prepared = false;
  return restored;
}

int Fail(unsigned int issue) {
  g_issues |= issue;
  if (!ResetState()) g_issues |= RECOVERED_LEVEL_PATH_FAILURE;
  return FALSE;
}

}  // namespace

int RecoveredLevelRuntime_Prepare(const char* directory) {
  g_issues = 0;
  if (!ResetState()) {
    g_issues |= RECOVERED_LEVEL_PATH_FAILURE;
    return FALSE;
  }
  if (!RecoveredSoftwareGraph_IsReady()) {
    g_issues |= RECOVERED_LEVEL_MISSING_GRAPH;
    return FALSE;
  }
  if (directory == nullptr || directory[0] == 0) {
    g_issues |= RECOVERED_LEVEL_INVALID_DIRECTORY;
    return FALSE;
  }

  std::string targetDirectory;
  if (!FullPath(directory, targetDirectory) ||
      !IsDirectory(targetDirectory)) {
    g_issues |= RECOVERED_LEVEL_INVALID_DIRECTORY;
    return FALSE;
  }

  std::string originalDirectory;
  if (!CurrentDirectory(originalDirectory)) {
    g_issues |= RECOVERED_LEVEL_PATH_FAILURE;
    return FALSE;
  }

  const std::string configPath =
      JoinPath(targetDirectory, kLevelConfigName);
  if (!IsRegularFile(configPath)) {
    g_issues |= RECOVERED_LEVEL_MISSING_CONFIG;
    return FALSE;
  }
  if (!ReadAndValidateConfig(configPath)) {
    g_issues |= RECOVERED_LEVEL_INVALID_CONFIG;
    return FALSE;
  }

  if (SetCurrentDirectoryA(targetDirectory.c_str()) == FALSE) {
    g_issues |= RECOVERED_LEVEL_PATH_FAILURE;
    return FALSE;
  }
  g_restoreDirectory = originalDirectory;
  g_directory = targetDirectory;

  try {
    g_config.reset(new CConfigFile(kLevelConfigName));
  } catch (...) {
    return Fail(RECOVERED_LEVEL_INVALID_CONFIG);
  }

  g_settings = ReadSettings(*g_config);
  const char* configuredScene = (*g_config)("Scene", "Load");
  g_sceneFile = configuredScene == nullptr ? kDefaultSceneName : configuredScene;
  if (g_sceneFile.empty() || !IsRegularFile(g_sceneFile)) {
    return Fail(RECOVERED_LEVEL_MISSING_SCENE);
  }

  g_prepared = true;
  return TRUE;
}

void RecoveredLevelRuntime_Release() {
  if (!ResetState()) g_issues |= RECOVERED_LEVEL_PATH_FAILURE;
}

bool RecoveredLevelRuntime_IsPrepared() { return g_prepared; }

unsigned int RecoveredLevelRuntime_Issues() { return g_issues; }

CConfigFile* RecoveredLevelRuntime_Config() { return g_config.get(); }

const SRecoveredLevelSettings* RecoveredLevelRuntime_Settings() {
  return g_prepared ? &g_settings : nullptr;
}

const char* RecoveredLevelRuntime_Directory() {
  return g_prepared ? g_directory.c_str() : nullptr;
}

const char* RecoveredLevelRuntime_SceneFile() {
  return g_prepared ? g_sceneFile.c_str() : nullptr;
}
