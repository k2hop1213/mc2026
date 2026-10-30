/*
  시험 대비 도구 - UART 바이트 모니터

  조교 UART 프로그램이 실제로 "어떤 바이트를" 보내는지 확인한다.
  프로그램이 COM 포트를 잡고 있으면 시리얼 모니터를 같이 못 여니까 LED 로 보여 준다.

  동작
    받은 바이트를 하나씩 LED 8개에 2진수로 0.6초씩 띄운다. (D2 = bit0)
    바이트 사이에는 0.15초 동안 LED 를 끈다.
    ECHO_HEX 가 true 면 받은 바이트를 "RX 02 31 33 03" 처럼 16진수로 되돌려 보낸다.
    (조교 프로그램에 받은 글자를 보여 주는 창이 있으면 거기서 보인다)

  읽는 법
    0000 0010 (0x02) = STX       0000 0011 (0x03) = ETX
    0011 0001 (0x31) = 문자 '1'   0000 0001 (0x01) = 숫자 1
    0011 1100 (0x3C) = '<'        0011 1110 (0x3E) = '>'
    0000 1101 (0x0D) = '\r'       0000 1010 (0x0A) = '\n'

  LED : D2 ~ D9 (Active Low)   UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const long BAUD = 9600;     // 조교 프로그램 설정과 맞출 것
const bool ECHO_HEX = true; // 받은 바이트를 16진수로 되돌려 보낼지

const int SHOW_MS = 600; // 바이트 하나를 보여 주는 시간
const int GAP_MS = 150;  // 바이트 사이 꺼 두는 시간

// 비트를 LED 핀으로 뿌린다.  bit0 = D2
// 회로가 Active Low 라서 뒤집어 내보낸다
void led_write(byte value) {
  value = ~value;

  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

// ── 4차시에서 만든 Printf ──────────────────────────
void tx_char(char txChar) {
  Serial.write(txChar);
}

void tx_str(char* txStr, int len) {
  for (int i = 0; i < len; i++) {
    tx_char(txStr[i]);
  }
}

void Printf(const char* fmt, ...) {
  char text[96];
  va_list args;

  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  tx_str(text, strlen(text));
}

// ── 받은 바이트 보관 (링 버퍼) ─────────────────────
// LED 로 천천히 보여 주는 동안 들어오는 바이트를 잃지 않도록 모아 둔다.
const int BUF_SIZE = 64;
byte buf[BUF_SIZE];
int head = 0; // 다음에 넣을 자리
int tail = 0; // 다음에 꺼낼 자리

void collect() {
  bool got = false;

  while (Serial.available() > 0) {
    byte c = Serial.read();
    int next = (head + 1) % BUF_SIZE;

    if (next != tail) { // 가득 차면 버린다
      buf[head] = c;
      head = next;
    }

    if (ECHO_HEX) {
      if (!got) { Printf("RX"); }
      Printf(" %02X", c);
    }
    got = true;
  }

  if (ECHO_HEX && got) { Printf("\r\n"); }
}

// 기다리는 동안에도 계속 받아 둔다
void wait_ms(int ms) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    collect();
  }
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }

  // 켜졌다는 표시 : 전체 한 번 깜빡
  led_write(0xFF);
  delay(300);
  led_write(0x00);

  Serial.begin(BAUD);
}

void loop() {
  collect();

  if (tail != head) {
    byte c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;

    led_write(c);
    wait_ms(SHOW_MS);
    led_write(0x00);
    wait_ms(GAP_MS);
  }
}
