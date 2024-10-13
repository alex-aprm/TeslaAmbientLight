#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>

//#include "Utils.cpp"
#include "Car.h"
#define LED_PIN 12
#define NUMPIXELS 134

const char* ssid = "ESP32";
const char* password = "53EYMJTV";
const char* udpAddress = "255.255.255.255";
const int udpPort = 3333;
WiFiUDP udp;

enum DoorState : byte { WAIT,
                        DOOR_OPEN,
                        DOOR_WAIT,
                        IDLE_INIT,
                        IDLE,
                        TURNING,
                        TURNING_LIGHT,
                        BLIND_SPOT,
                        TURNING_BLIND_SPOT,
                        TURNING_LIGHT_BLIND_SPOT };
DoorState doorLightState[4] = { WAIT, WAIT, WAIT, WAIT };
unsigned long doorLightMs[4] = { 0, 0, 0, 0 };
unsigned long doorLightAge[4] = { 0, 0, 0, 0 };

Adafruit_NeoPixel strip(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
double currentColor[3][NUMPIXELS];
double oldColor[3][NUMPIXELS];
double targetColor[3][NUMPIXELS];

unsigned char autopilotState[8];
unsigned char driveAssistState[8];
unsigned char gtw[8] = { 0x02, 0x7F, 0x56, 0x2C, 0xDE, 0x4D, 0xD6, 0xA2 };
unsigned char lever[3] = { 0x00, 0x10, 0x00 };

Car car;

unsigned char role = 10;

void setup() {
  EEPROM.begin(8);
  //EEPROM.write(0, byte(2));
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
    //blinkSuccess(strip);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
      //blinkError(strip);
      delay(1000);
      ESP.restart();
    }
    //blinkSuccess(strip);
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

  //strip.clear();
  bool changed = false;
  for (int i = 0; i < NUMPIXELS; i++) {
    if (oldColor[0][i] != currentColor[0][i] || oldColor[1][i] != currentColor[1][i] || oldColor[2][i] != currentColor[2][i]) {
      changed = true;
      oldColor[0][i] == currentColor[0][i];
      oldColor[1][i] == currentColor[1][i];
      oldColor[2][i] == currentColor[2][i];
    }
  }
  if (changed) {
    strip.clear();
    for (int i = 0; i < NUMPIXELS; i++) {
      strip.setPixelColor(i, strip.Color(round(currentColor[0][i]), round(currentColor[1][i]), round(currentColor[2][i])));
    }
    strip.show();
  }
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

  for (byte i = 0; i < 2; i++) {
    bool blindSpot = car.blindSpotRight;
    bool turning = car.turningRight;
    bool turningLight = car.turningRightLight;
    if (i == 1) {
      blindSpot = car.blindSpotLeft;
      turning = car.turningLeft;
      turningLight = car.turningLeftLight;
    }
    if (blindSpot) {
      if (doorLightState[i] == IDLE)
        changeDoorLightState(i, BLIND_SPOT);
      if (doorLightState[i] == TURNING)
        changeDoorLightState(i, TURNING_BLIND_SPOT);
      if (doorLightState[i] == TURNING_LIGHT)
        changeDoorLightState(i, TURNING_LIGHT_BLIND_SPOT);
    }
    if (!blindSpot) {
      if (doorLightState[i] == BLIND_SPOT)
        changeDoorLightState(i, IDLE);
      if (doorLightState[i] == TURNING_BLIND_SPOT)
        changeDoorLightState(i, TURNING);
      if (doorLightState[i] == TURNING_LIGHT_BLIND_SPOT)
        changeDoorLightState(i, TURNING_LIGHT);
    }
    if (turning && (doorLightState[i] == IDLE || doorLightState[i] == TURNING_LIGHT))
      changeDoorLightState(i, TURNING);
    if (turningLight && (doorLightState[i] == IDLE || doorLightState[i] == TURNING))
      changeDoorLightState(i, TURNING_LIGHT);
    if (turning && (doorLightState[i] == BLIND_SPOT || doorLightState[i] == TURNING_LIGHT_BLIND_SPOT))
      changeDoorLightState(i, TURNING_BLIND_SPOT);
    if (turningLight && (doorLightState[i] == IDLE || doorLightState[i] == TURNING_BLIND_SPOT))
      changeDoorLightState(i, TURNING_LIGHT_BLIND_SPOT);
    if (!turningLight && !turning && (doorLightState[i] == TURNING_LIGHT || doorLightState[i] == TURNING))
      changeDoorLightState(i, IDLE);
    if (!turningLight && !turning && (doorLightState[i] == TURNING_LIGHT_BLIND_SPOT || doorLightState[i] == TURNING_BLIND_SPOT))
      changeDoorLightState(i, BLIND_SPOT);
  }

  bool allWait = true;
  for (byte i = 0; i < 4; i++)
    allWait = (doorLightState[i] == WAIT) && allWait;
  bool allIdle = true;
  for (byte i = 0; i < 4; i++)
    allIdle = (doorLightState[i] == IDLE) && allIdle;

  if (allWait || allIdle)
    udpInterval = 1000;
  else
    udpInterval = 50;

  byte oldBrightness = brightness;
  brightness = car.brightness;

  stateChanged = stateChanged || (oldBrightness != brightness);

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
    stateChanged = false;
  }

  byte doorNum = 0;
  state = doorLightState[doorNum];
  stateAge = doorLightAge[doorNum];
}

long unsigned int lastUdpMs;
void getStateFromMaster() {
  //if (millis() - lastUdpMs > 10000)
  //  ESP.restart();

  byte doorNum = role - 1;
  char packetBuffer[255];
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      lastUdpMs = millis();
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
      setTargetColor(i, 255, 0, 0);
    }
  } else if (state == WAIT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      setTargetColor(i, 0, 0, 0);
    }
  } else if (state == DOOR_WAIT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      setCurrentColor(i, 20, 0, 0);
      setTargetColor(i, 20, 0, 0);
    }
  } else if (state == IDLE_INIT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      setCurrentColor(i, 0, 0, 0);
      setTargetColor(i, 0, 0, 40);
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
        setTargetColor(i, 255, 255, 255);
        setCurrentColor(i, 255, 255, 255);
      }
    }

  } else if (state == IDLE || state == TURNING || state == TURNING_LIGHT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      setTargetColor(i, 255, 0, 255);
    }
  }

  if (state == TURNING) {
    for (int i = 0; i < 20; i++) {
      setTargetColor(i, 255, 128, 0);
      setCurrentColor(i, 255, 128, 0);
    }
  }
  if (state == TURNING_LIGHT) {
    for (int i = 20; i < 40; i++) {
      setTargetColor(i, 255, 128, 0);
      setCurrentColor(i, 255, 128, 0);
    }
  }
}

void setTargetColor(byte i, double r, double g, double b) {
  targetColor[0][i] = r;
  targetColor[1][i] = g;
  targetColor[2][i] = b;
}

void setCurrentColor(byte i, double r, double g, double b) {
  currentColor[0][i] = r;
  currentColor[1][i] = g;
  currentColor[2][i] = b;
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
