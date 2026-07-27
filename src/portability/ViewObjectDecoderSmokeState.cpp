#include <intrin.h>

#include "graph.h"

namespace {

// Decoder coverage pulls legacy draw references from monolithic object files.
// These callbacks are intentionally test-only and never enter the game runtime.
void IgnoreSpriteDraw(int x0, int y0, int x1, int y1, int u0, int v0,
                      int u1, int v1, int iz, void* texture) {
  (void)x0;
  (void)y0;
  (void)x1;
  (void)y1;
  (void)u0;
  (void)v0;
  (void)u1;
  (void)v1;
  (void)iz;
  (void)texture;
}

void IgnoreZPrecision(int shift) {
  (void)shift;
}

}  // namespace

void (*_pGRDrawSprite)(int, int, int, int, int, int, int, int, int, void*) =
    IgnoreSpriteDraw;
void (*_pGRSetZPrecision)(int) = IgnoreZPrecision;

void GETCYCLE(unsigned* words) {
  const unsigned __int64 cycle = __rdtsc();
  words[0] = static_cast<unsigned>(cycle);
  words[1] = static_cast<unsigned>(cycle >> 32);
}
