#include "GameStartup.h"

#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return 5;
  }

  const int result = rr2nw::RunGameStartup(instance, argc, argv);
  LocalFree(argv);
  return result;
}
