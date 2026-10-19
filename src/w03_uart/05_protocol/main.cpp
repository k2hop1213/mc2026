/*
  4차시 UART - 프로토콜  [ASCII 버전]

  형식 :  | STX('<') | command(1byte) | ETX('>') |

  command
    '1'   LED ALL ON OFF
    '2'   LED ALTERNATING ON OFF
    '4'   LED SHIFT

  예) <1> 을 보내면 전체 점멸

  받는 쪽은 상태 기계(FSM)로 처리한다.
    0 보통        : STX 가 오면 1 로
    1 자료 수신   : 한 바이트를 명령으로 저장하고 2 로
    2 끝 기다림   : ETX 면 실행, 아니면 오류

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
// 자료와 값만 다르고 구조는 똑같다.
const byte STX = '<'; // 자료 : 0x02
const byte ETX = '>'; // 자료 : 0x03

const byte CMD_ALL_ONOFF = '1'; // 자료 : 0x01
const byte CMD_ALT = '2';       // 자료 : 0x02
const byte CMD_SHIFT = '4';     // 자료 : 0x04

// 0 : 보통,  1 : 자료 수신,  2 : 끝 기다림
int decode = 0;
byte command = 0;
bool ready = false;

void rx_decode() {
  // 패킷 하나가 완성되면 멈춘다. 나머지는 실행이 끝난 뒤 버퍼에서 이어 읽는다.
  while (Serial.available() > 0 && !ready) {
    byte c = Serial.read();

    if (decode == 0) {
      if (c == STX) {
        decode = 1;
      } // 시작 표시를 만나면 다음 상태로
    } // 그 밖의 글자(Enter 포함)는 무시
    else if (decode == 1) {
      command = c; // 한 바이트를 명령으로 저장
      decode = 2;
    } else {
      if (c == ETX) {
        ready = true;
      } // 끝 표시면 실행 준비
      else {
        Printf("RX error\n");
      }
      decode = 0; // 어느 쪽이든 보통 상태로 돌아간다
    }
  }
}

void run_command(byte cd) {
  if (cd == CMD_ALL_ONOFF) {
    Printf("CMD 1 : LED ALL ON OFF\n");
    led_all_onoff(3);
  } else if (cd == CMD_ALT) {
    Printf("CMD 2 : LED ALTERNATING ON OFF\n");
    led_alternating(0xAA, 3);
  } else if (cd == CMD_SHIFT) {
    Printf("CMD 4 : LED SHIFT\n");
    led_shift(3);
  } else {
    Printf("Invalid Command %c\n", cd);
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
    run_command(command);

    ready = false;
    command = 0;
    led_write(ALL_OFF);
  }
}
