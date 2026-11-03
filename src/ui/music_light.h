// Music's light show: what the ambient halo does while a song plays. It reads two numbers the
// engine measures for every audio frame (the loudness, and how often the wave crosses zero, a
// cheap measure of the highs) and turns them into motion:
//   energy   the light's reach and glow follow the music's body (slow in, slower out)
//   breath   a steady in-and-out underneath, like breathing: deeper and a little quicker when the
//            music is full, shallow in quiet passages, settling while paused
//   waves    a beat sends one soft ring of light outward from the window
//   sparkle  the highs light a few cells at the halo's edge
//   section  a louder stretch (a chorus) turns the cover's colours half way round the window
// Readings arrive before they are heard (the engine works ahead of the speakers), so each one
// waits for its moment. Limits keep it calm: at most three rings a second, never full light,
// and nothing but a slow breath when Windows asks for less motion.
#pragma once
#include <deque>
#include <vector>

struct MusicLightFrame {
  double energy = 0;    // 0..1, smoothed: the music's body
  double breath = 0;    // -1..1: the breathing cycle times its depth (0 at rest)
  double section = 0;   // 0 verse .. 1 chorus, smoothed
  double turn = 0;      // where the colours sit around the window, in turns (grows slowly)
  double sparkle = 0;   // 0..1: how many cells twinkle
  double time = 0;      // seconds since the show began (twinkles follow it)
  struct Wave { double r, a; };  // radius in halo reaches (0 at the window), amplitude 0..1
  std::vector<Wave> waves;
};

class MusicLight {
 public:
  static constexpr double kMaxRingsPerSecond = 3;  // the photosensitivity limit, kept with margin
  static constexpr double kRingLife = 0.85;        // seconds from the window to the halo's edge
  static constexpr double kMaxWave = 0.5;

  void reset();
  // a reading made at `now` (app clock), heard `latency` seconds later; rms in dBFS (-inf ok),
  // zcr 0..1 (zero crossings per sample)
  void feed(double now, double rms_db, double zcr, double latency);
  // advance to `now`; playing false lets everything settle to a still, soft glow
  void step(double now, double dt, bool playing, bool reduced_motion);
  const MusicLightFrame& frame() const { return f_; }
  // still changing (the halo needs another paint)
  bool moving() const { return moving_; }
  int rings_started() const { return rings_; }  // tests

 private:
  struct Reading { double at, level, zcr; };
  std::deque<Reading> pending_;
  MusicLightFrame f_;
  double peak_ = 0.02;          // slow automatic gain: the song's own loud, not an absolute one
  double fast_ = 0, slow_ = 0;  // envelopes for the beat
  double mid_ = 0, long_ = 0;   // the level over a few seconds and over half a minute: verse or chorus
  double zavg_ = 0.05;
  double last_ring_ = -10;
  double turn_target_ = 0, turn_from_ = 0, turn_t0_ = -10;
  double level_ = 0, zcr_ = 0;
  bool have_ = false, chorus_ = false, moving_ = false;
  double started_ = -1;
  double phase_ = 0, depth_ = 0;  // the breathing cycle (radians) and how deep it goes
  int rings_ = 0;
};
