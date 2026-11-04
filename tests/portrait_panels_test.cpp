// A portrait (or square) window raises the control bar to fill the width; its menus, the subtitle
// list and up next do not follow it: they keep the window's own scale, or a size that suits the
// width (a menu about half of it), never bigger than the bar and never past the window.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include "../src/core/ui_scale.h"
#include <cassert>
#include <cstdio>

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  int checked = 0;
  for (auto size : {std::pair{980, 1700}, std::pair{720, 1280}, std::pair{540, 960}, std::pair{1675, 1675}, std::pair{1080, 1080}})
    for (double base : {1.5, 2., 2.5, 3.})
      for (int which : {3, 0, 10}) {  // "...", subtitles, up next
        auto [W, H] = size;
        double bar = portrait_scale(base, W, H, 2);
        PlayView p; p.in.dur = 100; p.in.queue_active = true; p.in.queue = {{1, "a.mp4", true}, {2, "b.mp4", false}};
        Track a{1, "sub", "por", "", "subrip"}; p.in.subs = {a}; p.in.sid = 1; p.in.kind = SubKind::Text;
        p.set_base_scale(base);
        p.resize(W, H, bar);
        double t = 1; p.reset(t);
        if (which == 10) p.key("q", false, false, t); else p.open_panel_for_preview(which, t);
        for (int i = 0; i < 40; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); }
        double ps = p.panel_s();
        assert(ps <= bar && ps >= std::min(bar, base));
        Rect r = p.panel_rect();
        assert(r.x >= 0 && r.r() <= W && r.y >= 0 && r.b() <= H);
        if (which == 3 && bar > base) assert(r.w <= W * 0.8);  // the menu no longer spans the window
        if (which == 10 && p.compact_) {  // up next: the bar's own column (the whole window when the bar overflows it)
          Rect d = p.dock_rect();
          if (d.x >= 0 && d.r() <= W) assert(std::abs(r.x - d.x) <= 2 && std::abs(r.w - d.w) <= 2);
          else assert(r.x >= 0 && r.r() <= W);
        }
        ++checked;
      }
  // a landscape window: nothing changes (menus at the bar's own scale)
  PlayView l; l.set_base_scale(2); l.resize(1920, 1080, 2); assert(l.panel_s() == 2);
  // the portrait window of the report: the bar at 3.5, its menus at 3 (half way from the window's
  // 2.5), about as wide as the bar
  PlayView v; v.set_base_scale(2.5); v.resize(980, 1700, portrait_scale(2.5, 980, 1700, 2));
  assert(v.s_ == 3.5 && v.panel_s() == 3.0);
  std::printf("Portrait panels: menus, subtitles and up next half way between the window's scale and a raised bar's (%d windows) passed\n", checked);
}
