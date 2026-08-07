#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <string>

using TRecoveredSoftwareWindowMessageHook =
    LRESULT (*)(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

enum ERecoveredWindowMode {
  RECOVERED_WINDOW_MODE_WINDOWED = 0,
  RECOVERED_WINDOW_MODE_BORDERLESS = 1,
  RECOVERED_WINDOW_MODE_EXCLUSIVE = 2
};

struct SRecoveredWindowPresentation {
  ERecoveredWindowMode mode = RECOVERED_WINDOW_MODE_WINDOWED;
  int clientWidth = 640;
  int clientHeight = 480;
  int bitsPerPixel = 0;
  int displayFrequency = 0;
  std::wstring displayDevice;
};

struct SRecoveredDisplayMode {
  int width = 0;
  int height = 0;
  int bitsPerPixel = 0;
  int displayFrequency = 0;
  bool current = false;
  std::wstring displayDevice;
};

struct SRecoveredWindowsPresentationState {
  bool dpiAware = false;
  bool recoveryConfigured = false;
  bool staleModeRecovered = false;
  unsigned int corruptRecoveryMarkers = 0;
  unsigned int recoveryFailures = 0;
  bool exclusiveActive = false;
  bool exclusiveSuspended = false;
  unsigned int catalogRefreshes = 0;
  unsigned int exclusiveApplies = 0;
  unsigned int desktopRestores = 0;
  unsigned int focusSuspends = 0;
  unsigned int focusResumes = 0;
  unsigned int focusFallbacks = 0;
  unsigned int dpiChanges = 0;
  unsigned int displayChanges = 0;
};

int RecoveredSoftwareGraph_Initialize(HINSTANCE instance);
bool RecoveredSoftwareGraph_IsReady();
int RecoveredSoftwareGraph_Width();
int RecoveredSoftwareGraph_Height();
bool RecoveredSoftwareGraph_ApplyPresentation(
    const SRecoveredWindowPresentation& presentation,
    SRecoveredWindowPresentation* previous = nullptr);
SRecoveredWindowPresentation RecoveredSoftwareGraph_Presentation();
unsigned long RecoveredSoftwareGraph_LastPresentationError();
bool RecoveredSoftwareGraph_ValidatePresentation(
    const SRecoveredWindowPresentation& presentation);
bool RecoveredSoftwareGraph_ConfigureDisplayRecovery(
    const std::wstring& recoveryPath);
bool RecoveredSoftwareGraph_RefreshDisplayModes();
std::size_t RecoveredSoftwareGraph_DisplayModeCount();
bool RecoveredSoftwareGraph_DisplayMode(
    std::size_t index, SRecoveredDisplayMode* mode);
bool RecoveredSoftwareGraph_FindDisplayMode(
    int width, int height, int bitsPerPixel, int displayFrequency,
    std::size_t* index);
SRecoveredWindowsPresentationState
RecoveredSoftwareGraph_WindowsPresentationState();
void RecoveredSoftwareGraph_ConfigureWindowMessageHook(
    TRecoveredSoftwareWindowMessageHook hook);
