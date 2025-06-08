#ifndef CAR_LIGHT_H
#define CAR_LIGHT_H
#include <Arduino.h>
#include <WiFiUdp.h>
#include "Car.h"

enum DoorRole : byte {
  DOOR_MASTER,
  DOOR_FRONT_RIGHT,
  DOOR_FRONT_LEFT,
  DOOR_REAR_RIGHT,
  DOOR_REAR_LEFT
};

enum DoorState : byte {
  WAIT,
  DOOR_OPEN,
  DOOR_WAIT,
  IDLE_INIT,
  IDLE,
  TURNING,
  BLIND_SPOT,
  TURNING_BLIND_SPOT,
  HANDS_ON_REQUIRED,
  HANDS_ON_WARNING,
  HANDS_ON_ALERT
};

enum FootwellState : byte {
  FOOTWELL_OFF,
  FOOTWELL_ON
};

class CarLight {
public:
  CarLight();
  void init();
  void processCarState(Car& car);
  void sendLightState();
  void receiveLightState();
  FootwellState footwellLightState = FOOTWELL_ON;
  DoorState doorLightState[4] = { WAIT, WAIT, WAIT, WAIT };
  unsigned long doorLightMs[4] = { 0, 0, 0, 0 };
  unsigned long doorLightAge[4] = { 0, 0, 0, 0 };
  byte brightness = 0;
  bool allWait = true;
  bool allIdle = false;
  bool someDoorOpen = false;
  bool stateChanged = false;
private:
  const char* _udpAddress = "255.255.255.255";
  const int _udpPort = 3333;
  WiFiUDP* _udp;
  unsigned long _lastUdp = 0;
  unsigned int _udpInterval = 100;
  int _stateCnt;
  void _changeDoorLightState(byte door, DoorState state);
  void _changeDoorLightSubState(byte door, DoorState state);
  Gear _oldGear;
};

#endif