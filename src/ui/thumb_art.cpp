#include "thumb_art.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace thumb_art {
namespace {

inline uint32_t rgb_px(Rgb c) { return 0xFF000000u | (uint32_t)c.r << 16 | (uint32_t)c.g << 8 | c.b; }
inline Rgb px_rgb(uint32_t v) { return {(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; }

// the phone's stand-in colours (assets/remote: pixelThumb), the same family on both screens
const Rgb kBase[4] = {{0xa0, 0x50, 0x8f}, {0xd9, 0x82, 0x2b}, {0x3f, 0x7f, 0xd1}, {0x3f, 0xa3, 0x6b}};
const int kBayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

Rgb shade(Rgb c, double f) {
  auto ch = [f](uint8_t v) {
    double x = f < 1 ? v * f : v + (255 - v) * (f - 1);
    return (uint8_t)std::clamp((int)std::lround(x), 0, 255);
  };
  return {ch(c.r), ch(c.g), ch(c.b)};
}

// glyphs for audio stand-ins, the home's own 7-row icons (the list's k-audio note, a microphone, a book)
struct Glyph { int w, h; const char* cells; };
const Glyph kNote{7, 7, "...###....#.##...#......#....###...####....##...."};
const Glyph kMic{7, 7, "..###....###..#.###.##.###.#.#...#...###.....#..."};
const Glyph kBook{11, 7, ".###...###.#####.##########.##########.##########.##########.#####....#.#...."};

void cut_corners(Canvas& out, int cut) {
  if (cut <= 0 || out.w <= 2 * cut || out.h <= 2 * cut) return;
  for (int y = 0; y < cut; ++y)
    for (int x = 0; x < cut; ++x) {
      out.px[(size_t)y * out.w + x] = 0;
      out.px[(size_t)y * out.w + out.w - 1 - x] = 0;
      out.px[(size_t)(out.h - 1 - y) * out.w + x] = 0;
      out.px[(size_t)(out.h - 1 - y) * out.w + out.w - 1 - x] = 0;
    }
}

}  // namespace

uint32_t seed_of(const std::string& s) {
  uint32_t v = 0;
  for (unsigned char ch : s) v = v * 31u + ch;
  return v;
}

void resample(const uint32_t* src, int sw, int sh, double sx, double sy, double cw, double ch,
              uint32_t* dst, int stride, int w, int h) {
  if (!src || sw <= 0 || sh <= 0 || w <= 0 || h <= 0 || cw <= 0 || ch <= 0) return;
  const double fx = cw / w, fy = ch / h;
  if (fx >= 1 && fy >= 1) {
    // shrinking: each target pixel the average of the source pixels under it (fractional edges weighed)
    for (int y = 0; y < h; ++y) {
      const double y0 = sy + y * fy, y1 = y0 + fy;
      for (int x = 0; x < w; ++x) {
        const double x0 = sx + x * fx, x1 = x0 + fx;
        double r = 0, g = 0, b = 0, n = 0;
        for (int yy = std::max(0, (int)y0); yy < std::min(sh, (int)std::ceil(y1)); ++yy) {
          const double wy = std::min<double>(yy + 1, y1) - std::max<double>(yy, y0);
          if (wy <= 0) continue;
          const uint32_t* row = src + (size_t)yy * sw;
          for (int xx = std::max(0, (int)x0); xx < std::min(sw, (int)std::ceil(x1)); ++xx) {
            const double wx = std::min<double>(xx + 1, x1) - std::max<double>(xx, x0);
            if (wx <= 0) continue;
            const double k = wx * wy;
            const uint32_t v = row[xx];
            r += k * ((v >> 16) & 255); g += k * ((v >> 8) & 255); b += k * (v & 255); n += k;
          }
        }
        if (n <= 0) n = 1;
        dst[(size_t)y * stride + x] = 0xFF000000u | (uint32_t)std::lround(r / n) << 16 |
                                      (uint32_t)std::lround(g / n) << 8 | (uint32_t)std::lround(b / n);
      }
    }
    return;
  }
  // growing (a small picture on a large screen): bilinear, so it stays a photograph
  for (int y = 0; y < h; ++y) {
    const double v = std::clamp(sy + (y + 0.5) * fy - 0.5, 0.0, sh - 1.0);
    const int y0 = (int)v, y1 = std::min(sh - 1, y0 + 1);
    const double ty = v - y0;
    for (int x = 0; x < w; ++x) {
      const double u = std::clamp(sx + (x + 0.5) * fx - 0.5, 0.0, sw - 1.0);
      const int x0 = (int)u, x1 = std::min(sw - 1, x0 + 1);
      const double tx = u - x0;
      const uint32_t a = src[(size_t)y0 * sw + x0], b = src[(size_t)y0 * sw + x1];
      const uint32_t c = src[(size_t)y1 * sw + x0], d = src[(size_t)y1 * sw + x1];
      uint32_t out = 0xFF000000u;
      for (int sh8 = 0; sh8 < 24; sh8 += 8) {
        const double top = ((a >> sh8) & 255) * (1 - tx) + ((b >> sh8) & 255) * tx;
        const double bot = ((c >> sh8) & 255) * (1 - tx) + ((d >> sh8) & 255) * tx;
        out |= (uint32_t)std::clamp((int)std::lround(top * (1 - ty) + bot * ty), 0, 255) << sh8;
      }
      dst[(size_t)y * stride + x] = out;
    }
  }
}

namespace {
// a cover whole in a side x side square at (ox, 0) of `out` (centred if it is not square, on
// black), its corners cut
void cover_square(const HomeThumb& t, Canvas& out, int side, int cut) {
  int ch = side, cw = (int)std::lround(side * (double)t.w / t.h);
  if (cw > side) { cw = side; ch = (int)std::lround(side * (double)t.h / t.w); }
  cw = std::max(1, cw); ch = std::max(1, ch);
  const int x = (side - cw) / 2, y = (side - ch) / 2;
  resample(t.px.data(), t.w, t.h, 0, 0, t.w, t.h, out.px.data() + (size_t)y * out.w + x, out.w, cw, ch);
  cut_corners(out, cut);
}
}  // namespace

bool square(HomeKind kind, bool audio) {
  return audio || kind == HomeKind::Music || kind == HomeKind::Listen || kind == HomeKind::Book;
}

void make_tile(const HomeThumb& t, int w, int h, int cut, Canvas& out) {
  if (t.cover) w = h;  // a cover is square: the tile's height
  out.resize(w, h);
  if (w <= 0 || h <= 0) return;
  if (t.px.empty() || t.w <= 0 || t.h <= 0 || t.px.size() < (size_t)t.w * t.h) { out.clear({16, 16, 16}); cut_corners(out, cut); return; }
  if (t.cover) {
    out.clear({0, 0, 0});
    cover_square(t, out, h, cut);
    return;
  }
  // a frame fills the tile: its middle, at the tile's shape
  double cw = t.w, ch = t.h, sx = 0, sy = 0;
  const double ar = (double)w / h;
  if (t.w / (double)t.h > ar) { cw = t.h * ar; sx = (t.w - cw) / 2; } else { ch = t.w / ar; sy = (t.h - ch) / 2; }
  resample(t.px.data(), t.w, t.h, sx, sy, cw, ch, out.px.data(), w, w, h);
  cut_corners(out, cut);
}

uint32_t accent(const HomeThumb& t) {
  // the colour a person would name the cover by: its most vivid colours weigh most; a grey
  // cover gives its own grey, lifted so the label still reads as a label
  double r = 0, g = 0, b = 0, n = 0, ar = 0, ag = 0, ab = 0, an = 0;
  for (size_t i = 0; i < t.px.size(); i += 3) {
    const int R = (t.px[i] >> 16) & 255, G = (t.px[i] >> 8) & 255, B = t.px[i] & 255;
    const int mx = std::max({R, G, B}), mn = std::min({R, G, B});
    ar += R; ag += G; ab += B; an += 1;
    if (mx < 48) continue;  // the dark of a cover is not its colour
    const double sat = (mx - mn) / (double)mx, wgt = sat * sat * (mx / 255.0);
    r += R * wgt; g += G * wgt; b += B * wgt; n += wgt;
  }
  Rgb c;
  if (n > an * 0.004 && n > 0) c = {(uint8_t)(r / n), (uint8_t)(g / n), (uint8_t)(b / n)};
  else if (an > 0) c = {(uint8_t)(ar / an), (uint8_t)(ag / an), (uint8_t)(ab / an)};
  else c = {120, 120, 120};
  // its light kept between a dark label and a glaring one
  const double l = (c.r * 3 + c.g * 6 + c.b) / 10.0;
  if (l < 90) c = mix(c, {255, 255, 255}, std::min(0.6, (90 - l) / 255.0 * 1.6));
  if (l > 200) c = mix(c, {0, 0, 0}, (l - 200) / 255.0);
  return rgb_px(c);
}

uint32_t stand_in_accent(const std::string& seed) { return rgb_px(shade(kBase[seed_of(seed) % 4], 1.15)); }

void make_stand_in(HomeKind kind, bool audio, const std::string& seed, int w, int h, double s, int cut, Canvas& out) {
  if (square(kind, audio)) w = h;  // audio is square, as its covers are
  out.resize(w, h);
  if (w <= 0 || h <= 0) return;
  s = std::max(1.0, s);
  const uint32_t sd = seed_of(seed);
  const Rgb base = kBase[sd % 4];
  const int aw = std::max(1, (int)std::lround(w / s)), ah = std::max(1, (int)std::lround(h / s));
  auto cell = [&](int x, int y, Rgb c) { out.fill(x * s, y * s, s, s, c); };
  auto bay = [](int x, int y) { return kBayer[(y & 3) * 4 + (x & 3)] / 16.0; };
  // a gradient in three tones, dithered only where one gives way to the next (pixel-art bands)
  auto tone = [&](double v, int x, int y, Rgb a, Rgb b, Rgb c) {
    const int k = std::clamp((int)std::floor(v * 2 + (bay(x, y) - 0.5) * 0.7 + 0.5), 0, 2);
    return k == 0 ? a : k == 1 ? b : c;
  };
  const double f = 48.0 / aw;  // the shapes are drawn for the large tile and follow its size
  const bool music_like = audio || kind == HomeKind::Music || kind == HomeKind::Listen || kind == HomeKind::Book;
  if (!music_like) {
    // a film: dusk over two rows of hills, its sun or moon low in the sky
    const double ph1 = (sd >> 3) % 628 / 100.0, ph2 = (sd >> 7) % 628 / 100.0, ph3 = (sd >> 11) % 628 / 100.0;
    const double hor = ah * (0.56 + ((sd >> 15) % 3) * 0.05);
    const double sunx = aw * (0.18 + ((sd >> 17) % 64) / 100.0), suny = hor - ah * 0.3, sunr = ah * 0.115;
    const Rgb sky0 = shade(base, .12), sky1 = shade(base, .3), sky2 = shade(base, .62);
    const Rgb far = shade(base, .24), near = shade(base, .09), sun = shade(base, 1.5);
    for (int x = 0; x < aw; ++x) {
      const double X = x * f;
      const double far_y = hor - ah * (0.07 + 0.08 * (0.5 + 0.5 * std::sin(X * 0.13 + ph1)) + 0.03 * std::sin(X * 0.37 + ph2));
      const double near_y = hor + ah * (0.1 + 0.07 * std::sin(X * 0.09 + ph3) + 0.03 * std::sin(X * 0.29 + ph1));
      for (int y = 0; y < ah; ++y) {
        Rgb c;
        if (y + 0.5 >= near_y) c = near;
        else if (y + 0.5 >= far_y) c = far;
        else {
          const double dx = x + 0.5 - sunx, dy = y + 0.5 - suny;
          if (dx * dx + dy * dy <= sunr * sunr + 0.6) c = sun;
          else c = tone(std::pow(std::clamp((y + 0.5) / hor, 0.0, 1.0), 1.1), x, y, sky0, sky1, sky2);
        }
        cell(x, y, c);
      }
    }
  } else {
    // audio: a quiet ground of its colour, a glow rising from below, and its sign in the middle
    const Rgb back = shade(base, .14), mid = shade(base, .24), glow = shade(base, .36);
    for (int y = 0; y < ah; ++y)
      for (int x = 0; x < aw; ++x) {
        const double u = std::clamp((y + 0.5 - ah * 0.3) / (ah * 0.7), 0.0, 1.0);
        cell(x, y, tone(u * u * 0.95, x, y, back, mid, glow));
      }
    const Glyph& g = kind == HomeKind::Book ? kBook : kind == HomeKind::Listen ? kMic : kNote;
    const int k = std::max(1, (int)std::lround(ah * 0.5 / g.h));
    const int ox = (aw - g.w * k) / 2, oy = (ah - g.h * k) / 2;
    const Rgb ink = shade(base, 1.82);
    for (int j = 0; j < g.h; ++j)
      for (int i = 0; i < g.w; ++i)
        if (g.cells[j * g.w + i] == '#') out.fill((ox + i * k) * s, (oy + j * k) * s, k * s, k * s, ink);
  }
  cut_corners(out, cut);
}

void make_disc(uint32_t label_rgb, int side, double s, double turn, double light, Canvas& out) {
  out.resize(side, side);
  out.clear_transparent();
  if (side <= 0) return;
  const int A = std::max(1, (int)std::lround(s));
  const double R = side / 2.0;
  const Rgb label = px_rgb(label_rgb);
  auto lit = [light](Rgb c) { return mix({0, 0, 0}, c, light); };
  for (int y = 0; y < side; y += A)
    for (int x = 0; x < side; x += A) {
      const double dx = x + A / 2.0 - R, dy = y + A / 2.0 - R, d = std::sqrt(dx * dx + dy * dy);
      if (d > R) continue;
      const double u = d / R, a = std::atan2(dy, dx) - turn;
      Rgb c;
      if (u < 0.1) c = {4, 4, 4};                         // the spindle hole
      else if (u < 0.37) {
        c = label;                                        // the label, and a mark on it that turns
        const double mx = std::cos(turn) * R * 0.24, my = std::sin(turn) * R * 0.24;
        if (std::hypot(dx - mx, dy - my) < A * 0.9) c = mix(label, {255, 255, 255}, 0.55);
      } else if (u > 0.95) c = {9, 9, 9};                 // the rim
      else {
        // grooves, and the two soft reflections a record has (they turn with it)
        const bool groove = ((int)(d / (2.0 * A))) % 2 == 0;
        c = groove ? Rgb{24, 24, 24} : Rgb{16, 16, 16};
        const double sheen = std::pow(std::fabs(std::cos(a - 0.8)), 6);
        c = mix(c, {72, 72, 72}, sheen * 0.55);
      }
      out.fill_pixels(x, y, std::min(A, side - x), std::min(A, side - y), lit(c));
    }
}

}  // namespace thumb_art
