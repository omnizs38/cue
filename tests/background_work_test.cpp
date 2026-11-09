#include "../src/core/bounded_worker.h"
#include "../src/core/async_sample.h"
#include "../src/ui/music_light.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <future>
#include <vector>

int main() {
  // A delayed engine query must neither grow a request queue nor light a new
  // file with data from the last song, pause, seek or minimized interval.
  AsyncSample sample;
  const auto first = sample.begin(10);
  assert(first && sample.busy());
  for (int i = 0; i < 10000; ++i) assert(!sample.begin(10.01));
  assert(!sample.finish(first + 1, 10.02) && sample.busy());
  sample.invalidate();
  assert(!sample.begin(10.03));
  assert(!sample.finish(first, 10.04) && !sample.busy());
  auto second = sample.begin(10.05);
  assert(second != first && sample.finish(second, 10.06));
  auto slow = sample.begin(11);
  assert(!sample.finish(slow, 11.3) && !sample.busy());
  auto rejected = sample.begin(12);
  sample.finish(rejected, 12);  // an enqueue failure frees the slot immediately
  assert(!sample.busy() && sample.begin(12.01));
  puts("async sample: 10000 coalesced reads, generation, age and enqueue failures passed");

  // Use a barrier rather than scheduler timing: while IO is blocked, only the
  // four newest waiting requests survive a burst. Every replaced job finishes.
  std::promise<void> started, release;
  auto gate = release.get_future().share();
  std::atomic<int> ran{0}, cancelled{0}, live{0}, peak{0};
  std::vector<int> order;
  std::promise<void> last;
  {
    BoundedWorker worker(4);
    worker.submit({[&] { ++live; peak = live.load(); started.set_value(); gate.wait(); --live; }, {}});
    started.get_future().wait();
    for (int i = 0; i < 1000; ++i) {
      worker.submit({[&, i] { const int n = ++live; if (n > peak) peak = n; order.push_back(i); ++ran; --live; if (i == 999) last.set_value(); }, [&] { ++cancelled; }});
    }
    assert(cancelled == 996 && ran == 0 && peak == 1);
    // A completion barrier behind the surviving work proves actual execution.
    std::promise<void> complete;
    // Release then wait using a task without evicting the four survivors.
    release.set_value();
    last.get_future().wait();
    worker.submit({[&] { complete.set_value(); }, {}});
    complete.get_future().wait();
  }
  assert(live == 0 && peak == 1 && ran == 4 && cancelled == 996);
  assert((order == std::vector<int>{996, 997, 998, 999}));
  puts("bounded worker: 1000 requests, 1 active + 4 pending, 996 obsolete jobs discarded, newest completed");

  // Destruction drops queued work and joins the running operation.
  std::atomic<int> disposed{0};
  std::promise<void> entered, stop;
  auto stop_gate = stop.get_future().share();
  auto worker = std::make_unique<BoundedWorker>(4);
  worker->submit({[&] { entered.set_value(); stop_gate.wait(); }, {}});
  entered.get_future().wait();
  worker->submit({[] { assert(false); }, [&] { ++disposed; stop.set_value(); }});
  worker.reset();
  assert(disposed == 1);

  // Clearing unheard meter readings keeps the visual state but prevents the
  // previous passage's queued loud beat from entering the next song.
  MusicLight light;
  light.feed(1, 0, 0.5, 0.2);
  light.discard_readings();
  light.step(1.3, 1.0 / 30, true, false);
  assert(light.rings_started() == 0 && light.frame().waves.empty());
  light.feed(1.4, 0, 0.5, 0);
  light.step(1.4, 1.0 / 30, true, false);
  assert(light.rings_started() == 1);
  puts("background lifecycle: shutdown and obsolete music readings passed");
}
