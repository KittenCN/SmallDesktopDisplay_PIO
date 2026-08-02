#ifndef SDD_DISPLAY_LOGIC_H
#define SDD_DISPLAY_LOGIC_H

#include <stdint.h>

namespace sdd {

enum class AqiLevel : uint8_t {
  Excellent,
  Good,
  Light,
  Moderate,
  Heavy,
  Severe,
  Unknown,
};

constexpr bool isValidBrightness(int value) {
  return value >= 0 && value <= 100;
}

constexpr bool isValidRotation(int value) {
  return value >= 0 && value <= 3;
}

constexpr bool isValidWeatherInterval(int value) {
  return value >= 1 && value <= 60;
}

constexpr bool isValidCityCode(uint32_t value) {
  return value == 0 || (value >= 101000000UL && value <= 101999999UL);
}

constexpr uint16_t brightnessToPwm(int brightness) {
  return brightness <= 0
             ? 1023
             : (brightness >= 100
                    ? 0
                    : static_cast<uint16_t>((100 - brightness) * 1023L / 100L));
}

constexpr uint8_t temperatureBarWidth(int temperatureCelsius) {
  return temperatureCelsius <= -10
             ? 0
             : (temperatureCelsius >= 40
                    ? 50
                    : static_cast<uint8_t>(temperatureCelsius + 10));
}

constexpr uint8_t humidityBarWidth(int relativeHumidity) {
  return relativeHumidity <= 0
             ? 0
             : (relativeHumidity >= 100
                    ? 50
                    : static_cast<uint8_t>(relativeHumidity / 2));
}

// weather.com.cn returns the Chinese AQI index, not a raw PM2.5 concentration.
constexpr AqiLevel classifyAqi(int aqi) {
  return aqi < 0     ? AqiLevel::Unknown
         : aqi <= 50  ? AqiLevel::Excellent
         : aqi <= 100 ? AqiLevel::Good
         : aqi <= 150 ? AqiLevel::Light
         : aqi <= 200 ? AqiLevel::Moderate
         : aqi <= 300 ? AqiLevel::Heavy
                      : AqiLevel::Severe;
}

}  // namespace sdd

#endif
