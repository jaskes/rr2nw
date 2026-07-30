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
  int corruptionRejects;
  int rollbacks;
  std::size_t containerBytes;
  std::uint64_t worldFingerprint;

  SActiveWorldRuntimeProbeSummary();
};

bool ActiveWorldRuntime_CaptureProbe(
    SimulationContext* context, std::uint64_t contentFingerprint,
    std::uint64_t simulationTick, double simulationTime,
    const std::string& level, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure);

bool ActiveWorldRuntime_RestoreProbe(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary, std::string* failure);

#endif
