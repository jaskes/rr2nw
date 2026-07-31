#include "IndexedPng.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

constexpr std::uint32_t kMaximumDimension = 16384u;
constexpr std::size_t kPaletteBytes = 256u * 3u;
constexpr std::size_t kMaximumRawBytes = 64u * 1024u * 1024u;
constexpr std::uint64_t kHashOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kHashPrime = UINT64_C(1099511628211);
const std::uint8_t kSignature[8] =
    {0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au};

bool Fail(std::string* failure, const char* detail) {
  if (failure != nullptr) *failure = detail;
  return false;
}

void PutBigEndian32(std::vector<std::uint8_t>* bytes,
                    std::uint32_t value) {
  bytes->push_back(static_cast<std::uint8_t>(value >> 24u));
  bytes->push_back(static_cast<std::uint8_t>(value >> 16u));
  bytes->push_back(static_cast<std::uint8_t>(value >> 8u));
  bytes->push_back(static_cast<std::uint8_t>(value));
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

void AppendChunk(std::vector<std::uint8_t>* png, const char type[4],
                 const std::vector<std::uint8_t>& payload) {
  PutBigEndian32(png, static_cast<std::uint32_t>(payload.size()));
  const std::size_t typeOffset = png->size();
  png->insert(png->end(), type, type + 4);
  png->insert(png->end(), payload.begin(), payload.end());
  std::uint32_t crc = UINT32_C(0xffffffff);
  crc = UpdateCrc(crc, png->data() + typeOffset,
                  4u + payload.size());
  PutBigEndian32(png, crc ^ UINT32_C(0xffffffff));
}

std::uint32_t Adler32(const std::vector<std::uint8_t>& bytes) {
  constexpr std::uint32_t kModulus = 65521u;
  std::uint32_t first = 1u;
  std::uint32_t second = 0u;
  for (std::uint8_t value : bytes) {
    first += value;
    if (first >= kModulus) first -= kModulus;
    second += first;
    if (second >= kModulus) second -= kModulus;
  }
  return (second << 16u) | first;
}

std::uint64_t Fingerprint(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t hash = kHashOffset;
  for (std::uint8_t value : bytes) {
    hash ^= value;
    hash *= kHashPrime;
  }
  return hash;
}

}  // namespace

bool IndexedPng_Encode(
    std::uint32_t width, std::uint32_t height,
    const std::uint8_t* pixels, std::size_t stride,
    const std::uint8_t* paletteRgb, std::size_t paletteBytes,
    std::vector<std::uint8_t>* png, SIndexedPngSummary* summary,
    std::string* failure) {
  if (failure != nullptr) failure->clear();
  if (png == nullptr || summary == nullptr) {
    return Fail(failure, "indexed PNG output is null");
  }
  *summary = {};
  if (width == 0u || height == 0u || width > kMaximumDimension ||
      height > kMaximumDimension || pixels == nullptr ||
      paletteRgb == nullptr || paletteBytes != kPaletteBytes ||
      stride < width) {
    return Fail(failure, "indexed PNG input is invalid");
  }
  if (height > 1u &&
      stride > (std::numeric_limits<std::size_t>::max() - width) /
                   (static_cast<std::size_t>(height) - 1u)) {
    return Fail(failure, "indexed PNG source stride overflows");
  }
  const std::size_t rowBytes = static_cast<std::size_t>(width) + 1u;
  if (rowBytes > kMaximumRawBytes ||
      static_cast<std::size_t>(height) >
          kMaximumRawBytes / rowBytes) {
    return Fail(failure, "indexed PNG scanlines exceed their size bound");
  }

  std::vector<std::uint8_t> scanlines(
      rowBytes * static_cast<std::size_t>(height));
  for (std::uint32_t row = 0; row < height; ++row) {
    std::uint8_t* target =
        scanlines.data() + static_cast<std::size_t>(row) * rowBytes;
    target[0] = 0u;
    std::memcpy(target + 1u,
                pixels + static_cast<std::size_t>(row) * stride, width);
  }

  std::vector<std::uint8_t> compressed;
  const std::size_t blocks =
      (scanlines.size() + UINT16_MAX - 1u) / UINT16_MAX;
  compressed.reserve(2u + scanlines.size() + blocks * 5u + 4u);
  // RFC 1950 CMF/FLG for deflate with the fastest compression hint.
  compressed.push_back(0x78u);
  compressed.push_back(0x01u);
  std::size_t offset = 0;
  while (offset < scanlines.size()) {
    const std::size_t remaining = scanlines.size() - offset;
    const std::uint16_t length = static_cast<std::uint16_t>(
        (std::min)(remaining, static_cast<std::size_t>(UINT16_MAX)));
    const bool finalBlock = static_cast<std::size_t>(length) == remaining;
    // Stored deflate blocks begin on a byte boundary. BTYPE is 00.
    compressed.push_back(finalBlock ? 0x01u : 0x00u);
    compressed.push_back(static_cast<std::uint8_t>(length));
    compressed.push_back(static_cast<std::uint8_t>(length >> 8u));
    const std::uint16_t inverse = static_cast<std::uint16_t>(~length);
    compressed.push_back(static_cast<std::uint8_t>(inverse));
    compressed.push_back(static_cast<std::uint8_t>(inverse >> 8u));
    compressed.insert(compressed.end(), scanlines.begin() + offset,
                      scanlines.begin() + offset + length);
    offset += length;
  }
  PutBigEndian32(&compressed, Adler32(scanlines));

  std::vector<std::uint8_t> encoded;
  encoded.reserve(sizeof(kSignature) + 12u + 13u + 12u + kPaletteBytes +
                  12u + compressed.size() + 12u);
  encoded.insert(encoded.end(), kSignature,
                 kSignature + sizeof(kSignature));

  std::vector<std::uint8_t> header;
  header.reserve(13u);
  PutBigEndian32(&header, width);
  PutBigEndian32(&header, height);
  header.push_back(8u);
  header.push_back(3u);
  header.push_back(0u);
  header.push_back(0u);
  header.push_back(0u);
  AppendChunk(&encoded, "IHDR", header);

  std::vector<std::uint8_t> palette(
      paletteRgb, paletteRgb + paletteBytes);
  AppendChunk(&encoded, "PLTE", palette);
  AppendChunk(&encoded, "IDAT", compressed);
  AppendChunk(&encoded, "IEND", {});

  SIndexedPngSummary candidate;
  candidate.ready = true;
  candidate.width = width;
  candidate.height = height;
  candidate.scanlineBytes = scanlines.size();
  candidate.encodedBytes = encoded.size();
  candidate.fingerprint = Fingerprint(encoded);
  if (candidate.fingerprint == 0u) {
    return Fail(failure, "indexed PNG fingerprint is invalid");
  }
  *png = std::move(encoded);
  *summary = candidate;
  return true;
}
