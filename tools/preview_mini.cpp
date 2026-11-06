// The mini player as the interface draws it (Linux, no engine): a song's card, its list and its
// menu, and a film's mini row, at the sizes Windows gives them. The engine's picture is stood in
// for by a flat cover square, so only the interface's own layers are judged here.
// Build (from the repository root):
//   g++ -std=c++20 -O1 -o build/preview_mini tools/preview_mini.cpp src/ui/*.cpp src/core/*.cpp
//   ./build/preview_mini out [dpi] [ui size: -1 small, 2 medium, 0 large, 3 larger] [card widths, in px at 100%]
#include <cstdio>
#include <memory>
#include <cstdlib>
#include <string>
#include <vector>

#include "../src/core/i18n.h"
#include "../src/core/ui_scale.h"
#include "../src/ui/play.h"

static void save_canvas(const Canvas& c, const std::string& name) {
  std::string raw = "/tmp/preview_mini.rgba";
  FILE* f = std::fopen(raw.c_str(), "wb");
  if (!f) return;
  std::vector<unsigned char> buf((size_t)c.w * c.h * 4);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i];
    unsigned a = v >> 24;
    auto un = [&](unsigned x) { return a ? std::min(255u, (x * 255 + a / 2) / a) : 0u; };
    buf[i * 4 + 0] = (unsigned char)un((v >> 16) & 255);
    buf[i * 4 + 1] = (unsigned char)un((v >> 8) & 255);
    buf[i * 4 + 2] = (unsigned char)un(v & 255);
    buf[i * 4 + 3] = (unsigned char)a;
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

// the room behind the card (the engine's dim light) and the cover where the card puts it
static void shot(PlayView& pv, double& now, const std::string& name) {
  run(pv, now, 0.7);
  pv.prepare(now);
  Canvas fr;
  fr.resize(pv.in.w, pv.in.h);
  for (int y = 0; y < fr.h; ++y)
    for (int x = 0; x < fr.w; ++x) {
      int r = 22 + 30 * x / fr.w, g = 18 + 14 * x / fr.w, b = 14;
      fr.px[(size_t)y * fr.w + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
  Rect cv = pv.card_cover();
  if (cv.w > 0) fr.fill(cv.x, cv.y, cv.w, cv.h, Rgb{96, 70, 48});
  for (auto& L : pv.layers) if (L.visible) fr.blit(L.c, L.x, L.y);
  save_canvas(fr, name);
}

int main(int argc, char** argv) {
  i18n::set_language(i18n::Lang::PtBR);
  std::string out = argc > 1 ? argv[1] : "out";
  const unsigned dpi = argc > 2 ? (unsigned)std::atoi(argv[2]) : 144;
  const double k = dpi / 96.0;
  const int ui = argc > 3 ? std::atoi(argv[3]) : kDefaultUiSize;
  std::vector<int> lengths{480, 640};
  if (argc > 4) { lengths.clear(); for (int i = 4; i < argc; ++i) lengths.push_back(std::atoi(argv[i])); }
  // the card at its first size (480 px at 100%, the song's 1.9:1) and at its largest (640)
  for (int lng : lengths) {
    const int cw = (int)std::lround(lng * k), ch = (int)std::lround(lng * k / 1.9);
    // the app's scale for the mini player: the window's own, never under the screen's base
    const double s = mini_scale(dpi, cw, ch, ui, true), ps = s;
    PlayView pv;
    pv.in.audio_only = true; pv.in.cover_scene = true; pv.in.cover_art = true; pv.in.has_cover = true;
    pv.in.mini_player = true;
    pv.in.dur = 250; pv.in.time = 126; pv.in.volume = 70;
    pv.in.music_title = "Dream Sequence";
    pv.in.music_artist = "Jane Remover";
    const char* names[] = {"Jane Remover - Beauty Sleep (320 Kbps).mp3", "Ice Palace Adolescence.mp3",
                           "Jane Remover - Dream Sequence (audio).mp3", "Jane Remover - Music Baby (audio).mp3",
                           "Number Teddie - DNA.mp3", "Jane Remover - Flash in the Pan.mp3", "Frailty.mp3",
                           "Jane Remover - Psychedelic Switch.mp3", "Jane Remover - Magic I Want U.mp3", "Holiday.mp3"};
    for (uint32_t i = 0; i < 10; ++i) pv.in.queue.push_back({i + 1, names[i], i == 2, true});
    pv.in.queue_active = true; pv.in.has_next = pv.in.has_prev = true;
    pv.in.queue_revision = 1;
    pv.in.w = cw; pv.in.h = ch;
    pv.set_base_scale(s);
    pv.resize(cw, ch, ps);
    double now = 1;
    pv.reset(now);
    pv.changed(now);
    std::string tag = std::to_string(lng) + "-" + std::to_string(dpi) + (ui == kDefaultUiSize ? "" : "-ui" + std::to_string(ui));
    std::printf("card %dx%d s=%.2f ps=%.2f\n", cw, ch, s, ps);
    pv.set_hover_for_preview(cw * 3 / 4, ch - 20, now);
    shot(pv, now, out + "/mini-card-" + tag + ".png");
    // the spectrum-alone look: the cover drawn by Cue over the card's square (a stand-in picture)
    {
      auto art = std::make_shared<CoverArt>();
      const int side = pv.card_cover().w;
      art->side = side; art->px.resize((size_t)side * side);
      for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
          const int r = 40 + 150 * x / side, g = 60 + 90 * y / side, bl = 140 - 80 * x / side;
          art->px[(size_t)y * side + x] = 0xFF000000u | (r << 16) | (g << 8) | bl;
        }
      pv.in.card_art = art; pv.in.card_art_show = true; pv.changed(now);
      shot(pv, now, out + "/mini-card-drawn-" + tag + ".png");
      pv.in.card_art.reset(); pv.in.card_art_show = false; pv.changed(now);
    }
    pv.open_panel_for_preview(10, now);
    pv.set_hover_for_preview(cw * 3 / 4, ch / 2, now);
    shot(pv, now, out + "/mini-list-" + tag + ".png");
    pv.key("esc", false, false, now);
    run(pv, now, 0.5);
    pv.open_panel_for_preview(3, now);
    shot(pv, now, out + "/mini-menu-" + tag + ".png");
  }
  // a film's mini player: one row on its bottom edge, and its menu
  {
    const int cw = (int)std::lround(540 * k), ch = (int)std::lround(540 * k * 9 / 16);
    const double s = mini_scale(dpi, cw, ch, ui, false);
    PlayView pv;
    pv.in.mini_player = true; pv.in.dur = 3883; pv.in.time = 1472; pv.in.volume = 70;
    Track a{1, "sub", "por", "", "subrip"};
    pv.in.subs = {a}; pv.in.sid = 1; pv.in.kind = SubKind::Text;
    pv.in.w = cw; pv.in.h = ch;
    pv.set_base_scale(s);
    pv.resize(cw, ch, s);
    double now = 1;
    pv.reset(now); pv.changed(now);
    pv.set_hover_for_preview(cw / 2, ch - 10, now);
    std::printf("film %dx%d s=%.2f\n", cw, ch, s);
    shot(pv, now, out + "/mini-film-" + std::to_string(dpi) + (ui == kDefaultUiSize ? "" : "-ui" + std::to_string(ui)) + ".png");
    pv.open_panel_for_preview(3, now);
    shot(pv, now, out + "/mini-film-menu-" + std::to_string(dpi) + (ui == kDefaultUiSize ? "" : "-ui" + std::to_string(ui)) + ".png");
    pv.key("esc", false, false, now);
    run(pv, now, 0.5);
    // its subtitles: the short panel (on, which track by its arrows, how big)
    pv.in.subs = {a, Track{2, "sub", "eng", "English (SDH)", "subrip"}, Track{3, "sub", "spa", "", "ass"}};
    pv.in.mini_sub_size = 1;
    pv.changed(now);
    pv.open_panel_for_preview(0, now);
    shot(pv, now, out + "/mini-film-subs-" + std::to_string(dpi) + ".png");
    pv.key("esc", false, false, now);
    run(pv, now, 0.5);
    // without the bar's background (a choice in Settings): the buttons in white over the picture
    pv.in.controls_free = true;
    pv.changed(now);
    pv.resize(cw, ch, s);
    pv.set_hover_for_preview(cw / 2, ch - 10, now);
    shot(pv, now, out + "/mini-film-free-" + std::to_string(dpi) + ".png");
  }
}
