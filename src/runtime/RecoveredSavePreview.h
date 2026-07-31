#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SRecoveredSavePreviewImage {
  bool ready = false;
  std::uint32_t sourceWidth = 0;
  std::uint32_t sourceHeight = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::size_t stride = 0;
  std::uint64_t sourceFingerprint = 0;
  std::vector<std::uint8_t> bgra;
};

// Decodes the first frame of a bounded PNG through the Windows Imaging
// Component and scales it to fit inside maxWidth/maxHeight while preserving
// aspect ratio. The output is top-down 32-bit BGRA for direct GDI use.
bool RecoveredSavePreview_DecodePng(
    const std::vector<std::uint8_t>& png, std::uint32_t maxWidth,
    std::uint32_t maxHeight, SRecoveredSavePreviewImage* image,
    std::string* failure);
