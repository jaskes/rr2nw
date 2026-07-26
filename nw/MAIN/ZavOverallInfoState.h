#ifndef RR2NW_ZAV_OVERALL_INFO_STATE_H
#define RR2NW_ZAV_OVERALL_INFO_STATE_H

#include <cstddef>

#include "stdtypes.h"

extern dword m_dwPrevTime;
extern dword dwTime0;
extern dword dwFrames;
extern dword nWhiteColor;
extern dword dwMem0;
extern dword dwMem1;

int ZAV_FormatOverallInfo(char* output, std::size_t outputSize);
void ZAV_PrintOverallInfo();

#endif  // RR2NW_ZAV_OVERALL_INFO_STATE_H
