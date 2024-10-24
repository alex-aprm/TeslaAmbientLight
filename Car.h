#ifndef CAR_H
#define CAR_H
#include <mcp_can.h>
#include <Arduino.h>

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

  bool blindSpotLeft = false;
  bool blindSpotRight = false;
  bool blindSpotLeftAlert = false;
  bool blindSpotRightAlert = false;
  bool doorOpen[4] = {false, false, false, false};
  bool frunkOpen = false;
  bool doorHandlePull[2] = {false, false};
  void openFrunk();
  void unlock();
  void unlockRemote();
private:
  bool _v_enabled = false;
  bool _c_enabled = false;
  MCP_CAN* _VCAN;
  MCP_CAN* _CCAN;
  unsigned long _doorChanged; 
  unsigned char _vehicleControlFrame[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  void _processRightDoors(unsigned char len, unsigned char data[]);
  void _processLeftDoors(unsigned char len, unsigned char data[]);
  void _processDisplayOn(unsigned char len, unsigned char data[]);
  void _processLights(unsigned char len, unsigned char data[]);
  void _processAutopilot(unsigned char len, unsigned char data[]);
  void _processVehicleControl(unsigned char len, unsigned char data[]);
  void _processVehicleStatus(unsigned char len, unsigned char data[]);
  void _monitor(long unsigned int rxId, unsigned char len, unsigned char rxBuf[]);
  void _printMessage(unsigned char len, unsigned char data[], bool out);
};
#endif