// Esc to the home: the pixel iris closes over the picture (App::close_to_home drives the engine
// from PlayView::iris_open). Checks the curve, its continuity from an opening iris, the dock and
// the cancel used when a new file arrives while it closes.
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

int main() {
  const double from = PlayView::kIrisCloseFrom;
  {  // from a playing film: starts just inside the corners, shrinks steadily, closed at 0.22 s
    PlayView p;
    p.resize(1280, 720, 2);
    double t = 10;
    p.reset(t);
    assert(p.iris_open(t) == -1);
    p.close_iris(t);
    assert(p.iris_closing() && p.iris_active() && !p.iris_waiting());
    assert(std::fabs(p.iris_open(t) - from) < 1e-9);
    assert(p.vis_.target == 0 && p.vis_.v == 0);  // the controls go at once
    double last = p.iris_open(t);
    for (int i = 1; i <= 26; ++i) {
      double k = p.iris_open(t + i / 120.0);
      assert(k <= last + 1e-12 && k >= 0);
      last = k;
    }
    assert(!p.iris_closed(t + 0.20));
    assert(p.iris_closed(t + 0.2201) && p.iris_open(t + 0.5) == 0);
    // ticking does not reopen it or end it on its own (the App stops the engine, then goes home)
    for (int i = 0; i < 60; ++i) p.tick(t + 0.25 + i / 60.0, 1 / 60.0);
    assert(p.iris_closing() && p.iris_open(t + 2) == 0);
    p.close_iris(t + 2);  // a second call changes nothing
    assert(p.iris_open(t + 2) == 0);
    p.cancel_close();
    assert(!p.iris_active() && p.iris_open(t + 2) == -1);
  }
  {  // Esc while the opening iris is still opening: it turns around from where it is
    for (double at : {0.02, 0.08, 0.15, 0.3}) {
      PlayView p;
      p.resize(1280, 720, 2);
      double t = 5;
      p.reset(t);
      p.begin_cover();
      p.start_iris(t);
      const double open = p.iris_open(t + at);
      p.close_iris(t + at);
      const double k = p.iris_open(t + at);
      assert(k <= open + 1e-9 && k >= std::min(open, from) - 1e-9);
      assert(std::fabs(k - std::min(open, from)) < 1e-6);
      assert(p.iris_closed(t + at + 0.2201));
    }
  }
  {  // still black (waiting for the first picture): already closed
    PlayView p;
    p.resize(1280, 720, 2);
    p.reset(1);
    p.begin_cover();
    p.close_iris(1);
    assert(p.iris_closed(1));
  }
  {  // reduced motion: closed on the spot
    reduce_motion() = true;
    PlayView p;
    p.resize(1280, 720, 2);
    p.reset(1);
    p.close_iris(1);
    assert(p.iris_closed(1));
    reduce_motion() = false;
  }
  {  // Music fades as one composition (cover, spectrum, room and audio), also when paused.
    for (bool paused : {false, true}) {
      PlayView p; p.in.audio_only = true; p.in.paused = paused;
      p.resize(1280, 720, 2); p.reset(10);
      assert(p.close_opacity(10) == 1);
      p.close_iris(10);
      double last = 1;
      for (int i = 0; i <= 30; ++i) {
        const double opacity = p.close_opacity(10 + i / 120.0);
        assert(opacity >= 0 && opacity <= last && opacity <= 1);
        last = opacity;
      }
      assert(last == 0 && p.close_opacity(12) == 0); // held black through asynchronous stop
      p.cancel_close(); assert(p.close_opacity(12) == 1);
    }
  }
  puts("Close iris: curve, continuity from an opening iris, dock, cancel and reduced motion passed");
}
