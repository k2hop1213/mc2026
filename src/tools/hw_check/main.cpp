/*
  시험 대비 도구 - 보드 자가 점검

  프로토콜 없이 부품만 확인한다. 회로가 바뀌면 아래 상수만 고쳐서 올린다.

  LED      : D2 부터 하나씩 0.3초씩 차례로 켠다. 지금 켜진 핀을 시리얼에 표시한다.
  스위치   : 누를 때마다 "SW1 pressed" 를 출력한다. (인터럽트는 쓰지 않는다)
  아날로그 : 가변저항, 조도센서 값을 0.5초마다 출력한다.
  부저     : 시작할 때 3번 울리고, 스위치를 누를 때마다 짧게 울린다.

  판단
    여기서 정상이면 배선은 맞다.   -> 프로토콜 코드나 프로그램 쪽을 본다
    여기서도 이상하면 배선 문제다. -> 해당 부품의 배선을 본다

  시리얼 : 9600bps
*/

#include <Arduino.h>

// ── 회로가 바뀌면 여기만 고친다 ──────────────────────
const int LED_PINS[] = {2, 3, 4, 5, 6, 7, 8, 9};
const int LED_COUNT = 8;
const bool LED_ACTIVE_LOW = true;

const int SW_PINS[] = {10, 11, 12};
const int SW_COUNT = 3;

const int POT_PIN = A1;
const int CDS_PIN = A3;
const int BUZZER_PIN = A0;
// ────────────────────────────────────────────────

const unsigned long LED_MS = 300;
const unsigned long PRINT_MS = 500;
const unsigned long DEBOUNCE_MS = 30;
const unsigned int BEEP_HZ = 1000;

int led_index = 0;
unsigned long led_time = 0;
unsigned long print_time = 0;

bool sw_stable[SW_COUNT];
bool sw_last[SW_COUNT];
unsigned long sw_time[SW_COUNT];

void led_set(int index, bool on) {
  digitalWrite(LED_PINS[index], on != LED_ACTIVE_LOW ? HIGH : LOW);
}

void update_led() {
  if (millis() - led_time < LED_MS) {
    return;
  }
  led_time = millis();

  led_set(led_index, false);
  led_index = (led_index + 1) % LED_COUNT;
  led_set(led_index, true);
}

void update_switches() {
  for (int i = 0; i < SW_COUNT; i++) {
    bool now = digitalRead(SW_PINS[i]);

    if (now != sw_last[i]) {
      sw_last[i] = now;
      sw_time[i] = millis();
    }

    // 값이 일정 시간 그대로일 때만 바뀐 것으로 본다
    if (now != sw_stable[i] && millis() - sw_time[i] >= DEBOUNCE_MS) {
      sw_stable[i] = now;

      if (now == LOW) {
        Serial.print(F("SW"));
        Serial.print(i + 1);
        Serial.println(F(" pressed"));
        tone(BUZZER_PIN, BEEP_HZ, 100);
      }
    }
  }
}

void print_status() {
  if (millis() - print_time < PRINT_MS) {
    return;
  }
  print_time = millis();

  Serial.print(F("POT "));
  Serial.print(analogRead(POT_PIN));
  Serial.print(F("   CDS "));
  Serial.print(analogRead(CDS_PIN));
  Serial.print(F("   LED D"));
  Serial.print(LED_PINS[led_index]);
  Serial.print(F("   SW "));

  for (int i = 0; i < SW_COUNT; i++) {
    Serial.print(sw_stable[i] == LOW ? F("ON ") : F("off "));
  }
  Serial.println();
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    led_set(i, false);
  }

  for (int i = 0; i < SW_COUNT; i++) {
    pinMode(SW_PINS[i], INPUT_PULLUP);
    sw_stable[i] = HIGH;
    sw_last[i] = HIGH;
  }

  led_set(led_index, true);

  Serial.begin(9600);
  Serial.println(F("HW CHECK start"));

  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, BEEP_HZ, 200);
    delay(400);
  }
}

void loop() {
  update_led();
  update_switches();
  print_status();
}
