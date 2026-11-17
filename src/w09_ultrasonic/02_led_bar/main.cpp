/*
  9차시 초음파센서 - 예제 2  거리를 LED 막대로 표시

  HC-SR04
    VCC  -> 5V
    Trig -> A4
    Echo -> D12
    GND  -> GND
  LED : D2 ~ D9  (Active Low : LOW 를 주면 켜진다)

  동작
    물체가 가까울수록 LED 가 많이 켜진다.
      40cm 이상   0개
      35 ~ 40cm   1개
      ...
      5cm 미만    8개
    (5cm 마다 1개씩)
*/

#include <Arduino.h>

const int TRIG_PIN = A4;  // Trig : A4 (PC4)
const int ECHO_PIN = 12;  // Echo : D12 (PB4)
const int LED_FIRST = 2;  // D2 ~ D9
const int LED_COUNT = 8;

const unsigned long TIMEOUT_US = 25000;

// 거리 (cm) 를 잰다.  범위를 벗어나면 -1
float read_distance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long t = pulseIn(ECHO_PIN, HIGH, TIMEOUT_US);
  if (t == 0) {
    return -1;
  }
  return t / 58.0;
}

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
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(LED_FIRST + i, OUTPUT);
  }
  led_bar(0);
}

void loop() {
  float d = read_distance();
  int n = 0;

  if (d >= 0 && d < 40) {
    n = 8 - (int)(d / 5); // 0 ~ 5cm -> 8개,  35 ~ 40cm -> 1개
  }
  led_bar(n);

  Serial.print("Distance : ");
  Serial.print(d, 1);
  Serial.print(" cm   LED : ");
  Serial.println(n);

  delay(100);
}
