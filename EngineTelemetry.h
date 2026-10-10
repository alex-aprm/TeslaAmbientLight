#ifndef ENGINE_TELEMETRY_H
#define ENGINE_TELEMETRY_H
#include <Arduino.h>
#include <WiFiUdp.h>
#include "Car.h"

// Powertrain telemetry broadcast for the EngineSound node (see EngineSound/).
// The master already decodes V-CAN; this module just packs the powertrain
// signals and broadcasts them over the master's Wi-Fi AP. The door nodes
// listen on 3333 and never see these packets.
//
// Keep this struct byte-identical to EngineSound/TelemetryPacket.h.

#define ENGINE_TELEMETRY_PORT 3334
#define ENGINE_TELEMETRY_VERSION 1

enum EngineTelemetryFlags : uint8_t {
  ET_DISPLAY_ON = 0x01,   // car awake (center display on)
  ET_BRAKE = 0x02,        // brake pedal pressed
  ET_VALID = 0x04,        // powertrain frames seen in the last 500 ms
  ET_HAS_TORQUE = 0x08    // torque signal present (0x108 seen)
};

struct __attribute__((packed)) EngineTelemetryPacket {
  uint8_t magic[2];        // 'E','T'
  uint8_t version;         // ENGINE_TELEMETRY_VERSION
  uint8_t flags;           // EngineTelemetryFlags
  uint16_t seq;            // increments every packet
  uint8_t gear;            // Gear enum from Car.h (P/R/N/D)
  uint8_t pedal;           // accelerator, 0..250 = 0..100 % (0.4 % steps)
  int16_t speed;           // vehicle speed, 0.01 km/h
  int16_t rearTorque;      // rear drive unit torque, Nm
  int16_t frontTorque;     // front drive unit torque, Nm (0 on RWD)
  int16_t axleRpm;         // rear axle speed, 0.1 RPM
  int16_t power;           // rear inverter power, 0.1 kW (0 if not decoded)
  uint32_t masterMs;       // master millis() at send time
};

class EngineTelemetry {
public:
  void init();
  // Call every loop on the master. Sends at 50 Hz while the car is awake and
  // once per second while it sleeps.
  void send(Car& car);
private:
  WiFiUDP _udp;
  unsigned long _lastSend = 0;
  uint16_t _seq = 0;
  bool _seenTorque = false;
};
#endif
