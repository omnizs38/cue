// Deterministic scheduler model, NOT a decoder or Windows latency benchmark.
// g++ -std=c++20 -O2 tools/benchmark_preview_dispatch.cpp -o dispatch-bench
#include "../src/core/preview_dispatch.h"
#include <cstdio>
#include <map>

int main() {
  std::puts("simulated_decode_ms,started,cancelled,completed_while_moving,matched_updates_while_moving,final_wait_ms");
  for (int latency : {12, 60, 120, 300}) {
    PreviewDispatch dispatch;
    dispatch.begin(1);
    std::map<int, bool> cache;
    int active = -1, wanted = -1, finish = -1, shown = -1;
    int started = 0, cancelled = 0, completed = 0, matched = 0, final_wait = -1;
    constexpr int stop = 2000;
    for (int ms = 0; ms < stop + 2 * latency + 100; ++ms) {
      // A long-video sweep at 125 pointer events/s, reversing halfway through.
      if (ms <= stop && ms % 8 == 0)
        wanted = ms == stop ? 999 : ms < stop / 2 ? ms / 8 : 250 - ms / 8;
      if (active >= 0 && ms >= finish) {
        cache[active] = true;
        if (ms < stop) ++completed;
        active = -1;
      }
      if (wanted >= 0 && cache.count(wanted)) {
        if (shown != wanted && ms < stop) ++matched;
        shown = wanted; wanted = -1;
      }
      if (wanted >= 0 && dispatch.delay(ms / 1000., active >= 0, false) == 0) {
        if (active >= 0) ++cancelled;
        active = wanted; finish = ms + latency;
        dispatch.sent(ms / 1000.); ++started;
      }
      if (ms >= stop && wanted < 0 && shown == 999) { final_wait = ms - stop; break; }
    }
    std::printf("%d,%d,%d,%d,%d,%d\n", latency, started, cancelled, completed, matched, final_wait);
  }
}
