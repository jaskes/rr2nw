#include "RecoveredModProfile.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

std::string Join(const std::string& base, const std::string& child) {
  return base + "\\" + child;
}

std::wstring Wide(const std::string& value) {
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       value.c_str(), -1, nullptr, 0);
  if (size <= 1) return {};
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1,
                          &result[0], size) == 0)
    return {};
  result.pop_back();
  return result;
}

bool MakeDirectory(const std::string& path) {
  return CreateDirectoryA(path.c_str(), nullptr) != FALSE ||
         GetLastError() == ERROR_ALREADY_EXISTS;
}

bool Write(const std::string& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  return output && output.write(text.data(),
                                static_cast<std::streamsize>(text.size())).good();
}

std::string Read(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return input ? std::string((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>())
               : std::string();
}

std::string Manifest(const std::string& id, const std::string& source,
                     const std::string& target,
                     const std::string& relations = std::string()) {
  return "{\n"
         "  \"schema\": 1,\n"
         "  \"engine_api\": 1,\n"
         "  \"id\": \"" + id + "\",\n"
         "  \"version\": \"1.0.0\",\n" + relations +
         "  \"files\": [{\"source\": \"" + source +
         "\", \"target\": \"" + target + "\"}]\n"
         "}\n";
}

int Fail(const char* message) {
  const SRecoveredModSelectorSnapshot* snapshot =
      RecoveredModProfile_Snapshot();
  std::fprintf(stderr,
               "recovered mod profile smoke: %s; source=%s status=%s "
               "missing=%u corrupt=%u candidates=%u active=%u ready=%u "
               "reason=%s\n",
               message, snapshot == nullptr ? "none" : snapshot->source.c_str(),
               snapshot == nullptr ? "none" : snapshot->status.c_str(),
               snapshot != nullptr && snapshot->profileMissing ? 1u : 0u,
               snapshot != nullptr && snapshot->corruptProfileRecovered ? 1u : 0u,
               snapshot == nullptr ? 0u : snapshot->candidateCount,
               snapshot == nullptr ? 0u : snapshot->activePackageCount,
               snapshot != nullptr && snapshot->planReady ? 1u : 0u,
               snapshot == nullptr ? "none" : snapshot->reason.c_str());
  RecoveredModProfile_Release();
  RecoveredModRuntime_Release();
  return 1;
}

std::vector<std::uint8_t> Bytes(const std::string& text) {
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::size_t CandidateIndex(const SRecoveredModSelectorSnapshot& snapshot,
                           const char* id) {
  for (std::size_t index = 0; index < snapshot.candidates.size(); ++index)
    if (snapshot.candidates[index].id == id) return index;
  return snapshot.candidates.size();
}

bool MountIs(const SRecoveredModSelectorSnapshot& snapshot,
             std::initializer_list<const char*> ids) {
  if (snapshot.mountOrder.size() != ids.size()) return false;
  std::size_t index = 0u;
  for (const char* id : ids)
    if (snapshot.mountOrder[index++] != id) return false;
  return true;
}

bool CodecGate() {
  SRecoveredModProfileCatalog catalog;
  catalog.version = 1u;
  catalog.activeProfile = "modded";
  catalog.profiles.push_back({"default", {}});
  catalog.profiles.push_back(
      {"modded", {"rr2nw.stack.core", "rr2nw.stack.addon"}});
  SRecoveredModProfileStatus status;
  std::vector<std::uint8_t> encoded;
  SRecoveredModProfileCatalog decoded;
  std::vector<std::uint8_t> reencoded;
  if (!RecoveredModProfile_Encode(catalog, &encoded, &status) ||
      !RecoveredModProfile_Decode(encoded, &decoded, &status) ||
      !RecoveredModProfile_Encode(decoded, &reencoded, &status) ||
      encoded != reencoded || decoded.fingerprint == 0u ||
      decoded.profiles.size() != 2u ||
      decoded.profiles[1].selectedIds[0] != "rr2nw.stack.addon")
    return false;

  const std::string canonical(encoded.begin(), encoded.end());
  std::size_t cursor = 0u;
  while ((cursor = canonical.find("\r\n", cursor)) != std::string::npos) {
    if (cursor != canonical.size() - 2u) {
      std::vector<std::uint8_t> truncated(encoded.begin(),
                                          encoded.begin() + cursor + 1u);
      if (RecoveredModProfile_Decode(truncated, &decoded, &status))
        return false;
    }
    cursor += 2u;
  }

  const std::vector<std::vector<std::uint8_t>> rejected = {
      {},
      Bytes("RR2MODPROFILE2\r\nversion=1\r\n"),
      Bytes("RR2MODPROFILE1\r\nversion=2\r\nactive=default\r\nprofiles=1\r\n"
            "profile_0_name=default\r\nprofile_0_mods=0\r\n"),
      Bytes("RR2MODPROFILE1\r\nversion=1\r\nactive=default\r\nprofiles=65\r\n")};
  for (const std::vector<std::uint8_t>& candidate : rejected)
    if (RecoveredModProfile_Decode(candidate, &decoded, &status)) return false;
  std::vector<std::uint8_t> nul = encoded;
  nul.insert(nul.begin() + 4, 0u);
  if (RecoveredModProfile_Decode(nul, &decoded, &status)) return false;
  std::vector<std::uint8_t> oversized(
      kRecoveredModProfileMaximumBytes + 1u, static_cast<std::uint8_t>('x'));
  if (RecoveredModProfile_Decode(oversized, &decoded, &status)) return false;

  catalog.profiles.push_back({"default", {}});
  if (RecoveredModProfile_Encode(catalog, &encoded, &status) ||
      status.error != ERecoveredModProfileError::DuplicateProfile)
    return false;
  catalog.profiles.pop_back();
  catalog.profiles[1].selectedIds.push_back("rr2nw.stack.core");
  if (RecoveredModProfile_Encode(catalog, &encoded, &status) ||
      status.error != ERecoveredModProfileError::DuplicatePackage)
    return false;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 || !CodecGate()) return Fail("codec boundary failed");
  const std::string root =
      Join(argv[1], "case-" + std::to_string(GetCurrentProcessId()));
  const std::string base = Join(root, "base");
  const std::string core = Join(root, "core");
  const std::string addon = Join(root, "addon");
  const std::string coreFiles = Join(core, "localization");
  const std::string addonFiles = Join(addon, "localization");
  const std::string profile = Join(root, "mod-profiles.cfg");
  if (!MakeDirectory(argv[1]) || !MakeDirectory(root) ||
      !MakeDirectory(base) || !MakeDirectory(core) ||
      !MakeDirectory(addon) || !MakeDirectory(coreFiles) ||
      !MakeDirectory(addonFiles) ||
      !Write(Join(coreFiles, "core.txt"), "core") ||
      !Write(Join(addonFiles, "addon.txt"), "addon") ||
      !Write(Join(core, "mod.json"),
             Manifest("rr2nw.stack.core", "localization/core.txt",
                      "RR2NW/localization/core.txt")) ||
      !Write(Join(addon, "mod.json"),
             Manifest("rr2nw.stack.addon", "localization/addon.txt",
                      "RR2NW/localization/addon.txt",
                      "  \"dependencies\": [{\"id\": "
                      "\"rr2nw.stack.core\", \"version\": \"1.0.0\"}],\n"
                      "  \"load_after\": [\"rr2nw.stack.core\"],\n")))
    return Fail("fixture creation failed");

  const char* candidates[2] = {addon.c_str(), core.c_str()};
  const std::wstring profileWide = Wide(profile);
  if (profileWide.empty() ||
      !RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, nullptr, 0u, false, true, false))
    return Fail("missing-profile configuration failed");
  const SRecoveredModSelectorSnapshot* snapshot =
      RecoveredModProfile_Snapshot();
  if (snapshot == nullptr || !snapshot->profileMissing ||
      snapshot->cliOverride || snapshot->candidateCount != 2u ||
      snapshot->activePackageCount != 0u || !snapshot->planReady)
    return Fail("missing profile did not recover to vanilla");
  const std::size_t addonIndex =
      CandidateIndex(*snapshot, "rr2nw.stack.addon");
  if (addonIndex >= snapshot->candidates.size() ||
      !RecoveredModProfile_ToggleCandidate(addonIndex))
    return Fail("addon could not be staged");
  snapshot = RecoveredModProfile_Snapshot();
  if (snapshot == nullptr || !snapshot->dirty || !snapshot->planReady ||
      snapshot->activePackageCount != 2u ||
      !MountIs(*snapshot, {"rr2nw.stack.core", "rr2nw.stack.addon"}) ||
      snapshot->candidates[addonIndex].state !=
          RECOVERED_MOD_SELECTOR_SELECTED)
    return Fail("dependency closure or mount order is wrong");
  if (!RecoveredModProfile_CommitStaged())
    return Fail("profile commit failed");
  snapshot = RecoveredModProfile_Snapshot();
  const std::uint64_t fingerprint =
      snapshot == nullptr ? 0u : snapshot->stagedFingerprint;
  const std::string committedText = Read(profile);
  if (snapshot == nullptr || !snapshot->restartRequired ||
      snapshot->dirty || snapshot->writes != 1u || fingerprint == 0u ||
      committedText.empty())
    return Fail("committed restart boundary is wrong");

  RecoveredModProfile_Release();
  if (!RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, nullptr, 0u, false, true, false))
    return Fail("fresh profile reload failed");
  snapshot = RecoveredModProfile_Snapshot();
  std::vector<std::string> requested;
  bool activateAll = true;
  if (snapshot == nullptr || snapshot->profileMissing ||
      snapshot->restartRequired || snapshot->stagedFingerprint != fingerprint ||
      !RecoveredModProfile_StartupSelection(&requested, &activateAll) ||
      activateAll || requested != std::vector<std::string>{"rr2nw.stack.addon"})
    return Fail("fresh-process selection is not exact");

  const std::size_t freshAddonIndex =
      CandidateIndex(*snapshot, "rr2nw.stack.addon");
  if (!RecoveredModProfile_ToggleCandidate(freshAddonIndex))
    return Fail("atomic-failure fixture could not be staged");
  RecoveredModProfile_FailNextAtomicCommitForTesting();
  if (RecoveredModProfile_CommitStaged() || Read(profile) != committedText)
    return Fail("failed atomic commit changed the profile file");
  RecoveredModProfile_Release();
  if (!RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, nullptr, 0u, false, true, false) ||
      RecoveredModProfile_Snapshot()->stagedFingerprint != fingerprint)
    return Fail("atomic failure did not preserve committed selection");

  const char* cliRequested[1] = {"rr2nw.stack.addon"};
  RecoveredModProfile_Release();
  if (!RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, cliRequested, 1u, false,
                                     false, false))
    return Fail("CLI override configuration failed");
  snapshot = RecoveredModProfile_Snapshot();
  if (snapshot == nullptr || !snapshot->cliOverride ||
      snapshot->source != "command-line" ||
      snapshot->activePackageCount != 2u ||
      RecoveredModProfile_ToggleCandidate(
          CandidateIndex(*snapshot, "rr2nw.stack.addon")) ||
      RecoveredModProfile_CommitStaged())
    return Fail("CLI override was not read-only");

  const char* cliMissing[1] = {"rr2nw.stack.not-discovered"};
  RecoveredModProfile_Release();
  if (!RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, cliMissing, 1u, false,
                                     false, false))
    return Fail("invalid CLI override configuration failed");
  snapshot = RecoveredModProfile_Snapshot();
  requested.clear();
  activateAll = false;
  if (snapshot == nullptr || !snapshot->cliOverride || snapshot->planReady ||
      !RecoveredModProfile_StartupSelection(&requested, &activateAll) ||
      activateAll ||
      requested != std::vector<std::string>{"rr2nw.stack.not-discovered"})
    return Fail("invalid CLI override was silently neutralized");

  RecoveredModProfile_Release();
  if (!RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, nullptr, 0u, false, true, true))
    return Fail("safe-mode configuration failed");
  snapshot = RecoveredModProfile_Snapshot();
  requested.assign(1u, "stale");
  activateAll = true;
  if (snapshot == nullptr || !snapshot->safeMode ||
      snapshot->source != "safe-mode" || snapshot->candidateCount != 0u ||
      !RecoveredModProfile_StartupSelection(&requested, &activateAll) ||
      !requested.empty() || activateAll)
    return Fail("safe mode did not disable user mods");

  RecoveredModProfile_Release();
  if (!Write(profile, "broken-profile") ||
      !RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, nullptr, 0u, false, true, false))
    return Fail("corrupt profile recovery failed");
  snapshot = RecoveredModProfile_Snapshot();
  if (snapshot == nullptr || !snapshot->corruptProfileRecovered ||
      snapshot->activePackageCount != 0u || !snapshot->planReady)
    return Fail("corrupt profile did not fail closed");

  SRecoveredModProfileCatalog unknown;
  unknown.version = 1u;
  unknown.activeProfile = "default";
  unknown.profiles.push_back(
      {"default", {"rr2nw.stack.not-discovered"}});
  SRecoveredModProfileStatus unknownStatus;
  RecoveredModProfile_Release();
  if (!RecoveredModProfile_WriteAtomic(profileWide, unknown,
                                       &unknownStatus) ||
      !RecoveredModProfile_Configure(profileWide, base.c_str(), candidates,
                                     2u, 0u, nullptr, 0u, false, true,
                                     false))
    return Fail("unknown-id profile fixture failed");
  snapshot = RecoveredModProfile_Snapshot();
  requested.assign(1u, "stale");
  activateAll = true;
  if (snapshot == nullptr || snapshot->planReady ||
      snapshot->activePackageCount != 0u ||
      snapshot->reason != "selected dependency is missing" ||
      !RecoveredModProfile_StartupSelection(&requested, &activateAll) ||
      !requested.empty() || activateAll)
    return Fail("unknown profile id did not fail closed");

  RecoveredModProfile_Release();
  RecoveredModRuntime_Release();
  std::printf("recovered mod profile smoke: codec=1 truncation=1 bounds=1 "
              "dependency_closure=2 mount_order=core,addon atomic=1 "
              "fresh_reload=1 cli_override=1 safe_mode=1 corrupt=1 "
              "unknown_id=1 fingerprint=%llu\n",
              static_cast<unsigned long long>(fingerprint));
  return 0;
}
