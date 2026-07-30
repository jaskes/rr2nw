#include "SimulationRandom.h"

#include <cstddef>

namespace {

const std::size_t kStateBytes = 12;
std::uint32_t g_state = 1u;
std::uint64_t g_drawCount = 0u;

void PutU32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes->push_back(static_cast<std::uint8_t>(value >> shift));
}

void PutU64(std::vector<std::uint8_t>* bytes, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8)
    bytes->push_back(static_cast<std::uint8_t>(value >> shift));
}

bool GetU32(const std::vector<std::uint8_t>& bytes, std::size_t* offset,
            std::uint32_t* value) {
  if (offset == nullptr || value == nullptr || *offset > bytes.size() ||
      bytes.size() - *offset < 4)
    return false;
  *value = 0;
  for (int shift = 0; shift < 32; shift += 8)
    *value |= static_cast<std::uint32_t>(bytes[(*offset)++]) << shift;
  return true;
}

bool GetU64(const std::vector<std::uint8_t>& bytes, std::size_t* offset,
            std::uint64_t* value) {
  if (offset == nullptr || value == nullptr || *offset > bytes.size() ||
      bytes.size() - *offset < 8)
    return false;
  *value = 0;
  for (int shift = 0; shift < 64; shift += 8)
    *value |= static_cast<std::uint64_t>(bytes[(*offset)++]) << shift;
  return true;
}

}  // namespace

void SimulationRandom_Reset(std::uint32_t seed) {
  g_state = seed;
  g_drawCount = 0u;
}

int SimulationRandom_Next() {
  // This is the 15-bit MSVC CRT sequence used by the current Windows port,
  // made explicit so rendering calls to rand() can no longer perturb gameplay.
  g_state = g_state * 214013u + 2531011u;
  ++g_drawCount;
  return static_cast<int>((g_state >> 16) & 0x7fffu);
}

std::uint32_t SimulationRandom_Algorithm() {
  return RR2NW_SIMULATION_RANDOM_MSVC_LCG15;
}

std::uint64_t SimulationRandom_DrawCount() { return g_drawCount; }

bool SimulationRandom_Capture(std::vector<std::uint8_t>* bytes) {
  if (bytes == nullptr) return false;
  bytes->clear();
  bytes->reserve(kStateBytes);
  PutU32(bytes, g_state);
  PutU64(bytes, g_drawCount);
  return true;
}

bool SimulationRandom_Decode(const std::vector<std::uint8_t>& bytes,
                             std::uint32_t* state,
                             std::uint64_t* drawCount) {
  if (state == nullptr || drawCount == nullptr ||
      bytes.size() != kStateBytes)
    return false;
  std::size_t offset = 0;
  return GetU32(bytes, &offset, state) &&
         GetU64(bytes, &offset, drawCount) && offset == bytes.size();
}

bool SimulationRandom_Validate(std::uint32_t algorithm,
                               const std::vector<std::uint8_t>& bytes) {
  std::uint32_t state = 0;
  std::uint64_t drawCount = 0;
  return algorithm == SimulationRandom_Algorithm() &&
         SimulationRandom_Decode(bytes, &state, &drawCount);
}

bool SimulationRandom_Apply(std::uint32_t algorithm,
                            const std::vector<std::uint8_t>& bytes) {
  std::uint32_t state = 0;
  std::uint64_t drawCount = 0;
  if (algorithm != SimulationRandom_Algorithm() ||
      !SimulationRandom_Decode(bytes, &state, &drawCount))
    return false;
  g_state = state;
  g_drawCount = drawCount;
  return true;
}

bool SimulationRandom_Matches(std::uint32_t algorithm,
                              const std::vector<std::uint8_t>& bytes) {
  std::vector<std::uint8_t> current;
  return algorithm == SimulationRandom_Algorithm() &&
         SimulationRandom_Capture(&current) && current == bytes;
}
