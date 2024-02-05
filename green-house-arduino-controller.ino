/*
██████╗░██╗░░░██╗  ███╗░░██╗███████╗███████╗░██████╗░██╗████████╗
██╔══██╗╚██╗░██╔╝  ████╗░██║██╔════╝╚════██║██╔════╝░██║╚══██╔══╝
██████╦╝░╚████╔╝░  ██╔██╗██║█████╗░░░░███╔═╝██║░░██╗░██║░░░██║░░░
██╔══██╗░░╚██╔╝░░  ██║╚████║██╔══╝░░██╔══╝░░██║░░╚██╗██║░░░██║░░░
██████╦╝░░░██║░░░  ██║░╚███║███████╗███████╗╚██████╔╝██║░░░██║░░░
╚═════╝░░░░╚═╝░░░  ╚═╝░░╚══╝╚══════╝╚══════╝░╚═════╝░╚═╝░░░╚═╝░░░
last update 05.02.2024 16:29
*/

#include "DHT.h" 
#include <SD.h>  
#include <SPI.h>
#include <Wire.h>
#include "TroykaRTC.h"
#include <Adafruit_Sensor.h>
#include "GyverWDT.h"

//настройки шагового двигателя
const byte stepPin = 7;
const byte directionPin = 8;
const byte enablePin = 11;

const int numReadings = 3;

//переменыые состояния форточки и полива
bool state_moister = false;
bool state_open = false; 
bool state_light = false; 

int delayTime = 2;
int pinMoisture=A0; //пин влажности почвы
int pinPhoto=A2; //пин фоторезистора

//среднее арефмитическое для датчика влажности почвы
int readings[numReadings];      // данные, считанные с входного аналогового контакта
int index = 0;                  // индекс для значения, которое считывается в данный момент
int total = 0;                  // суммарное значение
int average = 0;                // среднее значение


#define LEN_DOW 12
#define LEN_TIME 12
#define LEN_DATE 12
#define DHTTYPE DHT11
#define CS_PIN 10   
#define DHTPIN 9    
#define RELAY_PUMP 5 //пин реле помпы
#define RELAY_LAMP 6 //пин реле освещения
#define RELAY_FAN 2 //пмн вентилятора

//настройка значений для условий
int LIGHT_DETECT = 0;
int MOISTURE_DETECT = 0;
float TEMP_DETECT = 0.0;
float TEMP_DOWN = 0.0;
int STEP_NUM = 0;

RTC clock;

char time[LEN_TIME];
char date[LEN_DATE];
char weekDay[LEN_DOW];

DHT dht(DHTPIN, DHTTYPE);
int refresh_rate = 5000;

void setup() {
  
Serial.begin(9600);
  dht.begin();

clock.begin();
clock.set(__TIMESTAMP__);

pinMode(stepPin, OUTPUT);
pinMode(directionPin, OUTPUT);
pinMode(enablePin, OUTPUT);
pinMode(RELAY_PUMP,OUTPUT);digitalWrite(RELAY_PUMP,LOW);
pinMode(RELAY_LAMP,OUTPUT);digitalWrite(RELAY_LAMP,LOW);
pinMode(RELAY_FAN, OUTPUT);digitalWrite(RELAY_FAN,HIGH);
Watchdog.enable(RESET_MODE, WDT_PRESCALER_512);

  Serial.println("Initializing Card"); // Распечатываем в мониторе последовательного порта "Initializing Card"("Инициализация карты")
  pinMode(CS_PIN, OUTPUT); // Определяем CS(контакт выбора) контакт как выход
 
  // Инициализация SD-карты
if (!SD.begin(CS_PIN)) {
  Serial.println("Card Failure");
  return;
}

Serial.println("Card Ready");

// Чтение конфигурационного файла speed.txt
File commandFile = SD.open("speed.txt");
if (commandFile) {
  Serial.println("Reading Command File");
  
  while(commandFile.available()) {
    refresh_rate = commandFile.parseInt();
  }

  Serial.print("Refresh Rate = ");
  Serial.print(refresh_rate);
  Serial.println("ms");
  
  commandFile.close();
} else {
  Serial.println("Could not read command file.");
  return;
}

// Чтение настроек из файла setting.txt
File settingsFile = SD.open("setting.txt");
if (settingsFile) {
  while (settingsFile.available()) {
    String line = settingsFile.readStringUntil('\n');
    if (line.startsWith("LIGHT_DETECT")) {
      LIGHT_DETECT = line.substring(line.indexOf(' ') + 1).toInt();
    } else if (line.startsWith("MOISTURE_DETECT")) {
      MOISTURE_DETECT = line.substring(line.indexOf(' ') + 1).toInt();
    } else if (line.startsWith("TEMP_DETECT")) {
      TEMP_DETECT = line.substring(line.indexOf(' ') + 1).toFloat();
    } else if (line.startsWith("EMP_DOWN")) {
      TEMP_DOWN = line.substring(line.indexOf(' ') + 1).toFloat();
    } else if (line.startsWith("STEP_NUM")) {
      STEP_NUM = line.substring(line.indexOf(' ') + 1).toInt();
    }
  }
  settingsFile.close();
} else {
  Serial.println("Could not read setting file.");
  return;
}

for (int thisReading = 0; thisReading < numReadings; thisReading++) {
  readings[thisReading] = 0;
  }
Watchdog.reset();
}
void loop() {
  
delay (2000);

clock.read();
clock.getTimeStamp(time, date, weekDay);
float h = dht.readHumidity(); //считываем влажность
float t = dht.readTemperature(); //считываем температуру
int photo=analogRead(pinPhoto);

  total= total - readings[index]; 
  // ...которое было считано от сенсора:
  readings[index] = analogRead(pinMoisture); 
  // добавляем его к общей сумме:
  total= total + readings[index];       
  // продвигаемся к следующему значению в массиве:  
  index = index + 1;                    

if (index >= numReadings){              
  // ...возвращаемся к началу: 
  index = 0;                           

  // вычисляем среднее значение:
  average = total / numReadings;         
  // выводим его на компьютер цифрами в кодировке ASCII    
  delay(1);        // делаем задержку между считываниями – для стабильности программы

} 

 digitalWrite(enablePin, HIGH);
 digitalWrite(directionPin, LOW);

if (t >= TEMP_DETECT) {
  if (state_open == false){
    digitalWrite(RELAY_FAN, LOW);
    for (int i = 0; i < STEP_NUM; ++i) {
    // Делаем шаг
    digitalWrite(stepPin, HIGH);
    delay(delayTime);
    digitalWrite(stepPin, LOW);
    delay(delayTime);
    state_open = true;
    Watchdog.reset();
    }
   }
} else {
  if (t <= TEMP_DOWN) {
    if (state_open == true){ 
    digitalWrite(directionPin, HIGH);
    digitalWrite(enablePin, HIGH);
    digitalWrite(RELAY_FAN, LOW);
    for (int i = 0; i < STEP_NUM; ++i) {
    digitalWrite(stepPin, HIGH);
    delay(delayTime);
    digitalWrite(stepPin, LOW);
    delay(delayTime);
    state_open = false;
    Watchdog.reset();
      }
    }
  }
}

digitalWrite(RELAY_FAN, HIGH);
digitalWrite(directionPin, LOW);
digitalWrite(enablePin, LOW);
digitalWrite(RELAY_FAN,HIGH);

if (average > MOISTURE_DETECT) {
  digitalWrite(RELAY_PUMP,HIGH); 
  state_moister == false;
}
else  {
  digitalWrite(RELAY_PUMP,LOW);
  state_moister == true;
  Watchdog.reset();
  delay (3000);
}  
  
if (photo > LIGHT_DETECT) {
  digitalWrite(RELAY_LAMP,HIGH);
  state_light = false;
}
else {
  digitalWrite(RELAY_LAMP,LOW);
  state_light = true;
}

// Открываем файл и записываем в него данные
  File dataFile = SD.open("log.txt", FILE_WRITE);
  if (dataFile)
  {
   
    dataFile.println(" ");
    dataFile.println(" ");
    dataFile.print(time);dataFile.print("\t");
    dataFile.print(date);dataFile.print("\t");
    dataFile.println(" ");
    dataFile.print("temp=");dataFile.print(t);
    dataFile.println(" ");
    dataFile.print("humidity=");dataFile.print(h);
    dataFile.println(" ");
    dataFile.print("moisture=");dataFile.print(average);
    dataFile.println(" ");
    dataFile.print("light=");dataFile.print(photo);
    dataFile.println(" ");
    dataFile.print("state_open=");dataFile.print(state_open);
    dataFile.println(" ");
    dataFile.print("state_moister=");dataFile.print(state_moister);
    dataFile.close(); // Внимание! Данные не будут записаны, пока вы не закроете соединение!
  }
/*
Serial.println(" ");
Serial.print(time);Serial.print("\t");
Serial.print(date);Serial.print("\t");  
Serial.println(" ");
Serial.print("temp: ");Serial.println(t);
Serial.print("humidity: ");Serial.println(h);
Serial.print("moisture: ");Serial.println(average);
Serial.print("light: ");Serial.println(photo);
Serial.print("state: ");Serial.println(state_open);
*/
Watchdog.reset();
}
