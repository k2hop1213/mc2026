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
    for (int b = 6; b >= 0; b--) {
      led_write(0x01 << b);
      delay(ms);
    }
  }

  prev_sw = sw_read();
}

void led_cross(int cnt) {
  byte left = 0x01;
  byte right = 0x80;
  for (int i = 0; i < cnt * LED_COUNT; i++) {
    led_write(right | left);
    delay(300);
    left = (left << 1) | ((left & 0x80) >> 7);
    right = (right >> 1) | ((right & 0x01) << 7);
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
  byte pressed = sw_pressed();
  if (pressed & 0x01) {
    led_shift(2, 300);
    led_write(ALL_OFF);
  } else if (pressed & 0x02) {
    led_alternating(0x55, 2, 300);
    led_write(ALL_OFF);

  } else if (pressed & 0x04) {
    led_cross(2);
    led_write(ALL_OFF);
  }
}
