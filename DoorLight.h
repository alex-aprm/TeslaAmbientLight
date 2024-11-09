#ifndef DOOR_LIGHT_H
#define DOOR_LIGHT_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "CarLight.h"

const int numPixelsFront = 135;
const int numPixelsRear = 100;
const int numPixelsPillar = 5;
const int numPixels = numPixelsFront + numPixelsPillar + numPixelsRear;

class DoorLight {
public:
  DoorLight();

  void init(byte doorNum, byte ledPin, byte pocketLedPin);
  void setColorByCarState(CarLight& carLight);
private:
  Adafruit_NeoPixel* _strip;
  byte _doorNum;
  byte _ledPin;
  byte _pocketLedPin;
  byte _oldBrightness;
  byte _stripBrightness;
  int _firstPixel = 0;
  int _lastPixel = numPixelsFront - 1;
  double _currentColor[3][numPixels];
  uint32_t _oldColor[numPixels];
  double _targetColor[3][numPixels];
  uint32_t _oldTargetColor[numPixels];
  unsigned long _colorTransitionLastMs;
  unsigned long _targetStateMs;

  void _setTargetColor(int i, double r, double g, double b);
  void _setCurrentColor(int i, double r, double g, double b);

  void _fadeColor();
  void _pushColorToStrip();
};
#endif