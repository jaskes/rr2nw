#include "ReplayHashJournal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

const std::uint32_t kMagic = 0x31485052u;  // RPH1
const std::uint32_t kVersion = 1u;
const std::size_t kMaximumControlBytes = 64u * 1024u * 1024u;
const std::size_t kMaximumSamples = 1000000u;
const double kMinimumStepSeconds = 0.001;
const double kMaximumStepSeconds = 0.05;
const double kTimeTolerance = 1.0e-9;
const std::uint64_t kHashOffset = 14695981039346656037ull;
const std::uint64_t kHashPrime = 1099511628211ull;

struct Writer {
  std::vector<std::uint8_t>* bytes;

  void U32(std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
      bytes->push_back(static_cast<std::uint8_t>(value >> shift));
  }
  void U64(std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8)
      bytes->push_back(static_cast<std::uint8_t>(value >> shift));
  }
  void Double(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    U64(bits);
  }
  void Bytes(const std::uint8_t* source, std::size_t count) {
    if (count != 0u) bytes->insert(bytes->end(), source, source + count);
  }
};

struct Reader {
  const std::vector<std::uint8_t>& bytes;
  std::size_t offset = 0;

  bool U32(std::uint32_t* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < 4u)
      return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
      *value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    return true;
  }
  bool U64(std::uint64_t* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < 8u)
      return false;
    *value = 0;
    for (int shift = 0; shift < 64; shift += 8)
      *value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    return true;
  }
  bool Double(double* value) {
    std::uint64_t bits = 0;
    if (value == nullptr || !U64(&bits)) return false;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
  }
  bool Bytes(std::size_t count, const std::uint8_t** source) {
    if (source == nullptr || offset > bytes.size() ||
        bytes.size() - offset < count)
      return false;
    *source = bytes.data() + offset;
    offset += count;
    return true;
  }
};

bool NearlyEqual(double left, double right) {
  const double scale = (std::max)(
      1.0, (std::max)(std::fabs(left), std::fabs(right)));
  const double precisionFloor =
      8.0 * (std::numeric_limits<double>::epsilon)() * scale;
  return std::fabs(left - right) <=
      (std::max)(kTimeTolerance, precisionFloor);
}

void HashBytes(std::uint64_t* hash, const std::uint8_t* bytes,
               std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

}  // namespace

bool ReplayHashJournal_Create(
    std::uint64_t contentFingerprint, double simulationStepSeconds,
    const SVehicleControlJournal& controls,
    const std::vector<SReplayHashSample>& samples,
    SReplayHashJournal* journal) {
  return ReplayHashJournal_CreateWithAlgorithm(
      contentFingerprint, simulationStepSeconds,
      RR2NW_REPLAY_HASH_VEHICLE_CLK1_RNG_FNV1A64,
      controls, samples, journal);
}

bool ReplayHashJournal_CreateWithAlgorithm(
    std::uint64_t contentFingerprint, double simulationStepSeconds,
    std::uint32_t stateHashAlgorithm,
    const SVehicleControlJournal& controls,
    const std::vector<SReplayHashSample>& samples,
    SReplayHashJournal* journal) {
  if (journal == nullptr) return false;
  SReplayHashJournal candidate;
  candidate.contentFingerprint = contentFingerprint;
  candidate.simulationStepSeconds = simulationStepSeconds;
  candidate.stateHashAlgorithm = stateHashAlgorithm;
  candidate.controls = controls;
  candidate.samples = samples;
  if (!ReplayHashJournal_Validate(candidate)) return false;
  *journal = candidate;
  return true;
}

bool ReplayHashJournal_Validate(const SReplayHashJournal& journal) {
  if (journal.contentFingerprint == 0u ||
      !std::isfinite(journal.simulationStepSeconds) ||
      journal.simulationStepSeconds < kMinimumStepSeconds ||
      journal.simulationStepSeconds > kMaximumStepSeconds ||
      (journal.stateHashAlgorithm !=
           RR2NW_REPLAY_HASH_VEHICLE_CLK1_RNG_FNV1A64 &&
       journal.stateHashAlgorithm !=
           RR2NW_REPLAY_HASH_ACTIVE_GAMEPLAY_CORE_FNV1A64) ||
      !journal.controls.sealed ||
      !VehicleControlJournal_Validate(journal.controls) ||
      journal.samples.empty() ||
      journal.samples.size() > kMaximumSamples ||
      journal.controls.finalTick < journal.controls.checkpointTick ||
      journal.controls.finalTick - journal.controls.checkpointTick !=
          journal.samples.size())
    return false;

  std::uint64_t previousTick = journal.controls.checkpointTick;
  for (std::size_t index = 0; index < journal.samples.size(); ++index) {
    const SReplayHashSample& sample = journal.samples[index];
    const double expectedTime = journal.controls.checkpointTime +
        static_cast<double>(index + 1u) * journal.simulationStepSeconds;
    if (sample.tick != previousTick + 1u ||
        !std::isfinite(sample.simulationTime) ||
        !std::isfinite(expectedTime) ||
        !NearlyEqual(sample.simulationTime, expectedTime) ||
        sample.stateHash == 0u)
      return false;
    previousTick = sample.tick;
  }
  return previousTick == journal.controls.finalTick &&
         NearlyEqual(journal.samples.back().simulationTime,
                     journal.controls.finalTime);
}

bool ReplayHashJournal_MatchesIdentity(
    const SReplayHashJournal& journal, std::uint64_t contentFingerprint,
    std::uint64_t controlFingerprint) {
  return contentFingerprint != 0u && controlFingerprint != 0u &&
         ReplayHashJournal_Validate(journal) &&
         journal.contentFingerprint == contentFingerprint &&
         VehicleControlJournal_Fingerprint(journal.controls) ==
             controlFingerprint;
}

bool ReplayHashJournal_Encode(
    const SReplayHashJournal& journal,
    std::vector<std::uint8_t>* bytes) {
  if (bytes == nullptr || !ReplayHashJournal_Validate(journal)) return false;
  std::vector<std::uint8_t> controls;
  if (!VehicleControlJournal_Encode(journal.controls, &controls) ||
      controls.empty() || controls.size() > kMaximumControlBytes)
    return false;
  bytes->clear();
  Writer writer = {bytes};
  writer.U32(kMagic);
  writer.U32(kVersion);
  writer.U64(journal.contentFingerprint);
  writer.Double(journal.simulationStepSeconds);
  writer.U32(journal.stateHashAlgorithm);
  writer.U32(static_cast<std::uint32_t>(controls.size()));
  writer.Bytes(controls.data(), controls.size());
  writer.U32(static_cast<std::uint32_t>(journal.samples.size()));
  for (const SReplayHashSample& sample : journal.samples) {
    writer.U64(sample.tick);
    writer.Double(sample.simulationTime);
    writer.U64(sample.stateHash);
  }
  return true;
}

bool ReplayHashJournal_Decode(
    const std::vector<std::uint8_t>& bytes,
    SReplayHashJournal* journal) {
  if (journal == nullptr || bytes.empty()) return false;
  Reader reader = {bytes};
  SReplayHashJournal candidate;
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t controlSize = 0;
  std::uint32_t sampleCount = 0;
  const std::uint8_t* controlSource = nullptr;
  if (!reader.U32(&magic) || !reader.U32(&version) ||
      magic != kMagic || version != kVersion ||
      !reader.U64(&candidate.contentFingerprint) ||
      !reader.Double(&candidate.simulationStepSeconds) ||
      !reader.U32(&candidate.stateHashAlgorithm) ||
      !reader.U32(&controlSize) || controlSize == 0u ||
      controlSize > kMaximumControlBytes ||
      !reader.Bytes(controlSize, &controlSource))
    return false;
  std::vector<std::uint8_t> controls(controlSource,
                                     controlSource + controlSize);
  if (!VehicleControlJournal_Decode(controls, &candidate.controls) ||
      !reader.U32(&sampleCount) || sampleCount == 0u ||
      sampleCount > kMaximumSamples)
    return false;
  candidate.samples.resize(sampleCount);
  for (SReplayHashSample& sample : candidate.samples)
    if (!reader.U64(&sample.tick) ||
        !reader.Double(&sample.simulationTime) ||
        !reader.U64(&sample.stateHash))
      return false;
  if (reader.offset != bytes.size() ||
      !ReplayHashJournal_Validate(candidate))
    return false;
  *journal = candidate;
  return true;
}

std::uint64_t ReplayHashJournal_Fingerprint(
    const SReplayHashJournal& journal) {
  std::vector<std::uint8_t> bytes;
  if (!ReplayHashJournal_Encode(journal, &bytes)) return 0u;
  std::uint64_t hash = kHashOffset;
  HashBytes(&hash, bytes.data(), bytes.size());
  return hash;
}
