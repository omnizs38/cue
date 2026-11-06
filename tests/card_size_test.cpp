// A song's card at every interface size: its window keeps a mini player's sizes (440 to 640 px at
// 100%: it grew with the large sizes for a day, a giant mini player), and at the large sizes its
// pixels step down until it fits. The title used to vanish there (no room above the playback),
// the controls climbed under the window's buttons and the time ran past the window's edge.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <tuple>
#include "../src/core/ui_scale.h"
#define private public
#include "../src/ui/play.h"

static void check(unsigned dpi, int choice, int cw, int ch, double& worst) {
  const double s = mini_scale(dpi, cw, ch, choice, true), ps = s;  // (as the app: App::scale_for, play_scale)
  PlayView p;
  p.in.audio_only = p.in.cover_scene = p.in.cover_art = p.in.has_cover = true;
  p.in.mini_player = true; p.in.dur = 250; p.in.time = 126;
  p.in.music_title = "Dream Sequence"; p.in.music_artist = "Jane Remover"; p.in.music_scene = MusicScene::wide();
  for (uint32_t i = 1; i <= 10; ++i) p.in.queue.push_back({i, "song " + std::to_string(i) + ".mp3", i == 3, true});
  p.in.queue_active = true; p.in.queue_revision = 1;
  p.set_base_scale(s); p.resize(cw, ch, ps);
  double t = 1; p.reset(t);
  for (int i = 0; i < 30; ++i) { p.changed(t); p.activity(t); t += 1 / 60.; p.tick(t, 1 / 60.); }
  assert(p.card_);
  // the title and the artist, whole, in the room above the playback
  const Layer& M = p.layers[PlayView::L_MUSIC];
  assert(M.visible && p.music_base_.h >= (int)std::lround(24 * ps) - 1);
  assert(p.music_base_.w >= text_w("Dream Sequence", ps) - 1 || p.music_base_.w >= (int)(60 * ps));
  // the playback under the window's strip, the words above it
  const Rect d = p.dock_rect();
  assert(d.y >= (int)std::lround(PlayView::kStrip * ps) - 2);
  assert(M.y + p.music_base_.h <= d.y + (int)std::lround(p.card_rows_[0] * ps));
  // all of it inside the window
  assert(d.x >= 0 && d.r() <= cw && d.b() <= ch && M.x + M.c.w <= cw);
  worst = std::min(worst, ps / ui_base_scale(dpi, choice));
}

int main() {
  double worst = 9;
  int cases = 0;
  for (unsigned dpi : {96u, 120u, 144u, 168u, 192u, 240u, 288u})
    for (int choice : {-1, 2, 0, 3}) {
      const double k = dpi / 96.0;
      for (int lng : {440, 480, 560, 640}) {
        const int cw = (int)std::lround(lng * k), ch = (int)std::lround(cw / 1.9);
        double w = 9; check(dpi, choice, cw, ch, w); ++cases;
        // the small and medium sizes are the card as it was: its pixels the size chosen
        if (choice == -1 || choice == kDefaultUiSize) assert(w >= 1.0 - 1e-9);
        worst = std::min(worst, w);
      }
    }
  std::printf("Card size: %d cards at seven DPIs and four sizes, a mini player's window at all of them, the title always whole above the playback, the controls under the window's buttons (smallest pixels %.2f of the chosen) passed\n", cases, worst);
}
