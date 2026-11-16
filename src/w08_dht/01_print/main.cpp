/*
  8차시 온습도센서 - 예제 1  온도와 습도를 시리얼 모니터에 출력

  DHT22 모듈 (keyestudio)
    S   -> A5
    VCC -> 5V
    GND -> GND

  A5 (= PC5) 는 아날로그 핀이지만 디지털 입출력으로도 쓸 수 있다.
  모듈 기판에 풀업 저항이 있어서 따로 달지 않는다.

  라이브러리
    Adafruit DHT 라이브러리를 쓴다.
    platformio.ini 의 env 아래 lib_deps 에 다음 두 줄을 넣는다.
      adafruit/DHT sensor library@1.4.6
      adafruit/Adafruit Unified Sensor@1.1.15

  DHT22 는 2초에 한 번보다 자주 읽으면 안 된다.
*/

#include <Arduino.h>
#include <DHT.h>

const int DHT_PIN = A5; // 모듈 S : A5 (PC5)

DHT dht(DHT_PIN, DHT22);

void setup() {
  Serial.begin(9600);
  dht.begin();
  Serial.println("DHT22 start");
}

void loop() {
  delay(2000); // 2초 간격

  float humi = dht.readHumidity();    // 습도 (%)
  float temp = dht.readTemperature(); // 온도 (섭씨)

  // 읽기에 실패하면 NAN (숫자가 아님) 이 돌아온다
  if (isnan(humi) || isnan(temp)) {
    Serial.println("read fail");
    return;
  }

  Serial.print("Humidity : ");
  Serial.print(humi, 1); // 소수점 아래 1자리
  Serial.print(" %   Temperature : ");
  Serial.print(temp, 1);
  Serial.println(" C");
}
