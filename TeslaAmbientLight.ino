#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>

#include "Utils.cpp"
#include "Car.h"
#define LED_PIN 12
#define NUMPIXELS 133

const char* ssid = "ESP32";
const char* password = "53EYMJTV";
const char* udpAddress = "255.255.255.255";
const int udpPort = 3333;
WiFiUDP udp;

enum DoorState : byte { WAIT,
                        DOOR_OPEN,
                        DOOR_CLOSING,
                        DOOR_WAIT,
                        IDLE_INIT,
                        IDLE,
                        TURNING,
                        BLIND_SPOT };
DoorState doorLightState[4] = { WAIT, WAIT, WAIT, WAIT };
unsigned long doorLightMs[4] = { 0, 0, 0, 0 };
unsigned long doorLightAge[4] = { 0, 0, 0, 0 };

Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
double currentColor[3][NUMPIXELS];
double targetColor[3][NUMPIXELS];

unsigned char autopilotState[8];
unsigned char driveAssistState[8];
unsigned char gtw[8] = { 0x02, 0x7F, 0x56, 0x2C, 0xDE, 0x4D, 0xD6, 0xA2 };
unsigned char lever[3] = { 0x00, 0x10, 0x00 };

Car car;

unsigned char role = 10;

void setup() {
  EEPROM.begin(8);
  //EEPROM.write(0, byte(1));
  //EEPROM.commit();
  role = byte(EEPROM.read(0));

  Serial.begin(921600);
  Serial.println();
  Serial.print("Role: ");
  Serial.println(role);

  strip.begin();
  strip.setBrightness(50);

  if (role == 0) {
    car.init(13, 5);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    blinkSuccess(strip);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
      blinkError(strip);
      delay(1000);
      ESP.restart();
    }
    blinkSuccess(strip);
    udp.begin(udpPort);
  }

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  for (int i = 0; i < NUMPIXELS; i++) {
    targetColor[0][i] = 0;
    targetColor[1][i] = 0;
    targetColor[2][i] = 0;
    currentColor[0][i] = 0;
    currentColor[1][i] = 0;
    currentColor[2][i] = 0;
  }
}

unsigned long lastUdp = 0;
unsigned int udpInterval = 100;
bool stateChanged = false;
byte brightness;
byte state;
unsigned long stateAge;

void loop() {
  ArduinoOTA.handle();
  if (role == 0) {
    getStateFromCar();
  } else {
    getStateFromMaster();
  }

  strip.setBrightness(brightness);
  setColorByState();
  fadeColor();

  strip.clear();
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, strip.Color(round(currentColor[0][i]), round(currentColor[1][i]), round(currentColor[2][i])));
  }
  strip.show();
  // Serial.println();
}

void getStateFromCar() {
  car.process();
  long now = millis();
  /*
Serial.print("Display ");
Serial.println(car.displayOn);
  */

  for (byte i = 0; i < 4; i++) {
    if (doorLightState[i] == IDLE_INIT && now - doorLightMs[i] > 500)
      changeDoorLightState(i, IDLE);
  }

  bool someDoorOpen = false;
  for (byte i = 0; i < 4; i++) {
    someDoorOpen = car.doorOpen[i] || someDoorOpen;
    if (car.doorOpen[i] && doorLightState[i] != DOOR_OPEN)
      changeDoorLightState(i, DOOR_OPEN);
    if (!car.doorOpen[i] && doorLightState[i] == DOOR_OPEN)
      changeDoorLightState(i, IDLE_INIT);
  }

  if (someDoorOpen) {
    for (byte i = 0; i < 4; i++)
      if (!car.doorOpen[i] && doorLightState[i] != DOOR_WAIT)
        changeDoorLightState(i, DOOR_WAIT);
  } else {
    for (byte i = 0; i < 4; i++)
      if (doorLightState[i] == DOOR_WAIT)
        changeDoorLightState(i, IDLE_INIT);
  }

  for (byte i = 0; i < 4; i++)
    doorLightAge[i] = millis() - doorLightMs[i];

  if (!car.displayOn) {
    for (byte i = 0; i < 4; i++)
      if (doorLightState[i] == IDLE || doorLightState[i] == IDLE_INIT)
        changeDoorLightState(i, WAIT);
  } else {
    for (byte i = 0; i < 4; i++)
      if (doorLightState[i] == WAIT)
        changeDoorLightState(i, IDLE_INIT);
  }

  bool allWait = false;
  for (byte i = 0; i < 4; i++)
    allWait = (doorLightState[i] == WAIT) && allWait;
  bool allIdle = false;
  for (byte i = 0; i < 4; i++)
    allWait = (doorLightState[i] == IDLE) && allWait;

  if (allWait || allIdle)
    udpInterval = 1000;

  brightness = car.brightness;

  if (someDoorOpen && brightness < 100)
    brightness += 100;

  if (stateChanged || (millis() - lastUdp > udpInterval)) {
    udp.beginPacket(udpAddress, udpPort);
    udp.write(brightness);
    for (byte i = 0; i < 4; i++) {
      udp.write(doorLightState[i]);
      udp.write((byte)doorLightAge[i]);
      udp.write((byte)(doorLightAge[i] >> 8));
      udp.write((byte)(doorLightAge[i] >> 16));
      udp.write((byte)(doorLightAge[i] >> 24));
    }
    udp.endPacket();
    lastUdp = millis();
  }

  byte doorNum = 0;
  state = doorLightState[doorNum];
  stateAge = doorLightAge[doorNum];
}

void getStateFromMaster() {
  byte doorNum = role - 1;
  char packetBuffer[255];
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      brightness = packetBuffer[0];
      byte doorLen = 5;
      state = packetBuffer[1 + doorNum * doorLen];
      stateAge = 0;
      for (byte i = 0; i < 4; i++)
        stateAge += packetBuffer[2 + i + doorNum * doorLen] * pow(255, i);
    }
  }
}

void setColorByState() {
  if (state == DOOR_OPEN) {
    for (int i = 0; i < NUMPIXELS; i++) {
      targetColor[0][i] = 255;
      targetColor[1][i] = 0;
      targetColor[2][i] = 0;
    }
  } else if (state == WAIT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      targetColor[0][i] = 0;
      targetColor[1][i] = 0;
      targetColor[2][i] = 0;
    }
  } else if (state == DOOR_WAIT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      currentColor[0][i] = 20;
      currentColor[1][i] = 0;
      currentColor[2][i] = 0;
      targetColor[0][i] = 20;
      targetColor[1][i] = 0;
      targetColor[2][i] = 0;
    }
  } else if (state == IDLE_INIT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      currentColor[0][i] = 0;
      currentColor[1][i] = 0;
      currentColor[2][i] = 0;
      targetColor[0][i] = 0;
      targetColor[1][i] = 0;
      targetColor[2][i] = 40;
    }

    unsigned int pos1 = round((stateAge) / 4.0);
    unsigned int pos2 = round((stateAge - 200) / 4.0);
    unsigned int fade = round((stateAge) / 6.0);
    if (fade > 255)
      fade = 255;
    for (int i = 0; i < NUMPIXELS; i++) {
      if (pos2 < i && i < pos1) {
        targetColor[0][i] = fade;
        targetColor[2][i] = fade;
        currentColor[0][i] = fade;
        currentColor[2][i] = fade;
      }
      if (i == pos2) {
        targetColor[0][i] = 255;
        targetColor[2][i] = 255;
        targetColor[2][i] = 255;
        currentColor[0][i] = 255;
        currentColor[1][i] = 255;
        currentColor[2][i] = 255;
      }
    }

  } else if (state == IDLE) {
    for (int i = 0; i < NUMPIXELS; i++) {
      targetColor[0][i] = 255;
      targetColor[1][i] = 0;
      targetColor[2][i] = 255;
    }
  }
}


unsigned long colorTransitionLastMs;
void fadeColor() {
  unsigned long start = millis() - stateAge;
  unsigned int transition = 300;
  unsigned long end = start + transition;
  int left = end - colorTransitionLastMs;
  if (left < 0)
    left = 0;
  unsigned int step = millis() - colorTransitionLastMs;
  for (byte c = 0; c < 3; c++) {
    for (byte i = 0; i < NUMPIXELS; i++) {
      if (currentColor[c][i] == targetColor[c][i])
        continue;
      if (left == 0) {
        currentColor[c][i] = targetColor[c][i];
        continue;
      }
      double delta = targetColor[c][i] - currentColor[c][i];

      double needToChange = delta * step / left;
      currentColor[c][i] += needToChange;
      /* if (c == 2 && i == 0) {
        Serial.print("delta = ");
        Serial.print(delta);
        Serial.print(" current = ");
        Serial.print(currentColor[c][i]);
        Serial.println();
      }*/
    }
  }
  colorTransitionLastMs = millis();
}

void changeDoorLightState(byte door, DoorState state) {
  doorLightState[door] = state;
  doorLightMs[door] = millis();
  stateChanged = true;
  Serial.print("Door ");
  Serial.print(door);
  Serial.print(" changed to ");
  Serial.println(state);
}
