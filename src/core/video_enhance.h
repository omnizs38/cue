// When the picture is helped, and how: decisions only (the platform reads the facts).
#pragma once
#include <algorithm>
#include <cmath>

// Super resolution (the graphics driver's AI upscaling, e.g. NVIDIA RTX Video): only for a film
// up to 1080p (a high-quality film, 1440p or 4K, keeps its own picture) and smaller than the
// screen, and only as far as the screen (never a scale it would throw away). 0: not used.
inline double vsr_scale(int video_w, int video_h, int screen_w, int screen_h) {
  if (video_w <= 0 || video_h <= 0 || screen_w <= 0 || screen_h <= 0) return 0;
  if (video_h < 360 || video_h > 1088 || video_w > 1920 + 8) return 0;
  const double k = std::min((double)screen_w / video_w, (double)screen_h / video_h);
  if (k < 1.05) return 0;  // already the screen's size (or larger): nothing to add
  return std::floor(std::min(k, 4.0) * 100) / 100;  // two decimals, at most four times
}

// Smoothing helps only when the film's frames do not fall evenly on the screen's refreshes
// (24 fps on 60 Hz: 3:2 judder on pans). A whole multiple (30 on 60, 24 on 120 or 144, 60 on
// 60) is already even: nothing to smooth, nothing spent.
inline bool cadence_uneven(double fps, double hz) {
  if (!(fps > 1) || !(hz > 1) || fps > hz + 0.5) return false;
  const double ratio = hz / fps;
  return std::fabs(ratio - std::round(ratio)) > 0.02 * ratio;
}
