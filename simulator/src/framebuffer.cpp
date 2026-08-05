#include "sdd_sim/framebuffer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "sdd_sim/vlw_font.h"

namespace sdd_sim {

namespace {

std::uint32_t readBigEndian32(const std::uint8_t* bytes) noexcept {
  return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
         (static_cast<std::uint32_t>(bytes[1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[2]) << 8U) |
         static_cast<std::uint32_t>(bytes[3]);
}

std::uint16_t blend565(std::uint16_t foreground, std::uint16_t background,
                       std::uint8_t alpha) noexcept {
  if (alpha == 0) return background;
  if (alpha == 255) return foreground;
  const unsigned inverse = 255U - alpha;
  const unsigned fr = (foreground >> 11U) & 0x1FU;
  const unsigned fg = (foreground >> 5U) & 0x3FU;
  const unsigned fb = foreground & 0x1FU;
  const unsigned br = (background >> 11U) & 0x1FU;
  const unsigned bg = (background >> 5U) & 0x3FU;
  const unsigned bb = background & 0x1FU;
  const unsigned red = (fr * alpha + br * inverse + 127U) / 255U;
  const unsigned green = (fg * alpha + bg * inverse + 127U) / 255U;
  const unsigned blue = (fb * alpha + bb * inverse + 127U) / 255U;
  return static_cast<std::uint16_t>((red << 11U) | (green << 5U) | blue);
}

}  // namespace

Framebuffer::Framebuffer()
    : pixels_(static_cast<std::size_t>(kWidth * kHeight), color::Black) {}

void Framebuffer::clear(std::uint16_t value) noexcept {
  std::fill(pixels_.begin(), pixels_.end(), value);
}

void Framebuffer::setPixel(int x, int y, std::uint16_t value) noexcept {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
  pixels_[static_cast<std::size_t>(y * kWidth + x)] = value;
}

std::uint16_t Framebuffer::pixel(int x, int y) const noexcept {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return color::Black;
  return pixels_[static_cast<std::size_t>(y * kWidth + x)];
}

void Framebuffer::fillRect(int x, int y, int width, int height,
                           std::uint16_t value) noexcept {
  if (width <= 0 || height <= 0) return;
  const auto rightEdge = static_cast<std::int64_t>(x) + width;
  const auto bottomEdge = static_cast<std::int64_t>(y) + height;
  const int left = std::max(0, x);
  const int top = std::max(0, y);
  const int right = static_cast<int>(std::min<std::int64_t>(kWidth, rightEdge));
  const int bottom = static_cast<int>(std::min<std::int64_t>(kHeight, bottomEdge));
  if (left >= right || top >= bottom) return;
  for (int row = top; row < bottom; ++row) {
    auto begin = pixels_.begin() + static_cast<std::ptrdiff_t>(row * kWidth + left);
    std::fill(begin, begin + (right - left), value);
  }
}

void Framebuffer::drawFastHLine(int x, int y, int width,
                                std::uint16_t value) noexcept {
  fillRect(x, y, width, 1, value);
}

void Framebuffer::drawFastVLine(int x, int y, int height,
                                std::uint16_t value) noexcept {
  fillRect(x, y, 1, height, value);
}

void Framebuffer::drawRect(int x, int y, int width, int height,
                           std::uint16_t value) noexcept {
  if (width <= 0 || height <= 0) return;
  drawFastHLine(x, y, width, value);
  drawFastHLine(x, y + height - 1, width, value);
  drawFastVLine(x, y, height, value);
  drawFastVLine(x + width - 1, y, height, value);
}

void Framebuffer::fillRoundRect(int x, int y, int width, int height, int radius,
                                std::uint16_t value) noexcept {
  if (width <= 0 || height <= 0) return;
  radius = std::max(0, std::min(radius, std::min(width, height) / 2));
  if (radius == 0) {
    fillRect(x, y, width, height, value);
    return;
  }
  fillRect(x + radius, y, width - 2 * radius, height, value);
  fillRect(x, y + radius, radius, height - 2 * radius, value);
  fillRect(x + width - radius, y + radius, radius, height - 2 * radius, value);
  const int r2 = radius * radius;
  for (int dy = 0; dy < radius; ++dy) {
    for (int dx = 0; dx < radius; ++dx) {
      const int cx = radius - 1 - dx;
      const int cy = radius - 1 - dy;
      if (cx * cx + cy * cy <= r2) {
        setPixel(x + dx, y + dy, value);
        setPixel(x + width - 1 - dx, y + dy, value);
        setPixel(x + dx, y + height - 1 - dy, value);
        setPixel(x + width - 1 - dx, y + height - 1 - dy, value);
      }
    }
  }
}

void Framebuffer::drawRoundRect(int x, int y, int width, int height, int radius,
                                std::uint16_t value) noexcept {
  if (width <= 0 || height <= 0) return;
  radius = std::max(0, std::min(radius, std::min(width, height) / 2));
  if (radius == 0) {
    drawRect(x, y, width, height, value);
    return;
  }
  drawFastHLine(x + radius, y, width - 2 * radius, value);
  drawFastHLine(x + radius, y + height - 1, width - 2 * radius, value);
  drawFastVLine(x, y + radius, height - 2 * radius, value);
  drawFastVLine(x + width - 1, y + radius, height - 2 * radius, value);
  const int outer = radius * radius;
  const int inner = (radius - 1) * (radius - 1);
  for (int dy = 0; dy < radius; ++dy) {
    for (int dx = 0; dx < radius; ++dx) {
      const int cx = radius - 1 - dx;
      const int cy = radius - 1 - dy;
      const int distance = cx * cx + cy * cy;
      if (distance <= outer && distance >= inner) {
        setPixel(x + dx, y + dy, value);
        setPixel(x + width - 1 - dx, y + dy, value);
        setPixel(x + dx, y + height - 1 - dy, value);
        setPixel(x + width - 1 - dx, y + height - 1 - dy, value);
      }
    }
  }
}

void Framebuffer::blit(int x, int y, int width, int height,
                       const std::uint16_t* source, int sourceStride,
                       const Rect* clip) noexcept {
  if (source == nullptr || width <= 0 || height <= 0) return;
  if (sourceStride <= 0) sourceStride = width;
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const int targetX = x + column;
      const int targetY = y + row;
      if (clip != nullptr && !clip->contains(targetX, targetY)) continue;
      setPixel(targetX, targetY, source[row * sourceStride + column]);
    }
  }
}

void Framebuffer::blendPixel(int x, int y, std::uint16_t foreground,
                             std::uint8_t alpha, const Rect* clip) noexcept {
  if (clip != nullptr && !clip->contains(x, y)) return;
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
  setPixel(x, y, blend565(foreground, pixel(x, y), alpha));
}

std::vector<std::uint32_t> Framebuffer::toArgb8888(int rotation,
                                                   int brightness) const {
  rotation = ((rotation % 4) + 4) % 4;
  brightness = std::max(0, std::min(100, brightness));
  std::vector<std::uint32_t> output(static_cast<std::size_t>(kWidth * kHeight));
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      int sourceX = x;
      int sourceY = y;
      if (rotation == 1) {
        sourceX = y;
        sourceY = kHeight - 1 - x;
      } else if (rotation == 2) {
        sourceX = kWidth - 1 - x;
        sourceY = kHeight - 1 - y;
      } else if (rotation == 3) {
        sourceX = kWidth - 1 - y;
        sourceY = x;
      }
      const std::uint16_t value = pixel(sourceX, sourceY);
      unsigned red = ((value >> 11U) & 0x1FU) * 255U / 31U;
      unsigned green = ((value >> 5U) & 0x3FU) * 255U / 63U;
      unsigned blue = (value & 0x1FU) * 255U / 31U;
      red = red * static_cast<unsigned>(brightness) / 100U;
      green = green * static_cast<unsigned>(brightness) / 100U;
      blue = blue * static_cast<unsigned>(brightness) / 100U;
      output[static_cast<std::size_t>(y * kWidth + x)] =
          0xFF000000U | (red << 16U) | (green << 8U) | blue;
    }
  }
  return output;
}

std::uint16_t Framebuffer::rgb565(std::uint8_t red, std::uint8_t green,
                                  std::uint8_t blue) noexcept {
  return static_cast<std::uint16_t>(((red & 0xF8U) << 8U) |
                                    ((green & 0xFCU) << 3U) | (blue >> 3U));
}

VlwFont::VlwFont(const std::uint8_t* bytes, std::size_t byteCount)
    : bytes_(bytes), byteCount_(byteCount) {
  if (bytes == nullptr || byteCount < 24) return;
  const std::uint32_t count = readBigEndian32(bytes);
  if (count == 0 || count > 65535U || count > (byteCount - 24U) / 28U) return;
  ascent_ = static_cast<int>(readBigEndian32(bytes + 16));
  descent_ = static_cast<int>(readBigEndian32(bytes + 20));
  maxAscent_ = ascent_;
  maxDescent_ = descent_;
  std::size_t bitmapOffset = 24U + static_cast<std::size_t>(count) * 28U;
  glyphs_.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    const std::size_t offset = 24U + static_cast<std::size_t>(index) * 28U;
    Glyph item;
    item.unicode = readBigEndian32(bytes + offset);
    const std::uint32_t rawHeight = readBigEndian32(bytes + offset + 4U);
    const std::uint32_t rawWidth = readBigEndian32(bytes + offset + 8U);
    const std::uint32_t rawAdvance = readBigEndian32(bytes + offset + 12U);
    if (rawHeight > 255U || rawWidth > 255U || rawAdvance > 255U) return;
    item.height = static_cast<std::uint8_t>(rawHeight);
    item.width = static_cast<std::uint8_t>(rawWidth);
    item.advance = static_cast<std::uint8_t>(rawAdvance);
    item.deltaY = static_cast<std::int16_t>(
        static_cast<std::int32_t>(readBigEndian32(bytes + offset + 16U)));
    item.deltaX = static_cast<std::int8_t>(
        static_cast<std::int32_t>(readBigEndian32(bytes + offset + 20U)));
    item.bitmapOffset = bitmapOffset;
    const std::size_t bitmapSize =
        static_cast<std::size_t>(item.width) * static_cast<std::size_t>(item.height);
    if (bitmapOffset > byteCount || bitmapSize > byteCount - bitmapOffset) return;
    bitmapOffset += bitmapSize;
    if (static_cast<int>(item.height) - item.deltaY > maxDescent_ &&
        ((item.unicode > 0x20U && item.unicode < 0xA0U && item.unicode != 0x7FU) ||
         item.unicode > 0xFFU)) {
      maxDescent_ = static_cast<int>(item.height) - item.deltaY;
    }
    glyphIndex_.emplace(item.unicode, glyphs_.size());
    glyphs_.push_back(item);
  }
  lineHeight_ = maxAscent_ + maxDescent_;
  spaceWidth_ = (ascent_ + descent_) * 2 / 7;
  if (lineHeight_ <= 0 || spaceWidth_ <= 0) return;
  valid_ = true;
}

const VlwFont::Glyph* VlwFont::glyph(std::uint32_t codepoint) const noexcept {
  const auto iterator = glyphIndex_.find(codepoint);
  return iterator == glyphIndex_.end() ? nullptr : &glyphs_[iterator->second];
}

bool VlwFont::hasGlyph(std::uint32_t codepoint) const noexcept {
  return codepoint == 0x20U || glyph(codepoint) != nullptr;
}

std::vector<std::uint32_t> VlwFont::decodeUtf8(const std::string& text) noexcept {
  std::vector<std::uint32_t> result;
  result.reserve(text.size());
  std::size_t index = 0;
  while (index < text.size()) {
    const auto first = static_cast<std::uint8_t>(text[index++]);
    if (first < 0x80U) {
      result.push_back(first);
      continue;
    }
    unsigned continuationCount = 0;
    std::uint32_t codepoint = 0;
    if ((first & 0xE0U) == 0xC0U) {
      continuationCount = 1;
      codepoint = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
      continuationCount = 2;
      codepoint = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
      continuationCount = 3;
      codepoint = first & 0x07U;
    } else {
      result.push_back(0xFFFDU);
      continue;
    }
    if (index + continuationCount > text.size()) {
      result.push_back(0xFFFDU);
      break;
    }
    bool valid = true;
    for (unsigned part = 0; part < continuationCount; ++part) {
      const auto byte = static_cast<std::uint8_t>(text[index]);
      if ((byte & 0xC0U) != 0x80U) {
        valid = false;
        break;
      }
      ++index;
      codepoint = (codepoint << 6U) | (byte & 0x3FU);
    }
    if (!valid || codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
      result.push_back(0xFFFDU);
    } else {
      result.push_back(codepoint);
    }
  }
  return result;
}

int VlwFont::textWidth(const std::string& utf8) const noexcept {
  if (!valid_) return 0;
  const auto codepoints = decodeUtf8(utf8);
  int width = 0;
  for (std::size_t index = 0; index < codepoints.size(); ++index) {
    const std::uint32_t codepoint = codepoints[index];
    if (codepoint == 0x20U) {
      width += spaceWidth_;
      continue;
    }
    const Glyph* item = glyph(codepoint);
    if (item == nullptr) {
      width += spaceWidth_ + 1;
      continue;
    }
    if (width == 0 && item->deltaX < 0) width -= item->deltaX;
    if (index + 1U < codepoints.size())
      width += item->advance;
    else
      width += item->deltaX + item->width;
  }
  return width;
}

int VlwFont::drawText(Framebuffer& framebuffer, const std::string& utf8, int x,
                      int y, std::uint16_t foreground, std::uint16_t background,
                      TextAlign align, const Rect* clip) const noexcept {
  if (!valid_) return 0;
  const int width = textWidth(utf8);
  int cursorX = x;
  int cursorY = y;
  if (align == TextAlign::Center) {
    cursorX -= width / 2;
    cursorY -= lineHeight_ / 2;
  } else if (align == TextAlign::MiddleLeft) {
    cursorY -= lineHeight_ / 2;
  } else if (align == TextAlign::MiddleRight) {
    cursorX -= width;
    cursorY -= lineHeight_ / 2;
  }
  const auto codepoints = decodeUtf8(utf8);
  bool firstGlyph = true;
  for (std::uint32_t codepoint : codepoints) {
    if (codepoint == 0x20U) {
      cursorX += spaceWidth_;
      firstGlyph = false;
      continue;
    }
    const Glyph* item = glyph(codepoint);
    if (item == nullptr) {
      framebuffer.drawRect(cursorX, cursorY + maxAscent_ - ascent_, spaceWidth_,
                           ascent_, foreground);
      cursorX += spaceWidth_ + 1;
      firstGlyph = false;
      continue;
    }
    if (firstGlyph && item->deltaX < 0) cursorX -= item->deltaX;
    const int glyphX = cursorX + item->deltaX;
    const int glyphY = cursorY + maxAscent_ - item->deltaY;
    for (int glyphRow = 0; glyphRow < item->height; ++glyphRow) {
      for (int glyphColumn = 0; glyphColumn < item->width; ++glyphColumn) {
        const std::uint8_t alpha = bytes_[item->bitmapOffset +
            static_cast<std::size_t>(glyphRow * item->width + glyphColumn)];
        if (alpha == 0) continue;
        const int targetX = glyphX + glyphColumn;
        const int targetY = glyphY + glyphRow;
        if (clip != nullptr && !clip->contains(targetX, targetY)) continue;
        framebuffer.setPixel(targetX, targetY,
                             blend565(foreground, background, alpha));
      }
    }
    cursorX += item->advance;
    firstGlyph = false;
  }
  return width;
}

}  // namespace sdd_sim
