#include "RecoveredBriefingFileOpen.h"

#undef fopen

#include "RecoveredModRuntime.h"

#include <cstring>

std::FILE* RecoveredBriefingFileOpen(const char* path, const char* mode) {
  if (path != nullptr && mode != nullptr && std::strcmp(mode, "rb") == 0) {
    if (std::FILE* file = RecoveredModRuntime_OpenBaseRead(path, nullptr))
      return file;
  }
  return std::fopen(path, mode);
}
