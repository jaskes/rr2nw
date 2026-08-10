#include "RecoveredModProfile.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

constexpr std::uint32_t kProfileVersion = 1u;
constexpr std::size_t kMaximumLines = 1200u;
constexpr std::size_t kMaximumLineBytes = 256u;
constexpr std::uint64_t kHashOffset = 14695981039346656037ull;
constexpr std::uint64_t kHashPrime = 1099511628211ull;

struct RuntimeState {
  bool configured = false;
  bool profileMode = false;
  bool safeMode = false;
  bool cliOverride = false;
  bool profileMissing = false;
  bool corruptRecovered = false;
  std::wstring profilePath;
  std::string baseRoot;
  std::vector<std::string> candidateDirectories;
  std::vector<SRecoveredModCandidateInfo> candidateInfo;
  std::vector<std::string> cliRequestedIds;
  std::size_t explicitDirectoryCount = 0;
  bool cliActivateAll = false;
  SRecoveredModProfileCatalog committed;
  SRecoveredModProfileCatalog staged;
  SRecoveredModStackPlan startupPlan;
  SRecoveredModStackPlan stagedPlan;
  SRecoveredModSelectorSnapshot snapshot;
};

RuntimeState g_state;
bool g_failNextAtomicCommit = false;

void Fail(SRecoveredModProfileStatus* status,
          ERecoveredModProfileError error, std::size_t line,
          const std::string& detail) {
  if (status == nullptr) return;
  status->error = error;
  status->line = line;
  status->detail = detail;
}

void Succeed(SRecoveredModProfileStatus* status) {
  if (status != nullptr) *status = {};
}

std::uint64_t Fingerprint(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t hash = kHashOffset;
  for (std::uint8_t value : bytes) {
    hash ^= value;
    hash *= kHashPrime;
  }
  return hash == 0u ? 1u : hash;
}

bool ValidToken(const std::string& value) {
  if (value.empty() || value.size() > 64u) return false;
  for (char character : value) {
    if (!((character >= 'a' && character <= 'z') ||
          (character >= '0' && character <= '9') || character == '.' ||
          character == '_' || character == '-'))
      return false;
  }
  return true;
}

bool ParseUnsigned(const std::string& value, std::size_t maximum,
                   std::size_t* result) {
  if (result == nullptr || value.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
  if (errno != 0 || end == value.c_str() || *end != '\0' ||
      parsed > maximum)
    return false;
  *result = static_cast<std::size_t>(parsed);
  return true;
}

bool ReadAssignment(const std::string& line, const std::string& key,
                    std::string* value) {
  const std::string prefix = key + "=";
  if (line.compare(0, prefix.size(), prefix) != 0) return false;
  *value = line.substr(prefix.size());
  return !value->empty();
}

bool Canonicalize(SRecoveredModProfileCatalog* catalog,
                  SRecoveredModProfileStatus* status) {
  if (catalog == nullptr || catalog->version != kProfileVersion ||
      catalog->profiles.empty() ||
      catalog->profiles.size() > kRecoveredModProfileMaximumProfiles ||
      !ValidToken(catalog->activeProfile)) {
    Fail(status, ERecoveredModProfileError::Malformed, 0,
         "mod profile catalog shape is invalid");
    return false;
  }
  std::vector<std::string> names;
  for (SRecoveredModProfileEntry& profile : catalog->profiles) {
    if (!ValidToken(profile.name)) {
      Fail(status, ERecoveredModProfileError::InvalidName, 0,
           "profile name must use lowercase ASCII [a-z0-9._-]");
      return false;
    }
    if (profile.selectedIds.size() > kRecoveredModProfileMaximumSelected) {
      Fail(status, ERecoveredModProfileError::Malformed, 0,
           "profile selects more than 64 packages");
      return false;
    }
    std::sort(profile.selectedIds.begin(), profile.selectedIds.end());
    for (const std::string& id : profile.selectedIds) {
      if (!ValidToken(id)) {
        Fail(status, ERecoveredModProfileError::InvalidPackageId, 0,
             "selected package id is invalid");
        return false;
      }
    }
    if (std::adjacent_find(profile.selectedIds.begin(),
                           profile.selectedIds.end()) !=
        profile.selectedIds.end()) {
      Fail(status, ERecoveredModProfileError::DuplicatePackage, 0,
           "profile contains a duplicate package id");
      return false;
    }
    names.push_back(profile.name);
  }
  std::sort(names.begin(), names.end());
  if (std::adjacent_find(names.begin(), names.end()) != names.end()) {
    Fail(status, ERecoveredModProfileError::DuplicateProfile, 0,
         "catalog contains a duplicate profile name");
    return false;
  }
  if (std::find(names.begin(), names.end(), catalog->activeProfile) ==
      names.end()) {
    Fail(status, ERecoveredModProfileError::MissingActiveProfile, 0,
         "active profile is not present in the catalog");
    return false;
  }
  return true;
}

SRecoveredModProfileCatalog DefaultCatalog() {
  SRecoveredModProfileCatalog catalog;
  catalog.version = kProfileVersion;
  catalog.activeProfile = "default";
  catalog.profiles.push_back({"default", {}});
  std::vector<std::uint8_t> bytes;
  SRecoveredModProfileStatus status;
  if (RecoveredModProfile_Encode(catalog, &bytes, &status))
    catalog.fingerprint = Fingerprint(bytes);
  return catalog;
}

std::size_t FindProfile(const SRecoveredModProfileCatalog& catalog,
                        const std::string& name) {
  for (std::size_t index = 0; index < catalog.profiles.size(); ++index)
    if (catalog.profiles[index].name == name) return index;
  return catalog.profiles.size();
}

std::vector<std::string> ActiveIds(
    const SRecoveredModProfileCatalog& catalog) {
  const std::size_t index = FindProfile(catalog, catalog.activeProfile);
  return index < catalog.profiles.size()
      ? catalog.profiles[index].selectedIds
      : std::vector<std::string>();
}

std::vector<const char*> Pointers(const std::vector<std::string>& values) {
  std::vector<const char*> pointers;
  pointers.reserve(values.size());
  for (const std::string& value : values) pointers.push_back(value.c_str());
  return pointers;
}

std::vector<const char*> CandidatePointers() {
  return Pointers(g_state.candidateDirectories);
}

bool SameMount(const SRecoveredModStackPlan& left,
               const SRecoveredModStackPlan& right) {
  if (!left.ready || !right.ready ||
      left.modFingerprint != right.modFingerprint ||
      left.packages.size() != right.packages.size())
    return false;
  for (std::size_t index = 0; index < left.packages.size(); ++index)
    if (std::strcmp(left.packages[index].id, right.packages[index].id) != 0 ||
        std::strcmp(left.packages[index].version,
                    right.packages[index].version) != 0)
      return false;
  return true;
}

bool BuildPlan(const std::vector<std::string>& requested,
               std::size_t explicitCount, bool activateAll,
               SRecoveredModStackPlan* plan) {
  const std::vector<const char*> candidates = CandidatePointers();
  const std::vector<const char*> requestedPointers = Pointers(requested);
  return RecoveredModRuntime_PlanStack(
      g_state.baseRoot.c_str(),
      candidates.empty() ? nullptr : candidates.data(), candidates.size(),
      explicitCount,
      requestedPointers.empty() ? nullptr : requestedPointers.data(),
      requestedPointers.size(), activateAll, plan);
}

void RefreshSnapshot() {
  SRecoveredModSelectorSnapshot snapshot;
  snapshot.configured = g_state.configured;
  snapshot.safeMode = g_state.safeMode;
  snapshot.cliOverride = g_state.cliOverride;
  snapshot.profileMissing = g_state.profileMissing;
  snapshot.corruptProfileRecovered = g_state.corruptRecovered;
  snapshot.startupFingerprint = g_state.startupPlan.modFingerprint;
  snapshot.catalogFingerprint = g_state.committed.fingerprint;
  snapshot.source = g_state.safeMode
      ? "safe-mode"
      : g_state.cliOverride ? "command-line" : "profile";
  for (const SRecoveredModProfileEntry& profile : g_state.staged.profiles)
    snapshot.profileNames.push_back(profile.name);
  snapshot.activeProfile = g_state.committed.activeProfile;
  snapshot.stagedProfile = g_state.staged.activeProfile;
  snapshot.activeProfileIndex =
      FindProfile(g_state.committed, g_state.committed.activeProfile);
  snapshot.stagedProfileIndex =
      FindProfile(g_state.staged, g_state.staged.activeProfile);

  std::vector<std::string> requested;
  std::size_t explicitCount = 0u;
  bool activateAll = false;
  if (g_state.safeMode) {
    g_state.stagedPlan = {};
    g_state.stagedPlan.ready = true;
  } else if (g_state.cliOverride) {
    requested = g_state.cliRequestedIds;
    explicitCount = g_state.explicitDirectoryCount;
    activateAll = g_state.cliActivateAll;
    BuildPlan(requested, explicitCount, activateAll, &g_state.stagedPlan);
  } else {
    requested = ActiveIds(g_state.staged);
    BuildPlan(requested, 0u, false, &g_state.stagedPlan);
  }
  snapshot.planReady = g_state.stagedPlan.ready;
  snapshot.stagedFingerprint = g_state.stagedPlan.modFingerprint;
  snapshot.activePackageCount =
      static_cast<unsigned int>(g_state.stagedPlan.packages.size());
  snapshot.reason = g_state.safeMode
      ? "Safe mode disables all user mods"
      : g_state.cliOverride
            ? "Command-line mod selection overrides persisted profiles"
            : g_state.stagedPlan.ready
                  ? "Resolved by the production dependency/mount owner"
                  : g_state.stagedPlan.reason;

  std::vector<std::uint8_t> committedBytes;
  std::vector<std::uint8_t> stagedBytes;
  SRecoveredModProfileStatus status;
  const bool comparable =
      RecoveredModProfile_Encode(g_state.committed, &committedBytes, &status) &&
      RecoveredModProfile_Encode(g_state.staged, &stagedBytes, &status);
  snapshot.dirty = !g_state.safeMode && !g_state.cliOverride &&
      (!comparable || committedBytes != stagedBytes);
  snapshot.restartRequired = g_state.snapshot.restartRequired;
  if (g_state.configured && !g_state.safeMode && !g_state.cliOverride &&
      !snapshot.dirty && g_state.stagedPlan.ready)
    snapshot.restartRequired =
        !SameMount(g_state.startupPlan, g_state.stagedPlan);
  snapshot.writes = g_state.snapshot.writes;
  snapshot.blockedCommits = g_state.snapshot.blockedCommits;
  snapshot.status = g_state.snapshot.status;

  const std::vector<std::string> explicitIds =
      g_state.cliOverride ? g_state.cliRequestedIds : ActiveIds(g_state.staged);
  for (std::size_t index = 0; index < g_state.candidateInfo.size(); ++index) {
    const SRecoveredModCandidateInfo& info = g_state.candidateInfo[index];
    SRecoveredModSelectorCandidate candidate;
    candidate.id = info.valid ? info.id
                              : "invalid-candidate-" + std::to_string(index + 1u);
    candidate.version = info.valid ? info.version : "?";
    candidate.issue = info.issue;
    candidate.reason = info.reason;
    if (!info.valid) {
      candidate.state = RECOVERED_MOD_SELECTOR_INVALID;
      ++snapshot.invalidCandidateCount;
    } else {
      candidate.explicitlySelected =
          std::find(explicitIds.begin(), explicitIds.end(), candidate.id) !=
              explicitIds.end() ||
          (g_state.cliOverride && index < g_state.explicitDirectoryCount) ||
          (g_state.cliOverride && g_state.cliActivateAll);
      for (const SRecoveredModPackage& mounted :
           g_state.stagedPlan.packages) {
        if (candidate.id == mounted.id) {
          candidate.active = true;
          candidate.mountIndex = mounted.mountIndex;
          break;
        }
      }
      if (!g_state.stagedPlan.ready && candidate.explicitlySelected) {
        candidate.state = RECOVERED_MOD_SELECTOR_BLOCKED;
        candidate.reason = g_state.stagedPlan.reason;
      } else if (candidate.explicitlySelected) {
        candidate.state = RECOVERED_MOD_SELECTOR_SELECTED;
        candidate.reason = g_state.cliOverride
            ? "selected by the command line"
            : "selected by the active profile";
      } else if (candidate.active) {
        candidate.state = RECOVERED_MOD_SELECTOR_DEPENDENCY;
        candidate.reason = "enabled by dependency closure";
      } else {
        candidate.state = RECOVERED_MOD_SELECTOR_DISABLED;
        candidate.reason = "disabled";
      }
    }
    snapshot.candidates.push_back(std::move(candidate));
  }
  std::sort(snapshot.candidates.begin(), snapshot.candidates.end(),
            [](const SRecoveredModSelectorCandidate& left,
               const SRecoveredModSelectorCandidate& right) {
              return left.id < right.id;
            });
  snapshot.candidateCount =
      static_cast<unsigned int>(snapshot.candidates.size());
  for (const SRecoveredModPackage& package : g_state.stagedPlan.packages)
    snapshot.mountOrder.push_back(package.id);
  if (snapshot.status.empty()) snapshot.status = snapshot.reason;
  g_state.snapshot = std::move(snapshot);
}

bool ReadBoundedFile(const std::wstring& path,
                     std::vector<std::uint8_t>* bytes,
                     SRecoveredModProfileStatus* status) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    Fail(status, ERecoveredModProfileError::FileUnavailable, 0,
         "mod profile file is unavailable");
    return false;
  }
  LARGE_INTEGER size = {};
  if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart <= 0 ||
      static_cast<unsigned long long>(size.QuadPart) >
          kRecoveredModProfileMaximumBytes) {
    CloseHandle(file);
    Fail(status, size.QuadPart == 0
                     ? ERecoveredModProfileError::EmptyInput
                     : ERecoveredModProfileError::InputTooLarge,
         0, "mod profile file size is invalid");
    return false;
  }
  std::vector<std::uint8_t> candidate(
      static_cast<std::size_t>(size.QuadPart));
  DWORD read = 0;
  const bool ok = ReadFile(file, candidate.data(),
                           static_cast<DWORD>(candidate.size()), &read,
                           nullptr) != FALSE &&
      read == candidate.size();
  CloseHandle(file);
  if (!ok) {
    Fail(status, ERecoveredModProfileError::FileUnavailable, 0,
         "mod profile file could not be read completely");
    return false;
  }
  *bytes = std::move(candidate);
  return true;
}

}  // namespace

bool RecoveredModProfile_Decode(
    const std::vector<std::uint8_t>& bytes,
    SRecoveredModProfileCatalog* catalog,
    SRecoveredModProfileStatus* status) {
  if (catalog == nullptr) {
    Fail(status, ERecoveredModProfileError::InvalidArgument, 0,
         "mod profile destination is null");
    return false;
  }
  if (bytes.empty()) {
    Fail(status, ERecoveredModProfileError::EmptyInput, 0,
         "mod profile is empty");
    return false;
  }
  if (bytes.size() > kRecoveredModProfileMaximumBytes) {
    Fail(status, ERecoveredModProfileError::InputTooLarge, 0,
         "mod profile exceeds 64 KiB");
    return false;
  }
  if (std::find(bytes.begin(), bytes.end(), 0u) != bytes.end()) {
    Fail(status, ERecoveredModProfileError::EmbeddedNul, 0,
         "mod profile contains an embedded NUL");
    return false;
  }
  std::istringstream stream(std::string(bytes.begin(), bytes.end()));
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() > kMaximumLineBytes || lines.size() >= kMaximumLines) {
      Fail(status, ERecoveredModProfileError::Malformed, lines.size() + 1u,
           "mod profile line/count limit exceeded");
      return false;
    }
    lines.push_back(line);
  }
  if (lines.empty() || lines[0] != "RR2MODPROFILE1") {
    Fail(status, ERecoveredModProfileError::InvalidHeader, 1,
         "mod profile header is invalid");
    return false;
  }
  std::size_t cursor = 1u;
  std::string value;
  std::size_t numeric = 0u;
  if (cursor >= lines.size() ||
      !ReadAssignment(lines[cursor], "version", &value) ||
      !ParseUnsigned(value, kProfileVersion, &numeric) ||
      numeric != kProfileVersion) {
    Fail(status, ERecoveredModProfileError::UnsupportedVersion, cursor + 1u,
         "mod profile version is unsupported");
    return false;
  }
  ++cursor;
  SRecoveredModProfileCatalog candidate;
  candidate.version = kProfileVersion;
  if (cursor >= lines.size() ||
      !ReadAssignment(lines[cursor], "active", &candidate.activeProfile)) {
    Fail(status, ERecoveredModProfileError::Malformed, cursor + 1u,
         "active profile field is missing");
    return false;
  }
  ++cursor;
  std::size_t profileCount = 0u;
  if (cursor >= lines.size() ||
      !ReadAssignment(lines[cursor], "profiles", &value) ||
      !ParseUnsigned(value, kRecoveredModProfileMaximumProfiles,
                     &profileCount) || profileCount == 0u) {
    Fail(status, ERecoveredModProfileError::Malformed, cursor + 1u,
         "profile count is invalid");
    return false;
  }
  ++cursor;
  for (std::size_t profileIndex = 0; profileIndex < profileCount;
       ++profileIndex) {
    SRecoveredModProfileEntry profile;
    const std::string prefix = "profile_" + std::to_string(profileIndex);
    if (cursor >= lines.size() ||
        !ReadAssignment(lines[cursor], prefix + "_name", &profile.name)) {
      Fail(status, ERecoveredModProfileError::Malformed, cursor + 1u,
           "profile name field is missing or out of order");
      return false;
    }
    ++cursor;
    std::size_t selectedCount = 0u;
    if (cursor >= lines.size() ||
        !ReadAssignment(lines[cursor], prefix + "_mods", &value) ||
        !ParseUnsigned(value, kRecoveredModProfileMaximumSelected,
                       &selectedCount)) {
      Fail(status, ERecoveredModProfileError::Malformed, cursor + 1u,
           "profile package count is invalid");
      return false;
    }
    ++cursor;
    for (std::size_t selectedIndex = 0; selectedIndex < selectedCount;
         ++selectedIndex) {
      std::string id;
      if (cursor >= lines.size() ||
          !ReadAssignment(lines[cursor],
                          prefix + "_mod_" +
                              std::to_string(selectedIndex),
                          &id)) {
        Fail(status, ERecoveredModProfileError::Malformed, cursor + 1u,
             "profile package row is missing or out of order");
        return false;
      }
      profile.selectedIds.push_back(std::move(id));
      ++cursor;
    }
    candidate.profiles.push_back(std::move(profile));
  }
  if (cursor != lines.size() || !Canonicalize(&candidate, status)) {
    if (status != nullptr && status->error == ERecoveredModProfileError::None)
      Fail(status, ERecoveredModProfileError::Malformed, cursor + 1u,
           "mod profile contains trailing data");
    return false;
  }
  std::vector<std::uint8_t> canonical;
  if (!RecoveredModProfile_Encode(candidate, &canonical, status)) return false;
  candidate.fingerprint = Fingerprint(canonical);
  *catalog = std::move(candidate);
  Succeed(status);
  return true;
}

bool RecoveredModProfile_Encode(
    const SRecoveredModProfileCatalog& catalog,
    std::vector<std::uint8_t>* bytes,
    SRecoveredModProfileStatus* status) {
  if (bytes == nullptr) {
    Fail(status, ERecoveredModProfileError::InvalidArgument, 0,
         "mod profile byte destination is null");
    return false;
  }
  SRecoveredModProfileCatalog canonical = catalog;
  if (!Canonicalize(&canonical, status)) return false;
  std::ostringstream output;
  output << "RR2MODPROFILE1\r\n"
         << "version=" << kProfileVersion << "\r\n"
         << "active=" << canonical.activeProfile << "\r\n"
         << "profiles=" << canonical.profiles.size() << "\r\n";
  for (std::size_t profileIndex = 0;
       profileIndex < canonical.profiles.size(); ++profileIndex) {
    const SRecoveredModProfileEntry& profile =
        canonical.profiles[profileIndex];
    output << "profile_" << profileIndex << "_name=" << profile.name
           << "\r\n"
           << "profile_" << profileIndex << "_mods="
           << profile.selectedIds.size() << "\r\n";
    for (std::size_t selectedIndex = 0;
         selectedIndex < profile.selectedIds.size(); ++selectedIndex)
      output << "profile_" << profileIndex << "_mod_" << selectedIndex
             << "=" << profile.selectedIds[selectedIndex] << "\r\n";
  }
  const std::string text = output.str();
  if (text.size() > kRecoveredModProfileMaximumBytes) {
    Fail(status, ERecoveredModProfileError::InputTooLarge, 0,
         "canonical mod profile exceeds 64 KiB");
    return false;
  }
  bytes->assign(text.begin(), text.end());
  Succeed(status);
  return true;
}

bool RecoveredModProfile_Read(
    const std::wstring& path, SRecoveredModProfileCatalog* catalog,
    SRecoveredModProfileStatus* status) {
  std::vector<std::uint8_t> bytes;
  return ReadBoundedFile(path, &bytes, status) &&
      RecoveredModProfile_Decode(bytes, catalog, status);
}

bool RecoveredModProfile_WriteAtomic(
    const std::wstring& path, const SRecoveredModProfileCatalog& catalog,
    SRecoveredModProfileStatus* status) {
  if (path.empty()) {
    Fail(status, ERecoveredModProfileError::InvalidArgument, 0,
         "mod profile path is empty");
    return false;
  }
  std::vector<std::uint8_t> bytes;
  if (!RecoveredModProfile_Encode(catalog, &bytes, status)) return false;
  const std::wstring temporary = path + L".tmp";
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    Fail(status, ERecoveredModProfileError::AtomicCommitFailed, 0,
         "mod profile temporary file could not be created");
    return false;
  }
  DWORD written = 0;
  const bool body = WriteFile(file, bytes.data(),
                              static_cast<DWORD>(bytes.size()), &written,
                              nullptr) != FALSE &&
      written == bytes.size() && FlushFileBuffers(file) != FALSE;
  CloseHandle(file);
  const bool injected = g_failNextAtomicCommit;
  g_failNextAtomicCommit = false;
  if (!body || injected ||
      MoveFileExW(temporary.c_str(), path.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ==
          FALSE) {
    DeleteFileW(temporary.c_str());
    Fail(status, ERecoveredModProfileError::AtomicCommitFailed, 0,
         "mod profile atomic replacement failed");
    return false;
  }
  Succeed(status);
  return true;
}

bool RecoveredModProfile_Configure(
    const std::wstring& profilePath, const char* baseRoot,
    const char* const* candidateDirectories, std::size_t candidateCount,
    std::size_t explicitDirectoryCount, const char* const* requestedIds,
    std::size_t requestedIdCount, bool activateAllCandidates,
    bool profileMode, bool safeMode) {
  RecoveredModProfile_Release();
  if (profilePath.empty() || baseRoot == nullptr || baseRoot[0] == '\0' ||
      candidateCount > 128u || explicitDirectoryCount > candidateCount ||
      (candidateCount != 0u && candidateDirectories == nullptr) ||
      (requestedIdCount != 0u && requestedIds == nullptr))
    return false;
  g_state.profilePath = profilePath;
  g_state.baseRoot = baseRoot;
  g_state.profileMode = profileMode;
  g_state.safeMode = safeMode;
  g_state.cliOverride = !profileMode && !safeMode;
  g_state.explicitDirectoryCount = explicitDirectoryCount;
  g_state.cliActivateAll = activateAllCandidates;
  for (std::size_t index = 0; index < candidateCount; ++index) {
    if (candidateDirectories[index] == nullptr) return false;
    g_state.candidateDirectories.push_back(candidateDirectories[index]);
  }
  for (std::size_t index = 0; index < requestedIdCount; ++index) {
    if (requestedIds[index] == nullptr) return false;
    g_state.cliRequestedIds.push_back(requestedIds[index]);
  }
  if (!safeMode) {
    for (const std::string& directory : g_state.candidateDirectories) {
      SRecoveredModCandidateInfo info;
      (void)RecoveredModRuntime_InspectCandidate(
          g_state.baseRoot.c_str(), directory.c_str(), &info);
      g_state.candidateInfo.push_back(info);
    }
  }

  g_state.committed = DefaultCatalog();
  if (profileMode && !safeMode) {
    SRecoveredModProfileStatus status;
    if (!RecoveredModProfile_Read(profilePath, &g_state.committed, &status)) {
      if (status.error == ERecoveredModProfileError::FileUnavailable) {
        g_state.profileMissing = true;
      } else {
        g_state.corruptRecovered = true;
      }
      g_state.committed = DefaultCatalog();
    }
  }
  g_state.staged = g_state.committed;
  std::vector<std::string> requested;
  std::size_t explicitCount = 0u;
  bool activateAll = false;
  if (safeMode) {
    g_state.startupPlan.ready = true;
  } else if (g_state.cliOverride) {
    requested = g_state.cliRequestedIds;
    explicitCount = g_state.explicitDirectoryCount;
    activateAll = g_state.cliActivateAll;
    (void)BuildPlan(requested, explicitCount, activateAll,
                    &g_state.startupPlan);
  } else {
    requested = ActiveIds(g_state.committed);
    (void)BuildPlan(requested, 0u, false, &g_state.startupPlan);
  }
  g_state.configured = true;
  g_state.snapshot.status = safeMode
      ? "Safe mode: user mods disabled"
      : g_state.corruptRecovered
            ? "Corrupt profile recovered to disabled default"
            : g_state.profileMissing
                  ? "No profile file; disabled default staged"
                  : g_state.startupPlan.ready
                        ? "Mod selection resolved"
                        : "Profile rejected; user mods disabled";
  RefreshSnapshot();
  return true;
}

void RecoveredModProfile_Release() {
  g_state = RuntimeState{};
  g_failNextAtomicCommit = false;
}

const SRecoveredModSelectorSnapshot* RecoveredModProfile_Snapshot() {
  return g_state.configured ? &g_state.snapshot : nullptr;
}

bool RecoveredModProfile_StartupSelection(
    std::vector<std::string>* requestedIds, bool* activateAllCandidates) {
  if (!g_state.configured || requestedIds == nullptr ||
      activateAllCandidates == nullptr)
    return false;
  requestedIds->clear();
  *activateAllCandidates = false;
  if (g_state.safeMode) return true;
  if (g_state.cliOverride) {
    *requestedIds = g_state.cliRequestedIds;
    *activateAllCandidates = g_state.cliActivateAll;
    return true;
  }
  if (!g_state.startupPlan.ready) return true;
  *requestedIds = ActiveIds(g_state.committed);
  return true;
}

bool RecoveredModProfile_SelectRelative(int direction) {
  if (!g_state.configured || g_state.safeMode || g_state.cliOverride ||
      direction == 0 || g_state.staged.profiles.empty())
    return false;
  std::size_t index = FindProfile(g_state.staged, g_state.staged.activeProfile);
  if (index >= g_state.staged.profiles.size()) index = 0u;
  const std::size_t count = g_state.staged.profiles.size();
  index = direction > 0 ? (index + 1u) % count
                        : (index + count - 1u) % count;
  g_state.staged.activeProfile = g_state.staged.profiles[index].name;
  g_state.snapshot.status = "Profile staged; apply for the next launch";
  RefreshSnapshot();
  return true;
}

bool RecoveredModProfile_ToggleCandidate(std::size_t index) {
  if (!g_state.configured || g_state.safeMode || g_state.cliOverride ||
      index >= g_state.snapshot.candidates.size())
    return false;
  const SRecoveredModSelectorCandidate& candidate =
      g_state.snapshot.candidates[index];
  if (candidate.state == RECOVERED_MOD_SELECTOR_INVALID) return false;
  const std::size_t profileIndex =
      FindProfile(g_state.staged, g_state.staged.activeProfile);
  if (profileIndex >= g_state.staged.profiles.size()) return false;
  std::vector<std::string>& ids =
      g_state.staged.profiles[profileIndex].selectedIds;
  const auto found = std::lower_bound(ids.begin(), ids.end(), candidate.id);
  if (found != ids.end() && *found == candidate.id)
    ids.erase(found);
  else
    ids.insert(found, candidate.id);
  g_state.snapshot.status = "Package selection staged; live world unchanged";
  RefreshSnapshot();
  return true;
}

bool RecoveredModProfile_ResetStaged() {
  if (!g_state.configured || g_state.safeMode || g_state.cliOverride)
    return false;
  const std::size_t index =
      FindProfile(g_state.staged, g_state.staged.activeProfile);
  if (index >= g_state.staged.profiles.size()) return false;
  g_state.staged.profiles[index].selectedIds.clear();
  g_state.snapshot.status = "Active profile staged as vanilla";
  RefreshSnapshot();
  return true;
}

bool RecoveredModProfile_CommitStaged() {
  if (!g_state.configured || g_state.safeMode || g_state.cliOverride ||
      !g_state.snapshot.dirty || !g_state.stagedPlan.ready) {
    ++g_state.snapshot.blockedCommits;
    g_state.snapshot.status = !g_state.stagedPlan.ready
        ? "Cannot apply: " + std::string(g_state.stagedPlan.reason)
        : "No writable profile change is staged";
    return false;
  }
  SRecoveredModProfileStatus status;
  if (!RecoveredModProfile_WriteAtomic(
          g_state.profilePath, g_state.staged, &status)) {
    g_state.snapshot.status = status.detail;
    return false;
  }
  g_state.committed = g_state.staged;
  std::vector<std::uint8_t> bytes;
  if (RecoveredModProfile_Encode(g_state.committed, &bytes, &status))
    g_state.committed.fingerprint = Fingerprint(bytes);
  const unsigned int writes = g_state.snapshot.writes + 1u;
  const bool restart = !SameMount(g_state.startupPlan, g_state.stagedPlan);
  g_state.snapshot.writes = writes;
  g_state.snapshot.restartRequired = restart;
  g_state.snapshot.status = restart
      ? "Profile saved; restart the game to apply it"
      : "Profile saved; mounted content is unchanged";
  RefreshSnapshot();
  g_state.snapshot.writes = writes;
  g_state.snapshot.restartRequired = restart;
  g_state.snapshot.status = restart
      ? "Profile saved; restart the game to apply it"
      : "Profile saved; mounted content is unchanged";
  return true;
}

void RecoveredModProfile_FailNextAtomicCommitForTesting() {
  g_failNextAtomicCommit = true;
}

const char* RecoveredModProfile_ErrorName(ERecoveredModProfileError error) {
  switch (error) {
    case ERecoveredModProfileError::None: return "none";
    case ERecoveredModProfileError::InvalidArgument: return "invalid-argument";
    case ERecoveredModProfileError::FileUnavailable: return "file-unavailable";
    case ERecoveredModProfileError::EmptyInput: return "empty-input";
    case ERecoveredModProfileError::InputTooLarge: return "input-too-large";
    case ERecoveredModProfileError::EmbeddedNul: return "embedded-nul";
    case ERecoveredModProfileError::InvalidHeader: return "invalid-header";
    case ERecoveredModProfileError::UnsupportedVersion:
      return "unsupported-version";
    case ERecoveredModProfileError::Malformed: return "malformed";
    case ERecoveredModProfileError::InvalidName: return "invalid-name";
    case ERecoveredModProfileError::InvalidPackageId:
      return "invalid-package-id";
    case ERecoveredModProfileError::DuplicateProfile:
      return "duplicate-profile";
    case ERecoveredModProfileError::DuplicatePackage:
      return "duplicate-package";
    case ERecoveredModProfileError::MissingActiveProfile:
      return "missing-active-profile";
    case ERecoveredModProfileError::AtomicCommitFailed:
      return "atomic-commit-failed";
  }
  return "unknown";
}
