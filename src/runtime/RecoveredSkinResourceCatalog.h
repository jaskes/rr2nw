#pragma once

#include <cstddef>
#include <string>

enum ERecoveredSkinResourceKind {
  RECOVERED_SKIN_RESOURCE_MODEL = 0,
  RECOVERED_SKIN_RESOURCE_SPRITE = 1
};

enum ERecoveredSkinResourceCatalogIssue {
  RECOVERED_SKIN_CATALOG_INVALID_ARGUMENT = 1u << 0,
  RECOVERED_SKIN_CATALOG_SOURCE_UNAVAILABLE = 1u << 1,
  RECOVERED_SKIN_CATALOG_SOURCE_TOO_LARGE = 1u << 2,
  RECOVERED_SKIN_CATALOG_PARSE_FAILURE = 1u << 3,
  RECOVERED_SKIN_CATALOG_TABLE_INVALID = 1u << 4,
  RECOVERED_SKIN_CATALOG_DUPLICATE_NAME = 1u << 5,
  RECOVERED_SKIN_CATALOG_CAPACITY_EXCEEDED = 1u << 6,
  RECOVERED_SKIN_CATALOG_ASSET_UNAVAILABLE = 1u << 7,
  RECOVERED_SKIN_CATALOG_ASSET_TOO_LARGE = 1u << 8
};

struct SRecoveredSkinResourceEntry {
  ERecoveredSkinResourceKind kind;
  char fileName[100];
  char objectName[100];
  unsigned long long byteSize;
  unsigned long long fingerprint;
};

struct SRecoveredSkinResourceCatalog {
  enum { MAX_ENTRIES = 96 };

  int modelCapacity;
  int spriteCapacity;
  int modelCount;
  int spriteCount;
  int entryCount;
  unsigned long long sourceFingerprint;
  unsigned long long fingerprint;
  SRecoveredSkinResourceEntry entries[MAX_ENTRIES];
};

struct SRecoveredSkinResourceCatalogResult {
  unsigned int issues;
  char error[256];
};

bool RecoveredSkinResourceCatalog_Load(
    const char* levelDirectory, SRecoveredSkinResourceCatalog* catalog,
    SRecoveredSkinResourceCatalogResult* result);
bool RecoveredSkinResourceCatalog_IsKnown(
    const SRecoveredSkinResourceCatalog* catalog);
bool RecoveredSkinResourceCatalog_BuildAnimationProgram(
    const char* levelDirectory, std::string* program, int* entryCallCount,
    unsigned long long* fingerprint,
    SRecoveredSkinResourceCatalogResult* result);
