#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SRecoveredFramePreviewSummary {
  bool ready = false;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::size_t paletteBytes = 0;
  std::size_t pngBytes = 0;
  std::uint64_t framebufferFingerprint = 0;
  std::uint64_t pngFingerprint = 0;
};

bool RecoveredFramePreview_CapturePng(
    std::vector<std::uint8_t>* png,
    SRecoveredFramePreviewSummary* summary,
    std::string* failure);
