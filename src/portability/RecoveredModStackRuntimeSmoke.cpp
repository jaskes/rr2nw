#include "RecoveredModRuntime.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "filesys.h"

namespace {

std::string Join(const std::string& base, const std::string& child) {
  return base + "\\" + child;
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

std::string Manifest(const std::string& id, const std::string& source,
                     const std::string& target,
                     const std::string& relations = std::string(),
                     const std::string& version = "1.0.0") {
  return "{\n"
         "  \"schema\": 1,\n"
         "  \"engine_api\": 1,\n"
         "  \"id\": \"" + id + "\",\n"
         "  \"version\": \"" + version + "\",\n" +
         relations +
         "  \"files\": [{\"source\": \"" + source +
         "\", \"target\": \"" + target + "\"}]\n"
         "}\n";
}

bool ReadThroughResource(const std::string& path, std::string* text) {
  long length = -1;
  FILE* file = CFileResource::FOpenCurrent(path.c_str(), &length);
  if (file == nullptr || length < 0 || length > 1024) {
    if (file != nullptr) std::fclose(file);
    return false;
  }
  text->assign(static_cast<std::size_t>(length), '\0');
  const bool read = length == 0 ||
                    std::fread(&(*text)[0], static_cast<std::size_t>(length),
                               1, file) == 1;
  return std::fclose(file) == 0 && read;
}

int Fail(const char* message) {
  std::fprintf(stderr, "recovered mod stack smoke: %s; issues=%u error=%s\n",
               message, RecoveredModRuntime_Issues(),
               RecoveredModRuntime_LastError());
  RecoveredModRuntime_Release();
  return 1;
}

bool PackageIs(unsigned int index, const char* id) {
  SRecoveredModPackage package;
  return RecoveredModRuntime_Mod(index, &package) &&
         package.mountIndex == index && std::strcmp(package.id, id) == 0 &&
         std::strcmp(package.version, "1.0.0") == 0 &&
         package.fileCount == 1 && package.fingerprint != 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return Fail("expected scratch-root argument");
  const std::string root =
      Join(argv[1], "case-" + std::to_string(GetCurrentProcessId()));
  const std::string base = Join(root, "base");
  const std::string level = Join(base, "Level.03N");
  const std::string core = Join(root, "core");
  const std::string addon = Join(root, "addon");
  const std::string cosmetic = Join(root, "cosmetic");
  const std::string duplicate = Join(root, "duplicate");
  const std::string coreFiles = Join(core, "textures");
  const std::string addonFiles = Join(addon, "textures");
  const std::string cosmeticFiles = Join(cosmetic, "localization");
  const std::string duplicateFiles = Join(duplicate, "textures");
  if (!MakeDirectory(argv[1]) || !MakeDirectory(root) ||
      !MakeDirectory(base) || !MakeDirectory(level) ||
      !MakeDirectory(core) || !MakeDirectory(addon) ||
      !MakeDirectory(cosmetic) || !MakeDirectory(duplicate) ||
      !MakeDirectory(coreFiles) || !MakeDirectory(addonFiles) ||
      !MakeDirectory(cosmeticFiles) || !MakeDirectory(duplicateFiles))
    return Fail("could not create fixture directories");

  const std::string baseSample = Join(level, "sample.txt");
  const std::string baseOther = Join(level, "other.txt");
  const std::string coreManifest = Join(core, "mod.json");
  const std::string addonManifest = Join(addon, "mod.json");
  const std::string cosmeticManifest = Join(cosmetic, "mod.json");
  const std::string duplicateManifest = Join(duplicate, "mod.json");
  const std::string coreText =
      Manifest("rr2nw.stack.core", "textures/core.txt",
               "Level.03N/sample.txt");
  const std::string addonRelations =
      "  \"dependencies\": [{\"id\": \"rr2nw.stack.core\", "
      "\"version\": \"1.0.0\"}],\n"
      "  \"overrides\": [\"rr2nw.stack.core\"],\n";
  const std::string addonText =
      Manifest("rr2nw.stack.addon", "textures/addon.txt",
               "Level.03N/sample.txt", addonRelations);
  const std::string cosmeticRelations =
      "  \"dependencies\": [{\"id\": \"rr2nw.stack.addon\", "
      "\"version\": \"1.0.0\"}],\n"
      "  \"load_after\": [\"rr2nw.stack.core\"],\n";
  const std::string cosmeticText =
      Manifest("rr2nw.stack.cosmetic", "localization/other.txt",
               "Level.03N/other.txt", cosmeticRelations);
  if (!Write(baseSample, "base") || !Write(baseOther, "base-other") ||
      !Write(Join(coreFiles, "core.txt"), "core") ||
      !Write(Join(addonFiles, "addon.txt"), "addon") ||
      !Write(Join(cosmeticFiles, "other.txt"), "cosmetic") ||
      !Write(Join(duplicateFiles, "duplicate.txt"), "duplicate") ||
      !Write(coreManifest, coreText) ||
      !Write(addonManifest, addonText) ||
      !Write(cosmeticManifest, cosmeticText))
    return Fail("could not write fixture files");

  const char* shuffled[3] = {cosmetic.c_str(), core.c_str(), addon.c_str()};
  const char* requested[1] = {"rr2nw.stack.cosmetic"};
  const std::uint64_t baseFingerprint = UINT64_C(0x123456789abcdef0);
  if (!RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                          requested, 1, false))
    return Fail("dependency closure was not admitted");
  const SRecoveredModRuntimeSummary* summary =
      RecoveredModRuntime_Summary();
  const std::uint64_t stackFingerprint =
      summary == nullptr ? 0 : summary->modFingerprint;
  const std::uint64_t contentFingerprint =
      RecoveredModRuntime_CombineContentFingerprint(baseFingerprint);
  std::string text;
  if (summary == nullptr || summary->candidateCount != 3 ||
      summary->modCount != 3 || summary->fileCount != 2 ||
      summary->totalBytes != 17 || stackFingerprint == 0 ||
      contentFingerprint == 0 || contentFingerprint == baseFingerprint ||
      RecoveredModRuntime_ModCount() != 3 ||
      !PackageIs(0, "rr2nw.stack.core") ||
      !PackageIs(1, "rr2nw.stack.addon") ||
      !PackageIs(2, "rr2nw.stack.cosmetic") ||
      !ReadThroughResource(baseSample, &text) || text != "addon" ||
      !ReadThroughResource(baseOther, &text) || text != "cosmetic")
    return Fail("deterministic stack summary or overlay result is wrong");

  SRecoveredModCandidateInfo inspected;
  SRecoveredModStackPlan planned;
  const char* addonOnly[1] = {"rr2nw.stack.addon"};
  if (!RecoveredModRuntime_InspectCandidate(base.c_str(), addon.c_str(),
                                            &inspected) ||
      !inspected.valid || std::strcmp(inspected.id, "rr2nw.stack.addon") != 0 ||
      inspected.dependencyCount != 1u ||
      !RecoveredModRuntime_PlanStack(base.c_str(), shuffled, 3u, 0u,
                                     addonOnly, 1u, false, &planned) ||
      !planned.ready || planned.packages.size() != 2u ||
      std::strcmp(planned.packages[0].id, "rr2nw.stack.core") != 0 ||
      std::strcmp(planned.packages[1].id, "rr2nw.stack.addon") != 0 ||
      RecoveredModRuntime_Issues() != 0u ||
      RecoveredModRuntime_Summary()->modFingerprint != stackFingerprint ||
      RecoveredModRuntime_ModCount() != 3u ||
      !ReadThroughResource(baseOther, &text) || text != "cosmetic")
    return Fail("read-only stack planning changed the mounted runtime");

  const char* reversed[3] = {addon.c_str(), cosmetic.c_str(), core.c_str()};
  if (!RecoveredModRuntime_ConfigureStack(base.c_str(), reversed, 3, 0,
                                          nullptr, 0, true) ||
      RecoveredModRuntime_Summary()->modFingerprint != stackFingerprint ||
      RecoveredModRuntime_CombineContentFingerprint(baseFingerprint) !=
          contentFingerprint ||
      !PackageIs(0, "rr2nw.stack.core") ||
      !PackageIs(1, "rr2nw.stack.addon") ||
      !PackageIs(2, "rr2nw.stack.cosmetic"))
    return Fail("candidate enumeration changed deterministic mount identity");

  const char* explicitCosmetic[3] = {cosmetic.c_str(), addon.c_str(),
                                     core.c_str()};
  if (!RecoveredModRuntime_ConfigureStack(base.c_str(), explicitCosmetic, 3,
                                          1, nullptr, 0, false) ||
      RecoveredModRuntime_ModCount() != 3)
    return Fail("explicit selection did not acquire dependency closure");

  const char* missingDependency[1] = {addon.c_str()};
  if (RecoveredModRuntime_ConfigureStack(base.c_str(), missingDependency, 1,
                                         1, nullptr, 0, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_MISSING_DEPENDENCY) == 0 ||
      RecoveredModRuntime_Summary()->modFingerprint != stackFingerprint)
    return Fail("missing dependency was not transactional");

  const std::string wrongVersionRelations =
      "  \"dependencies\": [{\"id\": \"rr2nw.stack.core\", "
      "\"version\": \"9.9.9\"}],\n"
      "  \"overrides\": [\"rr2nw.stack.core\"],\n";
  if (!Write(addonManifest,
             Manifest("rr2nw.stack.addon", "textures/addon.txt",
                      "Level.03N/sample.txt", wrongVersionRelations)) ||
      RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                         requested, 1, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_DEPENDENCY_VERSION) == 0)
    return Fail("dependency version mismatch was not rejected");

  const std::string collisionRelations =
      "  \"dependencies\": [{\"id\": \"rr2nw.stack.core\", "
      "\"version\": \"1.0.0\"}],\n";
  if (!Write(addonManifest,
             Manifest("rr2nw.stack.addon", "textures/addon.txt",
                      "Level.03N/sample.txt", collisionRelations)) ||
      RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                         requested, 1, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_TARGET_CONFLICT) == 0)
    return Fail("undeclared target collision was not rejected");

  const std::string conflictRelations =
      cosmeticRelations +
      "  \"conflicts\": [\"rr2nw.stack.core\"],\n";
  if (!Write(addonManifest, addonText) ||
      !Write(cosmeticManifest,
             Manifest("rr2nw.stack.cosmetic", "localization/other.txt",
                      "Level.03N/other.txt", conflictRelations)) ||
      RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                         requested, 1, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_CONFLICT) == 0)
    return Fail("declared conflict was not rejected");

  const std::string cycleCore =
      "  \"load_after\": [\"rr2nw.stack.addon\"],\n";
  if (!Write(cosmeticManifest, cosmeticText) ||
      !Write(coreManifest,
             Manifest("rr2nw.stack.core", "textures/core.txt",
                      "Level.03N/sample.txt", cycleCore)) ||
      RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                         requested, 1, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_ORDER_CYCLE) == 0)
    return Fail("mount-order cycle was not rejected");

  if (!Write(coreManifest, coreText) ||
      !Write(duplicateManifest,
             Manifest("rr2nw.stack.core", "textures/duplicate.txt",
                      "Level.03N/duplicate.txt")))
    return Fail("could not restore duplicate-id fixture");
  const char* duplicateIds[2] = {core.c_str(), duplicate.c_str()};
  if (RecoveredModRuntime_ConfigureStack(base.c_str(), duplicateIds, 2, 2,
                                         nullptr, 0, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_DUPLICATE_ID) == 0)
    return Fail("duplicate candidate id was not rejected");

  const char* duplicatePaths[2] = {core.c_str(), core.c_str()};
  if (RecoveredModRuntime_ConfigureStack(base.c_str(), duplicatePaths, 2, 2,
                                         nullptr, 0, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_DUPLICATE_ID) == 0)
    return Fail("duplicate candidate path was not rejected");

  const char* duplicateRequests[2] = {"rr2nw.stack.core",
                                      "rr2nw.stack.core"};
  if (RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                         duplicateRequests, 2, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_INVALID_RELATION) == 0)
    return Fail("duplicate requested id was not rejected");

  const char* absentRequest[1] = {"rr2nw.stack.absent"};
  if (RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                         absentRequest, 1, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_MISSING_DEPENDENCY) == 0)
    return Fail("undiscovered requested id was not rejected");
  if (RecoveredModRuntime_ConfigureStack(base.c_str(), nullptr, 0, 0,
                                         absentRequest, 1, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_MISSING_DEPENDENCY) == 0)
    return Fail("requested id with an empty discovery set was not rejected");

  if (!Write(coreManifest,
             Manifest("rr2nw.stack.core", "textures/core.txt",
                      "Level.03N/sample.txt",
                      "  \"load_after\": [\"rr2nw.stack.core\"],\n")) ||
      RecoveredModRuntime_ConfigureStack(base.c_str(), shuffled, 3, 0,
                                         requested, 1, false) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_INVALID_RELATION) == 0)
    return Fail("self-referential relation was not rejected");

  RecoveredModRuntime_Release();
  std::printf("recovered mod stack smoke: candidates=3 active=3 order=core,addon,cosmetic "
              "effective_files=2 bytes=17 dependency_closure=1 conflicts=1 "
              "cycles=1 deterministic=1 fail_closed=10 fingerprint=%llu\n",
              static_cast<unsigned long long>(stackFingerprint));
  return 0;
}
