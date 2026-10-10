#ifndef TELEMETRY_PACKET_H
#define TELEMETRY_PACKET_H
#include <stdint.h>

// Byte-identical copy of EngineTelemetryPacket from ../EngineTelemetry.h
// (the master side). Keep both in sync.

#define ENGINE_TELEMETRY_PORT 3334
#define ENGINE_TELEMETRY_VERSION 1

enum EngineTelemetryFlags : uint8_t {
  ET_DISPLAY_ON = 0x01,
  ET_BRAKE = 0x02,
  ET_VALID = 0x04,
  ET_HAS_TORQUE = 0x08
};

// Gear values as sent by the master (Gear enum in ../Car.h)
enum TelemetryGear : uint8_t {
  TG_UNKNOWN = 0,
  TG_PARK = 1,
  TG_REVERSE = 2,
  TG_NEUTRAL = 3,
  TG_DRIVE = 4
};

struct __attribute__((packed)) EngineTelemetryPacket {
  uint8_t magic[2];        // 'E','T'
  uint8_t version;
  uint8_t flags;
  uint16_t seq;
  uint8_t gear;
  uint8_t pedal;           // 0..250 = 0..100 %
  int16_t speed;           // 0.01 km/h
  int16_t rearTorque;      // Nm
  int16_t frontTorque;     // Nm
  int16_t axleRpm;         // 0.1 RPM
  int16_t power;           // 0.1 kW
  uint32_t masterMs;
};
#endif
