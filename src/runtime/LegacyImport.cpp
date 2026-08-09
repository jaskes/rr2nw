#include "LegacyImport.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

constexpr std::uint64_t kHashOffset = 14695981039346656037ull;
constexpr std::uint64_t kHashPrime = 1099511628211ull;
constexpr std::uint32_t kLegacyProfile = 1u;
constexpr std::size_t kMaximumFrames = 100000u;
constexpr std::size_t kMaximumFrameBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaximumLegacyBindings = 128u;
constexpr std::size_t kMaximumConfigLines = 512u;
constexpr std::size_t kMaximumConfigLineBytes = 1024u;

struct Frame {
  std::size_t prefix = 0;
  std::size_t payload = 0;
  std::uint32_t size = 0;
};

struct LegacyBinding {
  std::string action;
  std::string key;
};

void Fail(SLegacyImportStatus* status, ELegacyImportError error,
          std::size_t offset, const std::string& detail) {
  if (status == nullptr) return;
  status->error = error;
  status->offset = offset;
  status->detail = detail;
}

void Succeed(SLegacyImportStatus* status) {
  if (status == nullptr) return;
  *status = {};
}

std::uint64_t Fingerprint(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t hash = kHashOffset;
  for (std::uint8_t byte : bytes) {
    hash ^= byte;
    hash *= kHashPrime;
  }
  return hash;
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes,
                      std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
      (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
      (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
      (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

std::int32_t ReadI32(const std::vector<std::uint8_t>& bytes,
                     std::size_t offset) {
  return static_cast<std::int32_t>(ReadU32(bytes, offset));
}

double ReadDouble(const std::vector<std::uint8_t>& bytes,
                  std::size_t offset) {
  std::uint64_t bits = 0;
  for (unsigned int shift = 0; shift < 64u; shift += 8u)
    bits |= static_cast<std::uint64_t>(bytes[offset + shift / 8u]) << shift;
  double value = 0.0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

bool ReadBoundedFile(const std::wstring& path, std::size_t maximum,
                     std::vector<std::uint8_t>* bytes,
                     SLegacyImportStatus* status) {
  if (bytes == nullptr || path.empty() || path.find(L'\0') != std::wstring::npos) {
    Fail(status, ELegacyImportError::FileUnavailable, 0,
         "legacy source path is invalid");
    return false;
  }
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    Fail(status, ELegacyImportError::FileUnavailable, 0,
         "legacy source file is unavailable");
    return false;
  }
  LARGE_INTEGER size = {};
  if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0) {
    CloseHandle(file);
    Fail(status, ELegacyImportError::FileUnavailable, 0,
         "legacy source size is unavailable");
    return false;
  }
  if (size.QuadPart == 0) {
    CloseHandle(file);
    Fail(status, ELegacyImportError::EmptyInput, 0,
         "legacy source is empty");
    return false;
  }
  if (static_cast<unsigned long long>(size.QuadPart) > maximum) {
    CloseHandle(file);
    Fail(status, ELegacyImportError::InputTooLarge, 0,
         "legacy source exceeds the bounded input limit");
    return false;
  }
  std::vector<std::uint8_t> candidate(
      static_cast<std::size_t>(size.QuadPart));
  std::size_t completed = 0;
  while (completed < candidate.size()) {
    const DWORD requested = static_cast<DWORD>((std::min)(
        candidate.size() - completed,
        static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    DWORD read = 0;
    if (ReadFile(file, candidate.data() + completed, requested, &read,
                 nullptr) == FALSE || read == 0u) {
      CloseHandle(file);
      Fail(status, ELegacyImportError::FileUnavailable, completed,
           "legacy source could not be read completely");
      return false;
    }
    completed += read;
  }
  CloseHandle(file);
  *bytes = std::move(candidate);
  return true;
}

bool DecodeFrames(const std::vector<std::uint8_t>& bytes,
                  std::vector<Frame>* frames,
                  SLegacyImportStatus* status) {
  frames->clear();
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    if (bytes.size() - offset < 4u) {
      Fail(status, ELegacyImportError::TruncatedFrame, offset,
           "legacy frame length is truncated");
      return false;
    }
    const std::uint32_t size = ReadU32(bytes, offset);
    if (size > kMaximumFrameBytes) {
      Fail(status, ELegacyImportError::InvalidFrameLength, offset,
           "legacy frame exceeds the bounded record limit");
      return false;
    }
    if (static_cast<std::size_t>(size) > bytes.size() - offset - 4u) {
      Fail(status, ELegacyImportError::TruncatedFrame, offset,
           "legacy frame payload is truncated");
      return false;
    }
    if (frames->size() >= kMaximumFrames) {
      Fail(status, ELegacyImportError::TooManyFrames, offset,
           "legacy frame count exceeds the bounded limit");
      return false;
    }
    frames->push_back({offset, offset + 4u, size});
    offset += 4u + size;
  }
  return true;
}

bool IsPinPrefix(const std::vector<std::uint8_t>& bytes,
                 const Frame& frame, std::int32_t* type) {
  if (frame.size != 8u || bytes[frame.payload] != 'P' ||
      bytes[frame.payload + 1u] != 'I' ||
      bytes[frame.payload + 2u] != 'N' ||
      bytes[frame.payload + 3u] != 0u)
    return false;
  if (type != nullptr) *type = ReadI32(bytes, frame.payload + 4u);
  return true;
}

bool ReadLegacyName(const std::vector<std::uint8_t>& bytes,
                    const Frame& frame, bool allowEmpty,
                    std::string* result) {
  std::size_t length = 0;
  while (length < frame.size && bytes[frame.payload + length] != 0u) {
    const std::uint8_t value = bytes[frame.payload + length];
    if (value < 0x20u) return false;
    ++length;
  }
  if (length == frame.size || (!allowEmpty && length == 0u)) return false;
  result->assign(reinterpret_cast<const char*>(bytes.data() + frame.payload),
                 length);
  return true;
}

bool ContinuationOwnsTable(const std::string& table) {
  static const char* const owned[] = {
      "Commander", "TankGroup", "People", "Tank", "Vehicle",
      "Project", "RecruitCenter", "Bullet", "Explosion", "Spark",
      "Smoke", "Corpse", "Taxi", "Orphan", "Howitzer", "Artefact",
      "Portal"};
  for (const char* candidate : owned)
    if (table == candidate) return true;
  return false;
}

bool ParseUnsigned(const std::string& value, unsigned int* result) {
  if (result == nullptr || value.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
  if (errno != 0 || end == value.c_str() || *end != '\0' ||
      parsed > (std::numeric_limits<unsigned int>::max)())
    return false;
  *result = static_cast<unsigned int>(parsed);
  return true;
}

bool ParseDouble(const std::string& value, double* result) {
  if (result == nullptr || value.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (errno != 0 || end == value.c_str() || *end != '\0' ||
      !std::isfinite(parsed))
    return false;
  *result = parsed;
  return true;
}

bool ParseLegacyBool(const std::string& value, bool* result) {
  double parsed = 0.0;
  if (!ParseDouble(value, &parsed) || (parsed != 0.0 && parsed != 1.0))
    return false;
  *result = parsed != 0.0;
  return true;
}

bool LegacyKey(const std::string& name, std::uint32_t* key) {
  if (key == nullptr || name.empty()) return false;
  if (name.size() == 1u) {
    const unsigned char character = static_cast<unsigned char>(name[0]);
    if ((character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9')) {
      *key = character;
      return true;
    }
  }
  struct NamedKey { const char* name; std::uint32_t key; };
  static const NamedKey keys[] = {
      {"Space", VK_SPACE}, {"Left", VK_LEFT}, {"Right", VK_RIGHT},
      {"Up", VK_UP}, {"Down", VK_DOWN}, {"Ins", VK_INSERT},
      {"Del", VK_DELETE}, {"Home", VK_HOME}, {"End", VK_END},
      // Preserve the archived Hardware table's literal PgDn/PgUp mapping.
      {"PgDn", VK_PRIOR}, {"PgUp", VK_NEXT}, {"Bs", VK_BACK},
      {"Tab", VK_TAB}, {"Enter", VK_RETURN}, {"LShift", VK_LSHIFT},
      {"RShift", VK_RSHIFT}, {"LCtrl", VK_LCONTROL},
      {"RCtrl", VK_RCONTROL}, {"LAlt", VK_LMENU}, {"RAlt", VK_RMENU},
      {"Pause", VK_PAUSE}, {"MouseL", VK_LBUTTON},
      {"MouseR", VK_RBUTTON}, {"Num/", VK_DIVIDE},
      {"Num*", VK_MULTIPLY}, {"Num-", VK_SUBTRACT},
      {"Num+", VK_ADD}, {"NumEnter", VK_RETURN},
      {"NumIns", VK_INSERT}, {"NumDel", VK_DELETE},
      {"[", VK_OEM_4}, {"\\", VK_OEM_5}, {"]", VK_OEM_6},
      {";", VK_OEM_1}, {"'", VK_OEM_7}, {"<", VK_OEM_COMMA},
      {"=", VK_OEM_PLUS}, {"-", VK_OEM_MINUS}, {">", VK_OEM_PERIOD},
      {"/", VK_OEM_2}, {"`", VK_OEM_3}};
  for (const NamedKey& candidate : keys)
    if (name == candidate.name) {
      *key = candidate.key;
      return true;
    }
  if (name.size() >= 2u && name[0] == 'F') {
    unsigned int function = 0;
    if (ParseUnsigned(name.substr(1u), &function) && function >= 1u &&
        function <= 12u) {
      *key = VK_F1 + function - 1u;
      return true;
    }
  }
  if (name.size() == 4u && name.compare(0, 3u, "Num") == 0 &&
      name[3] >= '0' && name[3] <= '9') {
    *key = VK_NUMPAD0 + static_cast<std::uint32_t>(name[3] - '0');
    return true;
  }
  // MouseM has no maintained message owner; Joy1..4 have no admitted backend.
  return false;
}

bool BindingTarget(const std::string& action,
                   std::vector<std::size_t>* targets) {
  targets->clear();
  struct Mapping { const char* action; ERecoveredInputBinding binding; };
  static const Mapping mappings[] = {
      {"Forward", RECOVERED_BIND_MOVE_FORWARD},
      {"Backward", RECOVERED_BIND_MOVE_BACKWARD},
      {"StrafeLeft", RECOVERED_BIND_STRAFE_LEFT},
      {"StrafeRight", RECOVERED_BIND_STRAFE_RIGHT},
      {"StrafeUp", RECOVERED_BIND_MOVE_UP},
      {"StrafeDown", RECOVERED_BIND_MOVE_DOWN},
      {"TurnLeft", RECOVERED_BIND_TURN_LEFT},
      {"TurnRight", RECOVERED_BIND_TURN_RIGHT},
      {"LookUp", RECOVERED_BIND_LOOK_UP},
      {"LookDown", RECOVERED_BIND_LOOK_DOWN},
      {"Jump", RECOVERED_BIND_JUMP},
      {"FireSecondary", RECOVERED_BIND_FIRE_SECONDARY},
      {"StopVehicle", RECOVERED_BIND_STOP_VEHICLE},
      {"ChangeVehicle", RECOVERED_BIND_CHANGE_VEHICLE},
      {"DMap", RECOVERED_BIND_MAP},
      {"DMapScrollLeft", RECOVERED_BIND_MAP_SCROLL_LEFT},
      {"DMapScrollRight", RECOVERED_BIND_MAP_SCROLL_RIGHT},
      {"DMapScrollUp", RECOVERED_BIND_MAP_SCROLL_UP},
      {"DMapScrollDown", RECOVERED_BIND_MAP_SCROLL_DOWN},
      {"DMapToggleFollowMode", RECOVERED_BIND_MAP_TOGGLE_FOLLOW},
      {"DMapNextMission", RECOVERED_BIND_MAP_NEXT_MISSION},
      {"DMapPreviousMission", RECOVERED_BIND_MAP_PREVIOUS_MISSION},
      {"DMapTextBoxUp", RECOVERED_BIND_MAP_TEXT_UP},
      {"DMapTextBoxDown", RECOVERED_BIND_MAP_TEXT_DOWN}};
  if (action == "FirePrimary") {
    targets->push_back(RECOVERED_BIND_FIRE_PRIMARY);
    targets->push_back(RECOVERED_BIND_FIRE_PRIMARY_ALTERNATE);
    return true;
  }
  for (const Mapping& mapping : mappings)
    if (action == mapping.action) {
      targets->push_back(mapping.binding);
      return true;
    }
  return false;
}

bool ProjectBindings(const std::vector<LegacyBinding>& source,
                     SLegacyConfigImport* imported) {
  SRecoveredInputBindings candidate = RecoveredWindowsInput_DefaultBindings();
  std::map<std::size_t, std::vector<std::uint32_t>> projected;
  bool complete = true;
  for (const LegacyBinding& binding : source) {
    std::vector<std::size_t> targets;
    if (!BindingTarget(binding.action, &targets)) {
      ++imported->ignoredBindingRecords;
      continue;
    }
    ++imported->supportedBindingRecords;
    std::uint32_t key = 0;
    if (!LegacyKey(binding.key, &key) || key == 0u || key >= 256u ||
        key == VK_ESCAPE) {
      ++imported->unrepresentableBindingRecords;
      complete = false;
      continue;
    }
    const std::size_t owner = targets.front();
    std::vector<std::uint32_t>& values = projected[owner];
    if (std::find(values.begin(), values.end(), key) == values.end())
      values.push_back(key);
  }
  for (const auto& entry : projected) {
    std::vector<std::size_t> targets;
    if (entry.first == RECOVERED_BIND_FIRE_PRIMARY) {
      targets = {RECOVERED_BIND_FIRE_PRIMARY,
                 RECOVERED_BIND_FIRE_PRIMARY_ALTERNATE};
    } else {
      targets = {entry.first};
    }
    if (entry.second.size() != targets.size()) {
      imported->excessBindingRecords += static_cast<std::uint32_t>(
          entry.second.size() > targets.size()
              ? entry.second.size() - targets.size()
              : targets.size() - entry.second.size());
      complete = false;
      continue;
    }
    for (std::size_t index = 0; index < targets.size(); ++index)
      candidate.key[targets[index]] = entry.second[index];
  }
  std::size_t first = 0;
  std::size_t second = 0;
  if (complete &&
      !RecoveredWindowsInput_ValidateBindings(candidate, &first, &second)) {
    complete = false;
    imported->bindingBoundary =
        "projected controls conflict in the maintained input contexts";
  }
  if (complete) {
    imported->bindings = candidate;
    imported->bindingsProjected = true;
    imported->bindingBoundary =
        "all representable legacy controls projected without cardinality loss";
  } else if (imported->bindingBoundary.empty()) {
    imported->bindings = RecoveredWindowsInput_DefaultBindings();
    imported->bindingsProjected = false;
    imported->bindingBoundary =
        "legacy multi-bind or physical-device controls exceed the maintained schema; current bindings must be retained";
  }
  return true;
}

}  // namespace

bool LegacyImport_InspectSaveBytes(
    const std::vector<std::uint8_t>& bytes,
    SLegacySaveInspection* inspection, SLegacyImportStatus* status) {
  if (inspection == nullptr) {
    Fail(status, ELegacyImportError::InvalidHeader, 0,
         "legacy save result is null");
    return false;
  }
  if (bytes.empty()) {
    Fail(status, ELegacyImportError::EmptyInput, 0,
         "legacy save is empty");
    return false;
  }
  if (bytes.size() > kLegacySaveMaximumBytes) {
    Fail(status, ELegacyImportError::InputTooLarge, 0,
         "legacy save exceeds 128 MiB");
    return false;
  }
  std::vector<Frame> frames;
  if (!DecodeFrames(bytes, &frames, status)) return false;
  static const std::uint32_t prelude[] = {
      19u, 8u, 8u, 32u, 8u, 8u, 4u, 13348u, 4u, 8u, 4u, 8u, 8u,
      8u, 8u, 8u, 8u, 4u, 24u, 24u, 72000u, 24000u, 4u, 5529u,
      11786u};
  if (frames.size() <= sizeof(prelude) / sizeof(prelude[0])) {
    Fail(status, ELegacyImportError::UnsupportedLayout, 0,
         "legacy save has no complete March/May prelude");
    return false;
  }
  for (std::size_t index = 0;
       index < sizeof(prelude) / sizeof(prelude[0]); ++index) {
    if (frames[index].size != prelude[index]) {
      Fail(status, ELegacyImportError::UnsupportedLayout,
           frames[index].prefix,
           "legacy save prelude does not match the installed March/May layout");
      return false;
    }
  }
  static const char signature[] = "Next Worlds";
  if (std::memcmp(bytes.data() + frames[0].payload, signature,
                  sizeof(signature)) != 0) {
    Fail(status, ELegacyImportError::InvalidHeader, frames[0].payload,
         "legacy save header signature is invalid");
    return false;
  }
  std::int32_t prefixType = -1;
  if (!IsPinPrefix(bytes, frames[1], &prefixType) || prefixType != 0) {
    Fail(status, ELegacyImportError::InvalidPrefix, frames[1].payload,
         "legacy save does not begin with one context prefix");
    return false;
  }
  const std::int32_t started = ReadI32(bytes, frames[2].payload);
  const double aspect = ReadDouble(bytes, frames[3].payload);
  const double timerCurrent = ReadDouble(bytes, frames[3].payload + 8u);
  const double timerPause = ReadDouble(bytes, frames[3].payload + 16u);
  const double sessionMoment = ReadDouble(bytes, frames[4].payload);
  const double sessionView = ReadDouble(bytes, frames[5].payload);
  if ((started != 0 && started != 1) || !std::isfinite(aspect) ||
      aspect <= 0.0 || aspect > 100.0 || !std::isfinite(timerCurrent) ||
      !std::isfinite(timerPause) || !std::isfinite(sessionMoment) ||
      sessionMoment < 0.0 || !std::isfinite(sessionView) ||
      sessionView < 0.0) {
    Fail(status, ELegacyImportError::NonFiniteTime, frames[2].payload,
         "legacy context/timer prelude is not finite and bounded");
    return false;
  }
  const std::int32_t level = ReadI32(bytes, frames[6].payload);
  if (level < 0 || level >= 9) {
    Fail(status, ELegacyImportError::InvalidLevel, frames[6].payload,
         "legacy Level index is outside the nine installed worlds");
    return false;
  }

  SLegacySaveInspection candidate;
  candidate.profile = kLegacyProfile;
  candidate.sourceFingerprint = Fingerprint(bytes);
  candidate.sourceBytes = bytes.size();
  candidate.frameCount = static_cast<std::uint32_t>(frames.size());
  candidate.levelIndex = static_cast<std::uint32_t>(level);
  std::map<std::string, std::uint32_t> classes;
  bool finalSeen = false;
  for (std::size_t index = sizeof(prelude) / sizeof(prelude[0]);
       index < frames.size(); ++index) {
    std::int32_t type = -1;
    if (!IsPinPrefix(bytes, frames[index], &type)) continue;
    if (type == 0 || type < 0 || type > 4) {
      Fail(status, ELegacyImportError::InvalidPrefix,
           frames[index].payload, "legacy stream contains an invalid prefix");
      return false;
    }
    if (type == 1) {
      if (index + 1u >= frames.size() || frames[index + 1u].size != 181u) {
        Fail(status, ELegacyImportError::UnsupportedLayout,
             frames[index].payload,
             "legacy event record does not match the installed ABI");
        return false;
      }
      ++candidate.eventCount;
      ++index;
      continue;
    }
    if (type == 2) {
      if (index + 5u >= frames.size() ||
          frames[index + 1u].size != 81u ||
          frames[index + 2u].size != 128u ||
          frames[index + 3u].size != 4u ||
          frames[index + 4u].size != 4u ||
          frames[index + 5u].size != 8u) {
        Fail(status, ELegacyImportError::InvalidObjectHeader,
             frames[index].payload,
             "legacy object header is truncated or ABI-incompatible");
        return false;
      }
      std::string table;
      std::string symbolic;
      if (!ReadLegacyName(bytes, frames[index + 1u], false, &table) ||
          !ReadLegacyName(bytes, frames[index + 2u], false, &symbolic)) {
        Fail(status, ELegacyImportError::InvalidObjectHeader,
             frames[index + 1u].payload,
             "legacy object identity is invalid");
        return false;
      }
      ++classes[table];
      ++candidate.objectCount;
      index += 5u;
      continue;
    }
    if (type == 3) {
      if (index + 1u >= frames.size() || frames[index + 1u].size != 68u) {
        Fail(status, ELegacyImportError::UnsupportedLayout,
             frames[index].payload,
             "legacy Fountain branch record does not match the installed ABI");
        return false;
      }
      ++candidate.branchCount;
      ++index;
      continue;
    }
    if (type == 4) {
      if (finalSeen || index + 1u != frames.size()) {
        Fail(status, ELegacyImportError::TrailingData,
             frames[index].payload,
             "legacy terminator is duplicated or not the final frame");
        return false;
      }
      finalSeen = true;
    }
  }
  if (!finalSeen) {
    Fail(status, ELegacyImportError::MissingTerminator, bytes.size(),
         "legacy save has no final IP_FINITALACOMEDIA marker");
    return false;
  }
  if (candidate.eventCount > 5000u || candidate.objectCount > 5000u ||
      candidate.branchCount > 2000u) {
    Fail(status, ELegacyImportError::UnsupportedLayout, 0,
         "legacy save exceeds recovered pool capacities");
    return false;
  }
  for (const auto& entry : classes) {
    SLegacySaveClassCount row;
    row.table = entry.first;
    row.objects = entry.second;
    row.continuationOwned = ContinuationOwnsTable(row.table);
    candidate.classes.push_back(row);
    if (!row.continuationOwned) {
      candidate.deferredOwnerObjects += row.objects;
      candidate.deferredOwnerTables.push_back(row.table);
    }
  }
  candidate.structurallyValid = true;
  candidate.contentIdentityPresent = false;
  candidate.conversionReady = false;
  std::ostringstream boundary;
  boundary << "legacy save has no content identity";
  if (!candidate.deferredOwnerTables.empty()) {
    boundary << "; deferred LCN1 owners=";
    for (std::size_t index = 0;
         index < candidate.deferredOwnerTables.size(); ++index) {
      if (index != 0u) boundary << ',';
      boundary << candidate.deferredOwnerTables[index];
    }
  }
  candidate.conversionBoundary = boundary.str();
  *inspection = std::move(candidate);
  Succeed(status);
  return true;
}

bool LegacyImport_InspectSaveFile(
    const std::wstring& path, SLegacySaveInspection* inspection,
    SLegacyImportStatus* status) {
  std::vector<std::uint8_t> bytes;
  return ReadBoundedFile(path, kLegacySaveMaximumBytes, &bytes, status) &&
      LegacyImport_InspectSaveBytes(bytes, inspection, status);
}

bool LegacyImport_DecodeConfigBytes(
    const std::vector<std::uint8_t>& bytes,
    SLegacyConfigImport* imported, SLegacyImportStatus* status) {
  if (imported == nullptr) {
    Fail(status, ELegacyImportError::InvalidConfig, 0,
         "legacy config result is null");
    return false;
  }
  if (bytes.empty()) {
    Fail(status, ELegacyImportError::EmptyInput, 0,
         "legacy config is empty");
    return false;
  }
  if (bytes.size() > kLegacyConfigMaximumBytes) {
    Fail(status, ELegacyImportError::InputTooLarge, 0,
         "legacy config exceeds 64 KiB");
    return false;
  }
  if (std::find(bytes.begin(), bytes.end(), 0u) != bytes.end()) {
    Fail(status, ELegacyImportError::EmbeddedNul, 0,
         "legacy config contains an embedded NUL");
    return false;
  }
  const std::string text(bytes.begin(), bytes.end());
  std::istringstream stream(text);
  std::string line;
  bool inSettings = false;
  bool haveSettings = false;
  bool haveBindCount = false;
  bool haveSensitivityX = false;
  bool haveSensitivityY = false;
  bool haveInvertY = false;
  bool haveSound = false;
  bool haveSpatial = false;
  bool haveEngine = false;
  bool haveEngineIntensity = false;
  unsigned int declaredBindings = 0;
  std::map<unsigned int, LegacyBinding> bindings;
  SLegacyConfigImport candidate;
  candidate.profile = kLegacyProfile;
  candidate.sourceFingerprint = Fingerprint(bytes);
  std::size_t lineNumber = 0;
  while (std::getline(stream, line)) {
    ++lineNumber;
    if (lineNumber > kMaximumConfigLines ||
        line.size() > kMaximumConfigLineBytes) {
      Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
           "legacy config line/count limit exceeded");
      return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    if (line.front() == '[') {
      if (line != "[Setings]" || haveSettings) {
        Fail(status, ELegacyImportError::UnsupportedConfig, lineNumber,
             "legacy config section is unsupported");
        return false;
      }
      haveSettings = true;
      inSettings = true;
      continue;
    }
    if (!inSettings) {
      Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
           "legacy config value appears before [Setings]");
      return false;
    }
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos || equals == 0u ||
        equals + 1u >= line.size()) {
      Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
           "legacy config assignment is malformed");
      return false;
    }
    const std::string key = line.substr(0, equals);
    const std::string value = line.substr(equals + 1u);
    if (key.compare(0, 4u, "Bind") == 0 && key.size() > 4u) {
      unsigned int index = 0;
      if (!ParseUnsigned(key.substr(4u), &index) ||
          index >= kMaximumLegacyBindings || bindings.count(index) != 0u) {
        Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
             "legacy binding index is invalid or duplicated");
        return false;
      }
      const std::size_t comma = value.find(',');
      if (comma == std::string::npos || comma == 0u ||
          comma + 1u >= value.size() ||
          value.find(',', comma + 1u) != std::string::npos) {
        Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
             "legacy binding assignment is malformed");
        return false;
      }
      bindings[index] = {value.substr(0, comma), value.substr(comma + 1u)};
      continue;
    }
    if (key == "UserBindsNum") {
      if (haveBindCount || !ParseUnsigned(value, &declaredBindings) ||
          declaredBindings > kMaximumLegacyBindings) {
        Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
             "legacy binding count is invalid or duplicated");
        return false;
      }
      haveBindCount = true;
    } else if (key == "MouseSensX") {
      if (haveSensitivityX || !ParseDouble(value, &candidate.mouseSensitivityX))
        goto invalid_value;
      haveSensitivityX = true;
    } else if (key == "MouseSensY") {
      if (haveSensitivityY || !ParseDouble(value, &candidate.mouseSensitivityY))
        goto invalid_value;
      haveSensitivityY = true;
    } else if (key == "MouseInvY") {
      if (haveInvertY || !ParseLegacyBool(value, &candidate.mouseInvertY))
        goto invalid_value;
      haveInvertY = true;
    } else if (key == "Sound") {
      if (haveSound || !ParseLegacyBool(value, &candidate.soundEnabled))
        goto invalid_value;
      haveSound = true;
    } else if (key == "3DrawSound") {
      if (haveSpatial ||
          !ParseLegacyBool(value, &candidate.spatialSoundEnabled))
        goto invalid_value;
      haveSpatial = true;
    } else if (key == "EngineSound") {
      if (haveEngine || !ParseLegacyBool(value, &candidate.engineSoundEnabled))
        goto invalid_value;
      haveEngine = true;
    } else if (key == "EngineIntensity") {
      if (haveEngineIntensity ||
          !ParseDouble(value, &candidate.engineIntensity))
        goto invalid_value;
      haveEngineIntensity = true;
    } else {
      ++candidate.ignoredSettings;
    }
    continue;
invalid_value:
    Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
         "legacy config value is invalid or duplicated");
    return false;
  }
  if (!haveSettings || !haveBindCount || !haveSensitivityX ||
      !haveSensitivityY || !haveInvertY || !haveSound || !haveSpatial ||
      !haveEngine || !haveEngineIntensity ||
      bindings.size() != declaredBindings) {
    Fail(status, ELegacyImportError::InvalidConfig, lineNumber,
         "legacy config is missing a required bounded field");
    return false;
  }
  for (unsigned int index = 0; index < declaredBindings; ++index)
    if (bindings.count(index) == 0u) {
      Fail(status, ELegacyImportError::InvalidConfig, index,
           "legacy binding indices are not contiguous");
      return false;
    }
  if (candidate.mouseSensitivityX < 0.01 ||
      candidate.mouseSensitivityX > 1.01 ||
      candidate.mouseSensitivityY < 0.01 ||
      candidate.mouseSensitivityY > 1.01 ||
      candidate.engineIntensity < 0.0 || candidate.engineIntensity > 1.0) {
    Fail(status, ELegacyImportError::InvalidConfig, 0,
         "legacy sensitivity or engine intensity is out of range");
    return false;
  }
  std::vector<LegacyBinding> ordered;
  ordered.reserve(bindings.size());
  for (unsigned int index = 0; index < declaredBindings; ++index)
    ordered.push_back(bindings[index]);
  candidate.sourceBindings = declaredBindings;
  ProjectBindings(ordered, &candidate);
  candidate.effectsVolume = candidate.soundEnabled ? 1.0 : 0.0;
  candidate.cinematicVolume = candidate.soundEnabled ? 1.0 : 0.0;
  candidate.vehicleVolume =
      candidate.soundEnabled && candidate.engineSoundEnabled
          ? candidate.engineIntensity : 0.0;
  *imported = std::move(candidate);
  Succeed(status);
  return true;
}

bool LegacyImport_ReadConfigFile(
    const std::wstring& path, SLegacyConfigImport* imported,
    SLegacyImportStatus* status) {
  std::vector<std::uint8_t> bytes;
  return ReadBoundedFile(path, kLegacyConfigMaximumBytes, &bytes, status) &&
      LegacyImport_DecodeConfigBytes(bytes, imported, status);
}

const char* LegacyImport_ErrorName(ELegacyImportError error) {
  switch (error) {
    case ELegacyImportError::None: return "none";
    case ELegacyImportError::FileUnavailable: return "file-unavailable";
    case ELegacyImportError::EmptyInput: return "empty-input";
    case ELegacyImportError::InputTooLarge: return "input-too-large";
    case ELegacyImportError::EmbeddedNul: return "embedded-nul";
    case ELegacyImportError::TruncatedFrame: return "truncated-frame";
    case ELegacyImportError::InvalidFrameLength: return "invalid-frame-length";
    case ELegacyImportError::TooManyFrames: return "too-many-frames";
    case ELegacyImportError::InvalidHeader: return "invalid-header";
    case ELegacyImportError::UnsupportedLayout: return "unsupported-layout";
    case ELegacyImportError::InvalidPrefix: return "invalid-prefix";
    case ELegacyImportError::InvalidLevel: return "invalid-level";
    case ELegacyImportError::NonFiniteTime: return "nonfinite-time";
    case ELegacyImportError::InvalidObjectHeader: return "invalid-object-header";
    case ELegacyImportError::MissingTerminator: return "missing-terminator";
    case ELegacyImportError::TrailingData: return "trailing-data";
    case ELegacyImportError::InvalidConfig: return "invalid-config";
    case ELegacyImportError::UnsupportedConfig: return "unsupported-config";
    case ELegacyImportError::AtomicCommitFailed: return "atomic-commit-failed";
  }
  return "unknown";
}
