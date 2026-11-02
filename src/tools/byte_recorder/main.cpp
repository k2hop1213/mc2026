/*
  시험 대비 도구 - UART 바이트 기록기

  조교 프로그램이 COM 포트를 잡고 있으면 PC 에서는 오가는 바이트를 볼 수 없다.
  이 코드는 보드가 받은 바이트를 EEPROM 에 저장한다.
  포트를 열면 Uno 가 리셋되어 RAM 은 지워지지만 EEPROM 은 남는다.

  사용 순서
    1. 이 코드를 올린다
    2. python scripts/read_log.py COM5 --clear        (이전 기록 지우기)
    3. 조교 프로그램을 연결하고 버튼을 하나씩 누른다
    4. 조교 프로그램 연결을 끊는다
    5. python scripts/read_log.py COM5                (기록 읽기)

  기록 : 바이트 1 칸 + 앞 바이트와의 시간 간격 1 칸 (10ms 단위, 최대 255)
  LED  : 마지막으로 받은 바이트. 기록이 가득 차면 전체 ON
  명령 : "@@LOGD" = 기록 출력,  "@@LOGC" = 기록 지우기

  LED : D2 ~ D9 (Active Low)   UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <EEPROM.h>

const long BAUD = 9600; // 조교 프로그램 설정과 맞출 것

const int FIRST_PIN = 2; // LED : D2 ~ D9   (Active Low)
const int LED_COUNT = 8;

const int HEAD_ADDR = 0;  // 기록 개수를 저장하는 곳 (2 바이트)
const int LOG_ADDR = 2;   // 기록 시작 주소
const int MAX_LOG = 511;  // (1024 - 2) / 2

const int RING_SIZE = 128;
const byte MAGIC[] = {'@', '@', 'L', 'O', 'G'};
const int MAGIC_LEN = 5;

byte ring_value[RING_SIZE]; // EEPROM 쓰기가 느려서 먼저 모아 둔다
byte ring_gap[RING_SIZE];
int ring_head = 0;
int ring_tail = 0;

int log_count = 0;
bool count_dirty = false;
bool first_byte = true;
unsigned long last_rx = 0;
int magic_idx = 0;

// 비트를 LED 핀으로 뿌린다.  bit0 = D2
// 회로가 Active Low 라서 뒤집어 내보낸다
void led_write(byte value) {
  value = ~value;

  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(FIRST_PIN + i, (value >> i) & 1);
  }
}

void ring_push(byte value) {
  unsigned long now = millis();
  unsigned long gap = (now - last_rx) / 10;
  int next = (ring_head + 1) % RING_SIZE;

  if (first_byte || gap > 255) {
    gap = 255; // 리셋 뒤 첫 바이트도 긴 간격으로 표시
  }
  first_byte = false;
  last_rx = now;

  if (next != ring_tail) { // 가득 차면 버린다
    ring_value[ring_head] = value;
    ring_gap[ring_head] = gap;
    ring_head = next;
  }
}

// 모아 둔 바이트 하나를 EEPROM 으로 옮긴다
void ring_flush_one() {
  if (log_count < MAX_LOG) {
    EEPROM.update(LOG_ADDR + log_count * 2, ring_value[ring_tail]);
    EEPROM.update(LOG_ADDR + log_count * 2 + 1, ring_gap[ring_tail]);
    log_count++;
    count_dirty = true;
  } else {
    led_write(0xFF);
  }

  ring_tail = (ring_tail + 1) % RING_SIZE;
}

void save_count() {
  EEPROM.put(HEAD_ADDR, log_count);
  count_dirty = false;
}

void print_hex(byte value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void dump_log() {
  Serial.print(F("LOG "));
  Serial.println(log_count);

  for (int i = 0; i < log_count; i++) {
    print_hex(EEPROM.read(LOG_ADDR + i * 2));
    Serial.print(' ');
    print_hex(EEPROM.read(LOG_ADDR + i * 2 + 1));
    Serial.println();
  }

  Serial.println(F("END"));
}

void run_magic(byte c) {
  while (ring_tail != ring_head) {
    ring_flush_one();
  }

  if (c == 'D') {
    save_count();
    dump_log();
  } else if (c == 'C') {
    log_count = 0;
    save_count();
    Serial.println(F("CLEARED"));
  }
}

void check_magic(byte c) {
  if (magic_idx == MAGIC_LEN) {
    magic_idx = 0;
    run_magic(c);
  } else if (c == MAGIC[magic_idx]) {
    magic_idx++;
  } else {
    magic_idx = (c == MAGIC[0]) ? 1 : 0;
  }
}

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(FIRST_PIN + i, OUTPUT);
  }
  led_write(0x00);

  EEPROM.get(HEAD_ADDR, log_count);
  if (log_count < 0 || log_count > MAX_LOG) {
    log_count = 0; // 처음 쓰는 EEPROM 은 0xFFFF 로 차 있다
  }

  Serial.begin(BAUD);
}

void loop() {
  while (Serial.available() > 0) {
    byte c = Serial.read();

    led_write(c);
    ring_push(c);
    check_magic(c);
  }

  if (ring_tail != ring_head) {
    ring_flush_one();
  } else if (count_dirty && millis() - last_rx > 50) {
    save_count(); // 조용해지면 개수를 저장한다
  }
}
