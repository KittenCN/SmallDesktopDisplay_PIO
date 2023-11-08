/* *****************************************************************
 *
 * SmallDesktopDisplay
 *    小型桌面显示器
 *
 * 原  作  者：Misaka
 * 修      改：微车游
 * 再次  修改：丘山鹤
 * 讨  论  群：811058758、887171863、720661626
 * 创 建 日 期：2021.07.19
 * 最后更改日期：2022.1.17
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

#include "config.h"                  //配置文件
#include "weatherNum/weatherNum.h"   //天气图库
#include "Animate/Animate.h"         //动画模块
#include "wifiReFlash/wifiReFlash.h" //WIFI功能模块
#include "font/font_td_20.h"              //字体库

#define Version "SDD V1.4.3 MOD"
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

//定义按钮引脚
Button2 Button_sw1 = Button2(4);

/* *****************************************************************
 *  字库、图片库
 * *****************************************************************/
#include "font/ZdyLwFont_20.h"  //字体库
#include "font/timeClockFont.h" //字体库
#include "img/temperature.h"    //温度图标
#include "img/humidity.h"       //湿度图标

//函数声明
void sendNTPpacket(IPAddress &address); //向NTP服务器发送请求
time_t getNtpTime();                    //从NTP获取时间

// void digitalClockDisplay(int reflash_en);
void printDigits(int digits);
String num2str(int digits);
void LCD_reflash();
void savewificonfig();         // wifi ssid，psw保存到eeprom
void readwificonfig();         //从eeprom读取WiFi信息ssid，psw
void deletewificonfig();       //删除原有eeprom中的信息
void getCityCode();            //发送HTTP请求并且将服务器响应通过串口输出
void getCityWeater();          //获取城市天气
void wifi_reset(Button2 &btn); // WIFI重设
void saveParamCallback();
void esp_reset(Button2 &btn);
void scrollBanner();
void weaterData(String *cityDZ, String *dataSK, String *dataFC); //天气信息写到屏幕上
void refresh_AnimatedImage();                                    //更新右下角
void getTD();
void saveTDKeytoEEP(String td_api_key);
void readTDKeyfromEEP();
void openWifi();
void closeWifi();

//创建时间更新函数线程
Thread reflash_time = Thread();
//创建副标题切换线程
Thread reflash_Banner = Thread();
//创建恢复WIFI链接
Thread reflash_openWifi = Thread();
//创建动画绘制线程
Thread reflash_Animate = Thread();

//创建协程池
StaticThreadController<4> controller(&reflash_time, &reflash_Banner, &reflash_openWifi, &reflash_Animate);

//联网后所有需要更新的数据
Thread WIFI_reflash = Thread();

/* *****************************************************************
 *  参数设置
 * *****************************************************************/
struct config_type
{
  char stassid[32]; //定义配网得到的WIFI名长度(最大32字节)
  char stapsw[64];  //定义配网得到的WIFI密码长度(最大64字节)
};
//---------------修改此处""内的信息--------------------
//如开启WEB配网则可不用设置这里的参数，前一个为wifi ssid，后一个为密码
config_type wificonf = {{"WiFi名"}, {"密码"}};

//天气更新时间  X 分钟
unsigned int updateweater_time = 1;

//----------------------------------------------------

// LCD屏幕相关设置
TFT_eSPI tft = TFT_eSPI(); // 引脚请自行配置tft_espi库中的 User_Setup.h文件
TFT_eSprite clk = TFT_eSprite(&tft);
#define LCD_BL_PIN 5 // LCD背光引脚
uint16_t bgColor = 0x0000;

//其余状态标志位
int LCD_Rotation = 0;        // LCD屏幕方向
int LCD_BL_PWM = 50;         //屏幕亮度0-100，默认50
uint8_t Wifi_en = 1;         // WIFI模块启动  1：打开    0：关闭
uint8_t UpdateWeater_en = 0; //更新时间标志位
int prevTime = 0;            //滚动显示更新标志位
int DHT_img_flag = 0;        // DHT传感器使用标志位

// EEPROM参数存储地址位
int BL_addr = 1;    //被写入数据的EEPROM地址编号  1亮度
int Ro_addr = 2;    //被写入数据的EEPROM地址编号  2 旋转方向
int DHT_addr = 3;   // 3 DHT使能标志位
int CC_addr = 10;   //被写入数据的EEPROM地址编号  10城市
int wifi_addr = 30; //被写入数据的EEPROM地址编号  20wifi-ssid-psw
int td_key_addr = 130;

time_t prevDisplay = 0;       //显示时间显示记录
int Amimate_reflash_Time = 0; //更新时间记录
String TD_key = "";           //天地图密钥

/*** Component objects ***/
WeatherNum wrat;

uint32_t targetTime = 0;
String cityCode = "101090609"; //天气城市代码
int tempnum = 0;               //温度百分比
int huminum = 0;               //湿度百分比
int tempcol = 0xffff;          //温度显示颜色
int humicol = 0xffff;          //湿度显示颜色

// NTP服务器参数
static const char ntpServerName[] = "ntp6.aliyun.com";
const int timeZone = 8; //东八区

// wifi连接UDP设置参数
WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
float duty = 0;

//星期
String week()
{
  String wk[7] = {"日", "一", "二", "三", "四", "五", "六"};
  String s = "周" + wk[weekday() - 1];
  return s;
}

//月日
String monthDay()
{
  String s = String(month());
  s = s + "月" + day() + "日";
  return s;
}

/* *****************************************************************
 *  函数
 * *****************************************************************/

// wifi ssid，psw保存到eeprom
void savewificonfig()
{
  //开始写入
  uint8_t *p = (uint8_t *)(&wificonf);
  for (unsigned int i = 0; i < sizeof(wificonf); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //在闪存内模拟写入
  }
  delay(10);
  EEPROM.commit(); //执行写入ROM
  delay(10);
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

//进度条函数
byte loadNum = 6;
void loading(byte delayTime) //绘制进度条
{
  clk.setColorDepth(8);

  clk.createSprite(200, 100); //创建窗口
  clk.fillSprite(0x0000);     //填充率

  clk.drawRoundRect(0, 0, 200, 16, 8, 0xFFFF);     //空心圆角矩形
  clk.fillRoundRect(3, 3, loadNum, 10, 5, 0xFFFF); //实心圆角矩形
  clk.setTextDatum(CC_DATUM);                      //设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("Connecting to WiFi......", 100, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawRightString(Version, 180, 60, 2);
  clk.pushSprite(20, 120); //窗口位置

  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, 0x0000);
  // clk.pushSprite(130,180);

  clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

//湿度图标显示函数
void humidityWin()
{
  clk.setColorDepth(8);

  huminum = huminum / 2;
  clk.createSprite(52, 6);                         //创建窗口
  clk.fillSprite(0x0000);                          //填充率
  clk.drawRoundRect(0, 0, 52, 6, 3, 0xFFFF);       //空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1, 1, huminum, 4, 2, humicol); //实心圆角矩形
  clk.pushSprite(45, 222);                         //窗口位置
  clk.deleteSprite();
}

//温度图标显示函数
void tempWin()
{
  clk.setColorDepth(8);

  clk.createSprite(52, 6);                         //创建窗口
  clk.fillSprite(0x0000);                          //填充率
  clk.drawRoundRect(0, 0, 52, 6, 3, 0xFFFF);       //空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1, 1, tempnum, 4, 2, tempcol); //实心圆角矩形
  clk.pushSprite(45, 192);                         //窗口位置
  clk.deleteSprite();
}

#if DHT_EN
//外接DHT11传感器，显示数据
void IndoorTem()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  String s = "内温";
  /***绘制相关文字***/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  //位置
  clk.createSprite(58, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(s, 29, 16);
  clk.pushSprite(172, 150);
  clk.deleteSprite();

  //温度
  clk.createSprite(60, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawFloat(t, 1, 20, 13);
  //  clk.drawString(sk["temp"].as<String>()+"℃",28,13);
  clk.drawString("℃", 50, 13);
  clk.pushSprite(170, 184);
  clk.deleteSprite();

  //湿度
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
//微信配网函数
void SmartConfig(void)
{
  WiFi.mode(WIFI_STA); //设置STA模式
  // tft.pushImage(0, 0, 240, 240, qr);
  tft.pushImage(0, 0, 240, 240, qr);
  Serial.println("\r\nWait for Smartconfig..."); //打印log信息
  WiFi.beginSmartConfig();                       //开始SmartConfig，等待手机端发出用户名和密码
  while (1)
  {
    Serial.print(".");
    delay(100);                 // wait for a second
    if (WiFi.smartConfigDone()) //配网成功，接收到SSID和密码
    {
      Serial.println("SmartConfig Success");
      Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
      Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
      break;
    }
  }
  loadNum = 194;
}
#endif

String SMOD = ""; // 0亮度
//串口调试设置函数
void Serial_set()
{
  String incomingByte = "";
  if (Serial.available() > 0)
  {
    while (Serial.available() > 0) //监测串口缓存，当有数据输入时，循环赋值给incomingByte
    {
      incomingByte += char(Serial.read()); //读取单个字符值，转换为字符，并按顺序一个个赋值给incomingByte
      delay(2);                            //不能省略，因为读取缓冲区数据需要时间
    }
    if (SMOD == "0x01") //设置1亮度设置
    {
      int LCDBL = atoi(incomingByte.c_str()); // int n = atoi(xxx.c_str());//String转int
      if (LCDBL >= 0 && LCDBL <= 100)
      {
        EEPROM.write(BL_addr, LCDBL); //亮度地址写入亮度值
        EEPROM.commit();              //保存更改的数据
        delay(5);
        LCD_BL_PWM = EEPROM.read(BL_addr);
        delay(5);
        SMOD = "";
        Serial.printf("亮度调整为：");
        analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM * 10));
        Serial.println(LCD_BL_PWM);
        Serial.println("");
      }
      else
        Serial.println("亮度调整错误，请输入0-100");
    }
    if (SMOD == "0x02") //设置2地址设置
    {
      int CityCODE = 0;
      int CityC = atoi(incomingByte.c_str()); // int n = atoi(xxx.c_str());//String转int
      if (((CityC >= 101000000) && (CityC <= 102000000)) || (CityC == 0))
      {
        for (int cnum = 0; cnum < 5; cnum++)
        {
          EEPROM.write(CC_addr + cnum, CityC % 100); //城市地址写入城市代码
          EEPROM.commit();                           //保存更改的数据
          CityC = CityC / 100;
          delay(5);
        }
        for (int cnum = 5; cnum > 0; cnum--)
        {
          CityCODE = CityCODE * 100;
          CityCODE += EEPROM.read(CC_addr + cnum - 1);
          delay(5);
        }

        cityCode = CityCODE;

        if (cityCode == "0")
        {
          Serial.println("城市代码调整为：自动");
          getCityCode(); //获取城市代码
        }
        else
        {
          Serial.printf("城市代码调整为：");
          Serial.println(cityCode);
        }
        Serial.println("");
        getCityWeater(); //更新城市天气
        SMOD = "";
      }
      else
        Serial.println("城市调整错误，请输入9位城市代码，自动获取请输入0");
    }
    if (SMOD == "0x03") //设置3屏幕显示方向
    {
      int RoSet = atoi(incomingByte.c_str());
      if (RoSet >= 0 && RoSet <= 3)
      {
        EEPROM.write(Ro_addr, RoSet); //屏幕方向地址写入方向值
        EEPROM.commit();              //保存更改的数据
        SMOD = "";
        //设置屏幕方向后重新刷屏并显示
        tft.setRotation(RoSet);
        tft.fillScreen(0x0000);
        LCD_reflash(); //屏幕刷新程序
        UpdateWeater_en = 1;
        TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); //温度图标
        TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       //湿度图标

        Serial.print("屏幕方向设置为：");
        Serial.println(RoSet);
      }
      else
      {
        Serial.println("屏幕方向值错误，请输入0-3内的值");
      }
    }
    if (SMOD == "0x04") //设置天气更新时间
    {
      int wtup = atoi(incomingByte.c_str()); // int n = atoi(xxx.c_str());//String转int
      if (wtup >= 1 && wtup <= 60)
      {
        updateweater_time = wtup;
        SMOD = "";
        Serial.printf("天气更新时间更改为：");
        Serial.print(updateweater_time);
        Serial.println("分钟");
      }
      else
        Serial.println("更新时间太长，请重新设置（1-60）");
    }
    if (SMOD == "0x06")
    {
      if (incomingByte.length() == 32)
      {
        saveTDKeytoEEP(incomingByte);
        SMOD = "";
        Serial.println("TD KEY设置成功");
        delay(5);
        readTDKeyfromEEP();
        delay(5);
        Serial.print("TD KEY:");
        Serial.println(TD_key);
        getTD();
      }
      else
      {
        Serial.println("TD KEY设置失败");
      }
    }
    else
    {
      SMOD = incomingByte;
      delay(2);
      if (SMOD == "0x01")
        Serial.println("请输入亮度值，范围0-100");
      else if (SMOD == "0x02")
        Serial.println("请输入9位城市代码，自动获取请输入0");
      else if (SMOD == "0x03")
      {
        Serial.println("请输入屏幕方向值，");
        Serial.println("0-USB接口朝下");
        Serial.println("1-USB接口朝右");
        Serial.println("2-USB接口朝上");
        Serial.println("3-USB接口朝左");
      }
      else if (SMOD == "0x04")
      {
        Serial.print("当前天气更新时间：");
        Serial.print(updateweater_time);
        Serial.println("分钟");
        Serial.println("请输入天气更新时间（1-60）分钟");
      }
      else if (SMOD == "0x05")
      {
        Serial.println("重置WiFi设置中......");
        delay(10);
        wm.resetSettings();
        deletewificonfig();
        delay(10);
        Serial.println("重置WiFi成功");
        SMOD = "";
        ESP.restart();
      }
      else if (SMOD == "0x06"){
        Serial.println("请输入TD_KEY：");
      }
      else if (SMOD == "0x99"){
        ESP.restart();
      }
      else if (SMOD == "0x07"){
        getNtpTime();
      }
      else
      {
        Serial.println("");
        Serial.println("请输入需要修改的代码：");
        Serial.println("亮度设置输入        0x01");
        Serial.println("地址设置输入        0x02");
        Serial.println("屏幕方向设置输入    0x03");
        Serial.println("更改天气更新时间    0x04");
        Serial.println("重置WiFi(会重启)    0x05");
        Serial.println("输入TD KEY         0x06");
        Serial.println("重置时间            0x07");
        Serial.println("重启设备            0x99");
        Serial.println("");
      }
    }
  }
}

#if WM_EN
// WEB配网LCD显示函数
void Web_win()
{
  clk.setColorDepth(8);

  clk.createSprite(200, 60); //创建窗口
  clk.fillSprite(0x0000);    //填充率

  clk.setTextDatum(CC_DATUM); //设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("WiFi Connect Fail!", 100, 10, 2);
  clk.drawString("SSID:", 45, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawString("AutoConnectAP", 125, 40, 2);
  clk.pushSprite(20, 50); //窗口位置

  clk.deleteSprite();
}

// WEB配网函数
void Webconfig()
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  delay(3000);
  wm.resetSettings(); // wipe settings

  // add a custom input field
  // int customFieldLength = 40;

  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\"");

  // test custom html input type(checkbox)
  //  new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type

  // test custom html(radio)
  // const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  // new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input

  const char *set_rotation = "<br/><label for='set_rotation'>显示方向设置</label>\
                              <input type='radio' name='set_rotation' value='0' checked> USB接口朝下<br>\
                              <input type='radio' name='set_rotation' value='1'> USB接口朝右<br>\
                              <input type='radio' name='set_rotation' value='2'> USB接口朝上<br>\
                              <input type='radio' name='set_rotation' value='3'> USB接口朝左<br>";
  WiFiManagerParameter custom_rot(set_rotation); // custom html input
  WiFiManagerParameter custom_bl("LCDBL", "屏幕亮度（1-100）", "10", 3);
#if DHT_EN
  WiFiManagerParameter custom_DHT11_en("DHT11_en", "Enable DHT11 sensor", "0", 1);
#endif
  WiFiManagerParameter custom_weatertime("WeaterUpdateTime", "天气刷新时间（分钟）", "10", 3);
  WiFiManagerParameter custom_cc("CityCode", "城市代码", "0", 9);
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

  // wm.setConnectTimeout(20); // how long to try to connect for before continuing
  //  wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
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
  res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  //  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  while (!res)
    ;
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

//删除原有eeprom中的信息
void deletewificonfig()
{
  config_type deletewifi = {{""}, {""}};
  uint8_t *p = (uint8_t *)(&deletewifi);
  for (unsigned int i = 0; i < sizeof(deletewifi); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //在闪存内模拟写入
  }
  delay(10);
  EEPROM.commit(); //执行写入ROM
  delay(10);
}

//从eeprom读取WiFi信息ssid，psw
void readwificonfig()
{
  uint8_t *p = (uint8_t *)(&wificonf);
  for (unsigned int i = 0; i < sizeof(wificonf); i++)
  {
    *(p + i) = EEPROM.read(i + wifi_addr);
  }
  // EEPROM.commit();
  // ssid = wificonf.stassid;
  // pass = wificonf.stapsw;
  Serial.printf("Read WiFi Config.....\r\n");
  Serial.printf("SSID:%s\r\n", wificonf.stassid);
  Serial.printf("PSW:%s\r\n", wificonf.stapsw);
  Serial.printf("Connecting.....\r\n");
}

void saveTDKeytoEEP(String td_api_key) {
  for (int cnum = 0; cnum < 32; cnum++) {
    EEPROM.write(td_key_addr + cnum, td_api_key[cnum]);  
    Serial.print(td_api_key[cnum]);
    EEPROM.commit();                                
    delay(5);
  }
  Serial.println("");
}
void readTDKeyfromEEP() {
  TD_key = "";
  for (int cnum = 0; cnum < 32; cnum++) {
    TD_key += char(EEPROM.read(td_key_addr + cnum));
    delay(5);
  }
}

void saveParamCallback()
{
  int CCODE = 0, cc;

  Serial.println("[CALLBACK] saveParamCallback fired");
  // Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
  // Serial.println("PARAM CityCode = " + getParam("CityCode"));
  // Serial.println("PARAM LCD BackLight = " + getParam("LCDBL"));
  // Serial.println("PARAM WeaterUpdateTime = " + getParam("WeaterUpdateTime"));
  // Serial.println("PARAM Rotation = " + getParam("set_rotation"));
  // Serial.println("PARAM DHT11_en = " + getParam("DHT11_en"));
//将从页面中获取的数据保存
#if DHT_EN
  DHT_img_flag = getParam("DHT11_en").toInt();
#endif
  updateweater_time = getParam("WeaterUpdateTime").toInt();
  cc = getParam("CityCode").toInt();
  LCD_Rotation = getParam("set_rotation").toInt();
  LCD_BL_PWM = getParam("LCDBL").toInt();

  //对获取的数据进行处理
  //城市代码
  Serial.print("CityCode = ");
  Serial.println(cc);
  if (((cc >= 101000000) && (cc <= 102000000)) || (cc == 0))
  {
    for (int cnum = 0; cnum < 5; cnum++)
    {
      EEPROM.write(CC_addr + cnum, cc % 100); //城市地址写入城市代码
      EEPROM.commit();                        //保存更改的数据
      cc = cc / 100;
      delay(5);
    }
    for (int cnum = 5; cnum > 0; cnum--)
    {
      CCODE = CCODE * 100;
      CCODE += EEPROM.read(CC_addr + cnum - 1);
      delay(5);
    }
    cityCode = CCODE;
  }
  //屏幕方向
  Serial.print("LCD_Rotation = ");
  Serial.println(LCD_Rotation);
  if (EEPROM.read(Ro_addr) != LCD_Rotation)
  {
    EEPROM.write(Ro_addr, LCD_Rotation);
    EEPROM.commit();
    delay(5);
  }
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(0x0000);
  Web_win();
  loadNum--;
  loading(1);
  if (EEPROM.read(BL_addr) != LCD_BL_PWM)
  {
    EEPROM.write(BL_addr, LCD_BL_PWM);
    EEPROM.commit();
    delay(5);
  }
  // 屏幕亮度
  Serial.printf("亮度调整为：");
  analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM * 10));
  Serial.println(LCD_BL_PWM);
  // 天气更新时间
  Serial.printf("天气更新时间调整为：");
  Serial.println(updateweater_time);

#if DHT_EN
  // 是否使用DHT11传感器
  Serial.printf("DHT11传感器：");
  EEPROM.write(DHT_addr, DHT_img_flag);
  EEPROM.commit(); //保存更改的数据
  Serial.println((DHT_img_flag ? "已启用" : "未启用"));
#endif
}
#endif

// 发送HTTP请求并且将服务器响应通过串口输出
void getCityCode()
{
  String URL = "http://wgeo.weather.com.cn/ip/?_=" + String(now());
  //创建 HTTPClient 对象
  HTTPClient httpClient;

  //配置请求地址。此处也可以不使用端口号和PATH而单纯的
  httpClient.begin(wificlient, URL);

  //设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");

  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);

  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK)
  {
    String str = httpClient.getString();

    int aa = str.indexOf("id=");
    if (aa > -1)
    {
      // cityCode = str.substring(aa+4,aa+4+9).toInt();
      cityCode = str.substring(aa + 4, aa + 4 + 9);
      Serial.println(cityCode);
      getCityWeater();
    }
    else
    {
      Serial.println("获取城市代码失败");
    }
  }
  else
  {
    Serial.println("请求城市代码错误：");
    Serial.println(httpCode);
  }

  //关闭ESP8266与服务器连接
  httpClient.end();
}

// 获取城市天气
void getCityWeater()
{
  // String URL = "http://d1.weather.com.cn/dingzhi/" + cityCode + ".html?_="+String(now());//新
  String URL = "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_=" + String(now()); //原来
  //创建 HTTPClient 对象
  HTTPClient httpClient;

  // httpClient.begin(URL);
  httpClient.begin(wificlient, URL); //使用新方法

  //设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");

  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.println("正在获取天气数据");
  // Serial.println(URL);

  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK)
  {

    String str = httpClient.getString();
    int indexStart = str.indexOf("weatherinfo\":");
    int indexEnd = str.indexOf("};var alarmDZ");

    String jsonCityDZ = str.substring(indexStart + 13, indexEnd);
    // Serial.println(jsonCityDZ);

    indexStart = str.indexOf("dataSK =");
    indexEnd = str.indexOf(";var dataZS");
    String jsonDataSK = str.substring(indexStart + 8, indexEnd);
    // Serial.println(jsonDataSK);

    indexStart = str.indexOf("\"f\":[");
    indexEnd = str.indexOf(",{\"fa");
    String jsonFC = str.substring(indexStart + 5, indexEnd);
    // Serial.println(jsonFC);

    weaterData(&jsonCityDZ, &jsonDataSK, &jsonFC);
    Serial.println("获取成功");
  }
  else
  {
    Serial.println("请求城市天气错误：");
    Serial.print(httpCode);
  }

  //关闭ESP8266与服务器连接
  httpClient.end();
}

String HTTPS_request(String host, String url, String parameter = "", String fingerprint = "", int Port = 443, int Receive_cache = 1024)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi未连接！");
        WiFi.begin(wificonf.stassid, wificonf.stapsw);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }
        Serial.println("WiFi连接成功！");
    }
    WiFiClientSecure HTTPS; //建立WiFiClientSecure对象
    if (parameter != "")
        parameter = "?" + parameter;
    String postRequest = (String)("GET ") + url + parameter + " HTTP/1.1\r\n" +
                         "Host: " + host + "\r\n" +
                         "User-Agent: Mozilla/5.0 (iPhone; CPU iPhone OS 13_2_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.0.3 Mobile/15E148 Safari/604.1 Edg/103.0.5060.53" +
                         "\r\n\r\n";
    if (fingerprint.length() == 0)
        HTTPS.setInsecure(); //不进行服务器身份认证
    else
    {
        HTTPS.setFingerprint(fingerprint.c_str()); //服务器证书指纹进行服务器身份认证
    }
    int cache = sizeof(postRequest) + 10;
    Serial.print("发送缓存：");
    // Serial.println(postRequest);
    HTTPS.setBufferSizes(Receive_cache, cache); //接收和发送缓存大小
    HTTPS.setTimeout(15000);                    //设置等待的最大毫秒数
    Serial.println("初始化参数完毕！\n开始连接服务器==>>>>>");
    if (!HTTPS.connect(host, Port))
    {
        delay(100);
        Serial.println();
        Serial.println("服务器连接失败！");
        return "0";
    }
    else
        Serial.println("服务器连接成功！\r");
    Serial.println("发送请求：\n");
    HTTPS.print(postRequest.c_str()); // 发送HTTP请求

    // 检查服务器响应信息。通过串口监视器输出服务器状态码和响应头信息
    // 从而确定ESP8266已经成功连接服务器
    Serial.println("获取响应信息========>：\r");
    Serial.println("响应头：");
    while (HTTPS.connected())
    {
        String line = HTTPS.readStringUntil('\n');
        // Serial.println(line);
        if (line == "\r")
        {
            Serial.println("响应头输出完毕！"); // Serial.println("响应头屏蔽完毕！\r");
            break;
        }
    }
    Serial.println("截取响应体==========>");
    String line;
    while (HTTPS.connected())
    {
        line = HTTPS.readStringUntil('\n'); // Serial.println(line);
        if (line.length() > 10)
            break;
    }
    Serial.println("响应体信息：\n");
    Serial.println("====================================>");
    Serial.println("变量长度：" + String(line.length()));
    Serial.println("变量大小：" + String(sizeof(line)) + "字节");
    Serial.println("====================================>");
    HTTPS.stop(); //操作结束，断开服务器连接
    delay(500);
    return line;
} 

String TD_year = "year";
String TD_month = "month";
String TD_day = "day";
String TD_animal = "animal";
String TD_lubarmonth = "lubarmonth";
String TD_lunarday = "lunarday";

void getTD() {
  // String URL = "https://apis.tianapi.com/lunar/index?key=" + TD_key;
  String str = HTTPS_request("apis.tianapi.com", "/lunar/index", "key=" + TD_key);

  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (str != "0" && str.length() != 0) {
    DynamicJsonDocument doc(str.length() * 2);
    deserializeJson(doc, str);
    JsonObject sk = doc.as<JsonObject>();
    TD_year = sk["result"]["tiangandizhiyear"].as<String>();
    TD_month = sk["result"]["tiangandizhimonth"].as<String>();
    TD_day = sk["result"]["tiangandizhiday"].as<String>();
    TD_animal = sk["result"]["shengxiao"].as<String>();
    TD_lubarmonth = sk["result"]["lubarmonth"].as<String>();
    TD_lunarday = sk["result"]["lunarday"].as<String>();
    Serial.println("获取成功");
  } else {
    Serial.println("请求天干地支错误");
  }
}

String scrollText[7];
// int scrollTextWidth = 0;
String strTDDate[3];

// 天气信息写到屏幕上
void weaterData(String *cityDZ, String *dataSK, String *dataFC)
{
  // 解析第一段JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, *dataSK);
  JsonObject sk = doc.as<JsonObject>();

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
  tempnum = sk["temp"].as<int>();
  tempnum = tempnum + 10;
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
  huminum = atoi((sk["SD"].as<String>()).substring(0, 2).c_str());

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
  clk.createSprite(94, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["cityname"].as<String>(), 44, 16);
  clk.pushSprite(15, 15);
  clk.deleteSprite();

  //PM2.5空气指数
  uint16_t pm25BgColor = tft.color565(156, 202, 127);  //优
  String aqiTxt = "优";
  int pm25V = sk["aqi"];
  String strOtherAqiInfo = " " + String(int(pm25V)) + "";
  if (pm25V > 200) {
    pm25BgColor = tft.color565(136, 11, 32);  //重度
    aqiTxt = "重度";
  } else if (pm25V > 150) {
    pm25BgColor = tft.color565(186, 55, 121);  //中度
    aqiTxt = "中度";
  } else if (pm25V > 100) {
    pm25BgColor = tft.color565(242, 159, 57);  //轻
    aqiTxt = "轻度";
  } else if (pm25V > 50) {
    pm25BgColor = tft.color565(247, 219, 100);  //良
    aqiTxt = "良";
  } else if (pm25V <= 50) {
    pm25BgColor = tft.color565(156, 202, 127);  //优
    aqiTxt = "优";
  }
  aqiTxt = aqiTxt + strOtherAqiInfo;
  clk.createSprite(66, 24);
  clk.fillSprite(bgColor);
  clk.fillRoundRect(0, 0, 60, 24, 4, pm25BgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(0x0000);
  clk.drawString(aqiTxt, 25, 13);
  clk.pushSprite(94, 18);
  clk.deleteSprite();

  scrollText[0] = "实时天气 " + sk["weather"].as<String>();
  scrollText[1] = "空气质量 " + aqiTxt;
  scrollText[2] = "风向 " + sk["WD"].as<String>() + sk["WS"].as<String>();

  // scrollText[6] = atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()) ;

  //天气图标
  wrat.printfweather(170, 15, atoi((sk["weathercode"].as<String>()).substring(1, 3).c_str()));

  //左上角滚动字幕
  //解析第二段JSON
  deserializeJson(doc, *cityDZ);
  JsonObject dz = doc.as<JsonObject>();
  // Serial.println(sk["ws"].as<String>());
  //横向滚动方式
  // String aa = "今日天气:" + dz["weather"].as<String>() + "，温度:最低" + dz["tempn"].as<String>() + "，最高" + dz["temp"].as<String>() + " 空气质量:" + aqiTxt + "，风向:" + dz["wd"].as<String>() + dz["ws"].as<String>();
  // scrollTextWidth = clk.textWidth(scrollText);
  // Serial.println(aa);
  scrollText[3] = "今日" + dz["weather"].as<String>();

  deserializeJson(doc, *dataFC);
  JsonObject fc = doc.as<JsonObject>();

  scrollText[4] = "最低温度" + fc["fd"].as<String>() + "℃";
  scrollText[5] = "最高温度" + fc["fc"].as<String>() + "℃";

  // Serial.println(scrollText[0]);

  clk.unloadFont();
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
    clkb.pushSprite(10, 45);

    clkb.deleteSprite();
    clkb.unloadFont();

    if (currentIndex >= 5)
      currentIndex = 0; //回第一个
    else
      currentIndex += 1; //准备切换到下一个
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
  int now_hour = hour();     //获取小时
  int now_minute = minute(); //获取分钟
  int now_second = second(); //获取秒针
  //小时刷新
  if ((now_hour != Hour_sign) || (reflash_en == 1))
  {
    drawLineFont(20, timeY, now_hour / 10, 3, SD_FONT_WHITE);
    drawLineFont(60, timeY, now_hour % 10, 3, SD_FONT_WHITE);
    Hour_sign = now_hour;
    getTD();
  }
  //分钟刷新
  if ((now_minute != Minute_sign) || (reflash_en == 1))
  {
    drawLineFont(101, timeY, now_minute / 10, 3, SD_FONT_YELLOW);
    drawLineFont(141, timeY, now_minute % 10, 3, SD_FONT_YELLOW);
    Minute_sign = now_minute;
  }
  //秒针刷新
  if ((now_second != Second_sign) || (reflash_en == 1)) //分钟刷新
  {
    drawLineFont(182, timeY + 30, now_second / 10, 2, SD_FONT_WHITE);
    drawLineFont(202, timeY + 30, now_second % 10, 2, SD_FONT_WHITE);
    Second_sign = now_second;
  }

  if (reflash_en == 1)
    reflash_en = 0;
  /***日期****/
  strTDDate[0] = String(monthDay()) + " " + String(week());
  strTDDate[1] = TD_lubarmonth + " " + TD_lunarday + " " + TD_animal;
  strTDDate[2] = TD_year + " " + TD_month + " " + TD_day;
  /***日期****/
}

int currentTDIndex = 0;
void TDBanner() {
  if (strTDDate[currentTDIndex]) {
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

    if (currentTDIndex >= 2)
      currentTDIndex = 0;  //回第一个
    else
      currentTDIndex += 1;  //准备切换到下一个
  }
  prevTime = 1;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;     // NTP时间在消息的前48字节中
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  // Serial.println("Transmit NTP Request");
  //  get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  // Serial.print(ntpServerName);
  // Serial.print(": ");
  // Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      // Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // 无法获取时间时返回0
}

// 向NTP服务器发送请求
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void esp_reset(Button2 &btn)
{
  ESP.reset();
}

void wifi_reset(Button2 &btn)
{
  wm.resetSettings();
  deletewificonfig();
  delay(10);
  Serial.println("重置WiFi成功");
  ESP.restart();
}

//更新时间
void reflashTime()
{
  prevDisplay = now();
  // timeClockDisplay(1);
  digitalClockDisplay();
  prevTime = 0;
}

//切换天气 or 空气质量
void reflashBanner()
{
#if DHT_EN
  if (DHT_img_flag != 0)
    IndoorTem();
#endif
  scrollBanner();
  TDBanner();
}

//所有需要联网后更新的方法都放在这里
void WIFI_reflash_All()
{
  if (Wifi_en == 1)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("WIFI connected");

      // Serial.println("getCityWeater start");
      getCityWeater();
      getTD();
      // Serial.println("getCityWeater end");

      getNtpTime();
      //其他需要联网的方法写在后面

      // WiFi.forceSleepBegin(); // Wifi Off
      // Serial.println("WIFI sleep......");
      // Wifi_en = 0;
      closeWifi();
    }
    else
    {
      // Serial.println("WIFI unconnected");
    }
  }
}

// 打开WIFI
void openWifi()
{
  Serial.println("WIFI reset......");
  WiFi.forceSleepWake(); // wifi on
  Wifi_en = 1;
}

void closeWifi()
{
  WiFi.forceSleepBegin(); // Wifi Off
  Serial.println("WIFI sleep......");
  Wifi_en = 0;
}

// 强制屏幕刷新
void LCD_reflash()
{
  reflashTime();
  reflashBanner();
  openWifi();
}

// 守护线程池
void Supervisor_controller()
{
  if (controller.shouldRun())
  {
    // Serial.println("controller 启动");
    controller.run();
  }
}

void setup()
{
  Button_sw1.setClickHandler(esp_reset);
  Button_sw1.setLongClickHandler(wifi_reset);
  Serial.begin(115200);
  EEPROM.begin(1024);
  // WiFi.forceSleepWake();
  // wm.resetSettings();    //在初始化中使wifi重置，需重新配置WiFi

#if DHT_EN
  dht.begin();
  //从eeprom读取DHT传感器使能标志
  DHT_img_flag = EEPROM.read(DHT_addr);
#endif
  readTDKeyfromEEP();
  //从eeprom读取背光亮度设置
  if (EEPROM.read(BL_addr) > 0 && EEPROM.read(BL_addr) < 100)
    LCD_BL_PWM = EEPROM.read(BL_addr);
  //从eeprom读取屏幕方向设置
  if (EEPROM.read(Ro_addr) >= 0 && EEPROM.read(Ro_addr) <= 3)
    LCD_Rotation = EEPROM.read(Ro_addr);

  pinMode(LCD_BL_PIN, OUTPUT);
  analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM * 10));

  tft.begin();          /* TFT init */
  tft.invertDisplay(1); //反转所有显示颜色：1反转，0正常
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(0x0000);
  tft.setTextColor(TFT_BLACK, bgColor);

  targetTime = millis() + 1000;
  readwificonfig(); //读取存储的wifi信息
  Serial.print("正在连接WIFI ");
  Serial.println(wificonf.stassid);
  WiFi.begin(wificonf.stassid, wificonf.stapsw);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  while (WiFi.status() != WL_CONNECTED)
  {
    loading(30);

    if (loadNum >= 194)
    {
//使能web配网后自动将smartconfig配网失效
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
  while (loadNum < 194) //让动画走完
  {
    loading(1);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("SSID:");
    Serial.println(WiFi.SSID().c_str());
    Serial.print("PSW:");
    Serial.println(WiFi.psk().c_str());
    strcpy(wificonf.stassid, WiFi.SSID().c_str()); //名称复制
    strcpy(wificonf.stapsw, WiFi.psk().c_str());   //密码复制
    savewificonfig();
    readwificonfig();
  }

  Serial.print("本地IP： ");
  Serial.println(WiFi.localIP());
  Serial.println("启动UDP");
  Udp.begin(localPort);
  Serial.println("等待同步...");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  int CityCODE = 0;
  for (int cnum = 5; cnum > 0; cnum--)
  {
    CityCODE = CityCODE * 100;
    CityCODE += EEPROM.read(CC_addr + cnum - 1);
    delay(5);
  }
  if (CityCODE >= 101000000 && CityCODE <= 102000000)
    cityCode = CityCODE;
  else
    getCityCode(); //获取城市代码

  tft.fillScreen(TFT_BLACK); //清屏

  TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); //温度图标
  TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       //湿度图标

  getCityWeater();
#if DHT_EN
  if (DHT_img_flag != 0)
    IndoorTem();
#endif

  // WiFi.forceSleepBegin(); // wifi off
  // Serial.println("WIFI休眠......");
  // Wifi_en = 0;
  closeWifi();
  reflash_time.setInterval(300); //设置所需间隔 100毫秒
  reflash_time.onRun(reflashTime);

  reflash_Banner.setInterval(2 * TMS); //设置所需间隔 2秒
  reflash_Banner.onRun(reflashBanner);

  reflash_openWifi.setInterval(updateweater_time * 60 * TMS); //设置所需间隔 10分钟
  reflash_openWifi.onRun(openWifi);

  reflash_Animate.setInterval(TMS / 10); //设置帧率
  reflash_openWifi.onRun(refresh_AnimatedImage);
  controller.run();
}

const uint8_t *Animate_value; //指向关键帧的指针
uint32_t Animate_size;        //指向关键帧大小的指针
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
    }
  }
#endif
}

void loop()
{
  // refresh_AnimatedImage(&TJpgDec); //更新右下角
  refresh_AnimatedImage(); //更新右下角
  Supervisor_controller(); // 守护线程池
  WIFI_reflash_All();      // WIFI应用
  Serial_set();            //串口响应
  Button_sw1.loop();       //按钮轮询
}