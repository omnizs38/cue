// The player's menus as the interface draws them (Linux, no engine): the picture's menu (right
// click), "..." and Settings with each of its pictures (the row under the pointer picks it).
// Build (from the repository root):
//   g++ -std=c++20 -O1 -o build/preview_menus tools/preview_menus.cpp src/ui/*.cpp src/core/*.cpp
//   ./build/preview_menus out [scale]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"

static void save_canvas(const Canvas& c, const std::string& name) {
  std::string raw = "/tmp/preview_menus.rgba";
  FILE* f = std::fopen(raw.c_str(), "wb");
  if (!f) return;
  std::vector<unsigned char> buf((size_t)c.w * c.h * 4);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i];
    buf[i * 4 + 0] = (unsigned char)((v >> 16) & 255); buf[i * 4 + 1] = (unsigned char)((v >> 8) & 255);
    buf[i * 4 + 2] = (unsigned char)(v & 255); buf[i * 4 + 3] = 255;
  }
  std::fwrite(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  std::string cmd = "convert -size " + std::to_string(c.w) + "x" + std::to_string(c.h) + " -depth 8 rgba:" + raw + " " + name;
  if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "convert failed\n");
  std::printf("%s\n", name.c_str());
}
static void run(PlayView& pv, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; pv.tick(now, 1 / 60.0); pv.take_actions(); }
}
static void shot(PlayView& pv, double& now, const std::string& name) {
  run(pv, now, 0.5);
  Canvas fr;
  fr.resize(pv.in.w, pv.in.h);
  for (int y = 0; y < fr.h; ++y)
    for (int x = 0; x < fr.w; ++x) {
      int r = 30 + 40 * x / fr.w, g = 40 + 30 * y / fr.h, b = 60 + 30 * x / fr.w;
      fr.px[(size_t)y * fr.w + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
  for (auto& L : pv.layers) if (L.visible) fr.blit(L.c, L.x, L.y);
  save_canvas(fr, name);
}
// rests the pointer on the row with this id (its first segment, or a given one)
static void hover_row(PlayView& pv, double& now, int id, int part = -1) {
  Rect pr = pv.panel_rect();
  for (auto& r : pv.rows_)
    if (r.id == id) {
      int x = pr.x + r.r.x + r.r.w / 2, y = pr.y + r.r.y + r.r.h / 2;
      if (part >= 0 && part < (int)r.parts.size()) { x = pr.x + r.parts[part].cx(); y = pr.y + r.parts[part].cy(); }
      pv.mouse_move(x, y, now);
      return;
    }
}

int main(int argc, char** argv) {
  i18n::set_language(argc > 3 ? (i18n::Lang)std::atoi(argv[3]) : i18n::Lang::PtBR);
  std::string out = argc > 1 ? argv[1] : "out";
  const double s = argc > 2 ? std::atof(argv[2]) : 2;
  const int W = (int)(1280 * s / 2 * 1.5), H = (int)(720 * s / 2 * 1.5);
  PlayView pv;
  pv.in.dur = 6500; pv.in.time = 1550; pv.in.volume = 70;
  Track a{1, "sub", "por", "", "subrip"};
  pv.in.subs = {a}; pv.in.sid = 1; pv.in.kind = SubKind::Text;
  pv.in.smooth_available = true;
  pv.in.settings_examples = true;  // (the examples shown: the "i" in Settings' header)
  pv.in.w = W; pv.in.h = H;
  pv.set_base_scale(s);
  pv.resize(W, H, s);
  double now = 1;
  pv.reset(now); pv.changed(now);
  pv.set_hover_for_preview(W / 2, H - 20, now);
  run(pv, now, 0.3);
  pv.right_click(W / 3, H / 3, now);
  shot(pv, now, out + "/menu-context.png");
  pv.close_panel();
  run(pv, now, 0.3);
  pv.open_panel_for_preview(3, now);
  shot(pv, now, out + "/menu-more.png");
  pv.open_panel_for_preview(11, now);
  run(pv, now, 0.2);
  hover_row(pv, now, PlayView::R_AMBIENT);
  shot(pv, now, out + "/menu-settings-ambient.png");
  hover_row(pv, now, PlayView::R_SMOOTH);
  shot(pv, now, out + "/menu-settings-smooth.png");
  hover_row(pv, now, PlayView::R_UISCALE, 3);
  shot(pv, now, out + "/menu-settings-size.png");
  hover_row(pv, now, PlayView::R_CONTROLS_FREE, 1);
  shot(pv, now, out + "/menu-settings-bg.png");
  hover_row(pv, now, PlayView::R_LANGUAGE);
  shot(pv, now, out + "/menu-settings-language.png");
  hover_row(pv, now, PlayView::R_ABOUT);
  shot(pv, now, out + "/menu-settings-about.png");
  // About: a page of Settings, in its place
  for (int i = 0; i < (int)pv.rows_.size(); ++i) if (pv.rows_[i].id == PlayView::R_ABOUT) { pv.panel_click(i, -1, 0, now); break; }
  pv.mouse_move(10, 10, now);
  shot(pv, now, out + "/menu-about.png");
  pv.open_panel_for_preview(1, now);  // the subtitles' style
  hover_row(pv, now, PlayView::R_LINEGAP);
  shot(pv, now, out + "/menu-style.png");
  // music: the picture's menu, and Settings with its own first group
  {
    PlayView m;
    m.in.audio_only = true; m.in.cover_scene = true; m.in.cover_art = true; m.in.has_cover = true;
    m.in.dur = 250; m.in.time = 35; m.in.volume = 70; m.in.music_title = "Dream Sequence"; m.in.music_artist = "Jane Remover";
    m.in.settings_examples = true;
    m.in.w = W; m.in.h = H; m.set_base_scale(s); m.resize(W, H, s);
    double t = 1; m.reset(t); m.changed(t); m.set_hover_for_preview(W / 2, H - 20, t); run(m, t, 0.3);
    m.right_click(W / 3, H / 3, t);
    shot(m, t, out + "/music-context.png");
    m.close_panel(); run(m, t, 0.3);
    m.open_panel_for_preview(3, t);
    shot(m, t, out + "/music-more.png");
    m.open_panel_for_preview(11, t); run(m, t, 0.2);
    hover_row(m, t, PlayView::R_MUSIC_LOOK, 1);
    shot(m, t, out + "/music-settings-look.png");
    hover_row(m, t, PlayView::R_MUSIC_COLOR, 1);
    shot(m, t, out + "/music-settings-color.png");
    hover_row(m, t, PlayView::R_COVER_TILT);
    shot(m, t, out + "/music-settings-tilt.png");
  }
  // the examples off (the default): the settings alone
  pv.in.settings_examples = false;
  pv.open_panel_for_preview(11, now); run(pv, now, 0.2);
  hover_row(pv, now, PlayView::R_AMBIENT);
  shot(pv, now, out + "/menu-settings-plain.png");
  pv.in.settings_examples = true;
  // a short window: the settings without the picture
  pv.close_panel();
  pv.in.h = H / 2; pv.resize(W, H / 2, s);
  run(pv, now, 0.3);
  pv.open_panel_for_preview(11, now);
  shot(pv, now, out + "/menu-settings-short.png");
  // shorter windows, film and music, with and without the bar's background: the picture beside
  // the rows while the window is wide enough
  for (int music = 0; music < 2; ++music)
    for (double k : {0.62, 0.75, 0.9}) {
      PlayView m;
      if (music) {
        m.in.audio_only = true; m.in.cover_scene = true; m.in.cover_art = true; m.in.has_cover = true;
        m.in.music_title = "Music Baby"; m.in.music_artist = "Jane Remover";
      } else {
        m.in.subs = {a}; m.in.sid = 1; m.in.kind = SubKind::Text; m.in.smooth_available = true;
      }
      m.in.controls_free = true;
      m.in.settings_examples = true;
      m.in.dur = 250; m.in.time = 35; m.in.volume = 70;
      const int h = (int)(H * k);
      m.in.w = W; m.in.h = h; m.set_base_scale(s); m.resize(W, h, s);
      double t = 1; m.reset(t); m.changed(t); m.set_hover_for_preview(W / 2, h - 20, t); run(m, t, 0.3);
      m.open_panel_for_preview(11, t); run(m, t, 0.2);
      hover_row(m, t, PlayView::R_AMBIENT);
      shot(m, t, out + "/" + (music ? "music" : "menu") + "-settings-h" + std::to_string((int)(k * 100)) + ".png");
    }
}
