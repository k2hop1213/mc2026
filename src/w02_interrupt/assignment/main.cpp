#include <Arduino.h>
#include <avr/interrupt.h>

const int FIRST_PIN = 2;
const int LED_COUNT = 8;
const int SW1_PIN = 10;
const int SW2_PIN = 11;
const int SW3_PIN = 12;
const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

void led_write(byte value) {
  value = ~value;
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

volatile byte sw_event = 0;
volatile byte prev_pb = 0;
volatile unsigned long last_press = 0;

// D10~D12 는 외부 인터럽트 핀이 아니라서 PORTB 핀 변화 인터럽트를 쓴다
ISR(PCINT0_vect) {
  byte now = PINB & 0x1C;
  byte fell = prev_pb & ~now; // HIGH -> LOW 로 바뀐 핀 = 눌린 스위치
  prev_pb = now;

  if (fell == 0) {
    return; // 뗄 때 생기는 변화는 무시
  }
  if (millis() - last_press < 200) {
    return;
  } // 채터링 무시
  last_press = millis();

  if (fell & 0x04) {
    sw_event |= 0x01;
  }
  if (fell & 0x08) {
    sw_event |= 0x02;
  }
  if (fell & 0x10) {
    sw_event |= 0x04;
  }
}

const int SHIFT_STEPS = 14; // 0~7 올라갔다가 6~1 내려오는 한 바퀴
const int ALT_STEPS = 2;

byte frame_shift(int s) {
  int i = s;
  if (s >= 8) {
    i = SHIFT_STEPS - s;
  }
  return 1 << i;
}

byte frame_alt(int s, byte pattern) {
  if (s % 2 == 0) {
    return pattern;
  }
  return ~pattern;
}

const byte TASK_DEFAULT = 0;
const byte TASK_ALT1 = 1;
const byte TASK_ALT2 = 2;
const byte TASK_SHIFT = 3;
const byte TASK_COUNT = 4;

const int INTERVAL_MIN = 100;
const int INTERVAL_MAX = 500;
const int INTERVAL_STEP = 50;

byte task = TASK_DEFAULT;
int step = 0;
int saved_step[TASK_COUNT] = {0, 0, 0, 0}; // 동작마다 떠날 때 기억해 두는 위치
int interval = 300;

unsigned long last_frame = 0;

void draw_frame() {
  if (task == TASK_ALT1) {
    led_write(frame_alt(step, 0xF0));
  } else if (task == TASK_ALT2) {
    led_write(frame_alt(step, 0xC3));
  } else if (task == TASK_SHIFT) {
    led_write(frame_shift(step));
  } else {
    led_write(frame_alt(step, 0xAA));
  }
}

int task_steps() {
  if (task == TASK_SHIFT) {
    return SHIFT_STEPS;
  }
  return ALT_STEPS;
}

void next_task() {
  saved_step[task] = step; // 지금 켜져 있는 위치를 기억

  task = (task + 1) % TASK_COUNT;

  step = saved_step[task]; // 기억한 위치부터 다시 시작

  draw_frame();
  last_frame = millis();
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, HIGH); // active-low: OUTPUT 전환 전에 꺼진 값을 미리 설정
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);

  noInterrupts();
  prev_pb = PINB & 0x1C;
  PCMSK0 |= (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT4); // D10, D11, D12
  PCIFR |= (1 << PCIF0);                                   // 남아 있던 플래그 제거
  PCICR |= (1 << PCIE0);
  interrupts();

  draw_frame();
  last_frame = millis();
}

void loop() {

  // 깃발을 읽고 비우는 동안에는 인터럽트를 잠시 막는다

  noInterrupts();

  byte event = sw_event;
  sw_event = 0;

  interrupts();

  if (event & 0x01) { // SW1 : 동작 변경
    next_task();
  }

  if (event & 0x02) { // SW2 : 주기 증가
    interval += INTERVAL_STEP;
    if (interval > INTERVAL_MAX) {
      interval = INTERVAL_MIN;
    }
  }

  if (event & 0x04) { // SW3 : 주기 감소
    interval -= INTERVAL_STEP;
    if (interval < INTERVAL_MIN) {
      interval = INTERVAL_MAX;
    }
  }

  if (millis() - last_frame >= (unsigned long)interval) {
    last_frame += interval;
    step = (step + 1) % task_steps();
    draw_frame();
  }
}
