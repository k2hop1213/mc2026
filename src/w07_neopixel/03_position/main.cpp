/*
  7차시 NeoPixel - 예제 3  한 개의 LED 를 원하는 위치에 출력

  NeoPixel 링 (16개, RGBW)
    IN (Data In) -> A4
    V+           -> 5V
    G            -> GND

  라이브러리 : platformio.ini 의 env 아래에
      lib_deps =
          adafruit/Adafruit NeoPixel@1.15.5

  LED 번호는 0 ~ 15.  0번은 링의 IN 바로 다음 LED 이다.
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

// index 번 LED 하나만 color 로 켜고 나머지는 끈다
void led_on(uint32_t color, int index) {
  for (int i = 0; i < LED_COUNT; i++) {
    if (i == index)
      ring.setPixelColor(i, color);
    else
      ring.setPixelColor(i, 0); // 0 = 꺼짐
  }
  ring.show();
}

void led_all_off() {
  ring.clear();
  ring.show();
}

void setup() {
  ring.begin();
  led_all_off();

  //                      R    G    B    W
  redColor = ring.Color(128, 0, 0, 0);
  greenColor = ring.Color(0, 128, 0, 0);
  blueColor = ring.Color(0, 0, 128, 0);
  whiteColor = ring.Color(0, 0, 0, 128); // 흰색은 W 칸을 쓴다
}

void loop() {
  led_on(redColor, 0);
  delay(330);
  led_on(greenColor, 4);
  delay(330);
  led_on(blueColor, 8);
  delay(330);
  led_on(whiteColor, 12);
  delay(330);
}
