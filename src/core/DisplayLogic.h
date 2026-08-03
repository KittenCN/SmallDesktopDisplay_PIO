#ifndef SDD_DISPLAY_LOGIC_H
#define SDD_DISPLAY_LOGIC_H

#include <stddef.h>
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

constexpr bool isValidTemperature(int value) {
  return value >= -80 && value <= 80;
}

constexpr bool isValidHumidity(int value) {
  return value >= 0 && value <= 100;
}

constexpr bool isValidWeatherCode(int value) {
  return value >= 0 && value <= 999;
}

inline bool isValidIsoDate(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }

  for (size_t index = 0; index < 10; ++index) {
    if (value[index] == '\0') {
      return false;
    }
    if ((index == 4 || index == 7) ? value[index] != '-'
                                   : value[index] < '0' || value[index] > '9') {
      return false;
    }
  }
  if (value[10] != '\0') {
    return false;
  }

  const int year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
                   (value[2] - '0') * 10 + value[3] - '0';
  const int month = (value[5] - '0') * 10 + value[6] - '0';
  const int day = (value[8] - '0') * 10 + value[9] - '0';
  if (year < 1970 || year > 2099 || month < 1 || month > 12 || day < 1) {
    return false;
  }

  static const uint8_t daysPerMonth[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maximumDay = daysPerMonth[month - 1];
  const bool leapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (month == 2 && leapYear) {
    maximumDay = 29;
  }
  return day <= maximumDay;
}

constexpr int bannerMaximumOffset(int textWidth, int viewportWidth) {
  return textWidth > viewportWidth ? textWidth - viewportWidth : 0;
}

constexpr int nextBannerOffset(int currentOffset, int maximumOffset, int step) {
  return currentOffset >= maximumOffset
             ? maximumOffset
             : (currentOffset + step < maximumOffset ? currentOffset + step
                                                       : maximumOffset);
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

// Returns the first non-empty carousel item at or after start, wrapping once.
// Text only needs to provide length(), so this works with Arduino String while
// remaining independently testable without the Arduino framework.
template <typename Text>
inline int nextNonEmptyIndex(const Text* items, size_t count, size_t start) {
  if (items == nullptr || count == 0) {
    return -1;
  }

  start %= count;
  for (size_t offset = 0; offset < count; ++offset) {
    const size_t index = (start + offset) % count;
    if (items[index].length() > 0) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

}  // namespace sdd

#endif
