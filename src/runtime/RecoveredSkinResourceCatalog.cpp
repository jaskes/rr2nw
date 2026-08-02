#include "RecoveredSkinResourceCatalog.h"

#include "RecoveredModRuntime.h"

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
  std::size_t begin;
  std::size_t end;
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
  FILE* file = RecoveredModRuntime_OpenRead(path.c_str(), nullptr);
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
    token.begin = index;
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
    token.end = index;
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
  FILE* file = RecoveredModRuntime_OpenRead(path.c_str(), nullptr);
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

bool RecoveredSkinResourceCatalog_BuildAnimationProgram(
    const char* levelDirectory, std::string* program, int* entryCallCount,
    unsigned long long* fingerprint,
    SRecoveredSkinResourceCatalogResult* result) {
  static const char prefix[] = R"RR2NW_SCRIPT(
const int EDO_WRITE = 1;
const int VECTOR3D_F = 6;
const int sk_EV_PROG extern;
const int pe_EV_SETANIM extern;
const int anim_UPDATE = 0;
const int anim_MOVE = 1;
const int anim_ROTATEOX = 2;
const int anim_ROTATEOY = 3;
const int anim_ROTATEOZ = 4;
const int anim_ROTATEOXC = 5;
const int anim_ROTATEOYC = 6;
const int anim_ROTATEOZC = 7;
const int anim_ROTATE = 8;
const int anim_ROTATEC = 9;
const int anim_LOADIDENTITY = 10;
const int anim_ROTATEOX_CLIP = 11;
const int anim_ROCKOX = 12;
const int anim_ROCKOY = 13;
const int anim_ROCKOZ = 14;
const int anim_ROTATEOYOut = 15;

func int s_OpenEventData(int style) extern;
func void s_CloseEventData(int event) extern;
func void s_Descend(int event, int tag, int index) extern;
func void s_Ascend(int event) extern;
func void s_WriteInt(int event, int value) extern;
func void s_WriteFloat(int event, float value) extern;
func void s_WriteStr(int event, str value) extern;
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;

func void SetAnimateBlock(int objectID, int cachePos, int animatedSet,
                          int position, str name)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  if animatedSet=0 then s_WriteStr(event, "set0");
  else s_WriteStr(event, "set");
  s_WriteInt(event, position);
  s_WriteStr(event, name);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_PROG, objectID, cachePos);
}

func void CreateAnimSets(var int objectID, var int cachePos,
                         str skinName, int initCount, int count)
var int event;
{
  s_SearchObjectID(objectID, cachePos, skinName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, "createAniSets");
  s_WriteInt(event, initCount);
  s_WriteInt(event, count);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_PROG, objectID, cachePos);
}

func void CreateAnimSetsAuto(var int objectID, var int cachePos,
                             str skinName, int initCount, int count,
                             int programLength)
var int event;
{
  s_SearchObjectID(objectID, cachePos, skinName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, "createAniSetsAuto");
  s_WriteInt(event, initCount);
  s_WriteInt(event, count);
  s_WriteInt(event, programLength);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_PROG, objectID, cachePos);
}

func void WriteVector(int event, vector value)
{
  s_Descend(event, VECTOR3D_F, 0);
  s_WriteFloat(event, value.x);
  s_WriteFloat(event, value.y);
  s_WriteFloat(event, value.z);
  s_Ascend(event);
}

func void skin_SetAnimProg_UPDATE(int objectID, int cachePos, int block)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, block);
  s_WriteInt(event, anim_UPDATE);
  s_CloseEventData(event);
  s_SendEventNow(event, pe_EV_SETANIM, objectID, cachePos);
}

func void skin_SetAnimProg_LOADIDENTITY(int objectID, int cachePos, int block)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, block);
  s_WriteInt(event, anim_LOADIDENTITY);
  s_CloseEventData(event);
  s_SendEventNow(event, pe_EV_SETANIM, objectID, cachePos);
}

func void skin_SetAnimProg_MOVE(int objectID, int cachePos, int block,
                               vector axis, vector direction,
                               float amplitude, float speed, float phase)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, block);
  s_WriteInt(event, anim_MOVE);
  WriteVector(event, axis);
  WriteVector(event, direction);
  s_WriteFloat(event, amplitude);
  s_WriteFloat(event, speed);
  s_WriteFloat(event, phase);
  s_CloseEventData(event);
  s_SendEventNow(event, pe_EV_SETANIM, objectID, cachePos);
}

func void skin_SetAnimProgReadDataRot(int objectID, int cachePos, int block,
                                     int command, vector axis,
                                     float speed, float phase)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, block);
  s_WriteInt(event, command);
  WriteVector(event, axis);
  s_WriteFloat(event, speed);
  s_WriteFloat(event, phase);
  s_CloseEventData(event);
  s_SendEventNow(event, pe_EV_SETANIM, objectID, cachePos);
}

func void skin_SetAnimProgReadDataRotC(int objectID, int cachePos, int block,
                                      int command, vector axis,
                                      float amplitude, float speed,
                                      float phase)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, block);
  s_WriteInt(event, command);
  WriteVector(event, axis);
  s_WriteFloat(event, amplitude);
  s_WriteFloat(event, speed);
  s_WriteFloat(event, phase);
  s_CloseEventData(event);
  s_SendEventNow(event, pe_EV_SETANIM, objectID, cachePos);
}

func void skin_SetAnimProgReadDataRock(int objectID, int cachePos, int block,
                                      int command, vector axis,
                                      float amplitude, float speed,
                                      float phase, float split,
                                      float asplit, float offset)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, block);
  s_WriteInt(event, command);
  WriteVector(event, axis);
  s_WriteFloat(event, amplitude);
  s_WriteFloat(event, speed);
  s_WriteFloat(event, phase);
  s_WriteFloat(event, split);
  s_WriteFloat(event, asplit);
  s_WriteFloat(event, offset);
  s_CloseEventData(event);
  s_SendEventNow(event, pe_EV_SETANIM, objectID, cachePos);
}

func void skin_SetAnimProg_ROTATEOYOut(int o, int c, int b,
                                      vector a, float p)
{ skin_SetAnimProgReadDataRot(o,c,b,anim_ROTATEOYOut,a,0,p); }
func void skin_SetAnimProg_ROTATEOX(int o, int c, int b,
                                   vector a, float w, float p)
{ skin_SetAnimProgReadDataRot(o,c,b,anim_ROTATEOX,a,w,p); }
func void skin_SetAnimProg_ROTATEOY(int o, int c, int b,
                                   vector a, float w, float p)
{ skin_SetAnimProgReadDataRot(o,c,b,anim_ROTATEOY,a,w,p); }
func void skin_SetAnimProg_ROTATEOZ(int o, int c, int b,
                                   vector a, float w, float p)
{ skin_SetAnimProgReadDataRot(o,c,b,anim_ROTATEOZ,a,w,p); }
func void skin_SetAnimProg_ROTATEOXC(int o, int c, int b,
                                    vector a, float A, float w, float p)
{ skin_SetAnimProgReadDataRotC(o,c,b,anim_ROTATEOXC,a,A,w,p); }
func void skin_SetAnimProg_ROTATEOYC(int o, int c, int b,
                                    vector a, float A, float w, float p)
{ skin_SetAnimProgReadDataRotC(o,c,b,anim_ROTATEOYC,a,A,w,p); }
func void skin_SetAnimProg_ROTATEOZC(int o, int c, int b,
                                    vector a, float A, float w, float p)
{ skin_SetAnimProgReadDataRotC(o,c,b,anim_ROTATEOZC,a,A,w,p); }
func void skin_SetAnimProg_ROTATEOX_CLIP(int o, int c, int b,
                                       vector a, float w, float p)
{ skin_SetAnimProgReadDataRot(o,c,b,anim_ROTATEOX_CLIP,a,w,p); }
func void skin_SetAnimProg_ROCKOX(int o, int c, int b, vector a,
                                 float A, float w, float p, float lo,
                                 float hi, float ofs)
{ skin_SetAnimProgReadDataRock(o,c,b,anim_ROCKOX,a,A,w,p,lo,hi,ofs); }
func void skin_SetAnimProg_ROCKOY(int o, int c, int b, vector a,
                                 float A, float w, float p, float lo,
                                 float hi, float ofs)
{ skin_SetAnimProgReadDataRock(o,c,b,anim_ROCKOY,a,A,w,p,lo,hi,ofs); }
func void skin_SetAnimProg_ROCKOZ(int o, int c, int b, vector a,
                                 float A, float w, float p, float lo,
                                 float hi, float ofs)
{ skin_SetAnimProgReadDataRock(o,c,b,anim_ROCKOZ,a,A,w,p,lo,hi,ofs); }
)RR2NW_SCRIPT";

  if (program != nullptr) program->clear();
  if (entryCallCount != nullptr) *entryCallCount = 0;
  if (fingerprint != nullptr) *fingerprint = 0;
  if (levelDirectory == nullptr || levelDirectory[0] == 0 ||
      program == nullptr || entryCallCount == nullptr ||
      fingerprint == nullptr || result == nullptr) {
    return Fail(result, RECOVERED_SKIN_CATALOG_INVALID_ARGUMENT,
                "Skin animation builder received invalid input");
  }
  std::memset(result, 0, sizeof(*result));

  std::string source;
  std::vector<Token> tokens;
  if (!ReadSource(JoinPath(levelDirectory, "SCINC\\SKIN.SCI"), &source,
                  result) ||
      !Tokenize(source, &tokens, result)) {
    return false;
  }

  std::string commonSource;
  std::vector<Token> commonTokens;
  if (!ReadSource(JoinPath(levelDirectory, "..\\SYSF.SCI"),
                  &commonSource, result) ||
      !Tokenize(commonSource, &commonTokens, result)) {
    return false;
  }
  const auto appendSourceRange = [](std::string* destination,
                                    const std::string& rangeSource,
                                    const std::vector<Token>& rangeTokens,
                                    std::size_t first, std::size_t last) {
    destination->append(rangeSource, rangeTokens[first].begin,
                        rangeTokens[last].end - rangeTokens[first].begin);
    destination->push_back('\n');
  };
  std::string commonAnimations;
  int commonFunctionCount = 0;
  for (std::size_t index = 0; index + 3 < commonTokens.size(); ++index) {
    if (!Is(commonTokens, index, TOKEN_IDENTIFIER, "func") ||
        !Is(commonTokens, index + 1, TOKEN_IDENTIFIER, "void") ||
        commonTokens[index + 2].kind != TOKEN_IDENTIFIER ||
        commonTokens[index + 2].text.compare(
            0, 16, "CreateAnimation_") != 0) {
      continue;
    }
    std::size_t open = index + 3;
    while (open < commonTokens.size() &&
           !Is(commonTokens, open, TOKEN_MARK, "{")) {
      ++open;
    }
    if (open == commonTokens.size()) {
      return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                  "common Skin animation function has no body");
    }
    int functionDepth = 0;
    std::size_t end = open;
    for (; end < commonTokens.size(); ++end) {
      if (Is(commonTokens, end, TOKEN_MARK, "{")) ++functionDepth;
      if (Is(commonTokens, end, TOKEN_MARK, "}")) {
        --functionDepth;
        if (functionDepth == 0) break;
      }
    }
    if (end == commonTokens.size()) {
      return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                  "common Skin animation function is unbalanced");
    }
    appendSourceRange(&commonAnimations, commonSource, commonTokens, index,
                      end);
    ++commonFunctionCount;
    index = end;
  }
  if (commonFunctionCount != 0 && commonFunctionCount != 3) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "common Skin animation function roster changed");
  }

  std::string localDeclarations;
  int sourceDepth = 0;
  for (std::size_t index = 0; index < tokens.size(); ++index) {
    if (Is(tokens, index, TOKEN_MARK, "{")) {
      ++sourceDepth;
      continue;
    }
    if (Is(tokens, index, TOKEN_MARK, "}")) {
      --sourceDepth;
      continue;
    }
    if (sourceDepth != 0 ||
        !Is(tokens, index, TOKEN_IDENTIFIER, "const"))
      continue;
    std::size_t end = index + 1;
    while (end < tokens.size() && !Is(tokens, end, TOKEN_MARK, ";")) ++end;
    if (end == tokens.size()) {
      return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                  "local Skin animation constant is unterminated");
    }
    appendSourceRange(&localDeclarations, source, tokens, index, end);
    index = end;
  }

  std::string localAnimations;
  for (std::size_t index = 0; index + 3 < tokens.size(); ++index) {
    if (!Is(tokens, index, TOKEN_IDENTIFIER, "func") ||
        tokens[index + 1].kind != TOKEN_IDENTIFIER ||
        tokens[index + 2].kind != TOKEN_IDENTIFIER ||
        tokens[index + 2].text == "main_LoadSkin") {
      continue;
    }
    std::size_t open = index + 3;
    while (open < tokens.size() && !Is(tokens, open, TOKEN_MARK, "{")) {
      ++open;
    }
    if (open == tokens.size()) {
      return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                  "local Skin animation function has no body");
    }
    int functionDepth = 0;
    std::size_t end = open;
    for (; end < tokens.size(); ++end) {
      if (Is(tokens, end, TOKEN_MARK, "{")) ++functionDepth;
      if (Is(tokens, end, TOKEN_MARK, "}")) {
        --functionDepth;
        if (functionDepth == 0) break;
      }
    }
    if (end == tokens.size()) {
      return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                  "local Skin animation function is unbalanced");
    }
    appendSourceRange(&localAnimations, source, tokens, index, end);
    index = end;
  }

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
                "main_LoadSkin animation entry was not found");
  }

  std::string calls;
  int depth = 1;
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
        tokens[index].text.compare(0, 16, "CreateAnimation_") != 0) {
      continue;
    }
    bool complete = false;
    const std::size_t callStart = index;
    for (; index < tokens.size(); ++index) {
      const Token& token = tokens[index];
      if (token.kind == TOKEN_MARK && token.text == ";") {
        appendSourceRange(&calls, source, tokens, callStart, index);
        complete = true;
        break;
      }
    }
    if (!complete) {
      return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                  "unterminated animation entry call in main_LoadSkin");
    }
    ++*entryCallCount;
  }
  if (depth != 0) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "main_LoadSkin animation entry has unbalanced braces");
  }
  if (*entryCallCount != 0 && commonFunctionCount != 3) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "common Skin animation functions are incomplete");
  }

  try {
    program->reserve(sizeof(prefix) + localDeclarations.size() +
                     commonAnimations.size() +
                     localAnimations.size() + calls.size() + 128u);
    program->append(prefix);
    program->append(localDeclarations);
    program->append(commonAnimations);
    program->append(localAnimations);
    program->append("\nfunc void main()\n{\n");
    program->append(calls);
    program->append("}\n");
  } catch (...) {
    return Fail(result, RECOVERED_SKIN_CATALOG_PARSE_FAILURE,
                "could not allocate Skin animation program");
  }
  *fingerprint = kHashOffset;
  HashBytes(fingerprint, program->data(), program->size());
  return true;
}
