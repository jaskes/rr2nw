#pragma once

#include <cstddef>
#include <cstdint>

enum : std::size_t {
  RECOVERED_WINDOWS_INPUT_MAX_ACTIONS = 16u
};

struct SRecoveredWindowsInputAction {
  int action = -1;
  double value = 0.0;
  std::uint32_t code = 0;
  int repeat = 0;
};

struct SRecoveredWindowsInputBatch {
  SRecoveredWindowsInputAction actions[RECOVERED_WINDOWS_INPUT_MAX_ACTIONS] = {};
  std::size_t count = 0;
  bool consumed = false;
  bool applicationActiveChanged = false;
  bool applicationActive = true;
};

struct SRecoveredWindowsInputTelemetry {
  std::uint64_t keyboardMessages = 0;
  std::uint64_t mouseButtonMessages = 0;
  std::uint64_t focusMessages = 0;
  std::uint64_t emittedActions = 0;
  std::uint64_t filteredRepeats = 0;
  std::uint64_t redundantReleases = 0;
  std::uint64_t suppressedMessages = 0;
  std::uint64_t focusClearActions = 0;
};

// Owns the physical state used by the recovered Windows gameplay path. The
// adapter consumes raw Win32 keyboard and mouse-button messages and publishes
// only semantic action snapshots; simulation code never polls a key.
class RecoveredWindowsInputAdapter {
 public:
  RecoveredWindowsInputAdapter();

  void Reset(bool applicationActive = true);
  bool ProcessWindowMessage(
      unsigned int message, std::uintptr_t wParam, std::intptr_t lParam,
      double keySensitivity, SRecoveredWindowsInputBatch* batch);

  bool ApplicationActive() const { return applicationActive_; }
  bool IsNeutral() const;
  const SRecoveredWindowsInputTelemetry& Telemetry() const {
    return telemetry_;
  }

 private:
  bool Emit(SRecoveredWindowsInputBatch* batch, int action, double value,
            std::uint32_t code, int repeat);
  bool HandleKeyboard(unsigned int message, std::uint32_t virtualKey,
                      std::intptr_t lParam, double sensitivity,
                      SRecoveredWindowsInputBatch* batch);
  bool HandleMouseButton(unsigned int message,
                         SRecoveredWindowsInputBatch* batch);
  bool HandleFocus(bool active, double sensitivity,
                   SRecoveredWindowsInputBatch* batch);
  bool EmitFocusClear(double sensitivity,
                      SRecoveredWindowsInputBatch* batch);
  double Axis(std::uint32_t positive, std::uint32_t negative,
              double sensitivity) const;
  bool FireDown() const;

  bool keys_[256] = {};
  bool mouseLeft_ = false;
  bool applicationActive_ = true;
  SRecoveredWindowsInputTelemetry telemetry_ = {};
};
