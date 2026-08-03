#include "GameStartup.h"

#include "RR2NWBuildRevision.h"
#include "GameEntryRuntimeState.h"
#include "RecoveredArenaSeanceRuntime.h"
#include "RecoveredDrawableSceneRuntime.h"
#include "RecoveredGameLevelRuntime.h"
#include "RecoveredGameServicesRuntime.h"
#include "RecoveredGameplayTuningRuntime.h"
#include "RecoveredScriptEventRuntime.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredModRuntime.h"
#include "RecoveredRetailScriptManifest.h"
#include "ZavOverallInfoState.h"
#include "ZavShutdownState.h"
#include "graph.h"
#include "h/super.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/howitzer/HowitzerActiveWorldState.h"
#include "obase/people/PeopleSubjectState.h"
#include "obase/recrcen/RecruitCenterSubjectState.h"
#include "suavik.h"

#include <shlobj.h>

#include <algorithm>
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
  std::wstring saveDirectory;
  std::vector<std::wstring> modDirectories;
  std::wstring modsDirectory;
  std::vector<std::wstring> selectedMods;
  std::wstring startLevel;
  std::wstring missionCenter;
  int startupSaveSlot = -1;
  int startupLoadSlot = -1;
  bool launchSmoke = false;
  bool runtimeSmoke = false;
  bool missionSmoke = false;
  bool missionBriefingSmoke = false;
  bool missionContinuationSmoke = false;
  bool debugMenu = false;
  bool showHelp = false;
  bool showVersion = false;
};

struct RetailData {
  std::wstring root;
  std::vector<std::wstring> levels;
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

bool DiscoverModDirectories(const std::wstring& root,
                            std::vector<std::wstring>* directories,
                            std::wstring* failure) {
  if (!IsDirectory(root)) {
    *failure = L"--mods-dir is not an existing directory: " + root;
    return false;
  }
  std::vector<std::wstring> names;
  WIN32_FIND_DATAW found = {};
  const std::wstring pattern = JoinPath(root, L"*");
  HANDLE search = FindFirstFileW(pattern.c_str(), &found);
  if (search == INVALID_HANDLE_VALUE) {
    *failure = L"cannot enumerate --mods-dir: " + root;
    return false;
  }
  do {
    const std::wstring name(found.cFileName);
    if (name == L"." || name == L".." ||
        (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
      continue;
    const std::wstring candidate = JoinPath(root, name);
    if (IsFile(JoinPath(candidate, L"mod.json"))) names.push_back(name);
  } while (FindNextFileW(search, &found) != FALSE);
  const DWORD enumerationError = GetLastError();
  FindClose(search);
  if (enumerationError != ERROR_NO_MORE_FILES) {
    *failure = L"--mods-dir enumeration failed: " + root;
    return false;
  }
  std::sort(names.begin(), names.end(), [](const std::wstring& left,
                                           const std::wstring& right) {
    const int folded = _wcsicmp(left.c_str(), right.c_str());
    return folded == 0 ? left < right : folded < 0;
  });
  for (const std::wstring& name : names)
    directories->push_back(JoinPath(root, name));
  return true;
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

bool ParseStartupSlot(const std::wstring& value, const wchar_t* option,
                      int* slot, std::wstring* failure) {
  errno = 0;
  wchar_t* end = nullptr;
  const long numeric = std::wcstol(value.c_str(), &end, 10);
  if (errno != 0 || end == value.c_str() || *end != L'\0' ||
      numeric < 1 || numeric > 8) {
    *failure = std::wstring(option) + L" must be between 1 and 8";
    return false;
  }
  *slot = static_cast<int>(numeric - 1);
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
    } else if (argument == L"--mission-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
    } else if (argument == L"--mission-briefing-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionBriefingSmoke = true;
    } else if (argument == L"--mission-continuation-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionContinuationSmoke = true;
    } else if (argument == L"--debug-menu") {
      options->debugMenu = true;
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
    } else if (argument == L"--mod-dir") {
      std::wstring value;
      if (!ParseOptionValue(argc, argv, &index, L"--mod-dir", &value,
                            failure)) {
        return false;
      }
      options->modDirectories.push_back(value);
    } else if (argument.compare(0, 10, L"--mod-dir=") == 0) {
      const std::wstring value = argument.substr(10);
      if (value.empty()) {
        *failure = L"empty value for --mod-dir";
        return false;
      }
      options->modDirectories.push_back(value);
    } else if (argument == L"--mods-dir") {
      if (!options->modsDirectory.empty()) {
        *failure = L"--mods-dir may be specified only once";
        return false;
      }
      if (!ParseOptionValue(argc, argv, &index, L"--mods-dir",
                            &options->modsDirectory, failure)) {
        return false;
      }
    } else if (argument.compare(0, 11, L"--mods-dir=") == 0) {
      if (!options->modsDirectory.empty()) {
        *failure = L"--mods-dir may be specified only once";
        return false;
      }
      options->modsDirectory = argument.substr(11);
      if (options->modsDirectory.empty()) {
        *failure = L"empty value for --mods-dir";
        return false;
      }
    } else if (argument == L"--mod") {
      std::wstring value;
      if (!ParseOptionValue(argc, argv, &index, L"--mod", &value,
                            failure)) {
        return false;
      }
      options->selectedMods.push_back(value);
    } else if (argument.compare(0, 6, L"--mod=") == 0) {
      const std::wstring value = argument.substr(6);
      if (value.empty()) {
        *failure = L"empty value for --mod";
        return false;
      }
      options->selectedMods.push_back(value);
    } else if (argument == L"--diagnostics-dir") {
      if (!ParseOptionValue(argc, argv, &index, L"--diagnostics-dir",
                            &options->diagnosticsDirectory, failure)) {
        return false;
      }
    } else if (argument.compare(0, 18, L"--diagnostics-dir=") == 0) {
      options->diagnosticsDirectory = argument.substr(18);
    } else if (argument == L"--save-dir") {
      if (!ParseOptionValue(argc, argv, &index, L"--save-dir",
                            &options->saveDirectory, failure)) {
        return false;
      }
    } else if (argument.compare(0, 11, L"--save-dir=") == 0) {
      options->saveDirectory = argument.substr(11);
    } else if (argument == L"--start-level") {
      if (!ParseOptionValue(argc, argv, &index, L"--start-level",
                            &options->startLevel, failure)) {
        return false;
      }
    } else if (argument.compare(0, 14, L"--start-level=") == 0) {
      options->startLevel = argument.substr(14);
    } else if (argument == L"--mission-center") {
      if (!ParseOptionValue(argc, argv, &index, L"--mission-center",
                            &options->missionCenter, failure)) {
        return false;
      }
    } else if (argument.compare(0, 17, L"--mission-center=") == 0) {
      options->missionCenter = argument.substr(17);
      if (options->missionCenter.empty()) {
        *failure = L"empty value for --mission-center";
        return false;
      }
    } else if (argument == L"--save-slot" ||
               argument == L"--load-slot") {
      std::wstring value;
      if (!ParseOptionValue(argc, argv, &index, argument.c_str(), &value,
                            failure) ||
          !ParseStartupSlot(
              value, argument.c_str(),
              argument == L"--save-slot" ? &options->startupSaveSlot
                                          : &options->startupLoadSlot,
              failure)) {
        return false;
      }
    } else if (argument.compare(0, 12, L"--save-slot=") == 0) {
      if (!ParseStartupSlot(argument.substr(12), L"--save-slot",
                            &options->startupSaveSlot, failure))
        return false;
    } else if (argument.compare(0, 12, L"--load-slot=") == 0) {
      if (!ParseStartupSlot(argument.substr(12), L"--load-slot",
                            &options->startupLoadSlot, failure))
        return false;
    } else {
      *failure = std::wstring(L"unknown argument: ") + argument;
      return false;
    }
  }
  if (options->startupSaveSlot >= 0 && options->startupLoadSlot >= 0) {
    *failure = L"--save-slot and --load-slot cannot be used together";
    return false;
  }
  if (!options->missionCenter.empty() && !options->missionSmoke) {
    *failure = L"--mission-center requires --mission-smoke";
    return false;
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

std::wstring DefaultSaveDirectory() {
  std::wstring base = EnvironmentValue(L"LOCALAPPDATA");
  if (base.empty()) {
    base = EnvironmentValue(L"TEMP");
  }
  if (base.empty()) {
    base = CurrentDirectory();
  }
  return JoinPath(JoinPath(base, L"RR2NW"), L"saves");
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

class ModRuntimeScope {
 public:
  ~ModRuntimeScope() { RecoveredModRuntime_Release(); }
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
  inspected.levels.reserve(kRetailLevelCount);
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
    inspected.levels.push_back(value);
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

bool AppendDeclaredModLevels(RetailData* data, std::wstring* failure) {
  if (data == nullptr || data->levels.size() != kRetailLevelCount) {
    *failure = L"retail Level catalog is incomplete before mod admission";
    return false;
  }
  const unsigned int count = RecoveredModRuntime_LevelCount();
  data->levels.reserve(data->levels.size() + count);
  for (unsigned int index = 0; index < count; ++index) {
    SRecoveredModLevel declared;
    if (!RecoveredModRuntime_Level(index, &declared)) {
      *failure = L"mod Level catalog could not be enumerated";
      return false;
    }
    const std::wstring id = Utf8ToWide(declared.id);
    const std::wstring base = Utf8ToWide(declared.base);
    if (id.empty() || base.empty()) {
      *failure = L"mod Level catalog contains a non-UTF-8 identity";
      return false;
    }
    bool baseListed = false;
    for (int retail = 0; retail < kRetailLevelCount; ++retail) {
      if (_wcsicmp(base.c_str(),
                   data->levels[static_cast<std::size_t>(retail)].c_str()) ==
          0) {
        baseListed = true;
        break;
      }
    }
    if (!baseListed) {
      *failure = L"mod Level base is not listed in retail game.cfg: " + base;
      return false;
    }
    for (const std::wstring& existing : data->levels) {
      if (_wcsicmp(id.c_str(), existing.c_str()) == 0) {
        *failure = L"mod Level id collides with the active catalog: " + id;
        return false;
      }
    }
    data->levels.push_back(id);
  }
  return true;
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
    if (numeric < 0 ||
        numeric >= static_cast<long>(data->levels.size())) {
      *failure = L"--start-level index is outside the active Level catalog";
      return false;
    }
    data->startLevel = static_cast<int>(numeric);
    return true;
  }

  for (std::size_t index = 0; index < data->levels.size(); ++index) {
    if (_wcsicmp(options.startLevel.c_str(),
                 data->levels[static_cast<std::size_t>(index)].c_str()) == 0) {
      data->startLevel = static_cast<int>(index);
      return true;
    }
  }

  *failure = L"--start-level must be an active catalog index or a Level name "
             L"listed by retail game.cfg/the selected mod";
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

int FindRetailLevel(const RetailData& data, const std::string& identity) {
  const std::wstring wideIdentity = Utf8ToWide(identity.c_str());
  if (wideIdentity.empty()) return -1;
  for (std::size_t index = 0; index < data.levels.size(); ++index) {
    if (_wcsicmp(wideIdentity.c_str(),
                 data.levels[index].c_str()) == 0)
      return static_cast<int>(index);
  }
  return -1;
}

std::string RecoveredLevelStartFailure(const char* stage) {
  std::string detail = stage;
  detail += " (game_entry=";
  detail += std::to_string(GameEntry_RuntimeIssues());
  detail += ", game_level=";
  detail += std::to_string(RecoveredGameLevel_Issues());
  detail += ", level_runtime=";
  detail += std::to_string(RecoveredLevelRuntime_Issues());
  detail += ", services=";
  detail += std::to_string(RecoveredGameServices_Issues());
  detail += ")";
  const char* manifestFailure = RecoveredRetailScriptManifest_LastError();
  if (manifestFailure != nullptr && manifestFailure[0] != '\0') {
    detail += ": ";
    detail += manifestFailure;
  }
  return detail;
}

bool StartRecoveredLevel(const RetailData& data, int levelIndex,
                         std::string* failure) {
  if (failure != nullptr) failure->clear();
  if (levelIndex < 0 ||
      levelIndex >= static_cast<int>(data.levels.size())) {
    if (failure != nullptr) *failure = "Level catalog index is invalid";
    return false;
  }
  std::string levelIdentity;
  if (!WideToSystemPath(
          data.levels[static_cast<std::size_t>(levelIndex)],
          &levelIdentity)) {
    if (failure != nullptr)
      *failure = "Level identity is not representable by the Windows ANSI "
                 "code page";
    return false;
  }
  std::string levelDirectory;
  char physicalDirectory[4096] = {};
  if (!RecoveredModRuntime_ActivateLevel(
          levelIdentity.c_str(), physicalDirectory,
          sizeof(physicalDirectory))) {
    if (failure != nullptr) {
      *failure = RecoveredModRuntime_LastError();
    }
    return false;
  }
  levelDirectory = physicalDirectory;
  if (ZAV_InitLevel(levelDirectory.c_str()) == FALSE ||
      !RecoveredGameLevel_IsReady()) {
    if (failure != nullptr)
      *failure = RecoveredLevelStartFailure("Level initialization failed");
    return false;
  }
  PIN_InitEverything();
  SUA_InitEverything();
  if (!RecoveredGameServices_SessionReady()) {
    if (failure != nullptr)
      *failure = RecoveredLevelStartFailure("session initialization failed");
    return false;
  }
  ZAV_BeginLoop();
  // The retail main loop rebuilt the briefing viewport after every Level
  // start. Briefing flights own a synchronous render loop and cannot borrow
  // the regular gameplay viewport, so preserve that lifecycle boundary here.
  RecoveredGameServices_RefreshBriefingViewport();
  if (!RecoveredGameServices_IsReady()) {
    if (failure != nullptr)
      *failure = RecoveredLevelStartFailure("loop initialization failed");
    return false;
  }
  return true;
}

bool ProcessCrossLevelLoad(const RetailData& data, int* currentLevelIndex,
                           bool silent, StartupLog* log) {
  SRecoveredCrossLevelLoadRequest request;
  if (!RecoveredGameServices_TakeCrossLevelLoadRequest(&request))
    return true;

  const int sourceLevelIndex =
      FindRetailLevel(data, request.sourceLevel);
  const int targetLevelIndex =
      FindRetailLevel(data, request.targetLevel);
  if (sourceLevelIndex < 0 || sourceLevelIndex != *currentLevelIndex ||
      targetLevelIndex < 0) {
    const std::string detail =
        targetLevelIndex < 0
            ? "save slot names a Level that is not in the active catalog"
            : "cross-Level source no longer matches the active Level";
    RecoveredGameServices_RecordCrossLevelLoadFailure(
        request, detail, false, false);
    if (log != nullptr) {
      log->Line("cross_level_load_preflight_failure=" + detail);
    }
    ShowMessage(silent, MB_ICONERROR, L"RR2NW save/load error",
                Utf8ToWide(detail.c_str()));
    return true;
  }

  if (log != nullptr) {
    log->Line("cross_level_load_begin=" + request.sourceLevel + "->" +
              request.targetLevel);
  }
  ZAV_DeInitLevel();

  std::string targetFailure;
  bool targetStarted =
      StartRecoveredLevel(data, targetLevelIndex, &targetFailure);
  SLevelContinuationSummary restored;
  bool targetRestored =
      targetStarted && RecoveredGameServices_ApplyCrossLevelLoad(
                           request, &restored);
  if (targetRestored) {
    *currentLevelIndex = targetLevelIndex;
    if (log != nullptr) {
      log->Line("cross_level_load_commit=" + request.targetLevel);
      log->Line("cross_level_load_world_fingerprint=" +
                std::to_string(restored.restoredWorldFingerprint));
    }
    return true;
  }

  if (targetStarted) {
    const SRecoveredSaveMenuState* state =
        RecoveredGameServices_SaveMenuState();
    targetFailure =
        state != nullptr && !state->lastError.empty()
            ? state->lastError
            : "target Level continuation restore failed";
  }
  ZAV_DeInitLevel();

  std::string sourceFailure;
  const bool sourceStarted =
      StartRecoveredLevel(data, sourceLevelIndex, &sourceFailure);
  SLevelContinuationSummary rolledBack;
  const bool sourceRestored =
      sourceStarted && RecoveredGameServices_RestoreLevelContinuation(
                           request.sourceContinuation, &rolledBack);
  std::string detail = "cross-Level load failed: " + targetFailure;
  if (!sourceRestored) {
    detail += "; source rollback failed: ";
    if (!sourceStarted) {
      detail += sourceFailure;
    } else {
      detail += RecoveredGameServices_LastLevelContinuationError();
    }
  }
  RecoveredGameServices_RecordCrossLevelLoadFailure(
      request, detail, true, sourceRestored);
  if (log != nullptr) {
    log->Line(std::string("cross_level_load_rollback=") +
              (sourceRestored ? "restored" : "failed"));
    log->Line("cross_level_load_failure=" + detail);
  }
  ShowMessage(silent, MB_ICONERROR, L"RR2NW save/load error",
              Utf8ToWide(detail.c_str()));
  return sourceRestored;
}

bool ProcessDebugLevelSwitch(const RetailData& data,
                             int* currentLevelIndex, bool silent,
                             StartupLog* log) {
  SRecoveredDebugLevelSwitchRequest request;
  if (!RecoveredGameServices_TakeDebugLevelSwitchRequest(&request))
    return true;

  const int sourceLevelIndex =
      FindRetailLevel(data, request.sourceLevel);
  const int targetLevelIndex =
      FindRetailLevel(data, request.targetLevel);
  if (sourceLevelIndex < 0 || sourceLevelIndex != *currentLevelIndex ||
      targetLevelIndex < 0 || request.sourceContinuation.empty()) {
    const std::string detail =
        "debug Level switch no longer matches the active catalog/session";
    RecoveredGameServices_RecordDebugLevelSwitchResult(
        request, false, false, false, detail);
    if (log != nullptr)
      log->Line("debug_level_switch_preflight_failure=" + detail);
    ShowMessage(silent, MB_ICONERROR, L"RR2NW debug Level switch error",
                Utf8ToWide(detail.c_str()));
    return true;
  }

  if (log != nullptr)
    log->Line("debug_level_switch_begin=" + request.sourceLevel + "->" +
              request.targetLevel);
  ZAV_DeInitLevel();

  std::string targetFailure;
  if (StartRecoveredLevel(data, targetLevelIndex, &targetFailure)) {
    *currentLevelIndex = targetLevelIndex;
    RecoveredGameServices_RecordDebugLevelSwitchResult(
        request, true, false, false, std::string());
    if (log != nullptr)
      log->Line("debug_level_switch_commit=" + request.targetLevel);
    return true;
  }

  ZAV_DeInitLevel();
  std::string sourceFailure;
  const bool sourceStarted =
      StartRecoveredLevel(data, sourceLevelIndex, &sourceFailure);
  SLevelContinuationSummary restored;
  const bool sourceRestored =
      sourceStarted && RecoveredGameServices_RestoreLevelContinuation(
                           request.sourceContinuation, &restored);
  std::string detail = "debug Level switch failed: " + targetFailure;
  if (!sourceRestored) {
    detail += "; source rollback failed: ";
    detail += sourceStarted
                  ? RecoveredGameServices_LastLevelContinuationError()
                  : sourceFailure;
  }
  RecoveredGameServices_RecordDebugLevelSwitchResult(
      request, false, true, sourceRestored, detail);
  if (log != nullptr) {
    log->Line(std::string("debug_level_switch_rollback=") +
              (sourceRestored ? "restored" : "failed"));
    log->Line("debug_level_switch_failure=" + detail);
  }
  ShowMessage(silent, MB_ICONERROR, L"RR2NW debug Level switch error",
              Utf8ToWide(detail.c_str()));
  return sourceRestored;
}

bool ProcessCampaignRestart(const RetailData& data,
                            int* currentLevelIndex, bool silent,
                            StartupLog* log) {
  SRecoveredCampaignRestartRequest request;
  if (!RecoveredGameServices_TakeCampaignRestartRequest(&request))
    return true;

  const int sourceLevelIndex = FindRetailLevel(data, request.level);
  if (sourceLevelIndex < 0 || sourceLevelIndex != *currentLevelIndex ||
      request.sourceContinuation.empty() ||
      !request.sourceContinuationSummary.ready) {
    const std::string detail =
        "current Level restart no longer matches the active catalog/session";
    RecoveredGameServices_RecordCampaignRestartResult(
        request, false, false, false, detail);
    if (log != nullptr)
      log->Line("campaign_restart_preflight_failure=" + detail);
    ShowMessage(silent, MB_ICONERROR, L"RR2NW Level restart error",
                Utf8ToWide(detail.c_str()));
    return true;
  }

  if (log != nullptr)
    log->Line("campaign_restart_begin=" + request.level);
  ZAV_DeInitLevel();

  std::string restartFailure;
  if (StartRecoveredLevel(data, sourceLevelIndex, &restartFailure)) {
    *currentLevelIndex = sourceLevelIndex;
    RecoveredGameServices_RecordCampaignRestartResult(
        request, true, false, false, std::string());
    if (log != nullptr)
      log->Line("campaign_restart_commit=" + request.level);
    return true;
  }

  ZAV_DeInitLevel();
  std::string rollbackStartFailure;
  const bool rollbackStarted =
      StartRecoveredLevel(data, sourceLevelIndex, &rollbackStartFailure);
  SLevelContinuationSummary restored;
  const bool rollbackRestored = rollbackStarted &&
      RecoveredGameServices_RestoreLevelContinuation(
          request.sourceContinuation, &restored);
  std::string detail = "current Level restart failed: " + restartFailure;
  if (!rollbackRestored) {
    detail += "; source rollback failed: ";
    detail += rollbackStarted
                  ? RecoveredGameServices_LastLevelContinuationError()
                  : rollbackStartFailure;
  }
  RecoveredGameServices_RecordCampaignRestartResult(
      request, false, true, rollbackRestored, detail);
  if (log != nullptr) {
    log->Line(std::string("campaign_restart_rollback=") +
              (rollbackRestored ? "restored" : "failed"));
    log->Line("campaign_restart_failure=" + detail);
  }
  ShowMessage(silent, MB_ICONERROR, L"RR2NW Level restart error",
              Utf8ToWide(detail.c_str()));
  return rollbackRestored;
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
                L"          [--mod-dir <path>]... [--mods-dir <path>]\n"
                L"          [--mod <id>]...\n"
                L"          [--diagnostics-dir <path>] [--save-dir <path>]\n"
                L"          [--save-slot <1..8> | --load-slot <1..8>]\n"
                L"          [--debug-menu] [--launch-smoke] [--runtime-smoke]\n"
                L"          [--mission-smoke | --mission-briefing-smoke |\n"
                L"           --mission-continuation-smoke]\n"
                L"          [--mission-center <name>]\n"
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
  for (std::wstring& directory : options.modDirectories)
    directory = AbsolutePath(directory);
  if (!options.modsDirectory.empty())
    options.modsDirectory = AbsolutePath(options.modsDirectory);
  const bool defaultSaveDirectory = options.saveDirectory.empty();
  if (defaultSaveDirectory) {
    options.saveDirectory = DefaultSaveDirectory();
  } else {
    options.saveDirectory = AbsolutePath(options.saveDirectory);
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
  log.Line(std::string("debug_menu_requested=") +
           (options.debugMenu ? "1" : "0"));

  RetailData data;
  if (!LocateRetailData(options, &data, &failure)) {
    log.WideLine("failure", failure);
    log.Line("marker=data-not-ready");
    ShowMessage(options.launchSmoke || options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW data error",
                failure + L"\n\nDiagnostic log:\n" + log.path());
    return kDataNotReady;
  }
  ModRuntimeScope modRuntimeScope;
  std::string baseDataPath;
  std::string modFailure;
  bool modReady = false;
  const std::size_t explicitModCount = options.modDirectories.size();
  std::vector<std::wstring> candidateModDirectories =
      options.modDirectories;
  if (!options.modsDirectory.empty() &&
      !DiscoverModDirectories(options.modsDirectory,
                              &candidateModDirectories, &failure)) {
    modFailure = WideToUtf8(failure);
  }
  std::vector<std::string> candidateModPaths;
  std::vector<const char*> candidateModPathPointers;
  std::vector<std::string> requestedModIds;
  std::vector<const char*> requestedModIdPointers;
  if (!WideToSystemPath(data.root, &baseDataPath)) {
    modFailure = "base data path is not representable by the Windows ANSI "
                 "code page";
  }
  if (modFailure.empty()) {
    candidateModPaths.reserve(candidateModDirectories.size());
    for (const std::wstring& directory : candidateModDirectories) {
      std::string path;
      if (!WideToSystemPath(directory, &path)) {
        modFailure = "a mod path is not representable by the Windows ANSI "
                     "code page";
        break;
      }
      candidateModPaths.push_back(std::move(path));
    }
  }
  if (modFailure.empty()) {
    requestedModIds.reserve(options.selectedMods.size());
    for (const std::wstring& selected : options.selectedMods) {
      std::string id;
      if (!WideToSystemPath(selected, &id)) {
        modFailure = "a requested mod id is not representable by the "
                     "Windows ANSI code page";
        break;
      }
      requestedModIds.push_back(std::move(id));
    }
  }
  if (modFailure.empty()) {
    candidateModPathPointers.reserve(candidateModPaths.size());
    for (const std::string& path : candidateModPaths)
      candidateModPathPointers.push_back(path.c_str());
    requestedModIdPointers.reserve(requestedModIds.size());
    for (const std::string& id : requestedModIds)
      requestedModIdPointers.push_back(id.c_str());
    const bool activateAllDiscovered =
        !options.modsDirectory.empty() && options.selectedMods.empty();
    modReady = RecoveredModRuntime_ConfigureStack(
        baseDataPath.c_str(),
        candidateModPathPointers.empty() ? nullptr
                                         : candidateModPathPointers.data(),
        candidateModPathPointers.size(), explicitModCount,
        requestedModIdPointers.empty() ? nullptr
                                       : requestedModIdPointers.data(),
        requestedModIdPointers.size(), activateAllDiscovered);
    if (!modReady) modFailure = RecoveredModRuntime_LastError();
  }
  if (!modReady) {
    if (modFailure.empty()) modFailure = "mod runtime rejected the data set";
    log.Line("mod_issues=" +
             std::to_string(RecoveredModRuntime_Issues()));
    log.Line("mod_error=" + modFailure);
    log.Line("marker=mod-not-ready");
    ShowMessage(options.launchSmoke || options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW mod error",
                Utf8ToWide(modFailure.c_str()) +
                    L"\n\nDiagnostic log:\n" + log.path());
    return kDataNotReady;
  }
  if (!AppendDeclaredModLevels(&data, &failure)) {
    log.WideLine("failure", failure);
    log.Line("marker=mod-level-catalog-not-ready");
    ShowMessage(options.launchSmoke || options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW mod Level catalog error",
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
  log.Line("level_catalog_count=" + std::to_string(data.levels.size()));
  log.Line(std::string("start_level_source=") +
           (options.startLevel.empty() ? "game.cfg" : "command-line"));
  if (!options.startLevel.empty()) {
    log.WideLine("start_level_requested", options.startLevel);
  }
  log.Line("start_level=" + std::to_string(data.startLevel));
  log.WideLine("start_level_dir",
               data.levels[static_cast<std::size_t>(data.startLevel)]);
  log.Line("data_access=read-only");
  const SRecoveredModRuntimeSummary* modSummary =
      RecoveredModRuntime_Summary();
  log.Line(std::string("mod_active=") +
           (RecoveredModRuntime_IsActive() ? "1" : "0"));
  if (options.modDirectories.size() == 1)
    log.WideLine("mod_dir", options.modDirectories[0]);
  for (std::size_t index = 0; index < options.modDirectories.size(); ++index) {
    const std::string key = "mod_dir_" + std::to_string(index);
    log.WideLine(key.c_str(), options.modDirectories[index]);
  }
  if (!options.modsDirectory.empty())
    log.WideLine("mods_dir", options.modsDirectory);
  for (std::size_t index = 0; index < options.selectedMods.size(); ++index) {
    const std::string key = "mod_requested_" + std::to_string(index);
    log.WideLine(key.c_str(), options.selectedMods[index]);
  }
  if (modSummary != nullptr && RecoveredModRuntime_IsActive()) {
    log.Line("mod_id=" + std::string(modSummary->id));
    log.Line("mod_version=" + std::string(modSummary->version));
    log.Line("mod_schema=" + std::to_string(modSummary->schemaVersion));
    log.Line("mod_engine_api=" + std::to_string(modSummary->engineApi));
    log.Line("mod_files=" + std::to_string(modSummary->fileCount));
    log.Line("mod_levels=" + std::to_string(modSummary->levelCount));
    log.Line("mod_bytes=" + std::to_string(modSummary->totalBytes));
    log.Line("mod_fingerprint=" +
             std::to_string(modSummary->modFingerprint));
    log.Line("mod_candidates=" +
             std::to_string(modSummary->candidateCount));
    log.Line("mod_count=" + std::to_string(modSummary->modCount));
    std::string mountOrder;
    for (unsigned int index = 0; index < RecoveredModRuntime_ModCount();
         ++index) {
      SRecoveredModPackage package;
      if (!RecoveredModRuntime_Mod(index, &package)) continue;
      if (!mountOrder.empty()) mountOrder += ",";
      mountOrder += package.id;
      const std::string prefix = "mod_" + std::to_string(index) + "_";
      log.Line(prefix + "id=" + package.id);
      log.Line(prefix + "version=" + package.version);
      log.Line(prefix + "files=" + std::to_string(package.fileCount));
      log.Line(prefix + "levels=" + std::to_string(package.levelCount));
      log.Line(prefix + "bytes=" + std::to_string(package.totalBytes));
      log.Line(prefix + "fingerprint=" +
               std::to_string(package.fingerprint));
    }
    log.Line("mod_mount_order=" + mountOrder);
  }
  log.Line("mod_access=read-only");
  log.WideLine("save_dir", options.saveDirectory);
  log.Line(std::string("save_dir_source=") +
           (defaultSaveDirectory ? "local-app-data" : "command-line"));
  log.Line("marker=retail-data-ready");

  if (options.launchSmoke) {
    log.Line("recovered_runtime=skipped-for-launch-smoke");
    log.Line("marker=pre-content-ready");
    return kSuccess;
  }

  if (!EnsureDirectory(options.saveDirectory) ||
      !RecoveredGameServices_ConfigureSaveDirectory(
          options.saveDirectory)) {
    log.WideLine("failure_save_dir", options.saveDirectory);
    log.Line("marker=save-directory-not-ready");
    ShowMessage(options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW save directory error",
                L"Cannot create or configure the save directory:\n" +
                    options.saveDirectory +
                    L"\n\nDiagnostic log:\n" + log.path());
    return kRuntimeNotReady;
  }
  log.Line("save_directory_ready=1");
  std::vector<std::string> debugLevelCatalog;
  debugLevelCatalog.reserve(data.levels.size());
  for (const std::wstring& level : data.levels) {
    const std::string identity = WideToUtf8(level);
    if (identity.empty()) {
      log.Line("failure=debug Level identity is not representable as UTF-8");
      return kRuntimeNotReady;
    }
    debugLevelCatalog.push_back(identity);
  }
  if (!RecoveredGameServices_ConfigureDebugMenu(
          options.debugMenu, debugLevelCatalog)) {
    const SRecoveredDebugMenuState* debugState =
        RecoveredGameServices_DebugMenuState();
    const std::string detail =
        debugState == nullptr || debugState->lastError.empty()
            ? "debug menu configuration failed"
            : debugState->lastError;
    log.Line("failure_debug_menu=" + detail);
    ShowMessage(options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW debug menu error", Utf8ToWide(detail.c_str()));
    return kRuntimeNotReady;
  }
  log.Line(std::string("debug_menu_configured=") +
           (options.debugMenu ? "1" : "0"));
  if (options.startupSaveSlot >= 0) {
    log.Line("startup_save_slot=" +
             std::to_string(options.startupSaveSlot + 1));
  }
  if (options.startupLoadSlot >= 0) {
    log.Line("startup_load_slot=" +
             std::to_string(options.startupLoadSlot + 1));
  }

  std::string levelIdentity;
  std::string levelDirectory;
  char physicalLevelDirectory[4096] = {};
  if (!WideToSystemPath(
          data.levels[static_cast<std::size_t>(data.startLevel)],
          &levelIdentity) ||
      !RecoveredModRuntime_ActivateLevel(
          levelIdentity.c_str(), physicalLevelDirectory,
          sizeof(physicalLevelDirectory))) {
    log.Line(std::string("failure=") +
             (RecoveredModRuntime_LastError()[0] == '\0'
                  ? "level identity/path is not representable"
                  : RecoveredModRuntime_LastError()));
    log.Line("marker=level-not-ready");
    ShowMessage(options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW runtime error",
                L"The selected Level path cannot be represented by the "
                L"current Windows ANSI code page.\n\nDiagnostic log:\n" +
                    log.path());
    return kRuntimeNotReady;
  }
  levelDirectory = physicalLevelDirectory;
  log.Line(std::string("level_catalog_derived=") +
           (RecoveredModRuntime_ActiveLevelIsDerived() ? "1" : "0"));
  if (RecoveredModRuntime_ActiveLevelBase() != nullptr)
    log.Line(std::string("level_catalog_base=") +
             RecoveredModRuntime_ActiveLevelBase());
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
    log.Line("arena_seance_issues=" +
             std::to_string(RecoveredArenaSeance_Issues()));
    log.Line("arena_seance_extended_issues=" +
             std::to_string(RecoveredArenaSeance_ExtendedIssues()));
    if (RecoveredArenaSeance_LastError()[0] != 0) {
      log.Line(std::string("arena_seance_error=") +
               RecoveredArenaSeance_LastError());
    }
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
  const SRecoveredSaveMenuState* saveMenuState =
      RecoveredGameServices_SaveMenuState();
  log.Line("save_menu_configured=" +
           std::to_string(saveMenuState != nullptr &&
                                  saveMenuState->configured
                              ? 1
                              : 0));
  log.Line("save_menu_native_installed=" +
           std::to_string(saveMenuState != nullptr &&
                                  saveMenuState->nativeMenuInstalled
                              ? 1
                              : 0));
  log.Line("save_menu_slots=" +
           std::to_string(LevelSaveSlot_Count()));
  log.Line("save_menu_preview_format=PNG-indexed-640x480");
  const SRecoveredDebugMenuState* debugMenuState =
      RecoveredGameServices_DebugMenuState();
  log.Line("debug_menu_native_installed=" +
           std::to_string(debugMenuState != nullptr &&
                                  debugMenuState->nativeMenuInstalled
                              ? 1
                              : 0));
  log.Line("debug_menu_vehicle_types=" +
           std::to_string(debugMenuState == nullptr
                              ? 0u
                              : debugMenuState->vehicleTypeCount));
  log.Line("debug_menu_first_occupied_vehicle_index=" +
           std::to_string(debugMenuState == nullptr
                              ? -1
                              : debugMenuState
                                    ->firstOccupiedVehicleIndex));
  const std::size_t debugVehicleTypeCount =
      RecoveredGameServices_DebugVehicleTypeCount();
  for (std::size_t index = 0; index < debugVehicleTypeCount; ++index) {
    SRecoveredDebugVehicleType type;
    if (!RecoveredGameServices_DebugVehicleType(index, &type))
      continue;
    const std::string key =
        "debug_menu_vehicle_" + std::to_string(index);
    log.Line(key + "=" + std::to_string(type.vehicleType) + "/" +
             std::to_string(type.vesselKind) + "/" +
             std::to_string(type.vesselProfile));
    log.Line(key + "_identity=" + type.taxiAttribute + "/" +
             type.vehicleAttribute + "/" + type.dynamic);
  }
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
  log.Line("vehicle_death_camera_initialized=" +
           std::to_string(
               RecoveredGameServices_VehicleDeathCameraReady() ? 1 : 0));
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
  log.Line("vehicle_probe_stability_recoveries=" + std::to_string(
               RecoveredGameServices_VehicleProbeStabilityRecoveries()));
  log.Line("vehicle_probe_rollbacks=" + std::to_string(
               RecoveredGameServices_VehicleProbeRollbacks()));
  log.Line("vehicle_probe_horizontal_distance=" + std::to_string(
               RecoveredGameServices_VehicleProbeHorizontalDistance()));
  log.Line("vehicle_death_camera_probe_activations=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraProbeActivations()));
  log.Line("vehicle_death_camera_probe_ascent_frames=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraProbeAscentFrames()));
  log.Line("vehicle_death_camera_probe_terminal_frames=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraProbeTerminalFrames()));
  log.Line("vehicle_death_camera_probe_completion_transitions=" +
           std::to_string(
               RecoveredGameServices_VehicleDeathCameraProbeCompletionTransitions()));
  log.Line("vehicle_death_camera_probe_finite_cameras=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraProbeFiniteCameras()));
  log.Line("vehicle_death_camera_probe_rollbacks=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraProbeRollbacks()));
  log.Line("vehicle_control_replay_ready=" + std::to_string(
               RecoveredGameServices_VehicleControlReplayReady() ? 1 : 0));
  SRecoveredVehicleControlReplayTelemetry replayTelemetry = {};
  if (RecoveredGameServices_VehicleControlReplayTelemetry(
          &replayTelemetry)) {
    log.Line("vehicle_control_replay_records=" + std::to_string(
                 replayTelemetry.actionRecords) + "/" +
             std::to_string(replayTelemetry.focusRecords));
    log.Line("vehicle_control_replay_synthetic_releases=" +
             std::to_string(replayTelemetry.syntheticReleases));
    log.Line("vehicle_control_replay_frames=" +
             std::to_string(replayTelemetry.simulationFrames));
    log.Line("vehicle_control_replay_state_match=" +
             std::to_string(replayTelemetry.stateMatches));
    log.Line("vehicle_control_replay_clock_match=" +
             std::to_string(replayTelemetry.clockMatches));
    log.Line("vehicle_control_replay_random_match=" +
             std::to_string(replayTelemetry.randomMatches));
    log.Line("vehicle_control_replay_rollbacks=" +
             std::to_string(replayTelemetry.rollbacks));
    log.Line("vehicle_control_replay_encoded_bytes=" +
             std::to_string(replayTelemetry.encodedBytes));
    log.Line("vehicle_control_replay_journal_fingerprint=" +
             std::to_string(replayTelemetry.journalFingerprint));
    log.Line("vehicle_control_replay_state_fingerprints=" +
             std::to_string(replayTelemetry.recordedStateFingerprint) +
             "/" +
             std::to_string(replayTelemetry.replayedStateFingerprint));
  }
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
               RecoveredArenaSeance_BulletActiveWorldResumedMoves()) + "/" +
           std::to_string(
               RecoveredArenaSeance_BulletActiveWorldTombstonedMasters()));
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
  log.Line("skin_animations_initialized=" +
           std::to_string(
               RecoveredArenaSeance_SkinAnimationsReady() ? 1 : 0));
  log.Line("skin_animation_entry_calls=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationEntryCallCount()));
  log.Line("skin_animated_models=" + std::to_string(
               RecoveredArenaSeance_SkinAnimatedModelCount()));
  log.Line("skin_animation_commands=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationCommandCount()));
  log.Line("skin_animation_source_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationSourceFingerprint()));
  log.Line("skin_animation_state_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationStateFingerprint()));
  log.Line("skin_animation_pose_temporal_models=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationPoseTemporalModelCount()));
  log.Line("skin_animation_pose_changed_models=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationPoseChangedModelCount()));
  log.Line("skin_animation_pose_samples=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationPoseSampleCount()));
  log.Line("skin_animation_pose_restored_modifiers=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationPoseRestoredModifierCount()));
  log.Line("skin_animation_pose_fingerprint=" + std::to_string(
               RecoveredArenaSeance_SkinAnimationPoseFingerprint()));
  log.Line("static_mechanisms_initialized=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismsReady() ? 1 : 0));
  log.Line("static_mechanism_target_level=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismTargetLevel() ? 1 : 0));
  log.Line("static_mechanism_level_one=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismLevelOne() ? 1 : 0));
  log.Line("static_mechanism_level_five=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismLevelFive() ? 1 : 0));
  log.Line("static_mechanism_bindings=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismBindingCount()));
  log.Line("static_mechanism_waterwheels=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismWaterwheelCount()));
  log.Line("static_mechanism_flags=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismFlagCount()));
  log.Line("static_mechanism_rotating=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismRotatingCount()));
  log.Line("static_mechanism_doors=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismDoorCount()));
  log.Line("static_mechanism_pol16=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismPol16Count()));
  log.Line("static_mechanism_changed_bindings=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismChangedBindingCount()));
  log.Line("static_mechanism_pose_samples=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismPoseSampleCount()));
  log.Line("static_mechanism_restored_modifiers=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismRestoredModifierCount()));
  log.Line("static_mechanism_fingerprint=" + std::to_string(
               RecoveredArenaSeance_StaticMechanismFingerprint()));
  log.Line("teleport_routes_initialized=" + std::to_string(
               RecoveredArenaSeance_TeleportRoutesReady() ? 1 : 0));
  log.Line("teleport_target_level=" + std::to_string(
               RecoveredArenaSeance_TeleportTargetLevel() ? 1 : 0));
  log.Line("teleport_capacity=" + std::to_string(
               RecoveredArenaSeance_TeleportCapacity()));
  log.Line("teleport_route_count=" + std::to_string(
               RecoveredArenaSeance_TeleportRouteCount()));
  log.Line("teleport_probe_rejected_non_player=" + std::to_string(
               RecoveredArenaSeance_TeleportProbeRejectedNonPlayer()));
  log.Line("teleport_probe_physics_collisions=" + std::to_string(
               RecoveredArenaSeance_TeleportProbePhysicsCollisions()));
  log.Line("teleport_probe_applied_player=" + std::to_string(
               RecoveredArenaSeance_TeleportProbeAppliedPlayer()));
  log.Line("teleport_probe_vehicle_rollbacks=" + std::to_string(
               RecoveredArenaSeance_TeleportProbeVehicleRollbacks()));
  log.Line("teleport_fingerprint=" + std::to_string(
               RecoveredArenaSeance_TeleportFingerprint()));
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
  log.Line("people_near_far_pose_probe=" +
           std::to_string(
               RecoveredArenaSeance_PeopleProbeCadenceBounded()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleProbeRenderedPoseFrames()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleProbeViewBoundaryResets()));
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
  log.Line("tank_near_far_pose_probe=" +
           std::to_string(RecoveredArenaSeance_TankProbeCadenceBounded()) +
           "/" + std::to_string(
               RecoveredArenaSeance_TankProbeRenderedPoseFrames()) +
           "/" + std::to_string(
               RecoveredArenaSeance_TankProbeViewBoundaryResets()));
  log.Line("commander_roster=" +
           std::to_string(RecoveredArenaSeance_CommanderCount()) + "/" +
           std::to_string(RecoveredArenaSeance_CommanderCapacity()));
  log.Line("commander_hostile_links=" +
           std::to_string(RecoveredArenaSeance_CommanderHostileLinks()));
  log.Line("commander_fingerprint=" + std::to_string(
               RecoveredArenaSeance_CommanderFingerprint()));
  log.Line("mission_project_table=" +
           std::to_string(
               RecoveredArenaSeance_MissionProjectsReady() ? 1 : 0) + "/" +
           std::to_string(RecoveredArenaSeance_MissionProjectCapacity()) +
           "/" + std::to_string(
               RecoveredArenaSeance_MissionProjectNodeCapacity()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionProjectHeapCapacity()));
  log.Line("mission_project_catalog=" +
           std::to_string(RecoveredArenaSeance_MissionProjectCount()) + "/" +
           std::to_string(RecoveredArenaSeance_MissionProjectNodeCount()) +
           "/" + std::to_string(
               RecoveredArenaSeance_MissionProjectDataBytes()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionProjectSummaryCount()) + "/" +
           std::to_string(
               RecoveredArenaSeance_MissionProjectPermanentCount()));
  log.Line("mission_project_fingerprint=" + std::to_string(
               RecoveredArenaSeance_MissionProjectFingerprint()));
  log.Line("recruit_center_roster=" +
           std::to_string(RecoveredArenaSeance_RecruitCentersReady() ? 1 : 0) +
           "/" + std::to_string(RecoveredArenaSeance_RecruitCenterCapacity()) +
           "/" + std::to_string(RecoveredArenaSeance_RecruitCenterCount()) +
           "/" + std::to_string(RecoveredArenaSeance_RecruitCenterVideoCount()) +
           "/" + std::to_string(RecoveredArenaSeance_RecruitCenterDefaultTaxiCount()) +
           "/" + std::to_string(RecoveredArenaSeance_RecruitCenterDictionaryCount()));
  log.Line("recruit_center_fingerprint=" + std::to_string(
               RecoveredArenaSeance_RecruitCenterFingerprint()));
  log.Line("recruit_center_admission_initial=" + std::to_string(
               RecoveredArenaSeance_RecruitCenterRejectedCollisions()) + "/" +
           std::to_string(
               RecoveredArenaSeance_RecruitCenterPlayerCollisions()) + "/" +
           std::to_string(RecoveredArenaSeance_RecruitCenterAdmissions()) +
           "/" + std::to_string(
               RecoveredArenaSeance_RecruitCenterStagedMissions()) + "/" +
           std::to_string(
               RecoveredArenaSeance_RecruitCenterExistingMissionVisits()) +
           "/" + std::to_string(
               RecoveredArenaSeance_RecruitCenterNoProjectVisits()) + "/" +
           std::to_string(RecoveredArenaSeance_RecruitCenterEjections()) +
           "/" + std::to_string(
               RecoveredArenaSeance_RecruitCenterAdmissionFailures()));
  log.Line("mission_project_deferred_howitzers=" + std::to_string(
               RecoveredArenaSeance_MissionProjectDeferredHowitzerCount()));
  log.Line("mission_project_deferred_destroyables=" + std::to_string(
               RecoveredArenaSeance_MissionProjectDeferredDestroyableCount()));
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
  log.Line("active_world_engine_compatibility=" + std::to_string(
               RecoveredArenaSeance_ActiveWorldEngineCompatibility()));
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
  const int howitzerLive = RecoveredArenaSeance_HowitzerLiveCount();
  const int howitzerReady = RecoveredArenaSeance_HowitzerReadyLiveCount();
  const int howitzerOccupied =
      RecoveredArenaSeance_HowitzerOccupiedHolderCount();
  log.Line("howitzer_subject_roster=" + std::to_string(howitzerLive) + "/" +
           std::to_string(howitzerReady) + "/" +
           std::to_string(howitzerOccupied));
  std::vector<unsigned char> startupHowitzerState;
  const bool startupHowitzerStateReady =
      HowitzerActiveWorldState_CaptureStable(
          g_super.m_context, &startupHowitzerState);
  log.Line("howitzer_active_world_state=" +
           std::to_string(startupHowitzerStateReady ? 1 : 0) + "/" +
           std::to_string(HowitzerActiveWorldState_SchedulerEventCount(
               startupHowitzerState)) + "/" +
           std::to_string(startupHowitzerState.size()) + "/" +
           std::to_string(HowitzerActiveWorldState_Fingerprint(
               g_super.m_context)));
  if (!startupHowitzerStateReady)
    log.Line(std::string("howitzer_active_world_state_error=") +
             HowitzerActiveWorldState_LastFailure());
  log.Line("mission_active_world_probe=" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldMissionRecords()) + "/" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldMissionConditionReferences()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ActiveWorldMissionRouteReferences()) +
           "/" + std::to_string(
               RecoveredArenaSeance_ActiveWorldMissionCheckEvents()) + "/" +
           std::to_string(RecoveredArenaSeance_ActiveWorldRollbacks()));
  log.Line("continuation_state_probe=" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldClockRecords()) + "/" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldRngAlgorithm()) + "/" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldRngStateBytes()) + "/" +
           std::to_string(
               RecoveredArenaSeance_ActiveWorldRngDrawCount()) + "/" +
           std::to_string(RecoveredArenaSeance_ActiveWorldRollbacks()));
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
  const SRecoveredGameplayTuningSummary* tuningSummary =
      RecoveredGameplayTuning_Summary();
  log.Line(std::string("gameplay_tuning_active=") +
           (RecoveredGameplayTuning_IsActive() ? "1" : "0"));
  if (tuningSummary != nullptr) {
    log.Line("gameplay_tuning_schema=" +
             std::to_string(tuningSummary->schemaVersion));
    log.Line("gameplay_tuning_vehicle_patches=" +
             std::to_string(tuningSummary->vehiclePatchCount));
    log.Line("gameplay_tuning_projectile_patches=" +
             std::to_string(tuningSummary->projectilePatchCount));
    log.Line("gameplay_tuning_people_patches=" +
             std::to_string(tuningSummary->peoplePatchCount));
    log.Line("gameplay_tuning_tank_patches=" +
             std::to_string(tuningSummary->tankPatchCount));
    log.Line("gameplay_tuning_projectile_ballistic_proofs=" +
             std::to_string(tuningSummary->projectileBallisticProofs));
    log.Line("gameplay_tuning_projectile_ballistic_moves=" +
             std::to_string(tuningSummary->projectileBallisticMoves));
    log.Line("gameplay_tuning_secondary_reference_proofs=" +
             std::to_string(
                 tuningSummary->secondaryProjectileReferenceProofs));
    log.Line("gameplay_tuning_secondary_ballistic_proofs=" +
             std::to_string(
                 tuningSummary->secondaryProjectileBallisticProofs));
    log.Line("gameplay_tuning_secondary_ballistic_moves=" +
             std::to_string(
                 tuningSummary->secondaryProjectileBallisticMoves));
    log.Line("gameplay_tuning_people_lifecycle_proofs=" +
             std::to_string(tuningSummary->peopleLifecycleProofs));
    log.Line("gameplay_tuning_tank_lifecycle_proofs=" +
             std::to_string(tuningSummary->tankLifecycleProofs));
    log.Line("gameplay_tuning_people_projectile_reference_proofs=" +
             std::to_string(
                 tuningSummary->peopleProjectileReferenceProofs));
    log.Line("gameplay_tuning_people_outgoing_projectile_starts=" +
             std::to_string(
                 tuningSummary->peopleOutgoingProjectileStarts));
    log.Line("gameplay_tuning_tank_mass_consumer_proofs=" +
             std::to_string(tuningSummary->tankMassConsumerProofs));
    log.Line("gameplay_tuning_tank_projectile_reference_proofs=" +
             std::to_string(
                 tuningSummary->tankProjectileReferenceProofs));
    log.Line("gameplay_tuning_tank_outgoing_projectile_starts=" +
             std::to_string(
                 tuningSummary->tankOutgoingProjectileStarts));
    log.Line("gameplay_tuning_fingerprint=" +
             std::to_string(tuningSummary->tuningFingerprint));
    log.Line("gameplay_tuning_vehicle_attribute_fingerprint=" +
             std::to_string(
                 tuningSummary->vehicleAttributeFingerprint));
    log.Line("gameplay_tuning_vehicle_reference_fingerprint=" +
             std::to_string(
                 tuningSummary->vehicleReferenceFingerprint));
    log.Line("gameplay_tuning_bullet_attribute_fingerprint=" +
             std::to_string(
                 tuningSummary->bulletAttributeFingerprint));
    log.Line("gameplay_tuning_people_fingerprint=" +
             std::to_string(tuningSummary->peopleGameplayFingerprint));
    log.Line("gameplay_tuning_tank_fingerprint=" +
             std::to_string(tuningSummary->tankGameplayFingerprint));
    log.Line("gameplay_tuning_default_present=" +
             std::to_string(tuningSummary->defaultVehiclePresent));
    log.Line("gameplay_tuning_default_max_speed=" +
             std::to_string(tuningSummary->defaultMaxSpeed));
    log.Line("gameplay_tuning_default_reverse_speed=" +
             std::to_string(tuningSummary->defaultReverseSpeed));
    log.Line("gameplay_tuning_default_acceleration_time=" +
             std::to_string(tuningSummary->defaultAccelerationTime));
    log.Line("gameplay_tuning_default_turn_speed=" +
             std::to_string(tuningSummary->defaultTurnSpeed));
    log.Line("gameplay_tuning_default_primary_fire_interval=" +
             std::to_string(
                 tuningSummary->defaultPrimaryFireInterval));
    log.Line("gameplay_tuning_default_secondary_fire_interval=" +
             std::to_string(
                 tuningSummary->defaultSecondaryFireInterval));
    log.Line(std::string("gameplay_tuning_default_secondary_projectile=") +
             tuningSummary->defaultSecondaryProjectile);
    log.Line("gameplay_tuning_default_damage_power=" +
             std::to_string(tuningSummary->defaultDamagePower));
    log.Line("gameplay_tuning_primary_projectile_present=" +
             std::to_string(tuningSummary->primaryProjectilePresent));
    log.Line("gameplay_tuning_primary_projectile_speed=" +
             std::to_string(tuningSummary->primaryProjectileSpeed));
    log.Line(std::string("gameplay_tuning_observed_people=") +
             tuningSummary->observedPeople);
    log.Line("gameplay_tuning_people_movement_speed=" +
             std::to_string(
                 tuningSummary->observedPeopleMovementSpeed));
    log.Line("gameplay_tuning_people_initial_health=" +
             std::to_string(
                 tuningSummary->observedPeopleInitialHealth));
    log.Line("gameplay_tuning_people_fire_interval=" +
             std::to_string(
                 tuningSummary->observedPeopleFireInterval));
    log.Line("gameplay_tuning_people_burst_count=" +
             std::to_string(tuningSummary->observedPeopleBurstCount));
    log.Line(std::string("gameplay_tuning_people_projectile=") +
             tuningSummary->observedPeopleProjectile);
    log.Line(std::string("gameplay_tuning_observed_tank=") +
             tuningSummary->observedTank);
    log.Line("gameplay_tuning_tank_max_speed=" +
             std::to_string(tuningSummary->observedTankMaxSpeed));
    log.Line("gameplay_tuning_tank_attack_power=" +
             std::to_string(tuningSummary->observedTankAttackPower));
    log.Line("gameplay_tuning_tank_attack_delay=" +
             std::to_string(tuningSummary->observedTankAttackDelay));
    log.Line("gameplay_tuning_tank_mass=" +
             std::to_string(tuningSummary->observedTankMass));
    log.Line(std::string("gameplay_tuning_tank_projectile=") +
             tuningSummary->observedTankProjectile);
  }
  const SRecoveredScriptEventSummary* scriptEventSummary =
      RecoveredScriptEvents_Summary();
  log.Line(std::string("script_events_active=") +
           (RecoveredScriptEvents_IsActive() ? "1" : "0"));
  if (scriptEventSummary != nullptr) {
    log.Line("script_events_schema=" +
             std::to_string(scriptEventSummary->schemaVersion));
    log.Line("script_events_count=" +
             std::to_string(scriptEventSummary->eventCount));
    log.Line("script_events_explosions=" +
             std::to_string(scriptEventSummary->explosionCount));
    log.Line("script_events_sparks=" +
             std::to_string(scriptEventSummary->sparkCount));
    log.Line("script_events_queued=" +
             std::to_string(scriptEventSummary->queuedCount));
    log.Line("script_events_evt1_proofs=" +
             std::to_string(scriptEventSummary->semanticProofCount));
    log.Line("script_events_fingerprint=" +
             std::to_string(scriptEventSummary->eventFingerprint));
    log.Line("script_events_minimum_delay=" +
             std::to_string(scriptEventSummary->minimumDelay));
    log.Line("script_events_maximum_delay=" +
             std::to_string(scriptEventSummary->maximumDelay));
  }
  log.Line("arena_seance_issues=" +
           std::to_string(RecoveredArenaSeance_Issues()));
  log.Line("arena_seance_extended_issues=" +
           std::to_string(RecoveredArenaSeance_ExtendedIssues()));
  if (RecoveredArenaSeance_LastError()[0] != 0) {
    log.Line(std::string("arena_seance_error=") +
             RecoveredArenaSeance_LastError());
  }
  ZAV_BeginLoop();
  RecoveredGameServices_RefreshBriefingViewport();
  log.Line("loop_initialized=" +
           std::to_string(RecoveredGameServices_LoopReady() ? 1 : 0));
  int currentLevelIndex = data.startLevel;
  bool loopFailed = !RecoveredGameServices_IsReady();
  if (!loopFailed && options.startupSaveSlot >= 0 &&
      !RecoveredGameServices_RequestSaveSlot(
          static_cast<std::uint32_t>(options.startupSaveSlot), false)) {
    loopFailed = true;
  }
  if (!loopFailed && options.startupLoadSlot >= 0 &&
      !RecoveredGameServices_RequestLoadSlot(
          static_cast<std::uint32_t>(options.startupLoadSlot))) {
    loopFailed = true;
  }
  const auto runCompleteFrame = [&]() {
    if (!RecoveredGameServices_RunFrame()) return false;
    if (RecoveredGameServices_CampaignRestartPending() &&
        !ProcessCampaignRestart(data, &currentLevelIndex,
                                options.runtimeSmoke, &log))
      return false;
    if (RecoveredGameServices_CrossLevelLoadPending() &&
        !ProcessCrossLevelLoad(data, &currentLevelIndex,
                               options.runtimeSmoke, &log))
      return false;
    return !RecoveredGameServices_DebugLevelSwitchPending() ||
           ProcessDebugLevelSwitch(data, &currentLevelIndex,
                                   options.runtimeSmoke, &log);
  };
  // Startup save/load is itself a closed-frame operation.  Complete it before
  // staging the synthetic map mission: otherwise a save captures the probe,
  // while a cross-Level load carries source-Level probe baselines into the
  // restored DebugMap and can reject an otherwise successful load.
  if (!loopFailed && options.runtimeSmoke &&
      (options.startupSaveSlot >= 0 || options.startupLoadSlot >= 0))
    loopFailed = !runCompleteFrame();
  if (!loopFailed && options.runtimeSmoke &&
      options.startupSaveSlot < 0 && options.startupLoadSlot < 0) {
    loopFailed = !RecoveredGameServices_StageMissionMapProbe() ||
                 !runCompleteFrame() ||
                 !RecoveredGameServices_RequestDebugMapToggle() ||
                 !runCompleteFrame() ||
                 !RecoveredGameServices_VerifyMissionMapProbe() ||
                 !RecoveredGameServices_RequestDebugMapToggle() ||
                 !RecoveredGameServices_ClearMissionMapProbe();
  }
  if (!loopFailed && options.missionSmoke) {
    bool missionStaged = false;
    RecruitCenterMissionProbeSummary mission = {};
    const double missionTime =
        (std::max)(0.1, Session::m_viewTime + 0.25);
    const std::string missionCenter = WideToUtf8(options.missionCenter);
    log.Line("mission_smoke_center=" +
             (missionCenter.empty() ? std::string("<first-eligible>")
                                    : missionCenter));
    const bool missionExecuted = options.missionBriefingSmoke
        ? (missionCenter.empty()
               ? RecruitCenterSubjectState_StageMissionPresentationProbe(
                     g_super.m_context, missionTime, &missionStaged, &mission)
               : RecruitCenterSubjectState_StageMissionPresentationProbeForCenter(
                     g_super.m_context, missionTime, missionCenter.c_str(),
                     &missionStaged, &mission))
        : (missionCenter.empty()
               ? RecruitCenterSubjectState_StageMissionExecutionProbe(
                     g_super.m_context, missionTime, &missionStaged, &mission)
               : RecruitCenterSubjectState_StageMissionExecutionProbeForCenter(
                     g_super.m_context, missionTime, missionCenter.c_str(),
                     &missionStaged, &mission));
    log.Line(std::string("mission_smoke_staged=") +
             (missionStaged ? "1" : "0"));
    log.Line(std::string("mission_smoke_selected_center=") +
             mission.centerName);
    log.Line(std::string("mission_smoke_selected_project=") +
             mission.projectName);
    log.Line("mission_smoke_scripts=" +
             std::to_string(mission.executedScripts));
    log.Line("mission_smoke_created_objects=" +
             std::to_string(mission.createdMissionObjects));
    log.Line("mission_smoke_conditions=" +
             std::to_string(mission.conditionReferences));
    log.Line("mission_smoke_rebound_conditions=" +
             std::to_string(mission.reboundConditionReferences));
    log.Line("mission_smoke_briefings=" +
             std::to_string(mission.presentedBriefings));
    log.Line("mission_smoke_briefing_commands=" +
             std::to_string(mission.briefingCommands));
    log.Line("mission_smoke_script_commands=" +
             std::to_string(mission.scriptCommands));
    log.Line("mission_smoke_rollbacks=" +
             std::to_string(mission.scriptRollbacks));
    log.Line("mission_smoke_deferred_artefact_rewards=" +
             std::to_string(mission.deferredArtefactRewards));
    if (!missionExecuted) {
      log.Line(std::string("mission_smoke_error=") +
               RecruitCenterSubjectState_LastError());
    }
    const int expectedBriefings =
        options.missionBriefingSmoke ? mission.briefingCommands : 0;
    loopFailed = !missionExecuted || !missionStaged ||
                 mission.executedScripts < 1 ||
                 mission.createdMissionObjects < 1 ||
                 mission.presentedBriefings != expectedBriefings ||
                 mission.scriptRollbacks != 0 || !runCompleteFrame();
    log.Line("mission_smoke_howitzers=" + std::to_string(
                 RecoveredArenaSeance_HowitzerLiveCount()) + "/" +
             std::to_string(RecoveredArenaSeance_HowitzerReadyLiveCount()) +
             "/" + std::to_string(
                 RecoveredArenaSeance_HowitzerOccupiedHolderCount()));
    std::vector<unsigned char> howitzerState;
    const bool howitzerStateReady =
        HowitzerActiveWorldState_CaptureStable(
            g_super.m_context, &howitzerState);
    log.Line("mission_smoke_howitzer_state=" +
             std::to_string(howitzerStateReady ? 1 : 0) + "/" +
             std::to_string(
                 HowitzerActiveWorldState_SchedulerEventCount(
                     howitzerState)) + "/" +
             std::to_string(howitzerState.size()) + "/" +
             std::to_string(
                 HowitzerActiveWorldState_Fingerprint(
                     g_super.m_context)));
    if (!howitzerStateReady)
      log.Line(std::string("mission_smoke_howitzer_state_error=") +
               HowitzerActiveWorldState_LastFailure());
    SPeopleRouteMotionProbeSummary guide = {};
    const bool guideReady = PeopleSubjectState_ProbeNewestDelayedRoute(
        g_super.m_context, &guide);
    log.Line(std::string("mission_smoke_guide=") +
             (guide.owner[0] == 0 ? "<none>" : guide.owner) + "/" +
             std::to_string(guide.available) + "/" +
             std::to_string(guide.phaseExact) + "/" +
             std::to_string(guide.groundedRouteEvent) + "/" +
             std::to_string(guide.finiteMotion) + "/" +
             std::to_string(guide.movedTowardTarget) + "/" +
             std::to_string(guide.boundedStep) + "/" +
             std::to_string(guide.startNode) + "/" +
             std::to_string(guide.targetNode) + "/" +
             std::to_string(guide.backSpaceNode) + "/" +
             std::to_string(guide.startMoveDelay) + "/" +
             std::to_string(guide.elapsed) + "/" +
             std::to_string(guide.displacement));
    loopFailed = loopFailed || !guideReady;
    if (!loopFailed && options.missionContinuationSmoke) {
      std::vector<std::uint8_t> continuation;
      std::vector<std::uint8_t> recaptured;
      SLevelContinuationSummary captured;
      SLevelContinuationSummary restored;
      SLevelContinuationSummary verified;
      const bool captureReady =
          RecoveredGameServices_CaptureLevelContinuation(
              &continuation, &captured);
      const bool restoreReady = captureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              continuation, &restored);
      const bool recaptureReady = restoreReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &recaptured, &verified);
      const bool successfulExact =
          recaptureReady && continuation == recaptured &&
          captured.ready && restored.ready && verified.ready &&
          captured.sections == 15 && restored.sections == 15 &&
          restored.ownerPhases == 15 &&
          restored.referencePhases == 15 &&
          captured.worldFingerprint == restored.restoredWorldFingerprint &&
          captured.worldFingerprint == verified.worldFingerprint;
      SLevelContinuationSummary rejected;
      std::vector<std::uint8_t> rolledBack;
      SLevelContinuationSummary rollbackVerified;
      if (successfulExact)
        RecoveredGameServices_FailNextRestoredGameplayAuthorityForTesting();
      const bool restoreRejected = successfulExact &&
          !RecoveredGameServices_RestoreLevelContinuation(
              continuation, &rejected);
      const std::string rejection =
          RecoveredGameServices_LastLevelContinuationError();
      const bool rollbackRecaptured = restoreRejected &&
          RecoveredGameServices_CaptureLevelContinuation(
              &rolledBack, &rollbackVerified);
      const bool rollbackExact = rollbackRecaptured &&
          rolledBack == continuation && rollbackVerified.ready &&
          rollbackVerified.sections == 15 &&
          rollbackVerified.worldFingerprint == captured.worldFingerprint;
      const bool exact = successfulExact && rollbackExact;
      log.Line("mission_continuation_capture=" +
               std::to_string(captureReady ? 1 : 0) + "/" +
               std::to_string(captured.sections) + "/" +
               std::to_string(continuation.size()));
      log.Line("mission_continuation_restore=" +
               std::to_string(restoreReady ? 1 : 0) + "/" +
               std::to_string(restored.ownerPhases) + "/" +
               std::to_string(restored.referencePhases));
      log.Line("mission_continuation_exact=" +
               std::to_string(successfulExact ? 1 : 0));
      log.Line("mission_continuation_rollback=" +
               std::to_string(restoreRejected ? 1 : 0) + "/" +
               std::to_string(rollbackRecaptured ? 1 : 0) + "/" +
               std::to_string(rollbackExact ? 1 : 0));
      if (!rollbackExact)
        log.Line("mission_continuation_rollback_error=" + rejection);
      if (!exact)
        log.Line(std::string("mission_continuation_error=") +
                 RecoveredGameServices_LastLevelContinuationError());
      loopFailed = !exact;
    }
  }
  while (!loopFailed && !options.runtimeSmoke &&
         !RecoveredGameServices_QuitRequested()) {
    if (!runCompleteFrame()) {
      loopFailed = !RecoveredGameServices_QuitRequested();
      break;
    }
    Sleep(1);
  }
  if (loopFailed) {
    log.Line("game_services_issues=" +
             std::to_string(RecoveredGameServices_Issues()));
    log.Line("marker=loop-not-ready");
    const SRecoveredSaveMenuState* failedSaveState =
        RecoveredGameServices_SaveMenuState();
    if (failedSaveState != nullptr &&
        !failedSaveState->lastError.empty()) {
      log.Line("save_menu_error=" + failedSaveState->lastError);
    }
    const SRecoveredCampaignRestartState* failedRestartState =
        RecoveredGameServices_CampaignRestartState();
    if (failedRestartState != nullptr &&
        !failedRestartState->lastError.empty()) {
      log.Line("campaign_restart_error=" +
               failedRestartState->lastError);
    }
    ZAV_DeInitLevel();
    ZAV_Deinit();
    ShowMessage(options.runtimeSmoke, MB_ICONERROR,
                L"RR2NW runtime error",
                L"The recovered services could not complete the software "
                L"loop.\n\nDiagnostic log:\n" + log.path());
    return kRuntimeNotReady;
  }

  summary = RecoveredDrawableScene_Summary();
  scriptManifest = RecoveredRetailScriptManifest_Summary();
  if (summary == nullptr || scriptManifest == nullptr) {
    log.Line("failure=final Level lost its scene/script summary");
    log.Line("marker=loop-not-ready");
    ZAV_DeInitLevel();
    ZAV_Deinit();
    return kRuntimeNotReady;
  }
  log.Line("recovered_runtime=connected");
  log.Line("final_level=" + std::to_string(currentLevelIndex));
  log.WideLine(
      "final_level_dir",
      data.levels[static_cast<std::size_t>(currentLevelIndex)]);
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
  log.Line("active_content_fingerprint=" + std::to_string(
               RecoveredModRuntime_CombineContentFingerprint(
                   scriptManifest->contentFingerprint)));
  modSummary = RecoveredModRuntime_Summary();
  if (modSummary != nullptr) {
    log.Line("mod_resolve_count=" +
             std::to_string(modSummary->resolveCount));
    log.Line("mod_override_hits=" +
             std::to_string(modSummary->overrideHitCount));
  }
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
  log.Line("input_mode=authoritative-windows-semantic-adapter");
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
  log.Line("vehicle_physical_reconciliation_count=" + std::to_string(
               RecoveredGameServices_VehiclePhysicalReconciliationCount()));
  SRecoveredWindowsInputTelemetry windowsInput = {};
  if (RecoveredGameServices_WindowsInputTelemetry(&windowsInput)) {
    log.Line("windows_input_keyboard_messages=" +
             std::to_string(windowsInput.keyboardMessages));
    log.Line("windows_input_mouse_button_messages=" +
             std::to_string(windowsInput.mouseButtonMessages));
    log.Line("windows_input_focus_messages=" +
             std::to_string(windowsInput.focusMessages));
    log.Line("windows_input_emitted_actions=" +
             std::to_string(windowsInput.emittedActions));
    log.Line("windows_input_filtered_repeats=" +
             std::to_string(windowsInput.filteredRepeats));
    log.Line("windows_input_redundant_releases=" +
             std::to_string(windowsInput.redundantReleases));
    log.Line("windows_input_suppressed_messages=" +
             std::to_string(windowsInput.suppressedMessages));
    log.Line("windows_input_focus_clear_actions=" +
             std::to_string(windowsInput.focusClearActions));
  }
  log.Line("windows_input_map_toggle_presses=" + std::to_string(
               RecoveredGameServices_MapTogglePresses()));
  log.Line("debug_map_initialized=" + std::to_string(
               RecoveredGameServices_DebugMapReady() ? 1 : 0));
  log.Line("debug_map_active=" + std::to_string(
               RecoveredGameServices_DebugMapActive() ? 1 : 0));
  log.Line("debug_map_size=" + std::to_string(
               RecoveredGameServices_DebugMapWidth()) + "/" +
           std::to_string(RecoveredGameServices_DebugMapHeight()));
  log.Line("debug_map_toggle_probe=" + std::to_string(
               RecoveredGameServices_DebugMapOpenTransitions()) + "/" +
           std::to_string(
               RecoveredGameServices_DebugMapCloseTransitions()) + "/" +
           std::to_string(RecoveredGameServices_DebugMapDrawFrames()));
  SRecoveredMissionMapProbeTelemetry missionMapProbe = {};
  if (RecoveredGameServices_MissionMapProbeTelemetry(&missionMapProbe)) {
    log.Line("mission_map_probe=" +
             std::to_string(missionMapProbe.staged) + "/" +
             std::to_string(missionMapProbe.summaryPublished) + "/" +
             std::to_string(missionMapProbe.missionCount) + "/" +
             std::to_string(missionMapProbe.textCount) + "/" +
             std::to_string(missionMapProbe.routeCount) + "/" +
             std::to_string(missionMapProbe.renderedFrames) + "/" +
             std::to_string(missionMapProbe.rollbacks) + "/" +
             std::to_string(missionMapProbe.framebufferHash) + "/" +
              std::to_string(missionMapProbe.framebufferNonClearPixels));
  }
  log.Line("recruit_center_admission_final=" + std::to_string(
               RecoveredArenaSeance_RecruitCenterRejectedCollisions()) + "/" +
           std::to_string(
               RecoveredArenaSeance_RecruitCenterPlayerCollisions()) + "/" +
           std::to_string(RecoveredArenaSeance_RecruitCenterAdmissions()) +
           "/" + std::to_string(
               RecoveredArenaSeance_RecruitCenterStagedMissions()) + "/" +
           std::to_string(
               RecoveredArenaSeance_RecruitCenterExistingMissionVisits()) +
           "/" + std::to_string(
               RecoveredArenaSeance_RecruitCenterNoProjectVisits()) + "/" +
           std::to_string(RecoveredArenaSeance_RecruitCenterEjections()) +
           "/" + std::to_string(
               RecoveredArenaSeance_RecruitCenterAdmissionFailures()));
  RecruitCenterMissionProbeSummary lastMission = {};
  if (RecruitCenterSubjectState_LastMissionSummary(&lastMission)) {
    log.Line(std::string("recruit_center_last_mission=") +
             lastMission.centerName + "/" + lastMission.projectName + "/" +
             std::to_string(lastMission.executedScripts) + "/" +
             std::to_string(lastMission.createdMissionObjects) + "/" +
             std::to_string(lastMission.deferredCommands) + "/" +
             std::to_string(lastMission.briefingCommands) + "/" +
             std::to_string(lastMission.scriptCommands) + "/" +
             std::to_string(lastMission.presentedBriefings) + "/" +
             std::to_string(lastMission.scriptRollbacks));
  }
  log.Line("windows_input_primary_fire_presses=" + std::to_string(
               RecoveredGameServices_VehiclePrimaryFirePresses()));
  log.Line("windows_input_secondary_fire_presses=" + std::to_string(
               RecoveredGameServices_VehicleSecondaryFirePresses()));
  log.Line("windows_input_secondary_fire_accepted_shots=" +
           std::to_string(
               RecoveredGameServices_VehicleSecondaryFireAcceptedShots()));
  log.Line("windows_input_jump_presses=" + std::to_string(
               RecoveredGameServices_VehicleJumpPresses()));
  SRecoveredVehiclePrimaryFireTelemetry finalPrimaryFire = {};
  if (RecoveredGameServices_VehiclePrimaryFireTelemetry(
          &finalPrimaryFire)) {
    log.Line("windows_input_primary_fire_accepted_shots=" +
             std::to_string(finalPrimaryFire.acceptedShots));
    log.Line("windows_input_primary_fire_move_events=" +
             std::to_string(finalPrimaryFire.moveEvents));
    log.Line("windows_input_primary_fire_collision_checks=" +
             std::to_string(finalPrimaryFire.collisionChecks));
  }
  log.Line("windows_input_pending_events=" + std::to_string(
               RecoveredGameServices_WindowsInputPendingEvents()));
  SRecoveredObserverAxes vehicleControlAxes = {};
  if (RecoveredGameServices_VehicleControlAxes(&vehicleControlAxes)) {
    log.Line("vehicle_control_axes=" +
             std::to_string(vehicleControlAxes.forward) + "," +
             std::to_string(vehicleControlAxes.strafe) + "," +
             std::to_string(vehicleControlAxes.vertical) + "," +
             std::to_string(vehicleControlAxes.turn) + "," +
             std::to_string(vehicleControlAxes.look));
  }
  SRecoveredVehicleControlJournalTelemetry journalTelemetry = {};
  if (RecoveredGameServices_VehicleControlJournalTelemetry(
          &journalTelemetry)) {
    log.Line("vehicle_control_journal_recording=" +
             std::to_string(journalTelemetry.recording));
    log.Line("vehicle_control_journal_records=" +
             std::to_string(journalTelemetry.recordCount));
    log.Line("vehicle_control_journal_action_records=" +
             std::to_string(journalTelemetry.actionRecords));
    log.Line("vehicle_control_journal_focus_records=" +
             std::to_string(journalTelemetry.focusRecords));
    log.Line("vehicle_control_journal_checkpoint_tick=" +
             std::to_string(journalTelemetry.checkpointTick));
    log.Line("vehicle_control_journal_last_tick=" +
             std::to_string(journalTelemetry.lastRecordTick));
    log.Line("vehicle_control_journal_encoded_bytes=" +
             std::to_string(journalTelemetry.encodedBytes));
    log.Line("vehicle_control_journal_fingerprint=" +
             std::to_string(journalTelemetry.journalFingerprint));
    log.Line("vehicle_control_journal_append_failures=" +
             std::to_string(journalTelemetry.appendFailures));
  }
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
  log.Line("vehicle_camera_mode=" + std::to_string(
               RecoveredGameServices_VehicleCameraMode()));
  log.Line("vehicle_camera_transform_frames=" + std::to_string(
               RecoveredGameServices_VehicleCameraTransformFrameCount()));
  log.Line("vehicle_death_camera_frames=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraFrameCount()));
  log.Line("vehicle_death_camera_completions=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraCompletions()));
  log.Line("vehicle_death_camera_offset_y=" + std::to_string(
               RecoveredGameServices_VehicleDeathCameraOffsetY()));
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
    log.Line("vehicle_stability_recoveries=" + std::to_string(
                  vehicleTelemetry.stabilityRecoveries));
    log.Line("vehicle_last_stability_reason=" + std::to_string(
                  vehicleTelemetry.lastStabilityReason));
    if (vehicleTelemetry.stabilityRecoveries != 0) {
      log.Line("vehicle_recovery_identity=" + std::to_string(
                   vehicleTelemetry.recoveryVesselKind) + "," +
               std::to_string(vehicleTelemetry.recoveryBumpFlags) + "," +
               std::to_string(
                   vehicleTelemetry.recoveryTouchingGround));
      log.Line("vehicle_recovery_times=" + std::to_string(
                   vehicleTelemetry.recoveryFrameStartTime) + "," +
               std::to_string(vehicleTelemetry.recoveryRejectedTime) + "," +
               std::to_string(vehicleTelemetry.recoveryTargetTime));
      log.Line("vehicle_recovery_start_position=" + std::to_string(
                   vehicleTelemetry.recoveryFrameStartPositionX) + "," +
               std::to_string(
                   vehicleTelemetry.recoveryFrameStartPositionY) + "," +
               std::to_string(
                   vehicleTelemetry.recoveryFrameStartPositionZ));
      log.Line("vehicle_recovery_start_speed=" + std::to_string(
                   vehicleTelemetry.recoveryFrameStartSpeedX) + "," +
               std::to_string(vehicleTelemetry.recoveryFrameStartSpeedY) +
               "," + std::to_string(
                   vehicleTelemetry.recoveryFrameStartSpeedZ));
      log.Line("vehicle_recovery_rejected_position=" + std::to_string(
                   vehicleTelemetry.recoveryRejectedPositionX) + "," +
               std::to_string(
                   vehicleTelemetry.recoveryRejectedPositionY) + "," +
               std::to_string(
                   vehicleTelemetry.recoveryRejectedPositionZ));
      log.Line("vehicle_recovery_rejected_speed=" + std::to_string(
                   vehicleTelemetry.recoveryRejectedSpeedX) + "," +
               std::to_string(vehicleTelemetry.recoveryRejectedSpeedY) +
               "," + std::to_string(
                   vehicleTelemetry.recoveryRejectedSpeedZ));
      log.Line("vehicle_recovery_ground=" + std::to_string(
                   vehicleTelemetry.recoveryGroundX) + "," +
               std::to_string(vehicleTelemetry.recoveryGroundY) + "," +
               std::to_string(vehicleTelemetry.recoveryGroundZ) + "," +
               std::to_string(vehicleTelemetry.recoveryGroundLength));
      log.Line("vehicle_recovery_tangents=" + std::to_string(
                   vehicleTelemetry.recoveryForwardTangentLength) + "," +
               std::to_string(
                   vehicleTelemetry.recoveryRightTangentLength) + "," +
               std::to_string(vehicleTelemetry.recoveryTangentDot));
      log.Line("vehicle_recovery_controls=" + std::to_string(
                   vehicleTelemetry.recoverySuspensionTravel) + "," +
               std::to_string(
                   vehicleTelemetry.recoveryAccelerationFactor) + "," +
               std::to_string(vehicleTelemetry.recoveryThrottle));
    }
  }
  SRecoveredVehicleAuthorityState vehicleAuthority = {};
  if (RecoveredGameServices_VehicleAuthorityState(&vehicleAuthority)) {
    log.Line("vehicle_authority_identity_fingerprint=" + std::to_string(
                 vehicleAuthority.identityFingerprint));
    log.Line("vehicle_authority_damage=" +
             std::to_string(vehicleAuthority.damage));
    log.Line("vehicle_authority_vessel_kind=" +
             std::to_string(vehicleAuthority.vesselKind));
    log.Line("vehicle_authority_vessel_profile=" +
             std::to_string(vehicleAuthority.vesselProfile));
    log.Line("vehicle_authority_active=" +
             std::to_string(vehicleAuthority.active));
    log.Line("vehicle_authority_frame_begun=" +
             std::to_string(vehicleAuthority.frameBegun));
    log.Line("vehicle_authority_dead=" +
             std::to_string(vehicleAuthority.dead));
    log.Line("vehicle_authority_taking_taxi=" +
             std::to_string(vehicleAuthority.takingTaxi));
    log.Line("vehicle_authority_panel_ready=" +
             std::to_string(vehicleAuthority.panelReady));
    log.Line("vehicle_authority_panel_open=" +
             std::to_string(vehicleAuthority.panelOpen));
    log.Line("vehicle_authority_taxi_change_enabled=" +
             std::to_string(vehicleAuthority.taxiChangeEnabled));
  }
  log.Line(
      "script_mode=bounded-retail-farter-subject-sound-object-farter-corpse-"
      "reference-wav-smoker-dyn-smoker-emission-light-corona-smoke-terrain-"
       "simulation-visual-lamp-skin-resource-smoke-explosion-attribute-taxi-"
       "attribute-bullet-collision-impact-explosion-damage-sound-particles-"
       "vehicle-bootstrap-taxi-subject-vehicle-transition");
  log.Line("vehicle_object=Vehicle.Default");
  log.Line(
      "vehicle_controls=W,S,A,D,T,G,Space-jump,arrows,MouseL-primary,"
      "MouseR-secondary,X-stop,F1-change,M-map,Escape");
  log.Line("observer_mode=fallback-suspended");
  log.Line("service_hooks=12");
  log.Line("service_frames=" + std::to_string(dwFrames));
  SRecoveredFrameTimingTelemetry frameTiming = {};
  if (RecoveredGameServices_FrameTimingTelemetry(&frameTiming)) {
    log.Line("frame_profile_samples=" +
             std::to_string(frameTiming.frames));
    log.Line("frame_profile_total_us=" +
             std::to_string(frameTiming.totalMicroseconds));
    log.Line("frame_profile_total_max_us=" +
             std::to_string(frameTiming.maximumFrameMicroseconds));
    log.Line("frame_profile_input_us=" +
             std::to_string(frameTiming.inputMicroseconds));
    log.Line("frame_profile_input_max_us=" +
             std::to_string(frameTiming.maximumInputMicroseconds));
    log.Line("frame_profile_simulation_us=" +
             std::to_string(frameTiming.simulationMicroseconds));
    log.Line("frame_profile_simulation_max_us=" +
             std::to_string(frameTiming.maximumSimulationMicroseconds));
    log.Line("frame_profile_render_us=" +
             std::to_string(frameTiming.renderMicroseconds));
    log.Line("frame_profile_render_max_us=" +
             std::to_string(frameTiming.maximumRenderMicroseconds));
    log.Line("frame_profile_present_us=" +
             std::to_string(frameTiming.presentMicroseconds));
    log.Line("frame_profile_present_max_us=" +
             std::to_string(frameTiming.maximumPresentMicroseconds));
    log.Line("frame_profile_boundary_us=" +
             std::to_string(frameTiming.boundaryMicroseconds));
    log.Line("frame_profile_boundary_max_us=" +
             std::to_string(frameTiming.maximumBoundaryMicroseconds));
  }
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
  saveMenuState = RecoveredGameServices_SaveMenuState();
  if (saveMenuState != nullptr) {
    log.Line("save_menu_save_requests=" +
             std::to_string(saveMenuState->saveRequests));
    log.Line("save_menu_load_requests=" +
             std::to_string(saveMenuState->loadRequests));
    log.Line("save_menu_completed_saves=" +
             std::to_string(saveMenuState->completedSaves));
    log.Line("save_menu_completed_loads=" +
             std::to_string(saveMenuState->completedLoads));
    log.Line("save_menu_failed_commands=" +
             std::to_string(saveMenuState->failedCommands));
    log.Line("save_menu_deferred_commands=" +
             std::to_string(saveMenuState->deferredCommands));
    log.Line("save_menu_last_command_attempts=" +
             std::to_string(saveMenuState->lastCommandAttempts));
    log.Line("save_menu_slot_detail_views=" +
             std::to_string(saveMenuState->slotDetailViews));
    log.Line("save_menu_preview_views=" +
             std::to_string(saveMenuState->previewViews));
    log.Line("save_menu_preview_decode_failures=" +
             std::to_string(saveMenuState->previewDecodeFailures));
    log.Line("save_menu_custom_metadata_requests=" +
             std::to_string(saveMenuState->customMetadataSaveRequests));
    log.Line("save_menu_last_title=" + saveMenuState->lastSlot.title);
    log.Line("save_menu_last_requested_title=" +
             saveMenuState->lastRequestedTitle);
    log.Line("save_menu_last_slot_world_fingerprint=" +
             std::to_string(saveMenuState->lastSlot.worldFingerprint));
    log.Line("save_menu_last_slot_continuation_fingerprint=" +
             std::to_string(
                 saveMenuState->lastSlot.continuationFingerprint));
    log.Line("save_menu_last_restore_world_fingerprint=" +
             std::to_string(
                 saveMenuState->lastContinuation.worldFingerprint));
    log.Line("save_menu_last_restored_world_fingerprint=" +
             std::to_string(
                 saveMenuState->lastContinuation
                     .restoredWorldFingerprint));
    log.Line("save_menu_last_restore_container_fingerprint=" +
             std::to_string(
                 saveMenuState->lastContinuation.containerFingerprint));
    log.Line("save_menu_cross_level_requests=" +
             std::to_string(saveMenuState->crossLevelRequests));
    log.Line("save_menu_completed_cross_level_loads=" +
             std::to_string(saveMenuState->completedCrossLevelLoads));
    log.Line("save_menu_cross_level_rollbacks=" +
             std::to_string(saveMenuState->crossLevelRollbacks));
    log.Line("save_menu_cross_level_rollback_failures=" +
             std::to_string(saveMenuState->crossLevelRollbackFailures));
    log.Line("save_menu_cross_level_source=" +
             saveMenuState->crossLevelSourceLevel);
    log.Line("save_menu_cross_level_target=" +
             saveMenuState->crossLevelTargetLevel);
    log.Line("save_menu_last_error=" + saveMenuState->lastError);
  }
  const SRecoveredCampaignRestartState* campaignRestartState =
      RecoveredGameServices_CampaignRestartState();
  if (campaignRestartState != nullptr) {
    log.Line("campaign_restart_requests=" +
             std::to_string(campaignRestartState->requests));
    log.Line("campaign_restart_completed=" +
             std::to_string(
                 campaignRestartState->completedRestarts));
    log.Line("campaign_restart_failures=" +
             std::to_string(campaignRestartState->failedRestarts));
    log.Line("campaign_restart_dead_sources=" +
             std::to_string(
                 campaignRestartState->deadSourceRestarts));
    log.Line("campaign_restart_deferred=" +
             std::to_string(
                 campaignRestartState->deferredCommands));
    log.Line("campaign_restart_last_attempts=" +
             std::to_string(
                 campaignRestartState->lastCommandAttempts));
    log.Line("campaign_restart_rollbacks=" +
             std::to_string(campaignRestartState->rollbacks));
    log.Line("campaign_restart_rollback_failures=" +
             std::to_string(
                 campaignRestartState->rollbackFailures));
    log.Line("campaign_restart_pending=" +
             std::to_string(campaignRestartState->pending ? 1 : 0));
    log.Line("campaign_restart_coordinator_pending=" +
             std::to_string(
                 campaignRestartState->coordinatorPending ? 1 : 0));
    log.Line("campaign_restart_level=" +
             campaignRestartState->currentLevel);
  }
  debugMenuState = RecoveredGameServices_DebugMenuState();
  if (debugMenuState != nullptr) {
    log.Line("debug_menu_catalog_builds=" +
             std::to_string(debugMenuState->catalogBuilds));
    log.Line("debug_menu_catalog_failures=" +
             std::to_string(debugMenuState->catalogFailures));
    log.Line("debug_menu_requests=" +
             std::to_string(debugMenuState->requests));
    log.Line("debug_menu_completed_commands=" +
             std::to_string(debugMenuState->completedCommands));
    log.Line("debug_menu_failed_commands=" +
             std::to_string(debugMenuState->failedCommands));
    log.Line("debug_menu_deferred_commands=" +
             std::to_string(debugMenuState->deferredCommands));
    log.Line("debug_menu_last_command_attempts=" +
             std::to_string(debugMenuState->lastCommandAttempts));
    log.Line("debug_menu_rollback_attempts=" +
             std::to_string(debugMenuState->rollbackAttempts));
    log.Line("debug_menu_rollback_completions=" +
             std::to_string(debugMenuState->rollbackCompletions));
    log.Line("debug_menu_spawned_vehicles=" +
             std::to_string(debugMenuState->spawnedVehicles));
    log.Line("debug_menu_grounded_vehicle_spawns=" +
             std::to_string(debugMenuState->groundedVehicleSpawns));
    log.Line("debug_menu_sweep_grounded_vehicle_spawns=" +
             std::to_string(
                 debugMenuState->sweepGroundedVehicleSpawns));
    log.Line("debug_menu_terrain_fallback_vehicle_spawns=" +
             std::to_string(
                 debugMenuState->terrainFallbackVehicleSpawns));
    log.Line("debug_menu_spawn_placement_failures=" +
             std::to_string(debugMenuState->spawnPlacementFailures));
    log.Line("debug_menu_spawn_settlement_proofs=" +
             std::to_string(debugMenuState->spawnSettlementProofs));
    log.Line("debug_menu_spawn_settlement_failures=" +
             std::to_string(debugMenuState->spawnSettlementFailures));
    log.Line("debug_menu_last_spawn_settlement_frames=" +
             std::to_string(debugMenuState->lastSpawnSettlementFrames));
    log.Line("debug_menu_max_spawn_settlement_drift=" +
             std::to_string(debugMenuState->maxSpawnSettlementDrift));
    log.Line("debug_menu_last_spawn_bump_kind=" +
             std::to_string(debugMenuState->lastSpawnBumpKind));
    log.Line("debug_menu_last_spawn_sweep_time=" +
             std::to_string(debugMenuState->lastSpawnSweepTime));
    log.Line("debug_menu_last_spawn_drop_distance=" +
             std::to_string(debugMenuState->lastSpawnDropDistance));
    log.Line("debug_menu_last_spawn_origin_clearance=" +
             std::to_string(debugMenuState->lastSpawnOriginClearance));
    log.Line("debug_menu_last_spawn_model_bottom_clearance=" +
             std::to_string(
                 debugMenuState->lastSpawnModelBottomClearance));
    log.Line("debug_menu_last_spawn_height=" +
             std::to_string(debugMenuState->lastSpawnRequestedY) + "/" +
             std::to_string(debugMenuState->lastSpawnSurfaceY) + "/" +
             std::to_string(debugMenuState->lastSpawnResolvedY));
    log.Line("debug_menu_entered_vehicles=" +
             std::to_string(debugMenuState->enteredVehicles));
    log.Line("debug_menu_stabilized_vehicles=" +
             std::to_string(debugMenuState->stabilizedVehicles));
    log.Line("debug_menu_forced_deaths=" +
             std::to_string(debugMenuState->forcedDeaths));
    log.Line("debug_menu_death_corpse_creations=" +
             std::to_string(debugMenuState->deathCorpseCreations));
    log.Line("debug_menu_death_camera_proofs=" +
             std::to_string(debugMenuState->deathCameraProofs));
    log.Line("debug_menu_death_save_proofs=" +
             std::to_string(debugMenuState->deathSaveProofs));
    log.Line("debug_menu_restored_pre_death_checkpoints=" +
             std::to_string(
                 debugMenuState->restoredPreDeathCheckpoints));
    log.Line("debug_menu_pre_death_checkpoint_available=" +
             std::to_string(
                 debugMenuState->preDeathCheckpointAvailable ? 1 : 0));
    log.Line("debug_menu_death_world_fingerprint=" +
             std::to_string(debugMenuState->deathWorldFingerprint));
    log.Line("debug_menu_death_continuation_fingerprint=" +
             std::to_string(
                 debugMenuState->deathContinuationFingerprint));
    log.Line("debug_menu_forced_vehicle_destructions=" +
             std::to_string(
                 debugMenuState->forcedVehicleDestructions));
    log.Line("debug_menu_destruction_orphan_creations=" +
             std::to_string(
                 debugMenuState->destructionOrphanCreations));
    log.Line("debug_menu_destruction_save_proofs=" +
             std::to_string(debugMenuState->destructionSaveProofs));
    log.Line("debug_menu_restored_pre_vehicle_destruction_checkpoints=" +
             std::to_string(
                 debugMenuState
                     ->restoredPreVehicleDestructionCheckpoints));
    log.Line("debug_menu_damaged_occupied_vehicles=" +
             std::to_string(
                 debugMenuState->damagedOccupiedVehicles));
    log.Line("debug_menu_last_vehicle_damage=" +
             std::to_string(debugMenuState->lastVehicleDamageBefore) +
             "/" +
             std::to_string(debugMenuState->lastVehicleDamageAfter));
    log.Line("debug_menu_pre_vehicle_destruction_checkpoint_available=" +
             std::to_string(
                 debugMenuState
                         ->preVehicleDestructionCheckpointAvailable
                     ? 1
                     : 0));
    log.Line("debug_menu_destruction_world_fingerprint=" +
             std::to_string(
                 debugMenuState->destructionWorldFingerprint));
    log.Line("debug_menu_destruction_continuation_fingerprint=" +
             std::to_string(
                 debugMenuState->destructionContinuationFingerprint));
    log.Line("debug_menu_destruction_orphan_fingerprint=" +
             std::to_string(
                 debugMenuState->destructionOrphanFingerprint));
    log.Line("debug_menu_level_switch_requests=" +
             std::to_string(debugMenuState->levelSwitchRequests));
    log.Line("debug_menu_completed_level_switches=" +
             std::to_string(debugMenuState->completedLevelSwitches));
    log.Line("debug_menu_level_switch_rollbacks=" +
             std::to_string(debugMenuState->levelSwitchRollbacks));
    log.Line("debug_menu_level_switch_rollback_failures=" +
             std::to_string(debugMenuState->levelSwitchRollbackFailures));
    log.Line("debug_menu_last_action=" + debugMenuState->lastAction);
    log.Line("debug_menu_last_object=" + debugMenuState->lastObject);
    log.Line("debug_menu_last_taxi_attribute=" +
             debugMenuState->lastTaxiAttribute);
    log.Line("debug_menu_last_vehicle_attribute=" +
             debugMenuState->lastVehicleAttribute);
    if (!debugMenuState->lastError.empty())
      log.Line("debug_menu_last_error=" + debugMenuState->lastError);
    log.Line("debug_menu_first_deferred_error=" +
             debugMenuState->firstDeferredError);
  }
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
