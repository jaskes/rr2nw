#include "ActiveWorldReplayHash.h"

#include "ActiveWorldSemanticEvents.h"
#include "ClockActiveWorldState.h"
#include "MissionActiveWorldState.h"
#include "ReplayHashJournal.h"
#include "SimulationRandom.h"
#include "obase/bullet/BulletActiveWorldState.h"
#include "obase/comander/CommanderState.h"
#include "obase/group/TankGroupState.h"
#include "obase/people/PeopleActiveWorldState.h"
#include "obase/tank/TankActiveWorldState.h"
#include "obase/vehicle/VehicleActiveWorldState.h"

#include <cstring>

namespace {

constexpr std::uint64_t kHashOffset = 14695981039346656037ull;
constexpr std::uint64_t kHashPrime = 1099511628211ull;

void HashBytes(std::uint64_t* hash, const void* source, std::size_t count) {
  const std::uint8_t* bytes = static_cast<const std::uint8_t*>(source);
  for (std::size_t index = 0; index < count; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

void HashU32(std::uint64_t* hash, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    const std::uint8_t byte = static_cast<std::uint8_t>(value >> shift);
    HashBytes(hash, &byte, 1u);
  }
}

void HashU64(std::uint64_t* hash, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    const std::uint8_t byte = static_cast<std::uint8_t>(value >> shift);
    HashBytes(hash, &byte, 1u);
  }
}

void HashDouble(std::uint64_t* hash, double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  HashU64(hash, bits);
}

void HashString(std::uint64_t* hash, const std::string& value) {
  HashU32(hash, static_cast<std::uint32_t>(value.size()));
  if (!value.empty()) HashBytes(hash, value.data(), value.size());
}

void HashPayload(std::uint64_t* hash,
                 const std::vector<std::uint8_t>& value) {
  HashU32(hash, static_cast<std::uint32_t>(value.size()));
  if (!value.empty()) HashBytes(hash, value.data(), value.size());
}

std::uint64_t HashBlob(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t hash = kHashOffset;
  HashU32(&hash, static_cast<std::uint32_t>(bytes.size()));
  if (!bytes.empty()) HashBytes(&hash, bytes.data(), bytes.size());
  return hash;
}

std::uint64_t HashEvents(const std::vector<SActiveWorldEvent>& events) {
  std::uint64_t hash = kHashOffset;
  HashU32(&hash, static_cast<std::uint32_t>(events.size()));
  for (const SActiveWorldEvent& event : events) {
    HashU32(&hash, event.sequence);
    HashU64(&hash, event.tick);
    HashDouble(&hash, event.timeStamp);
    HashU32(&hash, static_cast<std::uint32_t>(event.label));
    HashString(&hash, event.source);
    HashString(&hash, event.destination);
    HashU32(&hash, event.payloadVersion);
    HashPayload(&hash, event.payload);
  }
  return hash;
}

std::uint64_t CombinedHash(
    const std::vector<SActiveWorldReplayHashComponent>& components,
    std::uint32_t eventCount) {
  std::uint64_t hash = kHashOffset;
  HashU32(&hash, RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64);
  HashU32(&hash, static_cast<std::uint32_t>(components.size()));
  HashU32(&hash, eventCount);
  for (const SActiveWorldReplayHashComponent& component : components) {
    HashU32(&hash, component.kind);
    HashU64(&hash, component.fingerprint);
  }
  return hash;
}

void SetFailure(std::string* failure, const char* detail) {
  if (failure != nullptr) *failure = detail == nullptr ? "" : detail;
}

}  // namespace

bool ActiveWorldReplayHash_Create(
    const std::vector<SActiveWorldReplayHashComponent>& components,
    std::uint32_t eventCount, SActiveWorldReplayHashSnapshot* snapshot) {
  if (snapshot == nullptr ||
      components.size() != kActiveWorldReplayHashComponentCount)
    return false;
  for (std::size_t index = 0; index < components.size(); ++index)
    if (components[index].kind != index + 1u ||
        components[index].fingerprint == 0u)
      return false;
  SActiveWorldReplayHashSnapshot candidate;
  candidate.algorithm = RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64;
  candidate.eventCount = eventCount;
  candidate.components = components;
  candidate.stateHash = CombinedHash(candidate.components,
                                     candidate.eventCount);
  if (!ActiveWorldReplayHash_Validate(candidate)) return false;
  *snapshot = candidate;
  return true;
}

bool ActiveWorldReplayHash_Validate(
    const SActiveWorldReplayHashSnapshot& snapshot) {
  if (snapshot.algorithm !=
          RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64 ||
      snapshot.stateHash == 0u ||
      snapshot.components.size() != kActiveWorldReplayHashComponentCount ||
      snapshot.stateHash !=
          CombinedHash(snapshot.components, snapshot.eventCount))
    return false;
  for (std::size_t index = 0; index < snapshot.components.size(); ++index)
    if (snapshot.components[index].kind != index + 1u ||
        snapshot.components[index].fingerprint == 0u)
      return false;
  return true;
}

bool ActiveWorldReplayHash_Capture(
    SimulationContext* context, std::uint64_t contentFingerprint,
    std::uint64_t controllerFingerprint,
    SActiveWorldReplayHashSnapshot* snapshot, std::string* failure) {
  if (context == nullptr || contentFingerprint == 0u ||
      controllerFingerprint == 0u || snapshot == nullptr) {
    SetFailure(failure, "active-world replay hash arguments are invalid");
    return false;
  }

  std::vector<std::uint8_t> clock;
  std::vector<std::uint8_t> random;
  std::vector<SActiveWorldEvent> events;
  const std::uint64_t commander = CommanderState_Fingerprint(context);
  const std::uint64_t groups = TankGroupState_Fingerprint(context);
  const std::uint64_t people =
      PeopleActiveWorldState_AuthoritativeFingerprint(context);
  const std::uint64_t tanks =
      TankActiveWorldState_AuthoritativeFingerprint(context);
  const std::uint64_t vehicle =
      VehicleActiveWorldState_AuthoritativeFingerprint(context);
  const std::uint64_t missionFingerprint =
      MissionActiveWorldState_AuthoritativeFingerprint(context);
  const std::uint64_t bullets =
      BulletActiveWorldState_Fingerprint(context);
  std::string eventFailure;
  if (commander == 0u || groups == 0u || people == 0u || tanks == 0u ||
      vehicle == 0u || missionFingerprint == 0u || bullets == 0u ||
      !ClockActiveWorldState_CaptureStable(&clock) || clock.empty() ||
      !SimulationRandom_Capture(&random) || random.empty() ||
      !ActiveWorldSemanticEvents_Capture(context, &events, &eventFailure)) {
    SetFailure(failure, eventFailure.empty()
                            ? "active gameplay-core capture failed"
                            : eventFailure.c_str());
    return false;
  }

  std::uint64_t randomFingerprint = kHashOffset;
  HashU32(&randomFingerprint, SimulationRandom_Algorithm());
  HashPayload(&randomFingerprint, random);
  std::vector<SActiveWorldReplayHashComponent> components = {
      {ACTIVE_WORLD_HASH_CONTENT, contentFingerprint},
      {ACTIVE_WORLD_HASH_CONTROLLER, controllerFingerprint},
      {ACTIVE_WORLD_HASH_COMMANDER, commander},
      {ACTIVE_WORLD_HASH_TANK_GROUP, groups},
      {ACTIVE_WORLD_HASH_PEOPLE, people},
      {ACTIVE_WORLD_HASH_TANK, tanks},
      {ACTIVE_WORLD_HASH_VEHICLE, vehicle},
      {ACTIVE_WORLD_HASH_MISSION, missionFingerprint},
      {ACTIVE_WORLD_HASH_BULLET, bullets},
      {ACTIVE_WORLD_HASH_CLOCK, HashBlob(clock)},
      {ACTIVE_WORLD_HASH_RANDOM, randomFingerprint},
      {ACTIVE_WORLD_HASH_SEMANTIC_EVENTS, HashEvents(events)}};
  if (!ActiveWorldReplayHash_Create(
          components, static_cast<std::uint32_t>(events.size()), snapshot)) {
    SetFailure(failure, "active gameplay-core hash construction failed");
    return false;
  }
  if (failure != nullptr) failure->clear();
  return true;
}

bool ActiveWorldReplayHash_FirstMismatch(
    const SActiveWorldReplayHashSnapshot& expected,
    const SActiveWorldReplayHashSnapshot& actual,
    std::uint32_t* component) {
  if (component == nullptr)
    return false;
  if (!ActiveWorldReplayHash_Validate(expected) ||
      !ActiveWorldReplayHash_Validate(actual)) {
    *component = ACTIVE_WORLD_HASH_PROFILE_OR_ROSTER;
    return false;
  }
  *component = 0u;
  for (std::size_t index = 0; index < expected.components.size(); ++index)
    if (expected.components[index].fingerprint !=
        actual.components[index].fingerprint) {
      *component = expected.components[index].kind;
      return true;
    }
  if (expected.eventCount != actual.eventCount) {
    *component = ACTIVE_WORLD_HASH_SEMANTIC_EVENTS;
    return true;
  }
  return expected.stateHash == actual.stateHash;
}

const char* ActiveWorldReplayHash_ComponentName(std::uint32_t component) {
  switch (component) {
    case ACTIVE_WORLD_HASH_CONTENT: return "content";
    case ACTIVE_WORLD_HASH_CONTROLLER: return "controller";
    case ACTIVE_WORLD_HASH_COMMANDER: return "Commander";
    case ACTIVE_WORLD_HASH_TANK_GROUP: return "TankGroup";
    case ACTIVE_WORLD_HASH_PEOPLE: return "People";
    case ACTIVE_WORLD_HASH_TANK: return "Tank";
    case ACTIVE_WORLD_HASH_VEHICLE: return "Vehicle";
    case ACTIVE_WORLD_HASH_MISSION: return "Mission";
    case ACTIVE_WORLD_HASH_BULLET: return "Bullet";
    case ACTIVE_WORLD_HASH_CLOCK: return "Clock";
    case ACTIVE_WORLD_HASH_RANDOM: return "RNG";
    case ACTIVE_WORLD_HASH_SEMANTIC_EVENTS: return "events";
    case ACTIVE_WORLD_HASH_PROFILE_OR_ROSTER: return "profile/roster";
    default: return "none";
  }
}
