#include "ActiveWorldSave.h"
#include "IndexedPng.h"
#include "LevelContinuation.h"
#include "LevelSaveSlot.h"
#include "RecoveredSaveSlotCatalog.h"
#include "SimulationRandom.h"
#include "hardware.h"

#include <windows.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "level-save-slot-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool BuildContinuation(
    std::vector<std::uint8_t>* bytes,
    const std::string& level = "Level.04D",
    std::uint64_t contentFingerprint =
        UINT64_C(0x525232534c4f5431)) {
  if (bytes == nullptr) return false;
  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  SVehicleControlJournal journal;
  if (!VehicleControlJournal_Begin(
          "Vehicle.Default", true, held, &journal) ||
      !VehicleControlJournal_AppendAction(
          &journal, 2400u, 80.0, MOVE_FORWARD, 1.0) ||
      !VehicleControlJournal_AppendAction(
          &journal, 2400u, 80.0, MOVE_FORWARD, 0.0) ||
      !VehicleControlJournal_Seal(&journal, 2400u, 80.0))
    return false;

  SActiveWorldSnapshot world;
  world.engineCompatibility = ActiveWorldSave_EngineCompatibilityVersion();
  world.contentFingerprint = contentFingerprint;
  world.simulationTick = 2400u;
  world.simulationTime = 80.0;
  world.rngAlgorithm = SimulationRandom_Algorithm();
  SimulationRandom_Reset(0x534c4f54u);
  if (!SimulationRandom_Capture(&world.rngState)) return false;
  world.level = level;
  SActiveWorldSection commander = {};
  commander.kind = EActiveWorldSectionKind::Commander;
  commander.schemaVersion = 1u;
  commander.owner = "Commander";
  commander.payload = {0x52u, 0x52u, 0x32u};
  world.sections.push_back(commander);

  SLevelContinuation continuation;
  SActiveWorldSaveStatus status;
  if (!ActiveWorldSave_Encode(world, &continuation.activeWorld, &status))
    return false;
  continuation.controlJournal = journal;
  return LevelContinuation_Encode(continuation, bytes);
}

void RemoveFixture(const std::wstring& directory) {
  for (std::uint32_t slot = 0; slot < LevelSaveSlot_Count(); ++slot) {
    const std::wstring path = LevelSaveSlot_Path(directory, slot);
    if (!path.empty()) DeleteFileW(path.c_str());
  }
  RemoveDirectoryW(directory.c_str());
}

bool WriteCatalogSlot(
    const std::wstring& directory, std::uint32_t slot,
    const std::string& level, std::uint64_t contentFingerprint,
    const std::vector<std::uint8_t>& preview, const char* title) {
  std::vector<std::uint8_t> continuation;
  SLevelSaveSlot archive;
  SLevelSaveSlotStatus status;
  return BuildContinuation(&continuation, level, contentFingerprint) &&
         LevelSaveSlot_Create(
             slot, UINT64_C(1771000000) + slot, title,
             "Catalog fixture", preview, continuation, &archive,
             &status) &&
         LevelSaveSlot_WriteAtomic(directory, archive, &status);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2 && argc != 3)
    return Fail("expected output directory and optional --keep-catalog-fixture");
  const bool keepCatalogFixture =
      argc == 3 && std::wcscmp(argv[2], L"--keep-catalog-fixture") == 0;
  if (argc == 3 && !keepCatalogFixture)
    return Fail("unknown optional argument");
  const std::wstring directory = argv[1];
  RemoveFixture(directory);

  std::vector<std::uint8_t> continuation;
  if (!BuildContinuation(&continuation) || continuation.empty())
    return Fail("could not construct the LCN1 fixture");

  const std::vector<std::uint8_t> preview = {
      0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au, 0x31u};
  SLevelSaveSlotStatus status;
  SLevelSaveSlot first;
  if (!LevelSaveSlot_Create(
          3u, UINT64_C(1770000000), "Rail yard",
          "Vehicle.Default at the station", preview, continuation,
          &first, &status) ||
      first.archiveFingerprint == 0 ||
      first.continuationFingerprint == 0 ||
      first.worldFingerprint == 0)
    return Fail("canonical save slot creation failed");

  std::vector<std::uint8_t> encoded;
  SLevelSaveSlot decoded;
  SLevelSaveSlotSummary summary;
  if (!LevelSaveSlot_Encode(first, &encoded, &status) ||
      encoded.empty() ||
      !LevelSaveSlot_Decode(encoded, &decoded, &status) ||
      !LevelSaveSlot_Summarize(decoded, &summary, &status) ||
      !summary.ready || summary.slot != 3u ||
      summary.title != "Rail yard" ||
      summary.level != "Level.04D" ||
      summary.previewBytes != preview.size() ||
      summary.continuationBytes != continuation.size() ||
      summary.archiveBytes != encoded.size() ||
      summary.archiveFingerprint != first.archiveFingerprint)
    return Fail("save slot canonical round trip diverged");

  const std::uint64_t retainedFingerprint = decoded.archiveFingerprint;
  std::vector<std::uint8_t> corrupt = encoded;
  corrupt[corrupt.size() / 2u] ^= 0x40u;
  if (LevelSaveSlot_Decode(corrupt, &decoded, &status) ||
      status.error != ELevelSaveSlotError::IntegrityMismatch ||
      decoded.archiveFingerprint != retainedFingerprint)
    return Fail("corrupt archive was accepted or mutated its destination");
  corrupt = encoded;
  corrupt.pop_back();
  if (LevelSaveSlot_Decode(corrupt, &decoded, &status) ||
      decoded.archiveFingerprint != retainedFingerprint)
    return Fail("truncated archive was accepted or mutated its destination");

  const std::string maximumTitle(96u, 't');
  const std::string maximumDescription(1024u, 'd');
  const std::string invalidUtf8("\xc0\x80", 2u);
  if (!LevelSaveSlot_ValidateDisplayMetadata(
          maximumTitle, maximumDescription, &status) ||
      LevelSaveSlot_ValidateDisplayMetadata(
          std::string(97u, 't'), std::string(), &status) ||
      status.error != ELevelSaveSlotError::InvalidMetadata ||
      LevelSaveSlot_ValidateDisplayMetadata(
          "Title", std::string(1025u, 'd'), &status) ||
      LevelSaveSlot_ValidateDisplayMetadata(
          invalidUtf8, std::string(), &status))
    return Fail("editable display metadata bounds diverged");

  SLevelSaveSlot invalidMetadata = first;
  invalidMetadata.title.clear();
  if (LevelSaveSlot_Encode(invalidMetadata, &encoded, &status) ||
      status.error != ELevelSaveSlotError::InvalidMetadata)
    return Fail("invalid display metadata was admitted");
  invalidMetadata = first;
  invalidMetadata.worldFingerprint ^= 1u;
  if (LevelSaveSlot_Encode(invalidMetadata, &encoded, &status) ||
      status.error != ELevelSaveSlotError::MetadataMismatch)
    return Fail("metadata detached from LCN1 was admitted");
  invalidMetadata = first;
  invalidMetadata.title = "modified after canonical decode";
  if (LevelSaveSlot_Encode(invalidMetadata, &encoded, &status) ||
      status.error != ELevelSaveSlotError::IntegrityMismatch)
    return Fail("modified canonical slot retained a stale fingerprint");
  if (!LevelSaveSlot_Path(directory, 8u).empty())
    return Fail("out-of-range slot produced a path");

  if (!LevelSaveSlot_WriteAtomic(directory, first, &status) ||
      !LevelSaveSlot_Read(directory, 3u, &decoded, &status) ||
      decoded.archiveFingerprint != first.archiveFingerprint)
    return Fail("initial atomic slot commit failed");

  SLevelSaveSlot replacement;
  if (!LevelSaveSlot_Create(
          3u, UINT64_C(1770000300), "Rail yard - replacement",
          "New committed description", {}, continuation,
          &replacement, &status) ||
      !LevelSaveSlot_WriteAtomic(directory, replacement, &status) ||
      !LevelSaveSlot_Read(directory, 3u, &decoded, &status) ||
      decoded.archiveFingerprint != replacement.archiveFingerprint ||
      decoded.title != replacement.title ||
      !decoded.previewPng.empty())
    return Fail("atomic replacement did not publish one complete slot");

  const std::uint64_t committedFingerprint =
      replacement.archiveFingerprint;
  SLevelSaveSlot blockedReplacement;
  if (!LevelSaveSlot_Create(
          3u, UINT64_C(1770000600), "Blocked replacement",
          "MoveFileEx must preserve the committed target", {},
          continuation, &blockedReplacement, &status))
    return Fail("could not create the blocked replacement fixture");
  const std::wstring slotThree = LevelSaveSlot_Path(directory, 3u);
  HANDLE retainedHandle = CreateFileW(
      slotThree.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (retainedHandle == INVALID_HANDLE_VALUE)
    return Fail("could not retain the committed slot handle");
  const bool blockedCommit =
      !LevelSaveSlot_WriteAtomic(directory, blockedReplacement, &status) &&
      status.error == ELevelSaveSlotError::AtomicCommitFailed;
  CloseHandle(retainedHandle);
  if (!blockedCommit ||
      !LevelSaveSlot_Read(directory, 3u, &decoded, &status) ||
      decoded.archiveFingerprint != committedFingerprint)
    return Fail("failed atomic commit changed the retained target");

  invalidMetadata = replacement;
  invalidMetadata.description.assign(1025u, 'x');
  if (LevelSaveSlot_WriteAtomic(directory, invalidMetadata, &status) ||
      !LevelSaveSlot_Read(directory, 3u, &decoded, &status) ||
      decoded.archiveFingerprint != committedFingerprint)
    return Fail("failed replacement changed the committed slot");

  const std::wstring slotTwo = LevelSaveSlot_Path(directory, 2u);
  if (!CopyFileW(slotThree.c_str(), slotTwo.c_str(), FALSE) ||
      LevelSaveSlot_Read(directory, 2u, &decoded, &status) ||
      status.error != ELevelSaveSlotError::MetadataMismatch)
    return Fail("slot/file identity mismatch was not rejected");

  // Exercise the read-only shell catalog with all player-facing states.
  RemoveFixture(directory);
  constexpr std::uint32_t previewWidth = 176u;
  constexpr std::uint32_t previewHeight = 132u;
  std::vector<std::uint8_t> previewPixels(
      static_cast<std::size_t>(previewWidth) * previewHeight);
  std::array<std::uint8_t, 256u * 3u> palette = {};
  for (std::size_t index = 0; index < 256u; ++index) {
    palette[index * 3u] = static_cast<std::uint8_t>(index);
    palette[index * 3u + 1u] =
        static_cast<std::uint8_t>(255u - index);
    palette[index * 3u + 2u] =
        static_cast<std::uint8_t>((index * 3u) & 0xffu);
  }
  for (std::uint32_t y = 0; y < previewHeight; ++y) {
    for (std::uint32_t x = 0; x < previewWidth; ++x) {
      previewPixels[static_cast<std::size_t>(y) * previewWidth + x] =
          static_cast<std::uint8_t>((x + y * 3u) & 0xffu);
    }
  }
  std::vector<std::uint8_t> validPreview;
  SIndexedPngSummary pngSummary;
  std::string catalogFailure;
  if (!IndexedPng_Encode(
          previewWidth, previewHeight, previewPixels.data(), previewWidth,
          palette.data(), palette.size(), &validPreview, &pngSummary,
          &catalogFailure) ||
      !pngSummary.ready)
    return Fail("catalog fixture PNG could not be encoded");
  const std::vector<std::uint8_t> invalidPreview = {
      0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au, 0x31u};
  if (!WriteCatalogSlot(directory, 0u, "Level.04D",
                        UINT64_C(0x1111222233334444), {},
                        "Legacy previewless") ||
      !WriteCatalogSlot(directory, 1u, "Level.04D",
                        UINT64_C(0x1111222233334444), validPreview,
                        "Valid preview") ||
      !WriteCatalogSlot(directory, 2u, "Level.04D",
                        UINT64_C(0x1111222233334444), invalidPreview,
                        "Broken preview") ||
      !WriteCatalogSlot(directory, 3u, "Level.03N",
                        UINT64_C(0x9999aaaabbbbcccc), validPreview,
                        "Incompatible content"))
    return Fail("catalog fixture slots could not be committed");
  if (!CopyFileW(LevelSaveSlot_Path(directory, 1u).c_str(),
                 LevelSaveSlot_Path(directory, 4u).c_str(), FALSE))
    return Fail("catalog corrupt slot fixture could not be copied");

  SRecoveredSaveSlotCatalogSnapshot catalog;
  if (!RecoveredSaveSlotCatalog_Build(
          directory, "Level.03N", UINT64_C(0x1111222233334444),
          palette.data(), palette.size(), 16u, 12u, 7u, &catalog,
          &catalogFailure) ||
      !catalog.ready || catalog.generation != 7u ||
      catalog.paletteFingerprint == 0u || catalog.emptySlots != 3u ||
      catalog.readySlots != 3u || catalog.incompatibleSlots != 1u ||
      catalog.corruptSlots != 1u || catalog.previewReadySlots != 2u ||
      catalog.previewMissingSlots != 1u ||
      catalog.previewDecodeFailures != 1u ||
      catalog.archiveBytesRead == 0u ||
      catalog.entry[0].state != RECOVERED_SAVE_SLOT_CATALOG_READY ||
      !catalog.entry[0].previewMissing ||
      catalog.entry[1].state != RECOVERED_SAVE_SLOT_CATALOG_READY ||
      !catalog.entry[1].previewReady ||
      catalog.entry[1].previewWidth != 16u ||
      catalog.entry[1].previewHeight != 12u ||
      catalog.entry[1].previewIndices.size() != 16u * 12u ||
      catalog.entry[2].state != RECOVERED_SAVE_SLOT_CATALOG_READY ||
      !catalog.entry[2].previewDecodeFailed ||
      catalog.entry[3].state !=
          RECOVERED_SAVE_SLOT_CATALOG_INCOMPATIBLE ||
      catalog.entry[3].loadable ||
      catalog.entry[4].state != RECOVERED_SAVE_SLOT_CATALOG_CORRUPT ||
      catalog.entry[5].state != RECOVERED_SAVE_SLOT_CATALOG_EMPTY) {
    return Fail(catalogFailure.empty()
                    ? "save-slot catalog states diverged"
                    : catalogFailure.c_str());
  }

  if (!keepCatalogFixture) RemoveFixture(directory);
  std::printf(
      "level save slot format=RR2SLOT1 slots=%u level=%s "
      "continuation=LCN1 bytes=%zu fingerprint=%llu atomic=replace-safe "
      "catalog=3-ready/1-incompatible/1-corrupt/3-empty%s\n",
      LevelSaveSlot_Count(), replacement.level.c_str(),
      replacement.continuation.size(),
      static_cast<unsigned long long>(committedFingerprint),
      keepCatalogFixture ? " retained" : "");
  return EXIT_SUCCESS;
}
