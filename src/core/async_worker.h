#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

// A bounded IO lane. The thread owns its state, never its owner. Destruction
// cancels queued tasks without waiting for an OS call on a disconnected drive.
// Callbacks must own their captures and must not throw. At most one task runs.
class AsyncWorker {
 public:
  struct Task { std::function<void()> run, cancel; std::function<bool()> obsolete; };
  explicit AsyncWorker(size_t limit) : state_(std::make_shared<State>(limit ? limit : 1)) {
    std::thread([s = state_] {
      for (;;) {
        Task task;
        {
          std::unique_lock lock(s->mu);
          s->cv.wait(lock, [&] { return s->stopped || !s->pending.empty(); });
          if (s->stopped) return;
          task = std::move(s->pending.front()); s->pending.pop_front();
        }
        task.run();
      }
    }).detach();
  }
  AsyncWorker(const AsyncWorker&) = delete;
  AsyncWorker& operator=(const AsyncWorker&) = delete;
  ~AsyncWorker() {
    std::deque<Task> dropped;
    {
      std::lock_guard lock(state_->mu);
      state_->stopped = true; dropped.swap(state_->pending);
    }
    for (auto& task : dropped) if (task.cancel) task.cancel();
    state_->cv.notify_one();
  }
  void clear_pending() {
    std::deque<Task> dropped;
    { std::lock_guard lock(state_->mu); dropped.swap(state_->pending); }
    for (auto& task : dropped) if (task.cancel) task.cancel();
  }
  // Ordinary queues reject overload; replace_waiting is for latest-only discovery.
  bool submit(Task task, bool replace_waiting = false) {
    std::deque<Task> dropped;
    bool accepted = false;
    {
      std::lock_guard lock(state_->mu);
      if (state_->stopped) return false;
      for (auto it = state_->pending.begin(); it != state_->pending.end();) {
        if (replace_waiting || (it->obsolete && it->obsolete())) {
          dropped.push_back(std::move(*it)); it = state_->pending.erase(it);
        } else ++it;
      }
      if (state_->pending.size() < state_->limit) { state_->pending.push_back(std::move(task)); accepted = true; }
    }
    for (auto& old : dropped) if (old.cancel) old.cancel();
    state_->cv.notify_one();
    return accepted;
  }
 private:
  struct State {
    explicit State(size_t count) : limit(count) {}
    const size_t limit;
    std::mutex mu;
    std::condition_variable cv;
    std::deque<Task> pending;
    bool stopped = false;
  };
  std::shared_ptr<State> state_;
};
