/*
  11차시 DC 모터 - 예제 1  정회전, 역회전

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
    정회전 2초 -> 정지 1초 -> 역회전 2초 -> 정지 1초 -> 반복

    AIN1  AIN2  PWMA
     H     L     H     정회전
     L     H     H     역회전
     L     L     H     정지

  정회전인데 원하는 방향과 반대로 돌면 전원을 끄고 모터 선 2개를 맞바꾼다.
*/

#include <Arduino.h>

const int PWMA_PIN = 9;  // 속도   : D9 (PB1)
const int AIN1_PIN = A4; // 방향 1 : A4 (PC4)
const int AIN2_PIN = A5; // 방향 2 : A5 (PC5)
const int STBY_PIN = A0; // Enable : A0 (PC0)

const int RUN_MS = 2000;  // 도는 시간
const int STOP_MS = 1000; // 멈추는 시간

void motor_forward() {
  digitalWrite(AIN2_PIN, LOW);
  digitalWrite(AIN1_PIN, HIGH);
}

void motor_reverse() {
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, HIGH);
}

void motor_stop() {
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
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

  digitalWrite(STBY_PIN, HIGH); // 드라이버 켜기
}

void loop() {
  Serial.println("Forward");
  motor_forward();
  delay(RUN_MS);

  Serial.println("Stop");
  motor_stop();
  delay(STOP_MS); // 방향은 멈춘 뒤에 바꾼다

  Serial.println("Reverse");
  motor_reverse();
  delay(RUN_MS);

  Serial.println("Stop");
  motor_stop();
  delay(STOP_MS);
}
