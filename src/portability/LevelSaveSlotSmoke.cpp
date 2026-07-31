#include "ActiveWorldSave.h"
#include "LevelContinuation.h"
#include "LevelSaveSlot.h"
#include "SimulationRandom.h"
#include "hardware.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "level-save-slot-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool BuildContinuation(std::vector<std::uint8_t>* bytes) {
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
  world.contentFingerprint = UINT64_C(0x525232534c4f5431);
  world.simulationTick = 2400u;
  world.simulationTime = 80.0;
  world.rngAlgorithm = SimulationRandom_Algorithm();
  SimulationRandom_Reset(0x534c4f54u);
  if (!SimulationRandom_Capture(&world.rngState)) return false;
  world.level = "Level.04D";
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

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2) return Fail("expected one output directory");
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

  RemoveFixture(directory);
  std::printf(
      "level save slot format=RR2SLOT1 slots=%u level=%s "
      "continuation=LCN1 bytes=%zu fingerprint=%llu atomic=replace-safe\n",
      LevelSaveSlot_Count(), replacement.level.c_str(),
      replacement.continuation.size(),
      static_cast<unsigned long long>(committedFingerprint));
  return EXIT_SUCCESS;
}
