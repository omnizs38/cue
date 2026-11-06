// The card's turning cover, drawn by Cue itself instead of the engine.
//
// A song change in the mini player's card turns the cover about its upright axis. Drawn by the
// engine's shader, the turn could only move when the engine drew a picture: the song's own pictures
// come on the music's clock, not the screen's, and around a song change the engine draws nothing
// at all for a moment (the old song closes, the new one opens). So the turn stepped. Here the turn
// is drawn by Cue into a small see-through window over the cover, on its own thread, once per
// screen refresh, from the clock alone; the engine only stops drawing the cover meanwhile.
//
// What it draws is what blur_shader.h's cover pass draws (the same camera, the same soft shadow,
// the same rim of light), so the window can come and go over the engine's cover without a seam:
// pixels in premultiplied BGRA (0xAARRGGBB), black with the shadow's alpha where the room shows
// through, the cover's own colours where it stands.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// A cover as the engine shows it: square, `side` px a side, opaque BGRA.
struct CoverArt {
  std::vector<uint32_t> px;
  int side = 0;
  bool ok() const { return side > 0 && px.size() == (size_t)side * side; }
};

// The square part of a picture as the engine's graph takes it: the bars' crop (shares of the
// picture), then the middle square of what is left (scaled to cover, cut to the square), resampled
// to `side` px with 4 bilinear samples a pixel (the source is about the size of the result or a
// little larger: a box this size keeps it smooth without blurring it).
inline CoverArt cover_art_square(const uint32_t* src, int w, int h, int side, double cx = 0, double cy = 0, double cw = 1, double ch = 1) {
  CoverArt a;
  if (!src || w <= 0 || h <= 0 || side <= 0) return a;
  double x0 = cx * w, y0 = cy * h, rw = std::max(1.0, cw * w), rh = std::max(1.0, ch * h);
  const double sq = std::min(rw, rh);
  x0 += (rw - sq) / 2; y0 += (rh - sq) / 2;
  a.side = side;
  a.px.resize((size_t)side * side);
  const double step = sq / side;
  auto at = [&](int x, int y) { return src[(size_t)std::clamp(y, 0, h - 1) * w + std::clamp(x, 0, w - 1)]; };
  auto bilinear = [&](double fx, double fy, double out[3]) {
    fx -= 0.5; fy -= 0.5;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const double tx = fx - ix, ty = fy - iy;
    const uint32_t p00 = at(ix, iy), p10 = at(ix + 1, iy), p01 = at(ix, iy + 1), p11 = at(ix + 1, iy + 1);
    for (int c = 0; c < 3; ++c) {
      const int sh = c * 8;
      const double top = ((p00 >> sh) & 255) * (1 - tx) + ((p10 >> sh) & 255) * tx;
      const double bot = ((p01 >> sh) & 255) * (1 - tx) + ((p11 >> sh) & 255) * tx;
      out[c] += top * (1 - ty) + bot * ty;
    }
  };
  for (int y = 0; y < side; ++y)
    for (int x = 0; x < side; ++x) {
      double acc[3] = {0, 0, 0};
      for (int sy = 0; sy < 2; ++sy)
        for (int sx = 0; sx < 2; ++sx) bilinear(x0 + (x + 0.25 + 0.5 * sx) * step, y0 + (y + 0.25 + 0.5 * sy) * step, acc);
      uint32_t v = 0xFF000000u;
      for (int c = 0; c < 3; ++c) v |= (uint32_t)std::clamp((int)std::lround(acc[c] / 4), 0, 255) << (c * 8);
      a.px[(size_t)y * side + x] = v;
    }
  return a;
}

// Cue's music tile (a song without a cover of its own), as audio_placeholder_cover draws it:
// near-black, with the note of Cue's music icon in big pixels, 26x26 cells scaled up whole.
inline CoverArt cover_art_tile(int side) {
  static const char* note[] = {"...###..", "...#.##.", "...#..##", "...#...#", "...#....",
                               "...#....", ".###....", "####....", "####....", ".##....."};
  CoverArt a;
  if (side <= 0) return a;
  a.side = side;
  a.px.assign((size_t)side * side, 0xFF0E0E0Eu);
  for (int y = 0; y < side; ++y) {
    const int cy = std::min(25, y * 26 / side) - 8;
    if (cy < 0 || cy >= 10) continue;
    for (int x = 0; x < side; ++x) {
      const int cx = std::min(25, x * 26 / side) - 9;
      if (cx >= 0 && cx < 8 && note[cy][cx] == '#') a.px[(size_t)y * side + x] = 0xFFD2D2D2u;
    }
  }
  return a;
}

// One picture of the turn into `out` (ow x oh, premultiplied BGRA). shadow_k: how much of the soft
// shadow (0: none; the engine's own shadow fades out as this one fades in, and back: see
// cover_turn_window.h). (ccx, ccy): the cover's centre
// in `out`, in pixels (pixel centres at +0.5, as the shader's HOOKED_pos); h: half the cover's side;
// rx, ry: the turn (radians: rx about the level axis, ry about the upright one); k: the light under
// the pointer (0..1), as the shader's cov_k. Nothing is drawn past 1.4 h from the centre (the
// shader leaves the picture as it is there too).
inline void cover_turn_draw(const CoverArt& art, uint32_t* out, int ow, int oh, double ccx, double ccy, double h,
                            double rx, double ry, double k, double shadow_k = 1.0) {
  const bool with_shadow = shadow_k > 0.001;
  if (!out || ow <= 0 || oh <= 0) return;
  std::fill(out, out + (size_t)ow * oh, 0u);
  if (h <= 0) return;
  k = std::clamp(k, 0.0, 1.0);
  const double cx = std::cos(rx), sx = std::sin(rx), cy = std::cos(ry), sy = std::sin(ry);
  // the camera: the cover's axes and its normal, turned; the eye 4 h in front of it
  const double e1x = cy, e1y = 0, e1z = -sy;
  const double e2x = sy * sx, e2y = cx, e2z = cy * sx;
  const double nx = sy * cx, ny = -sx, nz = cy * cx;
  const double d = 4.0 * h;
  const double zoom = 1.0 + 0.025 * k;
  // the shadow: leans with a tilt, narrows with the cover as it turns right round
  const double sox = -std::clamp(ry, -0.11, 0.11) * h * 0.35, soy = h * 0.06 + rx * h * 0.35;
  const double shw = h * std::max(std::fabs(cy), 0.02) - h * 0.02, shh = h - h * 0.02;
  const double shadow = with_shadow ? (0.42 + 0.12 * k) * std::clamp(shadow_k, 0.0, 1.0) : 0.0;
  const double reach = h * 1.4, r = h * 0.02;
  const bool have = art.ok();
  const double scale = have ? art.side / (2.0 * h) : 1.0;  // the art's pixels per screen pixel of the flat cover
  auto box = [&](double qx, double qy) {
    const double dx = std::fabs(qx) - (h - r), dy = std::fabs(qy) - (h - r);
    const double ox = std::max(dx, 0.0), oy = std::max(dy, 0.0);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0) - r;
  };
  auto sample = [&](double u, double v, double rgb[3]) {
    // the cover's picture at (u, v) from its centre (screen px of the flat cover)
    double fx = (u + h) * scale - 0.5, fy = (v + h) * scale - 0.5;
    const int s = art.side;
    fx = std::clamp(fx, 0.0, s - 1.0); fy = std::clamp(fy, 0.0, s - 1.0);
    const int ix = std::min((int)fx, s - 2 < 0 ? 0 : s - 2), iy = std::min((int)fy, s - 2 < 0 ? 0 : s - 2);
    const double tx = s > 1 ? fx - ix : 0, ty = s > 1 ? fy - iy : 0;
    const int ix1 = std::min(ix + 1, s - 1), iy1 = std::min(iy + 1, s - 1);
    const uint32_t p00 = art.px[(size_t)iy * s + ix], p10 = art.px[(size_t)iy * s + ix1];
    const uint32_t p01 = art.px[(size_t)iy1 * s + ix], p11 = art.px[(size_t)iy1 * s + ix1];
    for (int c = 0; c < 3; ++c) {
      const int sh = 16 - c * 8;  // r, g, b
      const double top = ((p00 >> sh) & 255) * (1 - tx) + ((p10 >> sh) & 255) * tx;
      const double bot = ((p01 >> sh) & 255) * (1 - tx) + ((p11 >> sh) & 255) * tx;
      rgb[c] = (top * (1 - ty) + bot * ty) / 255.0;
    }
  };
  const int x0 = std::max(0, (int)std::floor(ccx - reach)), x1 = std::min(ow, (int)std::ceil(ccx + reach) + 1);
  const int y0 = std::max(0, (int)std::floor(ccy - reach)), y1 = std::min(oh, (int)std::ceil(ccy + reach) + 1);
  if (x1 <= x0 || y1 <= y0) return;
  // the shadow's falloff, looked up (an exp a pixel was most of the time)
  constexpr int kLut = 2048;
  constexpr double kLutEnd = 16.0;  // in tenths of h: past that the shadow is gone
  static thread_local std::vector<float> lut;
  if (lut.empty()) {
    lut.resize(kLut + 1);
    for (int i = 0; i <= kLut; ++i) lut[i] = (float)std::exp(-kLutEnd * i / kLut);
  }
  auto falloff = [&](double sd) {
    const double z = std::max(sd, 0.0) / (h * 0.10);
    if (z >= kLutEnd) return 0.0;
    return (double)lut[(int)(z * (kLut / kLutEnd))];
  };
  if (std::fabs(rx) < 1e-9) {
    // A turn about the upright axis alone (a song change; the tilt under the pointer is at rest):
    // each column of the window looks at one column of the cover, scaled. So the camera is worked
    // out once a column, and a pixel is only a lookup.
    const int n = x1 - x0;
    struct Col { bool ok; double u, vs, dx, sqx; int ix, ix1; uint32_t wx; };
    std::vector<Col> col(n);
    const int s = art.side;
    const uint32_t* ap = have ? art.px.data() : nullptr;
    for (int i = 0; i < n; ++i) {
      Col& c = col[i];
      const double qx = x0 + i + 0.5 - ccx;
      const double den = nx * qx + nz * d;
      c.ok = den > 1e-9 && std::fabs(qx) <= reach;
      c.sqx = std::fabs(qx - sox) - shw;
      if (!c.ok) continue;
      const double t = d * nz / den;
      c.u = (t * qx * e1x + (t * d - d) * e1z) / zoom;
      c.vs = t / zoom;
      c.dx = std::fabs(c.u) - (h - r);
      if (ap) {
        const double fx = std::clamp((c.u + h) * scale - 0.5, 0.0, s - 1.0);
        c.ix = std::min((int)fx, std::max(0, s - 2));
        c.ix1 = std::min(c.ix + 1, s - 1);
        c.wx = (uint32_t)std::clamp((int)((fx - c.ix) * 256 + 0.5), 0, 256);
      }
    }
    // two pixels blended by w (0..256), the channels two at a time
    auto lerp_px = [](uint32_t a, uint32_t b, uint32_t w) {
      const uint32_t iw = 256 - w;
      const uint32_t rb = (((a & 0x00FF00FFu) * iw + (b & 0x00FF00FFu) * w) >> 8) & 0x00FF00FFu;
      const uint32_t g = (((a & 0x0000FF00u) * iw + (b & 0x0000FF00u) * w) >> 8) & 0x0000FF00u;
      return rb | g;
    };
    const double inside = r - 0.5;   // the box's distance this far in: the cover entirely
    for (int y = y0; y < y1; ++y) {
      const double qy = y + 0.5 - ccy;
      if (std::fabs(qy) > reach) continue;
      const double sqy = std::fabs(qy - soy) - shh, my = std::max(sqy, 0.0);
      uint32_t* row = out + (size_t)y * ow + x0;
      for (int i = 0; i < n; ++i) {
        const Col& c = col[i];
        double cover = 0;
        uint32_t rgb = 0;
        if (c.ok) {
          const double v = c.vs * qy;
          const double dy = std::fabs(v) - (h - r);
          const double m = std::max(c.dx, dy);
          if (m <= inside) cover = 1;
          else if (m < r + 0.5) {
            double bd;
            if (c.dx > 0 && dy > 0) bd = std::sqrt(c.dx * c.dx + dy * dy) - r;
            else bd = m - r;
            cover = std::clamp(0.5 - bd, 0.0, 1.0);
          }
          if (cover > 0) {
            if (ap) {
              const double fy = std::clamp((v + h) * scale - 0.5, 0.0, s - 1.0);
              const int iy = std::min((int)fy, std::max(0, s - 2)), iy1 = std::min(iy + 1, s - 1);
              const uint32_t wy = (uint32_t)std::clamp((int)((fy - iy) * 256 + 0.5), 0, 256);
              const uint32_t* r0 = ap + (size_t)iy * s;
              const uint32_t* r1 = ap + (size_t)iy1 * s;
              rgb = lerp_px(lerp_px(r0[c.ix], r0[c.ix1], c.wx), lerp_px(r1[c.ix], r1[c.ix1], c.wx), wy);
            }
            if (k > 0) {
              const double rim = std::clamp((std::max(std::fabs(c.u), std::fabs(v)) - (h - 3.0)) / 3.0, 0.0, 1.0) * 0.14 * k;
              if (rim > 0) {
                const uint32_t add = (uint32_t)(rim * 255 + 0.5);
                uint32_t o = 0;
                for (int sh = 0; sh < 24; sh += 8) o |= std::min(255u, ((rgb >> sh) & 255) + add) << sh;
                rgb = o;
              }
            }
            if (cover < 1) {
              const uint32_t w = (uint32_t)(cover * 256 + 0.5);
              rgb = lerp_px(0, rgb, w);
            }
          }
        }
        if (cover >= 1) { row[i] = 0xFF000000u | rgb; continue; }
        if (!with_shadow) {
          if (cover > 0) row[i] = ((uint32_t)std::clamp((int)(cover * 255 + 0.5), 0, 255) << 24) | rgb;
          continue;
        }
        double sd;
        const double sqx = c.sqx;
        if (sqx > 0 && sqy > 0) sd = std::sqrt(sqx * sqx + my * my);
        else sd = std::max(sqx, sqy);
        const double a_shadow = shadow * falloff(sd - h * 0.02);
        const double a = cover + (1 - cover) * a_shadow;
        if (a <= 0.0005) continue;
        row[i] = ((uint32_t)std::clamp((int)(a * 255 + 0.5), 0, 255) << 24) | rgb;
      }
    }
    return;
  }
  for (int y = y0; y < y1; ++y) {
    const double qy = y + 0.5 - ccy;
    for (int x = x0; x < x1; ++x) {
      const double qx = x + 0.5 - ccx;
      if (std::max(std::fabs(qx), std::fabs(qy)) > reach) continue;
      // the cover where this pixel looks at it
      const double den = nx * qx + ny * qy + nz * d;
      double cover = 0, rgb[3] = {0, 0, 0};
      if (den > 1e-9) {
        const double t = d * nz / den;
        const double Px = t * qx, Py = t * qy, Pz = t * d - d;
        const double u = (Px * e1x + Py * e1y + Pz * e1z) / zoom, v = (Px * e2x + Py * e2y + Pz * e2z) / zoom;
        cover = std::clamp(0.5 - box(u, v), 0.0, 1.0);
        if (cover > 0) {
          if (have) sample(u, v, rgb);
          const double rim = std::clamp((std::max(std::fabs(u), std::fabs(v)) - (h - 3.0)) / 3.0, 0.0, 1.0) * 0.14 * k;
          for (double& c : rgb) c = std::min(1.0, c + rim);
        }
      }
      // the shadow's alpha (the room under it darkens by that much; under the cover, hidden)
      double a_shadow = 0;
      if (cover < 1 && with_shadow) {
        const double sqx = std::fabs(qx - sox) - shw, sqy = std::fabs(qy - soy) - shh;
        const double sd = std::sqrt(std::max(sqx, 0.0) * std::max(sqx, 0.0) + std::max(sqy, 0.0) * std::max(sqy, 0.0)) +
                          std::min(std::max(sqx, sqy), 0.0) - h * 0.02;
        a_shadow = shadow * falloff(sd);
      }
      const double a = cover + (1 - cover) * a_shadow;
      if (a <= 0.0005) continue;
      auto ch = [&](double c) { return (uint32_t)std::clamp((int)std::lround(c * cover * 255), 0, 255); };
      const uint32_t A = (uint32_t)std::clamp((int)std::lround(a * 255), 0, 255);
      out[(size_t)y * ow + x] = (A << 24) | (ch(rgb[0]) << 16) | (ch(rgb[1]) << 8) | ch(rgb[2]);
    }
  }
}

// The turn's timing, shared by the thread that draws it and the app that hands the songs over.
// Away with an ease-in (fastest at the edge), in from the other side with an ease-out that leaves
// the edge at that same speed, passes flat by a few degrees and lands.
struct CoverTurnTiming {
  static constexpr double kEdge = 1.5707963267948966;  // edge-on: the cover is a line, then nothing
  static constexpr double kOut = 0.20, kIn = 0.42, kBack = 1.2;
  // turning away from `from` (radians): k = 0..1 of kOut
  static double away(double from, double k) { k = std::clamp(k, 0.0, 1.0); return from + (kEdge - from) * k * k; }
  // turning in: k = 0..1 of kIn; from -kEdge to 0, a whisper past flat
  static double in(double k) {
    k = std::clamp(k, 0.0, 1.0);
    const double c1 = kBack, c3 = c1 + 1, x = k - 1;
    const double e = 1 + c3 * x * x * x + c1 * x * x;
    return -kEdge * (1 - e);
  }
};
