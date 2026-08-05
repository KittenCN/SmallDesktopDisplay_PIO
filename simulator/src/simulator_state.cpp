#include "sdd_sim/simulator_state.h"

#include <algorithm>
#include <cmath>

namespace sdd_sim {

namespace {

template <std::size_t Count>
int nextNonEmptyIndex(const std::array<std::string, Count>& items,
                      std::size_t start) noexcept {
  if (Count == 0) return -1;
  start %= Count;
  for (std::size_t offset = 0; offset < Count; ++offset) {
    const std::size_t index = (start + offset) % Count;
    if (!items[index].empty()) return static_cast<int>(index);
  }
  return -1;
}

template <std::size_t Count>
void advanceCarousel(CarouselState& state,
                     const std::array<std::string, Count>& items,
                     const VlwFont& font) {
  int selected = state.activeIndex;
  if (selected < 0 || selected >= static_cast<int>(Count) ||
      items[static_cast<std::size_t>(selected)].empty()) {
    selected = nextNonEmptyIndex(items, state.currentIndex);
  }
  if (selected < 0) {
    state = {};
    return;
  }
  const int maximumOffset =
      std::max(0, font.textWidth(items[static_cast<std::size_t>(selected)]) - 150);
  if (state.offset < maximumOffset) {
    state.offset = std::min(maximumOffset, state.offset + 75);
    state.activeIndex = selected;
  } else {
    state.offset = 0;
    state.activeIndex = -1;
    state.currentIndex = (static_cast<std::size_t>(selected) + 1U) % Count;
  }
}

}  // namespace

void SimulatorState::reset() {
  *this = SimulatorState{};
}

void SimulatorState::tick(std::uint32_t elapsedMilliseconds) {
  if (paused || elapsedMilliseconds == 0U) return;
  const double boundedScale = std::max(0.0, std::min(1000.0, timeScale));
  const auto scaled = static_cast<std::uint64_t>(
      std::llround(static_cast<double>(elapsedMilliseconds) * boundedScale));
  clockAccumulatorMs_ += scaled;
  animationAccumulatorMs_ += scaled;
  bannerAccumulatorMs_ += scaled;

  while (clockAccumulatorMs_ >= 1000U) {
    clockAccumulatorMs_ -= 1000U;
    if (++clock.second >= 60) {
      clock.second = 0;
      if (++clock.minute >= 60) {
        clock.minute = 0;
        clock.hour = (clock.hour + 1) % 24;
      }
    }
  }
  while (animationAccumulatorMs_ >= 100U) {
    animationAccumulatorMs_ -= 100U;
    stepAnimation();
  }
  while (bannerAccumulatorMs_ >= 2000U) {
    bannerAccumulatorMs_ -= 2000U;
    advanceBanners();
  }
}

void SimulatorState::stepAnimation() {
  const auto& frames = animationFrames(settings.animation);
  animationFrame = frames.empty() ? 0U : (animationFrame + 1U) % frames.size();
}

void SimulatorState::advanceBanners() {
  advanceCarousel(weatherCarousel, weather.banners, weatherFont());
  advanceCarousel(calendarCarousel, calendar.banners, calendarFont());
}

void SimulatorState::cycleBrightness() {
  static constexpr int levels[] = {25, 50, 75, 100};
  for (int level : levels) {
    if (settings.brightness < level) {
      settings.brightness = level;
      return;
    }
  }
  settings.brightness = levels[0];
}

void SimulatorState::setRotation(int rotation) noexcept {
  settings.rotation = ((rotation % 4) + 4) % 4;
}

void SimulatorState::setBrightness(int brightness) noexcept {
  settings.brightness = std::max(0, std::min(100, brightness));
}

void SimulatorState::setAnimation(AnimationKind animation) noexcept {
  settings.animation = animation;
  animationFrame = 0;
  animationAccumulatorMs_ = 0;
}

}  // namespace sdd_sim
