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


const char ssid[] = "*****";
const char pass[] = "*****";
const char time_zone[] = "JST-9";

void loopCore1(void *pvParameters);
void sntpCallBack(struct timeval *tv);
char getDigitData(int digit);
void IRAM_ATTR onTimer();
int getPWMFrequencyForBrightness();

hw_timer_t *interrupt_timer = NULL;
DS3232RTC myRTC;
bool is_interrupt = false;
bool is_bright = true;


void setup() {
  Serial.begin(115200);
  
  pinMode(HC595_LATCH_PIN, OUTPUT);
  pinMode(HC595_CLOCK_PIN, OUTPUT);
  pinMode(HC595_DATA_PIN, OUTPUT);
  pinMode(POTENTIONMETER_PIN, INPUT);
  pinMode(CDS_PIN, INPUT);

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

  //WiFi接続処理
  Serial.print("WiFi connecting");
  WiFi.begin(ssid, pass);
  for(int i = 0; i < 10; i++) {
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("Connected!");
      break;
    }else {
      Serial.print(".");
      delay(500);
    }
  }
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.print("ERROR: Failed to connect to ");
    Serial.println(ssid);
  }

  //外部RTCから時刻を読み出し内部RTCにセット(NTPが使えない場合を想定)
  myRTC.begin();
  setSyncProvider(myRTC.get);
  if(timeStatus() != timeSet) {
      Serial.println("ERROR: Unable to sync with the RTC");
  }else {
      Serial.println("RTC has set the system time");
  }

  //NTPで時刻を取得
  configTzTime(time_zone, "ntp.nict.jp", "time.google.com", "time.aws.com");
  sntp_set_time_sync_notification_cb(sntpCallBack);  //NTP同期された時に呼び出す関数を指定

  //タイマー割り込み
  interrupt_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(interrupt_timer, &onTimer, true);
  timerAlarmWrite(interrupt_timer, 8000000, true);  //8秒
  timerAlarmEnable(interrupt_timer);

  xTaskCreatePinnedToCore(loopCore1, "loopCore1", 4096, NULL, 1, NULL, 1);  //core1で関数を開始
}

void loop() {
  if(is_interrupt) {
    is_interrupt = false;
    is_bright = (analogRead(CDS_PIN) > 1200);
  }
}

//ダイナミック点灯処理
void loopCore1(void *pvParameters) {
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

//NTPで時刻の取得に成功したらNTP->内部RTC->外部RTCの順にセットします
void sntpCallBack(struct timeval *tv) {
  if(sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    struct tm time;
    getLocalTime(&time);
    setTime(time.tm_hour, time.tm_min, time.tm_sec, time.tm_mday, time.tm_mon + 1, time.tm_year + 1900);
    myRTC.set(now());
    Serial.println("RTC and SystemRTC has been synchronized with NTP");
  }else {
    Serial.println("ERROR: NTP process was failed");
  }
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