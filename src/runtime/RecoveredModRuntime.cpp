#include "RecoveredModRuntime.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <string>
#include <utility>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "filesys.h"

namespace {

constexpr int kManifestSchema = 1;
constexpr int kEngineApi = 1;
constexpr std::streamoff kMaximumManifestSize = 256 * 1024;
constexpr std::streamoff kMaximumFileSize = 64 * 1024 * 1024;
constexpr std::uint64_t kMaximumTotalSize = 512ull * 1024ull * 1024ull;
constexpr std::size_t kMaximumFiles = 1024;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

struct OverlayEntry {
  std::string sourceRelative;
  std::string sourceFinal;
  std::string targetRelative;
  std::string targetFolded;
  std::uint64_t size = 0;
};

struct Candidate {
  std::string baseLexical;
  std::string baseFinal;
  std::string modFinal;
  std::string id;
  std::string version;
  int schema = 0;
  int engineApi = 0;
  std::vector<OverlayEntry> entries;
  std::uint64_t totalBytes = 0;
  std::uint64_t fingerprint = kFnvOffset;
};

struct ManifestFile {
  std::string source;
  std::string target;
};

struct Manifest {
  int schema = 0;
  int engineApi = 0;
  std::string id;
  std::string version;
  std::vector<ManifestFile> files;
};

std::string g_baseLexical;
std::string g_baseFinal;
std::string g_modFinal;
std::vector<OverlayEntry> g_entries;
SRecoveredModRuntimeSummary g_summary;
unsigned int g_issues = 0;
char g_lastError[512] = {};
bool g_configured = false;
bool g_active = false;

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

std::string StripExtendedPrefix(const std::string& path) {
  if (path.size() >= 8 && path.compare(0, 8, "\\\\?\\UNC\\") == 0)
    return "\\\\" + path.substr(8);
  if (path.size() >= 4 && path.compare(0, 4, "\\\\?\\") == 0)
    return path.substr(4);
  return path;
}

std::string FoldPath(std::string path) {
  std::replace(path.begin(), path.end(), '/', '\\');
  for (char& character : path) {
    const unsigned char value = static_cast<unsigned char>(character);
    if (value >= 'A' && value <= 'Z')
      character = static_cast<char>(value - 'A' + 'a');
  }
  while (path.size() > 3 && path.back() == '\\') path.pop_back();
  return path;
}

bool FullPath(const std::string& path, std::string* result) {
  const DWORD required = GetFullPathNameA(path.c_str(), 0, nullptr, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied =
      GetFullPathNameA(path.c_str(), required, buffer.data(), nullptr);
  if (copied == 0 || copied >= required) return false;
  result->assign(buffer.data(), copied);
  return true;
}

bool FinalPath(const std::string& path, bool directory,
               std::string* result) {
  const DWORD attributes = GetFileAttributesA(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES ||
      directory != ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0))
    return false;
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
  *result = StripExtendedPrefix(std::string(buffer.data(), copied));
  return true;
}

std::string JoinPath(const std::string& base, const std::string& child) {
  if (base.empty() || base.back() == '\\' || base.back() == '/')
    return base + child;
  return base + "\\" + child;
}

bool IsWithin(const std::string& path, const std::string& root) {
  const std::string foldedPath = FoldPath(path);
  const std::string foldedRoot = FoldPath(root);
  return foldedPath == foldedRoot ||
         (foldedPath.size() > foldedRoot.size() &&
          foldedPath.compare(0, foldedRoot.size(), foldedRoot) == 0 &&
          foldedPath[foldedRoot.size()] == '\\');
}

bool ReadBounded(const std::string& path, std::streamoff maximum,
                 std::string* text, std::streamoff* sizeOut = nullptr) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) return false;
  const std::streamoff size = input.tellg();
  if (size < 0 || size > maximum) return false;
  input.seekg(0, std::ios::beg);
  text->assign(static_cast<std::size_t>(size), '\0');
  if (size != 0 &&
      !input.read(&(*text)[0], static_cast<std::streamsize>(size)))
    return false;
  if (sizeOut != nullptr) *sizeOut = size;
  return true;
}

class JsonCursor {
 public:
  explicit JsonCursor(const std::string& text) : text_(text) {}

  void Space() {
    while (position_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[position_])) != 0)
      ++position_;
  }

  bool Consume(char expected) {
    Space();
    if (position_ >= text_.size() || text_[position_] != expected)
      return Fail("expected JSON punctuation");
    ++position_;
    return true;
  }

  bool TryConsume(char value) {
    Space();
    if (position_ >= text_.size() || text_[position_] != value) return false;
    ++position_;
    return true;
  }

  bool Integer(int* value) {
    Space();
    const std::size_t begin = position_;
    if (position_ < text_.size() && text_[position_] == '-') ++position_;
    if (position_ >= text_.size() ||
        std::isdigit(static_cast<unsigned char>(text_[position_])) == 0)
      return Fail("expected JSON integer");
    if (text_[position_] == '0' && position_ + 1u < text_.size() &&
        std::isdigit(static_cast<unsigned char>(text_[position_ + 1u])) != 0)
      return Fail("JSON integer has a leading zero");
    while (position_ < text_.size() &&
           std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
      ++position_;
    errno = 0;
    char* end = nullptr;
    const std::string token = text_.substr(begin, position_ - begin);
    const long parsed = std::strtol(token.c_str(), &end, 10);
    if (errno != 0 || end == token.c_str() || *end != '\0' ||
        parsed < INT_MIN || parsed > INT_MAX)
      return Fail("JSON integer is out of range");
    *value = static_cast<int>(parsed);
    return true;
  }

  bool String(std::string* value) {
    Space();
    if (position_ >= text_.size() || text_[position_] != '"')
      return Fail("expected JSON string");
    ++position_;
    value->clear();
    while (position_ < text_.size()) {
      unsigned char character =
          static_cast<unsigned char>(text_[position_++]);
      if (character == '"') return true;
      if (character < 0x20u || character > 0x7eu)
        return Fail("manifest strings must use printable ASCII");
      if (character != '\\') {
        value->push_back(static_cast<char>(character));
        continue;
      }
      if (position_ >= text_.size()) return Fail("unterminated JSON escape");
      const char escaped = text_[position_++];
      switch (escaped) {
        case '"': value->push_back('"'); break;
        case '\\': value->push_back('\\'); break;
        case '/': value->push_back('/'); break;
        case 'b': value->push_back('\b'); break;
        case 'f': value->push_back('\f'); break;
        case 'n': value->push_back('\n'); break;
        case 'r': value->push_back('\r'); break;
        case 't': value->push_back('\t'); break;
        default: return Fail("unsupported JSON string escape");
      }
    }
    return Fail("unterminated JSON string");
  }

  bool Finished() {
    Space();
    return position_ == text_.size() || Fail("trailing JSON data");
  }

  const std::string& error() const { return error_; }

 private:
  bool Fail(const char* message) {
    if (error_.empty()) {
      error_ = std::string(message) + " at byte " +
               std::to_string(position_);
    }
    return false;
  }

  const std::string& text_;
  std::size_t position_ = 0;
  std::string error_;
};

bool ParseFile(JsonCursor* cursor, ManifestFile* file) {
  if (!cursor->Consume('{')) return false;
  bool sourceSeen = false;
  bool targetSeen = false;
  if (cursor->TryConsume('}')) return false;
  for (;;) {
    std::string key;
    if (!cursor->String(&key) || !cursor->Consume(':')) return false;
    if (key == "source") {
      if (sourceSeen || !cursor->String(&file->source)) return false;
      sourceSeen = true;
    } else if (key == "target") {
      if (targetSeen || !cursor->String(&file->target)) return false;
      targetSeen = true;
    } else {
      return false;
    }
    if (cursor->TryConsume('}')) break;
    if (!cursor->Consume(',')) return false;
  }
  return sourceSeen && targetSeen;
}

bool ParseFiles(JsonCursor* cursor, std::vector<ManifestFile>* files) {
  if (!cursor->Consume('[')) return false;
  if (cursor->TryConsume(']')) return true;
  for (;;) {
    if (files->size() >= kMaximumFiles) return false;
    ManifestFile file;
    if (!ParseFile(cursor, &file)) return false;
    files->push_back(std::move(file));
    if (cursor->TryConsume(']')) return true;
    if (!cursor->Consume(',')) return false;
  }
}

bool ParseManifest(const std::string& text, Manifest* manifest,
                   std::string* failure) {
  JsonCursor cursor(text);
  if (!cursor.Consume('{')) {
    *failure = cursor.error();
    return false;
  }
  bool schemaSeen = false;
  bool engineSeen = false;
  bool idSeen = false;
  bool versionSeen = false;
  bool filesSeen = false;
  if (cursor.TryConsume('}')) {
    *failure = "manifest object is empty";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cursor.String(&key) || !cursor.Consume(':')) break;
    bool accepted = true;
    if (key == "schema") {
      accepted = !schemaSeen && cursor.Integer(&manifest->schema);
      schemaSeen = true;
    } else if (key == "engine_api") {
      accepted = !engineSeen && cursor.Integer(&manifest->engineApi);
      engineSeen = true;
    } else if (key == "id") {
      accepted = !idSeen && cursor.String(&manifest->id);
      idSeen = true;
    } else if (key == "version") {
      accepted = !versionSeen && cursor.String(&manifest->version);
      versionSeen = true;
    } else if (key == "files") {
      accepted = !filesSeen && ParseFiles(&cursor, &manifest->files);
      filesSeen = true;
    } else {
      *failure = "manifest contains unknown key: " + key;
      return false;
    }
    if (!accepted) break;
    if (cursor.TryConsume('}')) {
      if (!cursor.Finished()) break;
      if (!schemaSeen || !engineSeen || !idSeen || !versionSeen ||
          !filesSeen) {
        *failure = "manifest is missing a required key";
        return false;
      }
      return true;
    }
    if (!cursor.Consume(',')) break;
  }
  *failure = cursor.error().empty() ? "invalid manifest value"
                                    : cursor.error();
  return false;
}

bool ValidId(const std::string& id) {
  if (id.empty() || id.size() > 64) return false;
  for (std::size_t index = 0; index < id.size(); ++index) {
    const unsigned char value = static_cast<unsigned char>(id[index]);
    const bool alphanumeric =
        (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
    if (!alphanumeric && (index == 0 || (value != '.' && value != '_' &&
                                         value != '-')))
      return false;
  }
  return true;
}

bool ValidVersion(const std::string& version) {
  if (version.empty() || version.size() > 32) return false;
  int components = 0;
  std::size_t position = 0;
  while (position < version.size()) {
    const std::size_t begin = position;
    while (position < version.size() &&
           std::isdigit(static_cast<unsigned char>(version[position])) != 0)
      ++position;
    if (position == begin || (position - begin > 1 && version[begin] == '0'))
      return false;
    ++components;
    if (position == version.size()) break;
    if (version[position++] != '.') return false;
  }
  return components == 3;
}

bool NormalizeRelative(const std::string& input, std::string* normalized) {
  if (input.empty() || input.size() > 1024) return false;
  std::string path = input;
  std::replace(path.begin(), path.end(), '/', '\\');
  if (path.front() == '\\' || path.find(':') != std::string::npos) return false;
  normalized->clear();
  std::size_t position = 0;
  while (position < path.size()) {
    const std::size_t separator = path.find('\\', position);
    const std::size_t end =
        separator == std::string::npos ? path.size() : separator;
    const std::string part = path.substr(position, end - position);
    if (part.empty() || part == "." || part == ".." ||
        part.find_first_of("*?\"<>|") != std::string::npos)
      return false;
    if (!normalized->empty()) normalized->push_back('\\');
    normalized->append(part);
    if (separator == std::string::npos) break;
    position = separator + 1u;
  }
  return !normalized->empty();
}

bool AllowedSourceCategory(const std::string& source) {
  const std::size_t separator = source.find('\\');
  if (separator == std::string::npos) return false;
  const std::string category = FoldPath(source.substr(0, separator));
  return category == "maps" || category == "objects" ||
         category == "textures" || category == "sounds" ||
         category == "scripts" || category == "localization";
}

bool ProtectedTarget(const std::string& target) {
  const std::string folded = FoldPath(target);
  return folded == "game.cfg" || folded == "saves" || folded == "mods" ||
         folded.compare(0, 6, "saves\\") == 0 ||
         folded.compare(0, 5, "mods\\") == 0;
}

void HashByte(std::uint64_t* hash, unsigned char value) {
  *hash ^= value;
  *hash *= kFnvPrime;
}

void HashText(std::uint64_t* hash, const std::string& text) {
  for (unsigned char value : text) HashByte(hash, value);
  HashByte(hash, 0);
}

void HashU64(std::uint64_t* hash, std::uint64_t value) {
  for (int byte = 0; byte < 8; ++byte)
    HashByte(hash, static_cast<unsigned char>((value >> (byte * 8)) & 0xffu));
}

bool HashFile(std::uint64_t* hash, const OverlayEntry& entry) {
  std::ifstream input(entry.sourceFinal, std::ios::binary);
  if (!input) return false;
  char buffer[64 * 1024];
  while (input) {
    input.read(buffer, sizeof(buffer));
    const std::streamsize count = input.gcount();
    for (std::streamsize index = 0; index < count; ++index)
      HashByte(hash, static_cast<unsigned char>(buffer[index]));
  }
  HashByte(hash, 0xffu);
  return input.eof();
}

bool BuildCandidate(const char* baseRoot, const char* modDirectory,
                    Candidate* candidate) {
  if (!FullPath(baseRoot, &candidate->baseLexical) ||
      !FinalPath(candidate->baseLexical, true, &candidate->baseFinal)) {
    SetFailure(RECOVERED_MOD_INVALID_BASE_ROOT,
               "selected base data root is invalid", baseRoot);
    return false;
  }
  if (modDirectory == nullptr || modDirectory[0] == '\0') return true;

  std::string modLexical;
  if (!FullPath(modDirectory, &modLexical) ||
      !FinalPath(modLexical, true, &candidate->modFinal)) {
    SetFailure(RECOVERED_MOD_INVALID_DIRECTORY,
               "selected mod directory is invalid", modDirectory);
    return false;
  }
  const std::string manifestPath = JoinPath(candidate->modFinal, "mod.json");
  std::string manifestFinal;
  if (!FinalPath(manifestPath, false, &manifestFinal) ||
      !IsWithin(manifestFinal, candidate->modFinal)) {
    SetFailure(RECOVERED_MOD_MISSING_MANIFEST,
               "mod directory has no regular in-root mod.json", manifestPath);
    return false;
  }
  std::string text;
  std::streamoff manifestSize = 0;
  if (!ReadBounded(manifestFinal, kMaximumManifestSize, &text,
                   &manifestSize)) {
    SetFailure(RECOVERED_MOD_MANIFEST_TOO_LARGE,
               "mod.json is unreadable or exceeds 256 KiB", manifestPath);
    return false;
  }
  if (text.size() >= 3u &&
      static_cast<unsigned char>(text[0]) == 0xefu &&
      static_cast<unsigned char>(text[1]) == 0xbbu &&
      static_cast<unsigned char>(text[2]) == 0xbfu)
    text.erase(0, 3u);
  Manifest manifest;
  std::string parseFailure;
  if (!ParseManifest(text, &manifest, &parseFailure)) {
    SetFailure(RECOVERED_MOD_MANIFEST_MALFORMED,
               "mod.json is malformed", parseFailure);
    return false;
  }
  if (manifest.schema != kManifestSchema) {
    SetFailure(RECOVERED_MOD_UNSUPPORTED_SCHEMA,
               "unsupported mod manifest schema",
               std::to_string(manifest.schema));
    return false;
  }
  if (manifest.engineApi != kEngineApi) {
    SetFailure(RECOVERED_MOD_UNSUPPORTED_ENGINE,
               "mod requires an unsupported engine API",
               std::to_string(manifest.engineApi));
    return false;
  }
  if (!ValidId(manifest.id)) {
    SetFailure(RECOVERED_MOD_INVALID_ID,
               "mod id must be lowercase ASCII [a-z0-9._-]", manifest.id);
    return false;
  }
  if (!ValidVersion(manifest.version)) {
    SetFailure(RECOVERED_MOD_INVALID_VERSION,
               "mod version must be canonical major.minor.patch",
               manifest.version);
    return false;
  }

  candidate->schema = manifest.schema;
  candidate->engineApi = manifest.engineApi;
  candidate->id = manifest.id;
  candidate->version = manifest.version;
  candidate->entries.reserve(manifest.files.size());
  for (const ManifestFile& file : manifest.files) {
    OverlayEntry entry;
    if (!NormalizeRelative(file.source, &entry.sourceRelative) ||
        !NormalizeRelative(file.target, &entry.targetRelative) ||
        !AllowedSourceCategory(entry.sourceRelative) ||
        ProtectedTarget(entry.targetRelative)) {
      SetFailure(RECOVERED_MOD_INVALID_FILE_ENTRY,
                 "mod file entry has an invalid source or target",
                 file.source + " -> " + file.target);
      return false;
    }
    const std::string sourceCandidate =
        JoinPath(candidate->modFinal, entry.sourceRelative);
    if (!FinalPath(sourceCandidate, false, &entry.sourceFinal)) {
      SetFailure(RECOVERED_MOD_MISSING_SOURCE,
                 "mod source file is missing or not regular",
                 entry.sourceRelative);
      return false;
    }
    if (!IsWithin(entry.sourceFinal, candidate->modFinal)) {
      SetFailure(RECOVERED_MOD_PATH_OUTSIDE_ROOT,
                 "mod source resolves outside the mod directory",
                 entry.sourceRelative);
      return false;
    }
    std::ifstream source(entry.sourceFinal, std::ios::binary | std::ios::ate);
    std::streamoff size = -1;
    if (source) size = source.tellg();
    if (size < 0) {
      SetFailure(RECOVERED_MOD_MISSING_SOURCE,
                 "mod source file cannot be measured", entry.sourceRelative);
      return false;
    }
    if (size > kMaximumFileSize) {
      SetFailure(RECOVERED_MOD_FILE_TOO_LARGE,
                 "mod source exceeds the 64 MiB file limit",
                 entry.sourceRelative);
      return false;
    }
    entry.size = static_cast<std::uint64_t>(size);
    if (candidate->totalBytes + entry.size > kMaximumTotalSize) {
      SetFailure(RECOVERED_MOD_TOTAL_SIZE_LIMIT,
                 "mod exceeds the 512 MiB admitted data limit", manifest.id);
      return false;
    }
    candidate->totalBytes += entry.size;
    entry.targetFolded = FoldPath(entry.targetRelative);
    candidate->entries.push_back(std::move(entry));
  }
  std::sort(candidate->entries.begin(), candidate->entries.end(),
            [](const OverlayEntry& left, const OverlayEntry& right) {
              return left.targetFolded < right.targetFolded;
            });
  for (std::size_t index = 1; index < candidate->entries.size(); ++index) {
    if (candidate->entries[index - 1].targetFolded ==
        candidate->entries[index].targetFolded) {
      SetFailure(RECOVERED_MOD_DUPLICATE_TARGET,
                 "mod contains duplicate case-insensitive targets",
                 candidate->entries[index].targetRelative);
      return false;
    }
  }

  HashU64(&candidate->fingerprint,
          static_cast<std::uint64_t>(candidate->schema));
  HashU64(&candidate->fingerprint,
          static_cast<std::uint64_t>(candidate->engineApi));
  HashText(&candidate->fingerprint, candidate->id);
  HashText(&candidate->fingerprint, candidate->version);
  for (const OverlayEntry& entry : candidate->entries) {
    HashText(&candidate->fingerprint, entry.targetFolded);
    HashU64(&candidate->fingerprint, entry.size);
    if (!HashFile(&candidate->fingerprint, entry)) {
      SetFailure(RECOVERED_MOD_MISSING_SOURCE,
                 "mod source changed while it was being admitted",
                 entry.sourceRelative);
      return false;
    }
  }
  if (candidate->fingerprint == 0) candidate->fingerprint = 1;
  return true;
}

std::string RelativeToBase(const std::string& requestedFull) {
  const std::string foldedRequested = FoldPath(requestedFull);
  const std::string foldedLexical = FoldPath(g_baseLexical);
  if (foldedRequested == foldedLexical) return std::string();
  if (foldedRequested.size() > foldedLexical.size() &&
      foldedRequested.compare(0, foldedLexical.size(), foldedLexical) == 0 &&
      foldedRequested[foldedLexical.size()] == '\\')
    return requestedFull.substr(g_baseLexical.size() + 1u);
  const std::string foldedFinal = FoldPath(g_baseFinal);
  if (foldedRequested.size() > foldedFinal.size() &&
      foldedRequested.compare(0, foldedFinal.size(), foldedFinal) == 0 &&
      foldedRequested[foldedFinal.size()] == '\\')
    return requestedFull.substr(g_baseFinal.size() + 1u);
  return std::string();
}

}  // namespace

bool RecoveredModRuntime_Configure(const char* baseRoot,
                                   const char* modDirectory) {
  g_issues = 0;
  g_lastError[0] = '\0';
  if (baseRoot == nullptr || baseRoot[0] == '\0') {
    SetFailure(RECOVERED_MOD_INVALID_ARGUMENT,
               "mod runtime received no base data root");
    return false;
  }
  try {
    Candidate candidate;
    if (!BuildCandidate(baseRoot, modDirectory, &candidate)) return false;
    const bool active = !candidate.modFinal.empty();
    g_baseLexical = std::move(candidate.baseLexical);
    g_baseFinal = std::move(candidate.baseFinal);
    g_modFinal = std::move(candidate.modFinal);
    g_entries = std::move(candidate.entries);
    g_summary = SRecoveredModRuntimeSummary{};
    g_summary.schemaVersion = candidate.schema;
    g_summary.engineApi = candidate.engineApi;
    std::snprintf(g_summary.id, sizeof(g_summary.id), "%s",
                  candidate.id.c_str());
    std::snprintf(g_summary.version, sizeof(g_summary.version), "%s",
                  candidate.version.c_str());
    g_summary.fileCount = static_cast<unsigned int>(g_entries.size());
    g_summary.totalBytes = candidate.totalBytes;
    g_summary.modFingerprint = active ? candidate.fingerprint : 0;
    g_active = active;
    g_configured = true;
    CFileResource::SetReadOpenHook(&RecoveredModRuntime_OpenRead);
    return true;
  } catch (const std::bad_alloc&) {
    SetFailure(RECOVERED_MOD_ALLOCATION_FAILURE,
               "mod runtime allocation failed");
  } catch (...) {
    SetFailure(RECOVERED_MOD_PATH_FAILURE,
               "mod runtime raised an unexpected exception");
  }
  return false;
}

void RecoveredModRuntime_Release() {
  CFileResource::SetReadOpenHook(nullptr);
  g_baseLexical.clear();
  g_baseFinal.clear();
  g_modFinal.clear();
  g_entries.clear();
  g_summary = SRecoveredModRuntimeSummary{};
  g_configured = false;
  g_active = false;
}

bool RecoveredModRuntime_IsConfigured() { return g_configured; }

bool RecoveredModRuntime_IsActive() { return g_configured && g_active; }

unsigned int RecoveredModRuntime_Issues() { return g_issues; }

const char* RecoveredModRuntime_LastError() { return g_lastError; }

const SRecoveredModRuntimeSummary* RecoveredModRuntime_Summary() {
  return g_configured ? &g_summary : nullptr;
}

bool RecoveredModRuntime_HasOverlayTarget(const char* target) {
  if (!RecoveredModRuntime_IsActive() || target == nullptr ||
      target[0] == '\0')
    return false;
  const std::string folded = FoldPath(target);
  const auto found = std::lower_bound(
      g_entries.begin(), g_entries.end(), folded,
      [](const OverlayEntry& entry, const std::string& value) {
        return entry.targetFolded < value;
      });
  return found != g_entries.end() && found->targetFolded == folded;
}

FILE* RecoveredModRuntime_OpenOverlayTarget(const char* target,
                                             long* length) {
  if (!RecoveredModRuntime_IsActive() || target == nullptr ||
      target[0] == '\0')
    return nullptr;
  ++g_summary.resolveCount;
  const std::string folded = FoldPath(target);
  const auto found = std::lower_bound(
      g_entries.begin(), g_entries.end(), folded,
      [](const OverlayEntry& entry, const std::string& value) {
        return entry.targetFolded < value;
      });
  if (found == g_entries.end() || found->targetFolded != folded)
    return nullptr;
  FILE* file = std::fopen(found->sourceFinal.c_str(), "rb");
  if (file == nullptr) return nullptr;
  if (length != nullptr) {
    if (found->size > static_cast<std::uint64_t>(LONG_MAX)) {
      std::fclose(file);
      return nullptr;
    }
    *length = static_cast<long>(found->size);
  }
  ++g_summary.overrideHitCount;
  return file;
}

bool RecoveredModRuntime_ResolveReadPath(const char* requested,
                                         char* resolved,
                                         std::size_t resolvedSize) {
  if (requested == nullptr || requested[0] == '\0' || resolved == nullptr ||
      resolvedSize == 0)
    return false;
  const char* selected = requested;
  if (g_configured && g_active) {
    ++g_summary.resolveCount;
    std::string requestedFull;
    if (FullPath(requested, &requestedFull)) {
      const std::string relative = RelativeToBase(requestedFull);
      if (!relative.empty()) {
        const std::string folded = FoldPath(relative);
        const auto found = std::lower_bound(
            g_entries.begin(), g_entries.end(), folded,
            [](const OverlayEntry& entry, const std::string& value) {
              return entry.targetFolded < value;
            });
        if (found != g_entries.end() && found->targetFolded == folded) {
          selected = found->sourceFinal.c_str();
          ++g_summary.overrideHitCount;
        }
      }
    }
  }
  const std::size_t length = std::strlen(selected);
  if (length + 1u > resolvedSize) return false;
  std::memcpy(resolved, selected, length + 1u);
  return true;
}

FILE* RecoveredModRuntime_OpenRead(const char* requested, long* length) {
  char resolved[4096] = {};
  if (!RecoveredModRuntime_ResolveReadPath(requested, resolved,
                                           sizeof(resolved)))
    return nullptr;
  FILE* file = std::fopen(resolved, "rb");
  if (file == nullptr) return nullptr;
  if (length != nullptr) {
    if (std::fseek(file, 0, SEEK_END) != 0) {
      std::fclose(file);
      return nullptr;
    }
    const long measured = std::ftell(file);
    if (measured < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
      std::fclose(file);
      return nullptr;
    }
    *length = measured;
  }
  return file;
}

std::uint64_t RecoveredModRuntime_CombineContentFingerprint(
    std::uint64_t baseFingerprint) {
  if (!RecoveredModRuntime_IsActive() || baseFingerprint == 0)
    return baseFingerprint;
  std::uint64_t hash = kFnvOffset;
  HashText(&hash, "RR2NW-MOD-CONTENT-1");
  HashU64(&hash, baseFingerprint);
  HashText(&hash, g_summary.id);
  HashText(&hash, g_summary.version);
  HashU64(&hash, g_summary.modFingerprint);
  return hash == 0 ? 1 : hash;
}
