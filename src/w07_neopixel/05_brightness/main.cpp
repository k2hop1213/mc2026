/*
  7차시 NeoPixel - 예제 5  LED 링 밝기 변화

  NeoPixel 링 (16개, RGBW)
    IN (Data In) -> A4
    V+           -> 5V
    G            -> GND

  라이브러리 : platformio.ini 의 env 아래에
      lib_deps =
          adafruit/Adafruit NeoPixel@1.15.5

  16개 전부 초록색으로, 밝기를 0 -> 240 까지 10 씩 올렸다가 다시 0 까지 내린다.
*/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

const int NEO_PIN = A4; // 링 IN : A4 (PC4)
const int LED_COUNT = 16;

Adafruit_NeoPixel ring(LED_COUNT, NEO_PIN, NEO_GRBW + NEO_KHZ800);

// 16개 전부 같은 색으로 켠다
void led_all(uint32_t color) {
  for (int j = 0; j < LED_COUNT; j++) {
    ring.setPixelColor(j, color);
  }
  ring.show();
}

void led_ring_brightness() {
  // 밝아지기 : 0, 10, 20, ... , 240
  for (int i = 0; i < 25; i++) {
    led_all(ring.Color(0, i * 10, 0, 0)); // R, G, B, W
    delay(40);
  }

  // 어두워지기 : 230, 220, ... , 0
  for (int i = 23; i >= 0; i--) {
    led_all(ring.Color(0, i * 10, 0, 0));
    delay(40);
  }
}

void setup() {
  ring.begin();
  ring.clear();
  ring.show();
}

void loop() {
  led_ring_brightness();
}
