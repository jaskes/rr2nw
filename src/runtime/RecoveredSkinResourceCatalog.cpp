#include "RecoveredSkinResourceCatalog.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kMaximumSourceBytes = 128u * 1024u;
constexpr unsigned long long kMaximumAssetBytes = 64ull * 1024ull * 1024ull;
constexpr unsigned long long kHashOffset = 14695981039346656037ull;
constexpr unsigned long long kHashPrime = 1099511628211ull;

enum ETokenKind { TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_INTEGER, TOKEN_MARK };

struct Token {
  ETokenKind kind;
  std::string text;
  int line;
};

void Reset(SRecoveredSkinResourceCatalog* catalog,
           SRecoveredSkinResourceCatalogResult* result) {
  if (catalog != nullptr) std::memset(catalog, 0, sizeof(*catalog));
  if (result != nullptr) std::memset(result, 0, sizeof(*result));
}

bool Fail(SRecoveredSkinResourceCatalogResult* result, unsigned int issue,
          const char* message) {
  if (result != nullptr) {
    result->issues |= issue;
    if (result->error[0] == 0) {
      std::snprintf(result->error, sizeof(result->error), "%s",
                    message == nullptr ? "skin catalog failure" : message);
    }
  }
  return false;
}

void HashBytes(unsigned long long* hash, const void* data, std::size_t size) {
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

void HashString(unsigned long long* hash, const char* value) {
  HashBytes(hash, value, std::strlen(value) + 1);
}

std::string JoinPath(const char* root, const char* relative) {
  std::string result(root == nullptr ? "" : root);
  if (!result.empty() && result.back() != '\\' && result.back() != '/') {
    result.push_back('\\');
  }
  result.append(relative == nullptr ? "" : relative);
  return result;
}

bool ReadSource(const std::string& path, std::string* source,
                SRecoveredSkinResourceCatalogResult* result) {
  FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return Fail(result, RECOVERED_SKIN_CATALOG_SOURCE_UNAVAILABLE,
                "could not open SCINC\\SKIN.SCI");
  }
  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return Fail(result, RECOVERED_SKIN_CATALOG_SOURCE_UNAVAILABLE,
                "could not measure SCINC\\SKIN.SCI");
  }
  const long size = std::ftell(file);
  if (size <= 0 || static_cast<std::size_t>(size) > kMaximumSourceBytes) {
    std::fclose(file);
    return Fail(result, RECOVERED_SKIN_CATALOG_SOURCE_TOO_LARGE,
                "SCINC\\SKIN.SCI has an invalid bounded size");
  }
  if (std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return Fail(result, RECOVERED_SKIN_CATALOG_SOURCE_UNAVAILABLE,
                "could not rewind SCINC\\SKIN.SCI");
  }
  try {
    source->assign(static_cast<std::size_t>(size), '\0');
  } catch (...) {
    std::fclose(file);
    return Fail(result, RECOVERED_SKIN_CATALOG_SOURCE_UNAVAILABLE,
                "could not allocate the bounded Skin source");
  }
  const bool read =
      std::fread(&(*source)[0], 1, source->size(), file) == source->size();
  const bool closed = std::fclose(file) == 0;
  return read && closed
             ? true
             : Fail(result, RECOVERED_SKIN_CATALOG_SOURCE_UNAVAILABLE,
                    "could not read the complete Skin source");
}

bool IsIdentifierStart(unsigned char value) {
  return std::isalpha(value) != 0 || value == '_';
}

bool IsIdentifierPart(unsigned char value) {
  return std::isalnum(value) != 0 || value == '_';
}

bool Tokenize(const std::string& source, std::vector<Token>* tokens,
              SRecoveredSkinResourceCatalogResult* result) {
  int line = 1;
  for (std::size_t index = 0; index < source.size();) {
    const unsigned char value = static_cast<unsigned char>(source[index]);
    if (value == '\r' || value == '\n' || std::isspace(value) != 0) {
      if (value == '\n') ++line;
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
        if (source[index] == '\n') ++line;
        if (source[index] == '*' && source[index + 1] == '/') {
          index += 2;
          closed = true;
          break;
        }
        ++index;
      }
      if (!closed) {
        return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                    "unterminated comment in SKIN.SCI");
      }
      continue;
    }
    Token token = {};
    token.line = line;
    if (IsIdentifierStart(value)) {
      token.kind = TOKEN_IDENTIFIER;
      const std::size_t start = index++;
      while (index < source.size() &&
             IsIdentifierPart(static_cast<unsigned char>(source[index]))) {
        ++index;
      }
      token.text.assign(source, start, index - start);
    } else if (std::isdigit(value) != 0) {
      token.kind = TOKEN_INTEGER;
      const std::size_t start = index++;
      while (index < source.size() &&
             std::isdigit(static_cast<unsigned char>(source[index])) != 0) {
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
        if (current == '\\' && index < source.size()) {
          token.text.push_back(source[index++]);
        } else {
          if (current == '\n') ++line;
          token.text.push_back(current);
        }
      }
      if (!closed) {
        return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                    "unterminated string in SKIN.SCI");
      }
    } else {
      token.kind = TOKEN_MARK;
      token.text.assign(1, static_cast<char>(value));
      ++index;
    }
    try {
      tokens->push_back(token);
    } catch (...) {
      return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                  "could not allocate Skin source tokens");
    }
  }
  return true;
}

bool Is(const std::vector<Token>& tokens, std::size_t index,
        ETokenKind kind, const char* text) {
  return index < tokens.size() && tokens[index].kind == kind &&
         tokens[index].text == text;
}

bool ParsePositiveInt(const Token& token, int* value) {
  if (token.kind != TOKEN_INTEGER || token.text.empty() ||
      token.text.size() > 4) {
    return false;
  }
  int parsed = 0;
  for (char digit : token.text) parsed = parsed * 10 + digit - '0';
  if (parsed <= 0 || parsed > SRecoveredSkinResourceCatalog::MAX_ENTRIES) {
    return false;
  }
  *value = parsed;
  return true;
}

bool CopyString(const std::string& value, char* destination,
                std::size_t capacity) {
  if (value.empty() || value.size() >= capacity) return false;
  std::memcpy(destination, value.c_str(), value.size() + 1);
  return true;
}

bool ParseTableCall(const std::vector<Token>& tokens, std::size_t index,
                    ERecoveredSkinResourceKind* kind, int* capacity) {
  if (!Is(tokens, index, TOKEN_IDENTIFIER, "s_AddClassTable") ||
      !Is(tokens, index + 1, TOKEN_MARK, "(") ||
      index + 5 >= tokens.size() || tokens[index + 2].kind != TOKEN_STRING ||
      !Is(tokens, index + 3, TOKEN_MARK, ",") ||
      !ParsePositiveInt(tokens[index + 4], capacity) ||
      !Is(tokens, index + 5, TOKEN_MARK, ")")) {
    return false;
  }
  if (tokens[index + 2].text == "Skin") {
    *kind = RECOVERED_SKIN_RESOURCE_MODEL;
  } else if (tokens[index + 2].text == "SkinSpr") {
    *kind = RECOVERED_SKIN_RESOURCE_SPRITE;
  } else {
    return false;
  }
  return true;
}

bool AddEntry(const std::vector<Token>& tokens, std::size_t index,
              ERecoveredSkinResourceKind kind,
              SRecoveredSkinResourceCatalog* catalog,
              std::set<std::string>* names,
              SRecoveredSkinResourceCatalogResult* result) {
  if (!Is(tokens, index, TOKEN_IDENTIFIER, "LoadSkin") ||
      !Is(tokens, index + 1, TOKEN_MARK, "(") ||
      index + 7 >= tokens.size() ||
      tokens[index + 2].kind != TOKEN_IDENTIFIER ||
      !Is(tokens, index + 3, TOKEN_MARK, ",") ||
      tokens[index + 4].kind != TOKEN_STRING ||
      !Is(tokens, index + 5, TOKEN_MARK, ",") ||
      tokens[index + 6].kind != TOKEN_STRING ||
      !Is(tokens, index + 7, TOKEN_MARK, ")")) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "malformed LoadSkin call in main_LoadSkin");
  }
  const std::string& fileName = tokens[index + 4].text;
  std::string lowerFileName = fileName;
  std::transform(lowerFileName.begin(), lowerFileName.end(),
                 lowerFileName.begin(), [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  const char* expectedExtension =
      kind == RECOVERED_SKIN_RESOURCE_MODEL ? ".vbc" : ".txr";
  if (fileName.find("..") != std::string::npos ||
      fileName.find_first_of("\\/:") != std::string::npos ||
      lowerFileName.size() < std::strlen(expectedExtension) ||
      lowerFileName.compare(lowerFileName.size() -
                                std::strlen(expectedExtension),
                            std::strlen(expectedExtension),
                            expectedExtension) != 0) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "Skin resource path escapes the bounded Level root");
  }
  if (catalog->entryCount >= SRecoveredSkinResourceCatalog::MAX_ENTRIES) {
    return Fail(result, RECOVERED_SKIN_CATALOG_CAPACITY_EXCEEDED,
                "Skin catalog exceeds the bounded entry count");
  }
  if (!names->insert(tokens[index + 6].text).second) {
    return Fail(result, RECOVERED_SKIN_CATALOG_DUPLICATE_NAME,
                "main_LoadSkin contains a duplicate object name");
  }
  SRecoveredSkinResourceEntry* entry =
      &catalog->entries[catalog->entryCount++];
  entry->kind = kind;
  if (!CopyString(fileName, entry->fileName,
                  sizeof(entry->fileName)) ||
      !CopyString(tokens[index + 6].text, entry->objectName,
                  sizeof(entry->objectName))) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "Skin resource name exceeds the legacy event boundary");
  }
  if (kind == RECOVERED_SKIN_RESOURCE_MODEL) {
    ++catalog->modelCount;
  } else {
    ++catalog->spriteCount;
  }
  return true;
}

bool Parse(const std::string& source, SRecoveredSkinResourceCatalog* catalog,
           SRecoveredSkinResourceCatalogResult* result) {
  std::vector<Token> tokens;
  if (!Tokenize(source, &tokens, result)) return false;
  std::size_t body = tokens.size();
  for (std::size_t index = 0; index + 3 < tokens.size(); ++index) {
    if (Is(tokens, index, TOKEN_IDENTIFIER, "func") &&
        Is(tokens, index + 1, TOKEN_IDENTIFIER, "void") &&
        Is(tokens, index + 2, TOKEN_IDENTIFIER, "main_LoadSkin")) {
      for (std::size_t cursor = index + 3; cursor < tokens.size(); ++cursor) {
        if (Is(tokens, cursor, TOKEN_MARK, "{")) {
          body = cursor + 1;
          break;
        }
      }
      break;
    }
  }
  if (body == tokens.size()) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "main_LoadSkin body was not found");
  }

  int depth = 1;
  bool sawModelTable = false;
  bool sawSpriteTable = false;
  bool haveCurrentTable = false;
  ERecoveredSkinResourceKind currentKind = RECOVERED_SKIN_RESOURCE_MODEL;
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
    if (depth != 1 || tokens[index].kind != TOKEN_IDENTIFIER) continue;
    if (tokens[index].text == "s_AddClassTable") {
      ERecoveredSkinResourceKind kind = RECOVERED_SKIN_RESOURCE_MODEL;
      int capacity = 0;
      if (!ParseTableCall(tokens, index, &kind, &capacity)) {
        return Fail(result, RECOVERED_SKIN_CATALOG_TABLE_INVALID,
                    "malformed Skin class-table declaration");
      }
      if ((kind == RECOVERED_SKIN_RESOURCE_MODEL && sawModelTable) ||
          (kind == RECOVERED_SKIN_RESOURCE_SPRITE && sawSpriteTable)) {
        return Fail(result, RECOVERED_SKIN_CATALOG_TABLE_INVALID,
                    "duplicate Skin class-table declaration");
      }
      if (kind == RECOVERED_SKIN_RESOURCE_MODEL) {
        sawModelTable = true;
        catalog->modelCapacity = capacity;
      } else {
        sawSpriteTable = true;
        catalog->spriteCapacity = capacity;
      }
      currentKind = kind;
      haveCurrentTable = true;
    } else if (tokens[index].text == "LoadSkin") {
      if (!haveCurrentTable) {
        return Fail(result, RECOVERED_SKIN_CATALOG_TABLE_INVALID,
                    "LoadSkin precedes its class-table declaration");
      }
      if (!AddEntry(tokens, index, currentKind, catalog, &names, result)) {
        return false;
      }
    }
  }
  if (depth != 0) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "main_LoadSkin has unbalanced braces");
  }
  if (!sawModelTable || !sawSpriteTable || catalog->modelCapacity <= 0 ||
      catalog->spriteCapacity <= 0) {
    return Fail(result, RECOVERED_SKIN_CATALOG_TABLE_INVALID,
                "main_LoadSkin must declare Skin and SkinSpr tables");
  }
  if (catalog->modelCount > catalog->modelCapacity ||
      catalog->spriteCount > catalog->spriteCapacity) {
    return Fail(result, RECOVERED_SKIN_CATALOG_CAPACITY_EXCEEDED,
                "Skin resource roster exceeds its declared table capacity");
  }
  return true;
}

bool HashAsset(const std::string& path, SRecoveredSkinResourceEntry* entry,
               SRecoveredSkinResourceCatalogResult* result) {
  FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    char error[256] = {};
    std::snprintf(error, sizeof(error), "missing Skin resource %.180s",
                  entry->fileName);
    return Fail(result, RECOVERED_SKIN_CATALOG_ASSET_UNAVAILABLE, error);
  }
  unsigned long long size = 0;
  unsigned long long hash = kHashOffset;
  unsigned char buffer[8192];
  for (;;) {
    const std::size_t read = std::fread(buffer, 1, sizeof(buffer), file);
    if (read > 0) {
      size += read;
      if (size > kMaximumAssetBytes) {
        std::fclose(file);
        return Fail(result, RECOVERED_SKIN_CATALOG_ASSET_TOO_LARGE,
                    "Skin resource exceeds the bounded asset size");
      }
      HashBytes(&hash, buffer, read);
    }
    if (read < sizeof(buffer)) {
      if (std::ferror(file) != 0) {
        std::fclose(file);
        return Fail(result, RECOVERED_SKIN_CATALOG_ASSET_UNAVAILABLE,
                    "could not read a complete Skin resource");
      }
      break;
    }
  }
  const bool closed = std::fclose(file) == 0;
  if (!closed || size == 0) {
    return Fail(result, RECOVERED_SKIN_CATALOG_ASSET_UNAVAILABLE,
                "Skin resource is empty or could not be closed");
  }
  entry->byteSize = size;
  entry->fingerprint = hash;
  return true;
}

void FinishFingerprint(const std::string& source,
                       SRecoveredSkinResourceCatalog* catalog) {
  catalog->sourceFingerprint = kHashOffset;
  HashBytes(&catalog->sourceFingerprint, source.data(), source.size());
  unsigned long long hash = kHashOffset;
  HashBytes(&hash, &catalog->modelCapacity, sizeof(catalog->modelCapacity));
  HashBytes(&hash, &catalog->spriteCapacity, sizeof(catalog->spriteCapacity));
  HashBytes(&hash, &catalog->sourceFingerprint,
            sizeof(catalog->sourceFingerprint));
  for (int index = 0; index < catalog->entryCount; ++index) {
    const SRecoveredSkinResourceEntry& entry = catalog->entries[index];
    const int kind = static_cast<int>(entry.kind);
    HashBytes(&hash, &kind, sizeof(kind));
    HashString(&hash, entry.fileName);
    HashString(&hash, entry.objectName);
    HashBytes(&hash, &entry.byteSize, sizeof(entry.byteSize));
    HashBytes(&hash, &entry.fingerprint, sizeof(entry.fingerprint));
  }
  catalog->fingerprint = hash;
}

}  // namespace

bool RecoveredSkinResourceCatalog_Load(
    const char* levelDirectory, SRecoveredSkinResourceCatalog* catalog,
    SRecoveredSkinResourceCatalogResult* result) {
  Reset(catalog, result);
  if (levelDirectory == nullptr || levelDirectory[0] == 0 ||
      catalog == nullptr || result == nullptr) {
    return Fail(result, RECOVERED_SKIN_CATALOG_INVALID_ARGUMENT,
                "Skin catalog received invalid input");
  }
  std::string source;
  if (!ReadSource(JoinPath(levelDirectory, "SCINC\\SKIN.SCI"), &source,
                  result) ||
      !Parse(source, catalog, result)) {
    return false;
  }
  for (int index = 0; index < catalog->entryCount; ++index) {
    if (!HashAsset(JoinPath(levelDirectory, catalog->entries[index].fileName),
                   &catalog->entries[index], result)) {
      return false;
    }
  }
  FinishFingerprint(source, catalog);
  return true;
}

bool RecoveredSkinResourceCatalog_IsKnown(
    const SRecoveredSkinResourceCatalog* catalog) {
  if (catalog == nullptr) return false;
  // The first nine values cover the canonical May retail Levels in order.
  // The last value is the empty public lifecycle fixture used by CI.
  static const unsigned long long known[] = {
      1413472398250490146ull,
      11859523755204284989ull,
      10036339473467796241ull,
      11348318312366732924ull,
      10639645034542172860ull,
      7391849982136336596ull,
      250404433711419488ull,
      4365639098974117513ull,
      12242754344231949391ull,
      16730492880719981010ull};
  for (unsigned long long fingerprint : known) {
    if (catalog->fingerprint == fingerprint) return true;
  }
  return false;
}
