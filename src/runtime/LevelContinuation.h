#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "VehicleControlJournal.h"

class SimulationContext;

struct SLevelContinuationSummary {
  bool ready = false;
  bool sealedJournal = false;
  bool boundaryMatches = false;
  bool worldMatches = false;
  int sections = 0;
  int events = 0;
  int ownerPhases = 0;
  int referencePhases = 0;
  int eventPhases = 0;
  std::uint32_t actionRecords = 0;
  std::uint32_t focusRecords = 0;
  std::size_t activeWorldBytes = 0;
  std::size_t controlJournalBytes = 0;
  std::size_t containerBytes = 0;
  std::uint64_t simulationTick = 0;
  double simulationTime = 0.0;
  std::uint64_t worldFingerprint = 0;
  std::uint64_t restoredWorldFingerprint = 0;
  std::uint64_t journalFingerprint = 0;
  std::uint64_t containerFingerprint = 0;
};

struct SLevelContinuation {
  std::vector<std::uint8_t> activeWorld;
  SVehicleControlJournal controlJournal;
  std::uint64_t fingerprint = 0;
};

bool LevelContinuation_Encode(
    const SLevelContinuation& continuation,
    std::vector<std::uint8_t>* bytes);
bool LevelContinuation_Decode(
    const std::vector<std::uint8_t>& bytes,
    SLevelContinuation* continuation);

bool LevelContinuation_Capture(
    SimulationContext* context, std::uint64_t contentFingerprint,
    const std::string& level,
    const SVehicleControlJournal& liveJournal,
    std::vector<std::uint8_t>* bytes,
    SLevelContinuationSummary* summary, std::string* failure);

// Restores the admitted world and proves it by a non-mutating recapture. The
// decoded sealed journal is returned for the platform control owner to adopt.
// A caller that needs cross-component rollback must capture and restore a
// target-session backup, as RecoveredGameServicesRuntime does.
bool LevelContinuation_RestoreWorld(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    std::uint64_t expectedContentFingerprint,
    const std::string& expectedLevel,
    SVehicleControlJournal* restoredJournal,
    SLevelContinuationSummary* summary, std::string* failure);
