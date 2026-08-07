#include "RecoveredSaveSlotCatalog.h"

#include "RecoveredSavePreview.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <utility>

namespace {

constexpr std::size_t kPaletteBytes = 256u * 3u;
constexpr std::size_t kQuantizedColorCount = 32u * 32u * 32u;
constexpr std::uint64_t kHashOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kHashPrime = UINT64_C(1099511628211);

bool Fail(SRecoveredSaveSlotCatalogSnapshot* snapshot,
          std::string* failure, const char* detail) {
  if (snapshot != nullptr) *snapshot = {};
  if (failure != nullptr) *failure = detail;
  return false;
}

bool LevelIdentityMatches(const std::string& left,
                          const std::string& right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(),
                    [](char first, char second) {
                      return std::tolower(
                                 static_cast<unsigned char>(first)) ==
                             std::tolower(
                                 static_cast<unsigned char>(second));
                    });
}

std::uint64_t Fingerprint(const std::uint8_t* bytes, std::size_t count) {
  std::uint64_t hash = kHashOffset;
  for (std::size_t index = 0; index < count; ++index) {
    hash ^= bytes[index];
    hash *= kHashPrime;
  }
  return hash;
}

std::array<std::uint8_t, kQuantizedColorCount> BuildPaletteLookup(
    const std::uint8_t* palette) {
  std::array<std::uint8_t, kQuantizedColorCount> lookup = {};
  for (std::size_t red = 0; red < 32u; ++red) {
    for (std::size_t green = 0; green < 32u; ++green) {
      for (std::size_t blue = 0; blue < 32u; ++blue) {
        const int sourceRed = static_cast<int>(red * 8u + 4u);
        const int sourceGreen = static_cast<int>(green * 8u + 4u);
        const int sourceBlue = static_cast<int>(blue * 8u + 4u);
        unsigned long bestDistance =
            (std::numeric_limits<unsigned long>::max)();
        std::uint8_t best = 0u;
        for (std::size_t index = 0; index < 256u; ++index) {
          const int deltaRed =
              sourceRed - static_cast<int>(palette[index * 3u]);
          const int deltaGreen =
              sourceGreen - static_cast<int>(palette[index * 3u + 1u]);
          const int deltaBlue =
              sourceBlue - static_cast<int>(palette[index * 3u + 2u]);
          const unsigned long distance = static_cast<unsigned long>(
              deltaRed * deltaRed + deltaGreen * deltaGreen +
              deltaBlue * deltaBlue);
          if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<std::uint8_t>(index);
          }
        }
        lookup[(red << 10u) | (green << 5u) | blue] = best;
      }
    }
  }
  return lookup;
}

void ConvertPreview(
    const SRecoveredSavePreviewImage& image,
    const std::array<std::uint8_t, kQuantizedColorCount>& lookup,
    SRecoveredSaveSlotCatalogEntry* entry) {
  entry->previewWidth = image.width;
  entry->previewHeight = image.height;
  entry->previewFingerprint = image.sourceFingerprint;
  entry->previewIndices.resize(
      static_cast<std::size_t>(image.width) * image.height);
  for (std::uint32_t y = 0; y < image.height; ++y) {
    const std::uint8_t* source =
        image.bgra.data() + static_cast<std::size_t>(y) * image.stride;
    std::uint8_t* target = entry->previewIndices.data() +
        static_cast<std::size_t>(y) * image.width;
    for (std::uint32_t x = 0; x < image.width; ++x) {
      const std::size_t offset = static_cast<std::size_t>(x) * 4u;
      const std::size_t key =
          (static_cast<std::size_t>(source[offset + 2u] >> 3u) << 10u) |
          (static_cast<std::size_t>(source[offset + 1u] >> 3u) << 5u) |
          static_cast<std::size_t>(source[offset] >> 3u);
      target[x] = lookup[key];
    }
  }
  entry->previewReady = !entry->previewIndices.empty();
}

}  // namespace

bool RecoveredSaveSlotCatalog_Build(
    const std::wstring& directory, const std::string& currentLevel,
    std::uint64_t currentContentFingerprint,
    const std::uint8_t* palette, std::size_t paletteBytes,
    std::uint32_t previewWidth, std::uint32_t previewHeight,
    std::uint64_t generation,
    SRecoveredSaveSlotCatalogSnapshot* snapshot,
    std::string* failure) {
  if (failure != nullptr) failure->clear();
  if (snapshot == nullptr || directory.empty() || currentLevel.empty() ||
      palette == nullptr || paletteBytes != kPaletteBytes ||
      previewWidth == 0u || previewHeight == 0u || generation == 0u ||
      LevelSaveSlot_Count() != 8u) {
    return Fail(snapshot, failure,
                "save-slot catalog arguments are invalid");
  }
  *snapshot = {};

  const HRESULT comResult =
      CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool comOwned = SUCCEEDED(comResult);
  const bool comAvailable = comOwned || comResult == RPC_E_CHANGED_MODE;
  const std::array<std::uint8_t, kQuantizedColorCount> lookup =
      BuildPaletteLookup(palette);

  SRecoveredSaveSlotCatalogSnapshot candidate;
  candidate.generation = generation;
  candidate.paletteFingerprint = Fingerprint(palette, paletteBytes);
  for (std::uint32_t slot = 0; slot < LevelSaveSlot_Count(); ++slot) {
    SRecoveredSaveSlotCatalogEntry& entry = candidate.entry[slot];
    entry.slot = slot;
    const std::wstring path = LevelSaveSlot_Path(directory, slot);
    entry.occupied = !path.empty() &&
        GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (!entry.occupied) {
      ++candidate.emptySlots;
      entry.detail = "Empty";
      continue;
    }

    SLevelSaveSlot archive;
    SLevelSaveSlotStatus status;
    if (!LevelSaveSlot_Read(directory, slot, &archive, &status) ||
        !LevelSaveSlot_Summarize(archive, &entry.summary, &status)) {
      entry.state = RECOVERED_SAVE_SLOT_CATALOG_CORRUPT;
      entry.detail = status.detail.empty()
                         ? "Corrupt or unsupported"
                         : status.detail;
      ++candidate.corruptSlots;
      continue;
    }

    entry.readable = true;
    entry.switchesLevel =
        !LevelIdentityMatches(archive.level, currentLevel);
    entry.loadable = entry.switchesLevel ||
        archive.contentFingerprint == currentContentFingerprint;
    entry.state = entry.loadable
                      ? RECOVERED_SAVE_SLOT_CATALOG_READY
                      : RECOVERED_SAVE_SLOT_CATALOG_INCOMPATIBLE;
    entry.detail = entry.loadable
                       ? (entry.switchesLevel ? "Switch Level" : "Ready")
                       : "Incompatible retail/mod content";
    candidate.archiveBytesRead += entry.summary.archiveBytes;
    if (entry.loadable)
      ++candidate.readySlots;
    else
      ++candidate.incompatibleSlots;

    if (archive.previewPng.empty()) {
      entry.previewMissing = true;
      ++candidate.previewMissingSlots;
      continue;
    }
    SRecoveredSavePreviewImage decoded;
    std::string previewFailure;
    if (!comAvailable ||
        !RecoveredSavePreview_DecodePng(
            archive.previewPng, previewWidth, previewHeight,
            &decoded, &previewFailure)) {
      entry.previewDecodeFailed = true;
      ++candidate.previewDecodeFailures;
      if (!previewFailure.empty()) entry.detail += "; " + previewFailure;
      continue;
    }
    ConvertPreview(decoded, lookup, &entry);
    if (entry.previewReady)
      ++candidate.previewReadySlots;
    else {
      entry.previewDecodeFailed = true;
      ++candidate.previewDecodeFailures;
    }
  }
  if (comOwned) CoUninitialize();
  candidate.ready = true;
  *snapshot = std::move(candidate);
  return true;
}
