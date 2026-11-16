/*
  8차시 온습도센서 - 추가  라이브러리 없이 DHT22 직접 읽기

  자료의 Single-Bus 통신 과정을 그대로 코드로 옮긴 것.
  라이브러리가 안에서 하는 일을 이해하려고 만든 예제다.  (예제 1 과 결과가 같아야 한다)

  DHT22 모듈 : S -> A5 (PC5),  VCC -> 5V,  GND -> GND   (모듈에 풀업 저항 있음)

  통신 순서
    ① 시작  : MCU 가 1ms 이상 LOW 로 당긴 뒤 놓는다 (풀업이 HIGH 로 올림, 20 ~ 40us)
    ② 응답  : 센서가 LOW 80us -> HIGH 80us
    ③ 데이터 : 40비트, MSB 부터.  비트마다 LOW 50us 뒤 HIGH
                HIGH 26 ~ 28us = 0,   HIGH 70us = 1   -> 40us 를 기준으로 가른다
    ④ 종료  : LOW 50us 뒤 HIGH

  데이터 5바이트
    [0] 습도 상위  [1] 습도 하위  [2] 온도 상위  [3] 온도 하위  [4] 체크섬
    습도 = 16비트 ÷ 10,   온도 = 아래 15비트 ÷ 10,  bit15 = 1 이면 영하
    체크섬 = [0] + [1] + [2] + [3] 의 하위 8비트
*/

#include <Arduino.h>

const int DHT_PIN = A5; // PC5

const unsigned int TIMEOUT = 0xFFFF;

// 핀 값 읽기 : digitalRead 보다 빠르게 레지스터로 직접
byte dht_level() {
  return (PINC >> PINC5) & 1;
}

// 핀이 level 인 동안 기다리고, 그 시간이 몇 us 였는지 돌려준다.
// limit_us 를 넘기면 센서가 응답하지 않는 것이므로 TIMEOUT
unsigned int wait_while(byte level, unsigned int limit_us) {
  unsigned long start = micros();
  while (dht_level() == level) {
    if (micros() - start > limit_us) {
      return TIMEOUT;
    }
  }
  return micros() - start;
}

// 40비트를 data[5] 에 받는다.  성공 0,  실패하면 어느 단계에서 멈췄는지 번호
byte dht_read(byte data[5]) {
  for (int i = 0; i < 5; i++) {
    data[i] = 0;
  }

  // ① 시작 신호 : LOW 2ms (1ms 이상) 뒤 놓기
  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(2);
  pinMode(DHT_PIN, INPUT_PULLUP); // 놓으면 풀업이 HIGH 로 올린다
  delayMicroseconds(10);          // 선이 HIGH 로 올라갈 시간

  // MCU HIGH 20 ~ 40us : 센서가 LOW 로 당길 때까지
  if (wait_while(HIGH, 100) == TIMEOUT) {
    return 1;
  }

  // ② 응답 : LOW 80us, HIGH 80us
  if (wait_while(LOW, 100) == TIMEOUT) {
    return 2;
  }
  if (wait_while(HIGH, 100) == TIMEOUT) {
    return 3;
  }

  // ③ 데이터 40비트
  for (int i = 0; i < 40; i++) {
    if (wait_while(LOW, 100) == TIMEOUT) { // 비트 앞 LOW 50us
      return 4;
    }
    unsigned int high = wait_while(HIGH, 100); // 이 길이로 0 / 1 판단
    if (high == TIMEOUT) {
      return 5;
    }

    data[i / 8] = data[i / 8] << 1; // MSB 부터 오므로 왼쪽으로 밀고
    if (high > 40) {
      data[i / 8] = data[i / 8] | 1; // 길면 1
    }
  }

  // ④ 종료 LOW 50us 는 기다리지 않아도 된다

  // 체크섬 확인
  byte sum = data[0] + data[1] + data[2] + data[3]; // byte 라서 하위 8비트만 남는다
  if (sum != data[4]) {
    return 6;
  }
  return 0;
}

// 0.1 단위 정수를 "12.3" 처럼 출력
void print_tenth(int value) {
  if (value < 0) {
    Serial.print('-');
    value = -value;
  }
  Serial.print(value / 10);
  Serial.print('.');
  Serial.print(value % 10);
}

void setup() {
  Serial.begin(9600);
  pinMode(DHT_PIN, INPUT_PULLUP);
  Serial.println("DHT22 raw start");
  delay(1000); // 전원 직후 센서 안정 시간
}

void loop() {
  delay(2000); // 2초 이상 간격

  byte data[5];
  byte err = dht_read(data);

  // 받은 5바이트를 16진수로 (자료의 "데이터 해석 예시" 와 비교해 보기)
  for (int i = 0; i < 5; i++) {
    Serial.print("0x");
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }

  if (err != 0) {
    Serial.print(" read fail (step ");
    Serial.print(err);
    Serial.println(")");
    return;
  }

  int humi = (data[0] << 8) | data[1];          // 0.1 %RH 단위
  int temp = ((data[2] & 0x7F) << 8) | data[3]; // 아래 15비트, 0.1 ℃ 단위
  if (data[2] & 0x80) {                         // bit15 = 1 이면 영하
    temp = -temp;
  }

  Serial.print(" Humidity : ");
  print_tenth(humi);
  Serial.print(" %   Temperature : ");
  print_tenth(temp);
  Serial.println(" C");
}
