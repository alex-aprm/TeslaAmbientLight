#include <Arduino.h>
#include "Car.h"

enum DoorState : byte { WAIT,
                        DOOR_OPEN,
                        DOOR_WAIT,
                        IDLE_INIT,
                        IDLE,
                        TURNING,
                        BLIND_SPOT,
                        TURNING_BLIND_SPOT};
class Light {
public:
  Light();
  //void init(byte v_pin, byte c_pin);
  void processCarState(Car &car);
  DoorState doorLightState[4] = { WAIT, WAIT, WAIT, WAIT };
  unsigned long doorLightMs[4] = { 0, 0, 0, 0 };
  unsigned long doorLightAge[4] = { 0, 0, 0, 0 };
  byte brightness = 0;
  bool allWait = true;
  bool allIdle = false;
  bool someDoorOpen = false;
  bool stateChanged = false;
private:
  void _changeDoorLightState(byte door, DoorState state);
  void _changeDoorLightSubState(byte door, DoorState state);
};