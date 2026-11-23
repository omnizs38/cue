// (1.4.1) start_detached: the work runs once on its own thread, as std::thread(...).detach() did
// (and under Wine, tools/wine-tests.sh, through CreateThread: its Windows branch).
#include "../src/core/detached_thread.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

int main() {
  std::atomic<int> done{0}, live{0};
  // many short workers made and let go while others end: the pattern that crashed with std::thread
  std::vector<std::thread> makers;
  for (int m = 0; m < 4; ++m)
    makers.emplace_back([&] {
      for (int i = 0; i < 500; ++i) {
        while (live.load() > 32) std::this_thread::yield();
        ++live;
        start_detached([&] { ++done; --live; });
      }
    });
  for (auto& t : makers) t.join();
  for (int i = 0; i < 2000 && done.load() < 2000; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  assert(done.load() == 2000);
  // a worker may own what cannot be copied
  std::atomic<int> got{0};
  auto p = std::make_unique<int>(7);
  start_detached([q = std::move(p), &got] { got = *q; });
  for (int i = 0; i < 400 && !got.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  assert(got.load() == 7);
  puts("detached workers: 2000 short ones while others end, a move-only one passed");
}
