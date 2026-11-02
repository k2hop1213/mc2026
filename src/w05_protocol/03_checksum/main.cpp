/*
  프로토콜 연습 3단계 - 체크섬(SUM)

  형식 :  | STX(0x02) | CMD | LEN | DATA x LEN | SUM | ETX(0x03) |

  SUM = CMD + LEN + DATA 를 모두 더한 값의 하위 8비트

  CMD
    0x01   DATA[0] = 횟수   전체 점멸
    0x02   DATA[0] = 패턴   패턴과 그 반전을 3회 번갈아 표시
    0x04   DATA[0] = 횟수   왕복 시프트

  예) 02 01 01 03 05 03 을 보내면 전체 점멸 3회   (01 + 01 + 03 = 05)
      scripts/packet_tool.py 에서 p 01 03 으로 보낸다. pb 는 일부러 틀린 체크섬을 보낸다.

  전송 중 바이트가 바뀌면 받은 SUM 과 직접 더한 값이 달라져서 패킷을 버린다.

  받는 쪽은 상태 기계(FSM)로 처리한다.
    WAIT_STX  : STX 가 오면 GET_CMD 로
    GET_CMD   : 한 바이트를 명령으로 저장하고 GET_LEN 으로
    GET_LEN   : 길이를 저장. 0 이면 GET_SUM, 너무 크면 오류, 아니면 GET_DATA 로
    GET_DATA  : LEN 개를 모두 받으면 GET_SUM 으로
    GET_SUM   : 직접 더한 값과 같으면 WAIT_ETX 로, 다르면 오류
    WAIT_ETX  : ETX 면 실행 준비, 아니면 오류. 어느 쪽이든 WAIT_STX 로

  패킷 도중 100ms 동안 아무것도 안 오면 잘린 패킷으로 보고 WAIT_STX 로 돌아간다.

  LED : D2 ~ D9 (Active Low)   UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <stdarg.h>
#include <string.h>

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

// ── 보내기 ─────────────────────────────────────────

// 한 바이트 보내기
void tx_char(char txChar) {
  Serial.write(txChar);
}

// 바이트 스트림 보내기 - tx_char 를 len 번 부른다
void tx_str(char *txStr, int len) {
  for (int i = 0; i < len; i++) {
    tx_char(txStr[i]);
  }
}

// ── 나만의 printf ──────────────────────────────────
void Printf(const char *fmt, ...) {
  char text[96];
  va_list args;

  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  tx_str(text, strlen(text));
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

const int MAX_DATA = 8;              // DATA 버퍼 크기
const unsigned long RX_TIMEOUT = 100; // 패킷 도중 기다리는 시간 (ms)

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
    Printf("RX timeout\n");
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
        Printf("RX error : LEN %d\n", data_len);
        state = WAIT_STX;
      } else if (data_len == 0) {
        state = GET_SUM;
      } else {
        state = GET_DATA;
      }
    } else if (state == GET_DATA) {
      data[data_idx] = c; // 값이 ETX 와 같아도 여기서는 자료다
      data_idx++;
      sum += c;

      if (data_idx == data_len) {
        state = GET_SUM;
      }
    } else if (state == GET_SUM) {
      if (c == sum) {
        state = WAIT_ETX;
      } else {
        Printf("RX error : SUM 0x%02X, expected 0x%02X\n", c, sum);
        state = WAIT_STX;
      }
    } else {
      if (c == ETX) {
        ready = true;
      } else {
        Printf("RX error : no ETX\n");
      }
      state = WAIT_STX; // 어느 쪽이든 처음으로 돌아간다
    }
  }
}

void run_command() {
  // 지금 명령은 모두 DATA 1 바이트를 쓴다
  if (command != CMD_ALL_ONOFF && command != CMD_ALT && command != CMD_SHIFT) {
    Printf("Invalid Command 0x%02X\n", command);
  } else if (data_len != 1) {
    Printf("Invalid LEN %d\n", data_len);
  } else if (command == CMD_ALL_ONOFF) {
    Printf("CMD 1 : LED ALL ON OFF x %d\n", data[0]);
    led_all_onoff(data[0]);
  } else if (command == CMD_ALT) {
    Printf("CMD 2 : LED ALTERNATING 0x%02X\n", data[0]);
    led_alternating(data[0], 3);
  } else {
    Printf("CMD 4 : LED SHIFT x %d\n", data[0]);
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
