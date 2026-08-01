#include "RecoveredWindowsInputAdapter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "hardware.h"

namespace {

double BoundedSensitivity(double value) {
  if (!std::isfinite(value)) return 1.0;
  return (std::max)(0.01, (std::min)(1.0, value));
}

bool IsKeyboardMessage(unsigned int message) {
  return message == WM_KEYDOWN || message == WM_KEYUP ||
         message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
}

bool IsMouseButtonMessage(unsigned int message) {
  return message == WM_LBUTTONDOWN || message == WM_LBUTTONUP;
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

}  // namespace

RecoveredWindowsInputAdapter::RecoveredWindowsInputAdapter() { Reset(); }

void RecoveredWindowsInputAdapter::Reset(bool applicationActive) {
  std::memset(keys_, 0, sizeof(keys_));
  mouseLeft_ = false;
  applicationActive_ = applicationActive;
  telemetry_ = {};
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
  if (mouseLeft_) return false;
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

bool RecoveredWindowsInputAdapter::FireDown() const {
  return mouseLeft_ || keys_[VK_LCONTROL];
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

  const bool fireBefore = FireDown();
  keys_[virtualKey] = down;
  const bool fireAfter = FireDown();
  switch (virtualKey) {
    case 'W':
    case 'S':
      return Emit(batch, MOVE_FORWARD, Axis('W', 'S', sensitivity),
                  virtualKey, FALSE);
    case 'D':
    case 'A':
      return Emit(batch, STRAFE_RIGHT, Axis('D', 'A', sensitivity),
                  virtualKey, FALSE);
    case 'T':
    case 'G':
      return Emit(batch, STRAFE_UP, Axis('T', 'G', sensitivity),
                  virtualKey, FALSE);
    case VK_RIGHT:
    case VK_LEFT:
      return Emit(batch, TURN_RIGHT,
                  Axis(VK_RIGHT, VK_LEFT, sensitivity), virtualKey, FALSE);
    case VK_UP:
    case VK_DOWN:
      return Emit(batch, LOOK_UP,
                  Axis(VK_UP, VK_DOWN, sensitivity), virtualKey, FALSE);
    case VK_SPACE:
      return Emit(batch, JUMP, down ? 1.0 : 0.0, virtualKey, FALSE);
    case VK_LCONTROL:
      if (fireBefore == fireAfter) return true;
      return Emit(batch, FIRE_PRIMARY, fireAfter ? 1.0 : 0.0,
                  virtualKey, FALSE);
    case 'X':
      return Emit(batch, STOP_VEHICLE, down ? 1.0 : 0.0,
                  virtualKey, FALSE);
    case VK_F1:
      return Emit(batch, CHANGE_VEHICLE, down ? 1.0 : 0.0,
                  virtualKey, FALSE);
    case VK_ESCAPE:
      return Emit(batch, EXIT, down ? 1.0 : 0.0, virtualKey, FALSE);
    case 'M':
      return down ? Emit(batch, DMAP_TOGGLE, 1.0, virtualKey, FALSE) : true;
    default:
      return true;
  }
}

bool RecoveredWindowsInputAdapter::HandleMouseButton(
    unsigned int message, SRecoveredWindowsInputBatch* batch) {
  if (!applicationActive_) {
    ++telemetry_.suppressedMessages;
    return true;
  }
  const bool down = message == WM_LBUTTONDOWN;
  if (down == mouseLeft_) {
    if (down)
      ++telemetry_.filteredRepeats;
    else
      ++telemetry_.redundantReleases;
    return true;
  }
  const bool fireBefore = FireDown();
  mouseLeft_ = down;
  const bool fireAfter = FireDown();
  if (fireBefore == fireAfter) return true;
  return Emit(batch, FIRE_PRIMARY, fireAfter ? 1.0 : 0.0,
              VK_LBUTTON, FALSE);
}

bool RecoveredWindowsInputAdapter::EmitFocusClear(
    double sensitivity, SRecoveredWindowsInputBatch* batch) {
  const double forward = Axis('W', 'S', sensitivity);
  const double strafe = Axis('D', 'A', sensitivity);
  const double vertical = Axis('T', 'G', sensitivity);
  const double turn = Axis(VK_RIGHT, VK_LEFT, sensitivity);
  const double look = Axis(VK_UP, VK_DOWN, sensitivity);
  const bool jump = keys_[VK_SPACE];
  const bool fire = FireDown();
  const bool stop = keys_['X'];
  const bool changeVehicle = keys_[VK_F1];
  std::memset(keys_, 0, sizeof(keys_));
  mouseLeft_ = false;

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
