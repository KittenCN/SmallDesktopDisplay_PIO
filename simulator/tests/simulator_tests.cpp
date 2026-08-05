#include "sdd_sim/sdd_sim.h"
#include "sdd_sim/gdiplus_jpeg_decoder.h"

#include "../../src/core/DisplayLogic.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* expression, const char* file, int line) {
  if (condition) return;
  ++failures;
  std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
}

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)

std::size_t countPixels(const sdd_sim::Framebuffer& framebuffer,
                        std::uint16_t value) {
  return static_cast<std::size_t>(std::count(framebuffer.pixels().begin(),
                                             framebuffer.pixels().end(), value));
}

std::uint64_t fnv1a64(const std::vector<std::uint16_t>& pixels) noexcept {
  std::uint64_t hash = 14695981039346656037ULL;
  for (std::uint16_t pixel : pixels) {
    // Canonical little-endian RGB565 bytes; independent of host byte order.
    hash ^= static_cast<std::uint8_t>(pixel & 0xFFU);
    hash *= 1099511628211ULL;
    hash ^= static_cast<std::uint8_t>(pixel >> 8U);
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string hex64(std::uint64_t value) {
  std::ostringstream output;
  output << std::hex;
  output.width(16);
  output.fill('0');
  output << value;
  return output.str();
}

std::string readText(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot open scenario: " + path.string());
  return std::string(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
}

std::string fieldText(const std::string& json, const std::string& key) {
  const std::string marker = '"' + key + '"';
  std::size_t cursor = json.find(marker);
  if (cursor == std::string::npos) throw std::runtime_error("missing field: " + key);
  cursor = json.find(':', cursor + marker.size());
  if (cursor == std::string::npos) throw std::runtime_error("missing colon: " + key);
  cursor = json.find_first_not_of(" \t\r\n", cursor + 1U);
  if (cursor == std::string::npos) throw std::runtime_error("missing value: " + key);
  if (json[cursor] == '"') {
    const std::size_t end = json.find('"', cursor + 1U);
    if (end == std::string::npos) throw std::runtime_error("unterminated string: " + key);
    return json.substr(cursor + 1U, end - cursor - 1U);
  }
  const std::size_t end = json.find_first_of(",}\r\n", cursor);
  return json.substr(cursor, end - cursor);
}

int fieldInt(const std::string& json, const std::string& key) {
  const std::string value = fieldText(json, key);
  std::size_t consumed = 0;
  const int parsed = std::stoi(value, &consumed);
  if (consumed != value.size()) throw std::runtime_error("invalid integer: " + key);
  return parsed;
}

sdd_sim::AnimationKind animationFromText(const std::string& value) {
  if (value == "none") return sdd_sim::AnimationKind::None;
  if (value == "astronaut") return sdd_sim::AnimationKind::Astronaut;
  if (value == "hutao") return sdd_sim::AnimationKind::Hutao;
  if (value == "miku") return sdd_sim::AnimationKind::Miku;
  throw std::runtime_error("unknown animation: " + value);
}

class DeterministicJpegDecoder final : public sdd_sim::JpegDecoder {
 public:
  bool decode(const std::uint8_t* bytes, std::size_t byteCount,
              sdd_sim::JpegImage& output) override {
    if (bytes == nullptr || byteCount < 4U) return false;
    std::uint32_t seed = 2166136261U;
    for (std::size_t index = 0; index < byteCount; ++index) {
      seed ^= bytes[index];
      seed *= 16777619U;
    }
    output.width = 7;
    output.height = 5;
    output.rgb565.resize(35U);
    for (std::size_t index = 0; index < output.rgb565.size(); ++index) {
      seed = seed * 1664525U + 1013904223U;
      output.rgb565[index] = static_cast<std::uint16_t>(seed >> 16U);
    }
    return true;
  }
};

void testFramebufferAndRgb565() {
  sdd_sim::Framebuffer framebuffer;
  CHECK(framebuffer.width() == 240);
  CHECK(framebuffer.height() == 240);
  CHECK(framebuffer.pixels().size() == 57600U);
  CHECK(countPixels(framebuffer, sdd_sim::color::Black) == 57600U);

  framebuffer.fillRect(-20, 10, 5, 5, sdd_sim::color::White);
  framebuffer.fillRect(250, 10, 5, 5, sdd_sim::color::White);
  framebuffer.fillRect(10, -20, 5, 5, sdd_sim::color::White);
  framebuffer.fillRect(10, 250, 5, 5, sdd_sim::color::White);
  framebuffer.fillRect(std::numeric_limits<int>::max(), 10, 5, 5,
                       sdd_sim::color::White);
  framebuffer.fillRect(std::numeric_limits<int>::min(), 10,
                       std::numeric_limits<int>::max(), 5,
                       sdd_sim::color::White);
  CHECK(countPixels(framebuffer, sdd_sim::color::White) == 0U);

  framebuffer.fillRect(-2, -2, 4, 4, sdd_sim::color::White);
  CHECK(countPixels(framebuffer, sdd_sim::color::White) == 4U);
  CHECK(framebuffer.pixel(0, 0) == sdd_sim::color::White);
  CHECK(framebuffer.pixel(1, 1) == sdd_sim::color::White);
  CHECK(framebuffer.pixel(2, 2) == sdd_sim::color::Black);
  framebuffer.setPixel(-1, -1, sdd_sim::color::Green);
  framebuffer.setPixel(240, 240, sdd_sim::color::Green);
  CHECK(countPixels(framebuffer, sdd_sim::color::Green) == 0U);

  CHECK(sdd_sim::Framebuffer::rgb565(255, 0, 0) == 0xF800U);
  CHECK(sdd_sim::Framebuffer::rgb565(0, 255, 0) == 0x07E0U);
  CHECK(sdd_sim::Framebuffer::rgb565(0, 0, 255) == 0x001FU);
  CHECK(sdd_sim::Framebuffer::rgb565(255, 255, 255) == 0xFFFFU);

  framebuffer.clear();
  framebuffer.setPixel(0, 0, 0xF800U);
  framebuffer.setPixel(0, 239, 0x07E0U);
  framebuffer.setPixel(239, 239, 0x001FU);
  framebuffer.setPixel(239, 0, 0xFFFFU);
  CHECK(framebuffer.toArgb8888(0, 100).front() == 0xFFFF0000U);
  CHECK(framebuffer.toArgb8888(1, 100).front() == 0xFF00FF00U);
  CHECK(framebuffer.toArgb8888(2, 100).front() == 0xFF0000FFU);
  CHECK(framebuffer.toArgb8888(3, 100).front() == 0xFFFFFFFFU);
  CHECK(framebuffer.toArgb8888(3, 50).front() == 0xFF7F7F7FU);
  CHECK(framebuffer.toArgb8888(0, 0).front() == 0xFF000000U);
}

void testDisplayLogicBoundaries() {
  CHECK(sdd::classifyAqi(-1) == sdd::AqiLevel::Unknown);
  CHECK(sdd::classifyAqi(0) == sdd::AqiLevel::Excellent);
  CHECK(sdd::classifyAqi(50) == sdd::AqiLevel::Excellent);
  CHECK(sdd::classifyAqi(51) == sdd::AqiLevel::Good);
  CHECK(sdd::classifyAqi(100) == sdd::AqiLevel::Good);
  CHECK(sdd::classifyAqi(101) == sdd::AqiLevel::Light);
  CHECK(sdd::classifyAqi(150) == sdd::AqiLevel::Light);
  CHECK(sdd::classifyAqi(151) == sdd::AqiLevel::Moderate);
  CHECK(sdd::classifyAqi(200) == sdd::AqiLevel::Moderate);
  CHECK(sdd::classifyAqi(201) == sdd::AqiLevel::Heavy);
  CHECK(sdd::classifyAqi(300) == sdd::AqiLevel::Heavy);
  CHECK(sdd::classifyAqi(301) == sdd::AqiLevel::Severe);

  CHECK(sdd::temperatureBarWidth(-80) == 0U);
  CHECK(sdd::temperatureBarWidth(-10) == 0U);
  CHECK(sdd::temperatureBarWidth(-9) == 1U);
  CHECK(sdd::temperatureBarWidth(39) == 49U);
  CHECK(sdd::temperatureBarWidth(40) == 50U);
  CHECK(sdd::temperatureBarWidth(80) == 50U);
  CHECK(sdd::humidityBarWidth(-1) == 0U);
  CHECK(sdd::humidityBarWidth(0) == 0U);
  CHECK(sdd::humidityBarWidth(1) == 0U);
  CHECK(sdd::humidityBarWidth(99) == 49U);
  CHECK(sdd::humidityBarWidth(100) == 50U);
  CHECK(sdd::humidityBarWidth(101) == 50U);
  CHECK(sdd::isValidTemperature(-80));
  CHECK(!sdd::isValidTemperature(-81));
  CHECK(sdd::isValidTemperature(80));
  CHECK(!sdd::isValidTemperature(81));
  CHECK(sdd::isValidHumidity(0));
  CHECK(sdd::isValidHumidity(100));
  CHECK(!sdd::isValidHumidity(101));
}

void checkJpeg(const sdd_sim::ByteSpan asset) {
  CHECK(static_cast<bool>(asset));
  if (!asset || asset.size < 4U) return;
  CHECK(asset.data[0] == 0xFFU);
  CHECK(asset.data[1] == 0xD8U);
  CHECK(asset.data[asset.size - 2U] == 0xFFU);
  CHECK(asset.data[asset.size - 1U] == 0xD9U);
}

void testWeatherMappingAndAssets() {
  using Pair = std::pair<int, sdd_sim::WeatherIcon>;
  const std::array<Pair, 43> cases{{
      {0, sdd_sim::WeatherIcon::Sunny}, {1, sdd_sim::WeatherIcon::Cloudy},
      {2, sdd_sim::WeatherIcon::Overcast}, {3, sdd_sim::WeatherIcon::Shower},
      {4, sdd_sim::WeatherIcon::ThunderShower}, {5, sdd_sim::WeatherIcon::ThunderHail},
      {6, sdd_sim::WeatherIcon::Sleet}, {7, sdd_sim::WeatherIcon::LightRain},
      {8, sdd_sim::WeatherIcon::LightRain}, {21, sdd_sim::WeatherIcon::LightRain},
      {22, sdd_sim::WeatherIcon::LightRain}, {9, sdd_sim::WeatherIcon::ModerateRain},
      {10, sdd_sim::WeatherIcon::ModerateRain}, {23, sdd_sim::WeatherIcon::ModerateRain},
      {24, sdd_sim::WeatherIcon::ModerateRain}, {11, sdd_sim::WeatherIcon::HeavyRain},
      {12, sdd_sim::WeatherIcon::HeavyRain}, {25, sdd_sim::WeatherIcon::HeavyRain},
      {301, sdd_sim::WeatherIcon::HeavyRain}, {13, sdd_sim::WeatherIcon::SnowShower},
      {14, sdd_sim::WeatherIcon::LightSnow}, {26, sdd_sim::WeatherIcon::LightSnow},
      {15, sdd_sim::WeatherIcon::ModerateSnow}, {27, sdd_sim::WeatherIcon::ModerateSnow},
      {16, sdd_sim::WeatherIcon::HeavySnow}, {17, sdd_sim::WeatherIcon::HeavySnow},
      {28, sdd_sim::WeatherIcon::HeavySnow}, {302, sdd_sim::WeatherIcon::HeavySnow},
      {18, sdd_sim::WeatherIcon::Fog}, {19, sdd_sim::WeatherIcon::FreezingRain},
      {20, sdd_sim::WeatherIcon::Dust}, {29, sdd_sim::WeatherIcon::FloatingDust},
      {30, sdd_sim::WeatherIcon::BlowingSand}, {31, sdd_sim::WeatherIcon::Sandstorm},
      {32, sdd_sim::WeatherIcon::Haze}, {49, sdd_sim::WeatherIcon::Haze},
      {53, sdd_sim::WeatherIcon::Haze}, {54, sdd_sim::WeatherIcon::Haze},
      {55, sdd_sim::WeatherIcon::Haze}, {56, sdd_sim::WeatherIcon::Haze},
      {57, sdd_sim::WeatherIcon::Haze}, {58, sdd_sim::WeatherIcon::Haze},
      {999, sdd_sim::WeatherIcon::Unknown},
  }};
  for (const auto& item : cases) {
    CHECK(sdd_sim::weatherIconForCode(item.first) == item.second);
    checkJpeg(sdd_sim::weatherIconAsset(item.second));
  }
  const auto floatingDust = sdd_sim::weatherIconAsset(sdd_sim::weatherIconForCode(29));
  const auto blowingSand = sdd_sim::weatherIconAsset(sdd_sim::weatherIconForCode(30));
  const auto sandstorm = sdd_sim::weatherIconAsset(sdd_sim::weatherIconForCode(31));
  CHECK(floatingDust.data != blowingSand.data);
  CHECK(blowingSand.data != sandstorm.data);
  CHECK(floatingDust.data != sandstorm.data);
  checkJpeg(sdd_sim::temperatureIconAsset());
  checkJpeg(sdd_sim::humidityIconAsset());
  CHECK(sdd_sim::weatherFont().valid());
  CHECK(sdd_sim::calendarFont().valid());
}

void testCarouselsRotationAndBrightness() {
  const std::array<std::string, 5> pages{{"", "", "WEATHER WAIT", "", "NEXT"}};
  CHECK(sdd::nextNonEmptyIndex(pages.data(), pages.size(), 0) == 2);
  CHECK(sdd::nextNonEmptyIndex(pages.data(), pages.size(), 3) == 4);
  CHECK(sdd::nextNonEmptyIndex(pages.data(), pages.size(), 5) == 2);
  const std::array<std::string, 2> emptyPages{{"", ""}};
  CHECK(sdd::nextNonEmptyIndex(emptyPages.data(), emptyPages.size(), 0) == -1);
  CHECK(sdd::bannerMaximumOffset(150, 150) == 0);
  CHECK(sdd::bannerMaximumOffset(188, 150) == 38);
  CHECK(sdd::nextBannerOffset(0, 188, 75) == 75);
  CHECK(sdd::nextBannerOffset(150, 188, 75) == 188);

  sdd_sim::SimulatorState state;
  state.weather.banners = {{"WWWWWWWWWWWWWWWWWWWWWWWW", "", "NEXT", "", ""}};
  state.weatherCarousel = {};
  state.advanceBanners();
  CHECK(state.weatherCarousel.activeIndex == 0);
  CHECK(state.weatherCarousel.offset == 75);
  for (int index = 0; index < 20 && state.weatherCarousel.currentIndex == 0U; ++index)
    state.advanceBanners();
  CHECK(state.weatherCarousel.currentIndex == 1U);
  state.advanceBanners();
  CHECK(state.weatherCarousel.currentIndex == 3U);

  state.setBrightness(-10);
  CHECK(state.settings.brightness == 0);
  const std::array<int, 5> levels{{25, 50, 75, 100, 25}};
  for (int expected : levels) {
    state.cycleBrightness();
    CHECK(state.settings.brightness == expected);
  }
  state.setBrightness(110);
  CHECK(state.settings.brightness == 100);

  state.setRotation(-1);
  CHECK(state.settings.rotation == 3);
  state.setRotation(0);
  CHECK(state.settings.rotation == 0);
  state.setRotation(5);
  CHECK(state.settings.rotation == 1);
  state.setRotation(6);
  CHECK(state.settings.rotation == 2);
}

void testAnimationsAndClockTick() {
  using Animation = sdd_sim::AnimationKind;
  const std::array<std::pair<Animation, std::size_t>, 4> counts{{
      {Animation::None, 0U}, {Animation::Astronaut, 10U},
      {Animation::Hutao, 32U}, {Animation::Miku, 4U},
  }};
  sdd_sim::SimulatorState state;
  for (const auto& item : counts) {
    const auto& frames = sdd_sim::animationFrames(item.first);
    CHECK(frames.size() == item.second);
    for (sdd_sim::ByteSpan frame : frames) checkJpeg(frame);
    state.setAnimation(item.first);
    CHECK(state.animationFrame == 0U);
    for (std::size_t index = 0; index < item.second; ++index) state.stepAnimation();
    CHECK(state.animationFrame == 0U);
  }

  state.clock = {23, 59, 59};
  state.setAnimation(Animation::Miku);
  state.tick(1000U);
  CHECK(state.clock.hour == 0);
  CHECK(state.clock.minute == 0);
  CHECK(state.clock.second == 0);
  CHECK(state.animationFrame == 2U);  // Ten 100 ms steps modulo four frames.
  state.paused = true;
  state.tick(5000U);
  CHECK(state.clock.second == 0);
  CHECK(state.animationFrame == 2U);
}

void testRealJpegDecoder() {
  sdd_sim::GdiPlusSession session;
  CHECK(session.ready());
  if (!session.ready()) return;
  sdd_sim::GdiPlusJpegDecoder decoder;
  sdd_sim::JpegImage decoded;
  const auto weather = sdd_sim::weatherIconAsset(sdd_sim::WeatherIcon::Sunny);
  CHECK(decoder.decode(weather.data, weather.size, decoded));
  CHECK(decoded.valid());
  CHECK(decoded.width == 60);
  CHECK(decoded.height == 60);
  const auto temperature = sdd_sim::temperatureIconAsset();
  CHECK(decoder.decode(temperature.data, temperature.size, decoded));
  CHECK(decoded.width == 24);
  CHECK(decoded.height == 24);
  const std::array<sdd_sim::AnimationKind, 3> animations{{
      sdd_sim::AnimationKind::Astronaut,
      sdd_sim::AnimationKind::Hutao,
      sdd_sim::AnimationKind::Miku,
  }};
  for (sdd_sim::AnimationKind animation : animations) {
    const auto& frames = sdd_sim::animationFrames(animation);
    CHECK(!frames.empty());
    if (frames.empty()) continue;
    CHECK(decoder.decode(frames.front().data, frames.front().size, decoded));
    CHECK(decoded.width == 70);
    CHECK(decoded.height == 70);
  }
}

sdd_sim::SimulatorState stateFromScenario(const std::string& json) {
  sdd_sim::SimulatorState state;
  state.clock.hour = fieldInt(json, "hour");
  state.clock.minute = fieldInt(json, "minute");
  state.clock.second = fieldInt(json, "second");
  state.weather.city = fieldText(json, "city");
  state.weather.temperatureText = fieldText(json, "temperature_text");
  state.weather.temperatureCelsius = static_cast<float>(fieldInt(json, "temperature_celsius"));
  state.weather.humidityText = fieldText(json, "humidity_text");
  state.weather.relativeHumidity = fieldInt(json, "relative_humidity");
  state.weather.aqi = fieldInt(json, "aqi");
  state.weather.weatherCode = fieldInt(json, "weather_code");
  state.setBrightness(fieldInt(json, "brightness"));
  state.setRotation(fieldInt(json, "rotation"));
  state.setAnimation(animationFromText(fieldText(json, "animation")));
  state.animationFrame = static_cast<std::size_t>(fieldInt(json, "animation_frame"));
  state.weather.banners = {{"WEATHER WAIT", "AQI 42", "WIND", "TODAY", "LOW", "HIGH"}};
  state.calendar.banners = {{"2026", "8-5", "LUNAR", "6-23", "DATE"}};
  return state;
}

void testFixedScenarioHashes() {
  const std::filesystem::path scenarioRoot =
      std::filesystem::path(__FILE__).parent_path().parent_path() / "scenarios";
  const std::array<const char*, 2> names{{"fixed_default.json", "fixed_boundaries.json"}};
  DeterministicJpegDecoder decoder;
  sdd_sim::DisplayRenderer renderer(&decoder);
  for (const char* name : names) {
    const std::string json = readText(scenarioRoot / name);
    sdd_sim::SimulatorState state = stateFromScenario(json);
    sdd_sim::Framebuffer first;
    sdd_sim::Framebuffer second;
    renderer.render(state, first);
    renderer.render(state, second);
    CHECK(first.pixels() == second.pixels());
    const std::string actual = hex64(fnv1a64(first.pixels()));
    const std::string expected = fieldText(json, "expected_fnv1a64");
    if (expected == "pending") {
      std::cerr << name << " expected_fnv1a64=" << actual << '\n';
      CHECK(false);
    } else {
      CHECK(actual == expected);
    }
  }
}

}  // namespace

int main() {
  try {
    testFramebufferAndRgb565();
    testDisplayLogicBoundaries();
    testWeatherMappingAndAssets();
    testCarouselsRotationAndBrightness();
    testAnimationsAndClockTick();
    testRealJpegDecoder();
    testFixedScenarioHashes();
  } catch (const std::exception& error) {
    ++failures;
    std::cerr << "unexpected exception: " << error.what() << '\n';
  }
  if (failures == 0) {
    std::cout << "All simulator self-tests passed.\n";
    return 0;
  }
  std::cerr << failures << " simulator self-test(s) failed.\n";
  return 1;
}
