#include "../src/core/async_worker.h"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <vector>

int main() {
  std::promise<void> entered, release, finished;
  auto gate = release.get_future().share();
  std::atomic<int> cancelled{0}, ran{0};
  {
    AsyncWorker worker(1);
    assert(worker.submit({[&] { entered.set_value(); gate.wait(); }, {}, {}}));
    entered.get_future().wait();
    for (int i=0;i<1000;++i)
      assert(worker.submit({[&,i] { assert(i==999); ++ran; finished.set_value(); }, [&] { ++cancelled; }, {}}, true));
    assert(cancelled==999 && ran==0);
    release.set_value(); finished.get_future().wait();
  }
  assert(ran==1);

  std::promise<void> started, unblock, exited;
  auto blocked=unblock.get_future().share();
  auto worker=std::make_unique<AsyncWorker>(2);
  worker->submit({[&] {started.set_value();blocked.wait();exited.set_value();}, {}, {}});
  started.get_future().wait();
  std::atomic<int> discarded{0};
  assert(worker->submit({[] {assert(false);},[&] {++discarded;}, {}}));
  assert(worker->submit({[] {assert(false);},[&] {++discarded;}, {}}));
  assert(!worker->submit({[] {assert(false);}, {}, {}}));
  // Destruction must return before the blocked IO finishes, without a dangling owner.
  worker.reset(); assert(discarded==2);
  unblock.set_value(); exited.get_future().wait();

  // Obsolete navigation jobs release their slots before another slow IO finishes.
  std::promise<void> busy, resume, latest;
  auto hold=resume.get_future().share();std::atomic<bool> stale{false};
  AsyncWorker pruned(1);
  pruned.submit({[&]{busy.set_value();hold.wait();},{},{}});busy.get_future().wait();
  assert(pruned.submit({[]{assert(false);},[&]{++discarded;},[&]{return stale.load();}}));
  stale=true;
  assert(pruned.submit({[&]{latest.set_value();},{},{}}));
  assert(discarded==3);resume.set_value();latest.get_future().wait();

  AsyncWorker fifo(128);
  std::promise<void> first, go, end;
  auto pause=go.get_future().share();std::vector<int> order;
  fifo.submit({[&]{first.set_value();pause.wait();},{},{}});first.get_future().wait();
  for(int i=0;i<128;++i)assert(fifo.submit({[&,i]{order.push_back(i);if(i==127)end.set_value();},{},{}}));
  assert(!fifo.submit({[]{assert(false);},{},{}}));
  go.set_value();end.get_future().wait();
  for(int i=0;i<128;++i)assert(order[i]==i);
  std::cout << "async IO: 1000 latest-only requests => 999 cancelled, 1 completed; 128 FIFO limit/order; nonblocking destruction passed\n";
}
