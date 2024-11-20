#ifndef CAR_H
#define CAR_H
#include <mcp_can.h>
#include <Arduino.h>

enum Gear : byte {
  GEAR_UNKNOWN,
  GEAR_PARK,
  GEAR_REAR,
  GEAR_NEUTRAL,
  GEAR_DRIVE
};

class Car {
public:
  Car();
  void init(byte v_pin, byte c_pin);
  void process();
  byte brightness = 10;
  bool displayOn = false;
  bool turningLeft = false;
  bool turningLeftLight = false;
  bool turningRight = false;
  bool turningRightLight = false;
  bool openFrunkWithDoor = false;
  bool blindSpotLeft = false;
  bool blindSpotRight = false;
  bool blindSpotLeftAlert = false;
  bool blindSpotRightAlert = false;
  bool autosteerOn = false;
  bool handsOn = true;
  bool handsOnRequired = false;
  bool handsOnWarning = false;
  bool handsOnAlert = false;
  Gear gear;
  bool doorOpen[4] = { false, false, false, false };
  bool frunkOpen = false;
  bool doorHandlePull[2] = { false, false };
  void openFrunk();
  void unlock();
  void unlockRemote();
  void wakeup();
private:
  bool _v_enabled = false;
  bool _c_enabled = false;
  MCP_CAN* _VCAN;
  MCP_CAN* _CCAN;
  bool _doorHandlePull = false;
  unsigned long _doorHandlePullMs;
  byte _doorHandlePullCount = 0;
  unsigned long _frunkOpenMs;
  unsigned long _doorChanged;
  unsigned char _vehicleControlFrame[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  void _processRightDoors(unsigned char len, unsigned char data[]);
  void _processLeftDoors(unsigned char len, unsigned char data[]);
  void _processDisplayOn(unsigned char len, unsigned char data[]);
  void _processLights(unsigned char len, unsigned char data[]);
  void _processAutopilot(unsigned char len, unsigned char data[]);
  void _processVehicleControl(unsigned char len, unsigned char data[]);
  void _processVehicleStatus(unsigned char len, unsigned char data[]);
  void _processGear(unsigned char len, unsigned char data[]);
  void _monitor(long unsigned int rxId, unsigned char len, unsigned char rxBuf[]);
  void _printMessage(unsigned char len, unsigned char data[], bool out);
};
#endif