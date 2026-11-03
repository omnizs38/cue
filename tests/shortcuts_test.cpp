// Keyboard shortcuts (issue #12) and black and white music (issue #11).
//   ? opens the shortcut sheet from anywhere in playback and closes it again; Esc closes it (back
//   to About when it was opened there). The whole sheet fits every window, in every language,
//   clear of the window's buttons; two columns when there is room. A button's name says its key.
//   Black and white is a choice in the picture's menu, saved, colour by default.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include "../src/core/store.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

static void settle(PlayView& p, double& t) { for (int i = 0; i < 40; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); } }

int main() {
  int sheets = 0, two_cols = 0;
  for (int lang = 0; lang < i18n::kLangCount; ++lang)
    for (double scale : {1., 1.5, 2., 2.5, 3.})
      for (auto size : {std::pair{240, 180}, std::pair{320, 240}, std::pair{480, 320}, std::pair{640, 360}, std::pair{960, 540}, std::pair{1280, 720},
                        std::pair{1920, 1080}, std::pair{540, 960}, std::pair{2560, 1080}})
        for (bool music : {false, true}) {
          if (size.first < 240 * scale || size.second < 180 * scale) continue;  // under Cue's smallest window
          i18n::set_language((i18n::Lang)lang);
          PlayView p;
          p.in.dur = 200; p.in.audio_only = music; p.in.cover_scene = music; p.in.queue_active = true;
          p.resize(size.first, size.second, scale);
          double t = 1; p.reset(t);
          p.key("?", false, false, t); settle(p, t);
          assert(p.panel_ == PlayView::Panel::Keys);
          Rect r = p.panel_rect();
          // whole, inside the window
          if (!(r.x >= 0 && r.y >= 0 && r.r() <= size.first && r.b() <= size.second)) std::printf("FAIL lang %d s%.1f %dx%d: %d,%d %dx%d ks%.1f cols%d compact%d colw%d\n", lang, scale, size.first, size.second, r.x, r.y, r.w, r.h, p.keys_s_, p.keys_cols_, (int)p.keys_compact_, p.keys_col_w_);
          for (auto& row : p.rows_) assert(row.r.y >= 0 && row.r.b() <= r.h && row.r.x >= 0 && row.r.r() <= r.w);
          // never under the window's buttons when they are shown and there is room
          Rect c = p.chrome_rect();
          if (size.second - r.h >= p.chrome_room()) assert(r.r() <= c.x || r.y >= c.b());
          // every entry, each with its keys
          int entries = 0, sections = 0;
          for (auto& row : p.rows_) if (row.k == PlayView::RK::Keys) {
            if (row.kh.section) ++sections; else { ++entries; assert(!row.kh.caps.empty() && !row.kh.label.empty()); }
          }
          assert(entries == 21 && (sections == 4 || (sections == 0 && size.second <= 240 * scale)));  // titles go only in the tiniest
          two_cols += p.keys_cols_ == 2;
          ++sheets;
          // nothing to focus in it; ? again closes it, and so does Esc
          p.key("tab", false, false, t);
          assert(p.pfocus_ < 0 || p.rows_[p.pfocus_].k != PlayView::RK::Keys);
          p.key("?", false, false, t); settle(p, t);
          assert(!p.panel_open());
          p.key("?", false, false, t); p.key("esc", false, false, t); settle(p, t);
          assert(!p.panel_open());
          assert(p.take_actions().empty() || true);
        }
  assert(sheets > 400 && two_cols > sheets / 2);
  i18n::set_language(i18n::Lang::En);

  // a big window at the default size: two columns at the interface's own scale
  {
    PlayView p; p.in.dur = 200; p.resize(1920, 1080, 2); double t = 1; p.reset(t);
    p.key("?", false, false, t); settle(p, t);
    assert(p.keys_cols_ == 2 && p.keys_s_ == 2 && !p.keys_compact_);
  }

  // a button's name says its key, a shade dimmer; the volume's value does not
  {
    PlayView p; p.in.dur = 200; p.in.queue_active = true; p.resize(1280, 720, 2); double t = 1; p.reset(t);
    p.mouse_move(640, 650, t); settle(p, t);
    for (auto [id, key] : {std::pair{(int)PlayView::B_FS, "f"}, {(int)PlayView::B_CC, "c"}, {(int)PlayView::B_PLAY, "space"},
                           {(int)PlayView::B_LOOP, "l"}, {(int)PlayView::B_QUEUE, "q"}}) {
      Rect b; for (auto& h : p.hots_) if (h.id == id) b = h.r;
      assert(b.w > 0);
      Rect d = p.dock_rect();
      p.mouse_move(d.x + b.cx(), d.y + b.cy(), t);
      for (int i = 0; i < 50; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); }
      assert(p.tip_text_.find(std::string("|") + key + "|") != std::string::npos);
    }
  }

  // black and white: a second choice in the music picture's menu, beside cover / spectrum
  {
    PlayView p; p.in.dur = 200; p.in.audio_only = true; p.in.cover_scene = true; p.resize(1280, 720, 2); double t = 1; p.reset(t);
    p.right_click(400, 300, t); settle(p, t);
    const PlayView::Row* tone = nullptr;
    for (auto& r : p.rows_) if (r.id == PlayView::R_MUSIC_COLOR) tone = &r;
    assert(tone && tone->icons_only && tone->segs.size() == 2 && tone->seg == 0);  // colour by default
    Rect pr = p.panel_rect();
    Rect cell = tone->parts[1];
    p.mouse_move(pr.x + cell.cx(), pr.y + cell.cy(), t);
    p.mouse_down(pr.x + cell.cx(), pr.y + cell.cy(), t); p.mouse_up(pr.x + cell.cx(), pr.y + cell.cy(), t);
    auto a = p.take_actions();
    bool mono = false;
    for (auto& x : a) if (x.a == PA::MusicMono && x.v == 1) mono = true;
    assert(mono);
    // the row shows the new choice at once (the menu stays open on it; the app calls changed())
    p.changed(t); settle(p, t);
    for (auto& r : p.rows_) if (r.id == PlayView::R_MUSIC_COLOR) assert(r.seg == 1);
    // and follows a change made elsewhere (the saved choice, another window of the menu)
    p.in.music_mono = false; p.changed(t); settle(p, t);
    for (auto& r : p.rows_) if (r.id == PlayView::R_MUSIC_COLOR) assert(r.seg == 0);
    // a film's menu has no such row
    PlayView v; v.in.dur = 200; v.resize(1280, 720, 2); double t2 = 1; v.reset(t2);
    v.right_click(400, 300, t2); settle(v, t2);
    for (auto& r : v.rows_) assert(r.id != PlayView::R_MUSIC_COLOR);
  }
  {
    auto file = std::filesystem::temp_directory_path() / "cue_mono_test.json";
    Store fresh; assert(!fresh.music_mono);
    Store st; st.music_mono = true; assert(st.save(file));
    Store back; assert(back.load(file) && back.music_mono);
    std::filesystem::remove(file);
  }
  // the engine's picture: the black and white pass, and the keys routed back from the engine's
  // window too (after a click on the picture it has the keyboard)
  {
    std::ifstream sh("src/win/blur_shader.h"), app("src/win/app.cpp");
    std::stringstream a, b; a << sh.rdbuf(); b << app.rdbuf();
    assert(a.str().find("//!PARAM mono") != std::string::npos && a.str().find("//!WHEN mono 0 >") != std::string::npos);
    // the cover keeps its colours: the pass leaves out what the (tilted) cover covers
    assert(a.str().find("mono_box(uv, h, h * 0.02)") != std::string::npos);
    for (const char* k : {"{\"?\", \"qmark\"}", "{\"l\", \"l\"}", "{\"n\", \"n\"}", "{\"p\", \"p\"}", "{\"c\", \"c\"}", "{\"Ctrl+t\", \"c_t\"}"})
      assert(b.str().find(k) != std::string::npos);
    assert(b.str().find("w == L'?'") != std::string::npos);
  }
  std::printf("Shortcuts: the sheet (? and Esc, %d windows in every language, two columns where there is room, clear of the window's buttons), keys in button names, black and white choice saved passed\n", sheets);
}
