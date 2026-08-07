#pragma once

#include <cstddef>
#include <cstdint>

enum ERecoveredInputBinding : std::size_t {
  RECOVERED_BIND_MOVE_FORWARD = 0,
  RECOVERED_BIND_MOVE_BACKWARD,
  RECOVERED_BIND_STRAFE_LEFT,
  RECOVERED_BIND_STRAFE_RIGHT,
  RECOVERED_BIND_MOVE_UP,
  RECOVERED_BIND_MOVE_DOWN,
  RECOVERED_BIND_TURN_LEFT,
  RECOVERED_BIND_TURN_RIGHT,
  RECOVERED_BIND_LOOK_UP,
  RECOVERED_BIND_LOOK_DOWN,
  RECOVERED_BIND_JUMP,
  RECOVERED_BIND_FIRE_PRIMARY,
  RECOVERED_BIND_FIRE_PRIMARY_ALTERNATE,
  RECOVERED_BIND_FIRE_SECONDARY,
  RECOVERED_BIND_STOP_VEHICLE,
  RECOVERED_BIND_CHANGE_VEHICLE,
  RECOVERED_BIND_MAP,
  RECOVERED_BIND_MAP_SCROLL_LEFT,
  RECOVERED_BIND_MAP_SCROLL_RIGHT,
  RECOVERED_BIND_MAP_SCROLL_UP,
  RECOVERED_BIND_MAP_SCROLL_DOWN,
  RECOVERED_BIND_MAP_TOGGLE_FOLLOW,
  RECOVERED_BIND_MAP_NEXT_MISSION,
  RECOVERED_BIND_MAP_PREVIOUS_MISSION,
  RECOVERED_BIND_MAP_TEXT_UP,
  RECOVERED_BIND_MAP_TEXT_DOWN,
  RECOVERED_BIND_COUNT
};

struct SRecoveredInputBindings {
  std::uint32_t key[RECOVERED_BIND_COUNT] = {};
};

SRecoveredInputBindings RecoveredWindowsInput_DefaultBindings();
bool RecoveredWindowsInput_ValidateBindings(
    const SRecoveredInputBindings& bindings, std::size_t* conflictFirst,
    std::size_t* conflictSecond);
const char* RecoveredWindowsInput_BindingName(std::size_t binding);
const char* RecoveredWindowsInput_KeyName(std::uint32_t key);

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
  bool SetBindings(const SRecoveredInputBindings& bindings);
  const SRecoveredInputBindings& Bindings() const { return bindings_; }
  void SetMapOverlayActive(bool active);
  bool EnterOverlay(double keySensitivity,
                    SRecoveredWindowsInputBatch* batch);
  void LeaveOverlay();
  bool ProcessWindowMessage(
      unsigned int message, std::uintptr_t wParam, std::intptr_t lParam,
      double keySensitivity, SRecoveredWindowsInputBatch* batch);

  bool ApplicationActive() const { return applicationActive_; }
  bool OverlayActive() const { return overlayActive_; }
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
  bool BoundDown(ERecoveredInputBinding binding) const;
  bool PrimaryFireDown() const;

  bool keys_[256] = {};
  bool mouseLeft_ = false;
  bool mouseRight_ = false;
  bool applicationActive_ = true;
  bool overlayActive_ = false;
  bool mapOverlayActive_ = false;
  SRecoveredInputBindings bindings_ = {};
  SRecoveredWindowsInputTelemetry telemetry_ = {};
};
