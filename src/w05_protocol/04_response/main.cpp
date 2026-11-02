/*
  프로토콜 연습 4단계 - 응답 패킷

  받기 :  | STX(0x02) | CMD | LEN | DATA x LEN | SUM | ETX(0x03) |
  보내기도 형식이 같다. SUM = CMD + LEN + DATA 의 하위 8비트

  CMD
    0x01   DATA[0] = 횟수   전체 점멸
    0x02   DATA[0] = 패턴   패턴과 그 반전을 3회 번갈아 표시
    0x04   DATA[0] = 횟수   왕복 시프트

  응답
    성공 : CMD | 0x80, LEN = 0            예) 명령 01 -> 02 81 00 81 03
    실패 : 0xFF, LEN = 1, DATA = 오류 번호  예) SUM 오류 -> 02 FF 01 03 03 03

  오류 번호
    1 LEN 이 너무 큼   2 ETX 아님   3 SUM 틀림
    4 시간 초과        5 없는 명령  6 명령과 LEN 이 안 맞음

  PC 프로그램은 정해진 패킷만 알아듣는다고 보고 글자(Printf)는 보내지 않는다.
  응답은 LED 동작을 시작하기 전에 보낸다. PC 가 동작이 끝날 때까지 기다리지 않게 한다.

  LED : D2 ~ D9 (Active Low)   UART : D0(RX), D1(TX)
*/

#include <Arduino.h>

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

// ── LED 동작 ───────────────────────────────────────
void led_all_onoff(int count) {
  for (int i = 0; i < count; i++) {
    led_write(ALL_ON);
    delay(300);
    led_write(ALL_OFF);
    delay(300);
  }
}

void led_alternating(byte pattern, int count) {
  for (int i = 0; i < count; i++) {
    led_write(pattern);
    delay(300);
    led_write(~pattern);
    delay(300);
  }
}

void led_shift(int count) {
  for (int c = 0; c < count; c++) {
    for (int i = 0; i < LED_COUNT; i++) {
      led_write(1 << i);
      delay(80);
    }
    for (int i = LED_COUNT - 2; i > 0; i--) {
      led_write(1 << i);
      delay(80);
    }
  }

  led_write(1 << 0);
  delay(80);
}

// ── 프로토콜 ───────────────────────────────────────
const byte STX = 0x02;
const byte ETX = 0x03;

const byte CMD_ALL_ONOFF = 0x01;
const byte CMD_ALT = 0x02;
const byte CMD_SHIFT = 0x04;

const byte ACK_FLAG = 0x80; // 성공 응답 = CMD | ACK_FLAG
const byte CMD_ERROR = 0xFF;

const byte ERR_LEN = 1;
const byte ERR_ETX = 2;
const byte ERR_SUM = 3;
const byte ERR_TIMEOUT = 4;
const byte ERR_CMD = 5;
const byte ERR_DATA_LEN = 6;

const int MAX_DATA = 8;               // DATA 버퍼 크기
const unsigned long RX_TIMEOUT = 100; // 패킷 도중 기다리는 시간 (ms)

// ── 보내기 ─────────────────────────────────────────

// 패킷 하나를 보낸다. 체크섬은 여기서 계산한다
void send_packet(byte cmd, const byte *payload, byte len) {
  byte checksum = cmd + len;

  Serial.write(STX);
  Serial.write(cmd);
  Serial.write(len);

  for (byte i = 0; i < len; i++) {
    Serial.write(payload[i]);
    checksum += payload[i];
  }

  Serial.write(checksum);
  Serial.write(ETX);
}

void send_ack(byte cmd) {
  send_packet(cmd | ACK_FLAG, NULL, 0);
}

void send_error(byte code) {
  send_packet(CMD_ERROR, &code, 1);
}

// ── 받기 ───────────────────────────────────────────
enum { WAIT_STX, GET_CMD, GET_LEN, GET_DATA, GET_SUM, WAIT_ETX };
byte state = WAIT_STX;

byte command = 0;
byte data_len = 0;
byte data_idx = 0;
byte data[MAX_DATA];
byte sum = 0; // 받으면서 직접 더하는 체크섬
bool ready = false;
unsigned long last_rx = 0;

void rx_decode() {
  // 패킷이 잘리면 처음부터 다시 기다린다
  if (state != WAIT_STX && millis() - last_rx > RX_TIMEOUT) {
    send_error(ERR_TIMEOUT);
    state = WAIT_STX;
  }

  // 패킷 하나가 완성되면 멈춘다. 나머지는 실행이 끝난 뒤 버퍼에서 이어 읽는다.
  while (Serial.available() > 0 && !ready) {
    byte c = Serial.read();
    last_rx = millis();

    if (state == WAIT_STX) {
      if (c == STX) {
        state = GET_CMD;
      } // 그 밖의 바이트는 무시
    } else if (state == GET_CMD) {
      command = c;
      sum = c; // 체크섬은 CMD 부터 더한다
      state = GET_LEN;
    } else if (state == GET_LEN) {
      data_len = c;
      data_idx = 0;
      sum += c;

      if (data_len > MAX_DATA) { // 버퍼를 넘으면 받지 않는다
        send_error(ERR_LEN);
        state = WAIT_STX;
      } else if (data_len == 0) {
        state = GET_SUM;
      } else {
        state = GET_DATA;
      }
    } else if (state == GET_DATA) {
      data[data_idx] = c; // 값이 STX, ETX 와 같아도 여기서는 자료다
      data_idx++;
      sum += c;

      if (data_idx == data_len) {
        state = GET_SUM;
      }
    } else if (state == GET_SUM) {
      if (c == sum) {
        state = WAIT_ETX;
      } else {
        send_error(ERR_SUM);
        state = WAIT_STX;
      }
    } else {
      if (c == ETX) {
        ready = true;
      } else {
        send_error(ERR_ETX);
      }
      state = WAIT_STX; // 어느 쪽이든 처음으로 돌아간다
    }
  }
}

void run_command() {
  // 지금 명령은 모두 DATA 1 바이트를 쓴다
  if (command != CMD_ALL_ONOFF && command != CMD_ALT && command != CMD_SHIFT) {
    send_error(ERR_CMD);
    return;
  }
  if (data_len != 1) {
    send_error(ERR_DATA_LEN);
    return;
  }

  send_ack(command);

  if (command == CMD_ALL_ONOFF) {
    led_all_onoff(data[0]);
  } else if (command == CMD_ALT) {
    led_alternating(data[0], 3);
  } else {
    led_shift(data[0]);
  }
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }

  led_write(ALL_OFF);
  Serial.begin(9600);
}

void loop() {
  rx_decode();

  if (ready) {
    run_command();

    ready = false;
    led_write(ALL_OFF);
  }
}
