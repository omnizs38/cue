#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

static void settle(PlayView& p, double& t) {
  for (int i = 0; i < 40; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); }
}

int main() {
  // a roomy window: About opens where Settings is, its right edge on Settings' (over "..."),
  // as wide, above the bar
  for (bool audio : {false, true}) {
    PlayView p;
    p.in.audio_only = audio; p.in.cover_scene = audio; p.in.dur = 120;
    p.resize(1600, 900, 2); double t = 1; p.reset(t);
    p.open_panel_for_preview(11, t); settle(p, t);
    const Rect st = p.panel_rect();
    for (int i = 0; i < 20; ++i) p.key("down", false, false, t);
    p.key("enter", false, false, t); settle(p, t);
    assert(p.panel_ == PlayView::Panel::About);
    const Rect ab = p.panel_rect(), d = p.dock_rect();
    assert(ab.w == st.w && std::abs(ab.r() - st.r()) <= 2 && ab.b() <= d.y);
  }
  for (bool audio : {false, true}) for (double scale : {1., 1.5, 2., 2.5, 3., 4.}) {
    PlayView p;
    const int w = (int)(400 * scale), h = (int)(220 * scale);
    p.in.audio_only = audio; p.in.dur = 120;
    p.resize(w, h, scale); double t = 1; p.reset(t);
    p.open_panel_for_preview(11, t);
    // The last item in Settings opens About with either Enter or the right arrow.
    for (int i = 0; i < 20; ++i) p.key("down", false, false, t);
    p.key(audio ? "enter" : "right", false, false, t);
    settle(p, t);
    const Layer& panel = p.layers[PlayView::L_PANEL];
    assert(panel.visible && panel.x >= 0 && panel.y >= 0);
    assert(panel.x + panel.c.w <= w && panel.y + panel.c.h <= h);
    // a page of Settings: where Settings was (over the "..."), as wide as Settings, clear of the bar
    const Rect d = p.dock_rect();
    assert(panel.c.w >= (int)std::lround(p.settings_sheet_w_ * p.panel_draw_s()) - 1);  // (at its own scale: a small window steps it down)
    // (clear of the bar when the window has the room above it; a very short window at a large
    // size has not, like Settings itself there)
    if (d.y - p.chrome_room() - 4 * scale >= panel.c.h) assert(panel.y + panel.c.h <= d.y);
    assert(panel.y >= 0 && panel.y + panel.c.h <= h);
    p.take_actions();
    // Static credits are skipped by keyboard navigation. The name opens LinkedIn.
    // The name opens LinkedIn; the contact row is one stop whose icons the arrows walk.
    p.key("down", false, false, t); p.key("enter", false, false, t);
    auto first = p.take_actions(); assert(first.size() == 1 && first[0].a == PA::OpenLinkedIn);
    p.key("down", false, false, t);
    for (PA expected : {PA::OpenLinkedIn, PA::OpenGitHub, PA::OpenEmail}) {
      p.key("enter", false, false, t);
      auto actions = p.take_actions();
      assert(actions.size() == 1 && actions[0].a == expected);
      p.key("right", false, false, t);
    }
    const PlayView::Row* contacts = nullptr;
    for (auto& r : p.rows_) if (r.k == PlayView::RK::Contacts) contacts = &r;
    assert(contacts && contacts->parts.size() == 3);
    for (int i = 0; i < 3; ++i) {
      const Rect& cell = contacts->parts[i];
      int x = panel.x + cell.cx(), y = panel.y + cell.y + cell.h / 2;
      p.mouse_move(x, y, t);
      assert(p.cursor_kind() == 1);
      p.mouse_down(x, y, t); p.mouse_up(x, y, t);
      auto actions = p.take_actions();
      assert(actions.size() == 1 && actions[0].a == (i == 0 ? PA::OpenLinkedIn : i == 1 ? PA::OpenGitHub : PA::OpenEmail));
    }
    assert(!p.in.paused);
    for (int i = 0; i < 8; ++i) p.key("down", false, false, t);
    p.key("up", false, false, t);  // the last row is the shortcut sheet's link; credits just above
    p.key("enter", false, false, t);
    auto license_actions = p.take_actions();
    assert(license_actions.size() == 1 && license_actions[0].a == PA::OpenLicenses);
    // the shortcut sheet opens from About, and Esc brings About back
    p.key("down", false, false, t); p.key("enter", false, false, t); settle(p, t);
    assert(p.panel_ == PlayView::Panel::Keys && p.take_actions().empty());
    p.key("esc", false, false, t); settle(p, t);
    assert(p.panel_ == PlayView::Panel::About);
    p.key("esc", false, false, t); settle(p, t);
    assert(p.panel_ == PlayView::Panel::Settings);  // About lives in Settings
    p.key("esc", false, false, t); settle(p, t);
    assert(p.panel_ == PlayView::Panel::More);
    p.key("esc", false, false, t); settle(p, t);
    assert(!p.panel_open());
    // An outside click dismisses About without pausing the content underneath.
    p.open_panel_for_preview(5, t); settle(p, t); p.take_actions();
    p.mouse_down(1, h / 2, t); p.mouse_up(1, h / 2, t);
    settle(p, t);
    assert(!p.panel_open() && p.take_actions().empty());
  }
  std::puts("About: menu navigation, centered layout, keyboard links, back, the shortcut sheet's link and outside dismissal passed for audio/video at six scales");
}
