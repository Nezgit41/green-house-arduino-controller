/*
██████╗░██╗░░░██╗  ███╗░░██╗███████╗███████╗░██████╗░██╗████████╗
██╔══██╗╚██╗░██╔╝  ████╗░██║██╔════╝╚════██║██╔════╝░██║╚══██╔══╝
██████╦╝░╚████╔╝░  ██╔██╗██║█████╗░░░░███╔═╝██║░░██╗░██║░░░██║░░░
██╔══██╗░░╚██╔╝░░  ██║╚████║██╔══╝░░██╔══╝░░██║░░╚██╗██║░░░██║░░░
██████╦╝░░░██║░░░  ██║░╚███║███████╗███████╗╚██████╔╝██║░░░██║░░░
╚═════╝░░░░╚═╝░░░  ╚═╝░░╚══╝╚══════╝╚══════╝░╚═════╝░╚═╝░░░╚═╝░░░
last update 18.05.2025 15:30
*/

// ----------------- Библиотеки -----------------
#include <Arduino.h>
#include <Wire.h>
#include <TroykaRTC.h>
#include "DHT.h"
#include <SD.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "GyverWDT.h"

// ----------------- Определение пинов -----------------
#define DHTPIN       9
#define DHTTYPE      DHT11
#define CS_PIN       10
#define RELAY_PUMP   5
#define RELAY_LAMP   6
#define RELAY_FAN    2

const unsigned long WATER_PORTION_TIME = 500;   // длительность полива (мс)
const unsigned long LOOP_INTERVAL      = 2000;  // интервал цикла (мс)
const int NUM_READINGS = 3;

enum Mode { RUN_MODE, CONFIG_MODE, MANUAL_MODE };
Mode currentMode = RUN_MODE;

// ----------------- Класс настроек -----------------
class Settings {
public:
  int lightThreshold    = 600;
  int moistureThreshold = 650;
  int moistureUpper     = 690;
  float tempUpper       = 32.0;
  float tempLower       = 30.0;

  bool load() {
    File f = SD.open("setting.txt");
    if (!f) return false;
    lightThreshold    = f.parseInt();
    moistureThreshold = f.parseInt();
    moistureUpper     = f.parseInt();
    tempUpper         = f.parseFloat();
    tempLower         = f.parseFloat();
    f.close();
    return true;
  }

  bool save() {
    SD.remove("setting.txt");
    File f = SD.open("setting.txt", FILE_WRITE);
    if (!f) return false;
    f.println(lightThreshold);
    f.println(moistureThreshold);
    f.println(moistureUpper);
    f.println(tempUpper);
    f.println(tempLower);
    f.close();
    return true;
  }
};

// ----------------- Сенсоры -----------------
class MoistureSensor {
  int readings[NUM_READINGS];
  int index;
  long total;
  int pin;
public:
  MoistureSensor(int p): pin(p), index(0), total(0) {}
  void begin() { pinMode(pin, INPUT); }
  void calibrate() {
    total = 0;
    for (int i = 0; i < NUM_READINGS; i++) {
      readings[i] = analogRead(pin);
      total += readings[i];
      delay(50);
    }
    index = 0;
  }
  int read() {
    total -= readings[index];
    readings[index] = analogRead(pin);
    total += readings[index];
    index = (index + 1) % NUM_READINGS;
    return total / NUM_READINGS;
  }
};

class LightSensor {
  int pin;
public:
  LightSensor(int p): pin(p) { pinMode(pin, INPUT); }
  int read() { return analogRead(pin); }
};

class TempSensor {
  DHT dht;
public:
  TempSensor(uint8_t pin, uint8_t type): dht(pin, type) {}
  void begin() { dht.begin(); }
  bool read(float &t, float &h) {
    t = dht.readTemperature();
    h = dht.readHumidity();
    return !(isnan(t) || isnan(h));
  }
};

// ----------------- Актюаторы -----------------
class Actuator {
  int pin;
  bool activeLow;
public:
  Actuator(int p, bool al = true): pin(p), activeLow(al) {}
  void begin(bool offState = false) {
    pinMode(pin, OUTPUT);
    write(offState);
  }
  void write(bool on) {
    digitalWrite(pin, activeLow ? !on : on);
  }
};

// ----------------- RTC -----------------
class RTCManager {
  RTC clock;
  static const int LEN_TIME = 20, LEN_DATE = 20;
  char timeStr[LEN_TIME];
  char dateStr[LEN_DATE];
public:
  void begin() {
    clock.begin();
    // при первом запуске раскомментировать:
    // clock.set(2025,5,18,10,44,0);
  }
  void update() { clock.read(); }
  void getTimeStamp() {
    snprintf(timeStr, LEN_TIME, "%02d:%02d:%02d",
             clock.getHour(), clock.getMinute(), clock.getSecond());
    int year = clock.getYear();
    snprintf(dateStr, LEN_DATE, "%02d.%02d.%04d",
             clock.getDay(), clock.getMonth(), year);
  }
  const char* time() const { return timeStr; }
  const char* date() const { return dateStr; }
};

// ----------------- Логгер -----------------
class Logger {
public:
  void logSD(const RTCManager &rtc,
             float t, float h,
             int m, int l,
             bool fan, bool lamp, bool pump) {
    File f = SD.open("log.txt", FILE_WRITE);
    if (!f) return;
    f.print("date=");     f.print(rtc.date());
    f.print(" time=");    f.println(rtc.time());
    f.print("temp=");     f.println(t);
    f.print("humidity="); f.println(h);
    f.print("moisture="); f.println(m);
    f.print("light=");    f.println(l);
    f.print("fan=");      f.println(fan);
    f.print("lamp=");     f.println(lamp);
    f.print("pump=");     f.println(pump);
    f.println();
    f.close();
  }
  void logSerial(const RTCManager &rtc,
                 float t, float h,
                 int m, int l,
                 bool fan, bool lamp, bool pump) {
    Serial.print("date: ");     Serial.print(rtc.date());
    Serial.print(" time: ");    Serial.println(rtc.time());
    Serial.print("temp: ");     Serial.println(t);
    Serial.print("humidity: "); Serial.println(h);
    Serial.print("moisture: "); Serial.println(m);
    Serial.print("light: ");    Serial.println(l);
    Serial.print("fan: ");      Serial.println(fan);
    Serial.print("lamp: ");     Serial.println(lamp);
    Serial.print("pump: ");     Serial.println(pump);
    Serial.println();
  }
};

// ----------------- Контроллер -----------------
class Controller {
  Settings       settings;
  MoistureSensor moisture{A0};
  LightSensor    light{A2};
  TempSensor     temp{DHTPIN, DHTTYPE};
  Actuator       pump{RELAY_PUMP}, lamp{RELAY_LAMP}, fan{RELAY_FAN};
  RTCManager     rtc;
  Logger         logger;

  unsigned long prevLoop  = 0;
  unsigned long pumpStart = 0;
  bool watering = false;

public:
  void setup() {
    Serial.begin(9600);
    while (!Serial);
    if (!SD.begin(CS_PIN)) {
      Serial.println("SD init error");
    } else if (!settings.load()) {
      Serial.println("Settings load error; using defaults");
    }
    Watchdog.enable(RESET_MODE, WDT_PRESCALER_1024);
    rtc.begin();
    temp.begin();
    moisture.begin();
    moisture.calibrate();
    lamp.begin(false);
    pump.begin(false);
    fan.begin(false);
    prevLoop = millis();
  }

  void handleSerial() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "config") {
      currentMode = CONFIG_MODE;
      Serial.println("CONFIG MODE");
      Serial.print("light=");      Serial.println(settings.lightThreshold);
      Serial.print("moisture=");   Serial.println(settings.moistureThreshold);
      Serial.print("moistureMax=");Serial.println(settings.moistureUpper);
      Serial.print("tempMax=");    Serial.println(settings.tempUpper,1);
      Serial.print("tempMin=");    Serial.println(settings.tempLower,1);
    }
    else if (cmd == "manual") {
      currentMode = MANUAL_MODE;
      Serial.println("MANUAL MODE");
    }
    else if (cmd == "run") {
      currentMode = RUN_MODE;
      Serial.println("RUN MODE");
    }
    else if (currentMode == CONFIG_MODE) {
      if (cmd.startsWith("light="))         settings.lightThreshold    = cmd.substring(6).toInt();
      else if (cmd.startsWith("moisture=")) settings.moistureThreshold = cmd.substring(9).toInt();
      else if (cmd.startsWith("moistureMax=")) settings.moistureUpper   = cmd.substring(12).toInt();
      else if (cmd.startsWith("tempMax="))     settings.tempUpper       = cmd.substring(8).toFloat();
      else if (cmd.startsWith("tempMin="))     settings.tempLower       = cmd.substring(8).toFloat();
      else if (cmd == "save") {
        if (settings.save()) {
          Serial.println("Settings saved");
          currentMode = RUN_MODE;
        } else Serial.println("Error saving");
      }
    }
    else if (currentMode == MANUAL_MODE) {
      if (cmd == "reboot") {
        Serial.println("Rebooting...");
        // триггерим WDT с минимальным таймаутом
        Watchdog.enable(RESET_MODE, WDT_PRESCALER_16);
        while (true);  // ждём перезагрузки
      }
      else if (cmd.startsWith("lamp=")) {
        lamp.write(cmd.substring(5).toInt() != 0);
      }
      else if (cmd.startsWith("fan=")) {
        fan.write(cmd.substring(4).toInt() != 0);
      }
      else if (cmd.startsWith("pump=")) {
        pump.write(cmd.substring(5).toInt() != 0);
      }
    }
  }

  void loop() {
    Watchdog.reset();
    handleSerial();

    unsigned long now = millis();
    if (now - prevLoop < LOOP_INTERVAL) return;
    prevLoop = now;

    rtc.update();
    rtc.getTimeStamp();

    float t, h;
    bool ok = temp.read(t, h);
    int m = moisture.read();
    int l = light.read();

    if (currentMode == RUN_MODE) {
      if (ok) {
        if (t >= settings.tempUpper) fan.write(true);
        else if (t <= settings.tempLower) fan.write(false);
      }
      if (!watering && m <= settings.moistureThreshold) {
        watering = true;
        pump.write(true);
        pumpStart = now;
      }
      if (watering && (now - pumpStart >= WATER_PORTION_TIME || m >= settings.moistureUpper)) {
        pump.write(false);
        watering = false;
      }
      lamp.write(l < settings.lightThreshold);
    }

    bool fanSt  = digitalRead(RELAY_FAN)  == LOW;
    bool lampSt = digitalRead(RELAY_LAMP) == LOW;
    bool pumpSt = digitalRead(RELAY_PUMP) == LOW;
    logger.logSD   (rtc, ok? t:-999, ok? h:-999, m, l, fanSt, lampSt, pumpSt);
    logger.logSerial(rtc, ok? t:-999, ok? h:-999, m, l, fanSt, lampSt, pumpSt);

    Watchdog.reset();
  }
};

Controller systemCtrl;
void setup() { systemCtrl.setup(); }
void loop()  { systemCtrl.loop(); }


