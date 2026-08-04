#include "ActiveWorldSave.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace {

constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint32_t kEngineCompatibilityVersion = 4;
constexpr std::size_t kMaximumFileBytes = 64u * 1024u * 1024u;
constexpr std::size_t kMaximumStringBytes = 4096;
constexpr std::size_t kMaximumMods = 256;
constexpr std::size_t kMaximumSections = 4096;
constexpr std::size_t kMaximumEvents = 65536;
constexpr std::size_t kMaximumPayloadBytes = 16u * 1024u * 1024u;
constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);
const std::uint8_t kMagic[8] = {'R', 'R', '2', 'N', 'W', 'S', 'V', '1'};

void ResetStatus(SActiveWorldSaveStatus* status) {
  if (status == nullptr) return;
  status->error = EActiveWorldSaveError::None;
  status->offset = 0;
  status->detail.clear();
}

bool Fail(SActiveWorldSaveStatus* status, EActiveWorldSaveError error,
          std::size_t offset, const std::string& detail) {
  if (status != nullptr) {
    status->error = error;
    status->offset = offset;
    status->detail = detail;
  }
  return false;
}

std::uint64_t HashBytes(const std::uint8_t* data, std::size_t size) {
  std::uint64_t hash = kFnvOffset;
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= data[index];
    hash *= kFnvPrime;
  }
  return hash;
}

void HashAppend(std::uint64_t* hash, const void* data, std::size_t size) {
  const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= kFnvPrime;
  }
}

void HashU32(std::uint64_t* hash, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    const std::uint8_t byte = static_cast<std::uint8_t>(value >> shift);
    HashAppend(hash, &byte, 1);
  }
}

void HashU64(std::uint64_t* hash, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    const std::uint8_t byte = static_cast<std::uint8_t>(value >> shift);
    HashAppend(hash, &byte, 1);
  }
}

void HashString(std::uint64_t* hash, const std::string& value) {
  HashU32(hash, static_cast<std::uint32_t>(value.size()));
  if (!value.empty()) HashAppend(hash, value.data(), value.size());
}

void HashPayload(std::uint64_t* hash,
                 const std::vector<std::uint8_t>& value) {
  HashU32(hash, static_cast<std::uint32_t>(value.size()));
  if (!value.empty()) HashAppend(hash, value.data(), value.size());
}

bool IsKnownKind(EActiveWorldSectionKind kind) {
  const std::uint32_t value = static_cast<std::uint32_t>(kind);
  return value >= static_cast<std::uint32_t>(
                      EActiveWorldSectionKind::Commander) &&
         value <= static_cast<std::uint32_t>(
                      EActiveWorldSectionKind::Artefact);
}

bool ValidString(const std::string& value, bool allowEmpty) {
  return (allowEmpty || !value.empty()) &&
         value.size() <= kMaximumStringBytes &&
         value.find('\0') == std::string::npos;
}

bool SectionLess(const SActiveWorldSection& left,
                 const SActiveWorldSection& right) {
  if (left.kind != right.kind) {
    return static_cast<std::uint32_t>(left.kind) <
           static_cast<std::uint32_t>(right.kind);
  }
  return left.owner < right.owner;
}

bool ValidateSnapshot(const SActiveWorldSnapshot& snapshot,
                      bool requireFingerprint,
                      SActiveWorldSaveStatus* status) {
  if (snapshot.engineCompatibility != kEngineCompatibilityVersion ||
      snapshot.contentFingerprint == 0 ||
      !std::isfinite(snapshot.simulationTime) ||
      snapshot.simulationTime < 0.0 || !ValidString(snapshot.level, false)) {
    return Fail(status,
                snapshot.engineCompatibility == 0
                    ? EActiveWorldSaveError::InvalidMetadata
                    : EActiveWorldSaveError::UnsupportedEngineCompatibility,
                0, "active-world engine metadata is incompatible");
  }
  if ((snapshot.rngAlgorithm == 0) != snapshot.rngState.empty() ||
      snapshot.rngState.size() > kMaximumPayloadBytes) {
    return Fail(status, EActiveWorldSaveError::InvalidMetadata, 0,
                "active-world RNG identity/state is inconsistent");
  }
  if (snapshot.mods.size() > kMaximumMods ||
      snapshot.sections.size() > kMaximumSections ||
      snapshot.events.size() > kMaximumEvents) {
    return Fail(status, EActiveWorldSaveError::LimitExceeded, 0,
                "active-world collection limit exceeded");
  }
  for (std::size_t index = 0; index < snapshot.mods.size(); ++index) {
    if (!ValidString(snapshot.mods[index], false)) {
      return Fail(status, EActiveWorldSaveError::InvalidMetadata, index,
                  "invalid mod identity");
    }
    if (index != 0 && snapshot.mods[index - 1] >= snapshot.mods[index]) {
      return Fail(status, EActiveWorldSaveError::InvalidOrdering, index,
                  "mod identities must be unique and sorted");
    }
  }
  for (std::size_t index = 0; index < snapshot.sections.size(); ++index) {
    const SActiveWorldSection& section = snapshot.sections[index];
    if (!IsKnownKind(section.kind) || section.schemaVersion == 0 ||
        !ValidString(section.owner, false) ||
        section.payload.size() > kMaximumPayloadBytes) {
      return Fail(status, EActiveWorldSaveError::InvalidSection, index,
                  "invalid active-world section");
    }
    if (index != 0) {
      const SActiveWorldSection& previous = snapshot.sections[index - 1];
      if (!SectionLess(previous, section)) {
        const bool duplicate = previous.kind == section.kind &&
                               previous.owner == section.owner;
        return Fail(status,
                    duplicate ? EActiveWorldSaveError::DuplicateSection
                              : EActiveWorldSaveError::InvalidOrdering,
                    index, duplicate ? "duplicate active-world section"
                                     : "active-world sections are not sorted");
      }
    }
  }
  for (std::size_t index = 0; index < snapshot.events.size(); ++index) {
    const SActiveWorldEvent& event = snapshot.events[index];
    if (event.sequence != index || !std::isfinite(event.timeStamp) ||
        event.timeStamp < 0.0 || !ValidString(event.source, true) ||
        !ValidString(event.destination, false) || event.payloadVersion == 0 ||
        event.payload.size() > kMaximumPayloadBytes) {
      return Fail(status, EActiveWorldSaveError::InvalidEvent, index,
                  "invalid semantic event record");
    }
    if (index != 0 && snapshot.events[index - 1].tick > event.tick) {
      return Fail(status, EActiveWorldSaveError::InvalidOrdering, index,
                  "semantic events are not ordered by tick");
    }
  }
  if (requireFingerprint) {
    const std::uint64_t computed =
        ActiveWorldSave_ComputeWorldFingerprint(snapshot);
    if (snapshot.worldFingerprint == 0 ||
        snapshot.worldFingerprint != computed) {
      return Fail(status, EActiveWorldSaveError::IntegrityMismatch, 0,
                  "active-world fingerprint does not match the model");
    }
  }
  return true;
}

class Writer {
 public:
  explicit Writer(std::vector<std::uint8_t>* bytes) : bytes_(bytes) {}

  void PutU32(std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
      bytes_->push_back(static_cast<std::uint8_t>(value >> shift));
  }

  void PutU64(std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8)
      bytes_->push_back(static_cast<std::uint8_t>(value >> shift));
  }

  void PutDouble(double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double must be 64-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    PutU64(bits);
  }

  void PutString(const std::string& value) {
    PutU32(static_cast<std::uint32_t>(value.size()));
    PutData(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  void PutPayload(const std::vector<std::uint8_t>& value) {
    PutU32(static_cast<std::uint32_t>(value.size()));
    PutU64(HashBytes(value.data(), value.size()));
    PutData(value.data(), value.size());
  }

  void PutData(const std::uint8_t* data, std::size_t size) {
    if (size != 0) bytes_->insert(bytes_->end(), data, data + size);
  }

 private:
  std::vector<std::uint8_t>* bytes_;
};

class Reader {
 public:
  Reader(const std::vector<std::uint8_t>& bytes, std::size_t limit)
      : bytes_(bytes), limit_(limit), offset_(0) {}

  std::size_t offset() const { return offset_; }

  bool GetU32(std::uint32_t* value) {
    if (value == nullptr || Remaining() < 4) return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
      *value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
    return true;
  }

  bool GetU64(std::uint64_t* value) {
    if (value == nullptr || Remaining() < 8) return false;
    *value = 0;
    for (int shift = 0; shift < 64; shift += 8)
      *value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
    return true;
  }

  bool GetDouble(double* value) {
    std::uint64_t bits = 0;
    if (value == nullptr || !GetU64(&bits)) return false;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
  }

  bool GetString(std::string* value, bool allowEmpty) {
    std::uint32_t size = 0;
    if (value == nullptr || !GetU32(&size) || size > kMaximumStringBytes ||
        Remaining() < size)
      return false;
    if (size == 0)
      value->clear();
    else
      value->assign(reinterpret_cast<const char*>(&bytes_[offset_]), size);
    offset_ += size;
    return ValidString(*value, allowEmpty);
  }

  bool GetPayload(std::vector<std::uint8_t>* value) {
    std::uint32_t size = 0;
    std::uint64_t expected = 0;
    if (value == nullptr || !GetU32(&size) ||
        size > kMaximumPayloadBytes || !GetU64(&expected) ||
        Remaining() < size)
      return false;
    value->assign(bytes_.begin() + offset_, bytes_.begin() + offset_ + size);
    offset_ += size;
    return HashBytes(value->data(), value->size()) == expected;
  }

  bool GetMagic() {
    if (Remaining() < sizeof(kMagic)) return false;
    const bool matches =
        std::memcmp(&bytes_[offset_], kMagic, sizeof(kMagic)) == 0;
    offset_ += sizeof(kMagic);
    return matches;
  }

  bool AtEnd() const { return offset_ == limit_; }

 private:
  std::size_t Remaining() const {
    return offset_ <= limit_ ? limit_ - offset_ : 0;
  }

  const std::vector<std::uint8_t>& bytes_;
  std::size_t limit_;
  std::size_t offset_;
};

std::string WindowsError(const char* operation, DWORD code) {
  std::ostringstream stream;
  stream << operation << " failed with Win32 error " << code;
  return stream.str();
}

void PutTrailingHash(std::vector<std::uint8_t>* bytes) {
  const std::uint64_t hash = HashBytes(bytes->data(), bytes->size());
  Writer writer(bytes);
  writer.PutU64(hash);
}

std::uint64_t ReadTrailingHash(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t value = 0;
  const std::size_t start = bytes.size() - 8;
  for (int shift = 0; shift < 64; shift += 8)
    value |= static_cast<std::uint64_t>(bytes[start + shift / 8]) << shift;
  return value;
}

}  // namespace

SActiveWorldSaveStatus::SActiveWorldSaveStatus()
    : error(EActiveWorldSaveError::None), offset(0) {}

SActiveWorldSnapshot::SActiveWorldSnapshot()
    : engineCompatibility(0),
      contentFingerprint(0),
      worldFingerprint(0),
      simulationTick(0),
      simulationTime(0.0),
      rngAlgorithm(0) {}

std::uint32_t ActiveWorldSave_FormatVersion() { return kFormatVersion; }

std::uint32_t ActiveWorldSave_EngineCompatibilityVersion() {
  return kEngineCompatibilityVersion;
}

std::uint64_t ActiveWorldSave_ComputeWorldFingerprint(
    const SActiveWorldSnapshot& snapshot) {
  std::uint64_t hash = kFnvOffset;
  HashU32(&hash, snapshot.engineCompatibility);
  HashU64(&hash, snapshot.contentFingerprint);
  HashU64(&hash, snapshot.simulationTick);
  std::uint64_t timeBits = 0;
  std::memcpy(&timeBits, &snapshot.simulationTime, sizeof(timeBits));
  HashU64(&hash, timeBits);
  HashU32(&hash, snapshot.rngAlgorithm);
  HashPayload(&hash, snapshot.rngState);
  HashString(&hash, snapshot.level);
  HashU32(&hash, static_cast<std::uint32_t>(snapshot.mods.size()));
  for (const std::string& mod : snapshot.mods) HashString(&hash, mod);
  HashU32(&hash, static_cast<std::uint32_t>(snapshot.sections.size()));
  for (const SActiveWorldSection& section : snapshot.sections) {
    HashU32(&hash, static_cast<std::uint32_t>(section.kind));
    HashU32(&hash, section.schemaVersion);
    HashString(&hash, section.owner);
    HashPayload(&hash, section.payload);
  }
  HashU32(&hash, static_cast<std::uint32_t>(snapshot.events.size()));
  for (const SActiveWorldEvent& event : snapshot.events) {
    HashU32(&hash, event.sequence);
    HashU64(&hash, event.tick);
    std::uint64_t eventTimeBits = 0;
    std::memcpy(&eventTimeBits, &event.timeStamp, sizeof(eventTimeBits));
    HashU64(&hash, eventTimeBits);
    HashU32(&hash, static_cast<std::uint32_t>(event.label));
    HashString(&hash, event.source);
    HashString(&hash, event.destination);
    HashU32(&hash, event.payloadVersion);
    HashPayload(&hash, event.payload);
  }
  return hash;
}

bool ActiveWorldSave_Encode(const SActiveWorldSnapshot& snapshot,
                            std::vector<std::uint8_t>* bytes,
                            SActiveWorldSaveStatus* status) {
  ResetStatus(status);
  if (bytes == nullptr)
    return Fail(status, EActiveWorldSaveError::InvalidArgument, 0,
                "output byte vector is null");
  if (!ValidateSnapshot(snapshot, false, status)) return false;

  bytes->clear();
  bytes->reserve(256);
  Writer writer(bytes);
  writer.PutData(kMagic, sizeof(kMagic));
  writer.PutU32(kFormatVersion);
  writer.PutU32(snapshot.engineCompatibility);
  writer.PutU64(snapshot.contentFingerprint);
  writer.PutU64(ActiveWorldSave_ComputeWorldFingerprint(snapshot));
  writer.PutU64(snapshot.simulationTick);
  writer.PutDouble(snapshot.simulationTime);
  writer.PutU32(snapshot.rngAlgorithm);
  writer.PutPayload(snapshot.rngState);
  writer.PutString(snapshot.level);
  writer.PutU32(static_cast<std::uint32_t>(snapshot.mods.size()));
  for (const std::string& mod : snapshot.mods) writer.PutString(mod);
  writer.PutU32(static_cast<std::uint32_t>(snapshot.sections.size()));
  for (const SActiveWorldSection& section : snapshot.sections) {
    writer.PutU32(static_cast<std::uint32_t>(section.kind));
    writer.PutU32(section.schemaVersion);
    writer.PutString(section.owner);
    writer.PutPayload(section.payload);
  }
  writer.PutU32(static_cast<std::uint32_t>(snapshot.events.size()));
  for (const SActiveWorldEvent& event : snapshot.events) {
    writer.PutU32(event.sequence);
    writer.PutU64(event.tick);
    writer.PutDouble(event.timeStamp);
    writer.PutU32(static_cast<std::uint32_t>(event.label));
    writer.PutString(event.source);
    writer.PutString(event.destination);
    writer.PutU32(event.payloadVersion);
    writer.PutPayload(event.payload);
  }
  if (bytes->size() > kMaximumFileBytes - 8) {
    bytes->clear();
    return Fail(status, EActiveWorldSaveError::LimitExceeded, 0,
                "encoded active-world save exceeds the file limit");
  }
  PutTrailingHash(bytes);
  return true;
}

bool ActiveWorldSave_Decode(const std::vector<std::uint8_t>& bytes,
                            SActiveWorldSnapshot* snapshot,
                            SActiveWorldSaveStatus* status) {
  ResetStatus(status);
  if (snapshot == nullptr)
    return Fail(status, EActiveWorldSaveError::InvalidArgument, 0,
                "output snapshot is null");
  if (bytes.size() < sizeof(kMagic) + 4 + 8 ||
      bytes.size() > kMaximumFileBytes)
    return Fail(status, EActiveWorldSaveError::Truncated, bytes.size(),
                "active-world save has an invalid size");
  const std::size_t bodySize = bytes.size() - 8;
  if (HashBytes(bytes.data(), bodySize) != ReadTrailingHash(bytes))
    return Fail(status, EActiveWorldSaveError::IntegrityMismatch, bodySize,
                "active-world container checksum mismatch");

  Reader reader(bytes, bodySize);
  if (!reader.GetMagic())
    return Fail(status, EActiveWorldSaveError::InvalidMagic, 0,
                "active-world magic does not match");
  std::uint32_t format = 0;
  if (!reader.GetU32(&format))
    return Fail(status, EActiveWorldSaveError::Truncated, reader.offset(),
                "active-world format version is truncated");
  if (format != kFormatVersion)
    return Fail(status, EActiveWorldSaveError::UnsupportedFormat,
                reader.offset() - 4,
                "unsupported active-world format version");

  SActiveWorldSnapshot decoded;
  std::uint32_t modCount = 0;
  std::uint32_t sectionCount = 0;
  std::uint32_t eventCount = 0;
  if (!reader.GetU32(&decoded.engineCompatibility) ||
      !reader.GetU64(&decoded.contentFingerprint) ||
      !reader.GetU64(&decoded.worldFingerprint) ||
      !reader.GetU64(&decoded.simulationTick) ||
      !reader.GetDouble(&decoded.simulationTime) ||
      !reader.GetU32(&decoded.rngAlgorithm) ||
      !reader.GetPayload(&decoded.rngState) ||
      !reader.GetString(&decoded.level, false) || !reader.GetU32(&modCount) ||
      modCount > kMaximumMods) {
    return Fail(status, EActiveWorldSaveError::Truncated, reader.offset(),
                "active-world metadata is invalid or truncated");
  }
  for (std::uint32_t index = 0; index < modCount; ++index) {
    std::string mod;
    if (!reader.GetString(&mod, false))
      return Fail(status, EActiveWorldSaveError::Truncated, reader.offset(),
                  "active-world mod identity is truncated");
    decoded.mods.push_back(mod);
  }
  if (!reader.GetU32(&sectionCount) || sectionCount > kMaximumSections)
    return Fail(status, EActiveWorldSaveError::Truncated, reader.offset(),
                "active-world section count is invalid");
  for (std::uint32_t index = 0; index < sectionCount; ++index) {
    std::uint32_t kind = 0;
    SActiveWorldSection section;
    if (!reader.GetU32(&kind) || !reader.GetU32(&section.schemaVersion) ||
        !reader.GetString(&section.owner, false) ||
        !reader.GetPayload(&section.payload)) {
      return Fail(status, EActiveWorldSaveError::InvalidSection,
                  reader.offset(), "active-world section is corrupt");
    }
    section.kind = static_cast<EActiveWorldSectionKind>(kind);
    decoded.sections.push_back(section);
  }
  if (!reader.GetU32(&eventCount) || eventCount > kMaximumEvents)
    return Fail(status, EActiveWorldSaveError::Truncated, reader.offset(),
                "active-world event count is invalid");
  for (std::uint32_t index = 0; index < eventCount; ++index) {
    std::uint32_t label = 0;
    SActiveWorldEvent event;
    if (!reader.GetU32(&event.sequence) || !reader.GetU64(&event.tick) ||
        !reader.GetDouble(&event.timeStamp) || !reader.GetU32(&label) ||
        !reader.GetString(&event.source, true) ||
        !reader.GetString(&event.destination, false) ||
        !reader.GetU32(&event.payloadVersion) ||
        !reader.GetPayload(&event.payload)) {
      return Fail(status, EActiveWorldSaveError::InvalidEvent,
                  reader.offset(), "semantic event record is corrupt");
    }
    event.label = static_cast<std::int32_t>(label);
    decoded.events.push_back(event);
  }
  if (!reader.AtEnd())
    return Fail(status, EActiveWorldSaveError::InvalidMetadata,
                reader.offset(), "active-world save has trailing body data");
  if (!ValidateSnapshot(decoded, true, status)) return false;
  *snapshot = decoded;
  return true;
}

bool ActiveWorldSave_WriteAtomic(const std::wstring& path,
                                 const SActiveWorldSnapshot& snapshot,
                                 SActiveWorldSaveStatus* status) {
  ResetStatus(status);
  if (path.empty())
    return Fail(status, EActiveWorldSaveError::InvalidArgument, 0,
                "active-world save path is empty");
  std::vector<std::uint8_t> bytes;
  if (!ActiveWorldSave_Encode(snapshot, &bytes, status)) return false;

  const std::wstring temporary =
      path + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
      std::to_wstring(GetCurrentThreadId());
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return Fail(status, EActiveWorldSaveError::IoOpenFailed, 0,
                WindowsError("CreateFileW", GetLastError()));

  bool written = true;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const DWORD request = static_cast<DWORD>((std::min)(
        bytes.size() - offset,
        static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    DWORD completed = 0;
    if (!WriteFile(file, bytes.data() + offset, request, &completed, nullptr) ||
        completed == 0) {
      written = false;
      break;
    }
    offset += completed;
  }
  const DWORD writeError = written ? ERROR_SUCCESS : GetLastError();
  const bool flushed = written && FlushFileBuffers(file) != FALSE;
  const DWORD flushError = flushed ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (!written || !flushed) {
    DeleteFileW(temporary.c_str());
    return Fail(status,
                written ? EActiveWorldSaveError::IoFlushFailed
                        : EActiveWorldSaveError::IoWriteFailed,
                offset,
                WindowsError(written ? "FlushFileBuffers" : "WriteFile",
                             written ? flushError : writeError));
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    const DWORD error = GetLastError();
    DeleteFileW(temporary.c_str());
    return Fail(status, EActiveWorldSaveError::AtomicCommitFailed, 0,
                WindowsError("MoveFileExW", error));
  }
  return true;
}

bool ActiveWorldSave_Read(const std::wstring& path,
                          SActiveWorldSnapshot* snapshot,
                          SActiveWorldSaveStatus* status) {
  ResetStatus(status);
  if (path.empty() || snapshot == nullptr)
    return Fail(status, EActiveWorldSaveError::InvalidArgument, 0,
                "active-world read argument is invalid");
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return Fail(status, EActiveWorldSaveError::IoOpenFailed, 0,
                WindowsError("CreateFileW", GetLastError()));
  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size)) {
    const DWORD error = GetLastError();
    CloseHandle(file);
    return Fail(status, EActiveWorldSaveError::IoReadFailed, 0,
                WindowsError("GetFileSizeEx", error));
  }
  if (size.QuadPart <= 0 ||
      static_cast<unsigned long long>(size.QuadPart) > kMaximumFileBytes) {
    CloseHandle(file);
    return Fail(status, EActiveWorldSaveError::LimitExceeded, 0,
                "active-world file size is outside the supported range");
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.QuadPart));
  std::size_t offset = 0;
  bool read = true;
  while (offset < bytes.size()) {
    const DWORD request = static_cast<DWORD>((std::min)(
        bytes.size() - offset,
        static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    DWORD completed = 0;
    if (!ReadFile(file, bytes.data() + offset, request, &completed, nullptr) ||
        completed == 0) {
      read = false;
      break;
    }
    offset += completed;
  }
  const DWORD error = read ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (!read)
    return Fail(status, EActiveWorldSaveError::IoReadFailed, offset,
                WindowsError("ReadFile", error));
  return ActiveWorldSave_Decode(bytes, snapshot, status);
}

bool ActiveWorldSave_RestoreTransactional(
    const SActiveWorldSnapshot& snapshot, IActiveWorldRestoreTarget* target,
    SActiveWorldSaveStatus* status) {
  ResetStatus(status);
  if (target == nullptr)
    return Fail(status, EActiveWorldSaveError::InvalidArgument, 0,
                "active-world restore target is null");
  if (!ValidateSnapshot(snapshot, true, status)) return false;

  std::string failure;
  if (!target->Begin(snapshot, &failure))
    return Fail(status, EActiveWorldSaveError::RestoreBeginFailed, 0,
                failure.empty() ? "restore begin failed" : failure);
  for (std::size_t index = 0; index < snapshot.sections.size(); ++index) {
    if (!target->RestoreOwner(snapshot.sections[index], &failure)) {
      target->Rollback();
      return Fail(status, EActiveWorldSaveError::RestoreOwnerFailed, index,
                  failure.empty() ? "owner restore failed" : failure);
    }
  }
  for (std::size_t index = 0; index < snapshot.sections.size(); ++index) {
    if (!target->ResolveReferences(snapshot.sections[index], &failure)) {
      target->Rollback();
      return Fail(status, EActiveWorldSaveError::RestoreReferenceFailed, index,
                  failure.empty() ? "reference resolution failed" : failure);
    }
  }
  for (std::size_t index = 0; index < snapshot.events.size(); ++index) {
    if (!target->RestoreEvent(snapshot.events[index], &failure)) {
      target->Rollback();
      return Fail(status, EActiveWorldSaveError::RestoreEventFailed, index,
                  failure.empty() ? "event restore failed" : failure);
    }
  }
  if (!target->Validate(snapshot.worldFingerprint, &failure)) {
    target->Rollback();
    return Fail(status, EActiveWorldSaveError::RestoreValidationFailed, 0,
                failure.empty() ? "world validation failed" : failure);
  }
  if (!target->Commit(&failure)) {
    target->Rollback();
    return Fail(status, EActiveWorldSaveError::RestoreCommitFailed, 0,
                failure.empty() ? "restore commit failed" : failure);
  }
  return true;
}
