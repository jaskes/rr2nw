#pragma once

#include <string>

namespace rr2nw {

enum class WindowsRetailDataSelectionReadResult {
  kReady,
  kMissing,
  kCorrupt,
  kIoFailure,
};

// RR2DATA1 is deliberately separate from gameplay settings. It stores one
// bounded, hex-encoded UTF-8 directory and never carries developer capability,
// retail payload, or live-world state.
WindowsRetailDataSelectionReadResult WindowsRetailDataSelection_Read(
    const std::wstring& filePath, std::wstring* directory,
    std::string* detail);

// Replaces the selection atomically. A failed write leaves an existing file
// byte-for-byte unchanged.
bool WindowsRetailDataSelection_WriteAtomic(const std::wstring& filePath,
                                            const std::wstring& directory,
                                            std::string* detail);

}  // namespace rr2nw
