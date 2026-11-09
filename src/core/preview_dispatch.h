#pragma once
#include <algorithm>
#include <cstdint>

// Finish one navigation decode, then take the latest destination. Restarting a slow
// seek on every pointer update can starve it indefinitely. Colour work may be preempted.
struct PreviewDispatch {
  static constexpr double kInterval = 1.0 / 30.0;
  double sent_at = -1;
  uint64_t epoch = 0;
  void reset() { sent_at = -1; epoch = 0; }
  void begin(uint64_t interaction) {
    if (epoch != interaction) { epoch = interaction; sent_at = -1; }
  }
  double delay(double now, bool busy, bool colour) const {
    if (busy && !colour) return -1;
    if (busy && colour) return 0;  // interactive work preempts background colour sampling
    return sent_at < 0 ? 0 : std::max(0.0, kInterval - (now - sent_at));
  }
  void sent(double now) { sent_at = now; }
};
