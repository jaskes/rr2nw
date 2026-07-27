#pragma once

enum ERecoveredWavMetadataCatalogIssue {
  RECOVERED_WAV_CATALOG_INVALID_ARGUMENT = 1u << 0,
  RECOVERED_WAV_CATALOG_SOURCE_UNAVAILABLE = 1u << 1,
  RECOVERED_WAV_CATALOG_SOURCE_TOO_LARGE = 1u << 2,
  RECOVERED_WAV_CATALOG_PARSE_FAILURE = 1u << 3,
  RECOVERED_WAV_CATALOG_TABLE_INVALID = 1u << 4,
  RECOVERED_WAV_CATALOG_DUPLICATE_NAME = 1u << 5,
  RECOVERED_WAV_CATALOG_CAPACITY_EXCEEDED = 1u << 6
};

struct SRecoveredWavMetadataEntry {
  char objectName[100];
  char fileName[100];
  float minFront;
  float minBack;
  float maxFront;
  float maxBack;
  float intensity;
  int flags;
};

struct SRecoveredWavMetadataCatalog {
  enum { MAX_ENTRIES = 64 };

  int capacity;
  int entryCount;
  unsigned long long localMainSourceFingerprint;
  unsigned long long loadWavSourceFingerprint;
  unsigned long long fingerprint;
  SRecoveredWavMetadataEntry entries[MAX_ENTRIES];
};

struct SRecoveredWavMetadataCatalogResult {
  unsigned int issues;
  char error[256];
};

bool RecoveredWavMetadataCatalog_Load(
    const char* levelDirectory, SRecoveredWavMetadataCatalog* catalog,
    SRecoveredWavMetadataCatalogResult* result);
bool RecoveredWavMetadataCatalog_IsKnown(
    const SRecoveredWavMetadataCatalog* catalog);
