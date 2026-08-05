#include "sdd_sim/display_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "core/DisplayLogic.h"
#include "sdd_sim/firmware_assets.h"

namespace sdd_sim {

namespace {

template <std::size_t Count>
int selectedBanner(const CarouselState& carousel,
                   const std::array<std::string, Count>& banners) noexcept {
  if (carousel.activeIndex >= 0 && carousel.activeIndex < static_cast<int>(Count) &&
      !banners[static_cast<std::size_t>(carousel.activeIndex)].empty()) {
    return carousel.activeIndex;
  }
  const std::size_t start = Count == 0 ? 0 : carousel.currentIndex % Count;
  for (std::size_t offset = 0; offset < Count; ++offset) {
    const std::size_t index = (start + offset) % Count;
    if (!banners[index].empty()) return static_cast<int>(index);
  }
  return -1;
}

std::uint16_t temperatureColor(int width) noexcept {
  if (width < 10) return 0x00FF;
  if (width < 28) return 0x0AFF;
  if (width < 34) return 0x0F0F;
  if (width < 41) return 0xFF0F;
  return 0xF00F;
}

std::uint16_t humidityColor(int humidity) noexcept {
  if (humidity > 90) return 0x00FF;
  if (humidity > 70) return 0x0AFF;
  if (humidity > 40) return 0x0F0F;
  if (humidity > 20) return 0xFF0F;
  return 0xF00F;
}

struct AqiPresentation {
  std::string label;
  std::uint16_t background;
};

AqiPresentation aqiPresentation(int aqi) {
  switch (sdd::classifyAqi(aqi)) {
    case sdd::AqiLevel::Excellent:
      return {"优", Framebuffer::rgb565(156, 202, 127)};
    case sdd::AqiLevel::Good:
      return {"良", Framebuffer::rgb565(247, 219, 100)};
    case sdd::AqiLevel::Light:
      return {"轻度", Framebuffer::rgb565(242, 159, 57)};
    case sdd::AqiLevel::Moderate:
      return {"中度", Framebuffer::rgb565(186, 55, 121)};
    case sdd::AqiLevel::Heavy:
      return {"重度", Framebuffer::rgb565(136, 11, 32)};
    case sdd::AqiLevel::Severe:
      return {"严重", Framebuffer::rgb565(88, 6, 20)};
    case sdd::AqiLevel::Unknown:
      return {"未知", Framebuffer::rgb565(80, 80, 80)};
  }
  return {"未知", Framebuffer::rgb565(80, 80, 80)};
}

template <std::size_t Count>
void drawBanner(Framebuffer& framebuffer, const VlwFont& font,
                const std::array<std::string, Count>& banners,
                const CarouselState& carousel, int screenY) {
  const Rect region{5, screenY, 150, 30};
  framebuffer.fillRect(region.x, region.y, region.width, region.height, color::Black);
  const int selected = selectedBanner(carousel, banners);
  if (selected < 0) return;
  const std::string text = replaceUnsupportedGlyphs(
      font, banners[static_cast<std::size_t>(selected)]);
  const int width = font.textWidth(text);
  if (width > region.width) {
    font.drawText(framebuffer, text, region.x - carousel.offset, region.y + 15,
                  color::White, color::Black, TextAlign::MiddleLeft, &region);
  } else {
    font.drawText(framebuffer, text, region.x + 74, region.y + 16,
                  color::White, color::Black, TextAlign::Center, &region);
  }
}

std::string formatOneDecimal(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1) << value;
  return stream.str();
}

}  // namespace

void DisplayRenderer::setJpegDecoder(JpegDecoder* decoder) noexcept {
  if (jpegDecoder_ == decoder) return;
  jpegDecoder_ = decoder;
  jpegCache_.clear();
}

void DisplayRenderer::drawJpeg(Framebuffer& framebuffer, int x, int y,
                               ByteSpan asset) const {
  if (!asset || jpegDecoder_ == nullptr) return;
  auto iterator = jpegCache_.find(asset.data);
  if (iterator == jpegCache_.end()) {
    JpegImage decoded;
    if (!jpegDecoder_->decode(asset.data, asset.size, decoded) || !decoded.valid()) return;
    iterator = jpegCache_.emplace(asset.data, std::move(decoded)).first;
  }
  const JpegImage& image = iterator->second;
  framebuffer.blit(x, y, image.width, image.height, image.rgb565.data(), image.width);
}

void DisplayRenderer::render(const SimulatorState& state,
                             Framebuffer& framebuffer) const {
  framebuffer.clear(color::Black);

  const int hour = std::max(0, std::min(23, state.clock.hour));
  const int minute = std::max(0, std::min(59, state.clock.minute));
  const int second = std::max(0, std::min(59, state.clock.second));
  drawClockDigit(framebuffer, 20, 82, static_cast<unsigned>(hour / 10), 3,
                 color::White);
  drawClockDigit(framebuffer, 60, 82, static_cast<unsigned>(hour % 10), 3,
                 color::White);
  drawClockDigit(framebuffer, 101, 82, static_cast<unsigned>(minute / 10), 3,
                 color::ClockYellow);
  drawClockDigit(framebuffer, 141, 82, static_cast<unsigned>(minute % 10), 3,
                 color::ClockYellow);
  drawClockDigit(framebuffer, 182, 112, static_cast<unsigned>(second / 10), 2,
                 color::White);
  drawClockDigit(framebuffer, 202, 112, static_cast<unsigned>(second % 10), 2,
                 color::White);

  const VlwFont& weatherTextFont = weatherFont();
  const Rect cityRegion{5, 15, 70, 30};
  weatherTextFont.drawText(
      framebuffer, replaceUnsupportedGlyphs(weatherTextFont, state.weather.city),
      49, 31, color::White, color::Black, TextAlign::Center, &cityRegion);

  const AqiPresentation aqi = aqiPresentation(state.weather.aqi);
  const Rect aqiRegion{80, 18, 85, 24};
  framebuffer.fillRoundRect(aqiRegion.x, aqiRegion.y, aqiRegion.width,
                            aqiRegion.height, 4, aqi.background);
  std::string aqiText = aqi.label;
  if (state.weather.aqi >= 0) aqiText += " " + std::to_string(state.weather.aqi);
  weatherTextFont.drawText(
      framebuffer, replaceUnsupportedGlyphs(weatherTextFont, aqiText), 120, 31,
      color::Black, aqi.background, TextAlign::Center, &aqiRegion);

  drawJpeg(framebuffer, 170, 15,
           weatherIconAsset(weatherIconForCode(state.weather.weatherCode)));
  drawBanner(framebuffer, weatherTextFont, state.weather.banners,
             state.weatherCarousel, 45);
  drawBanner(framebuffer, calendarFont(), state.calendar.banners,
             state.calendarCarousel, 150);

  drawJpeg(framebuffer, 15, 183, temperatureIconAsset());
  drawJpeg(framebuffer, 15, 213, humidityIconAsset());

  const Rect temperatureTextRegion{100, 184, 58, 24};
  weatherTextFont.drawText(
      framebuffer,
      replaceUnsupportedGlyphs(weatherTextFont,
                               state.weather.temperatureText + "℃"),
      128, 197, color::White, color::Black, TextAlign::Center,
      &temperatureTextRegion);
  const int temperatureWidth =
      static_cast<int>(sdd::temperatureBarWidth(
          static_cast<int>(state.weather.temperatureCelsius)));
  framebuffer.drawRoundRect(45, 192, 52, 6, 3, color::White);
  framebuffer.fillRoundRect(46, 193, temperatureWidth, 4, 2,
                            temperatureColor(temperatureWidth));

  const Rect humidityTextRegion{100, 214, 58, 24};
  weatherTextFont.drawText(
      framebuffer,
      replaceUnsupportedGlyphs(weatherTextFont, state.weather.humidityText),
      128, 227, color::White, color::Black, TextAlign::Center,
      &humidityTextRegion);
  const int humidity = std::max(0, std::min(100, state.weather.relativeHumidity));
  framebuffer.drawRoundRect(45, 222, 52, 6, 3, color::White);
  framebuffer.fillRoundRect(46, 223, humidity / 2, 4, 2,
                            humidityColor(humidity));

  if (state.indoor.enabled) {
    framebuffer.fillRect(160, 150, 80, 90, color::Black);
    const Rect labelRegion{172, 150, 58, 30};
    weatherTextFont.drawText(framebuffer, "内温", 201, 166, color::White,
                             color::Black, TextAlign::Center, &labelRegion);
    if (state.indoor.valid && std::isfinite(state.indoor.temperatureCelsius) &&
        std::isfinite(state.indoor.relativeHumidity)) {
      const Rect indoorTemperatureRegion{170, 184, 60, 24};
      weatherTextFont.drawText(
          framebuffer,
          replaceUnsupportedGlyphs(weatherTextFont,
                                   formatOneDecimal(state.indoor.temperatureCelsius) +
                                       "℃"),
          200, 197, color::White, color::Black, TextAlign::Center,
          &indoorTemperatureRegion);
      const Rect indoorHumidityRegion{170, 214, 60, 24};
      weatherTextFont.drawText(
          framebuffer,
          replaceUnsupportedGlyphs(weatherTextFont,
                                   formatOneDecimal(state.indoor.relativeHumidity) +
                                       "%"),
          200, 227, color::White, color::Black, TextAlign::Center,
          &indoorHumidityRegion);
    }
  } else {
    const auto& frames = animationFrames(state.settings.animation);
    if (!frames.empty()) {
      drawJpeg(framebuffer, 160, 160, frames[state.animationFrame % frames.size()]);
    }
  }
}

}  // namespace sdd_sim
