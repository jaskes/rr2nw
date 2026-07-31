#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SIndexedPngSummary {
  bool ready = false;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::size_t scanlineBytes = 0;
  std::size_t encodedBytes = 0;
  std::uint64_t fingerprint = 0;
};

bool IndexedPng_Encode(
    std::uint32_t width, std::uint32_t height,
    const std::uint8_t* pixels, std::size_t stride,
    const std::uint8_t* paletteRgb, std::size_t paletteBytes,
    std::vector<std::uint8_t>* png, SIndexedPngSummary* summary,
    std::string* failure);
