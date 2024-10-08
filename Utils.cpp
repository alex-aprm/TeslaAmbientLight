#include <Adafruit_NeoPixel.h>

void blinkSuccess(Adafruit_NeoPixel& strip) {
  strip.clear();
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 10, 0));
  }
  strip.show();
  delay(800);
  strip.clear();
  strip.show();
}

void blinkError(Adafruit_NeoPixel& strip) {
  strip.clear();
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(10, 0, 0));
  }
  strip.show();
  delay(300);
  strip.clear();
  strip.show();
}


