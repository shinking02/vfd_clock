#include <Arduino.h>
#include <WiFi.h>
#include <sntp.h>
#include <DS3232RTC.h>  //https://github.com/JChristensen/DS3232RTC
#define NUMBER_OF_LED_DIGITS 6
#define HC595_LATCH_PIN 5
#define HC595_CLOCK_PIN 18
#define HC595_DATA_PIN 19
#define POTENTIONMETER_PIN 35
#define CDS_PIN 34
#define SW1_PIN 16
#define SW2_PIN 4
#define LED1_PIN 27


const char ssid[] = "***REMOVED***";
const char pass[] = "***REMOVED***";
const char time_zone[] = "JST-9";

void core1DynamicLightingLoop(void *pvParameters);
void core1DynamicLightingLoopSetRTCMode(void *pvParameters);
void sntpCallBack(struct timeval *tv);
void checkStatus();
bool connectToWifi();
char getDigitData(int digit);
void IRAM_ATTR onTimer();
int getPWMFrequencyForBrightness();

hw_timer_t *interrupt_timer = NULL;
DS3232RTC myRTC;
bool is_interrupt = false;
bool is_bright = true;
int interrupts_count = 500;


void setup() {
  Serial.begin(115200);
  myRTC.begin();
  
  pinMode(HC595_LATCH_PIN, OUTPUT);
  pinMode(HC595_CLOCK_PIN, OUTPUT);
  pinMode(HC595_DATA_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(POTENTIONMETER_PIN, INPUT);
  pinMode(CDS_PIN, INPUT);
  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);

  const int LEDC_FREQ = 17800;
  const int LEDC_TIMERBIT = 8;
  ledcSetup(0, LEDC_FREQ, LEDC_TIMERBIT);
  ledcSetup(1, LEDC_FREQ, LEDC_TIMERBIT);
  ledcSetup(2, LEDC_FREQ, LEDC_TIMERBIT);
  ledcSetup(3, LEDC_FREQ, LEDC_TIMERBIT);
  ledcSetup(4, LEDC_FREQ, LEDC_TIMERBIT);
  ledcSetup(5, LEDC_FREQ, LEDC_TIMERBIT);

  ledcAttachPin(12, 0);
  ledcAttachPin(13, 1);
  ledcAttachPin(25, 2);
  ledcAttachPin(26, 3);
  ledcAttachPin(32, 4);
  ledcAttachPin(33, 5);

  connectToWifi();
  setSyncProvider(myRTC.get);

  //NTPで時刻を取得
  configTzTime(time_zone, "ntp.nict.jp", "time.google.com", "time.aws.com");
  sntp_set_time_sync_notification_cb(sntpCallBack);  //NTP同期された時に呼び出す関数を指定

  //タイマー割り込み
  interrupt_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(interrupt_timer, &onTimer, true);
  timerAlarmWrite(interrupt_timer, 6000000, true);  //6秒
  timerAlarmEnable(interrupt_timer);

  xTaskCreatePinnedToCore(core1DynamicLightingLoop, "core1DynamicLightingLoop", 4096, NULL, 1, NULL, 1);  //core1で関数を開始

  delay(2000);
  checkStatus();
}

void loop() {
  if(digitalRead(SW1_PIN) == LOW && digitalRead(SW2_PIN) == LOW) {
  }
  if(is_interrupt) {
    is_interrupt = false;
    checkStatus();
  }
  delay(80);
}

//ダイナミック点灯処理
void core1DynamicLightingLoop(void *pvParameters) {
  while(1){
    const unsigned char segment_patterns[] = {0xfc, 0x60, 0xda, 0xf2, 0x66, 0xb6, 0xbe, 0xe4, 0xfe, 0xf6, 0xee, 0x3e, 0x9c, 0x7a, 0x9e, 0x8e};
    int brightness = getPWMFrequencyForBrightness();
    for(int i = 0; i < NUMBER_OF_LED_DIGITS; i++) {
      shiftOut(HC595_DATA_PIN, HC595_CLOCK_PIN, LSBFIRST, segment_patterns[getDigitData(i)]);
      digitalWrite(HC595_LATCH_PIN, HIGH);
      digitalWrite(HC595_LATCH_PIN, LOW);
      ledcWrite(i, brightness);
      ets_delay_us(700);
      ledcWrite(i, 0);
      ets_delay_us(700);  //ゴースト対策
    }
  }
}

//NTPで時刻の取得に成功したら呼び出され、NTP->内部RTC->外部RTCの順にセットします
void sntpCallBack(struct timeval *tv) {
    struct tm time;
    getLocalTime(&time);
    setTime(time.tm_hour, time.tm_min, time.tm_sec, time.tm_mday, time.tm_mon + 1, time.tm_year + 1900);
    myRTC.set(now());
}

//LEDのdigit番目に表示する数字を返却します
char getDigitData(int digit) {
  char time = 0;
  switch(digit) {
    case 0:
      time = (hour() / 10) % 10;
      break;
    case 1:
      time = hour() % 10;
      break;
    case 2:
      time = (minute() / 10) % 10;
      break;
    case 3:
      time = minute() % 10;
      break;
    case 4:
      time = (second() / 10) % 10;
      break;
    case 5:
      time = second() % 10;
      break;
    default:
      break;
  }
  return time;
}

//タイマー割り込みで実行されフラグを立てます
void IRAM_ATTR onTimer() {
  is_interrupt = true;
}

//VFDの明るさを変えるPWM周波数を返却します
int getPWMFrequencyForBrightness() {
  int brigtness = map(analogRead(POTENTIONMETER_PIN), 0, 4095, 0, 255);
  if(!is_bright) {brigtness = brigtness * 0.4;}
  return brigtness;
}

//現在の状態を確認しそれに応じて動きます
void checkStatus() {
  interrupts_count++;
  is_bright = (analogRead(CDS_PIN) > 1200);
  digitalWrite(LED1_PIN, (WiFi.status() != WL_CONNECTED));
  if(interrupts_count == 500) {
    interrupts_count = 0;
    if(WiFi.status() != WL_CONNECTED) {
      if(connectToWifi()) {
        sntp_restart();
      }else {
        WiFi.disconnect();
        sntp_stop();
        setSyncProvider(myRTC.get);
      }
    }
  }
}

//WiFiの接続処理をし、結果を返却します
bool connectToWifi() {
  WiFi.begin(ssid, pass);
  for(int i = 0; i < 20; i++) {
    if(WiFi.status() == WL_CONNECTED) {
      return true;
    }else {
      delay(200);
    }
  }
  return false;
}