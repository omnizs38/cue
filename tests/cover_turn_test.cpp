// The card's turning cover drawn by Cue (core/cover_turn.h): what it draws where the engine's
// cover pass would draw the cover, and the turn's timing.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "../src/core/cover_turn.h"

static int alpha(uint32_t v) { return (int)(v >> 24); }

int main() {
  // a square cut from a wide picture: the middle, scaled to the side; the bars' crop first
  {
    std::vector<uint32_t> src(400 * 200);
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 400; ++x) src[(size_t)y * 400 + x] = x < 100 || x >= 300 ? 0xFFFF0000u : 0xFF00FF00u;
    CoverArt a = cover_art_square(src.data(), 400, 200, 50);
    assert(a.ok() && a.side == 50);
    for (uint32_t v : a.px) assert(v == 0xFF00FF00u);  // only the green middle
    // bars: the left quarter cut away, the square from what is left (centred)
    CoverArt b = cover_art_square(src.data(), 400, 200, 20, 0.25, 0, 0.75, 1);
    assert(b.ok() && b.px[(size_t)10 * 20 + 10] == 0xFF00FF00u);
  }
  // Cue's tile: near-black with a light note
  {
    CoverArt t = cover_art_tile(52);
    assert(t.ok());
    int light = 0;
    for (uint32_t v : t.px) light += v == 0xFFD2D2D2u;
    assert(light > 52 * 52 / 50 && light < 52 * 52 / 8);
  }
  CoverArt art;
  art.side = 100;
  art.px.assign(100 * 100, 0xFF336699u);
  const int W = 300, H = 300;
  std::vector<uint32_t> out((size_t)W * H);
  // flat: the cover exactly where the engine draws it (100 px around the centre), opaque, its own
  // colours; nothing past it when the shadow is left to the engine
  cover_turn_draw(art, out.data(), W, H, 150, 150, 50, 0, 0, 0, 0.0);
  assert(out[(size_t)150 * W + 150] == 0xFF336699u);
  assert(out[(size_t)101 * W + 101] == 0xFF336699u && out[(size_t)198 * W + 198] == 0xFF336699u);
  assert(alpha(out[(size_t)150 * W + 98]) == 0 && alpha(out[(size_t)150 * W + 202]) == 0);
  assert(alpha(out[(size_t)97 * W + 150]) == 0 && alpha(out[(size_t)203 * W + 150]) == 0);
  // with the shadow: below the cover the room darkens (the shader's 0.42, a little down)
  cover_turn_draw(art, out.data(), W, H, 150, 150, 50, 0, 0, 0, 1.0);
  assert(alpha(out[(size_t)203 * W + 150]) > 40 && alpha(out[(size_t)203 * W + 150]) < 120);
  assert(alpha(out[(size_t)10 * W + 150]) == 0);  // past 1.4 h: nothing
  // turned 60 degrees: narrower; the near side taller than the far one
  cover_turn_draw(art, out.data(), W, H, 150, 150, 50, 0, 1.047, 0, 0.0);
  int left = W, right = 0;
  for (int x = 0; x < W; ++x) if (alpha(out[(size_t)150 * W + x]) > 128) { left = std::min(left, x); right = std::max(right, x); }
  assert(right - left > 35 && right - left < 75);
  auto height = [&](int x) { int n = 0; for (int y = 0; y < H; ++y) n += alpha(out[(size_t)y * W + x]) > 128; return n; };
  assert(std::abs(height(left + 2) - height(right - 2)) > 10);
  // edge-on: nothing at all
  cover_turn_draw(art, out.data(), W, H, 150, 150, 50, 0, CoverTurnTiming::kEdge, 0, 0.0);
  for (uint32_t v : out) assert(alpha(v) <= 2);
  // the quick path (a turn about the upright axis) draws what the general camera draws
  {
    std::vector<uint32_t> a((size_t)W * H), b((size_t)W * H);
    for (int i = 0; i < 100 * 100; ++i) art.px[i] = 0xFF000000u | ((i % 100) * 2 << 16) | ((i / 100) * 2 << 8) | 90;
    for (double ry : {0.0, 0.3, -0.9, 1.4}) {
      cover_turn_draw(art, a.data(), W, H, 150.3, 149.6, 50, 0, ry, 0.5, 1.0);
      cover_turn_draw(art, b.data(), W, H, 150.3, 149.6, 50, 2e-9, ry, 0.5, 1.0);
      int worst = 0;
      for (size_t i = 0; i < a.size(); ++i)
        for (int sh = 0; sh < 32; sh += 8) worst = std::max(worst, std::abs((int)((a[i] >> sh) & 255) - (int)((b[i] >> sh) & 255)));
      assert(worst <= 3);
    }
  }
  // the timing: away from flat to the edge, in from the other edge to flat, past it by a hair
  assert(CoverTurnTiming::away(0, 0) == 0 && std::fabs(CoverTurnTiming::away(0, 1) - CoverTurnTiming::kEdge) < 1e-12);
  assert(std::fabs(CoverTurnTiming::in(0) + CoverTurnTiming::kEdge) < 1e-12 && std::fabs(CoverTurnTiming::in(1)) < 1e-12);
  double past = 0;
  for (int i = 0; i <= 100; ++i) past = std::max(past, CoverTurnTiming::in(i / 100.0));
  assert(past > 0.04 && past < 0.15);  // about 5 degrees past flat
  // one movement: the speed into the edge is the speed out of it
  const double v_out = (CoverTurnTiming::away(0, 1) - CoverTurnTiming::away(0, 0.999)) / (0.001 * CoverTurnTiming::kOut);
  const double v_in = (CoverTurnTiming::in(0.001) - CoverTurnTiming::in(0)) / (0.001 * CoverTurnTiming::kIn);
  assert(std::fabs(v_out - v_in) / v_out < 0.05);
  std::puts("Cover turn: the cover where the engine draws it, its shadow, narrowing, edge-on, the quick path and the timing passed");
}
