

/* *****************************************************************
 *
 * SmallDesktopDisplay
 *    С��������ʾ��
 *
 * ԭ  ��  �ߣ�Misaka
 * ��      �ģ�΢����
 * �ٴ�  �޸ģ���ɽ��
 * ��  ��  Ⱥ��811058758��887171863��720661626
 * �� �� �� �ڣ�2021.07.19
 * ���������ڣ�2021.11.28
 * �� �� ˵ ����V1.1��Ӵ��ڵ��ԣ�������115200\8\n\1�����Ӱ汾����ʾ��
 *            V1.2���Ⱥͳ��д��뱣�浽EEPROM���ϵ�ɱ���
 *            V1.3.1 ����smartconfig��ΪWEB����ģʽ��ͬʱ��������ͬʱ�������ȡ���Ļ�������á�
 *            V1.3.2 ����wifi����ģʽ��������Ҫ���ӵ�����¿���wifi������ʱ��ر�wifi������wifi������eeprom��Ŀǰ������һ��ssid�����룩
 *            V1.3.3  �޸�WiFi������޷�ɾ�������⡣Ŀǰ����Ϊʹ�ô��ڿ��ƣ����� 0x05 ����WiFi���ݲ�������
 *                    ����web�����Լ�����������������ʱ��Ĺ��ܡ�
 *            V1.3.4  �޸�web����ҳ�����ã���wifi����ҳ���Լ���������ѡ�����ͬһҳ���С�
 *                    ����webҳ�������Ƿ�ʹ��DHT����������ʹ��DHT��ſ�ʹ�ã�
 *             1.4.2  ���ӳ���SD3 Plus�ײ���ť����WiFi
 *                    ����������ҳ����ҳ��
 *
 * �� �� �� �䣺 SCK  GPIO14
 *             MOSI  GPIO13
 *             RES   GPIO2
 *             DC    GPIO0
 *             LCDBL GPIO5
 *
 *             ����DHT11��ʪ�ȴ��������������ӿ�Ϊ GPIO 12
 *
 *    ��лȺ�� @���ʧ��  ���ѷ���WiFi������޷����õ����⣬Ŀǰ�ѽ��������鿴����˵����
 * *****************************************************************/

/* *****************************************************************
 *  ���ļ���ͷ�ļ�
 * *****************************************************************/
#include "ArduinoJson.h"
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <EEPROM.h>
#include "qr.h"
#include "number.h"
#include "weathernum.h"
#include <Button2.h> //��ť��

#define Version "SDD V1.4.2"
/* *****************************************************************
 *  ����ʹ��λ
 * *****************************************************************/
// WEB����ʹ�ܱ�־λ----WEB�����򿪺��Ĭ�Ϲر�smartconfig����
#define WM_EN 1
//�趨DHT11��ʪ�ȴ�����ʹ�ܱ�־
#define DHT_EN 0
//����̫����ͼƬ�Ƿ�ʹ��
#define imgAst_EN 1

#if WM_EN
#include <WiFiManager.h>
// WiFiManager ����
WiFiManager wm; // global wm instance
// WiFiManagerParameter custom_field; // global param ( for non blocking w params )
#endif

#if DHT_EN
#include "DHT.h"
#define DHTPIN 12
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#endif

//���尴ť����
Button2 Button_sw1 = Button2(4);

/* *****************************************************************
 *  �ֿ⡢ͼƬ��
 * *****************************************************************/
#include "font/ZdyLwFont_20.h"
#include "img/misaka.h"
#include "img/temperature.h"
#include "img/humidity.h"

#if imgAst_EN
#include "img/pangzi/i0.h"
#include "img/pangzi/i1.h"
#include "img/pangzi/i2.h"
#include "img/pangzi/i3.h"
#include "img/pangzi/i4.h"
#include "img/pangzi/i5.h"
#include "img/pangzi/i6.h"
#include "img/pangzi/i7.h"
#include "img/pangzi/i8.h"
#include "img/pangzi/i9.h"

int Anim = 0;      //̫����ͼ����ʾָ���¼
int AprevTime = 0; //̫���˸���ʱ���¼
#endif

/* *****************************************************************
 *  ��������
 * *****************************************************************/

struct config_type
{
  char stassid[32]; //���������õ���WIFI������(���32�ֽ�)
  char stapsw[64];  //���������õ���WIFI���볤��(���64�ֽ�)
};

//---------------�޸Ĵ˴�""�ڵ���Ϣ--------------------
//�翪��WEB������ɲ�����������Ĳ�����ǰһ��Ϊwifi ssid����һ��Ϊ����
config_type wificonf = {{"HUAWEI-0G17LY"}, {"19990823"}};

//��������ʱ��  X ����
int updateweater_time = 10;

//----------------------------------------------------

// LCD��Ļ�������
TFT_eSPI tft = TFT_eSPI(); // ��������������tft_espi���е� User_Setup.h�ļ�
TFT_eSprite clk = TFT_eSprite(&tft);
#define LCD_BL_PIN 5 // LCD��������
uint16_t bgColor = 0x0000;

//����״̬��־λ
int LCD_Rotation = 0;        // LCD��Ļ����
int LCD_BL_PWM = 50;         //��Ļ����0-100��Ĭ��50
uint8_t Wifi_en = 1;         // wifi״̬��־λ  1����    0���ر�
uint8_t UpdateWeater_en = 0; //����ʱ���־λ
int prevTime = 0;            //������ʾ���±�־λ
int DHT_img_flag = 0;        // DHT������ʹ�ñ�־λ

time_t prevDisplay = 0;       //��ʾʱ����ʾ��¼
unsigned long weaterTime = 0; //��������ʱ���¼

/*** Component objects ***/
Number dig;
WeatherNum wrat;

uint32_t targetTime = 0;
String cityCode = "0"; //�������д��� ��ɳ:101250101����:101250301����:101250401
int tempnum = 0;       //�¶Ȱٷֱ�
int huminum = 0;       //ʪ�Ȱٷֱ�
int tempcol = 0xffff;  //�¶���ʾ��ɫ
int humicol = 0xffff;  //ʪ����ʾ��ɫ

// EEPROM�����洢��ַλ
int BL_addr = 1;    //��д�����ݵ�EEPROM��ַ���  1����
int Ro_addr = 2;    //��д�����ݵ�EEPROM��ַ���  2 ��ת����
int DHT_addr = 3;   // 3 DHTʹ�ܱ�־λ
int CC_addr = 10;   //��д�����ݵ�EEPROM��ַ���  10����
int wifi_addr = 30; //��д�����ݵ�EEPROM��ַ���  20wifi-ssid-psw

// NTP����������
static const char ntpServerName[] = "ntp6.aliyun.com";
const int timeZone = 8; //������

// wifi����UDP���ò���
WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
float duty = 0;

//��������
time_t getNtpTime();
void digitalClockDisplay(int reflash_en);
void printDigits(int digits);
String num2str(int digits);
void sendNTPpacket(IPAddress &address);
void LCD_reflash(int en);
void savewificonfig();
void readwificonfig();
void deletewificonfig();

/* *****************************************************************
 *  ����
 * *****************************************************************/

// wifi ssid��psw���浽eeprom
void savewificonfig()
{
  //��ʼд��
  uint8_t *p = (uint8_t *)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //��������ģ��д��
  }
  delay(10);
  EEPROM.commit(); //ִ��д��ROM
  delay(10);
}
//ɾ��ԭ��eeprom�е���Ϣ
void deletewificonfig()
{
  config_type deletewifi = {{""}, {""}};
  uint8_t *p = (uint8_t *)(&deletewifi);
  for (int i = 0; i < sizeof(deletewifi); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //��������ģ��д��
  }
  delay(10);
  EEPROM.commit(); //ִ��д��ROM
  delay(10);
}

//��eeprom��ȡWiFi��Ϣssid��psw
void readwificonfig()
{
  uint8_t *p = (uint8_t *)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++)
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

// TFT��Ļ�������
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (y >= tft.height())
    return 0;
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

//����������
byte loadNum = 6;
void loading(byte delayTime) //���ƽ�����
{
  clk.setColorDepth(8);

  clk.createSprite(200, 100); //��������
  clk.fillSprite(0x0000);     //�����

  clk.drawRoundRect(0, 0, 200, 16, 8, 0xFFFF);     //����Բ�Ǿ���
  clk.fillRoundRect(3, 3, loadNum, 10, 5, 0xFFFF); //ʵ��Բ�Ǿ���
  clk.setTextDatum(CC_DATUM);                      //�����ı�����
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("Connecting to WiFi......", 100, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawRightString(Version, 180, 60, 2);
  clk.pushSprite(20, 120); //����λ��

  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, 0x0000);
  // clk.pushSprite(130,180);

  clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

//ʪ��ͼ����ʾ����
void humidityWin()
{
  clk.setColorDepth(8);

  huminum = huminum / 2;
  clk.createSprite(52, 6);                         //��������
  clk.fillSprite(0x0000);                          //�����
  clk.drawRoundRect(0, 0, 52, 6, 3, 0xFFFF);       //����Բ�Ǿ���  ��ʼλx,y,���ȣ���ȣ�Բ���뾶����ɫ
  clk.fillRoundRect(1, 1, huminum, 4, 2, humicol); //ʵ��Բ�Ǿ���
  clk.pushSprite(45, 222);                         //����λ��
  clk.deleteSprite();
}

//�¶�ͼ����ʾ����
void tempWin()
{
  clk.setColorDepth(8);

  clk.createSprite(52, 6);                         //��������
  clk.fillSprite(0x0000);                          //�����
  clk.drawRoundRect(0, 0, 52, 6, 3, 0xFFFF);       //����Բ�Ǿ���  ��ʼλx,y,���ȣ���ȣ�Բ���뾶����ɫ
  clk.fillRoundRect(1, 1, tempnum, 4, 2, tempcol); //ʵ��Բ�Ǿ���
  clk.pushSprite(45, 192);                         //����λ��
  clk.deleteSprite();
}

#if DHT_EN
//���DHT11����������ʾ����
void IndoorTem()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  String s = "����";
  /***�����������***/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  //λ��
  clk.createSprite(58, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(s, 29, 16);
  clk.pushSprite(172, 150);
  clk.deleteSprite();

  //�¶�
  clk.createSprite(60, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawFloat(t, 1, 20, 13);
  //  clk.drawString(sk["temp"].as<String>()+"��",28,13);
  clk.drawString("��", 50, 13);
  clk.pushSprite(170, 184);
  clk.deleteSprite();

  //ʪ��
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
//΢����������
void SmartConfig(void)
{
  WiFi.mode(WIFI_STA); //����STAģʽ
  // tft.pushImage(0, 0, 240, 240, qr);
  tft.pushImage(0, 0, 240, 240, qr);
  Serial.println("\r\nWait for Smartconfig..."); //��ӡlog��Ϣ
  WiFi.beginSmartConfig();                       //��ʼSmartConfig���ȴ��ֻ��˷����û���������
  while (1)
  {
    Serial.print(".");
    delay(100);                 // wait for a second
    if (WiFi.smartConfigDone()) //�����ɹ������յ�SSID������
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

String SMOD = ""; // 0����
//���ڵ������ú���
void Serial_set()
{
  String incomingByte = "";
  if (Serial.available() > 0)
  {

    while (Serial.available() > 0) //��⴮�ڻ��棬������������ʱ��ѭ����ֵ��incomingByte
    {
      incomingByte += char(Serial.read()); //��ȡ�����ַ�ֵ��ת��Ϊ�ַ�������˳��һ������ֵ��incomingByte
      delay(2);                            //����ʡ�ԣ���Ϊ��ȡ������������Ҫʱ��
    }
    if (SMOD == "0x01") //����1��������
    {
      int LCDBL = atoi(incomingByte.c_str()); // int n = atoi(xxx.c_str());//Stringתint
      if (LCDBL >= 0 && LCDBL <= 100)
      {
        EEPROM.write(BL_addr, LCDBL); //���ȵ�ַд������ֵ
        EEPROM.commit();              //������ĵ�����
        delay(5);
        LCD_BL_PWM = EEPROM.read(BL_addr);
        delay(5);
        SMOD = "";
        Serial.printf("���ȵ���Ϊ��");
        analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM * 10));
        Serial.println(LCD_BL_PWM);
        Serial.println("");
      }
      else
        Serial.println("���ȵ�������������0-100");
    }
    if (SMOD == "0x02") //����2��ַ����
    {
      int CityCODE = 0;
      int CityC = atoi(incomingByte.c_str()); // int n = atoi(xxx.c_str());//Stringתint
      if (CityC >= 101000000 && CityC <= 102000000 || CityC == 0)
      {
        for (int cnum = 0; cnum < 5; cnum++)
        {
          EEPROM.write(CC_addr + cnum, CityC % 100); //���е�ַд����д���
          EEPROM.commit();                           //������ĵ�����
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
          Serial.println("���д������Ϊ���Զ�");
          getCityCode(); //��ȡ���д���
        }
        else
        {
          Serial.printf("���д������Ϊ��");
          Serial.println(cityCode);
        }
        Serial.println("");
        getCityWeater(); //���³�������
        SMOD = "";
      }
      else
        Serial.println("���е�������������9λ���д��룬�Զ���ȡ������0");
    }
    if (SMOD == "0x03") //����3��Ļ��ʾ����
    {
      int RoSet = atoi(incomingByte.c_str());
      if (RoSet >= 0 && RoSet <= 3)
      {
        EEPROM.write(Ro_addr, RoSet); //��Ļ�����ַд�뷽��ֵ
        EEPROM.commit();              //������ĵ�����
        SMOD = "";
        //������Ļ���������ˢ������ʾ
        tft.setRotation(RoSet);
        tft.fillScreen(0x0000);
        LCD_reflash(1); //��Ļˢ�³���
        UpdateWeater_en = 1;
        TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); //�¶�ͼ��
        TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       //ʪ��ͼ��
        // getCityWeater();
        // digitalClockDisplay(1);
        // scrollBanner();
        // #if DHT_EN
        //   if(DHT_img_flag == 1)
        //   IndoorTem();
        // #endif
        // #if imgAst_EN
        //   if(DHT_img_flag == 0)
        //   imgAnim();
        // #endif

        Serial.print("��Ļ��������Ϊ��");
        Serial.println(RoSet);
      }
      else
      {
        Serial.println("��Ļ����ֵ����������0-3�ڵ�ֵ");
      }
    }
    if (SMOD == "0x04") //������������ʱ��
    {
      int wtup = atoi(incomingByte.c_str()); // int n = atoi(xxx.c_str());//Stringתint
      if (wtup >= 1 && wtup <= 60)
      {
        updateweater_time = wtup;
        SMOD = "";
        Serial.printf("��������ʱ�����Ϊ��");
        Serial.print(updateweater_time);
        Serial.println("����");
      }
      else
        Serial.println("����ʱ��̫�������������ã�1-60��");
    }
    else
    {
      SMOD = incomingByte;
      delay(2);
      if (SMOD == "0x01")
        Serial.println("����������ֵ����Χ0-100");
      else if (SMOD == "0x02")
        Serial.println("������9λ���д��룬�Զ���ȡ������0");
      else if (SMOD == "0x03")
      {
        Serial.println("��������Ļ����ֵ��");
        Serial.println("0-USB�ӿڳ���");
        Serial.println("1-USB�ӿڳ���");
        Serial.println("2-USB�ӿڳ���");
        Serial.println("3-USB�ӿڳ���");
      }
      else if (SMOD == "0x04")
      {
        Serial.print("��ǰ��������ʱ�䣺");
        Serial.print(updateweater_time);
        Serial.println("����");
        Serial.println("��������������ʱ�䣨1-60������");
      }
      else if (SMOD == "0x05")
      {
        Serial.println("����WiFi������......");
        delay(10);
        wm.resetSettings();
        deletewificonfig();
        delay(10);
        Serial.println("����WiFi�ɹ�");
        SMOD = "";
        ESP.restart();
      }
      else
      {
        Serial.println("");
        Serial.println("��������Ҫ�޸ĵĴ��룺");
        Serial.println("������������        0x01");
        Serial.println("��ַ��������        0x02");
        Serial.println("��Ļ������������    0x03");
        Serial.println("������������ʱ��    0x04");
        Serial.println("����WiFi(������)    0x05");
        Serial.println("");
      }
    }
  }
}

#if WM_EN
// WEB����LCD��ʾ����
void Web_win()
{
  clk.setColorDepth(8);

  clk.createSprite(200, 60); //��������
  clk.fillSprite(0x0000);    //�����

  clk.setTextDatum(CC_DATUM); //�����ı�����
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("WiFi Connect Fail!", 100, 10, 2);
  clk.drawString("SSID:", 45, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawString("AutoConnectAP", 125, 40, 2);
  clk.pushSprite(20, 50); //����λ��

  clk.deleteSprite();
}

// WEB��������
void Webconfig()
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  delay(3000);
  wm.resetSettings(); // wipe settings

  // add a custom input field
  int customFieldLength = 40;

  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\"");

  // test custom html input type(checkbox)
  //  new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type

  // test custom html(radio)
  // const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  // new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input

  const char *set_rotation = "<br/><label for='set_rotation'>��ʾ��������</label>\
                              <input type='radio' name='set_rotation' value='0' checked> USB�ӿڳ���<br>\
                              <input type='radio' name='set_rotation' value='1'> USB�ӿڳ���<br>\
                              <input type='radio' name='set_rotation' value='2'> USB�ӿڳ���<br>\
                              <input type='radio' name='set_rotation' value='3'> USB�ӿڳ���<br>";
  WiFiManagerParameter custom_rot(set_rotation); // custom html input
  WiFiManagerParameter custom_bl("LCDBL", "��Ļ���ȣ�1-100��", "10", 3);
#if DHT_EN
  WiFiManagerParameter custom_DHT11_en("DHT11_en", "Enable DHT11 sensor", "0", 1);
#endif
  WiFiManagerParameter custom_weatertime("WeaterUpdateTime", "����ˢ��ʱ�䣨���ӣ�", "10", 3);
  WiFiManagerParameter custom_cc("CityCode", "���д���", "0", 9);
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

//����ҳ���л�ȡ�����ݱ���
#if DHT_EN
  DHT_img_flag = getParam("DHT11_en").toInt();
#endif
  updateweater_time = getParam("WeaterUpdateTime").toInt();
  cc = getParam("CityCode").toInt();
  LCD_Rotation = getParam("set_rotation").toInt();
  LCD_BL_PWM = getParam("LCDBL").toInt();

  //�Ի�ȡ�����ݽ��д���
  //���д���
  Serial.print("CityCode = ");
  Serial.println(cc);
  if (cc >= 101000000 && cc <= 102000000 || cc == 0)
  {
    for (int cnum = 0; cnum < 5; cnum++)
    {
      EEPROM.write(CC_addr + cnum, cc % 100); //���е�ַд����д���
      EEPROM.commit();                        //������ĵ�����
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
  //��Ļ����
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
  //��Ļ����
  Serial.printf("���ȵ���Ϊ��");
  analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM * 10));
  Serial.println(LCD_BL_PWM);
  //��������ʱ��
  Serial.printf("��������ʱ�����Ϊ��");
  Serial.println(updateweater_time);

#if DHT_EN
  //�Ƿ�ʹ��DHT11������
  Serial.printf("DHT11��������");
  EEPROM.write(DHT_addr, DHT_img_flag);
  EEPROM.commit(); //������ĵ�����
  Serial.println((DHT_img_flag ? "������" : "δ����"));
#endif
}
#endif

void setup()
{
  Button_sw1.setClickHandler(esp_reset);
  Button_sw1.setLongClickHandler(wifi_reset);
  Serial.begin(115200);
  EEPROM.begin(1024);
  // WiFi.forceSleepWake();
  // wm.resetSettings();    //�ڳ�ʼ����ʹwifi���ã�����������WiFi

#if DHT_EN
  dht.begin();
  //��eeprom��ȡDHT������ʹ�ܱ�־
  DHT_img_flag = EEPROM.read(DHT_addr);
#endif
  //��eeprom��ȡ������������
  if (EEPROM.read(BL_addr) > 0 && EEPROM.read(BL_addr) < 100)
    LCD_BL_PWM = EEPROM.read(BL_addr);
  //��eeprom��ȡ��Ļ��������
  if (EEPROM.read(Ro_addr) >= 0 && EEPROM.read(Ro_addr) <= 3)
    LCD_Rotation = EEPROM.read(Ro_addr);

  pinMode(LCD_BL_PIN, OUTPUT);
  analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM * 10));

  tft.begin();          /* TFT init */
  tft.invertDisplay(1); //��ת������ʾ��ɫ��1��ת��0����
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(0x0000);
  tft.setTextColor(TFT_BLACK, bgColor);

  targetTime = millis() + 1000;
  readwificonfig(); //��ȡ�洢��wifi��Ϣ
  Serial.print("��������WIFI ");
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
//ʹ��web�������Զ���smartconfig����ʧЧ
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
  while (loadNum < 194) //�ö�������
  {
    loading(1);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("SSID:");
    Serial.println(WiFi.SSID().c_str());
    Serial.print("PSW:");
    Serial.println(WiFi.psk().c_str());
    strcpy(wificonf.stassid, WiFi.SSID().c_str()); //���Ƹ���
    strcpy(wificonf.stapsw, WiFi.psk().c_str());   //���븴��
    savewificonfig();
    readwificonfig();
  }

  Serial.print("����IP�� ");
  Serial.println(WiFi.localIP());
  Serial.println("����UDP");
  Udp.begin(localPort);
  Serial.println("�ȴ�ͬ��...");
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
    getCityCode(); //��ȡ���д���

  tft.fillScreen(TFT_BLACK); //����

  TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); //�¶�ͼ��
  TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       //ʪ��ͼ��

  getCityWeater();
#if DHT_EN
  if (DHT_img_flag != 0)
    IndoorTem();
#endif

  WiFi.forceSleepBegin(); // wifi off
  Serial.println("WIFI����......");
  Wifi_en = 0;
}

void loop()
{
  LCD_reflash(0);
  Serial_set();
  Button_sw1.loop(); //��ť��ѯ
}

void LCD_reflash(int en)
{
  if (now() != prevDisplay || en == 1)
  {
    prevDisplay = now();
    digitalClockDisplay(en);
    prevTime = 0;
  }

  //�����Ӹ���һ��
  if (second() % 2 == 0 && prevTime == 0 || en == 1)
  {
#if DHT_EN
    if (DHT_img_flag != 0)
      IndoorTem();
#endif
    scrollBanner();
  }
#if imgAst_EN
  if (DHT_img_flag == 0)
    imgAnim();
#endif

  if (millis() - weaterTime > (60000 * updateweater_time) || en == 1 || UpdateWeater_en != 0)
  { // 10���Ӹ���һ������
    if (Wifi_en == 0)
    {
      WiFi.forceSleepWake(); // wifi on
      Serial.println("WIFI�ָ�......");
      Wifi_en = 1;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("WIFI������");
      getCityWeater();
      if (UpdateWeater_en != 0)
        UpdateWeater_en = 0;
      weaterTime = millis();
      while (!getNtpTime())
        ;
      WiFi.forceSleepBegin(); // Wifi Off
      Serial.println("WIFI����......");
      Wifi_en = 0;
    }
  }
}

// ����HTTP�����ҽ���������Ӧͨ���������
void getCityCode()
{
  String URL = "http://wgeo.weather.com.cn/ip/?_=" + String(now());
  //���� HTTPClient ����
  HTTPClient httpClient;

  //���������ַ���˴�Ҳ���Բ�ʹ�ö˿ںź�PATH��������
  httpClient.begin(wificlient, URL);

  //��������ͷ�е�User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");

  //�������Ӳ�����HTTP����
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);

  //�����������ӦOK��ӷ�������ȡ��Ӧ����Ϣ��ͨ���������
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
      Serial.println("��ȡ���д���ʧ��");
    }
  }
  else
  {
    Serial.println("������д������");
    Serial.println(httpCode);
  }

  //�ر�ESP8266�����������
  httpClient.end();
}

// ��ȡ��������
void getCityWeater()
{
  // String URL = "http://d1.weather.com.cn/dingzhi/" + cityCode + ".html?_="+String(now());//��
  String URL = "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_=" + String(now()); //ԭ��
  //���� HTTPClient ����
  HTTPClient httpClient;

  //  WiFiClient client; //�����
  //  httpClient.begin(client, URL);

  httpClient.begin(URL);

  //��������ͷ�е�User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");

  //�������Ӳ�����HTTP����
  int httpCode = httpClient.GET();
  Serial.println("���ڻ�ȡ��������");
  Serial.println(URL);

  //�����������ӦOK��ӷ�������ȡ��Ӧ����Ϣ��ͨ���������
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
    Serial.println("��ȡ�ɹ�");
  }
  else
  {
    Serial.println("���������������");
    Serial.print(httpCode);
  }

  //�ر�ESP8266�����������
  httpClient.end();
}

String scrollText[7];
// int scrollTextWidth = 0;
//������Ϣд����Ļ��
void weaterData(String *cityDZ, String *dataSK, String *dataFC)
{
  //������һ��JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, *dataSK);
  JsonObject sk = doc.as<JsonObject>();

  // TFT_eSprite clkb = TFT_eSprite(&tft);

  /***�����������***/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  //�¶�
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["temp"].as<String>() + "��", 28, 13);
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

  //ʪ��
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

  //��������
  clk.createSprite(94, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["cityname"].as<String>(), 44, 16);
  clk.pushSprite(15, 15);
  clk.deleteSprite();

  // PM2.5����ָ��
  uint16_t pm25BgColor = tft.color565(156, 202, 127); //��
  String aqiTxt = "��";
  int pm25V = sk["aqi"];
  if (pm25V > 200)
  {
    pm25BgColor = tft.color565(136, 11, 32); //�ض�
    aqiTxt = "�ض�";
  }
  else if (pm25V > 150)
  {
    pm25BgColor = tft.color565(186, 55, 121); //�ж�
    aqiTxt = "�ж�";
  }
  else if (pm25V > 100)
  {
    pm25BgColor = tft.color565(242, 159, 57); //��
    aqiTxt = "���";
  }
  else if (pm25V > 50)
  {
    pm25BgColor = tft.color565(247, 219, 100); //��
    aqiTxt = "��";
  }
  clk.createSprite(56, 24);
  clk.fillSprite(bgColor);
  clk.fillRoundRect(0, 0, 50, 24, 4, pm25BgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(0x0000);
  clk.drawString(aqiTxt, 25, 13);
  clk.pushSprite(104, 18);
  clk.deleteSprite();

  scrollText[0] = "ʵʱ���� " + sk["weather"].as<String>();
  scrollText[1] = "�������� " + aqiTxt;
  scrollText[2] = "���� " + sk["WD"].as<String>() + sk["WS"].as<String>();

  // scrollText[6] = atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()) ;

  //����ͼ��
  wrat.printfweather(170, 15, atoi((sk["weathercode"].as<String>()).substring(1, 3).c_str()));

  //���Ͻǹ�����Ļ
  //�����ڶ���JSON
  deserializeJson(doc, *cityDZ);
  JsonObject dz = doc.as<JsonObject>();
  // Serial.println(sk["ws"].as<String>());
  //���������ʽ
  // String aa = "��������:" + dz["weather"].as<String>() + "���¶�:���" + dz["tempn"].as<String>() + "�����" + dz["temp"].as<String>() + " ��������:" + aqiTxt + "������:" + dz["wd"].as<String>() + dz["ws"].as<String>();
  // scrollTextWidth = clk.textWidth(scrollText);
  // Serial.println(aa);
  scrollText[3] = "����" + dz["weather"].as<String>();

  deserializeJson(doc, *dataFC);
  JsonObject fc = doc.as<JsonObject>();

  scrollText[4] = "����¶�" + fc["fd"].as<String>() + "��";
  scrollText[5] = "����¶�" + fc["fc"].as<String>() + "��";

  // Serial.println(scrollText[0]);

  clk.unloadFont();
}

int currentIndex = 0;
TFT_eSprite clkb = TFT_eSprite(&tft);

void scrollBanner()
{
  // if(millis() - prevTime > 2333) //3���л�һ��
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
      currentIndex = 0; //�ص�һ��
    else
      currentIndex += 1; //׼���л�����һ��
  }
  prevTime = 1;
  //  }
}

#if imgAst_EN
void imgAnim()
{
  int x = 160, y = 160;
  if (millis() - AprevTime > 37) // x ms�л�һ��
  {
    Anim++;
    AprevTime = millis();
  }
  if (Anim == 10)
    Anim = 0;

  switch (Anim)
  {
  case 0:
    TJpgDec.drawJpg(x, y, i0, sizeof(i0));
    break;
  case 1:
    TJpgDec.drawJpg(x, y, i1, sizeof(i1));
    break;
  case 2:
    TJpgDec.drawJpg(x, y, i2, sizeof(i2));
    break;
  case 3:
    TJpgDec.drawJpg(x, y, i3, sizeof(i3));
    break;
  case 4:
    TJpgDec.drawJpg(x, y, i4, sizeof(i4));
    break;
  case 5:
    TJpgDec.drawJpg(x, y, i5, sizeof(i5));
    break;
  case 6:
    TJpgDec.drawJpg(x, y, i6, sizeof(i6));
    break;
  case 7:
    TJpgDec.drawJpg(x, y, i7, sizeof(i7));
    break;
  case 8:
    TJpgDec.drawJpg(x, y, i8, sizeof(i8));
    break;
  case 9:
    TJpgDec.drawJpg(x, y, i9, sizeof(i9));
    break;
  default:
    Serial.println("��ʾAnim����");
    break;
  }
}
#endif

unsigned char Hour_sign = 60;
unsigned char Minute_sign = 60;
unsigned char Second_sign = 60;
void digitalClockDisplay(int reflash_en)
{
  int timey = 82;
  if (hour() != Hour_sign || reflash_en == 1) //ʱ��ˢ��
  {
    dig.printfW3660(20, timey, hour() / 10);
    dig.printfW3660(60, timey, hour() % 10);
    Hour_sign = hour();
  }
  if (minute() != Minute_sign || reflash_en == 1) //����ˢ��
  {
    dig.printfO3660(101, timey, minute() / 10);
    dig.printfO3660(141, timey, minute() % 10);
    Minute_sign = minute();
  }
  if (second() != Second_sign || reflash_en == 1) //����ˢ��
  {
    dig.printfW1830(182, timey + 30, second() / 10);
    dig.printfW1830(202, timey + 30, second() % 10);
    Second_sign = second();
  }

  if (reflash_en == 1)
    reflash_en = 0;
  /***����****/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  //����
  clk.createSprite(58, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(week(), 29, 16);
  clk.pushSprite(102, 150);
  clk.deleteSprite();

  //����
  clk.createSprite(95, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(monthDay(), 49, 16);
  clk.pushSprite(5, 150);
  clk.deleteSprite();

  clk.unloadFont();
  /***����****/
}

//����
String week()
{
  String wk[7] = {"��", "һ", "��", "��", "��", "��", "��"};
  String s = "��" + wk[weekday() - 1];
  return s;
}

//����
String monthDay()
{
  String s = String(month());
  s = s + "��" + day() + "��";
  return s;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;     // NTPʱ������Ϣ��ǰ48�ֽ���
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
  return 0; // �޷���ȡʱ��ʱ����0
}

// ��NTP��������������
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
  Serial.println("����WiFi�ɹ�");
  ESP.restart();
}
