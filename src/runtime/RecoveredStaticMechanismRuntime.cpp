#include "RecoveredStaticMechanismRuntime.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define LAST_H__SCENE
#include "game.h"
#include "kernel/h/session.h"

namespace {

constexpr unsigned long long kHashOffset = 1469598103934665603ull;
constexpr unsigned long long kHashPrime = 1099511628211ull;

enum EMechanismKind {
  MECHANISM_WTR_B05,
  MECHANISM_WTR_F04,
  MECHANISM_FLG_CIV,
  MECHANISM_FLG_VILL
};

struct ModifierSnapshot {
  CViewBaseModifier0* modifier = nullptr;
  std::vector<CFVector3> vertices;
};

struct MechanismBinding {
  EMechanismKind kind = MECHANISM_WTR_B05;
  std::string name;
  int ordinal = 0;
  CViewObjectRef* reference = nullptr;
  CViewObjectBase* base = nullptr;
  double speed = 0.0;
  CFVector3 axes[6];
  CViewBaseModifier0* modifiers[8] = {};
  int modifierCount = 0;
  std::vector<ModifierSnapshot> snapshots;
  unsigned long long baselineFingerprint = 0;
};

std::vector<std::unique_ptr<MechanismBinding>> g_bindings;
SRecoveredStaticMechanismSummary g_summary = {};
std::string g_lastError;

bool Fail(const std::string& message) {
  g_lastError = message;
  return false;
}

void HashBytes(unsigned long long& hash, const void* bytes, std::size_t size) {
  const unsigned char* data = static_cast<const unsigned char*>(bytes);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= data[index];
    hash *= kHashPrime;
  }
}

void HashString(unsigned long long& hash, const char* value) {
  const std::size_t size = value == nullptr ? 0 : std::strlen(value);
  HashBytes(hash, &size, sizeof(size));
  if (size != 0) HashBytes(hash, value, size);
}

bool EqualsAsciiInsensitive(const char* left, const char* right) {
  if (left == nullptr || right == nullptr) return false;
  while (*left != '\0' && *right != '\0') {
    char a = *left++;
    char b = *right++;
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  return *left == *right;
}

const char* DirectoryLeaf(const char* directory) {
  if (directory == nullptr) return "";
  const char* leaf = directory;
  for (const char* cursor = directory; *cursor != '\0'; ++cursor)
    if (*cursor == '\\' || *cursor == '/') leaf = cursor + 1;
  return leaf;
}

double StableRange(const char* name, int ordinal, double minimum,
                   double maximum) {
  // Static presentation must not advance the authoritative simulation RNG.
  unsigned long long hash = kHashOffset;
  HashString(hash, name);
  HashBytes(hash, &ordinal, sizeof(ordinal));
  const double unit =
      static_cast<double>(hash & 0x00ffffffull) / 16777215.0;
  return minimum + (maximum - minimum) * unit;
}

bool NamedVertex(CViewObjectBase& base, const char* name, int index,
                 CFVector3* value) {
  CNameDecl* declaration = base.Names().vertices.Lookup(name);
  if (declaration == nullptr || index < 0 || index >= declaration->Count() ||
      (*declaration)[index] == nullptr || value == nullptr) {
    g_lastError = std::string("missing named vertex: ") + name + "[" +
                  std::to_string(index) + "]";
    return false;
  }
  *value = *static_cast<CFVector3*>((*declaration)[index]);
  return true;
}

bool Midpoint(CViewObjectBase& base, const char* name, CFVector3* value) {
  CFVector3 first;
  CFVector3 second;
  return NamedVertex(base, name, 0, &first) &&
         NamedVertex(base, name, 1, &second) &&
         ((*value = (first + second) * 0.5), true);
}

bool Difference(CViewObjectBase& base, const char* name, int firstIndex,
                int secondIndex, double scale, CFVector3* value) {
  CFVector3 first;
  CFVector3 second;
  return NamedVertex(base, name, firstIndex, &first) &&
         NamedVertex(base, name, secondIndex, &second) &&
         ((*value = (first - second) * scale), true);
}

bool AddModifier(MechanismBinding& binding, const char* name) {
  if (binding.modifierCount >= 8) {
    g_lastError = std::string("modifier capacity exceeded: ") + name;
    return false;
  }
  CViewBaseModifier0* modifier = binding.base->KFSet().LookupMod0(name);
  if (modifier == nullptr) {
    g_lastError = std::string("missing base modifier: ") + name;
    return false;
  }
  // Update-only modifiers are legal. The shipped flag "Planes" modifier has
  // no vertices of its own and recalculates normals after Point1..Point5 move.
  binding.modifiers[binding.modifierCount++] = modifier;
  return true;
}

bool CaptureBinding(MechanismBinding& binding) {
  binding.snapshots.clear();
  for (int index = 0; index < binding.modifierCount; ++index) {
    ModifierSnapshot snapshot;
    snapshot.modifier = binding.modifiers[index];
    const int count = snapshot.modifier->VertexCount();
    snapshot.vertices.reserve(static_cast<std::size_t>(count));
    for (int vertex = 0; vertex < count; ++vertex)
      snapshot.vertices.push_back(snapshot.modifier->Vertex(vertex));
    binding.snapshots.push_back(snapshot);
  }
  return !binding.snapshots.empty();
}

void RestoreBinding(MechanismBinding& binding) {
  for (std::size_t index = 0; index < binding.snapshots.size(); ++index) {
    ModifierSnapshot& snapshot = binding.snapshots[index];
    for (std::size_t vertex = 0; vertex < snapshot.vertices.size(); ++vertex)
      snapshot.modifier->Vertex(static_cast<int>(vertex)) =
          snapshot.vertices[vertex];
  }
  for (std::size_t index = 0; index < binding.snapshots.size(); ++index)
    binding.snapshots[index].modifier->Update();
}

unsigned long long PoseFingerprint(const MechanismBinding& binding) {
  unsigned long long hash = kHashOffset;
  for (std::size_t index = 0; index < binding.snapshots.size(); ++index) {
    const ModifierSnapshot& snapshot = binding.snapshots[index];
    const int count = snapshot.modifier->VertexCount();
    HashBytes(hash, &count, sizeof(count));
    for (int vertex = 0; vertex < count; ++vertex) {
      const CFVector3& value = snapshot.modifier->Vertex(vertex);
      HashBytes(hash, &value, sizeof(value));
    }
  }
  return hash;
}

void WtrB05Callback(CViewObjectBaseSet*, CViewObjectBase*,
                    CViewObjectRef* reference) {
  MechanismBinding* data =
      static_cast<MechanismBinding*>(reference->GetUserAttrib());
  if (data == nullptr) return;
  data->modifiers[0]->LoadIdentity()
      .RotateOz(0.4 * std::sin(Session::m_viewTime * data->speed * 2.0),
                data->axes[0])
      .Update();
  data->modifiers[1]->LoadIdentity()
      .RotateOz(Session::m_viewTime * data->speed * 1.5, data->axes[1])
      .Update();
  data->modifiers[2]->LoadIdentity()
      .RotateOz(Session::m_viewTime * data->speed, data->axes[2])
      .Update();
}

void WtrF04Callback(CViewObjectBaseSet*, CViewObjectBase*,
                    CViewObjectRef* reference) {
  MechanismBinding* data =
      static_cast<MechanismBinding*>(reference->GetUserAttrib());
  if (data == nullptr) return;
  const double phase = Session::m_viewTime * data->speed;
  data->modifiers[0]->LoadIdentity().RotateOz(phase, data->axes[0]).Update();
  data->modifiers[1]->LoadIdentity().RotateOz(phase, data->axes[1]).Update();
  data->modifiers[2]->LoadIdentity().RotateOz(phase, data->axes[2]).Update();
  data->modifiers[3]->LoadIdentity()
      .RotateOz(30.0 * 3.14 / 180.0 * std::sin(phase) - phase,
                data->axes[3])
      .RotateOz(phase, data->axes[2])
      .Update();
  data->modifiers[4]->LoadIdentity()
      .RotateOz(-30.0 * 3.14 / 180.0 * std::sin(phase) - phase,
                data->axes[4])
      .RotateOz(phase, data->axes[2])
      .Update();
  data->modifiers[5]->LoadIdentity()
      .Translate(0.34 * data->axes[5] *
                 (-1.0 + std::sin(phase + 0.5 * 3.14)))
      .Update();
  data->modifiers[6]->LoadIdentity()
      .Translate(0.4 * data->axes[5] *
                 (1.0 - std::sin(phase + 0.5 * 3.14)))
      .Update();
  data->modifiers[7]->LoadIdentity()
      .Translate(0.02 * data->axes[5] *
                 std::sin(Session::m_viewTime * 35.0))
      .Update();
}

void AnimateFlag(MechanismBinding& data, bool village) {
  const double phase = Session::m_viewTime * data.speed;
  data.modifiers[1]->LoadIdentity()
      .Translate(0.8 * (-5.0 + 2.0 * std::sin(phase + 3.14 * 0.2)) *
                 data.axes[1])
      .Translate(0.9 * std::sin(phase + 3.14 * 0.5) * data.axes[0]);
  data.modifiers[2]->LoadIdentity()
      .Translate((-6.0 + 2.5 * std::sin(phase + 3.14 * 0.8)) *
                 data.axes[1])
      .Translate(std::sin(phase + 3.14 * 0.75) * data.axes[0]);
  data.modifiers[3]->LoadIdentity()
      .Translate(1.1 * (-7.5 + 4.0 * std::sin(phase + 3.14 * 0.1)) *
                 data.axes[1])
      .Translate(1.1 * std::sin(phase) * data.axes[0]);
  data.modifiers[4]->LoadIdentity()
      .Translate((-11.0 + 7.0 * std::sin(phase + 3.14 * 0.4)) *
                 data.axes[1])
      .Translate(std::sin(phase + 3.14 * 0.25) * data.axes[0]);
  if (village)
    data.modifiers[5]->LoadIdentity()
        .Translate((-7.0 + 3.5 * std::sin(phase + 3.14 * 0.35)) *
                   data.axes[1])
        .Translate(std::sin(phase + 3.14 * 0.4) * data.axes[0]);
  data.modifiers[0]->LoadIdentity().Update();
}

void FlgCivCallback(CViewObjectBaseSet*, CViewObjectBase*,
                    CViewObjectRef* reference) {
  MechanismBinding* data =
      static_cast<MechanismBinding*>(reference->GetUserAttrib());
  if (data != nullptr) AnimateFlag(*data, false);
}

void FlgVillCallback(CViewObjectBaseSet*, CViewObjectBase*,
                     CViewObjectRef* reference) {
  MechanismBinding* data =
      static_cast<MechanismBinding*>(reference->GetUserAttrib());
  if (data != nullptr) AnimateFlag(*data, true);
}

bool ConfigureBinding(MechanismBinding& binding) {
  switch (binding.kind) {
    case MECHANISM_WTR_B05:
      binding.speed = 1.0;
      return Midpoint(*binding.base, "Axis0", &binding.axes[0]) &&
             Midpoint(*binding.base, "Axis1", &binding.axes[1]) &&
             Midpoint(*binding.base, "Axis2", &binding.axes[2]) &&
             AddModifier(binding, "Pendulum") &&
             AddModifier(binding, "Wheel1") &&
             AddModifier(binding, "Wheel2");
    case MECHANISM_WTR_F04:
      binding.speed = StableRange(binding.name.c_str(), binding.ordinal, 4.0,
                                  6.0);
      return Midpoint(*binding.base, "TWheel", &binding.axes[0]) &&
             Midpoint(*binding.base, "BWheel", &binding.axes[1]) &&
             Midpoint(*binding.base, "Kardan", &binding.axes[2]) &&
             NamedVertex(*binding.base, "Porsh1B", 0, &binding.axes[3]) &&
             NamedVertex(*binding.base, "Porsh2B", 0, &binding.axes[4]) &&
             Difference(*binding.base, "Porsh", 0, 1, 1.0,
                        &binding.axes[5]) &&
             AddModifier(binding, "TWheel") &&
             AddModifier(binding, "BWheel") &&
             AddModifier(binding, "Kardan") &&
             AddModifier(binding, "Porsh1B") &&
             AddModifier(binding, "Porsh2B") &&
             AddModifier(binding, "Porsh1T") &&
             AddModifier(binding, "Porsh2T") &&
             AddModifier(binding, "Tube");
    case MECHANISM_FLG_CIV:
    case MECHANISM_FLG_VILL: {
      binding.speed = StableRange(binding.name.c_str(), binding.ordinal, 6.0,
                                  8.0);
      const double axisScale =
          binding.kind == MECHANISM_FLG_VILL ? 2.0 : 1.0;
      if (!Difference(*binding.base, "Axis0", 0, 1, axisScale,
                      &binding.axes[0]) ||
          !Difference(*binding.base, "Axis1", 0, 1, 0.2,
                      &binding.axes[1]) ||
          !AddModifier(binding, "Planes") ||
          !AddModifier(binding, "Point1") ||
          !AddModifier(binding, "Point2") ||
          !AddModifier(binding, "Point3") ||
          !AddModifier(binding, "Point4"))
        return false;
      return binding.kind != MECHANISM_FLG_VILL ||
             AddModifier(binding, "Point5");
    }
  }
  return false;
}

CViewObjectRef::TAnimationCallback CallbackFor(EMechanismKind kind) {
  switch (kind) {
    case MECHANISM_WTR_B05:
      return WtrB05Callback;
    case MECHANISM_WTR_F04:
      return WtrF04Callback;
    case MECHANISM_FLG_CIV:
      return FlgCivCallback;
    case MECHANISM_FLG_VILL:
      return FlgVillCallback;
  }
  return nullptr;
}

bool BindNamedReferences(CViewScene& scene, const char* name,
                         EMechanismKind kind, int expectedCount) {
  CNameDecl* declaration = scene.ObjRefNames().Lookup(name);
  if (declaration == nullptr || declaration->Count() != expectedCount)
    return Fail(std::string("scene reference roster mismatch: ") + name);
  for (int ordinal = 0; ordinal < declaration->Count(); ++ordinal) {
    CViewObjectRef* reference =
        static_cast<CViewObjectRef*>((*declaration)[ordinal]);
    if (reference == nullptr || reference->Model() == nullptr ||
        reference->Model()->NumBaseSets() <= 0 ||
        reference->Model()->BaseSet(0).NumBases() <= 0 ||
        reference->GetAnimationCallback() != nullptr ||
        reference->GetUserAttrib() != nullptr)
      return Fail(std::string("scene reference already owned or invalid: ") +
                  name);
    std::unique_ptr<MechanismBinding> binding(new MechanismBinding());
    binding->kind = kind;
    binding->name = name;
    binding->ordinal = ordinal;
    binding->reference = reference;
    binding->base = &reference->Model()->BaseSet(0).Base(0);
    if (!ConfigureBinding(*binding) || !CaptureBinding(*binding)) {
      if (g_lastError.empty())
        g_lastError = std::string("mechanism model contract mismatch: ") +
                      name;
      return false;
    }
    binding->baselineFingerprint = PoseFingerprint(*binding);
    g_summary.restoredModifiers += binding->modifierCount;
    if (kind == MECHANISM_WTR_B05 || kind == MECHANISM_WTR_F04)
      ++g_summary.waterwheelBindings;
    else
      ++g_summary.flagBindings;
    g_bindings.push_back(std::move(binding));
  }
  return true;
}

void AttachAll() {
  for (std::size_t index = 0; index < g_bindings.size(); ++index) {
    MechanismBinding& binding = *g_bindings[index];
    binding.reference->SetUserAttrib(&binding);
    binding.reference->SetAnimationCallback(CallbackFor(binding.kind));
  }
}

void RestoreAll() {
  for (std::size_t index = 0; index < g_bindings.size(); ++index)
    RestoreBinding(*g_bindings[index]);
}

void AnimateAll() {
  for (std::size_t index = 0; index < g_bindings.size(); ++index) {
    MechanismBinding& binding = *g_bindings[index];
    CallbackFor(binding.kind)(nullptr, binding.base, binding.reference);
  }
}

bool Probe(double startTime) {
  const double savedViewTime = Session::m_viewTime;
  std::vector<unsigned long long> firstPoses;
  firstPoses.reserve(g_bindings.size());
  unsigned long long fingerprint = kHashOffset;

  RestoreAll();
  Session::m_viewTime = startTime;
  AnimateAll();
  for (std::size_t index = 0; index < g_bindings.size(); ++index) {
    const unsigned long long pose = PoseFingerprint(*g_bindings[index]);
    firstPoses.push_back(pose);
    HashString(fingerprint, g_bindings[index]->name.c_str());
    HashBytes(fingerprint, &g_bindings[index]->ordinal,
              sizeof(g_bindings[index]->ordinal));
    HashBytes(fingerprint, &pose, sizeof(pose));
    ++g_summary.sampledPoses;
  }

  RestoreAll();
  Session::m_viewTime = startTime + 0.5;
  AnimateAll();
  for (std::size_t index = 0; index < g_bindings.size(); ++index) {
    const unsigned long long pose = PoseFingerprint(*g_bindings[index]);
    if (pose != firstPoses[index]) ++g_summary.changedBindings;
    HashBytes(fingerprint, &pose, sizeof(pose));
    ++g_summary.sampledPoses;
  }

  RestoreAll();
  bool restored = true;
  for (std::size_t index = 0; index < g_bindings.size(); ++index)
    if (PoseFingerprint(*g_bindings[index]) !=
        g_bindings[index]->baselineFingerprint)
      restored = false;
  Session::m_viewTime = savedViewTime;
  if (!restored || g_summary.changedBindings !=
                       static_cast<int>(g_bindings.size()))
    return Fail("static mechanism live-pose or rollback proof failed");
  g_summary.fingerprint = fingerprint;
  return fingerprint != 0;
}

}  // namespace

bool RecoveredStaticMechanism_Initialize(
    CViewScene* scene, const char* levelDirectory, double startTime,
    SRecoveredStaticMechanismSummary* summary) {
  RecoveredStaticMechanism_Release();
  g_lastError.clear();
  if (summary == nullptr) return Fail("mechanism summary is unavailable");
  *summary = SRecoveredStaticMechanismSummary{};
  if (!std::isfinite(startTime))
    return Fail("mechanism start time is not finite");
  g_summary.targetLevel =
      EqualsAsciiInsensitive(DirectoryLeaf(levelDirectory), "Level.05D");
  if (!g_summary.targetLevel) {
    g_summary.initialized = true;
    *summary = g_summary;
    return true;
  }
  if (scene == nullptr ||
      !BindNamedReferences(*scene, "wtr_b05", MECHANISM_WTR_B05, 3) ||
      !BindNamedReferences(*scene, "wtr_f04", MECHANISM_WTR_F04, 8) ||
      !BindNamedReferences(*scene, "flg_civ", MECHANISM_FLG_CIV, 1) ||
      !BindNamedReferences(*scene, "flg_vill", MECHANISM_FLG_VILL, 1)) {
    if (scene == nullptr) g_lastError = "drawable scene is unavailable";
    const std::string error = g_lastError;
    RecoveredStaticMechanism_Release();
    g_lastError = error;
    return false;
  }
  g_summary.bindingCount = static_cast<int>(g_bindings.size());
  if (g_summary.bindingCount != 13) {
    g_lastError = "bounded mechanism roster proof failed";
    const std::string error = g_lastError;
    RecoveredStaticMechanism_Release();
    g_lastError = error;
    return false;
  }
  AttachAll();
  if (!Probe(startTime)) {
    if (g_lastError.empty()) g_lastError = "bounded mechanism proof failed";
    const std::string error = g_lastError;
    RecoveredStaticMechanism_Release();
    g_lastError = error;
    return false;
  }
  g_summary.initialized = true;
  *summary = g_summary;
  return true;
}

void RecoveredStaticMechanism_Release() {
  for (std::size_t index = 0; index < g_bindings.size(); ++index) {
    MechanismBinding& binding = *g_bindings[index];
    RestoreBinding(binding);
    if (binding.reference != nullptr &&
        binding.reference->GetUserAttrib() == &binding) {
      binding.reference->SetAnimationCallback(nullptr);
      binding.reference->SetUserAttrib(nullptr);
    }
  }
  g_bindings.clear();
  g_summary = SRecoveredStaticMechanismSummary{};
}

const SRecoveredStaticMechanismSummary* RecoveredStaticMechanism_State() {
  return g_summary.initialized ? &g_summary : nullptr;
}

const char* RecoveredStaticMechanism_LastError() {
  return g_lastError.c_str();
}
