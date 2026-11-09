/*
  6차시 Timer - 예제  부저로 라(A, 440Hz) 소리 내기  [Timer1 CTC 모드]

  부저 : A2 -> 부저(+),  부저(-) -> GND      (수동 부저)
  SW1  : D10 (PB2)   소리 켜기 (Play)
  SW2  : D11 (PB3)   소리 끄기 (Stop)

  자료는 외부 인터럽트(INT0, INT1)로 스위치를 받지만
  우리 보드는 D2, D3 이 LED 라서 3차시처럼 PCINT 를 쓴다.
  자료처럼 ISR 안에서 Printf 하지 않고, 깃발만 세운 뒤 loop 에서 출력한다.
*/

#include <Arduino.h>
#include <avr/interrupt.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const int SW1_PIN = 10;     // PB2 - PCINT2
const int SW2_PIN = 11;     // PB3 - PCINT3
const int SW3_PIN = 12;     // PB4 - PCINT4
const int BUZZER_PIN = A2;  // PC2

// 비트를 LED 핀으로 뿌린다.  bit0 = D2
// 회로가 Active Low 라서 뒤집어 내보낸다
void led_write(byte value) {
  value = ~value;

  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

// ── 스위치 : 핀 변화 인터럽트 (3차시와 같음) ──────────────
// D2, D3 은 LED 가 쓰고 있어 외부 인터럽트(INT0/INT1)를 못 쓴다.
volatile byte sw_event = 0;
volatile unsigned long last_press = 0;
volatile byte prev_pb = 0x1C; // PB2, PB3, PB4

ISR(PCINT0_vect) {
  byte now = PINB & 0x1C;
  byte fell = prev_pb & ~now; // 1 -> 0 으로 떨어진 비트 = 눌린 것
  prev_pb = now;

  if (millis() - last_press < 200) {
    return;
  } // 채터링 무시
  last_press = millis();

  if (fell & 0x04) {
    sw_event = sw_event | 0x01;
  } // SW1
  if (fell & 0x08) {
    sw_event = sw_event | 0x02;
  } // SW2
  if (fell & 0x10) {
    sw_event = sw_event | 0x04;
  } // SW3
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

// ── 부저 : Timer1 CTC 모드 ──────────────────────────
// Timer1 출력핀 OC1A(D9), OC1B(D10) 는 LED 와 스위치가 쓰고 있다.
// 그래서 하드웨어 토글 출력 대신, 비교 일치 ISR 에서 A2 를 직접 뒤집는다.
//
//   부저 주파수 = 16MHz / (2 x 분주 x (1 + OCR1A))      분주 8
//   OCR1A       = 16MHz / (2 x 8 x 주파수) - 1
//   2 를 곱하는 이유 : ISR 두 번(켜기, 끄기)이 모여야 한 주기가 된다
#define OCR_OF(f) (unsigned int)(F_CPU / (16UL * (f)) - 1)

ISR(TIMER1_COMPA_vect) {
  PORTC = PORTC ^ (1 << PORTC2); // A2 (PC2) 뒤집기
}

void timer1_init() {
  // 아두이노는 시작할 때 Timer1 을 PWM 용으로 미리 설정해 둔다.
  // 그래서 |= 로 더하지 않고 = 로 통째로 덮어쓴다.
  TCCR1A = 0x00;
  TCCR1B = (1 << WGM12) | (1 << CS11); // CTC 모드 (TOP = OCR1A), 분주 8
}

// 소리 내기
void play(unsigned int ocr) {
  noInterrupts();
  OCR1A = ocr;
  TCNT1 = 0;                            // 이미 새 OCR1A 보다 커져 있으면 65535 까지 돌기 때문
  TIMSK1 = TIMSK1 | (1 << OCIE1A);      // 비교 일치 인터럽트 켜기
  interrupts();
}

// 소리 끄기
void stop() {
  TIMSK1 = TIMSK1 & ~(1 << OCIE1A); // 인터럽트 끄기
  PORTC = PORTC & ~(1 << PORTC2);   // 부저 핀 LOW
}

const unsigned int OCR_LA = OCR_OF(440); // 라 : 16MHz / (16 x 440) - 1 = 2271

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  led_write(0x00);

  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  PCICR = PCICR | (1 << PCIE0);
  PCMSK0 = PCMSK0 | (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT4);

  timer1_init();
  Serial.begin(9600);
  Printf("SW1 : Play,  SW2 : Stop   (OCR1A = %u)\n", OCR_LA);
}

void loop() {
  noInterrupts();
  byte event = sw_event;
  sw_event = 0;
  interrupts();

  if (event & 0x01) { // SW1 : 켜기
    play(OCR_LA);
    Printf("01 Clicked : PLAY\n");
  }

  if (event & 0x02) { // SW2 : 끄기
    stop();
    Printf("02 Clicked : STOP\n");
  }
}
