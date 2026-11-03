#pragma once
#include <algorithm>
#include <cmath>
struct VideoWindowSize { int w, h; };
inline VideoWindowSize video_window_size(double ar, int work_w, int work_h, int dpi, double ui) {
  ar = std::clamp(ar, .1, 10.0);
  double max_w=work_w*.90, max_h=work_h*.84;
  double w=ar<.95 ? max_h*ar : std::min(work_w*.67,1600.*dpi/96);
  double h=w/ar;
  double shrink=std::min({1.,max_w/w,max_h/h}); w*=shrink; h*=shrink;
  double grow=std::max({1.,240.*ui/w,180.*ui/h});
  grow=std::min(grow,std::min(max_w/w,max_h/h)); w*=grow;h*=grow;
  return {std::max(1,(int)std::lround(w)),std::max(1,(int)std::lround(h))};
}

// Where a window of that size goes: grown from the centre of `from`, kept inside the work area
// (`fx`, `fy`: the invisible resize borders, which may hang over its edges). Used for the window
// on screen and, while it is maximized, for the place it will restore to.
struct WinRect { int left, top, right, bottom; };
inline WinRect video_window_rect(WinRect from, WinRect work, VideoWindowSize size, int fx, int fy) {
  int W = size.w + fx, H = size.h + fy;
  int cx = (from.left + from.right) / 2, cy = (from.top + from.bottom) / 2;
  int x = std::clamp(cx - W / 2, work.left - fx / 2, std::max(work.left - fx / 2, work.right - W + fx / 2));
  int y = std::clamp(cy - H / 2, work.top, std::max(work.top, work.bottom - H + fy));
  return {x, y, x + W, y + H};
}
