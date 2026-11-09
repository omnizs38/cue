#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

// One active operation and a finite FIFO. When saturated, replace the oldest
// waiting operation; never interrupt codec/OS calls already running. Callbacks
// must not throw. Lazy owners create this only when work is first requested.
class BoundedWorker {
 public:
  struct Task { std::function<void()> run, cancel; };
  explicit BoundedWorker(size_t limit) : limit_(limit ? limit : 1), worker_([this] { loop(); }) {}
  BoundedWorker(const BoundedWorker&) = delete;
  BoundedWorker& operator=(const BoundedWorker&) = delete;
  ~BoundedWorker() {
    std::deque<Task> abandoned;
    {
      std::lock_guard lock(mu_);
      stopped_ = true; abandoned.swap(pending_);
    }
    for (auto& task : abandoned) if (task.cancel) task.cancel();
    cv_.notify_one();
    worker_.join();
  }
  void submit(Task task) {
    Task replaced;
    {
      std::lock_guard lock(mu_);
      pending_.push_back(std::move(task));
      if (pending_.size() > limit_) { replaced = std::move(pending_.front()); pending_.pop_front(); }
    }
    if (replaced.cancel) replaced.cancel();
    cv_.notify_one();
  }
 private:
  void loop() {
    for (;;) {
      Task task;
      {
        std::unique_lock lock(mu_);
        cv_.wait(lock, [this] { return stopped_ || !pending_.empty(); });
        if (stopped_) return;
        task = std::move(pending_.front()); pending_.pop_front();
      }
      task.run();
    }
  }
  size_t limit_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<Task> pending_;
  bool stopped_ = false;
  std::thread worker_;
};
