#include "LegacyImport.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int Fail(const char* detail) {
  std::cerr << "legacy-import-smoke: " << detail << '\n';
  return 1;
}

void AppendU32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (unsigned int shift = 0u; shift < 32u; shift += 8u)
    bytes->push_back(static_cast<std::uint8_t>(value >> shift));
}

void PutU32(std::vector<std::uint8_t>* bytes, std::size_t offset,
            std::uint32_t value) {
  for (unsigned int shift = 0u; shift < 32u; shift += 8u)
    (*bytes)[offset + shift / 8u] =
        static_cast<std::uint8_t>(value >> shift);
}

void PutU64(std::vector<std::uint8_t>* bytes, std::size_t offset,
            std::uint64_t value) {
  for (unsigned int shift = 0u; shift < 64u; shift += 8u)
    (*bytes)[offset + shift / 8u] =
        static_cast<std::uint8_t>(value >> shift);
}

void PutDouble(std::vector<std::uint8_t>* bytes, std::size_t offset,
               double value) {
  std::uint64_t bits = 0u;
  std::memcpy(&bits, &value, sizeof(bits));
  PutU64(bytes, offset, bits);
}

std::size_t AppendFrame(std::vector<std::uint8_t>* bytes,
                        std::size_t size, const char* text = nullptr) {
  const std::size_t prefix = bytes->size();
  AppendU32(bytes, static_cast<std::uint32_t>(size));
  bytes->resize(bytes->size() + size, 0u);
  if (text != nullptr) {
    const std::size_t length = std::strlen(text);
    if (length + 1u <= size)
      std::memcpy(bytes->data() + prefix + 4u, text, length + 1u);
  }
  return prefix;
}

std::size_t AppendPrefix(std::vector<std::uint8_t>* bytes,
                         std::int32_t type) {
  const std::size_t prefix = AppendFrame(bytes, 8u);
  std::memcpy(bytes->data() + prefix + 4u, "PIN", 3u);
  PutU32(bytes, prefix + 8u, static_cast<std::uint32_t>(type));
  return prefix;
}

void AppendObject(std::vector<std::uint8_t>* bytes, const char* table,
                  const char* symbolic, std::uint32_t id) {
  AppendPrefix(bytes, 2);
  AppendFrame(bytes, 81u, table);
  AppendFrame(bytes, 128u, symbolic);
  AppendFrame(bytes, 4u);
  AppendFrame(bytes, 4u);
  const std::size_t objectId = AppendFrame(bytes, 8u);
  PutU32(bytes, objectId + 4u, id);
}

std::vector<std::uint8_t> SaveFixture(
    std::vector<std::size_t>* boundaries = nullptr,
    unsigned int extraObjects = 0u) {
  static const std::uint32_t prelude[] = {
      19u, 8u, 8u, 32u, 8u, 8u, 4u, 13348u, 4u, 8u, 4u, 8u, 8u,
      8u, 8u, 8u, 8u, 4u, 24u, 24u, 72000u, 24000u, 4u, 5529u,
      11786u};
  std::vector<std::uint8_t> bytes;
  std::vector<std::size_t> local;
  for (std::uint32_t size : prelude)
    local.push_back(AppendFrame(&bytes, size));
  std::memcpy(bytes.data() + local[0] + 4u, "Next Worlds", 12u);
  std::memcpy(bytes.data() + local[1] + 4u, "PIN", 3u);
  PutU32(&bytes, local[1] + 8u, 0u);
  PutU32(&bytes, local[2] + 4u, 1u);
  PutDouble(&bytes, local[3] + 4u, 4.0 / 3.0);
  PutDouble(&bytes, local[3] + 12u, 1.0);
  PutDouble(&bytes, local[3] + 20u, 0.0);
  PutDouble(&bytes, local[4] + 4u, 1.0);
  PutDouble(&bytes, local[5] + 4u, 1.0);
  PutU32(&bytes, local[6] + 4u, 0u);
  local.push_back(AppendPrefix(&bytes, 1));
  local.push_back(AppendFrame(&bytes, 181u));
  local.push_back(bytes.size());
  AppendObject(&bytes, "Vehicle", "Player.Vehicle", 1u);
  local.push_back(bytes.size());
  AppendObject(&bytes, "Route", "Guide.Route", 2u);
  for (unsigned int index = 0u; index < extraObjects; ++index)
    AppendObject(&bytes, "Vehicle", "Pool.Vehicle", index + 3u);
  local.push_back(AppendPrefix(&bytes, 3));
  local.push_back(AppendFrame(&bytes, 68u));
  local.push_back(AppendPrefix(&bytes, 4));
  if (boundaries != nullptr) *boundaries = std::move(local);
  return bytes;
}

std::vector<std::uint8_t> Config(const std::string& extra = std::string()) {
  const std::string text =
      "# privacy-safe synthetic legacy settings\r\n"
      "[Setings]\r\n"
      "Bind0=Forward,W\r\n"
      "Bind1=FirePrimary,MouseL\r\n"
      "Bind2=FirePrimary,LCtrl\r\n"
      "UserBindsNum=3\r\n"
      "UseMouse=1\r\n"
      "MouseSensX=0.625\r\n"
      "MouseSensY=0.375\r\n"
      "MouseInvY=1\r\n"
      "Sound=1\r\n"
      "3DrawSound=1\r\n"
      "EngineSound=1\r\n"
      "EngineIntensity=0.750\r\n" + extra;
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

bool RejectedSaveWithoutMutation(const std::vector<std::uint8_t>& bytes) {
  SLegacySaveInspection output;
  output.profile = 0xC0DEu;
  SLegacyImportStatus status;
  return !LegacyImport_InspectSaveBytes(bytes, &output, &status) &&
      output.profile == 0xC0DEu && status.error != ELegacyImportError::None;
}

bool RejectedConfigWithoutMutation(const std::vector<std::uint8_t>& bytes) {
  SLegacyConfigImport output;
  output.profile = 0xC0DEu;
  SLegacyImportStatus status;
  return !LegacyImport_DecodeConfigBytes(bytes, &output, &status) &&
      output.profile == 0xC0DEu && status.error != ELegacyImportError::None;
}

int InspectSave(const wchar_t* path) {
  SLegacySaveInspection inspection;
  SLegacyImportStatus status;
  if (!LegacyImport_InspectSaveFile(path, &inspection, &status)) {
    std::cout << "status=invalid\nerror="
              << LegacyImport_ErrorName(status.error) << "\noffset="
              << status.offset << '\n';
    return 2;
  }
  std::cout << "status=valid\nprofile=" << inspection.profile
            << "\nstructural=" << (inspection.structurallyValid ? 1 : 0)
            << "\nconversion_ready=" << (inspection.conversionReady ? 1 : 0)
            << "\ncontent_identity="
            << (inspection.contentIdentityPresent ? 1 : 0)
            << "\nframes=" << inspection.frameCount
            << "\nobjects=" << inspection.objectCount
            << "\nevents=" << inspection.eventCount
            << "\nbranches=" << inspection.branchCount
            << "\ndeferred_owner_objects="
            << inspection.deferredOwnerObjects << '\n';
  return 0;
}

int InspectConfig(const wchar_t* path) {
  SLegacyConfigImport imported;
  SLegacyImportStatus status;
  if (!LegacyImport_ReadConfigFile(path, &imported, &status)) {
    std::cout << "status=invalid\nerror="
              << LegacyImport_ErrorName(status.error) << "\noffset="
              << status.offset << '\n';
    return 2;
  }
  std::cout << "status=valid\nprofile=" << imported.profile
            << "\nbindings=" << imported.sourceBindings
            << "\nbindings_projected="
            << (imported.bindingsProjected ? 1 : 0)
            << "\nignored_settings=" << imported.ignoredSettings
            << "\neffects_volume=" << imported.effectsVolume
            << "\nvehicle_volume=" << imported.vehicleVolume
            << "\ncinematic_volume=" << imported.cinematicVolume << '\n';
  return 0;
}

int RunSynthetic() {
  std::vector<std::size_t> boundaries;
  const std::vector<std::uint8_t> save = SaveFixture(&boundaries);
  SLegacySaveInspection inspection;
  SLegacyImportStatus status;
  if (!LegacyImport_InspectSaveBytes(save, &inspection, &status))
    return Fail("valid save fixture was rejected");
  if (!inspection.structurallyValid || inspection.conversionReady ||
      inspection.contentIdentityPresent || inspection.levelIndex != 0u ||
      inspection.eventCount != 1u || inspection.objectCount != 2u ||
      inspection.branchCount != 1u || inspection.deferredOwnerObjects != 1u)
    return Fail("save structural boundary is incorrect");

  for (std::size_t boundary : boundaries) {
    if (boundary == 0u) continue;
    std::vector<std::uint8_t> truncated(save.begin(), save.begin() + boundary);
    if (!RejectedSaveWithoutMutation(truncated))
      return Fail("frame-boundary truncation was accepted or mutated output");
  }
  std::vector<std::uint8_t> malformed = save;
  malformed.back() = 1u;
  if (!RejectedSaveWithoutMutation(malformed))
    return Fail("invalid final prefix was accepted");
  malformed = save;
  PutU64(&malformed, boundaries[3] + 12u,
         0x7ff8000000000000ull);
  if (!RejectedSaveWithoutMutation(malformed))
    return Fail("nonfinite TimerData was accepted");
  malformed = save;
  PutU32(&malformed, boundaries[6] + 4u, 9u);
  if (!RejectedSaveWithoutMutation(malformed))
    return Fail("invalid Level index was accepted");
  malformed = SaveFixture(nullptr, 5000u);
  if (!RejectedSaveWithoutMutation(malformed))
    return Fail("oversized object population was accepted");

  const std::vector<std::uint8_t> config = Config("Panel=1\r\n");
  SLegacyConfigImport imported;
  if (!LegacyImport_DecodeConfigBytes(config, &imported, &status))
    return Fail("valid config fixture was rejected");
  if (!imported.bindingsProjected || imported.sourceBindings != 3u ||
      imported.ignoredSettings != 2u ||
      std::fabs(imported.mouseSensitivityX - 0.625) > 0.000001 ||
      std::fabs(imported.mouseSensitivityY - 0.375) > 0.000001 ||
      !imported.mouseInvertY ||
      std::fabs(imported.vehicleVolume - 0.75) > 0.000001)
    return Fail("config projection differs from the proven subset");
  SLegacyConfigImport repeated;
  if (!LegacyImport_DecodeConfigBytes(config, &repeated, &status) ||
      repeated.sourceFingerprint != imported.sourceFingerprint ||
      repeated.bindingsProjected != imported.bindingsProjected)
    return Fail("config decode is not idempotent");

  std::vector<std::uint8_t> bad = Config("MouseSensX=nan\r\n");
  if (!RejectedConfigWithoutMutation(bad))
    return Fail("duplicate/nonfinite config value was accepted");
  bad = Config();
  bad.push_back(0u);
  if (!RejectedConfigWithoutMutation(bad))
    return Fail("embedded NUL was accepted");
  bad.assign(kLegacyConfigMaximumBytes + 1u, 'A');
  if (!RejectedConfigWithoutMutation(bad))
    return Fail("oversized config was accepted");
  const std::string unsupported = "[Settings]\r\nUserBindsNum=0\r\n";
  bad.assign(unsupported.begin(), unsupported.end());
  if (!RejectedConfigWithoutMutation(bad))
    return Fail("unsupported config profile was accepted");

  std::cout << "legacy_import_smoke=ok\n"
            << "save_profile=" << inspection.profile << "\n"
            << "save_conversion_ready=0\n"
            << "config_profile=" << imported.profile << "\n"
            << "config_projection=1\n";
  return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc == 3 && std::wcscmp(argv[1], L"--inspect-save") == 0)
    return InspectSave(argv[2]);
  if (argc == 3 && std::wcscmp(argv[1], L"--inspect-config") == 0)
    return InspectConfig(argv[2]);
  if (argc != 1) return Fail("usage: [--inspect-save|--inspect-config] path");
  return RunSynthetic();
}
