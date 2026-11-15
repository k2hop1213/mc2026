/*
  7차시 NeoPixel - 과제  별 그리기 + 회전 + 버튼으로 속도 변경

  NeoPixel 링 (16개, RGBW)
    IN (Data In) -> A4
    V+           -> 5V
    G            -> GND
  SW1 : D10 (PB2) -> 스위치 -> GND   (INPUT_PULLUP, 3주차 보드 그대로)

  동작
    1. 링에 별 꼭짓점 5개를 정해 획 순서(1 -> 2 -> 3 -> 4 -> 5)대로 하나씩 켠다.
    2. 별이 다 그려지면 한 칸씩 돌려 한 바퀴(16칸) 회전한다.
    3. 다시 1 부터 반복한다.
    4. 버튼을 누를 때마다 속도 변경 : 중간(시작) -> 빠르게 -> 느리게 -> 중간 ...

  꼭짓점 정하기
    16칸을 5 로 나누면 3.2 칸이라 딱 떨어지지 않는다.  3, 3, 4, 3, 3 칸 간격으로 잡는다.
    링 번호는 앞에서 볼 때 **반시계 방향**으로 늘어난다.  (실제 링에서 확인)
      위 = 0,  왼쪽 = 3,  왼쪽 아래 = 6,  오른쪽 아래 = 10,  오른쪽 = 13
    별은 꼭짓점을 하나 건너뛰며 긋는다.  (그림의 번호)
      1 위(0) -> 2 왼쪽 아래(6) -> 3 오른쪽(13) -> 4 왼쪽(3) -> 5 오른쪽 아래(10)
    링의 0번이 위로 오도록 링을 돌려 놓으면 그림과 같은 모양이 된다.

  delay 를 쓰면 기다리는 동안 버튼을 못 읽는다.
  그래서 millis() 로 "다음 단계 시각" 이 되었는지만 보고, loop 는 계속 돌면서 버튼을 읽는다.
*/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

const int NEO_PIN = A4;   // 링 IN : A4 (PC4)
const int LED_COUNT = 16;
const int SW_PIN = 10;    // SW1 : D10 (PB2)

Adafruit_NeoPixel ring(LED_COUNT, NEO_PIN, NEO_GRBW + NEO_KHZ800);

// 획 순서대로 적은 꼭짓점 LED 번호
const int STAR_COUNT = 5;
const int star[STAR_COUNT] = {0, 6, 13, 3, 10};

// 한 단계(점 하나 켜기 / 한 칸 돌리기) 걸리는 시간 (ms)
//   중간(시작) -> 빠르게 -> 느리게 -> 중간 ...
const int SPEED_COUNT = 3;
const unsigned long speed_ms[SPEED_COUNT] = {200, 80, 400};
const char* speed_name[SPEED_COUNT] = {"MID", "FAST", "SLOW"};
int speed = 0;

uint32_t starColor;

// 별 상태
//   drawn  : 지금까지 켜진 꼭짓점 수 (1 ~ 5)
//   offset : 별을 돌린 칸 수 (0 ~ 16)
int drawn = 0;
int offset = 0;
unsigned long next_step = 0;

// 꼭짓점 앞 count 개를 offset 칸 돌려서 켠다
void star_show(int count, int shift) {
  ring.clear();
  for (int i = 0; i < count; i++) {
    ring.setPixelColor((star[i] + shift) % LED_COUNT, starColor);
  }
  ring.show();
}

// 한 단계 진행 : 그리기 5 번 -> 회전 16 번 -> 처음으로
void star_step() {
  if (drawn < STAR_COUNT) {
    drawn = drawn + 1;      // 다음 꼭짓점 켜기
  } else if (offset < LED_COUNT) {
    offset = offset + 1;    // 한 칸 돌리기
  }

  if (offset == LED_COUNT) { // 한 바퀴 다 돌았으면 처음부터
    drawn = 1;
    offset = 0;
  }

  star_show(drawn, offset);
}

// ── 버튼 : 눌리는 순간(HIGH -> LOW) 만 true ──────────────
bool sw_prev = HIGH;
unsigned long sw_time = 0;

bool button_pressed() {
  bool now = digitalRead(SW_PIN);

  if (now == sw_prev) {
    return false;
  }
  if (millis() - sw_time < 30) {
    return false;
  } // 채터링 무시

  sw_time = millis();
  sw_prev = now;
  return now == LOW;
}

void setup() {
  pinMode(SW_PIN, INPUT_PULLUP);
  Serial.begin(9600);

  ring.begin();
  ring.clear();
  ring.show();

  //                     R  G  B  W
  starColor = ring.Color(0, 0, 0, 64); // 흰색 (W 칸)

  Serial.println("SW1 : speed MID -> FAST -> SLOW");
  Serial.println(speed_name[speed]);
  next_step = millis();
}

void loop() {
  if (button_pressed()) {
    speed = (speed + 1) % SPEED_COUNT;
    Serial.println(speed_name[speed]);
  }

  if ((long)(millis() - next_step) >= 0) {
    next_step = next_step + speed_ms[speed];
    star_step();
  }
}
