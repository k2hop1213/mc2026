/*
  11차시 DC 모터 - 예제 2  브레이크, 정지, 대기 비교

  TB6612FNG
    PWMA     -> D9 (PB1)
    AIN1     -> A4 (PC4)
    AIN2     -> A5 (PC5)
    STBY     -> A0 (PC0)
    VCC      -> UNO 5V
    VM       -> 외부 5V 어댑터 (+)
    GND      -> 어댑터 GND, UNO GND 모두
    AO1, AO2 -> 모터

  동작
    3초 회전 -> 브레이크 2초
    3초 회전 -> 정지 2초
    3초 회전 -> 대기 2초 -> 반복

              AIN1  AIN2  PWMA  STBY
    브레이크    H     H     H     H     모터 양 끝을 GND 쪽으로 단락
    정지        L     L     H     H     모터 양 끝을 끊는다
    대기        -     -     -     L     드라이버 출력을 끈다 (절전)

  축에 테이프를 작게 붙이면 멈추는 모습이 잘 보인다.
*/

#include <Arduino.h>

const int PWMA_PIN = 9;  // 속도   : D9 (PB1)
const int AIN1_PIN = A4; // 방향 1 : A4 (PC4)
const int AIN2_PIN = A5; // 방향 2 : A5 (PC5)
const int STBY_PIN = A0; // Enable : A0 (PC0)

const int RUN_MS = 3000;  // 도는 시간
const int STOP_MS = 2000; // 멈춘 모습을 보는 시간

void motor_forward() {
  digitalWrite(AIN2_PIN, LOW);
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(STBY_PIN, HIGH); // 대기에서 돌아올 때를 위해 다시 켠다
}

void motor_brake() {
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, HIGH);
}

void motor_stop() {
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
}

void motor_standby() {
  // AIN1, AIN2 가 정회전 상태 그대로여도 출력이 꺼진다
  digitalWrite(STBY_PIN, LOW);
}

void run_3sec() {
  Serial.println("Run");
  motor_forward();
  delay(RUN_MS);
}

void setup() {
  Serial.begin(9600);

  pinMode(STBY_PIN, OUTPUT);
  digitalWrite(STBY_PIN, LOW); // 설정이 끝날 때까지 대기 상태

  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);

  digitalWrite(PWMA_PIN, HIGH); // 속도는 100% 로 고정
  motor_stop();
}

void loop() {
  run_3sec();
  Serial.println("  -> Brake");
  motor_brake();
  delay(STOP_MS);

  run_3sec();
  Serial.println("  -> Stop");
  motor_stop();
  delay(STOP_MS);

  run_3sec();
  Serial.println("  -> Standby");
  motor_standby();
  delay(STOP_MS);
}
