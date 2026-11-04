// The click that brings an inactive window forward only wakes it: the controls appear, the
// picture does not pause. Buttons on the bar act on that first click; a double click still
// goes fullscreen without touching playback; the next ordinary click pauses as always.
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

static void count(PlayView& v, int& pause, int& fs, int& other) {
  pause = fs = other = 0;
  for (auto a : v.take_actions()) {
    if (a.a == PA::TogglePause) ++pause;
    else if (a.a == PA::Fullscreen) ++fs;
    else ++other;
  }
}

int main() {
  for (bool paused : {false, true}) {
    PlayView v;
    v.in.paused = paused; v.in.dur = 240;
    v.resize(1280, 720, 2); v.reset(1); v.set_double_click_time(0.5);
    v.tick(2, 0.05); v.take_actions();
    int p, f, o;
    // activation click on the picture: no pause
    v.activation_click = true;
    v.mouse_down(640, 300, 3); v.mouse_up(640, 300, 3.02);
    v.tick(3.6, 0.05); count(v, p, f, o);
    assert(p == 0 && f == 0 && v.in.paused == paused && !v.activation_click);
    // the next click pauses as always
    v.mouse_down(640, 300, 4); v.mouse_up(640, 300, 4.02);
    v.tick(4.6, 0.05); count(v, p, f, o);
    assert(p == 1 && v.in.paused != paused);
    v.changed(4.7);
    // activation by a double click: fullscreen, playback untouched
    const bool before = v.in.paused;
    v.activation_click = true;
    v.mouse_down(640, 300, 6); v.mouse_up(640, 300, 6.02);
    v.mouse_down(641, 300, 6.2); v.mouse_up(641, 300, 6.22);
    v.tick(6.8, 0.05); count(v, p, f, o);
    assert(p == 0 && f == 1 && v.in.paused == before);
  }
  // a button on the bar acts on the activation click itself
  {
    PlayView v;
    v.in.dur = 240; v.resize(1280, 720, 2); v.reset(1); v.set_double_click_time(0.5);
    v.activity(2); for (int i = 0; i < 30; ++i) v.tick(2 + i / 60.0, 1 / 60.0);
    v.take_actions();
    Rect d = v.dock_rect();
    int bx = -1, by = -1;
    for (int y = d.y; y < d.b() && bx < 0; ++y)
      for (int x = d.x; x < d.r(); ++x)
        if (v.hit_dock(x, y) == PlayView::B_PLAY) { bx = x; by = y; break; }
    assert(bx >= 0);
    v.activation_click = true;
    v.mouse_down(bx, by, 3); v.mouse_up(bx, by, 3.02);
    int p, f, o; count(v, p, f, o);
    assert(p == 1);
  }
  std::puts("activation click: wakes without pausing (playing and paused), next click pauses, double click goes fullscreen only, bar buttons act on the first click passed");
}
