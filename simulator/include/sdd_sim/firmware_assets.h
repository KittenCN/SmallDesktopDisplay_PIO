#ifndef SDD_SIM_FIRMWARE_ASSETS_H
#define SDD_SIM_FIRMWARE_ASSETS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sdd_sim/framebuffer.h"
#include "sdd_sim/vlw_font.h"

namespace sdd_sim {

struct ByteSpan {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;
  constexpr explicit operator bool() const noexcept { return data != nullptr && size > 0; }
};

enum class WeatherIcon {
  Sunny,
  Cloudy,
  Overcast,
  Shower,
  ThunderShower,
  ThunderHail,
  Sleet,
  LightRain,
  ModerateRain,
  HeavyRain,
  SnowShower,
  LightSnow,
  ModerateSnow,
  HeavySnow,
  Fog,
  FreezingRain,
  Dust,
  FloatingDust,
  BlowingSand,
  Sandstorm,
  Haze,
  Unknown,
};

enum class AnimationKind { None, Astronaut, Hutao, Miku };

WeatherIcon weatherIconForCode(int weatherCode) noexcept;
ByteSpan weatherIconAsset(WeatherIcon icon) noexcept;
ByteSpan temperatureIconAsset() noexcept;
ByteSpan humidityIconAsset() noexcept;
const std::vector<ByteSpan>& animationFrames(AnimationKind kind) noexcept;

const VlwFont& weatherFont() noexcept;
const VlwFont& calendarFont() noexcept;

// Draws the original PROGMEM LineAtom digit with the same 9x14, 18x30 and
// 36x90 logical cells used by the firmware.
void drawClockDigit(Framebuffer& framebuffer, int x, int y, unsigned digit,
                    unsigned size, std::uint16_t color) noexcept;

std::string replaceUnsupportedGlyphs(const VlwFont& font,
                                     const std::string& utf8);

}  // namespace sdd_sim

#endif
