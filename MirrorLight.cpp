#include <Arduino.h>
#include "MirrorLight.h"

MirrorLight::MirrorLight() {
}

void MirrorLight::init(byte doorNum, byte ledPin) {
  _numPixels = numPixelsMirror;
  _lastPixel = numPixelsMirror - 1;
  _doorNum = doorNum;
  _transition = 100;
  _strip = new Adafruit_NeoPixel(_numPixels, ledPin, NEO_GRB + NEO_KHZ800);
  _strip->begin();
}

void MirrorLight::setColorByCarState(CarLight& carLight) {
  DoorState state = carLight.doorLightState[_doorNum];
  _brightness = carLight.brightness;
  unsigned long stateAge = millis() - carLight.doorLightMs[_doorNum];
  if (_brightness < 0x20)
    _brightness = 0x20;
  else {
    _brightness = map(_brightness, 0x20, 0xFF, 0x20, 0x60);
  }
  for (int i = 0; i < _numPixels; i++) {
    _setTargetColor(i, 0, 0, 0);
  }
  if (state == TURNING_BLIND_SPOT) {
    for (int i = 0; i < _numPixels - 5; i++) {
      _setTargetColor(i, 255, 0, 0);
    }
  } else if (state == BLIND_SPOT) {
    for (int i = 4; i < _numPixels - 5; i++) {
      _setTargetColor(i, 255, 80, 0);
    }
  }
  _fadeColor();
  _fadeBrightness();
  _pushColorToStrip();
  _oldBrightness = _stripBrightness;
}
