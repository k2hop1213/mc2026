#include <Arduino.h>

const int FIRST_PIN = 2;
const int LED_COUNT = 8;
const int FIRST_SW = 10;
const int SW_COUNT = 3;
const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

void led_write(byte value) {
  value = ~value;
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

void led_all_on_off(int cnt, int ms) {
  for (int i = 0; i < cnt; i++) {
    led_write(ALL_ON);
    delay(ms);
    led_write(ALL_OFF);
    delay(ms);
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

// void led_shift(int cnt, int ms) {
//   for (int i = 0; i < cnt; i++) {
//     for (int b = 0; b < 8; b++) { // 왕복: 0 -> 7
//       led_write(0x01 << b);
//       delay(ms);
//     }
//     for (int b = 6; b >= 0; b--) { // 왕복: 7 -> 0
//       led_write(0x01 << b);
//       delay(ms);
//     }
//   }
// }
byte pos = 0x00; // 현재 켜진 LED 위치 (0 = 아직 시작 안 함)
byte sw_read();
byte prev_sw = 0;

void led_shift() {
  for (int i = 0; i < 2; i++) {
    for (int b = 0; b < 8; b++) { // 왕복: 0 -> 7
      led_write(0x01 << b);
      delay(500);
    }
    for (int b = 6; b >= 0; b--) { // 왕복: 7 -> 0
      led_write(0x01 << b);
      delay(500);
    }
  }
  pos = 0x01;          // 마지막에 켜진 LED0 과 pos 를 일치시킴
  prev_sw = sw_read(); // 동작 중 누른 스위치가 뒤늦게 인식되지 않도록 동기화
}
void led_shift_once() {
  if (pos == 0x00) {
    pos = 0x01;
    led_write(pos);
  } else {
    pos = (pos << 1) | ((pos & 0x80) >> 7);
    led_write(pos);
  }
}

void led_right_once() {
  if (pos == 0x00) {
    pos = 0x80;
    led_write(pos);
  } else {
    pos = (pos >> 1) | ((pos & 0x01) << 7);
    led_write(pos);
  }
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

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, HIGH); // active-low: OUTPUT 전환 전에 꺼진 값을 미리 설정
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  for (int i = 0; i < SW_COUNT; i++) {
    pinMode(FIRST_SW + i, INPUT_PULLUP);
  }
  led_write(ALL_OFF);
}

void loop() {
  // led_all_on_off(3, 300);
  // led_alternating(0x33, 3, 300);
  // led_alternating(0x0F, 3, 300);
  // led_alternating(0x3C, 3, 300);
  // led_shift(5, 150);
  byte pressed = sw_pressed();
  if (pressed & 0x01) {
    led_shift_once();
    delay(300);
  } else if (pressed & 0x02) {
    led_right_once();
    delay(300);
  } else if (pressed & 0x04) {
    led_shift();
    delay(300);
  }
}
