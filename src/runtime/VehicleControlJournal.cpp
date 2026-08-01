#include "VehicleControlJournal.h"

#include "ClockActiveWorldState.h"
#include "SimulationRandom.h"
#include "TimeRuntimeState.h"
#include "hardware.h"
#include "message/hardmsg.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

const std::uint32_t kMagic = 0x314a5443u;  // CTJ1
const std::uint32_t kLegacyVersion = 1u;
const std::uint32_t kVersion = 2u;
const std::size_t kLegacyHeldActionCount = 11u;
const std::size_t kMaximumTargetBytes = 255u;
const std::size_t kMaximumCheckpointBytes = 4096u;
const std::size_t kMaximumRecords = 1000000u;
const std::uint64_t kHashOffset = 14695981039346656037ull;
const std::uint64_t kHashPrime = 1099511628211ull;

const int kHeldActions[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {
    MOVE_FORWARD, MOVE_BACKWARD, STRAFE_LEFT, STRAFE_RIGHT,
    STRAFE_UP, STRAFE_DOWN, TURN_LEFT, TURN_RIGHT,
    LOOK_UP, LOOK_DOWN, FIRE_PRIMARY, FIRE_SECONDARY};

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
  void Double(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    U64(bits);
  }
  void Bytes(const std::uint8_t* source, std::size_t count) {
    bytes->insert(bytes->end(), source, source + count);
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
  bool Double(double* value) {
    std::uint64_t bits = 0;
    if (value == nullptr || !U64(&bits)) return false;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
  }
  bool Bytes(std::size_t count, const std::uint8_t** source) {
    if (source == nullptr || offset > bytes.size() ||
        bytes.size() - offset < count)
      return false;
    *source = bytes.data() + offset;
    offset += count;
    return true;
  }
};

bool FiniteControlValue(double value) {
  return std::isfinite(value) && value >= -1.0 && value <= 1.0;
}

bool ValidRecord(const SVehicleControlJournalRecord& record) {
  if (!std::isfinite(record.eventTime) || record.eventTime < 0.0 ||
      !FiniteControlValue(record.value))
    return false;
  if (record.kind == VEHICLE_CONTROL_JOURNAL_ACTION)
    return record.origin == VEHICLE_CONTROL_JOURNAL_NORMALIZED_INPUT &&
           VehicleControlJournal_IsRecordableAction(record.action);
  return record.kind == VEHICLE_CONTROL_JOURNAL_FOCUS &&
         record.origin == VEHICLE_CONTROL_JOURNAL_APPLICATION_FOCUS &&
         record.action == 0 &&
         (record.value == 0.0 || record.value == 1.0);
}

bool Append(SVehicleControlJournal* journal,
            SVehicleControlJournalRecord record) {
  if (journal == nullptr || journal->sealed ||
      journal->records.size() >= kMaximumRecords || !ValidRecord(record))
    return false;
  if (record.tick < journal->checkpointTick ||
      record.eventTime < journal->checkpointTime ||
      record.tick < journal->finalTick ||
      record.eventTime < journal->finalTime)
    return false;
  if (!journal->records.empty()) {
    const SVehicleControlJournalRecord& previous = journal->records.back();
    if (record.tick < previous.tick ||
        record.eventTime < previous.eventTime)
      return false;
  }
  record.sequence = static_cast<std::uint32_t>(journal->records.size());
  journal->records.push_back(record);
  journal->finalTick = record.tick;
  journal->finalTime = record.eventTime;
  return true;
}

void HashBytes(std::uint64_t* hash, const std::uint8_t* bytes,
               std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

}  // namespace

int VehicleControlJournal_HeldAction(std::size_t index) {
  return index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT
             ? kHeldActions[index]
             : -1;
}

int VehicleControlJournal_HeldActionIndex(int action) {
  for (std::size_t index = 0;
       index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index) {
    if (kHeldActions[index] == action) return static_cast<int>(index);
  }
  return -1;
}

bool VehicleControlJournal_IsRecordableAction(int action) {
  switch (action) {
    case MOVE_FORWARD:
    case MOVE_BACKWARD:
    case STRAFE_LEFT:
    case STRAFE_RIGHT:
    case STRAFE_UP:
    case STRAFE_DOWN:
    case LOOK_UP:
    case LOOK_DOWN:
    case TURN_LEFT:
    case TURN_RIGHT:
    case FIRE_PRIMARY:
    case FIRE_SECONDARY:
    case JUMP:
    case STOP_VEHICLE:
    case CHANGE_VEHICLE:
      return true;
    default:
      return false;
  }
}

bool VehicleControlJournal_Begin(
    const char* target, bool applicationActive,
    const double heldActions[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT],
    SVehicleControlJournal* journal) {
  if (target == nullptr || heldActions == nullptr || journal == nullptr)
    return false;
  const std::size_t targetLength = std::strlen(target);
  if (targetLength == 0u || targetLength > kMaximumTargetBytes)
    return false;
  SSimulationClockState clock;
  SVehicleControlJournal candidate;
  candidate.target.assign(target, targetLength);
  candidate.initialApplicationActive = applicationActive;
  for (std::size_t index = 0;
       index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index) {
    if (!FiniteControlValue(heldActions[index])) return false;
    candidate.initialHeldActions[index] = heldActions[index];
  }
  if (!SUA_CaptureSimulationClock(&clock) ||
      !ClockActiveWorldState_CaptureStable(&candidate.clockCheckpoint) ||
      !SimulationRandom_Capture(&candidate.randomCheckpoint))
    return false;
  candidate.checkpointTick = clock.tick;
  candidate.checkpointTime = (std::max)(clock.eventMoment, clock.viewTime);
  candidate.randomAlgorithm = SimulationRandom_Algorithm();
  candidate.finalTick = candidate.checkpointTick;
  candidate.finalTime = candidate.checkpointTime;
  if (!VehicleControlJournal_Validate(candidate)) return false;
  *journal = candidate;
  return true;
}

bool VehicleControlJournal_AppendAction(
    SVehicleControlJournal* journal, std::uint64_t tick,
    double eventTime, int action, double value) {
  SVehicleControlJournalRecord record;
  record.kind = VEHICLE_CONTROL_JOURNAL_ACTION;
  record.origin = VEHICLE_CONTROL_JOURNAL_NORMALIZED_INPUT;
  record.tick = tick;
  record.action = action;
  record.eventTime = eventTime;
  record.value = value;
  return Append(journal, record);
}

bool VehicleControlJournal_AppendFocus(
    SVehicleControlJournal* journal, std::uint64_t tick,
    double eventTime, bool active) {
  SVehicleControlJournalRecord record;
  record.kind = VEHICLE_CONTROL_JOURNAL_FOCUS;
  record.origin = VEHICLE_CONTROL_JOURNAL_APPLICATION_FOCUS;
  record.tick = tick;
  record.action = 0;
  record.eventTime = eventTime;
  record.value = active ? 1.0 : 0.0;
  return Append(journal, record);
}

bool VehicleControlJournal_Seal(
    SVehicleControlJournal* journal, std::uint64_t finalTick,
    double finalTime) {
  if (journal == nullptr || journal->sealed ||
      !std::isfinite(finalTime) ||
      finalTick < journal->checkpointTick ||
      finalTime < journal->checkpointTime)
    return false;
  if (!journal->records.empty() &&
      (finalTick < journal->records.back().tick ||
       finalTime < journal->records.back().eventTime))
    return false;
  journal->finalTick = finalTick;
  journal->finalTime = finalTime;
  journal->sealed = true;
  return VehicleControlJournal_Validate(*journal);
}

bool VehicleControlJournal_DeriveLifecycle(
    const SVehicleControlJournal& journal, bool* applicationActive,
    double heldActions[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT]) {
  if (applicationActive == nullptr || heldActions == nullptr ||
      !VehicleControlJournal_Validate(journal))
    return false;
  bool active = journal.initialApplicationActive;
  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  for (std::size_t index = 0;
       index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index)
    held[index] = journal.initialHeldActions[index];
  for (const SVehicleControlJournalRecord& record : journal.records) {
    if (record.kind == VEHICLE_CONTROL_JOURNAL_ACTION) {
      if (!active) return false;
      const int heldIndex =
          VehicleControlJournal_HeldActionIndex(record.action);
      if (heldIndex >= 0) held[heldIndex] = record.value;
      continue;
    }
    const bool nextActive = record.value != 0.0;
    if (nextActive == active) return false;
    if (!nextActive)
      for (double& value : held) value = 0.0;
    active = nextActive;
  }
  *applicationActive = active;
  for (std::size_t index = 0;
       index < VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT; ++index)
    heldActions[index] = held[index];
  return true;
}

bool VehicleControlJournal_Resume(SVehicleControlJournal* journal) {
  bool active = false;
  double held[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  if (journal == nullptr || !journal->sealed ||
      !VehicleControlJournal_DeriveLifecycle(*journal, &active, held))
    return false;
  (void)active;
  journal->sealed = false;
  return VehicleControlJournal_Validate(*journal);
}

bool VehicleControlJournal_Validate(
    const SVehicleControlJournal& journal) {
  if (journal.target.empty() ||
      journal.target.size() > kMaximumTargetBytes ||
      journal.clockCheckpoint.empty() ||
      journal.clockCheckpoint.size() > kMaximumCheckpointBytes ||
      journal.randomCheckpoint.empty() ||
      journal.randomCheckpoint.size() > kMaximumCheckpointBytes ||
      journal.records.size() > kMaximumRecords ||
      !std::isfinite(journal.checkpointTime) ||
      journal.checkpointTime < 0.0 ||
      !ClockActiveWorldState_ValidateStable(journal.clockCheckpoint) ||
      !ClockActiveWorldState_MetadataMatches(
          journal.clockCheckpoint, journal.checkpointTick,
          journal.checkpointTime) ||
      !SimulationRandom_Validate(journal.randomAlgorithm,
                                 journal.randomCheckpoint))
    return false;
  for (double held : journal.initialHeldActions) {
    if (!FiniteControlValue(held)) return false;
  }
  std::uint64_t previousTick = journal.checkpointTick;
  double previousTime = journal.checkpointTime;
  for (std::size_t index = 0; index < journal.records.size(); ++index) {
    const SVehicleControlJournalRecord& record = journal.records[index];
    if (!ValidRecord(record) || record.sequence != index ||
        record.tick < previousTick || record.eventTime < previousTime)
      return false;
    previousTick = record.tick;
    previousTime = record.eventTime;
  }
  if (!std::isfinite(journal.finalTime)) return false;
  return journal.finalTick >= previousTick &&
         journal.finalTime >= previousTime;
}

bool VehicleControlJournal_Statistics(
    const SVehicleControlJournal& journal,
    SVehicleControlJournalStatistics* statistics) {
  if (statistics == nullptr || !VehicleControlJournal_Validate(journal))
    return false;
  *statistics = {};
  statistics->firstTick = journal.records.empty()
                              ? journal.checkpointTick
                              : journal.records.front().tick;
  statistics->lastTick = journal.records.empty()
                             ? journal.checkpointTick
                             : journal.records.back().tick;
  for (const SVehicleControlJournalRecord& record : journal.records) {
    if (record.kind == VEHICLE_CONTROL_JOURNAL_ACTION)
      ++statistics->actionRecords;
    else
      ++statistics->focusRecords;
  }
  return true;
}

bool VehicleControlJournal_Encode(
    const SVehicleControlJournal& journal,
    std::vector<std::uint8_t>* bytes) {
  if (bytes == nullptr || !VehicleControlJournal_Validate(journal))
    return false;
  bytes->clear();
  Writer writer = {bytes};
  writer.U32(kMagic);
  writer.U32(kVersion);
  writer.U32(static_cast<std::uint32_t>(journal.target.size()));
  writer.Bytes(reinterpret_cast<const std::uint8_t*>(journal.target.data()),
               journal.target.size());
  writer.U64(journal.checkpointTick);
  writer.Double(journal.checkpointTime);
  writer.U32(journal.initialApplicationActive ? 1u : 0u);
  for (double held : journal.initialHeldActions) writer.Double(held);
  writer.U32(static_cast<std::uint32_t>(journal.clockCheckpoint.size()));
  writer.Bytes(journal.clockCheckpoint.data(), journal.clockCheckpoint.size());
  writer.U32(journal.randomAlgorithm);
  writer.U32(static_cast<std::uint32_t>(journal.randomCheckpoint.size()));
  writer.Bytes(journal.randomCheckpoint.data(),
               journal.randomCheckpoint.size());
  writer.U32(journal.sealed ? 1u : 0u);
  writer.U64(journal.finalTick);
  writer.Double(journal.finalTime);
  writer.U32(static_cast<std::uint32_t>(journal.records.size()));
  for (const SVehicleControlJournalRecord& record : journal.records) {
    writer.U32(record.kind);
    writer.U32(record.origin);
    writer.U64(record.tick);
    writer.U32(record.sequence);
    writer.U32(static_cast<std::uint32_t>(record.action));
    writer.Double(record.eventTime);
    writer.Double(record.value);
  }
  return true;
}

bool VehicleControlJournal_Decode(
    const std::vector<std::uint8_t>& bytes,
    SVehicleControlJournal* journal) {
  if (journal == nullptr) return false;
  Reader reader = {bytes};
  SVehicleControlJournal candidate;
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t size = 0;
  std::uint32_t active = 0;
  const std::uint8_t* source = nullptr;
  if (!reader.U32(&magic) || !reader.U32(&version) ||
      magic != kMagic ||
      (version != kLegacyVersion && version != kVersion) ||
      !reader.U32(&size) ||
      size == 0u || size > kMaximumTargetBytes ||
      !reader.Bytes(size, &source))
    return false;
  candidate.target.assign(reinterpret_cast<const char*>(source), size);
  if (!reader.U64(&candidate.checkpointTick) ||
      !reader.Double(&candidate.checkpointTime) ||
      !reader.U32(&active) || active > 1u)
    return false;
  candidate.initialApplicationActive = active != 0u;
  const std::size_t heldActionCount =
      version == kLegacyVersion ? kLegacyHeldActionCount
                                : VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT;
  for (std::size_t index = 0; index < heldActionCount; ++index)
    if (!reader.Double(&candidate.initialHeldActions[index])) return false;
  if (!reader.U32(&size) || size == 0u ||
      size > kMaximumCheckpointBytes || !reader.Bytes(size, &source))
    return false;
  candidate.clockCheckpoint.assign(source, source + size);
  if (!reader.U32(&candidate.randomAlgorithm) || !reader.U32(&size) ||
      size == 0u || size > kMaximumCheckpointBytes ||
      !reader.Bytes(size, &source))
    return false;
  candidate.randomCheckpoint.assign(source, source + size);
  std::uint32_t sealed = 0;
  std::uint32_t recordCount = 0;
  if (!reader.U32(&sealed) || sealed > 1u ||
      !reader.U64(&candidate.finalTick) ||
      !reader.Double(&candidate.finalTime) ||
      !reader.U32(&recordCount) || recordCount > kMaximumRecords)
    return false;
  candidate.sealed = sealed != 0u;
  candidate.records.reserve(recordCount);
  for (std::uint32_t index = 0; index < recordCount; ++index) {
    SVehicleControlJournalRecord record;
    std::uint32_t action = 0;
    if (!reader.U32(&record.kind) || !reader.U32(&record.origin) ||
        !reader.U64(&record.tick) || !reader.U32(&record.sequence) ||
        !reader.U32(&action) || !reader.Double(&record.eventTime) ||
        !reader.Double(&record.value))
      return false;
    record.action = static_cast<int>(action);
    candidate.records.push_back(record);
  }
  if (reader.offset != bytes.size() ||
      !VehicleControlJournal_Validate(candidate))
    return false;
  *journal = candidate;
  return true;
}

bool VehicleControlJournal_ApplyCheckpoint(
    const SVehicleControlJournal& journal) {
  if (!VehicleControlJournal_Validate(journal)) return false;
  std::vector<std::uint8_t> clockBefore;
  std::vector<std::uint8_t> randomBefore;
  if (!ClockActiveWorldState_CaptureStable(&clockBefore) ||
      !SimulationRandom_Capture(&randomBefore))
    return false;
  if (ClockActiveWorldState_ApplyStableReferences(journal.clockCheckpoint) &&
      SimulationRandom_Apply(journal.randomAlgorithm,
                             journal.randomCheckpoint) &&
      ClockActiveWorldState_MatchesStable(journal.clockCheckpoint) &&
      SimulationRandom_Matches(journal.randomAlgorithm,
                               journal.randomCheckpoint))
    return true;
  ClockActiveWorldState_ApplyStableReferences(clockBefore);
  SimulationRandom_Apply(SimulationRandom_Algorithm(), randomBefore);
  return false;
}

std::uint64_t VehicleControlJournal_Fingerprint(
    const SVehicleControlJournal& journal) {
  std::vector<std::uint8_t> bytes;
  if (!VehicleControlJournal_Encode(journal, &bytes)) return 0;
  std::uint64_t hash = kHashOffset;
  HashBytes(&hash, bytes.data(), bytes.size());
  return hash;
}
