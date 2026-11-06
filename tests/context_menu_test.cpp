// The picture's right-click menu: the mini player is offered for a film and for a song (music's
// card), never in full screen, and the mini player itself has no menu on the picture.
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

using B = PlayView;

static int row_of(const PlayView& p, int id) {
  for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == id) return i;
  return -1;
}

int main() {
  for (bool music : {false, true})
    for (double scale : {1., 2.}) {
      PlayView p;
      p.in.dur = 200; p.in.audio_only = music;
      p.resize(1280, 720, scale);
      double t = 1;
      p.reset(t);
      p.right_click(640, 200, t);
      assert(p.panel_ == B::Panel::Context);
      const int mini = row_of(p, B::R_MINI);
      assert(mini >= 0);
      assert(row_of(p, B::R_CLOSE) > mini);         // close stays last, on its own
      assert((row_of(p, B::R_FRAME) >= 0) == !music);  // save frame: films only
      p.take_actions();
      p.panel_click(mini, -1, 0, t);
      bool asked = false;
      for (auto& a : p.take_actions()) asked |= a.a == PA::MiniPlayer && a.v == 1;
      assert(asked && p.panel_ == B::Panel::None);
      // full screen: the menu has no mini player
      p.in.fullscreen = true; p.changed(t);
      p.right_click(640, 200, t);
      assert(p.panel_ == B::Panel::Context && row_of(p, B::R_MINI) < 0);
      p.close_panel();
      // the mini player: no menu at all
      p.in.fullscreen = false; p.in.mini_player = true; p.changed(t);
      p.right_click(100, 60, t);
      assert(p.panel_ == B::Panel::None);
    }
  puts("Context menu: mini player offered for films and songs (not in full screen, no menu in the mini player) passed");
}
