#include <EEPROM.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "Car.h"
#include "CarLight.h"
#include "DoorLight.h"
#include "FootLight.h"

const char* ssid = "ESP32";
const char* password = "53EYMJTV";
const int ledPin = 12;

#ifdef CONFIG_IDF_TARGET_ESP32S3
const int pocketLedPin = 10;
const int mirrorPin = 11;
#else
const int pocketLedPin = 14;
const int mirrorPin = 15;
#endif

const int leftFootwellPin = 16;
const int rightFootwellPin = 17;

const int vCanPin = 5;
const int cCanPin = 13;
const bool externalWifi = false;

unsigned char role = DOOR_MASTER;  // DOOR_FRONT_RIGHT, DOOR_FRON_LEFT, DOOR_REAR_RIGHT, DOOR_REAR_LEFT
const bool saveRoleToEEPROM = false;
const bool readRoleFromEEPROM = true;

Car car;
CarLight carLight;
DoorLight doorLight;
FootLight leftFootLight;
FootLight rightFootLight;

void setup() {
  car.openFrunkWithDoor = true;

  EEPROM.begin(8);
  if (saveRoleToEEPROM) {
    EEPROM.write(0, role);
    EEPROM.commit();
  }
  if (readRoleFromEEPROM)
    role = byte(EEPROM.read(0));

  Serial.begin(921600);
  Serial.println();
  Serial.print("Role: ");
  Serial.println(role);

  if (role == 0) {
    car.init(vCanPin, cCanPin);
    leftFootLight.init(1, leftFootwellPin);

    if (externalWifi) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(1000);
        ESP.restart();
      }
      Serial.println("Connected");
    } else {
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid, password, 1, 0, 10, false);
      Serial.println("WIFI started");
    }
  } else {

    doorLight.init(role - 1, ledPin, pocketLedPin, mirrorPin);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
      delay(1000);
      ESP.restart();
    }
    Serial.println("Connected");
  }

  carLight.init();
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
}

void loop() {
  ArduinoOTA.handle();
  //delay(100);
  //return;
  if (role == 0) {
    car.process();
    carLight.processCarState(car);
    carLight.sendLightState();
    leftFootLight.setColorByCarState(carLight);
  } else {
    carLight.receiveLightState();
    doorLight.setColorByCarState(carLight);
  }
  delay(10);
}
