#include "ClockActiveWorldState.h"

#include "TimeRuntimeState.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace {

const std::uint32_t kClockMagic = 0x314b4c43u;  // CLK1
const std::uint32_t kClockVersion = 1u;

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
};

struct Reader {
  const std::vector<std::uint8_t>& bytes;
  std::size_t offset;

  explicit Reader(const std::vector<std::uint8_t>& source)
      : bytes(source), offset(0) {}
  bool U32(std::uint32_t* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < 4)
      return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
      *value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    return true;
  }
  bool U64(std::uint64_t* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < 8)
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
};

bool Encode(const SSimulationClockState& state,
            std::vector<std::uint8_t>* bytes) {
  if (bytes == nullptr || !SUA_ValidateSimulationClock(state)) return false;
  bytes->clear();
  Writer writer = {bytes};
  writer.U32(kClockMagic);
  writer.U32(kClockVersion);
  writer.U64(state.tick);
  writer.Double(state.eventMoment);
  writer.Double(state.viewTime);
  writer.Double(state.frameSeconds);
  writer.Double(state.timerAspect);
  writer.U32(state.clampedSamples);
  writer.Double(state.clampedSeconds);
  return true;
}

bool Decode(const std::vector<std::uint8_t>& bytes,
            SSimulationClockState* state) {
  if (state == nullptr) return false;
  Reader reader(bytes);
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t clampedSamples = 0;
  if (!reader.U32(&magic) || !reader.U32(&version) ||
      magic != kClockMagic || version != kClockVersion ||
      !reader.U64(&state->tick) ||
      !reader.Double(&state->eventMoment) ||
      !reader.Double(&state->viewTime) ||
      !reader.Double(&state->frameSeconds) ||
      !reader.Double(&state->timerAspect) ||
      !reader.U32(&clampedSamples) ||
      !reader.Double(&state->clampedSeconds) ||
      reader.offset != bytes.size())
    return false;
  state->clampedSamples = clampedSamples;
  return SUA_ValidateSimulationClock(*state);
}

double CanonicalTime(const SSimulationClockState& state) {
  return (std::max)(state.eventMoment, state.viewTime);
}

}  // namespace

bool ClockActiveWorldState_CaptureStable(
    std::vector<std::uint8_t>* bytes) {
  SSimulationClockState state;
  return SUA_CaptureSimulationClock(&state) && Encode(state, bytes);
}

bool ClockActiveWorldState_ValidateStable(
    const std::vector<std::uint8_t>& bytes) {
  SSimulationClockState state;
  return Decode(bytes, &state);
}

bool ClockActiveWorldState_MatchesStable(
    const std::vector<std::uint8_t>& bytes) {
  std::vector<std::uint8_t> current;
  return ClockActiveWorldState_CaptureStable(&current) && current == bytes;
}

bool ClockActiveWorldState_CreateStableOwners(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    std::vector<KR_ObjectID>* created) {
  return context != nullptr && created != nullptr && created->empty() &&
         ClockActiveWorldState_ValidateStable(bytes);
}

bool ClockActiveWorldState_ApplyStableReferences(
    const std::vector<std::uint8_t>& bytes) {
  SSimulationClockState state;
  return Decode(bytes, &state) && SUA_ApplySimulationClock(state) &&
         ClockActiveWorldState_MatchesStable(bytes);
}

void ClockActiveWorldState_RemoveStableOwners(
    SimulationContext*, std::vector<KR_ObjectID>* created) {
  if (created != nullptr) created->clear();
}

bool ClockActiveWorldState_MetadataMatches(
    const std::vector<std::uint8_t>& bytes, std::uint64_t tick,
    double simulationTime) {
  SSimulationClockState state;
  return Decode(bytes, &state) && state.tick == tick &&
         CanonicalTime(state) == simulationTime;
}
