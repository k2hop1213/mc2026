/*
  시험용 통합 틀

  시험 때 고치는 곳
    1. 패킷 형식     : "패킷 형식" 상수   (BAUD, SERIAL_FRAME, STX, ETX, USE_LEN, USE_SUM, USE_ETX, SUM_XOR)
    2. 송신과 진단   : "송신과 진단" 상수 (SEND_ACK, SEND_ERROR, DEBUG_LED)
    3. 명령 번호     : "명령" 상수와 COMMANDS 표
    4. 응답 형식     : send_ack, send_error, send_adc, send_sw_events
    5. 명령별 동작   : run_command
    6. 핀            : "핀" 상수. LED 를 옮기면 ISR(TIMER1_COMPA_vect) 도 고친다

  패킷 :  | STX(0x02) | CMD | LEN | DATA x LEN | SUM | ETX(0x03) |
  SUM = CMD + LEN + DATA 의 하위 8비트      ** 시험 사양이 아니라 연습용 가정이다 **

  CMD   LEN  DATA               동작
  0x01  1    횟수               전체 점멸
  0x02  1    패턴               패턴과 그 반전을 3회 번갈아 표시
  0x03  1    패턴               패턴을 그대로 표시
  0x04  1    횟수               왕복 시프트
  0x05  1    채널               ADC 값 요청 (0 = 가변저항 A1, 1 = 조도센서 A3)
  0x06  1    밝기 0 ~ 255       LED 8개 전체 밝기
  0x07  4    주파수 2, 시간 2   부저 (상위 먼저, ms). 시간 0 = 계속, 주파수 0 = 끄기
  0x08  0    -                  부저 도레미파솔라시도
  0x09  2    채널, 주기         ADC 주기 전송 (주기 x 10ms, 0 = 끄기)
  0x0A  1    채널               ADC 값을 LED 막대로 표시
  0x0B  0    -                  LED, 부저, 주기 전송 모두 정지

  응답
    성공 : CMD | 0x80, LEN = 0
    ADC  : 0x85, LEN = 2, DATA = 값 상위, 하위
    실패 : 0xFF, LEN = 1, DATA = 오류 번호
  보드가 먼저 보내는 패킷
    스위치   : 0x90, LEN = 1, DATA = 스위치 번호(1 ~ 3)
    주기 ADC : 0x91, LEN = 3, DATA = 채널, 값 상위, 하위

  오류 번호
    1 LEN 이 너무 큼   2 ETX 아님   3 SUM 틀림
    4 시간 초과        5 없는 명령  6 명령과 LEN 이 안 맞음
    7 없는 ADC 채널

  delay 를 쓰지 않아서 LED 나 부저가 동작하는 중에도 명령과 스위치를 처리한다.
  새 LED 동작이 오면 하던 LED 동작을 멈추고 바꾼다.

  타이머
    Timer0 : millis      Timer1 : LED 밝기 (2kHz)      Timer2 : tone (부저)

  LED : D2 ~ D9 (Active Low)   SW : D10 ~ D12   가변저항 : A1   조도센서 : A3
  부저 : A0                    UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <avr/interrupt.h>

// ── 패킷 형식 ──────────────────────────────────────
const long BAUD = 9600;
const byte SERIAL_FRAME = SERIAL_8N1; // SERIAL_8E1, SERIAL_8O1, SERIAL_8N2 ...
const byte STX = 0x02;
const byte ETX = 0x03;
const bool USE_LEN = true;  // false 면 COMMANDS 표의 길이로 받는다
const bool USE_SUM = true;
const bool USE_ETX = true;
const bool SUM_XOR = false; // true 면 더하기 대신 XOR

// ── 송신과 진단 ─────────────────────────────────────
const bool SEND_ACK = true;   // false 면 성공 응답을 보내지 않는다
const bool SEND_ERROR = true; // false 면 오류 응답을 보내지 않는다. 패킷은 똑같이 버린다
const bool DEBUG_LED = false; // true 면 LED 에 CMD, 오류는 0x80 | 오류 번호만 표시한다

// ── 명령 ───────────────────────────────────────────
const byte CMD_BLINK = 0x01;
const byte CMD_ALT = 0x02;
const byte CMD_PATTERN = 0x03;
const byte CMD_SHIFT = 0x04;
const byte CMD_ADC = 0x05;
const byte CMD_BRIGHT = 0x06;
const byte CMD_TONE = 0x07;
const byte CMD_MELODY = 0x08;
const byte CMD_STREAM = 0x09;
const byte CMD_BAR = 0x0A;
const byte CMD_STOP = 0x0B;

const byte CMD_SW_EVENT = 0x90;
const byte CMD_ADC_EVENT = 0x91;

const byte ACK_FLAG = 0x80; // 성공 응답 = CMD | ACK_FLAG
const byte CMD_ERROR = 0xFF;

struct Command {
  byte cmd;
  byte len;
};

const Command COMMANDS[] = {
    {CMD_BLINK, 1}, {CMD_ALT, 1},    {CMD_PATTERN, 1}, {CMD_SHIFT, 1},
    {CMD_ADC, 1},   {CMD_BRIGHT, 1}, {CMD_TONE, 4},    {CMD_MELODY, 0},
    {CMD_STREAM, 2}, {CMD_BAR, 1},   {CMD_STOP, 0},
};
const int COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

const byte ERR_LEN = 1;
const byte ERR_ETX = 2;
const byte ERR_SUM = 3;
const byte ERR_TIMEOUT = 4;
const byte ERR_CMD = 5;
const byte ERR_DATA_LEN = 6;
const byte ERR_CHANNEL = 7;

const int MAX_DATA = 16;              // DATA 버퍼 크기
const unsigned long RX_TIMEOUT = 100; // 패킷 도중 기다리는 시간 (ms)

// ── 핀 ─────────────────────────────────────────────
const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const int POT_PIN = A1;    // 가변저항 : PC1 - ADC1
const int CDS_PIN = A3;    // 조도센서 : PC3 - ADC3
const int BUZZER_PIN = A0; // 부저

const int SW1_PIN = 10; // PB2 - PCINT2
const int SW2_PIN = 11; // PB3 - PCINT3
const int SW3_PIN = 12; // PB4 - PCINT4

const byte SW_COUNT = 3;
const unsigned long SW_DEBOUNCE_MS = 50;

const byte ALL_ON = 0xFF;
const byte ALL_OFF = 0x00;

// ── LED 출력 : Timer1 인터럽트 ───────────────────────
const byte LEVEL_MAX = 16; // 밝기 단계

volatile byte led_value = ALL_OFF;
volatile byte led_level = LEVEL_MAX;

// 16 번 중 led_level 번만 켜서 밝기를 만든다 (125Hz)
ISR(TIMER1_COMPA_vect) {
  static byte phase = 0;

  phase = (phase + 1) & 0x0F;

  // 회로가 Active Low 라서 뒤집어 내보낸다
  byte out = ~(phase < led_level ? led_value : ALL_OFF);

  PORTD = (PORTD & 0x03) | (out << 2); // D2 ~ D7 = PD2 ~ PD7
  PORTB = (PORTB & 0xFC) | (out >> 6); // D8, D9  = PB0, PB1
}

void led_timer_init() {
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11); // CTC, 8 분주
  OCR1A = 999;                         // 16MHz / 8 / 1000 = 2kHz
  TIMSK1 = (1 << OCIE1A);
}

// 비트를 LED 로 내보낸다.  bit0 = D2
void led_write(byte value) {
  if (!DEBUG_LED) {
    led_value = value;
  }
}

// DEBUG_LED 일 때만 LED 에 진단 값을 표시한다
void debug_show(byte value) {
  if (DEBUG_LED) {
    led_value = value;
  }
}

// ── 스위치 : 핀 변화 인터럽트 ─────────────────────────
volatile byte sw_event = 0;
volatile unsigned long sw_edge[SW_COUNT]; // 스위치별 마지막 엣지 시각
volatile byte prev_pb = 0x1C; // PB2, PB3, PB4

// 누를 때와 뗄 때 모두 일정 시간 안의 엣지를 무시한다.
// 떨어지는 엣지만 재면 뗄 때 튄 신호가 새 입력으로 들어온다.
ISR(PCINT0_vect) {
  byte now = PINB & 0x1C;

  for (byte i = 0; i < SW_COUNT; i++) {
    byte mask = 0x04 << i;

    if (((now ^ prev_pb) & mask) == 0) { continue; }         // 이 스위치는 안 바뀜
    if (millis() - sw_edge[i] < SW_DEBOUNCE_MS) { continue; } // 채터링 무시

    sw_edge[i] = millis();
    prev_pb ^= mask;

    if ((now & mask) == 0) { // 1 -> 0 으로 떨어진 것 = 눌린 것
      sw_event |= (1 << i);
    }
  }
}

// ── 보내기 ─────────────────────────────────────────
byte sum_add(byte sum, byte value) {
  return SUM_XOR ? (sum ^ value) : (sum + value);
}

// 패킷 하나를 보낸다. 체크섬은 여기서 계산한다
void send_packet(byte cmd, const byte *payload, byte len) {
  byte checksum = sum_add(0, cmd);

  Serial.write(STX);
  Serial.write(cmd);

  if (USE_LEN) {
    Serial.write(len);
    checksum = sum_add(checksum, len);
  }

  for (byte i = 0; i < len; i++) {
    Serial.write(payload[i]);
    checksum = sum_add(checksum, payload[i]);
  }

  if (USE_SUM) {
    Serial.write(checksum);
  }
  if (USE_ETX) {
    Serial.write(ETX);
  }
}

void send_ack(byte cmd) {
  if (SEND_ACK) {
    send_packet(cmd | ACK_FLAG, NULL, 0);
  }
}

void send_error(byte code) {
  debug_show(0x80 | code);

  if (SEND_ERROR) {
    send_packet(CMD_ERROR, &code, 1);
  }
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

// 표에 없는 명령이면 -1
int command_len(byte cmd) {
  for (int i = 0; i < COMMAND_COUNT; i++) {
    if (COMMANDS[i].cmd == cmd) {
      return COMMANDS[i].len;
    }
  }
  return -1;
}

void finish_packet() {
  debug_show(command);
  ready = true;
  state = WAIT_STX;
}

// DATA 다음 단계로 넘어간다
void after_data() {
  if (USE_SUM) {
    state = GET_SUM;
  } else if (USE_ETX) {
    state = WAIT_ETX;
  } else {
    finish_packet();
  }
}

void start_data() {
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
const byte CHANNEL_COUNT = 2;

int read_channel(byte channel) {
  return analogRead(channel == 0 ? POT_PIN : CDS_PIN);
}

// ADC 값(0 ~ 1023)은 1 바이트에 안 들어가서 상위, 하위 2 바이트로 나눠 보낸다
void send_adc(byte channel) {
  int value = read_channel(channel);
  byte payload[2];

  payload[0] = value >> 8;   // 상위 바이트
  payload[1] = value & 0xFF; // 하위 바이트

  send_packet(CMD_ADC | ACK_FLAG, payload, 2);
}

byte stream_channel = 0;
unsigned long stream_ms = 0; // 0 = 끄기
unsigned long stream_time = 0;

void update_stream() {
  if (stream_ms == 0 || millis() - stream_time < stream_ms) {
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

const unsigned long TOGGLE_MS = 300;
const unsigned long SHIFT_MS = 80;
const unsigned long BAR_MS = 50;
const int SHIFT_PERIOD = (LED_COUNT - 1) * 2; // 왕복 한 번의 단계 수

byte act_mode = ACT_NONE;
byte act_a = 0;        // ACT_TOGGLE : 번갈아 표시할 두 패턴
byte act_b = 0;
byte act_channel = 0;  // ACT_BAR : 표시할 ADC 채널
int act_step = 0;
int act_steps = 0;     // 전체 단계 수
unsigned long act_ms = 0;
unsigned long act_time = 0;

void action_stop() {
  act_mode = ACT_NONE;
  led_write(ALL_OFF);
}

void action_start(byte mode, unsigned long period, int steps) {
  act_mode = mode;
  act_ms = period;
  act_steps = steps;
  act_step = 0;
  act_time = millis() - period; // 다음 update 에서 바로 시작한다
}

void action_toggle(byte a, byte b, int count) {
  act_a = a;
  act_b = b;
  action_start(ACT_TOGGLE, TOGGLE_MS, count * 2);
}

void show_bar() {
  long value = read_channel(act_channel);
  byte count = (value * (LED_COUNT + 1)) >> 10; // 0 ~ 1023 -> 0 ~ 8 개

  led_write((1 << count) - 1);
}

void update_action() {
  if (act_mode == ACT_NONE || millis() - act_time < act_ms) {
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
    led_write(1 << (pos < LED_COUNT ? pos : SHIFT_PERIOD - pos));
  }
  act_step++;
}

// ── 부저 ───────────────────────────────────────────
const unsigned int SCALE[] = {262, 294, 330, 349, 392, 440, 494, 523};
const int SCALE_COUNT = sizeof(SCALE) / sizeof(SCALE[0]);
const unsigned long NOTE_MS = 250;

int melody_idx = -1; // -1 = 재생 안 함
unsigned long melody_time = 0;

void buzzer_stop() {
  melody_idx = -1;
  noTone(BUZZER_PIN);
}

void buzzer_tone(unsigned int hz, unsigned int ms) {
  buzzer_stop();

  if (hz == 0) {
    return;
  }
  if (ms == 0) {
    tone(BUZZER_PIN, hz);
  } else {
    tone(BUZZER_PIN, hz, ms);
  }
}

void update_melody() {
  if (melody_idx < 0 || millis() - melody_time < NOTE_MS) {
    return;
  }
  melody_time += NOTE_MS;

  if (melody_idx == SCALE_COUNT) {
    buzzer_stop();
    return;
  }
  tone(BUZZER_PIN, SCALE[melody_idx], NOTE_MS - 30);
  melody_idx++;
}

void melody_start() {
  buzzer_stop();
  melody_idx = 0;
  melody_time = millis() - NOTE_MS;
}

// ── 명령 실행 ──────────────────────────────────────
void run_command() {
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
    action_toggle(data[0], ~data[0], 3);
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
    buzzer_tone((data[0] << 8) | data[1], (data[2] << 8) | data[3]);
  } else if (command == CMD_MELODY) {
    melody_start();
  } else if (command == CMD_STREAM) {
    stream_channel = data[0];
    stream_ms = data[1] * 10UL;
    stream_time = millis();
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
void send_sw_events() {
  noInterrupts();
  byte event = sw_event;
  sw_event = 0;
  interrupts();

  for (byte i = 0; i < SW_COUNT; i++) {
    if (event & (1 << i)) {
      byte number = i + 1;
      send_packet(CMD_SW_EVENT, &number, 1);
    }
  }
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
    digitalWrite(FIRST_PIN + i, HIGH); // Active Low : 꺼짐
  }

  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);

  PCICR = PCICR | (1 << PCIE0);
  PCMSK0 = PCMSK0 | (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT4);

  led_timer_init();
  Serial.begin(BAUD, SERIAL_FRAME);
}

void loop() {
  rx_decode();
  send_sw_events();

  if (ready) {
    run_command();
    ready = false;
  }

  update_action();
  update_melody();
  update_stream();
}
