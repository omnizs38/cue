#pragma once
#include <algorithm>

// Keep the 16:9 reading size when a landscape window becomes wider/shorter.
// Portrait and taller windows retain their established height-based scale.
// A continuous cap protects extremely short windows from oversized text.
inline double subtitle_typography_scale(int width, int height) {
  if(width<=0||height<=0)return 1.0;
  return std::clamp((double)width*9.0/(16.0*height),1.0,2.0);
}
