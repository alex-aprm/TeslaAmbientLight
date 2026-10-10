#ifndef ENGINE_MODEL_H
#define ENGINE_MODEL_H
// Virtual engine + automatic gearbox. Plain C++ (no Arduino), so the same code
// runs on the ESP32 and in the desktop renderer (tools/render_wav.cpp).
//
// Input: what the Tesla reports (speed, pedal, motor torque, P/R/N/D).
// Output: engine RPM, load and events for the synthesizer.

#include <math.h>
#include <stdint.h>

namespace engine {

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
// First-order smoothing toward target with time constant tau (seconds)
static inline float follow(float cur, float target, float dt, float tau) {
  if (tau <= 0) return target;
  return cur + (target - cur) * (1.0f - expf(-dt / tau));
}

struct Resonator {
  float freq;   // Hz
  float q;
  float gain;
};

struct Profile {
  const char* id;
  const char* name;
  // --- engine ---
  uint8_t cylinders;
  float slot[12];       // firing (exhaust arrival) position in the 720° cycle, 0..1
  uint8_t bank[12];     // which exhaust bank/side the pulse goes to (0 = left, 1 = right)
  float idleRpm;
  float redlineRpm;     // auto upshift ceiling
  float limiterRpm;     // ignition cut above this
  float revUpRate;      // free-revving rpm/s at full throttle
  float revDownRate;    // free-revving decay rpm/s
  // --- gearbox ---
  uint8_t gears;
  float ratio[8];
  float finalDrive;
  // --- timbre ---
  Resonator res[2][3];  // per bank: three exhaust resonances
  float lpMin, lpMax;   // brightness lowpass range, Hz
  float pulseWidth;     // exhaust pulse length, crank revolutions
  float noise;          // combustion turbulence in each pulse
  float variation;      // cycle-to-cycle pulse amplitude variation (idle lope)
  float crackle;        // overrun pop probability per firing
  float pipeMs[2];      // exhaust pipe reflection delay per bank
  float pipeFeedback;
  float intake;         // induction noise level
  float gain;           // output trim
};

// Four built-in engines. Slots/banks follow real firing orders; the boxer uses
// unequal-length header timing for its rumble.
static const Profile PROFILES[] = {
  { "v8", "Muscle V8 (crossplane)",
    8,
    { 0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f },
    // firing order 1-8-4-3-6-5-7-2, left bank = 1,3,5,7
    { 0, 1, 1, 0, 1, 0, 0, 1 },
    750, 6400, 6700, 9000, 3800,
    6, { 2.97f, 2.07f, 1.43f, 1.00f, 0.84f, 0.56f }, 3.73f,
    { { { 92, 1.3f, 1.0f }, { 235, 2.2f, 0.55f }, { 610, 2.5f, 0.22f } },
      { { 98, 1.3f, 1.0f }, { 250, 2.2f, 0.55f }, { 650, 2.5f, 0.22f } } },
    550, 3600, 0.30f, 0.30f, 0.11f, 0.10f, { 4.1f, 4.7f }, 0.32f, 0.10f, 1.0f },

  { "i6", "Straight-six",
    6,
    { 0.0f, 0.1667f, 0.3333f, 0.5f, 0.6667f, 0.8333f },
    // firing order 1-5-3-6-2-4, twin pipes: cylinders 1-3 left, 4-6 right
    { 0, 1, 0, 1, 0, 1 },
    800, 7200, 7400, 11000, 4500,
    6, { 4.11f, 2.32f, 1.54f, 1.18f, 1.00f, 0.85f }, 3.15f,
    { { { 135, 1.4f, 1.0f }, { 380, 2.4f, 0.6f }, { 1100, 2.8f, 0.3f } },
      { { 140, 1.4f, 1.0f }, { 395, 2.4f, 0.6f }, { 1150, 2.8f, 0.3f } } },
    900, 6000, 0.25f, 0.16f, 0.04f, 0.06f, { 3.2f, 3.5f }, 0.28f, 0.14f, 0.9f },

  { "boxer", "Flat-four rumble",
    4,
    // even firing, unequal-length headers shift the pulse arrival times
    { 0.0f, 0.27f, 0.5f, 0.80f },
    { 0, 1, 0, 1 },
    850, 6800, 7000, 9500, 4200,
    6, { 3.636f, 2.235f, 1.521f, 1.137f, 0.971f, 0.756f }, 3.90f,
    { { { 78, 1.2f, 1.0f }, { 205, 2.0f, 0.6f }, { 520, 2.2f, 0.25f } },
      { { 82, 1.2f, 1.0f }, { 215, 2.0f, 0.6f }, { 540, 2.2f, 0.25f } } },
    500, 3200, 0.32f, 0.32f, 0.08f, 0.12f, { 5.0f, 5.6f }, 0.30f, 0.12f, 1.05f },

  { "v12", "V12 supercar",
    12,
    { 0.0f, 0.0833f, 0.1667f, 0.25f, 0.3333f, 0.4167f, 0.5f, 0.5833f, 0.6667f, 0.75f, 0.8333f, 0.9167f },
    { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
    1000, 8500, 8800, 14000, 5500,
    7, { 3.08f, 2.19f, 1.63f, 1.29f, 1.03f, 0.84f, 0.69f }, 3.70f,
    { { { 210, 1.5f, 1.0f }, { 560, 2.6f, 0.7f }, { 1500, 3.0f, 0.35f } },
      { { 220, 1.5f, 1.0f }, { 585, 2.6f, 0.7f }, { 1560, 3.0f, 0.35f } } },
    1400, 8000, 0.20f, 0.12f, 0.02f, 0.04f, { 2.6f, 2.9f }, 0.25f, 0.16f, 0.85f },
};
static const int PROFILE_COUNT = sizeof(PROFILES) / sizeof(PROFILES[0]);

enum CarGear : uint8_t { CAR_UNKNOWN = 0, CAR_PARK = 1, CAR_REVERSE = 2, CAR_NEUTRAL = 3, CAR_DRIVE = 4 };

struct DriveInput {
  bool awake = false;      // car display on
  bool valid = false;      // fresh powertrain data
  uint8_t gear = CAR_UNKNOWN;
  float pedal = 0;         // 0..1
  float speedKph = 0;
  float torqueNm = 0;      // total motor torque (rear + front)
  bool hasTorque = false;
  bool brake = false;
};

enum Phase : uint8_t { OFF, CRANKING, RUNNING, STOPPING };
enum ShiftPhase : uint8_t { SHIFT_NONE, SHIFT_UP_CUT, SHIFT_UP_SLEW, SHIFT_DOWN_BLIP };

// What the synthesizer needs, updated once per audio block
struct EngineState {
  Phase phase = OFF;
  float rpm = 0;
  float load = 0;          // -1 (hard overrun) .. 1 (full load)
  float throttle = 0;      // smoothed pedal 0..1
  bool ignitionCut = false;// limiter or shift cut
  bool combustion = false; // false while cranking / stopping
  float starter = 0;       // starter motor level 0..1
  int gear = 0;            // virtual gear: -1 R, 0 N/P, 1..n
  uint32_t popBursts = 0;  // incremented when the model wants a pop (shift cut, blip)
  float level = 0;         // overall output level 0..1 (fades on start/stop)
};

class Model {
public:
  float wheelRadius = 0.334f;  // m, 235/45 R18
  float launchFactor = 0.42f;  // clutch-slip launch rpm as share of redline

  void setProfile(const Profile* p) { _p = p; reset(); }
  const Profile& profile() const { return *_p; }
  void reset() { _s = EngineState(); _shift = SHIFT_NONE; _t = 0; _phaseT = 0; _vgear = 1; _torqueRef = 250; _offT = 0; }
  const EngineState& state() const { return _s; }

  void update(float dt, const DriveInput& in) {
    const Profile& p = *_p;
    _t += dt;
    _phaseT += dt;

    // ---------- power state ----------
    bool wantRun = in.awake && in.valid;
    if (!wantRun) _offT += dt; else _offT = 0;

    switch (_s.phase) {
      case OFF:
        _s.rpm = 0; _s.level = 0; _s.combustion = false; _s.starter = 0;
        // start when the driver selects a gear or touches the pedal
        if (wantRun && (in.gear == CAR_DRIVE || in.gear == CAR_REVERSE || in.gear == CAR_NEUTRAL || in.pedal > 0.05f)) {
          _s.phase = CRANKING; _phaseT = 0;
        }
        return;
      case CRANKING: {
        _s.level = 1;
        if (_phaseT < 0.75f) {
          _s.starter = 1;
          _s.combustion = false;
          _s.rpm = 200 + 35 * sinf(_phaseT * 38.0f);
        } else {
          _s.starter = clampf(1 - (_phaseT - 0.75f) * 8, 0, 1);
          _s.combustion = true;
          float flare = p.idleRpm * 2.1f;
          float target = _phaseT < 1.05f ? flare : p.idleRpm;
          _s.rpm = follow(_s.rpm, target, dt, _phaseT < 1.05f ? 0.09f : 0.35f);
          _s.load = _phaseT < 1.05f ? 0.6f : 0.05f;
          if (_phaseT > 2.0f) { _s.phase = RUNNING; _phaseT = 0; }
        }
        if (_offT > 2.0f) { _s.phase = STOPPING; _phaseT = 0; }
        return;
      }
      case STOPPING:
        _s.combustion = false; _s.starter = 0;
        _s.rpm = follow(_s.rpm, 0, dt, 0.25f);
        _s.load = -0.1f;
        _s.level = clampf(1 - _phaseT / 1.2f, 0, 1);
        if (_phaseT > 1.2f) { _s.phase = OFF; }
        if (wantRun && _phaseT > 0.3f) { _s.phase = CRANKING; _phaseT = 0; }
        return;
      case RUNNING:
        _s.level = 1;
        _s.combustion = true;
        if (_offT > 3.0f) { _s.phase = STOPPING; _phaseT = 0; return; }
        break;
    }

    // ---------- running ----------
    float pedal = clampf(in.pedal, 0, 1);
    _s.throttle = follow(_s.throttle, pedal, dt, 0.05f);

    // adaptive torque scale: 1.0 = strongest pull seen recently
    if (in.hasTorque && in.torqueNm > _torqueRef) _torqueRef = in.torqueNm;
    _torqueRef = fmaxf(250.0f, _torqueRef - 20.0f * dt);
    float tn = in.hasTorque ? clampf(in.torqueNm / _torqueRef, -1, 1) : 0;

    float speed = fabsf(in.speedKph);
    float wheelRpm = (speed / 3.6f) / (6.2831853f * wheelRadius) * 60.0f;

    float targetLoad;
    if (pedal > 0.03f) targetLoad = in.hasTorque ? clampf(0.35f * pedal + 0.65f * fmaxf(tn, 0), 0.05f, 1) : pedal;
    else if (in.hasTorque) targetLoad = clampf(tn * 1.2f, -1, 0);     // regen = engine braking
    else targetLoad = speed > 5 ? -0.4f : 0;

    bool cut = false;
    float rpmTarget;
    float rpmTau = 0.03f;

    if (in.gear == CAR_DRIVE || in.gear == CAR_REVERSE) {
      if (in.gear == CAR_REVERSE) { _vgear = 1; _shift = SHIFT_NONE; }
      _shiftLogic(dt, wheelRpm, pedal, in.gear == CAR_DRIVE, speed);
      int g = _vgear;
      float coupled = wheelRpm * p.ratio[g - 1] * p.finalDrive;

      // clutch slip at launch: engine held at a pedal-dependent rpm until the wheels catch up
      float slip = p.idleRpm + powf(_s.throttle, 0.8f) * (p.redlineRpm * launchFactor - p.idleRpm);
      if (g == 1 && coupled < slip) {
        rpmTarget = slip;
        rpmTau = 0.12f;
      } else {
        rpmTarget = fmaxf(coupled, p.idleRpm);
      }

      switch (_shift) {
        case SHIFT_UP_CUT:
          cut = true;
          targetLoad = -0.2f;
          rpmTarget = _shiftFromRpm;
          if (_shiftT > 0.07f + 0.05f * pedal) {
            _vgear++;
            _shift = SHIFT_UP_SLEW; _shiftT = 0;
          }
          break;
        case SHIFT_UP_SLEW:
          rpmTau = 0.06f;
          if (_shiftT > 0.14f) _shift = SHIFT_NONE;
          break;
        case SHIFT_DOWN_BLIP:
          rpmTau = 0.05f;
          rpmTarget += 250 * clampf(1 - _shiftT / 0.15f, 0, 1);
          if (_shiftT < 0.07f) targetLoad = fmaxf(targetLoad, 0.55f);
          if (_shiftT > 0.25f) _shift = SHIFT_NONE;
          break;
        default: break;
      }
      _shiftT += dt;
      _s.gear = in.gear == CAR_REVERSE ? -1 : _vgear;

      // top gear past the limiter: bounce off it
      if (rpmTarget > p.limiterRpm) rpmTarget = p.limiterRpm;
    } else {
      // P / N / unknown: free revving
      _s.gear = 0; _vgear = 1; _shift = SHIFT_NONE;
      rpmTarget = p.idleRpm + powf(_s.throttle, 1.15f) * (p.limiterRpm + 150 - p.idleRpm);
      float rate = rpmTarget > _s.rpm ? p.revUpRate * (0.25f + 0.75f * _s.throttle) : p.revDownRate;
      float step = rate * dt;
      float next = _s.rpm + clampf(rpmTarget - _s.rpm, -step, step);
      rpmTarget = next;
      rpmTau = 0.0f;
      if (rpmTarget > _s.rpm + 1) targetLoad = 0.3f + 0.7f * _s.throttle;
      else if (_s.throttle < 0.05f && _s.rpm > p.idleRpm + 300) targetLoad = -0.5f;
      else targetLoad = 0.05f + 0.3f * _s.throttle;
    }

    // rev limiter: short ignition cut, rpm drops a little
    if (_s.rpm >= p.limiterRpm - 5) { _limiterT = 0.055f; }
    if (_limiterT > 0) {
      _limiterT -= dt;
      cut = true;
      targetLoad = fminf(targetLoad, -0.1f);
      rpmTarget = fminf(rpmTarget, p.limiterRpm - 220);
      rpmTau = fmaxf(rpmTau, 0.03f);
    }

    // idle wobble
    rpmTarget += (rpmTarget < p.idleRpm * 1.3f) ? 12.0f * sinf(_t * 3.1f) + 7.0f * sinf(_t * 7.3f) : 0;

    _s.rpm = follow(_s.rpm, rpmTarget, dt, rpmTau);
    _s.load = follow(_s.load, targetLoad, dt, 0.04f);
    _s.ignitionCut = cut;
  }

private:
  const Profile* _p = &PROFILES[0];
  EngineState _s;
  ShiftPhase _shift = SHIFT_NONE;
  float _shiftT = 0, _shiftFromRpm = 0, _sinceShift = 10;
  float _t = 0, _phaseT = 0, _offT = 0, _limiterT = 0;
  float _torqueRef = 250;
  int _vgear = 1;

  float _coupledRpm(float wheelRpm, int g) const { return wheelRpm * _p->ratio[g - 1] * _p->finalDrive; }

  void _shiftLogic(float dt, float wheelRpm, float pedal, bool automatic, float speed) {
    const Profile& p = *_p;
    _sinceShift += dt;
    if (!automatic || _shift != SHIFT_NONE) return;

    if (speed < 4.0f && _vgear > 1) { _vgear = 1; return; }  // stopped: back to first

    float span = p.redlineRpm - p.idleRpm;
    float up = fminf(p.idleRpm + span * (0.22f + 0.72f * powf(pedal, 1.4f)), p.redlineRpm - 100);
    float down = p.idleRpm * 1.45f + span * 0.30f * powf(pedal, 1.2f);
    float rpmNow = _coupledRpm(wheelRpm, _vgear);

    if (_sinceShift < 0.6f) return;

    if (_vgear < p.gears && rpmNow > up) {
      _shift = SHIFT_UP_CUT; _shiftT = 0; _sinceShift = 0;
      _shiftFromRpm = rpmNow;
      if (pedal > 0.45f) _s.popBursts++;   // full-throttle upshift: bang
      return;
    }
    if (_vgear > 1) {
      float lowerRpm = _coupledRpm(wheelRpm, _vgear - 1);
      bool kickdown = pedal > 0.85f && rpmNow < p.redlineRpm * 0.62f && lowerRpm < p.redlineRpm * 0.88f;
      if ((rpmNow < down || kickdown) && lowerRpm < p.redlineRpm * 0.92f) {
        _vgear--;
        _shift = SHIFT_DOWN_BLIP; _shiftT = 0; _sinceShift = 0;
        if (pedal < 0.1f && rpmNow > p.redlineRpm * 0.45f) _s.popBursts++;  // high-rpm downshift on overrun
      }
    }
  }
};

}  // namespace engine
#endif
