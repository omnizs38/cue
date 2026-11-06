#pragma once
#include <cstdint>

// Observer events can lag a click, or be coalesced away (A -> B -> A).
// Only the latest command reply confirms a choice, even when sid never changes.
class SubtitleSelection {
 public:
  int64_t shown(int64_t observed) const { return target_ >= 0 ? target_ : observed; }
  void preview(int64_t id) { target_ = id; }
  uint64_t begin(int64_t id) { target_ = id; pending_ = true; return ++request_; }
  bool pending() const { return pending_; }
  bool finish(uint64_t request) {
    if (!pending_ || request != request_) return false;
    pending_ = false; target_ = -1;
    return true;
  }
  void reset() { ++request_; pending_ = false; target_ = -1; }
  static bool is_reply(uint64_t request) { return (request >> 40) == 1; }

 private:
  uint64_t request_ = 1ull << 40;
  int64_t target_ = -1;
  bool pending_ = false;
};
