/*
  4차시 UART - (3) 나만의 printf

  변수나 레지스터 값을 확인하려면 서식 있는 출력이 필요하다.
  stdio 의 vsnprintf 를 이용해 Printf 를 직접 만든다.

  켤 때 초기화 메시지를 출력한다.
*/

#include <Arduino.h>
#include <avr/interrupt.h>
#include <stdarg.h>
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

// ── 스위치 : 핀 변화 인터럽트 ─────────────────────────
// D2, D3 은 LED 가 쓰고 있어 외부 인터럽트(INT0/INT1)를 못 쓴다.
// 자료의 SW1(INT0), SW2(INT1) 자리를 PCINT 로 대신한다.
volatile byte sw_event = 0;
volatile unsigned long last_press = 0;
volatile byte prev_pb = 0x1C; // PB2, PB3, PB4

ISR(PCINT0_vect) {
  byte now = PINB & 0x1C;
  byte fell = prev_pb & ~now; // 1 -> 0 으로 떨어진 비트 = 눌린 것
  prev_pb = now;

  if (millis() - last_press < 200) { return; } // 채터링 무시
  last_press = millis();

  if (fell & 0x04) { sw_event = sw_event | 0x01; } // SW1
  if (fell & 0x08) { sw_event = sw_event | 0x02; } // SW2
  if (fell & 0x10) { sw_event = sw_event | 0x04; } // SW3
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

// ── 나만의 printf ──────────────────────────────────
// vsnprintf 가 서식을 글자로 바꿔 주면, 그것을 tx_str 로 내보낸다.
// 실제 printf 도 이런 식으로 만들어져 있다.
void Printf(const char *fmt, ...) {
  char text[96];
  va_list args;

  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  tx_str(text, strlen(text));
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }

  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);

  PCICR = PCICR | (1 << PCIE0);
  PCMSK0 = PCMSK0 | (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT4);

  led_write(ALL_OFF);

  Serial.begin(9600);
  delay(100); // 모니터가 붙을 시간을 조금 준다

  // 자료는 EIMSK 를 출력하지만, 이 회로는 PCINT 를 쓰므로 PCMSK0 을 출력한다.
  Printf("**************************************\n");
  Printf("Clock Speed = %lu\n", (unsigned long)F_CPU);
  Printf("PCMSK0      = 0x%02X\n", PCMSK0);
  Printf("**************************************\n");
}

void loop() {
  noInterrupts();
  byte event = sw_event;
  sw_event = 0;
  interrupts();

  if (event & 0x01) { Printf("SW1 pushed, millis = %lu\n", millis()); }
  if (event & 0x02) { Printf("SW2 pushed, millis = %lu\n", millis()); }
  if (event & 0x04) { Printf("SW3 pushed, millis = %lu\n", millis()); }
}
