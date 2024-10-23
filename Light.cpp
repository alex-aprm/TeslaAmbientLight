#ifndef LIGHT_H
#define LIGHT_H
#include "Arduino.h"
#include "Light.h"
#include "Car.h"

Light::Light() {
}

void Light::processCarState(Car &car) {
  long now = millis();
  /*
Serial.print("Display ");
Serial.println(car.displayOn);
  */

  for (byte i = 0; i < 4; i++) {
    if (doorLightState[i] == IDLE_INIT && now - doorLightMs[i] > 1000)
      _changeDoorLightState(i, IDLE);
  }

  someDoorOpen = false;
  for (byte i = 0; i < 4; i++) {
    someDoorOpen = car.doorOpen[i] || someDoorOpen;
    if (car.doorOpen[i] && doorLightState[i] != DOOR_OPEN)
      _changeDoorLightState(i, DOOR_OPEN);
    if (!car.doorOpen[i] && doorLightState[i] == DOOR_OPEN)
      _changeDoorLightState(i, IDLE_INIT);
  }

  if (someDoorOpen) {
    for (byte i = 0; i < 4; i++)
      if (!car.doorOpen[i] && doorLightState[i] != DOOR_WAIT)
        _changeDoorLightState(i, DOOR_WAIT);
  } else {
    for (byte i = 0; i < 4; i++)
      if (doorLightState[i] == DOOR_WAIT)
        _changeDoorLightState(i, IDLE_INIT);
  }

  if (!car.displayOn) {
    for (byte i = 0; i < 4; i++)
      if (doorLightState[i] == IDLE || doorLightState[i] == IDLE_INIT)
        _changeDoorLightState(i, WAIT);
  } else {
    for (byte i = 0; i < 4; i++)
      if (doorLightState[i] == WAIT)
        _changeDoorLightState(i, IDLE_INIT);
  }

  for (byte i = 0; i < 2; i++) {
    bool blindSpot = car.blindSpotRight;
    bool turning = car.turningRight;
    bool turningLight = car.turningRightLight;
    if (i == 1) {
      blindSpot = car.blindSpotLeft;
      turning = car.turningLeft;
      turningLight = car.turningLeftLight;
    }
    if (blindSpot) {
      if (doorLightState[i] == IDLE)
        _changeDoorLightState(i, BLIND_SPOT);
      if (doorLightState[i] == TURNING)
        _changeDoorLightSubState(i, TURNING_BLIND_SPOT);
    }
    if (!blindSpot) {
      if (doorLightState[i] == BLIND_SPOT)
        _changeDoorLightState(i, IDLE);
      if (doorLightState[i] == TURNING_BLIND_SPOT)
        _changeDoorLightSubState(i, TURNING);
    }
    if ((turning || turningLight) && (doorLightState[i] == IDLE))
      _changeDoorLightState(i, TURNING);

    if ((turning || turningLight) && (doorLightState[i] == BLIND_SPOT))
      _changeDoorLightSubState(i, TURNING_BLIND_SPOT);
      
    if (!(turning || turningLight) && doorLightState[i] == TURNING)
      _changeDoorLightState(i, IDLE);

    if (!(turning || turningLight) && (doorLightState[i] == TURNING_BLIND_SPOT))
      _changeDoorLightState(i, BLIND_SPOT);
  }
 
  allWait = true;
  for (byte i = 0; i < 4; i++)
    allWait = (doorLightState[i] == WAIT) && allWait;
  allIdle = true;
  for (byte i = 0; i < 4; i++)
    allIdle = (doorLightState[i] == IDLE) && allIdle;

  byte oldBrightness = brightness;
  brightness = car.brightness;

  stateChanged = stateChanged || (oldBrightness != brightness);

  if (someDoorOpen && brightness < 100)
    brightness += 100;
    
  for (byte i = 0; i < 4; i++)
    doorLightAge[i] = millis() - doorLightMs[i];
}

void Light::_changeDoorLightState(byte door, DoorState state) {
  doorLightState[door] = state;
  doorLightMs[door] = millis();
  stateChanged = true;
  Serial.print("Door ");
  Serial.print(door);
  Serial.print(" changed to ");
  Serial.println(state);
}

void Light::_changeDoorLightSubState(byte door, DoorState state) {
  doorLightState[door] = state;
  stateChanged = true;
  Serial.print("Door ");
  Serial.print(door);
  Serial.print(" changed to ");
  Serial.println(state);
}

#endif