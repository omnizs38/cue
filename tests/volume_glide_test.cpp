// The volume glides (core/volume_glide.h, App::set_volume): 80 ms, smooth at both ends, never
// overshooting, and a new place asked mid-glide starts from where the sound is.
#include "../src/core/volume_glide.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  for (double from : {0.0, 40.0, 95.0, 125.0})
    for (double to : {0.0, 45.0, 100.0, 125.0}) {
      VolumeGlide g{from, to, 1.0};
      assert(g.at(1.0) == from && g.at(1.0 + VolumeGlide::kDur) == to && g.at(5) == to);
      assert(!g.done(1.05) && g.done(1.0 + VolumeGlide::kDur));
      double last = from, biggest = 0;
      for (int i = 1; i <= 80; ++i) {  // every millisecond
        double v = g.at(1.0 + i / 1000.0);
        assert(v >= std::fmin(from, to) - 1e-9 && v <= std::fmax(from, to) + 1e-9);  // no overshoot
        assert(to >= from ? v >= last - 1e-9 : v <= last + 1e-9);                     // one way
        biggest = std::fmax(biggest, std::fabs(v - last));
        last = v;
      }
      // the steepest millisecond is under 2% of the jump (a jump was 100% in one step)
      if (from != to) assert(biggest <= std::fabs(to - from) * 0.02);
      // the timer's own steps (about 16 ms between Windows timer ticks): each under a third of it
      if (from != to)
        for (double t = 1.0; t < 1.0 + VolumeGlide::kDur; t += 0.016)
          assert(std::fabs(g.at(t + 0.016) - g.at(t)) <= std::fabs(to - from) * 0.33);
    }
  // asked again mid-glide: the new glide starts at the current value (continuous)
  VolumeGlide a{50, 100, 0};
  double mid = a.at(0.03);
  VolumeGlide b{mid, 60, 0.03};
  assert(std::fabs(b.at(0.03) - mid) < 1e-12);
  puts("Volume glide: 80 ms, smooth at both ends, no overshoot, small steps, continuous when asked again passed");
}
