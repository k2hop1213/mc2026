/*
  5차시 아날로그 센서 응용 - 과제 : 어두워질 때마다 카운트

  회로 (전압 분배)
    5V -> GL5528 -> A3 -> 10k옴 -> GND
  LED
    +5V -> 220옴 -> LED -> D2 ~ D9   (Active Low)

  밝음 -> 어두움 으로 바뀌는 순간에만 카운트를 1 올린다. (에지 검출)
  계속 가리고 있는 동안에는 올라가지 않는다.
  카운트는 LED 8개에 2진수로 표시한다.  D2 = 최하위 비트, 5 -> 0000 0101
  byte 라서 255 다음은 0 으로 돌아간다.

  값이 경계에서 떨려도 여러 번 세지 않도록 THRESHOLD 를 두 개 쓴다. (히스테리시스)
    밝은 상태   : ADC <  DARK_THRESHOLD   이면 어두워진 것
    어두운 상태 : ADC >= LIGHT_THRESHOLD  이면 밝아진 것
    그 사이 값이면 이전 상태를 그대로 유지한다.

  두 THRESHOLD 는 측정값(손 뗌 약 800, 가림 약 510)의 중간 650 을 기준으로 정했다.
  장소가 바뀌면 다시 재서 맞춘다.
*/

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const int CDS_PIN = A3;          // PC3 - ADC3
const int DARK_THRESHOLD = 600;  // 이보다 작아지면 어둡다고 본다
const int LIGHT_THRESHOLD = 700; // 이 이상이 되면 밝다고 본다

byte count = 0;        // 255 다음은 0
bool was_dark = false; // 이전 상태

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
  led_write(count);

  Serial.begin(9600);

  // 가린 채로 켜져도 세지 않도록 시작 상태를 읽어 둔다
  was_dark = analogRead(CDS_PIN) < DARK_THRESHOLD;
}

void loop() {
  int adc = analogRead(CDS_PIN); // 0 ~ 1023

  if (!was_dark && adc < DARK_THRESHOLD) {
    // 밝음 -> 어두움 : 이때만 센다
    was_dark = true;
    count++;
    led_write(count);
    Printf("Count : %3d   ADC : %4d\n", count, adc);
  } else if (was_dark && adc >= LIGHT_THRESHOLD) {
    // 어두움 -> 밝음 : 다음 에지를 기다린다
    was_dark = false;
  }

  delay(50);
}
