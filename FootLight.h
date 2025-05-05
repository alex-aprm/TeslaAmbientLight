#ifndef FOOT_LIGHT_H
#define FOOT_LIGHT_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "CarLight.h"
#include "DoorLight.h"
#include "driver/ledc.h"

const int numPixelsFoot = 3;

class FootLight : public DoorLight {
public:
  FootLight();
  void initRGB(byte doorNum, byte ledPin);
  void initHL(byte doorNum, byte ledHLPin);
  void setColorByCarState(CarLight& carLight);
private:
  ledc_channel_t _ledChannel;
  ledc_timer_t _ledTimer;
  byte _ledHLPin;
};
#endif