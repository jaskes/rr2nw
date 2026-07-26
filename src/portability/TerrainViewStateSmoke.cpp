#include <cstdlib>
#include <iostream>

#define LAST_H__VIEW
#include "game.h"

#include "TerrainViewState.h"

namespace {

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << "terrain-view-state-smoke: " << message << '\n';
  return false;
}

bool ExpectCell(const SVector2& cell, int x, int y, const char* message) {
  return Expect(cell.x == x && cell.y == y, message);
}

}  // namespace

int main() {
  const long rect[4] = {10, 20, 13, 23};
  const int edges[3][2] = {{20, 23}, {19, 24}, {18, 25}};

  CVector2 cell(11, 21);
  if (!Expect(!TerrainView_FitCell(
                  false, rect, edges, 0, false, false, cell),
              "hidden terrain accepted a cell")) {
    return EXIT_FAILURE;
  }

  cell = CVector2(9, 21);
  if (!Expect(!TerrainView_FitCell(
                  true, rect, edges, 0, false, false, cell),
              "forward X view accepted a cell before its near edge")) {
    return EXIT_FAILURE;
  }

  cell = CVector2(13, 100);
  if (!Expect(TerrainView_FitCell(
                  true, rect, edges, 0, false, true, cell),
              "forward X view rejected a clippable cell") ||
      !ExpectCell(cell, 12, 24,
                  "forward X view changed far/secondary clipping")) {
    return EXIT_FAILURE;
  }

  cell = CVector2(13, 21);
  if (!Expect(!TerrainView_FitCell(
                  true, rect, edges, 0, true, false, cell),
              "reverse X view accepted a cell before its near edge")) {
    return EXIT_FAILURE;
  }

  cell = CVector2(9, 100);
  if (!Expect(TerrainView_FitCell(
                  true, rect, edges, 0, true, false, cell),
              "reverse X view rejected a clippable cell") ||
      !ExpectCell(cell, 10, 22,
                  "reverse X view changed far/secondary clipping")) {
    return EXIT_FAILURE;
  }

  cell = CVector2(100, 23);
  if (!Expect(TerrainView_FitCell(
                  true, rect, edges, 1, false, false, cell),
              "forward Y view rejected a clippable cell") ||
      !ExpectCell(cell, 24, 22,
                  "forward Y view changed far/secondary clipping")) {
    return EXIT_FAILURE;
  }

  cell = CVector2(100, 19);
  if (!Expect(TerrainView_FitCell(
                  true, rect, edges, 1, true, true, cell),
              "reverse Y view rejected a clippable cell") ||
      !ExpectCell(cell, 22, 20,
                  "reverse Y view changed far/secondary clipping")) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
