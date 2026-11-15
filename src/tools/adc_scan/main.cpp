/*
  시험 대비 도구 - 아날로그 핀 전체 점검
  A0 ~ A5 값을 200ms 마다 한 줄로 출력한다. 센서가 어느 핀에 꽂혔는지 찾을 때 쓴다.
  시리얼 : 9600bps
*/

#include <Arduino.h>

void setup() {
  Serial.begin(9600);
}

void loop() {
  for (int i = 0; i < 6; i++) {
    Serial.print('A');
    Serial.print(i);
    Serial.print(' ');
    Serial.print(analogRead(A0 + i));
    Serial.print(i < 5 ? "  " : "\r\n");
  }
  delay(200);
}
