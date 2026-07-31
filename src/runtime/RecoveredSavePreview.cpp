#include "RecoveredSavePreview.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

#include <windows.h>
#include <wincodec.h>

namespace {

constexpr std::size_t kMaximumPngBytes = 8u * 1024u * 1024u;
constexpr std::uint32_t kMaximumDimension = 16384u;
constexpr std::uint64_t kHashOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kHashPrime = UINT64_C(1099511628211);

template <typename T>
void ReleaseCom(T** value) {
  if (value != nullptr && *value != nullptr) {
    (*value)->Release();
    *value = nullptr;
  }
}

bool Fail(SRecoveredSavePreviewImage* image, std::string* failure,
          const char* detail) {
  if (image != nullptr) *image = {};
  if (failure != nullptr) *failure = detail;
  return false;
}

std::uint64_t Fingerprint(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t hash = kHashOffset;
  for (std::uint8_t value : bytes) {
    hash ^= value;
    hash *= kHashPrime;
  }
  return hash;
}

}  // namespace

bool RecoveredSavePreview_DecodePng(
    const std::vector<std::uint8_t>& png, std::uint32_t maxWidth,
    std::uint32_t maxHeight, SRecoveredSavePreviewImage* image,
    std::string* failure) {
  if (failure != nullptr) failure->clear();
  if (image == nullptr || png.empty() || png.size() > kMaximumPngBytes ||
      png.size() > static_cast<std::size_t>(MAXDWORD) || maxWidth == 0u ||
      maxHeight == 0u || maxWidth > kMaximumDimension ||
      maxHeight > kMaximumDimension) {
    return Fail(image, failure, "save preview decode arguments are invalid");
  }
  *image = {};

  IWICImagingFactory* factory = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICBitmapScaler* scaler = nullptr;
  IWICFormatConverter* converter = nullptr;
  HRESULT result = CoCreateInstance(
      CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS(&factory));
  if (SUCCEEDED(result)) result = factory->CreateStream(&stream);
  if (SUCCEEDED(result)) {
    result = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(png.data())),
        static_cast<DWORD>(png.size()));
  }
  if (SUCCEEDED(result)) {
    result = factory->CreateDecoderFromStream(
        stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
  }
  if (SUCCEEDED(result)) result = decoder->GetFrame(0u, &frame);

  UINT sourceWidth = 0u;
  UINT sourceHeight = 0u;
  if (SUCCEEDED(result)) result = frame->GetSize(&sourceWidth, &sourceHeight);
  if (SUCCEEDED(result) &&
      (sourceWidth == 0u || sourceHeight == 0u ||
       sourceWidth > kMaximumDimension || sourceHeight > kMaximumDimension)) {
    result = E_INVALIDARG;
  }

  UINT targetWidth = sourceWidth;
  UINT targetHeight = sourceHeight;
  if (SUCCEEDED(result) &&
      (targetWidth > maxWidth || targetHeight > maxHeight)) {
    const double scale = (std::min)(
        static_cast<double>(maxWidth) / targetWidth,
        static_cast<double>(maxHeight) / targetHeight);
    targetWidth = (std::max)(
        1u, static_cast<UINT>(static_cast<double>(targetWidth) * scale));
    targetHeight = (std::max)(
        1u, static_cast<UINT>(static_cast<double>(targetHeight) * scale));
  }

  IWICBitmapSource* source = frame;
  if (SUCCEEDED(result) &&
      (targetWidth != sourceWidth || targetHeight != sourceHeight)) {
    result = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(result)) {
      result = scaler->Initialize(frame, targetWidth, targetHeight,
                                  WICBitmapInterpolationModeFant);
    }
    source = scaler;
  }
  if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
  if (SUCCEEDED(result)) {
    result = converter->Initialize(
        source, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
        nullptr, 0.0, WICBitmapPaletteTypeCustom);
  }

  const std::size_t stride = static_cast<std::size_t>(targetWidth) * 4u;
  if (SUCCEEDED(result) &&
      (stride > static_cast<std::size_t>(MAXDWORD) ||
       targetHeight >
           (std::numeric_limits<std::size_t>::max)() / stride ||
       stride * targetHeight > static_cast<std::size_t>(MAXDWORD))) {
    result = E_OUTOFMEMORY;
  }
  std::vector<std::uint8_t> pixels;
  if (SUCCEEDED(result)) {
    pixels.resize(stride * targetHeight);
    result = converter->CopyPixels(
        nullptr, static_cast<UINT>(stride),
        static_cast<UINT>(pixels.size()), pixels.data());
  }

  ReleaseCom(&converter);
  ReleaseCom(&scaler);
  ReleaseCom(&frame);
  ReleaseCom(&decoder);
  ReleaseCom(&stream);
  ReleaseCom(&factory);
  if (FAILED(result)) {
    return Fail(image, failure,
                "Windows could not decode the embedded save preview");
  }

  SRecoveredSavePreviewImage decoded;
  decoded.ready = true;
  decoded.sourceWidth = sourceWidth;
  decoded.sourceHeight = sourceHeight;
  decoded.width = targetWidth;
  decoded.height = targetHeight;
  decoded.stride = stride;
  decoded.sourceFingerprint = Fingerprint(png);
  decoded.bgra = std::move(pixels);
  if (decoded.sourceFingerprint == 0u || decoded.bgra.empty()) {
    return Fail(image, failure, "decoded save preview is empty");
  }
  *image = std::move(decoded);
  return true;
}
