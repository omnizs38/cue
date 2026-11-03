// Music's light show: rings on the beat (after the latency, at most three a second), a calm
// breath for reduced motion, quiet for silence and pauses, and a chorus turning the colours.
#include "../src/ui/music_light.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

static double db(double v) { return v > 0 ? 20 * std::log10(v) : -INFINITY; }
// a 120 BPM kick over a bed: the level at time t
static double groove(double t, double bed, double kick) { double ph = std::fmod(t, 0.5); return bed + kick * std::exp(-ph * 9); }

int main() {
  const double dt = 1 / 30.0, L = 0.2;
  // the beat: about two rings a second, never three in any second, none before they are heard
  {
    MusicLight m;
    std::vector<double> rings;
    for (int i = 0; i < 300; ++i) {
      double t = i * dt;
      m.feed(t, db(groove(t, 0.12, 0.7)), 0.06, L);
      int before = m.rings_started();
      m.step(t, dt, true, false);
      if (m.rings_started() > before) rings.push_back(t);
      assert(m.frame().energy >= 0 && m.frame().energy <= 1);
      for (auto& w : m.frame().waves) assert(w.a <= MusicLight::kMaxWave + 1e-9 && w.r >= 0);
    }
    assert(!rings.empty() && rings.front() >= L - 1e-9);
    assert(rings.size() >= 16 && rings.size() <= 22);
    for (size_t i = 0; i < rings.size(); ++i) {
      int in_second = 0;
      for (double r : rings) if (r >= rings[i] && r < rings[i] + 1.0) ++in_second;
      assert(in_second <= 3);
    }
    // each ring lands near a kick heard (kicks at multiples of 0.5 s, heard L later)
    for (double r : rings) { double ph = std::fmod(r - L + 1e-6, 0.5); assert(ph < 0.12 || ph > 0.45); }
    std::printf("music light: %zu rings in 10 s\n", rings.size());
  }
  // reduced motion: no rings, no sparkle, a soft breath only
  {
    MusicLight m;
    for (int i = 0; i < 300; ++i) {
      double t = i * dt;
      m.feed(t, db(groove(t, 0.12, 0.7)), 0.2, L);
      m.step(t, dt, true, true);
      assert(m.frame().waves.empty() && m.frame().sparkle == 0);
      if (t > 3) assert(m.frame().energy >= 0.28 && m.frame().energy <= 0.72);
    }
    assert(m.rings_started() == 0);
  }
  // silence and a pause settle into a still glow
  {
    MusicLight m;
    for (int i = 0; i < 90; ++i) { double t = i * dt; m.feed(t, -INFINITY, 0, L); m.step(t, dt, true, false); }
    assert(m.rings_started() == 0);
    for (int i = 90; i < 300; ++i) { double t = i * dt; m.step(t, dt, false, false); }
    assert(!m.moving() && std::fabs(m.frame().energy - 0.3) < 0.01 && m.frame().waves.empty());
  }
  // a chorus: thirty quiet seconds, then a loud stretch turns the colours half way round
  {
    MusicLight m;
    double turn_at_chorus = 0;
    for (int i = 0; i < 30 * 50; ++i) {
      double t = i * dt;
      bool loud = t > 30;
      if (t > 30 && turn_at_chorus == 0) turn_at_chorus = m.frame().turn;
      // loud: the bed rises against the peak the quiet part set
      m.feed(t, db(loud ? groove(t, 0.55, 0.6) : groove(t, 0.08, 0.35)), 0.05, L);
      m.step(t, dt, true, false);
    }
    assert(m.frame().section > 0.5);
    assert(m.frame().turn - turn_at_chorus > 0.5);
  }
  // highs: a bright stretch sparkles more than a dull one
  {
    MusicLight a, b;
    double sa = 0, sb = 0;
    for (int i = 0; i < 600; ++i) {
      double t = i * dt;
      a.feed(t, db(groove(t, 0.3, 0.5)), t > 10 ? 0.25 : 0.05, L); a.step(t, dt, true, false);
      b.feed(t, db(groove(t, 0.3, 0.5)), 0.05, L); b.step(t, dt, true, false);
      if (t > 10.5 && t < 13) { sa += a.frame().sparkle; sb += b.frame().sparkle; }
    }
    assert(sa > sb * 1.5);
  }
  // breathing: a slow in-and-out under the music, deeper and quicker when it is full, still at a pause
  {
    auto run = [&](double bed, double kick, double& depth, double& period) {
      MusicLight m;
      double lo = 0, hi = 0, last_up = -1, first_up = -1;
      int ups = 0;
      double prev = 0;
      for (int i = 0; i < 40 * 30; ++i) {
        double t = i * dt;
        m.feed(t, db(groove(t, bed, kick)), 0.06, L);
        m.step(t, dt, true, false);
        double b = m.frame().breath;
        assert(b >= -1 && b <= 1);
        if (t > 10) {
          lo = std::min(lo, b); hi = std::max(hi, b);
          if (prev < 0 && b >= 0) { if (first_up < 0) first_up = t; last_up = t; ++ups; }
        }
        prev = b;
      }
      depth = (hi - lo) / 2;
      period = ups > 1 ? (last_up - first_up) / (ups - 1) : 99;
    };
    double d_full, p_full, d_quiet, p_quiet;
    run(0.5, 0.5, d_full, p_full);     // full: the bed sits near the song's own peak
    run(0.02, 0.9, d_quiet, p_quiet);  // sparse: mostly quiet between kicks
    assert(d_full > 0.6 && p_full > 3.5 && p_full < 6.0);
    assert(d_quiet < d_full && p_quiet > p_full);
    MusicLight m;
    for (int i = 0; i < 300; ++i) { double t = i * dt; m.feed(t, db(groove(t, 0.5, 0.5)), 0.06, L); m.step(t, dt, true, false); }
    for (int i = 300; i < 600; ++i) { double t = i * dt; m.step(t, dt, false, false); }
    assert(m.frame().breath == 0 && !m.moving());
    std::printf("music light: breath %.2f deep every %.1f s (full), %.2f every %.1f s (sparse)\n", d_full, p_full, d_quiet, p_quiet);
  }
  // a seek (readings from the past) never stalls the queue
  {
    MusicLight m;
    m.feed(10, -20, 0.1, 5);
    m.feed(1, -20, 0.1, 0.2);
    m.step(1.3, dt, true, false);
    assert(m.frame().energy > 0);
  }
  std::puts("music light: passed");
}
