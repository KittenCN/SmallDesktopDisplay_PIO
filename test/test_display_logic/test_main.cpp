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

extern "C" void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_brightness_validation_and_pwm_mapping);
  RUN_TEST(test_persisted_setting_ranges);
  RUN_TEST(test_bar_widths_are_clamped);
  RUN_TEST(test_aqi_boundaries_follow_the_chinese_index);
  UNITY_END();
}

extern "C" void loop() {}
