#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ELevelSaveSlotError : std::uint32_t {
  None = 0,
  InvalidArgument,
  InvalidSlot,
  InvalidMetadata,
  LimitExceeded,
  InvalidMagic,
  UnsupportedFormat,
  IntegrityMismatch,
  InvalidContinuation,
  MetadataMismatch,
  IoDirectoryFailed,
  IoOpenFailed,
  IoWriteFailed,
  IoFlushFailed,
  IoReadFailed,
  AtomicCommitFailed
};

struct SLevelSaveSlotStatus {
  ELevelSaveSlotError error = ELevelSaveSlotError::None;
  std::size_t offset = 0;
  std::string detail;
};

struct SLevelSaveSlot {
  std::uint32_t slot = 0;
  std::uint64_t savedAtUnixSeconds = 0;
  std::string title;
  std::string description;
  std::string level;
  std::uint64_t contentFingerprint = 0;
  std::uint64_t worldFingerprint = 0;
  std::uint64_t continuationFingerprint = 0;
  std::uint64_t simulationTick = 0;
  double simulationTime = 0.0;
  std::vector<std::uint8_t> previewPng;
  std::vector<std::uint8_t> continuation;
  std::uint64_t archiveFingerprint = 0;
};

struct SLevelSaveSlotSummary {
  bool ready = false;
  std::uint32_t slot = 0;
  std::uint64_t savedAtUnixSeconds = 0;
  std::string title;
  std::string description;
  std::string level;
  std::uint64_t contentFingerprint = 0;
  std::uint64_t worldFingerprint = 0;
  std::uint64_t continuationFingerprint = 0;
  std::uint64_t archiveFingerprint = 0;
  std::uint64_t simulationTick = 0;
  double simulationTime = 0.0;
  std::size_t previewBytes = 0;
  std::size_t continuationBytes = 0;
  std::size_t archiveBytes = 0;
};

std::uint32_t LevelSaveSlot_FormatVersion();
std::uint32_t LevelSaveSlot_Count();
std::wstring LevelSaveSlot_Path(const std::wstring& directory,
                                std::uint32_t slot);

bool LevelSaveSlot_Create(
    std::uint32_t slot, std::uint64_t savedAtUnixSeconds,
    const std::string& title, const std::string& description,
    const std::vector<std::uint8_t>& previewPng,
    const std::vector<std::uint8_t>& continuation,
    SLevelSaveSlot* archive, SLevelSaveSlotStatus* status);
bool LevelSaveSlot_Encode(const SLevelSaveSlot& archive,
                          std::vector<std::uint8_t>* bytes,
                          SLevelSaveSlotStatus* status);
bool LevelSaveSlot_Decode(const std::vector<std::uint8_t>& bytes,
                          SLevelSaveSlot* archive,
                          SLevelSaveSlotStatus* status);
bool LevelSaveSlot_Summarize(const SLevelSaveSlot& archive,
                             SLevelSaveSlotSummary* summary,
                             SLevelSaveSlotStatus* status);

bool LevelSaveSlot_WriteAtomic(const std::wstring& directory,
                               const SLevelSaveSlot& archive,
                               SLevelSaveSlotStatus* status);
bool LevelSaveSlot_Read(const std::wstring& directory, std::uint32_t slot,
                        SLevelSaveSlot* archive,
                        SLevelSaveSlotStatus* status);
