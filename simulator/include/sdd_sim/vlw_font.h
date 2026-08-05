#ifndef SDD_SIM_VLW_FONT_H
#define SDD_SIM_VLW_FONT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "sdd_sim/framebuffer.h"

namespace sdd_sim {

enum class TextAlign { TopLeft, MiddleLeft, Center, MiddleRight };

class VlwFont {
 public:
  VlwFont() = default;
  VlwFont(const std::uint8_t* bytes, std::size_t byteCount);

  bool valid() const noexcept { return valid_; }
  int lineHeight() const noexcept { return lineHeight_; }
  int textWidth(const std::string& utf8) const noexcept;
  bool hasGlyph(std::uint32_t codepoint) const noexcept;

  // Draws using the same VLW metrics/alpha bitmaps consumed by TFT_eSPI.
  // Unknown glyphs use TFT_eSPI's outlined-box advance rather than a host font.
  int drawText(Framebuffer& framebuffer, const std::string& utf8, int x, int y,
               std::uint16_t foreground, std::uint16_t background,
               TextAlign align = TextAlign::TopLeft,
               const Rect* clip = nullptr) const noexcept;

 private:
  struct Glyph {
    std::uint32_t unicode = 0;
    std::uint8_t height = 0;
    std::uint8_t width = 0;
    std::uint8_t advance = 0;
    std::int16_t deltaY = 0;
    std::int8_t deltaX = 0;
    std::size_t bitmapOffset = 0;
  };

  const Glyph* glyph(std::uint32_t codepoint) const noexcept;
  static std::vector<std::uint32_t> decodeUtf8(const std::string& text) noexcept;

  const std::uint8_t* bytes_ = nullptr;
  std::size_t byteCount_ = 0;
  bool valid_ = false;
  int ascent_ = 0;
  int descent_ = 0;
  int maxAscent_ = 0;
  int maxDescent_ = 0;
  int lineHeight_ = 0;
  int spaceWidth_ = 0;
  std::vector<Glyph> glyphs_;
  std::unordered_map<std::uint32_t, std::size_t> glyphIndex_;
};

}  // namespace sdd_sim

#endif
