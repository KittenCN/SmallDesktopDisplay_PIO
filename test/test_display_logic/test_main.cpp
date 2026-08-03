#include <unity.h>

#include "core/DisplayLogic.h"

void test_brightness_validation_and_pwm_mapping() {
  TEST_ASSERT_FALSE(sdd::isValidBrightness(-1));
  TEST_ASSERT_TRUE(sdd::isValidBrightness(0));
  TEST_ASSERT_TRUE(sdd::isValidBrightness(100));
  TEST_ASSERT_FALSE(sdd::isValidBrightness(101));
  TEST_ASSERT_EQUAL_UINT16(1023, sdd::brightnessToPwm(0));
  TEST_ASSERT_EQUAL_UINT16(0, sdd::brightnessToPwm(100));
  TEST_ASSERT_INT_WITHIN(1, 511, sdd::brightnessToPwm(50));
}

void test_persisted_setting_ranges() {
  TEST_ASSERT_TRUE(sdd::isValidRotation(0));
  TEST_ASSERT_TRUE(sdd::isValidRotation(3));
  TEST_ASSERT_FALSE(sdd::isValidRotation(4));
  TEST_ASSERT_TRUE(sdd::isValidWeatherInterval(1));
  TEST_ASSERT_TRUE(sdd::isValidWeatherInterval(60));
  TEST_ASSERT_FALSE(sdd::isValidWeatherInterval(0));
  TEST_ASSERT_TRUE(sdd::isValidCityCode(0));
  TEST_ASSERT_TRUE(sdd::isValidCityCode(101020200));
  TEST_ASSERT_FALSE(sdd::isValidCityCode(102000000));
}

void test_weather_value_ranges() {
  TEST_ASSERT_TRUE(sdd::isValidTemperature(-80));
  TEST_ASSERT_TRUE(sdd::isValidTemperature(80));
  TEST_ASSERT_FALSE(sdd::isValidTemperature(-81));
  TEST_ASSERT_FALSE(sdd::isValidTemperature(81));
  TEST_ASSERT_TRUE(sdd::isValidHumidity(0));
  TEST_ASSERT_TRUE(sdd::isValidHumidity(100));
  TEST_ASSERT_FALSE(sdd::isValidHumidity(101));
  TEST_ASSERT_TRUE(sdd::isValidWeatherCode(0));
  TEST_ASSERT_TRUE(sdd::isValidWeatherCode(999));
  TEST_ASSERT_FALSE(sdd::isValidWeatherCode(1000));
}

void test_iso_date_validation() {
  TEST_ASSERT_TRUE(sdd::isValidIsoDate("2024-02-29"));
  TEST_ASSERT_TRUE(sdd::isValidIsoDate("2026-08-03"));
  TEST_ASSERT_FALSE(sdd::isValidIsoDate("2025-02-29"));
  TEST_ASSERT_FALSE(sdd::isValidIsoDate("2026-13-01"));
  TEST_ASSERT_FALSE(sdd::isValidIsoDate("2026-04-31"));
  TEST_ASSERT_FALSE(sdd::isValidIsoDate("2026-8-03"));
  TEST_ASSERT_FALSE(sdd::isValidIsoDate("2026-08-03T00:00:00"));
  TEST_ASSERT_FALSE(sdd::isValidIsoDate(nullptr));
}

void test_bar_widths_are_clamped() {
  TEST_ASSERT_EQUAL_UINT8(0, sdd::temperatureBarWidth(-20));
  TEST_ASSERT_EQUAL_UINT8(10, sdd::temperatureBarWidth(0));
  TEST_ASSERT_EQUAL_UINT8(50, sdd::temperatureBarWidth(50));
  TEST_ASSERT_EQUAL_UINT8(0, sdd::humidityBarWidth(-1));
  TEST_ASSERT_EQUAL_UINT8(50, sdd::humidityBarWidth(100));
  TEST_ASSERT_EQUAL_UINT8(50, sdd::humidityBarWidth(101));
}

void test_aqi_boundaries_follow_the_chinese_index() {
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Unknown, sdd::classifyAqi(-1));
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Excellent, sdd::classifyAqi(0));
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Excellent, sdd::classifyAqi(50));
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Good, sdd::classifyAqi(100));
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Light, sdd::classifyAqi(150));
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Moderate, sdd::classifyAqi(200));
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Heavy, sdd::classifyAqi(300));
  TEST_ASSERT_EQUAL(sdd::AqiLevel::Severe, sdd::classifyAqi(301));
}

struct TestBannerText {
  size_t textLength;
  size_t length() const { return textLength; }
};

void test_carousel_skips_empty_pages_and_wraps() {
  const TestBannerText pages[] = {{3}, {0}, {0}, {5}, {0}};
  TEST_ASSERT_EQUAL_INT(0, sdd::nextNonEmptyIndex(pages, 5, 0));
  TEST_ASSERT_EQUAL_INT(3, sdd::nextNonEmptyIndex(pages, 5, 1));
  TEST_ASSERT_EQUAL_INT(3, sdd::nextNonEmptyIndex(pages, 5, 3));
  TEST_ASSERT_EQUAL_INT(0, sdd::nextNonEmptyIndex(pages, 5, 4));
  TEST_ASSERT_EQUAL_INT(0, sdd::nextNonEmptyIndex(pages, 5, 10));
}

void test_carousel_handles_no_available_pages() {
  const TestBannerText emptyPages[] = {{0}, {0}, {0}};
  TEST_ASSERT_EQUAL_INT(-1, sdd::nextNonEmptyIndex(emptyPages, 3, 0));
  TEST_ASSERT_EQUAL_INT(-1, sdd::nextNonEmptyIndex(emptyPages, 3, 2));
  TEST_ASSERT_EQUAL_INT(-1, sdd::nextNonEmptyIndex<TestBannerText>(nullptr, 0, 0));
}

void test_long_banner_scroll_offsets_are_bounded() {
  TEST_ASSERT_EQUAL_INT(0, sdd::bannerMaximumOffset(150, 150));
  TEST_ASSERT_EQUAL_INT(38, sdd::bannerMaximumOffset(188, 150));
  TEST_ASSERT_EQUAL_INT(25, sdd::nextBannerOffset(0, 38, 25));
  TEST_ASSERT_EQUAL_INT(38, sdd::nextBannerOffset(25, 38, 25));
  TEST_ASSERT_EQUAL_INT(38, sdd::nextBannerOffset(38, 38, 25));
}

int run_display_logic_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_brightness_validation_and_pwm_mapping);
  RUN_TEST(test_persisted_setting_ranges);
  RUN_TEST(test_weather_value_ranges);
  RUN_TEST(test_iso_date_validation);
  RUN_TEST(test_bar_widths_are_clamped);
  RUN_TEST(test_aqi_boundaries_follow_the_chinese_index);
  RUN_TEST(test_carousel_skips_empty_pages_and_wraps);
  RUN_TEST(test_carousel_handles_no_available_pages);
  RUN_TEST(test_long_banner_scroll_offsets_are_bounded);
  return UNITY_END();
}

#ifdef ARDUINO
extern "C" void setup() { (void)run_display_logic_tests(); }
extern "C" void loop() {}
#else
int main() { return run_display_logic_tests(); }
#endif
