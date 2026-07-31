#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "RecoveredGameplayTuningRuntime.h"
#include "RecoveredModRuntime.h"
#include "RecoveredScriptEventRuntime.h"

namespace {

constexpr int kRetailLevelCount = 9;
constexpr int kExitUsage = 2;
constexpr int kExitRetail = 3;
constexpr int kExitMod = 4;
constexpr int kExitContract = 5;
constexpr int kExitReport = 6;

struct Options {
  std::wstring dataDirectory;
  std::vector<std::wstring> explicitModDirectories;
  std::wstring discoveryDirectory;
  std::vector<std::wstring> requestedMods;
  std::wstring reportPath;
  bool help = false;
};

struct RetailCatalog {
  std::wstring root;
  std::vector<std::wstring> levels;
};

class ModRuntimeScope {
 public:
  ~ModRuntimeScope() { RecoveredModRuntime_Release(); }
};

std::wstring JoinPath(const std::wstring& base,
                      const std::wstring& child) {
  if (base.empty()) return child;
  if (base.back() == L'\\' || base.back() == L'/') return base + child;
  return base + L"\\" + child;
}

std::wstring AbsolutePath(const std::wstring& path) {
  const DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
  if (required == 0) return path;
  std::vector<wchar_t> buffer(static_cast<std::size_t>(required));
  const DWORD copied =
      GetFullPathNameW(path.c_str(), required, buffer.data(), nullptr);
  return copied == 0 || copied >= required ? path
                                           : std::wstring(buffer.data());
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

std::string WideToUtf8(const std::wstring& text) {
  if (text.empty()) return std::string();
  const int required = WideCharToMultiByte(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
      nullptr, nullptr);
  if (required <= 0) return std::string();
  std::string result(static_cast<std::size_t>(required), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, text.data(),
                          static_cast<int>(text.size()), &result[0], required,
                          nullptr, nullptr) != required)
    return std::string();
  return result;
}

bool WideToSystemPath(const std::wstring& text, std::string* result) {
  if (text.empty() || result == nullptr) return false;
  BOOL usedDefault = FALSE;
  const int required = WideCharToMultiByte(
      CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(),
      static_cast<int>(text.size()), nullptr, 0, nullptr, &usedDefault);
  if (required <= 0 || usedDefault != FALSE) return false;
  result->assign(static_cast<std::size_t>(required), '\0');
  usedDefault = FALSE;
  return WideCharToMultiByte(
             CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(),
             static_cast<int>(text.size()), &(*result)[0], required, nullptr,
             &usedDefault) == required &&
         usedDefault == FALSE;
}

bool ParseValue(int argc, wchar_t** argv, int* index,
                const wchar_t* option, std::wstring* value,
                std::string* failure) {
  if (*index + 1 >= argc) {
    *failure = "missing value for " + WideToUtf8(option);
    return false;
  }
  *value = argv[++(*index)];
  if (value->empty()) {
    *failure = "empty value for " + WideToUtf8(option);
    return false;
  }
  return true;
}

bool ParseOptions(int argc, wchar_t** argv, Options* options,
                  std::string* failure) {
  for (int index = 1; index < argc; ++index) {
    const std::wstring argument(argv[index]);
    if (argument == L"--help" || argument == L"-h") {
      options->help = true;
    } else if (argument == L"--data-dir") {
      if (!options->dataDirectory.empty()) {
        *failure = "--data-dir may be specified only once";
        return false;
      }
      if (!ParseValue(argc, argv, &index, L"--data-dir",
                      &options->dataDirectory, failure))
        return false;
    } else if (argument.compare(0, 11, L"--data-dir=") == 0) {
      if (!options->dataDirectory.empty()) {
        *failure = "--data-dir may be specified only once";
        return false;
      }
      options->dataDirectory = argument.substr(11);
    } else if (argument == L"--mod-dir") {
      std::wstring value;
      if (!ParseValue(argc, argv, &index, L"--mod-dir", &value, failure))
        return false;
      options->explicitModDirectories.push_back(value);
    } else if (argument.compare(0, 10, L"--mod-dir=") == 0) {
      const std::wstring value = argument.substr(10);
      if (value.empty()) {
        *failure = "empty value for --mod-dir";
        return false;
      }
      options->explicitModDirectories.push_back(value);
    } else if (argument == L"--mods-dir") {
      if (!options->discoveryDirectory.empty()) {
        *failure = "--mods-dir may be specified only once";
        return false;
      }
      if (!ParseValue(argc, argv, &index, L"--mods-dir",
                      &options->discoveryDirectory, failure))
        return false;
    } else if (argument.compare(0, 11, L"--mods-dir=") == 0) {
      if (!options->discoveryDirectory.empty()) {
        *failure = "--mods-dir may be specified only once";
        return false;
      }
      options->discoveryDirectory = argument.substr(11);
    } else if (argument == L"--mod") {
      std::wstring value;
      if (!ParseValue(argc, argv, &index, L"--mod", &value, failure))
        return false;
      options->requestedMods.push_back(value);
    } else if (argument.compare(0, 6, L"--mod=") == 0) {
      const std::wstring value = argument.substr(6);
      if (value.empty()) {
        *failure = "empty value for --mod";
        return false;
      }
      options->requestedMods.push_back(value);
    } else if (argument == L"--report") {
      if (!options->reportPath.empty()) {
        *failure = "--report may be specified only once";
        return false;
      }
      if (!ParseValue(argc, argv, &index, L"--report",
                      &options->reportPath, failure))
        return false;
    } else if (argument.compare(0, 9, L"--report=") == 0) {
      if (!options->reportPath.empty()) {
        *failure = "--report may be specified only once";
        return false;
      }
      options->reportPath = argument.substr(9);
    } else {
      *failure = "unknown argument: " + WideToUtf8(argument);
      return false;
    }
  }
  if (!options->help && options->dataDirectory.empty()) {
    *failure = "--data-dir is required";
    return false;
  }
  return true;
}

bool Discover(const std::wstring& root,
              std::vector<std::wstring>* directories,
              std::string* failure) {
  if (!IsDirectory(root)) {
    *failure = "--mods-dir is not an existing directory: " +
               WideToUtf8(root);
    return false;
  }
  std::vector<std::wstring> names;
  WIN32_FIND_DATAW found = {};
  HANDLE search = FindFirstFileW(JoinPath(root, L"*").c_str(), &found);
  if (search == INVALID_HANDLE_VALUE) {
    *failure = "cannot enumerate --mods-dir: " + WideToUtf8(root);
    return false;
  }
  do {
    const std::wstring name(found.cFileName);
    if (name == L"." || name == L".." ||
        (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
      continue;
    if (IsFile(JoinPath(JoinPath(root, name), L"mod.json")))
      names.push_back(name);
  } while (FindNextFileW(search, &found) != FALSE);
  const DWORD enumerationError = GetLastError();
  FindClose(search);
  if (enumerationError != ERROR_NO_MORE_FILES) {
    *failure = "--mods-dir enumeration failed: " + WideToUtf8(root);
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

bool InspectRetail(const std::wstring& requested, RetailCatalog* catalog,
                   std::string* failure) {
  catalog->root = AbsolutePath(requested);
  if (!IsDirectory(catalog->root)) {
    *failure = "data directory does not exist: " + WideToUtf8(catalog->root);
    return false;
  }
  const std::wstring config = JoinPath(catalog->root, L"game.cfg");
  if (!IsFile(config) || !IsFile(JoinPath(catalog->root, L"LEVEL0.SC"))) {
    *failure = "data directory requires regular game.cfg and LEVEL0.SC";
    return false;
  }
  const int startLevel =
      GetPrivateProfileIntW(L"Init", L"StartLevel", -1, config.c_str());
  if (startLevel < 0 || startLevel >= kRetailLevelCount) {
    *failure = "game.cfg has an invalid Init/StartLevel";
    return false;
  }
  for (int index = 0; index < kRetailLevelCount; ++index) {
    wchar_t key[16] = {};
    std::swprintf(key, sizeof(key) / sizeof(key[0]), L"%d", index);
    wchar_t value[260] = {};
    if (GetPrivateProfileStringW(L"Levels", key, L"", value,
                                 sizeof(value) / sizeof(value[0]),
                                 config.c_str()) == 0) {
      *failure = "game.cfg is missing Levels/" + std::to_string(index);
      return false;
    }
    const std::wstring level(value);
    if (!IsDirectory(JoinPath(catalog->root, level))) {
      *failure = "configured Level directory is missing: " +
                 WideToUtf8(level);
      return false;
    }
    for (const std::wstring& existing : catalog->levels) {
      if (_wcsicmp(existing.c_str(), level.c_str()) == 0) {
        *failure = "game.cfg contains duplicate Level identities";
        return false;
      }
    }
    catalog->levels.push_back(level);
  }
  return true;
}

bool ValidateDerivedCatalog(const RetailCatalog& catalog,
                            std::string* failure) {
  std::vector<std::wstring> identities = catalog.levels;
  const unsigned int count = RecoveredModRuntime_LevelCount();
  for (unsigned int index = 0; index < count; ++index) {
    SRecoveredModLevel level;
    if (!RecoveredModRuntime_Level(index, &level)) {
      *failure = "mod Level catalog could not be enumerated";
      return false;
    }
    const std::wstring id(level.id, level.id + std::strlen(level.id));
    const std::wstring base(level.base, level.base + std::strlen(level.base));
    bool baseListed = false;
    for (const std::wstring& retail : catalog.levels)
      if (_wcsicmp(retail.c_str(), base.c_str()) == 0) baseListed = true;
    if (!baseListed) {
      *failure = "mod Level base is not listed in retail game.cfg: " +
                 std::string(level.base);
      return false;
    }
    for (const std::wstring& existing : identities) {
      if (_wcsicmp(existing.c_str(), id.c_str()) == 0) {
        *failure = "mod Level id collides with the active catalog: " +
                   std::string(level.id);
        return false;
      }
    }
    identities.push_back(id);
  }
  return true;
}

bool ReadOverlay(const char* target, std::string* text,
                 std::string* failure) {
  long length = -1;
  FILE* file = RecoveredModRuntime_OpenOverlayTarget(target, &length);
  if (file == nullptr || length < 0) {
    if (file != nullptr) std::fclose(file);
    *failure = std::string("cannot open declared reserved target: ") + target;
    return false;
  }
  try {
    text->assign(static_cast<std::size_t>(length), '\0');
  } catch (...) {
    std::fclose(file);
    *failure = std::string("cannot allocate reserved target: ") + target;
    return false;
  }
  const std::size_t read = length == 0
      ? 0
      : std::fread(&(*text)[0], 1, static_cast<std::size_t>(length), file);
  const bool valid = read == static_cast<std::size_t>(length) &&
                     std::ferror(file) == 0 && std::fclose(file) == 0;
  if (!valid) {
    *failure = std::string("cannot read declared reserved target: ") + target;
    return false;
  }
  return true;
}

bool ValidateReservedContracts(std::string* tuningStatus,
                               std::string* eventStatus,
                               std::string* failure) {
  *tuningStatus = "absent";
  *eventStatus = "absent";
  const char* tuningTarget = "RR2NW/gameplay-tuning.json";
  if (RecoveredModRuntime_HasOverlayTarget(tuningTarget)) {
    std::string text;
    char error[512] = {};
    if (!ReadOverlay(tuningTarget, &text, failure) ||
        !RecoveredGameplayTuning_ValidateText(
            text.data(), text.size(), error, sizeof(error))) {
      if (failure->empty())
        *failure = std::string("invalid gameplay-tuning contract: ") + error;
      return false;
    }
    *tuningStatus = "valid";
  }
  const char* eventTarget = "RR2NW/script-events.json";
  if (RecoveredModRuntime_HasOverlayTarget(eventTarget)) {
    std::string text;
    char error[512] = {};
    if (!ReadOverlay(eventTarget, &text, failure) ||
        !RecoveredScriptEvents_ValidateText(
            text.data(), text.size(), error, sizeof(error))) {
      if (failure->empty())
        *failure = std::string("invalid script-events contract: ") + error;
      return false;
    }
    *eventStatus = "valid";
  }
  return true;
}

std::string SafeValue(std::string value) {
  for (char& character : value)
    if (character == '\r' || character == '\n') character = ' ';
  return value;
}

bool WriteReport(const std::wstring& path, const std::string& report) {
  if (path.empty()) return true;
  FILE* file = _wfopen(AbsolutePath(path).c_str(), L"wb");
  if (file == nullptr) return false;
  const bool written =
      std::fwrite(report.data(), 1, report.size(), file) == report.size();
  return std::fclose(file) == 0 && written;
}

int Emit(const Options& options, int code, const std::string& stage,
         const std::string& error, const std::string& body = std::string()) {
  std::string report = "status=";
  report += code == 0 ? "valid\n" : "invalid\n";
  report += "stage=" + stage + "\n";
  if (!error.empty()) report += "error=" + SafeValue(error) + "\n";
  report += body;
  std::fwrite(report.data(), 1, report.size(),
              code == 0 ? stdout : stderr);
  if (!WriteReport(options.reportPath, report)) {
    std::fprintf(stderr, "status=invalid\nstage=report\n"
                         "error=could not write report\n");
    return kExitReport;
  }
  return code;
}

void PrintHelp() {
  std::puts(
      "rr2nw-mod-validator --data-dir <retail-root>\n"
      "  [--mod-dir <package>]... [--mods-dir <root>] [--mod <id>]...\n"
      "  [--report <path>]\n\n"
      "Selection and dependency semantics are identical to rr2nw.exe.\n"
      "A discovery root without --mod validates/activates every candidate.");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  Options options;
  std::string failure;
  if (!ParseOptions(argc, argv, &options, &failure))
    return Emit(options, kExitUsage, "arguments", failure);
  if (options.help) {
    PrintHelp();
    return 0;
  }

  RetailCatalog retail;
  if (!InspectRetail(options.dataDirectory, &retail, &failure))
    return Emit(options, kExitRetail, "retail", failure);

  for (std::wstring& directory : options.explicitModDirectories)
    directory = AbsolutePath(directory);
  if (!options.discoveryDirectory.empty())
    options.discoveryDirectory = AbsolutePath(options.discoveryDirectory);
  std::vector<std::wstring> candidates = options.explicitModDirectories;
  if (!options.discoveryDirectory.empty() &&
      !Discover(options.discoveryDirectory, &candidates, &failure))
    return Emit(options, kExitMod, "discovery", failure);

  std::string basePath;
  if (!WideToSystemPath(retail.root, &basePath))
    return Emit(options, kExitRetail, "retail",
                "data path is not representable by the Windows ANSI code page");
  std::vector<std::string> candidatePaths;
  std::vector<const char*> candidatePointers;
  for (const std::wstring& directory : candidates) {
    std::string path;
    if (!WideToSystemPath(directory, &path))
      return Emit(options, kExitMod, "mod",
                  "a mod path is not representable by the Windows ANSI code page");
    candidatePaths.push_back(path);
  }
  for (const std::string& path : candidatePaths)
    candidatePointers.push_back(path.c_str());
  std::vector<std::string> requestedIds;
  std::vector<const char*> requestedPointers;
  for (const std::wstring& requested : options.requestedMods) {
    std::string id;
    if (!WideToSystemPath(requested, &id))
      return Emit(options, kExitMod, "selection",
                  "a requested mod id is not representable by the Windows ANSI code page");
    requestedIds.push_back(id);
  }
  for (const std::string& id : requestedIds)
    requestedPointers.push_back(id.c_str());

  ModRuntimeScope runtime;
  const bool activateAll = !options.discoveryDirectory.empty() &&
                           options.requestedMods.empty();
  if (!RecoveredModRuntime_ConfigureStack(
          basePath.c_str(),
          candidatePointers.empty() ? nullptr : candidatePointers.data(),
          candidatePointers.size(), options.explicitModDirectories.size(),
          requestedPointers.empty() ? nullptr : requestedPointers.data(),
          requestedPointers.size(), activateAll)) {
    const std::string detail = RecoveredModRuntime_LastError();
    return Emit(options, kExitMod, "mod", detail,
                "issues=" +
                    std::to_string(RecoveredModRuntime_Issues()) + "\n");
  }
  if (!ValidateDerivedCatalog(retail, &failure))
    return Emit(options, kExitMod, "catalog", failure);
  std::string tuningStatus;
  std::string eventStatus;
  if (!ValidateReservedContracts(&tuningStatus, &eventStatus, &failure))
    return Emit(options, kExitContract, "reserved-contract", failure);

  const SRecoveredModRuntimeSummary* summary =
      RecoveredModRuntime_Summary();
  if (summary == nullptr)
    return Emit(options, kExitMod, "summary", "runtime returned no summary");
  std::string mountOrder;
  std::string packages;
  for (unsigned int index = 0; index < RecoveredModRuntime_ModCount(); ++index) {
    SRecoveredModPackage package;
    if (!RecoveredModRuntime_Mod(index, &package))
      return Emit(options, kExitMod, "summary",
                  "runtime package enumeration failed");
    if (!mountOrder.empty()) mountOrder += ',';
    mountOrder += std::string(package.id) + "@" + package.version;
    packages += "package_" + std::to_string(index) + "=" + package.id +
                "@" + package.version + ";files=" +
                std::to_string(package.fileCount) + ";levels=" +
                std::to_string(package.levelCount) + ";bytes=" +
                std::to_string(package.totalBytes) + ";fingerprint=" +
                std::to_string(package.fingerprint) + "\n";
  }
  std::string body;
  body += "retail_levels=9\n";
  body += "candidates=" + std::to_string(summary->candidateCount) + "\n";
  body += "mods=" + std::to_string(summary->modCount) + "\n";
  body += "files=" + std::to_string(summary->fileCount) + "\n";
  body += "levels=" + std::to_string(summary->levelCount) + "\n";
  body += "bytes=" + std::to_string(summary->totalBytes) + "\n";
  body += "fingerprint=" + std::to_string(summary->modFingerprint) + "\n";
  body += "mount_order=" + mountOrder + "\n";
  body += packages;
  body += "gameplay_tuning=" + tuningStatus + "\n";
  body += "script_events=" + eventStatus + "\n";
  return Emit(options, 0, "complete", std::string(), body);
}
