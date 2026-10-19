#include <Arduino.h>

// =============================================================
//  1. 하드웨어 설정
// =============================================================
const int FIRST_PIN = 2; // LED: D2 ~ D9 (active-low)
const int LED_COUNT = 8;
const int FIRST_SW = 10; // 스위치: D10 ~ D12 (PB2 ~ PB4)
const int SW_COUNT = 3;

const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

// =============================================================
//  2. LED 출력
// =============================================================
void led_write(byte value) {
  value = ~value; // active-low 이므로 반전
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

// =============================================================
//  3. 패턴 프레임 (step -> LED 값)
// =============================================================
// 한 칸씩 왕복 이동 (주기 14)
byte frame_shift(int s) {
  int n = s % 14;
  int i = n;
  if (n >= 8) {
    i = 14 - n;
  }
  return 1 << i;
}

// pattern 과 그 반전을 번갈아 출력
byte frame_alt(int s, byte pattern) {
  if (s % 2 == 0) {
    return pattern;
  }
  return ~pattern;
}

// 양 끝에서 가운데로 모였다 벌어짐 (주기 6)
byte frame_cross(int s) {
  int n = s % 6;
  int i = n;
  if (n >= 4) {
    i = 6 - n;
  }
  return (1 << i) | (1 << (LED_COUNT - 1 - i));
}

// =============================================================
//  4. 작업(task) 정의
// =============================================================
const byte TASK_DEFAULT = 0;
const byte TASK_SHIFT = 1;
const byte TASK_ALT = 2;
const byte TASK_CROSS = 3;

const int DEFAULT_STEPS = 2;
const int SHIFT_STEPS = 14 * 2 + 1;
const int ALT_STEPS = 2 * 2;
const int CROSS_STEPS = 6 * 2 + 1;

byte task = TASK_DEFAULT;
int step = 0;
int interval = 500;
unsigned long last_frame = 0;

void draw_frame() {
  if (task == TASK_SHIFT) {
    led_write(frame_shift(step));
  } else if (task == TASK_ALT) {
    led_write(frame_alt(step, 0xC3));
  } else if (task == TASK_CROSS) {
    led_write(frame_cross(step));
  } else {
    led_write(frame_alt(step, ALL_ON));
  }
}

bool task_finished() {
  if (task == TASK_SHIFT) {
    return step >= SHIFT_STEPS;
  } else if (task == TASK_ALT) {
    return step >= ALT_STEPS;
  } else if (task == TASK_CROSS) {
    return step >= CROSS_STEPS;
  }
  return false; // DEFAULT 는 끝나지 않음
}

// =============================================================
//  5. 작업 스택 (중단된 작업 저장 / 복원)
// =============================================================
const int STACK_MAX = 4;

byte stack_task[STACK_MAX];
int stack_step[STACK_MAX];
int stack_interval[STACK_MAX];
int stack_left[STACK_MAX];
int stack_top = 0;

void push_task() {
  if (stack_top >= STACK_MAX) {
    return;
  }

  unsigned long used = millis() - last_frame;
  int left = interval - (int)used;
  if (left < 0) {
    left = 0;
  }

  stack_task[stack_top] = task;
  stack_step[stack_top] = step;
  stack_interval[stack_top] = interval;
  stack_left[stack_top] = left;
  stack_top = stack_top + 1;
}

void restore_task() {
  int left;
  if (stack_top > 0) {
    stack_top = stack_top - 1;
    task = stack_task[stack_top];
    step = stack_step[stack_top];
    interval = stack_interval[stack_top];
    left = stack_left[stack_top];

  } else {
    task = TASK_DEFAULT;
    step = 0;
    interval = 500;
    left = interval;
  }

  draw_frame();
  last_frame = millis() - (interval - left); // 남은 시간만큼 기다린 뒤 다음 프레임
}

// =============================================================
//  6. 스위치 인터럽트 (PCINT0: PB2 ~ PB4)
// =============================================================
volatile byte sw_event = 0;
volatile byte prev_pb = 0;
volatile unsigned long last_press = 0;

ISR(PCINT0_vect) {
  byte now = PINB & 0x1C;
  byte fell = prev_pb & ~now; // HIGH -> LOW 로 바뀐 핀 = 눌린 스위치
  prev_pb = now;

  if (fell == 0) {
    return; // 뗄 때 생기는 변화는 무시
  }
  if (millis() - last_press < 200) {
    return; // 채터링 무시
  }
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

// =============================================================
//  7. setup
// =============================================================
void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, HIGH); // active-low: OUTPUT 전환 전에 꺼진 값을 미리 설정
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  for (int i = 0; i < SW_COUNT; i++) {
    pinMode(FIRST_SW + i, INPUT_PULLUP);
  }

  prev_pb = PINB & 0x1C; // 첫 입력을 놓치지 않도록 현재 핀 상태로 초기화
  PCICR = PCICR | (1 << PCIE0);
  PCMSK0 = PCMSK0 | (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT4);

  draw_frame();
  last_frame = millis();
}

// =============================================================
//  8. loop
// =============================================================
void loop() {
  // 깃발을 읽고 비우는 동안에는 인터럽트를 잠시 막는다
  noInterrupts();
  byte event = sw_event;
  sw_event = 0;
  interrupts();

  // --- 스위치 입력: 현재 작업을 저장하고 새 작업 시작 ---
  if (event != 0) {
    push_task();
    if (event & 0x01) {
      task = TASK_SHIFT;
      interval = 80;
    }
    if (event & 0x02) {
      task = TASK_ALT;
      interval = 300;
    }
    if (event & 0x04) {
      task = TASK_CROSS;
      interval = 150;
    }
    step = 0;
    draw_frame();
    last_frame = millis();
    return;
  }

  // --- 프레임 진행 ---
  if (millis() - last_frame >= (unsigned long)interval) {
    step = step + 1;
    if (task == TASK_DEFAULT && step >= DEFAULT_STEPS) {
      step = 0;
    }
    if (task_finished()) {
      restore_task(); // 끝나면 이전 작업으로 복귀
      return;
    }
    draw_frame();
    last_frame = millis();
  }
}
