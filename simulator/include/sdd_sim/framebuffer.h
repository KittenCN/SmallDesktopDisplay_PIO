#ifndef SDD_SIM_FRAMEBUFFER_H
#define SDD_SIM_FRAMEBUFFER_H

#include <cstdint>
#include <vector>

namespace sdd_sim {

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  constexpr bool contains(int px, int py) const noexcept {
    return px >= x && py >= y && px < x + width && py < y + height;
  }
};

namespace color {
constexpr std::uint16_t Black = 0x0000;
constexpr std::uint16_t White = 0xFFFF;
constexpr std::uint16_t Green = 0x07E0;
constexpr std::uint16_t ClockYellow = 0xD404;
}  // namespace color

class Framebuffer {
 public:
  static constexpr int kWidth = 240;
  static constexpr int kHeight = 240;

  Framebuffer();

  int width() const noexcept { return kWidth; }
  int height() const noexcept { return kHeight; }
  const std::vector<std::uint16_t>& pixels() const noexcept { return pixels_; }
  std::vector<std::uint16_t>& pixels() noexcept { return pixels_; }

  void clear(std::uint16_t value = color::Black) noexcept;
  void setPixel(int x, int y, std::uint16_t value) noexcept;
  std::uint16_t pixel(int x, int y) const noexcept;
  void fillRect(int x, int y, int width, int height, std::uint16_t value) noexcept;
  void drawFastHLine(int x, int y, int width, std::uint16_t value) noexcept;
  void drawFastVLine(int x, int y, int height, std::uint16_t value) noexcept;
  void drawRect(int x, int y, int width, int height, std::uint16_t value) noexcept;
  void fillRoundRect(int x, int y, int width, int height, int radius,
                     std::uint16_t value) noexcept;
  void drawRoundRect(int x, int y, int width, int height, int radius,
                     std::uint16_t value) noexcept;
  void blit(int x, int y, int width, int height, const std::uint16_t* source,
            int sourceStride = 0, const Rect* clip = nullptr) noexcept;
  void blendPixel(int x, int y, std::uint16_t foreground, std::uint8_t alpha,
                  const Rect* clip = nullptr) noexcept;

  // Converts the logical ST7789 framebuffer into host ARGB pixels. Rotation is
  // applied only at presentation time so all shared renderer coordinates remain
  // canonical. Brightness is a deterministic 0..100 linear scale.
  std::vector<std::uint32_t> toArgb8888(int rotation = 0, int brightness = 100) const;

  static std::uint16_t rgb565(std::uint8_t red, std::uint8_t green,
                              std::uint8_t blue) noexcept;

 private:
  std::vector<std::uint16_t> pixels_;
};

}  // namespace sdd_sim

#endif
