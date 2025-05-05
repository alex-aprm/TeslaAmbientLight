#include "Arduino.h"
#include "Car.h"
#include <SPI.h>
#include <mcp_can.h>
#include "driver/twai.h"

Car::Car() {
}

void Car::initAS(byte c_pin, byte v_rx_pin, byte v_tx_pin) {
  _CCAN = new MCP_CAN(c_pin);
  if (_CCAN->begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    _CCAN->init_Mask(0, 0, 0x07FF0000);
    _CCAN->init_Filt(0, 0, 0x02730000);
    _CCAN->init_Filt(1, 0, 0x03990000);
    _CCAN->setMode(MCP_NORMAL);
    _c_enabled = true;
    Serial.println("Chassis CAN BUS OK");
  } else
    Serial.println("Error Initializing Cassis CAN bus...");

  // Initialize configuration structures using macro initializers
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)v_tx_pin, (gpio_num_t)v_rx_pin, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();  //Look in the api-reference for other speed sets.
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver installed");
  } else {
    Serial.println("Failed to install driver");
    return;
  }

  // Start TWAI driver
  if (twai_start() == ESP_OK) {
    Serial.println("Driver started");
  } else {
    Serial.println("Failed to start driver");
    return;
  }

  // Reconfigure alerts to detect frame receive, Bus-Off error and RX queue full states
  uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_TX_IDLE | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED;
  if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
    Serial.println("CAN Alerts reconfigured");
  } else {
    Serial.println("Failed to reconfigure alerts");
    return;
  }

  _v_twai_enabled = true;
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

  byte ap = (data[0] & 0x0F);
  autosteerOn = ap == 3 || ap == 4 || ap == 5;

  byte ho = ((data[5] >> 2) & 0x0F);
  handsOn = ho == 1;
  handsOnRequired = ho > 0;
  handsOnWarning = ho > 2;
  handsOnAlert = ho > 3;
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

  if (_v_twai_enabled) {
    // Check if alert happened
    uint32_t alerts_triggered;
    twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(10));
    twai_status_info_t twaistatus;
    twai_get_status_info(&twaistatus);

    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
      Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
      Serial.printf("Bus error count: %lu\n", twaistatus.bus_error_count);
    }
    if (alerts_triggered & TWAI_ALERT_TX_FAILED) {
      Serial.println("Alert: The Transmission failed.");
      Serial.printf("TX buffered: %lu\t", twaistatus.msgs_to_tx);
      Serial.printf("TX error: %lu\t", twaistatus.tx_error_counter);
      Serial.printf("TX failed: %lu\n", twaistatus.tx_failed_count);
    }
    if (alerts_triggered & TWAI_ALERT_TX_SUCCESS) {
      //Serial.println("Alert: The Transmission was successful.");
      // Serial.printf("TX buffered: %lu\t", twaistatus.msgs_to_tx);
    }

    // Handle alerts
    if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
      Serial.println("Alert: TWAI controller has become error passive.");
    }
    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
      Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
      Serial.printf("Bus error count: %lu\n", twaistatus.bus_error_count);
    }
    // if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
    //   Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
    //   Serial.printf("RX buffered: %lu\t", twaistatus.msgs_to_rx);
    //   Serial.printf("RX missed: %lu\t", twaistatus.rx_missed_count);
    //   Serial.printf("RX overrun %lu\n", twaistatus.rx_overrun_count);
    // }

    // Check if message is received
    if (alerts_triggered & TWAI_ALERT_RX_DATA) {
      // One or more messages received. Handle all.
      twai_message_t message;
      while (twai_receive(&message, 0) == ESP_OK) {
        _handle_twai_rx_message(message);
      }
      twai_clear_receive_queue();
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


void Car::_handle_twai_rx_message(twai_message_t &message) {

  long unsigned int rxId = message.identifier;
  unsigned char len = message.data_length_code;
  unsigned char rxBuf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  
  if (!(message.rtr)) {
    for (int i = 0; i < message.data_length_code; i++) {
      rxBuf[i] = message.data[i];
    }
    if (rxId == 0x3F5) {
      _processLights(len, rxBuf);
    } else if (rxId == 0x103) {
      _processRightDoors(len, rxBuf);
    } else if (rxId == 0x102) {
      _processLeftDoors(len, rxBuf);
    } else if (rxId == 0x2E1) {
      _processVehicleStatus(len, rxBuf);
    } else if (rxId == 0x118) {
      _processGear(len, rxBuf);
    }
  }
}
