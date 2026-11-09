#include "../src/core/preview_dispatch.h"
#include "../src/core/preview_session.h"
#include "../src/core/playback_warmup.h"
#include "../src/core/preview_timeline.h"
#include <cassert>
#include <cstdio>

int main() {
  for (int k = 0; k < 24 * 3600; ++k) {
    const double t = PreviewTimeline::time(k);
    assert(PreviewTimeline::key(t) == k);
    assert(PreviewTimeline::key(t + .9) == k);
    if (k) assert(PreviewTimeline::key(t - 1e-6) == k - 1);
  }
  PreviewDispatch d;
  d.begin(1);
  assert(d.delay(0, false, false) == 0);
  d.sent(0);
  assert(d.delay(.010, true, false) < 0);
  assert(d.delay(.010, false, false) > .02);
  assert(d.delay(.034, true, false) < 0); // even a different destination cannot starve navigation
  assert(d.delay(.034, false, false) == 0);
  assert(d.delay(.010, true, true) == 0);
  // A fresh entry does not inherit a 40-120 ms cooldown from the previous hover.
  d.begin(2);
  assert(d.delay(.010, false, false) == 0);
  d.sent(.010);
  d.begin(2);
  assert(d.delay(.011, false, false) > .03);
  int count = 0;
  d.reset(); d.begin(3);
  for (int i = 0; i < 1000; ++i) {
    const double t = i / 1000.;
    if (d.delay(t, false, false) == 0) { d.sent(t); ++count; }
  }
  assert(count >= 29 && count <= 31);
  assert(d.delay(1.04, true, false) < 0);
  assert(d.delay(1.04, false, false) == 0);
  PreviewWait seek(false, true);
  using E = PreviewWait::Event;
  using R = PreviewWait::Result;
  assert(seek.consume(E::Restart) == R::Pending);
  assert(seek.consume(E::Seek) == R::Pending);
  assert(seek.consume(E::Restart) == R::Ready);
  PlaybackWarmup warm;
  assert(!warm.ready(100, false, PlaybackWarmup::kPreviewWarmupDelay));
  warm.first_frame(100);
  assert(!warm.ready(100.3, true, PlaybackWarmup::kPreviewWarmupDelay));
  assert(warm.ready(100.3, false, PlaybackWarmup::kPreviewWarmupDelay));
  assert(!warm.ready(100.3, false, PlaybackWarmup::kAtmosphereDelay));
  puts("Preview dispatch: immediate entry, 30 Hz burst bound, stable buckets, stale events and warmup passed");
}
