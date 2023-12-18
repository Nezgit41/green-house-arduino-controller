// библиотека для работы I²C
#include <Wire.h>
// библиотека для работы с часами реального времени
#include <TroykaRTC.h>

// размер массива для времени
#define LEN_TIME 12
// размер массива для даты
#define LEN_DATE 12
// размер массива для дня недели
#define LEN_DOW 12

// создаём объект для работы с часами реального времени
RTC clock;

// массив для хранения текущего времени
char time[LEN_TIME];
// массив для хранения текущей даты
char date[LEN_DATE];
// массив для хранения текущего дня недели
char weekDay[LEN_DOW];
  
void setup() {
  // открываем последовательный порт
  Serial.begin(9600);
  // инициализация часов
  clock.begin();
  // метод установки времени и даты в модуль вручную
  // clock.set(10,25,45,27,07,2005,THURSDAY);    
  // метод установки времени и даты автоматически при компиляции
  clock.set(__TIMESTAMP__);
  // что бы время менялось при прошивки или сбросе питания
  // закоментируйте оба метода clock.set();
}

void loop() {
  // запрашиваем данные с часов
  clock.read();
  // получаем текущее время, дату и день недели в переменные
  clock.getTimeStamp(time, date, weekDay);
  // выводим в serial порт текущее время, дату и день недели
  Serial.print(time);
  Serial.print("\t");
  Serial.print(date);
  Serial.print("\t");
  Serial.println(weekDay);
  // ждём одну секунду
  delay(1000);
}
