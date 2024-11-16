#ifndef MIRROR_LIGHT_H
#define MIRROR_LIGHT_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "CarLight.h"
#include "DoorLight.h"

const int numPixelsMirror = 11;

class MirrorLight : public DoorLight {
public:
  MirrorLight();
  void init(byte doorNum, byte ledPin);
  void setColorByCarState(CarLight& carLight);
};
#endif