/*
  6차시 Timer - 실습  음계(scale) 출력하기

  부저 : A2 -> 부저(+),  부저(-) -> GND      (수동 부저)
  LED  : +5V -> 220옴 -> LED -> D2 ~ D9   (Active Low)
  SW1  : D10 (PB2)   한 음 올림
  SW2  : D11 (PB3)   한 음 내림
  SW3  : D12 (PB4)   소리 켜기 / 끄기 (토글)

  조건
    '도' 부터 시작한다.  (가운데 도, C4)
    타이머로 낼 수 있는 옥타브 끝까지 올라가고 내려간다.  (C0 ~ B7)
    LED : 꺼져 있으면 모두 꺼짐,  켜져 있으면 지금 계이름 자리 하나 (도 = D2 ... 시 = D8)

  음 번호 = 옥타브 x 7 + 계이름
    C4 (도) = 4 x 7 + 0 = 28
  주파수는 가장 높은 옥타브(7) 표를 반으로 나눠 가며 구한다.
    한 옥타브 내려갈 때마다 주파수는 절반   ->   >> 1
    C4 = 2093 >> (7 - 4) = 261 Hz

  낼 수 있는 가장 낮은 음
    OCR1A 는 16비트라 최대 65535  ->  16MHz / (16 x 65536) = 약 15Hz
    C0 = 2093 >> 7 = 16Hz 까지 된다.  (너무 낮아서 부저로는 딱딱 소리만 난다)
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

// 7 옥타브의 도 레 미 파 솔 라 시  (Hz)
const unsigned int note_freq[7] = {2093, 2349, 2637, 2794, 3136, 3520, 3951};
const char* note_name[7] = {"C", "D", "E", "F", "G", "A", "B"};

const int NOTE_MIN = 0;          // C0
const int NOTE_MAX = 7 * 8 - 1;  // B7
const int NOTE_START = 4 * 7 + 0; // C4 (가운데 도)

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

// 음 번호 -> 주파수(Hz)
unsigned int freq_of(int note) {
  int octave = note / 7; // 0 ~ 7
  int degree = note % 7; // 0(도) ~ 6(시)

  return note_freq[degree] >> (7 - octave);
}

int note = NOTE_START; // 지금 음
bool playing = false;  // 소리가 나고 있는가

// 지금 상태를 부저, LED, 시리얼에 반영한다
void update() {
  unsigned int f = freq_of(note);

  if (playing) {
    play(OCR_OF(f));
    led_write(1 << (note % 7)); // 계이름 자리 LED 하나
  } else {
    stop();
    led_write(0x00);
  }

  Printf("%s%d  %4u Hz  %s\n", note_name[note % 7], note / 7, f, playing ? "ON" : "OFF");
}

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
  update(); // 처음 상태 : C4, 꺼짐
}

void loop() {
  noInterrupts();
  byte event = sw_event;
  sw_event = 0;
  interrupts();

  if (event == 0) {
    return;
  }

  if (event & 0x01) { // SW1 : 한 음 올림
    if (note < NOTE_MAX) {
      note = note + 1;
    }
  }

  if (event & 0x02) { // SW2 : 한 음 내림
    if (note > NOTE_MIN) {
      note = note - 1;
    }
  }

  if (event & 0x04) { // SW3 : 켜기 / 끄기
    playing = !playing;
  }

  update();
}
