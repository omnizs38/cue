// The home draws again only the eyes' corner when nothing else changed (App::present_home):
// random sessions (the pointer anywhere, blinks and thoughts over long idles, hovers, keys, menus,
// drags with the mouth open, new rows and pictures, notices, updates, what's new, the survey,
// the window's buttons, other sizes, languages, the palette, reduced motion, leaving and coming
// back, "clear recent") are drawn the way the app does it (whole frames compared with the last,
// or the eyes' corner alone when the key allows) and, beside them, as whole new frames every
// time. The two windows must be the same, pixel for pixel, at every frame.
#include "../src/ui/home.h"
#include "../src/core/i18n.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>

static std::shared_ptr<HomeThumb> picture(std::mt19937& rng, bool cover) {
  auto t = std::make_shared<HomeThumb>();
  t->w = cover ? 64 : 96; t->h = cover ? 64 : 54; t->cover = cover;
  t->px.resize((size_t)t->w * t->h);
  const uint32_t base = rng() & 0xFFFFFF;
  for (size_t i = 0; i < t->px.size(); ++i) t->px[i] = 0xFF000000u | ((base + (uint32_t)i * 2654435761u) & 0xFFFFFF);
  return t;
}

static std::vector<HomeRow> some_rows(std::mt19937& rng) {
  const char* names[] = {"Cena", "Twin Peaks", "música", "palestra", "livro", "日本語のタイトル", "a very long title that will not fit anywhere at all"};
  const int n = (int)(rng() % 7);
  std::vector<HomeRow> r;
  for (int i = 0; i < n; ++i) {
    HomeRow row;
    row.name = names[rng() % 7];
    row.right = rng() % 2 ? "faltam 41 min" : "4 min";
    row.progress = rng() % 3 ? (rng() % 100) / 100.0 : -1;
    row.sub = rng() % 2 ? "S01E04 · ontem" : "hoje";
    row.audio = rng() % 3 == 0;
    row.kind = (HomeKind)(rng() % 5);
    row.seed = "c:\\v\\" + std::to_string(rng() % 50) + ".mkv";
    r.push_back(row);
  }
  return r;
}

int main() {
  std::mt19937 rng(20261112);
  long frames = 0, partial = 0;
  struct Size { int w, h; double s; } sizes[] = {{1280, 720, 2}, {960, 540, 1.5}, {640, 360, 1}, {1920, 1080, 3}, {700, 900, 2}, {1600, 900, 2.5}};
  for (int session = 0; session < 36; ++session) {
    i18n::set_language(session % 3 == 0 ? i18n::Lang::PtBR : session % 3 == 1 ? i18n::Lang::En : i18n::Lang::Ja);
    reduce_motion() = session % 7 == 3;
    set_high_contrast(session % 9 == 4);
    HomeView hv;
    Size sz = sizes[session % 6];
    hv.set_size(sz.w, sz.h, sz.s);
    hv.set_rows(some_rows(rng));
    double now = 1 + session;
    hv.enter(now);
    Canvas shown, back, whole, scratch;  // the app's two canvases; the reference; clear's scratch
    uint64_t key_of_shown = 0;
    int cover_until = -1;
    for (int step = 0; step < 900; ++step) {
      if (step == cover_until) hv.cover_list(false);
      // something happens (often nothing: the eyes alone follow a pointer)
      // (stretches where only the pointer moves, as on a real home, between bursts of everything)
      const bool calm = (step / 60) % 2 == 1;
      const int e = calm ? (int)(rng() % 41) : (int)(rng() % 100);
      const int x = (int)(rng() % (unsigned)(hv.w * 2)) - hv.w / 2, y = (int)(rng() % (unsigned)(hv.h * 2)) - hv.h / 2;
      if (e < 40) hv.track(x, y, now);
      else if (e < 48) hv.mouse_move(std::clamp(x, 0, hv.w - 1), std::clamp(y, 0, hv.h - 1), now);
      else if (e < 50) hv.mouse_leave(now);
      else if (e < 52) { static const char* keys[] = {"tab", "down", "up", "escape", "enter", "right"}; hv.key(keys[rng() % 6], rng() % 2, now); }
      else if (e < 53) hv.mouse_down(std::clamp(x, 0, hv.w - 1), std::clamp(y, 0, hv.h - 1), now);
      else if (e < 55) hv.drag_over(std::clamp(x, 0, hv.w - 1), std::clamp(y, 0, hv.h - 1), now);
      else if (e < 56) hv.drag_leave(now);
      else if (e < 57) hv.set_rows(some_rows(rng));
      else if (e < 58 && !hv.rows.empty()) hv.set_thumb((int)(rng() % hv.rows.size()), picture(rng, rng() % 2), now);
      else if (e < 59) hv.show_notice(rng() % 2 ? "não deu pra abrir" : "", now);
      else if (e < 60) { hv.update_version = rng() % 2 ? "1.4.0" : ""; hv.update_progress = rng() % 2 ? (rng() % 100) / 100.0 : -1; hv.update_busy = rng() % 2; }
      else if (e < 61) hv.news = !hv.news;
      else if (e < 62) hv.survey = rng() % 4 == 0;
      else if (e < 63) { hv.chrome.maximized = rng() % 2; hv.chrome.pinned = rng() % 2; hv.chrome.hot = (int)(rng() % 5) - 1; hv.chrome.vis.target = rng() % 2; }
      else if (e < 64) hv.engine_problem = rng() % 4 == 0 ? "o motor não abriu" : "";
      else if (e < 65) hv.language = (int)(rng() % 9) - 1;
      else if (e < 66) { Size n = sizes[rng() % 6]; hv.set_size(n.w, n.h, n.s); }
      else if (e < 67) hv.leave(now, rng() % 2);
      else if (e < 68) hv.cancel_leave(now);
      else if (e < 69) hv.enter(now, rng() % 2);
      else if (e < 70 && !hv.rows.empty()) hv.clear_animated(now, &scratch);
      else if (e < 71) { i18n::set_language(rng() % 2 ? i18n::Lang::De : i18n::Lang::PtBR); hv.relayout(); }
      else if (e < 72) { hv.cover_list(true); cover_until = step + 20; }  // (the Settings panel over the list for a moment)
      // time passes: one display frame, or a long idle (blinks, a thought, the eyes giving up following)
      const double dt = e % 13 == 0 ? 0.5 + (rng() % 4000) / 1000.0 : 1 / 60.0;
      now += dt;
      hv.tick(now, std::min(0.05, dt));
      // the app: the eyes' corner alone when the key allows, else a whole frame compared with the last
      const uint64_t key = hv.eyes_only_key(now);
      if (key && key == key_of_shown && shown.w == hv.w && shown.h == hv.h && !shown.px.empty()) {
        const Rect r = hv.face_reach();
        assert(r.w > 0 && r.h > 0 && r.x >= 0 && r.y >= 0 && r.r() <= hv.w && r.b() <= hv.h);
        shown.set_clip(r);
        hv.render(shown, now);
        shown.reset_clip();
        ++partial;
      } else {
        key_of_shown = key;
        hv.render(back, now);
        std::swap(shown, back);
      }
      // the reference: a whole new frame, every time
      hv.render(whole, now);
      ++frames;
      if (whole.px != shown.px) {
        size_t i = 0;
        while (i < whole.px.size() && whole.px[i] == shown.px[i]) ++i;
        std::printf("differ: session %d step %d at (%zu, %zu), %dx%d s %.1f\n", session, step, i % (size_t)whole.w, i / (size_t)whole.w, hv.w, hv.h, hv.s);
        return 1;
      }
    }
  }
  set_high_contrast(false);
  reduce_motion() = false;
  // the point of it: most frames of a home where the eyes follow the pointer are the eyes' corner alone
  assert(partial > frames / 4);
  // and the corner is small: at 4K, under 5% of the window (3.9%)
  {
    HomeView hv;
    hv.set_size(3840, 2160, 3);
    const Rect r = hv.face_reach();
    assert(r.w > 0 && (double)r.w * r.h < 0.05 * 3840 * 2160);
  }
  std::printf("Home partial frames: %ld frames in 36 random sessions, %ld drawn as the eyes' corner alone, every one identical to a whole new frame passed\n",
              frames, partial);
}
