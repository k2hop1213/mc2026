/*
  10차시 서보모터 - 예제 2  타이머 인터럽트로 0 ~ 180도 왕복

  SG90
    주황 (Signal) -> A0 (PC0)
    빨강 (V+)     -> 외부 5V (+)
    갈색 (GND)    -> 외부 전원 GND, UNO GND 모두

  예제 1 은 펄스를 만드는 동안 다른 일을 할 수 없다.
  Timer1 인터럽트에 맡기면 loop() 는 각도만 정하면 된다.

  Timer1
    CTC 모드, 8분주 -> 1칸 = 0.5us
    20ms = 40000칸  -> OCR1A = 39999

    비교 일치 A (20ms 마다) : A0 를 HIGH
    비교 일치 B (펄스 폭 뒤) : A0 를 LOW

  각도 -> 펄스 폭
    펄스 폭 (us) = 500 + 각도 x 1900 / 180
*/

#include <Arduino.h>
#include <avr/interrupt.h>

const unsigned int PULSE_MIN = 500;  //   0도 펄스 폭 (us)
const unsigned int PULSE_MAX = 2400; // 180도 펄스 폭 (us)

volatile unsigned int pulse_ticks = PULSE_MIN * 2; // 1us = 2칸

// ── 인터럽트 : 펄스 만들기 ──────────────────────────
ISR(TIMER1_COMPA_vect) {
  PORTC |= (1 << PORTC0); // 펄스 시작
  OCR1B = pulse_ticks;    // 이번 펄스가 끝날 시각
}

ISR(TIMER1_COMPB_vect) {
  PORTC &= ~(1 << PORTC0); // 펄스 끝
}

// ── 서보 함수 ──────────────────────────────────
void servo_begin() {
  DDRC |= (1 << DDC0);     // A0 출력
  PORTC &= ~(1 << PORTC0); // 처음에는 LOW

  noInterrupts();
  TCCR1A = 0;                          // 핀 직접 출력은 안 쓴다
  TCCR1B = (1 << WGM12) | (1 << CS11); // CTC, 8분주
  TCNT1 = 0;
  OCR1A = 39999; // 20ms
  OCR1B = pulse_ticks;
  TIFR1 = (1 << OCF1A) | (1 << OCF1B);     // 남아 있는 플래그 지우기
  TIMSK1 = (1 << OCIE1A) | (1 << OCIE1B); // 인터럽트 A, B 허용
  interrupts();
}

void servo_write(int angle) {
  // 곱한 값이 int 범위 (32767) 를 넘으므로 long 으로 계산
  unsigned int us = PULSE_MIN + (long)angle * (PULSE_MAX - PULSE_MIN) / 180;

  noInterrupts(); // 2바이트 값이라 쓰는 동안 인터럽트를 막는다
  pulse_ticks = us * 2;
  interrupts();
}

// ─────────────────────────────────────────────
const int STEP_MS = 20; // 1도 움직이는 시간.  크게 하면 느려진다

void setup() {
  Serial.begin(9600);
  servo_begin();
  servo_write(0);
  delay(1000); // 0도로 갈 시간
}

void loop() {
  Serial.println("0 -> 180");
  for (int angle = 0; angle <= 180; angle++) {
    servo_write(angle);
    delay(STEP_MS);
  }
  delay(500);

  Serial.println("180 -> 0");
  for (int angle = 180; angle >= 0; angle--) {
    servo_write(angle);
    delay(STEP_MS);
  }
  delay(500);
}
