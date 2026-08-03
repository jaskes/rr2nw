#include "RecoveredModRuntime.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

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

std::string Manifest(const std::string& source,
                     const std::string& target,
                     int engineApi = 1,
                     bool duplicate = false,
                     const std::string& levels = std::string()) {
  std::string text =
      "{\n"
      "  \"schema\": 1,\n"
      "  \"engine_api\": " + std::to_string(engineApi) + ",\n"
      "  \"id\": \"rr2nw.example.overlay\",\n"
      "  \"version\": \"1.0.0\",\n";
  if (!levels.empty())
    text += "  \"levels\": " + levels + ",\n";
  text +=
      "  \"files\": [\n"
      "    {\"source\": \"" + source + "\", \"target\": \"" +
      target + "\"}";
  if (duplicate) {
    text += ",\n    {\"source\": \"" + source +
            "\", \"target\": \"level.03n/SAMPLE.txt\"}";
  }
  text += "\n  ]\n}\n";
  return text;
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
  std::fprintf(stderr, "recovered mod runtime smoke: %s; issues=%u error=%s\n",
               message, RecoveredModRuntime_Issues(),
               RecoveredModRuntime_LastError());
  RecoveredModRuntime_Release();
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4)
    return Fail("expected scratch-root and two example-mod arguments");
  const std::string root = Join(
      argv[1], "case-" + std::to_string(GetCurrentProcessId()));
  const std::string base = Join(root, "base");
  const std::string level = Join(base, "Level.03N");
  const std::string physicalCollision = Join(base, "Level.Physical");
  const std::string routeDirectory = Join(level, "Route");
  const std::string routeGroup = Join(routeDirectory, "S00");
  const std::string mod = Join(root, "mod");
  const std::string textures = Join(mod, "textures");
  if (!MakeDirectory(argv[1]) || !MakeDirectory(root) ||
      !MakeDirectory(base) || !MakeDirectory(level) || !MakeDirectory(mod) ||
      !MakeDirectory(physicalCollision) || !MakeDirectory(textures) ||
      !MakeDirectory(routeDirectory) || !MakeDirectory(routeGroup))
    return Fail("could not create fixture directories");

  const std::string baseTarget = Join(level, "sample.txt");
  const std::string baseOther = Join(level, "other.txt");
  const std::string modSource = Join(textures, "replacement.txt");
  const std::string manifest = Join(mod, "mod.json");
  const std::string baseRoute = Join(routeGroup, "base.rt");
  if (!Write(baseTarget, "base-data") || !Write(baseOther, "base-other") ||
      !Write(baseRoute, "ms00.base\n2\n[0,0,0]\n[1,0,0]\n") ||
      !Write(modSource, "mod-data") ||
      !Write(manifest, "\xef\xbb\xbf" +
                           Manifest("textures/replacement.txt",
                                    "Level.03N/sample.txt")))
    return Fail("could not write fixture files");

  const std::uint64_t baseFingerprint = UINT64_C(0x1122334455667788);
  std::string text;
  if (!RecoveredModRuntime_Configure(base.c_str(), nullptr) ||
      RecoveredModRuntime_IsActive() ||
      RecoveredModRuntime_CombineContentFingerprint(baseFingerprint) !=
          baseFingerprint ||
      !ReadThroughResource(baseTarget, &text) || text != "base-data")
    return Fail("base-only resolver contract failed");

  if (!RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      !RecoveredModRuntime_IsActive())
    return Fail("valid manifest was not admitted");
  const SRecoveredModRuntimeSummary* summary =
      RecoveredModRuntime_Summary();
  const std::uint64_t combined =
      RecoveredModRuntime_CombineContentFingerprint(baseFingerprint);
  if (summary == nullptr || summary->schemaVersion != 1 ||
      summary->engineApi != 1 ||
      std::strcmp(summary->id, "rr2nw.example.overlay") != 0 ||
      std::strcmp(summary->version, "1.0.0") != 0 ||
      summary->fileCount != 1 || summary->totalBytes != 8 ||
      summary->modFingerprint == 0 || combined == 0 ||
      combined == baseFingerprint)
    return Fail("valid manifest summary or identity is incomplete");
  const std::uint64_t modFingerprint = summary->modFingerprint;

  const std::string foldedRequest = Join(base, "level.03n\\SAMPLE.TXT");
  if (!ReadThroughResource(foldedRequest, &text) || text != "mod-data" ||
      !ReadThroughResource(baseOther, &text) || text != "base-other")
    return Fail("exact overlay or base fallback read failed");
  summary = RecoveredModRuntime_Summary();
  if (summary == nullptr || summary->resolveCount < 2 ||
      summary->overrideHitCount != 1)
    return Fail("overlay resolution telemetry is wrong");

  if (!Write(manifest, Manifest("textures/replacement.txt",
                                "Level.03N/sample.txt", 1, true)))
    return Fail("could not write duplicate-target fixture");
  if (RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_DUPLICATE_TARGET) == 0 ||
      !RecoveredModRuntime_IsActive() ||
      RecoveredModRuntime_CombineContentFingerprint(baseFingerprint) !=
          combined ||
      !ReadThroughResource(baseTarget, &text) || text != "mod-data")
    return Fail("failed reconfiguration mutated the admitted overlay");

  if (!Write(manifest, Manifest("../outside.txt", "Level.03N/sample.txt")) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_INVALID_FILE_ENTRY) == 0)
    return Fail("source traversal did not fail closed");

  if (!Write(manifest, Manifest("textures/replacement.txt", "game.cfg")) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_INVALID_FILE_ENTRY) == 0)
    return Fail("protected target did not fail closed");

  if (!Write(manifest, Manifest("textures/replacement.txt", "saves")) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_INVALID_FILE_ENTRY) == 0)
    return Fail("protected root target did not fail closed");

  if (!Write(manifest, Manifest("textures/replacement.txt",
                                "Level.03N/sample.txt", 2)) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_UNSUPPORTED_ENGINE) == 0)
    return Fail("unsupported engine API did not fail closed");

  if (!Write(manifest, Manifest("textures/missing.txt",
                                "Level.03N/sample.txt")) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_MISSING_SOURCE) == 0)
    return Fail("missing source did not fail closed");

  if (!Write(manifest, Manifest("textures/replacement.txt",
                                "Level.03N/sample.txt")) ||
      !RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      RecoveredModRuntime_CombineContentFingerprint(baseFingerprint) !=
          combined)
    return Fail("valid manifest identity is not reproducible");

  const std::string derivedLevels =
      "[{\"id\":\"Level.Example\",\"base\":\"Level.03N\"}]";
  if (!Write(manifest,
             Manifest("textures/replacement.txt", "Level.Example/sample.txt",
                      1, false, derivedLevels)) ||
      !RecoveredModRuntime_Configure(base.c_str(), mod.c_str()))
    return Fail("derived Level manifest was not admitted");
  summary = RecoveredModRuntime_Summary();
  SRecoveredModLevel derived;
  char activated[4096] = {};
  if (summary == nullptr || summary->levelCount != 1 ||
      RecoveredModRuntime_LevelCount() != 1 ||
      !RecoveredModRuntime_Level(0, &derived) ||
      std::strcmp(derived.id, "Level.Example") != 0 ||
      std::strcmp(derived.base, "Level.03N") != 0)
    return Fail("derived Level catalog summary is incomplete");
  if (!RecoveredModRuntime_ActivateLevel("Level.Example", activated,
                                         sizeof(activated)))
    return Fail("derived Level activation failed");
  const DWORD activatedAttributes = GetFileAttributesA(activated);
  if (activatedAttributes == INVALID_FILE_ATTRIBUTES ||
      (activatedAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
      !RecoveredModRuntime_ActiveLevelIsDerived() ||
      std::strcmp(RecoveredModRuntime_ActiveLevelIdentity(),
                  "Level.Example") != 0)
    return Fail("derived Level activation failed");
  if (!ReadThroughResource(baseTarget, &text) || text != "mod-data")
    return Fail("derived Level overlay was not selected");
  if (!ReadThroughResource(baseOther, &text) || text != "base-other")
    return Fail("derived Level base fallback failed");
  const std::uint64_t derivedFingerprint = summary->modFingerprint;
  if (!Write(manifest,
             Manifest("textures/replacement.txt",
                      "Level.Example/Route/A20/overlay.rt", 1, false,
                      derivedLevels)) ||
      !RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      !RecoveredModRuntime_ActivateLevel("Level.Example", activated,
                                         sizeof(activated)))
    return Fail("derived Level Route overlay fixture was not admitted");
  std::vector<std::string> routeFiles;
  if (!RecoveredModRuntime_ListLevelFiles("Route", ".rt", &routeFiles) ||
      routeFiles.size() != 2 ||
      _stricmp(routeFiles[0].c_str(), "Route\\A20\\overlay.rt") != 0 ||
      _stricmp(routeFiles[1].c_str(), "Route\\S00\\base.rt") != 0)
    return Fail("effective Level file enumeration is incomplete");
  if (derivedFingerprint == 0 || derivedFingerprint == modFingerprint ||
      !RecoveredModRuntime_ActivateLevel("Level.03N", activated,
                                        sizeof(activated)) ||
      RecoveredModRuntime_ActiveLevelIsDerived() ||
      !ReadThroughResource(baseTarget, &text) || text != "base-data")
    return Fail("derived Level identity did not remain isolated");

  const std::string duplicateLevels =
      "[{\"id\":\"Level.Example\",\"base\":\"Level.03N\"},"
      "{\"id\":\"level.example\",\"base\":\"Level.03N\"}]";
  if (!Write(manifest,
             Manifest("textures/replacement.txt", "Level.Example/sample.txt",
                      1, false, duplicateLevels)) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_DUPLICATE_LEVEL) == 0)
    return Fail("duplicate derived Level id did not fail closed");

  const std::string missingBase =
      "[{\"id\":\"Level.Example\",\"base\":\"Level.Missing\"}]";
  if (!Write(manifest,
             Manifest("textures/replacement.txt", "Level.Example/sample.txt",
                      1, false, missingBase)) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_MISSING_LEVEL_BASE) == 0)
    return Fail("missing derived Level base did not fail closed");

  const std::string collidingLevel =
      "[{\"id\":\"Level.Physical\",\"base\":\"Level.03N\"}]";
  if (!Write(manifest,
             Manifest("textures/replacement.txt", "Level.Physical/sample.txt",
                      1, false, collidingLevel)) ||
      RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      (RecoveredModRuntime_Issues() & RECOVERED_MOD_LEVEL_COLLISION) == 0)
    return Fail("physical derived Level collision did not fail closed");

  if (!Write(manifest,
             Manifest("textures/replacement.txt", "Level.Example/sample.txt",
                      1, false, derivedLevels)) ||
      !RecoveredModRuntime_Configure(base.c_str(), mod.c_str()) ||
      RecoveredModRuntime_Summary()->modFingerprint != derivedFingerprint)
    return Fail("derived Level manifest identity is not reproducible");

  RecoveredModRuntime_Release();
  if (RecoveredModRuntime_IsConfigured() ||
      !ReadThroughResource(baseTarget, &text) || text != "base-data")
    return Fail("release did not restore unhooked base reads");

  if (!RecoveredModRuntime_Configure(base.c_str(), argv[2]))
    return Fail("repository example mod was not admitted");
  summary = RecoveredModRuntime_Summary();
  if (summary == nullptr ||
      std::strcmp(summary->id, "rr2nw.example.data-pack") != 0 ||
      summary->fileCount != 1 || summary->modFingerprint == 0)
    return Fail("repository example mod has an invalid identity");
  RecoveredModRuntime_Release();

  if (!RecoveredModRuntime_Configure(base.c_str(), argv[3]))
    return Fail("repository derived-Level example was not admitted");
  summary = RecoveredModRuntime_Summary();
  if (summary == nullptr ||
      std::strcmp(summary->id, "rr2nw.example.derived-level") != 0 ||
      summary->fileCount != 0 || summary->levelCount != 1 ||
      summary->modFingerprint == 0)
    return Fail("repository derived-Level example has an invalid identity");
  RecoveredModRuntime_Release();

  std::printf("recovered mod runtime smoke: files=1 bytes=8 fingerprint=%llu "
              "combined=%llu derived=%llu levels=1 bom=1 fail_closed=9 "
              "examples=2\n",
              static_cast<unsigned long long>(modFingerprint),
              static_cast<unsigned long long>(combined),
              static_cast<unsigned long long>(derivedFingerprint));
  return 0;
}
