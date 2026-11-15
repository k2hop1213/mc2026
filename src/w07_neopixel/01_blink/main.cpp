/*
  7차시 NeoPixel - 예제 1  한 개의 LED 를 반복 점멸

  NeoPixel 링 (16개, RGBW)
    IN (Data In) -> A4
    V+           -> 5V
    G            -> GND

  A4 (= PC4) 는 아날로그 핀이지만 디지털 출력으로도 쓸 수 있다.

  라이브러리
    Adafruit NeoPixel 라이브러리를 쓴다.
    platformio.ini 의 env 아래에 다음 두 줄을 넣으면 빌드할 때 자동으로 받아진다.
      lib_deps =
          adafruit/Adafruit NeoPixel@1.15.5

  색 순서
    링은 G, R, B, W 순서로 데이터를 받는다.
    라이브러리에 NEO_GRBW 를 알려 주면 순서는 라이브러리가 맞춰 준다.
    그래서 코드에서는 Color(빨강, 초록, 파랑, 흰색) 순서로 적으면 된다.
*/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

const int NEO_PIN = A4;    // 링 IN : A4 (PC4)
const int LED_COUNT = 16;  // 링의 LED 개수

Adafruit_NeoPixel ring(LED_COUNT, NEO_PIN, NEO_GRBW + NEO_KHZ800);

uint32_t redColor;

// 0번 LED 를 color 로 켠다
void led_on(uint32_t color) {
  ring.setPixelColor(0, color);
  ring.show(); // show() 를 해야 실제로 링에 전송된다
}

// 16개 전부 끈다
void led_all_off() {
  ring.clear(); // 16개 색을 모두 0 으로
  ring.show();
}

void setup() {
  ring.begin(); // NEO_PIN 을 출력으로 설정
  led_all_off();

  //                    R  G  B  W
  redColor = ring.Color(128, 0, 0, 0);
}

void loop() {
  led_on(redColor);
  delay(330);
  led_all_off();
  delay(330);
}
