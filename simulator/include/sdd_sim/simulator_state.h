#ifndef SDD_SIM_SIMULATOR_STATE_H
#define SDD_SIM_SIMULATOR_STATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "sdd_sim/firmware_assets.h"

namespace sdd_sim {

struct ClockViewModel {
  int hour = 12;
  int minute = 34;
  int second = 56;
};

struct WeatherViewModel {
  std::string city = "上海";
  std::string temperatureText = "26";
  float temperatureCelsius = 26.0F;
  std::string humidityText = "65%";
  int relativeHumidity = 65;
  int aqi = 42;
  int weatherCode = 1;
  std::array<std::string, 6> banners{{"天气 多云", "AQI 优 42", "风向 东南风3级",
                                             "今日 多云", "最低温度 22℃", "最高温度 29℃"}};
};

struct CalendarViewModel {
  std::array<std::string, 5> banners{{"公历 2026年", "8月5日 周三",
                                              "农历 2026年 午马", "6月 廿三",
                                              "丙午 乙未 辛亥"}};
};

struct IndoorViewModel {
  bool enabled = false;
  bool valid = true;
  float temperatureCelsius = 25.0F;
  float relativeHumidity = 60.0F;
};

struct DisplaySettings {
  int brightness = 50;
  int rotation = 0;
  AnimationKind animation = AnimationKind::Miku;
};

struct CarouselState {
  std::size_t currentIndex = 0;
  int activeIndex = -1;
  int offset = 0;
};

class SimulatorState {
 public:
  ClockViewModel clock;
  WeatherViewModel weather;
  CalendarViewModel calendar;
  IndoorViewModel indoor;
  DisplaySettings settings;
  CarouselState weatherCarousel;
  CarouselState calendarCarousel;

  std::size_t animationFrame = 0;
  bool paused = false;
  double timeScale = 1.0;

  void reset();
  void tick(std::uint32_t elapsedMilliseconds);
  void stepAnimation();
  void advanceBanners();
  void cycleBrightness();
  void setRotation(int rotation) noexcept;
  void setBrightness(int brightness) noexcept;
  void setAnimation(AnimationKind animation) noexcept;

 private:
  std::uint64_t clockAccumulatorMs_ = 0;
  std::uint64_t animationAccumulatorMs_ = 0;
  std::uint64_t bannerAccumulatorMs_ = 0;
};

}  // namespace sdd_sim

#endif
