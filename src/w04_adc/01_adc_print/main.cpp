/*
  5차시 ADC - 실습1  ADC 결과 보기

  가변저항 : 한쪽 끝 -> 5V,  반대쪽 끝 -> GND,  가운데 다리 -> A1 (PC1)

  가변저항을 돌릴 때 0 ~ 1023 이 출력되면 성공.
*/

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const int POT_PIN = A1; // PC1 - ADC1

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
  Serial.begin(9600);
}

void loop() {
  int value = analogRead(POT_PIN); // 0 ~ 1023

  Printf("ADC : %d\n", value);
  delay(500);
}
