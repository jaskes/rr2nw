#pragma once

#include "LevelSaveSlot.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum ERecoveredSaveSlotCatalogState {
  RECOVERED_SAVE_SLOT_CATALOG_EMPTY = 0,
  RECOVERED_SAVE_SLOT_CATALOG_READY = 1,
  RECOVERED_SAVE_SLOT_CATALOG_INCOMPATIBLE = 2,
  RECOVERED_SAVE_SLOT_CATALOG_CORRUPT = 3
};

struct SRecoveredSaveSlotCatalogEntry {
  ERecoveredSaveSlotCatalogState state =
      RECOVERED_SAVE_SLOT_CATALOG_EMPTY;
  std::uint32_t slot = 0;
  bool occupied = false;
  bool readable = false;
  bool loadable = false;
  bool switchesLevel = false;
  bool previewMissing = false;
  bool previewReady = false;
  bool previewDecodeFailed = false;
  std::string detail;
  SLevelSaveSlotSummary summary;
  std::uint32_t previewWidth = 0;
  std::uint32_t previewHeight = 0;
  std::uint64_t previewFingerprint = 0;
  std::vector<std::uint8_t> previewIndices;
};

struct SRecoveredSaveSlotCatalogSnapshot {
  bool ready = false;
  std::uint64_t generation = 0;
  std::uint64_t paletteFingerprint = 0;
  unsigned int emptySlots = 0;
  unsigned int readySlots = 0;
  unsigned int incompatibleSlots = 0;
  unsigned int corruptSlots = 0;
  unsigned int previewReadySlots = 0;
  unsigned int previewMissingSlots = 0;
  unsigned int previewDecodeFailures = 0;
  std::size_t archiveBytesRead = 0;
  std::array<SRecoveredSaveSlotCatalogEntry, 8u> entry = {};
};

// Reads the fixed RR2SLOT1 catalog without mutating the save directory. Each
// archive and PNG remains subject to the existing LevelSaveSlot/WIC bounds.
// Preview pixels are converted to the supplied 256-color software palette so
// the render thread can publish them without file I/O or image decoding.
bool RecoveredSaveSlotCatalog_Build(
    const std::wstring& directory, const std::string& currentLevel,
    std::uint64_t currentContentFingerprint,
    const std::uint8_t* palette, std::size_t paletteBytes,
    std::uint32_t previewWidth, std::uint32_t previewHeight,
    std::uint64_t generation,
    SRecoveredSaveSlotCatalogSnapshot* snapshot,
    std::string* failure);
