// Desktop preview of the engine sound: simulates a drive cycle and writes a WAV.
//
//   g++ -O2 -std=c++17 -I.. render_wav.cpp -o render_wav
//   ./render_wav v8 drive.wav        (profiles: v8, i6, boxer, v12)
//
// Uses exactly the same EngineModel.h / EngineSynth.h as the ESP32 firmware,
// so you can tune profiles here before flashing.

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <chrono>
#include "../EngineModel.h"
#include "../EngineSynth.h"

using namespace engine;

struct Key { float t, pedal; uint8_t gear; };

// Scripted driver: time, pedal, gear
static const Key SCRIPT[] = {
  { 0.0f, 0.00f, CAR_PARK },
  { 1.0f, 0.00f, CAR_DRIVE },     // select D -> engine cranks
  { 4.0f, 0.00f, CAR_PARK },      // back to P for a few revs
  { 4.5f, 0.70f, CAR_PARK },
  { 5.1f, 0.00f, CAR_PARK },
  { 5.9f, 1.00f, CAR_PARK },      // hold on the limiter
  { 7.2f, 0.00f, CAR_PARK },
  { 9.0f, 0.00f, CAR_DRIVE },
  { 9.5f, 0.30f, CAR_DRIVE },     // gentle pull-away
  { 17.0f, 1.00f, CAR_DRIVE },    // floor it
  { 27.0f, 0.00f, CAR_DRIVE },    // lift: overrun, downshifts, pops
  { 33.0f, 0.95f, CAR_DRIVE },    // kickdown
  { 37.0f, 0.00f, CAR_DRIVE },    // coast + regen to a stop
  { 52.0f, 0.00f, CAR_PARK },
  { 54.0f, 0.00f, CAR_PARK },
};

int main(int argc, char** argv) {
  const char* id = argc > 1 ? argv[1] : "v8";
  const char* path = argc > 2 ? argv[2] : "engine.wav";
  const Profile* prof = &PROFILES[0];
  for (int i = 0; i < PROFILE_COUNT; i++)
    if (!strcmp(PROFILES[i].id, id)) prof = &PROFILES[i];

  const float fs = 44100;
  const int block = 256;
  const float duration = 54.0f;
  Model model; model.setProfile(prof);
  Synth synth; synth.init(fs, prof);

  std::vector<int16_t> pcm;
  float speed = 0;  // km/h
  int nKeys = sizeof(SCRIPT) / sizeof(SCRIPT[0]);
  double synthSec = 0;
  int lastGear = -99;

  for (float t = 0; t < duration; t += block / fs) {
    int k = 0;
    while (k + 1 < nKeys && SCRIPT[k + 1].t <= t) k++;
    DriveInput in;
    in.awake = t < 53.0f;
    in.valid = in.awake;
    in.gear = SCRIPT[k].gear;
    in.pedal = SCRIPT[k].pedal;

    // crude Model 3 longitudinal dynamics
    float dt = block / fs;
    float torque = 0;
    if (in.gear == CAR_DRIVE) {
      float maxT = speed < 80 ? 420 : 420 * 80 / speed;   // constant power above ~80 km/h
      torque = in.pedal * maxT;
      if (in.pedal < 0.02f && speed > 5) torque = -160;   // regen
      if (in.pedal < 0.02f && t > 44) torque = speed > 1 ? -300 : 0;  // braking
    }
    float accel = torque * 9.0f / (0.334f * 1850) - 0.0004f * speed * speed / 3.6f - 0.15f;  // m/s^2
    speed += accel * 3.6f * dt;
    if (speed < 0) speed = 0;
    in.speedKph = speed;
    in.torqueNm = torque;
    in.hasTorque = true;

    model.update(dt, in);
    const EngineState& st = model.state();
    if (st.gear != lastGear) {
      printf("%6.2fs  gear %2d  %5.0f rpm  %5.1f km/h\n", t, st.gear, st.rpm, speed);
      lastGear = st.gear;
    }
    size_t off = pcm.size();
    pcm.resize(off + block * 2);
    auto t0 = std::chrono::high_resolution_clock::now();
    synth.render(&pcm[off], block, st);
    synthSec += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
  }

  // WAV header
  FILE* f = fopen(path, "wb");
  if (!f) { perror(path); return 1; }
  uint32_t dataBytes = pcm.size() * 2, rate = 44100, byteRate = rate * 4;
  uint16_t ch = 2, bits = 16, align = 4, fmt = 1;
  uint32_t riffSize = 36 + dataBytes, fmtSize = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riffSize, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmtSize, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f);
  fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f);
  fwrite(pcm.data(), 2, pcm.size(), f);
  fclose(f);

  int peak = 0; double sum = 0;
  for (int16_t s : pcm) { peak = abs(s) > peak ? abs(s) : peak; sum += (double)s * s; }
  printf("%s: %s, %.0f s, peak %d, rms %.0f, synth %.1f%% of realtime on this PC\n",
         path, prof->name, duration, peak, sqrt(sum / pcm.size()), 100.0 * synthSec / duration);
  return 0;
}
