/*
  7차시 NeoPixel - 예제 2  한 개의 LED 를 R, G, B, W 순으로 반복 출력

  NeoPixel 링 (16개, RGBW)
    IN (Data In) -> A4
    V+           -> 5V
    G            -> GND

  라이브러리 : platformio.ini 의 env 아래에
      lib_deps =
          adafruit/Adafruit NeoPixel@1.15.5

  색은 Color(빨강, 초록, 파랑, 흰색) 순서로 적는다.
  링이 받는 G, R, B, W 순서로 바꾸는 일은 라이브러리가 한다.
*/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

const int NEO_PIN = A4; // 링 IN : A4 (PC4)
const int LED_COUNT = 16;

Adafruit_NeoPixel ring(LED_COUNT, NEO_PIN, NEO_GRBW + NEO_KHZ800);

uint32_t redColor;
uint32_t greenColor;
uint32_t blueColor;
uint32_t whiteColor;

// 0번 LED 를 color 로 켠다
void led_on(uint32_t color) {
  ring.setPixelColor(0, color);
  ring.show();
}

void setup() {
  ring.begin();
  ring.clear();
  ring.show();

  //                      R    G    B    W
  redColor = ring.Color(128, 0, 0, 0);
  greenColor = ring.Color(0, 128, 0, 0);
  blueColor = ring.Color(0, 0, 128, 0);
  whiteColor = ring.Color(0, 0, 0, 128); // 흰색은 W 칸을 쓴다
}

void loop() {
  led_on(redColor);
  delay(330);
  led_on(greenColor);
  delay(330);
  led_on(blueColor);
  delay(330);
  led_on(whiteColor);
  delay(330);
}
