/*
  5차시 아날로그 센서 응용 - (2) 어두우면 LED 켜기

  회로 (전압 분배)
    5V -> GL5528 -> A3 -> 10k옴 -> GND
  LED
    +5V -> 220옴 -> LED -> D2 ~ D9   (Active Low)

  밝을수록 GL5528 의 저항이 작아져서 A3 값이 커진다.
    ADC <  THRESHOLD  ->  어둡다  ->  LED 전체 ON
    ADC >= THRESHOLD  ->  밝다    ->  LED 전체 OFF

  THRESHOLD 는 (1) 에서 손으로 가렸을 때와 뗐을 때의 값을 보고
  그 중간쯤으로 정한다.
*/

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const int CDS_PIN = A3;     // PC3 - ADC3
const int THRESHOLD = 650;  // 이보다 작으면 어둡다고 본다

const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

// 비트를 LED 핀으로 뿌린다.  bit0 = D2
// 회로가 Active Low 라서 뒤집어 내보낸다
void led_write(byte value) {
  value = ~value;

  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

// ── 4차시에서 만든 Printf ──────────────────────────
void tx_char(char txChar) {
  Serial.write(txChar);
}

void tx_str(char* txStr, int len) {
  for (int i = 0; i < len; i++) {
    tx_char(txStr[i]);
  }
}

void Printf(const char* fmt, ...) {
  char text[96];
  va_list args;

  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  tx_str(text, strlen(text));
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  led_write(ALL_OFF);

  Serial.begin(9600);
}

void loop() {
  int adc = analogRead(CDS_PIN); // 0 ~ 1023

  if (adc < THRESHOLD) {
    led_write(ALL_ON); // 어둡다
    Printf("ADC : %4d  ->  DARK   LED ON\n", adc);
  } else {
    led_write(ALL_OFF); // 밝다
    Printf("ADC : %4d  ->  LIGHT  LED OFF\n", adc);
  }

  delay(300);
}
