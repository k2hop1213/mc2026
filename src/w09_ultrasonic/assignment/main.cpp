/*
  9차시 초음파센서 - 과제  주차 경보기

  HC-SR04
    VCC  -> 5V
    Trig -> A4 (PC4)
    Echo -> D12 (PB4)
    GND  -> GND
  부저 : A2 -> 부저(+),  부저(-) -> GND   (수동 부저, 6차시 배선 그대로)

  동작
    30cm 보다 멀다     : 조용
    10 ~ 30cm          : 삑삑.  가까울수록 빨리
    10cm 미만          : 계속 울림
    측정 실패 (범위 밖) : 조용

  구현
    소리 : 6차시처럼 Timer1 CTC 비교 일치 ISR 에서 A2 를 뒤집는다.  (1kHz)
           sound_on() / sound_off() 는 인터럽트를 켜고 끄기만 한다.
    박자 : delay() 로 기다리면 그동안 거리를 못 잰다.
           millis() 로 "언제 켜고 끌지" 만 확인하고 loop 는 계속 돈다.
           삑 한 번 = 60ms 켜짐,  주기 = 거리 x 20ms   (10cm -> 200ms,  30cm -> 600ms)
    측정 : 60ms 마다 한 번 (HC-SR04 측정 간격 60ms 이상)
*/

#include <Arduino.h>
#include <avr/interrupt.h>

const int TRIG_PIN = A4;   // PC4
const int ECHO_PIN = 12;   // PB4
const int BUZZER_PIN = A2; // PC2

const unsigned long TIMEOUT_US = 25000; // 약 4.3m

const float NEAR_CM = 10;  // 이보다 가까우면 계속 울림
const float FAR_CM = 30;   // 이보다 멀면 조용

const unsigned long MEASURE_MS = 60; // 측정 간격
const unsigned long BEEP_ON_MS = 60; // 삑 한 번의 길이
const unsigned long MS_PER_CM = 20;  // 삑삑 주기 = 거리 x 20ms

// ── 부저 : Timer1 CTC, A2 직접 토글 (6차시 03_buzzer 와 같음) ──
//   OCR1A = 16MHz / (2 x 8 x 주파수) - 1      분주 8
const unsigned int OCR_1KHZ = F_CPU / (16UL * 1000) - 1; // 999

ISR(TIMER1_COMPA_vect) {
  PORTC = PORTC ^ (1 << PORTC2); // A2 뒤집기
}

void timer1_init() {
  TCCR1A = 0x00;                       // 아두이노가 미리 해 둔 PWM 설정을 덮어쓴다
  TCCR1B = (1 << WGM12) | (1 << CS11); // CTC (TOP = OCR1A), 분주 8
  OCR1A = OCR_1KHZ;
}

bool sounding = false;

void sound_on() {
  if (sounding) {
    return; // 이미 울리는 중이면 건드리지 않는다
  }
  noInterrupts();
  TCNT1 = 0;
  TIMSK1 = TIMSK1 | (1 << OCIE1A);
  interrupts();
  sounding = true;
}

void sound_off() {
  if (!sounding) {
    return;
  }
  TIMSK1 = TIMSK1 & ~(1 << OCIE1A);
  PORTC = PORTC & ~(1 << PORTC2); // 핀은 LOW 로 두고 끝낸다
  sounding = false;
}

// ── 초음파 ──────────────────────────────────────
// 거리 (cm).  범위를 벗어나면 -1
float read_distance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long t = pulseIn(ECHO_PIN, HIGH, TIMEOUT_US);
  if (t == 0) {
    return -1;
  }
  return t / 58.0;
}

// ── 메인 ────────────────────────────────────────
float distance = -1;
unsigned long last_measure = 0;
unsigned long beep_start = 0; // 이번 삑이 시작된 시각

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  timer1_init();
  Serial.println("Parking alarm start");
}

void loop() {
  unsigned long now = millis();

  // 1. 60ms 마다 거리 측정
  if (now - last_measure >= MEASURE_MS) {
    last_measure = now;
    distance = read_distance();

    Serial.print("Distance : ");
    Serial.print(distance, 1);
    Serial.println(" cm");
  }

  // 2. 거리에 따라 부저
  if (distance < 0 || distance > FAR_CM) {
    sound_off(); // 멀거나 측정 실패 : 조용
  } else if (distance < NEAR_CM) {
    sound_on(); // 아주 가까움 : 계속
  } else {
    // 10 ~ 30cm : 주기마다 BEEP_ON_MS 동안만 울림
    unsigned long period = (unsigned long)(distance * MS_PER_CM);

    if (now - beep_start >= period) {
      beep_start = now; // 새 삑 시작
    }
    if (now - beep_start < BEEP_ON_MS) {
      sound_on();
    } else {
      sound_off();
    }
  }
}
