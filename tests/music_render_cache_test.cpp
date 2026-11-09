#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>
#include <array>
#define private public
#include "../src/ui/play.h"
#undef private

int main() {
  // Field boundaries matter: metadata containing newlines and adjacent numeric
  // framing/count values must not alias a different song or layout.
  PlayView::MusicKey key;
  std::array<int64_t, 7> values{1280,720,2000000,'b',1000,2,0};
  assert(key.update("title\nartist", "name", values));
  assert(!key.update("title\nartist", "name", values));
  assert(key.update("title", "artist\nname", values));
  for (size_t i=0; i<values.size(); ++i) {
    auto changed = values; ++changed[i];
    assert(key.update("title", "artist\nname", changed));
    assert(!key.update("title", "artist\nname", changed));
    assert(key.update("title", "artist\nname", values));
  }
  key.clear();
  assert(key.update("title", "artist\nname", values));

  // Test real composition, including late tags and invalidation after language
  // refresh/layout changes. Stable animation ticks reuse the drawn text surface.
  for (bool mini : {false,true}) for (bool free : {false,true}) {
    PlayView p;
    p.in.audio_only = p.in.cover_scene = p.in.has_cover = true;
    p.in.mini_player = mini; p.in.controls_free = free;
    p.in.music_title = "title"; p.in.music_artist = "artist";
    p.in.music_scene = MusicScene::wide(); p.in.dur = 240;
    p.resize(mini ? 500 : 1280, mini ? 300 : 720, 2);
    p.reset(0);
    double t = 0;
    auto settle = [&] { for (int i=0;i<100;++i) { t+=1./60; p.changed(t); p.tick(t,1./60); } };
    settle();
    const auto first = p.music_gen_;
    assert(first > 0);
    settle(); assert(p.music_gen_ == first);
    p.in.music_artist = "late artist metadata";
    settle(); assert(p.music_gen_ > first);
    const auto changed = p.music_gen_;
    p.music_key_.clear(); settle(); assert(p.music_gen_ > changed);
  }
  puts("music cache: stable full/mini text surfaces, both styles, late metadata and independent fields passed");
}
