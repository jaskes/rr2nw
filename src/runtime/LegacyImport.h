#pragma once

#include "RecoveredWindowsInputAdapter.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ELegacyImportError : std::uint32_t {
  None = 0,
  FileUnavailable,
  EmptyInput,
  InputTooLarge,
  EmbeddedNul,
  TruncatedFrame,
  InvalidFrameLength,
  TooManyFrames,
  InvalidHeader,
  UnsupportedLayout,
  InvalidPrefix,
  InvalidLevel,
  NonFiniteTime,
  InvalidObjectHeader,
  MissingTerminator,
  TrailingData,
  InvalidConfig,
  UnsupportedConfig,
  AtomicCommitFailed
};

struct SLegacyImportStatus {
  ELegacyImportError error = ELegacyImportError::None;
  std::size_t offset = 0;
  std::string detail;
};

struct SLegacySaveClassCount {
  std::string table;
  std::uint32_t objects = 0;
  bool continuationOwned = false;
};

struct SLegacySaveInspection {
  std::uint32_t profile = 0;
  std::uint64_t sourceFingerprint = 0;
  std::uint64_t sourceBytes = 0;
  std::uint32_t frameCount = 0;
  std::uint32_t levelIndex = 0;
  std::uint32_t eventCount = 0;
  std::uint32_t objectCount = 0;
  std::uint32_t branchCount = 0;
  std::uint32_t deferredOwnerObjects = 0;
  bool structurallyValid = false;
  bool contentIdentityPresent = false;
  bool conversionReady = false;
  std::vector<SLegacySaveClassCount> classes;
  std::vector<std::string> deferredOwnerTables;
  std::string conversionBoundary;
};

struct SLegacyConfigImport {
  std::uint32_t profile = 0;
  std::uint64_t sourceFingerprint = 0;
  std::uint32_t sourceBindings = 0;
  std::uint32_t supportedBindingRecords = 0;
  std::uint32_t ignoredBindingRecords = 0;
  std::uint32_t unrepresentableBindingRecords = 0;
  std::uint32_t excessBindingRecords = 0;
  std::uint32_t ignoredSettings = 0;
  bool bindingsProjected = false;
  SRecoveredInputBindings bindings = {};
  double mouseSensitivityX = 0.5;
  double mouseSensitivityY = 0.5;
  bool mouseInvertY = false;
  bool soundEnabled = true;
  bool spatialSoundEnabled = true;
  bool engineSoundEnabled = true;
  double engineIntensity = 1.0;
  double effectsVolume = 1.0;
  double vehicleVolume = 1.0;
  double cinematicVolume = 1.0;
  std::string bindingBoundary;
};

constexpr std::size_t kLegacySaveMaximumBytes = 128u * 1024u * 1024u;
constexpr std::size_t kLegacyConfigMaximumBytes = 64u * 1024u;

bool LegacyImport_InspectSaveBytes(
    const std::vector<std::uint8_t>& bytes,
    SLegacySaveInspection* inspection, SLegacyImportStatus* status);
bool LegacyImport_InspectSaveFile(
    const std::wstring& path, SLegacySaveInspection* inspection,
    SLegacyImportStatus* status);

bool LegacyImport_DecodeConfigBytes(
    const std::vector<std::uint8_t>& bytes,
    SLegacyConfigImport* imported, SLegacyImportStatus* status);
bool LegacyImport_ReadConfigFile(
    const std::wstring& path, SLegacyConfigImport* imported,
    SLegacyImportStatus* status);

const char* LegacyImport_ErrorName(ELegacyImportError error);
