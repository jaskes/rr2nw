#include "GameStartup.h"

#include "RR2NWBuildRevision.h"
#include "ActiveWorldSave.h"
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
#include "filesys.h"
#include "graph.h"
#include "h/super.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/howitzer/HowitzerActiveWorldState.h"
#include "obase/people/PeopleSubjectState.h"
#include "obase/portal/PortalActiveWorldState.h"
#include "obase/recrcen/RecruitCenterSubjectState.h"
#include "obase/taxi/TaxiSubjectState.h"
#include "suavik.h"

#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
  std::wstring missionProject;
  std::wstring missionNextProject;
  int startupSaveSlot = -1;
  int startupLoadSlot = -1;
  bool launchSmoke = false;
  bool runtimeSmoke = false;
  bool missionSmoke = false;
  bool missionBriefingSmoke = false;
  bool missionCombatSmoke = false;
  bool missionNaturalCombatSmoke = false;
  bool missionGuideRouteSmoke = false;
  bool missionContinuationSmoke = false;
  bool missionResultSmoke = false;
  bool missionNoRewardResultSmoke = false;
  bool missionTerminalNoRewardResultSmoke = false;
  bool missionNoRewardFreshSmoke = false;
  bool missionTerminalNoRewardFreshSmoke = false;
  bool campaignQuestChainSmoke = false;
  bool missionObjectiveChainSmoke = false;
  bool missionTerminalStateSmoke = false;
  bool portalTransitionSmoke = false;
  bool levelBriefingSmoke = false;
  bool skipLevelBriefing = false;
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
    } else if (argument == L"--mission-combat-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionCombatSmoke = true;
    } else if (argument == L"--mission-natural-combat-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionNaturalCombatSmoke = true;
    } else if (argument == L"--mission-guide-route-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionGuideRouteSmoke = true;
    } else if (argument == L"--mission-continuation-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionContinuationSmoke = true;
    } else if (argument == L"--mission-result-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionResultSmoke = true;
    } else if (argument == L"--mission-no-reward-result-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionNoRewardResultSmoke = true;
    } else if (argument == L"--mission-terminal-no-reward-result-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionTerminalNoRewardResultSmoke = true;
    } else if (argument == L"--mission-no-reward-fresh-smoke") {
      options->runtimeSmoke = true;
      options->missionNoRewardFreshSmoke = true;
    } else if (argument == L"--mission-terminal-no-reward-fresh-smoke") {
      options->runtimeSmoke = true;
      options->missionTerminalNoRewardFreshSmoke = true;
    } else if (argument == L"--campaign-quest-chain-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionResultSmoke = true;
      options->campaignQuestChainSmoke = true;
    } else if (argument == L"--mission-objective-chain-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionObjectiveChainSmoke = true;
    } else if (argument == L"--mission-terminal-state-smoke") {
      options->runtimeSmoke = true;
      options->missionSmoke = true;
      options->missionTerminalStateSmoke = true;
    } else if (argument == L"--portal-transition-smoke") {
      options->runtimeSmoke = true;
      options->portalTransitionSmoke = true;
    } else if (argument == L"--level-briefing-smoke") {
      options->runtimeSmoke = true;
      options->levelBriefingSmoke = true;
    } else if (argument == L"--skip-level-briefing") {
      options->skipLevelBriefing = true;
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
    } else if (argument == L"--mission-project") {
      if (!ParseOptionValue(argc, argv, &index, L"--mission-project",
                            &options->missionProject, failure)) {
        return false;
      }
    } else if (argument.compare(0, 18, L"--mission-project=") == 0) {
      options->missionProject = argument.substr(18);
      if (options->missionProject.empty()) {
        *failure = L"empty value for --mission-project";
        return false;
      }
    } else if (argument == L"--mission-next-project") {
      if (!ParseOptionValue(argc, argv, &index, L"--mission-next-project",
                            &options->missionNextProject, failure)) {
        return false;
      }
    } else if (argument.compare(0, 23, L"--mission-next-project=") == 0) {
      options->missionNextProject = argument.substr(23);
      if (options->missionNextProject.empty()) {
        *failure = L"empty value for --mission-next-project";
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
  const bool chainedNoRewardResultSave =
      options->startupSaveSlot >= 0 && options->startupLoadSlot >= 0 &&
      options->missionNoRewardResultSmoke &&
      options->startupSaveSlot != options->startupLoadSlot;
  if (options->startupSaveSlot >= 0 && options->startupLoadSlot >= 0 &&
      !chainedNoRewardResultSave) {
    *failure = L"--save-slot and --load-slot cannot be used together";
    return false;
  }
  if (!options->missionCenter.empty() && !options->missionSmoke &&
      !options->missionNoRewardFreshSmoke) {
    *failure = L"--mission-center requires --mission-smoke";
    return false;
  }
  if (!options->missionProject.empty() &&
      !options->missionNoRewardFreshSmoke &&
      (!options->campaignQuestChainSmoke || options->missionCenter.empty() ||
       options->missionBriefingSmoke || options->missionCombatSmoke ||
       options->missionNaturalCombatSmoke ||
       options->missionGuideRouteSmoke ||
       options->missionContinuationSmoke ||
       options->missionObjectiveChainSmoke ||
       options->missionTerminalStateSmoke)) {
    *failure = L"--mission-project requires --campaign-quest-chain-smoke "
               L"and --mission-center without another specialized mission "
               L"smoke";
    return false;
  }
  if (!options->missionNextProject.empty() &&
      !options->missionNoRewardFreshSmoke) {
    *failure = L"--mission-next-project requires "
               L"--mission-no-reward-fresh-smoke";
    return false;
  }
  if (options->levelBriefingSmoke && options->skipLevelBriefing) {
    *failure = L"--level-briefing-smoke cannot be combined with "
               L"--skip-level-briefing";
    return false;
  }
  if (options->missionObjectiveChainSmoke) {
    if (options->missionCenter.empty()) {
      options->missionCenter = L"Inhabitants.Recruit.0";
    } else if (_wcsicmp(options->missionCenter.c_str(),
                        L"Inhabitants.Recruit.0") != 0) {
      *failure = L"--mission-objective-chain-smoke starts at "
                 L"Inhabitants.Recruit.0";
      return false;
    }
    if (options->startupSaveSlot >= 0 || options->startupLoadSlot >= 0) {
      *failure = L"--mission-objective-chain-smoke owns its save/rollback "
                 L"transaction";
      return false;
    }
  }
  if (options->missionTerminalStateSmoke) {
    if (options->missionCenter.empty()) {
      options->missionCenter = L"Magician.Recruit.0";
    } else if (_wcsicmp(options->missionCenter.c_str(),
                        L"Magician.Recruit.0") != 0) {
      *failure = L"--mission-terminal-state-smoke starts at "
                 L"Magician.Recruit.0 on Level.02N";
      return false;
    }
    if (options->startupSaveSlot >= 0 || options->startupLoadSlot >= 0) {
      *failure = L"--mission-terminal-state-smoke owns its save/rollback "
                 L"transaction";
      return false;
    }
  }
  if (options->missionNoRewardResultSmoke) {
    if (options->missionCenter.empty())
      options->missionCenter = L"Recruit.Robots";
    if (options->startupLoadSlot >= 0 && options->startupSaveSlot < 0) {
      *failure = L"--mission-no-reward-result-smoke requires a distinct "
                 L"--save-slot when it loads a progression slot";
      return false;
    }
  }
  if (options->missionTerminalNoRewardResultSmoke) {
    if (options->missionCenter.empty()) {
      options->missionCenter = L"Recruit.Outsider";
    } else if (_wcsicmp(options->missionCenter.c_str(),
                        L"Recruit.Outsider") != 0) {
      *failure = L"--mission-terminal-no-reward-result-smoke starts at "
                 L"Recruit.Outsider on Level.01N";
      return false;
    }
    if (options->startupLoadSlot >= 0) {
      *failure = L"--mission-terminal-no-reward-result-smoke cannot load a "
                 L"slot";
      return false;
    }
  }
  if (options->missionTerminalNoRewardFreshSmoke) {
    if (options->startupLoadSlot < 0 || options->startupSaveSlot >= 0 ||
        !options->missionCenter.empty() || !options->missionProject.empty()) {
      *failure = L"--mission-terminal-no-reward-fresh-smoke requires one "
                 L"--load-slot and owns its fixed Level.01N center";
      return false;
    }
  }
  if (options->missionNoRewardFreshSmoke) {
    const bool hasFreshIdentity = !options->missionCenter.empty() ||
        !options->missionProject.empty() ||
        !options->missionNextProject.empty();
    const bool completeFreshIdentity = !options->missionCenter.empty() &&
        !options->missionProject.empty() &&
        !options->missionNextProject.empty();
    if (options->startupLoadSlot < 0 || options->startupSaveSlot >= 0 ||
        (hasFreshIdentity && !completeFreshIdentity)) {
      *failure = L"--mission-no-reward-fresh-smoke requires one --load-slot "
                 L"and either no identity override or a complete "
                 L"--mission-center/--mission-project/"
                 L"--mission-next-project triple";
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

enum class ELevelBriefingPolicy { Suppress, ValidateOnly, Present };

struct SLevelBriefingPreflight {
  bool configured = false;
  std::string authoredPath;
  std::string resolvedPath;
  int actionCount = 0;
  int flightCount = 0;
  int flicCount = 0;
  int flightPointCount = 0;
  int assetCount = 0;
  std::uint64_t assetBytes = 0;
  std::uint64_t fingerprint = 1469598103934665603ull;
};

std::string TrimBriefingField(const std::string& value) {
  const std::string whitespace(" \t\r\n");
  const std::string::size_type first = value.find_first_not_of(whitespace);
  if (first == std::string::npos) return std::string();
  const std::string::size_type last = value.find_last_not_of(whitespace);
  return value.substr(first, last - first + 1u);
}

std::vector<std::string> SplitBriefingAction(const char* value) {
  std::vector<std::string> fields;
  if (value == nullptr) return fields;
  const std::string source(value);
  std::string::size_type begin = 0;
  for (;;) {
    const std::string::size_type comma = source.find(',', begin);
    fields.push_back(TrimBriefingField(
        source.substr(begin, comma == std::string::npos
                                 ? std::string::npos
                                 : comma - begin)));
    if (comma == std::string::npos) break;
    begin = comma + 1u;
  }
  return fields;
}

bool IsSafeLevelBriefingPath(const std::string& path) {
  if (path.empty() || path.size() >= 260u || path[0] == '/' ||
      path[0] == '\\' || path.find(':') != std::string::npos)
    return false;
  std::string normalized(path);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  return normalized != ".." && normalized.find("../") != 0u &&
         normalized.find("/../") == std::string::npos &&
         (normalized.size() < 3u ||
          normalized.substr(normalized.size() - 3u) != "/..");
}

bool HashLevelBriefingAsset(const std::string& authoredPath,
                            bool baseRootAsset,
                            SLevelBriefingPreflight* summary,
                            std::string* resolvedPath,
                            std::string* failure) {
  if (summary == nullptr || !IsSafeLevelBriefingPath(authoredPath)) {
    if (failure != nullptr)
      *failure = "Level briefing contains an unsafe asset path: " +
                 authoredPath;
    return false;
  }
  char resolved[32768] = {};
  const bool resolvedAsset =
      baseRootAsset
          ? RecoveredModRuntime_ResolveBaseReadPath(
                authoredPath.c_str(), resolved, sizeof(resolved))
          : RecoveredModRuntime_ResolveReadPath(
                authoredPath.c_str(), resolved, sizeof(resolved));
  if (!resolvedAsset) {
    if (failure != nullptr)
      *failure = "Level briefing asset cannot be resolved: " + authoredPath;
    return false;
  }
  long length = 0;
  FILE* file = baseRootAsset
                   ? RecoveredModRuntime_OpenBaseRead(authoredPath.c_str(),
                                                      &length)
                   : RecoveredModRuntime_OpenRead(authoredPath.c_str(),
                                                  &length);
  constexpr long kMaximumBriefingAssetBytes = 64l * 1024l * 1024l;
  if (file == nullptr || length < 0 || length > kMaximumBriefingAssetBytes) {
    if (file != nullptr) std::fclose(file);
    if (failure != nullptr)
      *failure = "Level briefing asset is missing or outside its size limit: " +
                 authoredPath;
    return false;
  }
  std::array<unsigned char, 16384> buffer = {};
  long remaining = length;
  while (remaining > 0) {
    const std::size_t requested = static_cast<std::size_t>(std::min<long>(
        remaining, static_cast<long>(buffer.size())));
    const std::size_t read = std::fread(buffer.data(), 1, requested, file);
    if (read != requested) {
      std::fclose(file);
      if (failure != nullptr)
        *failure = "Level briefing asset could not be read completely: " +
                   authoredPath;
      return false;
    }
    for (std::size_t index = 0; index < read; ++index) {
      summary->fingerprint ^= buffer[index];
      summary->fingerprint *= 1099511628211ull;
    }
    remaining -= static_cast<long>(read);
  }
  std::fclose(file);
  ++summary->assetCount;
  summary->assetBytes += static_cast<std::uint64_t>(length);
  if (resolvedPath != nullptr) *resolvedPath = resolved;
  return true;
}

bool PreflightLevelBriefing(SLevelBriefingPreflight* summary,
                            std::string* failure) {
  if (summary == nullptr) {
    if (failure != nullptr) *failure = "Level briefing has no summary storage";
    return false;
  }
  *summary = SLevelBriefingPreflight{};
  summary->configured = ZAV_Config().GetInt("Briefing", "Play", 0) != 0;
  const char* configuredName = ZAV_Config()("Briefing", "Name");
  summary->authoredPath = configuredName == nullptr
                              ? std::string()
                              : TrimBriefingField(configuredName);
  if (!summary->configured) return true;
  if (!HashLevelBriefingAsset(summary->authoredPath, false, summary,
                              &summary->resolvedPath, failure))
    return false;

  CConfigFile config(const_cast<char*>(summary->resolvedPath.c_str()));
  summary->actionCount = config.GetInt("Root", "ActionsNum", 0);
  const int repeatCount = config.GetInt("Root", "RepeatsNum", 1);
  if (summary->actionCount <= 0 || summary->actionCount > 256 ||
      repeatCount <= 0 || repeatCount >= 10000) {
    if (failure != nullptr)
      *failure = "Level briefing Root action/repeat count is invalid";
    return false;
  }
  for (int actionIndex = 1; actionIndex <= summary->actionCount;
       ++actionIndex) {
    char actionName[32] = {};
    std::snprintf(actionName, sizeof(actionName), "Action%d", actionIndex);
    const std::vector<std::string> fields =
        SplitBriefingAction(config("Root", actionName));
    if (fields.size() < 2u || fields[1].empty()) {
      if (failure != nullptr)
        *failure = "Level briefing action " + std::to_string(actionIndex) +
                   " is incomplete";
      return false;
    }
    if (fields[0] == "PlayFlight") {
      if (fields.size() != 4u) {
        if (failure != nullptr)
          *failure = "Level briefing flight action has an invalid shape";
        return false;
      }
      char* end = nullptr;
      errno = 0;
      const long pointCount = std::strtol(fields[3].c_str(), &end, 10);
      if (errno != 0 || end == fields[3].c_str() || *end != '\0' ||
          pointCount <= 0 || pointCount > 4096) {
        if (failure != nullptr)
          *failure = "Level briefing flight point count is invalid";
        return false;
      }
      for (long point = 0; point < pointCount; ++point) {
        const std::string section =
            fields[1] + "." + std::to_string(point);
        char* mutableSection = const_cast<char*>(section.c_str());
        if (config(mutableSection, "Pos  ") == nullptr ||
            config(mutableSection, "Angle") == nullptr ||
            config(mutableSection, "Time ") == nullptr ||
            config(mutableSection, "Delay") == nullptr) {
          if (failure != nullptr)
            *failure = "Level briefing flight is missing authored point " +
                       section;
          return false;
        }
      }
      ++summary->flightCount;
      summary->flightPointCount += static_cast<int>(pointCount);
    } else if (fields[0] == "PlayFlic") {
      if (fields.size() != 2u) {
        if (failure != nullptr)
          *failure = "Level briefing FLC action has an invalid shape";
        return false;
      }
      char* section = const_cast<char*>(fields[1].c_str());
      const char* flicName = config(section, "Name ");
      const std::string flicPath =
          flicName == nullptr ? std::string() : TrimBriefingField(flicName);
      if (config(section, "Text ") == nullptr ||
          config.GetDouble(section, "Delay", -1.0) <= 0.0) {
        if (failure != nullptr)
          *failure = "Level briefing FLC section is incomplete: " + fields[1];
        return false;
      }
      if (!HashLevelBriefingAsset(flicPath, true, summary, nullptr, failure))
        return false;
      ++summary->flicCount;
    } else {
      if (failure != nullptr)
        *failure = "Level briefing action " + std::to_string(actionIndex) +
                   " has unsupported type " + fields[0];
      return false;
    }
  }
  return true;
}

const char* LevelBriefingPolicyName(ELevelBriefingPolicy policy) {
  switch (policy) {
    case ELevelBriefingPolicy::Suppress:
      return "suppressed";
    case ELevelBriefingPolicy::ValidateOnly:
      return "validated";
    case ELevelBriefingPolicy::Present:
      return "presented";
  }
  return "unknown";
}

bool HandleLevelBriefing(ELevelBriefingPolicy policy, const char* boundary,
                         StartupLog* log, std::string* failure) {
  const std::string prefix =
      boundary == nullptr || boundary[0] == '\0'
          ? std::string("level_briefing_")
          : std::string(boundary) + "_level_briefing_";
  if (log != nullptr)
    log->Line(prefix + "policy=" + LevelBriefingPolicyName(policy));
  if (policy == ELevelBriefingPolicy::Suppress) return true;

  SLevelBriefingPreflight summary;
  if (!PreflightLevelBriefing(&summary, failure)) {
    if (log != nullptr && failure != nullptr)
      log->Line(prefix + "failure=" + *failure);
    return false;
  }
  if (log != nullptr) {
    log->Line(prefix + "configured=" +
              std::to_string(summary.configured ? 1 : 0));
    log->Line(prefix + "name=" + summary.authoredPath);
    log->Line(prefix + "actions=" + std::to_string(summary.actionCount));
    log->Line(prefix + "flights=" + std::to_string(summary.flightCount));
    log->Line(prefix + "flics=" + std::to_string(summary.flicCount));
    log->Line(prefix + "flight_points=" +
              std::to_string(summary.flightPointCount));
    log->Line(prefix + "assets=" + std::to_string(summary.assetCount));
    log->Line(prefix + "bytes=" + std::to_string(summary.assetBytes));
    log->Line(prefix + "fingerprint=" +
              std::to_string(summary.fingerprint));
    log->Line(prefix + "preflight=complete");
  }
  if (policy == ELevelBriefingPolicy::Present && summary.configured) {
    if (!RecoveredGameServices_PlayLevelBriefing(
            summary.resolvedPath.c_str())) {
      if (failure != nullptr)
        *failure = "Level briefing presenter is not attached to the active "
                   "session";
      if (log != nullptr && failure != nullptr)
        log->Line(prefix + "failure=" + *failure);
      return false;
    }
    if (log != nullptr) log->Line(prefix + "playback=returned");
  } else if (log != nullptr) {
    log->Line(prefix + "playback=skipped");
  }
  return true;
}

bool StartRecoveredLevel(const RetailData& data, int levelIndex,
                         ELevelBriefingPolicy briefingPolicy,
                         const char* briefingBoundary, StartupLog* log,
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
  if (!HandleLevelBriefing(briefingPolicy, briefingBoundary, log, failure))
    return false;
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
  bool targetStarted = StartRecoveredLevel(
      data, targetLevelIndex, ELevelBriefingPolicy::Suppress,
      "cross_load_target", log, &targetFailure);
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
  const bool sourceStarted = StartRecoveredLevel(
      data, sourceLevelIndex, ELevelBriefingPolicy::Suppress,
      "cross_load_rollback", log, &sourceFailure);
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
                             bool suppressBriefing,
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
  if (StartRecoveredLevel(
          data, targetLevelIndex,
          suppressBriefing
              ? ELevelBriefingPolicy::Suppress
              : silent ? ELevelBriefingPolicy::ValidateOnly
                       : ELevelBriefingPolicy::Present,
          "debug_switch_target", log, &targetFailure)) {
    *currentLevelIndex = targetLevelIndex;
    RecoveredGameServices_RecordDebugLevelSwitchResult(
        request, true, false, false, std::string());
    if (log != nullptr)
      log->Line("debug_level_switch_commit=" + request.targetLevel);
    return true;
  }

  ZAV_DeInitLevel();
  std::string sourceFailure;
  const bool sourceStarted = StartRecoveredLevel(
      data, sourceLevelIndex, ELevelBriefingPolicy::Suppress,
      "debug_switch_rollback", log, &sourceFailure);
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

bool ProcessPortalLevelTransition(const RetailData& data,
                                  int* currentLevelIndex, bool silent,
                                  bool suppressBriefing,
                                  StartupLog* log) {
  if (!PortalActiveWorldState_TakeTransitionRequest()) return true;
  if (currentLevelIndex == nullptr || *currentLevelIndex < 0 ||
      *currentLevelIndex >= static_cast<int>(data.levels.size()) ||
      data.levels.empty()) {
    if (log != nullptr)
      log->Line("portal_transition_preflight_failure=invalid Level catalog");
    return false;
  }

  const int sourceLevelIndex = *currentLevelIndex;
  const bool completedCampaign =
      sourceLevelIndex == static_cast<int>(data.levels.size()) - 1;
  const int targetLevelIndex = completedCampaign ? 0 : sourceLevelIndex + 1;
  std::vector<std::uint8_t> sourceContinuation;
  SLevelContinuationSummary sourceSummary;
  if (!RecoveredGameServices_CaptureLevelContinuation(
          &sourceContinuation, &sourceSummary) || !sourceSummary.ready) {
    const std::string detail =
        RecoveredGameServices_LastLevelContinuationError();
    if (log != nullptr)
      log->Line("portal_transition_preflight_failure=" + detail);
    ShowMessage(silent, MB_ICONERROR, L"RR2NW Portal error",
                Utf8ToWide(detail.c_str()));
    return false;
  }

  if (log != nullptr) {
    log->Line("portal_transition_begin=" +
              WideToUtf8(data.levels[sourceLevelIndex]) + "->" +
              WideToUtf8(data.levels[targetLevelIndex]));
    log->Line(std::string("portal_campaign_completion=") +
              (completedCampaign ? "1" : "0"));
  }
  ZAV_DeInitLevel();

  std::string targetFailure;
  if (StartRecoveredLevel(
          data, targetLevelIndex,
          suppressBriefing
              ? ELevelBriefingPolicy::Suppress
              : silent ? ELevelBriefingPolicy::ValidateOnly
                       : ELevelBriefingPolicy::Present,
          "portal_target", log, &targetFailure)) {
    *currentLevelIndex = targetLevelIndex;
    if (log != nullptr)
      log->Line("portal_transition_commit=" +
                WideToUtf8(data.levels[targetLevelIndex]));
    if (completedCampaign)
      ShowMessage(silent, MB_ICONINFORMATION, L"RR2NW",
                  L"ПОЗДРАВЛЯЕМ!!!");
    return true;
  }

  ZAV_DeInitLevel();
  std::string sourceFailure;
  const bool sourceStarted = StartRecoveredLevel(
      data, sourceLevelIndex, ELevelBriefingPolicy::Suppress,
      "portal_rollback", log, &sourceFailure);
  SLevelContinuationSummary restored;
  const bool sourceRestored = sourceStarted &&
      RecoveredGameServices_RestoreLevelContinuation(
          sourceContinuation, &restored);
  std::string detail = "Portal Level transition failed: " + targetFailure;
  if (!sourceRestored) {
    detail += "; source rollback failed: ";
    detail += sourceStarted
                  ? RecoveredGameServices_LastLevelContinuationError()
                  : sourceFailure;
  }
  if (log != nullptr) {
    log->Line(std::string("portal_transition_rollback=") +
              (sourceRestored ? "restored" : "failed"));
    log->Line("portal_transition_failure=" + detail);
  }
  ShowMessage(silent, MB_ICONERROR, L"RR2NW Portal error",
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
  if (StartRecoveredLevel(
          data, sourceLevelIndex, ELevelBriefingPolicy::Suppress,
          "campaign_restart", log, &restartFailure)) {
    *currentLevelIndex = sourceLevelIndex;
    RecoveredGameServices_RecordCampaignRestartResult(
        request, true, false, false, std::string());
    if (log != nullptr)
      log->Line("campaign_restart_commit=" + request.level);
    return true;
  }

  ZAV_DeInitLevel();
  std::string rollbackStartFailure;
  const bool rollbackStarted = StartRecoveredLevel(
      data, sourceLevelIndex, ELevelBriefingPolicy::Suppress,
      "campaign_restart_rollback", log, &rollbackStartFailure);
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
                L"          [--save-slot <1..8>] [--load-slot <1..8>]\n"
                L"          [--debug-menu] [--launch-smoke] [--runtime-smoke]\n"
                L"          [--mission-smoke | --mission-briefing-smoke |\n"
                L"           --mission-combat-smoke |\n"
                L"           --mission-natural-combat-smoke |\n"
                L"           --mission-guide-route-smoke |\n"
                L"           --mission-continuation-smoke |\n"
                 L"           --mission-result-smoke |\n"
                 L"           --mission-no-reward-result-smoke |\n"
                 L"           --mission-no-reward-fresh-smoke |\n"
                 L"           --mission-terminal-no-reward-result-smoke |\n"
                 L"           --mission-terminal-no-reward-fresh-smoke |\n"
                 L"           --campaign-quest-chain-smoke |\n"
                 L"           --mission-objective-chain-smoke |\n"
                 L"           --mission-terminal-state-smoke]\n"
                L"          (both slots only chain a no-reward result smoke)\n"
                L"          [--portal-transition-smoke]\n"
                L"          [--level-briefing-smoke]\n"
                L"          [--skip-level-briefing]\n"
                L"          [--mission-center <name>]\n"
                L"          [--mission-project <name>]\n"
                L"          [--mission-next-project <name>]\n"
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
  log.Line("vehicle_projectile_render_submissions=" +
           std::to_string(primaryFire.bulletRenderSubmissions));
  log.Line("vehicle_projectile_particle_submissions=" +
           std::to_string(primaryFire.particleRenderSubmissions));
  log.Line("vehicle_projectile_skin_submissions=" +
           std::to_string(primaryFire.skinRenderSubmissions));
  log.Line("vehicle_projectile_skipped_skin_submissions=" +
           std::to_string(primaryFire.skippedSkinRenderSubmissions));
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
  log.Line("people_combat_probe=" +
           std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeAvailable()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeAttackerReady()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeTargetReady()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeRouteDisplacement()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeTargetAcquired()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeTargetCadence()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeProjectileStarted()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeDamageDelivered()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeDeathTransition()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeDeathEffects()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleCombatProbeRollbacks()));
  log.Line("people_active_world_probe=" +
           std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents()) +
           "/" + std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldRollbacks()));
  log.Line("people_active_world_fingerprint=" + std::to_string(
               RecoveredArenaSeance_PeopleActiveWorldFingerprint()));
  SPeopleCombatScheduleSummary peopleSchedule = {};
  const bool peopleScheduleReady = PeopleSubjectState_AuditCombatScheduling(
      g_super.m_context, &peopleSchedule);
  log.Line("people_combat_schedule=" +
           std::to_string(peopleScheduleReady ? 1 : 0) + "/" +
           std::to_string(peopleSchedule.livePeople) + "/" +
           std::to_string(peopleSchedule.shooters) + "/" +
           std::to_string(peopleSchedule.commandedShooters) + "/" +
           std::to_string(peopleSchedule.commanderInterfaces) + "/" +
           std::to_string(peopleSchedule.scheduledFindEnemy) + "/" +
           std::to_string(peopleSchedule.scheduledMotion) + "/" +
           std::to_string(peopleSchedule.attackStates) + "/" +
           std::to_string(peopleSchedule.malformedQueues));
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
  log.Line("portal_subject_roster=" + std::to_string(
               PortalActiveWorldState_LiveCount(g_super.m_context)));
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
  std::string levelBriefingFailure;
  const ELevelBriefingPolicy initialBriefingPolicy =
      options.startupLoadSlot >= 0 || options.skipLevelBriefing
          ? ELevelBriefingPolicy::Suppress
          : options.runtimeSmoke
                ? (options.levelBriefingSmoke
                       ? ELevelBriefingPolicy::ValidateOnly
                       : ELevelBriefingPolicy::Suppress)
                : ELevelBriefingPolicy::Present;
  if (!loopFailed && !HandleLevelBriefing(initialBriefingPolicy, nullptr,
                                          &log, &levelBriefingFailure)) {
    log.Line("failure_level_briefing=" + levelBriefingFailure);
    loopFailed = true;
  }
  // A mission acceptance save must be captured after the synthetic retail
  // mission has run.  Ordinary startup saves retain the original first-frame
  // boundary below; the mission path is committed later in the mission block.
  const bool saveAfterMission =
      options.startupSaveSlot >= 0 && options.missionSmoke;
  if (!loopFailed && options.startupSaveSlot >= 0 && !saveAfterMission &&
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
    if (PortalActiveWorldState_TransitionPending() &&
        !ProcessPortalLevelTransition(data, &currentLevelIndex,
                                      options.runtimeSmoke,
                                      options.skipLevelBriefing, &log))
      return false;
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
                                   options.runtimeSmoke,
                                   options.skipLevelBriefing, &log);
  };
  // Ordinary startup save/load is itself a closed-frame operation. Complete
  // it before staging the synthetic map mission. Mission acceptance saves are
  // deliberately postponed so a fresh process has to reconstruct the real
  // mission-created Route and owner graph.
  if (!loopFailed && options.runtimeSmoke &&
      ((!saveAfterMission && options.startupSaveSlot >= 0) ||
       options.startupLoadSlot >= 0))
    loopFailed = !runCompleteFrame();
  if (!loopFailed && options.runtimeSmoke &&
      options.startupSaveSlot < 0 && options.startupLoadSlot < 0) {
    loopFailed = !RecoveredGameServices_StageMissionMapProbe() ||
                 !runCompleteFrame() ||
                 !RecoveredGameServices_RequestDebugMapToggle() ||
                 !runCompleteFrame() ||
                 !RecoveredGameServices_VerifyMissionMapProbe() ||
                 !RecoveredGameServices_ProbeDebugMapControls() ||
                 !RecoveredGameServices_RequestDebugMapToggle() ||
                 !RecoveredGameServices_ClearMissionMapProbe();
  }
  if (!loopFailed && options.portalTransitionSmoke) {
    const int sourceLevelIndex = currentLevelIndex;
    const int expectedLevelIndex =
        sourceLevelIndex == static_cast<int>(data.levels.size()) - 1
            ? 0 : sourceLevelIndex + 1;
    SPortalPresentationProbeSummary portalPresentation;
    const bool portalPresentationReady =
        PortalActiveWorldState_StagePresentationProbe(
            g_super.m_context, &portalPresentation);
    log.Line("portal_presentation_probe=" +
             std::to_string(portalPresentation.portalCount) + "/" +
             std::to_string(portalPresentation.singularStatus) + "/" +
             std::to_string(portalPresentation.fewStatus) + "/" +
             std::to_string(portalPresentation.manyStatus) + "/" +
             std::to_string(portalPresentation.restoredStatus) + "/" +
             std::to_string(portalPresentation.messagesPublished) + "/" +
             std::to_string(portalPresentation.arabeskPresent) + "/" +
             std::to_string(portalPresentation.arabeskRemoved) + "/" +
             std::to_string(portalPresentation.arabeskRecreated) + "/" +
             std::to_string(portalPresentation.arabeskRestoreRemoved));
    SPortalTransitionProbeSummary portalTransition;
    const bool portalTransitionStaged = portalPresentationReady &&
        PortalActiveWorldState_StageTransitionProbe(
            g_super.m_context,
            (std::max)(0.1, Session::m_viewTime + 0.1),
            &portalTransition);
    const bool portalTransitionCompleted =
        portalTransitionStaged && runCompleteFrame() &&
        currentLevelIndex == expectedLevelIndex &&
        !PortalActiveWorldState_TransitionPending();
    log.Line("portal_transition_probe=" +
             std::to_string(portalTransition.portalCount) + "/" +
             std::to_string(portalTransition.fullPortal) + "/" +
             std::to_string(portalTransition.collisionAccepted) + "/" +
             std::to_string(portalTransition.transitionRequested));
    log.Line("portal_transition_catalog=" +
             std::to_string(sourceLevelIndex) + "/" +
             std::to_string(expectedLevelIndex) + "/" +
             std::to_string(currentLevelIndex));
    if (!portalPresentationReady || !portalTransitionStaged)
      log.Line(std::string("portal_transition_error=") +
               PortalActiveWorldState_LastFailure());
    loopFailed = !portalTransitionCompleted;
  }
  if (!loopFailed && options.missionSmoke) {
    std::vector<std::uint8_t> objectiveBaseline;
    RecruitCenterObjectiveStateSummary objectiveBaselineState = {};
    SLevelContinuationSummary objectiveBaselineSummary;
    const bool objectiveBaselineReady =
        (!options.missionObjectiveChainSmoke &&
         !options.missionTerminalStateSmoke) ||
        (RecruitCenterSubjectState_ObjectiveState(
             g_super.m_context, &objectiveBaselineState) &&
         RecoveredGameServices_CaptureLevelContinuation(
             &objectiveBaseline, &objectiveBaselineSummary));
    if (options.missionObjectiveChainSmoke) {
      log.Line("mission_objective_baseline=" +
               std::to_string(objectiveBaselineReady ? 1 : 0) + "/" +
               std::to_string(objectiveBaselineState.missions) + "/" +
               std::to_string(objectiveBaselineState.scheduledChecks) + "/" +
               std::to_string(objectiveBaseline.size()));
      loopFailed = !objectiveBaselineReady;
    }
    std::vector<KR_ObjectID> preMissionTaxis;
    const bool preMissionTaxisReady = TaxiSubjectState_ObjectIDs(
        g_super.m_context, &preMissionTaxis);
    std::vector<KR_ObjectID> preMissionPeople;
    const bool preMissionPeopleReady = PeopleSubjectState_ObjectIDs(
        g_super.m_context, &preMissionPeople);
    bool missionStaged = false;
    RecruitCenterMissionProbeSummary mission = {};
    const double missionTime =
        (std::max)(0.1, Session::m_viewTime + 0.25);
    const std::string missionCenter = WideToUtf8(options.missionCenter);
    const std::string requestedMissionProject =
        WideToUtf8(options.missionProject);
    log.Line("mission_smoke_center=" +
             (missionCenter.empty() ? std::string("<first-eligible>")
                                    : missionCenter));
    log.Line("mission_smoke_requested_project=" +
             (requestedMissionProject.empty()
                  ? std::string("<first-eligible>")
                  : requestedMissionProject));
    const bool missionExecuted = options.missionBriefingSmoke
        ? (missionCenter.empty()
               ? RecruitCenterSubjectState_StageMissionPresentationProbe(
                     g_super.m_context, missionTime, &missionStaged, &mission)
               : RecruitCenterSubjectState_StageMissionPresentationProbeForCenter(
                     g_super.m_context, missionTime, missionCenter.c_str(),
                     &missionStaged, &mission))
        : (!requestedMissionProject.empty()
               ? RecruitCenterSubjectState_StageMissionExecutionProbeForProject(
                     g_super.m_context, missionTime, missionCenter.c_str(),
                     requestedMissionProject.c_str(), &missionStaged,
                     &mission)
               : missionCenter.empty()
                     ? RecruitCenterSubjectState_StageMissionExecutionProbe(
                           g_super.m_context, missionTime, &missionStaged,
                           &mission)
                     : RecruitCenterSubjectState_StageMissionExecutionProbeForCenter(
                           g_super.m_context, missionTime,
                           missionCenter.c_str(), &missionStaged, &mission));
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
    log.Line("mission_smoke_reclaimed_routes=" +
             std::to_string(mission.reclaimedRouteObjects));
    log.Line("mission_smoke_conditions=" +
             std::to_string(mission.conditionReferences));
    log.Line("mission_smoke_routes=" +
             std::to_string(mission.routeReferences));
    log.Line("mission_smoke_rebound_conditions=" +
             std::to_string(mission.reboundConditionReferences));
    log.Line("mission_smoke_briefings=" +
             std::to_string(mission.presentedBriefings));
    log.Line("mission_smoke_center_presentations=" +
             std::to_string(mission.centerPresentationAttempts) + "/" +
             std::to_string(mission.presentedCenterFlicks) + "/" +
             std::to_string(mission.presentedHostilityBriefings) + "/" +
             std::to_string(mission.centerPresentationFailures));
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
    const int expectedCenterPresentations =
        options.missionBriefingSmoke ? 1 : 0;
    loopFailed = !missionExecuted || !missionStaged ||
                 mission.executedScripts < 1 ||
                 mission.createdMissionObjects < 1 ||
                 mission.presentedBriefings != expectedBriefings ||
                 mission.centerPresentationAttempts !=
                     expectedCenterPresentations ||
                 mission.presentedCenterFlicks +
                         mission.presentedHostilityBriefings !=
                     expectedCenterPresentations ||
                 mission.centerPresentationFailures != 0 ||
                 mission.scriptRollbacks != 0 || !runCompleteFrame();
    if (!loopFailed && options.missionNaturalCombatSmoke) {
      const bool missionEjectionReady =
          RecruitCenterSubjectState_EjectPlayerForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.1),
              mission.centerName);
      log.Line(std::string("mission_natural_ejection=") +
               (missionEjectionReady ? "1" : "0"));
      if (!missionEjectionReady)
        log.Line(std::string("mission_natural_ejection_error=") +
                 RecruitCenterSubjectState_LastError());
      loopFailed = !missionEjectionReady || !runCompleteFrame();
    }
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
    const bool loadedProgressionMission =
        options.missionNoRewardResultSmoke &&
        options.startupLoadSlot >= 0 && options.startupSaveSlot >= 0;
    if (loadedProgressionMission) {
      // A progression save deliberately retains the previous mission's
      // authored population.  The clean-admission guide probes select the
      // newest global guide and temporarily mutate it, so running them here
      // would inspect the retired mission instead of the newly admitted one.
      // The chained smoke proves the loaded population through continuation
      // fingerprints and the new mission through its rebound objectives.
      log.Line("mission_smoke_auxiliary_policy=loaded-progression-skip");
    } else {
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
    SPeopleDynamicObstacleProbeSummary guideObstacle = {};
    const bool guideObstacleReady =
        PeopleSubjectState_ProbeDynamicObstacleCollision(
            g_super.m_context, &guideObstacle);
    log.Line(std::string("mission_smoke_guide_obstacle=") +
             (guideObstacle.actor[0] == 0 ? "<none>" :
                                            guideObstacle.actor) + "/" +
             (guideObstacle.obstacle[0] == 0 ? "<none>" :
                                               guideObstacle.obstacle) + "/" +
             std::to_string(guideObstacle.available) + "/" +
             std::to_string(guideObstacle.collisionHit) + "/" +
             std::to_string(guideObstacle.ownerExact) + "/" +
             std::to_string(guideObstacle.contactCode) + "/" +
             std::to_string(guideObstacle.approachingAvoided) + "/" +
             std::to_string(guideObstacle.aheadIgnored) + "/" +
             std::to_string(guideObstacle.rollbackExact) + "/" +
             std::to_string(guideObstacle.collisionTime));
    loopFailed = loopFailed || !guideObstacleReady;
    SPeopleOccupiedVehicleObstacleProbeSummary guideVehicleObstacle = {};
    const bool guideVehicleObstacleReady =
        PeopleSubjectState_ProbeOccupiedVehicleObstacleCollision(
            g_super.m_context, &guideVehicleObstacle);
    log.Line(std::string("mission_smoke_guide_vehicle_obstacle=") +
             (guideVehicleObstacle.actor[0] == 0 ? "<none>" :
                                                   guideVehicleObstacle.actor) +
             "/" +
             (guideVehicleObstacle.vehicle[0] == 0 ? "<none>" :
                                                     guideVehicleObstacle.vehicle) +
             "/" + std::to_string(guideVehicleObstacle.available) +
             "/" + std::to_string(guideVehicleObstacle.playerBound) +
             "/" + std::to_string(guideVehicleObstacle.collisionHit) +
             "/" + std::to_string(guideVehicleObstacle.ownerExact) +
             "/" + std::to_string(guideVehicleObstacle.contactCode) +
             "/" + std::to_string(
                 guideVehicleObstacle.approachingAvoided) +
             "/" + std::to_string(guideVehicleObstacle.aheadIgnored) +
             "/" + std::to_string(
                 guideVehicleObstacle.vehicleStateRestored) +
             "/" + std::to_string(
                 guideVehicleObstacle.playerBindingRestored) +
             "/" + std::to_string(guideVehicleObstacle.rollbackExact) +
             "/" + std::to_string(guideVehicleObstacle.collisionTime));
    loopFailed = loopFailed || !guideVehicleObstacleReady;
    if (!loopFailed && options.missionGuideRouteSmoke) {
      std::vector<std::uint8_t> guideBaseline;
      std::vector<std::uint8_t> guideProgress;
      std::vector<std::uint8_t> guideProgressVerifiedBytes;
      std::vector<std::uint8_t> guideRolledBack;
      SLevelContinuationSummary guideBaselineSummary;
      SLevelContinuationSummary guideProgressSummary;
      SLevelContinuationSummary guideRestoredSummary;
      SLevelContinuationSummary guideProgressVerified;
      SLevelContinuationSummary guideRollbackSummary;
      SLevelContinuationSummary guideRollbackVerified;
      const bool guideBaselineReady =
          RecoveredGameServices_CaptureLevelContinuation(
              &guideBaseline, &guideBaselineSummary);
      SPeopleGuideRouteProbeSummary guideRoute = {};
      const bool guideRouteStaged = guideBaselineReady &&
          PeopleSubjectState_StageGuideRoute(
              g_super.m_context, &guideRoute) &&
          guideRoute.available == 1;
      const bool guideProgressReady = guideRouteStaged &&
          RecoveredGameServices_CaptureLevelContinuation(
              &guideProgress, &guideProgressSummary);
      const bool guideProgressRestored = guideProgressReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              guideProgress, &guideRestoredSummary);
      const bool guideProgressRecaptureReady = guideProgressRestored &&
          RecoveredGameServices_CaptureLevelContinuation(
              &guideProgressVerifiedBytes, &guideProgressVerified);
      const bool guideProgressExact = guideProgressRecaptureReady &&
          guideProgress == guideProgressVerifiedBytes &&
          guideProgressSummary.ready && guideRestoredSummary.ready &&
          guideProgressVerified.ready &&
          guideProgressSummary.sections == kActiveWorldOwnerSectionCount &&
          guideRestoredSummary.ownerPhases ==
              kActiveWorldOwnerSectionCount &&
          guideRestoredSummary.referencePhases ==
              kActiveWorldOwnerSectionCount &&
          guideProgressSummary.worldFingerprint ==
              guideRestoredSummary.restoredWorldFingerprint &&
          guideProgressSummary.worldFingerprint ==
              guideProgressVerified.worldFingerprint;
      const bool guideRollbackRestored = guideBaselineReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              guideBaseline, &guideRollbackSummary);
      const bool guideRollbackRecaptured = guideRollbackRestored &&
          RecoveredGameServices_CaptureLevelContinuation(
              &guideRolledBack, &guideRollbackVerified);
      const bool guideRollbackExact = guideRollbackRecaptured &&
          guideBaseline == guideRolledBack &&
          guideBaselineSummary.ready && guideRollbackSummary.ready &&
          guideRollbackVerified.ready &&
          guideBaselineSummary.worldFingerprint ==
              guideRollbackSummary.restoredWorldFingerprint &&
          guideBaselineSummary.worldFingerprint ==
              guideRollbackVerified.worldFingerprint;
      log.Line(std::string("mission_guide_route=") +
               (guideRoute.actor[0] == 0 ? "<none>" : guideRoute.actor) +
               "/" +
               (guideRoute.route[0] == 0 ? "<none>" : guideRoute.route) +
               "/" +
               (guideRoute.vehicle[0] == 0 ? "<none>" :
                                             guideRoute.vehicle) +
               "/" + std::to_string(guideRoute.available) +
               "/" + std::to_string(guideRoute.playerBound) +
               "/" + std::to_string(guideRoute.visible) +
               "/" + std::to_string(guideRoute.routeNodes) +
               "/" + std::to_string(guideRoute.startNode) +
               "/" + std::to_string(guideRoute.terminalNode) +
               "/" + std::to_string(guideRoute.moveEvents) +
               "/" + std::to_string(guideRoute.displacedEvents) +
               "/" + std::to_string(guideRoute.segmentTransitions) +
               "/" + std::to_string(guideRoute.staticSceneFrames) +
               "/" + std::to_string(guideRoute.staticContactFrames) +
               "/" + std::to_string(guideRoute.contactFrames) +
               "/" + std::to_string(guideRoute.finiteMotion) +
               "/" + std::to_string(guideRoute.boundedMotion) +
               "/" + std::to_string(guideRoute.terminalReached) +
               "/" + std::to_string(guideRoute.failureCode) +
               "/" + std::to_string(guideRoute.lastEventLabel) +
               "/" + std::to_string(guideRoute.endingPreviousNode) +
               "/" + std::to_string(guideRoute.endingCurrentNode) +
               "/" + std::to_string(guideRoute.contactCode1Frames) +
               "/" + std::to_string(guideRoute.contactCode2Frames) +
               "/" + std::to_string(guideRoute.contactCode3Frames) +
               "/" + std::to_string(guideRoute.contactCode9Frames) +
               "/" + std::to_string(guideRoute.contactCode11Frames) +
               "/" + std::to_string(guideRoute.elapsed) +
               "/" + std::to_string(guideRoute.authoredDistance) +
               "/" + std::to_string(guideRoute.travelledDistance) +
               "/" +
               std::to_string(guideRoute.closestTerminalDistance) +
               "/" + std::to_string(guideRoute.closestVehicleDistance) +
               "/" + std::to_string(guideRoute.endingX) +
               "/" + std::to_string(guideRoute.endingZ) +
               "/" + std::to_string(guideRoute.routeStartTime) +
               "/" + std::to_string(guideRoute.lastEventTime));
      log.Line("mission_guide_route_save=" +
               std::to_string(guideBaselineReady ? 1 : 0) + "/" +
               std::to_string(guideRouteStaged ? 1 : 0) + "/" +
               std::to_string(guideProgressReady ? 1 : 0) + "/" +
               std::to_string(guideProgressRestored ? 1 : 0) + "/" +
               std::to_string(guideProgressRecaptureReady ? 1 : 0) + "/" +
               std::to_string(guideProgressExact ? 1 : 0) + "/" +
               std::to_string(guideRollbackRestored ? 1 : 0) + "/" +
               std::to_string(guideRollbackRecaptured ? 1 : 0) + "/" +
               std::to_string(guideRollbackExact ? 1 : 0) + "/" +
               std::to_string(guideProgressSummary.sections));
      if (!guideRouteStaged)
        log.Line("mission_guide_route_error=route staging failed");
      else if (!guideProgressExact || !guideRollbackExact)
        log.Line(std::string("mission_guide_route_error=") +
                 RecoveredGameServices_LastLevelContinuationError());
      loopFailed = !guideRouteStaged || !guideProgressExact ||
                   !guideRollbackExact;
    }
    SPeopleCombatScheduleSummary missionPeopleSchedule = {};
    const bool missionPeopleScheduleReady =
        PeopleSubjectState_AuditCombatScheduling(
            g_super.m_context, &missionPeopleSchedule);
    log.Line("mission_smoke_people_schedule=" +
             std::to_string(missionPeopleScheduleReady ? 1 : 0) + "/" +
             std::to_string(missionPeopleSchedule.livePeople) + "/" +
             std::to_string(missionPeopleSchedule.shooters) + "/" +
             std::to_string(missionPeopleSchedule.commandedShooters) + "/" +
             std::to_string(missionPeopleSchedule.commanderInterfaces) +
             "/" +
             std::to_string(missionPeopleSchedule.scheduledFindEnemy) +
             "/" +
             std::to_string(missionPeopleSchedule.scheduledMotion) + "/" +
              std::to_string(missionPeopleSchedule.attackStates) + "/" +
              std::to_string(missionPeopleSchedule.malformedQueues));
    loopFailed = loopFailed || !missionPeopleScheduleReady;
    }
    const std::string missionProject = mission.projectName;
    if (!loopFailed && options.missionNaturalCombatSmoke) {
      std::vector<std::uint8_t> naturalCheckpoint;
      std::vector<std::uint8_t> naturalRecaptured;
      SLevelContinuationSummary naturalCaptured;
      SLevelContinuationSummary naturalRestored;
      SLevelContinuationSummary naturalVerified;
      const bool naturalCaptureReady = preMissionPeopleReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &naturalCheckpoint, &naturalCaptured);
      std::vector<SPeopleNaturalCombatSummary> naturalCohort;
      const bool naturalSelectionReady = naturalCaptureReady &&
          PeopleSubjectState_SelectNaturalMissionCombatCohort(
              g_super.m_context, preMissionPeople, &naturalCohort);
      struct NaturalCombatActorProbe {
        SPeopleNaturalCombatSummary selection;
        SPeopleNaturalCombatSummary live;
        BulletRuntimeTelemetry bulletBaseline;
        BulletRuntimeTelemetry bullets;
        bool target;
        bool attack;
        bool moved;
        bool shot;
        bool projectile;
        bool collision;
        bool dynamicImpact;
        double maximumDisplacement;
      };
      std::vector<NaturalCombatActorProbe> naturalActors;
      bool naturalBulletBaselineReady = naturalSelectionReady;
      bool naturalAllInitiallyVisible = true;
      for (std::size_t index = 0; naturalBulletBaselineReady &&
           index < naturalCohort.size(); ++index) {
        NaturalCombatActorProbe probe = {};
        probe.selection = naturalCohort[index];
        probe.live = naturalCohort[index];
        naturalAllInitiallyVisible = naturalAllInitiallyVisible &&
            probe.selection.visible != 0;
        naturalBulletBaselineReady =
            BulletSubjectState_OwnerRuntimeTelemetry(
                g_super.m_context, probe.selection.actor,
                &probe.bulletBaseline);
        probe.bullets = probe.bulletBaseline;
        naturalActors.push_back(probe);
      }
      SPeopleNaturalCombatSummary naturalSelection = {};
      if (!naturalCohort.empty()) naturalSelection = naturalCohort[0];
      PeopleSubjectState_ResetLiveCombatTelemetry();
      const bool naturalSampleReady = naturalBulletBaselineReady &&
          PeopleSubjectState_SampleLiveCombat(g_super.m_context);
      log.Line("mission_natural_stage=" +
               std::to_string(naturalCaptureReady ? 1 : 0) + "/" +
               std::to_string(naturalSelectionReady ? 1 : 0) + "/" +
               std::to_string(naturalSelection.baselinePeople) + "/" +
               std::to_string(naturalSelection.livePeople) + "/" +
               std::to_string(naturalSelection.missionPeople) + "/" +
               std::to_string(naturalSelection.missionShooters));
      log.Line("mission_natural_cohort=" +
               std::to_string(naturalActors.size()) + "/" +
               std::to_string(naturalAllInitiallyVisible ? 1 : 0));
      log.Line(std::string("mission_natural_seed=") +
               (naturalSelection.actor[0] == 0
                    ? "<none>" : naturalSelection.actor) + "/" +
               (naturalSelection.commander[0] == 0
                    ? "<none>" : naturalSelection.commander) + "/" +
               (naturalSelection.attribute[0] == 0
                    ? "<none>" : naturalSelection.attribute) + "/" +
               (naturalSelection.route[0] == 0
                    ? "<none>" : naturalSelection.route));
      log.Line("mission_natural_origin=" +
               std::to_string(naturalSelection.initialX) + "/" +
               std::to_string(naturalSelection.initialY) + "/" +
               std::to_string(naturalSelection.initialZ) + "/" +
               std::to_string(naturalSelection.initialShootTime));

      SPeopleLiveCombatTelemetry naturalPeople = {};
      int naturalFrames = 0;
      bool naturalProof = false;
      std::size_t naturalProofActor = 0;
      const double naturalMomentStart = Session::m_moment;
      const ULONGLONG naturalWallBudget =
          naturalAllInitiallyVisible ? 20000 : 165000;
      const ULONGLONG naturalWallStart = GetTickCount64();
      while (naturalSampleReady && !loopFailed && naturalFrames < 20000 &&
             !naturalProof &&
             GetTickCount64() - naturalWallStart < naturalWallBudget) {
        Sleep(5);
        ++naturalFrames;
        if (!runCompleteFrame() ||
            !PeopleSubjectState_SampleLiveCombat(g_super.m_context) ||
            !PeopleSubjectState_LiveCombatTelemetry(&naturalPeople)) {
          loopFailed = true;
          break;
        }
        for (std::size_t index = 0; index < naturalActors.size(); ++index) {
          NaturalCombatActorProbe& probe = naturalActors[index];
          if (!PeopleSubjectState_InspectNaturalMissionCombat(
                  g_super.m_context, &probe.selection, &probe.live) ||
              !BulletSubjectState_OwnerRuntimeTelemetry(
                  g_super.m_context, probe.selection.actor,
                  &probe.bullets)) {
            loopFailed = true;
            break;
          }
          probe.target = probe.target ||
              (probe.live.hasTarget != 0 &&
               probe.live.targetIsDynamic != 0);
          probe.attack = probe.attack || probe.live.attackState != 0;
          probe.maximumDisplacement = (std::max)(
              probe.maximumDisplacement,
              probe.live.horizontalDisplacement);
          probe.moved = probe.moved ||
              probe.live.horizontalDisplacement > 0.05;
          probe.shot = probe.shot || probe.live.shot != 0;
          probe.projectile = probe.projectile ||
              probe.bullets.acceptedStarts >
                  probe.bulletBaseline.acceptedStarts;
          probe.collision = probe.collision ||
              probe.bullets.collisionChecks >
                  probe.bulletBaseline.collisionChecks;
          probe.dynamicImpact = probe.dynamicImpact ||
              probe.bullets.dynamicImpacts >
                  probe.bulletBaseline.dynamicImpacts;
          if (probe.target && probe.attack && probe.moved && probe.shot &&
              probe.projectile && probe.collision && probe.dynamicImpact) {
            naturalProof = true;
            naturalProofActor = index;
            break;
          }
        }
      }
      if (!naturalProof && !naturalActors.empty()) {
        int bestScore = -1;
        for (std::size_t index = 0; index < naturalActors.size(); ++index) {
          const NaturalCombatActorProbe& probe = naturalActors[index];
          const int score = static_cast<int>(probe.target) +
              static_cast<int>(probe.attack) +
              static_cast<int>(probe.moved) +
              static_cast<int>(probe.shot) +
              static_cast<int>(probe.projectile) +
              static_cast<int>(probe.collision) +
              static_cast<int>(probe.dynamicImpact);
          if (score > bestScore) {
            bestScore = score;
            naturalProofActor = index;
          }
        }
      }
      NaturalCombatActorProbe naturalResult = {};
      if (!naturalActors.empty())
        naturalResult = naturalActors[naturalProofActor];
      const SPeopleNaturalCombatSummary& naturalLive = naturalResult.live;
      const BulletRuntimeTelemetry& naturalBulletBaseline =
          naturalResult.bulletBaseline;
      const BulletRuntimeTelemetry& naturalBullets = naturalResult.bullets;
      const bool naturalTarget = naturalResult.target;
      const bool naturalAttack = naturalResult.attack;
      const bool naturalMoved = naturalResult.moved;
      const bool naturalShot = naturalResult.shot;
      const bool naturalProjectile = naturalResult.projectile;
      const bool naturalCollision = naturalResult.collision;
      const bool naturalDynamicImpact = naturalResult.dynamicImpact;
      const double naturalMaximumDisplacement =
          naturalResult.maximumDisplacement;
      log.Line(std::string("mission_natural_actor=") +
               (naturalLive.actor[0] == 0 ? "<none>" : naturalLive.actor) +
               "/" +
               (naturalLive.commander[0] == 0
                    ? "<none>" : naturalLive.commander) + "/" +
               (naturalLive.attribute[0] == 0
                    ? "<none>" : naturalLive.attribute) + "/" +
               (naturalLive.route[0] == 0
                    ? "<none>" : naturalLive.route));
      log.Line("mission_natural_live=" +
               std::to_string(naturalFrames) + "/" +
               std::to_string(GetTickCount64() - naturalWallStart) + "/" +
               std::to_string(naturalPeople.sampleFrames) + "/" +
               std::to_string(naturalTarget ? 1 : 0) + "/" +
               std::to_string(naturalAttack ? 1 : 0) + "/" +
               std::to_string(naturalMoved ? 1 : 0) + "/" +
               std::to_string(naturalShot ? 1 : 0) + "/" +
               std::to_string(naturalProjectile ? 1 : 0) + "/" +
               std::to_string(naturalCollision ? 1 : 0) + "/" +
               std::to_string(naturalDynamicImpact ? 1 : 0));
      log.Line("mission_natural_time=" +
               std::to_string(naturalMomentStart) + "/" +
               std::to_string(Session::m_moment) + "/" +
               std::to_string(naturalWallBudget));
      log.Line(std::string("mission_natural_target=") +
               (naturalLive.target[0] == 0
                    ? "<none>" : naturalLive.target) + "/" +
               std::to_string(naturalLive.targetDistance) + "/" +
               std::to_string(naturalLive.currentX) + "/" +
               std::to_string(naturalLive.currentY) + "/" +
               std::to_string(naturalLive.currentZ) + "/" +
               std::to_string(naturalMaximumDisplacement));
      log.Line("mission_natural_people=" +
               std::to_string(naturalPeople.moveEvents) + "/" +
               std::to_string(naturalPeople.attackMoveEvents) + "/" +
               std::to_string(naturalPeople.findEvents) + "/" +
               std::to_string(naturalPeople.targetAcquisitions) + "/" +
               std::to_string(naturalPeople.shotsStarted) + "/" +
               std::to_string(
                   naturalPeople.maximumHorizontalDisplacement));
      log.Line(std::string("mission_natural_people_actors=") +
               (naturalPeople.lastAcquiringOwner[0] == 0
                    ? "<none>" : naturalPeople.lastAcquiringOwner) + "/" +
               (naturalPeople.lastShootingOwner[0] == 0
                    ? "<none>" : naturalPeople.lastShootingOwner) + "/" +
               (naturalPeople.lastDamagedOwner[0] == 0
                    ? "<none>" : naturalPeople.lastDamagedOwner));
      log.Line("mission_natural_bullets=" +
               std::to_string(naturalBullets.acceptedStarts -
                              naturalBulletBaseline.acceptedStarts) + "/" +
               std::to_string(naturalBullets.moveEvents -
                              naturalBulletBaseline.moveEvents) + "/" +
               std::to_string(naturalBullets.collisionChecks -
                              naturalBulletBaseline.collisionChecks) + "/" +
               std::to_string(naturalBullets.sceneImpacts -
                              naturalBulletBaseline.sceneImpacts) + "/" +
               std::to_string(naturalBullets.dynamicImpacts -
                              naturalBulletBaseline.dynamicImpacts));

      const bool naturalRestoreReady = naturalCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              naturalCheckpoint, &naturalRestored);
      const bool naturalRecaptureReady = naturalRestoreReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &naturalRecaptured, &naturalVerified);
      const bool naturalRollbackExact = naturalRecaptureReady &&
          naturalCheckpoint == naturalRecaptured && naturalCaptured.ready &&
          naturalRestored.ready && naturalVerified.ready &&
          naturalCaptured.worldFingerprint ==
              naturalRestored.restoredWorldFingerprint &&
          naturalCaptured.worldFingerprint ==
              naturalVerified.worldFingerprint;
      log.Line("mission_natural_rollback=" +
               std::to_string(naturalRestoreReady ? 1 : 0) + "/" +
               std::to_string(naturalRecaptureReady ? 1 : 0) + "/" +
               std::to_string(naturalRollbackExact ? 1 : 0));
      if (!naturalRollbackExact)
        log.Line(std::string("mission_natural_rollback_error=") +
                 RecoveredGameServices_LastLevelContinuationError());
      loopFailed = loopFailed || !naturalSelectionReady ||
                   !naturalSampleReady || !naturalProof ||
                   !naturalRollbackExact;
    }
    if (!loopFailed && options.missionCombatSmoke) {
      std::vector<std::uint8_t> combatCheckpoint;
      std::vector<std::uint8_t> combatRecaptured;
      SLevelContinuationSummary combatCaptured;
      SLevelContinuationSummary combatRestored;
      SLevelContinuationSummary combatVerified;
      const bool combatCaptureReady = preMissionPeopleReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &combatCheckpoint, &combatCaptured);
      SPeopleMissionCombatStageSummary combatStage = {};
      PeopleSubjectState_ResetLiveCombatTelemetry();
      const double combatTime =
          (std::max)(0.1, Session::m_moment + 0.1);
      const bool combatStageReady = combatCaptureReady &&
          PeopleSubjectState_StageMissionCombat(
              g_super.m_context, preMissionPeople, combatTime, &combatStage) &&
          PeopleSubjectState_SampleLiveCombat(g_super.m_context);
      log.Line("mission_combat_stage=" +
               std::to_string(combatCaptureReady ? 1 : 0) + "/" +
               std::to_string(combatStageReady ? 1 : 0) + "/" +
               std::to_string(combatStage.baselinePeople) + "/" +
               std::to_string(combatStage.livePeople) + "/" +
               std::to_string(combatStage.missionPeople) + "/" +
               std::to_string(combatStage.hostilePairs));
      log.Line(std::string("mission_combat_pair=") +
               (combatStage.attacker[0] == 0 ? "<none>"
                                             : combatStage.attacker) + "/" +
               (combatStage.target[0] == 0 ? "<none>"
                                           : combatStage.target) + "/" +
               (combatStage.attackerCommander[0] == 0
                    ? "<none>" : combatStage.attackerCommander) + "/" +
               (combatStage.targetCommander[0] == 0
                    ? "<none>" : combatStage.targetCommander));
      log.Line("mission_combat_geometry=" +
               std::to_string(combatStage.attackerOnLand) + "/" +
               std::to_string(combatStage.targetOnLand) + "/" +
               std::to_string(combatStage.separation) + "/" +
               std::to_string(combatStage.targetDamageBefore) + "/" +
               std::to_string(combatStage.targetDamageStaged) + "/" +
               std::to_string(combatStage.attackerViewDistanceBefore) + "/" +
               std::to_string(combatStage.attackerViewDistanceStaged));

      SPeopleLiveCombatTelemetry combatLive = {};
      SPeopleMissionCombatLiveState combatPairLive = {};
      int combatFrames = 0;
      bool combatProof = false;
      bool combatExactTarget = false;
      bool combatAttackerShot = false;
      bool combatTargetDamaged = false;
      bool combatTargetKilled = false;
      bool combatDamageSource = false;
      bool combatDeathScheduled = false;
      const ULONGLONG combatWallStart = GetTickCount64();
      while (combatStageReady && !loopFailed && combatFrames < 1200 &&
             !combatProof &&
             GetTickCount64() - combatWallStart < 15000) {
        Sleep(5);
        ++combatFrames;
        if (!runCompleteFrame() ||
            !PeopleSubjectState_LiveCombatTelemetry(&combatLive) ||
            !PeopleSubjectState_InspectMissionCombat(
                g_super.m_context, &combatStage, &combatPairLive)) {
          loopFailed = true;
          break;
        }
        combatExactTarget = combatExactTarget ||
            combatPairLive.attackerHasExactTarget != 0;
        combatAttackerShot = combatAttackerShot ||
            combatPairLive.attackerShot != 0;
        combatTargetDamaged = combatTargetDamaged ||
            (combatPairLive.targetExists != 0 &&
             combatPairLive.targetDamage <
                 combatStage.targetDamageStaged - 1e-9);
        combatTargetKilled = combatTargetKilled ||
            combatPairLive.targetKilled != 0;
        combatDamageSource = combatDamageSource ||
            combatPairLive.targetDamageSourceAttacker != 0;
        if (combatTargetKilled && combatDamageSource &&
            !combatDeathScheduled) {
          combatDeathScheduled =
              PeopleSubjectState_ScheduleMissionCombatDeath(
                  g_super.m_context, &combatStage,
                  (std::max)(0.1, Session::m_moment + 0.01));
          if (!combatDeathScheduled) {
            loopFailed = true;
            break;
          }
        }
        combatProof = combatLive.moveEvents > 0 &&
            combatLive.findEvents > 0 &&
            combatLive.targetAcquisitions > 0 &&
            combatAttackerShot && combatTargetDamaged &&
            combatTargetKilled && combatDamageSource &&
            combatDeathScheduled &&
            combatLive.damageApplications > 0 &&
            combatLive.killTransitions > 0 &&
            combatLive.explosionEffects > 0 &&
            combatLive.corpseEffects > 0 &&
            std::strcmp(combatLive.lastKilledOwner,
                        combatStage.target) == 0;
      }
      log.Line("mission_combat_live=" +
               std::to_string(combatFrames) + "/" +
               std::to_string(GetTickCount64() - combatWallStart) + "/" +
               std::to_string(combatLive.sampleFrames) + "/" +
               std::to_string(combatLive.moveEvents) + "/" +
               std::to_string(combatLive.findEvents) + "/" +
               std::to_string(combatLive.targetAcquisitions) + "/" +
               std::to_string(combatLive.shotsStarted) + "/" +
               std::to_string(combatLive.damageApplications) + "/" +
               std::to_string(combatLive.killTransitions) + "/" +
               std::to_string(combatLive.explosionEffects) + "/" +
               std::to_string(combatLive.corpseEffects));
      log.Line(std::string("mission_combat_owners=") +
               (combatLive.lastAcquiringOwner[0] == 0
                    ? "<none>" : combatLive.lastAcquiringOwner) + "/" +
               (combatLive.lastShootingOwner[0] == 0
                    ? "<none>" : combatLive.lastShootingOwner) + "/" +
               (combatLive.lastDamagedOwner[0] == 0
                    ? "<none>" : combatLive.lastDamagedOwner) + "/" +
               (combatLive.lastKilledOwner[0] == 0
                    ? "<none>" : combatLive.lastKilledOwner));
      log.Line(std::string("mission_combat_pair_live=") +
               std::to_string(combatExactTarget ? 1 : 0) + "/" +
               std::to_string(combatAttackerShot ? 1 : 0) + "/" +
               std::to_string(combatTargetDamaged ? 1 : 0) + "/" +
               std::to_string(combatTargetKilled ? 1 : 0) + "/" +
               std::to_string(combatDamageSource ? 1 : 0) + "/" +
               std::to_string(combatDeathScheduled ? 1 : 0) + "/" +
               std::to_string(combatPairLive.attackerExists) + "/" +
               std::to_string(combatPairLive.targetExists) + "/" +
               std::to_string(combatPairLive.targetDamage) + "/" +
               std::to_string(combatPairLive.bullets) + "/" +
               std::to_string(combatPairLive.explosions) + "/" +
               std::to_string(combatPairLive.corpses) + "/" +
               (combatPairLive.attackerTarget[0] == 0
                    ? "<none>" : combatPairLive.attackerTarget));

      const bool combatTuningRestored = !combatStageReady ||
          PeopleSubjectState_RestoreMissionCombatTuning(
              g_super.m_context, &combatStage);
      const bool combatRestoreReady = combatCaptureReady &&
          combatTuningRestored &&
          RecoveredGameServices_RestoreLevelContinuation(
              combatCheckpoint, &combatRestored);
      const bool combatRecaptureReady = combatRestoreReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &combatRecaptured, &combatVerified);
      const bool combatRollbackExact = combatRecaptureReady &&
          combatCheckpoint == combatRecaptured && combatCaptured.ready &&
          combatRestored.ready && combatVerified.ready &&
          combatCaptured.worldFingerprint ==
              combatRestored.restoredWorldFingerprint &&
          combatCaptured.worldFingerprint ==
              combatVerified.worldFingerprint;
      log.Line("mission_combat_rollback=" +
               std::to_string(combatTuningRestored ? 1 : 0) + "/" +
               std::to_string(combatRestoreReady ? 1 : 0) + "/" +
               std::to_string(combatRecaptureReady ? 1 : 0) + "/" +
               std::to_string(combatRollbackExact ? 1 : 0));
      if (!combatRollbackExact)
        log.Line(std::string("mission_combat_rollback_error=") +
                 RecoveredGameServices_LastLevelContinuationError());
      loopFailed = loopFailed || !combatStageReady || !combatProof ||
                   !combatRollbackExact;
    }
    if (!loopFailed && !options.missionGuideRouteSmoke &&
        (missionProject == "ProjectS22" ||
         missionProject == "ProjectS23" ||
         missionProject == "ProjectS24" ||
         missionProject == "ProjectS25")) {
      SRecoveredMissionVehicleDriveProbe missionVehicle = {};
      const bool missionVehicleReady = preMissionTaxisReady &&
          RecoveredGameServices_ProbeMissionTaxiForwardTravel(
              "Taxi.Obj", preMissionTaxis, &missionVehicle);
      log.Line("mission_smoke_pre_taxis=" +
               std::to_string(preMissionTaxis.size()));
      log.Line("mission_smoke_vehicle_drive=" +
               std::to_string(missionVehicle.availableTaxis) + "/" +
               std::to_string(missionVehicle.transitionedTaxis) + "/" +
               std::to_string(missionVehicle.panelReadyTaxis) + "/" +
               std::to_string(missionVehicle.panelOpenTaxis) + "/" +
               std::to_string(missionVehicle.alignedTaxis) + "/" +
               std::to_string(missionVehicle.movementFrames) + "/" +
               std::to_string(
                   missionVehicle.minimumHorizontalDistance) + "/" +
               std::to_string(missionVehicle.minimumForwardTravel) + "/" +
               std::to_string(missionVehicle.maximumLateralTravel) + "/" +
               std::to_string(missionVehicle.maximumLateralRatio) + "/" +
               std::to_string(missionVehicle.rollbackRestores) + "/" +
               std::to_string(missionVehicle.exactRollbacks));
      loopFailed = !missionVehicleReady;
    }
    if (!loopFailed && options.missionObjectiveChainSmoke) {
      const char *secondCenter = "Marauders.Recruit.0";
      bool secondStaged = false;
      RecruitCenterMissionProbeSummary secondMission = {};
      const bool secondReady =
          std::strcmp(mission.centerName, "Inhabitants.Recruit.0") == 0 &&
          RecruitCenterSubjectState_StageMissionExecutionProbeForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_viewTime + 0.5), secondCenter,
              &secondStaged, &secondMission) &&
          secondStaged && runCompleteFrame();

      RecruitCenterObjectiveStateSummary multiState = {};
      const bool multiStateReady = secondReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &multiState);
      const bool multiStateExact = multiStateReady &&
          multiState.missions == objectiveBaselineState.missions + 2 &&
          multiState.totalMissions ==
              objectiveBaselineState.totalMissions + 2 &&
          multiState.inProcessMissions == multiState.missions &&
          multiState.successMissions == 0 &&
          multiState.failedMissions == 0 &&
          multiState.surrenderMissions == 0 &&
          multiState.summaryMissions == 2 &&
          multiState.routeMissions == 2 &&
          multiState.conditionReferences > 0 &&
          multiState.boundConditionReferences ==
              multiState.conditionReferences &&
          multiState.scheduledChecks ==
              objectiveBaselineState.scheduledChecks + 2 &&
          multiState.distinctProjects == 2 &&
          multiState.distinctCommanders == 2 &&
          multiState.mapMissions == 2 && multiState.mapTexts == 2 &&
          multiState.mapRoutes == 2 && multiState.mapBindings == 2 &&
          std::strcmp(multiState.firstProjectName, "ProjectS22") == 0 &&
          secondMission.projectName[0] != 0 &&
          std::strcmp(multiState.secondProjectName,
                      secondMission.projectName) == 0 &&
          std::strcmp(multiState.firstProjectName,
                      multiState.secondProjectName) != 0;

      std::vector<std::uint8_t> multiCheckpoint;
      std::vector<std::uint8_t> multiRecaptured;
      SLevelContinuationSummary multiCaptured;
      SLevelContinuationSummary multiRestored;
      SLevelContinuationSummary multiVerified;
      RecruitCenterObjectiveStateSummary multiRestoredState = {};
      const bool multiCaptureReady = multiStateExact &&
          RecoveredGameServices_CaptureLevelContinuation(
              &multiCheckpoint, &multiCaptured);
      const bool multiRestoreReady = multiCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              multiCheckpoint, &multiRestored);
      const bool multiRestoredStateReady = multiRestoreReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &multiRestoredState);
      const bool multiRecaptureReady = multiRestoredStateReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &multiRecaptured, &multiVerified);
      const bool multiSaveExact = multiRecaptureReady &&
          multiCheckpoint == multiRecaptured &&
          std::memcmp(&multiState, &multiRestoredState,
                      sizeof(multiState)) == 0 &&
          multiCaptured.ready && multiRestored.ready &&
          multiVerified.ready &&
          multiCaptured.sections == kActiveWorldOwnerSectionCount &&
          multiRestored.ownerPhases == kActiveWorldOwnerSectionCount &&
          multiRestored.referencePhases == kActiveWorldOwnerSectionCount &&
          multiCaptured.worldFingerprint ==
              multiRestored.restoredWorldFingerprint &&
          multiCaptured.worldFingerprint == multiVerified.worldFingerprint;

      SRecoveredDebugMapControlProbeTelemetry mapControls = {};
      const bool mapOpened = multiSaveExact &&
          RecoveredGameServices_RequestDebugMapToggle() &&
          runCompleteFrame();
      const bool mapControlled = mapOpened &&
          RecoveredGameServices_ProbeDebugMapControls() &&
          RecoveredGameServices_DebugMapControlProbeTelemetry(&mapControls) &&
          mapControls.available == 1 &&
          mapControls.missionSelectable == 1 &&
          mapControls.missionSelectionPair == 1 &&
          mapControls.stateRestored == 1;
      const bool mapClosed = mapControlled &&
          RecoveredGameServices_RequestDebugMapToggle() &&
          runCompleteFrame() &&
          !RecoveredGameServices_DebugMapActive();

      RecruitCenterMissionResultProbeSummary firstResult = {};
      const bool firstCompleted = mapClosed &&
          RecruitCenterSubjectState_CompleteMissionProbeForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.1),
              mission.centerName, &firstResult);
      RecruitCenterObjectiveStateSummary remainingState = {};
      const bool remainingReady = firstCompleted &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &remainingState);
      const bool remainingExact = remainingReady &&
          remainingState.missions == objectiveBaselineState.missions + 1 &&
          remainingState.totalMissions ==
              objectiveBaselineState.totalMissions + 2 &&
          remainingState.inProcessMissions == remainingState.missions &&
          remainingState.successMissions == 0 &&
          remainingState.failedMissions == 0 &&
          remainingState.surrenderMissions == 0 &&
          remainingState.summaryMissions == 1 &&
          remainingState.routeMissions == 1 &&
          remainingState.conditionReferences > 0 &&
          remainingState.boundConditionReferences ==
              remainingState.conditionReferences &&
          remainingState.scheduledChecks ==
              objectiveBaselineState.scheduledChecks + 1 &&
          remainingState.distinctProjects == 1 &&
          remainingState.distinctCommanders == 1 &&
          remainingState.mapMissions == 1 && remainingState.mapTexts == 1 &&
          remainingState.mapRoutes == 1 && remainingState.mapBindings == 1 &&
          std::strcmp(remainingState.firstProjectName,
                      secondMission.projectName) == 0;

      std::vector<std::uint8_t> remainingCheckpoint;
      std::vector<std::uint8_t> remainingRecaptured;
      SLevelContinuationSummary remainingCaptured;
      SLevelContinuationSummary remainingRestored;
      SLevelContinuationSummary remainingVerified;
      RecruitCenterObjectiveStateSummary remainingRestoredState = {};
      const bool remainingCaptureReady = remainingExact &&
          RecoveredGameServices_CaptureLevelContinuation(
              &remainingCheckpoint, &remainingCaptured);
      const bool remainingRestoreReady = remainingCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              remainingCheckpoint, &remainingRestored) &&
          RecruitCenterSubjectState_RewardCarrierState(
              g_super.m_context, true);
      const bool remainingRestoredStateReady = remainingRestoreReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &remainingRestoredState);
      const bool remainingRecaptureReady = remainingRestoredStateReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &remainingRecaptured, &remainingVerified);
      const bool remainingSaveExact = remainingRecaptureReady &&
          remainingCheckpoint == remainingRecaptured &&
          std::memcmp(&remainingState, &remainingRestoredState,
                      sizeof(remainingState)) == 0 &&
          remainingCaptured.ready && remainingRestored.ready &&
          remainingVerified.ready &&
          remainingCaptured.worldFingerprint ==
              remainingRestored.restoredWorldFingerprint &&
          remainingCaptured.worldFingerprint ==
              remainingVerified.worldFingerprint;

      std::vector<std::uint8_t> multiRollbackBytes;
      SLevelContinuationSummary multiRollbackRestored;
      SLevelContinuationSummary multiRollbackVerified;
      RecruitCenterObjectiveStateSummary multiRollbackState = {};
      const bool multiRollbackReady = remainingSaveExact &&
          RecoveredGameServices_RestoreLevelContinuation(
              multiCheckpoint, &multiRollbackRestored) &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &multiRollbackState) &&
          RecoveredGameServices_CaptureLevelContinuation(
              &multiRollbackBytes, &multiRollbackVerified);
      const bool multiRollbackExact = multiRollbackReady &&
          multiRollbackBytes == multiCheckpoint &&
          std::memcmp(&multiState, &multiRollbackState,
                      sizeof(multiState)) == 0 &&
          multiCaptured.worldFingerprint ==
              multiRollbackRestored.restoredWorldFingerprint &&
          multiCaptured.worldFingerprint ==
              multiRollbackVerified.worldFingerprint;

      std::vector<std::uint8_t> baselineRollbackBytes;
      SLevelContinuationSummary baselineRollbackRestored;
      SLevelContinuationSummary baselineRollbackVerified;
      RecruitCenterObjectiveStateSummary baselineRollbackState = {};
      // Cleanup is unconditional once the baseline exists. A failed proof may
      // leave the result Artifact attached; abandoning that graph would turn a
      // useful assertion failure into a shutdown access violation.
      const bool baselineRollbackReady = objectiveBaselineReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              objectiveBaseline, &baselineRollbackRestored) &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &baselineRollbackState) &&
          RecoveredGameServices_CaptureLevelContinuation(
              &baselineRollbackBytes, &baselineRollbackVerified);
      const bool baselineRollbackExact = baselineRollbackReady &&
          baselineRollbackBytes == objectiveBaseline &&
          std::memcmp(&objectiveBaselineState, &baselineRollbackState,
                      sizeof(objectiveBaselineState)) == 0 &&
          objectiveBaselineSummary.worldFingerprint ==
              baselineRollbackRestored.restoredWorldFingerprint &&
          objectiveBaselineSummary.worldFingerprint ==
              baselineRollbackVerified.worldFingerprint;

      log.Line(std::string("mission_objective_projects=") +
               (multiState.firstProjectName[0] == 0 ? "<none>" :
                                                       multiState.firstProjectName) +
               "/" +
               (multiState.secondProjectName[0] == 0 ? "<none>" :
                                                        multiState.secondProjectName));
      log.Line("mission_objective_active=" +
               std::to_string(secondReady ? 1 : 0) + "/" +
               std::to_string(multiStateExact ? 1 : 0) + "/" +
               std::to_string(multiState.missions) + "/" +
               std::to_string(multiState.inProcessMissions) + "/" +
               std::to_string(multiState.conditionReferences) + "/" +
               std::to_string(multiState.boundConditionReferences) + "/" +
               std::to_string(multiState.scheduledChecks));
      log.Line("mission_objective_map=" +
               std::to_string(multiState.mapMissions) + "/" +
               std::to_string(multiState.mapTexts) + "/" +
               std::to_string(multiState.mapRoutes) + "/" +
               std::to_string(multiState.mapBindings) + "/" +
               std::to_string(mapControls.missionSelectable) + "/" +
               std::to_string(mapControls.missionSelectionPair) + "/" +
               std::to_string(mapControls.stateRestored) + "/" +
               std::to_string(mapClosed ? 1 : 0));
      log.Line("mission_objective_save=" +
               std::to_string(multiCaptureReady ? 1 : 0) + "/" +
               std::to_string(multiRestoreReady ? 1 : 0) + "/" +
               std::to_string(multiRecaptureReady ? 1 : 0) + "/" +
               std::to_string(multiSaveExact ? 1 : 0));
      log.Line(std::string("mission_objective_remaining=") +
               (remainingState.firstProjectName[0] == 0 ? "<none>" :
                                                           remainingState.firstProjectName) +
               "/" + std::to_string(remainingExact ? 1 : 0) + "/" +
               std::to_string(remainingState.missions) + "/" +
               std::to_string(remainingState.inProcessMissions) + "/" +
               std::to_string(remainingState.scheduledChecks) + "/" +
               std::to_string(remainingState.mapBindings));
      log.Line("mission_objective_remaining_state=" +
               std::to_string(remainingState.totalMissions) + "/" +
               std::to_string(remainingState.successMissions) + "/" +
               std::to_string(remainingState.failedMissions) + "/" +
               std::to_string(remainingState.surrenderMissions) + "/" +
               std::to_string(remainingState.summaryMissions) + "/" +
               std::to_string(remainingState.routeMissions) + "/" +
               std::to_string(remainingState.conditionReferences) + "/" +
               std::to_string(remainingState.boundConditionReferences) + "/" +
               std::to_string(remainingState.distinctProjects) + "/" +
               std::to_string(remainingState.distinctCommanders) + "/" +
               std::to_string(remainingState.mapMissions) + "/" +
               std::to_string(remainingState.mapTexts) + "/" +
               std::to_string(remainingState.mapRoutes));
      log.Line("mission_objective_remaining_save=" +
               std::to_string(remainingCaptureReady ? 1 : 0) + "/" +
               std::to_string(remainingRestoreReady ? 1 : 0) + "/" +
               std::to_string(remainingRecaptureReady ? 1 : 0) + "/" +
               std::to_string(remainingSaveExact ? 1 : 0));
      log.Line("mission_objective_rollback=" +
               std::to_string(multiRollbackReady ? 1 : 0) + "/" +
               std::to_string(multiRollbackExact ? 1 : 0) + "/" +
               std::to_string(baselineRollbackReady ? 1 : 0) + "/" +
               std::to_string(baselineRollbackExact ? 1 : 0));
      if (!secondReady || !multiStateReady || !remainingReady)
        log.Line(std::string("mission_objective_error=") +
                 RecruitCenterSubjectState_LastError());
      else if (!multiSaveExact || !remainingSaveExact ||
               !multiRollbackExact || !baselineRollbackExact)
        log.Line(std::string("mission_objective_error=") +
                 RecoveredGameServices_LastLevelContinuationError());
      loopFailed = !secondReady || !multiStateExact || !multiSaveExact ||
          !mapClosed || !firstCompleted || !remainingExact ||
          !remainingSaveExact || !multiRollbackExact ||
          !baselineRollbackExact;
    }
    if (!loopFailed && options.missionTerminalStateSmoke) {
      const char* firstCenter = "Magician.Recruit.0";
      const char* firstProject = "Project2G03";
      const char* secondCenter = "Kingdom.Recruit.0";
      const char* failureProject = "Project2G07";
      const bool terminalBaselineExact = objectiveBaselineReady &&
          objectiveBaselineState.missions == 0 &&
          objectiveBaselineState.scheduledChecks == 0 &&
          objectiveBaselineState.mapBindings == 0 &&
          std::strcmp(mission.centerName, firstCenter) == 0 &&
          std::strcmp(mission.projectName, firstProject) == 0;

      bool failureStaged = false;
      RecruitCenterMissionProbeSummary failureMission = {};
      const bool failureMissionReady = terminalBaselineExact &&
          RecruitCenterSubjectState_StageMissionExecutionProbeForProject(
              g_super.m_context,
              (std::max)(0.1, Session::m_viewTime + 0.5), secondCenter,
              failureProject, &failureStaged, &failureMission) &&
          failureStaged && runCompleteFrame();
      std::string terminalFailureDetail;
      if (!failureMissionReady) {
        const char* detail = RecruitCenterSubjectState_LastError();
        if (detail != nullptr) terminalFailureDetail = detail;
      }
      RecruitCenterObjectiveStateSummary dualState = {};
      const bool dualStateReady = failureMissionReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &dualState);
      const bool dualStateExact = dualStateReady &&
          dualState.missions == 2 && dualState.totalMissions == 2 &&
          dualState.inProcessMissions == 2 &&
          dualState.successMissions == 0 && dualState.failedMissions == 0 &&
          dualState.surrenderMissions == 0 &&
          dualState.summaryMissions == 2 && dualState.routeMissions == 2 &&
          dualState.conditionReferences > 0 &&
          dualState.boundConditionReferences == dualState.conditionReferences &&
          dualState.scheduledChecks == 2 && dualState.distinctProjects == 2 &&
          dualState.distinctCommanders == 2 &&
          dualState.mapMissions == 2 && dualState.mapBindings == 2 &&
          std::strcmp(dualState.firstProjectName, firstProject) == 0 &&
          std::strcmp(dualState.secondProjectName, failureProject) == 0;

      std::vector<std::uint8_t> dualCheckpoint;
      SLevelContinuationSummary dualCaptured;
      const bool dualCaptureReady = dualStateExact &&
          RecoveredGameServices_CaptureLevelContinuation(
              &dualCheckpoint, &dualCaptured);

      RecruitCenterMissionTerminalProbeSummary failureTransition = {};
      const bool failureTransitionReady = dualCaptureReady &&
          RecruitCenterSubjectState_FailMissionProbeForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.1), secondCenter,
              &failureTransition);
      RecruitCenterObjectiveStateSummary failureState = {};
      const bool failureStateReady = failureTransitionReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &failureState);
      const bool failureStateExact = failureStateReady &&
          failureState.missions == 2 && failureState.totalMissions == 2 &&
          failureState.inProcessMissions == 1 &&
          failureState.successMissions == 0 && failureState.failedMissions == 1 &&
          failureState.surrenderMissions == 0 &&
          failureState.scheduledChecks == 1 &&
          failureState.mapBindings == 2 &&
          std::strcmp(failureState.firstProjectName, firstProject) == 0 &&
          std::strcmp(failureState.secondProjectName, failureProject) == 0;

      std::vector<std::uint8_t> failureCheckpoint;
      std::vector<std::uint8_t> failureRecaptured;
      SLevelContinuationSummary failureCaptured;
      SLevelContinuationSummary failureRestored;
      SLevelContinuationSummary failureVerified;
      RecruitCenterObjectiveStateSummary failureRestoredState = {};
      const bool failureCaptureReady = failureStateExact &&
          RecoveredGameServices_CaptureLevelContinuation(
              &failureCheckpoint, &failureCaptured);
      const bool failureRestoreReady = failureCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              failureCheckpoint, &failureRestored);
      const bool failureRestoredStateReady = failureRestoreReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &failureRestoredState);
      const bool failureRecaptureReady = failureRestoredStateReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &failureRecaptured, &failureVerified);
      const bool failureSaveExact = failureRecaptureReady &&
          failureCheckpoint == failureRecaptured &&
          std::memcmp(&failureState, &failureRestoredState,
                      sizeof(failureState)) == 0 &&
          failureCaptured.ready && failureRestored.ready &&
          failureVerified.ready &&
          failureCaptured.worldFingerprint ==
              failureRestored.restoredWorldFingerprint &&
          failureCaptured.worldFingerprint == failureVerified.worldFingerprint;

      RecruitCenterMissionTerminalProbeSummary failureResult = {};
      const bool failureResolved = failureSaveExact &&
          RecruitCenterSubjectState_ResolveFailedMissionProbeForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.2), secondCenter,
              &failureResult);
      RecruitCenterObjectiveStateSummary failureRemainingState = {};
      const bool failureRemainingReady = failureResolved &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &failureRemainingState);
      const bool failureRemainingExact = failureRemainingReady &&
          failureRemainingState.missions == 1 &&
          failureRemainingState.totalMissions == 2 &&
          failureRemainingState.inProcessMissions == 1 &&
          failureRemainingState.failedMissions == 0 &&
          failureRemainingState.surrenderMissions == 0 &&
          failureRemainingState.scheduledChecks == 1 &&
          failureRemainingState.mapBindings == 1 &&
          std::strcmp(failureRemainingState.firstProjectName,
                      firstProject) == 0;

      std::vector<std::uint8_t> dualRollbackBytes;
      SLevelContinuationSummary dualRollbackRestored;
      SLevelContinuationSummary dualRollbackVerified;
      RecruitCenterObjectiveStateSummary dualRollbackState = {};
      const bool dualRollbackReady = dualCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              dualCheckpoint, &dualRollbackRestored) &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &dualRollbackState) &&
          RecoveredGameServices_CaptureLevelContinuation(
              &dualRollbackBytes, &dualRollbackVerified);
      const bool dualRollbackExact = dualRollbackReady &&
          dualRollbackBytes == dualCheckpoint &&
          std::memcmp(&dualState, &dualRollbackState,
                      sizeof(dualState)) == 0 &&
          dualCaptured.worldFingerprint ==
              dualRollbackRestored.restoredWorldFingerprint &&
          dualCaptured.worldFingerprint ==
              dualRollbackVerified.worldFingerprint;

      RecruitCenterMissionTerminalProbeSummary surrenderTransition = {};
      const bool surrenderTransitionReady = dualRollbackExact &&
          RecruitCenterSubjectState_SurrenderMissionProbe(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.3),
              &surrenderTransition);
      RecruitCenterObjectiveStateSummary surrenderState = {};
      const bool surrenderStateReady = surrenderTransitionReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &surrenderState);
      const bool surrenderStateExact = surrenderStateReady &&
          surrenderState.missions == 2 && surrenderState.totalMissions == 2 &&
          surrenderState.inProcessMissions == 0 &&
          surrenderState.successMissions == 0 &&
          surrenderState.failedMissions == 0 &&
          surrenderState.surrenderMissions == 2 &&
          surrenderState.scheduledChecks == 2 &&
          surrenderState.mapBindings == 2;

      std::vector<std::uint8_t> surrenderCheckpoint;
      std::vector<std::uint8_t> surrenderRecaptured;
      SLevelContinuationSummary surrenderCaptured;
      SLevelContinuationSummary surrenderRestored;
      SLevelContinuationSummary surrenderVerified;
      RecruitCenterObjectiveStateSummary surrenderRestoredState = {};
      const bool surrenderCaptureReady = surrenderStateExact &&
          RecoveredGameServices_CaptureLevelContinuation(
              &surrenderCheckpoint, &surrenderCaptured);
      const bool surrenderRestoreReady = surrenderCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              surrenderCheckpoint, &surrenderRestored);
      const bool surrenderRestoredStateReady = surrenderRestoreReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &surrenderRestoredState);
      const bool surrenderRecaptureReady = surrenderRestoredStateReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &surrenderRecaptured, &surrenderVerified);
      const bool surrenderSaveExact = surrenderRecaptureReady &&
          surrenderCheckpoint == surrenderRecaptured &&
          std::memcmp(&surrenderState, &surrenderRestoredState,
                      sizeof(surrenderState)) == 0 &&
          surrenderCaptured.worldFingerprint ==
              surrenderRestored.restoredWorldFingerprint &&
          surrenderCaptured.worldFingerprint ==
              surrenderVerified.worldFingerprint;

      RecruitCenterMissionTerminalProbeSummary firstSurrenderResult = {};
      const bool firstSurrenderResolved = surrenderSaveExact &&
          RecruitCenterSubjectState_ResolveSurrenderedMissionProbeForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.4),
              firstCenter, &firstSurrenderResult);
      RecruitCenterObjectiveStateSummary surrenderRemainingState = {};
      const bool surrenderRemainingReady = firstSurrenderResolved &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &surrenderRemainingState);
      const bool surrenderRemainingExact = surrenderRemainingReady &&
          surrenderRemainingState.missions == 1 &&
          surrenderRemainingState.totalMissions == 2 &&
          surrenderRemainingState.inProcessMissions == 0 &&
          surrenderRemainingState.failedMissions == 0 &&
          surrenderRemainingState.surrenderMissions == 1 &&
          surrenderRemainingState.scheduledChecks == 1 &&
          surrenderRemainingState.mapBindings == 1 &&
          std::strcmp(surrenderRemainingState.firstProjectName,
                      failureProject) == 0;

      std::vector<std::uint8_t> survivorCheckpoint;
      std::vector<std::uint8_t> survivorRecaptured;
      SLevelContinuationSummary survivorCaptured;
      SLevelContinuationSummary survivorRestored;
      SLevelContinuationSummary survivorVerified;
      RecruitCenterObjectiveStateSummary survivorRestoredState = {};
      const bool survivorCaptureReady = surrenderRemainingExact &&
          RecoveredGameServices_CaptureLevelContinuation(
              &survivorCheckpoint, &survivorCaptured);
      const bool survivorRestoreReady = survivorCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              survivorCheckpoint, &survivorRestored);
      const bool survivorRestoredStateReady = survivorRestoreReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &survivorRestoredState);
      const bool survivorRecaptureReady = survivorRestoredStateReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &survivorRecaptured, &survivorVerified);
      const bool survivorSaveExact = survivorRecaptureReady &&
          survivorCheckpoint == survivorRecaptured &&
          std::memcmp(&surrenderRemainingState, &survivorRestoredState,
                      sizeof(surrenderRemainingState)) == 0 &&
          survivorCaptured.worldFingerprint ==
              survivorRestored.restoredWorldFingerprint &&
          survivorCaptured.worldFingerprint ==
              survivorVerified.worldFingerprint;

      RecruitCenterMissionTerminalProbeSummary secondSurrenderResult = {};
      const bool secondSurrenderResolved = survivorSaveExact &&
          RecruitCenterSubjectState_ResolveSurrenderedMissionProbeForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.5), secondCenter,
              &secondSurrenderResult);
      RecruitCenterObjectiveStateSummary cleanedState = {};
      const bool cleanedStateReady = secondSurrenderResolved &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &cleanedState);
      const bool cleanedStateExact = cleanedStateReady &&
          cleanedState.missions == 0 && cleanedState.totalMissions == 2 &&
          cleanedState.inProcessMissions == 0 &&
          cleanedState.failedMissions == 0 &&
          cleanedState.surrenderMissions == 0 &&
          cleanedState.scheduledChecks == 0 &&
          cleanedState.mapBindings == 0;

      std::vector<std::uint8_t> baselineRollbackBytes;
      SLevelContinuationSummary baselineRollbackRestored;
      SLevelContinuationSummary baselineRollbackVerified;
      RecruitCenterObjectiveStateSummary baselineRollbackState = {};
      const bool baselineRollbackReady = objectiveBaselineReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              objectiveBaseline, &baselineRollbackRestored) &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &baselineRollbackState) &&
          RecoveredGameServices_CaptureLevelContinuation(
              &baselineRollbackBytes, &baselineRollbackVerified);
      const bool baselineRollbackExact = baselineRollbackReady &&
          baselineRollbackBytes == objectiveBaseline &&
          std::memcmp(&objectiveBaselineState, &baselineRollbackState,
                      sizeof(objectiveBaselineState)) == 0 &&
          objectiveBaselineSummary.worldFingerprint ==
              baselineRollbackRestored.restoredWorldFingerprint &&
          objectiveBaselineSummary.worldFingerprint ==
              baselineRollbackVerified.worldFingerprint;

      log.Line(std::string("mission_terminal_projects=") +
               (dualState.firstProjectName[0] == 0 ? "<none>" :
                                                       dualState.firstProjectName) +
               "/" +
               (dualState.secondProjectName[0] == 0 ? "<none>" :
                                                       dualState.secondProjectName));
      log.Line("mission_terminal_baseline=" +
               std::to_string(objectiveBaselineReady ? 1 : 0) + "/" +
               std::to_string(terminalBaselineExact ? 1 : 0) + "/" +
               std::to_string(objectiveBaselineState.missions) + "/" +
               std::to_string(objectiveBaselineState.scheduledChecks) + "/" +
               std::to_string(objectiveBaselineState.mapBindings) + "/" +
               std::to_string(failureStaged ? 1 : 0) + "/" +
               std::string(failureMission.projectName[0] == 0 ? "<none>" :
                    failureMission.projectName));
      log.Line("mission_terminal_failure=" +
               std::to_string(failureMissionReady ? 1 : 0) + "/" +
               std::to_string(failureTransitionReady ? 1 : 0) + "/" +
               std::to_string(failureTransition.statusPresentations) + "/" +
               std::to_string(failureTransition.statusTransitions) + "/" +
               std::to_string(failureState.failedMissions) + "/" +
               std::to_string(failureStateExact ? 1 : 0) + "/" +
               std::to_string(failureTransition.checkGraphExact));
      log.Line("mission_terminal_failure_save=" +
               std::to_string(failureCaptureReady ? 1 : 0) + "/" +
               std::to_string(failureRestoreReady ? 1 : 0) + "/" +
               std::to_string(failureRecaptureReady ? 1 : 0) + "/" +
               std::to_string(failureSaveExact ? 1 : 0));
      log.Line("mission_terminal_failure_result=" +
               std::to_string(failureResolved ? 1 : 0) + "/" +
               std::to_string(failureResult.removedMissions) + "/" +
               std::to_string(failureResult.resultPresentations) + "/" +
               std::to_string(failureResult.rewardCreated) + "/" +
               std::to_string(failureResult.damagePreserved) + "/" +
               std::to_string(failureResult.ammunitionPreserved) + "/" +
               std::to_string(failureResult.repeatIdempotent) + "/" +
               std::to_string(failureRemainingExact ? 1 : 0));
      log.Line("mission_terminal_surrender=" +
               std::to_string(surrenderTransitionReady ? 1 : 0) + "/" +
               std::to_string(surrenderTransition.commandAccepted) + "/" +
               std::to_string(surrenderTransition.statusPresentations) + "/" +
               std::to_string(surrenderTransition.statusTransitions) + "/" +
               std::to_string(surrenderState.surrenderMissions) + "/" +
               std::to_string(surrenderStateExact ? 1 : 0) + "/" +
               std::to_string(surrenderTransition.checkGraphExact));
      log.Line("mission_terminal_surrender_save=" +
               std::to_string(surrenderCaptureReady ? 1 : 0) + "/" +
               std::to_string(surrenderRestoreReady ? 1 : 0) + "/" +
               std::to_string(surrenderRecaptureReady ? 1 : 0) + "/" +
               std::to_string(surrenderSaveExact ? 1 : 0));
      log.Line("mission_terminal_survivor=" +
               std::to_string(firstSurrenderResolved ? 1 : 0) + "/" +
               std::to_string(firstSurrenderResult.removedMissions) + "/" +
               std::to_string(firstSurrenderResult.resultPresentations) + "/" +
               std::to_string(firstSurrenderResult.rewardCreated) + "/" +
               std::to_string(surrenderRemainingState.surrenderMissions) + "/" +
               std::to_string(surrenderRemainingState.scheduledChecks) + "/" +
               std::to_string(surrenderRemainingState.mapBindings) + "/" +
               std::to_string(surrenderRemainingExact ? 1 : 0));
      log.Line("mission_terminal_survivor_save=" +
               std::to_string(survivorCaptureReady ? 1 : 0) + "/" +
               std::to_string(survivorRestoreReady ? 1 : 0) + "/" +
               std::to_string(survivorRecaptureReady ? 1 : 0) + "/" +
               std::to_string(survivorSaveExact ? 1 : 0));
      log.Line("mission_terminal_cleanup=" +
               std::to_string(secondSurrenderResolved ? 1 : 0) + "/" +
               std::to_string(secondSurrenderResult.removedMissions) + "/" +
               std::to_string(secondSurrenderResult.resultPresentations) + "/" +
               std::to_string(cleanedState.missions) + "/" +
               std::to_string(cleanedState.scheduledChecks) + "/" +
               std::to_string(cleanedState.mapBindings) + "/" +
               std::to_string(cleanedStateExact ? 1 : 0));
      log.Line("mission_terminal_rollback=" +
               std::to_string(dualRollbackReady ? 1 : 0) + "/" +
               std::to_string(dualRollbackExact ? 1 : 0) + "/" +
               std::to_string(baselineRollbackReady ? 1 : 0) + "/" +
               std::to_string(baselineRollbackExact ? 1 : 0));
      const bool terminalExact = terminalBaselineExact && dualStateExact &&
          failureStateExact && failureSaveExact && failureResolved &&
          failureRemainingExact && dualRollbackExact && surrenderStateExact &&
          surrenderSaveExact && surrenderRemainingExact && survivorSaveExact &&
          cleanedStateExact && baselineRollbackExact;
      if (!terminalExact) {
        const char* terminalError = RecruitCenterSubjectState_LastError();
        if (!terminalFailureDetail.empty())
          log.Line("mission_terminal_error=" + terminalFailureDetail);
        else if (terminalError != nullptr && terminalError[0] != 0)
          log.Line(std::string("mission_terminal_error=") + terminalError);
        else
          log.Line(std::string("mission_terminal_error=") +
                   RecoveredGameServices_LastLevelContinuationError());
      }
      loopFailed = !terminalExact;
    }
    if (!loopFailed && (options.missionNoRewardResultSmoke ||
                        options.missionTerminalNoRewardResultSmoke)) {
      const bool terminalNoReward =
          options.missionTerminalNoRewardResultSmoke;
      RecruitCenterObjectiveStateSummary noRewardBefore = {};
      std::vector<std::uint8_t> noRewardCheckpoint;
      SLevelContinuationSummary noRewardCheckpointSummary;
      const bool noRewardCheckpointReady =
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &noRewardBefore) &&
          noRewardBefore.missions > 0 &&
          noRewardBefore.inProcessMissions == noRewardBefore.missions &&
          RecoveredGameServices_CaptureLevelContinuation(
              &noRewardCheckpoint, &noRewardCheckpointSummary);

      RecruitCenterMissionNoRewardResultProbeSummary noReward = {};
      const bool noRewardResultReady = noRewardCheckpointReady &&
          (terminalNoReward
               ? RecruitCenterSubjectState_CompleteTerminalNoRewardMissionProbeForCenter(
                         g_super.m_context,
                         (std::max)(0.1, Session::m_moment + 0.1),
                         mission.centerName, &noReward)
               : RecruitCenterSubjectState_CompleteNoRewardMissionProbeForCenter(
                         g_super.m_context,
                         (std::max)(0.1, Session::m_moment + 0.1),
                         mission.centerName, &noReward));
      RecruitCenterObjectiveStateSummary noRewardAfter = {};
      const bool noRewardAfterReady = noRewardResultReady &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &noRewardAfter);
      const bool noRewardObjectiveExact = noRewardAfterReady &&
          noRewardAfter.missions == noRewardBefore.missions - 1 &&
          noRewardAfter.totalMissions == noRewardBefore.totalMissions &&
          noRewardAfter.inProcessMissions ==
              noRewardBefore.inProcessMissions - 1 &&
          noRewardAfter.successMissions == 0 &&
          noRewardAfter.failedMissions == 0 &&
          noRewardAfter.surrenderMissions == 0 &&
          noRewardAfter.scheduledChecks ==
              noRewardBefore.scheduledChecks - 1 &&
          noRewardAfter.mapBindings == noRewardBefore.mapBindings - 1;

      std::vector<std::uint8_t> noRewardResultState;
      std::vector<std::uint8_t> noRewardResultRecaptured;
      SLevelContinuationSummary noRewardResultCaptured;
      SLevelContinuationSummary noRewardResultRestored;
      SLevelContinuationSummary noRewardResultVerified;
      RecruitCenterObjectiveStateSummary noRewardResultRestoredState = {};
      const bool noRewardResultCaptureReady = noRewardObjectiveExact &&
          RecoveredGameServices_CaptureLevelContinuation(
              &noRewardResultState, &noRewardResultCaptured);
      const bool noRewardResultRestoreReady = noRewardResultCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              noRewardResultState, &noRewardResultRestored) &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &noRewardResultRestoredState);
      const bool noRewardResultRecaptureReady = noRewardResultRestoreReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &noRewardResultRecaptured, &noRewardResultVerified);
      const bool noRewardResultSaveExact = noRewardResultRecaptureReady &&
          noRewardResultState == noRewardResultRecaptured &&
          std::memcmp(&noRewardAfter, &noRewardResultRestoredState,
                      sizeof(noRewardAfter)) == 0 &&
          noRewardResultCaptured.ready && noRewardResultRestored.ready &&
          noRewardResultVerified.ready &&
          noRewardResultCaptured.sections ==
              kActiveWorldOwnerSectionCount &&
          noRewardResultRestored.ownerPhases ==
              kActiveWorldOwnerSectionCount &&
          noRewardResultRestored.referencePhases ==
              kActiveWorldOwnerSectionCount &&
          noRewardResultCaptured.worldFingerprint ==
              noRewardResultRestored.restoredWorldFingerprint &&
          noRewardResultCaptured.worldFingerprint ==
              noRewardResultVerified.worldFingerprint;

      std::vector<std::uint8_t> noRewardRolledBack;
      SLevelContinuationSummary noRewardRollbackRestored;
      SLevelContinuationSummary noRewardRollbackVerified;
      RecruitCenterObjectiveStateSummary noRewardRollbackState = {};
      const bool noRewardRollbackReady = noRewardResultSaveExact &&
          RecoveredGameServices_RestoreLevelContinuation(
              noRewardCheckpoint, &noRewardRollbackRestored) &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &noRewardRollbackState);
      const bool noRewardRollbackRecaptureReady = noRewardRollbackReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &noRewardRolledBack, &noRewardRollbackVerified);
      const bool noRewardRollbackExact = noRewardRollbackRecaptureReady &&
          noRewardCheckpoint == noRewardRolledBack &&
          std::memcmp(&noRewardBefore, &noRewardRollbackState,
                      sizeof(noRewardBefore)) == 0 &&
          noRewardCheckpointSummary.ready && noRewardRollbackRestored.ready &&
          noRewardRollbackVerified.ready &&
          noRewardCheckpointSummary.worldFingerprint ==
              noRewardRollbackRestored.restoredWorldFingerprint &&
          noRewardCheckpointSummary.worldFingerprint ==
              noRewardRollbackVerified.worldFingerprint;

      std::vector<std::uint8_t> noRewardReapplied;
      SLevelContinuationSummary noRewardReapplyRestored;
      SLevelContinuationSummary noRewardReapplyVerified;
      RecruitCenterObjectiveStateSummary noRewardReappliedState = {};
      // Leave both ordinary and terminal probes on the committed result.
      // This makes a requested save slot describe the state just proven by
      // the result smoke, while still exercising the pre-result rollback
      // before reapplying the exact captured continuation.
      const bool noRewardReapplyRestoreReady = noRewardRollbackExact &&
          RecoveredGameServices_RestoreLevelContinuation(
              noRewardResultState, &noRewardReapplyRestored) &&
          RecruitCenterSubjectState_ObjectiveState(
              g_super.m_context, &noRewardReappliedState);
      const bool noRewardReapplyRecaptureReady =
          noRewardReapplyRestoreReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &noRewardReapplied, &noRewardReapplyVerified);
      const bool noRewardReapplyExact = noRewardReapplyRecaptureReady &&
          noRewardResultState == noRewardReapplied &&
          std::memcmp(&noRewardAfter, &noRewardReappliedState,
                      sizeof(noRewardAfter)) == 0 &&
          noRewardReapplyRestored.ready && noRewardReapplyVerified.ready &&
          noRewardResultCaptured.worldFingerprint ==
              noRewardReapplyRestored.restoredWorldFingerprint &&
          noRewardResultCaptured.worldFingerprint ==
              noRewardReapplyVerified.worldFingerprint;

      const std::string noRewardPrefix = terminalNoReward
          ? "mission_terminal_no_reward_" : "mission_no_reward_";
      log.Line(noRewardPrefix + "project=" + noReward.completedProjectName +
               "/" + (terminalNoReward ? "<none>"
                                           : noReward.nextProjectName));
      log.Line(noRewardPrefix + "conditions=" +
                std::to_string(terminalNoReward
                                   ? noReward.reachedConditions
                                   : noReward.conditionsRemoved +
                                         noReward.reachedConditions) + "/" +
                std::to_string(noReward.statusTransitions));
      if (!terminalNoReward)
        log.Line(noRewardPrefix + "reached=" +
                 std::to_string(noReward.reachedConditions) + "/" +
                 std::to_string(noReward.failureGuardPreserved));
      log.Line(noRewardPrefix + "commit=" +
               std::to_string(noReward.completedMissions) + "/" +
               std::to_string(noReward.resultPresentations) + "/" +
               std::to_string(noReward.rewardsCreated) + "/" +
               std::to_string(noReward.rewardAbsent) + "/" +
               std::to_string(noReward.completedProjectRetired) + "/" +
               std::to_string(noReward.repaired) + "/" +
               std::to_string(noReward.refilled) + "/" +
               std::to_string(noReward.repeatIdempotent) +
               (terminalNoReward
                    ? "/" + std::to_string(noReward.noNextCandidate)
                    : std::string()));
      log.Line(noRewardPrefix + "progress=" +
               std::to_string(noReward.missionsBefore) + "/" +
               std::to_string(noReward.missionsAfter) + "/" +
               std::to_string(noReward.totalMissionsBefore) + "/" +
               std::to_string(noReward.totalMissionsAfter) + "/" +
               std::to_string(noReward.scheduledChecksBefore) + "/" +
               std::to_string(noReward.scheduledChecksAfter));
      log.Line(noRewardPrefix + "objective=" +
               std::to_string(noRewardBefore.missions) + "/" +
               std::to_string(noRewardAfter.missions) + "/" +
               std::to_string(noRewardBefore.mapBindings) + "/" +
               std::to_string(noRewardAfter.mapBindings) + "/" +
               std::to_string(noRewardObjectiveExact ? 1 : 0));
      log.Line(noRewardPrefix + "save=" +
               std::to_string(noRewardResultCaptureReady ? 1 : 0) + "/" +
               std::to_string(noRewardResultRestoreReady ? 1 : 0) + "/" +
               std::to_string(noRewardResultRecaptureReady ? 1 : 0) + "/" +
               std::to_string(noRewardResultSaveExact ? 1 : 0));
      log.Line(noRewardPrefix + "rollback=" +
               std::to_string(noRewardRollbackReady ? 1 : 0) + "/" +
               std::to_string(noRewardRollbackRecaptureReady ? 1 : 0) + "/" +
               std::to_string(noRewardRollbackExact ? 1 : 0));
      log.Line(noRewardPrefix + "reapply=" +
               std::to_string(noRewardReapplyRestoreReady ? 1 : 0) + "/" +
               std::to_string(noRewardReapplyRecaptureReady ? 1 : 0) +
               "/" + std::to_string(noRewardReapplyExact ? 1 : 0));
      const bool noRewardExact = noRewardResultReady &&
          noRewardObjectiveExact && noRewardResultSaveExact &&
          noRewardRollbackExact && noRewardReapplyExact;
      if (!noRewardExact) {
        const char* noRewardError = RecruitCenterSubjectState_LastError();
        if (noRewardError != nullptr && noRewardError[0] != 0)
          log.Line(noRewardPrefix + "error=" + noRewardError);
        else
          log.Line(noRewardPrefix + "error=" +
                   RecoveredGameServices_LastLevelContinuationError());
      }
      loopFailed = !noRewardExact;
    }
    if (!loopFailed && options.missionResultSmoke) {
      std::vector<std::uint8_t> resultCheckpoint;
      std::vector<std::uint8_t> resultState;
      std::vector<std::uint8_t> resultRecaptured;
      std::vector<std::uint8_t> resultDropState;
      std::vector<std::uint8_t> resultDropRecaptured;
      std::vector<std::uint8_t> resultPortalState;
      std::vector<std::uint8_t> resultPortalRecaptured;
      std::vector<std::uint8_t> resultRolledBack;
      SLevelContinuationSummary resultCheckpointSummary;
      SLevelContinuationSummary resultCaptured;
      SLevelContinuationSummary resultRestored;
      SLevelContinuationSummary resultVerified;
      SLevelContinuationSummary resultDropCaptured;
      SLevelContinuationSummary resultDropRestored;
      SLevelContinuationSummary resultDropVerified;
      SLevelContinuationSummary resultPortalCaptured;
      SLevelContinuationSummary resultPortalRestored;
      SLevelContinuationSummary resultPortalVerified;
      SLevelContinuationSummary resultRollbackRestored;
      SLevelContinuationSummary resultRollbackVerified;
      const bool checkpointReady =
          RecoveredGameServices_CaptureLevelContinuation(
              &resultCheckpoint, &resultCheckpointSummary);
      RecruitCenterMissionResultProbeSummary result = {};
      const bool resultReady = checkpointReady &&
          RecruitCenterSubjectState_CompleteMissionProbeForCenter(
              g_super.m_context,
              (std::max)(0.1, Session::m_moment + 0.1),
              mission.centerName, &result);
      const bool resultCaptureReady = resultReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &resultState, &resultCaptured);
      const bool resultRestoreReady = resultCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              resultState, &resultRestored);
      const bool resultCarrierRestoreReady = resultRestoreReady &&
          RecruitCenterSubjectState_RewardCarrierState(
              g_super.m_context, true);
      SPortalAdmissionProbeSummary portalAdmission;
      const bool portalAttachedRejected = resultCarrierRestoreReady &&
          PortalActiveWorldState_RejectAttachedProbe(
              g_super.m_context,
              g_super.m_context->searchObject("Artifact"),
              &portalAdmission);
      const bool resultRecaptureReady = resultCarrierRestoreReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &resultRecaptured, &resultVerified);
      const bool resultSaveExact = resultRecaptureReady &&
          resultState == resultRecaptured && resultCaptured.ready &&
          resultRestored.ready && resultVerified.ready &&
          resultCaptured.sections == kActiveWorldOwnerSectionCount &&
          resultRestored.ownerPhases == kActiveWorldOwnerSectionCount &&
          resultRestored.referencePhases == kActiveWorldOwnerSectionCount &&
          resultCaptured.worldFingerprint ==
              resultRestored.restoredWorldFingerprint &&
          resultCaptured.worldFingerprint == resultVerified.worldFingerprint;
      const bool resultDropReady = resultSaveExact &&
          RecruitCenterSubjectState_DropRewardProbe(
              g_super.m_context,
              (std::max)(0.1, Session::m_viewTime), &result);
      const bool resultDropCaptureReady = resultDropReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &resultDropState, &resultDropCaptured);
      const bool resultDropRestoreReady = resultDropCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              resultDropState, &resultDropRestored);
      const bool resultDropDetachReady = resultDropRestoreReady &&
          RecruitCenterSubjectState_RewardCarrierState(
              g_super.m_context, false);
      const bool resultDropRecaptureReady = resultDropDetachReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &resultDropRecaptured, &resultDropVerified);
      const bool resultDropSaveExact = resultDropRecaptureReady &&
          resultDropState == resultDropRecaptured &&
          resultDropCaptured.ready && resultDropRestored.ready &&
          resultDropVerified.ready &&
          resultDropCaptured.sections == kActiveWorldOwnerSectionCount &&
          resultDropRestored.ownerPhases == kActiveWorldOwnerSectionCount &&
          resultDropRestored.referencePhases ==
              kActiveWorldOwnerSectionCount &&
          resultDropCaptured.worldFingerprint ==
              resultDropRestored.restoredWorldFingerprint &&
          resultDropCaptured.worldFingerprint ==
              resultDropVerified.worldFingerprint;
      const bool portalAdmissionReady = resultDropSaveExact &&
          portalAttachedRejected && PortalActiveWorldState_AdmissionProbe(
              g_super.m_context,
              g_super.m_context->searchObject("Artifact"),
              (std::max)(0.1, Session::m_viewTime + 0.1),
              &portalAdmission);
      const bool resultPortalCaptureReady = portalAdmissionReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &resultPortalState, &resultPortalCaptured);
      const bool resultPortalRestoreReady = resultPortalCaptureReady &&
          RecoveredGameServices_RestoreLevelContinuation(
              resultPortalState, &resultPortalRestored);
      const bool resultPortalRecaptureReady = resultPortalRestoreReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &resultPortalRecaptured, &resultPortalVerified);
      const bool resultPortalSaveExact = resultPortalRecaptureReady &&
          resultPortalState == resultPortalRecaptured &&
          resultPortalCaptured.ready && resultPortalRestored.ready &&
          resultPortalVerified.ready &&
          resultPortalCaptured.sections == kActiveWorldOwnerSectionCount &&
          resultPortalRestored.ownerPhases ==
              kActiveWorldOwnerSectionCount &&
          resultPortalRestored.referencePhases ==
              kActiveWorldOwnerSectionCount &&
          resultPortalCaptured.worldFingerprint ==
              resultPortalRestored.restoredWorldFingerprint &&
          resultPortalCaptured.worldFingerprint ==
              resultPortalVerified.worldFingerprint;
      const bool resultRollbackReady = resultPortalSaveExact &&
          RecoveredGameServices_RestoreLevelContinuation(
              resultCheckpoint, &resultRollbackRestored);
      const bool resultRollbackRecaptured = resultRollbackReady &&
          RecoveredGameServices_CaptureLevelContinuation(
              &resultRolledBack, &resultRollbackVerified);
      const bool resultRollbackExact = resultRollbackRecaptured &&
          resultCheckpoint == resultRolledBack &&
          resultCheckpointSummary.ready && resultRollbackRestored.ready &&
          resultRollbackVerified.ready &&
          resultCheckpointSummary.worldFingerprint ==
              resultRollbackRestored.restoredWorldFingerprint &&
          resultCheckpointSummary.worldFingerprint ==
              resultRollbackVerified.worldFingerprint;
      log.Line(std::string("mission_result_project=") +
               result.completedProjectName + "/" + result.nextProjectName);
      log.Line("mission_result_conditions=" +
               std::to_string(result.conditionsRemoved) + "/" +
               std::to_string(result.statusTransitions));
      log.Line("mission_result_commit=" +
               std::to_string(result.completedMissions) + "/" +
               std::to_string(result.rewardsCreated) + "/" +
               std::to_string(result.rewardInterfaceReady) + "/" +
               std::to_string(result.repaired) + "/" +
               std::to_string(result.refilled) + "/" +
               std::to_string(result.repeatIdempotent));
      log.Line("mission_result_carrier=" +
               std::to_string(result.pickupAccepted) + "/" +
               std::to_string(result.bidirectionalAttachment) + "/" +
               std::to_string(result.carryEventsCancelled) + "/" +
               std::to_string(result.carryMoveMatched) + "/" +
               std::to_string(resultCarrierRestoreReady ? 1 : 0));
      log.Line("mission_result_drop=" +
               std::to_string(result.dropInputAccepted) + "/" +
               std::to_string(result.bidirectionalDetach) + "/" +
               std::to_string(result.dropMoveEventScheduled) + "/" +
               std::to_string(result.dropMotionMatched));
      log.Line("mission_result_progress=" +
               std::to_string(result.missionsBefore) + "/" +
               std::to_string(result.missionsAfter) + "/" +
               std::to_string(result.totalMissionsBefore) + "/" +
               std::to_string(result.totalMissionsAfter));
      log.Line("mission_result_save=" +
               std::to_string(resultCaptureReady ? 1 : 0) + "/" +
               std::to_string(resultRestoreReady ? 1 : 0) + "/" +
               std::to_string(resultRecaptureReady ? 1 : 0) + "/" +
               std::to_string(resultSaveExact ? 1 : 0));
      log.Line("mission_result_drop_save=" +
               std::to_string(resultDropCaptureReady ? 1 : 0) + "/" +
               std::to_string(resultDropRestoreReady ? 1 : 0) + "/" +
               std::to_string(resultDropRecaptureReady ? 1 : 0) + "/" +
               std::to_string(resultDropSaveExact ? 1 : 0));
      log.Line("mission_result_portal=" +
               std::to_string(portalAdmission.portalCount) + "/" +
               std::to_string(portalAdmission.attachedRejected) + "/" +
               std::to_string(portalAdmission.fullRejected) + "/" +
               std::to_string(portalAdmission.consumed) + "/" +
               std::to_string(portalAdmission.occupiedAdvanced) + "/" +
               std::to_string(portalAdmission.eventResidueCleared));
      log.Line("mission_result_portal_save=" +
               std::to_string(resultPortalCaptureReady ? 1 : 0) + "/" +
               std::to_string(resultPortalRestoreReady ? 1 : 0) + "/" +
               std::to_string(resultPortalRecaptureReady ? 1 : 0) + "/" +
               std::to_string(resultPortalSaveExact ? 1 : 0));
      log.Line("mission_result_rollback=" +
               std::to_string(resultRollbackReady ? 1 : 0) + "/" +
               std::to_string(resultRollbackRecaptured ? 1 : 0) + "/" +
               std::to_string(resultRollbackExact ? 1 : 0));
      if (!resultReady)
        log.Line(std::string("mission_result_error=") +
                 RecruitCenterSubjectState_LastError());
      else if (!portalAttachedRejected || !portalAdmissionReady)
        log.Line(std::string("mission_result_error=") +
                 PortalActiveWorldState_LastFailure());
      else if (!resultSaveExact || !resultDropSaveExact ||
               !resultPortalSaveExact || !resultRollbackExact)
        log.Line(std::string("mission_result_error=") +
                 RecoveredGameServices_LastLevelContinuationError());
      bool campaignChainExact = !options.campaignQuestChainSmoke;
      if (options.campaignQuestChainSmoke && resultRollbackExact) {
        const int chainSourceLevelIndex = currentLevelIndex;
        const int chainTargetLevelIndex =
            chainSourceLevelIndex == static_cast<int>(data.levels.size()) - 1
                ? 0 : chainSourceLevelIndex + 1;
        RecruitCenterMissionResultProbeSummary chainResult = {};
        const bool chainResultReady =
            RecruitCenterSubjectState_CompleteMissionProbeForCenter(
                g_super.m_context,
                (std::max)(0.1, Session::m_moment + 0.2),
                mission.centerName, &chainResult);
        const bool chainDropReady = chainResultReady &&
            RecruitCenterSubjectState_DropRewardProbe(
                g_super.m_context,
                (std::max)(0.1, Session::m_viewTime), &chainResult);

        SPortalFinalAdmissionProbeSummary chainPrepared;
        const bool chainPrepareReady = chainDropReady &&
            PortalActiveWorldState_PrepareFinalAdmissionProbe(
                g_super.m_context, &chainPrepared);
        std::vector<std::uint8_t> chainPreparedState;
        std::vector<std::uint8_t> chainPreparedRecaptured;
        SLevelContinuationSummary chainPreparedCaptured;
        SLevelContinuationSummary chainPreparedRestored;
        SLevelContinuationSummary chainPreparedVerified;
        const bool chainPreparedCaptureReady = chainPrepareReady &&
            RecoveredGameServices_CaptureLevelContinuation(
                &chainPreparedState, &chainPreparedCaptured);
        const bool chainPreparedRestoreReady = chainPreparedCaptureReady &&
            RecoveredGameServices_RestoreLevelContinuation(
                chainPreparedState, &chainPreparedRestored);
        const bool chainPreparedRewardFree = chainPreparedRestoreReady &&
            RecruitCenterSubjectState_RewardCarrierState(
                g_super.m_context, false);
        const bool chainPreparedRecaptureReady = chainPreparedRewardFree &&
            RecoveredGameServices_CaptureLevelContinuation(
                &chainPreparedRecaptured, &chainPreparedVerified);
        const bool chainPreparedSaveExact = chainPreparedRecaptureReady &&
            chainPreparedState == chainPreparedRecaptured &&
            chainPreparedCaptured.ready && chainPreparedRestored.ready &&
            chainPreparedVerified.ready &&
            chainPreparedCaptured.worldFingerprint ==
                chainPreparedRestored.restoredWorldFingerprint &&
            chainPreparedCaptured.worldFingerprint ==
                chainPreparedVerified.worldFingerprint;

        SPortalAdmissionProbeSummary chainAdmission;
        const bool chainAdmissionReady = chainPreparedSaveExact &&
            PortalActiveWorldState_AdmissionProbe(
                g_super.m_context,
                g_super.m_context->searchObject("Artifact"),
                (std::max)(0.1, Session::m_viewTime + 0.2),
                &chainAdmission);
        std::vector<std::uint8_t> chainFullState;
        std::vector<std::uint8_t> chainFullRecaptured;
        SLevelContinuationSummary chainFullCaptured;
        SLevelContinuationSummary chainFullRestored;
        SLevelContinuationSummary chainFullVerified;
        const bool chainFullCaptureReady = chainAdmissionReady &&
            RecoveredGameServices_CaptureLevelContinuation(
                &chainFullState, &chainFullCaptured);
        const bool chainFullRestoreReady = chainFullCaptureReady &&
            RecoveredGameServices_RestoreLevelContinuation(
                chainFullState, &chainFullRestored);
        const bool chainFullRecaptureReady = chainFullRestoreReady &&
            RecoveredGameServices_CaptureLevelContinuation(
                &chainFullRecaptured, &chainFullVerified);
        const bool chainFullSaveExact = chainFullRecaptureReady &&
            chainFullState == chainFullRecaptured &&
            chainFullCaptured.ready && chainFullRestored.ready &&
            chainFullVerified.ready &&
            chainFullCaptured.worldFingerprint ==
                chainFullRestored.restoredWorldFingerprint &&
            chainFullCaptured.worldFingerprint ==
                chainFullVerified.worldFingerprint;

        SPortalTransitionProbeSummary chainRejectedTransition;
        const bool chainRejectedTransitionStaged = chainFullSaveExact &&
            PortalActiveWorldState_StageReadyTransitionProbe(
                g_super.m_context,
                (std::max)(0.1, Session::m_viewTime + 0.3),
                &chainRejectedTransition);
        RetailData unavailableTarget = data;
        if (chainTargetLevelIndex >= 0 &&
            chainTargetLevelIndex <
                static_cast<int>(unavailableTarget.levels.size()))
          unavailableTarget.levels[chainTargetLevelIndex] =
              L"__rr2nw_missing_campaign_chain_target__";
        const bool chainRollbackProcessed = chainRejectedTransitionStaged &&
            ProcessPortalLevelTransition(
                unavailableTarget, &currentLevelIndex, true, false, &log);
        std::vector<std::uint8_t> chainRolledBack;
        SLevelContinuationSummary chainRollbackVerified;
        const bool chainRollbackRecaptured = chainRollbackProcessed &&
            currentLevelIndex == chainSourceLevelIndex &&
            !PortalActiveWorldState_TransitionPending() &&
            RecoveredGameServices_CaptureLevelContinuation(
                &chainRolledBack, &chainRollbackVerified);
        const bool chainRollbackExact = chainRollbackRecaptured &&
            chainRolledBack == chainFullState &&
            chainRollbackVerified.ready &&
            chainRollbackVerified.worldFingerprint ==
                chainFullCaptured.worldFingerprint;

        SPortalTransitionProbeSummary chainCommittedTransition;
        const bool chainCommittedTransitionStaged = chainRollbackExact &&
            PortalActiveWorldState_StageReadyTransitionProbe(
                g_super.m_context,
                (std::max)(0.1, Session::m_viewTime + 0.4),
                &chainCommittedTransition);
        const bool chainTransitionCommitted =
            chainCommittedTransitionStaged && runCompleteFrame() &&
            currentLevelIndex == chainTargetLevelIndex &&
            !PortalActiveWorldState_TransitionPending();

        std::vector<std::uint8_t> chainDestinationState;
        std::vector<std::uint8_t> chainDestinationRecaptured;
        SLevelContinuationSummary chainDestinationCaptured;
        SLevelContinuationSummary chainDestinationRestored;
        SLevelContinuationSummary chainDestinationVerified;
        const bool chainDestinationCaptureReady = chainTransitionCommitted &&
            RecoveredGameServices_CaptureLevelContinuation(
                &chainDestinationState, &chainDestinationCaptured);
        const bool chainDestinationRestoreReady =
            chainDestinationCaptureReady &&
            RecoveredGameServices_RestoreLevelContinuation(
                chainDestinationState, &chainDestinationRestored);
        const bool chainDestinationRecaptureReady =
            chainDestinationRestoreReady &&
            RecoveredGameServices_CaptureLevelContinuation(
                &chainDestinationRecaptured, &chainDestinationVerified);
        const bool chainDestinationSaveExact =
            chainDestinationRecaptureReady &&
            chainDestinationState == chainDestinationRecaptured &&
            chainDestinationCaptured.ready &&
            chainDestinationRestored.ready &&
            chainDestinationVerified.ready &&
            chainDestinationCaptured.worldFingerprint ==
                chainDestinationRestored.restoredWorldFingerprint &&
            chainDestinationCaptured.worldFingerprint ==
                chainDestinationVerified.worldFingerprint;

        log.Line(std::string("campaign_chain_project=") +
                 chainResult.completedProjectName + "/" +
                 chainResult.nextProjectName);
        log.Line("campaign_chain_reward=" +
                 std::to_string(chainResultReady ? 1 : 0) + "/" +
                 std::to_string(chainResult.rewardsCreated) + "/" +
                 std::to_string(chainResult.rewardInterfaceReady) + "/" +
                 std::to_string(chainDropReady ? 1 : 0));
        log.Line("campaign_chain_prepare=" +
                 std::to_string(chainPrepareReady ? 1 : 0) + "/" +
                 std::to_string(chainPrepared.portalCount) + "/" +
                 std::to_string(chainPrepared.slots) + "/" +
                 std::to_string(chainPrepared.occupiedBefore) + "/" +
                 std::to_string(chainPrepared.occupiedPrepared) + "/" +
                 std::to_string(chainPrepared.remaining));
        log.Line("campaign_chain_prepare_save=" +
                 std::to_string(chainPreparedCaptureReady ? 1 : 0) + "/" +
                 std::to_string(chainPreparedRestoreReady ? 1 : 0) + "/" +
                 std::to_string(chainPreparedRecaptureReady ? 1 : 0) + "/" +
                 std::to_string(chainPreparedSaveExact ? 1 : 0));
        log.Line("campaign_chain_final_admission=" +
                 std::to_string(chainAdmission.portalCount) + "/" +
                 std::to_string(chainAdmission.consumed) + "/" +
                 std::to_string(chainAdmission.occupiedAdvanced) + "/" +
                 std::to_string(chainAdmission.eventResidueCleared));
        log.Line("campaign_chain_full_save=" +
                 std::to_string(chainFullCaptureReady ? 1 : 0) + "/" +
                 std::to_string(chainFullRestoreReady ? 1 : 0) + "/" +
                 std::to_string(chainFullRecaptureReady ? 1 : 0) + "/" +
                 std::to_string(chainFullSaveExact ? 1 : 0));
        log.Line("campaign_chain_transition_rollback=" +
                 std::to_string(chainRejectedTransition.fullPortal) + "/" +
                 std::to_string(chainRejectedTransition.collisionAccepted) +
                 "/" +
                 std::to_string(chainRejectedTransition.transitionRequested) +
                 "/" + std::to_string(chainRollbackProcessed ? 1 : 0) +
                 "/" + std::to_string(chainRollbackRecaptured ? 1 : 0) +
                 "/" + std::to_string(chainRollbackExact ? 1 : 0));
        log.Line("campaign_chain_transition_commit=" +
                 std::to_string(chainCommittedTransition.fullPortal) + "/" +
                 std::to_string(chainCommittedTransition.collisionAccepted) +
                 "/" +
                 std::to_string(chainCommittedTransition.transitionRequested) +
                 "/" + std::to_string(chainSourceLevelIndex) + "/" +
                 std::to_string(chainTargetLevelIndex) + "/" +
                 std::to_string(currentLevelIndex) + "/" +
                 std::to_string(chainTransitionCommitted ? 1 : 0));
        log.Line("campaign_chain_destination_save=" +
                 std::to_string(chainDestinationCaptureReady ? 1 : 0) + "/" +
                 std::to_string(chainDestinationRestoreReady ? 1 : 0) + "/" +
                 std::to_string(chainDestinationRecaptureReady ? 1 : 0) +
                 "/" + std::to_string(chainDestinationSaveExact ? 1 : 0));
        campaignChainExact = chainResultReady && chainDropReady &&
            chainPreparedSaveExact && chainAdmissionReady &&
            chainFullSaveExact && chainRollbackExact &&
            chainTransitionCommitted && chainDestinationSaveExact;
        if (!campaignChainExact) {
          const char* portalError = PortalActiveWorldState_LastFailure();
          const char* missionError = RecruitCenterSubjectState_LastError();
          if (portalError != nullptr && portalError[0] != 0)
            log.Line(std::string("campaign_chain_error=") + portalError);
          else if (missionError != nullptr && missionError[0] != 0)
            log.Line(std::string("campaign_chain_error=") + missionError);
          else
            log.Line(std::string("campaign_chain_error=") +
                     RecoveredGameServices_LastLevelContinuationError());
        }
      }
      loopFailed = !resultReady || !resultSaveExact ||
          !resultDropSaveExact || !portalAdmissionReady ||
          !resultPortalSaveExact || !resultRollbackExact ||
          !campaignChainExact;
    }
    if (!loopFailed && saveAfterMission) {
      const bool missionSaveRequested =
          RecoveredGameServices_RequestSaveSlot(
              static_cast<std::uint32_t>(options.startupSaveSlot), false);
      log.Line(std::string("mission_smoke_save_requested=") +
               (missionSaveRequested ? "1" : "0"));
      loopFailed = !missionSaveRequested || !runCompleteFrame();
    }
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
          captured.sections == kActiveWorldOwnerSectionCount &&
          restored.sections == kActiveWorldOwnerSectionCount &&
          restored.ownerPhases == kActiveWorldOwnerSectionCount &&
          restored.referencePhases == kActiveWorldOwnerSectionCount &&
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
          rollbackVerified.sections == kActiveWorldOwnerSectionCount &&
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
  if (!loopFailed && options.missionTerminalNoRewardFreshSmoke) {
    RecruitCenterMissionTerminalNoRewardStateSummary terminalState = {};
    const bool terminalStateReady =
        RecruitCenterSubjectState_TerminalNoRewardStateProbeForCenter(
            g_super.m_context, "Recruit.Outsider", "Mission", &terminalState);
    log.Line("mission_terminal_no_reward_fresh=" +
             std::to_string(terminalState.missionAbsent) + "/" +
             std::to_string(terminalState.projectRetired) + "/" +
             std::to_string(terminalState.noNextCandidate) + "/" +
             std::to_string(terminalState.scheduledChecks) + "/" +
             std::to_string(terminalState.rewardDetached));
    if (!terminalStateReady) {
      log.Line(std::string("mission_terminal_no_reward_fresh_error=") +
               RecruitCenterSubjectState_LastError());
    }
    loopFailed = !terminalStateReady;
  }
  if (!loopFailed && options.missionNoRewardFreshSmoke) {
    const std::string freshCenter = options.missionCenter.empty()
        ? "C.Recr0" : WideToUtf8(options.missionCenter);
    const std::string freshCompletedProject = options.missionProject.empty()
        ? "ProjectG3" : WideToUtf8(options.missionProject);
    const std::string freshNextProject = options.missionNextProject.empty()
        ? "ProjectG5" : WideToUtf8(options.missionNextProject);
    RecruitCenterMissionNoRewardProgressionStateSummary progression = {};
    const bool progressionReady =
        RecruitCenterSubjectState_NoRewardProgressionStateProbeForCenter(
            g_super.m_context, freshCenter.c_str(),
            freshCompletedProject.c_str(), freshNextProject.c_str(),
            &progression);
    log.Line("mission_no_reward_fresh_identity=" + freshCenter + "/" +
             freshCompletedProject + "/" + freshNextProject);
    log.Line("mission_no_reward_fresh=" +
             std::to_string(progression.missionAbsent) + "/" +
             std::to_string(progression.projectRetired) + "/" +
             std::to_string(progression.nextCandidateExact) + "/" +
             std::to_string(progression.scheduledChecks) + "/" +
             std::to_string(progression.rewardDetached));
    if (!progressionReady) {
      log.Line(std::string("mission_no_reward_fresh_error=") +
               RecruitCenterSubjectState_LastError());
    }
    loopFailed = !progressionReady;
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
  SRecoveredDebugMapControlProbeTelemetry debugMapControls = {};
  if (RecoveredGameServices_DebugMapControlProbeTelemetry(
          &debugMapControls)) {
    log.Line("debug_map_control_probe=" +
             std::to_string(debugMapControls.available) + "/" +
             std::to_string(debugMapControls.followTogglePair) + "/" +
             std::to_string(debugMapControls.horizontalScrollPair) + "/" +
             std::to_string(debugMapControls.verticalScrollPair) + "/" +
             std::to_string(debugMapControls.textScrollable) + "/" +
             std::to_string(debugMapControls.textScrollPair) + "/" +
             std::to_string(debugMapControls.missionSelectable) + "/" +
             std::to_string(debugMapControls.missionSelectionPair) + "/" +
             std::to_string(debugMapControls.stateRestored));
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
             std::to_string(lastMission.scriptRollbacks) + "/" +
             std::to_string(lastMission.centerPresentationAttempts) + "/" +
             std::to_string(lastMission.presentedCenterFlicks) + "/" +
             std::to_string(lastMission.presentedHostilityBriefings) + "/" +
             std::to_string(lastMission.centerPresentationFailures));
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
    log.Line("windows_input_projectile_render_submissions=" +
             std::to_string(finalPrimaryFire.bulletRenderSubmissions));
    log.Line("windows_input_projectile_particle_submissions=" +
             std::to_string(finalPrimaryFire.particleRenderSubmissions));
    log.Line("windows_input_projectile_skin_submissions=" +
             std::to_string(finalPrimaryFire.skinRenderSubmissions));
    log.Line("windows_input_projectile_skipped_skin_submissions=" +
             std::to_string(
                 finalPrimaryFire.skippedSkinRenderSubmissions));
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
  SPeopleLiveCombatTelemetry peopleLive = {};
  if (PeopleSubjectState_LiveCombatTelemetry(&peopleLive)) {
    log.Line("people_live_samples=" +
             std::to_string(peopleLive.sampleFrames) + "/" +
             std::to_string(peopleLive.rosterSamples));
    log.Line("people_live_motion=" +
             std::to_string(peopleLive.moveEvents) + "/" +
             std::to_string(peopleLive.eligibleMoveEvents) + "/" +
             std::to_string(peopleLive.displacedMoveEvents) + "/" +
             std::to_string(peopleLive.stationaryMoveEvents) + "/" +
             std::to_string(peopleLive.attackMoveEvents) + "/" +
             std::to_string(peopleLive.contactMoveEvents));
    log.Line(std::string("people_live_legacy_slope_release=") +
             std::to_string(peopleLive.legacySlopeReleaseOpportunities) + "/" +
             std::to_string(peopleLive.legacySlopeReleasedMoves) + "/" +
             (peopleLive.lastLegacySlopeReleasedOwner[0] == 0
                  ? "<none>" : peopleLive.lastLegacySlopeReleasedOwner));
    log.Line("people_live_targeting=" +
             std::to_string(peopleLive.findEvents) + "/" +
             std::to_string(peopleLive.eligibleFindEvents) + "/" +
             std::to_string(peopleLive.targetAcquisitions) + "/" +
             std::to_string(peopleLive.targetMisses) + "/" +
             std::to_string(peopleLive.attackStateSamples));
    log.Line("people_live_combat=" +
             std::to_string(peopleLive.shotsStarted) + "/" +
             std::to_string(peopleLive.damageApplications) + "/" +
             std::to_string(peopleLive.killTransitions) + "/" +
             std::to_string(peopleLive.explosionEffects) + "/" +
             std::to_string(peopleLive.corpseEffects));
    log.Line("people_live_max_displacement=" + std::to_string(
                 peopleLive.maximumHorizontalDisplacement));
    log.Line(std::string("people_live_last_stationary=") +
             (peopleLive.lastStationaryOwner[0] == 0
                  ? "<none>" : peopleLive.lastStationaryOwner) + "/" +
             std::to_string(peopleLive.lastStationaryDeltaTime) + "/" +
             std::to_string(peopleLive.lastStationaryMoveSpeed) + "/" +
             std::to_string(peopleLive.lastStationaryContactCode) + "/" +
             std::to_string(peopleLive.lastStationaryState) + "/" +
             std::to_string(peopleLive.lastStationaryStopped) + "/" +
             std::to_string(peopleLive.lastStationaryPreviousNode) + "/" +
             std::to_string(peopleLive.lastStationaryCurrentNode));
    log.Line("people_live_last_stationary_position=" +
             std::to_string(peopleLive.lastStationaryX) + "/" +
             std::to_string(peopleLive.lastStationaryY) + "/" +
             std::to_string(peopleLive.lastStationaryZ));
    log.Line("people_live_last_stationary_motion=" +
             std::to_string(peopleLive.lastStationaryMoveStartX) + "/" +
             std::to_string(peopleLive.lastStationaryMoveStartZ) + "/" +
             std::to_string(peopleLive.lastStationaryDirectionX) + "/" +
             std::to_string(peopleLive.lastStationaryDirectionZ) + "/" +
             std::to_string(peopleLive.lastStationaryTargetX) + "/" +
             std::to_string(peopleLive.lastStationaryTargetZ));
    log.Line(std::string("people_live_last_stationary_refs=") +
             (peopleLive.lastStationaryAttribute[0] == 0
                  ? "<none>" : peopleLive.lastStationaryAttribute) + "/" +
             (peopleLive.lastStationaryRoute[0] == 0
                  ? "<none>" : peopleLive.lastStationaryRoute));
    log.Line("people_live_last_stationary_policy=" +
             std::to_string(peopleLive.lastStationaryOnLand) + "/" +
             std::to_string(peopleLive.lastStationaryStopIfAttack) + "/" +
             std::to_string(peopleLive.lastStationaryDeltaZeroSpeed));
    log.Line("people_live_last_stationary_prediction=" +
             std::to_string(
                 peopleLive.lastStationaryPredictedDisplacement) + "/" +
             std::to_string(
                 peopleLive.lastStationaryObstacleRecoveryTime));
    log.Line(std::string("people_live_last_actors=") +
             (peopleLive.lastAcquiringOwner[0] == 0
                  ? "<none>" : peopleLive.lastAcquiringOwner) + "/" +
             (peopleLive.lastShootingOwner[0] == 0
                  ? "<none>" : peopleLive.lastShootingOwner) + "/" +
             (peopleLive.lastDamagedOwner[0] == 0
                  ? "<none>" : peopleLive.lastDamagedOwner));
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
