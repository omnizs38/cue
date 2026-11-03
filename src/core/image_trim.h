// A picture boxed in flat dark bars (a video's frame padded into a square, as songs taken from
// video sites often carry for a cover): the part inside the bars, or the whole picture.
// Only even bars count (top and bottom, or left and right, of about the same size, nearly black
// from edge to edge), so a dark sky or a black border drawn into the art is left alone.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

struct TrimBox { int x = 0, y = 0, w = 0, h = 0; };

inline TrimBox trim_dark_bars(const std::vector<uint32_t>& px, int w, int h) {
  TrimBox all{0, 0, w, h};
  if (w < 16 || h < 16 || px.size() < (size_t)w * h) return all;
  auto lit = [&](int x, int y) {
    uint32_t c = px[(size_t)y * w + x];
    int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    return (r * 54 + g * 183 + b * 19) / 256 > 28;
  };
  // a line is a bar when nearly all of it is dark (a speck of noise or a watermark may remain)
  auto dark_row = [&](int y, int x0, int x1) { int n = 0; for (int x = x0; x < x1; ++x) n += lit(x, y); return n * 100 <= (x1 - x0); };
  auto dark_col = [&](int x, int y0, int y1) { int n = 0; for (int y = y0; y < y1; ++y) n += lit(x, y); return n * 100 <= (y1 - y0); };
  auto even = [](int a, int b, int size) { return a >= size * 3 / 100 && b >= size * 3 / 100 && std::abs(a - b) <= size * 4 / 100 + 2; };
  int top = 0, bottom = 0;
  while (top < h / 2 && dark_row(top, 0, w)) ++top;
  while (bottom < h / 2 && dark_row(h - 1 - bottom, 0, w)) ++bottom;
  TrimBox r = all;
  if (even(top, bottom, h) && h - top - bottom >= h * 35 / 100) { r.y = top; r.h = h - top - bottom; }
  int left = 0, right = 0;
  while (left < w / 2 && dark_col(left, r.y, r.y + r.h)) ++left;
  while (right < w / 2 && dark_col(w - 1 - right, r.y, r.y + r.h)) ++right;
  if (even(left, right, w) && w - left - right >= w * 35 / 100) { r.x = left; r.w = w - left - right; }
  return r;
}

inline void crop_pixels(std::vector<uint32_t>& px, int& w, int& h, const TrimBox& b) {
  if (b.x == 0 && b.y == 0 && b.w == w && b.h == h) return;
  std::vector<uint32_t> out((size_t)b.w * b.h);
  for (int y = 0; y < b.h; ++y)
    std::copy_n(px.begin() + (size_t)(y + b.y) * w + b.x, b.w, out.begin() + (size_t)y * b.w);
  px.swap(out);
  w = b.w;
  h = b.h;
}
