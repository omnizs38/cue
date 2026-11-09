// Portable UI cost only: no Windows, engine, audio decoder, shaders or display.
// Link against the portable objects produced by tools/test.sh.
#include "../src/ui/play.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
static size_t allocations = 0;
void* operator new(size_t n) { ++allocations; if (auto p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
int main() {
  for (bool audio : {false, true}) for (bool free : {false, true}) {
    PlayView p;
    p.in.audio_only = audio; p.in.cover_scene = audio; p.in.has_cover = audio;
    p.in.controls_free = free; p.in.dur = 240;
    p.in.music_title = "A deliberately long song title to exercise the metadata cache during animation";
    p.in.music_artist = "An artist name longer than the small string optimization";
    p.in.music_scene = MusicScene::wide();
    p.resize(1280,720,2); p.reset(0);
    double t = 0;
    auto step = [&] { t += 1./60; p.in.time = t; p.changed(t); p.activity(t); p.tick(t,1./60); };
    for (int i=0;i<120;++i) step();
    for (int pass=0;pass<5;++pass) {
      size_t before = allocations;
      auto start = std::chrono::steady_clock::now();
      for (int i=0;i<3600;++i) step();
      double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      auto count = allocations - before;
      uint64_t digest = 1469598103934665603ull;
      for (const auto& l : p.layers) {
        for (auto v : l.c.px) { digest ^= v; digest *= 1099511628211ull; }
        digest ^= l.visible; digest *= 1099511628211ull;
        digest ^= l.x; digest *= 1099511628211ull;
        digest ^= l.y; digest *= 1099511628211ull;
      }
      std::printf("%s free=%d pass=%d frames=3600 ms=%.3f allocations=%zu pixels=%016llx\n",audio?"audio":"video",free,pass,ms,count,(unsigned long long)digest);
    }
  }
}
