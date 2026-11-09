// A preview is one picture with its own timestamp, never a dissolve of two different shots.
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/util.h"
#include <cassert>
#include <cstdio>

int main() {
  PlayView p; p.in.dur = 300; p.resize(1280, 720, 2);
  double t = 1; p.reset(t);
  for (int i=0; i<60; ++i) { t+=1/60.; p.activity(t); p.tick(t,1/60.); }
  const Rect dock = p.dock_rect();
  auto hit = std::find_if(p.hots_.begin(), p.hots_.end(), [](const auto& h) { return h.id == PlayView::B_TIMELINE; });
  assert(hit != p.hots_.end());
  p.mouse_move(dock.x + hit->r.cx(), dock.y + hit->r.cy(), t);
  for (int i=0; i<40; ++i) { t+=1/60.; p.activity(t); p.tick(t,1/60.); }
  assert(p.tip_want() && p.layers[PlayView::L_TIP].visible);
  p.set_thumb(std::vector<uint32_t>(32, 0xFFFF0000u), 8, 4, 12.2);
  t+=1/60.; p.tick(t,1/60.);
  assert(p.tip_ts_ == fmt_time(12.2));
  // The very first paint of a newly decoded shot already contains only that shot.
  p.set_thumb(std::vector<uint32_t>(32, 0xFF0000FFu), 8, 4, 80.4);
  t+=1/60.; p.tick(t,1/60.);
  const Canvas& c = p.layers[PlayView::L_TIP].c;
  const int pad = 2 * p.s_;
  assert(c.px[(size_t)(pad+8)*c.w+pad+8] == 0xFF0000FFu);
  assert(p.tip_ts_ == fmt_time(80.4));
  // A later pointer move while decoding keeps the currently displayed picture's timestamp.
  p.mouse_move(dock.x + hit->r.cx()+20, dock.y + hit->r.cy(), t);
  t+=1/60.; p.tick(t,1/60.);
  assert(p.tip_ts_ == fmt_time(80.4));
  p.set_thumb({},0,0,-1);
  assert(p.thumb_.px.empty() && p.thumb_t_ == -1);
  const int tx = dock.x + hit->r.cx(), ty = dock.y + hit->r.cy();
  p.mouse_move(tx, ty, t);
  const uint64_t old_epoch = p.preview_epoch();
  assert(p.show_thumb(std::vector<uint32_t>(32, 0xFFFF0000u), 8, 4, 12.2, old_epoch));
  p.take_actions();
  // Leave and re-enter BEFORE another animation tick, at the identical timestamp.
  p.mouse_move(tx, ty - 100, t);
  assert(!p.preview_active());
  assert(!p.show_thumb(std::vector<uint32_t>(32, 0xFF00FF00u), 8, 4, 20, old_epoch));
  p.mouse_move(tx, ty, t);
  assert(p.preview_epoch() != old_epoch && p.thumb_.px.empty());
  const auto actions = p.take_actions();
  assert(std::any_of(actions.begin(), actions.end(), [](const auto& a) { return a.a == PA::Preview; }));
  // A delayed reply cannot bring the previous interaction back. A correct cache hit can.
  assert(!p.show_thumb(std::vector<uint32_t>(32, 0xFFFF0000u), 8, 4, 12.2, old_epoch));
  const uint64_t current = p.preview_epoch();
  assert(p.show_thumb(std::vector<uint32_t>(32, 0xFF0000FFu), 8, 4, 80.4, current));
  p.pointer_left();
  p.mouse_move(tx + 30, ty, t);
  assert(p.thumb_.px.empty() && p.preview_epoch() != current);
  t += 1/60.; p.tick(t,1/60.);
  assert(p.tip_ts_ != fmt_time(80.4));  // waiting UI uses the new destination, with no old picture
  const auto epoch = p.preview_epoch();
  assert(p.show_thumb(std::vector<uint32_t>(32, 0xFFFF0000u), 8, 4, 120, epoch));
  t += 1/60.; p.tick(t,1/60.);
  const int stable_w = p.layers[PlayView::L_TIP].c.w, stable_h = p.layers[PlayView::L_TIP].c.h;
  // Recorded regression: a continuous sweep must not collapse to a time-only tooltip
  // between decoder replies. Hold the picture and its own timestamp for the whole wait.
  for (int i = 0; i < 24; ++i) {
    t += 1/120.;
    p.mouse_move(tx + 32 + i, ty, t);
    p.tick(t,1/120.);
    assert(p.thumb_.w > 0 && p.preview_epoch() == epoch);
    assert(p.layers[PlayView::L_TIP].c.w == stable_w && p.layers[PlayView::L_TIP].c.h == stable_h);
    assert(p.tip_ts_ == fmt_time(120));
    assert(p.thumb_.px.front() == 0xFFFF0000u);
  }
  assert(p.show_thumb(std::vector<uint32_t>(32, 0xFF0000FFu), 8, 4, 124, epoch));
  t += 1/60.; p.tick(t,1/60.);
  assert(p.tip_ts_ == fmt_time(124));
  assert(p.layers[PlayView::L_TIP].c.w == stable_w && p.layers[PlayView::L_TIP].c.h == stable_h);
  // A NEW hover retains the card's dimensions, but absolutely none of its old pixels.
  p.pointer_left();
  p.mouse_move(tx + 60, ty, t);
  t += 1/60.; p.tick(t,1/60.);
  assert(p.preview_epoch() != epoch && p.thumb_.px.empty());
  const Canvas& waiting = p.layers[PlayView::L_TIP].c;
  assert(waiting.w == stable_w && waiting.h == stable_h);
  assert(waiting.px[(size_t)(pad+8)*waiting.w+pad+8] != 0xFF0000FFu);
  assert(!p.show_thumb(std::vector<uint32_t>(32, 0xFF0000FFu), 8, 4, 124, epoch));
  p.set_thumb({}, 0, 0, -1);
  assert(p.thumb_slot_w_ == 0 && p.thumb_slot_h_ == 0);
  puts("Hover frames: continuous card during a 200 ms sweep, stable dimensions, clean re-entry, timestamp pairing and no blends passed");
}
