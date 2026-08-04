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
constexpr std::size_t kMaximumLevels = 64;
constexpr std::size_t kMaximumRelations = 64;
constexpr std::size_t kMaximumCandidateMods = 128;
constexpr std::size_t kMaximumActiveMods = 64;
constexpr std::size_t kMaximumStackFiles = 4096;
constexpr std::size_t kMaximumEnumeratedLevelFiles = 16384;
constexpr std::uint64_t kMaximumStackBytes = 1024ull * 1024ull * 1024ull;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

struct OverlayEntry {
  std::string sourceRelative;
  std::string sourceFinal;
  std::string targetRelative;
  std::string targetFolded;
  std::uint64_t size = 0;
  std::string ownerId;
};

struct DependencyEntry {
  std::string id;
  std::string idFolded;
  std::string version;
};

struct LevelEntry {
  std::string id;
  std::string idFolded;
  std::string base;
  std::string baseFolded;
};

struct Candidate {
  std::string baseLexical;
  std::string baseFinal;
  std::string modFinal;
  std::string id;
  std::string version;
  int schema = 0;
  int engineApi = 0;
  bool explicitlySelected = false;
  bool active = false;
  std::vector<OverlayEntry> entries;
  std::vector<LevelEntry> levels;
  std::vector<DependencyEntry> dependencies;
  std::vector<std::string> conflicts;
  std::vector<std::string> loadAfter;
  std::vector<std::string> overrides;
  std::uint64_t totalBytes = 0;
  std::uint64_t fingerprint = kFnvOffset;
};

struct ManifestFile {
  std::string source;
  std::string target;
};

struct ManifestLevel {
  std::string id;
  std::string base;
};

struct ManifestDependency {
  std::string id;
  std::string version;
};

struct Manifest {
  int schema = 0;
  int engineApi = 0;
  std::string id;
  std::string version;
  std::vector<ManifestFile> files;
  std::vector<ManifestLevel> levels;
  std::vector<ManifestDependency> dependencies;
  std::vector<std::string> conflicts;
  std::vector<std::string> loadAfter;
  std::vector<std::string> overrides;
};

struct MountedPackage {
  std::string id;
  std::string version;
  unsigned int fileCount = 0;
  unsigned int levelCount = 0;
  std::uint64_t totalBytes = 0;
  std::uint64_t fingerprint = 0;
};

std::string g_baseLexical;
std::string g_baseFinal;
std::string g_modFinal;
std::vector<OverlayEntry> g_entries;
std::vector<LevelEntry> g_levels;
std::vector<MountedPackage> g_packages;
std::string g_activeLevelIdentity;
std::string g_activeLevelBase;
std::string g_activeLevelIdentityFolded;
std::string g_activeLevelBaseFolded;
bool g_activeLevelDerived = false;
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

bool ParseLevel(JsonCursor* cursor, ManifestLevel* level) {
  if (!cursor->Consume('{')) return false;
  bool idSeen = false;
  bool baseSeen = false;
  if (cursor->TryConsume('}')) return false;
  for (;;) {
    std::string key;
    if (!cursor->String(&key) || !cursor->Consume(':')) return false;
    if (key == "id") {
      if (idSeen || !cursor->String(&level->id)) return false;
      idSeen = true;
    } else if (key == "base") {
      if (baseSeen || !cursor->String(&level->base)) return false;
      baseSeen = true;
    } else {
      return false;
    }
    if (cursor->TryConsume('}')) break;
    if (!cursor->Consume(',')) return false;
  }
  return idSeen && baseSeen;
}

bool ParseLevels(JsonCursor* cursor, std::vector<ManifestLevel>* levels) {
  if (!cursor->Consume('[')) return false;
  if (cursor->TryConsume(']')) return false;
  for (;;) {
    if (levels->size() >= kMaximumLevels) return false;
    ManifestLevel level;
    if (!ParseLevel(cursor, &level)) return false;
    levels->push_back(std::move(level));
    if (cursor->TryConsume(']')) return true;
    if (!cursor->Consume(',')) return false;
  }
}

bool ParseDependency(JsonCursor* cursor, ManifestDependency* dependency) {
  if (!cursor->Consume('{')) return false;
  bool idSeen = false;
  bool versionSeen = false;
  if (cursor->TryConsume('}')) return false;
  for (;;) {
    std::string key;
    if (!cursor->String(&key) || !cursor->Consume(':')) return false;
    if (key == "id") {
      if (idSeen || !cursor->String(&dependency->id)) return false;
      idSeen = true;
    } else if (key == "version") {
      if (versionSeen || !cursor->String(&dependency->version)) return false;
      versionSeen = true;
    } else {
      return false;
    }
    if (cursor->TryConsume('}')) break;
    if (!cursor->Consume(',')) return false;
  }
  return idSeen && versionSeen;
}

bool ParseDependencies(JsonCursor* cursor,
                       std::vector<ManifestDependency>* dependencies) {
  if (!cursor->Consume('[')) return false;
  if (cursor->TryConsume(']')) return true;
  for (;;) {
    if (dependencies->size() >= kMaximumRelations) return false;
    ManifestDependency dependency;
    if (!ParseDependency(cursor, &dependency)) return false;
    dependencies->push_back(std::move(dependency));
    if (cursor->TryConsume(']')) return true;
    if (!cursor->Consume(',')) return false;
  }
}

bool ParseStringArray(JsonCursor* cursor, std::vector<std::string>* values) {
  if (!cursor->Consume('[')) return false;
  if (cursor->TryConsume(']')) return true;
  for (;;) {
    if (values->size() >= kMaximumRelations) return false;
    std::string value;
    if (!cursor->String(&value)) return false;
    values->push_back(std::move(value));
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
  bool levelsSeen = false;
  bool dependenciesSeen = false;
  bool conflictsSeen = false;
  bool loadAfterSeen = false;
  bool overridesSeen = false;
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
    } else if (key == "levels") {
      accepted = !levelsSeen && ParseLevels(&cursor, &manifest->levels);
      levelsSeen = true;
    } else if (key == "dependencies") {
      accepted = !dependenciesSeen &&
                 ParseDependencies(&cursor, &manifest->dependencies);
      dependenciesSeen = true;
    } else if (key == "conflicts") {
      accepted = !conflictsSeen &&
                 ParseStringArray(&cursor, &manifest->conflicts);
      conflictsSeen = true;
    } else if (key == "load_after") {
      accepted = !loadAfterSeen &&
                 ParseStringArray(&cursor, &manifest->loadAfter);
      loadAfterSeen = true;
    } else if (key == "overrides") {
      accepted = !overridesSeen &&
                 ParseStringArray(&cursor, &manifest->overrides);
      overridesSeen = true;
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

bool ContainsText(const std::vector<std::string>& values,
                  const std::string& value) {
  return std::binary_search(values.begin(), values.end(), value);
}

bool NormalizeRelations(const std::vector<std::string>& source,
                        const std::string& ownerFolded,
                        const char* relation,
                        std::vector<std::string>* result) {
  result->clear();
  result->reserve(source.size());
  for (const std::string& id : source) {
    if (!ValidId(id)) {
      SetFailure(RECOVERED_MOD_INVALID_RELATION,
                 "mod relation contains an invalid id",
                 std::string(relation) + ": " + id);
      return false;
    }
    const std::string folded = FoldPath(id);
    if (folded == ownerFolded) {
      SetFailure(RECOVERED_MOD_INVALID_RELATION,
                 "mod relation cannot reference its owner",
                 std::string(relation) + ": " + id);
      return false;
    }
    result->push_back(folded);
  }
  std::sort(result->begin(), result->end());
  if (std::adjacent_find(result->begin(), result->end()) != result->end()) {
    SetFailure(RECOVERED_MOD_INVALID_RELATION,
               "mod relation contains a duplicate id", relation);
    return false;
  }
  return true;
}

bool BuildRelations(const Manifest& manifest, Candidate* candidate) {
  const std::string ownerFolded = FoldPath(manifest.id);
  candidate->dependencies.reserve(manifest.dependencies.size());
  for (const ManifestDependency& declared : manifest.dependencies) {
    if (!ValidId(declared.id) || !ValidVersion(declared.version) ||
        FoldPath(declared.id) == ownerFolded) {
      SetFailure(RECOVERED_MOD_INVALID_RELATION,
                 "mod dependency has an invalid id/version",
                 declared.id + "@" + declared.version);
      return false;
    }
    DependencyEntry dependency;
    dependency.id = declared.id;
    dependency.idFolded = FoldPath(declared.id);
    dependency.version = declared.version;
    candidate->dependencies.push_back(std::move(dependency));
  }
  std::sort(candidate->dependencies.begin(), candidate->dependencies.end(),
            [](const DependencyEntry& left, const DependencyEntry& right) {
              return left.idFolded < right.idFolded;
            });
  for (std::size_t index = 1; index < candidate->dependencies.size(); ++index) {
    if (candidate->dependencies[index - 1].idFolded ==
        candidate->dependencies[index].idFolded) {
      SetFailure(RECOVERED_MOD_INVALID_RELATION,
                 "mod dependencies contain a duplicate id",
                 candidate->dependencies[index].id);
      return false;
    }
  }
  if (!NormalizeRelations(manifest.conflicts, ownerFolded, "conflicts",
                          &candidate->conflicts) ||
      !NormalizeRelations(manifest.loadAfter, ownerFolded, "load_after",
                          &candidate->loadAfter) ||
      !NormalizeRelations(manifest.overrides, ownerFolded, "overrides",
                          &candidate->overrides))
    return false;
  for (const DependencyEntry& dependency : candidate->dependencies) {
    if (ContainsText(candidate->conflicts, dependency.idFolded)) {
      SetFailure(RECOVERED_MOD_INVALID_RELATION,
                 "mod cannot both depend on and conflict with one id",
                 dependency.id);
      return false;
    }
  }
  for (const std::string& overridden : candidate->overrides) {
    if (ContainsText(candidate->conflicts, overridden)) {
      SetFailure(RECOVERED_MOD_INVALID_RELATION,
                 "mod cannot both override and conflict with one id",
                 overridden);
      return false;
    }
  }
  return true;
}

bool ValidLevelComponent(const std::string& value) {
  if (value.empty() || value.size() > 64 || value.back() == '.') return false;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const unsigned char character =
        static_cast<unsigned char>(value[index]);
    const bool alphanumeric =
        (character >= 'A' && character <= 'Z') ||
        (character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9');
    if (!alphanumeric &&
        (index == 0 || (character != '.' && character != '_' &&
                        character != '-')))
      return false;
  }
  return value != "." && value != "..";
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
  if (!BuildRelations(manifest, candidate)) return false;

  candidate->levels.reserve(manifest.levels.size());
  for (const ManifestLevel& declared : manifest.levels) {
    LevelEntry entry;
    entry.id = declared.id;
    entry.base = declared.base;
    if (!ValidLevelComponent(entry.id) ||
        !ValidLevelComponent(entry.base) ||
        FoldPath(entry.id) == FoldPath(entry.base)) {
      SetFailure(RECOVERED_MOD_INVALID_LEVEL_ENTRY,
                 "mod Level declaration has an invalid id or base",
                 entry.id + " -> " + entry.base);
      return false;
    }
    const std::string baseCandidate =
        JoinPath(candidate->baseLexical, entry.base);
    std::string baseFinal;
    if (!FinalPath(baseCandidate, true, &baseFinal) ||
        !IsWithin(baseFinal, candidate->baseFinal)) {
      SetFailure(RECOVERED_MOD_MISSING_LEVEL_BASE,
                 "mod Level base is not a regular in-root directory",
                 entry.base);
      return false;
    }
    const std::string identityCandidate =
        JoinPath(candidate->baseLexical, entry.id);
    if (GetFileAttributesA(identityCandidate.c_str()) !=
        INVALID_FILE_ATTRIBUTES) {
      SetFailure(RECOVERED_MOD_LEVEL_COLLISION,
                 "mod Level id collides with a physical base entry",
                 entry.id);
      return false;
    }
    entry.idFolded = FoldPath(entry.id);
    entry.baseFolded = FoldPath(entry.base);
    candidate->levels.push_back(std::move(entry));
  }
  std::sort(candidate->levels.begin(), candidate->levels.end(),
            [](const LevelEntry& left, const LevelEntry& right) {
              return left.idFolded < right.idFolded;
            });
  for (std::size_t index = 1; index < candidate->levels.size(); ++index) {
    if (candidate->levels[index - 1].idFolded ==
        candidate->levels[index].idFolded) {
      SetFailure(RECOVERED_MOD_DUPLICATE_LEVEL,
                 "mod contains duplicate case-insensitive Level ids",
                 candidate->levels[index].id);
      return false;
    }
  }

  candidate->schema = manifest.schema;
  candidate->engineApi = manifest.engineApi;
  candidate->id = manifest.id;
  candidate->version = manifest.version;
  candidate->entries.reserve(manifest.files.size());
  for (const ManifestFile& file : manifest.files) {
    OverlayEntry entry;
    entry.ownerId = manifest.id;
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
  if (!candidate->dependencies.empty() || !candidate->conflicts.empty() ||
      !candidate->loadAfter.empty() || !candidate->overrides.empty()) {
    HashText(&candidate->fingerprint, "RR2NW-MOD-RELATIONS-1");
    HashU64(&candidate->fingerprint,
            static_cast<std::uint64_t>(candidate->dependencies.size()));
    for (const DependencyEntry& dependency : candidate->dependencies) {
      HashText(&candidate->fingerprint, dependency.idFolded);
      HashText(&candidate->fingerprint, dependency.version);
    }
    const auto hashIds = [&candidate](const char* name,
                                      const std::vector<std::string>& ids) {
      HashText(&candidate->fingerprint, name);
      HashU64(&candidate->fingerprint,
              static_cast<std::uint64_t>(ids.size()));
      for (const std::string& id : ids)
        HashText(&candidate->fingerprint, id);
    };
    hashIds("conflicts", candidate->conflicts);
    hashIds("load_after", candidate->loadAfter);
    hashIds("overrides", candidate->overrides);
  }
  if (!candidate->levels.empty()) {
    HashText(&candidate->fingerprint, "RR2NW-DERIVED-LEVELS-1");
    HashU64(&candidate->fingerprint,
            static_cast<std::uint64_t>(candidate->levels.size()));
    for (const LevelEntry& level : candidate->levels) {
      HashText(&candidate->fingerprint, level.idFolded);
      HashText(&candidate->fingerprint, level.baseFolded);
    }
  }
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

bool HasExtension(const std::string& path, const std::string& extension) {
  if (extension.empty() || path.size() < extension.size()) return false;
  return FoldPath(path.substr(path.size() - extension.size())) ==
         FoldPath(extension);
}

bool EnumeratePhysicalFiles(const std::string& physicalDirectory,
                            const std::string& relativeDirectory,
                            const std::string& extension,
                            std::vector<std::string>* paths,
                            std::string* failure) {
  if (paths == nullptr || paths->size() > kMaximumEnumeratedLevelFiles) {
    if (failure != nullptr)
      *failure = "effective Level file catalog exceeded its safety limit";
    return false;
  }
  WIN32_FIND_DATAA data = {};
  const std::string pattern = JoinPath(physicalDirectory, "*");
  HANDLE find = FindFirstFileA(pattern.c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) {
    const DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
      return true;
    if (failure != nullptr)
      *failure = "FindFirstFile failed for " + physicalDirectory +
                 " (Win32 " + std::to_string(error) + ")";
    return false;
  }
  bool valid = true;
  do {
    const std::string name = data.cFileName;
    if (name == "." || name == "..") continue;
    const std::string childPhysical = JoinPath(physicalDirectory, name);
    const std::string childRelative = JoinPath(relativeDirectory, name);
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      // Do not follow junctions or symlinks out of the admitted data root.
      if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
        valid = EnumeratePhysicalFiles(childPhysical, childRelative,
                                       extension, paths, failure);
    } else if (HasExtension(name, extension)) {
      paths->push_back(childRelative);
      valid = paths->size() <= kMaximumEnumeratedLevelFiles;
      if (!valid && failure != nullptr)
        *failure = "effective Level file catalog exceeded its safety limit";
    }
  } while (valid && FindNextFileA(find, &data));
  const DWORD error = GetLastError();
  FindClose(find);
  if (!valid) return false;
  if (error != ERROR_NO_MORE_FILES) {
    if (failure != nullptr)
      *failure = "FindNextFile failed for " + physicalDirectory +
                 " (Win32 " + std::to_string(error) + ")";
    return false;
  }
  return true;
}

struct StackCandidate {
  std::string baseLexical;
  std::string baseFinal;
  std::vector<OverlayEntry> entries;
  std::vector<LevelEntry> levels;
  std::vector<MountedPackage> packages;
  std::uint64_t totalBytes = 0;
  std::uint64_t fingerprint = 0;
  unsigned int candidateCount = 0;
};

int FindCandidate(const std::vector<Candidate>& candidates,
                  const std::string& foldedId) {
  const auto found = std::lower_bound(
      candidates.begin(), candidates.end(), foldedId,
      [](const Candidate& candidate, const std::string& value) {
        return FoldPath(candidate.id) < value;
      });
  if (found == candidates.end() || FoldPath(found->id) != foldedId)
    return -1;
  return static_cast<int>(found - candidates.begin());
}

bool AddOrderEdge(std::vector<std::vector<unsigned char>>* edges,
                  std::vector<unsigned int>* indegree,
                  int before, int after) {
  if (before < 0 || after < 0 || before == after) return false;
  if ((*edges)[static_cast<std::size_t>(before)]
              [static_cast<std::size_t>(after)] == 0) {
    (*edges)[static_cast<std::size_t>(before)]
            [static_cast<std::size_t>(after)] = 1;
    ++(*indegree)[static_cast<std::size_t>(after)];
  }
  return true;
}

bool SelectAndOrder(std::vector<Candidate>* candidates,
                    const char* const* requestedIds,
                    std::size_t requestedIdCount,
                    bool activateAllCandidates,
                    std::vector<int>* order) {
  if (activateAllCandidates)
    for (Candidate& candidate : *candidates) candidate.active = true;
  std::vector<std::string> requested;
  requested.reserve(requestedIdCount);
  for (std::size_t index = 0; index < requestedIdCount; ++index) {
    if (requestedIds == nullptr || requestedIds[index] == nullptr ||
        !ValidId(requestedIds[index])) {
      SetFailure(RECOVERED_MOD_INVALID_RELATION,
                 "requested mod id is invalid",
                 requestedIds == nullptr || requestedIds[index] == nullptr
                     ? std::string()
                     : std::string(requestedIds[index]));
      return false;
    }
    requested.push_back(FoldPath(requestedIds[index]));
  }
  std::sort(requested.begin(), requested.end());
  if (std::adjacent_find(requested.begin(), requested.end()) !=
      requested.end()) {
    SetFailure(RECOVERED_MOD_INVALID_RELATION,
               "requested mod list contains a duplicate id");
    return false;
  }
  for (const std::string& id : requested) {
    const int selected = FindCandidate(*candidates, id);
    if (selected < 0) {
      SetFailure(RECOVERED_MOD_MISSING_DEPENDENCY,
                 "requested mod was not discovered", id);
      return false;
    }
    (*candidates)[static_cast<std::size_t>(selected)].active = true;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (Candidate& candidate : *candidates) {
      if (!candidate.active) continue;
      for (const DependencyEntry& dependency : candidate.dependencies) {
        const int found = FindCandidate(*candidates, dependency.idFolded);
        if (found < 0) {
          SetFailure(RECOVERED_MOD_MISSING_DEPENDENCY,
                     "active mod dependency was not discovered",
                     candidate.id + " -> " + dependency.id);
          return false;
        }
        Candidate& target = (*candidates)[static_cast<std::size_t>(found)];
        if (target.version != dependency.version) {
          SetFailure(RECOVERED_MOD_DEPENDENCY_VERSION,
                     "active mod dependency version does not match",
                     candidate.id + " -> " + dependency.id + "@" +
                         dependency.version + " (found " + target.version +
                         ")");
          return false;
        }
        if (!target.active) {
          target.active = true;
          changed = true;
        }
      }
    }
  }

  std::size_t activeCount = 0;
  for (const Candidate& candidate : *candidates)
    if (candidate.active) ++activeCount;
  if (activeCount > kMaximumActiveMods) {
    SetFailure(RECOVERED_MOD_STACK_LIMIT,
               "active mod stack exceeds the 64 package limit");
    return false;
  }
  for (const Candidate& candidate : *candidates) {
    if (!candidate.active) continue;
    for (const std::string& conflict : candidate.conflicts) {
      const int found = FindCandidate(*candidates, conflict);
      if (found >= 0 &&
          (*candidates)[static_cast<std::size_t>(found)].active) {
        SetFailure(RECOVERED_MOD_CONFLICT,
                   "active mods declare a conflict",
                   candidate.id + " <-> " +
                       (*candidates)[static_cast<std::size_t>(found)].id);
        return false;
      }
    }
  }

  std::vector<std::vector<unsigned char>> edges(
      candidates->size(),
      std::vector<unsigned char>(candidates->size(), 0));
  std::vector<unsigned int> indegree(candidates->size(), 0);
  for (std::size_t index = 0; index < candidates->size(); ++index) {
    const Candidate& candidate = (*candidates)[index];
    if (!candidate.active) continue;
    for (const DependencyEntry& dependency : candidate.dependencies) {
      const int found = FindCandidate(*candidates, dependency.idFolded);
      if (!AddOrderEdge(&edges, &indegree, found,
                        static_cast<int>(index)))
        return false;
    }
    const auto addSoftEdges = [&](const std::vector<std::string>& ids) {
      for (const std::string& id : ids) {
        const int found = FindCandidate(*candidates, id);
        if (found >= 0 &&
            (*candidates)[static_cast<std::size_t>(found)].active &&
            !AddOrderEdge(&edges, &indegree, found,
                          static_cast<int>(index)))
          return false;
      }
      return true;
    };
    if (!addSoftEdges(candidate.loadAfter) ||
        !addSoftEdges(candidate.overrides)) {
      SetFailure(RECOVERED_MOD_ORDER_CYCLE,
                 "mod ordering relation references itself", candidate.id);
      return false;
    }
  }

  order->clear();
  order->reserve(activeCount);
  std::vector<unsigned char> emitted(candidates->size(), 0);
  while (order->size() < activeCount) {
    int selected = -1;
    for (std::size_t index = 0; index < candidates->size(); ++index) {
      if ((*candidates)[index].active && emitted[index] == 0 &&
          indegree[index] == 0) {
        selected = static_cast<int>(index);
        break;
      }
    }
    if (selected < 0) {
      SetFailure(RECOVERED_MOD_ORDER_CYCLE,
                 "active mod dependency/mount order contains a cycle");
      return false;
    }
    emitted[static_cast<std::size_t>(selected)] = 1;
    order->push_back(selected);
    for (std::size_t next = 0; next < candidates->size(); ++next) {
      if (edges[static_cast<std::size_t>(selected)][next] != 0)
        --indegree[next];
    }
  }
  return true;
}

bool BuildStack(const char* baseRoot,
                const char* const* candidateDirectories,
                std::size_t candidateCount,
                std::size_t explicitDirectoryCount,
                const char* const* requestedIds,
                std::size_t requestedIdCount,
                bool activateAllCandidates,
                StackCandidate* stack) {
  Candidate base;
  if (!BuildCandidate(baseRoot, nullptr, &base)) return false;
  stack->baseLexical = base.baseLexical;
  stack->baseFinal = base.baseFinal;
  stack->candidateCount = static_cast<unsigned int>(candidateCount);
  if (candidateCount == 0) {
    if (explicitDirectoryCount != 0 || candidateDirectories != nullptr) {
      SetFailure(RECOVERED_MOD_CANDIDATE_LIMIT,
                 "empty mod candidate set has inconsistent arguments");
      return false;
    }
    if (requestedIdCount != 0) {
      SetFailure(RECOVERED_MOD_MISSING_DEPENDENCY,
                 "requested mod was not discovered");
      return false;
    }
    return true;
  }
  if (candidateCount > kMaximumCandidateMods ||
      requestedIdCount > kMaximumCandidateMods ||
      explicitDirectoryCount > candidateCount ||
      candidateDirectories == nullptr) {
    SetFailure(RECOVERED_MOD_CANDIDATE_LIMIT,
               "mod candidate set is invalid or exceeds 128 packages");
    return false;
  }

  std::vector<Candidate> candidates;
  candidates.reserve(candidateCount);
  for (std::size_t index = 0; index < candidateCount; ++index) {
    if (candidateDirectories[index] == nullptr ||
        candidateDirectories[index][0] == '\0') {
      SetFailure(RECOVERED_MOD_INVALID_DIRECTORY,
                 "mod candidate directory is empty");
      return false;
    }
    Candidate candidate;
    if (!BuildCandidate(baseRoot, candidateDirectories[index], &candidate))
      return false;
    candidate.explicitlySelected = index < explicitDirectoryCount;
    candidate.active = candidate.explicitlySelected;
    candidates.push_back(std::move(candidate));
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              return FoldPath(left.id) < FoldPath(right.id);
            });
  for (std::size_t index = 1; index < candidates.size(); ++index) {
    if (FoldPath(candidates[index - 1].id) == FoldPath(candidates[index].id)) {
      SetFailure(RECOVERED_MOD_DUPLICATE_ID,
                 "mod candidate set contains a duplicate id",
                 candidates[index].id);
      return false;
    }
  }
  for (std::size_t left = 0; left < candidates.size(); ++left) {
    for (std::size_t right = left + 1; right < candidates.size(); ++right) {
      if (FoldPath(candidates[left].modFinal) ==
          FoldPath(candidates[right].modFinal)) {
        SetFailure(RECOVERED_MOD_DUPLICATE_ID,
                   "mod candidate set contains a duplicate path",
                   candidates[right].modFinal);
        return false;
      }
    }
  }

  std::vector<int> order;
  if (!SelectAndOrder(&candidates, requestedIds, requestedIdCount,
                      activateAllCandidates, &order))
    return false;

  for (int orderedIndex : order) {
    const Candidate& candidate =
        candidates[static_cast<std::size_t>(orderedIndex)];
    if (stack->totalBytes + candidate.totalBytes > kMaximumStackBytes) {
      SetFailure(RECOVERED_MOD_STACK_LIMIT,
                 "active mod stack exceeds the 1 GiB admitted data limit");
      return false;
    }
    stack->totalBytes += candidate.totalBytes;
    MountedPackage package;
    package.id = candidate.id;
    package.version = candidate.version;
    package.fileCount = static_cast<unsigned int>(candidate.entries.size());
    package.levelCount = static_cast<unsigned int>(candidate.levels.size());
    package.totalBytes = candidate.totalBytes;
    package.fingerprint = candidate.fingerprint;
    stack->packages.push_back(std::move(package));

    for (const LevelEntry& level : candidate.levels) {
      const auto found = std::lower_bound(
          stack->levels.begin(), stack->levels.end(), level.idFolded,
          [](const LevelEntry& entry, const std::string& value) {
            return entry.idFolded < value;
          });
      if (found != stack->levels.end() && found->idFolded == level.idFolded) {
        SetFailure(RECOVERED_MOD_DUPLICATE_LEVEL,
                   "active mods declare the same Level id", level.id);
        return false;
      }
      stack->levels.insert(found, level);
    }

    for (const OverlayEntry& entry : candidate.entries) {
      const auto found = std::lower_bound(
          stack->entries.begin(), stack->entries.end(), entry.targetFolded,
          [](const OverlayEntry& existing, const std::string& value) {
            return existing.targetFolded < value;
          });
      if (found != stack->entries.end() &&
          found->targetFolded == entry.targetFolded) {
        if (!ContainsText(candidate.overrides, FoldPath(found->ownerId))) {
          SetFailure(RECOVERED_MOD_TARGET_CONFLICT,
                     "active mods target the same file without an explicit "
                     "override",
                     found->ownerId + " <-> " + candidate.id + ": " +
                         entry.targetRelative);
          return false;
        }
        *found = entry;
      } else {
        stack->entries.insert(found, entry);
        if (stack->entries.size() > kMaximumStackFiles) {
          SetFailure(RECOVERED_MOD_STACK_LIMIT,
                     "active mod stack exceeds 4096 effective files");
          return false;
        }
      }
    }
  }

  if (stack->packages.size() == 1) {
    stack->fingerprint = stack->packages[0].fingerprint;
  } else if (!stack->packages.empty()) {
    stack->fingerprint = kFnvOffset;
    HashText(&stack->fingerprint, "RR2NW-MOD-STACK-1");
    HashU64(&stack->fingerprint,
            static_cast<std::uint64_t>(stack->packages.size()));
    for (const MountedPackage& package : stack->packages) {
      HashText(&stack->fingerprint, package.id);
      HashText(&stack->fingerprint, package.version);
      HashU64(&stack->fingerprint, package.fingerprint);
    }
    if (stack->fingerprint == 0) stack->fingerprint = 1;
  }
  return true;
}

}  // namespace

bool RecoveredModRuntime_Configure(const char* baseRoot,
                                   const char* modDirectory) {
  const char* directories[1] = {modDirectory};
  const std::size_t count =
      modDirectory != nullptr && modDirectory[0] != '\0' ? 1u : 0u;
  return RecoveredModRuntime_ConfigureStack(
      baseRoot, count == 0 ? nullptr : directories, count, count, nullptr, 0,
      false);
}

bool RecoveredModRuntime_ConfigureStack(
    const char* baseRoot, const char* const* candidateDirectories,
    std::size_t candidateCount, std::size_t explicitDirectoryCount,
    const char* const* requestedIds, std::size_t requestedIdCount,
    bool activateAllCandidates) {
  g_issues = 0;
  g_lastError[0] = '\0';
  if (baseRoot == nullptr || baseRoot[0] == '\0') {
    SetFailure(RECOVERED_MOD_INVALID_ARGUMENT,
               "mod runtime received no base data root");
    return false;
  }
  try {
    StackCandidate stack;
    if (!BuildStack(baseRoot, candidateDirectories, candidateCount,
                    explicitDirectoryCount, requestedIds, requestedIdCount,
                    activateAllCandidates, &stack))
      return false;
    const bool active = !stack.packages.empty();
    g_baseLexical = std::move(stack.baseLexical);
    g_baseFinal = std::move(stack.baseFinal);
    g_modFinal.clear();
    g_entries = std::move(stack.entries);
    g_levels = std::move(stack.levels);
    g_packages = std::move(stack.packages);
    g_activeLevelIdentity.clear();
    g_activeLevelBase.clear();
    g_activeLevelIdentityFolded.clear();
    g_activeLevelBaseFolded.clear();
    g_activeLevelDerived = false;
    g_summary = SRecoveredModRuntimeSummary{};
    g_summary.schemaVersion = active ? kManifestSchema : 0;
    g_summary.engineApi = active ? kEngineApi : 0;
    g_summary.candidateCount = stack.candidateCount;
    g_summary.modCount = static_cast<unsigned int>(g_packages.size());
    if (g_packages.size() == 1) {
      std::snprintf(g_summary.id, sizeof(g_summary.id), "%s",
                    g_packages[0].id.c_str());
      std::snprintf(g_summary.version, sizeof(g_summary.version), "%s",
                    g_packages[0].version.c_str());
    } else if (g_packages.size() > 1) {
      std::snprintf(g_summary.id, sizeof(g_summary.id), "%s",
                    "rr2nw.mod-stack");
      std::snprintf(g_summary.version, sizeof(g_summary.version), "%s",
                    "1.0.0");
    }
    g_summary.fileCount = static_cast<unsigned int>(g_entries.size());
    g_summary.levelCount = static_cast<unsigned int>(g_levels.size());
    g_summary.totalBytes = stack.totalBytes;
    g_summary.modFingerprint = active ? stack.fingerprint : 0;
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
  g_levels.clear();
  g_packages.clear();
  g_activeLevelIdentity.clear();
  g_activeLevelBase.clear();
  g_activeLevelIdentityFolded.clear();
  g_activeLevelBaseFolded.clear();
  g_activeLevelDerived = false;
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

unsigned int RecoveredModRuntime_ModCount() {
  return g_configured ? static_cast<unsigned int>(g_packages.size()) : 0;
}

bool RecoveredModRuntime_Mod(unsigned int index,
                             SRecoveredModPackage* package) {
  if (!g_configured || package == nullptr || index >= g_packages.size())
    return false;
  const MountedPackage& mounted = g_packages[index];
  *package = SRecoveredModPackage{};
  std::snprintf(package->id, sizeof(package->id), "%s", mounted.id.c_str());
  std::snprintf(package->version, sizeof(package->version), "%s",
                mounted.version.c_str());
  package->mountIndex = index;
  package->fileCount = mounted.fileCount;
  package->levelCount = mounted.levelCount;
  package->totalBytes = mounted.totalBytes;
  package->fingerprint = mounted.fingerprint;
  return true;
}

unsigned int RecoveredModRuntime_LevelCount() {
  return g_configured ? static_cast<unsigned int>(g_levels.size()) : 0;
}

bool RecoveredModRuntime_Level(unsigned int index,
                               SRecoveredModLevel* level) {
  if (!g_configured || level == nullptr || index >= g_levels.size())
    return false;
  *level = SRecoveredModLevel{};
  std::snprintf(level->id, sizeof(level->id), "%s",
                g_levels[index].id.c_str());
  std::snprintf(level->base, sizeof(level->base), "%s",
                g_levels[index].base.c_str());
  return true;
}

bool RecoveredModRuntime_ActivateLevel(const char* identity,
                                       char* physicalDirectory,
                                       std::size_t physicalDirectorySize) {
  if (!g_configured || identity == nullptr || identity[0] == '\0' ||
      physicalDirectory == nullptr || physicalDirectorySize == 0 ||
      !ValidLevelComponent(identity)) {
    SetFailure(RECOVERED_MOD_INVALID_LEVEL_ENTRY,
               "cannot activate an invalid Level identity",
               identity == nullptr ? std::string() : std::string(identity));
    return false;
  }
  const std::string folded = FoldPath(identity);
  const auto found = std::lower_bound(
      g_levels.begin(), g_levels.end(), folded,
      [](const LevelEntry& entry, const std::string& value) {
        return entry.idFolded < value;
      });
  const bool derived =
      found != g_levels.end() && found->idFolded == folded;
  const std::string selectedIdentity = derived ? found->id : identity;
  const std::string selectedBase = derived ? found->base : identity;
  const std::string selectedIdentityFolded = FoldPath(selectedIdentity);
  const std::string selectedBaseFolded = FoldPath(selectedBase);
  const std::string directory = JoinPath(g_baseLexical, selectedBase);
  if (directory.size() + 1u > physicalDirectorySize) {
    SetFailure(RECOVERED_MOD_PATH_FAILURE,
               "activated Level path exceeds the destination buffer",
               selectedBase);
    return false;
  }
  std::memcpy(physicalDirectory, directory.c_str(), directory.size() + 1u);
  g_activeLevelIdentity = selectedIdentity;
  g_activeLevelBase = selectedBase;
  g_activeLevelIdentityFolded = selectedIdentityFolded;
  g_activeLevelBaseFolded = selectedBaseFolded;
  g_activeLevelDerived = derived;
  g_issues = 0;
  g_lastError[0] = '\0';
  return true;
}

const char* RecoveredModRuntime_ActiveLevelIdentity() {
  return g_configured && !g_activeLevelIdentity.empty()
             ? g_activeLevelIdentity.c_str()
             : nullptr;
}

const char* RecoveredModRuntime_ActiveLevelBase() {
  return g_configured && !g_activeLevelBase.empty()
             ? g_activeLevelBase.c_str()
             : nullptr;
}

bool RecoveredModRuntime_ActiveLevelIsDerived() {
  return g_configured && g_activeLevelDerived;
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
        std::string lookup = folded;
        if (g_activeLevelDerived &&
            folded.size() > g_activeLevelBaseFolded.size() &&
            folded.compare(0, g_activeLevelBaseFolded.size(),
                           g_activeLevelBaseFolded) == 0 &&
            folded[g_activeLevelBaseFolded.size()] == '\\') {
          lookup = g_activeLevelIdentityFolded +
                   folded.substr(g_activeLevelBaseFolded.size());
        }
        auto found = std::lower_bound(
            g_entries.begin(), g_entries.end(), lookup,
            [](const OverlayEntry& entry, const std::string& value) {
              return entry.targetFolded < value;
            });
        if ((found == g_entries.end() ||
             found->targetFolded != lookup) && lookup != folded) {
          found = std::lower_bound(
              g_entries.begin(), g_entries.end(), folded,
              [](const OverlayEntry& entry, const std::string& value) {
                return entry.targetFolded < value;
              });
          lookup = folded;
        }
        if (found != g_entries.end() && found->targetFolded == lookup) {
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

bool RecoveredModRuntime_ResolveBaseReadPath(const char* requested,
                                             char* resolved,
                                             std::size_t resolvedSize) {
  if (requested == nullptr || requested[0] == '\0' || resolved == nullptr ||
      resolvedSize == 0)
    return false;
  if (!g_configured)
    return RecoveredModRuntime_ResolveReadPath(requested, resolved,
                                               resolvedSize);

  const bool absolute =
      requested[0] == '\\' || requested[0] == '/' ||
      (std::strlen(requested) > 1u && requested[1] == ':');
  std::string full;
  if (!FullPath(absolute ? std::string(requested)
                         : JoinPath(g_baseLexical, requested),
                &full) ||
      (!IsWithin(full, g_baseLexical) && !IsWithin(full, g_baseFinal)))
    return false;
  return RecoveredModRuntime_ResolveReadPath(full.c_str(), resolved,
                                             resolvedSize);
}

bool RecoveredModRuntime_ListLevelFiles(
    const char* relativeDirectory, const char* extension,
    std::vector<std::string>* paths, std::string* failure) {
  if (failure != nullptr) failure->clear();
  if (!g_configured || g_activeLevelBase.empty()) {
    if (failure != nullptr)
      *failure = "no active Level is bound to the resource catalog";
    return false;
  }
  if (relativeDirectory == nullptr || extension == nullptr ||
      paths == nullptr || !paths->empty()) {
    if (failure != nullptr)
      *failure = "Level file catalog arguments are invalid";
    return false;
  }
  std::string directory;
  if (!NormalizeRelative(relativeDirectory, &directory) ||
      extension[0] != '.' || std::strchr(extension, '\\') != nullptr ||
      std::strchr(extension, '/') != nullptr ||
      std::strpbrk(extension, "*?\"<>|") != nullptr) {
    if (failure != nullptr)
      *failure = "Level file catalog directory or extension is invalid";
    return false;
  }

  const std::string physical =
      JoinPath(JoinPath(g_baseLexical, g_activeLevelBase), directory);
  if (!EnumeratePhysicalFiles(physical, directory, extension, paths,
                              failure)) {
    paths->clear();
    return false;
  }

  const std::string directoryFolded = FoldPath(directory);
  const auto appendOverlayTargets = [&](const std::string& levelFolded) {
    const std::string prefix = levelFolded + "\\";
    for (const OverlayEntry& entry : g_entries) {
      if (entry.targetFolded.size() <= prefix.size() ||
          entry.targetFolded.compare(0, prefix.size(), prefix) != 0)
        continue;
      const std::string levelRelative =
          entry.targetRelative.substr(prefix.size());
      const std::string foldedRelative = FoldPath(levelRelative);
      if (foldedRelative.size() <= directoryFolded.size() ||
          foldedRelative.compare(0, directoryFolded.size(),
                                 directoryFolded) != 0 ||
          foldedRelative[directoryFolded.size()] != '\\' ||
          !HasExtension(levelRelative, extension))
        continue;
      paths->push_back(levelRelative);
    }
  };
  // Derived targets win in ResolveReadPath; adding both here is safe because
  // the final case-insensitive de-duplication keeps a single virtual name.
  appendOverlayTargets(g_activeLevelIdentityFolded);
  if (g_activeLevelBaseFolded != g_activeLevelIdentityFolded)
    appendOverlayTargets(g_activeLevelBaseFolded);
  if (paths->size() > kMaximumEnumeratedLevelFiles) {
    paths->clear();
    if (failure != nullptr)
      *failure = "effective Level file catalog exceeded its safety limit";
    return false;
  }
  std::sort(paths->begin(), paths->end(),
            [](const std::string& left, const std::string& right) {
              const std::string leftFolded = FoldPath(left);
              const std::string rightFolded = FoldPath(right);
              return leftFolded == rightFolded ? left < right
                                               : leftFolded < rightFolded;
            });
  paths->erase(std::unique(paths->begin(), paths->end(),
                           [](const std::string& left,
                              const std::string& right) {
                             return FoldPath(left) == FoldPath(right);
                           }),
               paths->end());
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

FILE* RecoveredModRuntime_OpenBaseRead(const char* requested, long* length) {
  char resolved[4096] = {};
  if (!RecoveredModRuntime_ResolveBaseReadPath(requested, resolved,
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
  if (g_packages.size() == 1) {
    // Preserve the schema-1 single-package identity byte-for-byte so existing
    // saves and continuation journals remain compatible.
    HashText(&hash, "RR2NW-MOD-CONTENT-1");
    HashU64(&hash, baseFingerprint);
    HashText(&hash, g_packages[0].id);
    HashText(&hash, g_packages[0].version);
    HashU64(&hash, g_packages[0].fingerprint);
  } else {
    HashText(&hash, "RR2NW-MOD-CONTENT-SET-1");
    HashU64(&hash, baseFingerprint);
    HashU64(&hash, static_cast<std::uint64_t>(g_packages.size()));
    for (const MountedPackage& package : g_packages) {
      HashText(&hash, package.id);
      HashText(&hash, package.version);
      HashU64(&hash, package.fingerprint);
    }
  }
  return hash == 0 ? 1 : hash;
}
