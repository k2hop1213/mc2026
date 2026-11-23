/*
  10차시 서보모터 - 예제 3  가변저항으로 서보 위치 제어

  SG90
    주황 (Signal) -> A0 (PC0)
    빨강 (V+)     -> 외부 5V (+)
    갈색 (GND)    -> 외부 전원 GND, UNO GND 모두
  가변저항 (B10K) : A1   (5차시 배선 그대로)

  동작
    가변저항 0 ~ 5V -> ADC 0 ~ 1023 -> 각도 0 ~ 180도

  떨림 줄이기
    1. ADC 를 8번 읽어서 평균을 낸다
    2. 각도가 2도 이상 바뀔 때만 서보를 움직인다

  펄스는 예제 2 와 같이 Timer1 인터럽트가 만든다.
*/

#include <Arduino.h>
#include <avr/interrupt.h>

const int POT_PIN = A1; // 가변저항 : A1 (PC1)

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

// ── 가변저항 ──────────────────────────────────
int read_pot() {
  int sum = 0; // 1023 x 8 = 8184 라서 int 로 충분
  for (int i = 0; i < 8; i++) {
    sum = sum + analogRead(POT_PIN);
  }
  return (sum + 4) / 8; // + 4 는 반올림
}

// ADC 값 0 ~ 1023 -> 각도 0 ~ 180
int to_angle(int adc) {
  return ((long)adc * 180 + 511) / 1023; // + 511 은 반올림
}

// ─────────────────────────────────────────────
int servo_angle = 0;        // 지금 서보에 보낸 각도
unsigned long last_print = 0; // 마지막으로 출력한 시각

void setup() {
  Serial.begin(9600);

  // 켜지마자 가변저항 위치로 가도록 각도를 먼저 정한다
  servo_angle = to_angle(read_pot());
  servo_write(servo_angle);
  servo_begin();
}

void loop() {
  int adc = read_pot();
  int angle = to_angle(adc);

  // 2도 이상 바뀌었거나 양 끝 (0, 180도) 이면 움직인다
  if (abs(angle - servo_angle) >= 2 || angle == 0 || angle == 180) {
    servo_angle = angle;
    servo_write(servo_angle);
  }

  if (millis() - last_print >= 200) {
    last_print = millis();
    Serial.print("ADC : ");
    Serial.print(adc);
    Serial.print("   Angle : ");
    Serial.println(servo_angle);
  }

  delay(20); // 서보 신호 주기와 같은 20ms
}
