
/*
CN4
1 VCC   +5V
2 VEE   GND
3 SCLK    pin 2
4 SOUT1   pin 3
5 LATCH1  pin 4
6 ENABLE1 pin 5
7 ENABLE2 pin 6
8 CNT   pin 7
 */
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "dotpattern.h"
#include "html.h"

//---------------------- for ssid
Preferences preferences;

#define SOFT_AP_SSID     "ESP32 DOT_CLOCK"
#define SOFT_AP_PASSWORD "kingPul#777"
WebServer server(80);

//---------------------- for dot
#define SCLK       4
#define SOUT      16
#define LATCH     17
#define ENABLE1    5
#define ENABLE2   18
#define CNT       19

//---------------------- for input
#define IN_RIGHT  23
#define IN_LEFT   22
#define IN_BTNA   21
#define IN_BTNB    0

//---------------------- for Time
struct tm timeInfo;

byte old;
byte level;
byte edge;

byte g_count;
byte g_pattern;
byte g_repeat;
DOTPATTERN *g_dotPtr;
DOTPATTERN *g_dot;
byte g_backBuffer[8 * 7];
byte g_drawTime;
int  g_mode;

char g_ssid[33];
char g_key[65];
char header[16] = "DOTCLOCK SSID  ";


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

int ReadSSID()
{
  static char buff[65];
  preferences.begin("Wifi:setting", false);
  preferences.getString("head", buff, 33);
  if (strcmp(header, buff) != 0)
  {
    preferences.end();
    return -1;
  }
  preferences.getString("ssid", buff, 33);
  memcpy(g_ssid, buff, 33);
  preferences.getString("key",  buff, 65);
  memcpy(g_key, buff, 65);
  preferences.end();
  return 0;
}


void WriteSSID()
{
  preferences.begin("Wifi:setting", false);
  preferences.putString("ssid", g_ssid);
  preferences.putString("key",  g_key);
  preferences.putString("head", header);
  preferences.end();
}

void ResetSSID()
{
  preferences.begin("Wifi:setting", false);
  preferences.putString("ssid", "");
  preferences.putString("key",  "");
  preferences.putString("head", "XXX");
  preferences.end();
}


void SetSSID(char *ssid)
{
    memset(g_ssid, 0x00, 33);
    int len = strlen(ssid);
    if (len > 32)
    {
        len = 32;
    }
    memcpy(g_ssid, ssid, len);
}

void SetKey(char *key)
{
    memset(g_key, 0x00, 65);
    int len = strlen(key);
    if (len > 64)
    {
        len = 64;
    }
    memcpy(g_key, key, len);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void handleRoot()
{
  server.send( 200, "text/html", html);
}

void wifiset()
{
  String setSsid = server.arg("SSID");
  String setKey  = server.arg("KEY");

  char buff[65];
  setSsid.toCharArray(buff, 65);
  Serial.println(buff);
  SetSSID(buff);

  setKey.toCharArray(buff, 65);
  Serial.println(buff);
  SetKey(buff);
  WriteSSID();
  
  server.send( 200, "text/html", postResp);
}

void wifiReset()
{
  ResetSSID();
  server.send( 200, "text/html", postResp);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void GetTimeFromNTP()
{
  Serial.println("Connecting to the WiFi network....");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (WiFi.begin(g_ssid, g_key) != WL_DISCONNECTED) {
    return;
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  Serial.println("Connected to the WiFi network!");
  configTime(
    9 * 3600L,
    0, 
    "ntp.nict.jp",
    "time.google.com",
    "ntp.jst.mfeed.ad.jp");//NTPの設定
 
}

//-----------------------------------------------------------
//-----------------------------------------------------------
void InitGPIO()
{
  pinMode(SCLK,   OUTPUT);
  pinMode(SOUT,   OUTPUT);
  pinMode(LATCH,  OUTPUT);
  pinMode(ENABLE1,OUTPUT);
  pinMode(ENABLE2,OUTPUT);
  pinMode(CNT,    OUTPUT);
  
  digitalWrite(CNT,     HIGH);
  digitalWrite(ENABLE1, HIGH);
  digitalWrite(ENABLE2, HIGH);
  digitalWrite(LATCH,   LOW);
  digitalWrite(SCLK,    HIGH);

  pinMode(IN_RIGHT, INPUT_PULLUP);
  pinMode(IN_LEFT,  INPUT_PULLUP);
  pinMode(IN_BTNA,  INPUT_PULLUP);
  pinMode(IN_BTNB,  INPUT_PULLUP);
}


void xShiftOut(byte dat)
{
  byte bitx = 0x01;
  for (int i = 0; i < 8; i++)
  {
    digitalWrite(SCLK, HIGH);
    digitalWrite(SOUT, (dat & bitx) != 0 ? HIGH : LOW);
    bitx <<= 1;
    digitalWrite(SCLK, LOW);
  }
}


void xShiftOut16(word dat)
{
  word bitx = 0x0001;
  for (int i = 0; i < 16; i++)
  {
    digitalWrite(SCLK, HIGH);
    digitalWrite(SOUT, (dat & bitx) != 0 ? HIGH : LOW);
    bitx <<= 1;
    digitalWrite(SCLK, LOW);
  }
}


void digitOut(byte digit, byte *buff)
{
  buff += 3;
  if ((digit & 0x01) == 0)
  {
    buff += 4;
  }
  digitalWrite(ENABLE1, HIGH);
  digitalWrite(ENABLE2, HIGH);

  xShiftOut(*buff);
  buff--;
  xShiftOut(*buff);
  buff--;
  xShiftOut(*buff);
  buff--;
  xShiftOut(*buff);

  xShiftOut16(digitBit[digit]);
  //xShiftOut(digitBit[digit * 2]);
  //xShiftOut(digitBit[digit * 2 + 1]);

  digitalWrite(CNT,     LOW);

  digitalWrite(LATCH,   HIGH);
  digitalWrite(LATCH,   LOW);

  digitalWrite(ENABLE1, LOW);
  digitalWrite(ENABLE2, LOW);
}



byte getInput()
{
  byte dat = 0;
  dat  = (digitalRead(IN_RIGHT) == LOW ? 0x01 : 0x00);
  dat |= (digitalRead(IN_LEFT)  == LOW ? 0x02 : 0x00);
  dat |= (digitalRead(IN_BTNA)  == LOW ? 0x04 : 0x00);
  dat |= (digitalRead(IN_BTNB)  == LOW ? 0x08 : 0x00);
  return dat;
}

void clearEdge(byte b)
{
  edge &= ~b;
}

byte getEdge()
{
  level = getInput();
  edge |= level & (level ^ old);
  old = level;
  return edge;
}



void SetMode()
{
  switch (g_mode)
  {
  case 0x00:
    g_drawTime = 1;
    g_dot = dotPattern2;
    break;

  case 0x01:
    g_drawTime = 1;
    g_dot = dotPattern3;
    break;

  case 0x02:
    g_drawTime = 1;
    g_dot = dotPattern4;
    break;

  case 0x03:
    g_drawTime = 1;
    g_dot = dotPattern5;
    break;

  case 0x04:
    g_drawTime = 1;
    g_dot = dotPattern6;
    break;

  case 0x05:
    g_drawTime = 0;
    g_dot = dotPattern;
    break;

  }
  g_dotPtr = g_dot;
  g_count = 0;
  g_repeat = 0;
  g_pattern = g_dotPtr->startNo;
}

void SetModeX(int mode)
{
  g_mode = mode;
  SetMode();
}

void InitDemo()
{
  memset(g_backBuffer, 0x00, sizeof(g_backBuffer));
  g_count = 0;
  g_repeat = 0;
  g_mode = 0;
  SetMode();
}

void drawAscii(int x, uint8_t code)
{
  if (code < 0x20)
  {
    code = 0x20;
  }
  uint8_t *buf = &ascii[(code - 0x20) * 7];
  if (x <= -8 || x > 63)
  {
    return;
  }

  if (x < 0)
  {
    for (int i = 0; i < 7; i++)
    {
      uint8_t dat = *buf;
      dat <<= -x;
      g_backBuffer[i * 8] |= dat;
      buf++;
    }

  }
  else if (x > 64 - 8)
  {
      for (int i = 0; i < 7; i++)
      {
        uint8_t dat = *buf;
        dat >>= (x & 7);
        g_backBuffer[7 + i * 8] |= dat;
        buf++;
      }
  } else {
    for (int i = 0; i < 7; i++)
    {
      uint8_t lo = *buf;
      uint8_t hi = *buf;
      lo >>= (x & 0x7);
      hi <<= 8 - (x & 0x7);
      g_backBuffer[(x / 8) + i * 8]     |= lo;
      g_backBuffer[(x / 8) + i * 8 + 1] |= hi;

      buf++;
    }
  }
}

void drawTime()
{
  getLocalTime(&timeInfo);
  int h = timeInfo.tm_hour;
  int m = timeInfo.tm_min;
  drawAscii(17, (h / 10) + '0');
  drawAscii(17 + 7, (h % 10) + '0');
  drawAscii(17 + 7 + 5, ':');
  drawAscii(34, (m / 10) + '0');
  drawAscii(34 + 7, (m % 10) + '0');
}


void drawString(int x, char *str)
{
  char *p = str;
  while (*p)
  {
    drawAscii(x, *p);
    p++;
    x += 8;
  }

}


void updateScreen()
{
  byte *addr = g_backBuffer;

  byte digit = 0;
  for (int i = 0; i < 14; i++)
  {
    delayMicroseconds(1500);
    digitOut(digit, addr);
    digit++;
    if ((digit & 0x01) == 0x00)
    {
      addr += 8;
    }
  }
}

void Animation()
{
  g_count++;
  if (g_count > g_dotPtr->timer)
  {
    g_count = 0;
    g_pattern++;
    if (g_pattern > g_dotPtr->endNo)
    {
      g_repeat++;
      if (g_repeat >= g_dotPtr->repeat)
      {
        g_repeat = 0;
        g_dotPtr++;
        if (g_dotPtr->timer == -1)
        {
          g_dotPtr = g_dot;
        }
      }
      g_pattern = g_dotPtr->startNo;
    }
  }

  //byte *addr = &demo[g_pattern * 8 * 7];

  memcpy(g_backBuffer, &demo[g_pattern * 8 * 7], 8 * 7);
  if (g_drawTime == 1)
  {
    drawTime();
  }
  updateScreen();
}


void normalUpdate()
{
  Animation();

  byte b = getEdge();
  if ((b & 0x08) != 0){
    clearEdge(0x08);
    g_mode = (g_mode + 1) % 6;
    SetMode();

  }
}

void apmodeUpdate()
{
  static int x = 8 * 8;
  memset(g_backBuffer, 0x00, sizeof(g_backBuffer));
  drawString(x, "[192.168.4.1]");

  x--;
  if (x < -12 * 8)
  {
    x = 8 * 8;
  }
  updateScreen();
}


void demoUpdate()
{
  Animation();
}


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

/*
// AP MODE task
void apmode_task(void *pvParameter)
{
  while (1)  
  {
    normalUpdate();
  }
}
*/


void setup() {
  Serial.begin(115200);
  InitGPIO();
  InitDemo();

  if ((getInput() & 0x02) != 0)
  {
    SetModeX(5);
    while (1)
    {
      demoUpdate();
    }
    return;
  } else if ((getInput() & 0x01) != 0)
  {
    
    WiFi.softAP(SOFT_AP_SSID, SOFT_AP_PASSWORD);
    IPAddress myIP = WiFi.softAPIP();
    
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.on("/",        handleRoot);
    server.on("/wifiset", wifiset);
    server.on("/reset",   wifiReset);
    server.begin();
    
    while(1){
      apmodeUpdate();
      server.handleClient();
    }
    return;
  }

  int a = ReadSSID();
  if (a == -1)
  {
    Serial.println("SSID is not set");
  } else {
    Serial.println(g_ssid);
    Serial.println(g_key);
    
    GetTimeFromNTP();
    
    getLocalTime(&timeInfo);
    int h = timeInfo.tm_hour;
    int m = timeInfo.tm_min;
    int s = timeInfo.tm_sec;
    Serial.print(h);
    Serial.print(":");
    Serial.print(m);
    Serial.print(":");
    Serial.println(s);
    WiFi.disconnect(); 
  }

 /*
  xTaskCreatePinnedToCore(
    apmode_task,
    "apmode_task",
    8192,
    NULL,
    12,
    NULL,
    1);
    */
}

void loop() 
{
  normalUpdate();
}
