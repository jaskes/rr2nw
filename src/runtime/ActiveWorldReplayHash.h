#pragma once

#include <cstdint>
#include <string>
#include <vector>

class SimulationContext;

enum EActiveWorldReplayHashComponent : std::uint32_t {
  ACTIVE_WORLD_HASH_CONTENT = 1u,
  ACTIVE_WORLD_HASH_CONTROLLER = 2u,
  ACTIVE_WORLD_HASH_COMMANDER = 3u,
  ACTIVE_WORLD_HASH_TANK_GROUP = 4u,
  ACTIVE_WORLD_HASH_PEOPLE = 5u,
  ACTIVE_WORLD_HASH_TANK = 6u,
  ACTIVE_WORLD_HASH_VEHICLE = 7u,
  ACTIVE_WORLD_HASH_MISSION = 8u,
  ACTIVE_WORLD_HASH_BULLET = 9u,
  ACTIVE_WORLD_HASH_CLOCK = 10u,
  ACTIVE_WORLD_HASH_RANDOM = 11u,
  ACTIVE_WORLD_HASH_SEMANTIC_EVENTS = 12u,
  ACTIVE_WORLD_HASH_PROFILE_OR_ROSTER = 13u
};

constexpr std::uint32_t kActiveWorldReplayHashComponentCount = 12u;
constexpr std::uint32_t kActiveWorldReplayHashOwnerComponentCount = 7u;

struct SActiveWorldReplayHashComponent {
  std::uint32_t kind = 0;
  std::uint64_t fingerprint = 0;
};

struct SActiveWorldReplayHashSnapshot {
  std::uint32_t algorithm = 0;
  std::uint32_t eventCount = 0;
  std::uint64_t stateHash = 0;
  std::vector<SActiveWorldReplayHashComponent> components;
};

// Builds the version-2 active gameplay-core profile from already canonical
// component fingerprints. This pure seam keeps mismatch tests independent
// from a live retail context.
bool ActiveWorldReplayHash_Create(
    const std::vector<SActiveWorldReplayHashComponent>& components,
    std::uint32_t eventCount, SActiveWorldReplayHashSnapshot* snapshot);
bool ActiveWorldReplayHash_Validate(
    const SActiveWorldReplayHashSnapshot& snapshot);

// Captures only the proven authoritative projection. Presentation/audio/UI
// owners are absent; PEO1/TAN1/VEH1/MSH1 are normalized by their codecs.
bool ActiveWorldReplayHash_Capture(
    SimulationContext* context, std::uint64_t contentFingerprint,
    std::uint64_t controllerFingerprint,
    SActiveWorldReplayHashSnapshot* snapshot, std::string* failure);

// Returns false only when either snapshot is invalid. A valid match reports
// component zero; a valid mismatch reports the first canonical component.
bool ActiveWorldReplayHash_FirstMismatch(
    const SActiveWorldReplayHashSnapshot& expected,
    const SActiveWorldReplayHashSnapshot& actual,
    std::uint32_t* component);
const char* ActiveWorldReplayHash_ComponentName(std::uint32_t component);
