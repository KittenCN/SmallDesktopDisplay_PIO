#include "sdd_sim/gdiplus_jpeg_decoder.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstring>
#include <limits>

#include "sdd_sim/framebuffer.h"

namespace sdd_sim {

GdiPlusSession::GdiPlusSession() noexcept {
  Gdiplus::GdiplusStartupInput input;
  ULONG_PTR token = 0;
  if (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok) {
    token_ = static_cast<std::uintptr_t>(token);
  }
}

GdiPlusSession::~GdiPlusSession() {
  if (token_ != 0) {
    Gdiplus::GdiplusShutdown(static_cast<ULONG_PTR>(token_));
  }
}

bool GdiPlusJpegDecoder::decode(const std::uint8_t* bytes,
                                std::size_t byteCount,
                                JpegImage& output) {
  output = {};
  if (bytes == nullptr || byteCount < 4 ||
      byteCount > static_cast<std::size_t>(std::numeric_limits<SIZE_T>::max())) {
    return false;
  }

  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byteCount);
  if (memory == nullptr) {
    return false;
  }
  void* destination = GlobalLock(memory);
  if (destination == nullptr) {
    GlobalFree(memory);
    return false;
  }
  std::memcpy(destination, bytes, byteCount);
  GlobalUnlock(memory);

  IStream* stream = nullptr;
  if (CreateStreamOnHGlobal(memory, TRUE, &stream) != S_OK) {
    GlobalFree(memory);
    return false;
  }

  bool success = false;
  {
    Gdiplus::Bitmap bitmap(stream, FALSE);
    const UINT width = bitmap.GetWidth();
    const UINT height = bitmap.GetHeight();
    if (bitmap.GetLastStatus() == Gdiplus::Ok && width > 0 && height > 0 &&
        width <= 4096 && height <= 4096) {
      Gdiplus::Rect rect(0, 0, static_cast<INT>(width), static_cast<INT>(height));
      Gdiplus::BitmapData bitmapData{};
      if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead,
                          PixelFormat32bppARGB, &bitmapData) == Gdiplus::Ok) {
        output.width = static_cast<int>(width);
        output.height = static_cast<int>(height);
        output.rgb565.resize(static_cast<std::size_t>(width) * height);

        const auto* firstRow = static_cast<const std::uint8_t*>(bitmapData.Scan0);
        for (UINT y = 0; y < height; ++y) {
          const std::uint8_t* row =
              bitmapData.Stride >= 0
                  ? firstRow + static_cast<std::ptrdiff_t>(y) * bitmapData.Stride
                  : firstRow + static_cast<std::ptrdiff_t>(height - 1U - y) *
                                   -bitmapData.Stride;
          for (UINT x = 0; x < width; ++x) {
            const std::uint8_t blue = row[x * 4U];
            const std::uint8_t green = row[x * 4U + 1U];
            const std::uint8_t red = row[x * 4U + 2U];
            output.rgb565[static_cast<std::size_t>(y) * width + x] =
                Framebuffer::rgb565(red, green, blue);
          }
        }
        bitmap.UnlockBits(&bitmapData);
        success = output.valid();
      }
    }
  }

  stream->Release();  // Also frees the HGLOBAL because fDeleteOnRelease is TRUE.
  if (!success) {
    output = {};
  }
  return success;
}

}  // namespace sdd_sim
