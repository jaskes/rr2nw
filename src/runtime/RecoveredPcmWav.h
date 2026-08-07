#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum ERecoveredPcmWavIssue {
  RECOVERED_PCM_WAV_INVALID_ARGUMENT = 1u << 0,
  RECOVERED_PCM_WAV_SIZE_LIMIT = 1u << 1,
  RECOVERED_PCM_WAV_RIFF_INVALID = 1u << 2,
  RECOVERED_PCM_WAV_FORMAT_UNSUPPORTED = 1u << 3,
  RECOVERED_PCM_WAV_DATA_INVALID = 1u << 4,
  RECOVERED_PCM_WAV_ALLOCATION_FAILURE = 1u << 5
};

struct SRecoveredPcmWav {
  std::uint16_t channels = 0;
  std::uint16_t bitsPerSample = 0;
  std::uint32_t sampleRate = 0;
  std::uint16_t blockAlign = 0;
  std::uint32_t averageBytesPerSecond = 0;
  std::vector<std::uint8_t> samples;
};

struct SRecoveredPcmWavResult {
  unsigned int issues = 0;
  char error[192] = {};
};

// Parses an in-memory RIFF/WAVE file without touching a device.  The admitted
// subset is deliberately narrow and matches all 86 inspected retail WAVs:
// integer PCM, one or two channels, 8/16-bit samples and bounded payloads.
bool RecoveredPcmWav_Decode(const void* bytes, std::size_t size,
                            SRecoveredPcmWav* wav,
                            SRecoveredPcmWavResult* result);

std::size_t RecoveredPcmWav_MaximumSourceBytes();
std::size_t RecoveredPcmWav_MaximumSampleBytes();
