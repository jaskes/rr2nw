#include <cstdlib>

#include "SimulationRandom.h"

// Keep the legacy interpreter source intact while routing its RNDI/RNDF
// opcodes into the authoritative simulation stream instead of the visual CRT
// rand() stream.
#define rand SimulationRandom_Next
#include "cinter.c"
#undef rand
