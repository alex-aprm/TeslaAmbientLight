#ifndef FOOT_LIGHT_H
#define FOOT_LIGHT_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "CarLight.h"
#include "DoorLight.h"

const int numPixelsFoot = 3;

class FootLight : public DoorLight {
public:
  FootLight();
  void init(byte doorNum, byte ledPin, byte ledHLPin, byte ledChannel);
  void setColorByCarState(CarLight& carLight);
private:
  byte _ledChannel;
  byte _ledHLPin;
};
#endif