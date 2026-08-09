#pragma once

#include "VehicleControlJournal.h"

#include <cstdint>
#include <vector>

enum : std::uint32_t {
  RR2NW_REPLAY_HASH_VEHICLE_CLK1_RNG_FNV1A64 = 1u,
  RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64 = 2u
};

struct SReplayHashSample {
  std::uint64_t tick = 0;
  double simulationTime = 0.0;
  std::uint64_t stateHash = 0;
};

struct SReplayHashJournal {
  std::uint64_t contentFingerprint = 0;
  double simulationStepSeconds = 0.0;
  std::uint32_t stateHashAlgorithm = 0;
  SVehicleControlJournal controls;
  std::vector<SReplayHashSample> samples;
};

bool ReplayHashJournal_Create(
    std::uint64_t contentFingerprint, double simulationStepSeconds,
    const SVehicleControlJournal& controls,
    const std::vector<SReplayHashSample>& samples,
    SReplayHashJournal* journal);
bool ReplayHashJournal_CreateWithAlgorithm(
    std::uint64_t contentFingerprint, double simulationStepSeconds,
    std::uint32_t stateHashAlgorithm,
    const SVehicleControlJournal& controls,
    const std::vector<SReplayHashSample>& samples,
    SReplayHashJournal* journal);
bool ReplayHashJournal_Validate(const SReplayHashJournal& journal);
bool ReplayHashJournal_MatchesIdentity(
    const SReplayHashJournal& journal, std::uint64_t contentFingerprint,
    std::uint64_t controlFingerprint);
bool ReplayHashJournal_Encode(
    const SReplayHashJournal& journal,
    std::vector<std::uint8_t>* bytes);
bool ReplayHashJournal_Decode(
    const std::vector<std::uint8_t>& bytes,
    SReplayHashJournal* journal);
std::uint64_t ReplayHashJournal_Fingerprint(
    const SReplayHashJournal& journal);
