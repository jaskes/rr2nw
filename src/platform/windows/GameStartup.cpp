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
#include "obase/explosion/ExplosionSubjectState.h"

#include <shlobj.h>

#include <array>
#include <cstdio>
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
                L"rr2nw.exe [--data-dir <path>] [--diagnostics-dir <path>]\n"
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

  log.WideLine("data_dir", data.root);
  log.Line("retail_level_count=9");
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

  RecoveredGameServices_UseRuntime();
  const bool graphInitialized = ZAV_InitGraph(instance) != FALSE;
  log.Line(std::string("graph_initialized=") +
           (graphInitialized ? "1" : "0"));
  const bool levelInitialized =
      graphInitialized && ZAV_InitLevel(levelDirectory.c_str()) != FALSE;
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
  log.Line("vehicle_last_input_failure=" + std::to_string(
               RecoveredGameServices_VehicleLastInputFailure()));
  log.Line("vehicle_frame_count=" + std::to_string(
               RecoveredGameServices_VehicleFrameCount()));
  log.Line("vehicle_camera_frame_count=" + std::to_string(
               RecoveredGameServices_VehicleCameraFrameCount()));
  log.Line("vehicle_dropped_time_frame_count=" + std::to_string(
               RecoveredGameServices_VehicleDroppedTimeFrameCount()));
  log.Line("vehicle_fallback_count=" + std::to_string(
               RecoveredGameServices_VehicleFallbackCount()));
  log.Line("vehicle_fallback_reason=" + std::to_string(
               RecoveredGameServices_VehicleFallbackReason()));
  log.Line(
      "script_mode=bounded-retail-farter-subject-sound-object-farter-corpse-"
      "reference-wav-smoker-dyn-smoker-emission-light-corona-smoke-terrain-"
      "simulation-visual-lamp-skin-resource-smoke-explosion-attribute-taxi-"
      "attribute-bullet-collision-impact-explosion-damage-sound-particles-"
      "vehicle-bootstrap");
  log.Line("vehicle_object=Vehicle.Default");
  log.Line("vehicle_controls=W,S,A,D,Space,LCtrl,arrows,Escape");
  log.Line("observer_mode=fallback-suspended");
  log.Line("service_hooks=12");
  log.Line("service_frames=" + std::to_string(dwFrames));
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
