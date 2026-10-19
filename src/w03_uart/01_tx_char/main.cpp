/*
  4차시 UART - (1) 한 문자 보내기  [폴링 버전]

  버튼을 누를 때마다 PC 로 한 바이트를 보낸다.
    SW1 -> 'A'      SW2 -> 'B'

  Serial 객체를 쓰지 않고 레지스터를 직접 다룬다.
  보낼 수 있을 때까지 UDRE0 비트를 계속 확인하는 것이 폴링이다.

  LED : D2 ~ D9   스위치 : D10 ~ D12   UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <avr/interrupt.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const int SW1_PIN = 10; // PB2 - PCINT2
const int SW2_PIN = 11; // PB3 - PCINT3
const int SW3_PIN = 12; // PB4 - PCINT4

const byte ALL_OFF = 0x00;

// 16MHz 에서 9600bps 를 만드는 값
//   UBRR = F_CPU / (16 * 9600) - 1 = 103
#define BAUD_RATE 103

void uart_init() {
  UBRR0H = (unsigned char)(BAUD_RATE >> 8); // 통신 속도 설정
  UBRR0L = (unsigned char)(BAUD_RATE);

  UCSR0B = (1 << TXEN0) | (1 << RXEN0);   // 송신, 수신 켜기
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 데이터 8비트
}

// 한 바이트 보내기 (폴링)
void tx_char(char txChar) {
  while (!(UCSR0A & (1 << UDRE0))) {
  } // 보낼 수 있을 때까지 계속 확인
  UDR0 = txChar; // 쓰면 TX 선으로 나간다
}

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

  if (millis() - last_press < 200) {
    return;
  } // 채터링 무시
  last_press = millis();

  if (fell & 0x04) {
    sw_event = sw_event | 0x01;
  } // SW1
  if (fell & 0x08) {
    sw_event = sw_event | 0x02;
  } // SW2
  if (fell & 0x10) {
    sw_event = sw_event | 0x04;
  } // SW3
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
  uart_init();
}

void loop() {
  noInterrupts();
  byte event = sw_event;
  sw_event = 0;
  interrupts();

  if (event & 0x01) {
    tx_char('A');
  }
  if (event & 0x02) {
    tx_char('B');
  }
}
