#ifndef SDD_SIM_DISPLAY_RENDERER_H
#define SDD_SIM_DISPLAY_RENDERER_H

#include <cstdint>
#include <unordered_map>

#include "sdd_sim/framebuffer.h"
#include "sdd_sim/jpeg_decoder.h"
#include "sdd_sim/simulator_state.h"

namespace sdd_sim {

class DisplayRenderer {
 public:
  explicit DisplayRenderer(JpegDecoder* jpegDecoder = nullptr) noexcept
      : jpegDecoder_(jpegDecoder) {}

  void setJpegDecoder(JpegDecoder* decoder) noexcept;
  JpegDecoder* jpegDecoder() const noexcept { return jpegDecoder_; }

  // Always renders the canonical 240x240 screen. Brightness and rotation are
  // presentation properties available through Framebuffer::toArgb8888().
  void render(const SimulatorState& state, Framebuffer& framebuffer) const;

 private:
  void drawJpeg(Framebuffer& framebuffer, int x, int y, ByteSpan asset) const;
  JpegDecoder* jpegDecoder_ = nullptr;
  mutable std::unordered_map<const std::uint8_t*, JpegImage> jpegCache_;
};

}  // namespace sdd_sim

#endif
