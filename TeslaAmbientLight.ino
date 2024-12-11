#include <EEPROM.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "Car.h"
#include "CarLight.h"
#include "DoorLight.h"
#include "FootLight.h"
#include "MirrorLight.h"

const int ledPin = 12;

#ifdef CONFIG_IDF_TARGET_ESP32S3
const int pocketLedPin = 10;
const int mirrorPin = 11;
#else
const int pocketLedPin = 14;
const int mirrorPin = 15;
#endif

const int leftFootwellPin = 26;
const int rightFootwellPin = 27;

const int leftFootwellHLPin = 32;
const int rightFootwellHLPin = 33;

const int vCanPin = 5;
const int cCanPin = 13;
const char* ssid = "T3LIGHT";
const char* password = "123456";
const char* passwordOTA = "123456";
const bool externalWifi = false;

unsigned char role = DOOR_MASTER;  // DOOR_FRONT_RIGHT, DOOR_FRON_LEFT, DOOR_REAR_RIGHT, DOOR_REAR_LEFT
const bool saveRoleToEEPROM = false;
const bool readRoleFromEEPROM = true;

Car car;
CarLight carLight;

FootLight leftFootLight;
FootLight rightFootLight;

DoorLight doorLight;
MirrorLight mirrorLight;

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
    leftFootLight.init(1, leftFootwellPin, leftFootwellHLPin, 0);
    rightFootLight.init(2, rightFootwellPin, rightFootwellHLPin, 1);
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
      IPAddress local_IP(192, 168, 4, 10);
      IPAddress subnet(255, 255, 255, 0);
      IPAddress gateway(192, 168, 4, 1);
      if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Configuration Failed!");
      }
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid, password, 1, 0, 10, false);
      Serial.println("WIFI started");
    }
  } else {
    IPAddress local_IP(192, 168, 4, 10 + role);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress gateway(192, 168, 4, 1);
    if (!WiFi.config(local_IP, gateway, subnet)) {
      Serial.println("Configuration Failed!");
    }
    doorLight.init(role - 1, ledPin, pocketLedPin);
    mirrorLight.init(role - 1, mirrorPin);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password, 1);
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
      else
        type = "filesystem";
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
  ArduinoOTA.setPassword(passwordOTA);
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  if (role == 0) {
    car.process();
    carLight.processCarState(car);
    carLight.sendLightState();
    leftFootLight.setColorByCarState(carLight);
    rightFootLight.setColorByCarState(carLight);
  } else {
    carLight.receiveLightState();
    doorLight.setColorByCarState(carLight);
    mirrorLight.setColorByCarState(carLight);
  }
  delay(10);
}
