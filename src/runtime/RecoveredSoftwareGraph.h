#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

using TRecoveredSoftwareWindowMessageHook =
    LRESULT (*)(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

int RecoveredSoftwareGraph_Initialize(HINSTANCE instance);
bool RecoveredSoftwareGraph_IsReady();
int RecoveredSoftwareGraph_Width();
int RecoveredSoftwareGraph_Height();
void RecoveredSoftwareGraph_ConfigureWindowMessageHook(
    TRecoveredSoftwareWindowMessageHook hook);
