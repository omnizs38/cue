// The film's "up next" (1.4.1, the author's choice of two previews): the words say "up next" and,
// when the next file starts by itself, a 1 px line along the notice's foot runs out with the
// film's time left, as "skip intro"'s line does; it stands still while the film is paused, and a
// next file that waits to be asked for has no line.
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>
#include <cstdlib>

// the line's length on the staged notice: its row 3 art px above the foot, pixels that are lit
static int line_px(PlayView& p, int s) {
  const Layer& T = p.layers[PlayView::L_TOAST];
  if (!T.visible || T.c.h < 4 * s) return 0;
  const int y = T.c.h - 3 * s;
  int n = 0;
  for (int x = 0; x < T.c.w; ++x) {
    const uint32_t v = T.c.px[(size_t)y * T.c.w + x];
    const int g = (v >> 8) & 255;
    if (g > 70) ++n;  // (the dim ink on the tip's dark ground)
  }
  return n;
}
static void run(PlayView& p, double& t, double secs) {
  for (double k = 0; k < secs; k += 1 / 60.0) { t += 1 / 60.0; p.tick(t, 1 / 60.0); }
}

int main() {
  const int s = 2;
  PlayView p;
  p.in.dur = 1200; p.in.time = 1190; p.in.has_next = true;
  p.resize(1280, 720, s);
  double t = 1;
  p.reset(t); p.changed(t);
  p.set_next_details("Severance · S02E05");
  p.next_notice("up next", t, true);
  run(p, t, 0.4);
  const int full = line_px(p, s);
  assert(full > 0);
  // five seconds later the line is half as long (whole pixels)
  p.in.time = 1195; p.changed(t); run(p, t, 0.1);
  const int half = line_px(p, s);
  assert(half > 0 && half < full && std::abs(half * 2 - full) <= 2 * s + 2);
  // paused: the film's time stands, the line too
  run(p, t, 1.0);
  assert(line_px(p, s) == half);
  // the same words again a second later draw nothing new
  const uint64_t gen = p.toast_gen_;
  p.next_notice("up next", t, true);
  assert(p.toast_gen_ == gen);
  // at twice the speed it counts twice as fast
  p.in.speed = 2; p.changed(t); run(p, t, 0.1);
  const int fast = line_px(p, s);
  assert(fast < half);
  p.in.speed = 1;
  // a next file that waits to be asked for: the words, no line
  PlayView q;
  q.in.dur = 1200; q.in.time = 1195; q.in.has_next = true;
  q.resize(1280, 720, s);
  double u = 1;
  q.reset(u); q.changed(u);
  q.set_next_details("Severance · S02E05");
  q.next_notice("up next", u, false);
  run(q, u, 0.4);
  assert(q.layers[PlayView::L_TOAST].visible && line_px(q, s) == 0);
  // and it goes when told
  q.next_notice("", u);
  assert(!q.next_toast_ && !q.next_counting_);
  std::puts("Up next: a line that runs out with the film's time left (half at half, still while paused, faster at 2x), none when the next file waits, the same words drawing nothing new passed");
}
