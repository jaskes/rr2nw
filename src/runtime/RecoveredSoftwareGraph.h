#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

int RecoveredSoftwareGraph_Initialize(HINSTANCE instance);
bool RecoveredSoftwareGraph_IsReady();
int RecoveredSoftwareGraph_Width();
int RecoveredSoftwareGraph_Height();
