// Where the bar stands, and who can move it:
//  - "without background": a drag that starts on the bar's empty places moves it (a click there
//    is still a click on the picture), the same anchors as the bar with a background;
//  - Shift held while it is dragged: the bar's centre stays on the window's middle (pressed
//    mid-drag, at once, with the pointer still), it moves only up and down and lands centred;
//  - a narrow window (a phone video): left, centre and right are a few px apart, so a drag there
//    can only centre the bar (a corner chosen there threw it to the side once the window widened);
//  - when the bar's width changes in place (a song's framing), centred stays centred;
//  - music framed close: the compact bar is raised to a size that reads in the window, and the
//    picture reaches the window's left edge (no dark band) in wide, short windows.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/audio_mode.h"
#include "../src/core/ui_scale.h"
#include "../src/ui/anim.h"
#include <fstream>
#include <sstream>
#include <tuple>
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdio>

static void settle(PlayView& p, double& t, int n = 90) {
  for (int i = 0; i < n; ++i) { t += 1 / 60.0; p.activity(t); p.tick(t, 1 / 60.0); p.take_actions(); }
}

int main() {
  // 1. without background: drag from an empty place of the bar moves it to a corner
  {
    PlayView p;
    p.in.dur = 300; p.in.controls_free = true;
    p.resize(1600, 900, 2); double t = 1; p.reset(t); settle(p, t);
    Rect d = p.dock_rect();
    // an empty place: inside the bar, on no button
    int ex = -1, ey = d.y + d.h / 2;
    for (int x = d.x + 2; x < d.x + d.w - 2 && ex < 0; ++x) {
      bool on = false;
      for (auto& h : p.hots_) if (h.r.contains(x - d.x, ey - d.y)) on = true;
      if (!on) ex = x;
    }
    assert(ex >= 0);
    // a click there: no drag, a click on the picture
    p.mouse_down(ex, ey, t); assert(p.free_dock_grab_ && !p.dragging_dock_);
    p.mouse_up(ex, ey, t); assert(!p.free_dock_grab_ && !p.dragging_dock_);
    settle(p, t, 30);
    // a drag: the bar follows, and lands bottom-left
    t += 1; p.mouse_down(ex, ey, t); assert(p.free_dock_grab_);
    for (int i = 1; i <= 40; ++i) { t += 1 / 60.0; p.mouse_move(ex - i * 30, ey, t); }
    assert(p.dragging_dock_);
    p.mouse_up(ex - 1200, ey, t);
    assert(!p.dragging_dock_ && p.place.anchor == DockAnchor::BottomLeft);
    p.take_actions();
  }
  // 1b. Shift: the bar's centre on the window's middle, at once, and it lands there
  for (bool free_style : {false, true})
    for (auto [W, H, sc] : {std::tuple{1600, 900, 2.0}, std::tuple{2560, 1440, 3.5}, std::tuple{1280, 720, 1.5}}) {
      PlayView p;
      p.in.dur = 300; p.in.controls_free = free_style;
      p.resize(W, H, sc); double t = 1; p.reset(t); settle(p, t);
      Rect d = p.dock_rect();
      int gx = -1, gy = d.y + d.h / 2;
      for (int x = d.x + 2; x < d.x + d.w - 2 && gx < 0; ++x) {
        bool on = false;
        for (auto& h : p.hots_) if (h.r.contains(x - d.x, gy - d.y)) on = true;
        if (!on) gx = x;
      }
      assert(gx >= 0);
      p.mouse_down(gx, gy, t);
      // off to the left and up, Shift not held: the bar goes with the pointer
      int x = gx, y = gy;
      for (int i = 1; i <= 30; ++i) { t += 1 / 60.0; x = gx - i * (W / 120); y = gy - i * (H / 90); p.mouse_move(x, y, t); }
      assert(p.dragging_dock_);
      Rect off = p.dock_rect();
      assert(off.x < d.x - 40 && off.y < d.y - 40);  // (a wide bar has little room to the side)
      // Shift pressed with the pointer still: centred at once
      assert(p.set_shift(true));
      Rect c = p.dock_rect();
      assert(std::abs(c.x + c.w / 2.0 - W / 2.0) <= 1 && c.y == off.y);
      // moved about with it held: still centred, following up and down
      for (int i = 1; i <= 20; ++i) { t += 1 / 60.0; p.mouse_move(x + i * (W / 60), y + i * 3, t); }
      Rect c2 = p.dock_rect();
      assert(std::abs(c2.x + c2.w / 2.0 - W / 2.0) <= 1 && c2.y > c.y);
      // let go of the bar: it lands centred (a centred anchor or a free place in the middle)
      p.mouse_up(x + 20 * (W / 60), y + 60, t);
      assert(!p.dragging_dock_);
      assert(p.place.anchor != DockAnchor::BottomLeft && p.place.anchor != DockAnchor::BottomRight);
      if (p.place.anchor == DockAnchor::Free) assert(std::fabs(p.place.fx - 0.5) < 1e-9);
      p.set_shift(false);
      settle(p, t);
      Rect landed = p.dock_rect();
      assert(std::abs(landed.x + landed.w / 2.0 - W / 2.0) <= 1);
      // Shift with no drag: nothing moves
      assert(!p.set_shift(true));
      p.take_actions();
    }
  // 2. a narrow window: a drag to the left still means centred
  {
    PlayView p;
    p.in.dur = 300;
    p.resize(540, 960, 3); double t = 1; p.reset(t); settle(p, t);
    Rect d = p.dock_rect();
    assert(p.side_room() < 24 * p.s_);
    int x0 = d.x + 1, y0 = d.y + 1;  // the bar's own edge (its background)
    p.mouse_down(x0, y0, t);
    for (int i = 1; i <= 20; ++i) { t += 1 / 60.0; p.mouse_move(x0 - i * 4, y0 + 2, t); }
    p.mouse_up(x0 - 80, y0 + 2, t);
    assert(p.place.anchor != DockAnchor::BottomLeft && p.place.anchor != DockAnchor::BottomRight);
    if (p.place.anchor == DockAnchor::Free) assert(std::fabs(p.place.fx - 0.5) < 1e-9);
    // widened to a landscape window: the bar is in the middle, not in a corner
    p.resize(2080, 1640, 3); settle(p, t);
    Rect w = p.dock_rect();
    assert(std::abs(w.x + w.w / 2 - 1040) <= 2);
  }
  // 3. a song's framing changes the bar's width in place: centred stays centred
  {
    PlayView p;
    p.in.dur = 250; p.in.audio_only = true; p.in.cover_scene = true; p.in.has_next = p.in.has_prev = true;
    p.in.music_scene = MusicScene::wide();
    const int W = 1140, H = 710; const double s = 2.5;
    p.in.music_view = music_view(p.in.music_scene, MusicFrame::Scene, W, H, H - 100);
    p.resize(W, H, s); double t = 1; p.reset(t); settle(p, t);
    p.in.music_view = music_view(p.in.music_scene, MusicFrame::Cover, W, H, H - 100);  // the frame changes, the size does not
    p.changed(t);  // what the app does after framing the song
    settle(p, t);
    Rect d = p.dock_rect();
    assert(p.music_compact_ && std::abs(d.x + d.w / 2 - W / 2) <= 1);
  }
  // 4. the compact music bar reads in the window: a small window's shrunken scale is given back
  //    the screen's standard size (never more: a bigger window gets the size the wide
  //    composition shows), within 80% of the width and a quarter of the height
  {
    for (auto [w, h, dpi] : std::vector<std::tuple<int, int, int>>{{1140, 710, 240}, {1640, 710, 240}, {600, 450, 144}, {800, 800, 96}, {1710, 1420, 144}, {700, 450, 144}}) {
      const double s = ui_scale(dpi, w, h, kDefaultUiSize), base = ui_base_scale(dpi, kDefaultUiSize);
      const double m = music_compact_scale(s, w, h, kDefaultUiSize, base);
      assert(m >= s && m <= std::max(s, base) && std::fmod(m * 2, 1.0) == 0);
      if (m > s) assert(204 * m <= w * 0.8 + 1e-9 && 38 * m <= h / 4.0 + 1e-9);
    }
    // the case that looked too big: a 1710 x 1420 window at 150% keeps the standard 2.5
    assert(music_compact_scale(ui_scale(144, 1710, 1420, kDefaultUiSize), 1710, 1420, kDefaultUiSize, 2.5) == 2.5);
    // and a small landscape window at 150% gets the standard size back
    assert(music_compact_scale(2.0, 700, 450, kDefaultUiSize, 2.5) == 2.5);
  }
  // 5. close framings of a song reach the window's left edge (no dark band) in wide, short windows
  {
    const MusicScene S = MusicScene::wide();
    for (auto [w, h] : std::vector<std::pair<int, int>>{{1640, 710}, {1140, 710}, {1400, 600}, {900, 500}, {600, 450}}) {
      MusicView v = music_view(S, MusicFrame::Cover, w, h, h - 120);
      assert(v.ox <= 0.5 && v.ox + S.W * v.f >= w - 0.5);
      double cx = v.ox + (S.x + S.side / 2.0) * v.f;
      assert(std::fabs(cx - w / 2.0) <= w * 0.10 + 0.5);                          // at most a little right of the middle
      assert(v.ox + S.left_x() * v.f >= 0 && v.ox + (S.right_x() + S.wing_w) * v.f <= w);  // wings whole
    }
  }
  // 7. music framed close, and a song's mini player: the cover, its words and the bar as one
  //    group a little above the middle; the bar right under the words (no hole down to the
  //    window's floor), centred on the cover, the timeline above previous · play · next
  for (auto [W, H, mini] : std::vector<std::tuple<int, int, bool>>{{600, 1185, false}, {765, 985, false}, {700, 450, false}}) {
    PlayView p;
    p.in.dur = 189; p.in.time = 100; p.in.audio_only = true; p.in.cover_scene = true; p.in.queue_active = true;
    p.in.has_next = p.in.has_prev = true; p.in.mini_player = mini;
    for (int i = 0; i < 9; ++i) p.in.queue.push_back(QueueEntry{});
    p.in.music_title = "Crimson"; p.in.music_artist = "Hunnygloss";
    p.in.music_scene = MusicScene::wide();
    const MusicFrame fr = mini ? MusicFrame::Cover : music_frame_for(W, H, 1.5);
    const double sc = std::max(portrait_scale(1.5, W, H, kDefaultUiSize), music_compact_scale(1.5, W, H, kDefaultUiSize, 1.5));
    p.in.music_view = music_view(p.in.music_scene, fr, W, H, H);
    p.resize(W, H, sc); double t = 1; p.reset(t);
    for (int i = 0; i < 60; ++i) {
      t += 1 / 60.0;
      if (p.music_stacked()) p.in.music_view = music_view(p.in.music_scene, fr, W, H, H, p.music_stack_below(), p.music_stack_above());
      p.changed(t); p.activity(t); p.tick(t, 1 / 60.0); p.take_actions(); p.prepare(t);
    }
    assert(p.music_stacked() && p.music_compact_ && p.dh_ == (int)std::lround(38 * p.s_));
    Rect cr = p.cover_screen_rect(), d = p.dock_rect();
    assert(std::abs(d.x + d.w / 2 - cr.cx()) <= 2);                         // under the cover's middle
    assert(d.y > cr.b() && d.b() <= H);                                       // under the cover, inside
    const double words = p.stack_words_h();
    assert(d.y - cr.b() <= (words > 0 ? words + 34 * p.s_ : 22 * p.s_) + 2);  // right under the words
    const double top = cr.y - p.music_stack_above(), bottom = H - d.b();
    assert(top >= 0 && bottom >= top * 0.8);                                  // the group a little above the middle
    assert(p.tl_.y < 16 * p.s_);                                              // the timeline is the first row
    for (int id : {PlayView::B_PREV, PlayView::B_PLAY, PlayView::B_NEXT, PlayView::B_VOL, PlayView::B_MORE}) assert(p.in_bar(id));
    assert(p.pinned());                                                       // part of the composition: it stays
  }
  // 9. a song's mini player is a card: the cover a square on the left, the words at the top of
  //    the column beside it, the bar at the column's foot level with the cover's bottom (the
  //    timeline first, then volume · previous · play · next · "..."), no bar background, the times
  //    shown, the card's controls always there
  for (auto [W, H, sc] : std::vector<std::tuple<int, int, double>>{{720, 380, 1.5}, {960, 506, 2.5}, {600, 316, 1.5}, {1280, 674, 3}}) {
    PlayView p;
    p.in.dur = 192; p.in.time = 100; p.in.audio_only = true; p.in.cover_scene = true; p.in.queue_active = true;
    p.in.has_next = p.in.has_prev = true; p.in.mini_player = true;
    for (int i = 0; i < 9; ++i) p.in.queue.push_back(QueueEntry{});
    p.in.music_title = "DNA"; p.in.music_artist = "Number Teddie";
    p.in.music_scene = MusicScene::wide();
    p.resize(W, H, sc); double t = 1; p.reset(t);
    for (int i = 0; i < 40; ++i) {
      t += 1 / 60.0;
      Rect c = p.card_cover(); MusicView v; v.frame = MusicFrame::Card; v.f = c.w / 520.0; v.ox = c.x - MusicScene::wide().x * v.f; v.oy = c.y - MusicScene::wide().y * v.f;
      p.in.music_view = v; p.changed(t); p.activity(t); p.tick(t, 1 / 60.0); p.take_actions(); p.prepare(t);
    }
    assert(p.music_card() && !p.music_stacked() && p.mini_ && p.music_compact_);
    Rect c = p.card_cover(), d = p.dock_rect(), cr = p.cover_screen_rect();
    assert(c.w == c.h && c.x > 0 && c.r() < W / 2 && std::abs(cr.x - c.x) <= 1 && std::abs(cr.w - c.w) <= 1);  // the engine's cover is the card's
    assert(d.x >= c.r() && d.r() <= W && d.b() <= H && std::abs(d.b() - c.b()) <= 4 * sc);                     // beside it, level with its foot
    // the playback over the timeline, the volume and "..." under it; the time ends where "..." ends
    {
      Rect play{}, vol{}, more{};
      for (auto& h : p.hots_) { if (h.id == PlayView::B_PLAY) play = h.r; if (h.id == PlayView::B_VOL) vol = h.r; if (h.id == PlayView::B_MORE) more = h.r; }
      assert(play.b() <= p.tl_.y && p.tl_.y < vol.y && std::abs(vol.y - more.y) <= 1);
    }
    for (int id : {PlayView::B_PREV, PlayView::B_PLAY, PlayView::B_NEXT, PlayView::B_VOL, PlayView::B_MORE}) assert(p.in_bar(id));
    assert(!p.in_bar(PlayView::B_EXPAND) && p.pinned());
    const Layer& M = p.layers[PlayView::L_MUSIC];
    assert(M.visible && M.x >= c.r() && M.y + M.c.h <= d.y);                                                  // the words above the bar
  }
  // 10. the card's list: the same list as beside a wide cover, smaller, in the words' column (the
  //     words step aside), above the bar; the card stays a card while a song changes (its cover's
  //     picture is down for a moment); no tilt of the cover under the pointer in the mini player
  {
    PlayView p;
    p.in.dur = 192; p.in.time = 100; p.in.audio_only = true; p.in.cover_scene = true; p.in.cover_art = true; p.in.cover_tilt = true;
    p.in.queue_active = true; p.in.has_next = p.in.has_prev = true; p.in.mini_player = true;
    for (int i = 0; i < 9; ++i) p.in.queue.push_back(QueueEntry{});
    p.in.music_title = "Ice Palace Adolescence"; p.in.music_artist = "";
    p.in.music_scene = MusicScene::wide();
    p.resize(960, 506, 2.5); double t = 1; p.reset(t);
    auto frame = [&] {
      Rect c = p.card_cover(); MusicView v; v.frame = MusicFrame::Card; v.f = c.w / 520.0; v.ox = c.x - MusicScene::wide().x * v.f; v.oy = c.y - MusicScene::wide().y * v.f;
      p.in.music_view = v; p.changed(t); p.activity(t); t += 1 / 60.0; p.tick(t, 1 / 60.0); p.take_actions(); p.prepare(t);
    };
    for (int i = 0; i < 30; ++i) frame();
    Rect c = p.card_cover();
    p.mouse_move(c.cx(), c.cy(), t); frame();
    assert(!p.cov_hot_);  // no tilt
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t);
    for (int i = 0; i < 40; ++i) frame();
    assert(p.scene_list());
    Rect L = p.scene_list_rect(), d = p.dock_rect();
    // (the list takes the column down to the timeline; the playback stepped into the foot's row)
    assert(L.x > c.x + c.w / 2 && L.r() <= 960 && L.b() <= d.y + p.tl_.y - (int)(6 * p.s_) && L.y >= p.card_col_.y);
    {
      Rect play{}, vol{};
      for (auto& h : p.hots_) { if (h.id == PlayView::B_PLAY) play = h.r; if (h.id == PlayView::B_VOL) vol = h.r; }
      assert(std::abs(play.cy() - vol.cy()) <= (int)p.s_ && play.y > p.tl_.y);
    }
    assert(p.scene_list_scale() <= p.s_ && p.scene_list_scale() >= 1);
    p.close_panel();
    p.in.cover_scene = false; p.changed(t); frame();  // a song change, for a moment
    assert(p.music_card() && p.mini_ && p.music_compact_);
  }
  // 11. the cover stands in the middle of the picture, across: centred in a roomy window, the
  // picture reaches as far on its left as on its right (the room's light is the same both sides)
  {
    const MusicScene S = MusicScene::wide();
    assert(std::abs(S.x + S.side / 2.0 - S.W / 2.0) < 1e-9);
    for (auto [w, h] : std::vector<std::pair<int, int>>{{1508, 852}, {1600, 900}, {1920, 1080}, {2560, 1080}}) {
      MusicView v = music_view(S, MusicFrame::Centre, w, h, h - 180, 120, 52);
      const double left = -v.ox, right = v.ox + S.W * v.f - w;
      assert(std::fabs(left - right) < 1.0);
    }
  }
  // 8. the mini player's flight lands with a little life (passes its mark by under 4%, settles)
  {
    double peak = 0;
    for (int i = 0; i <= 100; ++i) peak = std::max(peak, bounce_ease(i / 100.0));
    assert(peak > 1.01 && peak < 1.04 && bounce_ease(0) == 0 && bounce_ease(1) == 1 && std::fabs(bounce_ease(0.97) - 1) < 0.01);
  }
  // 6. cue.log: written in batches, kept small, cleared after 5 days; routine steps only with --log
  {
    std::ifstream f("src/win/app.cpp"); std::stringstream ss; ss << f.rdbuf(); std::string app = ss.str();
    assert(app.find("void cue_log_open(") != std::string::npos && app.find("today - day > 5") != std::string::npos);
    assert(app.find("kLogCap = 256 * 1024") != std::string::npos && app.find("f.flush()") == std::string::npos);
    assert(app.find("trace(\"first paint\")") != std::string::npos && app.find("log(\"first paint\")") == std::string::npos);
    assert(app.find("cue_log_flush(); }") != std::string::npos);  // the minute's write
    // the mini player: on top only when pinned (remembered), let go where the person wants it,
    // a corner only right beside one; no Snap resizing it while dragged
    assert(app.find("set_pin(store_.mini_pinned)") != std::string::npos && app.find("set_pin(true)") == std::string::npos);
    assert(app.find("void App::mini_settle()") != std::string::npos && app.find("if (mini_player_) mini_settle();") != std::string::npos);
    assert(app.find("case WM_WINDOWPOSCHANGING:") != std::string::npos && app.find("~(LONG_PTR)WS_MAXIMIZEBOX") != std::string::npos);
    // smoothing really happens: a film with it on is timed by the display (the engine ignores
    // the blend otherwise), whatever its size; never slowed by Windows while playing
    assert(app.find("smooth_on_ ? \"display-resample\"") != std::string::npos && app.find("playback_priority(true)") != std::string::npos);
    // a jump in the sound breathes out and back in (no pop), and the sound's queue is short
    // enough that a jump does not stall; the light around the window leaves a resize at once,
    // and a move keeps it; a playing window stops where it still looks right
    assert(app.find("seek_soft({\"seek\", std::to_string(a.v), \"absolute+exact\"})") != std::string::npos);
    assert(app.find("o(\"audio-buffer\", \"0.25\")") != std::string::npos);
    assert(app.find("!resizing_ &&") != std::string::npos && app.find("ambient_.hold(in_sizemove_)") != std::string::npos);
    assert(app.find("(song ? 260 : 320) * bs") != std::string::npos);
    // the flight into the mini player: the size changes once, under the pixels, then only the
    // place moves (a resize on every frame left the engine's picture a frame behind); the way
    // back takes its place at once and comes out of the pixels
    // the flight in three steps: pixels where it is, the size changed out of sight, then landing
    assert(app.find("if (!start_mini_fx(true)) mini_switch(true);") != std::string::npos);
    assert(app.find("if (!animate || !start_mini_fx(false)) { end_mini_fx(); mini_switch(false); }") != std::string::npos);
    assert(app.find("    set_cloak(true);\n    mini_fx_.cloaked = true;\n    geom_.on = false;\n    mini_switch(mini_fx_.entering);") != std::string::npos);
    assert(app.find("SetWindowPos(hwnd_, nullptr, cx - w / 2, cy - h / 2, w, h") != std::string::npos);
    assert(app.find("SetWindowPlacement(hwnd_, &mini_restore_);  // its old place and size") != std::string::npos);
    // ...out of sight (cloaked) while it changes size: the last big frame never shows squeezed
    assert(app.find("set_cloak(true);") != std::string::npos && app.find("if (cloaked_) return false;") != std::string::npos);
    // a song's card stays the card from song to song (no fall back to the spectrum in between)
    assert(app.find("music_frame_ = mini_player_ && !fullscreen_ ? MusicFrame::Card : music_frame_for(cw_, ch_, s_);") != std::string::npos);
    // a film's mini player never smaller than a watchable size; a song's card in its range, at
    // every interface size (see card_size_test: its pixels step down at the large sizes)
    assert(app.find("(audio_only_ ? (smallest ? 440 : 640) : (smallest ? 480 : 800)) * k") != std::string::npos);
    assert(app.find("if (mini_player_) return mini_scale(dpi_, cw, ch, choice, audio_only_);") != std::string::npos);
    assert(app.find("if (mini_player_) return s_;") != std::string::npos);
    // a new interface scale is laid out like a new size, music's framing too (the words gone
    // under a compact bar after "maior" to "média")
    assert(app.find("void App::set_ui_scale(int v)") < app.find("  rescale();  // (the same base scale too"));
    {
      const size_t r = app.find("void App::relayout() {");
      assert(r != std::string::npos && app.find("music_fit_window();", r) < app.find("\n}\n", r));
    }
    // a song picked in the open list: its first framing knows its frame (the list beside)
    assert(app.find("play_.in.music_view = {};\n    play_.in.music_view.frame = music_frame_;") != std::string::npos);
    // shuffle never runs out
    std::ifstream fr("src/win/app_remote.cpp"); std::stringstream sr; sr << fr.rdbuf();
    assert(sr.str().find("store_.loop_all);") == std::string::npos && sr.str().find("shuffle_.sync(queue_, true);") != std::string::npos);
    // the data folder moves to tinypeep\Cue once, in one rename, by the running Cue; until then
    // the old one stays in use
    std::ifstream fp("src/win/platform.cpp"); std::stringstream sp; sp << fp.rdbuf();
    assert(sp.str().find("if (!MoveFileExW(from.c_str(), now.c_str(), 0)) return false;") != std::string::npos);
    assert(sp.str().find("if (dir_exists(now)) return now;\n  if (dir_exists(was)) return was;") != std::string::npos);
    assert(app.find("const bool moved = migrate_data_dir();") < app.find("data_dir_ = std::filesystem::path(data_dir());"));
    // state.json: swapped in with the previous one kept as the backup in one call (no copy),
    // patient with an antivirus holding the file for a moment
    std::ifstream fs2("src/core/store.cpp"); std::stringstream ss2; ss2 << fs2.rdbuf();
    assert(ss2.str().find("ReplaceFileW(path.c_str(), tmp.c_str(), bak.c_str()") != std::string::npos);
    std::ifstream fa("src/win/ambient.cpp"); std::stringstream sa; sa << fa.rdbuf();
    assert(sa.str().find("if (defer_geometry) { alpha_ = 0; hide();") != std::string::npos);
    // the ambient light turned off and on again over a paused film: painted (shown) at once, not
    // left hidden until the next frame changes its colours
    assert(sa.str().find("if ((colors_dirty_ && since_paint_ >= 1.0 / 30 - 0.002) || last_a_ < 0) paint(owner);") != std::string::npos);
    // Shift read with every move of the pointer, and while the bar is held still
    assert(app.find("play_.set_shift(GetKeyState(VK_SHIFT) < 0);") != std::string::npos);
    assert(app.find("if (play_.dock_dragging() && play_.set_shift(GetKeyState(VK_SHIFT) < 0)) ensure_anim();") != std::string::npos);
  }
  std::puts("Bar placement: drag without background, Shift centres a dragged bar, narrow windows centre, in-place width changes stay centred, the compact music bar reads, close music framings reach the edges, a small log, the mini player's place and flight, smoothing's timing, music framed close as one group, a song's mini card and its list, the wide composition centred, soft jumps passed");
}
