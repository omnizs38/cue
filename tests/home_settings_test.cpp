// The home's "configurações": a line under "open a file" (the same column, never over the eyes,
// the list or the window's edge), and the player's own Settings panel rising from it, whole inside
// the window, never covering the line; back and Esc close it (there is no "..." under it on the
// home). Every language, the four interface sizes, small and large windows.
#define private public
#include "../src/ui/home.h"
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static bool overlap(const Rect& a, const Rect& b) { return a.x < b.r() && b.x < a.r() && a.y < b.b() && b.y < a.b(); }

int main() {
  int n = 0, covered = 0;
  for (int lang = 0; lang < i18n::kLangCount; ++lang) {
    i18n::set_language((i18n::Lang)lang);
    for (double s : {1.5, 2.0, 3.0, 4.0}) for (auto [aw, ah] : {std::pair{480, 300}, {640, 360}, {960, 540}, {760, 294}, {480, 240}}) {
      const int W = (int)(aw * s), H = (int)(ah * s);
      HomeView hv; hv.set_size(W, H, s);
      hv.set_rows({{"Twin Peaks", "faltam 12 min", 0.62, "S01E04", false, HomeKind::Episode, "C:\\v\\tp.mkv"},
                   {"Cena", "", -1, "", false, HomeKind::Video, "C:\\v\\c.mkv"}});
      double t = 10; for (int i = 0; i < 20; ++i) { t += 1 / 60.0; hv.tick(t, 1 / 60.0); }
      const Rect r = hv.settings_rect();
      assert(r.w > 0 && r.x >= 0 && r.r() <= W && r.b() <= H);
      assert(r.y >= hv.r_open_.b() && r.x == hv.r_open_.x);   // under "open a file", the same column
      assert(!overlap(r, hv.face.bounds()) && !overlap(r, hv.r_list_));
      // the click and the keyboard reach it
      HomeAction a = hv.mouse_down(r.cx(), r.cy(), t);
      assert(a.a == HomeAct::Settings);
      // the panel over it
      PlayView p; p.in.dur = 0; double u = 1; p.resize(W, H, s); p.reset(u);
      p.open_settings_over_home(hv.settings_anchor(), u);
      for (int i = 0; i < 90; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      const Layer& L = p.layers[PlayView::L_PANEL];
      assert(p.over_home() && L.visible);
      assert(L.x >= 0 && L.y >= 0 && L.x + L.c.w <= W && L.y + L.c.h <= H);
      const Rect pr{L.x, L.y, L.c.w, L.c.h};
      if (pr.b() <= r.y || pr.y >= r.b() || pr.x >= std::max(r.r(), hv.r_open_.r())) {} else { std::printf("covers the line: lang %d s %.1f %dx%d (panel %d..%d, line %d..%d)\n", lang, s, aw, ah, pr.y, pr.b(), r.y, r.b()); assert(false); }
      // (1.4.1) standing over any of the list, the list steps out whole (no title cut under the
      // panel, no end of a name beside it); clear of it, the list stays; closed, it comes back
      {
        const Rect lr = hv.list_area(), op = p.over_home_rect();
        assert(op.w == pr.w && op.h == pr.h);
        const bool over = overlap(op, lr);
        hv.cover_list(over);
        for (int i = 0; i < 30; ++i) { t += 1 / 60.0; hv.tick(t, 1 / 60.0); }
        Canvas c; hv.render(c, t);
        const uint32_t bg = c.px[0];  // (the window's corner: the background)
        bool any = false;
        for (int y = std::max(0, lr.y); y < std::min(H, lr.b()); ++y)
          for (int x = std::max(0, lr.x); x < std::min(W, lr.r()); ++x) any |= c.px[(size_t)y * W + x] != bg;
        if (over == any) { std::printf("list under the panel: lang %d s %.1f %dx%d over %d drawn %d\n", lang, s, aw, ah, over, any); assert(false); }
        hv.cover_list(false);
        for (int i = 0; i < 40; ++i) { t += 1 / 60.0; hv.tick(t, 1 / 60.0); }
        hv.render(c, t);
        any = false;
        for (int y = std::max(0, lr.y); y < std::min(H, lr.b()); ++y)
          for (int x = std::max(0, lr.x); x < std::min(W, lr.r()); ++x) any |= c.px[(size_t)y * W + x] != bg;
        assert(any);
        covered += over;
      }
      p.key("esc", false, false, u);
      assert(!p.over_home());
      p.open_settings_over_home(r, u); for (int i = 0; i < 30; ++i) { u += 1 / 60.0; p.tick(u, 1 / 60.0); }
      p.key("esc", false, false, u);  // (the same from a second opening)
      assert(!p.over_home() && p.take_actions().size() < 100);
      ++n;
    }
  }
  assert(covered > 0 && covered < n);  // (both cases met)
  std::printf("home_settings_test: ok (%d, the list stepping out under the panel in %d)\n", n, covered);
}
