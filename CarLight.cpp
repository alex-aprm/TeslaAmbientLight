#include <Arduino.h>
#include "CarLight.h"

CarLight::CarLight() {
}

void CarLight::init() {
  _udp = new WiFiUDP();
  _udp->begin(_udpPort);
}

void CarLight::processCarState(Car& car) {
  long now = millis();
  if (_oldGear != car.gear) {
    if (_oldGear <= GEAR_PARK && car.gear >= GEAR_PARK)
      footwellLightState = FOOTWELL_OFF;
    if (_oldGear > GEAR_PARK && car.gear <= GEAR_PARK)
      footwellLightState = FOOTWELL_ON;
    _oldGear = car.gear;
  }

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
    bool blindSpot = car.blindSpotRight || car.blindSpotRightAlert;
    bool turning = car.turningRight;
    bool turningLight = car.turningRightLight;
    if (i == 1) {
      blindSpot = car.blindSpotLeft || car.blindSpotLeftAlert;
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

  if ((someDoorOpen || car.gear < GEAR_PARK) && brightness < 0xC8)
    brightness = 0xC8;

  for (byte i = 0; i < 4; i++)
    doorLightAge[i] = millis() - doorLightMs[i];
}

void CarLight::sendLightState() {
  unsigned long doorLightMinAge = doorLightAge[0];
  for (byte i = 1; i < 4; i++)
    if (doorLightAge[i] < doorLightMinAge)
      doorLightMinAge = doorLightAge[i];
  if ((allWait || allIdle) && doorLightMinAge > 15000) {
    _udpInterval = 10000;
  } else
    _udpInterval = 30;
  if (stateChanged || ((millis() - _lastUdp) > _udpInterval)) {
    _udp->beginPacket(_udpAddress, _udpPort);
    _udp->write(brightness);
    for (byte i = 0; i < 4; i++) {
      _udp->write(doorLightState[i]);
      _udp->write((byte)doorLightAge[i]);
      _udp->write((byte)(doorLightAge[i] >> 8));
      _udp->write((byte)(doorLightAge[i] >> 16));
      _udp->write((byte)(doorLightAge[i] >> 24));
    }
    _udp->endPacket();
    _udp->flush();
    _lastUdp = millis();
    stateChanged = false;
  }
}


void CarLight::receiveLightState() {
  //if (millis() - lastUdpMs > 10000)
  //  ESP.restart();

  //byte doorNum = role - 1;
  char packetBuffer[255];
  int packetSize = _udp->parsePacket();
  if (packetSize) {
    int len = _udp->read(packetBuffer, 255);
    if (len > 0) {
      _lastUdp = millis();
      brightness = packetBuffer[0];
      byte doorLen = 5;

      for (byte doorNum = 0; doorNum < 4; doorNum++) {
        byte lastState = doorLightState[doorNum];
        unsigned long stateMs = doorLightMs[doorNum];

        byte state = packetBuffer[1 + doorNum * doorLen];
        unsigned long age = 0;
        for (byte i = 0; i < 4; i++)
          age += packetBuffer[2 + i + doorNum * doorLen] * pow(255, i);

        unsigned long stateNewMs = millis() - age;
        if (state == lastState) {
          int d = stateNewMs > stateMs ? stateNewMs - stateMs : -(stateMs - stateNewMs);

          stateMs = stateMs + round(d / ++_stateCnt);
        } else {
          _stateCnt = 1;
          stateMs = stateNewMs;
          lastState = state;
        }
        doorLightState[doorNum] = (DoorState)lastState;
        doorLightMs[doorNum] = stateMs;
        doorLightAge[doorNum] = age;
      }
    }
  }
}

void CarLight::_changeDoorLightState(byte door, DoorState state) {
  doorLightState[door] = state;
  doorLightMs[door] = millis();
  stateChanged = true;
  Serial.print("Door ");
  Serial.print(door);
  Serial.print(" changed to ");
  Serial.println(state);
}

void CarLight::_changeDoorLightSubState(byte door, DoorState state) {
  doorLightState[door] = state;
  stateChanged = true;
  Serial.print("Door ");
  Serial.print(door);
  Serial.print(" changed to ");
  Serial.println(state);
}