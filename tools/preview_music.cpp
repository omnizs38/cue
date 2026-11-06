// Music in the full player as the interface frames it (Linux, no engine): the engine's picture is
// stood in for by a sketch of the scene (the cover, its dot wings and the room's light, the way
// blur_shader.h draws it, inside the picture's own frame; outside it, the window's background),
// framed by music_view exactly as App does, and the interface's layers drawn over it. Window
// sizes from the reports: the roomy window, short wide ones, a small one, a tall one.
// Build (from the repository root):
//   g++ -std=c++20 -O1 -o build/preview_music tools/preview_music.cpp src/ui/*.cpp src/core/*.cpp
//   ./build/preview_music out [dpi]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../src/core/audio_mode.h"
#include "../src/core/i18n.h"
#include "../src/core/ui_scale.h"
#include "../src/ui/play.h"

static void save_canvas(const Canvas& c, const std::string& name) {
  std::string raw = "/tmp/preview_music.rgba";
  FILE* f = std::fopen(raw.c_str(), "wb");
  if (!f) return;
  std::vector<unsigned char> buf((size_t)c.w * c.h * 4);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i];
    buf[i * 4 + 0] = (unsigned char)((v >> 16) & 255);
    buf[i * 4 + 1] = (unsigned char)((v >> 8) & 255);
    buf[i * 4 + 2] = (unsigned char)(v & 255);
    buf[i * 4 + 3] = 255;
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

// the engine's picture, sketched: black room, the light around the cover (blur_shader.h's room
// pass), the wings' dots and the cover; outside the picture the window's background `bgc`, with
// the edge pass's fade toward it
static bool g_no_wings = false;  // the list's header: the wing pass steps them out
static Canvas scene(const MusicScene& S, const MusicView& v, int w, int h, Rgb room, Rgb bgc, bool fade_to_bg) {
  Canvas c;
  c.resize(w, h);
  const double x0 = v.ox, y0 = v.oy, x1 = v.ox + S.W * v.f, y1 = v.oy + S.H * v.f;
  const double cx = v.ox + (S.x + S.side / 2.0) * v.f, cy = v.oy + (S.y + S.side / 2.0) * v.f, hh = S.side / 2.0 * v.f;
  MusicVisible vis = music_visible(S, v, w, h);
  // the edge fade widths, as App::update_blur_cloud computes them
  double e[4] = {0, 0, 0, 0};
  {
    const double gx0 = v.ox + S.left_x() * v.f, gx1 = v.ox + (S.right_x() + S.wing_w) * v.f;
    const double gy0 = v.oy + std::min(S.y, S.wing_y()) * v.f, gy1 = v.oy + std::max(S.y + S.side, S.wing_y() + S.wing_h) * v.f;
    const double base = 0.24 * std::min(vis.w, vis.h);
    auto width = [&](bool bar, double gap, double size) { return bar ? std::clamp(std::min(base, gap * 0.85), 0.0, size) / size : 0.0; };
    e[0] = width(vis.x > 0.5, gx0 - vis.x, vis.w);
    e[1] = width(vis.x + vis.w < w - 0.5, vis.x + vis.w - gx1, vis.w);
    e[2] = width(vis.y > 0.5, gy0 - vis.y, vis.h);
    e[3] = width(vis.y + vis.h < h - 0.5, vis.y + vis.h - gy1, vis.h);
  }
  auto sm = [](double a, double b, double x) { double t = std::clamp((x - a) / (b - a), 0.0, 1.0); return t * t * (3 - 2 * t); };
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      double r = bgc.r / 255.0, g = bgc.g / 255.0, b = bgc.b / 255.0;
      if (x >= x0 && x < x1 && y >= y0 && y < y1 && x >= vis.x && x < vis.x + vis.w && y >= vis.y && y < vis.y + vis.h) {
        double qx = x - cx, qy = y - cy;
        double d = std::hypot(std::max(std::fabs(qx) - hh * 0.55, 0.0), std::max(std::fabs(qy) - hh * 0.55, 0.0)) / hh;
        double gl = 0.36 * std::exp(-(d / 2.0) * (d / 2.0)) + 0.15 * std::exp(-(d / 9.0) * (d / 9.0));
        double lr = room.r / 255.0 * gl, lg = room.g / 255.0 * gl, lb = room.b / 255.0 * gl;
        double px = (x - vis.x) / vis.w, py = (y - vis.y) / vis.h, k = 1;
        if (e[0] > 0) k *= sm(0, e[0], px);
        if (e[1] > 0) k *= sm(0, e[1], 1 - px);
        if (e[2] > 0) k *= sm(0, e[2], py);
        if (e[3] > 0) k *= sm(0, e[3], 1 - py);
        if (fade_to_bg) { r = r + (lr - r) * k; g = g + (lg - g) * k; b = b + (lb - b) * k; }
        else { r = lr * k; g = lg * k; b = lb * k; }
        // the wings: dots, brighter toward the cover's middle
        const double sx = (x - v.ox) / v.f, sy = (y - v.oy) / v.f;
        bool wing = sy >= S.wing_y() && sy < S.wing_y() + S.wing_h &&
                    ((sx >= S.left_x() && sx < S.left_x() + S.wing_w) || (sx >= S.right_x() && sx < S.right_x() + S.wing_w));
        if (wing && !g_no_wings) {
          double dx = sx < S.x ? (S.left_x() + S.wing_w - sx) : (sx - S.right_x());
          double reach = S.wing_w * (0.95 - 0.8 * std::fabs(sy - (S.wing_y() + S.wing_h / 2.0)) / (S.wing_h / 2.0));
          int gx = (int)std::floor(sx / 11), gy = (int)std::floor(sy / 11);
          bool gap = std::fmod(sx, 11.0) < 2 || std::fmod(sy, 11.0) < 2;
          if (dx < reach && !gap && ((gx + gy) % 7) != 0) { r = g = b = 0.72; }
        }
        if (std::fabs(qx) < hh && std::fabs(qy) < hh) {
          // Cue's tile: dark square, a pixel note
          r = g = b = 0.075;
          double ux = (qx + hh) / (2 * hh), uy = (qy + hh) / (2 * hh);
          if ((ux > 0.45 && ux < 0.5 && uy > 0.3 && uy < 0.7) || (ux > 0.33 && ux < 0.5 && uy > 0.6 && uy < 0.72) ||
              (ux > 0.5 && ux < 0.6 && uy > 0.3 && uy < 0.36))
            r = g = b = 0.82;
        }
      }
      auto q = [](double u) { return (uint32_t)std::lround(std::clamp(u, 0.0, 1.0) * 255); };
      c.px[(size_t)y * w + x] = 0xFF000000u | (q(r) << 16) | (q(g) << 8) | q(b);
    }
  return c;
}

static Canvas compose(PlayView& pv, const Canvas& base) {
  Canvas fr = base;
  for (auto& L : pv.layers) if (L.visible) fr.blit(L.c, L.x, L.y);
  return fr;
}

int main(int argc, char** argv) {
  i18n::set_language(i18n::Lang::PtBR);
  std::string out = argc > 1 ? argv[1] : "out";
  const unsigned dpi = argc > 2 ? (unsigned)std::atoi(argv[2]) : 144;
  const bool tint = argc > 3 && std::atoi(argv[3]) != 0;
  const bool free_bar = argc > 4 && std::atoi(argv[4]) != 0;  // the bar without its background
  struct Win { const char* name; int w, h; };
  // physical px at 150%: the roomy window, a short wide strip, a wider strip, a small one, tall
  const Win wins[] = {{"roomy", 2160, 1215}, {"strip", 2340, 520}, {"band", 2100, 680}, {"wide", 2400, 900},
                      {"small", 960, 620}, {"tall", 900, 1300}, {"tiny", 760, 300},
                      // the reports' windows (physical px at 150%)
                      {"report-wide", 1910, 1370}, {"report-small", 1180, 1060}, {"report-narrow", 650, 1530}, {"report-square", 1790, 1620}};
  for (const Win& win : wins) {
    const int cw = win.w, ch = win.h;
    const double s = ui_scale(dpi, cw, ch, kDefaultUiSize);
    MusicFrame fr = music_frame_for(cw, ch, s);
    double ps = s;  // (music keeps the screen's size: App::play_scale)
    if (music_frame_stacked(fr)) ps = std::max(ps, music_compact_scale(s, cw, ch, kDefaultUiSize, ui_base_scale(dpi, kDefaultUiSize)));
    PlayView pv;
    pv.in.audio_only = true; pv.in.cover_scene = true; pv.in.cover_art = true; pv.in.has_cover = true;
    pv.in.controls_free = free_bar;
    pv.in.dur = 190; pv.in.time = 89; pv.in.volume = 70;
    pv.in.music_title = std::string(win.name).rfind("report", 0) == 0 ? "DNA" : "Ice Palace Adolescence";
    pv.in.music_artist = std::string(win.name).rfind("report", 0) == 0 ? "Number Teddie" : "Jane Remover";
    const char* names[] = {"Jane Remover - Beauty Sleep (320 Kbps).mp3", "Ice Palace Adolescence.mp3",
                           "Jane Remover - Dream Sequence (audio).mp3", "Jane Remover - Music Baby (audio).mp3",
                           "Number Teddie - DNA.mp3"};
    for (uint32_t i = 0; i < 5; ++i) pv.in.queue.push_back({i + 1, names[i], i == 1, true});
    pv.in.queue_active = true; pv.in.has_next = pv.in.has_prev = true;
    pv.in.queue_revision = 1;
    pv.in.w = cw; pv.in.h = ch;
    pv.set_base_scale(s);
    pv.resize(cw, ch, ps);
    double now = 1;
    pv.reset(now);
    pv.changed(now);
    pv.set_hover_for_preview(cw / 2, ch - 20, now);
    // two passes, like App: the framing needs the bar's place, the bar's layout the framing
    MusicView v;
    for (int pass = 0; pass < 3; ++pass) {
      run(pv, now, 0.4);
      int top = ch;
      Rect d = pv.blur_cloud().controls;
      if (d.h > 0 && d.cy() > ch / 2) top = d.y;
      if (fr == MusicFrame::Centre) v = music_view(pv.in.music_scene, fr, cw, ch, top, pv.music_centre_below(), pv.music_stack_above(), -1, pv.music_list_beside());
      else if (fr != MusicFrame::Scene && pv.music_stacked()) v = music_view(pv.in.music_scene, fr, cw, ch, top, pv.music_stack_below(), pv.music_stack_above());
      else v = music_view(pv.in.music_scene, fr, cw, ch, top, -1, 0, pv.music_words_w());
      pv.in.music_view = v;
      pv.changed(now);
    }
    run(pv, now, 0.7);
    std::printf("%s %dx%d s=%.1f ps=%.1f frame=%d f=%.3f ox=%.0f oy=%.0f\n", win.name, cw, ch, s, ps, (int)fr, v.f, v.ox, v.oy);
    const Rgb room{150, 152, 168};
    const Rgb bgc = tint ? music_backdrop(room, 1.0) : Rgb{0, 0, 0};
    Canvas base = scene(pv.in.music_scene, v, cw, ch, room, bgc, tint);
    save_canvas(compose(pv, base), out + "/music-" + win.name + ".png");
    pv.open_panel_for_preview(10, now);
    for (int pass = 0; pass < 3; ++pass) {  // the composition makes room for the list (App's glide, landed)
      run(pv, now, 0.3);
      int top = ch;
      Rect d = pv.blur_cloud().controls;
      if (d.h > 0 && d.cy() > ch / 2) top = d.y;
      if (Rect lc = pv.list_cover(); lc.w > 0) {  // the list's header: the cover is its thumbnail (App::music_view_target)
        v.frame = fr; v.f = lc.w / (double)pv.in.music_scene.side;
        v.ox = lc.x - pv.in.music_scene.x * v.f; v.oy = lc.y - pv.in.music_scene.y * v.f;
      } else if (fr == MusicFrame::Centre) v = music_view(pv.in.music_scene, fr, cw, ch, top, pv.music_centre_below(), pv.music_stack_above(), -1, pv.music_list_beside());
      else if (fr != MusicFrame::Scene && pv.music_stacked()) v = music_view(pv.in.music_scene, fr, cw, ch, top, pv.music_stack_below(), pv.music_stack_above());
      else v = music_view(pv.in.music_scene, fr, cw, ch, top, -1, 0, pv.music_words_w());
      pv.in.music_view = v;
      pv.changed(now);
    }
    run(pv, now, 0.6);
    g_no_wings = pv.list_cover().w > 0;
    base = scene(pv.in.music_scene, v, cw, ch, room, g_no_wings ? Rgb{0, 0, 0} : bgc, tint);
    g_no_wings = false;
    Canvas veiled = base;
    for (auto& px : veiled.px) {  // the veil pass: the picture steps back
      const double k = 1 - 0.86 * pv.list_veil();
      px = 0xFF000000u | ((uint32_t)(((px >> 16) & 255) * k) << 16) | ((uint32_t)(((px >> 8) & 255) * k) << 8) | (uint32_t)((px & 255) * k);
    }
    save_canvas(compose(pv, veiled), out + "/music-" + win.name + "-list.png");
    // the list cleared while open: it stays music's list, ending on its line that adds songs
    if (std::string(win.name) == "roomy" || std::string(win.name) == "band") {
      pv.in.queue.clear(); pv.in.queue_active = false; ++pv.in.queue_revision; pv.in.has_next = pv.in.has_prev = false;
      pv.changed(now);
      for (int pass = 0; pass < 3; ++pass) {
        run(pv, now, 0.3);
        int top = ch;
        Rect d = pv.blur_cloud().controls;
        if (d.h > 0 && d.cy() > ch / 2) top = d.y;
        if (fr == MusicFrame::Centre) v = music_view(pv.in.music_scene, fr, cw, ch, top, pv.music_centre_below(), pv.music_stack_above(), -1, pv.music_list_beside());
        else v = music_view(pv.in.music_scene, fr, cw, ch, top, -1, 0, pv.music_words_w());
        pv.in.music_view = v;
        pv.changed(now);
      }
      run(pv, now, 0.6);
      base = scene(pv.in.music_scene, v, cw, ch, room, bgc, tint);
      save_canvas(compose(pv, base), out + "/music-" + win.name + "-empty.png");
    }
  }
}
