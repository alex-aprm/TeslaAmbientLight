#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//#include "Utils.cpp"
#include "Car.h"
#include "Light.h"

#define LED_PIN 12
const char* ssid = "ESP32";
const char* password = "53EYMJTV";
#define NUMPIXELS_FRONT 134
#define NUMPIXELS_REAR 90
#define NUMPIXELS_PILLAR 5
#define NUMPIXELS NUMPIXELS_FRONT + NUMPIXELS_PILLAR + NUMPIXELS_REAR

const char* udpAddress = "255.255.255.255";
const int udpPort = 3333;
WiFiUDP udp;

Adafruit_NeoPixel strip(max(NUMPIXELS_FRONT, NUMPIXELS_REAR), LED_PIN, NEO_GRB + NEO_KHZ800);
double currentColor[3][NUMPIXELS];
uint32_t oldColor[NUMPIXELS];
double targetColor[3][NUMPIXELS];

Car car;
Light light;

bool doorHandlePull = false;
unsigned long doorHandlePullMs;
byte doorHandlePullCount = 0;
unsigned long frunkOpenMs;
unsigned long unlockMs;

unsigned char role = 10;

void setup() {
  EEPROM.begin(8);
  //EEPROM.write(0, byte(0));
  //EEPROM.commit();
  role = byte(EEPROM.read(0));

  Serial.begin(921600);
  Serial.println();
  Serial.print("Role: ");
  Serial.println(role);

  if (role == 0) {
    car.init(13, 5);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    Serial.println("WIFI started");
    udp.begin(udpPort);
    delay(50);
    //blinkSuccess(strip);
  } else {
    strip.begin();
    strip.setBrightness(50);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
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
    setTargetColor(i, 0, 0, 0);
    setCurrentColor(i, 0, 0, 0);
  }
}

unsigned long lastUdp = 0;
unsigned int udpInterval = 100;
bool stateChanged = false;
byte brightness;
byte oldBrightness;
byte state;
byte lastState;
unsigned long stateMs;
unsigned int stateAge;
int stateCnt;

void loop() {
  ArduinoOTA.handle();
  if (role == 0) {
    car.process();
    light.processCarState(car);
    unsigned long doorLightMinAge = light.doorLightAge[0];
    for (byte i = 1; i < 4; i++)
      if (light.doorLightAge[i] < doorLightMinAge)
        doorLightMinAge = light.doorLightAge[i];
    if ((light.allWait || light.allIdle) && doorLightMinAge > 15000) {
      udpInterval = 10000;
    } else
      udpInterval = 30;
    if (light.stateChanged || ((millis() - lastUdp) > udpInterval)) {
      udp.beginPacket(udpAddress, udpPort);
      udp.write(light.brightness);
      for (byte i = 0; i < 4; i++) {
        udp.write(light.doorLightState[i]);
        udp.write((byte)light.doorLightAge[i]);
        udp.write((byte)(light.doorLightAge[i] >> 8));
        udp.write((byte)(light.doorLightAge[i] >> 16));
        udp.write((byte)(light.doorLightAge[i] >> 24));
      }
      udp.endPacket();
      udp.flush();

      lastUdp = millis();
      light.stateChanged = false;
    }

    delay(10);
    if (millis() - unlockMs > 30000) {
      // car.openFrunk();
      //unlockMs = millis();
    } else {
      //Serial.print("Unlock in ");
      //Serial.println(30000 - (millis() - unlockMs));
    }

    if (car.doorHandlePull[0] || car.doorHandlePull[1]) {
      if (!doorHandlePull) {
        doorHandlePull = true;
        if (millis() - doorHandlePullMs > 1500)
          doorHandlePullCount = 0;
        doorHandlePullMs = millis();
        Serial.println("Handle pulled");
      }
    } else {
      if (doorHandlePull) {
        if (millis() - doorHandlePullMs > 500)
          doorHandlePullCount++;
        else
          doorHandlePullCount = 0;
        doorHandlePull = false;
        Serial.println("Handle left");
      }
    }

    if (doorHandlePull && doorHandlePullCount == 2 && frunkOpenMs < doorHandlePullMs) {
      frunkOpenMs = millis();
      doorHandlePullCount = 0;
      Serial.println("Opening");
      //car.unlockRemote();
      car.openFrunk();
    }

  } else {
    getStateFromMaster();
    strip.setBrightness(brightness);
    setColorByState();
    fadeColor();
    //strip.clear();
    bool changed = false;
    byte firstPixel = 0;
    byte lastPixel = NUMPIXELS_FRONT - 1;
    if (role > 2) {
      firstPixel = NUMPIXELS_FRONT + NUMPIXELS_PILLAR;
      lastPixel = NUMPIXELS - 1;
    }
    for (int i = firstPixel; i <= lastPixel; i++) {
      uint32_t color = strip.Color(round(currentColor[0][i]), round(currentColor[1][i]), round(currentColor[2][i]));
      if (oldColor[i] != color) {
        changed = true;
        oldColor[i] = color;
      }
    }
    if (changed) {
      strip.clear();
      for (int i = firstPixel; i <= lastPixel; i++) {
        strip.setPixelColor(i - firstPixel, oldColor[i]);
      }
    }

    if (changed || oldBrightness != brightness) {
      strip.show();
    }
    oldBrightness = brightness;
  }
}

unsigned long lastUdpMs;
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
      unsigned long age = 0;
      for (int i = 0; i < 4; i++)
        age += packetBuffer[2 + i + doorNum * doorLen] * pow(255, i);

      unsigned long stateNewMs = millis() - age;
      if (state == lastState) {
        int d = stateNewMs > stateMs ? stateNewMs - stateMs : -(stateMs - stateNewMs);

        stateMs = stateMs + round(d / ++stateCnt);
      } else {
        stateCnt = 1;
        stateMs = stateNewMs;
        lastState = state;
      }
    }
  }
}


void setColorByState() {
  stateAge = millis() - stateMs;
  double max = 255;
  if (brightness == 0)
    max = 0;
  else
    max = map(brightness, 0x05, 0x7F, 0x60, 0xFF);
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
  } else if (state == IDLE_INIT || state == IDLE || state == TURNING || state == TURNING_BLIND_SPOT || state == BLIND_SPOT) {
    for (int i = 0; i < NUMPIXELS; i++) {
      double r = max - map(i, 0, NUMPIXELS, 0, max);
      double gMax = max *0.9;
      if (gMax < 60)
      gMax = 60; 
      double g = map(i, 0, NUMPIXELS, 0, gMax);
      setTargetColor(i, r, g, max);
    }
  }

  if (state == IDLE_INIT) {
        for (int i = 0; i < NUMPIXELS; i++) {
      setTargetColor(i, 255 - map(i, 0, NUMPIXELS, 0, 255), map(i, 0, NUMPIXELS, 0, 255), 255);
    }

    if (stateAge < 1000) {
      int pos1 = map(stateAge, 0, 1000, 0, NUMPIXELS);

      for (int i = pos1; i < NUMPIXELS; i++) {
        setCurrentColor(i, 0, 0, 0);
        setTargetColor(i, 0, 0, 0);
      }

      for (int i = 0; i < pos1; i++) {
        // setCurrentColor(i, 0, 0, 0);
      }

      setCurrentColor(pos1, 255, 255, 255);
      setTargetColor(pos1, 255, 255, 255);
    }

    /*
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
    }*/
  }

  int border = 38;
  if (state == BLIND_SPOT) {
    for (int i = border; i < 56; i++) {
      setTargetColor(i, max, 0, 0);
    }
  }

  if (state == TURNING_BLIND_SPOT) {
    for (int i = border; i < NUMPIXELS_FRONT; i++) {
      setTargetColor(i, max, 0, 0);
    }
  }

  if (state == TURNING || state == TURNING_BLIND_SPOT) {
    for (int i = 0; i < border; i++) {
      setTargetColor(i, 0, 0, 0);
      setCurrentColor(i, 0, 0, 0);
    }
    if (lround(floor((stateAge + 100) / 360)) % 2 == 0) {
      double g = max / 5;
      double r = max;
      if (r < 200)
        r = 200;
      if (g < 60)
        g = 60;
      for (int i = 0; i < 37; i++) {
        setTargetColor(i, r, g, 0);
        setCurrentColor(i, r, g, 0);
      }
    }
  }
}

void setTargetColor(int i, double r, double g, double b) {
  targetColor[0][i] = r;
  targetColor[1][i] = g;
  targetColor[2][i] = b;
}

void setCurrentColor(int i, double r, double g, double b) {
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
    for (int i = 0; i < NUMPIXELS; i++) {
      if (currentColor[c][i] == targetColor[c][i])
        continue;
      if (left == 0) {
        currentColor[c][i] = targetColor[c][i];
        continue;
      }
      double delta = targetColor[c][i] - currentColor[c][i];

      double needToChange = delta * step / left;
      currentColor[c][i] += needToChange;
    }
  }
  colorTransitionLastMs = millis();
}
