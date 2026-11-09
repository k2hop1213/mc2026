/*
  6차시 Timer - 예제  1초마다 LED 300ms 점등  [Normal 모드, 오버플로 인터럽트]

  LED : +5V -> 220옴 -> LED -> D2 ~ D9   (Active Low)

  자료는 Timer0 를 쓰지만 아두이노에서는 Timer0 를 millis(), delay() 가 쓰고 있다.
  ISR(TIMER0_OVF_vect) 를 직접 만들면 중복 정의 오류가 난다.
  그래서 구조가 같은 8비트 타이머인 Timer2 를 쓴다.

  타이머 계산  (16MHz, 분주 1024)
    한 칸 = 1024 / 16MHz = 64us
    255 칸까지 세도 16ms 남짓이라 1초를 한 번에 만들 수 없다.
    -> 약 5ms 마다 인터럽트를 내고, 200 번 세면 1초
    TCNT2 를 0xFF - 0x4E (= 177) 에서 시작하면
    256 - 177 = 79 칸 뒤에 넘친다.   79 x 64us = 약 5ms

  자료와 달리 ISR 안에서 delay 를 쓰지 않는다.
  ISR 안에서는 delay 가 동작하지 않고, 그동안 다른 인터럽트도 막힌다.
  300ms 도 횟수로 센다.   60 번 x 5ms = 300ms
*/

#include <Arduino.h>
#include <avr/interrupt.h>

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

const byte TCNT_START = 0xFF - 0x4E; // 여기서 시작하면 약 5ms 뒤에 넘친다

// 비트를 LED 핀으로 뿌린다.  bit0 = D2
// 회로가 Active Low 라서 뒤집어 내보낸다
void led_write(byte value) {
  value = ~value;

  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

volatile byte tov_count = 0; // 오버플로 횟수.  5ms 마다 1 씩

// 타이머가 255 를 넘어 0 으로 돌아갈 때마다 불린다
ISR(TIMER2_OVF_vect) {
  TCNT2 = TCNT_START; // Normal 모드는 0 부터 다시 세므로 시작값을 다시 넣는다

  if (tov_count == 0) {
    led_write(ALL_ON);
  } // 0ms   : 켬
  if (tov_count == 60) {
    led_write(ALL_OFF);
  } // 300ms : 끔

  tov_count = tov_count + 1;
  if (tov_count == 200) {
    tov_count = 0;
  } // 1초 : 처음으로
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  led_write(ALL_OFF);

  noInterrupts();

  // 아두이노는 시작할 때 Timer2 를 PWM 용으로 미리 설정해 둔다.
  // 그래서 |= 로 더하지 않고 = 로 통째로 덮어써야 한다.
  TCCR2A = 0x00;                                       // Normal 모드
  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);    // 분주 1024
  //  ※ Timer2 의 1024 는 CS22, CS21, CS20 셋 다 1 이다.  (Timer0 는 CS02, CS00)

  TCNT2 = TCNT_START;
  TIFR2 = (1 << TOV2);   // 남아 있던 플래그 지우기.  1 을 써야 지워진다
  TIMSK2 = (1 << TOIE2); // 오버플로 인터럽트 켜기

  interrupts();
}

void loop() {
  // 할 일이 없다.  LED 는 타이머 인터럽트가 알아서 켜고 끈다.
}
