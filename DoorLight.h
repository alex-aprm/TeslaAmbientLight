#ifndef DOOR_LIGHT_H
#define DOOR_LIGHT_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "CarLight.h"

const int numPixelsFront = 135;
const int numPixelsRear = 100;
const int numPixelsPillar = 5;
const int numPixelsMirror = 11;
const int numPixels = numPixelsFront + numPixelsPillar + numPixelsRear;
const int numPixelsTotal = numPixelsFront + numPixelsPillar + numPixelsRear + numPixelsMirror;

class DoorLight {
public:
  DoorLight();

  void init(byte doorNum, byte ledPin, byte pocketLedPin, byte mirrorPin);
  void setColorByCarState(CarLight& carLight);
protected:
  Adafruit_NeoPixel* _strip;
  Adafruit_NeoPixel* _stripMirror;
  byte _doorNum;
  byte _ledPin;
  byte _pocketLedPin;
  byte _oldBrightness;
  byte _stripBrightness;
  byte _brightness;
  unsigned int _transition = 100;
  int _firstPixel = 0;
  int _lastPixel = numPixelsFront - 1;
  int _firstPixelMirror = numPixelsFront + numPixelsPillar + numPixelsRear;
  int _lastPixelMirror = numPixelsFront + numPixelsPillar + numPixelsRear + numPixelsMirror;
  double _currentColor[3][numPixelsTotal];
  uint32_t _oldColor[numPixelsTotal];
  double _targetColor[3][numPixelsTotal];
  uint32_t _oldTargetColor[numPixelsTotal];
  unsigned long _colorTransitionLastMs;
  unsigned long _targetStateMs;

  void _setTargetColor(int i, double r, double g, double b);
  void _setCurrentColor(int i, double r, double g, double b);
  void _updateTargetColor(int i, double r, double g, double b);

  void _fadeColor();
  void _fadeBrightness();
  void _pushColorToStrip();
  void _pushColorToMirrorStrip();
};
#endif