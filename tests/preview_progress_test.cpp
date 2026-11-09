// Regression: pointer events faster than decoding must not keep restarting it.
#include "../src/core/preview_dispatch.h"
#include "../src/core/frame_cache.h"
#include "../src/core/preview_session.h"
#include <cassert>
#include <cstdio>

int main() {
  for (int latency : {12, 60, 120, 300}) {
    PreviewDispatch d;
    d.begin(1);
    PreviewSession session; session.reset(); session.loaded = true;
    FrameCache cache;
    int target = -1, active = -1, done_at = -1, shown = -1, completions = 0;
    uint32_t request = 0;
    constexpr int stop = 2000;
    for (int ms = 0; ms <= stop + 2*latency + 34; ++ms) {
      if (ms <= stop && ms % 8 == 0) target = ms == stop ? 999 : ms < 1000 ? ms/8 : 250-ms/8;
      if (session.busy && ms == done_at) {
        assert(session.accepts_frame(session.generation, request));
        FrameCache::Image image;
        image.w = image.h = 1; image.px = {(uint32_t)active}; image.sample_time = active;
        assert(cache.put(active, std::move(image)));
        session.busy = false;
        if (ms < stop) ++completions;
      }
      if (cache.count(target)) { shown = target; }
      else if (d.delay(ms/1000., session.busy, false) == 0) {
        // The new request cannot invalidate an in-flight navigation reply.
        assert(!session.busy);
        session.busy = true; request = ++session.request;
        active = target; done_at = ms+latency; d.sent(ms/1000.);
      }
      // Old results may fill this file's cache, but never replace the newer destination.
      if (shown >= 0 && cache.count(target)) assert(shown == target);
    }
    assert(completions > 0 && shown == 999 && !session.busy);
    assert(cache.count(999) && cache.retained_bytes() <= FrameCache::kMaxBytes);
    const auto last = session.request;
    // Cached revisit bypasses dispatch, even while another request is decoding.
    target = cache.begin()->first;
    assert(cache.count(target)); shown = target;
    assert(session.request == last);
    // Loading/closing a different file invalidates every old result.
    auto previous = session.generation;
    session.reset(); cache.clear();
    assert(!session.accepts_frame(previous, request) && cache.empty());
  }
  PreviewDispatch d; d.begin(10); d.sent(2);
  assert(d.delay(2.001, true, true) == 0);  // a hover still interrupts colour work immediately
  d.begin(11);
  assert(d.delay(2.002, true, false) < 0); // re-entry does not cancel active navigation
  assert(d.delay(2.002, false, false) == 0);
  puts("Preview progress: 125 Hz sweeps/reversals, 12-300 ms decodes, cached revisits, final destination, colour preemption and file cancellation passed");
}
