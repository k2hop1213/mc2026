/*
  시험용 통합 틀 - 레지스터 버전 (Arduino 함수 없이 avr-gcc 만으로 빌드)

  src/exam/base/main.cpp 와 패킷, 명령, 응답이 같다. 명령 표는 그쪽을 본다.
  시험 프로그램이 Arduino 함수(Serial, analogRead, millis)를 못 쓸 때 이 파일을 쓴다.
  파일 하나로 끝나며 C, C++ 어느 쪽으로도 컴파일된다.

  시험 때 고치는 곳
    1. 패킷 형식     : "패킷 형식" 상수
    2. 송신과 진단   : "송신과 진단" 상수 (SEND_ACK, SEND_ERROR, DEBUG_LED)
    3. 명령 번호     : "명령" 상수와 COMMANDS 표
    4. 응답 형식     : send_ack, send_error, send_adc, send_sw_events
    5. 명령별 동작   : run_command

  레지스터
    UART   : UBRR0, UCSR0A/B/C, UDR0   수신은 인터럽트로 버퍼에 넣고, 송신은 폴링
    ADC    : ADMUX, ADCSRA, ADC        AVcc 기준, 128 분주
    Timer0 : ms 카운터 (1kHz)
    Timer1 : LED 밝기 (2kHz)
    Timer2 : 부저 (주파수의 2배로 PC0 를 뒤집는다)
    PCINT0 : 스위치

  LED : D2 ~ D7 = PD2 ~ PD7, D8 ~ D9 = PB0 ~ PB1 (Active Low)
  SW  : D10 ~ D12 = PB2 ~ PB4    가변저항 : A1 = ADC1    조도센서 : A3 = ADC3
  부저 : A0 = PC0                UART : D0(RX), D1(TX)

  base/main.cpp 와 다른 점 : 부저는 123Hz 미만이면 소리가 나지 않는다
*/

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t byte;

// ── 패킷 형식 ──────────────────────────────────────
static const long BAUD = 9600;
static const byte UART_PARITY = 0;    // 0 = 없음, 2 = 짝수, 3 = 홀수
static const byte UART_STOP_BITS = 1; // 1 또는 2
static const byte STX = 0x02;
static const byte ETX = 0x03;
static const bool USE_LEN = true;  // false 면 COMMANDS 표의 길이로 받는다
static const bool USE_SUM = true;
static const bool USE_ETX = true;
static const bool SUM_XOR = false; // true 면 더하기 대신 XOR

// ── 송신과 진단 ─────────────────────────────────────
static const bool SEND_ACK = true;   // false 면 성공 응답을 보내지 않는다
static const bool SEND_ERROR = true; // false 면 오류 응답을 보내지 않는다. 패킷은 똑같이 버린다
static const bool DEBUG_LED = false; // true 면 LED 에 CMD, 오류는 0x80 | 오류 번호만 표시한다

// ── 명령 ───────────────────────────────────────────
#define CMD_BLINK 0x01
#define CMD_ALT 0x02
#define CMD_PATTERN 0x03
#define CMD_SHIFT 0x04
#define CMD_ADC 0x05
#define CMD_BRIGHT 0x06
#define CMD_TONE 0x07
#define CMD_MELODY 0x08
#define CMD_STREAM 0x09
#define CMD_BAR 0x0A
#define CMD_STOP 0x0B

#define CMD_SW_EVENT 0x90
#define CMD_ADC_EVENT 0x91

#define ACK_FLAG 0x80 // 성공 응답 = CMD | ACK_FLAG
#define CMD_ERROR 0xFF

typedef struct {
  byte cmd;
  byte len;
} Command;

static const Command COMMANDS[] = {
    {CMD_BLINK, 1}, {CMD_ALT, 1},    {CMD_PATTERN, 1}, {CMD_SHIFT, 1},
    {CMD_ADC, 1},   {CMD_BRIGHT, 1}, {CMD_TONE, 4},    {CMD_MELODY, 0},
    {CMD_STREAM, 2}, {CMD_BAR, 1},   {CMD_STOP, 0},
};
#define COMMAND_COUNT ((int)(sizeof(COMMANDS) / sizeof(COMMANDS[0])))

#define ERR_LEN 1
#define ERR_ETX 2
#define ERR_SUM 3
#define ERR_TIMEOUT 4
#define ERR_CMD 5
#define ERR_DATA_LEN 6
#define ERR_CHANNEL 7

#define MAX_DATA 16    // DATA 버퍼 크기
#define RX_TIMEOUT 100 // 패킷 도중 기다리는 시간 (ms)

#define ALL_ON 0xFF
#define ALL_OFF 0x00

// ── 시간 : Timer0 ──────────────────────────────────
static volatile unsigned long ms_count = 0;

ISR(TIMER0_COMPA_vect) {
  ms_count++;
}

static void ms_timer_init(void) {
  TCCR0A = (1 << WGM01);              // CTC
  TCCR0B = (1 << CS01) | (1 << CS00); // 64 분주
  OCR0A = 249;                        // 16MHz / 64 / 250 = 1kHz
  TIMSK0 = (1 << OCIE0A);
}

// 4 바이트라서 읽는 도중 인터럽트가 끼면 값이 깨진다
static unsigned long now_ms(void) {
  byte sreg = SREG;
  cli();
  unsigned long value = ms_count;
  SREG = sreg;
  return value;
}

// ── LED 출력 : Timer1 ──────────────────────────────
#define LEVEL_MAX 16 // 밝기 단계

static volatile byte led_value = ALL_OFF;
static volatile byte led_level = LEVEL_MAX;

// 16 번 중 led_level 번만 켜서 밝기를 만든다 (125Hz)
ISR(TIMER1_COMPA_vect) {
  static byte phase = 0;

  phase = (phase + 1) & 0x0F;

  // 회로가 Active Low 라서 뒤집어 내보낸다
  byte out = (byte)~(phase < led_level ? led_value : ALL_OFF);

  PORTD = (PORTD & 0x03) | (byte)(out << 2); // D2 ~ D7 = PD2 ~ PD7
  PORTB = (PORTB & 0xFC) | (out >> 6);       // D8, D9  = PB0, PB1
}

static void led_timer_init(void) {
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11); // CTC, 8 분주
  OCR1A = 999;                         // 16MHz / 8 / 1000 = 2kHz
  TIMSK1 = (1 << OCIE1A);
}

// 비트를 LED 로 내보낸다.  bit0 = D2
static void led_write(byte value) {
  if (!DEBUG_LED) {
    led_value = value;
  }
}

// DEBUG_LED 일 때만 LED 에 진단 값을 표시한다
static void debug_show(byte value) {
  if (DEBUG_LED) {
    led_value = value;
  }
}

// ── 스위치 : 핀 변화 인터럽트 ─────────────────────────
#define SW_COUNT 3
#define SW_DEBOUNCE_MS 50

static volatile byte sw_event = 0;
static volatile unsigned long sw_edge[SW_COUNT]; // 스위치별 마지막 엣지 시각
static volatile byte prev_pb = 0x1C; // PB2, PB3, PB4

// 누를 때와 뗄 때 모두 일정 시간 안의 엣지를 무시한다.
// 떨어지는 엣지만 재면 뗄 때 튄 신호가 새 입력으로 들어온다.
ISR(PCINT0_vect) {
  byte now = PINB & 0x1C;

  for (byte i = 0; i < SW_COUNT; i++) {
    byte mask = 0x04 << i;

    if (((now ^ prev_pb) & mask) == 0) { continue; }         // 이 스위치는 안 바뀜
    if (ms_count - sw_edge[i] < SW_DEBOUNCE_MS) { continue; } // 채터링 무시

    sw_edge[i] = ms_count;
    prev_pb ^= mask;

    if ((now & mask) == 0) { // 1 -> 0 으로 떨어진 것 = 눌린 것
      sw_event |= (1 << i);
    }
  }
}

// ── UART ───────────────────────────────────────────
#define RX_SIZE 64 // 2 의 거듭제곱

static volatile byte rx_buf[RX_SIZE];
static volatile byte rx_head = 0;
static volatile byte rx_tail = 0;

ISR(USART_RX_vect) {
  byte c = UDR0;
  byte next = (rx_head + 1) & (RX_SIZE - 1);

  if (next != rx_tail) { // 가득 차면 버린다
    rx_buf[rx_head] = c;
    rx_head = next;
  }
}

static void uart_init(void) {
  UCSR0A = (1 << U2X0); // 8 분주
  UBRR0 = (unsigned int)((F_CPU / 4 / BAUD - 1) / 2);
  UCSR0C = (UART_PARITY << UPM00) | ((UART_STOP_BITS == 2) << USBS0) | (1 << UCSZ01) |
           (1 << UCSZ00); // 데이터 8 비트
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
}

static bool uart_available(void) {
  return rx_head != rx_tail;
}

static byte uart_read(void) {
  byte c = rx_buf[rx_tail];

  rx_tail = (rx_tail + 1) & (RX_SIZE - 1);
  return c;
}

static void uart_write(byte c) {
  while (!(UCSR0A & (1 << UDRE0))) {
  }
  UDR0 = c;
}

// ── 보내기 ─────────────────────────────────────────
static byte sum_add(byte sum, byte value) {
  return SUM_XOR ? (sum ^ value) : (byte)(sum + value);
}

// 패킷 하나를 보낸다. 체크섬은 여기서 계산한다
static void send_packet(byte cmd, const byte *payload, byte len) {
  byte checksum = sum_add(0, cmd);

  uart_write(STX);
  uart_write(cmd);

  if (USE_LEN) {
    uart_write(len);
    checksum = sum_add(checksum, len);
  }

  for (byte i = 0; i < len; i++) {
    uart_write(payload[i]);
    checksum = sum_add(checksum, payload[i]);
  }

  if (USE_SUM) {
    uart_write(checksum);
  }
  if (USE_ETX) {
    uart_write(ETX);
  }
}

static void send_ack(byte cmd) {
  if (SEND_ACK) {
    send_packet(cmd | ACK_FLAG, NULL, 0);
  }
}

static void send_error(byte code) {
  debug_show(0x80 | code);

  if (SEND_ERROR) {
    send_packet(CMD_ERROR, &code, 1);
  }
}

// ── 받기 ───────────────────────────────────────────
enum { WAIT_STX, GET_CMD, GET_LEN, GET_DATA, GET_SUM, WAIT_ETX };
static byte state = WAIT_STX;

static byte command = 0;
static byte data_len = 0;
static byte data_idx = 0;
static byte data[MAX_DATA];
static byte sum = 0; // 받으면서 직접 더하는 체크섬
static bool ready = false;
static unsigned long last_rx = 0;

// 표에 없는 명령이면 -1
static int command_len(byte cmd) {
  for (int i = 0; i < COMMAND_COUNT; i++) {
    if (COMMANDS[i].cmd == cmd) {
      return COMMANDS[i].len;
    }
  }
  return -1;
}

static void finish_packet(void) {
  debug_show(command);
  ready = true;
  state = WAIT_STX;
}

// DATA 다음 단계로 넘어간다
static void after_data(void) {
  if (USE_SUM) {
    state = GET_SUM;
  } else if (USE_ETX) {
    state = WAIT_ETX;
  } else {
    finish_packet();
  }
}

static void start_data(void) {
  data_idx = 0;

  if (data_len > MAX_DATA) { // 버퍼를 넘으면 받지 않는다
    send_error(ERR_LEN);
    state = WAIT_STX;
  } else if (data_len == 0) {
    after_data();
  } else {
    state = GET_DATA;
  }
}

static void rx_decode(void) {
  // 패킷이 잘리면 처음부터 다시 기다린다
  if (state != WAIT_STX && now_ms() - last_rx > RX_TIMEOUT) {
    send_error(ERR_TIMEOUT);
    state = WAIT_STX;
  }

  // 패킷 하나가 완성되면 멈춘다. 나머지는 실행이 끝난 뒤 버퍼에서 이어 읽는다.
  while (uart_available() && !ready) {
    byte c = uart_read();
    last_rx = now_ms();

    if (state == WAIT_STX) {
      if (c == STX) {
        state = GET_CMD;
      } // 그 밖의 바이트는 무시
    } else if (state == GET_CMD) {
      command = c;
      sum = sum_add(0, c); // 체크섬은 CMD 부터 더한다

      if (USE_LEN) {
        state = GET_LEN;
      } else if (command_len(c) < 0) {
        send_error(ERR_CMD);
        state = WAIT_STX;
      } else {
        data_len = command_len(c);
        start_data();
      }
    } else if (state == GET_LEN) {
      data_len = c;
      sum = sum_add(sum, c);
      start_data();
    } else if (state == GET_DATA) {
      data[data_idx] = c; // 값이 STX, ETX 와 같아도 여기서는 자료다
      data_idx++;
      sum = sum_add(sum, c);

      if (data_idx == data_len) {
        after_data();
      }
    } else if (state == GET_SUM) {
      if (c != sum) {
        send_error(ERR_SUM);
        state = WAIT_STX;
      } else if (USE_ETX) {
        state = WAIT_ETX;
      } else {
        finish_packet();
      }
    } else {
      if (c == ETX) {
        finish_packet();
      } else {
        send_error(ERR_ETX);
        state = WAIT_STX;
      }
    }
  }
}

// ── ADC ────────────────────────────────────────────
#define CHANNEL_COUNT 2

static const byte ADC_MUX[CHANNEL_COUNT] = {1, 3}; // 0 = 가변저항 ADC1, 1 = 조도센서 ADC3

static void adc_init(void) {
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 128 분주
  DIDR0 = (1 << ADC1D) | (1 << ADC3D);                               // 디지털 입력 끄기
}

static int read_channel(byte channel) {
  ADMUX = (1 << REFS0) | ADC_MUX[channel]; // AVcc 기준
  ADCSRA |= (1 << ADSC);

  while (ADCSRA & (1 << ADSC)) {
  }
  return ADC; // ADCL 다음 ADCH 순서로 읽는다
}

// ADC 값(0 ~ 1023)은 1 바이트에 안 들어가서 상위, 하위 2 바이트로 나눠 보낸다
static void send_adc(byte channel) {
  int value = read_channel(channel);
  byte payload[2];

  payload[0] = value >> 8;   // 상위 바이트
  payload[1] = value & 0xFF; // 하위 바이트

  send_packet(CMD_ADC | ACK_FLAG, payload, 2);
}

static byte stream_channel = 0;
static unsigned long stream_ms = 0; // 0 = 끄기
static unsigned long stream_time = 0;

static void update_stream(void) {
  if (stream_ms == 0 || now_ms() - stream_time < stream_ms) {
    return;
  }
  stream_time += stream_ms;

  int value = read_channel(stream_channel);
  byte payload[3];

  payload[0] = stream_channel;
  payload[1] = value >> 8;
  payload[2] = value & 0xFF;

  send_packet(CMD_ADC_EVENT, payload, 3);
}

// ── LED 동작 (delay 없이 시간이 되면 한 단계씩) ─────────
enum { ACT_NONE, ACT_TOGGLE, ACT_SHIFT, ACT_BAR };

#define LED_COUNT 8
#define TOGGLE_MS 300UL
#define SHIFT_MS 80UL
#define BAR_MS 50UL
#define SHIFT_PERIOD ((LED_COUNT - 1) * 2) // 왕복 한 번의 단계 수

static byte act_mode = ACT_NONE;
static byte act_a = 0;        // ACT_TOGGLE : 번갈아 표시할 두 패턴
static byte act_b = 0;
static byte act_channel = 0;  // ACT_BAR : 표시할 ADC 채널
static int act_step = 0;
static int act_steps = 0;     // 전체 단계 수
static unsigned long act_ms = 0;
static unsigned long act_time = 0;

static void action_stop(void) {
  act_mode = ACT_NONE;
  led_write(ALL_OFF);
}

static void action_start(byte mode, unsigned long period, int steps) {
  act_mode = mode;
  act_ms = period;
  act_steps = steps;
  act_step = 0;
  act_time = now_ms() - period; // 다음 update 에서 바로 시작한다
}

static void action_toggle(byte a, byte b, int count) {
  act_a = a;
  act_b = b;
  action_start(ACT_TOGGLE, TOGGLE_MS, count * 2);
}

static void show_bar(void) {
  long value = read_channel(act_channel);
  byte count = (value * (LED_COUNT + 1)) >> 10; // 0 ~ 1023 -> 0 ~ 8 개

  led_write((byte)((1 << count) - 1));
}

static void update_action(void) {
  if (act_mode == ACT_NONE || now_ms() - act_time < act_ms) {
    return;
  }
  act_time += act_ms;

  if (act_mode == ACT_BAR) {
    show_bar();
    return;
  }
  if (act_step >= act_steps) {
    action_stop();
    return;
  }

  if (act_mode == ACT_TOGGLE) {
    led_write((act_step & 1) ? act_b : act_a);
  } else {
    int pos = act_step % SHIFT_PERIOD;
    led_write((byte)(1 << (pos < LED_COUNT ? pos : SHIFT_PERIOD - pos)));
  }
  act_step++;
}

// ── 부저 : Timer2 ──────────────────────────────────
ISR(TIMER2_COMPA_vect) {
  PORTC ^= (1 << PC0);
}

#define MIN_HZ 123 // 256 분주로 낼 수 있는 가장 낮은 음

static const unsigned int SCALE[] = {262, 294, 330, 349, 392, 440, 494, 523};
#define SCALE_COUNT ((int)(sizeof(SCALE) / sizeof(SCALE[0])))
#define NOTE_MS 250UL

static int melody_idx = -1; // -1 = 재생 안 함
static unsigned long melody_time = 0;
static bool tone_timed = false;
static unsigned long tone_start = 0;
static unsigned long tone_ms = 0;

static void tone_off(void) {
  TIMSK2 = 0;
  TCCR2B = 0;
  PORTC &= ~(1 << PC0);
  tone_timed = false;
}

// 반 주기마다 핀을 뒤집어서 소리를 낸다. ms 가 0 이면 끌 때까지 계속 낸다
static void tone_on(unsigned int hz, unsigned long ms) {
  // 1000Hz 이상은 64 분주가 더 정확하다
  bool fast = hz >= 1000;
  unsigned long clock = F_CPU / (fast ? 64 : 256);
  unsigned long top = clock / 2 / hz;

  if (top > 256) {
    top = 256;
  }

  TCCR2A = (1 << WGM21); // CTC
  TCCR2B = fast ? (1 << CS22) : ((1 << CS22) | (1 << CS21));
  OCR2A = (byte)(top - 1);
  TIMSK2 = (1 << OCIE2A);

  tone_timed = ms != 0;
  tone_start = now_ms();
  tone_ms = ms;
}

static void buzzer_stop(void) {
  melody_idx = -1;
  tone_off();
}

static void buzzer_tone(unsigned int hz, unsigned int ms) {
  buzzer_stop();

  if (hz >= MIN_HZ) {
    tone_on(hz, ms);
  }
}

static void update_buzzer(void) {
  if (tone_timed && now_ms() - tone_start >= tone_ms) {
    tone_off();
  }

  if (melody_idx < 0 || now_ms() - melody_time < NOTE_MS) {
    return;
  }
  melody_time += NOTE_MS;

  if (melody_idx == SCALE_COUNT) {
    buzzer_stop();
    return;
  }
  tone_on(SCALE[melody_idx], NOTE_MS - 30);
  melody_idx++;
}

static void melody_start(void) {
  buzzer_stop();
  melody_idx = 0;
  melody_time = now_ms() - NOTE_MS;
}

// ── 명령 실행 ──────────────────────────────────────
static void run_command(void) {
  int len = command_len(command);

  if (len < 0) {
    send_error(ERR_CMD);
    return;
  }
  if (data_len != len) {
    send_error(ERR_DATA_LEN);
    return;
  }

  // 채널을 쓰는 명령은 실행 전에 검사한다
  if ((command == CMD_ADC || command == CMD_STREAM || command == CMD_BAR) &&
      data[0] >= CHANNEL_COUNT) {
    send_error(ERR_CHANNEL);
    return;
  }

  if (command == CMD_ADC) {
    send_adc(data[0]); // 값이 곧 응답이라 ack 는 따로 없다
    return;
  }

  // 응답을 먼저 보내서 PC 가 동작이 끝날 때까지 기다리지 않게 한다
  send_ack(command);

  if (command == CMD_BLINK) {
    action_toggle(ALL_ON, ALL_OFF, data[0]);
  } else if (command == CMD_ALT) {
    action_toggle(data[0], (byte)~data[0], 3);
  } else if (command == CMD_PATTERN) {
    act_mode = ACT_NONE;
    led_write(data[0]);
  } else if (command == CMD_SHIFT) {
    action_start(ACT_SHIFT, SHIFT_MS, data[0] * SHIFT_PERIOD + 1);
  } else if (command == CMD_BRIGHT) {
    if (!DEBUG_LED) {
      led_level = (data[0] + 1) >> 4; // 0 ~ 255 -> 0 ~ 16
    }
  } else if (command == CMD_TONE) {
    buzzer_tone(((unsigned int)data[0] << 8) | data[1], ((unsigned int)data[2] << 8) | data[3]);
  } else if (command == CMD_MELODY) {
    melody_start();
  } else if (command == CMD_STREAM) {
    stream_channel = data[0];
    stream_ms = data[1] * 10UL;
    stream_time = now_ms();
  } else if (command == CMD_BAR) {
    act_channel = data[0];
    action_start(ACT_BAR, BAR_MS, 0);
  } else {
    action_stop();
    buzzer_stop();
    stream_ms = 0;
  }
}

// 눌린 스위치를 꺼내서 패킷으로 보낸다
static void send_sw_events(void) {
  cli();
  byte event = sw_event;
  sw_event = 0;
  sei();

  for (byte i = 0; i < SW_COUNT; i++) {
    if (event & (1 << i)) {
      byte number = i + 1;
      send_packet(CMD_SW_EVENT, &number, 1);
    }
  }
}

int main(void) {
  DDRD |= 0xFC; // LED : PD2 ~ PD7
  DDRB |= 0x03; // LED : PB0, PB1
  PORTD |= 0xFC; // Active Low : 꺼짐
  PORTB |= 0x03;

  DDRB &= ~0x1C;  // 스위치 : PB2 ~ PB4 입력
  PORTB |= 0x1C;  // 내부 풀업

  DDRC |= (1 << PC0); // 부저

  PCICR |= (1 << PCIE0);
  PCMSK0 |= (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT4);

  ms_timer_init();
  led_timer_init();
  adc_init();
  uart_init();
  sei();

  while (1) {
    rx_decode();
    send_sw_events();

    if (ready) {
      run_command();
      ready = false;
    }

    update_action();
    update_buzzer();
    update_stream();
  }
}
