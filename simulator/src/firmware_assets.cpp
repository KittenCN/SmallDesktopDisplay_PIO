#include "sdd_sim/firmware_assets.h"

#include <algorithm>
#include <cstring>

#include <pgmspace.h>

#include "font/ZdyLwFont_20.h"
#include "font/font_td_20.h"
#include "font/timeClockFont.h"
#include "img/humidity.h"
#include "img/temperature.h"
#include "weatherNum/img/tianqi/t0.h"
#include "weatherNum/img/tianqi/t1.h"
#include "weatherNum/img/tianqi/t2.h"
#include "weatherNum/img/tianqi/t3.h"
#include "weatherNum/img/tianqi/t4.h"
#include "weatherNum/img/tianqi/t5.h"
#include "weatherNum/img/tianqi/t6.h"
#include "weatherNum/img/tianqi/t7.h"
#include "weatherNum/img/tianqi/t9.h"
#include "weatherNum/img/tianqi/t11.h"
#include "weatherNum/img/tianqi/t13.h"
#include "weatherNum/img/tianqi/t14.h"
#include "weatherNum/img/tianqi/t15.h"
#include "weatherNum/img/tianqi/t16.h"
#include "weatherNum/img/tianqi/t18.h"
#include "weatherNum/img/tianqi/t19.h"
#include "weatherNum/img/tianqi/t20.h"
#include "weatherNum/img/tianqi/t26.h"
#include "weatherNum/img/tianqi/t29.h"
#include "weatherNum/img/tianqi/t30.h"
#include "weatherNum/img/tianqi/t31.h"
#include "weatherNum/img/tianqi/t53.h"
#include "weatherNum/img/tianqi/t99.h"
#include "Animate/img/astronaut.h"
#include "Animate/img/hutao.h"
#include "Animate/img/miku.h"

namespace sdd_sim {

namespace {

constexpr ByteSpan span(const std::uint8_t* data, std::size_t size) noexcept {
  return ByteSpan{data, size};
}

const std::vector<ByteSpan> kNoAnimation;
const std::vector<ByteSpan> kAstronautFrames = {
    span(i0, sizeof(i0)), span(i1, sizeof(i1)), span(i2, sizeof(i2)),
    span(i3, sizeof(i3)), span(i4, sizeof(i4)), span(i5, sizeof(i5)),
    span(i6, sizeof(i6)), span(i7, sizeof(i7)), span(i8, sizeof(i8)),
    span(i9, sizeof(i9)),
};

const std::vector<ByteSpan> kHutaoFrames = {
    span(hutao_0, sizeof(hutao_0)), span(hutao_1, sizeof(hutao_1)),
    span(hutao_2, sizeof(hutao_2)), span(hutao_3, sizeof(hutao_3)),
    span(hutao_4, sizeof(hutao_4)), span(hutao_5, sizeof(hutao_5)),
    span(hutao_6, sizeof(hutao_6)), span(hutao_7, sizeof(hutao_7)),
    span(hutao_8, sizeof(hutao_8)), span(hutao_9, sizeof(hutao_9)),
    span(hutao_10, sizeof(hutao_10)), span(hutao_11, sizeof(hutao_11)),
    span(hutao_12, sizeof(hutao_12)), span(hutao_13, sizeof(hutao_13)),
    span(hutao_14, sizeof(hutao_14)), span(hutao_15, sizeof(hutao_15)),
    span(hutao_16, sizeof(hutao_16)), span(hutao_17, sizeof(hutao_17)),
    span(hutao_18, sizeof(hutao_18)), span(hutao_19, sizeof(hutao_19)),
    span(hutao_20, sizeof(hutao_20)), span(hutao_21, sizeof(hutao_21)),
    span(hutao_22, sizeof(hutao_22)), span(hutao_23, sizeof(hutao_23)),
    span(hutao_24, sizeof(hutao_24)), span(hutao_25, sizeof(hutao_25)),
    span(hutao_26, sizeof(hutao_26)), span(hutao_27, sizeof(hutao_27)),
    span(hutao_28, sizeof(hutao_28)), span(hutao_29, sizeof(hutao_29)),
    span(hutao_30, sizeof(hutao_30)), span(hutao_31, sizeof(hutao_31)),
};

const std::vector<ByteSpan> kMikuFrames = {
    span(miku1, sizeof(miku1)), span(miku2, sizeof(miku2)),
    span(miku3, sizeof(miku3)), span(miku4, sizeof(miku4)),
};

bool decodeOne(const std::string& text, std::size_t& index,
               std::uint32_t& codepoint) noexcept {
  if (index >= text.size()) return false;
  const auto first = static_cast<std::uint8_t>(text[index++]);
  if (first < 0x80U) {
    codepoint = first;
    return true;
  }
  unsigned count = 0;
  if ((first & 0xE0U) == 0xC0U) {
    count = 1;
    codepoint = first & 0x1FU;
  } else if ((first & 0xF0U) == 0xE0U) {
    count = 2;
    codepoint = first & 0x0FU;
  } else if ((first & 0xF8U) == 0xF0U) {
    count = 3;
    codepoint = first & 0x07U;
  } else {
    codepoint = 0xFFFDU;
    return true;
  }
  if (index + count > text.size()) {
    index = text.size();
    codepoint = 0xFFFDU;
    return true;
  }
  for (unsigned part = 0; part < count; ++part) {
    const auto byte = static_cast<std::uint8_t>(text[index]);
    if ((byte & 0xC0U) != 0x80U) {
      codepoint = 0xFFFDU;
      return true;
    }
    ++index;
    codepoint = (codepoint << 6U) | (byte & 0x3FU);
  }
  return true;
}

void appendUtf8(std::string& target, std::uint32_t codepoint) {
  if (codepoint < 0x80U) {
    target.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800U) {
    target.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
    target.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  } else if (codepoint < 0x10000U) {
    target.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
    target.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
    target.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  } else {
    target.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
    target.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
    target.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
    target.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  }
}

}  // namespace

WeatherIcon weatherIconForCode(int weatherCode) noexcept {
  switch (weatherCode) {
    case 0: return WeatherIcon::Sunny;
    case 1: return WeatherIcon::Cloudy;
    case 2: return WeatherIcon::Overcast;
    case 3: return WeatherIcon::Shower;
    case 4: return WeatherIcon::ThunderShower;
    case 5: return WeatherIcon::ThunderHail;
    case 6: return WeatherIcon::Sleet;
    case 7:
    case 8:
    case 21:
    case 22: return WeatherIcon::LightRain;
    case 9:
    case 10:
    case 23:
    case 24: return WeatherIcon::ModerateRain;
    case 11:
    case 12:
    case 25:
    case 301: return WeatherIcon::HeavyRain;
    case 13: return WeatherIcon::SnowShower;
    case 14:
    case 26: return WeatherIcon::LightSnow;
    case 15:
    case 27: return WeatherIcon::ModerateSnow;
    case 16:
    case 17:
    case 28:
    case 302: return WeatherIcon::HeavySnow;
    case 18: return WeatherIcon::Fog;
    case 19: return WeatherIcon::FreezingRain;
    case 20: return WeatherIcon::Dust;
    case 29: return WeatherIcon::FloatingDust;
    case 30: return WeatherIcon::BlowingSand;
    case 31: return WeatherIcon::Sandstorm;
    case 32:
    case 49:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58: return WeatherIcon::Haze;
    default: return WeatherIcon::Unknown;
  }
}

ByteSpan weatherIconAsset(WeatherIcon icon) noexcept {
  switch (icon) {
    case WeatherIcon::Sunny: return span(t0, sizeof(t0));
    case WeatherIcon::Cloudy: return span(t1, sizeof(t1));
    case WeatherIcon::Overcast: return span(t2, sizeof(t2));
    case WeatherIcon::Shower: return span(t3, sizeof(t3));
    case WeatherIcon::ThunderShower: return span(t4, sizeof(t4));
    case WeatherIcon::ThunderHail: return span(t5, sizeof(t5));
    case WeatherIcon::Sleet: return span(t6, sizeof(t6));
    case WeatherIcon::LightRain: return span(t7, sizeof(t7));
    case WeatherIcon::ModerateRain: return span(t9, sizeof(t9));
    case WeatherIcon::HeavyRain: return span(t11, sizeof(t11));
    case WeatherIcon::SnowShower: return span(t13, sizeof(t13));
    case WeatherIcon::LightSnow: return span(t14, sizeof(t14));
    case WeatherIcon::ModerateSnow: return span(t15, sizeof(t15));
    case WeatherIcon::HeavySnow: return span(t16, sizeof(t16));
    case WeatherIcon::Fog: return span(t18, sizeof(t18));
    case WeatherIcon::FreezingRain: return span(t19, sizeof(t19));
    case WeatherIcon::Dust: return span(t20, sizeof(t20));
    case WeatherIcon::FloatingDust: return span(t29, sizeof(t29));
    case WeatherIcon::BlowingSand: return span(t30, sizeof(t30));
    case WeatherIcon::Sandstorm: return span(t31, sizeof(t31));
    case WeatherIcon::Haze: return span(t53, sizeof(t53));
    case WeatherIcon::Unknown: return span(t99, sizeof(t99));
  }
  return span(t99, sizeof(t99));
}

ByteSpan temperatureIconAsset() noexcept { return span(temperature, sizeof(temperature)); }
ByteSpan humidityIconAsset() noexcept { return span(humidity, sizeof(humidity)); }

const std::vector<ByteSpan>& animationFrames(AnimationKind kind) noexcept {
  switch (kind) {
    case AnimationKind::Astronaut: return kAstronautFrames;
    case AnimationKind::Hutao: return kHutaoFrames;
    case AnimationKind::Miku: return kMikuFrames;
    case AnimationKind::None: return kNoAnimation;
  }
  return kNoAnimation;
}

const VlwFont& weatherFont() noexcept {
  static const VlwFont font(ZdyLwFont_20, sizeof(ZdyLwFont_20));
  return font;
}

const VlwFont& calendarFont() noexcept {
  static const VlwFont font(font_td_20, sizeof(font_td_20));
  return font;
}

void drawClockDigit(Framebuffer& framebuffer, int x, int y, unsigned digit,
                    unsigned size, std::uint16_t color) noexcept {
  if (digit > 9U || size < 1U || size > 3U) return;
  const LineAtom* atoms = nullptr;
  std::uint8_t count = 0;
  int cellWidth = 0;
  int cellHeight = 0;
  if (size == 1U) {
    atoms = reinterpret_cast<const LineAtom*>(pgm_read_ptr(&smallLineFont[digit]));
    count = pgm_read_byte(&smallLineFont_size[digit]);
    cellWidth = 9;
    cellHeight = 14;
  } else if (size == 2U) {
    atoms = reinterpret_cast<const LineAtom*>(pgm_read_ptr(&middleLineFont[digit]));
    count = pgm_read_byte(&middleLineFont_size[digit]);
    cellWidth = 18;
    cellHeight = 30;
  } else {
    atoms = reinterpret_cast<const LineAtom*>(pgm_read_ptr(&largeLineFont[digit]));
    count = pgm_read_byte(&largeLineFont_size[digit]);
    cellWidth = 36;
    cellHeight = 90;
  }
  framebuffer.fillRect(x, y, cellWidth, cellHeight, color::Black);
  for (std::uint8_t index = 0; index < count; ++index) {
    LineAtom atom{};
    memcpy_P(&atom, atoms + index, sizeof(atom));
    framebuffer.drawFastHLine(x + atom.xValue, y + atom.yValue, atom.lValue, color);
  }
}

std::string replaceUnsupportedGlyphs(const VlwFont& font,
                                     const std::string& utf8) {
  std::string result;
  result.reserve(utf8.size());
  std::size_t index = 0;
  while (index < utf8.size()) {
    std::uint32_t codepoint = 0;
    if (!decodeOne(utf8, index, codepoint)) break;
    if (font.hasGlyph(codepoint))
      appendUtf8(result, codepoint);
    else
      result.push_back('-');
  }
  return result;
}

}  // namespace sdd_sim
