/*
  10차시 서보모터 - 예제 1  펄스 폭으로 0, 90, 180도 움직이기

  SG90
    주황 (Signal) -> A0
    빨강 (V+)     -> 외부 5V (+)
    갈색 (GND)    -> 외부 전원 GND, UNO GND 모두

  A0 (= PC0) 는 아날로그 핀이지만 디지털 출력으로도 쓸 수 있다.

  제어 신호
    20ms 마다 HIGH 펄스를 하나씩 보낸다.
    HIGH 인 시간 (펄스 폭) 이 각도를 정한다.

       0도 :  500us
      90도 : 1450us
     180도 : 2400us

  끝 각도에서 소리가 나며 떨리면 PULSE_0 은 키우고 PULSE_180 은 줄인다.
*/

#include <Arduino.h>

const int SERVO_PIN = A0; // 서보 Signal : A0 (PC0)

const int PULSE_0 = 500; // 각도별 펄스 폭 (us)
const int PULSE_90 = 1450;
const int PULSE_180 = 2400;

// 20ms 길이의 주기 하나 : HIGH pulse_us + 나머지는 LOW
void send_pulse(unsigned int pulse_us) {
  // delayMicroseconds() 는 인터럽트가 끼어든 만큼 길어지므로
  // micros() 로 시작 시각부터 잰다
  unsigned long start = micros();

  digitalWrite(SERVO_PIN, HIGH);
  while (micros() - start < pulse_us) {
  } // 펄스 폭만큼 HIGH

  digitalWrite(SERVO_PIN, LOW);
  while (micros() - start < 20000) {
  } // 20ms 가 될 때까지 LOW
}

// 펄스를 50번 = 1초 동안 보낸다
void hold_1sec(unsigned int pulse_us) {
  for (int i = 0; i < 50; i++) {
    send_pulse(pulse_us);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  Serial.println("Servo start");
}

void loop() {
  Serial.println("0 deg");
  hold_1sec(PULSE_0);

  Serial.println("90 deg");
  hold_1sec(PULSE_90);

  Serial.println("180 deg");
  hold_1sec(PULSE_180);

  Serial.println("90 deg");
  hold_1sec(PULSE_90);
}
