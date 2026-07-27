#include "RecoveredRetailScriptManifest.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <new>
#include <set>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

constexpr std::size_t kLegacyIncludeBufferSize = 1024;
constexpr std::streamoff kMaximumFileSize = 4 * 1024 * 1024;
constexpr unsigned long long kMaximumTotalSize = 64ull * 1024ull * 1024ull;
constexpr int kMaximumFileVisits = 256;
constexpr int kMaximumIncludeDepth = 32;
constexpr unsigned long long kFnvOffset = 14695981039346656037ull;
constexpr unsigned long long kFnvPrime = 1099511628211ull;

SRecoveredRetailScriptManifestSummary g_summary = {};
std::vector<std::string> g_files;
std::set<std::string> g_uniqueFiles;
std::set<std::string> g_activeFiles;
std::string g_rootDirectory;
std::string g_levelDirectory;
unsigned int g_issues = 0;
char g_lastError[512] = {};
bool g_ready = false;

void SetFailure(unsigned int issue, const char* message,
                const std::string& detail = std::string()) {
  g_issues |= issue;
  if (detail.empty()) {
    std::snprintf(g_lastError, sizeof(g_lastError), "%s", message);
  } else {
    std::snprintf(g_lastError, sizeof(g_lastError), "%s: %s", message,
                  detail.c_str());
  }
}

std::string LowerPath(std::string path) {
  std::replace(path.begin(), path.end(), '/', '\\');
  for (char& character : path) {
    const unsigned char value = static_cast<unsigned char>(character);
    if (value >= 'A' && value <= 'Z') {
      character = static_cast<char>(value - 'A' + 'a');
    }
  }
  while (path.size() > 3 && path.back() == '\\') path.pop_back();
  return path;
}

std::string StripExtendedPrefix(const std::string& path) {
  if (path.size() >= 8 && path.compare(0, 8, "\\\\?\\UNC\\") == 0) {
    return "\\\\" + path.substr(8);
  }
  if (path.size() >= 4 && path.compare(0, 4, "\\\\?\\") == 0) {
    return path.substr(4);
  }
  return path;
}

bool FullPath(const std::string& path, std::string& result) {
  const DWORD required = GetFullPathNameA(path.c_str(), 0, nullptr, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied =
      GetFullPathNameA(path.c_str(), required, buffer.data(), nullptr);
  if (copied == 0 || copied >= required) return false;
  result.assign(buffer.data(), copied);
  return true;
}

bool FinalPath(const std::string& path, bool directory, std::string& result) {
  const DWORD attributes = GetFileAttributesA(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES ||
      directory != ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)) {
    return false;
  }

  const DWORD flags = directory ? FILE_FLAG_BACKUP_SEMANTICS
                                : FILE_ATTRIBUTE_NORMAL;
  HANDLE handle = CreateFileA(path.c_str(), FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, flags, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;

  const DWORD required = GetFinalPathNameByHandleA(
      handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (required == 0) {
    CloseHandle(handle);
    return false;
  }
  std::vector<char> buffer(static_cast<std::size_t>(required) + 1u);
  const DWORD copied = GetFinalPathNameByHandleA(
      handle, buffer.data(), static_cast<DWORD>(buffer.size()),
      FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  CloseHandle(handle);
  if (copied == 0 || copied >= buffer.size()) return false;
  result = StripExtendedPrefix(std::string(buffer.data(), copied));
  return true;
}

std::string ParentPath(const std::string& path) {
  const std::size_t separator = path.find_last_of("\\/");
  if (separator == std::string::npos) return std::string();
  if (separator == 2 && path.size() >= 3 && path[1] == ':') {
    return path.substr(0, 3);
  }
  return path.substr(0, separator);
}

std::string JoinPath(const std::string& directory,
                     const std::string& child) {
  if (directory.empty() || directory.back() == '\\' ||
      directory.back() == '/') {
    return directory + child;
  }
  return directory + "\\" + child;
}

bool IsWithin(const std::string& path, const std::string& directory) {
  const std::string foldedPath = LowerPath(path);
  const std::string foldedDirectory = LowerPath(directory);
  if (foldedPath == foldedDirectory) return true;
  return foldedPath.size() > foldedDirectory.size() &&
         foldedPath.compare(0, foldedDirectory.size(), foldedDirectory) == 0 &&
         foldedPath[foldedDirectory.size()] == '\\';
}

std::string RelativeToRoot(const std::string& path) {
  if (!IsWithin(path, g_rootDirectory)) return path;
  if (LowerPath(path) == LowerPath(g_rootDirectory)) return ".";
  return path.substr(g_rootDirectory.size() + 1u);
}

bool ReadFile(const std::string& path, std::string& text) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) return false;
  const std::streamoff size = input.tellg();
  if (size < 0) return false;
  if (size > kMaximumFileSize) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_FILE_TOO_LARGE,
               "retail script file exceeds the per-file limit", path);
    return false;
  }
  input.seekg(0, std::ios::beg);
  text.assign(static_cast<std::size_t>(size), '\0');
  return size == 0 ||
         input.read(&text[0], static_cast<std::streamsize>(size)).good();
}

bool SkipSpaceAndComments(const std::string& text, std::size_t& position) {
  for (;;) {
    while (position < text.size() &&
           std::isspace(static_cast<unsigned char>(text[position])) != 0) {
      ++position;
    }
    if (position + 1u >= text.size() || text[position] != '/') {
      return true;
    }

    if (text[position + 1u] == '/') {
      position += 2u;
      while (position < text.size() && text[position] != '\r' &&
             text[position] != '\n') {
        ++position;
      }
      continue;
    }
    if (text[position + 1u] != '*') return true;

    int depth = 1;
    position += 2u;
    while (position < text.size() && depth > 0) {
      if (position + 1u < text.size() && text[position] == '/' &&
          text[position + 1u] == '*') {
        ++depth;
        position += 2u;
      } else if (position + 1u < text.size() && text[position] == '*' &&
                 text[position + 1u] == '/') {
        --depth;
        position += 2u;
      } else {
        ++position;
      }
    }
    if (depth != 0) return false;
  }
}

bool ExtractIncludes(const std::string& text,
                     std::vector<std::string>& includes) {
  std::size_t position = 0;
  while (position < text.size()) {
    if (std::isspace(static_cast<unsigned char>(text[position])) != 0) {
      ++position;
      continue;
    }
    if (position + 1u < text.size() && text[position] == '/' &&
        (text[position + 1u] == '/' || text[position + 1u] == '*')) {
      if (!SkipSpaceAndComments(text, position)) return false;
      continue;
    }
    if (text[position] == '"' || text[position] == '\'') {
      const char delimiter = text[position];
      ++position;
      while (position < text.size() && text[position] != delimiter) {
        ++position;
      }
      if (position < text.size()) ++position;
      continue;
    }

    const unsigned char first =
        static_cast<unsigned char>(text[position]);
    if (std::isalpha(first) == 0 && text[position] != '_') {
      ++position;
      continue;
    }
    const std::size_t tokenBegin = position++;
    while (position < text.size()) {
      const unsigned char next =
          static_cast<unsigned char>(text[position]);
      if (std::isalnum(next) == 0 && text[position] != '_') break;
      ++position;
    }
    if (text.compare(tokenBegin, position - tokenBegin, "include") != 0) {
      continue;
    }

    if (!SkipSpaceAndComments(text, position) || position >= text.size() ||
        (text[position] != '"' && text[position] != '\'')) {
      return false;
    }
    const char delimiter = text[position];
    const std::size_t pathBegin = ++position;
    while (position < text.size() && text[position] != delimiter &&
           text[position] != '\r' && text[position] != '\n') {
      ++position;
    }
    if (position >= text.size() || text[position] != delimiter ||
        position == pathBegin) {
      return false;
    }
    includes.push_back(text.substr(pathBegin, position - pathBegin));
    ++position;
  }
  return true;
}

void HashByte(unsigned char value) {
  g_summary.contentFingerprint ^= value;
  g_summary.contentFingerprint *= kFnvPrime;
}

void HashFile(const std::string& relativePath, const std::string& text) {
  const std::string foldedPath = LowerPath(relativePath);
  for (unsigned char value : foldedPath) HashByte(value);
  HashByte(0);
  for (unsigned char value : text) HashByte(value);
  HashByte(0xff);
}

bool ResolveInclude(const std::string& legacyPath,
                    std::string& finalPath) {
  if (legacyPath.size() >= kLegacyIncludeBufferSize) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_INCLUDE_PATH_TOO_LONG,
               "include name would overflow the legacy 1024-byte path buffer",
               legacyPath);
    return false;
  }

  std::string portablePath = legacyPath;
  std::replace(portablePath.begin(), portablePath.end(), '/', '\\');
  std::string lexicalPath;
  if (!FullPath(JoinPath(g_levelDirectory, portablePath), lexicalPath)) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_PATH_FAILURE,
               "could not normalize retail include", legacyPath);
    return false;
  }
  if (!IsWithin(lexicalPath, g_rootDirectory)) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_INCLUDE_OUTSIDE_ROOT,
               "include escapes the selected retail root", legacyPath);
    return false;
  }
  if (!FinalPath(lexicalPath, false, finalPath)) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_MISSING_INCLUDE,
               "retail include is missing or is not a regular file",
               legacyPath);
    return false;
  }
  if (!IsWithin(finalPath, g_rootDirectory)) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_INCLUDE_OUTSIDE_ROOT,
               "include resolves outside the selected retail root",
               legacyPath);
    return false;
  }
  return true;
}

bool VisitFile(const std::string& path, int depth) {
  if (depth > kMaximumIncludeDepth ||
      g_summary.fileVisits >= kMaximumFileVisits) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_FILE_LIMIT,
               "retail include graph exceeds its bounded file/depth budget",
               path);
    return false;
  }

  const std::string identity = LowerPath(path);
  if (g_activeFiles.find(identity) != g_activeFiles.end()) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_INCLUDE_CYCLE,
               "retail include graph contains a cycle", path);
    return false;
  }

  std::string text;
  if (!ReadFile(path, text)) {
    if (g_issues == 0) {
      SetFailure(RECOVERED_RETAIL_SCRIPT_READ_FAILURE,
                 "could not read retail script file", path);
    }
    return false;
  }
  if (g_summary.totalBytes + text.size() > kMaximumTotalSize) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_TOTAL_SIZE_LIMIT,
               "retail include graph exceeds its total byte budget", path);
    return false;
  }

  const std::string relativePath = RelativeToRoot(path);
  g_files.push_back(relativePath);
  ++g_summary.fileVisits;
  g_summary.totalBytes += text.size();
  if (g_uniqueFiles.insert(identity).second) ++g_summary.uniqueFiles;
  if (IsWithin(path, g_levelDirectory)) {
    ++g_summary.levelFiles;
  } else {
    ++g_summary.rootFiles;
  }
  HashFile(relativePath, text);

  std::vector<std::string> includes;
  if (!ExtractIncludes(text, includes)) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_MALFORMED_INCLUDE,
               "retail script contains a malformed include directive", path);
    return false;
  }
  g_summary.includeDirectives += static_cast<int>(includes.size());

  g_activeFiles.insert(identity);
  for (const std::string& include : includes) {
    std::string includePath;
    if (!ResolveInclude(include, includePath) ||
        !VisitFile(includePath, depth + 1)) {
      g_activeFiles.erase(identity);
      return false;
    }
  }
  g_activeFiles.erase(identity);
  return true;
}

}  // namespace

int RecoveredRetailScriptManifest_Preflight(const char* levelDirectory) {
  RecoveredRetailScriptManifest_Release();
  g_issues = 0;
  g_lastError[0] = 0;
  g_summary.contentFingerprint = kFnvOffset;
  if (levelDirectory == nullptr || levelDirectory[0] == 0) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_INVALID_ARGUMENT,
               "retail script preflight received no Level directory");
    return FALSE;
  }

  try {
    std::string lexicalLevel;
    if (!FullPath(levelDirectory, lexicalLevel) ||
        !FinalPath(lexicalLevel, true, g_levelDirectory)) {
      SetFailure(RECOVERED_RETAIL_SCRIPT_INVALID_LEVEL_DIRECTORY,
                 "selected retail Level directory is invalid",
                 levelDirectory);
      return FALSE;
    }

    const std::string lexicalRoot = ParentPath(g_levelDirectory);
    if (lexicalRoot.empty() ||
        !FinalPath(lexicalRoot, true, g_rootDirectory)) {
      SetFailure(RECOVERED_RETAIL_SCRIPT_PATH_FAILURE,
                 "could not resolve the selected retail root",
                 levelDirectory);
      return FALSE;
    }

    const std::string entryCandidate =
        JoinPath(g_rootDirectory, "LEVEL0.SC");
    std::string entryPath;
    if (!FinalPath(entryCandidate, false, entryPath)) {
      SetFailure(RECOVERED_RETAIL_SCRIPT_MISSING_ENTRY,
                 "retail root has no regular LEVEL0.SC", entryCandidate);
      return FALSE;
    }
    if (!IsWithin(entryPath, g_rootDirectory)) {
      SetFailure(RECOVERED_RETAIL_SCRIPT_INCLUDE_OUTSIDE_ROOT,
                 "LEVEL0.SC resolves outside the selected retail root",
                 entryCandidate);
      return FALSE;
    }
    if (!VisitFile(entryPath, 0)) return FALSE;

    g_ready = true;
    g_issues = 0;
    g_lastError[0] = 0;
    return TRUE;
  } catch (const std::bad_alloc&) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_ALLOCATION_FAILURE,
               "retail script manifest allocation failed");
  } catch (...) {
    SetFailure(RECOVERED_RETAIL_SCRIPT_READ_FAILURE,
               "retail script manifest raised an exception");
  }
  return FALSE;
}

void RecoveredRetailScriptManifest_Release() {
  g_summary = SRecoveredRetailScriptManifestSummary{};
  g_files.clear();
  g_uniqueFiles.clear();
  g_activeFiles.clear();
  g_rootDirectory.clear();
  g_levelDirectory.clear();
  g_ready = false;
}

bool RecoveredRetailScriptManifest_IsReady() { return g_ready; }

unsigned int RecoveredRetailScriptManifest_Issues() { return g_issues; }

const char* RecoveredRetailScriptManifest_LastError() { return g_lastError; }

const SRecoveredRetailScriptManifestSummary*
RecoveredRetailScriptManifest_Summary() {
  return g_ready ? &g_summary : nullptr;
}

const char* RecoveredRetailScriptManifest_File(int index) {
  if (!g_ready || index < 0 || index >= static_cast<int>(g_files.size())) {
    return nullptr;
  }
  return g_files[static_cast<std::size_t>(index)].c_str();
}
