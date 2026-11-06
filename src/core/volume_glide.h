#pragma once
// The person's volume glides to a new place instead of jumping (App::set_volume): the engine
// applies its volume as the sound leaves, at once, and a jump of the gain is a click.
#include <algorithm>

struct VolumeGlide {
  static constexpr double kDur = 0.08;  // 80 ms: too short to hear as a fade, long enough for no click
  double from = 0, to = 0, t0 = 0;
  // smooth at both ends (no step in the slope either)
  double at(double t) const {
    const double k = std::clamp((t - t0) / kDur, 0.0, 1.0);
    return k >= 1 ? to : from + (to - from) * k * k * (3 - 2 * k);
  }
  bool done(double t) const { return t - t0 >= kDur; }
};
