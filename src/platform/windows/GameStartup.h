#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace rr2nw {

int RunGameStartup(HINSTANCE instance, int argc, wchar_t** argv);

}  // namespace rr2nw
