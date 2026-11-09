#include "../src/core/preview_protocol.h"
#include "../src/core/preview_timeline.h"
#include "../src/core/frame_cache.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cstdio>

int main() {
  assert(!std::strcmp(preview_seek_mode(false), "absolute+keyframes"));
  assert(!std::strcmp(preview_seek_mode(true), "absolute+exact"));
  // Keep the diagnostic, the worker and the UI on the same explicit wire layout.
  assert(sizeof(PreviewHeader) == 56 && offsetof(PreviewHeader, t) == 24);
  assert(offsetof(PreviewHeader, decode_ms) == 32 && offsetof(PreviewHeader, flags) == 48);
  FrameCache cache;
  FrameCache::Image image; image.w = 2; image.h = 2;
  image.px.assign(4, 0xFFFF0000u); image.sample_time = 8;
  const int bucket = PreviewTimeline::key(10.9);
  assert(cache.put(bucket, std::move(image)));
  // A navigation sample is stable across this interval and retains its ACTUAL time,
  // rather than masquerading as the requested 10.9 s exact frame.
  auto at = cache.find(PreviewTimeline::key(10.1));
  assert(at != cache.end() && at->second.sample_time == 8);
  assert(at->second.px[0] == 0xFFFF0000u);
  assert(cache.find(PreviewTimeline::key(11.1)) == cache.end());
  cache.touch(bucket);
  assert(cache.find(bucket)->second.sample_time == 8);
  cache.clear();
  assert(cache.empty() && cache.retained_bytes() == 0);
  puts("Preview policy: fast navigation, precise colour mode, actual sample time, stable cache and wire layout passed");
}
