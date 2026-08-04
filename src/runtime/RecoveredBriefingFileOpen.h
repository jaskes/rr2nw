#pragma once

#include <cstdio>

// BRIEFING.CPP is kept byte-for-byte in its original code page.  This narrow
// bridge is force-included for that translation unit so its binary FLC reads
// participate in the recovered, case-insensitive retail/mod resource catalog.
std::FILE* RecoveredBriefingFileOpen(const char* path, const char* mode);

#define fopen(path, mode) RecoveredBriefingFileOpen(path, mode)
