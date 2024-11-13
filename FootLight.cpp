#include <Arduino.h>
#include "FootLight.h"

FootLight::FootLight() {
}

void FootLight::init(byte doorNum, byte ledPin) {
  _strip = new Adafruit_NeoPixel(numPixelsFoot, ledPin, NEO_GRB + NEO_KHZ800);
  _strip->begin();
}

void FootLight::setColorByCarState(CarLight& carLight) {
  _brightness = carLight.brightness;
  double max = 255;
  if (_brightness < 0x05)
    max = 0;
  else {
    max = map(_brightness, 0x05, 0xFF, 0x60, 0xFF);
  }
  for (int i = 0; i < numPixelsFoot; i++) {
    _setTargetColor(i, 255, 180, 180);
  }


  _fadeColor();
  _fadeBrightness();
  _pushColorToStrip();
  _oldBrightness = _stripBrightness;
}
