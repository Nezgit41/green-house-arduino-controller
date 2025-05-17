/*
██████╗░██╗░░░██╗  ███╗░░██╗███████╗███████╗░██████╗░██╗████████╗
██╔══██╗╚██╗░██╔╝  ████╗░██║██╔════╝╚════██║██╔════╝░██║╚══██╔══╝
██████╦╝░╚████╔╝░  ██╔██╗██║█████╗░░░░███╔═╝██║░░██╗░██║░░░██║░░░
██╔══██╗░░╚██╔╝░░  ██║╚████║██╔══╝░░██╔══╝░░██║░░╚██╗██║░░░██║░░░
██████╦╝░░░██║░░░  ██║░╚███║███████╗███████╗╚██████╔╝██║░░░██║░░░
╚═════╝░░░░╚═╝░░░  ╚═╝░░╚══╝╚══════╝╚══════╝░╚═════╝░╚═╝░░░╚═╝░░░
last update 17.05.2025 20:51
*/

#include "DHT.h" 
#include <SD.h>  
#include <SPI.h>
#include <Wire.h>
#include "TroykaRTC.h"
#include <Adafruit_Sensor.h>
#include "GyverWDT.h"

const int WATER_PORTION_TIME = 500; 
const int SENSOR_WAIT_TIME   = 3000;
const int numReadings = 3;
bool state_moister = false;
bool state_fan = false; 
bool state_light = false; 

int pinMoisture = A0;
int pinPhoto = A2;
int readings[numReadings];
int index = 0;
int total = 0;
int average = 0;

#define LEN_DOW 12
#define LEN_TIME 12
#define LEN_DATE 12
#define DHTTYPE DHT11
#define CS_PIN 10   
#define DHTPIN 9    
#define RELAY_PUMP 5 
#define RELAY_LAMP 6 
#define RELAY_FAN 2  

int LIGHT_DETECT = 0;
int MOISTURE_DETECT = 0;
int MOISTURE_UP = 0;
float TEMP_DETECT = 0;
float TEMP_DOWN = 0;

RTC clock;

char timeStamp[LEN_TIME];
char dateStamp[LEN_DATE];
char weekDay[LEN_DOW];

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600); // Используем аппаратный Serial для Bluetooth и отладки

  dht.begin();
  clock.begin();
  clock.set(__TIMESTAMP__);

  pinMode(RELAY_PUMP, OUTPUT); digitalWrite(RELAY_PUMP, LOW);
  pinMode(RELAY_LAMP, OUTPUT); digitalWrite(RELAY_LAMP, LOW);
  pinMode(RELAY_FAN, OUTPUT); digitalWrite(RELAY_FAN, HIGH);
  Watchdog.enable(RESET_MODE, WDT_PRESCALER_512);

  pinMode(CS_PIN, OUTPUT);
  if (!SD.begin(CS_PIN)) {
    Serial.println("Card Failure");
    return;
  }
  Serial.println("Card Ready");

  File settingsFile = SD.open("setting.txt");
  if (settingsFile) {
    Serial.println("Reading settings File");

    LIGHT_DETECT   = settingsFile.parseInt();
    MOISTURE_DETECT = settingsFile.parseInt();
    MOISTURE_UP = settingsFile.parseInt();
    TEMP_DETECT    = settingsFile.parseFloat();
    TEMP_DOWN      = settingsFile.parseFloat();
    Watchdog.reset();

    Serial.print("Light = "); Serial.println(LIGHT_DETECT);
    Serial.print("Moister = "); Serial.println(MOISTURE_DETECT);
    Serial.print("Moisture_up = "); Serial.println(MOISTURE_UP);
    Serial.print("Temp_Detect = "); Serial.println(TEMP_DETECT);
    Serial.print("Temp_Down = "); Serial.println(TEMP_DOWN);
    Watchdog.reset();
    settingsFile.close();
  } else {
    Serial.println("Could not read settings file.");
    return;
  }

  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  Watchdog.reset();
}

void loop() {
  delay(2000);
  Watchdog.reset();
  clock.read();
  clock.getTimeStamp(timeStamp, dateStamp, weekDay);

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int photo = analogRead(pinPhoto);

  total = total - readings[index];
  readings[index] = analogRead(pinMoisture);
  total = total + readings[index];
  index++;
  
  if (index >= numReadings) {
    index = 0;
    average = total / numReadings;
    delay(1);
  }

  if (t >= TEMP_DETECT) {
    digitalWrite(RELAY_FAN, LOW);
    state_fan = true;
    Watchdog.reset();
  } else if (t <= TEMP_DOWN) {
    digitalWrite(RELAY_FAN, HIGH);
    state_fan = false;
    Watchdog.reset();
  }

  /*if (average > MOISTURE_DETECT) {
    digitalWrite(RELAY_PUMP, HIGH);
    state_moister = false;
    Watchdog.reset();
  } else {
    digitalWrite(RELAY_PUMP, LOW);
    state_moister = true;
    Watchdog.reset();
  }*/

if (average <= MOISTURE_DETECT){
    while (average <= MOISTURE_UP){
        state_moister = true;
        digitalWrite(RELAY_PUMP, LOW);
        delay(WATER_PORTION_TIME);
        digitalWrite(RELAY_PUMP, HIGH);
        Watchdog.reset();
        Serial.println(" "); 
        Serial.print("date: "); Serial.print(dateStamp);
        Serial.print(" time: "); Serial.println(timeStamp);
        Serial.print("temp: "); Serial.println(t);
        Serial.print("humidity: "); Serial.println(h);
        Serial.print("moisture: "); Serial.println(average);
        Serial.print("light: "); Serial.println(photo);
        Serial.print("state_fan: "); Serial.println(state_fan);
        Serial.print("state_light: "); Serial.println(state_light);
        Serial.print("state_moister: "); Serial.println(state_moister);
        delay(SENSOR_WAIT_TIME);
        total = total - readings[index];
        readings[index] = analogRead(pinMoisture);
        total = total + readings[index];
        index++;
        if(index >= numReadings) {
            index = 0;
            average = total / numReadings;
        } else {
            average = total / numReadings;
        }
        Watchdog.reset();
    }
} else{
    state_moister = false;
    Watchdog.reset();
}


  if (photo > LIGHT_DETECT) {
    digitalWrite(RELAY_LAMP, HIGH);
    state_light = false;
  } else {
    digitalWrite(RELAY_LAMP, LOW);
    state_light = true;
  }
  
  File dataFile = SD.open("log.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.print("date=");
    dataFile.print(dateStamp);
    dataFile.print(" time=");
    dataFile.println(timeStamp);
    dataFile.print("temp=");
    dataFile.println(t);
    dataFile.print("humidity=");
    dataFile.println(h);
    dataFile.print("moisture=");
    dataFile.println(average);
    dataFile.print("light=");
    dataFile.println(photo);
    dataFile.print("state_fan=");
    dataFile.println(state_fan);
    dataFile.print("state_light=");
    dataFile.println(state_light);
    dataFile.print("state_moister=");
    dataFile.println(state_moister);
    dataFile.println(" ");
    dataFile.close();
    Watchdog.reset();
  }

  // Отправка данных по аппаратному Serial (Bluetooth)
  Serial.println(" "); 
  Serial.print("date: "); Serial.print(dateStamp);
  Serial.print(" time: "); Serial.println(timeStamp);
  Serial.print("temp: "); Serial.println(t);
  Serial.print("humidity: "); Serial.println(h);
  Serial.print("moisture: "); Serial.println(average);
  Serial.print("light: "); Serial.println(photo);
  Serial.print("state_fan: "); Serial.println(state_fan);
  Serial.print("state_light: "); Serial.println(state_light);
  Serial.print("state_moister: "); Serial.println(state_moister);

  Watchdog.reset();
}

