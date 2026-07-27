#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "RecoveredRetailScriptManifest.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr,
               "recovered-retail-script-manifest-smoke: %s "
               "(issues=%u error=%s)\n",
               message, RecoveredRetailScriptManifest_Issues(),
               RecoveredRetailScriptManifest_LastError());
  return EXIT_FAILURE;
}

std::string JoinPath(const std::string& directory, const char* name) {
  return directory + "\\" + name;
}

bool EnsureDirectory(const std::string& path) {
  if (CreateDirectoryA(path.c_str(), nullptr) != FALSE) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool WriteFile(const std::string& path, const char* contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(contents, static_cast<std::streamsize>(std::strlen(contents)));
  return output.good();
}

bool ExpectFailure(const std::string& levelDirectory,
                   unsigned int expectedIssue) {
  return !RecoveredRetailScriptManifest_Preflight(levelDirectory.c_str()) &&
         RecoveredRetailScriptManifest_Issues() == expectedIssue &&
         !RecoveredRetailScriptManifest_IsReady() &&
         RecoveredRetailScriptManifest_Summary() == nullptr;
}

bool IsFile(int index, const char* expected) {
  const char* observed = RecoveredRetailScriptManifest_File(index);
  return observed != nullptr && _stricmp(observed, expected) == 0;
}

int RunRetail(const char* levelDirectory) {
  if (!RecoveredRetailScriptManifest_Preflight(levelDirectory)) {
    return Fail("retail manifest preflight failed");
  }
  const SRecoveredRetailScriptManifestSummary* summary =
      RecoveredRetailScriptManifest_Summary();
  if (summary == nullptr) return Fail("retail manifest has no summary");
  std::printf("retail-script-manifest includes=%d visits=%d unique=%d "
              "root=%d level=%d bytes=%llu fingerprint=%llu\n",
              summary->includeDirectives, summary->fileVisits,
              summary->uniqueFiles, summary->rootFiles, summary->levelFiles,
              summary->totalBytes, summary->contentFingerprint);
  RecoveredRetailScriptManifest_Release();
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::strcmp(argv[1], "--retail") == 0) {
    return RunRetail(argv[2]);
  }
  if (argc != 2) return Fail("expected a fixture root or --retail Level path");

  const std::string root = argv[1];
  const std::string level = JoinPath(root, "Level.Fixture");
  const std::string scinc = JoinPath(level, "ScInC");
  if (!EnsureDirectory(root) || !EnsureDirectory(level) ||
      !EnsureDirectory(scinc)) {
    return Fail("could not create fixture directories");
  }

  const char validEntry[] =
      "include \"../common.sci\"\r\n"
      "include \"scinc/local.sci\"\r\n"
      "func void main()\r\n"
      "{\r\n"
      "}\r\n";
  const char validCommon[] =
      "// include \"../../ignored-comment.sci\"\r\n"
      "/* nested include keeps the selected-Level base */\r\n"
      "include \"scinc/nested.sci\"\r\n";
  const char validLocal[] = "const int local_value = 7;\r\n";
  const char validNested[] = "const int nested_value = 11;\r\n";
  const std::string entryPath = JoinPath(root, "LEVEL0.SC");
  const std::string commonPath = JoinPath(root, "common.sci");
  const std::string localPath = JoinPath(scinc, "local.sci");
  const std::string nestedPath = JoinPath(scinc, "nested.sci");
  if (!WriteFile(entryPath, validEntry) ||
      !WriteFile(commonPath, validCommon) ||
      !WriteFile(localPath, validLocal) ||
      !WriteFile(nestedPath, validNested)) {
    return Fail("could not write the valid manifest fixture");
  }

  if (!RecoveredRetailScriptManifest_Preflight(level.c_str())) {
    return Fail("valid mixed-case/mixed-separator fixture was rejected");
  }
  const SRecoveredRetailScriptManifestSummary* summary =
      RecoveredRetailScriptManifest_Summary();
  if (summary == nullptr || summary->includeDirectives != 3 ||
      summary->fileVisits != 4 || summary->uniqueFiles != 4 ||
      summary->rootFiles != 2 || summary->levelFiles != 2 ||
      summary->contentFingerprint == 0 ||
      !IsFile(0, "LEVEL0.SC") || !IsFile(1, "common.sci") ||
      !IsFile(2, "Level.Fixture\\ScInC\\nested.sci") ||
      !IsFile(3, "Level.Fixture\\ScInC\\local.sci") ||
      RecoveredRetailScriptManifest_File(4) != nullptr) {
    return Fail("valid fixture produced an incorrect manifest");
  }
  const unsigned long long firstFingerprint = summary->contentFingerprint;
  const unsigned long long firstBytes = summary->totalBytes;
  RecoveredRetailScriptManifest_Release();
  RecoveredRetailScriptManifest_Release();
  if (RecoveredRetailScriptManifest_IsReady() ||
      RecoveredRetailScriptManifest_Issues() != 0 ||
      RecoveredRetailScriptManifest_LastError()[0] != 0) {
    return Fail("manifest release was not idempotent");
  }

  if (!WriteFile(entryPath, "include common.sci\r\n") ||
      !ExpectFailure(level, RECOVERED_RETAIL_SCRIPT_MALFORMED_INCLUDE)) {
    return Fail("malformed include did not fail closed");
  }
  RecoveredRetailScriptManifest_Release();
  if (RecoveredRetailScriptManifest_Issues() !=
          RECOVERED_RETAIL_SCRIPT_MALFORMED_INCLUDE ||
      RecoveredRetailScriptManifest_LastError()[0] == 0) {
    return Fail("rollback discarded manifest diagnostics");
  }
  if (!WriteFile(entryPath, "include \"scinc/missing.sci\"\r\n") ||
      !ExpectFailure(level, RECOVERED_RETAIL_SCRIPT_MISSING_INCLUDE)) {
    return Fail("missing include did not fail closed");
  }
  if (!WriteFile(entryPath, "include \"../../outside.sci\"\r\n") ||
      !ExpectFailure(level,
                     RECOVERED_RETAIL_SCRIPT_INCLUDE_OUTSIDE_ROOT)) {
    return Fail("escaping include did not fail closed");
  }
  if (!WriteFile(entryPath, "include \"../common.sci\"\r\n") ||
      !WriteFile(commonPath, "include \"../common.sci\"\r\n") ||
      !ExpectFailure(level, RECOVERED_RETAIL_SCRIPT_INCLUDE_CYCLE)) {
    return Fail("include cycle did not fail closed");
  }

  if (!WriteFile(entryPath, validEntry) ||
      !WriteFile(commonPath, validCommon) ||
      !RecoveredRetailScriptManifest_Preflight(level.c_str())) {
    return Fail("valid manifest did not recover after rejected inputs");
  }
  summary = RecoveredRetailScriptManifest_Summary();
  if (summary == nullptr || summary->contentFingerprint != firstFingerprint ||
      summary->totalBytes != firstBytes) {
    return Fail("manifest recovery was not deterministic");
  }

  std::printf("retail-script-manifest-smoke includes=%d files=%d root=%d "
              "level=%d bytes=%llu fingerprint=%llu\n",
              summary->includeDirectives, summary->fileVisits,
              summary->rootFiles, summary->levelFiles, summary->totalBytes,
              summary->contentFingerprint);
  RecoveredRetailScriptManifest_Release();
  return EXIT_SUCCESS;
}
