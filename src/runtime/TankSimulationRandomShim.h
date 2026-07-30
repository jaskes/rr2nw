#ifndef RR2NW_TANK_SIMULATION_RANDOM_SHIM_H
#define RR2NW_TANK_SIMULATION_RANDOM_SHIM_H

#include <cstdlib>

#include "SimulationRandom.h"

// TANK.CPP remains in its original non-UTF-8 source encoding. The forced
// include is scoped to its legacy target and redirects its spawn jitter into
// the same authoritative stream used by SimulationContext and the script VM.
#define rand SimulationRandom_Next

#endif
