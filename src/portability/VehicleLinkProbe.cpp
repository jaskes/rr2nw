#include <cstdlib>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "h/vehicle.h"

int main() {
  return g_vehicle == nullptr ? EXIT_SUCCESS : EXIT_FAILURE;
}
