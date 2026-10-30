/*
  5차시 ADC - 실습2  ADC 값을 LED 로 보기

  가변저항 : 한쪽 끝 -> 5V,  반대쪽 끝 -> GND,  가운데 다리 -> A1 (PC1)
  LED      : +5V -> 220옴 -> LED -> D2 ~ D9   (Active Low)

  가변저항을 돌리면 켜지는 LED 개수가 1 ~ 8 개로 바뀐다. (막대 그래프)
*/

#include <Arduino.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9
const int LED_COUNT = 8;

const int POT_PIN = A1; // PC1 - ADC1

// 비트를 LED 핀으로 뿌린다.  bit0 = D2
// 회로가 Active Low 라서 뒤집어 내보낸다
void led_write(byte value) {
  value = ~value;

  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

// 0 ~ 1023 을 128 씩 8 칸으로 나눈다.
// 칸 번호가 0 이면 1 개, 7 이면 8 개를 켠다.
//     0 ~  127  ->  0000 0001
//   128 ~  255  ->  0000 0011
//          ...
//   896 ~ 1023  ->  1111 1111
void disp_led(int value) {
  int count = value / 128 + 1; // 켤 LED 개수 : 1 ~ 8
  byte pattern = 0;

  for (int i = 0; i < count; i++) {
    pattern = pattern | (1 << i); // 아래 비트부터 하나씩 채운다
  }

  led_write(pattern);
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }

  led_write(0x00);
}

void loop() {
  int value = analogRead(POT_PIN); // 0 ~ 1023

  disp_led(value);
  delay(100);
}
