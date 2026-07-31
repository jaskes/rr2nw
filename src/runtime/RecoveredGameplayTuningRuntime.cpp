#include "RecoveredGameplayTuningRuntime.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

class CGRPanel;
#include "h/vehicle.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "obase/bullet/BulletAttributeState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/vehicle/VehicleAttributeState.h"
#include "obase/vehicle/VehicleGameplayTuning.h"

#include "RecoveredModRuntime.h"

namespace {

constexpr const char kTuningTarget[] = "RR2NW/gameplay-tuning.json";
constexpr long kMaximumTuningBytes = 256 * 1024;
constexpr int kSchemaVersion = 1;
constexpr std::size_t kMaximumEntries = 64;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

struct VehiclePatch {
  std::string id;
  bool hasMaxSpeed = false;
  bool hasReverseSpeed = false;
  bool hasAccelerationTime = false;
  bool hasTurnSpeed = false;
  bool hasPrimaryFireInterval = false;
  bool hasSecondaryFireInterval = false;
  bool hasSecondaryProjectile = false;
  bool hasDamagePower = false;
  double maxSpeed = 0.0;
  double reverseSpeed = 0.0;
  double accelerationTime = 0.0;
  double turnSpeed = 0.0;
  double primaryFireInterval = 0.0;
  double secondaryFireInterval = 0.0;
  std::string secondaryProjectile;
  double damagePower = 0.0;
};

struct ProjectilePatch {
  std::string id;
  bool hasSpeed = false;
  double speed = 0.0;
};

struct Document {
  int schema = 0;
  std::vector<VehiclePatch> vehicles;
  std::vector<ProjectilePatch> projectiles;
};

struct VehicleRollback {
  AttributeVehicle* attribute = nullptr;
  double primaryFireInterval = 0.0;
  double secondaryFireInterval = 0.0;
  double damagePower = 0.0;
  bool secondaryProjectilePatched = false;
  ct_AttrStr secondaryProjectile = {};
  AttributeBullet* expectedSecondaryProjectile = nullptr;
  bool vesselCaptured = false;
  SVehicleVesselGameplayState vessel = {};
};

struct ProjectileRollback {
  AttributeBullet* attribute = nullptr;
  double speed = 0.0;
};

SRecoveredGameplayTuningSummary g_summary;
std::vector<VehicleRollback> g_vehicleRollback;
std::vector<ProjectileRollback> g_projectileRollback;
SimulationContext* g_context = nullptr;
unsigned int g_issues = 0;
char g_lastError[512] = {};
bool g_active = false;
bool g_vehicleReferencesFinalized = false;

void Fail(unsigned int issue, const std::string& message) {
  g_issues |= issue;
  std::snprintf(g_lastError, sizeof(g_lastError), "%s", message.c_str());
}

std::string FoldAscii(std::string value) {
  for (char& character : value) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (byte >= 'A' && byte <= 'Z')
      character = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

bool ValidIdentifier(const std::string& value) {
  if (value.empty() || value.size() > 63) return false;
  for (char character : value) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (!((byte >= 'A' && byte <= 'Z') ||
          (byte >= 'a' && byte <= 'z') ||
          (byte >= '0' && byte <= '9') || byte == '.' || byte == '_' ||
          byte == '-'))
      return false;
  }
  return true;
}

bool InRange(double value, double minimum, double maximum) {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

class JsonCursor {
 public:
  explicit JsonCursor(const std::string& text) : text_(text) {}

  bool Consume(char expected) {
    Space();
    if (position_ >= text_.size() || text_[position_] != expected)
      return Error("expected JSON punctuation");
    ++position_;
    return true;
  }

  bool TryConsume(char value) {
    Space();
    if (position_ >= text_.size() || text_[position_] != value) return false;
    ++position_;
    return true;
  }

  bool Integer(int* result) {
    double value = 0.0;
    const std::size_t begin = position_;
    if (!Number(&value)) return false;
    if (std::floor(value) != value || value < -2147483648.0 ||
        value > 2147483647.0) {
      position_ = begin;
      return Error("expected bounded JSON integer");
    }
    *result = static_cast<int>(value);
    return true;
  }

  bool Number(double* result) {
    Space();
    const std::size_t begin = position_;
    if (position_ < text_.size() && text_[position_] == '-') ++position_;
    if (position_ >= text_.size()) return Error("expected JSON number");
    if (text_[position_] == '0') {
      ++position_;
      if (position_ < text_.size() &&
          std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        return Error("JSON number has a leading zero");
    } else {
      if (text_[position_] < '1' || text_[position_] > '9')
        return Error("expected JSON number");
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        ++position_;
    }
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      const std::size_t fraction = position_;
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        ++position_;
      if (fraction == position_)
        return Error("JSON fraction has no digits");
    }
    if (position_ < text_.size() &&
        (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() &&
          (text_[position_] == '+' || text_[position_] == '-'))
        ++position_;
      const std::size_t exponent = position_;
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_])) != 0)
        ++position_;
      if (exponent == position_)
        return Error("JSON exponent has no digits");
    }
    const std::string token = text_.substr(begin, position_ - begin);
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (errno != 0 || end == token.c_str() || *end != '\0' ||
        !std::isfinite(value))
      return Error("JSON number is not finite");
    *result = value;
    return true;
  }

  bool String(std::string* result) {
    Space();
    if (position_ >= text_.size() || text_[position_] != '"')
      return Error("expected JSON string");
    ++position_;
    result->clear();
    while (position_ < text_.size()) {
      const unsigned char character =
          static_cast<unsigned char>(text_[position_++]);
      if (character == '"') return true;
      if (character < 0x20u || character > 0x7eu)
        return Error("tuning strings must use printable ASCII");
      if (character != '\\') {
        result->push_back(static_cast<char>(character));
        continue;
      }
      if (position_ >= text_.size())
        return Error("unterminated JSON escape");
      const char escaped = text_[position_++];
      if (escaped == '"' || escaped == '\\' || escaped == '/')
        result->push_back(escaped);
      else
        return Error("unsupported JSON string escape");
    }
    return Error("unterminated JSON string");
  }

  bool Finished() {
    Space();
    return position_ == text_.size() || Error("trailing JSON data");
  }

  const std::string& error() const { return error_; }

 private:
  void Space() {
    while (position_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[position_])) != 0)
      ++position_;
  }

  bool Error(const char* message) {
    if (error_.empty())
      error_ = std::string(message) + " at byte " +
               std::to_string(position_);
    return false;
  }

  const std::string& text_;
  std::size_t position_ = 0;
  std::string error_;
};

bool ParseVehicle(JsonCursor* cursor, VehiclePatch* patch,
                  std::string* failure) {
  if (!cursor->Consume('{')) return false;
  std::set<std::string> keys;
  if (cursor->TryConsume('}')) {
    *failure = "vehicle tuning object is empty";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cursor->String(&key) || !cursor->Consume(':')) return false;
    if (!keys.insert(key).second) {
      *failure = "duplicate vehicle tuning key: " + key;
      return false;
    }
    bool accepted = true;
    if (key == "id") accepted = cursor->String(&patch->id);
    else if (key == "max_speed") {
      patch->hasMaxSpeed = true;
      accepted = cursor->Number(&patch->maxSpeed);
    } else if (key == "reverse_speed") {
      patch->hasReverseSpeed = true;
      accepted = cursor->Number(&patch->reverseSpeed);
    } else if (key == "acceleration_time") {
      patch->hasAccelerationTime = true;
      accepted = cursor->Number(&patch->accelerationTime);
    } else if (key == "turn_speed") {
      patch->hasTurnSpeed = true;
      accepted = cursor->Number(&patch->turnSpeed);
    } else if (key == "primary_fire_interval") {
      patch->hasPrimaryFireInterval = true;
      accepted = cursor->Number(&patch->primaryFireInterval);
    } else if (key == "secondary_fire_interval") {
      patch->hasSecondaryFireInterval = true;
      accepted = cursor->Number(&patch->secondaryFireInterval);
    } else if (key == "secondary_projectile") {
      patch->hasSecondaryProjectile = true;
      accepted = cursor->String(&patch->secondaryProjectile);
    } else if (key == "damage_power") {
      patch->hasDamagePower = true;
      accepted = cursor->Number(&patch->damagePower);
    } else {
      *failure = "vehicle tuning contains unknown key: " + key;
      return false;
    }
    if (!accepted) return false;
    if (cursor->TryConsume('}')) break;
    if (!cursor->Consume(',')) return false;
  }
  if (keys.find("id") == keys.end() || !ValidIdentifier(patch->id)) {
    *failure = "vehicle tuning has an invalid or missing id";
    return false;
  }
  if (!(patch->hasMaxSpeed || patch->hasReverseSpeed ||
        patch->hasAccelerationTime || patch->hasTurnSpeed ||
        patch->hasPrimaryFireInterval ||
        patch->hasSecondaryFireInterval ||
        patch->hasSecondaryProjectile || patch->hasDamagePower)) {
    *failure = "vehicle tuning has no parameter";
    return false;
  }
  if ((patch->hasMaxSpeed && !InRange(patch->maxSpeed, 0.5, 250.0)) ||
      (patch->hasReverseSpeed &&
       !InRange(patch->reverseSpeed, 0.0, 250.0)) ||
      (patch->hasAccelerationTime &&
       !InRange(patch->accelerationTime, 0.05, 30.0)) ||
      (patch->hasTurnSpeed && !InRange(patch->turnSpeed, 1.0, 720.0)) ||
      (patch->hasPrimaryFireInterval &&
       !InRange(patch->primaryFireInterval, 0.02, 10.0)) ||
      (patch->hasSecondaryFireInterval &&
       !InRange(patch->secondaryFireInterval, 0.02, 10.0)) ||
      (patch->hasDamagePower &&
       !InRange(patch->damagePower, 0.1, 1000.0))) {
    *failure = "vehicle tuning value is outside the schema-1 range";
    return false;
  }
  if (patch->hasSecondaryProjectile &&
      (!ValidIdentifier(patch->secondaryProjectile) ||
       patch->secondaryProjectile.size() >= sizeof(ct_AttrStr))) {
    *failure = "secondary_projectile is not a storable symbolic id";
    return false;
  }
  return true;
}

bool ParseProjectile(JsonCursor* cursor, ProjectilePatch* patch,
                     std::string* failure) {
  if (!cursor->Consume('{')) return false;
  std::set<std::string> keys;
  if (cursor->TryConsume('}')) {
    *failure = "projectile tuning object is empty";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cursor->String(&key) || !cursor->Consume(':')) return false;
    if (!keys.insert(key).second) {
      *failure = "duplicate projectile tuning key: " + key;
      return false;
    }
    if (key == "id") {
      if (!cursor->String(&patch->id)) return false;
    } else if (key == "speed") {
      patch->hasSpeed = true;
      if (!cursor->Number(&patch->speed)) return false;
    } else {
      *failure = "projectile tuning contains unknown key: " + key;
      return false;
    }
    if (cursor->TryConsume('}')) break;
    if (!cursor->Consume(',')) return false;
  }
  if (keys.find("id") == keys.end() || !ValidIdentifier(patch->id)) {
    *failure = "projectile tuning has an invalid or missing id";
    return false;
  }
  if (!patch->hasSpeed || !InRange(patch->speed, 1.0, 2000.0)) {
    *failure = "projectile speed is missing or outside the schema-1 range";
    return false;
  }
  return true;
}

template <typename Patch, typename Parser>
bool ParseArray(JsonCursor* cursor, std::vector<Patch>* patches,
                Parser parser, std::string* failure) {
  if (!cursor->Consume('[')) return false;
  if (cursor->TryConsume(']')) return true;
  for (;;) {
    if (patches->size() >= kMaximumEntries) {
      *failure = "tuning array exceeds 64 entries";
      return false;
    }
    Patch patch;
    if (!parser(cursor, &patch, failure)) return false;
    patches->push_back(std::move(patch));
    if (cursor->TryConsume(']')) return true;
    if (!cursor->Consume(',')) return false;
  }
}

bool ParseDocument(const std::string& text, Document* document,
                   std::string* failure) {
  JsonCursor cursor(text);
  if (!cursor.Consume('{')) {
    *failure = cursor.error();
    return false;
  }
  bool schemaSeen = false;
  bool vehiclesSeen = false;
  bool projectilesSeen = false;
  if (cursor.TryConsume('}')) {
    *failure = "gameplay tuning object is empty";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cursor.String(&key) || !cursor.Consume(':')) break;
    bool accepted = true;
    if (key == "schema") {
      accepted = !schemaSeen && cursor.Integer(&document->schema);
      schemaSeen = true;
    } else if (key == "vehicles") {
      accepted = !vehiclesSeen &&
          ParseArray(&cursor, &document->vehicles, ParseVehicle, failure);
      vehiclesSeen = true;
    } else if (key == "projectiles") {
      accepted = !projectilesSeen &&
          ParseArray(&cursor, &document->projectiles, ParseProjectile,
                     failure);
      projectilesSeen = true;
    } else {
      *failure = "gameplay tuning contains unknown key: " + key;
      return false;
    }
    if (!accepted) break;
    if (cursor.TryConsume('}')) {
      if (!cursor.Finished()) break;
      if (!schemaSeen) {
        *failure = "gameplay tuning is missing schema";
        return false;
      }
      if (!vehiclesSeen && !projectilesSeen) {
        *failure = "gameplay tuning has no tuning arrays";
        return false;
      }
      if (document->vehicles.empty() && document->projectiles.empty()) {
        *failure = "gameplay tuning has no patches";
        return false;
      }
      return true;
    }
    if (!cursor.Consume(',')) break;
  }
  if (failure->empty())
    *failure = cursor.error().empty() ? "invalid gameplay tuning value"
                                      : cursor.error();
  return false;
}

bool ValidateUniqueTargets(const Document& document,
                           std::string* failure) {
  std::set<std::string> targets;
  for (const VehiclePatch& patch : document.vehicles) {
    if (!targets.insert("vehicle:" + FoldAscii(patch.id)).second) {
      *failure = "duplicate vehicle tuning target: " + patch.id;
      return false;
    }
  }
  for (const ProjectilePatch& patch : document.projectiles) {
    if (!targets.insert("projectile:" + FoldAscii(patch.id)).second) {
      *failure = "duplicate projectile tuning target: " + patch.id;
      return false;
    }
  }
  return true;
}

std::uint64_t Fingerprint(const std::string& text) {
  std::uint64_t hash = kFnvOffset;
  for (unsigned char byte : text) {
    hash ^= byte;
    hash *= kFnvPrime;
  }
  return hash == 0 ? 1 : hash;
}

AttributeVehicle* FindVehicle(SimulationContext* context,
                              const std::string& name) {
  KR_ObjectID id = context->searchObject(name.c_str());
  return id.isNUL() ? nullptr : static_cast<AttributeVehicle*>(
      __attrVehicleTable.searchAttribute(id));
}

AttributeBullet* FindProjectile(SimulationContext* context,
                                const std::string& name) {
  KR_ObjectID id = context->searchObject(name.c_str());
  return id.isNUL() ? nullptr : static_cast<AttributeBullet*>(
      __bulletAttrTable.searchAttribute(id));
}

void RestoreTransaction() {
  for (auto iterator = g_projectileRollback.rbegin();
       iterator != g_projectileRollback.rend(); ++iterator) {
    if (iterator->attribute != nullptr) iterator->attribute->m_startSpeed =
        iterator->speed;
  }
  for (auto iterator = g_vehicleRollback.rbegin();
       iterator != g_vehicleRollback.rend(); ++iterator) {
    if (iterator->attribute != nullptr) {
      iterator->attribute->m_bulletSlipTime = iterator->primaryFireInterval;
      iterator->attribute->m_bulletSecSlipTime =
          iterator->secondaryFireInterval;
      iterator->attribute->m_power = iterator->damagePower;
      std::memcpy(iterator->attribute->m_bulletSecAttrName,
                  iterator->secondaryProjectile, sizeof(ct_AttrStr));
    }
    if (iterator->vesselCaptured)
      VehicleGameplayTuning_Restore(&iterator->vessel);
  }
}

void ClearState() {
  g_summary = SRecoveredGameplayTuningSummary{};
  g_vehicleRollback.clear();
  g_projectileRollback.clear();
  g_context = nullptr;
  g_issues = 0;
  g_lastError[0] = '\0';
  g_active = false;
  g_vehicleReferencesFinalized = false;
}

bool ResolveTransaction(SimulationContext* context, const Document& document,
                        std::vector<AttributeVehicle*>* vehicles,
                        std::vector<AttributeBullet*>* secondaryProjectiles,
                        std::vector<AttributeBullet*>* projectiles) {
  std::set<std::string> targets;
  std::set<std::string> dynamics;
  for (const VehiclePatch& patch : document.vehicles) {
    const std::string folded = FoldAscii(patch.id);
    if (!targets.insert("vehicle:" + folded).second) {
      Fail(RECOVERED_GAMEPLAY_TUNING_DUPLICATE_TARGET,
           "duplicate vehicle tuning target: " + patch.id);
      return false;
    }
    AttributeVehicle* attribute = FindVehicle(context, patch.id);
    if (attribute == nullptr) {
      Fail(RECOVERED_GAMEPLAY_TUNING_UNKNOWN_TARGET,
           "unknown VehicleAttr tuning target: " + patch.id);
      return false;
    }
    const bool vesselPatch = patch.hasMaxSpeed || patch.hasReverseSpeed ||
        patch.hasAccelerationTime || patch.hasTurnSpeed;
    if (vesselPatch) {
      if (!VehicleGameplayTuning_SupportsDynamic(attribute->m_dynamic)) {
        Fail(RECOVERED_GAMEPLAY_TUNING_UNSUPPORTED_DYNAMIC,
             "VehicleAttr uses an unsupported tuning dynamic: " + patch.id +
                 " -> " + attribute->m_dynamic);
        return false;
      }
      const std::string dynamic = FoldAscii(attribute->m_dynamic);
      if (!dynamics.insert(dynamic).second) {
        Fail(RECOVERED_GAMEPLAY_TUNING_DUPLICATE_TARGET,
             "multiple VehicleAttr patches own the same dynamic: " +
                 std::string(attribute->m_dynamic));
        return false;
      }
    }
    vehicles->push_back(attribute);
    AttributeBullet* secondaryProjectile = nullptr;
    if (patch.hasSecondaryProjectile) {
      secondaryProjectile = FindProjectile(
          context, patch.secondaryProjectile);
      if (secondaryProjectile == nullptr) {
        Fail(RECOVERED_GAMEPLAY_TUNING_UNKNOWN_TARGET,
             "unknown secondary BulletAttr tuning target: " +
                 patch.secondaryProjectile);
        return false;
      }
    }
    secondaryProjectiles->push_back(secondaryProjectile);
  }
  for (const ProjectilePatch& patch : document.projectiles) {
    const std::string folded = FoldAscii(patch.id);
    if (!targets.insert("projectile:" + folded).second) {
      Fail(RECOVERED_GAMEPLAY_TUNING_DUPLICATE_TARGET,
           "duplicate projectile tuning target: " + patch.id);
      return false;
    }
    AttributeBullet* attribute = FindProjectile(context, patch.id);
    if (attribute == nullptr) {
      Fail(RECOVERED_GAMEPLAY_TUNING_UNKNOWN_TARGET,
           "unknown BulletAttr tuning target: " + patch.id);
      return false;
    }
    projectiles->push_back(attribute);
  }
  return true;
}

void CaptureObservations(SimulationContext* context) {
  AttributeVehicle* vehicle = FindVehicle(context, "Vehicle.Attr.default");
  if (vehicle != nullptr) {
    SVehicleVesselGameplayState state = {};
    g_summary.defaultVehiclePresent = 1;
    g_summary.defaultPrimaryFireInterval = vehicle->m_bulletSlipTime;
    g_summary.defaultSecondaryFireInterval =
        vehicle->m_bulletSecSlipTime;
    g_summary.defaultDamagePower = vehicle->m_power;
    std::snprintf(g_summary.defaultSecondaryProjectile,
                  sizeof(g_summary.defaultSecondaryProjectile), "%s",
                  vehicle->m_bulletSecAttrName);
    if (VehicleGameplayTuning_Capture(vehicle->m_dynamic, &state)) {
      g_summary.defaultMaxSpeed = state.maxSpeed;
      g_summary.defaultReverseSpeed = state.reverseSpeed;
      g_summary.defaultAccelerationTime = state.accelerationTime;
      g_summary.defaultTurnSpeed = state.turnSpeed;
    }
  }
  AttributeBullet* projectile = FindProjectile(context, "Bullet.Led.Prim");
  if (projectile != nullptr) {
    g_summary.primaryProjectilePresent = 1;
    g_summary.primaryProjectileSpeed = projectile->m_startSpeed;
  }
}

}  // namespace

bool RecoveredGameplayTuning_Apply(SimulationContext* context) {
  RecoveredGameplayTuning_Release(context);
  if (context == nullptr) {
    Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
         "gameplay tuning received no SimulationContext");
    return false;
  }
  if (!RecoveredModRuntime_HasOverlayTarget(kTuningTarget)) return true;

  try {
    long length = 0;
    FILE* file = RecoveredModRuntime_OpenOverlayTarget(kTuningTarget, &length);
    if (file == nullptr || length < 0) {
      if (file != nullptr) std::fclose(file);
      Fail(RECOVERED_GAMEPLAY_TUNING_IO_FAILURE,
           "could not open declared RR2NW/gameplay-tuning.json");
      return false;
    }
    if (length == 0 || length > kMaximumTuningBytes) {
      std::fclose(file);
      Fail(RECOVERED_GAMEPLAY_TUNING_TOO_LARGE,
           "gameplay tuning must contain 1..262144 bytes");
      return false;
    }
    std::string text(static_cast<std::size_t>(length), '\0');
    const std::size_t read =
        std::fread(&text[0], 1, static_cast<std::size_t>(length), file);
    const bool readFailed = read != static_cast<std::size_t>(length) ||
                            std::ferror(file) != 0;
    std::fclose(file);
    if (readFailed) {
      Fail(RECOVERED_GAMEPLAY_TUNING_IO_FAILURE,
           "could not read complete RR2NW/gameplay-tuning.json");
      return false;
    }

    Document document;
    std::string parseFailure;
    if (!ParseDocument(text, &document, &parseFailure)) {
      Fail(RECOVERED_GAMEPLAY_TUNING_MALFORMED,
           "invalid gameplay tuning: " + parseFailure);
      return false;
    }
    if (document.schema != kSchemaVersion) {
      Fail(RECOVERED_GAMEPLAY_TUNING_UNSUPPORTED_SCHEMA,
           "unsupported gameplay tuning schema: " +
               std::to_string(document.schema));
      return false;
    }
    if (!ValidateUniqueTargets(document, &parseFailure)) {
      Fail(RECOVERED_GAMEPLAY_TUNING_DUPLICATE_TARGET, parseFailure);
      return false;
    }
    if (!VehicleAttributeState_CachesUnresolved(context) ||
        !VehicleAttributeState_IsKnownRoster(context) ||
        !BulletAttributeState_CachesUnresolved(context) ||
        !BulletAttributeState_IsKnownRoster(context)) {
      Fail(RECOVERED_GAMEPLAY_TUNING_RETAIL_ROSTER_MISMATCH,
           "gameplay tuning requires a verified unresolved retail "
           "VehicleAttr/BulletAttr roster");
      return false;
    }

    std::vector<AttributeVehicle*> vehicles;
    std::vector<AttributeBullet*> secondaryProjectiles;
    std::vector<AttributeBullet*> projectiles;
    vehicles.reserve(document.vehicles.size());
    secondaryProjectiles.reserve(document.vehicles.size());
    projectiles.reserve(document.projectiles.size());
    if (!ResolveTransaction(context, document, &vehicles,
                            &secondaryProjectiles, &projectiles))
      return false;

    g_vehicleRollback.reserve(vehicles.size());
    g_projectileRollback.reserve(projectiles.size());
    for (std::size_t index = 0; index < vehicles.size(); ++index) {
      const VehiclePatch& patch = document.vehicles[index];
      AttributeVehicle* attribute = vehicles[index];
      VehicleRollback rollback;
      rollback.attribute = attribute;
      rollback.primaryFireInterval = attribute->m_bulletSlipTime;
      rollback.secondaryFireInterval = attribute->m_bulletSecSlipTime;
      rollback.damagePower = attribute->m_power;
      rollback.secondaryProjectilePatched =
          patch.hasSecondaryProjectile;
      std::memcpy(rollback.secondaryProjectile,
                  attribute->m_bulletSecAttrName, sizeof(ct_AttrStr));
      rollback.expectedSecondaryProjectile = secondaryProjectiles[index];
      const bool vesselPatch = patch.hasMaxSpeed || patch.hasReverseSpeed ||
          patch.hasAccelerationTime || patch.hasTurnSpeed;
      if (vesselPatch &&
          !VehicleGameplayTuning_Capture(attribute->m_dynamic,
                                         &rollback.vessel)) {
        Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
             "could not capture Vehicle dynamic before tuning: " + patch.id);
        RestoreTransaction();
        return false;
      }
      rollback.vesselCaptured = vesselPatch;
      g_vehicleRollback.push_back(rollback);
    }
    for (AttributeBullet* attribute : projectiles) {
      ProjectileRollback rollback;
      rollback.attribute = attribute;
      rollback.speed = attribute->m_startSpeed;
      g_projectileRollback.push_back(rollback);
    }

    for (std::size_t index = 0; index < vehicles.size(); ++index) {
      const VehiclePatch& patch = document.vehicles[index];
      AttributeVehicle* attribute = vehicles[index];
      SVehicleVesselGameplayPatch vessel = {};
      vessel.hasMaxSpeed = patch.hasMaxSpeed;
      vessel.hasReverseSpeed = patch.hasReverseSpeed;
      vessel.hasAccelerationTime = patch.hasAccelerationTime;
      vessel.hasTurnSpeed = patch.hasTurnSpeed;
      vessel.maxSpeed = patch.maxSpeed;
      vessel.reverseSpeed = patch.reverseSpeed;
      vessel.accelerationTime = patch.accelerationTime;
      vessel.turnSpeed = patch.turnSpeed;
      if ((patch.hasMaxSpeed || patch.hasReverseSpeed ||
           patch.hasAccelerationTime || patch.hasTurnSpeed) &&
          !VehicleGameplayTuning_Apply(attribute->m_dynamic, &vessel)) {
        Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
             "could not commit Vehicle dynamic tuning: " + patch.id);
        RestoreTransaction();
        return false;
      }
      if (patch.hasPrimaryFireInterval)
        attribute->m_bulletSlipTime = patch.primaryFireInterval;
      if (patch.hasSecondaryFireInterval)
        attribute->m_bulletSecSlipTime = patch.secondaryFireInterval;
      if (patch.hasSecondaryProjectile)
        std::snprintf(attribute->m_bulletSecAttrName,
                      sizeof(attribute->m_bulletSecAttrName), "%s",
                      patch.secondaryProjectile.c_str());
      if (patch.hasDamagePower) attribute->m_power = patch.damagePower;
    }
    for (std::size_t index = 0; index < projectiles.size(); ++index)
      projectiles[index]->m_startSpeed = document.projectiles[index].speed;

    unsigned int ballisticProofs = 0;
    unsigned int ballisticMoves = 0;
    for (const ProjectilePatch& patch : document.projectiles) {
      int moveCount = 0;
      if (!BulletSubjectState_ProbeBallisticLifecycle(
              context, patch.id.c_str(), Session::m_moment, &moveCount) ||
          moveCount != 2 || BulletSubjectState_LiveCount() != 0) {
        Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
             "tuned projectile failed real ballistic lifecycle: " +
                 patch.id);
        RestoreTransaction();
        return false;
      }
      ++ballisticProofs;
      ballisticMoves += static_cast<unsigned int>(moveCount);
    }

    const std::uint64_t vehicleFingerprint =
        VehicleAttributeState_Fingerprint(context);
    const std::uint64_t bulletFingerprint =
        BulletAttributeState_Fingerprint(context);
    if (vehicleFingerprint == 0 || bulletFingerprint == 0) {
      Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
           "tuned attribute roster did not produce stable fingerprints");
      RestoreTransaction();
      return false;
    }

    g_context = context;
    g_active = true;
    g_summary.schemaVersion = document.schema;
    g_summary.vehiclePatchCount =
        static_cast<unsigned int>(document.vehicles.size());
    g_summary.projectilePatchCount =
        static_cast<unsigned int>(document.projectiles.size());
    g_summary.projectileBallisticProofs = ballisticProofs;
    g_summary.projectileBallisticMoves = ballisticMoves;
    g_summary.tuningFingerprint = Fingerprint(text);
    g_summary.vehicleAttributeFingerprint = vehicleFingerprint;
    g_summary.bulletAttributeFingerprint = bulletFingerprint;
    CaptureObservations(context);
    return true;
  } catch (const std::bad_alloc&) {
    RestoreTransaction();
    Fail(RECOVERED_GAMEPLAY_TUNING_ALLOCATION_FAILURE,
         "gameplay tuning allocation failed");
  } catch (...) {
    RestoreTransaction();
    Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
         "gameplay tuning raised an unexpected exception");
  }
  return false;
}

bool RecoveredGameplayTuning_FinalizeVehicleReferences(
    SimulationContext* context) {
  if (!g_active) return true;
  if (context == nullptr || context != g_context) {
    Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
         "gameplay tuning reference finalization received the wrong "
         "SimulationContext");
    return false;
  }
  if (g_vehicleReferencesFinalized) {
    return VehicleAttributeState_ReferencesResolved(context) &&
           VehicleAttributeState_ReferenceFingerprint(context) ==
               g_summary.vehicleReferenceFingerprint;
  }
  if (!VehicleAttributeState_ReferencesResolved(context)) {
    Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
         "tuned VehicleAttr references were not completely resolved");
    return false;
  }

  AttributeBullet* lifecycleTargets[kMaximumEntries] = {};
  std::size_t lifecycleTargetCount = 0;
  unsigned int referenceProofs = 0;
  for (const VehicleRollback& rollback : g_vehicleRollback) {
    if (!rollback.secondaryProjectilePatched) continue;
    AttributeBullet* resolved = nullptr;
    if (rollback.attribute == nullptr ||
        rollback.expectedSecondaryProjectile == nullptr ||
        rollback.attribute->m_bulletSecAttrIndex == -1 ||
        !BulletAttributeState_ResolveEncodedIndex(
            context, rollback.attribute->m_bulletSecAttrIndex, &resolved) ||
        resolved != rollback.expectedSecondaryProjectile) {
      Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
           "secondary projectile did not resolve to the requested "
           "BulletAttr");
      return false;
    }
    ++referenceProofs;
    bool alreadyScheduled = false;
    for (std::size_t index = 0; index < lifecycleTargetCount; ++index)
      if (lifecycleTargets[index] == resolved) alreadyScheduled = true;
    if (!alreadyScheduled && lifecycleTargetCount >= kMaximumEntries) {
      Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
           "too many unique secondary projectile lifecycle targets");
      return false;
    }
    if (!alreadyScheduled)
      lifecycleTargets[lifecycleTargetCount++] = resolved;
  }

  unsigned int ballisticProofs = 0;
  unsigned int ballisticMoves = 0;
  for (std::size_t index = 0; index < lifecycleTargetCount; ++index) {
    AttributeBullet* target = lifecycleTargets[index];
    const char* name = context->searchObject(target->getObjectID());
    int moveCount = 0;
    if (name == nullptr || name[0] == '\0' ||
        !BulletSubjectState_ProbeBallisticLifecycle(
            context, name, Session::m_moment, &moveCount) ||
        moveCount != 2 || BulletSubjectState_LiveCount() != 0) {
      Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
           std::string("secondary projectile failed resolved ballistic ") +
               "lifecycle: " + (name == nullptr ? "<unknown>" : name));
      return false;
    }
    ++ballisticProofs;
    ballisticMoves += static_cast<unsigned int>(moveCount);
  }

  const std::uint64_t referenceFingerprint =
      VehicleAttributeState_ReferenceFingerprint(context);
  if (referenceFingerprint == 0) {
    Fail(RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE,
         "tuned VehicleAttr references have no stable fingerprint");
    return false;
  }
  g_summary.secondaryProjectileReferenceProofs = referenceProofs;
  g_summary.secondaryProjectileBallisticProofs = ballisticProofs;
  g_summary.secondaryProjectileBallisticMoves = ballisticMoves;
  g_summary.vehicleReferenceFingerprint = referenceFingerprint;
  g_vehicleReferencesFinalized = true;
  return true;
}

void RecoveredGameplayTuning_Release(SimulationContext* context) {
  // Attribute pointers belong to the open Arena seance. A null/different
  // context means that owner is already unavailable, so never dereference the
  // captured transaction during late process teardown.
  if (g_active && g_context != nullptr && context == g_context)
    RestoreTransaction();
  ClearState();
}

bool RecoveredGameplayTuning_IsActive() { return g_active; }

unsigned int RecoveredGameplayTuning_Issues() { return g_issues; }

const char* RecoveredGameplayTuning_LastError() { return g_lastError; }

const SRecoveredGameplayTuningSummary* RecoveredGameplayTuning_Summary() {
  return g_active ? &g_summary : nullptr;
}

bool RecoveredGameplayTuning_ValidateText(
    const char* text, std::size_t length, char* error,
    std::size_t errorSize) {
  if (error != nullptr && errorSize != 0) error[0] = '\0';
  std::string failure;
  bool valid = false;
  try {
    if (text == nullptr || length == 0 ||
        length > static_cast<std::size_t>(kMaximumTuningBytes)) {
      failure = "gameplay tuning text has an invalid size";
    } else {
      Document document;
      valid = ParseDocument(std::string(text, length), &document, &failure) &&
              document.schema == kSchemaVersion &&
              ValidateUniqueTargets(document, &failure);
      if (!valid && failure.empty())
        failure = "unsupported gameplay tuning schema";
    }
  } catch (const std::bad_alloc&) {
    failure = "gameplay tuning validation allocation failed";
  } catch (...) {
    failure = "gameplay tuning validation raised an exception";
  }
  if (!valid && error != nullptr && errorSize != 0)
    std::snprintf(error, errorSize, "%s", failure.c_str());
  return valid;
}

bool RecoveredGameplayTuning_AcceptsVehicleRoster(
    SimulationContext* context) {
  return !g_active ? VehicleAttributeState_IsKnownRoster(context)
                   : context == g_context &&
                         VehicleAttributeState_Fingerprint(context) ==
                             g_summary.vehicleAttributeFingerprint;
}

bool RecoveredGameplayTuning_AcceptsBulletRoster(
    SimulationContext* context) {
  return !g_active ? BulletAttributeState_IsKnownRoster(context)
                   : context == g_context &&
                         BulletAttributeState_Fingerprint(context) ==
                             g_summary.bulletAttributeFingerprint;
}

bool RecoveredGameplayTuning_AcceptsVehicleReferences(
    SimulationContext* context) {
  if (!g_active) return VehicleAttributeState_IsKnownReferenceRoster(context);
  return g_vehicleReferencesFinalized &&
         RecoveredGameplayTuning_AcceptsVehicleRoster(context) &&
         VehicleAttributeState_ReferencesResolved(context) &&
         VehicleAttributeState_ReferenceFingerprint(context) ==
             g_summary.vehicleReferenceFingerprint;
}

bool RecoveredGameplayTuning_AcceptsBulletReferences(
    SimulationContext* context) {
  if (!g_active) return BulletAttributeState_IsKnownReferenceRoster(context);
  return RecoveredGameplayTuning_AcceptsBulletRoster(context) &&
         BulletAttributeState_ReferencesResolved(context) &&
         BulletAttributeState_ReferenceFingerprint(context) != 0;
}
