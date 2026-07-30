#include "GameStartup.h"

#include "RR2NWBuildRevision.h"
#include "GameEntryRuntimeState.h"
#include "RecoveredArenaSeanceRuntime.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredGameServicesRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredRetailScriptManifest.h"
#include "ZavOverallInfoState.h"
#include "ZavShutdownState.h"
#include "graph.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "suavik.h"

#include <shlobj.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#ifndef RR2NW_BUILD_VERSION
#define RR2NW_BUILD_VERSION "unknown"
#endif

#ifndef RR2NW_BUILD_CONFIGURATION
#define RR2NW_BUILD_CONFIGURATION "unknown"
#endif

namespace rr2nw {
namespace {

constexpr int kSuccess = 0;
constexpr int kInvalidArguments = 2;
constexpr int kDataNotReady = 3;
constexpr int kRuntimeNotReady = 4;
constexpr int kDiagnosticsFailure = 5;
constexpr int kRetailLevelCount = 9;

struct StartupOptions {
  std::wstring dataDirectory;
  std::wstring diagnosticsDirectory;
  std::wstring startLevel;
  bool launchSmoke = false;
  bool runtimeSmoke = false;
  bool showHelp = false;
  bool showVersion = false;
};

struct RetailData {
  std::wstring root;
  std::array<std::wstring, kRetailLevelCount> levels;
  int startLevel = -1;
};

std::wstring Utf8ToWide(const char* text) {
  const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (length <= 1) {
    return std::wstring();
  }

  std::vector<wchar_t> result(static_cast<std::size_t>(length));
  MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), length);
  return std::wstring(result.data());
}

std::string WideToUtf8(const std::wstring& text) {
  if (text.empty()) {
    return std::string();
  }

  const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()), nullptr,
                                         0, nullptr, nullptr);
  if (length <= 0) {
    return std::string();
  }

  std::string result(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      &result[0], length, nullptr, nullptr);
  return result;
}

bool WideToSystemPath(const std::wstring& text, std::string* result) {
  if (text.empty()) {
    result->clear();
    return false;
  }

  BOOL usedDefaultCharacter = FALSE;
  const int length = WideCharToMultiByte(
      CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(),
      static_cast<int>(text.size()), nullptr, 0, nullptr,
      &usedDefaultCharacter);
  if (length <= 0 || usedDefaultCharacter != FALSE) {
    result->clear();
    return false;
  }

  result->assign(static_cast<std::size_t>(length), '\0');
  usedDefaultCharacter = FALSE;
  if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(),
                          static_cast<int>(text.size()), &(*result)[0],
                          length, nullptr, &usedDefaultCharacter) != length ||
      usedDefaultCharacter != FALSE) {
    result->clear();
    return false;
  }
  return true;
}

std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
  if (base.empty()) {
    return child;
  }
  if (base.back() == L'\\' || base.back() == L'/') {
    return base + child;
  }
  return base + L"\\" + child;
}

std::wstring AbsolutePath(const std::wstring& path) {
  const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
  if (length == 0) {
    return path;
  }

  std::vector<wchar_t> buffer(static_cast<std::size_t>(length));
  if (GetFullPathNameW(path.c_str(), length, buffer.data(), nullptr) == 0) {
    return path;
  }
  return std::wstring(buffer.data());
}

std::wstring ExecutableDirectory() {
  std::vector<wchar_t> buffer(512);
  for (;;) {
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return std::wstring();
    }
    if (length < buffer.size() - 1) {
      std::wstring path(buffer.data(), length);
      const std::wstring::size_type separator = path.find_last_of(L"\\/");
      return separator == std::wstring::npos ? std::wstring()
                                             : path.substr(0, separator);
    }
    buffer.resize(buffer.size() * 2);
  }
}

std::wstring CurrentDirectory() {
  const DWORD length = GetCurrentDirectoryW(0, nullptr);
  if (length == 0) {
    return std::wstring();
  }
  std::vector<wchar_t> buffer(static_cast<std::size_t>(length));
  if (GetCurrentDirectoryW(length, buffer.data()) == 0) {
    return std::wstring();
  }
  return std::wstring(buffer.data());
}

std::wstring EnvironmentValue(const wchar_t* name) {
  const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
  if (length == 0) {
    return std::wstring();
  }
  std::vector<wchar_t> buffer(static_cast<std::size_t>(length));
  if (GetEnvironmentVariableW(name, buffer.data(), length) == 0) {
    return std::wstring();
  }
  return std::wstring(buffer.data());
}

bool IsDirectory(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsFile(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool ParseOptionValue(int argc, wchar_t** argv, int* index,
                      const wchar_t* name, std::wstring* value,
                      std::wstring* failure) {
  if (*index + 1 >= argc) {
    *failure = std::wstring(L"missing value for ") + name;
    return false;
  }
  *value = argv[++(*index)];
  if (value->empty()) {
    *failure = std::wstring(L"empty value for ") + name;
    return false;
  }
  return true;
}

bool ParseOptions(int argc, wchar_t** argv, StartupOptions* options,
                  std::wstring* failure) {
  for (int index = 1; index < argc; ++index) {
    const std::wstring argument(argv[index]);
    if (argument == L"--launch-smoke") {
      options->launchSmoke = true;
    } else if (argument == L"--runtime-smoke") {
      options->runtimeSmoke = true;
    } else if (argument == L"--help" || argument == L"-h") {
      options->showHelp = true;
    } else if (argument == L"--version") {
      options->showVersion = true;
    } else if (argument == L"--data-dir") {
      if (!ParseOptionValue(argc, argv, &index, L"--data-dir",
                            &options->dataDirectory, failure)) {
        return false;
      }
    } else if (argument.compare(0, 11, L"--data-dir=") == 0) {
      options->dataDirectory = argument.substr(11);
    } else if (argument == L"--diagnostics-dir") {
      if (!ParseOptionValue(argc, argv, &index, L"--diagnostics-dir",
                            &options->diagnosticsDirectory, failure)) {
        return false;
      }
    } else if (argument.compare(0, 18, L"--diagnostics-dir=") == 0) {
      options->diagnosticsDirectory = argument.substr(18);
    } else if (argument == L"--start-level") {
      if (!ParseOptionValue(argc, argv, &index, L"--start-level",
                            &options->startLevel, failure)) {
        return false;
      }
    } else if (argument.compare(0, 14, L"--start-level=") == 0) {
      options->startLevel = argument.substr(14);
    } else {
      *failure = std::wstring(L"unknown argument: ") + argument;
      return false;
    }
  }
  return true;
}

std::wstring DefaultDiagnosticsDirectory() {
  std::wstring base = EnvironmentValue(L"LOCALAPPDATA");
  if (base.empty()) {
    base = EnvironmentValue(L"TEMP");
  }
  if (base.empty()) {
    base = CurrentDirectory();
  }
  return JoinPath(JoinPath(base, L"RR2NW"), L"logs");
}

bool EnsureDirectory(const std::wstring& path) {
  const int result = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
  return result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS ||
         result == ERROR_FILE_EXISTS;
}

class StartupLog {
 public:
  StartupLog() = default;
  ~StartupLog() {
    if (file_ != INVALID_HANDLE_VALUE) {
      CloseHandle(file_);
    }
  }

  bool Open(const std::wstring& directory) {
    if (!EnsureDirectory(directory)) {
      return false;
    }
    path_ = JoinPath(directory, L"rr2nw-startup.log");
    file_ = CreateFileW(path_.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) {
      return false;
    }
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    DWORD written = 0;
    return WriteFile(file_, bom, sizeof(bom), &written, nullptr) != FALSE &&
           written == sizeof(bom);
  }

  bool Line(const std::string& line) {
    const std::string record = line + "\r\n";
    DWORD written = 0;
    return file_ != INVALID_HANDLE_VALUE &&
           WriteFile(file_, record.data(), static_cast<DWORD>(record.size()),
                     &written, nullptr) != FALSE &&
           written == record.size();
  }

  bool WideLine(const char* key, const std::wstring& value) {
    return Line(std::string(key) + "=" + WideToUtf8(value));
  }

  const std::wstring& path() const { return path_; }

 private:
  HANDLE file_ = INVALID_HANDLE_VALUE;
  std::wstring path_;
};

bool InspectRetailData(const std::wstring& candidate, RetailData* data,
                       std::wstring* failure) {
  const std::wstring root = AbsolutePath(candidate);
  if (!IsDirectory(root)) {
    *failure = std::wstring(L"data directory does not exist: ") + root;
    return false;
  }

  const std::wstring configPath = JoinPath(root, L"game.cfg");
  if (!IsFile(configPath)) {
    *failure = std::wstring(L"game.cfg is missing in: ") + root;
    return false;
  }
  if (!IsFile(JoinPath(root, L"LEVEL0.SC"))) {
    *failure = std::wstring(L"LEVEL0.SC is missing in: ") + root;
    return false;
  }

  RetailData inspected;
  inspected.root = root;
  inspected.startLevel =
      GetPrivateProfileIntW(L"Init", L"StartLevel", -1, configPath.c_str());
  if (inspected.startLevel < 0 || inspected.startLevel >= kRetailLevelCount) {
    *failure = L"game.cfg has an invalid Init/StartLevel";
    return false;
  }

  for (int index = 0; index < kRetailLevelCount; ++index) {
    wchar_t key[16] = {};
    std::swprintf(key, sizeof(key) / sizeof(key[0]), L"%d", index);
    wchar_t value[260] = {};
    if (GetPrivateProfileStringW(L"Levels", key, L"", value,
                                 sizeof(value) / sizeof(value[0]),
                                 configPath.c_str()) == 0) {
      *failure = std::wstring(L"game.cfg is missing Levels/") + key;
      return false;
    }
    inspected.levels[static_cast<std::size_t>(index)] = value;
    if (!IsDirectory(JoinPath(root, value))) {
      *failure = std::wstring(L"configured level directory is missing: ") +
                 value;
      return false;
    }
  }

  *data = inspected;
  return true;
}

bool LocateRetailData(const StartupOptions& options, RetailData* data,
                      std::wstring* failure) {
  if (!options.dataDirectory.empty()) {
    return InspectRetailData(options.dataDirectory, data, failure);
  }

  const std::wstring executableDirectory = ExecutableDirectory();
  const std::wstring currentDirectory = CurrentDirectory();
  const std::array<std::wstring, 4> candidates = {
      executableDirectory, JoinPath(executableDirectory, L"nw"),
      currentDirectory, JoinPath(currentDirectory, L"nw")};

  for (const std::wstring& candidate : candidates) {
    std::wstring candidateFailure;
    if (!candidate.empty() &&
        InspectRetailData(candidate, data, &candidateFailure)) {
      return true;
    }
  }

  *failure = L"retail data was not found; pass --data-dir <path>";
  return false;
}

bool SelectStartLevel(const StartupOptions& options, RetailData* data,
                      std::wstring* failure) {
  if (options.startLevel.empty()) {
    return true;
  }

  errno = 0;
  wchar_t* end = nullptr;
  const long numeric = std::wcstol(options.startLevel.c_str(), &end, 10);
  if (errno == 0 && end != options.startLevel.c_str() && *end == L'\0') {
    if (numeric < 0 || numeric >= kRetailLevelCount) {
      *failure = L"--start-level index must be between 0 and 8";
      return false;
    }
    data->startLevel = static_cast<int>(numeric);
    return true;
  }

  for (int index = 0; index < kRetailLevelCount; ++index) {
    if (_wcsicmp(options.startLevel.c_str(),
                 data->levels[static_cast<std::size_t>(index)].c_str()) == 0) {
      data->startLevel = index;
      return true;
    }
  }

  *failure = L"--start-level must be an index from 0 to 8 or a Level name "
             L"listed in game.cfg";
  return false;
}

void ShowMessage(bool silent, UINT icon, const wchar_t* title,
                 const std::wstring& text) {
  if (!silent) {
    MessageBoxW(nullptr, text.c_str(), title, MB_OK | icon);
  }
}

std::wstring BuildIdentity() {
  return L"RR2NW " + Utf8ToWide(RR2NW_BUILD_VERSION) + L" (" +
         Utf8ToWide(RR2NW_BUILD_REVISION) + L", " +
         Utf8ToWide(RR2NW_BUILD_CONFIGURATION) + L")";
}

}  // namespace

int RunGameStartup(HINSTANCE instance, int argc, wchar_t** argv) {
  StartupOptions options;
  std::wstring failure;
  if (!ParseOptions(argc, argv, &options, &failure)) {
    ShowMessage(options.launchSmoke || options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW startup error",
                failure);
    return kInvalidArguments;
  }

  if (options.showHelp) {
    ShowMessage(false, MB_ICONINFORMATION, L"RR2NW command line",
                L"rr2nw.exe [--data-dir <path>] [--start-level <index|name>]\n"
                L"          [--diagnostics-dir <path>]\n"
                L"          [--launch-smoke] [--runtime-smoke]\n"
                L"          [--version] [--help]");
    return kSuccess;
  }
  if (options.showVersion) {
    ShowMessage(false, MB_ICONINFORMATION, L"RR2NW version", BuildIdentity());
    return kSuccess;
  }

  if (options.diagnosticsDirectory.empty()) {
    options.diagnosticsDirectory = DefaultDiagnosticsDirectory();
  } else {
    options.diagnosticsDirectory = AbsolutePath(options.diagnosticsDirectory);
  }

  StartupLog log;
  if (!log.Open(options.diagnosticsDirectory)) {
    ShowMessage(options.launchSmoke || options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW startup error",
                L"Cannot create the startup diagnostic log in:\n" +
                    options.diagnosticsDirectory);
    return kDiagnosticsFailure;
  }

  SYSTEMTIME utc = {};
  GetSystemTime(&utc);
  char timestamp[64] = {};
  std::snprintf(timestamp, sizeof(timestamp),
                "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", utc.wYear,
                utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond,
                utc.wMilliseconds);
  log.Line("timestamp_utc=" + std::string(timestamp));
  log.Line("version=" RR2NW_BUILD_VERSION);
  log.Line("revision=" RR2NW_BUILD_REVISION);
  log.Line("configuration=" RR2NW_BUILD_CONFIGURATION);
  log.Line("marker=process-ready");

  RetailData data;
  if (!LocateRetailData(options, &data, &failure)) {
    log.WideLine("failure", failure);
    log.Line("marker=data-not-ready");
    ShowMessage(options.launchSmoke || options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW data error",
                failure + L"\n\nDiagnostic log:\n" + log.path());
    return kDataNotReady;
  }
  if (!SelectStartLevel(options, &data, &failure)) {
    log.WideLine("failure", failure);
    log.WideLine("start_level_requested", options.startLevel);
    log.Line("marker=level-selection-invalid");
    ShowMessage(options.launchSmoke || options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW level selection error",
                failure + L"\n\nDiagnostic log:\n" + log.path());
    return kInvalidArguments;
  }

  log.WideLine("data_dir", data.root);
  log.Line("retail_level_count=9");
  log.Line(std::string("start_level_source=") +
           (options.startLevel.empty() ? "game.cfg" : "command-line"));
  if (!options.startLevel.empty()) {
    log.WideLine("start_level_requested", options.startLevel);
  }
  log.Line("start_level=" + std::to_string(data.startLevel));
  log.WideLine("start_level_dir",
               data.levels[static_cast<std::size_t>(data.startLevel)]);
  log.Line("data_access=read-only");
  log.Line("marker=retail-data-ready");

  if (options.launchSmoke) {
    log.Line("recovered_runtime=skipped-for-launch-smoke");
    log.Line("marker=pre-content-ready");
    return kSuccess;
  }

  std::string levelDirectory;
  if (!WideToSystemPath(
          JoinPath(data.root,
                   data.levels[static_cast<std::size_t>(data.startLevel)]),
          &levelDirectory)) {
    log.Line("failure=level path is not representable by the Windows ANSI "
             "code page");
    log.Line("marker=level-not-ready");
    ShowMessage(options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW runtime error",
                L"The selected Level path cannot be represented by the "
                L"current Windows ANSI code page.\n\nDiagnostic log:\n" +
                    log.path());
    return kRuntimeNotReady;
  }
  std::string ditherTablePath;
  if (!WideToSystemPath(JoinPath(data.root, L"DITH.DTH"),
                        &ditherTablePath)) {
    log.Line("failure=DITH.DTH path is not representable by the Windows ANSI "
             "code page");
    log.Line("marker=level-not-ready");
    return kRuntimeNotReady;
  }

  RecoveredGameServices_UseRuntime();
  const bool graphInitialized = ZAV_InitGraph(instance) != FALSE;
  log.Line(std::string("graph_initialized=") +
           (graphInitialized ? "1" : "0"));
  const bool ditherTableLoaded =
      graphInitialized &&
      GRSoftwareLoadDitherTable(ditherTablePath.c_str()) != FALSE;
  log.Line(std::string("software_dither_table_loaded=") +
           (ditherTableLoaded ? "1" : "0"));
  const bool levelInitialized =
      ditherTableLoaded && ZAV_InitLevel(levelDirectory.c_str()) != FALSE;
  log.Line(std::string("level_initialized=") +
           (levelInitialized ? "1" : "0"));
  if (!levelInitialized || !RecoveredGameLevel_IsReady()) {
    log.Line("game_entry_issues=" +
             std::to_string(GameEntry_RuntimeIssues()));
    log.Line("game_entry_missing_hooks=" +
             std::to_string(GameEntry_RuntimeMissingHooks()));
    log.Line("game_level_issues=" +
             std::to_string(RecoveredGameLevel_Issues()));
    log.Line("level_runtime_issues=" +
             std::to_string(RecoveredLevelRuntime_Issues()));
    log.Line("retail_script_manifest_issues=" +
             std::to_string(RecoveredRetailScriptManifest_Issues()));
    if (RecoveredRetailScriptManifest_LastError()[0] != 0) {
      log.Line(std::string("retail_script_manifest_error=") +
               RecoveredRetailScriptManifest_LastError());
    }
    log.Line("level_asset_issues=" +
             std::to_string(RecoveredLevelAssets_Issues()));
    log.Line("drawable_scene_issues=" +
             std::to_string(RecoveredDrawableScene_Issues()));
    log.Line("game_services_issues=" +
             std::to_string(RecoveredGameServices_Issues()));
    log.Line("marker=level-not-ready");
    RecoveredGameServices_Release();
    ZAV_Deinit();
    ShowMessage(options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW runtime error",
                L"The recovered runtime could not construct the selected "
                L"Level.\n\nDiagnostic log:\n" + log.path());
    return kRuntimeNotReady;
  }

  const SRecoveredDrawableSceneSummary* summary =
      RecoveredDrawableScene_Summary();
  const SRecoveredRetailScriptManifestSummary* scriptManifest =
      RecoveredRetailScriptManifest_Summary();
  if (summary == nullptr || scriptManifest == nullptr) {
    log.Line("failure=published Level has no drawable scene/script manifest "
             "summary");
    log.Line("marker=level-not-ready");
    ZAV_Deinit();
    return kRuntimeNotReady;
  }

  PIN_InitEverything();
  log.Line("platform_initialized=1");
  SUA_InitEverything();
  log.Line("session_initialized=" +
           std::to_string(RecoveredGameServices_SessionReady() ? 1 : 0));
  log.Line("arena_seance_initialized=" +
           std::to_string(RecoveredGameServices_SeanceReady() ? 1 : 0));
  log.Line("bird_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_BirdAttributesReady() ? 1 : 0));
  log.Line("portal_table_initialized=" +
           std::to_string(RecoveredGameServices_PortalReady() ? 1 : 0));
  log.Line("orphan_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_OrphanAttributesReady() ? 1 : 0));
  log.Line("orphan_references_initialized=" +
           std::to_string(
               RecoveredGameServices_OrphanReferencesReady() ? 1 : 0));
  log.Line("orphan_reference_fingerprint=" + std::to_string(
               RecoveredGameServices_OrphanReferenceFingerprint()));
  log.Line("orphan_subject_initialized=" +
           std::to_string(
               RecoveredGameServices_OrphanSubjectReady() ? 1 : 0));
  log.Line("orphan_subject_capacity=" + std::to_string(
               RecoveredGameServices_OrphanSubjectCapacity()));
  log.Line("orphan_subject_count=" + std::to_string(
               RecoveredGameServices_OrphanSubjectCount()));
  log.Line("orphan_subject_fingerprint=" + std::to_string(
               RecoveredGameServices_OrphanSubjectFingerprint()));
  log.Line("artefact_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_ArtefactAttributesReady() ? 1 : 0));
  log.Line("smoke_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokeAttributesReady() ? 1 : 0));
  log.Line("smoke_subject_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokeSubjectReady() ? 1 : 0));
  log.Line("smoke_simulation_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokeSubjectReady() ? 1 : 0));
  log.Line("smoke_terrain_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokeTerrainReady() ? 1 : 0));
  log.Line("smoke_rendering_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokeRenderingReady() ? 1 : 0));
  log.Line("smoke_subject_capacity=" + std::to_string(
               RecoveredArenaSeance_SmokeSubjectCapacity()));
  log.Line("smoke_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SmokeSubjectFingerprint()));
  log.Line("smoke_visual_resources_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokeVisualResourcesReady() ? 1 : 0));
  log.Line("smoke_visual_resource_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SmokeVisualResourceFingerprint()));
  log.Line("explosion_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_ExplosionAttributesReady() ? 1 : 0));
  log.Line("explosion_subject_initialized=" +
           std::to_string(
               RecoveredGameServices_ExplosionSubjectReady() ? 1 : 0));
  log.Line("explosion_subject_capacity=" + std::to_string(
               RecoveredArenaSeance_ExplosionSubjectCapacity()));
  log.Line(
      "explosion_subject_mode=bounded-impact-radial-damage-impulse-light-sound-particles-smoke-piece-trace");
  log.Line("explosion_subject_impulse=" +
           std::to_string(
               RecoveredGameServices_ExplosionImpulseReady() ? 1 : 0));
  log.Line("explosion_subject_light=" +
           std::to_string(
               RecoveredGameServices_ExplosionLightReady() ? 1 : 0));
  log.Line("explosion_subject_light_lifecycle=useLight-brightness-frame-expiry");
  log.Line("explosion_subject_sound=" +
           std::to_string(
               RecoveredGameServices_ExplosionSoundReady() ? 1 : 0));
  log.Line("explosion_subject_sound_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ExplosionSoundReferenceFingerprint()));
  log.Line("explosion_subject_sound_probe_started=" + std::to_string(
               RecoveredArenaSeance_ExplosionSoundProbeStarted()));
  log.Line("explosion_subject_sound_probe_dependency_skips=" +
           std::to_string(
               RecoveredArenaSeance_ExplosionSoundProbeDependencySkips()));
  log.Line("explosion_subject_sound_probe_rollbacks=" + std::to_string(
               RecoveredArenaSeance_ExplosionSoundProbeRollbacks()));
  log.Line(
      "explosion_subject_sound_lifecycle=SET_WAV-MOVE_TO-START(1)-parent-rollback");
  log.Line("explosion_subject_sound_backend=device-free");
  log.Line("explosion_subject_particles=" +
           std::to_string(
               RecoveredGameServices_ExplosionParticlesReady() ? 1 : 0));
  log.Line("explosion_particle_visual_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleVisualFingerprint()));
  log.Line("explosion_particle_branch_capacity=" + std::to_string(
               ExplosionSubjectState_ParticleBranchCapacity()));
  log.Line("explosion_particle_probe_started_branches=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeStartedBranches()));
  log.Line("explosion_particle_probe_simple=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles()));
  log.Line("explosion_particle_probe_snake=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles()));
  log.Line("explosion_particle_probe_rays=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeRays()));
  log.Line("explosion_particle_probe_dependency_skips=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeDependencySkips()));
  log.Line("explosion_particle_probe_move_steps=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeMoveSteps()));
  log.Line("explosion_particle_probe_expired_parents=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeExpiredParents()));
  log.Line("explosion_particle_probe_rolled_back_branches=" + std::to_string(
               RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches()));
  log.Line("explosion_particle_raster=safe-clipped-software-particle");
  log.Line("explosion_smoke_sprites=" +
           std::to_string(
               RecoveredGameServices_ExplosionSmokeReady() ? 1 : 0));
  log.Line("explosion_smoke_visual_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ExplosionSmokeVisualFingerprint()));
  log.Line("explosion_smoke_probe_started_sprites=" + std::to_string(
               RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites()));
  log.Line("explosion_smoke_probe_dependency_skips=" + std::to_string(
               RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips()));
  log.Line("explosion_smoke_probe_move_steps=" + std::to_string(
               RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps()));
  log.Line("explosion_smoke_probe_expired_parents=" + std::to_string(
               RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents()));
  log.Line("explosion_smoke_probe_rolled_back_sprites=" + std::to_string(
               RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites()));
  log.Line("explosion_smoke_raster=resource-backed-alpha-sprite-256-atlas");
  log.Line("explosion_piece_initialized=" +
           std::to_string(
               RecoveredGameServices_ExplosionPieceReady() ? 1 : 0));
  log.Line("explosion_piece_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ExplosionPieceReferenceFingerprint()));
  log.Line("explosion_piece_probe_started_pieces=" + std::to_string(
               RecoveredArenaSeance_ExplosionPieceProbeStartedPieces()));
  log.Line("explosion_piece_probe_dependency_skips=" + std::to_string(
               RecoveredArenaSeance_ExplosionPieceProbeDependencySkips()));
  log.Line("explosion_piece_probe_move_steps=" + std::to_string(
               RecoveredArenaSeance_ExplosionPieceProbeMoveSteps()));
  log.Line("explosion_piece_probe_expired_parents=" + std::to_string(
               RecoveredArenaSeance_ExplosionPieceProbeExpiredParents()));
  log.Line("explosion_piece_probe_rolled_back_pieces=" + std::to_string(
               RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces()));
  log.Line("explosion_piece_models=resource-backed-ballistic-land-dynamic");
  log.Line("explosion_trace_initialized=" +
           std::to_string(
               RecoveredGameServices_ExplosionTraceReady() ? 1 : 0));
  log.Line("explosion_trace_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceReferenceFingerprint()));
  log.Line("explosion_trace_probe_started_pieces=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceProbeStartedPieces()));
  log.Line("explosion_trace_probe_quota_gate_skips=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceProbeQuotaGateSkips()));
  log.Line("explosion_trace_probe_puff_events=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceProbePuffEvents()));
  log.Line("explosion_trace_probe_smoke_children=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceProbeSmokeChildren()));
  log.Line("explosion_trace_probe_move_steps=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceProbeMoveSteps()));
  log.Line("explosion_trace_probe_expired_parents=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceProbeExpiredParents()));
  log.Line("explosion_trace_probe_rolled_back_pieces=" + std::to_string(
               RecoveredArenaSeance_ExplosionTraceProbeRolledBackPieces()));
  log.Line("explosion_trace=coalesced-NEWPUFF-common-Smoke-4-parent-quota");
  log.Line("explosion_active_world_initialized=" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldReady() ? 1 : 0));
  log.Line("explosion_active_world_probe=" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldCapturedOwners()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldCapturedBranches()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldSchedulerEvents()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldSoundChildren()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldRollbacks()) + "/" +
           std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldReconstructedIDs()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldStableRoundTrips()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldResumedMoves()));
  log.Line("explosion_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ExplosionActiveWorldFingerprint()));
  log.Line("bullet_trace=deferred-first-step-index-guard");
  log.Line("vehicle_vessel_mass=" + std::to_string(
               RecoveredGameServices_VehicleVesselMass()));
  log.Line("explosion_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ExplosionSubjectFingerprint()));
  log.Line("explosion_probe_invalid_starts=" + std::to_string(
               RecoveredArenaSeance_ExplosionProbeInvalidStarts()));
  log.Line("explosion_probe_allocation_rollbacks=" + std::to_string(
               RecoveredArenaSeance_ExplosionProbeAllocationRollbacks()));
  log.Line("explosion_probe_queued_commands=" + std::to_string(
               RecoveredArenaSeance_ExplosionProbeQueuedCommands()));
  log.Line("explosion_probe_queue_rollbacks=" + std::to_string(
               RecoveredArenaSeance_ExplosionProbeQueueRollbacks()));
  log.Line("explosion_probe_executed_commands=" + std::to_string(
               RecoveredArenaSeance_ExplosionProbeExecutedCommands()));
  log.Line("explosion_probe_damage_applications=" + std::to_string(
               RecoveredArenaSeance_ExplosionProbeDamageApplications()));
  log.Line("vehicle_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_VehicleAttributesReady() ? 1 : 0));
  log.Line("vehicle_attribute_count=" +
           std::to_string(RecoveredArenaSeance_VehicleAttributeCount()));
  log.Line("vehicle_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_VehicleAttributeCapacity()));
  log.Line("vehicle_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_VehicleAttributeFingerprint()));
  log.Line("vehicle_references_resolved=" +
           std::to_string(
               RecoveredGameServices_VehicleReferencesReady() ? 1 : 0));
  log.Line("vehicle_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_VehicleReferenceFingerprint()));
  log.Line("vehicle_movement_initialized=" +
           std::to_string(
               RecoveredGameServices_VehicleMovementReady() ? 1 : 0));
  log.Line("vehicle_runtime_fingerprint=" + std::to_string(
               RecoveredGameServices_VehicleRuntimeFingerprint()));
  log.Line("vehicle_vessel_kind=" + std::to_string(
               RecoveredGameServices_VehicleVesselKind()));
  log.Line("vehicle_probe_invalid_activations=" + std::to_string(
               RecoveredGameServices_VehicleProbeInvalidActivations()));
  log.Line("vehicle_probe_activations=" + std::to_string(
               RecoveredGameServices_VehicleProbeActivations()));
  log.Line("vehicle_probe_stationary_steps=" + std::to_string(
               RecoveredGameServices_VehicleProbeStationarySteps()));
  log.Line("vehicle_probe_throttle_events=" + std::to_string(
               RecoveredGameServices_VehicleProbeThrottleEvents()));
  log.Line("vehicle_probe_movement_steps=" + std::to_string(
               RecoveredGameServices_VehicleProbeMovementSteps()));
  log.Line("vehicle_probe_turn_events=" + std::to_string(
               RecoveredGameServices_VehicleProbeTurnEvents()));
  log.Line("vehicle_probe_camera_transitions=" + std::to_string(
               RecoveredGameServices_VehicleProbeCameraTransitions()));
  log.Line("vehicle_probe_rollbacks=" + std::to_string(
               RecoveredGameServices_VehicleProbeRollbacks()));
  log.Line("vehicle_probe_horizontal_distance=" + std::to_string(
               RecoveredGameServices_VehicleProbeHorizontalDistance()));
  log.Line("taxi_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_TaxiAttributesReady() ? 1 : 0));
  log.Line("taxi_attribute_count=" +
           std::to_string(RecoveredArenaSeance_TaxiAttributeCount()));
  log.Line("taxi_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_TaxiAttributeCapacity()));
  log.Line("taxi_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_TaxiAttributeFingerprint()));
  log.Line("taxi_references_resolved=" +
           std::to_string(
               RecoveredGameServices_TaxiReferencesReady() ? 1 : 0));
  log.Line("taxi_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_TaxiReferenceFingerprint()));
  log.Line("taxi_subject_initialized=" +
           std::to_string(RecoveredGameServices_TaxiSubjectReady() ? 1 : 0));
  log.Line("taxi_subject_capacity=" +
           std::to_string(RecoveredArenaSeance_TaxiSubjectCapacity()));
  log.Line("taxi_subject_count=" +
           std::to_string(RecoveredArenaSeance_TaxiSubjectCount()));
  log.Line("taxi_subject_sound_count=" +
           std::to_string(RecoveredArenaSeance_TaxiSubjectSoundCount()));
  log.Line("taxi_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_TaxiSubjectFingerprint()));
  log.Line("taxi_probe_invalid_starts=" + std::to_string(
               RecoveredArenaSeance_TaxiProbeInvalidStarts()));
  log.Line("taxi_probe_valid_starts=" + std::to_string(
               RecoveredArenaSeance_TaxiProbeValidStarts()));
  log.Line("taxi_probe_render_ready=" + std::to_string(
               RecoveredArenaSeance_TaxiProbeRenderReady()));
  log.Line("taxi_probe_sound_ready=" + std::to_string(
               RecoveredArenaSeance_TaxiProbeSoundReady()));
  log.Line("taxi_probe_rollbacks=" +
           std::to_string(RecoveredArenaSeance_TaxiProbeRollbacks()));
  log.Line("taxi_event_set_to_position=5018");
  log.Line("taxi_vehicle_transition_initialized=" + std::to_string(
               RecoveredGameServices_TaxiVehicleTransitionReady() ? 1 : 0));
  log.Line("taxi_vehicle_probe_available_taxis=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbeAvailableTaxis()));
  log.Line("taxi_vehicle_probe_invalid_targets=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbeInvalidTargets()));
  log.Line("taxi_vehicle_probe_transitions=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbeTransitions()));
  log.Line("taxi_vehicle_probe_attribute_transfers=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbeAttributeTransfers()));
  log.Line("taxi_vehicle_probe_pose_transfers=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbePoseTransfers()));
  log.Line("taxi_vehicle_probe_payload_transfers=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbePayloadTransfers()));
  log.Line("taxi_vehicle_probe_removed_taxis=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbeRemovedTaxis()));
  log.Line("taxi_vehicle_probe_rollbacks=" + std::to_string(
               RecoveredGameServices_TaxiVehicleProbeRollbacks()));
  SRecoveredTaxiVehicleHandoffTelemetry taxiHandoff = {};
  const bool taxiHandoffInspected =
      RecoveredGameServices_TaxiVehicleHandoffTelemetry(&taxiHandoff);
  log.Line("taxi_vehicle_handoff_observable=" +
           std::to_string(taxiHandoffInspected ? 1 : 0));
  log.Line("taxi_vehicle_nearest_distance=" +
           std::to_string(taxiHandoff.nearestTaxiDistance));
  log.Line("taxi_vehicle_activation_distance=" +
           std::to_string(taxiHandoff.activationDistance));
  log.Line("taxi_vehicle_available=" +
           std::to_string(taxiHandoff.availableTaxis));
  log.Line("taxi_vehicle_nearby=" +
           std::to_string(taxiHandoff.nearbyTaxis));
  log.Line("taxi_vehicle_handoff_attempts=" +
           std::to_string(taxiHandoff.attempts));
  log.Line("taxi_vehicle_handoff_pending=" +
           std::to_string(taxiHandoff.pendingTransitions));
  log.Line("taxi_vehicle_handoff_successes=" +
           std::to_string(taxiHandoff.successfulTransitions));
  log.Line("taxi_vehicle_handoff_no_targets=" +
           std::to_string(taxiHandoff.noTargetAttempts));
  log.Line("taxi_vehicle_handoff_removed_taxis=" +
           std::to_string(taxiHandoff.removedTaxis));
  log.Line("taxi_vehicle_panel_ready=" +
           std::to_string(taxiHandoff.panelReady));
  log.Line("taxi_vehicle_panel_open=" +
           std::to_string(taxiHandoff.panelOpen));
  log.Line("taxi_vehicle_panel_open_transitions=" +
           std::to_string(taxiHandoff.panelOpenTransitions));
  log.Line("taxi_vehicle_panel_draws=" +
           std::to_string(taxiHandoff.panelDraws));
  log.Line("taxi_vehicle_hardware_subscription_preserved=" +
           std::to_string(taxiHandoff.hardwareSubscriptionPreserved));
  log.Line("taxi_vehicle_post_transition_frames=" +
           std::to_string(taxiHandoff.postTransitionFrames));
  log.Line("taxi_vehicle_post_transition_distance=" +
           std::to_string(taxiHandoff.postTransitionDistance));
  SRecoveredVehicleEmbodimentTelemetry embodiment = {};
  const bool embodimentInspected =
      RecoveredGameServices_VehicleEmbodimentTelemetry(&embodiment);
  log.Line("vehicle_embodiment_observable=" +
           std::to_string(embodimentInspected ? 1 : 0));
  log.Line("vehicle_exit_attempts=" +
           std::to_string(embodiment.exitAttempts));
  log.Line("vehicle_safe_exit_completions=" +
           std::to_string(embodiment.safeExitCompletions));
  log.Line("vehicle_unsafe_exit_completions=" +
           std::to_string(embodiment.unsafeExitCompletions));
  log.Line("vehicle_dropped_taxis=" +
           std::to_string(embodiment.droppedTaxis));
  log.Line("vehicle_dropped_orphans=" +
           std::to_string(embodiment.droppedOrphans));
  log.Line("vehicle_reentry_attempts=" +
           std::to_string(embodiment.reentryAttempts));
  log.Line("vehicle_reentry_completions=" +
           std::to_string(embodiment.reentryCompletions));
  log.Line("vehicle_panel_close_transitions=" +
           std::to_string(embodiment.panelCloseTransitions));
  log.Line("vehicle_panel_reopen_transitions=" +
           std::to_string(embodiment.panelReopenTransitions));
  log.Line("orphan_move_events=" +
           std::to_string(embodiment.orphanMoveEvents));
  log.Line("orphan_impacts=" +
           std::to_string(embodiment.orphanImpacts));
  log.Line("orphan_explosions=" +
           std::to_string(embodiment.orphanExplosions));
  log.Line("orphan_smoke_starts=" +
           std::to_string(embodiment.orphanSmokeStarts));
  log.Line("orphan_render_frames=" +
           std::to_string(embodiment.orphanRenderFrames));
  log.Line("orphan_live_objects=" +
           std::to_string(embodiment.liveOrphans));
  log.Line("vehicle_exit_pending=" +
           std::to_string(embodiment.exitPending));
  log.Line("vehicle_embodiment_hardware_subscription_preserved=" +
           std::to_string(embodiment.hardwareSubscriptionPreserved));
  SRecoveredVehiclePrimaryFireTelemetry primaryFire = {};
  const bool primaryFireInspected =
      RecoveredGameServices_VehiclePrimaryFireTelemetry(&primaryFire);
  log.Line("vehicle_primary_fire_observable=" +
           std::to_string(primaryFireInspected ? 1 : 0));
  log.Line("vehicle_primary_fire_trigger_presses=" +
           std::to_string(primaryFire.triggerPresses));
  log.Line("vehicle_primary_fire_accepted_shots=" +
           std::to_string(primaryFire.acceptedShots));
  log.Line("vehicle_primary_fire_rolled_back_shots=" +
           std::to_string(primaryFire.rolledBackShots));
  log.Line("vehicle_primary_fire_move_events=" +
           std::to_string(primaryFire.moveEvents));
  log.Line("vehicle_primary_fire_collision_checks=" +
           std::to_string(primaryFire.collisionChecks));
  log.Line("vehicle_primary_fire_scene_impacts=" +
           std::to_string(primaryFire.sceneImpacts));
  log.Line("vehicle_primary_fire_dynamic_impacts=" +
           std::to_string(primaryFire.dynamicImpacts));
  log.Line("vehicle_primary_fire_waterline_splashes=" +
           std::to_string(primaryFire.waterlineSplashes));
  log.Line("vehicle_primary_fire_effect_children=" +
           std::to_string(primaryFire.impactEffectChildren));
  log.Line("vehicle_primary_fire_ground_removals=" +
           std::to_string(primaryFire.groundRemovals));
  log.Line("vehicle_primary_fire_barrel_smokes=" +
           std::to_string(primaryFire.barrelSmokeStarts));
  log.Line("vehicle_primary_fire_live_bullets=" +
           std::to_string(primaryFire.liveBullets));
  log.Line("vehicle_primary_fire_peak_bullets=" +
           std::to_string(primaryFire.tablePeakLiveBullets));
  log.Line("vehicle_primary_fire_max_explosions=" +
           std::to_string(primaryFire.maximumExplosionSubjects));
  log.Line("vehicle_primary_fire_max_particles=" +
           std::to_string(primaryFire.maximumParticleBranches));
  log.Line("vehicle_primary_fire_max_smokes=" +
           std::to_string(primaryFire.maximumSmokeSubjects));
  log.Line("vehicle_primary_fire_max_sparks=" +
           std::to_string(primaryFire.maximumSparkSubjects));
  log.Line("vehicle_primary_fire_max_sounds=" +
           std::to_string(primaryFire.maximumSoundObjects));
  log.Line("vehicle_primary_fire_rendered_frames=" +
           std::to_string(primaryFire.renderedFramesAfterShot));
  log.Line("vehicle_primary_fire_effect_render_frames=" +
           std::to_string(primaryFire.effectRenderFrames));
  log.Line("vehicle_primary_fire_hardware_subscription_preserved=" +
           std::to_string(primaryFire.hardwareSubscriptionPreserved));
  log.Line("bullet_attributes_initialized=" + std::to_string(
               RecoveredArenaSeance_BulletAttributesReady() ? 1 : 0));
  log.Line("bullet_attribute_count=" +
           std::to_string(RecoveredArenaSeance_BulletAttributeCount()));
  log.Line("bullet_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_BulletAttributeCapacity()));
  log.Line("bullet_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_BulletAttributeFingerprint()));
  log.Line("bullet_references_resolved=" + std::to_string(
               RecoveredArenaSeance_BulletReferencesReady() ? 1 : 0));
  log.Line("bullet_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_BulletReferenceFingerprint()));
  log.Line("bullet_subject_registration_initialized=" + std::to_string(
               RecoveredArenaSeance_BulletSubjectRegistrationReady() ? 1
                                                                      : 0));
  log.Line("bullet_subject_initialized=" + std::to_string(
               RecoveredArenaSeance_BulletSubjectReady() ? 1 : 0));
  log.Line("bullet_subject_capacity=" +
           std::to_string(RecoveredArenaSeance_BulletSubjectCapacity()));
  log.Line(
      "bullet_subject_mode=ballistic-collision-impact-ground-waterline");
  log.Line("bullet_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_BulletSubjectFingerprint()));
  log.Line("bullet_subject_probe_move_count=" + std::to_string(
               RecoveredArenaSeance_BulletSubjectProbeMoveCount()));
  log.Line("bullet_active_world_initialized=" + std::to_string(
               RecoveredArenaSeance_BulletActiveWorldReady() ? 1 : 0));
  log.Line("bullet_active_world_probe=" + std::to_string(
               RecoveredArenaSeance_BulletActiveWorldCapturedOwners()) + "/" +
           std::to_string(
               RecoveredArenaSeance_BulletActiveWorldSchedulerEvents()) + "/" +
           std::to_string(
               RecoveredArenaSeance_BulletActiveWorldRollbacks()) + "/" +
           std::to_string(
               RecoveredArenaSeance_BulletActiveWorldReconstructedIDs()) + "/" +
           std::to_string(
               RecoveredArenaSeance_BulletActiveWorldStableRoundTrips()) + "/" +
           std::to_string(
               RecoveredArenaSeance_BulletActiveWorldResumedMoves()));
  log.Line("bullet_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_BulletActiveWorldFingerprint()));
  log.Line("bullet_collision_scheduled_checks=" + std::to_string(
               RecoveredArenaSeance_BulletCollisionScheduledChecks()));
  log.Line("bullet_collision_executed_checks=" + std::to_string(
               RecoveredArenaSeance_BulletCollisionExecutedChecks()));
  log.Line("bullet_collision_sphere_cases=" + std::to_string(
               RecoveredArenaSeance_BulletCollisionSphereCases()));
  log.Line("bullet_collision_earliest_hit_cases=" + std::to_string(
               RecoveredArenaSeance_BulletCollisionEarliestHitCases()));
  log.Line("bullet_collision_waterline_cases=" + std::to_string(
               RecoveredArenaSeance_BulletCollisionWaterlineCases()));
  log.Line("bullet_collision_scene_queries=" + std::to_string(
               RecoveredArenaSeance_BulletCollisionSceneQueries()));
  log.Line("bullet_impact_effects_initialized=" + std::to_string(
               RecoveredArenaSeance_BulletImpactEffectsReady() ? 1 : 0));
  log.Line("bullet_effect_queued_batches=" + std::to_string(
               RecoveredArenaSeance_BulletEffectQueuedBatches()));
  log.Line("bullet_effect_queued_children=" + std::to_string(
               RecoveredArenaSeance_BulletEffectQueuedChildren()));
  log.Line("bullet_effect_splash_first_cases=" + std::to_string(
               RecoveredArenaSeance_BulletEffectSplashFirstCases()));
  log.Line("bullet_effect_rolled_back_children=" + std::to_string(
               RecoveredArenaSeance_BulletEffectRolledBackChildren()));
  log.Line("farter_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_FarterAttributesReady() ? 1 : 0));
  log.Line("farter_attribute_count=" +
           std::to_string(RecoveredArenaSeance_FarterAttributeCount()));
  log.Line("farter_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_FarterAttributeCapacity()));
  log.Line("farter_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_FarterAttributeFingerprint()));
  log.Line("farter_references_resolved=" +
           std::to_string(
               RecoveredArenaSeance_FarterReferencesReady() ? 1 : 0));
  log.Line("farter_runtime_ready=" +
           std::to_string(
               RecoveredArenaSeance_FarterRuntimeReady() ? 1 : 0));
  log.Line("farter_subject_initialized=" +
           std::to_string(
               RecoveredGameServices_FarterSubjectReady() ? 1 : 0));
  log.Line("farter_subject_capacity=" + std::to_string(
               RecoveredArenaSeance_FarterSubjectCapacity()));
  log.Line("farter_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_FarterSubjectFingerprint()));
  log.Line("farter_script_objects=" + std::to_string(
               RecoveredArenaSeance_FarterScriptObjectCount()));
  log.Line("farter_live_objects=" + std::to_string(
               RecoveredArenaSeance_FarterLiveObjectCount()));
  log.Line("farter_sound_objects=" + std::to_string(
               RecoveredArenaSeance_FarterSoundObjectCount()));
  log.Line("sound_distance_initialized=" + std::to_string(
               RecoveredArenaSeance_SoundDistanceReady() ? 1 : 0));
  log.Line("sound_distance_max=" + std::to_string(
               RecoveredArenaSeance_SoundDistance()));
  log.Line("sound_distance_squared=" + std::to_string(
               RecoveredArenaSeance_SoundDistanceSquared()));
  log.Line("farter_near_frame_audible=" + std::to_string(
               RecoveredArenaSeance_FarterNearFrameAudibleCount()));
  log.Line("farter_far_frame_audible=" + std::to_string(
               RecoveredArenaSeance_FarterFarFrameAudibleCount()));
  log.Line("farter_audible_frame_transition=" + std::to_string(
               RecoveredArenaSeance_FarterAudibleFrameTransition() ? 1 : 0));
  log.Line("farter_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_FarterReferenceFingerprint()));
  log.Line("lamp_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_LampAttributesReady() ? 1 : 0));
  log.Line("lamp_attribute_count=" +
           std::to_string(RecoveredArenaSeance_LampAttributeCount()));
  log.Line("lamp_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_LampAttributeCapacity()));
  log.Line("lamp_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_LampAttributeFingerprint()));
  log.Line("corpse_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_CorpseAttributesReady() ? 1 : 0));
  log.Line("corpse_attribute_count=" +
           std::to_string(RecoveredArenaSeance_CorpseAttributeCount()));
  log.Line("corpse_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_CorpseAttributeCapacity()));
  log.Line("corpse_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_CorpseAttributeFingerprint()));
  log.Line("corpse_references_resolved=" +
           std::to_string(
               RecoveredArenaSeance_CorpseReferencesReady() ? 1 : 0));
  log.Line("corpse_runtime_ready=" +
           std::to_string(
               RecoveredArenaSeance_CorpseRuntimeReady() ? 1 : 0));
  log.Line("corpse_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_CorpseReferenceFingerprint()));
  log.Line("corpse_subject_initialized=" +
           std::to_string(
               RecoveredGameServices_CorpseSubjectReady() ? 1 : 0));
  log.Line("corpse_subject_capacity=" + std::to_string(
               RecoveredArenaSeance_CorpseSubjectCapacity()));
  log.Line("corpse_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_CorpseSubjectFingerprint()));
  log.Line("smoker_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokerAttributesReady() ? 1 : 0));
  log.Line("smoker_attribute_count=" +
           std::to_string(RecoveredArenaSeance_SmokerAttributeCount()));
  log.Line("smoker_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_SmokerAttributeCapacity()));
  log.Line("smoker_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SmokerAttributeFingerprint()));
  log.Line("smoker_references_resolved=" +
           std::to_string(
               RecoveredGameServices_SmokerReferencesReady() ? 1 : 0));
  log.Line("smoker_runtime_ready=" +
           std::to_string(
               RecoveredGameServices_SmokerRuntimeReady() ? 1 : 0));
  log.Line("smoker_emission_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokerEmissionReady() ? 1 : 0));
  log.Line("smoker_light_corona_initialized=" +
           std::to_string(
               RecoveredGameServices_SmokerLightCoronaReady() ? 1 : 0));
  log.Line("smoker_reference_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SmokerReferenceFingerprint()));
  log.Line("dyn_smoker_initialized=" +
           std::to_string(
               RecoveredGameServices_DynSmokerReady() ? 1 : 0));
  log.Line("dyn_smoker_capacity=" +
           std::to_string(RecoveredArenaSeance_DynSmokerCapacity()));
  log.Line("dyn_smoker_fingerprint=" + std::to_string(
               RecoveredArenaSeance_DynSmokerFingerprint()));
  log.Line("wav_metadata_initialized=" +
           std::to_string(
               RecoveredGameServices_WavMetadataReady() ? 1 : 0));
  log.Line("wav_metadata_count=" +
           std::to_string(RecoveredArenaSeance_WavMetadataCount()));
  log.Line("wav_metadata_capacity=" +
           std::to_string(RecoveredArenaSeance_WavMetadataCapacity()));
  log.Line("wav_catalog_fingerprint=" + std::to_string(
               RecoveredArenaSeance_WavCatalogFingerprint()));
  log.Line("wav_resource_fingerprint=" + std::to_string(
               RecoveredArenaSeance_WavResourceFingerprint()));
  log.Line("sound_object_initialized=" +
           std::to_string(
               RecoveredGameServices_SoundObjectReady() ? 1 : 0));
  log.Line("sound_object_capacity=" + std::to_string(
               RecoveredArenaSeance_SoundObjectCapacity()));
  log.Line("sound_object_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SoundObjectFingerprint()));
  log.Line("audio_backend=device-free-command-state");
  log.Line("skin_resources_initialized=" +
           std::to_string(
               RecoveredGameServices_SkinResourcesReady() ? 1 : 0));
  log.Line("skin_resource_models=" +
           std::to_string(RecoveredArenaSeance_SkinModelCount()));
  log.Line("skin_resource_sprites=" +
           std::to_string(RecoveredArenaSeance_SkinSpriteCount()));
  log.Line("skin_catalog_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SkinCatalogFingerprint()));
  log.Line("skin_resource_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SkinResourceFingerprint()));
  log.Line("spark_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_SparkAttributesReady() ? 1 : 0));
  log.Line("spark_subject_initialized=" +
           std::to_string(
               RecoveredGameServices_SparkSubjectReady() ? 1 : 0));
  log.Line("spark_subject_capacity=" +
           std::to_string(RecoveredArenaSeance_SparkSubjectCapacity()));
  log.Line("spark_rendering_initialized=" +
           std::to_string(
               RecoveredGameServices_SparkRenderingReady() ? 1 : 0));
  log.Line("spark_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SparkSubjectFingerprint()));
  log.Line("spark_visual_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SparkVisualResourceFingerprint()));
  log.Line("spark_lifecycle_probe=" +
           std::to_string(RecoveredArenaSeance_SparkProbeInvalidStarts()) +
           "/" +
           std::to_string(RecoveredArenaSeance_SparkProbeQueuedCreates()) +
           "/" +
           std::to_string(RecoveredArenaSeance_SparkProbeQueueRollbacks()) +
           "/" +
           std::to_string(
               RecoveredArenaSeance_SparkProbePhaseTransitions()) +
           "/" +
            std::to_string(RecoveredArenaSeance_SparkProbeExpirations()));
  log.Line("spark_active_world_initialized=" + std::to_string(
               RecoveredArenaSeance_SparkActiveWorldReady() ? 1 : 0));
  log.Line("spark_active_world_probe=" + std::to_string(
               RecoveredArenaSeance_SparkActiveWorldCapturedOwners()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SparkActiveWorldSchedulerEvents()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SparkActiveWorldRollbacks()) + "/" +
           std::to_string(
               RecoveredArenaSeance_SparkActiveWorldReconstructedIDs()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SparkActiveWorldStableRoundTrips()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SparkActiveWorldResumedPhases()));
  log.Line("spark_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SparkActiveWorldFingerprint()));
  log.Line("smoke_active_world_initialized=" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldReady() ? 1 : 0));
  log.Line("smoke_active_world_probe=" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldCapturedOwners()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldCapturedBlobs()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldSchedulerEvents()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldRollbacks()) + "/" +
           std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldReconstructedIDs()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldStableRoundTrips()) +
           "/" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldResumedMoves()));
  log.Line("smoke_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SmokeActiveWorldFingerprint()));
  log.Line("corpse_active_world_initialized=" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldReady() ? 1 : 0));
  log.Line("corpse_active_world_probe=" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldCapturedOwners()) +
           "/" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldOwnedSmokers()) +
           "/" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldSchedulerEvents()) +
           "/" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldRollbacks()) + "/" +
           std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldReconstructedObjects()) +
           "/" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldStableRoundTrips()) +
           "/" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldResumedEmissions()) +
           "/" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldResumedDeaths()));
  log.Line("corpse_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_CorpseActiveWorldFingerprint()));
  log.Line("people_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_PeopleAttributesReady() ? 1 : 0));
  log.Line("people_attribute_count=" +
           std::to_string(RecoveredArenaSeance_PeopleAttributeCount()));
  log.Line("people_attribute_capacity=" +
           std::to_string(RecoveredArenaSeance_PeopleAttributeCapacity()));
  log.Line("people_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_PeopleAttributeFingerprint()));
  log.Line("people_references_resolved=" +
           std::to_string(
               RecoveredGameServices_PeopleReferencesReady() ? 1 : 0));
  log.Line("people_subject_initialized=" +
           std::to_string(
               RecoveredGameServices_PeopleSubjectReady() ? 1 : 0));
  log.Line("people_subject_count=" +
           std::to_string(RecoveredArenaSeance_PeopleSubjectCount()));
  log.Line("people_subject_sound_count=" +
           std::to_string(
               RecoveredArenaSeance_PeopleSubjectSoundCount()));
  log.Line("people_subject_capacity=" +
           std::to_string(RecoveredArenaSeance_PeopleSubjectCapacity()));
  log.Line("people_subject_fingerprint=" + std::to_string(
               RecoveredArenaSeance_PeopleSubjectFingerprint()));
  log.Line("people_lifecycle_probe=" +
           std::to_string(RecoveredArenaSeance_PeopleProbeScheduledMoves()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleProbeBulletDamageApplications()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleProbeDeathTransitions()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips()) +
           "/" +
           std::to_string(RecoveredArenaSeance_PeopleProbeRollbacks()));
  log.Line("people_active_world_probe=" +
           std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldRollbacks()));
  log.Line("people_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldFingerprint()));
  log.Line("tank_cannon_attributes_initialized=" +
           std::to_string(
               RecoveredGameServices_TankCannonAttributesReady() ? 1 : 0));
  log.Line("tank_references_resolved=" +
           std::to_string(
               RecoveredGameServices_TankReferencesReady() ? 1 : 0));
  log.Line("tank_cannon_subject_tables_initialized=" +
           std::to_string(
               RecoveredGameServices_TankCannonSubjectTablesReady() ? 1 : 0));
  log.Line("tank_attribute_roster=" +
           std::to_string(RecoveredArenaSeance_TankAttributeCount()) + "/" +
           std::to_string(RecoveredArenaSeance_TankAttributeCapacity()));
  log.Line("cannon_attribute_roster=" +
           std::to_string(RecoveredArenaSeance_CannonAttributeCount()) +
           "/" +
           std::to_string(RecoveredArenaSeance_CannonAttributeCapacity()));
  log.Line("tank_subject_roster=" +
           std::to_string(RecoveredArenaSeance_TankSubjectCount()) + "/" +
           std::to_string(RecoveredArenaSeance_TankSubjectCapacity()));
  log.Line("cannon_subject_roster=" +
           std::to_string(RecoveredArenaSeance_CannonSubjectCount()) + "/" +
           std::to_string(RecoveredArenaSeance_CannonSubjectCapacity()));
  log.Line("tank_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_TankAttributeFingerprint()));
  log.Line("cannon_attribute_fingerprint=" + std::to_string(
               RecoveredArenaSeance_CannonAttributeFingerprint()));
  log.Line("tank_lifecycle_probe=" +
           std::to_string(RecoveredArenaSeance_TankProbeAvailable()) + "/" +
           std::to_string(RecoveredArenaSeance_TankProbeValidStarts()) +
           "/" +
           std::to_string(RecoveredArenaSeance_TankProbeDynamicReady()) +
           "/" +
           std::to_string(RecoveredArenaSeance_TankProbeRenderReady()) +
           "/" +
           std::to_string(RecoveredArenaSeance_TankProbeCannonReady()) +
           "/" +
           std::to_string(RecoveredArenaSeance_TankProbeScheduledMoves()) +
           "/" + std::to_string(
               RecoveredArenaSeance_TankProbeBulletDamageApplications()) +
           "/" + std::to_string(
               RecoveredArenaSeance_TankProbeDeathTransitions()) +
           "/" + std::to_string(
               RecoveredArenaSeance_TankProbeDeathEffects()) +
           "/" + std::to_string(
               RecoveredArenaSeance_TankProbeSaveStateRoundTrips()) +
           "/" +
           std::to_string(RecoveredArenaSeance_TankProbeRollbacks()));
  log.Line("commander_roster=" +
           std::to_string(RecoveredArenaSeance_CommanderCount()) + "/" +
           std::to_string(RecoveredArenaSeance_CommanderCapacity()));
  log.Line("commander_hostile_links=" +
           std::to_string(RecoveredArenaSeance_CommanderHostileLinks()));
  log.Line("commander_fingerprint=" + std::to_string(
               RecoveredArenaSeance_CommanderFingerprint()));
  log.Line("tank_group_subject_capacity=" +
           std::to_string(
               RecoveredArenaSeance_TankGroupSubjectCapacity()));
  log.Line("mission_tank_lifecycle_probe=" +
           std::to_string(RecoveredArenaSeance_MissionTankAvailable()) +
           "/" +
           std::to_string(RecoveredArenaSeance_MissionTankSpawns()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionTankMembershipLinks()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionTankFindEnemyCycles()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionTankMovingCycles()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionTankStableRoundTrips()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionTankReconstructedIDs()) + "/" +
           std::to_string(RecoveredArenaSeance_MissionTankRollbacks()));
  log.Line("mission_tank_fingerprint=" + std::to_string(
               RecoveredArenaSeance_MissionTankFingerprint()));
  log.Line("active_world_persistence_initialized=" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldPersistenceReady() ? 1 : 0));
  log.Line("active_world_format_version=" + std::to_string(
               RecoveredArenaSeance_ActiveWorldFormatVersion()));
  log.Line("active_world_owner_event_sections=" +
           std::to_string(RecoveredArenaSeance_ActiveWorldSections()) + "/" +
           std::to_string(RecoveredArenaSeance_ActiveWorldEvents()));
  log.Line("active_world_restore_phases=" +
           std::to_string(RecoveredArenaSeance_ActiveWorldOwnerPhases()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ActiveWorldReferencePhases()) +
           "/" +
           std::to_string(RecoveredArenaSeance_ActiveWorldEventPhases()));
  log.Line("active_world_integrity_probe=" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldCorruptionRejects()) +
           "/" +
           std::to_string(RecoveredArenaSeance_ActiveWorldRollbacks()));
  log.Line("active_world_created_owners=" + std::to_string(
               RecoveredArenaSeance_ActiveWorldCreatedOwners()));
  log.Line("active_world_container_bytes=" + std::to_string(
               RecoveredArenaSeance_ActiveWorldContainerBytes()));
  log.Line("active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_ActiveWorldFingerprint()));
  log.Line("vehicle_active_world_probe=" +
           std::to_string(
               RecoveredArenaSeance_VehicleActiveWorldReconstructedIDs()) +
           "/" + std::to_string(
               RecoveredArenaSeance_VehicleActiveWorldRollbacks()));
  log.Line("vehicle_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_VehicleActiveWorldFingerprint()));
  log.Line("bullet_ground_spark_initialized=" +
           std::to_string(
               RecoveredGameServices_BulletGroundSparkReady() ? 1 : 0));
  log.Line("bullet_ground_spark_probe=" +
           std::to_string(RecoveredArenaSeance_BulletGroundSparkQueued()) +
           "/" + std::to_string(
               RecoveredArenaSeance_BulletGroundSparkRolledBack()));
  log.Line("bullet_barrel_smoke_initialized=" +
           std::to_string(
               RecoveredGameServices_BulletBarrelSmokeReady() ? 1 : 0));
  log.Line("bullet_barrel_smoke_probe=" + std::to_string(
               RecoveredArenaSeance_BulletBarrelSmokeThresholdStarts()) +
           "/" + std::to_string(
               RecoveredArenaSeance_BulletBarrelSmokeFrameGateSkips()) +
           "/" + std::to_string(
               RecoveredArenaSeance_BulletBarrelSmokeAttributeGateSkips()) +
           "/" + std::to_string(
               RecoveredArenaSeance_BulletBarrelSmokeRollbacks()));
  log.Line("bullet_barrel_smoke_frame_gate=frameSec<=0.09");
  log.Line("route_table_initialized=" +
           std::to_string(RecoveredGameServices_RouteReady() ? 1 : 0));
  log.Line("vehicle_default_initialized=" +
           std::to_string(RecoveredGameServices_VehicleReady() ? 1 : 0));
  log.Line("arena_seance_issues=" +
           std::to_string(RecoveredArenaSeance_Issues()));
  log.Line("arena_seance_extended_issues=" +
           std::to_string(RecoveredArenaSeance_ExtendedIssues()));
  if (RecoveredArenaSeance_LastError()[0] != 0) {
    log.Line(std::string("arena_seance_error=") +
             RecoveredArenaSeance_LastError());
  }
  ZAV_BeginLoop();
  log.Line("loop_initialized=" +
           std::to_string(RecoveredGameServices_LoopReady() ? 1 : 0));
  bool loopFailed = !RecoveredGameServices_IsReady();
  if (!loopFailed && options.runtimeSmoke) {
    loopFailed = !RecoveredGameServices_RunFrame() ||
                 !RecoveredGameServices_RunFrame();
  }
  while (!loopFailed && !options.runtimeSmoke &&
         !RecoveredGameServices_QuitRequested()) {
    if (!RecoveredGameServices_RunFrame()) {
      loopFailed = !RecoveredGameServices_QuitRequested();
      break;
    }
    Sleep(1);
  }
  if (loopFailed) {
    log.Line("game_services_issues=" +
             std::to_string(RecoveredGameServices_Issues()));
    log.Line("marker=loop-not-ready");
    ZAV_DeInitLevel();
    ZAV_Deinit();
    ShowMessage(options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW runtime error",
                L"The recovered services could not complete the software "
                L"loop.\n\nDiagnostic log:\n" + log.path());
    return kRuntimeNotReady;
  }

  log.Line("recovered_runtime=connected");
  log.Line("retail_script_manifest_ready=1");
  log.Line("retail_script_manifest_includes=" +
           std::to_string(scriptManifest->includeDirectives));
  log.Line("retail_script_manifest_files=" +
           std::to_string(scriptManifest->fileVisits));
  log.Line("retail_script_manifest_unique_files=" +
           std::to_string(scriptManifest->uniqueFiles));
  log.Line("retail_script_manifest_root_files=" +
           std::to_string(scriptManifest->rootFiles));
  log.Line("retail_script_manifest_level_files=" +
           std::to_string(scriptManifest->levelFiles));
  log.Line("retail_script_manifest_bytes=" +
           std::to_string(scriptManifest->totalBytes));
  log.Line("retail_script_manifest_fingerprint=" +
           std::to_string(scriptManifest->contentFingerprint));
  log.Line("game_entry_missing_hooks=" +
           std::to_string(GameEntry_RuntimeMissingHooks()));
  log.Line("scene_bases=" + std::to_string(summary->bases));
  log.Line("scene_named_declarations=" +
           std::to_string(summary->namedDeclarations));
  log.Line("scene_resolved_references=" +
           std::to_string(summary->resolvedReferences));
  log.Line("scene_land_pieces=" + std::to_string(summary->landPieces));
  log.Line("scene_terrain_ready=" +
           std::to_string(summary->terrainHeightMapReady));
  log.Line("scene_bush_ready=" +
           std::to_string(summary->bushRendererReady));
  log.Line("runtime_mode=" +
           std::string(options.runtimeSmoke ? "bounded-smoke"
                                            : "interactive-vehicle"));
  log.Line("input_mode=legacy-hardware-keyboard");
  log.Line("camera_mode=Vehicle.Default");
  log.Line("vehicle_control_owner=RecoveredVehicleControl-exclusive");
  log.Line("vehicle_runtime=retail-spawn-live-BeginPreStep-UpdatePos-camera");
  log.Line("vehicle_control_ready=" + std::to_string(
               RecoveredGameServices_VehicleControlReady() ? 1 : 0));
  log.Line("vehicle_fallback_active=" + std::to_string(
               RecoveredGameServices_VehicleFallbackActive() ? 1 : 0));
  log.Line("vehicle_input_events=" + std::to_string(
               RecoveredGameServices_VehicleInputEvents()));
  log.Line("vehicle_forwarded_events=" + std::to_string(
               RecoveredGameServices_VehicleForwardedEvents()));
  log.Line("vehicle_housekeeping_events=" + std::to_string(
               RecoveredGameServices_VehicleHousekeepingEvents()));
  log.Line("vehicle_ignored_events=" + std::to_string(
               RecoveredGameServices_VehicleIgnoredEvents()));
  log.Line("vehicle_application_active=" + std::to_string(
               RecoveredGameServices_VehicleApplicationActive() ? 1 : 0));
  log.Line("vehicle_focus_loss_count=" + std::to_string(
               RecoveredGameServices_VehicleFocusLossCount()));
  log.Line("vehicle_focus_gain_count=" + std::to_string(
               RecoveredGameServices_VehicleFocusGainCount()));
  log.Line("vehicle_synthetic_release_count=" + std::to_string(
               RecoveredGameServices_VehicleSyntheticReleaseCount()));
  log.Line("vehicle_suppressed_input_count=" + std::to_string(
               RecoveredGameServices_VehicleSuppressedInputCount()));
  log.Line("vehicle_active_action_count=" + std::to_string(
               RecoveredGameServices_VehicleActiveActionCount()));
  log.Line("vehicle_last_input_failure=" + std::to_string(
               RecoveredGameServices_VehicleLastInputFailure()));
  log.Line("vehicle_last_frame_failure=" + std::to_string(
               RecoveredGameServices_VehicleLastFrameFailure()));
  log.Line("vehicle_last_frame_readiness_issue=" + std::to_string(
               RecoveredGameServices_VehicleLastFrameReadinessIssue()));
  log.Line("vehicle_frame_count=" + std::to_string(
               RecoveredGameServices_VehicleFrameCount()));
  log.Line("vehicle_camera_frame_count=" + std::to_string(
               RecoveredGameServices_VehicleCameraFrameCount()));
  log.Line("vehicle_dropped_time_frame_count=" + std::to_string(
               RecoveredGameServices_VehicleDroppedTimeFrameCount()));
  log.Line("timer_clamped_sample_count=" + std::to_string(
               SUA_ClampedTimerSampleCount()));
  log.Line("timer_clamped_seconds=" + std::to_string(
               SUA_ClampedTimerSeconds()));
  log.Line("vehicle_fallback_count=" + std::to_string(
               RecoveredGameServices_VehicleFallbackCount()));
  log.Line("vehicle_fallback_reason=" + std::to_string(
               RecoveredGameServices_VehicleFallbackReason()));
  SRecoveredVehicleDriveTelemetry vehicleTelemetry = {};
  if (RecoveredGameServices_VehicleDriveTelemetry(&vehicleTelemetry)) {
    log.Line("vehicle_position=" +
             std::to_string(vehicleTelemetry.positionX) + "," +
             std::to_string(vehicleTelemetry.positionY) + "," +
             std::to_string(vehicleTelemetry.positionZ));
    log.Line("vehicle_speed=" +
             std::to_string(vehicleTelemetry.speedX) + "," +
             std::to_string(vehicleTelemetry.speedY) + "," +
             std::to_string(vehicleTelemetry.speedZ));
    log.Line("vehicle_horizontal_distance=" + std::to_string(
                 vehicleTelemetry.horizontalDistance));
    log.Line("vehicle_maximum_horizontal_distance=" + std::to_string(
                 vehicleTelemetry.maximumHorizontalDistance));
    log.Line("vehicle_speed_magnitude=" + std::to_string(
                 vehicleTelemetry.speedMagnitude));
    log.Line("vehicle_maximum_speed_magnitude=" + std::to_string(
                 vehicleTelemetry.maximumSpeedMagnitude));
    log.Line("vehicle_heading_delta=" + std::to_string(
                 vehicleTelemetry.headingDelta));
    log.Line("vehicle_maximum_heading_delta=" + std::to_string(
                 vehicleTelemetry.maximumHeadingDelta));
    log.Line("vehicle_last_bump_flags=" + std::to_string(
                 vehicleTelemetry.lastBumpFlags));
    log.Line("vehicle_touching_ground=" + std::to_string(
                 vehicleTelemetry.touchingGround));
    log.Line("vehicle_ground_contact_frames=" + std::to_string(
                 vehicleTelemetry.groundContactFrames));
    log.Line("vehicle_static_collision_frames=" + std::to_string(
                 vehicleTelemetry.staticCollisionFrames));
    log.Line("vehicle_land_collision_frames=" + std::to_string(
                 vehicleTelemetry.landCollisionFrames));
    log.Line("vehicle_dynamic_collision_frames=" + std::to_string(
                 vehicleTelemetry.dynamicCollisionFrames));
  }
  log.Line(
      "script_mode=bounded-retail-farter-subject-sound-object-farter-corpse-"
      "reference-wav-smoker-dyn-smoker-emission-light-corona-smoke-terrain-"
       "simulation-visual-lamp-skin-resource-smoke-explosion-attribute-taxi-"
       "attribute-bullet-collision-impact-explosion-damage-sound-particles-"
       "vehicle-bootstrap-taxi-subject-vehicle-transition");
  log.Line("vehicle_object=Vehicle.Default");
  log.Line(
      "vehicle_controls=W,S,A,D,Space,LCtrl,arrows,X-stop,F1-change,Escape");
  log.Line("observer_mode=fallback-suspended");
  log.Line("service_hooks=12");
  log.Line("service_frames=" + std::to_string(dwFrames));
  SGRSoftwareRasterStats rasterStats = {};
  GRSoftwareGetTotalStats(&rasterStats);
  log.Line("renderer_frames=" + std::to_string(rasterStats.frames));
  log.Line("renderer_polygons_submitted=" +
           std::to_string(rasterStats.submitted));
  log.Line("renderer_polygons_accepted=" +
           std::to_string(rasterStats.accepted));
  log.Line("renderer_polygons_rasterized=" +
           std::to_string(rasterStats.rasterized));
  log.Line("renderer_rejected_invalid=" +
           std::to_string(rasterStats.rejectedInvalid));
  log.Line("renderer_rejected_unsupported=" +
           std::to_string(rasterStats.rejectedUnsupported));
  log.Line("renderer_rejected_outside=" +
           std::to_string(rasterStats.rejectedOutside));
  log.Line("renderer_rejected_texture=" +
           std::to_string(rasterStats.rejectedTexture));
  log.Line("renderer_pixels_covered=" +
           std::to_string(rasterStats.coveredPixels));
  log.Line("renderer_pixels_written=" +
           std::to_string(rasterStats.writtenPixels));
  log.Line("renderer_pixels_hazed=" +
           std::to_string(rasterStats.hazePixels));
  log.Line("renderer_pixels_transparent=" +
           std::to_string(rasterStats.transparentPixels));
  log.Line("renderer_approximated_bump_polygons=" +
           std::to_string(rasterStats.approximatedBumpPolygons));
  log.Line("renderer_ignored_nonperspective_bump_polygons=" +
           std::to_string(
               rasterStats.ignoredNonPerspectiveBumpPolygons));
  log.Line("renderer_approximated_light_polygons=" +
           std::to_string(rasterStats.approximatedLightPolygons));
  log.Line("renderer_dithered_bump_polygons=" +
           std::to_string(rasterStats.ditheredBumpPolygons));
  log.Line("renderer_light_through_polygons=" +
           std::to_string(rasterStats.lightThroughPolygons));
  log.Line("renderer_lit_polygons=" +
           std::to_string(rasterStats.litPolygons));
  log.Line("renderer_lit_pixels=" +
           std::to_string(rasterStats.litPixels));
  log.Line("renderer_framebuffer_hash=" +
           std::to_string(rasterStats.framebufferHash));
  log.Line("renderer_framebuffer_nonclear_pixels=" +
           std::to_string(rasterStats.framebufferNonClearPixels));
  static const char* const kPolygonTypeNames[TYPE_COUNT] = {
      "flat", "transparent", "gouraud", "texture_perspective",
      "texture_linear", "sprite_perspective", "texture_alpha",
      "sprite_linear", "gouraud_rgb", "texture_sampled",
      "sprite_mip", "texture_gouraud"};
  for (int type = 0; type < TYPE_COUNT; ++type) {
    const std::string prefix =
        std::string("renderer_type_") + kPolygonTypeNames[type];
    log.Line(prefix + "=" +
             std::to_string(rasterStats.submittedByType[type]) + "/" +
             std::to_string(rasterStats.acceptedByType[type]) + "/" +
             std::to_string(rasterStats.rasterizedByType[type]));
  }
  log.Line("game_services_issues=" +
           std::to_string(RecoveredGameServices_Issues()));
  const SRecoveredObserverState* observer =
      RecoveredGameServices_ObserverState();
  if (observer != nullptr) {
    log.Line("observer_input_events=" +
             std::to_string(observer->inputEvents));
    log.Line("observer_position=" + std::to_string(observer->x) + "," +
             std::to_string(observer->y) + "," +
             std::to_string(observer->z));
    log.Line("observer_angles=" + std::to_string(observer->yaw) + "," +
             std::to_string(observer->pitch));
  }
  log.Line("marker=level-ready");

  ZAV_DeInitLevel();
  ZAV_Deinit();
  log.Line("runtime_shutdown=clean");
  return kSuccess;
}

}  // namespace rr2nw
