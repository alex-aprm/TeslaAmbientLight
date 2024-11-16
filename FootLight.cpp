#include <Arduino.h>
#include "FootLight.h"

FootLight::FootLight() {
}

void FootLight::init(byte doorNum, byte ledPin, byte ledHLPin, byte ledChannel) {
  _firstPixel = 0;
  _lastPixel = numPixelsFoot - 1;
  _transition = 1000;
  _numPixels = numPixelsFoot;
  _ledHLPin = ledHLPin;
  _ledChannel = ledChannel;
  _strip = new Adafruit_NeoPixel(numPixels, ledPin, NEO_GRB + NEO_KHZ800);
  _strip->begin();

  const int freq = 15000;
  const int resolution = 8;
  ledcSetup(_ledChannel, freq, resolution);
  ledcAttachPin(_ledHLPin, _ledChannel);
}

void FootLight::setColorByCarState(CarLight& carLight) {
  _stripBrightness = 0x50;
  if (carLight.footwellLightState == FOOTWELL_ON) {
    for (int i = 0; i < numPixelsFoot; i++) {
      _setTargetColor(i, 255, 180, 180);
    }
  } else {
    for (int i = 0; i < numPixelsFoot; i++) {
      _setTargetColor(i, 0, 0, 0);
    }
  }

  _fadeColor();
  ledcWrite(_ledChannel, _currentColor[0][0]);
  _pushColorToStrip();
}
