/*
  시험 풀이 - UART 패킷으로 LED 모드, ADC, 스위치 제어

  통신 : 38400bps, 8 비트, 홀수 패리티, 정지 비트 2 (SERIAL_8O2)
  패킷 : | STX 0x02 | CMD | LEN 0 ~ 16 | DATA x LEN | ETX 0x03 |   체크섬 없음, 2 바이트 값은 상위 먼저

  명령 (PC -> 보드)
    01 PING           LEN 0                         -> ACK
    02 GET_STATUS     LEN 0                         -> STATUS
    10 LED_SET        패턴                          -> ACK
    11 LED_RUN        모드 1~3, 반복(0 = 무한), 시작 -> ACK
    12 LED_STOP       LEN 0                         -> ACK
    13 SET_PERIOD     주기 50 ~ 1000ms (2)          -> ACK   (속도 기준은 PC 로 바뀐다)
    14 SET_SPEED_SRC  0 = PC, 1 = 가변저항          -> ACK
    20 ADC_READ       채널 0 = 가변저항, 1 = 조도   -> ADC_DATA
    21 ADC_STREAM     채널, 끄기 0 / 켜기 1, 간격(2) -> ACK

  응답과 이벤트 (보드 -> PC)
    80 ACK        처리한 CMD
    B0 STATUS     모드, LED 패턴, 주기(2), 속도 기준, 스트림 비트     부팅 직후 1 회, GET_STATUS
    A0 ADC_DATA   채널, 값(2)
    90 EVT_SWITCH 스위치 번호
    91 EVT_LED    LED 패턴
    92 EVT_MODE   모드, 원인 (0 PC, 1 스위치, 2 완료)
    93 EVT_SPEED  새 주기(2), 가변저항 값(2)

  잘못된 패킷 (ETX 아님, LEN 17 이상, 없는 CMD, LEN 불일치, 범위 밖 값) 은 응답 없이 버린다.
  응답을 먼저 보내고, 명령 때문에 생긴 이벤트는 그 뒤에 보낸다. delay 는 쓰지 않는다.

  LED : D2 ~ D9 (Active Low, bit0 = D2)   SW : SW1 ~ SW4 = D10 ~ D13 (PB2 ~ PB5, PCINT0)
  가변저항 : A1   조도센서 : A3            UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <avr/interrupt.h>

// ── 통신, 패킷 ─────────────────────────────────────
const long BAUD = 38400;
const byte SERIAL_FRAME = SERIAL_8O2;

const byte STX = 0x02;
const byte ETX = 0x03;
const byte MAX_LEN = 16;
const unsigned long RX_TIMEOUT = 100; // 패킷 도중 끊기면 처음부터

// ── 명령 ───────────────────────────────────────────
const byte CMD_PING = 0x01;
const byte CMD_GET_STATUS = 0x02;
const byte CMD_LED_SET = 0x10;
const byte CMD_LED_RUN = 0x11;
const byte CMD_LED_STOP = 0x12;
const byte CMD_SET_PERIOD = 0x13;
const byte CMD_SET_SPEED_SRC = 0x14;
const byte CMD_ADC_READ = 0x20;
const byte CMD_ADC_STREAM = 0x21;

const byte RSP_ACK = 0x80;
const byte RSP_ADC_DATA = 0xA0;
const byte RSP_STATUS = 0xB0;
const byte EVT_SWITCH = 0x90;
const byte EVT_LED = 0x91;
const byte EVT_MODE = 0x92;
const byte EVT_SPEED = 0x93;

// ── 모드, 원인, 속도 기준 ──────────────────────────
enum { MODE_IDLE, MODE_ALL, MODE_ALT, MODE_SHIFT };
enum { CAUSE_PC, CAUSE_SWITCH, CAUSE_DONE };
enum { SRC_PC, SRC_POT };

const byte SWITCH_REPEAT = 3;    // 스위치로 시작한 모드는 3 회
const byte ALT_DEFAULT = 0xAA;   // ALTERNATING 시작 패턴이 0 이면
const bool LED_OFF_AT_DONE = true; // 반복을 다 하면 LED 를 끈다 (가정)

const unsigned int PERIOD_MIN = 50;
const unsigned int PERIOD_MAX = 1000;
const unsigned int SPEED_STEP = 20;          // 이만큼 이상 바뀌어야 주기를 바꾼다
const unsigned long SPEED_EVT_GAP = 100;     // EVT_SPEED 최소 간격
const unsigned long POT_CHECK_MS = 10;
const unsigned int STREAM_MIN = 51; // ADC_STREAM 간격 : 조교 채점에서 50 은 범위 밖이었다. 위쪽 한계는 모른다

// ── 핀 ─────────────────────────────────────────────
const int FIRST_PIN = 2; // LED : D2 ~ D9 (Active Low)
const int LED_COUNT = 8;
const int POT_PIN = A1;
const int CDS_PIN = A3;

const byte SW_COUNT = 4;    // D10 ~ D13 = PB2 ~ PB5
const byte SW_MASK = 0x3C;
const unsigned long SW_DEBOUNCE_MS = 50;

// ── 보내기 ─────────────────────────────────────────
void send_packet(byte cmd, const byte *payload, byte len) {
  Serial.write(STX);
  Serial.write(cmd);
  Serial.write(len);
  for (byte i = 0; i < len; i++) {
    Serial.write(payload[i]);
  }
  Serial.write(ETX);
}

void send1(byte cmd, byte value) {
  send_packet(cmd, &value, 1);
}

void send2(byte cmd, byte a, byte b) {
  byte payload[2] = {a, b};
  send_packet(cmd, payload, 2);
}

// ── 상태 ───────────────────────────────────────────
byte mode = MODE_IDLE;
byte led_value = 0;
unsigned int period = 500;
byte speed_src = SRC_POT;

bool stream_on[2] = {false, false};
unsigned int stream_ms[2] = {0, 0};
unsigned long stream_time[2] = {0, 0};

int read_channel(byte channel) {
  return analogRead(channel == 0 ? POT_PIN : CDS_PIN);
}

byte stream_bits() {
  return (stream_on[0] ? 0x01 : 0) | (stream_on[1] ? 0x02 : 0);
}

// 모드, LED 패턴, 주기(2), 속도 기준, 스트림 비트 = 6 바이트 (조교 프로그램이 해석하는 순서)
void send_status() {
  byte payload[6] = {mode, led_value, (byte)(period >> 8), (byte)(period & 0xFF), speed_src, stream_bits()};
  send_packet(RSP_STATUS, payload, 6);
}

void send_adc(byte channel) {
  int value = read_channel(channel);
  byte payload[3] = {channel, (byte)(value >> 8), (byte)(value & 0xFF)};
  send_packet(RSP_ADC_DATA, payload, 3);
}

// ── LED ────────────────────────────────────────────
void led_out(byte value) {
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1 ? LOW : HIGH); // Active Low
  }
}

// LED 를 바꾸고, 실제로 바뀌었을 때만 EVT_LED 를 보낸다
void led_show(byte value) {
  if (value == led_value) {
    return;
  }
  led_value = value;
  led_out(value);
  send1(EVT_LED, value);
}

// ── LED 모드 (millis 로 한 단계씩) ─────────────────
const int SHIFT_STEPS = 14; // 0 1 2 3 4 5 6 7 6 5 4 3 2 1

byte run_repeat = 0; // 0 = 무한
byte run_count = 0;  // 끝낸 반복 횟수
byte alt_p = ALT_DEFAULT;
int step = 0;
unsigned long step_time = 0;

int steps_per_cycle() {
  return mode == MODE_SHIFT ? SHIFT_STEPS : 2;
}

byte step_pattern(int s) {
  if (mode == MODE_ALL) {
    return (s % 2 == 0) ? 0xFF : 0x00;
  }
  if (mode == MODE_ALT) {
    return (s % 2 == 0) ? alt_p : (byte)~alt_p;
  }
  int pos = s < LED_COUNT ? s : SHIFT_STEPS - s;
  return (byte)(1 << pos);
}

void send_mode(byte m, byte cause) {
  send2(EVT_MODE, m, cause);
}

void mode_start(byte m, byte repeat, byte start, byte cause) {
  mode = m;
  run_repeat = repeat;
  run_count = 0;
  alt_p = start ? start : ALT_DEFAULT;
  step = 0;
  step_time = millis();

  send_mode(m, cause);
  led_show(step_pattern(0));
}

// 모드를 끝낸다. 하던 모드가 없으면 EVT_MODE 는 보내지 않는다
void mode_stop(byte cause, bool led_off) {
  if (mode != MODE_IDLE) {
    mode = MODE_IDLE;
    send_mode(MODE_IDLE, cause);
  }
  if (led_off) {
    led_show(0x00);
  }
}

void update_mode() {
  unsigned long now = millis();
  if (mode == MODE_IDLE || now - step_time < period) {
    return;
  }
  // 이전 단계 시각 + 주기로 맞춰서 간격이 밀리지 않게 한다. 많이 늦었으면 지금부터 다시 센다
  step_time = (now - step_time < 2UL * period) ? step_time + period : now;

  step++;
  if (step >= steps_per_cycle()) {
    step = 0;
    run_count++;
    if (run_repeat != 0 && run_count >= run_repeat) {
      mode_stop(CAUSE_DONE, LED_OFF_AT_DONE);
      return;
    }
  }
  led_show(step_pattern(step));
}

// ── 가변저항으로 속도 ──────────────────────────────
unsigned long pot_time = 0;
unsigned long speed_evt_time = 0;
long pot_smooth = 0; // 가변저항 값 x 16

// 가변저항 값이 흔들려서 8 번 읽어 평균을 낸다
int read_pot() {
  long total = 0;
  for (byte i = 0; i < 8; i++) {
    total += analogRead(POT_PIN);
  }
  return total / 8;
}

unsigned int pot_period(int pot) {
  return PERIOD_MIN + (unsigned long)pot * (PERIOD_MAX - PERIOD_MIN) / 1023;
}

// 주기를 바꾸고, 바뀌었으면 EVT_SPEED (새 주기, 가변저항 값) 를 보낸다
void set_period(unsigned int value, int pot) {
  if (value == period) {
    return;
  }
  period = value;
  speed_evt_time = millis();

  byte payload[4] = {(byte)(period >> 8), (byte)(period & 0xFF), (byte)(pot >> 8), (byte)(pot & 0xFF)};
  send_packet(EVT_SPEED, payload, 4);
}

void update_speed() {
  if (speed_src != SRC_POT || millis() - pot_time < POT_CHECK_MS) {
    return;
  }
  pot_time = millis();

  // 잡음으로 EVT_SPEED 가 나가지 않도록 한 번 더 부드럽게 한다 (16 배 고정소수점 이동 평균)
  pot_smooth += ((long)read_pot() * 16 - pot_smooth) / 4;
  int pot = (pot_smooth + 8) / 16;
  unsigned int next = pot_period(pot);
  unsigned int diff = next > period ? next - period : period - next;

  if (diff < SPEED_STEP || millis() - speed_evt_time < SPEED_EVT_GAP) {
    return;
  }
  set_period(next, pot); // 동작 중에도 다음 단계부터 바로 적용
}

// ── ADC 스트림 ─────────────────────────────────────
void update_stream() {
  for (byte ch = 0; ch < 2; ch++) {
    if (stream_on[ch] && millis() - stream_time[ch] >= stream_ms[ch]) {
      stream_time[ch] = millis();
      send_adc(ch);
    }
  }
}

// ── 받기 ───────────────────────────────────────────
enum { WAIT_STX, GET_CMD, GET_LEN, GET_DATA, WAIT_ETX, SKIP };
byte state = WAIT_STX;
int skip_left = 0; // LEN 17 이상일 때 건너뛸 바이트 수 (DATA + ETX)

byte command = 0;
byte data_len = 0;
byte data_idx = 0;
byte data[MAX_LEN];
bool ready = false;
unsigned long last_rx = 0;

void rx_decode() {
  if (state != WAIT_STX && millis() - last_rx > RX_TIMEOUT) {
    state = WAIT_STX;
  }

  while (Serial.available() > 0 && !ready) {
    byte c = Serial.read();
    last_rx = millis();

    if (state == WAIT_STX) {
      if (c == STX) { state = GET_CMD; }
    } else if (state == GET_CMD) {
      command = c;
      state = GET_LEN;
    } else if (state == GET_LEN) {
      if (c > MAX_LEN) { // LEN 17 이상은 버린다. 패킷 끝은 LEN 으로 판단해서 DATA 와 ETX 를 건너뛴다
        skip_left = c + 1;
        state = SKIP;
      } else {
        data_len = c;
        data_idx = 0;
        state = data_len == 0 ? WAIT_ETX : GET_DATA;
      }
    } else if (state == GET_DATA) {
      data[data_idx++] = c; // STX, ETX 와 같은 값도 자료다. 끝은 LEN 으로 판단
      if (data_idx == data_len) { state = WAIT_ETX; }
    } else if (state == SKIP) {
      if (--skip_left == 0) { state = WAIT_STX; }
    } else {
      ready = (c == ETX); // ETX 자리 바이트로 패킷이 끝난다. ETX 가 아니면 버린다
      state = WAIT_STX;
    }
  }
}

// ── 명령 실행 ──────────────────────────────────────
unsigned int value16(byte i) {
  return ((unsigned int)data[i] << 8) | data[i + 1];
}

// 형식과 범위가 맞으면 true. 틀리면 응답 없이 버린다
bool valid_command() {
  switch (command) {
  case CMD_PING:
  case CMD_GET_STATUS:
  case CMD_LED_STOP:
    return data_len == 0;
  case CMD_LED_SET:
    return data_len == 1;
  case CMD_LED_RUN:
    return data_len == 3 && data[0] >= MODE_ALL && data[0] <= MODE_SHIFT;
  case CMD_SET_PERIOD:
    return data_len == 2 && value16(0) >= PERIOD_MIN && value16(0) <= PERIOD_MAX;
  case CMD_SET_SPEED_SRC:
    return data_len == 1 && data[0] <= SRC_POT;
  case CMD_ADC_READ:
    return data_len == 1 && data[0] <= 1;
  case CMD_ADC_STREAM:
    // 켤 때 간격이 50 이하면 버린다
    return data_len == 4 && data[0] <= 1 && data[1] <= 1 && (data[1] == 0 || value16(2) >= STREAM_MIN);
  default:
    return false;
  }
}

void run_command() {
  if (!valid_command()) {
    return;
  }

  // 값으로 답하는 명령
  if (command == CMD_GET_STATUS) {
    send_status();
    return;
  }
  if (command == CMD_ADC_READ) {
    send_adc(data[0]);
    return;
  }

  send1(RSP_ACK, command); // 응답 먼저, 이벤트는 그 뒤

  if (command == CMD_LED_SET) {
    mode_stop(CAUSE_PC, false);
    led_show(data[0]);
  } else if (command == CMD_LED_RUN) {
    mode_start(data[0], data[1], data[2], CAUSE_PC);
  } else if (command == CMD_LED_STOP) {
    mode_stop(CAUSE_PC, true);
  } else if (command == CMD_SET_PERIOD) {
    speed_src = SRC_PC; // SET_PERIOD 를 받으면 속도 기준은 PC. EVT_SPEED 는 가변저항으로 바뀔 때만 보낸다
    period = value16(0);
  } else if (command == CMD_SET_SPEED_SRC) {
    speed_src = data[0];
  } else if (command == CMD_ADC_STREAM) {
    byte ch = data[0];
    stream_on[ch] = data[1] == 1;
    stream_ms[ch] = value16(2);
    stream_time[ch] = millis();
  }
}

// ── 스위치 : 핀 변화 인터럽트 ─────────────────────────
volatile byte sw_event = 0;
volatile unsigned long sw_edge[SW_COUNT];
volatile byte prev_pb = SW_MASK;

// 누를 때와 뗄 때 모두 디바운스한다
ISR(PCINT0_vect) {
  byte now = PINB & SW_MASK;

  for (byte i = 0; i < SW_COUNT; i++) {
    byte mask = 0x04 << i;

    if (((now ^ prev_pb) & mask) == 0) { continue; }
    if (millis() - sw_edge[i] < SW_DEBOUNCE_MS) { continue; }

    sw_edge[i] = millis();
    prev_pb ^= mask;

    if ((now & mask) == 0) { // 눌림
      sw_event |= (1 << i);
    }
  }
}

// 디바운스 시간 안에 뗀 탭이면 prev_pb 를 실제 핀에 맞춘다. 인터럽트를 끈 상태에서 부른다
void sw_resync() {
  byte now = PINB & SW_MASK;

  for (byte i = 0; i < SW_COUNT; i++) {
    byte mask = 0x04 << i;

    if ((now & (byte)~prev_pb & mask) && millis() - sw_edge[i] >= SW_DEBOUNCE_MS) {
      sw_edge[i] = millis();
      prev_pb |= mask;
    }
  }
}

void handle_switches() {
  noInterrupts();
  byte event = sw_event;
  sw_event = 0;
  sw_resync();
  interrupts();

  for (byte i = 0; i < SW_COUNT; i++) {
    if ((event & (1 << i)) == 0) {
      continue;
    }
    send1(EVT_SWITCH, i + 1);

    if (i < 3) {
      mode_start(i + 1, SWITCH_REPEAT, 0, CAUSE_SWITCH); // SW1 ~ 3 : 모드 1 ~ 3 을 3 회
    } else {
      mode_stop(CAUSE_SWITCH, true); // SW4 : 정지, LED 끔
    }
  }
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  led_out(0x00);

  for (byte i = 0; i < SW_COUNT; i++) {
    pinMode(10 + i, INPUT_PULLUP);
  }
  PCICR |= (1 << PCIE0);
  PCMSK0 |= (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT4) | (1 << PCINT5);

  int pot = read_pot();
  pot_smooth = (long)pot * 16;
  period = pot_period(pot); // 전원 켤 때 속도 기준 = 가변저항

  Serial.begin(BAUD, SERIAL_FRAME);
  send_status(); // 부팅 직후 1 회
}

void loop() {
  rx_decode();
  if (ready) {
    run_command();
    ready = false;
  }

  handle_switches();
  update_speed();
  update_mode();
  update_stream();
}
