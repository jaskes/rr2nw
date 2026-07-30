#ifndef RR2NW_SIMULATION_RANDOM_H
#define RR2NW_SIMULATION_RANDOM_H

#include <cstdint>
#include <vector>

enum : std::uint32_t {
  RR2NW_SIMULATION_RANDOM_MSVC_LCG15 = 1u
};

void SimulationRandom_Reset(std::uint32_t seed = 1u);
int SimulationRandom_Next();
std::uint32_t SimulationRandom_Algorithm();
std::uint64_t SimulationRandom_DrawCount();

bool SimulationRandom_Capture(std::vector<std::uint8_t>* bytes);
bool SimulationRandom_Validate(std::uint32_t algorithm,
                               const std::vector<std::uint8_t>& bytes);
bool SimulationRandom_Apply(std::uint32_t algorithm,
                            const std::vector<std::uint8_t>& bytes);
bool SimulationRandom_Matches(std::uint32_t algorithm,
                              const std::vector<std::uint8_t>& bytes);
bool SimulationRandom_Decode(const std::vector<std::uint8_t>& bytes,
                             std::uint32_t* state,
                             std::uint64_t* drawCount);

#endif
