/*
  8차시 온습도센서 - 예제 2  습도를 LED 막대로 표시

  DHT22 모듈 (keyestudio)
    S   -> A5
    VCC -> 5V
    GND -> GND
  LED : D2 ~ D9  (Active Low : LOW 를 주면 켜진다)

  라이브러리 : platformio.ini 의 env 아래 lib_deps 에
      adafruit/DHT sensor library@1.4.6
      adafruit/Adafruit Unified Sensor@1.1.15

  동작
    습도 0 ~ 100% 를 LED 0 ~ 8개로 보여 준다.  (12.5% 마다 1개)
    센서에 입김을 불어 보면 습도가 올라가 LED 가 늘어난다.
*/

#include <Arduino.h>
#include <DHT.h>

const int DHT_PIN = A5;   // 모듈 S : A5 (PC5)
const int LED_FIRST = 2;  // D2 ~ D9
const int LED_COUNT = 8;

DHT dht(DHT_PIN, DHT22);

// LED 를 아래(D2)에서부터 n 개 켠다
void led_bar(int n) {
  for (int i = 0; i < LED_COUNT; i++) {
    if (i < n) {
      digitalWrite(LED_FIRST + i, LOW); // 켜기
    } else {
      digitalWrite(LED_FIRST + i, HIGH); // 끄기
    }
  }
}

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(LED_FIRST + i, OUTPUT);
  }
  led_bar(0);
  dht.begin();
}

void loop() {
  delay(2000);

  float humi = dht.readHumidity();
  if (isnan(humi)) {
    Serial.println("read fail");
    return;
  }

  int n = (int)(humi / 12.5); // 0 ~ 8
  if (n > LED_COUNT) {
    n = LED_COUNT;
  }
  led_bar(n);

  Serial.print("Humidity : ");
  Serial.print(humi, 1);
  Serial.print(" %   LED : ");
  Serial.println(n);
}
