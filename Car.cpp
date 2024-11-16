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
    _CCAN->init_Filt(0, 0, 0x02730000);
    _CCAN->init_Filt(1, 0, 0x03990000);
    /*_CCAN->init_Mask(1, 0, 0x07FF0000);
    _CCAN->init_Filt(2, 0, 0x03FD0000);
    _CCAN->init_Filt(3, 0, 0x02380000);
    _CCAN->init_Filt(4, 0, 0x02730000);
    _CCAN->init_Filt(5, 0, 0x03990000);
    _CCAN->init_Filt(6, 0, 0x02290000);*/
    _CCAN->setMode(MCP_NORMAL);
    _c_enabled = true;
    Serial.println("Chassis CAN BUS OK");
  } else
    Serial.println("Error Initializing Cassis CAN bus...");

  if (_VCAN->begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    _v_enabled = true;
    _VCAN->init_Mask(0, 0, 0x07FF0000);
    _VCAN->init_Filt(0, 0, 0x03F50000);
    _VCAN->init_Mask(1, 0, 0x07FF0000);
    _VCAN->init_Filt(1, 0, 0x01020000);
    _VCAN->init_Filt(2, 0, 0x01030000);
    _VCAN->init_Filt(3, 0, 0x02E10000);
    _VCAN->init_Filt(4, 0, 0x01180000);
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

void Car::_processAutopilot(unsigned char len, unsigned char data[]) {
  blindSpotLeft = ((data[0] >> 4) & 0x03) > 0;
  blindSpotRight = ((data[0] >> 6) & 0x03) > 0;
  blindSpotLeftAlert = ((data[0] >> 4) & 0x03) > 1;
  blindSpotRightAlert = ((data[0] >> 6) & 0x03) > 1;
  /*Serial.print(blindSpotLeft);
  Serial.print("     ");
  Serial.print(blindSpotRight);
  Serial.print("     ");
  Serial.print(blindSpotLeftAlert);
  Serial.print("     ");
  Serial.print(blindSpotRightAlert);
  Serial.println();*/
}

void Car::_processLights(unsigned char len, unsigned char data[]) {
  turningRightLight = (data[0] & 0x08) == 0x08;
  turningRight = (data[0] & 0x04) == 0x04;
  turningLeftLight = (data[0] & 0x02) == 0x02;
  turningLeft = (data[0] & 0x01) == 0x01;

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
      brightness = map(brightness, 34, 0xFF, 0x05, 0xFF);
    }
  }
}

void Car::unlockRemote() {
  if (_vehicleControlFrame[0] > 0) {
    unsigned char snd[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for (byte i = 0; i < 8; i++)
      snd[i] = _vehicleControlFrame[i];
    snd[2] = snd[2] | 0x6;
    _CCAN->sendMsgBuf(0x273, 8, snd);
  }
}

void Car::unlock() {
  if (_vehicleControlFrame[0] > 0) {
    unsigned char snd[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for (byte i = 0; i < 8; i++)
      snd[i] = _vehicleControlFrame[i];
    snd[2] = snd[2] | 0x4;
    _CCAN->sendMsgBuf(0x273, 8, snd);
  }
}

void Car::openFrunk() {
  if (_vehicleControlFrame[0] > 0 && !frunkOpen) {
    unsigned char snd[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for (byte i = 0; i < 8; i++)
      snd[i] = _vehicleControlFrame[i];
    snd[0] = snd[0] | 0x20;
    _CCAN->sendMsgBuf(0x273, 8, snd);
  }
}


void Car::wakeup() {
  if (_vehicleControlFrame[0] > 0 && !frunkOpen) {
    unsigned char snd[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for (byte i = 0; i < 8; i++)
      snd[i] = _vehicleControlFrame[i];
    snd[0] = snd[0] | 0x01;
    _CCAN->sendMsgBuf(0x273, 8, snd);
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
  doorHandlePull[0] = data[1] & 0x04;
  //Serial.println();
  //_printMessage(len, data, false);
}

void Car::_processLeftDoors(unsigned char len, unsigned char data[]) {
  doorOpen[1] = data[0] & 0x01;
  doorOpen[3] = data[0] & 0x10;
  doorHandlePull[1] = data[1] & 0x04;
  //_printMessage(len, data, false);
}

void Car::_processGear(unsigned char len, unsigned char data[]) {
  gear = (Gear)((data[2] >> 5) & 0x07);
  if (gear == 0x07)
    gear = (Gear)GEAR_UNKNOWN;
}

void Car::_processVehicleStatus(unsigned char len, unsigned char data[]) {
  if ((data[0] & 0x07) != 0)
    return;
  frunkOpen = ((data[0] >> 3) & 0x0F) != 2;
}


void Car::_processVehicleControl(unsigned char len, unsigned char data[]) {
  //_printMessage(len, data, false);
  for (byte i = 0; i < len; i++)
    _vehicleControlFrame[i] = data[i];
}

void Car::process() {
  if (_v_enabled && _VCAN->checkReceive()) {
    long unsigned int rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];
    _VCAN->readMsgBuf(&rxId, &len, rxBuf);
    if (rxId == 0x3F5) {
      _processLights(len, rxBuf);
    } else if (rxId == 0x229) {
      //processGearStalk(len, rxBuf);
    } else if (rxId == 0x7FF) {
      //processGTW(len, rxBuf);
    } else if (rxId == 0x353) {
      // _processDisplayOn(len, rxBuf);
    } else if (rxId == 0x103) {
      _processRightDoors(len, rxBuf);
    } else if (rxId == 0x102) {
      _processLeftDoors(len, rxBuf);
    } else if (rxId == 0x2E1) {
      _processVehicleStatus(len, rxBuf);
    } else if (rxId == 0x118) {
      /*Serial.print("V ");
      Serial.print(rxId);
      Serial.print(" ");
      _printMessage(len, rxBuf, false);*/
       _processGear(len, rxBuf);
    } else if (rxId == 0x229) {
      //Serial.print(" VC 229 ");
      //_printMessage(len, rxBuf, false);
    } else if (rxId == 0x3DF) {
      //Serial.print(" VC 3DF ");
      // screen dark mode
      //_printMessage(len, rxBuf, false);
    } else if (rxId == 0x2D3) {
      //Serial.print(" VC 2D3 ");
      //_printMessage(len, rxBuf, false);
    }
  }

  if (_c_enabled && _CCAN->checkReceive()) {
    long unsigned int rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];
    _CCAN->readMsgBuf(&rxId, &len, rxBuf);  // Read data: len = data length, buf = data byte(s)

    if (rxId == 0x399) {
      _processAutopilot(len, rxBuf);
    } else if (rxId == 0x39D) {
      //processBrake(len, rxBuf);
    } else if (rxId == 0x3FD) {
      //_printMessage(len, rxBuf, false);
      //processHandsOn(len, rxBuf);
    } else if (rxId == 0x238) {
      //processHandsOn1(len, rxBuf);
    } else if (rxId == 0x273) {
      _processVehicleControl(len, rxBuf);
    } else if (rxId == 0x229) {
      //_processGearStalk(len, rxBuf);
    } else if (rxId == 0x118) {
      //Serial.print(" CC 118 ");
      //_printMessage(len, rxBuf, false);
    } else if (rxId == 0x229) {
      //Serial.print(" CC 229 ");
      //_printMessage(len, rxBuf, false);
    } else if (rxId == 0x339) {
      //Serial.print(" CC 339 ");
      // lock and keys data
      //_printMessage(len, rxBuf, false);
    }
  }


  if (openFrunkWithDoor) {
    if (doorHandlePull[0] || doorHandlePull[1]) {
      if (!_doorHandlePull) {
        _doorHandlePull = true;
        if (millis() - _doorHandlePullMs > 1500)
          _doorHandlePullCount = 0;
        _doorHandlePullMs = millis();
        Serial.println("Handle pulled");
      }
    } else {
      if (_doorHandlePull) {
        if (millis() - _doorHandlePullMs > 500)
          _doorHandlePullCount++;
        else
          _doorHandlePullCount = 0;
        _doorHandlePull = false;
        Serial.println("Handle left");
      }
    }

    if (_doorHandlePull && _doorHandlePullCount == 2 && _frunkOpenMs < _doorHandlePullMs) {
      _frunkOpenMs = millis();
      _doorHandlePullCount = 0;
      Serial.println("Opening");
      openFrunk();
    }
  }
 /*
  brightness = 0xFF;
  displayOn = true;

  if ((millis() / 10000) % 2 == 1) {
   blindSpotRight = false;
    blindSpotLeft = false;
  } else {
     blindSpotRight = true;
        blindSpotLeft = true;
  }
  
  
  if ((millis() / 5000) % 2 == 1) {
   gear = GEAR_PARK;
  } else {
   gear = GEAR_DRIVE;
  }
  */
  
  
}
