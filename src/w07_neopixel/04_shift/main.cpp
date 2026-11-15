/*
  7차시 NeoPixel - 예제 4  LED 링 shift

  NeoPixel 링 (16개, RGBW)
    IN (Data In) -> A4
    V+           -> 5V
    G            -> GND

  라이브러리 : platformio.ini 의 env 아래에
      lib_deps =
          adafruit/Adafruit NeoPixel@1.15.5

  켜진 LED 하나가 0번 -> 15번으로 100ms 마다 한 칸씩 이동하고, 다시 0번부터 반복한다.
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

// j 번 LED 만 켜는 것을 j = 0 ~ 15 까지 차례로 한다
void led_ring_shift(uint32_t color) {
  for (int j = 0; j < LED_COUNT; j++) {
    for (int i = 0; i < LED_COUNT; i++) {
      if (i == j)
        ring.setPixelColor(i, color);
      else
        ring.setPixelColor(i, 0);
    }
    ring.show();
    delay(100);
  }
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
  led_ring_shift(redColor);
}
