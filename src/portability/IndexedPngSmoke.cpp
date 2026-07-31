#include "IndexedPng.h"
#include "RecoveredSavePreview.h"

#include <objbase.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "indexed PNG smoke failed: %s\n", message);
  return EXIT_FAILURE;
}

std::uint32_t BigEndian32(const std::uint8_t* bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24u) |
         (static_cast<std::uint32_t>(bytes[1]) << 16u) |
         (static_cast<std::uint32_t>(bytes[2]) << 8u) |
         static_cast<std::uint32_t>(bytes[3]);
}

std::uint32_t UpdateCrc(std::uint32_t crc, const std::uint8_t* bytes,
                        std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    crc ^= bytes[index];
    for (unsigned int bit = 0; bit < 8u; ++bit)
      crc = (crc & 1u) != 0u ? (crc >> 1u) ^ UINT32_C(0xedb88320)
                             : crc >> 1u;
  }
  return crc;
}

std::uint32_t Adler32(const std::vector<std::uint8_t>& bytes) {
  constexpr std::uint32_t kModulus = 65521u;
  std::uint32_t first = 1u;
  std::uint32_t second = 0u;
  for (std::uint8_t value : bytes) {
    first = (first + value) % kModulus;
    second = (second + first) % kModulus;
  }
  return (second << 16u) | first;
}

bool InflateStoredZlib(const std::vector<std::uint8_t>& zlib,
                       std::vector<std::uint8_t>* raw) {
  if (raw == nullptr || zlib.size() < 11u ||
      zlib[0] != 0x78u || zlib[1] != 0x01u)
    return false;
  raw->clear();
  std::size_t offset = 2u;
  bool finalBlock = false;
  while (!finalBlock) {
    if (offset + 5u > zlib.size() - 4u) return false;
    const std::uint8_t header = zlib[offset++];
    if ((header & 0x06u) != 0u || (header & 0xf8u) != 0u)
      return false;
    finalBlock = (header & 1u) != 0u;
    const std::uint16_t length =
        static_cast<std::uint16_t>(zlib[offset]) |
        static_cast<std::uint16_t>(zlib[offset + 1u] << 8u);
    const std::uint16_t inverse =
        static_cast<std::uint16_t>(zlib[offset + 2u]) |
        static_cast<std::uint16_t>(zlib[offset + 3u] << 8u);
    offset += 4u;
    if (static_cast<std::uint16_t>(~length) != inverse ||
        offset + length > zlib.size() - 4u)
      return false;
    raw->insert(raw->end(), zlib.begin() + offset,
                zlib.begin() + offset + length);
    offset += length;
  }
  return offset + 4u == zlib.size() &&
         BigEndian32(zlib.data() + offset) == Adler32(*raw);
}

}  // namespace

int main() {
  constexpr std::uint32_t width = 4u;
  constexpr std::uint32_t height = 3u;
  const std::uint8_t pixels[width * height] = {
      0u, 1u, 2u, 3u,
      4u, 5u, 6u, 7u,
      8u, 9u, 10u, 11u};
  std::uint8_t palette[256u * 3u] = {};
  for (std::size_t index = 0; index < 256u; ++index) {
    palette[index * 3u] = static_cast<std::uint8_t>(index);
    palette[index * 3u + 1u] =
        static_cast<std::uint8_t>(255u - index);
    palette[index * 3u + 2u] =
        static_cast<std::uint8_t>(index ^ 0x55u);
  }

  std::vector<std::uint8_t> png;
  SIndexedPngSummary summary;
  std::string failure;
  if (!IndexedPng_Encode(width, height, pixels, width, palette,
                         sizeof(palette), &png, &summary, &failure) ||
      !summary.ready || summary.width != width ||
      summary.height != height ||
      summary.scanlineBytes != height * (width + 1u) ||
      summary.encodedBytes != png.size() ||
      summary.fingerprint == 0u || png.size() < 8u) {
    return Fail(failure.empty() ? "canonical encode failed"
                                : failure.c_str());
  }
  const std::uint8_t signature[8] =
      {0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au};
  if (std::memcmp(png.data(), signature, sizeof(signature)) != 0)
    return Fail("PNG signature differs");

  std::vector<std::uint8_t> idat;
  bool sawHeader = false;
  bool sawPalette = false;
  bool sawEnd = false;
  std::size_t offset = sizeof(signature);
  while (offset < png.size()) {
    if (png.size() - offset < 12u)
      return Fail("chunk header is truncated");
    const std::uint32_t length = BigEndian32(png.data() + offset);
    const std::size_t typeOffset = offset + 4u;
    const std::size_t payloadOffset = typeOffset + 4u;
    const std::size_t crcOffset = payloadOffset + length;
    if (crcOffset > png.size() || png.size() - crcOffset < 4u)
      return Fail("chunk payload is truncated");
    std::uint32_t crc = UINT32_C(0xffffffff);
    crc = UpdateCrc(crc, png.data() + typeOffset, 4u + length);
    if ((crc ^ UINT32_C(0xffffffff)) !=
        BigEndian32(png.data() + crcOffset))
      return Fail("chunk CRC differs");

    const char* type =
        reinterpret_cast<const char*>(png.data() + typeOffset);
    if (std::memcmp(type, "IHDR", 4u) == 0) {
      sawHeader = length == 13u &&
                  BigEndian32(png.data() + payloadOffset) == width &&
                  BigEndian32(png.data() + payloadOffset + 4u) == height &&
                  png[payloadOffset + 8u] == 8u &&
                  png[payloadOffset + 9u] == 3u;
    } else if (std::memcmp(type, "PLTE", 4u) == 0) {
      sawPalette =
          length == sizeof(palette) &&
          std::memcmp(png.data() + payloadOffset, palette,
                      sizeof(palette)) == 0;
    } else if (std::memcmp(type, "IDAT", 4u) == 0) {
      idat.insert(idat.end(), png.begin() + payloadOffset,
                  png.begin() + payloadOffset + length);
    } else if (std::memcmp(type, "IEND", 4u) == 0) {
      sawEnd = length == 0u;
    }
    offset = crcOffset + 4u;
  }
  if (!sawHeader || !sawPalette || idat.empty() || !sawEnd)
    return Fail("required PNG chunks are missing");

  std::vector<std::uint8_t> raw;
  if (!InflateStoredZlib(idat, &raw) ||
      raw.size() != height * (width + 1u))
    return Fail("stored zlib stream is invalid");
  for (std::uint32_t row = 0; row < height; ++row) {
    const std::size_t rowOffset =
        static_cast<std::size_t>(row) * (width + 1u);
    if (raw[rowOffset] != 0u ||
        std::memcmp(raw.data() + rowOffset + 1u,
                    pixels + static_cast<std::size_t>(row) * width,
                    width) != 0)
      return Fail("decoded scanline differs");
  }

  const HRESULT comResult =
      CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
    return Fail("COM initialization for preview decode failed");
  const bool comOwned = SUCCEEDED(comResult);
  SRecoveredSavePreviewImage decoded;
  if (!RecoveredSavePreview_DecodePng(
          png, width, height, &decoded, &failure) || !decoded.ready ||
      decoded.sourceWidth != width || decoded.sourceHeight != height ||
      decoded.width != width || decoded.height != height ||
      decoded.stride != width * 4u ||
      decoded.bgra.size() != width * height * 4u ||
      decoded.sourceFingerprint == 0u) {
    if (comOwned) CoUninitialize();
    return Fail(failure.empty() ? "Windows preview decode failed"
                                : failure.c_str());
  }
  for (std::size_t index = 0; index < width * height; ++index) {
    const std::uint8_t paletteIndex = pixels[index];
    const std::uint8_t* bgra = decoded.bgra.data() + index * 4u;
    if (bgra[0] != palette[paletteIndex * 3u + 2u] ||
        bgra[1] != palette[paletteIndex * 3u + 1u] ||
        bgra[2] != palette[paletteIndex * 3u] || bgra[3] != 255u) {
      if (comOwned) CoUninitialize();
      return Fail("Windows preview BGRA pixels differ");
    }
  }
  SRecoveredSavePreviewImage scaled;
  if (!RecoveredSavePreview_DecodePng(
          png, 2u, 2u, &scaled, &failure) || !scaled.ready ||
      scaled.width != 2u || scaled.height != 1u ||
      scaled.sourceWidth != width || scaled.sourceHeight != height) {
    if (comOwned) CoUninitialize();
    return Fail("Windows preview aspect-fit scaling differs");
  }
  std::vector<std::uint8_t> corruptPng = png;
  corruptPng[0] = 0u;
  SRecoveredSavePreviewImage rejectedPreview;
  rejectedPreview.ready = true;
  if (RecoveredSavePreview_DecodePng(
          corruptPng, width, height, &rejectedPreview, &failure) ||
      rejectedPreview.ready) {
    if (comOwned) CoUninitialize();
    return Fail("corrupt Windows preview was admitted");
  }
  if (comOwned) CoUninitialize();

  std::vector<std::uint8_t> preserved = {7u, 8u, 9u};
  SIndexedPngSummary rejected;
  if (IndexedPng_Encode(0u, height, pixels, width, palette,
                        sizeof(palette), &preserved, &rejected,
                        &failure) ||
      preserved != std::vector<std::uint8_t>({7u, 8u, 9u}) ||
      rejected.ready)
    return Fail("invalid encode mutated its output");
  if (IndexedPng_Encode(
          1u, 2u, pixels,
          (std::numeric_limits<std::size_t>::max)(), palette,
          sizeof(palette), &preserved, &rejected, &failure) ||
      preserved != std::vector<std::uint8_t>({7u, 8u, 9u}) ||
      rejected.ready)
    return Fail("overflowing stride mutated its output");

  std::printf(
      "indexed PNG width=%u height=%u palette=256 bytes=%zu "
      "fingerprint=%llu zlib=stored wic=BGRA/aspect-fit\n",
      summary.width, summary.height, summary.encodedBytes,
      static_cast<unsigned long long>(summary.fingerprint));
  return EXIT_SUCCESS;
}
