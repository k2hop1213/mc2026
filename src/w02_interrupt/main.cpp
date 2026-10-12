#include <Arduino.h>

const int FIRST_PIN = 4;
const int LED_COUNT = 8;
const int FIRST_SW = 2;
const int SW_COUNT = 2;
const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

void led_write(byte value) {
  value = ~value;
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

void led_alternating(byte pattern, int cnt, int ms) {
  for (int i = 0; i < cnt; i++) {
    led_write(pattern);
    delay(ms);
    led_write(~pattern);
    delay(ms);
  }
}
byte sw_read();
byte prev_sw = 0;

void led_shift(int cnt, int ms) {
  for (int i = 0; i < cnt; i++) {
    for (int b = 0; b < 8; b++) {
      led_write(0x01 << b);
      delay(ms);
    }
    for (int b = 6; b >= 1; b--) {
      led_write(0x01 << b);
      delay(ms);
    }
  }
  led_write(0x01);
  delay(ms);
  prev_sw = sw_read();
}
void led_all_on_off(int cnt, int ms) {
  for (int i = 0; i < cnt; i++) {
    led_write(ALL_ON);
    delay(ms);
    led_write(ALL_OFF);
    delay(ms);
  }
}
void led_cross(int cnt) {
  byte left = 0x01;
  byte right = 0x80;
  for (int c = 0; c < cnt; c++) {
    for (int i = 0; i < 4; i++) {
      led_write((right >> i) | (left << i));
      delay(100);
    }
    for (int i = 2; i >= 1; i--) {
      led_write((right >> i) | (left << i));
      delay(100);
    }
  }
  led_write((right) | (left));
  delay(100);
}

byte sw_read() {
  byte value = 0;
  for (int i = 0; i < SW_COUNT; i++) {
    if (digitalRead(FIRST_SW + i) == LOW) {
      value = value | (1 << i);
    }
  }
  return value;
}
byte sw_pressed() {
  byte now = sw_read();
  byte edge = now & ~prev_sw;

  prev_sw = now;
  return edge;
}

volatile byte sw_event = 0;
volatile unsigned long last_press = 0;

void isr_sw1() {
  if (millis() - last_press < 200) {
    return;
  } // 채터링 무시​
  last_press = millis();
  sw_event = sw_event | 0x01;
}

void isr_sw2() {
  if (millis() - last_press < 200) {
    return;
  }
  last_press = millis();
  sw_event = sw_event | 0x02;
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, HIGH); // active-low: OUTPUT 전환 전에 꺼진 값을 미리 설정
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  for (int i = 0; i < SW_COUNT; i++) {
    pinMode(FIRST_SW + i, INPUT_PULLUP);
  }
  attachInterrupt(digitalPinToInterrupt(FIRST_SW), isr_sw1, FALLING);
  attachInterrupt(digitalPinToInterrupt(FIRST_SW + 1), isr_sw2, FALLING);
  led_write(ALL_OFF);
}

void loop() {

  // 깃발을 읽고 비우는 동안에는 인터럽트를 잠시 막는다​

  noInterrupts();

  byte event = sw_event;
  sw_event = 0;

  interrupts();

  if (event & 0x01) { // SW1​

    led_shift(2, 80);

    led_write(ALL_OFF);
  }

  if (event & 0x02) { // SW2​

    led_alternating(0xAA, 2, 300);

    led_write(ALL_OFF);
  }

  // 기본 동작 : 전체 점멸​

  led_all_on_off(1, 500);
}
