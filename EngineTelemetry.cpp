#include "EngineTelemetry.h"

static int16_t _clamp16(float v) {
  if (v > 32767.0f) return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)lroundf(v);
}

void EngineTelemetry::init() {
  _udp.begin(ENGINE_TELEMETRY_PORT);
}

void EngineTelemetry::send(Car& car) {
  unsigned long now = millis();
  unsigned long interval = car.displayOn ? 20 : 1000;
  if (now - _lastSend < interval)
    return;
  _lastSend = now;

  if (car.rearTorqueNm != 0 || car.rearAxleRpm != 0)
    _seenTorque = true;

  EngineTelemetryPacket p;
  p.magic[0] = 'E';
  p.magic[1] = 'T';
  p.version = ENGINE_TELEMETRY_VERSION;
  p.flags = 0;
  if (car.displayOn) p.flags |= ET_DISPLAY_ON;
  if (car.brakePressed) p.flags |= ET_BRAKE;
  if (car.powertrainMs != 0 && now - car.powertrainMs < 500) p.flags |= ET_VALID;
  if (_seenTorque) p.flags |= ET_HAS_TORQUE;
  p.seq = _seq++;
  p.gear = (uint8_t)car.gear;
  float pedal = constrain(car.accelPedal, 0.0f, 100.0f);
  p.pedal = (uint8_t)lroundf(pedal / 0.4f);
  p.speed = _clamp16(car.speedKph * 100.0f);
  p.rearTorque = _clamp16(car.rearTorqueNm);
  p.frontTorque = _clamp16(car.frontTorqueNm);
  p.axleRpm = _clamp16(car.rearAxleRpm * 10.0f);
  p.power = _clamp16(car.rearPowerKw * 10.0f);
  p.masterMs = now;

  _udp.beginPacket(IPAddress(255, 255, 255, 255), ENGINE_TELEMETRY_PORT);
  _udp.write((const uint8_t*)&p, sizeof(p));
  _udp.endPacket();
}
