// The control bar by priority: one row of controls and the timeline at every width. As the
// window narrows the bar first shortens the volume line, then hands its least needed buttons to
// "..." (speed, repeat, up next, audio, subtitles), folds the volume into its icon, and with a
// playlist lets the 15-second steps go; below that it becomes the mini player. Play stays
// centred and "..." at the right end; nothing overlaps; nothing becomes unreachable; a window
// held at a threshold never flickers between two folds.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/audio_mode.h"
#include <cassert>
#include <cstdio>

using B = PlayView;

static Rect hot(const PlayView& p, int id) {
  for (auto& h : p.hots_) if (h.id == id) return h.r;
  return {};
}

int main() {
  int layouts = 0;
  for (bool music : {false, true})
    for (bool queue : {false, true})
      for (bool free_style : {false, true})
        for (double scale : {1., 1.5, 2., 3.})
          for (int art_w = 150; art_w <= 900; art_w += 6) {
            PlayView p;
            p.in.dur = 300; p.in.audio_only = music; p.in.controls_free = free_style;
            p.in.audios = {{1, "audio"}, {2, "audio"}};
            if (!music) { Track sub{1, "sub", "eng", "", "subrip"}; p.in.subs = {sub}; }
            p.in.queue_active = queue; p.in.has_next = queue;
            if (queue) p.in.queue = {{1, "a", true, false}, {2, "b", false, false}};
            p.resize((int)(art_w * scale), (int)(500 * scale), scale);
            double t = 1; p.reset(t);
            for (int i = 0; i < 40; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); }
            ++layouts;
            if (p.mini_) {
              // the mini player: two rows along the bottom edge (the volume, the playback and
              // "..."; under them the timeline)
              assert(hot(p, B::B_PLAY).w > 0 && hot(p, B::B_MORE).w > 0 && hot(p, B::B_TIMELINE).w > 0);
              assert(p.dh_ == (int)(40 * scale) && p.overflowed(B::B_FS) && hot(p, B::B_VOL).w > 0);
              assert(hot(p, B::B_TIMELINE).y > hot(p, B::B_PLAY).y);
              continue;
            }
            assert(p.dh_ == (int)(38 * scale));  // two rows: the controls and the timeline
            const int dw = p.dw_;
            assert(dw <= (int)std::lround(art_w * scale));
            // play on the centre, "..." at the right end, full screen always on the bar
            Rect play = hot(p, B::B_PLAY), more = hot(p, B::B_MORE);
            assert(std::abs(play.cx() - dw / 2) <= (int)scale + 1);
            assert(more.w > 0 && more.r() <= dw && dw - more.r() <= (int)(6 * scale));
            assert(hot(p, B::B_FS).w > 0 && hot(p, B::B_VOL).w > 0 && hot(p, B::B_TIMELINE).w > 0);
            // no two controls of the row overlap
            for (auto& a : p.hots_)
              for (auto& b : p.hots_) {
                if (&a == &b || a.r.h != (int)(16 * scale) || b.r.h != (int)(16 * scale)) continue;
                assert(a.r.r() <= b.r.x || b.r.r() <= a.r.x || a.r.b() <= b.r.y || b.r.b() <= a.r.y);
              }
            // every optional control is on the bar or in "..."
            for (int id : {B::B_SPEED, B::B_LOOP, B::B_QUEUE, B::B_AUDIO})
              assert(p.in_bar(id) != p.overflowed(id));
            if (!music) assert(p.in_bar(B::B_CC) != p.overflowed(B::B_CC));
            // the order of priority: speed leaves before repeat, repeat before up next, up next
            // before audio, audio before subtitles
            if (p.in_bar(B::B_LOOP)) assert(p.in_bar(B::B_QUEUE));
            if (p.in_bar(B::B_QUEUE)) assert(p.in_bar(B::B_AUDIO));
            if (!music && p.in_bar(B::B_AUDIO)) assert(p.in_bar(B::B_CC));
            if (p.in_bar(B::B_SPEED)) assert(p.in_bar(B::B_VOLBAR));
            // the steps go only with a playlist, and only after everything else
            if (p.fold_steps_) assert(!music && queue && p.vol_folded_ && !p.in_bar(B::B_CC) && hot(p, B::B_PREV).w > 0 && hot(p, B::B_NEXT).w > 0);
            // what "..." holds: its first row is those very buttons, and the menu fits the window
            if (p.overflow_ & ~(1u << B::B_FS)) {
              p.activate(B::B_MORE, t);
              for (int i = 0; i < 30; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); }
              assert(p.panel_ == B::Panel::More && p.rows_.front().k == B::RK::Tools);
              for (int id : p.tool_ids_) assert(p.overflowed(id));
              Rect r = p.panel_rect();
              if (!(r.x >= 0 && r.r() <= p.w_ && r.y >= 0)) { std::printf("menu out: music%d q%d free%d s%.1f w%d: %d,%d %dx%d win %dx%d collapsed%d\n", music, queue, free_style, scale, art_w, r.x, r.y, r.w, r.h, p.w_, p.h_, p.more_collapsed_); assert(false); }
            }
          }
  // the folded volume: its icon opens the volume above it (no mute), the slider sets it, and
  // the wheel shows the level in the middle
  {
    PlayView p; p.in.dur = 100; p.in.volume = 60;
    p.resize(200 * 2, 400 * 2, 2); double t = 1; p.reset(t);
    assert(p.vol_folded_ && !p.in_bar(B::B_VOLBAR));
    p.activate(B::B_VOL, t);
    assert(p.panel_ == B::Panel::Volume && p.take_actions().empty());
    int row = -1; for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == B::R_VOLUME) row = i;
    assert(row >= 0);
    p.panel_adjust(row, 1, t);
    auto a = p.take_actions(); assert(!a.empty() && a[0].a == PA::Volume && a[0].v == 65);
    p.activate(B::B_VOL, t); assert(p.panel_ == B::Panel::None);  // the icon closes it again
    p.wheel(1, t); assert(p.flash_text_ == "70%");
    // widened: the line comes back and the icon mutes as before
    p.resize(900 * 2, 500 * 2, 2); assert(!p.vol_folded_ && p.in_bar(B::B_VOLBAR));
    p.take_actions(); p.activate(B::B_VOL, t); a = p.take_actions(); assert(a.size() == 1 && a[0].a == PA::ToggleMute);
  }
  // a window held right at a threshold does not flicker: narrowing folds at once, widening
  // unfolds only with room to spare
  {
    PlayView p; p.in.dur = 100; p.resize(400, 600, 1); double t = 1; p.reset(t);
    const int start = p.dock_step_;
    int w = 400;
    while (p.dock_step_ == start && w > 200) { w -= 1; p.resize(w, 600, 1); }
    const int folded = p.dock_step_;
    assert(folded > start);
    p.resize(w + 1, 600, 1); assert(p.dock_step_ == folded);  // one pixel back: still folded
    p.resize(w + 20, 600, 1); assert(p.dock_step_ < folded);   // clearly past it: unfolds
  }
  // a menu whose button leaves the bar re-anchors on "..."
  {
    PlayView p; p.in.dur = 100; p.resize(900, 600, 1); double t = 1; p.reset(t);
    p.activate(B::B_LOOP, t); p.take_actions();
    p.activate(B::B_QUEUE, t); assert(p.queue_open() && p.panel_btn_ == B::B_QUEUE);
    p.resize(250, 600, 1); assert(!p.in_bar(B::B_QUEUE) && p.panel_btn_ == B::B_MORE);
  }
  // a film's bar has the mini player's button too; a narrow window hands it to "..." first
  {
    PlayView p; p.in.dur = 100; p.resize(1600, 900, 2); double t = 1; p.reset(t);
    assert(p.in_bar(B::B_MINI));
    p.take_actions(); p.activate(B::B_MINI, t);
    bool asked = false; for (auto& a : p.take_actions()) asked |= a.a == PA::MiniPlayer && a.v == 1;
    assert(asked);
    int w = 1200;
    while (w > 200 && p.in_bar(B::B_MINI)) { w -= 10; p.resize(w, 600, 1); }
    assert(!p.in_bar(B::B_MINI) && p.in_bar(B::B_LOOP) && p.in_bar(B::B_CC));  // (the first to go)
    p.activate(B::B_MORE, t);
    bool there = false; for (int id : p.tool_ids_) there |= id == B::B_MINI;
    assert(there);
  }
  // the mini player asked for: the bar is the mini player's at any size; its menu has a "back
  // to full size" row (as the arrow in its corner) and no full screen (the full player's)
  {
    PlayView p; p.in.dur = 100; p.in.mini_player = true; p.resize(1280, 720, 2); double t = 1; p.reset(t);
    assert(p.mini_);
    p.activate(B::B_MORE, t);
    bool back = false, fs = false;
    for (auto& r : p.rows_) back |= r.id == B::R_MINI;
    for (int id : p.tool_ids_) fs |= id == B::B_FS;
    assert(back && !fs);
    {  // "back to full size" by name
      int row = -1; for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == B::R_MINI) row = i;
      p.take_actions(); p.panel_click(row, -1, 0, t);
      bool asked = false; for (auto& a : p.take_actions()) asked |= a.a == PA::MiniPlayer && a.v == 0;
      assert(asked);
      p.activate(B::B_MORE, t);
    }
    p.key("esc", false, false, t);
    // its bar: play, the volume (a column above it), the timeline, "..."; back to full size is the
    // arrow in its top-left corner (where the full player's "home" arrow is)
    for (int id : {B::B_PLAY, B::B_VOL, B::B_MORE, B::B_TIMELINE}) assert(hot(p, id).w > 0);
    assert(hot(p, B::B_EXPAND).w == 0);
    p.take_actions(); p.activate(B::B_VOL, t); assert(p.panel_ == B::Panel::Volume); p.key("esc", false, false, t);
    p.take_actions();
    for (int i = 0; i < 30; ++i) { t += 1 / 60.0; p.activity(t); p.tick(t, 1 / 60.0); }
    { Rect br = p.back_rect(); p.mouse_move(br.cx(), br.cy(), t); p.mouse_down(br.cx(), br.cy(), t); p.mouse_up(br.cx(), br.cy(), t); }
    { auto a = p.take_actions(); bool back = false; for (auto& x : a) back |= x.a == PA::MiniPlayer && x.v == 0; assert(back); }
    // the bar's empty places move the mini window like the picture does (a click there does nothing)
    {
      Rect d = p.dock_rect(); int gx = -1, gy = -1;
      for (int y = (int)d.y; y < d.y + d.h && gx < 0; y += 2) for (int x = (int)d.x; x < d.x + d.w; x += 2) if (p.hit_dock(x, y) == B::B_DOCKBG) { gx = x; gy = y; break; }
      assert(gx >= 0);
      p.take_actions(); t += 1; p.mouse_move(gx, gy, t); p.mouse_down(gx, gy, t); p.mouse_up(gx, gy, t);
      for (auto& x : p.take_actions()) assert(x.a != PA::WinDragBegin && x.a != PA::TogglePause);
      t += 1; p.mouse_down(gx, gy, t); p.mouse_move(gx + 30, gy + 10, t);
      bool drag = false; for (auto& x : p.take_actions()) drag |= x.a == PA::WinDragBegin; assert(drag);
      p.mouse_up(gx + 30, gy + 10, t); p.take_actions();
    }
    p.in.mini_player = false; p.resize(1280, 720, 2); assert(!p.mini_ && p.dh_ == 38 * 2);
  }
  // music in a reduced window (framed on the cover, words under it): previous, play, next, the
  // timeline, the volume's icon and "..."; everything else (full screen too) in "..."
  for (MusicFrame m : {MusicFrame::Tall, MusicFrame::Cover, MusicFrame::Scene, MusicFrame::Centre}) {
    PlayView p; p.in.dur = 200; p.in.audio_only = true; p.in.cover_scene = true;
    p.in.queue_active = true; p.in.queue = {{1, "a", true, false}, {2, "b", false, false}};
    p.in.music_scene = MusicScene::wide();
    p.in.music_view = music_view(p.in.music_scene, m, 1300, 900, 800);
    p.resize(1300, 900, 2); double t = 1; p.reset(t);
    if (m == MusicFrame::Scene || m == MusicFrame::Centre) {
      // the roomy compositions with the bar's background: the film's compact bar (the buttons
      // over the timeline), centred low, with the list's count beside its icon
      assert(!p.music_compact_ && !p.music_full_ && p.in_bar(B::B_FS) && p.dh_ == 38 * 2 && p.dw_ < 1300 * 6 / 10);
      assert(hot(p, B::B_TIMELINE).y > hot(p, B::B_PLAY).y && p.layout_badge_ == 1);
      assert(hot(p, B::B_QUEUE).r() <= hot(p, B::B_LOOP).x);
      // a long list ("99+"): the slot grows with it, nothing overlaps, at any width the bar keeps
      for (int W : {1300, 900, 700, 560}) {
        p.in.queue.clear();
        for (uint32_t k = 1; k <= 120; ++k) p.in.queue.push_back({k, "song", k == 1, true});
        p.resize(W, 900, 2); p.changed(t);
        assert(p.layout_badge_ == 3 || !p.in_bar(B::B_QUEUE));
        std::vector<Rect> rs;
        for (int id : {B::B_QUEUE, B::B_LOOP, B::B_FS, B::B_MORE, B::B_NEXT})
          if (p.in_bar(id)) rs.push_back(hot(p, id));
        for (size_t i = 0; i < rs.size(); ++i)
          for (size_t j = i + 1; j < rs.size(); ++j) assert(rs[i].r() <= rs[j].x || rs[j].r() <= rs[i].x);
      }
      p.in.queue = {{1, "a", true, false}, {2, "b", false, false}};
      p.resize(1300, 900, 2); p.changed(t);
      // without it: the bar along the window's foot, nearly edge to edge, the timeline over the
      // buttons
      p.in.controls_free = true; p.changed(t);
      for (int i = 0; i < 40; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); }
      assert(!p.music_compact_ && p.music_full_ && p.in_bar(B::B_FS) && p.dh_ == 48 * 2);
      assert(p.dw_ >= 1300 - 2 * (int)std::lround(std::max(14.0, 650 * 0.05) * 2) - 2);
      assert(hot(p, B::B_TIMELINE).y < hot(p, B::B_PLAY).y && p.layout_badge_ == 1);
      // the count's tag never meets its neighbour: the list's slot ends before repeat's begins
      assert(hot(p, B::B_QUEUE).r() <= hot(p, B::B_LOOP).x);
      continue;
    }
    assert(p.music_compact_);
    for (int id : {B::B_PREV, B::B_PLAY, B::B_NEXT, B::B_VOL, B::B_MORE, B::B_TIMELINE}) assert(p.in_bar(id));
    for (int id : {B::B_FS, B::B_LOOP, B::B_QUEUE, B::B_SPEED}) assert(!p.in_bar(id) && p.overflowed(id));
    assert(p.vol_folded_ && p.dw_ <= 210 * 2);  // a compact bar, not the window's width
  }
  // buttons glide to a new fold and fade where they leave, only while the bar re-folds
  {
    PlayView p; p.in.dur = 100; p.resize(900, 600, 1); double t = 1; p.reset(t);
    p.activity(t); for (int i = 0; i < 30; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); }
    assert(p.slot_alpha(B::B_LOOP) == 1);
    p.resize(300, 600, 1);
    assert(!p.in_bar(B::B_LOOP) && p.slot_alpha(B::B_LOOP) > 0);  // still fading where it was
    for (int i = 0; i < 40; ++i) { t += 1.0 / 60; p.activity(t); p.tick(t, 1.0 / 60); }
    assert(p.slot_alpha(B::B_LOOP) == 0 && !p.step_slots(1.0 / 60));  // settled: nothing left moving
  }
  std::printf("Adaptive bar: %d layouts (video/music, playlist, both styles, four scales) with play centred, \"...\" at the end, no overlaps, priority order, everything reachable, folded volume, no flicker at thresholds, menus re-anchored, the mini player and settled motion passed\n", layouts);
}
