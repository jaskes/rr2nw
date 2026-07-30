#ifndef RR2NW_GRAPH_SOFTWARE_TEXTURE_INTERNAL_H
#define RR2NW_GRAPH_SOFTWARE_TEXTURE_INTERNAL_H

#include "graph.h"

#include <cstddef>
#include <cstdint>

namespace rr2nw_software {

constexpr std::uint32_t kSoftwareTextureMagic = 0x52525854UL;

// Keep this prefix in lockstep with the recovered Win32 STextDB layout.  The
// scalar rasterizer and the texture owner intentionally share the same view
// instead of passing legacy handles through integers.
struct SoftwareTexture {
  unsigned long flags;
  unsigned long counter;
  int* textureCache;
  unsigned char* dataPtr;
  char filename[64];
  TTextureLoadFunc loadFunc;
  void* loadFuncUserPar;
  TTextureLoadIntFunc loadIntFunc;
  unsigned long size;
  long w;
  long h;
  std::uint32_t magic;
};

static_assert(sizeof(void*) == 4 && sizeof(unsigned long) == 4,
              "recovered software texture handles require the Win32 ABI");
static_assert(offsetof(SoftwareTexture, textureCache) == 8 &&
                  offsetof(SoftwareTexture, dataPtr) == 12 &&
                  offsetof(SoftwareTexture, filename) == 16 &&
                  offsetof(SoftwareTexture, size) == 92 &&
                  offsetof(SoftwareTexture, w) == 96 &&
                  offsetof(SoftwareTexture, h) == 100,
              "software-visible STextDB prefix layout changed");

inline SoftwareTexture* TextureFromHandle(void* handle) {
  SoftwareTexture* texture = static_cast<SoftwareTexture*>(handle);
  return texture != nullptr && texture->magic == kSoftwareTextureMagic
             ? texture
             : nullptr;
}

}  // namespace rr2nw_software

#endif
