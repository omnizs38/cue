#include "../src/core/glide_clock.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  const double f = 1.0 / 60;
  // steady frames: the wall clock
  GlideClock c; c.start(10);
  assert(c.elapsed(10, f) == 0);
  for (int i = 1; i <= 15; ++i) assert(std::fabs(c.elapsed(10 + i * f, f) - i * f) < 1e-9);
  // a 150 ms stall right after the start (the seen case): one frame of progress, not 150 ms
  GlideClock s; s.start(10);
  s.elapsed(10, f);
  double e = s.elapsed(10.15, f);
  assert(std::fabs(e - f) < 1e-9);
  // and it goes on frame by frame from there
  assert(std::fabs(s.elapsed(10.15 + f, f) - 2 * f) < 1e-9);
  // a late frame or two (timer jitter) is not a stall: the wall clock stands
  GlideClock j; j.start(0); j.elapsed(0, f);
  assert(std::fabs(j.elapsed(2 * f, f) - 2 * f) < 1e-9);
  // 144 Hz: same rule with its own interval
  const double g = 1.0 / 144;
  GlideClock h; h.start(0); h.elapsed(0, g);
  assert(std::fabs(h.elapsed(0.1, g) - g) < 1e-9);
  // a new start forgets the last frame
  h.start(5);
  assert(std::fabs(h.elapsed(5.2, g) - 0.2) < 1e-9);
  std::puts("Glide clock: steady frames, a stall at the start is one frame, jitter untouched, 60/144 Hz, restart passed");
}
