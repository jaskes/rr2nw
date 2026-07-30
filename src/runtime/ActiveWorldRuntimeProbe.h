#ifndef RR2NW_ACTIVE_WORLD_RUNTIME_PROBE_H
#define RR2NW_ACTIVE_WORLD_RUNTIME_PROBE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class SimulationContext;

struct SActiveWorldRuntimeProbeSummary {
  bool ready;
  int sections;
  int events;
  int ownerPhases;
  int referencePhases;
  int eventPhases;
  int createdOwners;
  int missionRecords;
  int missionConditionReferences;
  int missionRouteReferences;
  int missionCheckEvents;
  int clockRecords;
  std::uint32_t rngAlgorithm;
  int rngStateBytes;
  std::uint64_t rngDrawCount;
  int corruptionRejects;
  int rollbacks;
  std::size_t containerBytes;
  std::uint64_t worldFingerprint;

  SActiveWorldRuntimeProbeSummary();
};

bool ActiveWorldRuntime_CaptureProbe(
    SimulationContext* context, std::uint64_t contentFingerprint,
    const std::string& level, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure);

// Production capture/restore omit the synthetic mission/effect fixtures and
// the deliberate rollback pass used by the admission probe. They operate on
// the exact live admitted world and leave a successful restore committed.
bool ActiveWorldRuntime_Capture(
    SimulationContext* context, std::uint64_t contentFingerprint,
    const std::string& level, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure);

bool ActiveWorldRuntime_RestoreProbe(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure);

bool ActiveWorldRuntime_Restore(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure);

#endif
