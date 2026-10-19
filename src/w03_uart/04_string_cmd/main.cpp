/*
  4차시 UART - (4) 문자열 명령으로 원격 제어

  PC 에서 보낸 한 줄('\n' 으로 끝남)을 모아서 명령인지 확인한다.
    "LED_ALL_ONOFF"  전체 점멸
    "LED_ALT"        번갈아 점멸
    "LED_SHIFT"      왕복 이동
  명령이 없으면 받은 문자열을 그대로 돌려보낸다.

  LED : D2 ~ D9 (Active Low)   UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <avr/interrupt.h>
#include <string.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const int SW1_PIN = 10; // PB2 - PCINT2
const int SW2_PIN = 11; // PB3 - PCINT3
const int SW3_PIN = 12; // PB4 - PCINT4

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
//
// 자료의 레지스터 방식과 하는 일이 같다.
//   while (!(UCSR0A & (1 << UDRE0)));   // UDR0 이 빌 때까지 대기
//   UDR0 = txChar;                      // 쓰면 TX 선으로 나간다
void tx_char(char txChar) {
  Serial.write(txChar);
}

// 바이트 스트림 보내기 - tx_char 를 len 번 부른다
void tx_str(char *txStr, int len) {
  for (int i = 0; i < len; i++) {
    tx_char(txStr[i]);
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

// ── 수신 버퍼 ──────────────────────────────────────
// 한 글자가 아니라 문자열을 받으려면, 끝 글자가 올 때까지 모아 둬야 한다.
const int MAX_BUF = 48;

char rx_buf[MAX_BUF];
int buf_len = 0;
bool line_ready = false; // 한 줄이 완성됐는가

// Arduino 는 RX 인터럽트와 수신 버퍼를 이미 제공한다.
// 자료의 ISR(USART_RX_vect) 는 HardwareSerial 이 이미 쓰고 있어
// 직접 정의하면 중복 정의 오류가 난다. 대신 받은 것을 여기서 모은다.
void rx_collect() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n') { // 끝 글자 -> 한 줄 완성
      rx_buf[buf_len] = '\0';
      line_ready = true;
      return;
    }

    if (c == '\r') {
      continue;
    } // 윈도우 줄바꿈의 앞 글자는 버린다

    if (buf_len < MAX_BUF - 1) {
      rx_buf[buf_len] = c;
      buf_len = buf_len + 1;
    }
  }
}

void rx_clear() {
  buf_len = 0;
  line_ready = false;
}

// 받은 줄에 명령이 들어 있으면 그 동작을 한다
void command(char *buff, int len) {
  if (strstr(buff, "LED_ALL_ONOFF") != NULL) {
    tx_str(buff, len);
    tx_char('\n');
    led_all_onoff(3);
  } else if (strstr(buff, "LED_ALT") != NULL) {
    tx_str(buff, len);
    tx_char('\n');
    led_alternating(0xAA, 3);
  } else if (strstr(buff, "LED_SHIFT") != NULL) {
    tx_str(buff, len);
    tx_char('\n');
    led_shift(3);
  } else {
    tx_str(buff, len); // 모르는 명령은 그대로 돌려보낸다
    tx_char('\n');
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
  rx_collect();

  if (line_ready) {
    command(rx_buf, buf_len);
    rx_clear();

    led_write(ALL_OFF); // 동작이 끝나면 LED 끄기
  }
}
