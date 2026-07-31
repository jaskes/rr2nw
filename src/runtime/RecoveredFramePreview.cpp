#include "RecoveredFramePreview.h"

#include "IndexedPng.h"
#include "RecoveredSoftwareGraph.h"
#include "graph.h"

extern unsigned char _currPalette[256u * 3u];

namespace {

constexpr std::size_t kPaletteBytes = 256u * 3u;
constexpr std::uint64_t kHashOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kHashPrime = UINT64_C(1099511628211);

std::uint64_t FramebufferFingerprint(const std::uint8_t* pixels,
                                     std::size_t count) {
  std::uint64_t hash = kHashOffset;
  for (std::size_t index = 0; index < count; ++index) {
    hash ^= pixels[index];
    hash *= kHashPrime;
  }
  return hash;
}

bool Fail(std::string* failure, const char* detail) {
  if (failure != nullptr) *failure = detail;
  return false;
}

}  // namespace

bool RecoveredFramePreview_CapturePng(
    std::vector<std::uint8_t>* png,
    SRecoveredFramePreviewSummary* summary,
    std::string* failure) {
  if (failure != nullptr) failure->clear();
  if (png == nullptr || summary == nullptr) {
    return Fail(failure, "frame preview output is null");
  }
  *summary = {};
  if (!RecoveredSoftwareGraph_IsReady() || _gr_pScreen == nullptr ||
      _gr_nScreenWidth <= 0 || _gr_nScreenHeight <= 0) {
    return Fail(failure, "software framebuffer is unavailable");
  }
  const std::uint32_t width =
      static_cast<std::uint32_t>(_gr_nScreenWidth);
  const std::uint32_t height =
      static_cast<std::uint32_t>(_gr_nScreenHeight);
  const std::size_t pixelBytes =
      static_cast<std::size_t>(width) * height;
  const std::uint64_t framebufferFingerprint =
      FramebufferFingerprint(_gr_pScreen, pixelBytes);

  SIndexedPngSummary pngSummary;
  std::vector<std::uint8_t> encoded;
  if (!IndexedPng_Encode(width, height, _gr_pScreen, width, _currPalette,
                        kPaletteBytes, &encoded, &pngSummary, failure)) {
    return false;
  }

  SRecoveredFramePreviewSummary candidate;
  candidate.ready = true;
  candidate.width = width;
  candidate.height = height;
  candidate.paletteBytes = kPaletteBytes;
  candidate.pngBytes = pngSummary.encodedBytes;
  candidate.framebufferFingerprint = framebufferFingerprint;
  candidate.pngFingerprint = pngSummary.fingerprint;
  *png = std::move(encoded);
  *summary = candidate;
  return true;
}
