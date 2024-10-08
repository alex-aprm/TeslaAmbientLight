/*
void processHandsOn1(unsigned char len, unsigned char data[]) {
  data[5] = data[5] | 0x80;
  CCAN.sendMsgBuf(0x328, 8, data);
}

void processHandsOn(unsigned char len, unsigned char data[]) {
  bool changed = false;
  for (byte i = 0; i < len; i++) {
    if (driveAssistState[i] != data[i]) {
      driveAssistState[i] = data[i];
      changed = true;
    }
  }
  data[1] = data[1] | 0x40;
  CCAN.sendMsgBuf(0x3F8, 8, data);
  if (changed)
    printMessage(len, data, false);
}*/

void processGearStalk(unsigned char len, unsigned char data[]) {
  printMessage(len, data, false);
}


void processBrake(unsigned char len, unsigned char data[]) {
  if ((data[2] & 0x03) > 1) {
    lastBrakeApplied = millis();
    autopilotEnableRequested = 0;
  }
}

void processDisplayOn(unsigned char len, unsigned char data[]) {
  printMessage(len, data, false);
  displayOn = data[0] & 0x20;
  //brightness = map(data[2], 0x0B, 0xC8, 0, 0xFF);
}

void processLights(unsigned char len, unsigned char data[]) {
  //printMessage(len, data, false);
  brightness = map(data[2], 0x0B, 0xC8, 0, 0xFF);
}

void processAutopilot(unsigned char len, unsigned char data[]) {
  if ((autopilotState[0] & 0x0F) == 0x03) {
    lastAutopilotActiveTime = millis();
  }
  for (byte i = 0; i < len; i++) {
    autopilotState[i] = data[i];
  }
  //printMessage(len, data, false);

  if (lastAutopilotState != (autopilotState[0] & 0x0F)) {
    lastAutopilotState = (autopilotState[0] & 0x0F);
    if (millis() - autopilotEnableRequested < 800 && lastAutopilotState == 0x02) {
      Serial.println("Trying to enable Autopilot back");
    }
    /*
    sprintf(msgString, "Autopilot changed 0x%.2X", lastAutopilotState);
    Serial.print(msgString);
      Serial.println();
      */
  }
  // processAutopilotResponse();
}

void processTurnSignals(unsigned char len, unsigned char data[]) {
  if ((data[5] & 0x03) > 0) {
    if (lastTurnSignalActivated == 0) {
      lastTurnSignalActivated = millis();
      Serial.print("Turning... ");
      for (byte i = 0; i < 20; i++) {
        VCAN.sendMsgBuf(0x229, 3, lever);
      }
      if (lastAutopilotState == 0x03) {
        Serial.print("Autopilot is ON");
      } else {
        Serial.print("Autopilot is OFF");
      }

      Serial.println();
    }
  } else {
    if (lastTurnSignalActivated > 0) {
      long turnTime = millis() - lastTurnSignalActivated;
      Serial.print("Turned! Took ");
      Serial.print(turnTime);
      if (millis() - lastAutopilotActiveTime < turnTime) {
        autopilotEnableRequested = millis();
      }
      sprintf(msgString, " 0x%.2X", autopilotState[0]);
      Serial.print(msgString);

      Serial.println();

      lastTurnSignalActivated = 0;
    }
  }
  //printMessage(len, data, false);
}
/*
void processAutopilotResponse() {
  //if (millis() - lastAutopilotStatusTime > 480 && millis() - lastAutopilotStatusResponseTime > 480) {
  unsigned char current = autopilotState[5] & 0x3C;

  autopilotState[5] = autopilotState[5] & 0xC3;
  autopilotState[7] = autopilotState[7] - current;
  autopilotState[6] = autopilotState[6] + 0x10;
  autopilotState[7] = autopilotState[7] + 0x10;

  CCAN.sendMsgBuf(0x339, 8, autopilotState);
  printMessage(8, autopilotState, true);
  //lastAutopilotStatusResponseTime = millis();
  // }
}*/
