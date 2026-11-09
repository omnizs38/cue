#include "../src/core/seek_fade.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  for (const double period : {.008, .010, 1. / 60, .250}) {
    SeekFade f; f.stage = 1; f.t0 = 10;
    int dispatches = 0;
    double gain = 1;
    for (int i = 0; i < 1000 && f.stage; ++i) {
      const auto s = f.step(10 + i * period);
      if (s.gain >= 0) { gain = s.gain; assert(gain >= 0 && gain <= 1); }
      dispatches += s.dispatch;
    }
    assert(dispatches == 1 && f.stage == 0 && gain == 1 && f.timer_ms() == 0);
  }
  SeekFade f; f.stage = 1; f.t0 = 1;
  assert(f.timer_ms() == 8 && f.step(1).gain == 1);
  assert(f.step(1.004).gain > .49 && f.step(1.004).gain < .51);
  assert(f.step(1.009).dispatch && f.stage == 2 && f.timer_ms() == 50);
  assert(!f.step(1.01).dispatch);  // a repeated timer cannot enqueue the seek twice
  // A playback restart releases the envelope without waiting for its watchdog.
  f.stage = 3; f.t0 = 1.02;
  assert(f.step(1.02).gain == 0);
  const double mid = f.step(1.035).gain;
  assert(mid > .49 && mid < .51);
  assert(f.step(1.06).gain == 1 && f.stage == 0);
  f = {}; assert(!f.step(20).dispatch && f.step(20).gain < 0);
  std::puts("Seek audio envelope: timer-only dispatch, one seek, gain recovery, restart, timeout and idle at 8/10/16/250 ms passed");
}
