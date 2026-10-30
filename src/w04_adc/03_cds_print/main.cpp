/*
  5차시 아날로그 센서 응용 - (1) 전압 분배로 조도 읽기

  회로 (전압 분배)
    5V -> GL5528 -> A3 -> 10k옴 -> GND

  밝을수록 GL5528 의 저항이 작아져서 A3 전압이 올라간다.  (밝음 = 큰 값)
  GL5528 과 10k옴 의 자리를 바꾸면 반대로 어두울수록 값이 커진다.

  두 저항에 걸리는 전압은 저항 크기에 비례해서 나뉜다.
    Vout  = 5V x 10k / (R_cds + 10k)
    ADC   = Vout / 5V x 1023

  거꾸로 풀면 ADC 값으로 GL5528 의 저항을 구할 수 있다.
    R_cds = 10k x (1023 - ADC) / ADC

  손으로 센서를 가렸다 떼면서 세 값이 어떻게 바뀌는지 본다.
    GL5528 : 밝은 곳(10 lux) 약 8 ~ 20k옴,  어두운 곳 1M옴 이상
*/

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const int CDS_PIN = A3;       // PC3 - ADC3
const long R_FIXED = 10000;   // 아래쪽 고정 저항 10k옴

// ── 4차시에서 만든 Printf ──────────────────────────
// 주의 : 아두이노의 vsnprintf 는 %f (실수) 를 지원하지 않는다. "?" 로 찍힌다.
//        그래서 이 실습은 정수(%d, %ld)만 쓴다.
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
  int adc = analogRead(CDS_PIN); // 0 ~ 1023

  // 실수를 쓰지 않으려고 mV 단위 정수로 계산한다
  long mv = (long)adc * 5000 / 1023; // 0 ~ 5000 mV

  if (adc == 0) {
    // 0 으로 나눌 수 없다.  아주 어둡거나 배선이 빠진 것이다
    Printf("ADC : %4d   V : %4ld mV   R_cds : too dark\n", adc, mv);
  } else {
    long r_cds = R_FIXED * (1023 - adc) / adc;
    Printf("ADC : %4d   V : %4ld mV   R_cds : %7ld ohm\n", adc, mv, r_cds);
  }

  delay(500);
}
