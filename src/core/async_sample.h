#pragma once
#include <cstdint>

// One engine read in flight, even across invalidation. Old replies free the slot
// but cannot feed a new file, a resumed animation or a seek destination.
class AsyncSample {
 public:
  uint64_t begin(double now) {
    if (pending_) return 0;
    pending_ = ++serial_; generation_sent_ = generation_; sent_ = now;
    return pending_;
  }
  bool finish(uint64_t id, double now) {
    if (!id || id != pending_) return false;
    pending_ = 0;
    return generation_sent_ == generation_ && now >= sent_ && now - sent_ <= 0.2;
  }
  void invalidate() { ++generation_; }
  bool busy() const { return pending_ != 0; }
  double sent() const { return sent_; }
 private:
  uint64_t serial_ = 1ull << 48, pending_ = 0, generation_ = 0, generation_sent_ = 0;
  double sent_ = 0;
};
