#include <cmath>
#include <cstdio>

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

  if (!input.ProcessWindowMessage(WM_LBUTTONDOWN, 0, 0, 1.0, &batch) ||
      !One(batch, FIRE_PRIMARY, 1.0) ||
      !SendKey(&input, WM_KEYDOWN, VK_CONTROL, 0, 1.0, &batch) ||
      batch.count != 0u ||
      !input.ProcessWindowMessage(WM_LBUTTONUP, 0, 0, 1.0, &batch) ||
      batch.count != 0u ||
      !SendKey(&input, WM_KEYUP, VK_CONTROL, 0, 1.0, &batch) ||
      !One(batch, FIRE_PRIMARY, 0.0)) {
    std::fprintf(stderr, "combined MouseL/LCtrl fire failed\n");
    return 6;
  }

  if (!input.ProcessWindowMessage(WM_RBUTTONDOWN, 0, 0, 1.0, &batch) ||
      !One(batch, FIRE_SECONDARY, 1.0) ||
      !input.ProcessWindowMessage(WM_RBUTTONUP, 0, 0, 1.0, &batch) ||
      !One(batch, FIRE_SECONDARY, 0.0)) {
    std::fprintf(stderr, "MouseR secondary fire failed\n");
    return 6;
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
    return 7;
  }
  for (std::size_t index = 0; index < batch.count; ++index) {
    if (batch.actions[index].value != 0.0) {
      std::fprintf(stderr, "focus clear emitted non-zero action\n");
      return 8;
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
    return 9;
  }

  const SRecoveredWindowsInputTelemetry& telemetry = input.Telemetry();
  if (telemetry.keyboardMessages != 28u ||
      telemetry.mouseButtonMessages != 6u || telemetry.focusMessages != 2u ||
      telemetry.emittedActions != 31u || telemetry.filteredRepeats != 2u ||
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
    return 10;
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
