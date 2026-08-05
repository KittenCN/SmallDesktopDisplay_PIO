#ifndef SDD_SIM_GDIPLUS_JPEG_DECODER_H
#define SDD_SIM_GDIPLUS_JPEG_DECODER_H

#include <cstddef>
#include <cstdint>

#include "sdd_sim/jpeg_decoder.h"

namespace sdd_sim {

class GdiPlusSession {
 public:
  GdiPlusSession() noexcept;
  ~GdiPlusSession();

  GdiPlusSession(const GdiPlusSession&) = delete;
  GdiPlusSession& operator=(const GdiPlusSession&) = delete;

  bool ready() const noexcept { return token_ != 0; }

 private:
  std::uintptr_t token_ = 0;
};

class GdiPlusJpegDecoder final : public JpegDecoder {
 public:
  bool decode(const std::uint8_t* bytes, std::size_t byteCount,
              JpegImage& output) override;
};

}  // namespace sdd_sim

#endif
