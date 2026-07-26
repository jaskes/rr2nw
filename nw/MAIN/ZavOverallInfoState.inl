#include "ZavOverallInfoState.h"

#include <cstdio>
#include <ctime>

dword m_dwPrevTime = 0;
dword dwTime0 = 0;
dword dwFrames = 0;
dword nWhiteColor = 0;
dword dwMem0 = 0;
dword dwMem1 = 0;

int ZAV_FormatOverallInfo(char* output, std::size_t outputSize) {
  if (output == 0 || outputSize == 0) return 0;

#if defined(MSDOS) || defined(__NT__)
  const double ticks = 1000.0;
#else
  const double ticks = CLOCKS_PER_SEC;
#endif
  const double seconds =
      ticks > 0.0 ? static_cast<double>(m_dwPrevTime - dwTime0) / ticks : 0.0;
  const double framesPerSecond =
      seconds > 0.0 ? static_cast<double>(dwFrames) / seconds : 0.0;
  const long memoryDelta =
      static_cast<long>(dwMem0) - static_cast<long>(dwMem1);
  const int written = std::snprintf(output, outputSize, "%lg Mem=%ld",
                                    framesPerSecond, memoryDelta);
  if (written < 0 || static_cast<std::size_t>(written) >= outputSize) {
    output[outputSize - 1] = '\0';
    return 0;
  }
  return 1;
}

void ZAV_PrintOverallInfo() {
  char output[40];
  (void)ZAV_FormatOverallInfo(output, sizeof(output));
}
