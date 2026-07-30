#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum EVehicleControlJournalRecordKind : std::uint32_t {
  VEHICLE_CONTROL_JOURNAL_ACTION = 1u,
  VEHICLE_CONTROL_JOURNAL_FOCUS = 2u
};

enum EVehicleControlJournalOrigin : std::uint32_t {
  VEHICLE_CONTROL_JOURNAL_NORMALIZED_INPUT = 1u,
  VEHICLE_CONTROL_JOURNAL_APPLICATION_FOCUS = 2u
};

enum : std::size_t {
  VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT = 11u
};

struct SVehicleControlJournalRecord {
  std::uint32_t kind = 0;
  std::uint32_t origin = 0;
  std::uint64_t tick = 0;
  std::uint32_t sequence = 0;
  int action = 0;
  double eventTime = 0.0;
  double value = 0.0;
};

struct SVehicleControlJournal {
  std::string target;
  std::uint64_t checkpointTick = 0;
  double checkpointTime = 0.0;
  bool initialApplicationActive = true;
  double initialHeldActions[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT] = {};
  std::vector<std::uint8_t> clockCheckpoint;
  std::uint32_t randomAlgorithm = 0;
  std::vector<std::uint8_t> randomCheckpoint;
  std::vector<SVehicleControlJournalRecord> records;
  bool sealed = false;
  std::uint64_t finalTick = 0;
  double finalTime = 0.0;
};

struct SVehicleControlJournalStatistics {
  std::uint32_t actionRecords = 0;
  std::uint32_t focusRecords = 0;
  std::uint64_t firstTick = 0;
  std::uint64_t lastTick = 0;
};

int VehicleControlJournal_HeldAction(std::size_t index);
int VehicleControlJournal_HeldActionIndex(int action);
bool VehicleControlJournal_IsRecordableAction(int action);

bool VehicleControlJournal_Begin(
    const char* target, bool applicationActive,
    const double heldActions[VEHICLE_CONTROL_JOURNAL_HELD_ACTION_COUNT],
    SVehicleControlJournal* journal);
bool VehicleControlJournal_AppendAction(
    SVehicleControlJournal* journal, std::uint64_t tick,
    double eventTime, int action, double value);
bool VehicleControlJournal_AppendFocus(
    SVehicleControlJournal* journal, std::uint64_t tick,
    double eventTime, bool active);
bool VehicleControlJournal_Seal(
    SVehicleControlJournal* journal, std::uint64_t finalTick,
    double finalTime);
bool VehicleControlJournal_Validate(
    const SVehicleControlJournal& journal);
bool VehicleControlJournal_Statistics(
    const SVehicleControlJournal& journal,
    SVehicleControlJournalStatistics* statistics);
bool VehicleControlJournal_Encode(
    const SVehicleControlJournal& journal,
    std::vector<std::uint8_t>* bytes);
bool VehicleControlJournal_Decode(
    const std::vector<std::uint8_t>& bytes,
    SVehicleControlJournal* journal);
bool VehicleControlJournal_ApplyCheckpoint(
    const SVehicleControlJournal& journal);
std::uint64_t VehicleControlJournal_Fingerprint(
    const SVehicleControlJournal& journal);
