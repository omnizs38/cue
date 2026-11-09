#pragma once
#include <cstdint>

// Replacing/stopping a previous file can leave its STOP event queued ahead of
// FILE_LOADED. It is not a failure of the new load; a stop while seeking is.
class PreviewWait {
 public:
  enum class Event { Other, Loaded, Seek, Restart, Stop, End, Shutdown };
  enum class Result { Pending, Ready, Failed };
  explicit PreviewWait(bool loading, bool require_seek = false) : loading_(loading), seek_seen_(!require_seek) {}
  Result consume(Event event) {
    if (event == Event::Seek) seek_seen_ = true;
    if (!loading_ && event == Event::Restart && !seek_seen_) return Result::Pending;
    if (event == (loading_ ? Event::Loaded : Event::Restart)) return Result::Ready;
    if (event == Event::Shutdown || event == Event::End || (!loading_ && event == Event::Stop)) return Result::Failed;
    return Result::Pending;
  }
 private:
  bool loading_;
  bool seek_seen_;
};

// All asynchronous preview replies belong to exactly one media load.
struct PreviewSession {
  uint32_t generation = 0, request = 0;
  bool loaded = false, busy = false, failed = false;
  int misses = 0;
  void reset() {
    if (++generation == 0) ++generation;
    ++request;
    loaded = busy = failed = false;
    misses = 0;
  }
  bool accepts(uint32_t media) const { return generation != 0 && media == generation; }
  bool accepts_frame(uint32_t media, uint32_t shot) const { return accepts(media) && shot == request; }
};
