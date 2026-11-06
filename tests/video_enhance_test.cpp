// When the picture is helped: super resolution only for a film smaller than the screen (within
// the driver's 360p-1440p), never past the screen; smoothing only when the film's frames do not
// land evenly on the screen's refreshes.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/video_enhance.h"
#include <cassert>
#include <cstdio>

int main() {
  // super resolution
  assert(vsr_scale(1280, 720, 1920, 1080) == 1.5);   // 720p on a 1080p screen
  assert(vsr_scale(1920, 1080, 3840, 2160) == 2.0);  // 1080p on 4K
  assert(vsr_scale(1920, 1080, 1920, 1080) == 0);    // already the screen's size
  assert(vsr_scale(3840, 2160, 3840, 2160) == 0);    // 4K: nothing to add
  assert(vsr_scale(2560, 1440, 3840, 2160) == 0);    // 1440p: a high-quality film keeps its own picture
  assert(vsr_scale(320, 240, 3840, 2160) == 0);      // below 360p: the driver does not
  assert(vsr_scale(640, 360, 3840, 2160) == 4.0);    // at most four times
  assert(vsr_scale(1920, 800, 1920, 1080) == 0);     // a wide film as wide as the screen
  assert(vsr_scale(0, 0, 1920, 1080) == 0);
  // smoothing: uneven cadences only
  assert(cadence_uneven(23.976, 60) && cadence_uneven(24, 60) && cadence_uneven(25, 60) && cadence_uneven(30, 144));
  assert(!cadence_uneven(30, 60) && !cadence_uneven(60, 60) && !cadence_uneven(24, 120) && !cadence_uneven(24, 144) &&
         !cadence_uneven(23.976, 119.88) && !cadence_uneven(120, 60) && !cadence_uneven(0, 60));
  // smoothing is a choice kept for every film: a film's Settings always has it, with its state (it acts
  // only where the cadences differ); "..." no longer carries it
  for (bool avail : {false, true}) for (bool audio : {false, true}) {
    PlayView p; p.in.dur = 100; p.in.smooth_available = avail; p.in.smooth_motion = true; p.in.audio_only = audio;
    assert(!PlayView{}.in.smooth_motion);  // off by default
    p.resize(1280, 720, 2); double t = 1; p.reset(t);
    p.open_panel_for_preview(3, t);
    for (auto& r : p.rows_) assert(r.id != PlayView::R_SMOOTH);
    p.open_panel_for_preview(11, t);
    int row = -1; for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == PlayView::R_SMOOTH) row = i;
    if (audio) { assert(row < 0); continue; }  // music's Settings has its own looks instead
    assert(row >= 0 && p.rows_[row].checked);
    p.take_actions(); p.panel_click(row, -1, 0, t);
    auto a = p.take_actions(); assert(a.size() == 1 && a[0].a == PA::SmoothMotion && a[0].v == 0 && !p.in.smooth_motion);
  }
  std::puts("Video helpers: super resolution only below the screen (360p-1080p, never 1440p or 4K, at most 4x), smoothing off by default, smoothing only on uneven cadences, kept in Settings passed");
}
