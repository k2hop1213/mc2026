/*
  11차시 DC 모터 - 예제 3  PWM 속도 제어

  TB6612FNG
    PWMA     -> D9 (PB1, OC1A)
    AIN1     -> A4 (PC4)
    AIN2     -> A5 (PC5)
    STBY     -> A0 (PC0)
    VCC      -> UNO 5V
    VM       -> 외부 5V 어댑터 (+)
    GND      -> 어댑터 GND, UNO GND 모두
    AO1, AO2 -> 모터
  LED : D2 (Active Low)

  Timer1
    Fast PWM (모드 14), 분주 1, TOP = ICR1 = 799
    16MHz / (799 + 1) = 20kHz

    듀티 (%) = (OCR1A + 1) / 800 x 100

  동작
    듀티 25 -> 50 -> 75 -> 100% 순서로
      2초 : 속도가 일정해질 때까지 기다림
      5초 : D2 LED 가 켜져 있는 동안 바퀴 수를 센다
      3초 : 정지

    RPM = 5초 동안 돈 바퀴 수 x 12
*/

#include <Arduino.h>

const int PWMA_PIN = 9;  // 속도   : D9 (PB1, OC1A)
const int AIN1_PIN = A4; // 방향 1 : A4 (PC4)
const int AIN2_PIN = A5; // 방향 2 : A5 (PC5)
const int STBY_PIN = A0; // Enable : A0 (PC0)
const int LED_PIN = 2;   // 세는 시간 표시 : D2

const unsigned int PWM_TOP = 799; // 20kHz

// ── PWM ──────────────────────────────────────
void pwm_begin() {
  TCCR1A = (1 << COM1A1) | (1 << WGM11); // OC1A 출력, 모드 14
  TCCR1B = (1 << WGM13) | (1 << WGM12);  // 모드 14, 타이머는 아직 정지
  ICR1 = PWM_TOP;
  OCR1A = PWM_TOP; // 100%
  TCNT1 = 0;
  TCCR1B |= (1 << CS10); // 분주 1 로 시작
}

// percent : 1 ~ 100
void set_duty(int percent) {
  // 곱한 값이 int 범위 (32767) 를 넘으므로 long 으로 계산
  OCR1A = (long)percent * (PWM_TOP + 1) / 100 - 1;
}

// ── 모터 ──────────────────────────────────────
void motor_forward(int percent) {
  set_duty(percent);
  digitalWrite(AIN2_PIN, LOW);
  digitalWrite(AIN1_PIN, HIGH);
}

void motor_stop() {
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  set_duty(100); // 정지 모드는 PWMA 가 H 여야 한다
}

// ── 듀티 하나를 측정 ──────────────────────────────
void measure(int percent) {
  Serial.print("Duty ");
  Serial.print(percent);
  Serial.println("%");

  motor_forward(percent);
  delay(2000); // 속도가 일정해질 때까지

  Serial.println("  count start");
  digitalWrite(LED_PIN, LOW); // LED 켜기
  delay(5000);
  digitalWrite(LED_PIN, HIGH); // LED 끄기
  Serial.println("  count end");

  motor_stop();
  delay(3000); // 바퀴 수를 적는 시간
}

void setup() {
  Serial.begin(9600);

  pinMode(STBY_PIN, OUTPUT);
  digitalWrite(STBY_PIN, LOW); // 설정이 끝날 때까지 대기 상태

  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH); // LED 는 꺼진 채로 시작
  pinMode(LED_PIN, OUTPUT);

  pwm_begin();
  motor_stop();

  digitalWrite(STBY_PIN, HIGH); // 드라이버 켜기
}

void loop() {
  measure(25);
  measure(50);
  measure(75);
  measure(100);
}
