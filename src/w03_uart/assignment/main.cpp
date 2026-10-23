/*
  4차시 UART - 과제  [ASCII 버전]

  형식 :  | STX('<') | COMMAND(1byte) | DATA(1~MAX_DATA byte) | ETX('>') |

  COMMAND   DATA                         예
    '1'     반복 횟수 (10진수)            <13>   전체 점멸 3회
    '2'     시작 패턴 (16진수, 3회 고정)  <2AA>  AA 로 시작해 번갈아 점멸
    '4'     왕복 횟수 (10진수)            <45>   왕복 시프트 5회

  자료의 (< | 1 | 3 | >) 는 칸을 구분해 그린 것이므로 실제로는 <13> 처럼 붙여 보낸다.
  붙여 보내도 되고, 칸 사이의 ' ' 나 '|' 는 무시하므로 <1|3> 도 된다.

  받는 쪽은 상태 기계(FSM)로 처리한다.
    0 보통        : STX 가 오면 1 로
    1 명령 수신   : 한 바이트를 명령으로 저장하고 2 로
    2 자료 수신   : ETX 가 올 때까지 자료를 모은다. ETX 면 실행 준비
                    (자료가 없거나 너무 길면 오류)

  LED : D2 ~ D9 (Active Low)   UART : D0(RX), D1(TX)
*/

#include <Arduino.h>
#include <stdarg.h>
#include <stdlib.h>
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
const byte STX = '<';
const byte ETX = '>';

const byte CMD_ALL_ONOFF = '1';
const byte CMD_ALT = '2';
const byte CMD_SHIFT = '4';

const int ALT_COUNT = 3;  // LED ALT 는 3회 고정
const int MAX_COUNT = 99; // 반복 횟수 상한 (너무 크면 한참 멈춰 있게 된다)
const int MAX_DATA = 8;   // 자료 최대 글자 수

// 0 : 보통,  1 : 명령 수신,  2 : 자료 수신
int decode = 0;
byte command = 0;
char data_buf[MAX_DATA + 1]; // 끝의 '\0' 자리까지
int data_len = 0;
bool ready = false;

void rx_decode() {
  // 패킷 하나가 완성되면 멈춘다. 나머지는 실행이 끝난 뒤 버퍼에서 이어 읽는다.
  while (Serial.available() > 0 && !ready) {
    byte c = Serial.read();

    // 패킷 도중에 STX 가 다시 오면 앞의 것은 버리고 새로 시작한다
    if (c == STX) {
      if (decode != 0) {
        Printf("RX error : packet restarted\n");
      }
      decode = 1;
      data_len = 0;
      continue;
    }

    if (decode == 0) {
      // STX 를 기다리는 중. 그 밖의 글자(Enter 포함)는 무시
    } else if (c == ' ' || c == '|') {
      // 칸 구분 글자는 무시
    } else if (decode == 1) {
      if (c == ETX) {
        Printf("RX error : no command\n");
        decode = 0;
      } else {
        command = c; // 한 바이트를 명령으로 저장
        decode = 2;
      }
    } else {
      if (c == ETX) {
        if (data_len == 0) {
          Printf("RX error : no data\n");
        } else {
          data_buf[data_len] = '\0';
          ready = true; // 끝 표시면 실행 준비
        }
        decode = 0;
      } else if (data_len < MAX_DATA) {
        data_buf[data_len] = c;
        data_len = data_len + 1;
      } else {
        Printf("RX error : data too long\n");
        decode = 0;
      }
    }
  }
}

// 자료 글자열을 숫자로 바꾼다. base 는 10 또는 16
// 숫자가 아닌 글자가 섞여 있으면 -1
long parse_number(const char *text, int base) {
  char *end;
  long value = strtol(text, &end, base);

  if (end == text || *end != '\0') {
    return -1;
  }
  return value;
}

// 반복 횟수 자료를 읽는다. 잘못되면 오류를 출력하고 -1
int parse_count(const char *text) {
  long n = parse_number(text, 10);

  if (n < 1 || n > MAX_COUNT) {
    Printf("Invalid Data \"%s\" (count 1~%d)\n", text, MAX_COUNT);
    return -1;
  }
  return (int)n;
}

void run_command(byte cd, const char *data) {
  if (cd == CMD_ALL_ONOFF) {
    int count = parse_count(data);
    if (count < 0) {
      return;
    }
    Printf("CMD 1 : LED ALL ON OFF x %d\n", count);
    led_all_onoff(count);
  } else if (cd == CMD_ALT) {
    long pattern = parse_number(data, 16);
    if (pattern < 0 || pattern > 0xFF) {
      Printf("Invalid Data \"%s\" (pattern 00~FF)\n", data);
      return;
    }
    Printf("CMD 2 : LED ALT start 0x%02X x %d\n", (unsigned int)pattern, ALT_COUNT);
    led_alternating((byte)pattern, ALT_COUNT);
  } else if (cd == CMD_SHIFT) {
    int count = parse_count(data);
    if (count < 0) {
      return;
    }
    Printf("CMD 4 : LED SHIFT x %d\n", count);
    led_shift(count);
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
    run_command(command, data_buf);

    ready = false;
    command = 0;
    data_len = 0;
    led_write(ALL_OFF);
  }
}
