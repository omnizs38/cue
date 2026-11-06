// Music's bar holds still from song to song (the full one without background, the compact one
// with it): the app redraws the interface while it sets
// the next song up (its scene decided a moment later), and the bar used to take the film's
// place for that moment and glide back. It also follows the scene on its own (a scene that
// goes and comes back relays the bar), and a resize meanwhile keeps it centred on the foot.
#include <cassert>
#include <cmath>
#include <cstdio>
#define private public
#include "../src/ui/play.h"

int main() {
  for (bool free_style : {false, true})
    for (auto [W, H, s] : {std::tuple{1600, 900, 2.0}, std::tuple{2560, 1400, 2.5}, std::tuple{1280, 720, 1.5}}) {
      PlayView p;
      p.in.dur = 200; p.in.audio_only = true; p.in.has_cover = true; p.in.cover_scene = true; p.in.cover_art = true;
      p.in.music_title = "Dream Sequence"; p.in.music_artist = "Jane Remover"; p.in.music_scene = MusicScene::wide();
      p.in.queue_active = true;
      for (uint32_t i = 1; i <= 4; ++i) p.in.queue.push_back({i, "song " + std::to_string(i) + ".mp3", i == 2, true});
      p.in.queue_revision = 1; p.in.has_prev = p.in.has_next = true; p.in.controls_free = free_style;
      p.set_base_scale(s); p.resize(W, H, s);
      double t = 1; p.reset(t);
      auto frame = [&](int w, int h) {
        int top = h; Rect d = p.blur_cloud().controls; if (d.h > 0 && d.cy() > h / 2) top = d.y;
        p.in.music_view = music_view(p.in.music_scene, music_frame_for(w, h, s), w, h, top, p.music_centre_below(), p.music_stack_above(), -1, p.music_list_beside());
        p.changed(t);
      };
      for (int i = 0; i < 60; ++i) { frame(W, H); p.activity(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
      const Rect d0 = p.dock_rect();
      // without its background the bar runs along the foot; with it, the film's compact bar
      const bool full = free_style;
      assert(p.music_full_ == full);
      if (!full) assert(d0.w < W / 2 && std::abs(d0.x + d0.w / 2 - W / 2) <= 1);
      // the next song: the scene is off and on again within one redraw, the view reset
      p.in.music_title = "DNA"; p.in.music_artist = "Number Teddie"; p.in.time = 0; p.in.dur = 0; p.in.has_next = false;
      p.in.cover_scene = false; p.in.music_view = {}; p.changed(t);
      p.in.cover_scene = true; p.changed(t);
      for (int i = 0; i < 40; ++i) {
        if (i == 5) { p.in.dur = 180; p.in.has_next = true; }
        frame(W, H); p.activity(t); t += 1 / 60.; p.tick(t, 1 / 60.);
        const Rect d = p.dock_rect();
        assert(p.music_full_ == full && d.x == d0.x && d.y == d0.y && d.w == d0.w && d.h == d0.h);
      }
      // a scene really gone (a song without its picture) and back: the bar follows both ways
      p.in.cover_scene = false; p.changed(t);
      assert(!p.music_full_);
      p.in.cover_scene = true; p.changed(t);
      assert(p.music_full_ == full);
      for (int i = 0; i < 90; ++i) { frame(W, H); p.activity(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
      const Rect d1 = p.dock_rect();
      assert(d1.x == d0.x && d1.w == d0.w && d1.h == d0.h);
      // a resize: still the full bar, centred, on the foot
      const int W2 = W * 9 / 10, H2 = H * 9 / 10;
      p.resize(W2, H2, s);
      for (int i = 0; i < 90; ++i) { frame(W2, H2); p.activity(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
      const Rect d2 = p.dock_rect();
      assert(p.music_full_ == full && std::abs(d2.x + d2.w / 2 - W2 / 2) <= 1 && d2.b() <= H2);
    }
  // music's list cleared while open beside the cover: it stays music's list, its line that adds
  // songs alone (it used to become the films' popover over the cover); opened again, the same
  {
    PlayView p;
    p.in.dur = 200; p.in.audio_only = true; p.in.has_cover = true; p.in.cover_scene = true; p.in.cover_art = true;
    p.in.music_title = "DNA"; p.in.music_artist = "Number Teddie"; p.in.music_scene = MusicScene::wide();
    p.in.queue_active = true;
    for (uint32_t i = 1; i <= 4; ++i) p.in.queue.push_back({i, "song " + std::to_string(i) + ".mp3", i == 1, true});
    p.set_base_scale(2); p.resize(1600, 900, 2);
    double t = 1; p.reset(t);
    p.in.music_view = music_view(p.in.music_scene, MusicFrame::Centre, 1600, 900, 740);
    p.changed(t);
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t);
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    assert(p.panel_ == PlayView::Panel::Queue && p.scene_list());
    // picking a song in music's list: it plays, the list stays open (Esc closes it)
    p.take_actions();
    p.queue_event({QueueView::Action::Play, 3, 2}, t);
    {
      bool asked = false;
      for (auto& a : p.take_actions()) asked |= a.a == PA::QueuePlay && a.id == 3;
      assert(asked && p.panel_ == PlayView::Panel::Queue);
    }
    p.in.queue.clear(); p.in.queue_active = false; ++p.in.queue_revision; p.changed(t);
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    assert(p.panel_ == PlayView::Panel::Queue && p.scene_list() && p.queue_view_.add_row().h > 0);
    p.close_panel();
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t);
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    assert(p.panel_ == PlayView::Panel::Queue && p.scene_list());
    const Rect lr = p.panel_rect();
    assert(lr.x >= 1600 / 2 && lr.r() <= 1600 && lr.b() <= p.dock_rect().y);
    // the card too: an empty list is the card's own, one line
    p.in.mini_player = true; p.resize(640, 337, 2); p.changed(t);
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t);
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    assert(p.panel_ == PlayView::Panel::Queue && p.card_ && p.scene_list());
  }
  // a song picked in the open list, while it changes (its words stepping out, its framing not
  // made yet): the list stays music's, beside the cover and in the card (in the card it became
  // the films' popover over the turning cover; beside the cover the new song was framed centred)
  for (bool card : {false, true}) {
    PlayView p;
    p.in.dur = 200; p.in.audio_only = true; p.in.has_cover = true; p.in.cover_scene = true; p.in.cover_art = true;
    p.in.music_title = "DNA"; p.in.music_artist = "Number Teddie"; p.in.music_scene = MusicScene::wide();
    p.in.queue_active = true; p.in.mini_player = card;
    for (uint32_t i = 1; i <= 4; ++i) p.in.queue.push_back({i, "song " + std::to_string(i) + ".mp3", i == 1, true});
    const int W = card ? 720 : 1600, H = card ? 379 : 900;
    p.set_base_scale(2); p.resize(W, H, 2);
    double t = 1; p.reset(t);
    p.in.music_view = card ? MusicView{} : music_view(p.in.music_scene, MusicFrame::Centre, W, H, 740);
    if (card) p.in.music_view.frame = MusicFrame::Card;
    p.changed(t);
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t);
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    assert(p.scene_list() && (card ? p.card_ : p.music_list_beside()));
    const Rect before = p.panel_rect();
    // picked: the words step out (and, beside the cover, the next song's framing is not made yet:
    // only its frame is known)
    p.queue_event({QueueView::Action::Play, 3, 2}, t);
    p.in.music_leaving = true; p.changed(t);
    for (int i = 0; i < 20; ++i) {
      t += 1 / 60.; p.tick(t, 1 / 60.);
      assert(p.panel_ == PlayView::Panel::Queue && p.scene_list());
      const Rect r = p.panel_rect();
      if (card) assert(r.x == before.x && r.y == before.y && r.w == before.w);  // (the card's list, where it was)
    }
    if (!card) {
      p.in.music_view = {}; p.in.music_view.frame = MusicFrame::Centre;  // (as the app now sets it)
      p.changed(t);
      assert(p.music_list_beside());
    }
    p.in.music_leaving = false;
    if (!card) p.in.music_view = music_view(p.in.music_scene, MusicFrame::Centre, W, H, 740);  // (framed by the app)
    p.changed(t);
    for (int i = 0; i < 30; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    const Rect after = p.panel_rect();
    assert(p.scene_list() && std::abs(after.x - before.x) <= 2);
  }
  // a film's up next over the picture: picking a file closes it (the film is what was chosen)
  {
    PlayView p;
    p.in.dur = 200; p.in.queue_active = true;
    for (uint32_t i = 1; i <= 3; ++i) p.in.queue.push_back({i, "ep " + std::to_string(i) + ".mkv", i == 1, false});
    p.resize(1600, 900, 2);
    double t = 1; p.reset(t);
    p.open_panel(PlayView::Panel::Queue, PlayView::B_QUEUE, t);
    for (int i = 0; i < 20; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
    assert(p.panel_ == PlayView::Panel::Queue && !p.scene_list());
    p.queue_event({QueueView::Action::Play, 2, 1}, t);
    assert(p.panel_ == PlayView::Panel::None);
  }
  std::puts("Song change: music's bar holds still between songs, follows its scene and a resize, the cleared list stays music's, a pick keeps music's list open, and music's while the song changes (card and beside) passed");
}
