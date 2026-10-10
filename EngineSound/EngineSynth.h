#ifndef ENGINE_SYNTH_H
#define ENGINE_SYNTH_H
// Procedural engine sound. Plain C++ (no Arduino).
//
// Each cylinder fires an exhaust pulse at its slot in the 720° cycle. Pulses
// go into one of two exhaust banks; each bank is a brightness lowpass, three
// resonances (the pipe "voice") and a short reflection delay. Load drives the
// pulse strength and brightness; overrun makes the pulses weak and dark and
// adds random pops; ignition cuts drop pulses (limiter stutter, shift bang).

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "EngineModel.h"

namespace engine {

struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  inline float process(float x) {
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
  void setLowpass(float fs, float f, float q) {
    float w = 6.2831853f * f / fs, c = cosf(w), a = sinf(w) / (2 * q), n = 1 / (1 + a);
    b0 = (1 - c) * 0.5f * n; b1 = (1 - c) * n; b2 = b0; a1 = -2 * c * n; a2 = (1 - a) * n;
  }
  void setBandpass(float fs, float f, float q) {  // 0 dB peak
    float w = 6.2831853f * f / fs, c = cosf(w), a = sinf(w) / (2 * q), n = 1 / (1 + a);
    b0 = a * n; b1 = 0; b2 = -a * n; a1 = -2 * c * n; a2 = (1 - a) * n;
  }
  void setLowShelf(float fs, float f, float dbGain) {  // RBJ, slope 1
    float A = powf(10.0f, dbGain / 40.0f), w = 6.2831853f * f / fs, c = cosf(w), sn = sinf(w);
    float alpha = sn / 2 * sqrtf(2.0f), k = 2 * sqrtf(A) * alpha;
    float n = 1 / ((A + 1) + (A - 1) * c + k);
    b0 = A * ((A + 1) - (A - 1) * c + k) * n;
    b1 = 2 * A * ((A - 1) - (A + 1) * c) * n;
    b2 = A * ((A + 1) - (A - 1) * c - k) * n;
    a1 = -2 * ((A - 1) + (A + 1) * c) * n;
    a2 = ((A + 1) + (A - 1) * c - k) * n;
  }
  void setHighpass(float fs, float f, float q) {
    float w = 6.2831853f * f / fs, c = cosf(w), a = sinf(w) / (2 * q), n = 1 / (1 + a);
    b0 = (1 + c) * 0.5f * n; b1 = -(1 + c) * n; b2 = b0; a1 = -2 * c * n; a2 = (1 - a) * n;
  }
};

class Synth {
public:
  static const int PIPE_LEN = 512;   // > 11 ms at 44.1 kHz
  static const int SUB = 32;         // control-rate sub-block

  void init(float sampleRate, const Profile* p) {
    _fs = sampleRate;
    setProfile(p);
  }

  void setProfile(const Profile* p) {
    _p = p;
    memset(_pipe, 0, sizeof(_pipe));
    for (int b = 0; b < 2; b++) {
      for (int r = 0; r < 3; r++) _res[b][r].setBandpass(_fs, p->res[b][r].freq, p->res[b][r].q);
      _lp[b] = Biquad(); _lp[b].setLowpass(_fs, p->lpMin, 0.7f);
      _pipeDelay[b] = (int)(p->pipeMs[b] * 0.001f * _fs);
      if (_pipeDelay[b] >= PIPE_LEN) _pipeDelay[b] = PIPE_LEN - 1;
      _dc[b] = Biquad(); _dc[b].setHighpass(_fs, 28, 0.7f);
    }
    _popBp.setBandpass(_fs, 1900, 0.9f);
    for (int i = 0; i < 12; i++) { _amp[i] = 1; _prevU[i] = 1; _misfire[i] = false; }
    _lastPopBursts = 0xFFFFFFFF;
  }

  float volume = 0.8f;  // 0..1, applied after the soft clipper
  float pops = 0.4f;    // 0..1, scales overrun crackle and shift bangs
  float bassDb = 6.0f;  // 0..12 dB low-shelf boost below ~150 Hz

  // Render interleaved stereo int16. state = engine state for this block.
  void render(int16_t* out, int frames, const EngineState& st) {
    const Profile& p = *_p;
    if (_lastPopBursts == 0xFFFFFFFF) _lastPopBursts = st.popBursts;
    if (st.popBursts != _lastPopBursts) {
      for (uint32_t k = _lastPopBursts; k != st.popBursts; k++)
        if (_randU() < pops) _pendingPops++;
      _lastPopBursts = st.popBursts;
    }
    if (bassDb != _shelfDb) {
      _shelfDb = bassDb;
      for (int b = 0; b < 2; b++) _shelf[b].setLowShelf(_fs, 150, _shelfDb);
    }
    // keep headroom when the bass is raised so the clipper doesn't flatten it
    const float bassTrim = powf(10.0f, -_shelfDb / 30.0f);

    float rpm0 = _rpm, rpm1 = st.rpm;
    for (int base = 0; base < frames; base += SUB) {
      int n = frames - base < SUB ? frames - base : SUB;
      float t = (float)(base + n) / frames;

      // ---- control rate ----
      _load += (st.load - _load) * 0.15f;
      _level += (st.level - _level) * 0.05f;
      float posLoad = clampf(_load, 0, 1);
      float overrun = clampf(-_load, 0, 1);
      float rpmNorm = clampf(st.rpm / p.redlineRpm, 0, 1.1f);
      float bright = clampf(0.15f + 0.6f * posLoad + 0.35f * rpmNorm - 0.25f * overrun, 0, 1);
      float cutoff = lerpf(p.lpMin, p.lpMax, bright * bright);
      for (int b = 0; b < 2; b++) _lp[b].setLowpass(_fs, cutoff, 0.8f);
      float pulseAmp = st.combustion ? fmaxf((0.25f + 0.75f * posLoad) * (1.0f - 0.45f * overrun), 0.12f) : 0.08f;
      float noiseAmt = p.noise * (st.combustion ? (0.5f + posLoad) : 0.3f);
      float variation = p.variation * (st.rpm < p.idleRpm * 1.4f ? 1.0f : 0.4f) + 0.25f * overrun;
      float popProb = st.combustion ? pops * p.crackle * overrun * clampf((st.rpm - 2200) / 2500.0f, 0, 1) : 0;
      if (st.ignitionCut) popProb = fmaxf(popProb, pops * p.crackle * 0.4f);
      float intakeAmp = p.intake * _throttleS * (0.3f + rpmNorm);
      _throttleS += (st.throttle - _throttleS) * 0.2f;
      float invPw = 1.0f / p.pulseWidth;

      for (int i = 0; i < n; i++) {
        float rpm = rpm0 + (rpm1 - rpm0) * ((float)(base + i) / frames);
        _rpm = rpm;
        // crank angle in revolutions, 4-stroke cycle = 2 revs
        _crank += rpm / 60.0f / _fs;
        if (_crank >= 2.0f) _crank -= 2.0f;
        float cycle = _crank * 0.5f;

        float exc[2] = { 0, 0 };
        float envSum = 0;
        for (int c = 0; c < p.cylinders; c++) {
          float u = cycle - p.slot[c];
          u -= floorf(u);
          u *= 2.0f;  // revolutions since this cylinder's exhaust opened
          if (u < _prevU[c]) {
            // new firing event for this cylinder
            float r = _rand();
            _amp[c] = 1.0f + variation * r;
            _misfire[c] = st.ignitionCut && _randU() < 0.65f;
            bool pop = false;
            if (_pendingPops > 0 && _randU() < 0.5f) { pop = true; _pendingPops--; }
            else if (_randU() < popProb) pop = true;
            if (pop && st.combustion) {
              _popEnv = 0.7f + 0.6f * _randU();
              _popBank = p.bank[c];
            }
          }
          _prevU[c] = u;
          if (u < p.pulseWidth) {
            float x = u * invPw;
            float shape = 4.0f * x * (1 - x) * (1 - x) * 1.7f;  // fast attack, slow release
            float a = _misfire[c] ? 0.1f * pulseAmp : pulseAmp * _amp[c];
            float e = a * shape * (1.0f + noiseAmt * _rand());
            exc[p.bank[c]] += e;
            envSum += shape;
          }
        }

        // pops: decaying noise burst, boomy into the pipe + bright crack on top
        float popSig = 0;
        if (_popEnv > 0.001f) {
          float nz = _rand();
          popSig = nz * _popEnv;
          exc[_popBank] += popSig * 1.6f;
          _popEnv *= _popDecay;
        }
        float crack = _popBp.process(popSig) * 1.2f;

        // starter motor
        float starter = 0;
        if (st.starter > 0.001f) {
          _starterPh += (120.0f + rpm * 0.35f) / _fs;
          if (_starterPh > 1) _starterPh -= 1;
          float s = sinf(6.2831853f * _starterPh);
          starter = st.starter * 0.12f * (s * fabsf(s) + 0.3f * _rand());
        }

        // intake roar, modulated by the firing pulses
        float intake = intakeAmp * _rand() * (0.4f + 0.6f * clampf(envSum, 0, 1));

        float outB[2];
        for (int b = 0; b < 2; b++) {
          float x = _lp[b].process(exc[b]);
          float y = x * 0.35f;
          for (int r = 0; r < 3; r++) y += _res[b][r].process(x) * p.res[b][r].gain * 2.2f;
          // pipe reflection
          int rd = _pipePos - _pipeDelay[b];
          if (rd < 0) rd += PIPE_LEN;
          float refl = _pipe[b][rd];
          float yo = y + refl * p.pipeFeedback;
          _pipe[b][_pipePos] = yo;
          outB[b] = _dc[b].process(yo);
        }
        _pipePos = (_pipePos + 1) % PIPE_LEN;

        float common = crack + starter + intake;
        float l = (0.72f * outB[0] + 0.28f * outB[1]) + common;
        float rr = (0.28f * outB[0] + 0.72f * outB[1]) + common;
        float g = p.gain * _level * 1.3f * bassTrim;
        l = _softClip(_shelf[0].process(l) * g) * volume;
        rr = _softClip(_shelf[1].process(rr) * g) * volume;
        out[(base + i) * 2] = (int16_t)(clampf(l, -1, 1) * 32000);
        out[(base + i) * 2 + 1] = (int16_t)(clampf(rr, -1, 1) * 32000);
      }
      (void)t;
    }
    _rpm = rpm1;
  }

private:
  const Profile* _p = &PROFILES[0];
  float _fs = 44100;
  Biquad _res[2][3], _lp[2], _dc[2], _popBp, _shelf[2];
  float _shelfDb = -1;
  float _pipe[2][PIPE_LEN];
  int _pipeDelay[2] = { 100, 100 };
  int _pipePos = 0;
  float _crank = 0, _rpm = 0, _load = 0, _level = 0, _throttleS = 0;
  float _amp[12], _prevU[12];
  bool _misfire[12];
  float _popEnv = 0, _popDecay = 0.9983f;  // ~13 ms
  uint8_t _popBank = 0;
  int _pendingPops = 0;
  uint32_t _lastPopBursts = 0xFFFFFFFF;
  float _starterPh = 0;
  uint32_t _rng = 0x9E3779B9;

  inline float _randU() {  // 0..1
    _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
    return (_rng >> 8) * (1.0f / 16777216.0f);
  }
  inline float _rand() { return _randU() * 2.0f - 1.0f; }
  static inline float _softClip(float x) {
    if (x > 3) return 1;
    if (x < -3) return -1;
    float x2 = x * x;
    return x * (27 + x2) / (27 + 9 * x2);
  }
};

}  // namespace engine
#endif
