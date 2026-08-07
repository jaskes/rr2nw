#include "RecoveredWindowsInputAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "hardware.h"

namespace {

const char* const kBindingNames[RECOVERED_BIND_COUNT] = {
    "Move forward", "Move backward", "Strafe left", "Strafe right",
    "Move up", "Move down", "Turn left", "Turn right", "Look up",
    "Look down", "Jump", "Primary fire", "Primary fire (alternate)",
    "Secondary fire", "Stop vehicle", "Change vehicle", "Map",
    "Map scroll left", "Map scroll right", "Map scroll up",
    "Map scroll down", "Map follow mode", "Map next mission",
    "Map previous mission", "Map text up", "Map text down"};

enum : unsigned int {
  kGameplayBindingDomain = 1u,
  kMapBindingDomain = 2u
};

unsigned int BindingDomain(std::size_t binding) {
  if (binding < RECOVERED_BIND_MAP) return kGameplayBindingDomain;
  if (binding == RECOVERED_BIND_MAP)
    return kGameplayBindingDomain | kMapBindingDomain;
  return kMapBindingDomain;
}

double BoundedSensitivity(double value) {
  if (!std::isfinite(value)) return 1.0;
  return (std::max)(0.01, (std::min)(1.0, value));
}

bool IsKeyboardMessage(unsigned int message) {
  return message == WM_KEYDOWN || message == WM_KEYUP ||
         message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
}

bool IsMouseButtonMessage(unsigned int message) {
  return message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
         message == WM_RBUTTONDOWN || message == WM_RBUTTONUP;
}

bool IsDownMessage(unsigned int message) {
  return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
}

std::uint32_t NormalizeVirtualKey(std::uint32_t key, std::intptr_t lParam) {
  if (key == VK_CONTROL)
    return (lParam & 0x01000000) != 0 ? VK_RCONTROL : VK_LCONTROL;
  if (key == VK_MENU)
    return (lParam & 0x01000000) != 0 ? VK_RMENU : VK_LMENU;
  return key;
}

std::uint32_t LegacyKeyCode(std::uint32_t key, std::intptr_t lParam) {
  // The archived Hardware table distinguishes the navigation cluster from
  // the numeric keypad by adding CTRL_EXTENDED_KEY to the Win32 virtual key.
  // Preserve that code in the semantic payload so DebugMap can reuse its
  // retail scroll-key comparison without polling the keyboard.
  if ((lParam & 0x01000000) != 0 && key != VK_RCONTROL && key != VK_RMENU)
    return key + CTRL_EXTENDED_KEY;
  return key;
}

}  // namespace

SRecoveredInputBindings RecoveredWindowsInput_DefaultBindings() {
  SRecoveredInputBindings result = {};
  result.key[RECOVERED_BIND_MOVE_FORWARD] = 'W';
  result.key[RECOVERED_BIND_MOVE_BACKWARD] = 'S';
  result.key[RECOVERED_BIND_STRAFE_LEFT] = 'A';
  result.key[RECOVERED_BIND_STRAFE_RIGHT] = 'D';
  result.key[RECOVERED_BIND_MOVE_UP] = 'T';
  result.key[RECOVERED_BIND_MOVE_DOWN] = 'G';
  result.key[RECOVERED_BIND_TURN_LEFT] = VK_LEFT;
  result.key[RECOVERED_BIND_TURN_RIGHT] = VK_RIGHT;
  result.key[RECOVERED_BIND_LOOK_UP] = VK_UP;
  result.key[RECOVERED_BIND_LOOK_DOWN] = VK_DOWN;
  result.key[RECOVERED_BIND_JUMP] = VK_SPACE;
  result.key[RECOVERED_BIND_FIRE_PRIMARY] = VK_LBUTTON;
  result.key[RECOVERED_BIND_FIRE_PRIMARY_ALTERNATE] = VK_LCONTROL;
  result.key[RECOVERED_BIND_FIRE_SECONDARY] = VK_RBUTTON;
  result.key[RECOVERED_BIND_STOP_VEHICLE] = 'X';
  result.key[RECOVERED_BIND_CHANGE_VEHICLE] = VK_F1;
  result.key[RECOVERED_BIND_MAP] = 'M';
  result.key[RECOVERED_BIND_MAP_SCROLL_LEFT] = VK_LEFT;
  result.key[RECOVERED_BIND_MAP_SCROLL_RIGHT] = VK_RIGHT;
  result.key[RECOVERED_BIND_MAP_SCROLL_UP] = VK_UP;
  result.key[RECOVERED_BIND_MAP_SCROLL_DOWN] = VK_DOWN;
  result.key[RECOVERED_BIND_MAP_TOGGLE_FOLLOW] = VK_DELETE;
  result.key[RECOVERED_BIND_MAP_NEXT_MISSION] = VK_OEM_6;
  result.key[RECOVERED_BIND_MAP_PREVIOUS_MISSION] = VK_OEM_4;
  result.key[RECOVERED_BIND_MAP_TEXT_UP] = VK_PRIOR;
  result.key[RECOVERED_BIND_MAP_TEXT_DOWN] = VK_NEXT;
  return result;
}

bool RecoveredWindowsInput_ValidateBindings(
    const SRecoveredInputBindings& bindings, std::size_t* conflictFirst,
    std::size_t* conflictSecond) {
  for (std::size_t index = 0; index < RECOVERED_BIND_COUNT; ++index) {
    const std::uint32_t key = bindings.key[index];
    const bool axisBinding = index <= RECOVERED_BIND_LOOK_DOWN;
    if (key == 0u || key >= 256u || key == VK_ESCAPE ||
        (axisBinding && (key == VK_LBUTTON || key == VK_RBUTTON))) {
      if (conflictFirst != nullptr) *conflictFirst = index;
      if (conflictSecond != nullptr) *conflictSecond = index;
      return false;
    }
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (bindings.key[previous] != key ||
          (BindingDomain(previous) & BindingDomain(index)) == 0u)
        continue;
      if (conflictFirst != nullptr) *conflictFirst = previous;
      if (conflictSecond != nullptr) *conflictSecond = index;
      return false;
    }
  }
  return true;
}

const char* RecoveredWindowsInput_BindingName(std::size_t binding) {
  return binding < RECOVERED_BIND_COUNT ? kBindingNames[binding] : "Unknown";
}

const char* RecoveredWindowsInput_KeyName(std::uint32_t key) {
  static char printable[2] = {};
  if (key >= 'A' && key <= 'Z') {
    printable[0] = static_cast<char>(key);
    printable[1] = '\0';
    return printable;
  }
  switch (key) {
    case VK_LBUTTON: return "Mouse left";
    case VK_RBUTTON: return "Mouse right";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_SPACE: return "Space";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_DELETE: return "Delete";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_OEM_4: return "[";
    case VK_OEM_6: return "]";
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    default: return "Key";
  }
}

RecoveredWindowsInputAdapter::RecoveredWindowsInputAdapter()
    : bindings_(RecoveredWindowsInput_DefaultBindings()) {
  Reset();
}

void RecoveredWindowsInputAdapter::Reset(bool applicationActive) {
  std::memset(keys_, 0, sizeof(keys_));
  mouseLeft_ = false;
  mouseRight_ = false;
  applicationActive_ = applicationActive;
  overlayActive_ = false;
  mapOverlayActive_ = false;
  telemetry_ = {};
}

bool RecoveredWindowsInputAdapter::SetBindings(
    const SRecoveredInputBindings& bindings) {
  if (!IsNeutral()) return false;
  if (!RecoveredWindowsInput_ValidateBindings(bindings, nullptr, nullptr))
    return false;
  bindings_ = bindings;
  return true;
}

void RecoveredWindowsInputAdapter::SetMapOverlayActive(bool active) {
  if (mapOverlayActive_ == active) return;
  // DebugMap owns an exclusive input context. Its open transition already
  // neutralizes the active Vehicle, so discard the adapter's physical latch
  // instead of replaying gameplay releases into the map owner.
  std::memset(keys_, 0, sizeof(keys_));
  mouseLeft_ = false;
  mouseRight_ = false;
  mapOverlayActive_ = active;
}

bool RecoveredWindowsInputAdapter::EnterOverlay(
    double keySensitivity, SRecoveredWindowsInputBatch* batch) {
  if (batch == nullptr) return false;
  *batch = {};
  if (overlayActive_) return true;
  if (!EmitFocusClear(BoundedSensitivity(keySensitivity), batch))
    return false;
  overlayActive_ = true;
  return true;
}

void RecoveredWindowsInputAdapter::LeaveOverlay() {
  overlayActive_ = false;
}

bool RecoveredWindowsInputAdapter::ProcessWindowMessage(
    unsigned int message, std::uintptr_t wParam, std::intptr_t lParam,
    double keySensitivity, SRecoveredWindowsInputBatch* batch) {
  if (batch == nullptr) return false;
  *batch = {};
  const double sensitivity = BoundedSensitivity(keySensitivity);
  if (message == WM_ACTIVATEAPP) {
    batch->consumed = false;
    ++telemetry_.focusMessages;
    return HandleFocus(wParam != FALSE, sensitivity, batch);
  }
  if (message == WM_CHAR || message == WM_DEADCHAR ||
      message == WM_SYSCHAR || message == WM_SYSDEADCHAR) {
    // TranslateMessage may synthesize character messages after a consumed
    // make. They belong to future text-entry UI, not the legacy translator.
    batch->consumed = true;
    ++telemetry_.keyboardMessages;
    return true;
  }
  if (overlayActive_ &&
      (IsKeyboardMessage(message) || IsMouseButtonMessage(message))) {
    batch->consumed = true;
    ++telemetry_.suppressedMessages;
    return true;
  }
  if (IsKeyboardMessage(message)) {
    batch->consumed = true;
    ++telemetry_.keyboardMessages;
    return HandleKeyboard(message, static_cast<std::uint32_t>(wParam),
                          lParam, sensitivity, batch);
  }
  if (IsMouseButtonMessage(message)) {
    batch->consumed = true;
    ++telemetry_.mouseButtonMessages;
    return HandleMouseButton(message, batch);
  }
  return true;
}

bool RecoveredWindowsInputAdapter::IsNeutral() const {
  if (mouseLeft_ || mouseRight_) return false;
  for (bool key : keys_)
    if (key) return false;
  return true;
}

bool RecoveredWindowsInputAdapter::Emit(
    SRecoveredWindowsInputBatch* batch, int action, double value,
    std::uint32_t code, int repeat) {
  if (batch == nullptr || !std::isfinite(value) || value < -1.0 ||
      value > 1.0 || batch->count >= RECOVERED_WINDOWS_INPUT_MAX_ACTIONS)
    return false;
  SRecoveredWindowsInputAction& output = batch->actions[batch->count++];
  output.action = action;
  output.value = value;
  output.code = code;
  output.repeat = repeat;
  ++telemetry_.emittedActions;
  return true;
}

double RecoveredWindowsInputAdapter::Axis(
    std::uint32_t positive, std::uint32_t negative,
    double sensitivity) const {
  return ((keys_[positive] ? 1.0 : 0.0) -
          (keys_[negative] ? 1.0 : 0.0)) * sensitivity;
}

bool RecoveredWindowsInputAdapter::BoundDown(
    ERecoveredInputBinding binding) const {
  const std::uint32_t key = bindings_.key[binding];
  if (key == VK_LBUTTON) return mouseLeft_;
  if (key == VK_RBUTTON) return mouseRight_;
  return key < 256u && keys_[key];
}

bool RecoveredWindowsInputAdapter::PrimaryFireDown() const {
  return BoundDown(RECOVERED_BIND_FIRE_PRIMARY) ||
         BoundDown(RECOVERED_BIND_FIRE_PRIMARY_ALTERNATE);
}

bool RecoveredWindowsInputAdapter::HandleKeyboard(
    unsigned int message, std::uint32_t virtualKey, std::intptr_t lParam,
    double sensitivity, SRecoveredWindowsInputBatch* batch) {
  const bool down = IsDownMessage(message);
  const bool messageRepeat = down && (lParam & 0x40000000) != 0;
  virtualKey = NormalizeVirtualKey(virtualKey, lParam);
  if (virtualKey >= 256u) return true;
  if (!applicationActive_) {
    ++telemetry_.suppressedMessages;
    return true;
  }

  const bool previous = keys_[virtualKey];
  if (down && (previous || messageRepeat)) {
    ++telemetry_.filteredRepeats;
    return true;
  }
  if (!down && !previous) {
    ++telemetry_.redundantReleases;
    return true;
  }

  const bool fireBefore = PrimaryFireDown();
  keys_[virtualKey] = down;
  const bool fireAfter = PrimaryFireDown();
  const std::uint32_t legacyCode = LegacyKeyCode(virtualKey, lParam);
  const auto matches = [this, virtualKey](ERecoveredInputBinding binding) {
    return bindings_.key[binding] == virtualKey;
  };
  if (matches(RECOVERED_BIND_MAP))
    return down ? Emit(batch, DMAP_TOGGLE, 1.0, virtualKey, FALSE) : true;
  if (mapOverlayActive_) {
    struct MapBinding {
      ERecoveredInputBinding binding;
      int action;
    };
    const MapBinding mapBindings[] = {
        {RECOVERED_BIND_MAP_SCROLL_LEFT, DMAP_SCROLL_LEFT},
        {RECOVERED_BIND_MAP_SCROLL_RIGHT, DMAP_SCROLL_RIGHT},
        {RECOVERED_BIND_MAP_SCROLL_UP, DMAP_SCROLL_UP},
        {RECOVERED_BIND_MAP_SCROLL_DOWN, DMAP_SCROLL_DOWN},
        {RECOVERED_BIND_MAP_TOGGLE_FOLLOW, DMAP_TOGGLE_FOLLOW_MODE},
        {RECOVERED_BIND_MAP_NEXT_MISSION, DMAP_NEXT_MISSION},
        {RECOVERED_BIND_MAP_PREVIOUS_MISSION, DMAP_PREVIOUS_MISSION},
        {RECOVERED_BIND_MAP_TEXT_UP, DMAP_TEXT_BOX_UP},
        {RECOVERED_BIND_MAP_TEXT_DOWN, DMAP_TEXT_BOX_DOWN}};
    for (const MapBinding& mapBinding : mapBindings) {
      if (matches(mapBinding.binding))
        return down ? Emit(batch, mapBinding.action, 1.0,
                           legacyCode, FALSE) : true;
    }
    return true;
  }
  if (matches(RECOVERED_BIND_MOVE_FORWARD) ||
      matches(RECOVERED_BIND_MOVE_BACKWARD))
    return Emit(batch, MOVE_FORWARD,
                Axis(bindings_.key[RECOVERED_BIND_MOVE_FORWARD],
                     bindings_.key[RECOVERED_BIND_MOVE_BACKWARD],
                     sensitivity), legacyCode, FALSE);
  if (matches(RECOVERED_BIND_STRAFE_RIGHT) ||
      matches(RECOVERED_BIND_STRAFE_LEFT))
    return Emit(batch, STRAFE_RIGHT,
                Axis(bindings_.key[RECOVERED_BIND_STRAFE_RIGHT],
                     bindings_.key[RECOVERED_BIND_STRAFE_LEFT],
                     sensitivity), legacyCode, FALSE);
  if (matches(RECOVERED_BIND_MOVE_UP) ||
      matches(RECOVERED_BIND_MOVE_DOWN))
    return Emit(batch, STRAFE_UP,
                Axis(bindings_.key[RECOVERED_BIND_MOVE_UP],
                     bindings_.key[RECOVERED_BIND_MOVE_DOWN],
                     sensitivity), legacyCode, FALSE);
  if (matches(RECOVERED_BIND_TURN_RIGHT) ||
      matches(RECOVERED_BIND_TURN_LEFT))
    return Emit(batch, TURN_RIGHT,
                Axis(bindings_.key[RECOVERED_BIND_TURN_RIGHT],
                     bindings_.key[RECOVERED_BIND_TURN_LEFT],
                     sensitivity), legacyCode, FALSE);
  if (matches(RECOVERED_BIND_LOOK_UP) ||
      matches(RECOVERED_BIND_LOOK_DOWN))
    return Emit(batch, LOOK_UP,
                Axis(bindings_.key[RECOVERED_BIND_LOOK_UP],
                     bindings_.key[RECOVERED_BIND_LOOK_DOWN],
                     sensitivity), legacyCode, FALSE);
  if (matches(RECOVERED_BIND_JUMP))
    return Emit(batch, JUMP, down ? 1.0 : 0.0, virtualKey, FALSE);
  if (matches(RECOVERED_BIND_FIRE_PRIMARY) ||
      matches(RECOVERED_BIND_FIRE_PRIMARY_ALTERNATE)) {
    if (fireBefore == fireAfter) return true;
    return Emit(batch, FIRE_PRIMARY, fireAfter ? 1.0 : 0.0,
                virtualKey, FALSE);
  }
  if (matches(RECOVERED_BIND_FIRE_SECONDARY))
    return Emit(batch, FIRE_SECONDARY, down ? 1.0 : 0.0,
                virtualKey, FALSE);
  if (matches(RECOVERED_BIND_STOP_VEHICLE))
    return Emit(batch, STOP_VEHICLE, down ? 1.0 : 0.0,
                virtualKey, FALSE);
  if (matches(RECOVERED_BIND_CHANGE_VEHICLE))
    return Emit(batch, CHANGE_VEHICLE, down ? 1.0 : 0.0,
                virtualKey, FALSE);
  return true;
}

bool RecoveredWindowsInputAdapter::HandleMouseButton(
    unsigned int message, SRecoveredWindowsInputBatch* batch) {
  if (!applicationActive_) {
    ++telemetry_.suppressedMessages;
    return true;
  }
  const bool secondary = message == WM_RBUTTONDOWN ||
                         message == WM_RBUTTONUP;
  const bool down = message == WM_LBUTTONDOWN ||
                    message == WM_RBUTTONDOWN;
  bool& held = secondary ? mouseRight_ : mouseLeft_;
  if (down == held) {
    if (down)
      ++telemetry_.filteredRepeats;
    else
      ++telemetry_.redundantReleases;
    return true;
  }
  const bool fireBefore = PrimaryFireDown();
  held = down;
  const bool fireAfter = PrimaryFireDown();
  const std::uint32_t key = secondary ? VK_RBUTTON : VK_LBUTTON;
  if (bindings_.key[RECOVERED_BIND_FIRE_PRIMARY] == key ||
      bindings_.key[RECOVERED_BIND_FIRE_PRIMARY_ALTERNATE] == key) {
    if (fireBefore == fireAfter) return true;
    return Emit(batch, FIRE_PRIMARY, fireAfter ? 1.0 : 0.0, key, FALSE);
  }
  if (bindings_.key[RECOVERED_BIND_FIRE_SECONDARY] == key)
    return Emit(batch, FIRE_SECONDARY, down ? 1.0 : 0.0, key, FALSE);
  if (bindings_.key[RECOVERED_BIND_MAP] == key)
    return down ? Emit(batch, DMAP_TOGGLE, 1.0, key, FALSE) : true;
  if (mapOverlayActive_) {
    struct MapBinding {
      ERecoveredInputBinding binding;
      int action;
    };
    const MapBinding mapBindings[] = {
        {RECOVERED_BIND_MAP_SCROLL_LEFT, DMAP_SCROLL_LEFT},
        {RECOVERED_BIND_MAP_SCROLL_RIGHT, DMAP_SCROLL_RIGHT},
        {RECOVERED_BIND_MAP_SCROLL_UP, DMAP_SCROLL_UP},
        {RECOVERED_BIND_MAP_SCROLL_DOWN, DMAP_SCROLL_DOWN},
        {RECOVERED_BIND_MAP_TOGGLE_FOLLOW, DMAP_TOGGLE_FOLLOW_MODE},
        {RECOVERED_BIND_MAP_NEXT_MISSION, DMAP_NEXT_MISSION},
        {RECOVERED_BIND_MAP_PREVIOUS_MISSION, DMAP_PREVIOUS_MISSION},
        {RECOVERED_BIND_MAP_TEXT_UP, DMAP_TEXT_BOX_UP},
        {RECOVERED_BIND_MAP_TEXT_DOWN, DMAP_TEXT_BOX_DOWN}};
    for (const MapBinding& mapBinding : mapBindings) {
      if (bindings_.key[mapBinding.binding] == key)
        return down ? Emit(batch, mapBinding.action, 1.0, key, FALSE)
                    : true;
    }
    return true;
  }
  struct GameplayBinding {
    ERecoveredInputBinding binding;
    int action;
  };
  const GameplayBinding gameplayBindings[] = {
      {RECOVERED_BIND_JUMP, JUMP},
      {RECOVERED_BIND_STOP_VEHICLE, STOP_VEHICLE},
      {RECOVERED_BIND_CHANGE_VEHICLE, CHANGE_VEHICLE}};
  for (const GameplayBinding& gameplayBinding : gameplayBindings) {
    if (bindings_.key[gameplayBinding.binding] == key)
      return Emit(batch, gameplayBinding.action, down ? 1.0 : 0.0,
                  key, FALSE);
  }
  return true;
}

bool RecoveredWindowsInputAdapter::EmitFocusClear(
    double sensitivity, SRecoveredWindowsInputBatch* batch) {
  const double forward = Axis(bindings_.key[RECOVERED_BIND_MOVE_FORWARD],
                              bindings_.key[RECOVERED_BIND_MOVE_BACKWARD],
                              sensitivity);
  const double strafe = Axis(bindings_.key[RECOVERED_BIND_STRAFE_RIGHT],
                             bindings_.key[RECOVERED_BIND_STRAFE_LEFT],
                             sensitivity);
  const double vertical = Axis(bindings_.key[RECOVERED_BIND_MOVE_UP],
                               bindings_.key[RECOVERED_BIND_MOVE_DOWN],
                               sensitivity);
  const double turn = Axis(bindings_.key[RECOVERED_BIND_TURN_RIGHT],
                           bindings_.key[RECOVERED_BIND_TURN_LEFT],
                           sensitivity);
  const double look = Axis(bindings_.key[RECOVERED_BIND_LOOK_UP],
                           bindings_.key[RECOVERED_BIND_LOOK_DOWN],
                           sensitivity);
  const bool jump = BoundDown(RECOVERED_BIND_JUMP);
  const bool fire = PrimaryFireDown();
  const bool secondaryFire = BoundDown(RECOVERED_BIND_FIRE_SECONDARY);
  const bool stop = BoundDown(RECOVERED_BIND_STOP_VEHICLE);
  const bool changeVehicle = BoundDown(RECOVERED_BIND_CHANGE_VEHICLE);
  std::memset(keys_, 0, sizeof(keys_));
  mouseLeft_ = false;
  mouseRight_ = false;

  const auto clear = [this, batch](bool held, int action,
                                   std::uint32_t code) {
    if (!held) return true;
    if (!Emit(batch, action, 0.0, code, FALSE)) return false;
    ++telemetry_.focusClearActions;
    return true;
  };
  return clear(forward != 0.0, MOVE_FORWARD, 0u) &&
         clear(strafe != 0.0, STRAFE_RIGHT, 0u) &&
         clear(vertical != 0.0, STRAFE_UP, 0u) &&
         clear(turn != 0.0, TURN_RIGHT, 0u) &&
         clear(look != 0.0, LOOK_UP, 0u) &&
         clear(jump, JUMP, VK_SPACE) &&
         clear(fire, FIRE_PRIMARY, VK_LBUTTON) &&
         clear(secondaryFire, FIRE_SECONDARY, VK_RBUTTON) &&
         clear(stop, STOP_VEHICLE, 'X') &&
         clear(changeVehicle, CHANGE_VEHICLE, VK_F1);
}

bool RecoveredWindowsInputAdapter::HandleFocus(
  bool active, double sensitivity, SRecoveredWindowsInputBatch* batch) {
  if (active == applicationActive_) return true;
  if (!active && !EmitFocusClear(sensitivity, batch)) return false;
  applicationActive_ = active;
  batch->applicationActiveChanged = true;
  batch->applicationActive = active;
  return batch->count <= RECOVERED_WINDOWS_INPUT_MAX_ACTIONS;
}
