/*
  9차시 초음파센서 - 예제 1  거리를 시리얼 모니터에 출력

  HC-SR04
    VCC  -> 5V
    Trig -> A4
    Echo -> D12
    GND  -> GND

  A4 (= PC4) 는 아날로그 핀이지만 디지털 출력으로도 쓸 수 있다.

  측정 순서
    1. Trig 에 10us HIGH 를 준다
    2. 센서가 40kHz 초음파를 8번 보낸다
    3. Echo 가 HIGH 인 시간 t (us) 를 잰다  = 왕복 시간
    4. 거리 (cm) = t x 0.034 / 2  =  t / 58

  pulseIn(핀, HIGH, 제한시간)
    핀이 HIGH 인 시간을 us 로 돌려준다.
    제한시간 안에 끝나지 않으면 0 을 돌려준다.
*/

#include <Arduino.h>

const int TRIG_PIN = A4; // Trig : A4 (PC4)
const int ECHO_PIN = 12; // Echo : D12 (PB4)

const unsigned long TIMEOUT_US = 25000; // 약 4.3m.  이보다 길면 포기

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  Serial.println("HC-SR04 start");
}

void loop() {
  // 1. Trig 에 10us HIGH
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 3. Echo 가 HIGH 인 시간
  unsigned long t = pulseIn(ECHO_PIN, HIGH, TIMEOUT_US);

  if (t == 0) {
    Serial.println("out of range");
  } else {
    float distance = t / 58.0; // 4. 거리 (cm)

    Serial.print("Echo : ");
    Serial.print(t);
    Serial.print(" us   Distance : ");
    Serial.print(distance, 1);
    Serial.println(" cm");
  }

  delay(200); // 측정 간격은 60ms 이상
}
