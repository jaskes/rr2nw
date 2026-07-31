#include "LevelSaveSlot.h"

#include "ActiveWorldSave.h"
#include "LevelContinuation.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace {

constexpr std::uint32_t kFormatVersion = 1u;
constexpr std::uint32_t kSlotCount = 8u;
constexpr std::size_t kMaximumTitleBytes = 96u;
constexpr std::size_t kMaximumDescriptionBytes = 1024u;
constexpr std::size_t kMaximumLevelBytes = 512u;
constexpr std::size_t kMaximumPreviewBytes = 8u * 1024u * 1024u;
constexpr std::size_t kMaximumContinuationBytes = 128u * 1024u * 1024u;
constexpr std::size_t kMaximumArchiveBytes = 137u * 1024u * 1024u;
constexpr std::uint64_t kHashOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kHashPrime = UINT64_C(1099511628211);
const std::uint8_t kMagic[8] = {'R', 'R', '2', 'S', 'L', 'O', 'T', '1'};
const std::uint8_t kPngSignature[8] =
    {0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au};

void ResetStatus(SLevelSaveSlotStatus* status) {
  if (status == nullptr) return;
  *status = {};
}

bool Fail(SLevelSaveSlotStatus* status, ELevelSaveSlotError error,
          std::size_t offset, const std::string& detail) {
  if (status != nullptr) {
    status->error = error;
    status->offset = offset;
    status->detail = detail;
  }
  return false;
}

std::string WindowsError(const char* operation, DWORD error) {
  std::ostringstream stream;
  stream << operation << " failed with Win32 error " << error;
  return stream.str();
}

bool IsValidUtf8(const std::string& value) {
  std::size_t offset = 0;
  while (offset < value.size()) {
    const std::uint8_t lead =
        static_cast<std::uint8_t>(value[offset++]);
    if (lead == 0u) return false;
    if (lead < 0x80u) continue;

    std::uint32_t codePoint = 0;
    std::size_t continuationBytes = 0;
    if (lead >= 0xc2u && lead <= 0xdfu) {
      codePoint = lead & 0x1fu;
      continuationBytes = 1;
    } else if (lead >= 0xe0u && lead <= 0xefu) {
      codePoint = lead & 0x0fu;
      continuationBytes = 2;
    } else if (lead >= 0xf0u && lead <= 0xf4u) {
      codePoint = lead & 0x07u;
      continuationBytes = 3;
    } else {
      return false;
    }
    if (offset > value.size() ||
        value.size() - offset < continuationBytes)
      return false;
    for (std::size_t index = 0; index < continuationBytes; ++index) {
      const std::uint8_t next =
          static_cast<std::uint8_t>(value[offset++]);
      if ((next & 0xc0u) != 0x80u) return false;
      codePoint = (codePoint << 6u) | (next & 0x3fu);
    }
    if ((continuationBytes == 2u && codePoint < 0x800u) ||
        (continuationBytes == 3u && codePoint < 0x10000u) ||
        codePoint > 0x10ffffu ||
        (codePoint >= 0xd800u && codePoint <= 0xdfffu))
      return false;
  }
  return true;
}

bool HasPngSignature(const std::vector<std::uint8_t>& bytes) {
  return bytes.empty() ||
         (bytes.size() >= sizeof(kPngSignature) &&
          std::equal(kPngSignature, kPngSignature + sizeof(kPngSignature),
                     bytes.begin()));
}

void HashBytes(std::uint64_t* hash, const std::uint8_t* bytes,
               std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    *hash ^= bytes[index];
    *hash *= kHashPrime;
  }
}

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
  void Blob(const void* data, std::size_t count) {
    const std::uint8_t* begin = static_cast<const std::uint8_t*>(data);
    bytes->insert(bytes->end(), begin, begin + count);
  }
  void String(const std::string& value) {
    if (!value.empty()) Blob(value.data(), value.size());
  }
  void Bytes(const std::vector<std::uint8_t>& value) {
    if (!value.empty()) Blob(value.data(), value.size());
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
  bool Blob(std::size_t count, std::vector<std::uint8_t>* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < count)
      return false;
    value->assign(bytes.begin() + offset, bytes.begin() + offset + count);
    offset += count;
    return true;
  }
  bool String(std::size_t count, std::string* value) {
    if (value == nullptr || offset > bytes.size() ||
        bytes.size() - offset < count)
      return false;
    value->assign(reinterpret_cast<const char*>(bytes.data() + offset),
                  count);
    offset += count;
    return true;
  }
};

bool InspectContinuation(const std::vector<std::uint8_t>& bytes,
                         SActiveWorldSnapshot* world,
                         std::uint64_t* continuationFingerprint,
                         SLevelSaveSlotStatus* status) {
  SLevelContinuation continuation;
  if (world == nullptr || continuationFingerprint == nullptr ||
      !LevelContinuation_Decode(bytes, &continuation))
    return Fail(status, ELevelSaveSlotError::InvalidContinuation, 0,
                "save slot does not contain a valid LCN1 continuation");
  SActiveWorldSaveStatus worldStatus;
  if (!ActiveWorldSave_Decode(continuation.activeWorld, world,
                              &worldStatus))
    return Fail(status, ELevelSaveSlotError::InvalidContinuation,
                worldStatus.offset, worldStatus.detail);
  *continuationFingerprint = continuation.fingerprint;
  return true;
}

bool ValidateMetadata(const SLevelSaveSlot& archive,
                      bool requireFingerprint,
                      SLevelSaveSlotStatus* status) {
  if (archive.slot >= kSlotCount)
    return Fail(status, ELevelSaveSlotError::InvalidSlot, 0,
                "save slot index is outside 0..7");
  if (archive.savedAtUnixSeconds == 0 || archive.title.empty() ||
      archive.title.size() > kMaximumTitleBytes ||
      archive.description.size() > kMaximumDescriptionBytes ||
      archive.level.empty() || archive.level.size() > kMaximumLevelBytes ||
      !IsValidUtf8(archive.title) ||
      !IsValidUtf8(archive.description) ||
      !IsValidUtf8(archive.level) ||
      archive.contentFingerprint == 0 || archive.worldFingerprint == 0 ||
      archive.continuationFingerprint == 0 ||
      !std::isfinite(archive.simulationTime) ||
      archive.simulationTime < 0.0)
    return Fail(status, ELevelSaveSlotError::InvalidMetadata, 0,
                "save slot metadata is invalid or exceeds its bound");
  if (archive.previewPng.size() > kMaximumPreviewBytes ||
      !HasPngSignature(archive.previewPng))
    return Fail(status, ELevelSaveSlotError::InvalidMetadata, 0,
                "save slot preview is not an admitted bounded PNG");
  if (archive.continuation.empty() ||
      archive.continuation.size() > kMaximumContinuationBytes)
    return Fail(status, ELevelSaveSlotError::LimitExceeded, 0,
                "save slot continuation is outside its size bound");
  if (requireFingerprint && archive.archiveFingerprint == 0)
    return Fail(status, ELevelSaveSlotError::InvalidMetadata, 0,
                "save slot archive fingerprint is missing");

  SActiveWorldSnapshot world;
  std::uint64_t continuationFingerprint = 0;
  if (!InspectContinuation(archive.continuation, &world,
                           &continuationFingerprint, status))
    return false;
  if (archive.level != world.level ||
      archive.contentFingerprint != world.contentFingerprint ||
      archive.worldFingerprint != world.worldFingerprint ||
      archive.continuationFingerprint != continuationFingerprint ||
      archive.simulationTick != world.simulationTick ||
      archive.simulationTime != world.simulationTime)
    return Fail(status, ELevelSaveSlotError::MetadataMismatch, 0,
                "save slot metadata does not match its LCN1 payload");
  return true;
}

bool EnsureDirectory(const std::wstring& directory,
                     SLevelSaveSlotStatus* status) {
  const DWORD attributes = GetFileAttributesW(directory.c_str());
  if (attributes != INVALID_FILE_ATTRIBUTES)
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
           Fail(status, ELevelSaveSlotError::IoDirectoryFailed, 0,
                "save slot path exists but is not a directory");
  if (CreateDirectoryW(directory.c_str(), nullptr) != FALSE)
    return true;
  const DWORD error = GetLastError();
  if (error == ERROR_ALREADY_EXISTS) return true;
  return Fail(status, ELevelSaveSlotError::IoDirectoryFailed, 0,
              WindowsError("CreateDirectoryW", error));
}

}  // namespace

std::uint32_t LevelSaveSlot_FormatVersion() { return kFormatVersion; }

std::uint32_t LevelSaveSlot_Count() { return kSlotCount; }

std::wstring LevelSaveSlot_Path(const std::wstring& directory,
                                std::uint32_t slot) {
  if (directory.empty() || slot >= kSlotCount) return std::wstring();
  std::wstring path = directory;
  if (path.back() != L'\\' && path.back() != L'/') path.push_back(L'\\');
  path += L"Slot";
  path += std::to_wstring(slot);
  path += L".rr2save";
  return path;
}

bool LevelSaveSlot_Create(
    std::uint32_t slot, std::uint64_t savedAtUnixSeconds,
    const std::string& title, const std::string& description,
    const std::vector<std::uint8_t>& previewPng,
    const std::vector<std::uint8_t>& continuation,
    SLevelSaveSlot* archive, SLevelSaveSlotStatus* status) {
  ResetStatus(status);
  if (archive == nullptr)
    return Fail(status, ELevelSaveSlotError::InvalidArgument, 0,
                "save slot output is null");
  SActiveWorldSnapshot world;
  std::uint64_t continuationFingerprint = 0;
  if (!InspectContinuation(continuation, &world,
                           &continuationFingerprint, status))
    return false;
  SLevelSaveSlot candidate;
  candidate.slot = slot;
  candidate.savedAtUnixSeconds = savedAtUnixSeconds;
  candidate.title = title;
  candidate.description = description;
  candidate.level = world.level;
  candidate.contentFingerprint = world.contentFingerprint;
  candidate.worldFingerprint = world.worldFingerprint;
  candidate.continuationFingerprint = continuationFingerprint;
  candidate.simulationTick = world.simulationTick;
  candidate.simulationTime = world.simulationTime;
  candidate.previewPng = previewPng;
  candidate.continuation = continuation;
  if (!ValidateMetadata(candidate, false, status)) return false;

  std::vector<std::uint8_t> encoded;
  if (!LevelSaveSlot_Encode(candidate, &encoded, status)) return false;
  SLevelSaveSlot canonical;
  if (!LevelSaveSlot_Decode(encoded, &canonical, status)) return false;
  *archive = canonical;
  return true;
}

bool LevelSaveSlot_Encode(const SLevelSaveSlot& archive,
                          std::vector<std::uint8_t>* bytes,
                          SLevelSaveSlotStatus* status) {
  ResetStatus(status);
  if (bytes == nullptr)
    return Fail(status, ELevelSaveSlotError::InvalidArgument, 0,
                "save slot byte output is null");
  if (!ValidateMetadata(archive, false, status)) return false;

  std::uint64_t simulationTimeBits = 0;
  static_assert(sizeof(simulationTimeBits) == sizeof(archive.simulationTime),
                "double must be 64-bit");
  std::memcpy(&simulationTimeBits, &archive.simulationTime,
              sizeof(simulationTimeBits));

  std::vector<std::uint8_t> encoded;
  encoded.reserve(92u + archive.title.size() + archive.description.size() +
                  archive.level.size() + archive.previewPng.size() +
                  archive.continuation.size());
  Writer writer = {&encoded};
  writer.Blob(kMagic, sizeof(kMagic));
  writer.U32(kFormatVersion);
  writer.U32(archive.slot);
  writer.U64(archive.savedAtUnixSeconds);
  writer.U64(archive.contentFingerprint);
  writer.U64(archive.worldFingerprint);
  writer.U64(archive.continuationFingerprint);
  writer.U64(archive.simulationTick);
  writer.U64(simulationTimeBits);
  writer.U32(static_cast<std::uint32_t>(archive.title.size()));
  writer.U32(static_cast<std::uint32_t>(archive.description.size()));
  writer.U32(static_cast<std::uint32_t>(archive.level.size()));
  writer.U32(static_cast<std::uint32_t>(archive.previewPng.size()));
  writer.U32(static_cast<std::uint32_t>(archive.continuation.size()));
  writer.String(archive.title);
  writer.String(archive.description);
  writer.String(archive.level);
  writer.Bytes(archive.previewPng);
  writer.Bytes(archive.continuation);
  std::uint64_t fingerprint = kHashOffset;
  HashBytes(&fingerprint, encoded.data(), encoded.size());
  if (archive.archiveFingerprint != 0 &&
      archive.archiveFingerprint != fingerprint)
    return Fail(status, ELevelSaveSlotError::IntegrityMismatch,
                encoded.size(),
                "save slot object was modified after canonical decode");
  writer.U64(fingerprint);
  if (encoded.size() > kMaximumArchiveBytes)
    return Fail(status, ELevelSaveSlotError::LimitExceeded, encoded.size(),
                "encoded save slot exceeds the archive bound");
  *bytes = std::move(encoded);
  return true;
}

bool LevelSaveSlot_Decode(const std::vector<std::uint8_t>& bytes,
                          SLevelSaveSlot* archive,
                          SLevelSaveSlotStatus* status) {
  ResetStatus(status);
  if (archive == nullptr)
    return Fail(status, ELevelSaveSlotError::InvalidArgument, 0,
                "save slot decode output is null");
  if (bytes.size() > kMaximumArchiveBytes)
    return Fail(status, ELevelSaveSlotError::LimitExceeded, bytes.size(),
                "save slot archive exceeds its size bound");
  if (bytes.size() < 92u)
    return Fail(status, ELevelSaveSlotError::InvalidMagic, 0,
                "save slot archive is too short");

  Reader reader = {bytes};
  std::vector<std::uint8_t> magic;
  std::uint32_t version = 0;
  std::uint64_t simulationTimeBits = 0;
  std::uint32_t titleBytes = 0;
  std::uint32_t descriptionBytes = 0;
  std::uint32_t levelBytes = 0;
  std::uint32_t previewBytes = 0;
  std::uint32_t continuationBytes = 0;
  SLevelSaveSlot candidate;
  if (!reader.Blob(sizeof(kMagic), &magic) ||
      !std::equal(kMagic, kMagic + sizeof(kMagic), magic.begin()))
    return Fail(status, ELevelSaveSlotError::InvalidMagic, 0,
                "save slot magic is not RR2SLOT1");
  if (!reader.U32(&version))
    return Fail(status, ELevelSaveSlotError::InvalidMetadata,
                reader.offset, "save slot version is truncated");
  if (version != kFormatVersion)
    return Fail(status, ELevelSaveSlotError::UnsupportedFormat,
                reader.offset - 4u, "save slot format version is unsupported");
  if (!reader.U32(&candidate.slot) ||
      !reader.U64(&candidate.savedAtUnixSeconds) ||
      !reader.U64(&candidate.contentFingerprint) ||
      !reader.U64(&candidate.worldFingerprint) ||
      !reader.U64(&candidate.continuationFingerprint) ||
      !reader.U64(&candidate.simulationTick) ||
      !reader.U64(&simulationTimeBits) ||
      !reader.U32(&titleBytes) ||
      !reader.U32(&descriptionBytes) ||
      !reader.U32(&levelBytes) ||
      !reader.U32(&previewBytes) ||
      !reader.U32(&continuationBytes))
    return Fail(status, ELevelSaveSlotError::InvalidMetadata,
                reader.offset, "save slot header is truncated");
  if (titleBytes == 0u || titleBytes > kMaximumTitleBytes ||
      descriptionBytes > kMaximumDescriptionBytes ||
      levelBytes == 0u || levelBytes > kMaximumLevelBytes ||
      previewBytes > kMaximumPreviewBytes ||
      continuationBytes == 0u ||
      continuationBytes > kMaximumContinuationBytes)
    return Fail(status, ELevelSaveSlotError::LimitExceeded, reader.offset,
                "save slot payload lengths exceed their bounds");
  std::memcpy(&candidate.simulationTime, &simulationTimeBits,
              sizeof(candidate.simulationTime));
  if (!reader.String(titleBytes, &candidate.title) ||
      !reader.String(descriptionBytes, &candidate.description) ||
      !reader.String(levelBytes, &candidate.level) ||
      !reader.Blob(previewBytes, &candidate.previewPng) ||
      !reader.Blob(continuationBytes, &candidate.continuation))
    return Fail(status, ELevelSaveSlotError::InvalidMetadata, reader.offset,
                "save slot body is truncated");
  const std::size_t fingerprintOffset = reader.offset;
  if (!reader.U64(&candidate.archiveFingerprint) ||
      reader.offset != bytes.size())
    return Fail(status, ELevelSaveSlotError::InvalidMetadata, reader.offset,
                "save slot fingerprint or body length is invalid");
  std::uint64_t computedFingerprint = kHashOffset;
  HashBytes(&computedFingerprint, bytes.data(), fingerprintOffset);
  if (candidate.archiveFingerprint == 0 ||
      candidate.archiveFingerprint != computedFingerprint)
    return Fail(status, ELevelSaveSlotError::IntegrityMismatch,
                fingerprintOffset, "save slot integrity fingerprint differs");
  if (!ValidateMetadata(candidate, true, status)) return false;
  *archive = std::move(candidate);
  return true;
}

bool LevelSaveSlot_Summarize(const SLevelSaveSlot& archive,
                             SLevelSaveSlotSummary* summary,
                             SLevelSaveSlotStatus* status) {
  ResetStatus(status);
  if (summary == nullptr)
    return Fail(status, ELevelSaveSlotError::InvalidArgument, 0,
                "save slot summary output is null");
  if (!ValidateMetadata(archive, true, status)) return false;
  std::vector<std::uint8_t> bytes;
  if (!LevelSaveSlot_Encode(archive, &bytes, status)) return false;
  SLevelSaveSlotSummary candidate;
  candidate.ready = true;
  candidate.slot = archive.slot;
  candidate.savedAtUnixSeconds = archive.savedAtUnixSeconds;
  candidate.title = archive.title;
  candidate.description = archive.description;
  candidate.level = archive.level;
  candidate.contentFingerprint = archive.contentFingerprint;
  candidate.worldFingerprint = archive.worldFingerprint;
  candidate.continuationFingerprint = archive.continuationFingerprint;
  candidate.archiveFingerprint = archive.archiveFingerprint;
  candidate.simulationTick = archive.simulationTick;
  candidate.simulationTime = archive.simulationTime;
  candidate.previewBytes = archive.previewPng.size();
  candidate.continuationBytes = archive.continuation.size();
  candidate.archiveBytes = bytes.size();
  *summary = candidate;
  return true;
}

bool LevelSaveSlot_WriteAtomic(const std::wstring& directory,
                               const SLevelSaveSlot& archive,
                               SLevelSaveSlotStatus* status) {
  ResetStatus(status);
  const std::wstring path = LevelSaveSlot_Path(directory, archive.slot);
  if (path.empty())
    return Fail(status, ELevelSaveSlotError::InvalidArgument, 0,
                "save slot directory or index is invalid");
  std::vector<std::uint8_t> bytes;
  if (!LevelSaveSlot_Encode(archive, &bytes, status)) return false;
  if (!EnsureDirectory(directory, status)) return false;

  static volatile LONG temporarySequence = 0;
  const LONG sequence = InterlockedIncrement(&temporarySequence);
  const std::wstring temporary =
      path + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
      std::to_wstring(GetCurrentThreadId()) + L"-" +
      std::to_wstring(sequence);
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return Fail(status, ELevelSaveSlotError::IoOpenFailed, 0,
                WindowsError("CreateFileW", GetLastError()));

  bool written = true;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const DWORD request = static_cast<DWORD>((std::min)(
        bytes.size() - offset,
        static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    DWORD completed = 0;
    if (!WriteFile(file, bytes.data() + offset, request, &completed,
                   nullptr) ||
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
                written ? ELevelSaveSlotError::IoFlushFailed
                        : ELevelSaveSlotError::IoWriteFailed,
                offset,
                WindowsError(written ? "FlushFileBuffers" : "WriteFile",
                             written ? flushError : writeError));
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    const DWORD error = GetLastError();
    DeleteFileW(temporary.c_str());
    return Fail(status, ELevelSaveSlotError::AtomicCommitFailed, 0,
                WindowsError("MoveFileExW", error));
  }
  return true;
}

bool LevelSaveSlot_Read(const std::wstring& directory, std::uint32_t slot,
                        SLevelSaveSlot* archive,
                        SLevelSaveSlotStatus* status) {
  ResetStatus(status);
  const std::wstring path = LevelSaveSlot_Path(directory, slot);
  if (path.empty() || archive == nullptr)
    return Fail(status, ELevelSaveSlotError::InvalidArgument, 0,
                "save slot read argument is invalid");
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return Fail(status, ELevelSaveSlotError::IoOpenFailed, 0,
                WindowsError("CreateFileW", GetLastError()));
  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size)) {
    const DWORD error = GetLastError();
    CloseHandle(file);
    return Fail(status, ELevelSaveSlotError::IoReadFailed, 0,
                WindowsError("GetFileSizeEx", error));
  }
  if (size.QuadPart <= 0 ||
      static_cast<unsigned long long>(size.QuadPart) >
          kMaximumArchiveBytes) {
    CloseHandle(file);
    return Fail(status, ELevelSaveSlotError::LimitExceeded, 0,
                "save slot file size is outside the supported range");
  }
  std::vector<std::uint8_t> bytes(
      static_cast<std::size_t>(size.QuadPart));
  std::size_t offset = 0;
  bool read = true;
  while (offset < bytes.size()) {
    const DWORD request = static_cast<DWORD>((std::min)(
        bytes.size() - offset,
        static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    DWORD completed = 0;
    if (!ReadFile(file, bytes.data() + offset, request, &completed,
                  nullptr) ||
        completed == 0) {
      read = false;
      break;
    }
    offset += completed;
  }
  const DWORD error = read ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (!read)
    return Fail(status, ELevelSaveSlotError::IoReadFailed, offset,
                WindowsError("ReadFile", error));
  SLevelSaveSlot candidate;
  if (!LevelSaveSlot_Decode(bytes, &candidate, status)) return false;
  if (candidate.slot != slot)
    return Fail(status, ELevelSaveSlotError::MetadataMismatch, 0,
                "save slot file contains a different slot index");
  *archive = std::move(candidate);
  return true;
}
