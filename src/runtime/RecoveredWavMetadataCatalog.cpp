#include "RecoveredWavMetadataCatalog.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr long kMaximumSourceBytes = 128 * 1024;
constexpr unsigned long long kHashOffset = 14695981039346656037ull;
constexpr unsigned long long kHashPrime = 1099511628211ull;

enum ETokenKind { TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER, TOKEN_MARK };

struct Token {
  ETokenKind kind;
  std::string text;
};

void HashBytes(unsigned long long* hash, const void* data,
               std::size_t size) {
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

void HashString(unsigned long long* hash, const char* value) {
  HashBytes(hash, value, std::strlen(value) + 1);
}

bool Fail(SRecoveredWavMetadataCatalogResult* result, unsigned int issue,
          const char* message) {
  if (result != nullptr) {
    result->issues |= issue;
    if (result->error[0] == 0) {
      std::snprintf(result->error, sizeof(result->error), "%s", message);
    }
  }
  return false;
}

void Reset(SRecoveredWavMetadataCatalog* catalog,
           SRecoveredWavMetadataCatalogResult* result) {
  if (catalog != nullptr) std::memset(catalog, 0, sizeof(*catalog));
  if (result != nullptr) std::memset(result, 0, sizeof(*result));
}

std::string JoinPath(const char* directory, const char* name) {
  std::string path(directory);
  if (!path.empty() && path.back() != '\\' && path.back() != '/') {
    path.push_back('\\');
  }
  path.append(name);
  return path;
}

bool ReadSource(const std::string& path, std::string* source,
                SRecoveredWavMetadataCatalogResult* result) {
  FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return Fail(result, RECOVERED_WAV_CATALOG_SOURCE_UNAVAILABLE,
                "could not open WAV metadata source");
  }
  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return Fail(result, RECOVERED_WAV_CATALOG_SOURCE_UNAVAILABLE,
                "could not measure WAV metadata source");
  }
  const long size = std::ftell(file);
  if (size <= 0 || size > kMaximumSourceBytes ||
      std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return Fail(result, RECOVERED_WAV_CATALOG_SOURCE_TOO_LARGE,
                "WAV metadata source has an invalid bounded size");
  }
  try {
    source->resize(static_cast<std::size_t>(size));
  } catch (...) {
    std::fclose(file);
    return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                "could not allocate WAV metadata source");
  }
  const bool read =
      std::fread(&(*source)[0], 1, static_cast<std::size_t>(size), file) ==
      static_cast<std::size_t>(size);
  std::fclose(file);
  if (!read || source->find('\0') != std::string::npos) {
    return Fail(result, RECOVERED_WAV_CATALOG_SOURCE_UNAVAILABLE,
                "could not read complete WAV metadata source");
  }
  return true;
}

bool IsIdentifierStart(unsigned char value) {
  return std::isalpha(value) != 0 || value == '_';
}

bool IsIdentifierPart(unsigned char value) {
  return std::isalnum(value) != 0 || value == '_';
}

bool Tokenize(const std::string& source, std::vector<Token>* tokens,
              SRecoveredWavMetadataCatalogResult* result) {
  for (std::size_t index = 0; index < source.size();) {
    const unsigned char value = static_cast<unsigned char>(source[index]);
    if (std::isspace(value) != 0) {
      ++index;
      continue;
    }
    if (value == '/' && index + 1 < source.size() &&
        source[index + 1] == '/') {
      index += 2;
      while (index < source.size() && source[index] != '\n') ++index;
      continue;
    }
    if (value == '/' && index + 1 < source.size() &&
        source[index + 1] == '*') {
      index += 2;
      bool closed = false;
      while (index + 1 < source.size()) {
        if (source[index] == '*' && source[index + 1] == '/') {
          index += 2;
          closed = true;
          break;
        }
        ++index;
      }
      if (!closed) {
        return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                    "unterminated comment in WAV metadata source");
      }
      continue;
    }

    Token token = {};
    if (IsIdentifierStart(value)) {
      token.kind = TOKEN_IDENTIFIER;
      const std::size_t start = index++;
      while (index < source.size() &&
             IsIdentifierPart(static_cast<unsigned char>(source[index]))) {
        ++index;
      }
      token.text.assign(source, start, index - start);
    } else if (value == '"') {
      token.kind = TOKEN_STRING;
      ++index;
      bool closed = false;
      while (index < source.size()) {
        const char current = source[index++];
        if (current == '"') {
          closed = true;
          break;
        }
        token.text.push_back(current);
      }
      if (!closed) {
        return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                    "unterminated string in WAV metadata source");
      }
    } else if (std::isdigit(value) != 0 || value == '.' ||
               ((value == '-' || value == '+') &&
                index + 1 < source.size() &&
                (std::isdigit(static_cast<unsigned char>(source[index + 1])) !=
                     0 ||
                 source[index + 1] == '.'))) {
      token.kind = TOKEN_NUMBER;
      const std::size_t start = index++;
      while (index < source.size()) {
        const unsigned char current =
            static_cast<unsigned char>(source[index]);
        if (std::isdigit(current) == 0 && current != '.' && current != 'e' &&
            current != 'E' && current != '+' && current != '-') {
          break;
        }
        ++index;
      }
      token.text.assign(source, start, index - start);
    } else {
      token.kind = TOKEN_MARK;
      token.text.assign(1, static_cast<char>(value));
      ++index;
    }
    try {
      tokens->push_back(token);
    } catch (...) {
      return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                  "could not allocate WAV metadata tokens");
    }
  }
  return true;
}

bool Is(const std::vector<Token>& tokens, std::size_t index,
        ETokenKind kind, const char* text) {
  return index < tokens.size() && tokens[index].kind == kind &&
         tokens[index].text == text;
}

std::size_t FindFunctionBody(const std::vector<Token>& tokens,
                             const char* name) {
  for (std::size_t index = 0; index + 3 < tokens.size(); ++index) {
    if (Is(tokens, index, TOKEN_IDENTIFIER, "func") &&
        Is(tokens, index + 1, TOKEN_IDENTIFIER, "void") &&
        Is(tokens, index + 2, TOKEN_IDENTIFIER, name)) {
      for (std::size_t cursor = index + 3; cursor < tokens.size(); ++cursor) {
        if (Is(tokens, cursor, TOKEN_MARK, "{")) return cursor + 1;
      }
    }
  }
  return tokens.size();
}

bool ParseInt(const Token& token, int* value) {
  if (token.kind != TOKEN_NUMBER || token.text.empty()) return false;
  char* end = nullptr;
  errno = 0;
  const long parsed = std::strtol(token.text.c_str(), &end, 10);
  if (errno != 0 || end == nullptr || *end != 0 || parsed < 0 ||
      parsed > 10000) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

bool ParseFloat(const Token& token, float* value) {
  if (token.kind != TOKEN_NUMBER || token.text.empty()) return false;
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(token.text.c_str(), &end);
  if (errno != 0 || end == nullptr || *end != 0 || parsed < 0.0 ||
      parsed > 1000000.0) {
    return false;
  }
  *value = static_cast<float>(parsed);
  return true;
}

bool CopyString(const std::string& value, char* destination,
                std::size_t capacity) {
  if (value.empty() || value.size() >= capacity) return false;
  std::memcpy(destination, value.c_str(), value.size() + 1);
  return true;
}

bool ParseCapacity(const std::string& source, int* capacity,
                   SRecoveredWavMetadataCatalogResult* result) {
  std::vector<Token> tokens;
  if (!Tokenize(source, &tokens, result)) return false;
  const std::size_t body = FindFunctionBody(tokens, "local_createTables");
  if (body == tokens.size()) {
    return Fail(result, RECOVERED_WAV_CATALOG_TABLE_INVALID,
                "local_createTables body was not found");
  }
  int depth = 1;
  bool found = false;
  for (std::size_t index = body; index < tokens.size() && depth > 0; ++index) {
    if (Is(tokens, index, TOKEN_MARK, "{")) {
      ++depth;
      continue;
    }
    if (Is(tokens, index, TOKEN_MARK, "}")) {
      --depth;
      continue;
    }
    if (depth != 1 ||
        !Is(tokens, index, TOKEN_IDENTIFIER, "s_AddClassTable")) {
      continue;
    }
    if (!Is(tokens, index + 1, TOKEN_MARK, "(") ||
        index + 5 >= tokens.size() ||
        !Is(tokens, index + 2, TOKEN_STRING, "WAVObj")) {
      continue;
    }
    int parsed = 0;
    if (!Is(tokens, index + 3, TOKEN_MARK, ",") ||
        !ParseInt(tokens[index + 4], &parsed) || parsed <= 0 ||
        parsed > SRecoveredWavMetadataCatalog::MAX_ENTRIES ||
        !Is(tokens, index + 5, TOKEN_MARK, ")") || found) {
      return Fail(result, RECOVERED_WAV_CATALOG_TABLE_INVALID,
                  "WAVObj class-table declaration is invalid");
    }
    *capacity = parsed;
    found = true;
  }
  if (depth != 0 || !found) {
    return Fail(result, RECOVERED_WAV_CATALOG_TABLE_INVALID,
                "WAVObj class-table declaration was not found");
  }
  return true;
}

bool ExpectMark(const std::vector<Token>& tokens, std::size_t* cursor,
                const char* mark) {
  if (!Is(tokens, *cursor, TOKEN_MARK, mark)) return false;
  ++*cursor;
  return true;
}

bool ParseCall(const std::vector<Token>& tokens, std::size_t index,
               bool extended, SRecoveredWavMetadataEntry* entry,
               SRecoveredWavMetadataCatalogResult* result) {
  std::size_t cursor = index + 1;
  if (!ExpectMark(tokens, &cursor, "(") ||
      cursor >= tokens.size() || tokens[cursor].kind != TOKEN_IDENTIFIER) {
    return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                "malformed LoadWAV class-table argument");
  }
  ++cursor;
  if (!ExpectMark(tokens, &cursor, ",") || cursor >= tokens.size() ||
      tokens[cursor].kind != TOKEN_STRING ||
      !CopyString(tokens[cursor++].text, entry->objectName,
                  sizeof(entry->objectName)) ||
      !ExpectMark(tokens, &cursor, ",") || cursor >= tokens.size() ||
      tokens[cursor].kind != TOKEN_STRING ||
      !CopyString(tokens[cursor++].text, entry->fileName,
                  sizeof(entry->fileName))) {
    return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                "malformed LoadWAV symbolic or file name");
  }

  float* values[] = {&entry->minFront, &entry->minBack, &entry->maxFront,
                     &entry->maxBack, &entry->intensity};
  for (float* value : values) {
    if (!ExpectMark(tokens, &cursor, ",") || cursor >= tokens.size() ||
        !ParseFloat(tokens[cursor++], value)) {
      return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                  "malformed LoadWAV distance or intensity");
    }
  }
  entry->flags = 0;
  if (extended) {
    if (!ExpectMark(tokens, &cursor, ",") || cursor >= tokens.size() ||
        !ParseInt(tokens[cursor++], &entry->flags) || entry->flags > 1) {
      return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                  "malformed LoadWAVEx flags");
    }
  }
  if (!ExpectMark(tokens, &cursor, ")")) {
    return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                "malformed LoadWAV closing delimiter");
  }
  if (std::strstr(entry->fileName, "..") != nullptr ||
      std::strpbrk(entry->fileName, "\\/:") != nullptr) {
    return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                "WAV file name escapes the legacy SOUND directory");
  }
  return true;
}

bool ParseEntries(const std::string& source,
                  SRecoveredWavMetadataCatalog* catalog,
                  SRecoveredWavMetadataCatalogResult* result) {
  std::vector<Token> tokens;
  if (!Tokenize(source, &tokens, result)) return false;
  const std::size_t body = FindFunctionBody(tokens, "LoadAllWaves");
  if (body == tokens.size()) {
    return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                "LoadAllWaves body was not found");
  }
  int depth = 1;
  std::set<std::string> names;
  for (std::size_t index = body; index < tokens.size() && depth > 0; ++index) {
    if (Is(tokens, index, TOKEN_MARK, "{")) {
      ++depth;
      continue;
    }
    if (Is(tokens, index, TOKEN_MARK, "}")) {
      --depth;
      continue;
    }
    if (depth != 1 || tokens[index].kind != TOKEN_IDENTIFIER ||
        (tokens[index].text != "LoadWAV" &&
         tokens[index].text != "LoadWAVEx")) {
      continue;
    }
    if (catalog->entryCount >= SRecoveredWavMetadataCatalog::MAX_ENTRIES) {
      return Fail(result, RECOVERED_WAV_CATALOG_CAPACITY_EXCEEDED,
                  "WAV metadata roster exceeds its bounded maximum");
    }
    SRecoveredWavMetadataEntry entry = {};
    if (!ParseCall(tokens, index, tokens[index].text == "LoadWAVEx", &entry,
                   result)) {
      return false;
    }
    if (!names.insert(entry.objectName).second) {
      return Fail(result, RECOVERED_WAV_CATALOG_DUPLICATE_NAME,
                  "LoadAllWaves contains a duplicate symbolic name");
    }
    catalog->entries[catalog->entryCount++] = entry;
  }
  if (depth != 0 || catalog->entryCount <= 0) {
    return Fail(result, RECOVERED_WAV_CATALOG_PARSE_FAILURE,
                "LoadAllWaves is empty or has unbalanced braces");
  }
  if (catalog->entryCount > catalog->capacity) {
    return Fail(result, RECOVERED_WAV_CATALOG_CAPACITY_EXCEEDED,
                "WAV metadata roster exceeds the WAVObj table capacity");
  }
  return true;
}

bool EntryLess(const SRecoveredWavMetadataEntry& left,
               const SRecoveredWavMetadataEntry& right) {
  return std::strcmp(left.objectName, right.objectName) < 0;
}

void FinishFingerprint(const std::string& localMain,
                       const std::string& loadWav,
                       SRecoveredWavMetadataCatalog* catalog) {
  catalog->localMainSourceFingerprint = kHashOffset;
  HashBytes(&catalog->localMainSourceFingerprint, localMain.data(),
            localMain.size());
  catalog->loadWavSourceFingerprint = kHashOffset;
  HashBytes(&catalog->loadWavSourceFingerprint, loadWav.data(),
            loadWav.size());
  std::sort(catalog->entries, catalog->entries + catalog->entryCount,
            EntryLess);
  unsigned long long hash = kHashOffset;
  HashBytes(&hash, &catalog->capacity, sizeof(catalog->capacity));
  for (int index = 0; index < catalog->entryCount; ++index) {
    const SRecoveredWavMetadataEntry& entry = catalog->entries[index];
    HashString(&hash, entry.objectName);
    HashString(&hash, entry.fileName);
    HashBytes(&hash, &entry.minFront, sizeof(entry.minFront));
    HashBytes(&hash, &entry.minBack, sizeof(entry.minBack));
    HashBytes(&hash, &entry.maxFront, sizeof(entry.maxFront));
    HashBytes(&hash, &entry.maxBack, sizeof(entry.maxBack));
    HashBytes(&hash, &entry.intensity, sizeof(entry.intensity));
    HashBytes(&hash, &entry.flags, sizeof(entry.flags));
  }
  catalog->fingerprint = hash;
}

}  // namespace

bool RecoveredWavMetadataCatalog_Load(
    const char* levelDirectory, SRecoveredWavMetadataCatalog* catalog,
    SRecoveredWavMetadataCatalogResult* result) {
  Reset(catalog, result);
  if (levelDirectory == nullptr || levelDirectory[0] == 0 ||
      catalog == nullptr || result == nullptr) {
    return Fail(result, RECOVERED_WAV_CATALOG_INVALID_ARGUMENT,
                "WAV metadata catalog received invalid input");
  }
  std::string localMain;
  std::string loadWav;
  if (!ReadSource(JoinPath(levelDirectory, "SCINC\\LOCALMAIN.SCI"),
                  &localMain, result) ||
      !ReadSource(JoinPath(levelDirectory, "SCINC\\LOADWAV.SCI"), &loadWav,
                  result) ||
      !ParseCapacity(localMain, &catalog->capacity, result) ||
      !ParseEntries(loadWav, catalog, result)) {
    return false;
  }
  FinishFingerprint(localMain, loadWav, catalog);
  return true;
}

bool RecoveredWavMetadataCatalog_IsKnown(
    const SRecoveredWavMetadataCatalog* catalog) {
  if (catalog == nullptr) return false;
  // Nine canonical May Levels followed by the public January CI fixture.
  struct KnownCatalog {
    unsigned long long fingerprint;
    unsigned long long localMainSourceFingerprint;
    unsigned long long loadWavSourceFingerprint;
  };
  static const KnownCatalog known[] = {
      {18427911194505023745ull, 12966194887948841299ull,
       10566301141390917609ull},
      {4633857832084587996ull, 5414602383695704108ull,
       17624194919597695080ull},
      {12826306996657882107ull, 16928167649038994112ull,
       12934614853612480047ull},
      {12370419092194669116ull, 4236501613845883799ull,
       425657172527409821ull},
      {9649152438776867277ull, 5361401251479619817ull,
       7617513766149151158ull},
      {13040795140785882021ull, 14395166287231743195ull,
       4538943902053419465ull},
      {6968472016635930248ull, 14020952532020993176ull,
       12262840689618526566ull},
      {1186999906182190271ull, 3946652576317980590ull,
       11637125649737780350ull},
      {14511910215820770629ull, 2766941644914621626ull,
       6451107337577768100ull},
      {12429727661699909001ull, 17986979700419621059ull,
       2837778032068376996ull}};
  for (const KnownCatalog& entry : known) {
    if (catalog->fingerprint == entry.fingerprint &&
        catalog->localMainSourceFingerprint ==
            entry.localMainSourceFingerprint &&
        catalog->loadWavSourceFingerprint ==
            entry.loadWavSourceFingerprint) {
      return true;
    }
  }
  return false;
}
