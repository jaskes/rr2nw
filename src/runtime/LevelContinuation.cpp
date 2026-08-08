#include "LevelContinuation.h"

#include "ActiveWorldRuntimeProbe.h"
#include "ActiveWorldSave.h"
#include "MissionActiveWorldState.h"

#include "kernel/h/context.h"
#include "obase/howitzer/HowitzerActiveWorldState.h"
#include "obase/people/PeopleActiveWorldState.h"

namespace {

const std::uint32_t kMagic = 0x314e434cu;  // LCN1
const std::uint32_t kVersion = 1u;
const std::size_t kMaximumPartBytes = 64u * 1024u * 1024u;
const std::uint64_t kHashOffset = 14695981039346656037ull;
const std::uint64_t kHashPrime = 1099511628211ull;

void SetFailure(std::string* failure, const std::string& message) {
  if (failure != nullptr) *failure = message;
}

void HashBytes(std::uint64_t* hash, const std::uint8_t* bytes,
               std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

struct Writer {
  std::vector<std::uint8_t>* bytes;

  void U32(std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
      bytes->push_back(static_cast<std::uint8_t>(value >> shift));
  }
  void U64(std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8)
      bytes->push_back(static_cast<std::uint8_t>(value >> shift));
  }
  void Bytes(const std::vector<std::uint8_t>& value) {
    bytes->insert(bytes->end(), value.begin(), value.end());
  }
};

struct Reader {
  const std::vector<std::uint8_t>& bytes;
  std::size_t offset = 0;

  bool U32(std::uint32_t* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < 4u)
      return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
      *value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    return true;
  }
  bool U64(std::uint64_t* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < 8u)
      return false;
    *value = 0;
    for (int shift = 0; shift < 64; shift += 8)
      *value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    return true;
  }
  bool Bytes(std::size_t count, std::vector<std::uint8_t>* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < count)
      return false;
    value->assign(bytes.begin() + offset, bytes.begin() + offset + count);
    offset += count;
    return true;
  }
};

bool DecodeParts(const SLevelContinuation& continuation,
                 SActiveWorldSnapshot* world,
                 SVehicleControlJournalStatistics* statistics) {
  if (world == nullptr || statistics == nullptr ||
      continuation.activeWorld.empty() ||
      !VehicleControlJournal_Validate(continuation.controlJournal) ||
      !continuation.controlJournal.sealed)
    return false;
  SActiveWorldSaveStatus status;
  if (!ActiveWorldSave_Decode(continuation.activeWorld, world, &status) ||
      !VehicleControlJournal_Statistics(continuation.controlJournal,
                                        statistics))
    return false;
  return continuation.controlJournal.finalTick == world->simulationTick &&
         continuation.controlJournal.finalTime == world->simulationTime &&
         continuation.controlJournal.randomAlgorithm == world->rngAlgorithm;
}

void PublishSummary(const SLevelContinuation& continuation,
                    const SActiveWorldSnapshot& world,
                    const SVehicleControlJournalStatistics& statistics,
                    std::size_t containerBytes,
                    SLevelContinuationSummary* summary) {
  std::vector<std::uint8_t> journalBytes;
  VehicleControlJournal_Encode(continuation.controlJournal, &journalBytes);
  summary->sealedJournal = continuation.controlJournal.sealed;
  summary->boundaryMatches =
      continuation.controlJournal.finalTick == world.simulationTick &&
      continuation.controlJournal.finalTime == world.simulationTime;
  summary->sections = static_cast<int>(world.sections.size());
  summary->events = static_cast<int>(world.events.size());
  summary->actionRecords = statistics.actionRecords;
  summary->focusRecords = statistics.focusRecords;
  summary->activeWorldBytes = continuation.activeWorld.size();
  summary->controlJournalBytes = journalBytes.size();
  summary->containerBytes = containerBytes;
  summary->simulationTick = world.simulationTick;
  summary->simulationTime = world.simulationTime;
  summary->worldFingerprint = world.worldFingerprint;
  summary->journalFingerprint =
      VehicleControlJournal_Fingerprint(continuation.controlJournal);
  summary->containerFingerprint = continuation.fingerprint;
}

bool NormalizeCompatibleSectionMigrations(
    SimulationContext* context, const SActiveWorldSnapshot& source,
    const SActiveWorldSnapshot& restored,
    std::uint64_t* normalizedFingerprint) {
  if (context == nullptr || normalizedFingerprint == nullptr ||
      source.sections.size() != restored.sections.size())
    return false;
  SActiveWorldSnapshot normalized = restored;
  bool peopleMigrated = false;
  bool missionMigrated = false;
  bool howitzerMigrated = false;
  for (std::size_t index = 0; index < source.sections.size(); ++index) {
    const SActiveWorldSection& expected = source.sections[index];
    SActiveWorldSection& actual = normalized.sections[index];
    if (expected.kind != actual.kind ||
        expected.schemaVersion != actual.schemaVersion ||
        expected.owner != actual.owner)
      return false;
    if (expected.payload == actual.payload) continue;
    if (expected.kind == EActiveWorldSectionKind::People &&
        !peopleMigrated &&
        PeopleActiveWorldState_MatchesStable(context, expected.payload)) {
      // Older PEO1 payloads omit state admitted by newer versions (v1 has no
      // route-geometry identity; v1/v2 have no explicit previous route node).
      peopleMigrated = true;
    } else if (expected.kind == EActiveWorldSectionKind::Mission &&
               !missionMigrated &&
               MissionActiveWorldState_MatchesStable(
                   context, expected.payload)) {
      // MSH1 v1 identifies a mission Route only by its symbolic name. MSH1 v2
      // additionally pins the route geometry reconstructed from retail/mod
      // data. A semantically verified v1 save retains its original outer
      // fingerprint after that one compatible migration.
      missionMigrated = true;
    } else if (expected.kind == EActiveWorldSectionKind::Howitzer &&
               !howitzerMigrated &&
               HowitzerActiveWorldState_MatchesStable(
                   context, expected.payload)) {
      // HWZ1 v1 represents destination-self scheduler sources by a symbolic
      // object name.  Duplicate authored names make that relation ambiguous,
      // so HWZ1 v2 stores it explicitly.  The owner verifier has proved the
      // restored v2 graph is semantically identical to the legacy payload.
      howitzerMigrated = true;
    } else {
      return false;
    }
    // The live graph has passed the matching legacy semantic verifier;
    // substituting only that payload lets the outer LCN1 fingerprint continue
    // to prove every other boundary remained identical.
    actual.payload = expected.payload;
  }
  if (!peopleMigrated && !missionMigrated && !howitzerMigrated) return false;
  *normalizedFingerprint =
      ActiveWorldSave_ComputeWorldFingerprint(normalized);
  return true;
}

}  // namespace

bool LevelContinuation_Encode(
    const SLevelContinuation& continuation,
    std::vector<std::uint8_t>* bytes) {
  if (bytes == nullptr || continuation.activeWorld.empty() ||
      continuation.activeWorld.size() > kMaximumPartBytes ||
      !VehicleControlJournal_Validate(continuation.controlJournal) ||
      !continuation.controlJournal.sealed)
    return false;
  SActiveWorldSnapshot world;
  SVehicleControlJournalStatistics statistics;
  if (!DecodeParts(continuation, &world, &statistics)) return false;
  std::vector<std::uint8_t> journalBytes;
  if (!VehicleControlJournal_Encode(continuation.controlJournal,
                                    &journalBytes) ||
      journalBytes.empty() || journalBytes.size() > kMaximumPartBytes)
    return false;

  bytes->clear();
  Writer writer = {bytes};
  writer.U32(kMagic);
  writer.U32(kVersion);
  writer.U32(static_cast<std::uint32_t>(continuation.activeWorld.size()));
  writer.U32(static_cast<std::uint32_t>(journalBytes.size()));
  writer.Bytes(continuation.activeWorld);
  writer.Bytes(journalBytes);
  std::uint64_t fingerprint = kHashOffset;
  HashBytes(&fingerprint, bytes->data(), bytes->size());
  writer.U64(fingerprint);
  return true;
}

bool LevelContinuation_Decode(
    const std::vector<std::uint8_t>& bytes,
    SLevelContinuation* continuation) {
  if (continuation == nullptr) return false;
  Reader reader = {bytes};
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t activeWorldSize = 0;
  std::uint32_t journalSize = 0;
  SLevelContinuation candidate;
  std::vector<std::uint8_t> journalBytes;
  if (!reader.U32(&magic) || !reader.U32(&version) || magic != kMagic ||
      version != kVersion || !reader.U32(&activeWorldSize) ||
      !reader.U32(&journalSize) || activeWorldSize == 0u ||
      journalSize == 0u || activeWorldSize > kMaximumPartBytes ||
      journalSize > kMaximumPartBytes ||
      !reader.Bytes(activeWorldSize, &candidate.activeWorld) ||
      !reader.Bytes(journalSize, &journalBytes))
    return false;
  const std::size_t fingerprintOffset = reader.offset;
  std::uint64_t storedFingerprint = 0;
  if (!reader.U64(&storedFingerprint) || reader.offset != bytes.size())
    return false;
  std::uint64_t computedFingerprint = kHashOffset;
  HashBytes(&computedFingerprint, bytes.data(), fingerprintOffset);
  if (storedFingerprint == 0 || storedFingerprint != computedFingerprint ||
      !VehicleControlJournal_Decode(journalBytes,
                                    &candidate.controlJournal))
    return false;
  candidate.fingerprint = computedFingerprint;
  SActiveWorldSnapshot world;
  SVehicleControlJournalStatistics statistics;
  if (!DecodeParts(candidate, &world, &statistics)) return false;
  *continuation = candidate;
  return true;
}

bool LevelContinuation_Capture(
    SimulationContext* context, std::uint64_t contentFingerprint,
    const std::string& level,
    const SVehicleControlJournal& liveJournal,
    std::vector<std::uint8_t>* bytes,
    SLevelContinuationSummary* summary, std::string* failure) {
  if (context == nullptr || bytes == nullptr || summary == nullptr ||
      level.empty() || !VehicleControlJournal_Validate(liveJournal)) {
    SetFailure(failure, "level continuation capture arguments are invalid");
    return false;
  }
  *summary = {};
  SLevelContinuation continuation;
  SActiveWorldRuntimeProbeSummary worldSummary;
  if (!ActiveWorldRuntime_Capture(context, contentFingerprint, level,
                                  &continuation.activeWorld, &worldSummary,
                                  failure))
    return false;
  SActiveWorldSnapshot world;
  SActiveWorldSaveStatus status;
  if (!ActiveWorldSave_Decode(continuation.activeWorld, &world, &status)) {
    SetFailure(failure, status.detail);
    return false;
  }
  continuation.controlJournal = liveJournal;
  if ((!continuation.controlJournal.sealed &&
       !VehicleControlJournal_Seal(&continuation.controlJournal,
                                   world.simulationTick,
                                   world.simulationTime)) ||
      (continuation.controlJournal.sealed &&
       (continuation.controlJournal.finalTick != world.simulationTick ||
        continuation.controlJournal.finalTime != world.simulationTime))) {
    SetFailure(failure,
               "CTJ1 cannot seal at the active-world continuation boundary");
    return false;
  }
  if (!LevelContinuation_Encode(continuation, bytes)) {
    SetFailure(failure, "LCN1 encoding failed");
    return false;
  }
  SLevelContinuation decoded;
  if (!LevelContinuation_Decode(*bytes, &decoded)) {
    SetFailure(failure, "LCN1 canonical round trip failed");
    return false;
  }
  SVehicleControlJournalStatistics statistics;
  if (!DecodeParts(decoded, &world, &statistics)) {
    SetFailure(failure, "LCN1 decoded boundary is invalid");
    return false;
  }
  PublishSummary(decoded, world, statistics, bytes->size(), summary);
  summary->worldMatches = true;
  summary->restoredWorldFingerprint = world.worldFingerprint;
  summary->ready = summary->boundaryMatches &&
                   summary->worldFingerprint != 0 &&
                   summary->journalFingerprint != 0 &&
                   summary->containerFingerprint != 0;
  return summary->ready;
}

bool LevelContinuation_RestoreWorld(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    std::uint64_t expectedContentFingerprint,
    const std::string& expectedLevel,
    SVehicleControlJournal* restoredJournal,
    SLevelContinuationSummary* summary, std::string* failure) {
  if (context == nullptr || expectedContentFingerprint == 0 ||
      expectedLevel.empty() || restoredJournal == nullptr ||
      summary == nullptr) {
    SetFailure(failure, "level continuation restore arguments are invalid");
    return false;
  }
  *summary = {};
  SLevelContinuation continuation;
  if (!LevelContinuation_Decode(bytes, &continuation)) {
    SetFailure(failure, "LCN1 decoding failed");
    return false;
  }
  SActiveWorldSnapshot sourceWorld;
  SVehicleControlJournalStatistics statistics;
  if (!DecodeParts(continuation, &sourceWorld, &statistics) ||
      sourceWorld.contentFingerprint != expectedContentFingerprint ||
      sourceWorld.level != expectedLevel ||
      !context->isExist(continuation.controlJournal.target.c_str())) {
    SetFailure(failure,
               "LCN1 content, Level, or symbolic target is incompatible");
    return false;
  }
  SActiveWorldRuntimeProbeSummary restoreSummary;
  if (!ActiveWorldRuntime_Restore(context, continuation.activeWorld,
                                  &restoreSummary, failure)) {
    if (failure != nullptr)
      *failure = "LCN1 world transaction failed: " + *failure;
    return false;
  }

  std::vector<std::uint8_t> recapturedBytes;
  SActiveWorldRuntimeProbeSummary recapturedSummary;
  if (!ActiveWorldRuntime_Capture(
          context, sourceWorld.contentFingerprint, sourceWorld.level,
          &recapturedBytes, &recapturedSummary, failure)) {
    if (failure != nullptr)
      *failure = "LCN1 post-restore recapture failed: " + *failure;
    return false;
  }
  SActiveWorldSnapshot restoredWorld;
  SActiveWorldSaveStatus status;
  if (!ActiveWorldSave_Decode(recapturedBytes, &restoredWorld, &status)) {
    SetFailure(failure, status.detail);
    return false;
  }

  PublishSummary(continuation, sourceWorld, statistics, bytes.size(), summary);
  summary->ownerPhases = restoreSummary.ownerPhases;
  summary->referencePhases = restoreSummary.referencePhases;
  summary->eventPhases = restoreSummary.eventPhases;
  std::uint64_t verifiedWorldFingerprint = restoredWorld.worldFingerprint;
  if (verifiedWorldFingerprint != sourceWorld.worldFingerprint) {
    std::uint64_t normalizedFingerprint = 0;
    if (NormalizeCompatibleSectionMigrations(
            context, sourceWorld, restoredWorld, &normalizedFingerprint))
      verifiedWorldFingerprint = normalizedFingerprint;
  }
  summary->restoredWorldFingerprint = verifiedWorldFingerprint;
  summary->worldMatches = verifiedWorldFingerprint ==
                          sourceWorld.worldFingerprint;
  summary->ready = restoreSummary.ready && summary->boundaryMatches &&
                   summary->worldMatches &&
                   summary->ownerPhases == kActiveWorldOwnerSectionCount &&
                   summary->referencePhases ==
                       kActiveWorldOwnerSectionCount &&
                   summary->eventPhases == summary->events;
  if (!summary->ready) {
    SetFailure(failure, "fresh Level recapture diverged from LCN1");
    return false;
  }
  *restoredJournal = continuation.controlJournal;
  return true;
}
