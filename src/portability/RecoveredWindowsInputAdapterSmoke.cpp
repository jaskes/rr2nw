#include <cmath>
#include <cstdio>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "RecoveredWindowsInputAdapter.h"
#include "hardware.h"

namespace {

bool One(const SRecoveredWindowsInputBatch& batch, int action, double value) {
  return batch.consumed && batch.count == 1u &&
         batch.actions[0].action == action &&
         std::fabs(batch.actions[0].value - value) <= 1.0e-12 &&
         batch.actions[0].repeat == 0;
}

bool OneCode(const SRecoveredWindowsInputBatch& batch, int action,
             std::uint32_t code) {
  return One(batch, action, 1.0) && batch.actions[0].code == code;
}

bool SendKey(RecoveredWindowsInputAdapter* input, unsigned int message,
             WPARAM key, LPARAM flags, double sensitivity,
             SRecoveredWindowsInputBatch* batch) {
  return input->ProcessWindowMessage(message, key, flags,
                                     sensitivity, batch);
}

bool Focus(RecoveredWindowsInputAdapter* input, bool active,
           SRecoveredWindowsInputBatch* batch) {
  return input->ProcessWindowMessage(WM_ACTIVATEAPP, active ? TRUE : FALSE,
                                     0, 0.75, batch);
}

}  // namespace

int main() {
  RecoveredWindowsInputAdapter input;
  SRecoveredWindowsInputBatch batch = {};

  if (!SendKey(&input, WM_KEYDOWN, 'W', 0, 0.75, &batch) ||
      !One(batch, MOVE_FORWARD, 0.75) ||
      !SendKey(&input, WM_KEYDOWN, 'W', 0x40000000, 0.75, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYDOWN, 'S', 0, 0.75, &batch) ||
      !One(batch, MOVE_FORWARD, 0.0) ||
      !SendKey(&input, WM_KEYUP, 'W', 0, 0.75, &batch) ||
      !One(batch, MOVE_FORWARD, -0.75) ||
      !SendKey(&input, WM_KEYUP, 'S', 0, 0.75, &batch) ||
      !One(batch, MOVE_FORWARD, 0.0)) {
    std::fprintf(stderr, "forward overlap/repeat sequence failed\n");
    return 1;
  }

  if (!SendKey(&input, WM_KEYDOWN, 'A', 0, 1.0, &batch) ||
      !One(batch, STRAFE_RIGHT, -1.0) ||
      !SendKey(&input, WM_KEYDOWN, 'D', 0, 1.0, &batch) ||
      !One(batch, STRAFE_RIGHT, 0.0) ||
      !SendKey(&input, WM_KEYUP, 'A', 0, 1.0, &batch) ||
      !One(batch, STRAFE_RIGHT, 1.0) ||
      !SendKey(&input, WM_KEYUP, 'D', 0, 1.0, &batch) ||
      !One(batch, STRAFE_RIGHT, 0.0)) {
    std::fprintf(stderr, "strafe reverse release order failed\n");
    return 2;
  }

  if (!SendKey(&input, WM_KEYDOWN, VK_LEFT, 0x01000000, 0.5, &batch) ||
      !One(batch, TURN_RIGHT, -0.5) ||
      !SendKey(&input, WM_KEYDOWN, VK_RIGHT, 0x01000000, 0.5, &batch) ||
      !One(batch, TURN_RIGHT, 0.0) ||
      !SendKey(&input, WM_KEYUP, VK_LEFT, 0x01000000, 0.5, &batch) ||
      !One(batch, TURN_RIGHT, 0.5) ||
      !SendKey(&input, WM_KEYUP, VK_RIGHT, 0x01000000, 0.5, &batch) ||
      !One(batch, TURN_RIGHT, 0.0)) {
    std::fprintf(stderr, "extended arrow sequence failed\n");
    return 3;
  }

  if (!SendKey(&input, WM_KEYDOWN, VK_SPACE, 0, 1.0, &batch) ||
      !One(batch, JUMP, 1.0) ||
      !SendKey(&input, WM_KEYUP, VK_SPACE, 0, 1.0, &batch) ||
      !One(batch, JUMP, 0.0) ||
      !SendKey(&input, WM_KEYDOWN, 'M', 0, 1.0, &batch) ||
      !One(batch, DMAP_TOGGLE, 1.0) ||
      !SendKey(&input, WM_KEYUP, 'M', 0, 1.0, &batch) ||
      batch.count != 0u) {
    std::fprintf(stderr, "Space/Map semantic edges failed\n");
    return 4;
  }
  if (!input.ProcessWindowMessage(WM_CHAR, 'W', 0, 1.0, &batch) ||
      !batch.consumed || batch.count != 0u) {
    std::fprintf(stderr, "character message escaped the adapter\n");
    return 5;
  }
  if (!input.ProcessWindowMessage(WM_SYSCHAR, 'W', 0, 1.0, &batch) ||
      !batch.consumed || batch.count != 0u) {
    std::fprintf(stderr, "system character message escaped the adapter\n");
    return 5;
  }

  if (!SendKey(&input, WM_KEYDOWN, VK_DELETE, 0x01000000, 1.0, &batch) ||
      !OneCode(batch, DMAP_TOGGLE_FOLLOW_MODE,
               VK_DELETE + CTRL_EXTENDED_KEY) ||
      !SendKey(&input, WM_KEYUP, VK_DELETE, 0x01000000, 1.0, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYDOWN, VK_OEM_6, 0, 1.0, &batch) ||
      !OneCode(batch, DMAP_NEXT_MISSION, VK_OEM_6) ||
      !SendKey(&input, WM_KEYUP, VK_OEM_6, 0, 1.0, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYDOWN, VK_OEM_4, 0, 1.0, &batch) ||
      !OneCode(batch, DMAP_PREVIOUS_MISSION, VK_OEM_4) ||
      !SendKey(&input, WM_KEYUP, VK_OEM_4, 0, 1.0, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYDOWN, VK_PRIOR, 0x01000000, 1.0, &batch) ||
      !OneCode(batch, DMAP_TEXT_BOX_UP, VK_PRIOR + CTRL_EXTENDED_KEY) ||
      !SendKey(&input, WM_KEYUP, VK_PRIOR, 0x01000000, 1.0, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYDOWN, VK_NEXT, 0x01000000, 1.0, &batch) ||
      !OneCode(batch, DMAP_TEXT_BOX_DOWN, VK_NEXT + CTRL_EXTENDED_KEY) ||
      !SendKey(&input, WM_KEYUP, VK_NEXT, 0x01000000, 1.0, &batch) ||
      batch.count != 0u) {
    std::fprintf(stderr, "retail map navigation bindings failed\n");
    return 6;
  }

  if (!input.ProcessWindowMessage(WM_LBUTTONDOWN, 0, 0, 1.0, &batch) ||
      !One(batch, FIRE_PRIMARY, 1.0) ||
      !SendKey(&input, WM_KEYDOWN, VK_CONTROL, 0, 1.0, &batch) ||
      batch.count != 0u ||
      !input.ProcessWindowMessage(WM_LBUTTONUP, 0, 0, 1.0, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYUP, VK_CONTROL, 0, 1.0, &batch) ||
      !One(batch, FIRE_PRIMARY, 0.0)) {
    std::fprintf(stderr, "combined MouseL/LCtrl fire failed\n");
    return 7;
  }

  if (!input.ProcessWindowMessage(WM_RBUTTONDOWN, 0, 0, 1.0, &batch) ||
      !One(batch, FIRE_SECONDARY, 1.0) ||
      !input.ProcessWindowMessage(WM_RBUTTONUP, 0, 0, 1.0, &batch) ||
      !One(batch, FIRE_SECONDARY, 0.0)) {
    std::fprintf(stderr, "MouseR secondary fire failed\n");
    return 7;
  }

  if (!SendKey(&input, WM_KEYDOWN, 'W', 0, 0.75, &batch) ||
      !SendKey(&input, WM_KEYDOWN, 'T', 0, 0.75, &batch) ||
      !SendKey(&input, WM_KEYDOWN, VK_SPACE, 0, 0.75, &batch) ||
      !input.ProcessWindowMessage(WM_LBUTTONDOWN, 0, 0, 0.75, &batch) ||
      !input.ProcessWindowMessage(WM_RBUTTONDOWN, 0, 0, 0.75, &batch) ||
      !Focus(&input, false, &batch) || batch.consumed ||
      !batch.applicationActiveChanged || batch.applicationActive ||
      batch.count != 5u || !input.IsNeutral()) {
    std::fprintf(stderr, "focus-loss clear failed\n");
    return 8;
  }
  for (std::size_t index = 0; index < batch.count; ++index) {
    if (batch.actions[index].value != 0.0) {
      std::fprintf(stderr, "focus clear emitted non-zero action\n");
      return 9;
    }
  }

  if (!SendKey(&input, WM_KEYDOWN, 'W', 0, 1.0, &batch) ||
      batch.count != 0u || !Focus(&input, true, &batch) ||
      !batch.applicationActiveChanged || !batch.applicationActive ||
      !SendKey(&input, WM_KEYDOWN, 'W', 0x40000000, 1.0, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYDOWN, 'W', 0, 1.0, &batch) ||
      !One(batch, MOVE_FORWARD, 1.0) ||
      !SendKey(&input, WM_KEYUP, 'W', 0, 1.0, &batch) ||
      !One(batch, MOVE_FORWARD, 0.0) || !input.IsNeutral()) {
    std::fprintf(stderr, "inactive/reacquire sequence failed\n");
    return 10;
  }

  const SRecoveredWindowsInputTelemetry& telemetry = input.Telemetry();
  if (telemetry.keyboardMessages != 38u ||
      telemetry.mouseButtonMessages != 6u || telemetry.focusMessages != 2u ||
      telemetry.emittedActions != 36u || telemetry.filteredRepeats != 2u ||
      telemetry.suppressedMessages != 1u || telemetry.focusClearActions != 5u) {
    std::fprintf(stderr,
                 "telemetry mismatch keys=%llu mouse=%llu focus=%llu "
                 "actions=%llu repeat=%llu suppressed=%llu clears=%llu\n",
                 static_cast<unsigned long long>(telemetry.keyboardMessages),
                 static_cast<unsigned long long>(telemetry.mouseButtonMessages),
                 static_cast<unsigned long long>(telemetry.focusMessages),
                 static_cast<unsigned long long>(telemetry.emittedActions),
                 static_cast<unsigned long long>(telemetry.filteredRepeats),
                 static_cast<unsigned long long>(telemetry.suppressedMessages),
                 static_cast<unsigned long long>(telemetry.focusClearActions));
    return 11;
  }

  SRecoveredInputBindings bindings =
      RecoveredWindowsInput_DefaultBindings();
  std::size_t conflictFirst = 99u;
  std::size_t conflictSecond = 99u;
  if (!RecoveredWindowsInput_ValidateBindings(
          bindings, &conflictFirst, &conflictSecond) ||
      std::strcmp(RecoveredWindowsInput_BindingName(
                      RECOVERED_BIND_MOVE_FORWARD),
                  "Move forward") != 0 ||
      std::strcmp(RecoveredWindowsInput_KeyName(
                      bindings.key[RECOVERED_BIND_FIRE_PRIMARY]),
                  "Mouse left") != 0) {
    std::fprintf(stderr, "default binding catalog failed\n");
    return 12;
  }
  bindings.key[RECOVERED_BIND_MOVE_FORWARD] = 'Z';
  bindings.key[RECOVERED_BIND_MOVE_BACKWARD] = 'Z';
  if (RecoveredWindowsInput_ValidateBindings(
          bindings, &conflictFirst, &conflictSecond) ||
      conflictFirst != RECOVERED_BIND_MOVE_FORWARD ||
      conflictSecond != RECOVERED_BIND_MOVE_BACKWARD) {
    std::fprintf(stderr, "binding conflict was not diagnosed\n");
    return 13;
  }
  bindings.key[RECOVERED_BIND_MOVE_BACKWARD] = 'S';
  RecoveredWindowsInputAdapter rebound;
  if (!rebound.SetBindings(bindings) ||
      !SendKey(&rebound, WM_KEYDOWN, 'Z', 0, 0.5, &batch) ||
      !One(batch, MOVE_FORWARD, 0.5)) {
    std::fprintf(stderr, "rebound forward action failed\n");
    return 14;
  }
  SRecoveredWindowsInputBatch releases = {};
  if (!rebound.EnterOverlay(0.5, &releases) ||
      !rebound.OverlayActive() || releases.count != 1u ||
      releases.actions[0].action != MOVE_FORWARD ||
      releases.actions[0].value != 0.0 || !rebound.IsNeutral() ||
      !SendKey(&rebound, WM_KEYDOWN, 'Z', 0, 0.5, &batch) ||
      !batch.consumed || batch.count != 0u) {
    std::fprintf(stderr, "overlay neutralization/suppression failed\n");
    return 15;
  }
  rebound.LeaveOverlay();
  if (!SendKey(&rebound, WM_KEYDOWN, 'Z', 0, 0.5, &batch) ||
      !One(batch, MOVE_FORWARD, 0.5) ||
      !SendKey(&rebound, WM_KEYUP, 'Z', 0, 0.5, &batch) ||
      !One(batch, MOVE_FORWARD, 0.0)) {
    std::fprintf(stderr, "overlay release did not reacquire bindings\n");
    return 16;
  }

  std::printf(
      "status=ok keyboard=%llu mouse=%llu focus=%llu actions=%llu "
      "repeat_filtered=%llu focus_clears=%llu\n",
      static_cast<unsigned long long>(telemetry.keyboardMessages),
      static_cast<unsigned long long>(telemetry.mouseButtonMessages),
      static_cast<unsigned long long>(telemetry.focusMessages),
      static_cast<unsigned long long>(telemetry.emittedActions),
      static_cast<unsigned long long>(telemetry.filteredRepeats),
      static_cast<unsigned long long>(telemetry.focusClearActions));
  return 0;
}
