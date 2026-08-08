#include "RecoveredPcmWav.h"

#include <cstdio>
#include <cstring>
#include <limits>

namespace {

constexpr std::size_t kMaximumSourceBytes = 8u * 1024u * 1024u;
constexpr std::size_t kMaximumSampleBytes = 8u * 1024u * 1024u;

std::uint16_t Read16(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         static_cast<std::uint16_t>(bytes[1] << 8u);
}

std::uint32_t Read32(const std::uint8_t* bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8u) |
         (static_cast<std::uint32_t>(bytes[2]) << 16u) |
         (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

bool IsFourCC(const std::uint8_t* bytes, const char* expected) {
  return std::memcmp(bytes, expected, 4u) == 0;
}

bool Fail(SRecoveredPcmWavResult* result, unsigned int issue,
          const char* message) {
  if (result != nullptr) {
    result->issues |= issue;
    if (result->error[0] == 0)
      std::snprintf(result->error, sizeof(result->error), "%s", message);
  }
  return false;
}

bool ReadExact(std::FILE* file, void* bytes, std::size_t size) {
  return file != nullptr && bytes != nullptr &&
         std::fread(bytes, 1u, size, file) == size;
}

bool ValidFormat(std::uint16_t formatTag, std::uint16_t channels,
                 std::uint32_t sampleRate, std::uint32_t averageBytes,
                 std::uint16_t blockAlign, std::uint16_t bits,
                 std::size_t sampleBytes) {
  if (formatTag != 1u || (channels != 1u && channels != 2u) ||
      (bits != 8u && bits != 16u) || sampleRate < 8000u ||
      sampleRate > 192000u)
    return false;
  const std::uint32_t expectedAlign =
      static_cast<std::uint32_t>(channels) * (bits / 8u);
  const std::uint64_t expectedAverage =
      static_cast<std::uint64_t>(sampleRate) * expectedAlign;
  return blockAlign == expectedAlign && averageBytes == expectedAverage &&
         sampleBytes != 0u && sampleBytes % blockAlign == 0u;
}

}  // namespace

bool RecoveredPcmWav_Decode(const void* source, std::size_t size,
                            SRecoveredPcmWav* wav,
                            SRecoveredPcmWavResult* result) {
  if (wav != nullptr) *wav = {};
  if (result != nullptr) *result = {};
  if (source == nullptr || wav == nullptr || size < 12u)
    return Fail(result, RECOVERED_PCM_WAV_INVALID_ARGUMENT,
                "PCM WAV source is missing or truncated");
  if (size > kMaximumSourceBytes)
    return Fail(result, RECOVERED_PCM_WAV_SIZE_LIMIT,
                "PCM WAV source exceeds the bounded admission limit");

  const auto* bytes = static_cast<const std::uint8_t*>(source);
  if (!IsFourCC(bytes, "RIFF") || !IsFourCC(bytes + 8u, "WAVE"))
    return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                "PCM WAV has no RIFF/WAVE identity");
  const std::uint32_t riffPayload = Read32(bytes + 4u);
  if (riffPayload < 4u || static_cast<std::uint64_t>(riffPayload) + 8u != size)
    return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                "PCM WAV RIFF extent does not match the source");

  bool haveFormat = false;
  bool haveData = false;
  std::uint16_t formatTag = 0;
  std::uint16_t channels = 0;
  std::uint32_t sampleRate = 0;
  std::uint32_t averageBytes = 0;
  std::uint16_t blockAlign = 0;
  std::uint16_t bits = 0;
  const std::uint8_t* sampleData = nullptr;
  std::size_t sampleBytes = 0;

  std::size_t cursor = 12u;
  while (cursor < size) {
    if (size - cursor < 8u)
      return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                  "PCM WAV has a truncated chunk header");
    const std::uint8_t* chunk = bytes + cursor;
    const std::uint32_t chunkBytes = Read32(chunk + 4u);
    const std::uint64_t padded = static_cast<std::uint64_t>(chunkBytes) +
                                 static_cast<std::uint64_t>(chunkBytes & 1u);
    if (padded > size - cursor - 8u)
      return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                  "PCM WAV chunk escapes the RIFF extent");
    const std::uint8_t* payload = chunk + 8u;
    if (IsFourCC(chunk, "fmt ")) {
      if (haveFormat || chunkBytes < 16u)
        return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                    "PCM WAV format chunk is missing or duplicated");
      formatTag = Read16(payload);
      channels = Read16(payload + 2u);
      sampleRate = Read32(payload + 4u);
      averageBytes = Read32(payload + 8u);
      blockAlign = Read16(payload + 12u);
      bits = Read16(payload + 14u);
      haveFormat = true;
    } else if (IsFourCC(chunk, "data")) {
      if (haveData || chunkBytes == 0u || chunkBytes > kMaximumSampleBytes)
        return Fail(result, RECOVERED_PCM_WAV_DATA_INVALID,
                    "PCM WAV data chunk is empty, duplicated or too large");
      sampleData = payload;
      sampleBytes = chunkBytes;
      haveData = true;
    }
    cursor += 8u + static_cast<std::size_t>(padded);
  }
  if (cursor != size || !haveFormat || !haveData)
    return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                "PCM WAV is missing a required format or data chunk");

  if (!ValidFormat(formatTag, channels, sampleRate, averageBytes, blockAlign,
                   bits, sampleBytes))
    return Fail(result, RECOVERED_PCM_WAV_FORMAT_UNSUPPORTED,
                "PCM WAV byte-rate or block alignment is inconsistent");

  wav->channels = channels;
  wav->bitsPerSample = bits;
  wav->sampleRate = sampleRate;
  wav->blockAlign = blockAlign;
  wav->averageBytesPerSecond = averageBytes;
  try {
    wav->samples.assign(sampleData, sampleData + sampleBytes);
  } catch (...) {
    *wav = {};
    return Fail(result, RECOVERED_PCM_WAV_ALLOCATION_FAILURE,
                "PCM WAV sample allocation failed");
  }
  return true;
}

bool RecoveredPcmWav_InspectStream(std::FILE* file, std::size_t size,
                                   SRecoveredPcmWavStreamInfo* stream,
                                   SRecoveredPcmWavResult* result) {
  if (stream != nullptr) *stream = {};
  if (result != nullptr) *result = {};
  if (file == nullptr || stream == nullptr || size < 12u)
    return Fail(result, RECOVERED_PCM_WAV_INVALID_ARGUMENT,
                "streamed PCM WAV source is missing or truncated");
  if (size > kMaximumSourceBytes)
    return Fail(result, RECOVERED_PCM_WAV_SIZE_LIMIT,
                "streamed PCM WAV source exceeds the bounded admission limit");
  if (std::fseek(file, 0, SEEK_SET) != 0)
    return Fail(result, RECOVERED_PCM_WAV_INVALID_ARGUMENT,
                "streamed PCM WAV source is not seekable");
  std::uint8_t header[12] = {};
  if (!ReadExact(file, header, sizeof(header)) ||
      !IsFourCC(header, "RIFF") || !IsFourCC(header + 8u, "WAVE") ||
      static_cast<std::uint64_t>(Read32(header + 4u)) + 8u != size)
    return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                "streamed PCM WAV has invalid RIFF/WAVE identity");

  bool haveFormat = false;
  bool haveData = false;
  std::uint16_t formatTag = 0;
  std::uint16_t channels = 0;
  std::uint32_t sampleRate = 0;
  std::uint32_t averageBytes = 0;
  std::uint16_t blockAlign = 0;
  std::uint16_t bits = 0;
  std::size_t dataOffset = 0;
  std::size_t dataBytes = 0;
  std::size_t cursor = 12u;
  while (cursor < size) {
    if (size - cursor < 8u)
      return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                  "streamed PCM WAV has a truncated chunk header");
    std::uint8_t chunk[8] = {};
    if (!ReadExact(file, chunk, sizeof(chunk)))
      return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                  "streamed PCM WAV chunk header could not be read");
    const std::uint32_t chunkBytes = Read32(chunk + 4u);
    const std::uint64_t padded = static_cast<std::uint64_t>(chunkBytes) +
                                 static_cast<std::uint64_t>(chunkBytes & 1u);
    if (padded > size - cursor - 8u)
      return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                  "streamed PCM WAV chunk escapes the RIFF extent");
    const std::size_t payloadOffset = cursor + 8u;
    if (IsFourCC(chunk, "fmt ")) {
      if (haveFormat || chunkBytes < 16u)
        return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                    "streamed PCM WAV format is missing or duplicated");
      std::uint8_t format[16] = {};
      if (!ReadExact(file, format, sizeof(format)))
        return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                    "streamed PCM WAV format could not be read");
      formatTag = Read16(format);
      channels = Read16(format + 2u);
      sampleRate = Read32(format + 4u);
      averageBytes = Read32(format + 8u);
      blockAlign = Read16(format + 12u);
      bits = Read16(format + 14u);
      haveFormat = true;
    } else if (IsFourCC(chunk, "data")) {
      if (haveData || chunkBytes == 0u || chunkBytes > kMaximumSampleBytes)
        return Fail(result, RECOVERED_PCM_WAV_DATA_INVALID,
                    "streamed PCM WAV data is empty, duplicated or too large");
      dataOffset = payloadOffset;
      dataBytes = chunkBytes;
      haveData = true;
    }
    cursor += 8u + static_cast<std::size_t>(padded);
    if (cursor > static_cast<std::size_t>((std::numeric_limits<long>::max)()) ||
        std::fseek(file, static_cast<long>(cursor), SEEK_SET) != 0)
      return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                  "streamed PCM WAV chunk seek failed");
  }
  if (cursor != size || !haveFormat || !haveData)
    return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                "streamed PCM WAV is missing format or data");
  if (!ValidFormat(formatTag, channels, sampleRate, averageBytes, blockAlign,
                   bits, dataBytes))
    return Fail(result, RECOVERED_PCM_WAV_FORMAT_UNSUPPORTED,
                "streamed PCM WAV format is outside the maintained subset");
  if (dataOffset > static_cast<std::size_t>((std::numeric_limits<long>::max)()) ||
      std::fseek(file, static_cast<long>(dataOffset), SEEK_SET) != 0)
    return Fail(result, RECOVERED_PCM_WAV_RIFF_INVALID,
                "streamed PCM WAV data seek failed");
  stream->channels = channels;
  stream->bitsPerSample = bits;
  stream->sampleRate = sampleRate;
  stream->blockAlign = blockAlign;
  stream->averageBytesPerSecond = averageBytes;
  stream->dataOffset = dataOffset;
  stream->dataBytes = dataBytes;
  stream->sourceBytes = size;
  return true;
}

std::size_t RecoveredPcmWav_MaximumSourceBytes() {
  return kMaximumSourceBytes;
}

std::size_t RecoveredPcmWav_MaximumSampleBytes() {
  return kMaximumSampleBytes;
}
