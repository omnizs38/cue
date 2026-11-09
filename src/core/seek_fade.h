#pragma once
#include <algorithm>
#include <string>
#include <vector>

// Audio transport work is clocked independently of rendering and window visibility.
struct SeekFade {
  int stage = 0;  // 1: fade out, 2: wait for playback restart, 3: fade in
  double t0 = 0;
  std::vector<std::string> cmd;
  struct Step { double gain = -1; bool dispatch = false; };
  Step step(double now) {
    if (stage == 1) {
      const double k = std::clamp((now - t0) / .008, 0.0, 1.0);
      if (k >= 1) { stage = 2; t0 = now; return {0, true}; }
      return {1 - k, false};
    }
    if (stage == 2) {
      if (now - t0 > 1.5) { stage = 3; t0 = now; }
      return {};
    }
    if (stage == 3) {
      const double k = std::clamp((now - t0) / .03, 0.0, 1.0);
      if (k >= 1) stage = 0;
      return {k * k * (3 - 2 * k), false};
    }
    return {};
  }
  unsigned timer_ms() const { return stage == 2 ? 50 : stage ? 8 : 0; }
};
