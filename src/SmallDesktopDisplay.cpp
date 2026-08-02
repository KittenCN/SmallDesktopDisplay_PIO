/* *****************************************************************
 *
 * SmallDesktopDisplay
 *    小型桌面显示器
 *
 * 原  作  者：Misaka
 * 修      改：微车游
 * 再次  修改：丘山鹤
 * 三次  修改: 猫道
 * 讨  论  群：811058758、887171863、720661626
 * 创 建 日 期：2021.07.19
 * 最后更改日期：2025.2.12
 *
 *
 * 引 脚 分 配：SCK   GPIO14
 *              MOSI  GPIO13
 *              RES   GPIO2
 *              DC    GPIO0
 *              LCDBL GPIO5
 *
 *             增加DHT11温湿度传感器，传感器接口为 GPIO 12
 *
 *    感谢群友 @你别失望  提醒发现WiFi保存后无法重置的问题，目前已解决。详情查看更改说明！
 * *****************************************************************/

/* *****************************************************************
 *  库文件、头文件
 * *****************************************************************/
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <EEPROM.h>                 //内存
#include <Button2.h>                //按钮库
#include <Thread.h>                 //协程
#include <StaticThreadController.h> //协程控制
#include <limits.h>

#include "config.h"                  //配置文件
#include "weatherNum/weatherNum.h"   //天气图库
#include "Animate/Animate.h"         //动画模块
#include "font/font_td_20.h"         //字体库
#include "core/DisplayLogic.h"       //纯逻辑与边界校验

#define Version "SDD V1.5.0"
/* *****************************************************************
 *  配置使能位
 * *****************************************************************/

#if WM_EN
#include <WiFiManager.h>
// WiFiManager 参数
WiFiManager wm; // global wm instance
// WiFiManagerParameter custom_field; // global param ( for non blocking w params )
#endif

#if DHT_EN
#include "DHT.h"
#define DHTPIN 12
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#endif

// 定义按钮引脚
Button2 Button_sw1 = Button2(4);

/* *****************************************************************
 *  字库、图片库
 * *****************************************************************/
#include "font/ZdyLwFont_20.h"  //字体库
#include "font/timeClockFont.h" //字体库
#include "img/temperature.h"    //温度图标
#include "img/humidity.h"       //湿度图标

// 函数声明
void sendNTPpacket(IPAddress &address); // 向NTP服务器发送请求
time_t getNtpTime();                    // 从NTP获取时间

// void digitalClockDisplay(int reflash_en);
void savewificonfig();         // wifi ssid，psw保存到eeprom
void readwificonfig();         // 从eeprom读取WiFi信息ssid，psw
void deletewificonfig();       // 删除原有eeprom中的信息
void getCityCode();            // 发送HTTP请求并且将服务器响应通过串口输出
void getCityWeather();          // 获取城市天气
void wifi_reset(Button2 &btn); // WIFI重设
void saveParamCallback();
void cycle_brightness(Button2 &btn);
void scrollBanner();
bool weatherData(const String &cityDZ, const String &dataSK, const String &dataFC); // 天气信息写到屏幕上
void refresh_AnimatedImage();                                    // 更新右下角
void getTD();
void saveTDKeytoEEP(String td_api_key);
void readTDKeyfromEEP();
void openWifi();
void closeWifi();
void reflashTime();
void updateWeatherInterval();
bool parseStrictInt(const String &text, int &value);
void applyBacklight(int brightness);
void printDeviceStatus();
bool isValidTianApiKey(const String &key);
void digitalClockDisplay(int reflash_en);
void reflashBanner();
extern int Hour_sign;
extern int Minute_sign;
extern int Second_sign;

// 创建时间更新函数线程
Thread reflash_time = Thread();
// 创建副标题切换线程
Thread reflash_Banner = Thread();
// 创建恢复WIFI链接
Thread reflash_openWifi = Thread();
// 创建动画绘制线程
Thread reflash_Animate = Thread();

// 创建协程池
StaticThreadController<4> controller(&reflash_time, &reflash_Banner, &reflash_openWifi, &reflash_Animate);

/* *****************************************************************
 *  参数设置
 * *****************************************************************/
struct config_type
{
  char stassid[33]; // 32字节SSID + C字符串结尾
  char stapsw[65];  // 64字节PSK + C字符串结尾
};
//---------------修改此处""内的信息--------------------
// 如开启WEB配网则可不用设置这里的参数，前一个为wifi ssid，后一个为密码
config_type wificonf = {{"WiFi名"}, {"密码"}};

// 天气更新时间  X 分钟
unsigned int weatherUpdateIntervalMinutes = DEFAULT_WEATHER_INTERVAL_MINUTES;

//----------------------------------------------------

void updateWeatherInterval()
{
  if (!sdd::isValidWeatherInterval(weatherUpdateIntervalMinutes))
  {
    weatherUpdateIntervalMinutes = DEFAULT_WEATHER_INTERVAL_MINUTES;
  }
  reflash_openWifi.setInterval(static_cast<unsigned long>(weatherUpdateIntervalMinutes) * 60UL * TMS);
}

// LCD屏幕相关设置
TFT_eSPI tft = TFT_eSPI(); // 引脚请自行配置tft_espi库中的 User_Setup.h文件
TFT_eSprite clk = TFT_eSprite(&tft);
#define LCD_BL_PIN 5 // LCD背光引脚
uint16_t bgColor = 0x0000;

// 其余状态标志位
int LCD_Rotation = 0;        // LCD屏幕方向
int LCD_BL_PWM = 50;         // 屏幕亮度0-100，默认50
uint8_t Wifi_en = 1;         // WIFI模块启动  1：打开    0：关闭
int prevTime = 0;            // 滚动显示更新标志位
int DHT_img_flag = 0;        // DHT传感器使用标志位

// EEPROM参数存储地址位
constexpr int BL_addr = 1;
constexpr int Ro_addr = 2;
constexpr int DHT_addr = 3;
constexpr int WeatherInterval_addr = 4;
constexpr int CC_addr = 10;
constexpr int wifi_addr = 30;
constexpr int td_key_addr = 130;
constexpr size_t STORED_SSID_BYTES = 32;
constexpr size_t STORED_PSK_BYTES = 64;
constexpr int WifiMagic_addr = 126;
constexpr int WifiVersion_addr = 127;
constexpr int WifiCrc_addr = 128;
constexpr uint8_t WIFI_CONFIG_MAGIC = 0xA5;
constexpr uint8_t WIFI_CONFIG_VERSION = 1;

uint16_t wifiConfigCrcFromEeprom()
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < STORED_SSID_BYTES + STORED_PSK_BYTES; i++)
  {
    crc ^= static_cast<uint16_t>(EEPROM.read(wifi_addr + i)) << 8;
    for (uint8_t bit = 0; bit < 8; bit++)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

int Amimate_reflash_Time = 0; // 更新时间记录
String TD_key = "";           // 天地图密钥

/*** Component objects ***/
WeatherNum wrat;

String defcityCode = "101020200"; // 默认天气城市代码
String cityCode = defcityCode; // 天气城市代码
int tempnum = 0;               // 温度百分比
int huminum = 0;               // 湿度百分比
int tempcol = 0xffff;          // 温度显示颜色
int humicol = 0xffff;          // 湿度显示颜色

// NTP服务器参数
static const char *const ntpServerNames[] = {
    "ntp.aliyun.com",
    "ntp.tencent.com",
    "pool.ntp.org",
};
const int timeZone = 8; // 东八区

// wifi连接UDP设置参数
WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
unsigned long wifiWakeStartedAt = 0;

// 星期
String week()
{
  String wk[7] = {"日", "一", "二", "三", "四", "五", "六"};
  String s = "周" + wk[weekday() - 1];
  return s;
}

// 月日
String monthDay()
{
  String s = String(month());
  s = s + "月" + day() + "日";
  return s;
}

/* *****************************************************************
 *  函数
 * *****************************************************************/
bool enter_flag = 1;
template <typename T>
void mySerialPrint(T content) {
    if (enter_flag == 1){
      unsigned long currentTime = millis();
      unsigned long hours = currentTime / 3600000;
      unsigned long mins = (currentTime % 3600000) / 60000;
      unsigned long secs = ((currentTime % 3600000) % 60000) / 1000;
      Serial.print("Current time: ");
      Serial.print(hours);
      Serial.print(":");
      Serial.print(mins);
      Serial.print(":");
      Serial.print(secs);
      Serial.print("------>");
      enter_flag = 0;
    }
    Serial.print(content);
}

template <typename T>
void mySerialPrint(T content, int num) {
    if (enter_flag == 1){
      unsigned long currentTime = millis();
      unsigned long hours = currentTime / 3600000;
      unsigned long mins = (currentTime % 3600000) / 60000;
      unsigned long secs = ((currentTime % 3600000) % 60000) / 1000;
      Serial.print("Current time: ");
      Serial.print(hours);
      Serial.print(":");
      Serial.print(mins);
      Serial.print(":");
      Serial.print(secs);
      Serial.print("------>");
      enter_flag = 0;
    }
    Serial.print(content, num);
}

template <typename T>
void mySerialPrintln(T content) {
    mySerialPrint(content);
    Serial.println();
    enter_flag = 1;
}

void mySerialPrintln() {
    Serial.println();
    enter_flag = 1;
}


// wifi ssid，psw保存到eeprom
void savewificonfig()
{
  // Keep the legacy 32 + 64 byte EEPROM layout while using terminated RAM
  // buffers. This preserves settings written by earlier firmware versions.
  for (size_t i = 0; i < STORED_SSID_BYTES; i++)
  {
    EEPROM.write(wifi_addr + i, i < strlen(wificonf.stassid) ? wificonf.stassid[i] : 0);
  }
  for (size_t i = 0; i < STORED_PSK_BYTES; i++)
  {
    EEPROM.write(wifi_addr + STORED_SSID_BYTES + i,
                 i < strlen(wificonf.stapsw) ? wificonf.stapsw[i] : 0);
  }
  const uint16_t crc = wifiConfigCrcFromEeprom();
  EEPROM.write(WifiMagic_addr, WIFI_CONFIG_MAGIC);
  EEPROM.write(WifiVersion_addr, WIFI_CONFIG_VERSION);
  EEPROM.write(WifiCrc_addr, crc & 0xFF);
  EEPROM.write(WifiCrc_addr + 1, crc >> 8);
  EEPROM.commit();
}

// TFT屏幕输出函数
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (y >= tft.height())
    return 0;
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

// 进度条函数
byte loadNum = 6;
void loading(byte delayTime) // 绘制进度条
{
  clk.setColorDepth(8);

  clk.createSprite(200, 100); // 创建窗口
  clk.fillSprite(0x0000);     // 填充率

  clk.drawRoundRect(0, 0, 200, 16, 8, 0xFFFF);     // 空心圆角矩形
  clk.fillRoundRect(3, 3, loadNum, 10, 5, 0xFFFF); // 实心圆角矩形
  clk.setTextDatum(CC_DATUM);                      // 设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("Connecting to WiFi......", 100, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawRightString(Version, 180, 60, 2);
  clk.pushSprite(20, 120); // 窗口位置

  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, 0x0000);
  // clk.pushSprite(130,180);

  clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

// 湿度图标显示函数
void humidityWin()
{
  clk.setColorDepth(8);

  uint8_t barWidth = constrain(huminum, 0, 100) / 2; // 0-100 -> 0-50px
  clk.createSprite(52, 6);                         // 创建窗口
  clk.fillSprite(0x0000);                          // 填充率
  clk.drawRoundRect(0, 0, 52, 6, 3, 0xFFFF);       // 空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1, 1, barWidth, 4, 2, humicol); // 实心圆角矩形
  clk.pushSprite(45, 222);                         // 窗口位置
  clk.deleteSprite();
}

// 温度图标显示函数
void tempWin()
{
  clk.setColorDepth(8);

  clk.createSprite(52, 6);                         // 创建窗口
  clk.fillSprite(0x0000);                          // 填充率
  clk.drawRoundRect(0, 0, 52, 6, 3, 0xFFFF);       // 空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1, 1, constrain(tempnum, 0, 50), 4, 2, tempcol); // 实心圆角矩形
  clk.pushSprite(45, 192);                         // 窗口位置
  clk.deleteSprite();
}

#if DHT_EN
// 外接DHT11传感器，显示数据
void IndoorTem()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h))
  {
    mySerialPrintln("DHT11 read failed; retaining previous values");
    return;
  }
  String s = "内温";
  /***绘制相关文字***/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  // 位置
  clk.createSprite(58, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(s, 29, 16);
  clk.pushSprite(172, 150);
  clk.deleteSprite();

  // 温度
  clk.createSprite(60, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawFloat(t, 1, 20, 13);
  //  clk.drawString(sk["temp"].as<String>()+"℃",28,13);
  clk.drawString("℃", 50, 13);
  clk.pushSprite(170, 184);
  clk.deleteSprite();

  // 湿度
  clk.createSprite(60, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  //  clk.drawString(sk["SD"].as<String>(),28,13);
  clk.drawFloat(h, 1, 20, 13);
  clk.drawString("%", 50, 13);
  // clk.drawString("100%",28,13);
  clk.pushSprite(170, 214);
  clk.deleteSprite();
}
#endif

#if !WM_EN
// 微信配网函数
void SmartConfig(void)
{
  WiFi.mode(WIFI_STA); // 设置STA模式
  tft.fillScreen(TFT_BLACK);
  mySerialPrintln("\r\nWait for Smartconfig..."); // 打印log信息
  WiFi.beginSmartConfig();                       // 开始SmartConfig，等待手机端发出用户名和密码
  const unsigned long smartConfigStartedAt = millis();
  while (millis() - smartConfigStartedAt < CONFIG_PORTAL_TIMEOUT_SECONDS * 1000UL)
  {
    mySerialPrint(".");
    delay(100);                 // wait for a second
    if (WiFi.smartConfigDone()) // 配网成功，接收到SSID和密码
    {
      mySerialPrintln("SmartConfig Success");
      Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
      break;
    }
  }
  if (!WiFi.smartConfigDone())
    mySerialPrintln("SmartConfig timed out; continuing with cached display");
  WiFi.stopSmartConfig();
  loadNum = 194;
}
#endif

String SMOD = ""; // 0亮度

bool parseStrictInt(const String &text, int &value)
{
  String normalized = text;
  normalized.trim();
  if (normalized.length() == 0)
    return false;

  char *end = nullptr;
  const long parsed = strtol(normalized.c_str(), &end, 10);
  if (end == normalized.c_str() || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
    return false;
  value = static_cast<int>(parsed);
  return true;
}

void applyBacklight(int brightness)
{
  LCD_BL_PWM = constrain(brightness, 0, 100);
  analogWrite(LCD_BL_PIN, sdd::brightnessToPwm(LCD_BL_PWM));
}

void printDeviceStatus()
{
  mySerialPrintln("--- SmallDesktopDisplay status ---");
  mySerialPrint("Version: ");
  mySerialPrintln(Version);
  mySerialPrint("Uptime ms: ");
  mySerialPrintln(millis());
  mySerialPrint("Free heap: ");
  mySerialPrintln(ESP.getFreeHeap());
  mySerialPrint("WiFi: ");
  mySerialPrintln(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  mySerialPrint("City code: ");
  mySerialPrintln(cityCode);
  mySerialPrint("Weather interval: ");
  mySerialPrint(weatherUpdateIntervalMinutes);
  mySerialPrintln(" min");
  mySerialPrint("Brightness / rotation: ");
  mySerialPrint(LCD_BL_PWM);
  mySerialPrint(" / ");
  mySerialPrintln(LCD_Rotation);
  mySerialPrint("TianAPI key: ");
  mySerialPrintln(TD_key.length() == 32 ? "configured" : "not configured");
}

bool isValidTianApiKey(const String &key)
{
  if (key.length() != 32)
    return false;
  for (size_t i = 0; i < key.length(); i++)
  {
    if (!isAlphaNumeric(key[i]))
      return false;
  }
  return true;
}

// 串口调试设置函数
void Serial_set()
{
  String incomingByte = "";
  if (Serial.available() > 0)
  {
    while (Serial.available() > 0) // 监测串口缓存，当有数据输入时，循环赋值给incomingByte
    {
      incomingByte += char(Serial.read()); // 读取单个字符值，转换为字符，并按顺序一个个赋值给incomingByte
      delay(2);                            // 不能省略，因为读取缓冲区数据需要时间
    }

    // 去除首尾空白与回车换行，避免比较失败
    incomingByte.trim();
    if (incomingByte.length() == 0) return;

    if (SMOD.length() == 0 && incomingByte == "0x00")
    {
      printDeviceStatus();
      return;
    }
    if (SMOD.length() == 0 && incomingByte == "0x08")
    {
      mySerialPrintln("Weather refresh scheduled");
      openWifi();
      return;
    }

    // 支持一次性命令和参数，例如: "0x01 80" 或 "0x01=80"
    int sep = incomingByte.indexOf(' ');
    if (sep < 0) sep = incomingByte.indexOf('=');
    if (sep < 0) sep = incomingByte.indexOf(':');
    if (sep >= 0)
    {
      String cmd = incomingByte.substring(0, sep);
      String arg = incomingByte.substring(sep + 1);
      cmd.trim();
      arg.trim();
      // 直接处理常见一次性命令，避免交互两步
      if (cmd == "0x01") // 亮度
      {
        int LCDBL = 0;
        if (parseStrictInt(arg, LCDBL) && sdd::isValidBrightness(LCDBL))
        {
          EEPROM.write(BL_addr, LCDBL);
          EEPROM.commit();
          applyBacklight(LCDBL);
          mySerialPrintln("亮度调整为：");
          mySerialPrintln(LCD_BL_PWM);
        }
        else mySerialPrintln("亮度调整错误，请输入0-100");
        return;
      }
      else if (cmd == "0x02") // 城市代码一次性设置
      {
        int CityC = 0;
        if (parseStrictInt(arg, CityC) && sdd::isValidCityCode(CityC))
        {
          int storedCity = CityC;
          for (int cnum = 0; cnum < 5; cnum++)
            EEPROM.write(CC_addr + cnum, storedCity % 100), storedCity = storedCity / 100;
          EEPROM.commit();
          mySerialPrintln("城市代码已设置");
          if (CityC == 0)
            getCityCode();
          else
          {
            cityCode = String(CityC);
            if (WiFi.status() == WL_CONNECTED)
              getCityWeather();
          }
        }
        else mySerialPrintln("城市调整错误，请输入9位城市代码，自动获取请输入0");
        return;
      }
      else if (cmd == "0x03")
      {
        int rotation = 0;
        if (parseStrictInt(arg, rotation) && sdd::isValidRotation(rotation))
        {
          LCD_Rotation = rotation;
          EEPROM.write(Ro_addr, rotation);
          EEPROM.commit();
          tft.setRotation(rotation);
          tft.fillScreen(TFT_BLACK);
          Hour_sign = Minute_sign = Second_sign = 60;
          digitalClockDisplay(1);
          reflashBanner();
          mySerialPrintln("Screen orientation updated");
        }
        else mySerialPrintln("Screen orientation must be 0-3");
        return;
      }
      else if (cmd == "0x04")
      {
        int interval = 0;
        if (parseStrictInt(arg, interval) && sdd::isValidWeatherInterval(interval))
        {
          weatherUpdateIntervalMinutes = interval;
          updateWeatherInterval();
          EEPROM.write(WeatherInterval_addr, interval);
          EEPROM.commit();
          mySerialPrintln("Weather interval updated");
        }
        else mySerialPrintln("Weather interval must be 1-60 minutes");
        return;
      }
      else if (cmd == "0x06")
      {
        if (isValidTianApiKey(arg))
        {
          saveTDKeytoEEP(arg);
          readTDKeyfromEEP();
          mySerialPrintln("TianAPI key updated");
        }
        else mySerialPrintln("TianAPI key must contain 32 characters");
        return;
      }
      else if (cmd == "0x07") // 立即更新时间
      {
        getNtpTime();
        reflashTime();
        return;
      }
      // 其他一次性命令仍走后续交互流程
    }

    if (SMOD == "0x01") // 设置1亮度设置
    {
      int LCDBL = 0;
      if (parseStrictInt(incomingByte, LCDBL) && sdd::isValidBrightness(LCDBL))
      {
        EEPROM.write(BL_addr, LCDBL); // 亮度地址写入亮度值
        EEPROM.commit();              // 保存更改的数据
        delay(5);
        LCD_BL_PWM = EEPROM.read(BL_addr);
        delay(5);
        SMOD = "";
        Serial.printf("亮度调整为：");
        applyBacklight(LCD_BL_PWM);
        mySerialPrintln(LCD_BL_PWM);
        mySerialPrintln("");
      }
      else
        mySerialPrintln("亮度调整错误，请输入0-100");
      return;
    }
    if (SMOD == "0x02") // 设置2地址设置
    {
      long CityCODE = 0;
      int CityC = 0;
      if (parseStrictInt(incomingByte, CityC) && sdd::isValidCityCode(CityC))
      {
        for (int cnum = 0; cnum < 5; cnum++)
        {
          EEPROM.write(CC_addr + cnum, CityC % 100); // 城市地址写入城市代码
          CityC = CityC / 100;
        }
        // 一次性提交，减少闪存擦写
        EEPROM.commit();
        delay(5);
        for (int cnum = 5; cnum > 0; cnum--)
        {
          CityCODE = CityCODE * 100;
          CityCODE += EEPROM.read(CC_addr + cnum - 1);
        }

        cityCode = CityCODE;

        if (cityCode == "0")
        {
          mySerialPrintln("城市代码调整为：自动");
          getCityCode(); // 获取城市代码

        }
        Serial.printf("城市代码调整为：");
        mySerialPrintln(cityCode);
        mySerialPrintln("");
        getCityWeather(); // 更新城市天气
        SMOD = "";
      }
      else
        mySerialPrintln("城市调整错误，请输入9位城市代码，自动获取请输入0");
      return;
    }
    if (SMOD == "0x03") // 设置3屏幕显示方向
    {
      int RoSet = 0;
      if (parseStrictInt(incomingByte, RoSet) && sdd::isValidRotation(RoSet))
      {
        EEPROM.write(Ro_addr, RoSet); // 屏幕方向地址写入方向值
        EEPROM.commit();              // 保存更改的数据
        SMOD = "";
        // 设置屏幕方向后重新刷屏并显示
        tft.setRotation(RoSet);
        tft.fillScreen(0x0000);
        Hour_sign = Minute_sign = Second_sign = 60;
        digitalClockDisplay(1);
        reflashBanner();
        TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); // 温度图标
        TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       // 湿度图标

        mySerialPrint("Screen orientation is set to：");
        mySerialPrintln(RoSet);
      }
      else
      {
        mySerialPrintln("Screen orientation value is wrong, please enter a value within 0-3");
      }
      return;
    }
    if (SMOD == "0x04") // 设置天气更新时间
    {
      int wtup = 0;
      if (parseStrictInt(incomingByte, wtup) && sdd::isValidWeatherInterval(wtup))
      {
        weatherUpdateIntervalMinutes = wtup;
        updateWeatherInterval();
        EEPROM.write(WeatherInterval_addr, wtup);
        EEPROM.commit();
        SMOD = "";
        Serial.printf("Weather update time changed to：");
        mySerialPrint(weatherUpdateIntervalMinutes);
        mySerialPrintln("minutes");
      }
      else
        mySerialPrintln("Update too long, please reset (1-60)");
      return;
    }
    if (SMOD == "0x06")
    {
      if (isValidTianApiKey(incomingByte))
      {
        saveTDKeytoEEP(incomingByte);
        SMOD = "";
        mySerialPrintln("TD KEY set successfully");
        readTDKeyfromEEP();
        mySerialPrintln("TD KEY loaded");
        getTD();
      }
      else
      {
        mySerialPrintln("TD KEY setup failure");
      }
      return;
    }

    // 如果之前没有模式，则把当前输入作为命令
    SMOD = incomingByte;
    delay(2);
    // 显示对应提示信息
    if (SMOD == "0x01")
      mySerialPrintln("Please enter the brightness value, range 0-100");
    else if (SMOD == "0x02")
      mySerialPrintln("Please enter 9-digit city code, to get it automatically please enter 0");
    else if (SMOD == "0x03")
    {
      mySerialPrintln("Please enter a value for the screen orientation.");
      mySerialPrintln("0-USB port facing down");
      mySerialPrintln("1-USB connector facing right");
      mySerialPrintln("2-USB ports facing up");
      mySerialPrintln("3-USB port facing left");
    }
    else if (SMOD == "0x04")
    {
      mySerialPrint("Current weather update time:");
      mySerialPrint(weatherUpdateIntervalMinutes);
      mySerialPrintln("minutes");
      mySerialPrintln("Please enter the weather update time (1-60) minutes");
    }
    else if (SMOD == "0x05")
    {
      mySerialPrintln("Reset WiFi settings in ......");
      delay(10);
#if WM_EN
      wm.resetSettings();
#else
      WiFi.disconnect(true);
#endif
      deletewificonfig();
      delay(10);
      mySerialPrintln("Successful WiFi setup");
      SMOD = "";
      ESP.restart();
    }
    else if (SMOD == "0x06")
    {
      mySerialPrintln("Please enter TD_KEY:");
    }
    else if (SMOD == "0x99")
    {
      ESP.restart();
    }
    else if (SMOD == "0x07")
    {
      getNtpTime();
      reflashTime();
      SMOD = "";
    }
    else
    {
      mySerialPrintln("");
      mySerialPrintln("Please enter the code to be modified:");
      mySerialPrintln("Brightness Setting Input 0x01");
      mySerialPrintln("Address setting input 0x02");
      mySerialPrintln("Screen orientation setting input 0x03"); 
      mySerialPrintln("Change weather update time 0x04"); 
      mySerialPrintln("Reset WiFi (it will reboot) 0x05");
      mySerialPrintln("Input TD KEY 0x06");
      mySerialPrintln("Reset Time 0x07");
      mySerialPrintln("Show device status 0x00");
      mySerialPrintln("Refresh weather now 0x08");
      mySerialPrintln("Rebooting the device 0x99");
      mySerialPrintln("");
    }
  }
}

#if WM_EN
// WEB配网LCD显示函数
void Web_win()
{
  clk.setColorDepth(8);

  clk.createSprite(200, 60); // 创建窗口
  clk.fillSprite(0x0000);    // 填充率

  clk.setTextDatum(CC_DATUM); // 设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("WiFi Connect Fail!", 100, 10, 2);
  clk.drawString("SSID:", 45, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawString("SmallDisplay-" + String(ESP.getChipId(), HEX), 135, 40, 2);
  clk.pushSprite(20, 50); // 窗口位置

  clk.deleteSprite();
}

// WEB配网函数
void Webconfig()
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SECONDS);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000UL);

  // add a custom input field
  // int customFieldLength = 40;

  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\");

  // test custom html input type(checkbox)
  //  new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type

  // test custom html(radio)
  // const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  // new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input

  String rotationOptions = "<br/><label for='set_rotation'>显示方向设置</label>";
  const char *rotationLabels[] = {"USB接口朝下", "USB接口朝右", "USB接口朝上", "USB接口朝左"};
  for (int rotation = 0; rotation < 4; rotation++)
  {
    rotationOptions += "<input type='radio' name='set_rotation' value='" + String(rotation) + "'";
    if (rotation == LCD_Rotation)
      rotationOptions += " checked";
    rotationOptions += "> " + String(rotationLabels[rotation]) + "<br>";
  }
  WiFiManagerParameter custom_rot(rotationOptions.c_str()); // custom html input
  char brightnessValue[4];
  char intervalValue[4];
  char cityValue[10];
  snprintf(brightnessValue, sizeof(brightnessValue), "%d", LCD_BL_PWM);
  snprintf(intervalValue, sizeof(intervalValue), "%u", weatherUpdateIntervalMinutes);
  long storedCityCode = 0;
  for (int cnum = 5; cnum > 0; cnum--)
  {
    storedCityCode = storedCityCode * 100 + EEPROM.read(CC_addr + cnum - 1);
  }
  snprintf(cityValue, sizeof(cityValue), "%ld",
           sdd::isValidCityCode(storedCityCode) ? storedCityCode : 0L);
  WiFiManagerParameter custom_bl("LCDBL", "屏幕亮度（0-100）", brightnessValue, 3);
#if DHT_EN
  char dhtEnabledValue[2];
  snprintf(dhtEnabledValue, sizeof(dhtEnabledValue), "%d", DHT_img_flag == 1 ? 1 : 0);
  WiFiManagerParameter custom_DHT11_en("DHT11_en", "Enable DHT11 sensor", dhtEnabledValue, 1);
#endif
  WiFiManagerParameter custom_weatertime("WeatherUpdateTime", "天气刷新时间（分钟）", intervalValue, 3);
  WiFiManagerParameter custom_cc("CityCode", "城市代码", cityValue, 9);
  WiFiManagerParameter p_lineBreak_notext("<p></p>");

  // wm.addParameter(&p_lineBreak_notext);
  // wm.addParameter(&custom_field);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_cc);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_bl);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_weatertime);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_rot);
#if DHT_EN
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_DHT11_en);
#endif
  wm.setSaveParamsCallback(saveParamCallback);

  // custom menu via array or vector
  //
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
  // const char* menu[] = {"wifi","info","param","sep","restart","exit"};
  // wm.setMenu(menu,6);
  std::vector<const char *> menu = {"wifi", "restart"};
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");

  // set static ip
  //  wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
  //  wm.setShowStaticFields(true); // force show static ip fields
  //  wm.setShowDnsFields(true);    // force show dns field always

  // wm.setCaptivePortalEnable(false); // disable captive portal redirection
  // wm.setAPClientCheck(true); // avoid timeout if client connected to softap

  // wifi scan settings
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  wm.setMinimumSignalQuality(20); // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons

  // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  String apName = "SmallDisplay-" + String(ESP.getChipId(), HEX);
  res = wm.autoConnect(apName.c_str());
  //  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if (!res)
  {
    mySerialPrintln("Config portal failed or timed out");
    delay(500);
  }
  else
  {
    mySerialPrintln("Config portal connected");
  }
}

String getParam(String name)
{
  // read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name))
  {
    value = wm.server->arg(name);
  }
  return value;
}

#endif // WM_EN

// 删除原有eeprom中的信息
void deletewificonfig()
{
  memset(&wificonf, 0, sizeof(wificonf));
  for (size_t i = 0; i < STORED_SSID_BYTES + STORED_PSK_BYTES; i++)
  {
    EEPROM.write(wifi_addr + i, 0);
  }
  EEPROM.write(WifiMagic_addr, 0);
  EEPROM.write(WifiVersion_addr, 0);
  EEPROM.write(WifiCrc_addr, 0);
  EEPROM.write(WifiCrc_addr + 1, 0);
  EEPROM.commit();
}

// 从eeprom读取WiFi信息ssid，psw
void readwificonfig()
{
  memset(&wificonf, 0, sizeof(wificonf));
  if (EEPROM.read(WifiMagic_addr) == WIFI_CONFIG_MAGIC)
  {
    const uint16_t storedCrc = EEPROM.read(WifiCrc_addr) |
                               (static_cast<uint16_t>(EEPROM.read(WifiCrc_addr + 1)) << 8);
    if (EEPROM.read(WifiVersion_addr) != WIFI_CONFIG_VERSION ||
        storedCrc != wifiConfigCrcFromEeprom())
    {
      mySerialPrintln("Stored WiFi configuration failed integrity validation");
      deletewificonfig();
      return;
    }
  }
  for (size_t i = 0; i < STORED_SSID_BYTES; i++)
  {
    const uint8_t value = EEPROM.read(wifi_addr + i);
    if (value == 0 || value == 0xFF)
      break;
    wificonf.stassid[i] = static_cast<char>(value);
  }
  for (size_t i = 0; i < STORED_PSK_BYTES; i++)
  {
    const uint8_t value = EEPROM.read(wifi_addr + STORED_SSID_BYTES + i);
    if (value == 0 || value == 0xFF)
      break;
    wificonf.stapsw[i] = static_cast<char>(value);
  }
  mySerialPrint("Stored WiFi SSID: ");
  mySerialPrintln(strlen(wificonf.stassid) ? wificonf.stassid : "<not configured>");
}

void saveTDKeytoEEP(String td_api_key)
{
  size_t keyLen = td_api_key.length();
  for (int cnum = 0; cnum < 32; cnum++)
  {
    char v = (static_cast<size_t>(cnum) < keyLen) ? td_api_key[cnum] : '\0';
    EEPROM.write(td_key_addr + cnum, v);
  }
  // 一次性提交，减少擦写次数
  EEPROM.commit();
  delay(5);
}
void readTDKeyfromEEP()
{
  TD_key = "";
  for (int cnum = 0; cnum < 32; cnum++)
  {
    char v = char(EEPROM.read(td_key_addr + cnum));
    if (v == '\0' || v == char(0xFF))
    {
      break;
    }
    TD_key += v;
  }
}

#if WM_EN
void saveParamCallback()
{
  int CCODE = 0;
  int cc = 0;
  int newRotation = 0;
  int newBrightness = 0;
  int newWeatherInterval = 0;
#if DHT_EN
  int newDhtEnabled = 0;
#endif

  mySerialPrintln("[CALLBACK] saveParamCallback fired");
  // mySerialPrintln("PARAM customfieldid = " + getParam("customfieldid"));
  // mySerialPrintln("PARAM CityCode = " + getParam("CityCode"));
  // mySerialPrintln("PARAM LCD BackLight = " + getParam("LCDBL"));
  // mySerialPrintln("PARAM WeatherUpdateTime = " + getParam("WeatherUpdateTime"));
  // mySerialPrintln("PARAM Rotation = " + getParam("set_rotation"));
  // mySerialPrintln("PARAM DHT11_en = " + getParam("DHT11_en"));
  const bool valid = parseStrictInt(getParam("CityCode"), cc) && sdd::isValidCityCode(cc) &&
                     parseStrictInt(getParam("set_rotation"), newRotation) && sdd::isValidRotation(newRotation) &&
                     parseStrictInt(getParam("LCDBL"), newBrightness) && sdd::isValidBrightness(newBrightness) &&
                     parseStrictInt(getParam("WeatherUpdateTime"), newWeatherInterval) &&
                         sdd::isValidWeatherInterval(newWeatherInterval)
#if DHT_EN
                     && parseStrictInt(getParam("DHT11_en"), newDhtEnabled) &&
                         (newDhtEnabled == 0 || newDhtEnabled == 1)
#endif
      ;
  if (!valid)
  {
    mySerialPrintln("Rejected invalid configuration portal parameters");
    return;
  }

  LCD_Rotation = newRotation;
  LCD_BL_PWM = newBrightness;
  weatherUpdateIntervalMinutes = newWeatherInterval;
  updateWeatherInterval();
#if DHT_EN
  DHT_img_flag = newDhtEnabled;
#endif

  // 对获取的数据进行处理
  // 城市代码
  mySerialPrint("CityCode = ");
  mySerialPrintln(cc);
  if (sdd::isValidCityCode(cc))
  {
    for (int cnum = 0; cnum < 5; cnum++)
    {
      EEPROM.write(CC_addr + cnum, cc % 100); // 城市地址写入城市代码
      cc = cc / 100;
    }
    for (int cnum = 5; cnum > 0; cnum--)
    {
      CCODE = CCODE * 100;
      CCODE += EEPROM.read(CC_addr + cnum - 1);
    }
    cityCode = CCODE;
  }
  // 屏幕方向
  mySerialPrint("LCD_Rotation = ");
  mySerialPrintln(LCD_Rotation);
  if (EEPROM.read(Ro_addr) != LCD_Rotation)
  {
    EEPROM.write(Ro_addr, LCD_Rotation);
    // defer commit until end of this callback
  }
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(0x0000);
  Web_win();
  loadNum--;
  loading(1);
  if (EEPROM.read(BL_addr) != LCD_BL_PWM)
  {
    EEPROM.write(BL_addr, LCD_BL_PWM);
    // defer commit until end of this callback
  }
  // 屏幕亮度
  Serial.printf("The brightness is adjusted to:");
  applyBacklight(LCD_BL_PWM);
  mySerialPrintln(LCD_BL_PWM);
  // 天气更新时间
  Serial.printf("Weather updates are rescheduled:");
  mySerialPrintln(weatherUpdateIntervalMinutes);
  EEPROM.write(WeatherInterval_addr, weatherUpdateIntervalMinutes);

#if DHT_EN
  // 是否使用DHT11传感器
  Serial.printf("DHT11传感器：");
  EEPROM.write(DHT_addr, DHT_img_flag);
  // defer commit until end of this callback
  mySerialPrintln((DHT_img_flag ? "已启用" : "未启用"));
#endif
  // 所有写入在此处一次性提交，减少擦写
  EEPROM.commit();
  delay(5);
}
#endif

// 发送HTTP请求并且将服务器响应通过串口输出
void getCityCode()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    mySerialPrintln("City lookup skipped: WiFi disconnected");
    return;
  }
  String URL = "http://wgeo.weather.com.cn/ip/?_=" + String(now());
  // 创建 HTTPClient 对象
  HTTPClient httpClient;

  // 配置请求地址。此处也可以不使用端口号和PATH而单纯的
  httpClient.begin(wificlient, URL);
  httpClient.setTimeout(WEATHER_HTTP_TIMEOUT_MS);

  // 设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");

  // 启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  mySerialPrint("Send GET request to URL: ");
  mySerialPrintln(URL);

  // 如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK)
  {
    String str = httpClient.getString();

    int aa = str.indexOf("id=");
    if (aa > -1)
    {
      int cityStart = aa + 3;
      while (cityStart < static_cast<int>(str.length()) &&
             (str[cityStart] == '\'' || str[cityStart] == '"'))
        cityStart++;
      const String candidate = str.substring(cityStart, cityStart + 9);
      const uint32_t candidateValue = candidate.toInt();
      if (candidate.length() == 9 && sdd::isValidCityCode(candidateValue) && candidateValue != 0)
      {
        cityCode = candidate;
        mySerialPrintln(cityCode);
        getCityWeather();
      }
      else
      {
        mySerialPrintln("Invalid city code in lookup response");
        cityCode = defcityCode;
      }
    }
    else
    {
      mySerialPrintln("Failed to get city code");
    }
  }
  else
  {
    mySerialPrintln("Request city code error:");
    mySerialPrintln(httpCode);
    cityCode = defcityCode;
  }

  // 关闭ESP8266与服务器连接
  httpClient.end();
}

// 获取城市天气
void getCityWeather()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    mySerialPrintln("Weather refresh skipped: WiFi disconnected");
    return;
  }
  // String URL = "http://d1.weather.com.cn/dingzhi/" + cityCode + ".html?_="+String(now());//新
  String URL = "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_=" + String(now()); // 原来
  // 创建 HTTPClient 对象
  HTTPClient httpClient;

  // httpClient.begin(URL);
  httpClient.begin(wificlient, URL); // 使用新方法
  httpClient.setTimeout(WEATHER_HTTP_TIMEOUT_MS);

  // 设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");

  // 启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  mySerialPrintln("Weather data being acquired");
  // mySerialPrintln(URL);

  // 如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK)
  {

    String str = httpClient.getString();
    int indexStart = str.indexOf("weatherinfo\":");
    int indexEnd = str.indexOf("};var alarmDZ");

    if (indexStart < 0 || indexEnd <= indexStart)
    {
      mySerialPrintln("Weather response missing city markers");
      httpClient.end();
      return;
    }
    String jsonCityDZ = str.substring(indexStart + 13, indexEnd);
    // mySerialPrintln(jsonCityDZ);

    indexStart = str.indexOf("dataSK =");
    indexEnd = str.indexOf(";var dataZS");
    if (indexStart < 0 || indexEnd <= indexStart)
    {
      mySerialPrintln("Weather response missing live-data markers");
      httpClient.end();
      return;
    }
    String jsonDataSK = str.substring(indexStart + 8, indexEnd);
    // mySerialPrintln(jsonDataSK);

    indexStart = str.indexOf("\"f\":[");
    indexEnd = str.indexOf(",{\"fa");
    if (indexStart < 0 || indexEnd <= indexStart)
    {
      mySerialPrintln("Weather response missing forecast markers");
      httpClient.end();
      return;
    }
    String jsonFC = str.substring(indexStart + 5, indexEnd);
    // mySerialPrintln(jsonFC);

    if (weatherData(jsonCityDZ, jsonDataSK, jsonFC))
      mySerialPrintln("Get Success");
    else
      mySerialPrintln("Weather JSON rejected; keeping previous display");
  }
  else
  {
    mySerialPrintln("Request City Weather Error:");
    mySerialPrint(httpCode);
  }

  // 关闭ESP8266与服务器连接
  httpClient.end();
}

String HTTPS_request(String host, String url, String parameter = "", String fingerprint = "", int Port = 443, int Receive_cache = 1024)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    mySerialPrintln("HTTPS request skipped: WiFi disconnected");
    return "0";
  }

  BearSSL::WiFiClientSecure client;
  const String configuredFingerprint = fingerprint.length() ? fingerprint : String(TIANAPI_TLS_FINGERPRINT);
  if (!configuredFingerprint.length())
  {
    mySerialPrintln("TianAPI skipped: TLS fingerprint is not configured");
    return "0";
  }
  client.setFingerprint(configuredFingerprint.c_str());

  client.setBufferSizes(Receive_cache, 512);
  client.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
  if (parameter != "")
    parameter = "?" + parameter;

  const String fullUrl = "https://" + host + (Port == 443 ? "" : ":" + String(Port)) + url + parameter;
  HTTPClient https;
  https.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
  if (!https.begin(client, fullUrl))
    return "0";
  https.setUserAgent("SmallDesktopDisplay/1.5");
  const int httpCode = https.GET();
  String body = "0";
  if (httpCode == HTTP_CODE_OK)
    body = https.getString();
  else
  {
    mySerialPrint("HTTPS request failed, code=");
    mySerialPrintln(httpCode);
  }
  https.end();
  return body;
}

String TD_gregoriandate;
String TD_gregoriandate_year;
String TD_gregoriandate_month;
String TD_gregoriandate_day;
String TD_lunardate;
String TD_lunardate_year;
String TD_lunardate_month;
String TD_lunardate_day;
String TD_year;
String TD_month;
String TD_day;
String TD_animal;
String TD_lubarmonth;
String TD_lunarday;
String TD_zodiac[12] = {"鼠", "牛", "虎", "兔", "龙", "蛇",
                        "马", "羊", "猴", "鸡", "狗", "猪"};
String TD_Earthly_Branches[12] = {"子", "丑", "寅", "卯", "辰", "巳",
                                  "午", "未", "申", "酉", "戌", "亥"};
String TD_jieqi = "";
// apis.tianapi.com 限流策略:
// Calls are made by the network refresh cycle. This guard prevents duplicate
// requests within one cycle and applies a bounded backoff after failures.
unsigned long td_next_attempt_ms = 0;
uint8_t td_consecutive_failures = 0;
const unsigned long TD_SUCCESS_RETRY_MS = 30UL * 60UL * 1000UL;
const unsigned long TD_FAIL_RETRY_MS = 1000UL;
const uint8_t TD_FAIL_FAST_RETRY_MAX = 10;

String full_zodiac(const String& zodiac){
  for (int i = 0; i < 12; ++i){
    if (zodiac == TD_zodiac[i]){
      return TD_Earthly_Branches[i] + zodiac;
    }
  }
  return zodiac;
}
void splitDate(const String& date, String& year, String& month, String& day) {
    int firstDash = date.indexOf('-');
    int secondDash = date.lastIndexOf('-');
    if (firstDash <= 0 || secondDash <= firstDash + 1 || secondDash >= static_cast<int>(date.length()) - 1)
    {
      year = month = day = "";
      return;
    }
    year = date.substring(0, firstDash);
    month = date.substring(firstDash + 1, secondDash);
    day = date.substring(secondDash + 1);
}
void getTD()
{
  if (!isValidTianApiKey(TD_key))
  {
    return;
  }
  unsigned long nowMs = millis();
  if ((long)(nowMs - td_next_attempt_ms) < 0)
  {
    return;
  }

  // String URL = "https://apis.tianapi.com/lunar/index?key=" + TD_key;
  String str = HTTPS_request("apis.tianapi.com", "/lunar/index", "key=" + TD_key);
  mySerialPrintln("Obtaining Heavenly Stems and Earthly Branches information");
  // 如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (str != "0" && str.length() != 0)
  {
    DynamicJsonDocument doc(2048);
    const DeserializationError jsonError = deserializeJson(doc, str);
    if (jsonError)
    {
      mySerialPrint("Invalid TianAPI JSON: ");
      mySerialPrintln(jsonError.c_str());
      td_next_attempt_ms = millis() + 60UL * 1000UL;
      return;
    }
    JsonObject sk = doc.as<JsonObject>();
    int tdCode = sk["code"] | -1;
    if (tdCode != 200 || sk["result"].isNull())
    {
      mySerialPrint("Request for Heavenly Stem and Earthly Branch Errors, code=");
      mySerialPrintln(tdCode);
      if (td_consecutive_failures < 255)
        td_consecutive_failures++;

      if (td_consecutive_failures >= TD_FAIL_FAST_RETRY_MAX)
      {
        td_consecutive_failures = 0;
        td_next_attempt_ms = millis() + TD_SUCCESS_RETRY_MS;
        mySerialPrintln("TianAPI failed 10 times continuously, waiting 30 minutes before retry");
      }
      else
      {
        td_next_attempt_ms = millis() + TD_FAIL_RETRY_MS;
      }
      return;
    }

    TD_gregoriandate = sk["result"]["gregoriandate"].as<String>();
    splitDate(TD_gregoriandate, TD_gregoriandate_year, TD_gregoriandate_month, TD_gregoriandate_day);
    TD_lunardate = sk["result"]["lunardate"].as<String>();
    splitDate(TD_lunardate, TD_lunardate_year, TD_lunardate_month, TD_lunardate_day);
    TD_year = sk["result"]["tiangandizhiyear"].as<String>();
    TD_month = sk["result"]["tiangandizhimonth"].as<String>();
    TD_day = sk["result"]["tiangandizhiday"].as<String>();
    TD_animal = sk["result"]["shengxiao"].as<String>();
    TD_animal = full_zodiac(TD_animal);
    TD_lubarmonth = sk["result"]["lubarmonth"].as<String>();
    TD_lunarday = sk["result"]["lunarday"].as<String>();
    TD_jieqi = sk["result"]["jieqi"].as<String>();
    mySerialPrintln("Get Success");
    td_consecutive_failures = 0;
    td_next_attempt_ms = millis() + TD_SUCCESS_RETRY_MS;
  }
  else
  {
    mySerialPrintln("Request for Heavenly Stem and Earthly Branch Errors");
    if (td_consecutive_failures < 255)
      td_consecutive_failures++;

    if (td_consecutive_failures >= TD_FAIL_FAST_RETRY_MAX)
    {
      td_consecutive_failures = 0;
      td_next_attempt_ms = millis() + TD_SUCCESS_RETRY_MS;
      mySerialPrintln("TianAPI failed 10 times continuously, waiting 30 minutes before retry");
    }
    else
    {
      td_next_attempt_ms = millis() + TD_FAIL_RETRY_MS;
    }
  }
}

String scrollText[7];
// int scrollTextWidth = 0;
String strTDDate[5];

// 天气信息写到屏幕上
bool weatherData(const String &cityDZ, const String &dataSK, const String &dataFC)
{
  // Parse every section before drawing, so a partial upstream response cannot
  // replace a previously valid screen with empty/zero fields.
  DynamicJsonDocument liveDoc(1536);
  DynamicJsonDocument cityDoc(1024);
  DynamicJsonDocument forecastDoc(512);
  if (deserializeJson(liveDoc, dataSK) || deserializeJson(cityDoc, cityDZ) ||
      deserializeJson(forecastDoc, dataFC))
    return false;

  JsonObject sk = liveDoc.as<JsonObject>();
  JsonObject dz = cityDoc.as<JsonObject>();
  JsonObject fc = forecastDoc.as<JsonObject>();
  if (!sk.containsKey("temp") || !sk.containsKey("SD") ||
      !sk.containsKey("cityname") || !sk.containsKey("weathercode") ||
      !dz.containsKey("weather") || !fc.containsKey("fd") || !fc.containsKey("fc"))
    return false;

  // TFT_eSprite clkb = TFT_eSprite(&tft);

  /***绘制相关文字***/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  // 温度
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["temp"].as<String>() + "℃", 28, 13);
  clk.pushSprite(100, 184);
  clk.deleteSprite();
  const int temperatureCelsius = sk["temp"].as<int>();
  tempnum = sdd::temperatureBarWidth(temperatureCelsius);
  if (tempnum < 10)
    tempcol = 0x00FF;
  else if (tempnum < 28)
    tempcol = 0x0AFF;
  else if (tempnum < 34)
    tempcol = 0x0F0F;
  else if (tempnum < 41)
    tempcol = 0xFF0F;
  else if (tempnum < 49)
    tempcol = 0xF00F;
  else
  {
    tempcol = 0xF00F;
    tempnum = 50;
  }
  tempWin();

  // 湿度
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["SD"].as<String>(), 28, 13);
  // clk.drawString("100%",28,13);
  clk.pushSprite(100, 214);
  clk.deleteSprite();
  // String A = sk["SD"].as<String>();
  huminum = sk["SD"].as<String>().toInt();
  huminum = constrain(huminum, 0, 100);

  if (huminum > 90)
    humicol = 0x00FF;
  else if (huminum > 70)
    humicol = 0x0AFF;
  else if (huminum > 40)
    humicol = 0x0F0F;
  else if (huminum > 20)
    humicol = 0xFF0F;
  else
    humicol = 0xF00F;
  humidityWin();

  // 城市名称
  clk.createSprite(70, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["cityname"].as<String>(), 44, 16);
  clk.pushSprite(5, 15);
  clk.deleteSprite();

  // PM2.5空气指数
  uint16_t pm25BgColor = tft.color565(80, 80, 80);
  String aqiTxt = "未知";
  int pm25V = sk["aqi"].isNull() ? -1 : sk["aqi"].as<int>();
  const sdd::AqiLevel aqiLevel = sdd::classifyAqi(pm25V);
  const bool hasValidAqi = aqiLevel != sdd::AqiLevel::Unknown;
  switch (aqiLevel)
  {
    case sdd::AqiLevel::Excellent:
      pm25BgColor = tft.color565(156, 202, 127); aqiTxt = "优"; break;
    case sdd::AqiLevel::Good:
      pm25BgColor = tft.color565(247, 219, 100); aqiTxt = "良"; break;
    case sdd::AqiLevel::Light:
      pm25BgColor = tft.color565(242, 159, 57); aqiTxt = "轻度"; break;
    case sdd::AqiLevel::Moderate:
      pm25BgColor = tft.color565(186, 55, 121); aqiTxt = "中度"; break;
    case sdd::AqiLevel::Heavy:
      pm25BgColor = tft.color565(136, 11, 32); aqiTxt = "重度"; break;
    case sdd::AqiLevel::Severe:
      pm25BgColor = tft.color565(88, 6, 20); aqiTxt = "严重"; break;
    case sdd::AqiLevel::Unknown:
      mySerialPrintln("AQI missing in response, using placeholder"); break;
  }
  if (hasValidAqi)
  {
    aqiTxt = aqiTxt + " " + String(int(pm25V));
  }
  clk.createSprite(85, 24);
  clk.fillSprite(bgColor);
  clk.fillRoundRect(0, 0, 85, 24, 4, pm25BgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(0x0000);
  clk.drawString(aqiTxt, 40, 13);
  clk.pushSprite(80, 18);
  clk.deleteSprite();

  scrollText[0] = "实时天气 " + sk["weather"].as<String>();
  scrollText[1] = "AQI " + aqiTxt;
  scrollText[2] = "风向 " + sk["WD"].as<String>() + sk["WS"].as<String>();

  // scrollText[6] = atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()) ;

  // 天气图标
  String weatherCode = sk["weathercode"].as<String>();
  if (weatherCode.length() > 0 && !isDigit(weatherCode[0]))
    weatherCode.remove(0, 1);
  wrat.draw(170, 15, weatherCode.toInt());

  // 左上角滚动字幕
  // 解析第二段JSON
  // mySerialPrintln(sk["ws"].as<String>());
  // 横向滚动方式
  // String aa = "今日天气:" + dz["weather"].as<String>() + "，温度:最低" + dz["tempn"].as<String>() + "，最高" + dz["temp"].as<String>() + " 空气质量:" + aqiTxt + "，风向:" + dz["wd"].as<String>() + dz["ws"].as<String>();
  // scrollTextWidth = clk.textWidth(scrollText);
  // mySerialPrintln(aa);
  scrollText[3] = "今日" + dz["weather"].as<String>();

  scrollText[4] = "最低温度" + fc["fd"].as<String>() + "℃";
  scrollText[5] = "最高温度" + fc["fc"].as<String>() + "℃";

  // mySerialPrintln(scrollText[0]);

  clk.unloadFont();
  return true;
}

int currentIndex = 0;
TFT_eSprite clkb = TFT_eSprite(&tft);

void scrollBanner()
{
  // if(millis() - prevTime > 2333) //3秒切换一次
  //  if(second()%2 ==0&& prevTime == 0)
  //  {
  if (scrollText[currentIndex])
  {
    clkb.setColorDepth(8);
    clkb.loadFont(ZdyLwFont_20);
    clkb.createSprite(150, 30);
    clkb.fillSprite(bgColor);
    clkb.setTextWrap(false);
    clkb.setTextDatum(CC_DATUM);
    clkb.setTextColor(TFT_WHITE, bgColor);
    clkb.drawString(scrollText[currentIndex], 74, 16);
    clkb.pushSprite(5, 45);

    clkb.deleteSprite();
    clkb.unloadFont();

    if (currentIndex >= 5)
      currentIndex = 0; // 回第一个
    else
      currentIndex += 1; // 准备切换到下一个
  }
  prevTime = 1;
  //  }
}

// 用快速线方法绘制数字
void drawLineFont(uint32_t _x, uint32_t _y, uint32_t _num, uint32_t _size, uint32_t _color)
{
  uint32_t fontSize;
  const LineAtom *fontOne;
  // 小号(9*14)
  if (_size == 1)
  {
    fontOne = smallLineFont[_num];
    fontSize = smallLineFont_size[_num];
    // 绘制前清理字体绘制区域
    tft.fillRect(_x, _y, 9, 14, TFT_BLACK);
  }
  // 中号(18*30)
  else if (_size == 2)
  {
    fontOne = middleLineFont[_num];
    fontSize = middleLineFont_size[_num];
    // 绘制前清理字体绘制区域
    tft.fillRect(_x, _y, 18, 30, TFT_BLACK);
  }
  // 大号(36*90)
  else if (_size == 3)
  {
    fontOne = largeLineFont[_num];
    fontSize = largeLineFont_size[_num];
    // 绘制前清理字体绘制区域
    tft.fillRect(_x, _y, 36, 90, TFT_BLACK);
  }
  else
    return;

  for (uint32_t i = 0; i < fontSize; i++)
  {
    tft.drawFastHLine(fontOne[i].xValue + _x, fontOne[i].yValue + _y, fontOne[i].lValue, _color);
  }
}

int Hour_sign = 60;
int Minute_sign = 60;
int Second_sign = 60;
// 日期刷新
void digitalClockDisplay(int reflash_en = 0)
{
  // 时钟刷新,输入1强制刷新
  int now_hour = hour();     // 获取小时
  int now_minute = minute(); // 获取分钟
  int now_second = second(); // 获取秒针

  // 小时刷新
  if ((now_hour != Hour_sign) || (reflash_en == 1))
  {
    drawLineFont(20, timeY, now_hour / 10, 3, SD_FONT_WHITE);
    drawLineFont(60, timeY, now_hour % 10, 3, SD_FONT_WHITE);
    Hour_sign = now_hour;
    if (Wifi_en == 1 && WiFi.status() == WL_CONNECTED)
      getTD();
  }
  // 分钟刷新
  if ((now_minute != Minute_sign) || (reflash_en == 1))
  {
    drawLineFont(101, timeY, now_minute / 10, 3, SD_FONT_YELLOW);
    drawLineFont(141, timeY, now_minute % 10, 3, SD_FONT_YELLOW);
    Minute_sign = now_minute;
    // mySerialPrintln(String(now_hour) + ' ' + String(now_minute) + ' ' + String(now_second));
  }
  // 秒针刷新
  if ((now_second != Second_sign) || (reflash_en == 1)) // 分钟刷新
  {
    drawLineFont(182, timeY + 30, now_second / 10, 2, SD_FONT_WHITE);
    drawLineFont(202, timeY + 30, now_second % 10, 2, SD_FONT_WHITE);
    Second_sign = now_second;
  }

  if (reflash_en == 1)
    reflash_en = 0;
  /***日期****/
  strTDDate[0] = timeStatus() == timeSet ? "公历 " + String(year()) + "年" : "时间尚未同步";
  strTDDate[1] = timeStatus() == timeSet ? String(monthDay()) + " " + String(week()) : "等待网络同步";
  strTDDate[2] = TD_lunardate_year.length() ? "农历 " + TD_lunardate_year + "年 " + TD_animal : "农历未配置";
  strTDDate[3] = TD_lubarmonth.length() ? TD_lubarmonth + " " + TD_lunarday + " " + TD_jieqi : "";
  strTDDate[4] = TD_year.length() ? TD_year + " " + TD_month + " " + TD_day : "";
  /***日期****/
}

int currentTDIndex = 0;
void TDBanner()
{
  if (strTDDate[currentTDIndex])
  {
    clk.setColorDepth(8);
    clk.loadFont(font_td_20);
    clk.createSprite(150, 30);
    clk.fillSprite(bgColor);
    clk.setTextWrap(false);
    clk.setTextDatum(CC_DATUM);
    clk.setTextColor(TFT_WHITE, bgColor);
    clk.drawString(strTDDate[currentTDIndex], 74, 16);
    clk.pushSprite(5, 150);

    clk.deleteSprite();
    clk.unloadFont();

    if (currentTDIndex >= 4)
      currentTDIndex = 0; // 回第一个
    else
      currentTDIndex += 1; // 准备切换到下一个
  }
  prevTime = 1;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;     // NTP时间在消息的前48字节中
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing packets
uint32_t ntpRequestCookie = 0;

time_t getNtpTime()
{
  const bool restoreSleep = Wifi_en == 0;
  auto finish = [restoreSleep](time_t result) -> time_t {
    if (restoreSleep)
    {
      Udp.stop();
      WiFi.forceSleepBegin();
      delay(1);
    }
    return result;
  };

  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.forceSleepWake();
    delay(1);
    WiFi.mode(WIFI_STA);
    if (strlen(wificonf.stassid))
      WiFi.begin(wificonf.stassid, wificonf.stapsw);
    else
      WiFi.begin();
    const unsigned long connectStarted = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - connectStarted < WIFI_CONNECT_TIMEOUT_MS)
      delay(100);
    if (WiFi.status() != WL_CONNECTED)
    {
      mySerialPrintln("NTP aborted: WiFi timeout");
      return finish(0);
    }
  }

  IPAddress ntpServerIP;
  bool resolved = false;
  for (size_t i = 0; i < sizeof(ntpServerNames) / sizeof(ntpServerNames[0]); i++)
  {
    if (WiFi.hostByName(ntpServerNames[i], ntpServerIP))
    {
      resolved = true;
      break;
    }
  }
  if (!resolved)
  {
    mySerialPrintln("NTP aborted: all DNS lookups failed");
    return finish(0);
  }

  Udp.begin(localPort);
  while (Udp.parsePacket() > 0) {}

  const uint8_t maxAttempts = 2;
  for (uint8_t attempt = 1; attempt <= maxAttempts; ++attempt)
  {
    sendNTPpacket(ntpServerIP);
    const uint32_t beginWait = millis();
    while (millis() - beginWait < 2000UL)
    {
      const int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE && Udp.remoteIP() == ntpServerIP && Udp.remotePort() == 123)
      {
        Udp.read(packetBuffer, NTP_PACKET_SIZE);
        const uint8_t leap = packetBuffer[0] >> 6;
        const uint8_t mode = packetBuffer[0] & 0x07;
        const uint8_t stratum = packetBuffer[1];
        const uint32_t originCookie = (static_cast<uint32_t>(packetBuffer[24]) << 24) |
                                      (static_cast<uint32_t>(packetBuffer[25]) << 16) |
                                      (static_cast<uint32_t>(packetBuffer[26]) << 8) |
                                      static_cast<uint32_t>(packetBuffer[27]);
        const uint32_t secsSince1900 = (static_cast<uint32_t>(packetBuffer[40]) << 24) |
                                       (static_cast<uint32_t>(packetBuffer[41]) << 16) |
                                       (static_cast<uint32_t>(packetBuffer[42]) << 8) |
                                       static_cast<uint32_t>(packetBuffer[43]);
        if (leap != 3 && mode == 4 && stratum >= 1 && stratum <= 15 &&
            originCookie == ntpRequestCookie && secsSince1900 >= 3786825600UL)
        {
          const time_t epoch = secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
          setTime(epoch);
          mySerialPrintln("NTP synchronization succeeded");
          return finish(epoch);
        }
        mySerialPrintln("Rejected invalid NTP response");
      }
      delay(20);
    }
  }

  mySerialPrintln("NTP synchronization timed out");
  return finish(0);
}

// 向NTP服务器发送请求
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0x23;       // LI=0, NTPv4, client mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  ntpRequestCookie = ESP.getCycleCount() ^ micros();
  packetBuffer[40] = ntpRequestCookie >> 24;
  packetBuffer[41] = ntpRequestCookie >> 16;
  packetBuffer[42] = ntpRequestCookie >> 8;
  packetBuffer[43] = ntpRequestCookie;
  packetBuffer[44] = ~packetBuffer[40];
  packetBuffer[45] = ~packetBuffer[41];
  packetBuffer[46] = ~packetBuffer[42];
  packetBuffer[47] = ~packetBuffer[43];
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void cycle_brightness(Button2 &btn)
{
  (void)btn;
  static const uint8_t levels[] = {25, 50, 75, 100};
  uint8_t next = levels[0];
  for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++)
  {
    if (LCD_BL_PWM < levels[i])
    {
      next = levels[i];
      break;
    }
  }
  applyBacklight(next);
  EEPROM.write(BL_addr, next);
  EEPROM.commit();
  mySerialPrint("Brightness: ");
  mySerialPrintln(next);
}

void wifi_reset(Button2 &btn)
{
  (void)btn;
#if WM_EN
  wm.resetSettings();
#else
  WiFi.disconnect(true);
#endif
  deletewificonfig();
  delay(10);
  mySerialPrintln("Reset WiFi successfully");
  ESP.restart();
}

// 更新时间
void reflashTime()
{
  digitalClockDisplay();
  prevTime = 0;
}

// 切换天气 or 空气质量
void reflashBanner()
{
#if DHT_EN
  if (DHT_img_flag != 0)
    IndoorTem();
#endif
  scrollBanner();
  TDBanner();
}

// 所有需要联网后更新的方法都放在这里
void WIFI_reflash_All()
{
  if (Wifi_en == 1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      mySerialPrintln("WIFI connected");
      getNtpTime();
      getCityWeather();
      getTD();
      closeWifi();
    }
    else if (wifiWakeStartedAt != 0 && millis() - wifiWakeStartedAt >= WIFI_CONNECT_TIMEOUT_MS)
    {
      mySerialPrintln("WIFI reconnect timed out; keeping cached display");
      closeWifi();
    }
  }
}

// 打开WIFI
void openWifi()
{
  mySerialPrintln("WIFI reset......");
  WiFi.forceSleepWake(); // wifi on
  delay(1);
  WiFi.mode(WIFI_STA);
  if (strlen(wificonf.stassid))
    WiFi.begin(wificonf.stassid, wificonf.stapsw);
  else
    WiFi.begin();
  wifiWakeStartedAt = millis();
  Wifi_en = 1;
  WIFI_reflash_All();
}

void closeWifi()
{
  Udp.stop();
  WiFi.forceSleepBegin(); // Wifi Off
  delay(1);
  mySerialPrintln("WIFI sleep......");
  Wifi_en = 0;
  wifiWakeStartedAt = 0;
}

// 守护线程池
void Supervisor_controller()
{
  if (controller.shouldRun())
  {
    // mySerialPrintln("controller 启动");
    controller.run();
  }
}

void setup()
{
  Button_sw1.setClickHandler(cycle_brightness);
  Button_sw1.setLongClickHandler(wifi_reset);
  Serial.begin(115200);
  EEPROM.begin(1024);
  // WiFi.forceSleepWake();
  // wm.resetSettings();    //在初始化中使wifi重置，需重新配置WiFi
#if DHT_EN
  dht.begin();
  // 从eeprom读取DHT传感器使能标志
  DHT_img_flag = EEPROM.read(DHT_addr) == 1 ? 1 : 0;
#endif
  readTDKeyfromEEP();
  // 从eeprom读取背光亮度设置
  if (sdd::isValidBrightness(EEPROM.read(BL_addr)))
    LCD_BL_PWM = EEPROM.read(BL_addr);
  // 从eeprom读取屏幕方向设置
  if (sdd::isValidRotation(EEPROM.read(Ro_addr)))
    LCD_Rotation = EEPROM.read(Ro_addr);
  if (sdd::isValidWeatherInterval(EEPROM.read(WeatherInterval_addr)))
    weatherUpdateIntervalMinutes = EEPROM.read(WeatherInterval_addr);

  pinMode(LCD_BL_PIN, OUTPUT);
  applyBacklight(LCD_BL_PWM);

  tft.begin();          /* TFT init */
  tft.invertDisplay(1); // 反转所有显示颜色：1反转，0正常
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(0x0000);
  tft.setTextColor(TFT_BLACK, bgColor);

  readwificonfig(); // 读取存储的wifi信息
  mySerialPrint("Connecting to WIFI");
  mySerialPrintln(wificonf.stassid);
  WiFi.begin(wificonf.stassid, wificonf.stapsw);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  while (WiFi.status() != WL_CONNECTED)
  {
    loading(30);

    if (loadNum >= 194)
    {
      // 使能web配网后自动将smartconfig配网失效
      #if WM_EN
            Web_win();
            Webconfig();
      #endif

      #if !WM_EN
            SmartConfig();
      #endif
            break;
    }
  }
  delay(10);
  while (loadNum < 194) // 让动画走完
  {
    loading(1);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    mySerialPrint("SSID:");
    mySerialPrintln(WiFi.SSID().c_str());
    strlcpy(wificonf.stassid, WiFi.SSID().c_str(), sizeof(wificonf.stassid));
    strlcpy(wificonf.stapsw, WiFi.psk().c_str(), sizeof(wificonf.stapsw));
    savewificonfig();
    readwificonfig();
  }

  mySerialPrint("Local IP:");
  mySerialPrintln(WiFi.localIP());
  mySerialPrintln("Starting UDP");
  Udp.begin(localPort);
  mySerialPrintln("Synchronizing time...");
  getNtpTime();

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  long CityCODE = 0;
  for (int cnum = 5; cnum > 0; cnum--)
  {
    CityCODE = CityCODE * 100;
    CityCODE += EEPROM.read(CC_addr + cnum - 1);
    delay(5);
  }
  if (sdd::isValidCityCode(CityCODE) && CityCODE != 0)
    cityCode = CityCODE;
  else
    getCityCode(); // 获取城市代码

  tft.fillScreen(TFT_BLACK); // 清屏

  TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); // 温度图标
  TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       // 湿度图标

  getCityWeather();
#if DHT_EN
  if (DHT_img_flag != 0)
    IndoorTem();
#endif

  // WiFi.forceSleepBegin(); // wifi off

  // mySerialPrintln("WIFI休眠......");
  // Wifi_en = 0;
  closeWifi();
  reflash_time.setInterval(300); // 设置所需间隔 100毫秒
  reflash_time.onRun(reflashTime);

  reflash_Banner.setInterval(2 * TMS); // 设置所需间隔 2秒
  reflash_Banner.onRun(reflashBanner);

  updateWeatherInterval(); // 设置所需间隔 10分钟
  reflash_openWifi.onRun(openWifi);

  reflash_Animate.setInterval(TMS / 10); // 设置帧率
  reflash_Animate.onRun(refresh_AnimatedImage);
  controller.run();
}

const uint8_t *Animate_value; // 指向关键帧的指针
uint32_t Animate_size;        // 指向关键帧大小的指针
void refresh_AnimatedImage()
{
#if Animate_Choice
  if (DHT_img_flag == 0)
  {
    if (millis() - Amimate_reflash_Time > 100) // x ms切换一次
    {
      Amimate_reflash_Time = millis();
      imgAnim(&Animate_value, &Animate_size);
      TJpgDec.drawJpg(160, 160, Animate_value, Animate_size);
      // TJpgDec.drawJpg(160, 160, Animate_value, sizeof(Animate_value));
    }
  }
#endif
}

void loop()
{
  // refresh_AnimatedImage(&TJpgDec); //更新右下角
  Supervisor_controller(); // 守护线程池（包含动画刷新）
  WIFI_reflash_All();      // WIFI应用
  Serial_set();            // 串口响应
  Button_sw1.loop();       // 按钮轮询
}
