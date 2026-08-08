#include "RecoveredPcmWav.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void Append16(std::vector<std::uint8_t>* bytes, std::uint16_t value) {
  bytes->push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
}

void Append32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (unsigned int shift = 0; shift < 32u; shift += 8u)
    bytes->push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

void AppendId(std::vector<std::uint8_t>* bytes, const char* id) {
  bytes->insert(bytes->end(), id, id + 4);
}

std::vector<std::uint8_t> Fixture(std::uint16_t format,
                                  bool duplicateData) {
  std::vector<std::uint8_t> bytes;
  AppendId(&bytes, "RIFF");
  Append32(&bytes, 0u);
  AppendId(&bytes, "WAVE");
  AppendId(&bytes, "fmt ");
  Append32(&bytes, 16u);
  Append16(&bytes, format);
  Append16(&bytes, 1u);
  Append32(&bytes, 22050u);
  Append32(&bytes, 44100u);
  Append16(&bytes, 2u);
  Append16(&bytes, 16u);
  AppendId(&bytes, "data");
  Append32(&bytes, 8u);
  for (std::uint8_t value = 0; value < 8u; ++value) bytes.push_back(value);
  if (duplicateData) {
    AppendId(&bytes, "data");
    Append32(&bytes, 2u);
    bytes.push_back(0u);
    bytes.push_back(0u);
  }
  const std::uint32_t riff = static_cast<std::uint32_t>(bytes.size() - 8u);
  bytes[4] = static_cast<std::uint8_t>(riff & 0xffu);
  bytes[5] = static_cast<std::uint8_t>((riff >> 8u) & 0xffu);
  bytes[6] = static_cast<std::uint8_t>((riff >> 16u) & 0xffu);
  bytes[7] = static_cast<std::uint8_t>((riff >> 24u) & 0xffu);
  return bytes;
}

}  // namespace

int main() {
  const std::vector<std::uint8_t> valid = Fixture(1u, false);
  SRecoveredPcmWav wav;
  SRecoveredPcmWavResult result;
  if (!RecoveredPcmWav_Decode(valid.data(), valid.size(), &wav, &result) ||
      wav.channels != 1u || wav.bitsPerSample != 16u ||
      wav.sampleRate != 22050u || wav.blockAlign != 2u ||
      wav.averageBytesPerSecond != 44100u || wav.samples.size() != 8u) {
    std::fprintf(stderr, "pcm-wav-smoke: valid fixture rejected: %s\n",
                 result.error);
    return EXIT_FAILURE;
  }
  std::FILE* streamFile = nullptr;
  const errno_t streamOpen = tmpfile_s(&streamFile);
  SRecoveredPcmWavStreamInfo stream;
  if (streamOpen != 0 || streamFile == nullptr ||
      std::fwrite(valid.data(), 1u, valid.size(), streamFile) != valid.size() ||
      !RecoveredPcmWav_InspectStream(streamFile, valid.size(), &stream,
                                     &result) ||
      stream.channels != 1u || stream.bitsPerSample != 16u ||
      stream.sampleRate != 22050u || stream.blockAlign != 2u ||
      stream.averageBytesPerSecond != 44100u || stream.dataOffset != 44u ||
      stream.dataBytes != 8u || stream.sourceBytes != valid.size() ||
      std::ftell(streamFile) != static_cast<long>(stream.dataOffset)) {
    if (streamFile != nullptr) std::fclose(streamFile);
    std::fprintf(stderr, "pcm-wav-smoke: stream inspection failed: %s\n",
                 result.error);
    return EXIT_FAILURE;
  }
  std::fclose(streamFile);
  std::vector<std::uint8_t> truncated = valid;
  truncated.pop_back();
  const std::vector<std::uint8_t> compressed = Fixture(2u, false);
  const std::vector<std::uint8_t> duplicate = Fixture(1u, true);
  if (RecoveredPcmWav_Decode(truncated.data(), truncated.size(), &wav,
                             &result) ||
      (result.issues & RECOVERED_PCM_WAV_RIFF_INVALID) == 0u ||
      RecoveredPcmWav_Decode(compressed.data(), compressed.size(), &wav,
                             &result) ||
      (result.issues & RECOVERED_PCM_WAV_FORMAT_UNSUPPORTED) == 0u ||
      RecoveredPcmWav_Decode(duplicate.data(), duplicate.size(), &wav,
                             &result) ||
      (result.issues & RECOVERED_PCM_WAV_DATA_INVALID) == 0u) {
    std::fprintf(stderr,
                 "pcm-wav-smoke: malformed or unsupported fixture admitted\n");
    return EXIT_FAILURE;
  }
  std::printf("pcm wav=RIFF/PCM channels=1-2 bits=8/16 source_limit=%zu "
              "sample_limit=%zu stream=44/8 malformed=fail-closed\n",
              RecoveredPcmWav_MaximumSourceBytes(),
              RecoveredPcmWav_MaximumSampleBytes());
  return EXIT_SUCCESS;
}
