#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

struct PreviewTimeline {
  static constexpr double kStep = 1.0;
  static int key(double t) {
    if (!std::isfinite(t) || t <= 0) return 0;
    // Stable one-second navigation samples. Keep numerical boundary tolerance if the
    // interval changes later; the image's actual timestamp is stored separately.
    return (int)std::min(std::floor(t / kStep + 1e-7), (double)std::numeric_limits<int>::max());
  }
  static double time(int key) { return std::max(0, key) * kStep; }
};
