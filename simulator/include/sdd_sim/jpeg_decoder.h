#ifndef SDD_SIM_JPEG_DECODER_H
#define SDD_SIM_JPEG_DECODER_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sdd_sim {

struct JpegImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint16_t> rgb565;

  bool valid() const noexcept {
    return width > 0 && height > 0 &&
           rgb565.size() == static_cast<std::size_t>(width * height);
  }
};

// Platform boundary for JPEG decoding. The SDL/Win32 application can implement
// this with GDI+, WIC, stb_image, or the bundled tjpgd core. The renderer owns no
// OS or windowing dependency and consumes deterministic RGB565 output only.
class JpegDecoder {
 public:
  virtual ~JpegDecoder() = default;
  virtual bool decode(const std::uint8_t* bytes, std::size_t byteCount,
                      JpegImage& output) = 0;
};

}  // namespace sdd_sim

#endif
