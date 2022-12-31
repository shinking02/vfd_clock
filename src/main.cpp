#include <Arduino.h>
#include <WiFi.h>
#include <sntp.h>
#include <DS3232RTC.h>  //https://github.com/JChristensen/DS3232RTC
#define NUMBER_OF_LED_DIGITS 6
#define HC595_LATCH_PIN 5
#define HC595_CLOCK_PIN 18
#define HC595_DATA_PIN 19

const char ssid[] = "*****";
const char pass[] = "*****";
const char time_zone[] = "JST-9";
void loopCore0(void *pvParameters);
void sntpCallBack(struct timeval *tv);
void getTimeArray(int time[6]);

hw_timer_t *interrupt_timer = NULL;
DS3232RTC myRTC;
bool is_interrupt = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(HC595_LATCH_PIN, OUTPUT);
  pinMode(HC595_CLOCK_PIN, OUTPUT);
  pinMode(HC595_DATA_PIN, OUTPUT);

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
    Serial.print("ERROR: Failed to connect to");
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

  xTaskCreatePinnedToCore(loopCore0, "loopCore0", 4096, NULL, 1, NULL, 0);  //core0で関数を開始
}

void loop() {
}

//ダイナミック点灯処理
void loopCore0(void *pvParameters) {
  while(1){
    const unsigned char segment_patterns[] = {0xfc, 0x60, 0xda, 0xf2, 0x66, 0xb6, 0xbe, 0xe4, 0xfe, 0xf6, 0xee, 0x3e, 0x9c, 0x7a, 0x9e, 0x8e};
    int time[6];
    getTimeArray(time);
    for(int i = 0; i < NUMBER_OF_LED_DIGITS; i++) {
      shiftOut(HC595_DATA_PIN, HC595_CLOCK_PIN, LSBFIRST, segment_patterns[time[i]]);
      digitalWrite(HC595_LATCH_PIN, HIGH);
      digitalWrite(HC595_LATCH_PIN, LOW);
      ledcWrite(i, 255);
      delay(1);
      ledcWrite(i, 0);
      delay(1);  //ゴースト対策(仮)
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

//LEDのdigit番目に表示する数字をセットします
void getTimeArray(int time[6]) {
  time[0] = (hour() / 10) % 10;
  time[1] = hour() % 10;
  time[2] = (minute() / 10) % 10;
  time[3] = minute() % 10;
  time[4] = (second() / 10) % 10;
  time[5] = second() % 10;
}