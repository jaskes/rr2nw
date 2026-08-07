#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

using TRecoveredSoftwareWindowMessageHook =
    LRESULT (*)(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

enum ERecoveredWindowMode {
  RECOVERED_WINDOW_MODE_WINDOWED = 0,
  RECOVERED_WINDOW_MODE_BORDERLESS = 1
};

struct SRecoveredWindowPresentation {
  ERecoveredWindowMode mode = RECOVERED_WINDOW_MODE_WINDOWED;
  int clientWidth = 640;
  int clientHeight = 480;
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
void RecoveredSoftwareGraph_ConfigureWindowMessageHook(
    TRecoveredSoftwareWindowMessageHook hook);
