#include "Arduino.h"
#include "Car.h"
#include <mcp_can.h>


Car::Car() {
}

void Car::init(byte v_pin, byte c_pin) {
  _VCAN = new MCP_CAN(v_pin);
  _CCAN = new MCP_CAN(c_pin);
  if (_CCAN->begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    _CCAN->init_Mask(0, 0, 0x07FF0000);
    _CCAN->init_Filt(0, 0, 0x03990000);
    _CCAN->init_Filt(1, 0, 0x039D0000);
    _CCAN->init_Mask(1, 0, 0x07FF0000);
    _CCAN->init_Filt(2, 0, 0x03F80000);
    _CCAN->init_Filt(3, 0, 0x02380000);
    _CCAN->setMode(MCP_NORMAL);
    _c_enabled = true;
    Serial.println("Chassis CAN BUS OK");
  } else
    Serial.println("Error Initializing Cassis CAN bus...");

  if (_VCAN->begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    _v_enabled = true;
    _VCAN->init_Mask(0, 0, 0x07FF0000);
    _VCAN->init_Filt(0, 0, 0x03F50000);
    _VCAN->init_Filt(1, 0, 0x03530000);
    _VCAN->init_Mask(1, 0, 0x07FF0000);
    _VCAN->init_Filt(2, 0, 0x01020000);
    _VCAN->init_Filt(3, 0, 0x01030000);
    _VCAN->setMode(MCP_NORMAL);
    Serial.println("Vehicle CAN BUS OK");
  } else
    Serial.println("Error Initializing Vehicle CAN bus...");
}

void Car::_processDisplayOn(unsigned char len, unsigned char data[]) {
  //printMessage(len, data, false);
  //displayOn =  data[0] & 0x20;
  //brightness = map(data[2], 0x0B, 0xC8, 0, 0xFF);
}

void Car::_processLights(unsigned char len, unsigned char data[]) {
  // _printMessage(len, data, false);
  if (data[2] == 0x00) {
    brightness = 0;
    displayOn = false;
  } else {
    displayOn = true;
    byte v = data[2];
    if (v == 0x0B)
      brightness = 0;
    else {
      brightness = map(v, 0x0B, 0xC8, 0, 0xFF);
      if (brightness < 34)
        brightness = 34;
      brightness = map(brightness, 34, 0xFF, 5, 0xFF);
    }
  }
}

void Car::_printMessage(unsigned char len, unsigned char data[], bool out) {
  char msgString[128];
  for (byte i = 0; i < len; i++) {
    sprintf(msgString, " 0x%.2X", data[i]);
    Serial.print(msgString);
  }
  if (out) {
    Serial.print(" OUT");
  }
  Serial.println();
}

void Car::_monitor(long unsigned int rxId, unsigned char len, unsigned char rxBuf[]) {
  Serial.write(0x55);
  Serial.write(0x55);
  Serial.write(0x55);
  Serial.write(0x55);
  Serial.write(0x55);
  Serial.write(0x55);
  Serial.write(0x55);
  Serial.write(0x55);
  byte buf[4];
  buf[0] = rxId & 255;
  buf[1] = (rxId >> 8) & 255;
  buf[2] = (rxId >> 16) & 255;
  buf[3] = (rxId >> 24) & 255;

  Serial.write(buf, sizeof(buf));
  Serial.write(len);
  for (byte i = 0; i < len; i++)
    Serial.write(rxBuf[i]);
}

void Car::_processRightDoors(unsigned char len, unsigned char data[]) {
  doorOpen[0] = data[0] & 0x01;
  doorOpen[2] = data[0] & 0x10;
  // printMessage(len, data, false);
}
void Car::_processLeftDoors(unsigned char len, unsigned char data[]) {
  doorOpen[1] = data[0] & 0x01;
  doorOpen[3] = data[0] & 0x10;
  // printMessage(len, data, false);
}

void Car::process() {
  long unsigned int rxId;
  unsigned char len = 0;
  unsigned char rxBuf[8];

  if (_v_enabled) {
    _VCAN->readMsgBuf(&rxId, &len, rxBuf);

    if (rxId == 0x3F5) {
      //processTurnSignals(len, rxBuf);
      _processLights(len, rxBuf);
    } else if (rxId == 0x229) {
      //processGearStalk(len, rxBuf);
    } else if (rxId == 0x7FF) {
      //processGTW(len, rxBuf);
    } else if (rxId == 0x353) {
      _processDisplayOn(len, rxBuf);
    } else if (rxId == 0x103) {
      _processRightDoors(len, rxBuf);
    } else if (rxId == 0x102) {
      _processLeftDoors(len, rxBuf);
    }
  }

  if (_c_enabled) {
    _CCAN->readMsgBuf(&rxId, &len, rxBuf);  // Read data: len = data length, buf = data byte(s)
    if (rxId == 0x399) {
      //processAutopilot(len, rxBuf);
    } else if (rxId == 0x39D) {
      //processBrake(len, rxBuf);
    } else if (rxId == 0x3F8) {
      //processHandsOn(len, rxBuf);
    } else if (rxId == 0x238) {
      //processHandsOn1(len, rxBuf);
    }
  }

  /*  if (millis() - _doorChanged > 3000) {
    _doorChanged = millis();
    doorOpen[0] = !doorOpen[0];
  }*/
}
