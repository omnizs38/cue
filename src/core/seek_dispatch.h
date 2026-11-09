#pragma once

// One queued keyframe seek, one replaceable destination. A rejected enqueue
// has no completion event, so it must never acquire the busy slot.
class SeekDispatch {
 public:
  template<class Send> void request(double time, Send send) {
    if (busy_) pending_ = time;
    else busy_ = send(time);
  }
  template<class Send> void complete(Send send) {
    busy_ = false;
    if (pending_ >= 0) {
      const double time = pending_;
      pending_ = -1;
      busy_ = send(time);
    }
  }
  void clear_pending() { pending_ = -1; }
 private:
  bool busy_ = false;
  double pending_ = -1;
};
