#include <Arduino.h>
#include "DoorLight.h"

DoorLight::DoorLight() {
}

void DoorLight::init(byte doorNum, byte ledPin, byte pocketLedPin) {
  _numPixels = numPixels;
  _doorNum = doorNum;
  if (doorNum > 1) {
    _firstPixel = numPixelsFront + numPixelsPillar;
    _lastPixel = numPixels - 1;
  }
  for (int i = 0; i < _numPixels; i++) {
    _setTargetColor(i, 50, 50, 50);
    _setCurrentColor(i, 50, 50, 50);
  }

  pinMode(pocketLedPin, OUTPUT);
  digitalWrite(pocketLedPin, HIGH);

  _strip = new Adafruit_NeoPixel(max(numPixelsFront, numPixelsRear), ledPin, NEO_GRB + NEO_KHZ800);

  _strip->begin();
  _strip->setBrightness(30);
  _strip->clear();
  for (int i = _firstPixel; i <= numPixelsFront; i++)
    _strip->setPixelColor(i - _firstPixel, Adafruit_NeoPixel::Color(0, 0, 0));
  _strip->show();
}

void DoorLight::setColorByCarState(CarLight& carLight) {
  DoorState state = carLight.doorLightState[_doorNum];
  _brightness = carLight.brightness;
  unsigned long stateAge = millis() - carLight.doorLightMs[_doorNum];
  double max = 255;
  if (_brightness < 0x05)
    max = 0;
  else {
    max = map(_brightness, 0x05, 0xFF, 0x60, 0xFF);
    _brightness = map(_brightness, 0x05, 0xFF, 0x05, 0x30);
  }

  if (state == DOOR_OPEN) {
    for (int i = 0; i < _numPixels; i++) {
      _setTargetColor(i, 255, 0, 0);
    }
  } else if (state == WAIT) {
    for (int i = 0; i < _numPixels; i++) {
      _setTargetColor(i, 0, 0, 0);
    }
  } else if (state == DOOR_WAIT) {
    for (int i = 0; i < _numPixels; i++) {
      _setCurrentColor(i, 20, 0, 0);
      _setTargetColor(i, 20, 0, 0);
    }
  } else if (state == IDLE_INIT || state == IDLE || state == TURNING || state == TURNING_BLIND_SPOT || state == BLIND_SPOT || state == HANDS_ON_REQUIRED || state == HANDS_ON_WARNING || state == HANDS_ON_ALERT) {
    for (int i = 0; i < _numPixels; i++) {
      if (_brightness < 0x10) {
        _setTargetColor(i, 0, 0, max);
      } else {
        double r = max - map(i, 0, _numPixels, 0, max);
        double gMax = max * 0.9;
        if (gMax < 60)
          gMax = 60;
        double g = map(i, 0, _numPixels, 0, gMax);
        _setTargetColor(i, r, g, max);
      }
    }
  }

  if (state == IDLE_INIT) {
    _stripBrightness = _brightness;
    for (int i = 0; i < _numPixels; i++) {
      _setTargetColor(i, 255 - map(i, 0, _numPixels, 0, 255), map(i, 0, _numPixels, 0, 255), 255);
    }
    if (stateAge < 100) {
      for (int i = 0; i < _numPixels; i++) {
        _setCurrentColor(i, 0, 0, 0);
        _setTargetColor(i, 0, 0, 0);
      }
    } else if (stateAge < 1000) {
      int pos1 = map(stateAge, 0, 1000, 0, _numPixels);

      for (int i = pos1; i < _numPixels; i++) {
        _setCurrentColor(i, 0, 0, 0);
        _updateTargetColor(i, 0, 0, 0);
      }

      for (int i = pos1; i < _numPixels && i < pos1 + 5; i++) {
        _setCurrentColor(i, 255, 255, 255);
        _updateTargetColor(i, 255, 255, 255);
      }
    }
  }

  double g = max / 5;
  double r = max;
  if (r < 200)
    r = 200;
  if (g < 60)
    g = 60;


  int border = 38;

  if (state == TURNING_BLIND_SPOT) {
    for (int i = border; i < numPixelsFront; i++) {
      _setTargetColor(i, max, 0, 0);
    }
  }

  if (state == TURNING || state == TURNING_BLIND_SPOT) {
    for (int i = 0; i < border; i++) {
      _setTargetColor(i, 0, 0, 0);
      _setCurrentColor(i, 0, 0, 0);
    }
    if (lround(floor((stateAge + 100) / 360)) % 2 == 0) {
      for (int i = 0; i < 37; i++) {
        _setTargetColor(i, r, g, 0);
        _setCurrentColor(i, r, g, 0);
      }
    }
  }

  _fadeColor();
  _fadeBrightness();
  _pushColorToStrip();
  _oldBrightness = _stripBrightness;
}

void DoorLight::_updateTargetColor(int i, double r, double g, double b) {
  uint32_t color = Adafruit_NeoPixel::Color(round(r), round(g), round(b));
  _oldTargetColor[i] = color;
  _targetColor[0][i] = r;
  _targetColor[1][i] = g;
  _targetColor[2][i] = b;
}

void DoorLight::_setTargetColor(int i, double r, double g, double b) {
  uint32_t color = Adafruit_NeoPixel::Color(round(r), round(g), round(b));
  if (_oldTargetColor[i] != color) {
    _oldTargetColor[i] = color;
    _targetColor[0][i] = r;
    _targetColor[1][i] = g;
    _targetColor[2][i] = b;
    _targetStateMs = millis();
  }
}

void DoorLight::_setCurrentColor(int i, double r, double g, double b) {
  _currentColor[0][i] = r;
  _currentColor[1][i] = g;
  _currentColor[2][i] = b;
}

void DoorLight::_fadeBrightness() {
  if (_brightness > _stripBrightness)
    _stripBrightness++;
  if (_brightness < _stripBrightness)
    _stripBrightness--;
}

void DoorLight::_fadeColor() {
  unsigned long end = _targetStateMs + _transition;

  int left = end - _colorTransitionLastMs;
  if (left < 0)
    left = 0;

  int step = millis() - _colorTransitionLastMs;
  for (int i = 0; i < _numPixels; i++) {
    for (byte c = 0; c < 3; c++) {
      if (_currentColor[c][i] == _targetColor[c][i])
        continue;
      if (left == 0) {
        _currentColor[c][i] = _targetColor[c][i];
        continue;
      }
      double delta = _targetColor[c][i] - _currentColor[c][i];

      double needToChange = delta * step / left;
      if (abs(needToChange) > abs(_currentColor[c][i] - _targetColor[c][i]))
        _currentColor[c][i] == _targetColor[c][i];
      else
        _currentColor[c][i] += needToChange;
    }
  }
  _colorTransitionLastMs = millis();
}

void DoorLight::_pushColorToStrip() {
  bool changed = false;
  _strip->setBrightness(_stripBrightness);
  for (int i = _firstPixel; i <= _lastPixel; i++) {
    uint32_t color = Adafruit_NeoPixel::Color(round(_currentColor[0][i]), round(_currentColor[1][i]), round(_currentColor[2][i]));
    if (_oldColor[i] != color) {
      changed = true;
      _oldColor[i] = color;
    }
  }
  if (changed || _oldBrightness != _stripBrightness) {
    _strip->clear();
    for (int i = _firstPixel; i <= _lastPixel; i++)
      _strip->setPixelColor(i - _firstPixel, _oldColor[i]);
    _strip->show();
  }
}
