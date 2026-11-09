/*
  6차시 Timer - 실습  앞의 예제를 CTC 모드로  [비교 일치 인터럽트]

  LED : +5V -> 220옴 -> LED -> D2 ~ D9   (Active Low)

  Normal 모드 : 0 ~ 255 까지 세고 넘칠 때 인터럽트
                원하는 주기를 만들려면 ISR 에서 TCNT 를 매번 다시 넣어야 한다
  CTC 모드    : 0 ~ OCR2A 까지 세고 같아지면 인터럽트,  TCNT 는 저절로 0 이 된다
                ISR 에서 TCNT 를 건드릴 필요가 없다

  타이머 계산  (16MHz, 분주 1024,  한 칸 = 64us)
    OCR2A = 0x4E (78)  ->  0 ~ 78 까지 79 칸  ->  79 x 64us = 약 5ms
    앞 예제와 같은 주기다.   200 번 = 1초,  60 번 = 300ms
*/

#include <Arduino.h>
#include <avr/interrupt.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

// 비트를 LED 핀으로 뿌린다.  bit0 = D2
// 회로가 Active Low 라서 뒤집어 내보낸다
void led_write(byte value) {
  value = ~value;

  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

volatile byte cmp_count = 0; // 비교 일치 횟수.  5ms 마다 1 씩

// TCNT2 가 OCR2A 와 같아질 때마다 불린다
ISR(TIMER2_COMPA_vect) {
  // Normal 모드와 달리 TCNT2 를 다시 넣지 않는다.  하드웨어가 0 으로 돌려 준다.

  if (cmp_count == 0) {
    led_write(ALL_ON);
  } // 0ms   : 켬
  if (cmp_count == 60) {
    led_write(ALL_OFF);
  } // 300ms : 끔

  cmp_count = cmp_count + 1;
  if (cmp_count == 200) {
    cmp_count = 0;
  } // 1초 : 처음으로
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  led_write(ALL_OFF);

  noInterrupts();

  // 아두이노가 미리 해 둔 PWM 설정을 = 로 덮어쓴다
  TCCR2A = (1 << WGM21);                               // CTC 모드 (TOP = OCR2A)
  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);    // 분주 1024

  OCR2A = 0x4E; // 0 ~ 78 까지 세면 약 5ms
  TCNT2 = 0;
  TIFR2 = (1 << OCF2A);   // 남아 있던 플래그 지우기
  TIMSK2 = (1 << OCIE2A); // 비교 일치 A 인터럽트 켜기

  interrupts();
}

void loop() {
  // 할 일이 없다.  LED 는 타이머 인터럽트가 알아서 켜고 끈다.
}
