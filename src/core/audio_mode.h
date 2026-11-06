#pragma once
#include "model.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// Decide from streams, never from the container extension (an MP4 may be audio-only).
// Album art is not a film and should not suppress the live waveform.
inline int64_t audio_visual_track(const std::vector<Track>& tracks, int64_t preferred = -1,
                                 const std::string& language = {}) {
  int64_t first = 0, def = 0, lang = 0, saved = 0;
  for (const auto& t : tracks) {
    if (t.type == "video" && !t.albumart) return 0;
    if (t.type != "audio") continue;
    if (!first) first = t.id;
    if (t.is_default && !def) def = t.id;
    if (!language.empty() && t.lang == language && !lang) lang = t.id;
    if (t.id == preferred && (language.empty() || t.lang == language)) saved = t.id;
  }
  return saved ? saved : lang ? lang : def ? def : first;
}

// Music with its cover. The scene is one picture: the cover, sharp, with the spectrum as two
// wings beside it (the bass next to the cover, the highs reaching out), and behind everything
// a soft light made of the cover's own colours that swells with the music, like the ambient
// light around the window. The cover and its wings sit on the left and leave the right for the
// title and artist (drawn by the interface). Other window shapes frame this same picture (see
// MusicFrame). The UI and the shader read these numbers to find the cover on screen.
struct MusicScene {
  int W = 1920, H = 1080;
  // (the cover in the middle, across: every composition puts the words beside or under it on its
  // own, and a picture with the cover in its middle reaches as far on both sides of it, so the
  // room's light is the same left and right of the cover. With the cover at 406 the left of a
  // roomy window was the window's colour, the right the room's light: a lit band, a dark side)
  int side = 520, x = 700, y = 237;  // the cover
  int wing_w = 220, wing_h = 528, wing_gap = 36;
  int text_x = 1520, text_w = 380;    // the picture's own words' column (only where nothing sets them apart)
  // the cover's own picture inside flat black bars (a video's frame boxed into the file's
  // picture, see core/image_trim.h), as shares of the picture; all of it when there are none
  double crop_x = 0, crop_y = 0, crop_w = 1, crop_h = 1;
  bool cropped() const { return crop_x > 0 || crop_y > 0 || crop_w < 1 || crop_h < 1; }
  int wing_y() const { return y + side / 2 - wing_h / 2; }
  int left_x() const { return x - wing_gap - wing_w; }
  int right_x() const { return x + side + wing_gap; }
  static MusicScene wide() { return {}; }
};

// How the scene sits in the window. The engine always draws the wide scene; the window frames
// it with the engine's zoom and pan (no rebuild, no pause, live while an edge is dragged):
//   Centre: the cover with its wings in the middle, the words under it, the bar along the
//          window's foot (roomy windows); with the album's list open, the cover and its words
//          glide into the left half and the list takes the right
//   Scene: all of it, the words beside the cover (a wide, short window: the split composition)
//   Tall:  the cover with its wings, the words under it, the bar right under them (portrait windows)
//   Cover: the cover and its wings alone (small windows)
//   Card:  a song's mini player: the cover on the left, its words and the bar beside it, the
//          wings stepped out (the app frames it from the card's own layout)
enum class MusicFrame { Scene, Tall, Cover, Card, Centre };
// the close compositions: the cover, its words and a compact bar stand together as one group
inline bool music_frame_stacked(MusicFrame f) { return f == MusicFrame::Tall || f == MusicFrame::Cover; }
// screen = (ox, oy) + scene * f
struct MusicView {
  double f = 0, ox = 0, oy = 0;
  MusicFrame frame = MusicFrame::Scene;
  // the words beside the cover, on screen (x of their column and its width), when the window
  // sets them apart from the scene's own column (the split composition of a wide, short window);
  // < 0: where the scene puts them
  double wx = -1, ww = -1;
  bool split() const { return wx >= 0; }
  bool words_beside() const { return frame == MusicFrame::Scene; }
  bool words_under() const { return frame == MusicFrame::Tall; }
};
// A window at least this much wider than tall that the whole picture cannot cover: the split
// composition (the cover and its wings in the left half, the words in the right half)
constexpr double kMusicSplitRatio = 2.2;
// The words' column on screen: the split composition's own, or the scene's
inline double scene_words_x(const MusicScene& S, const MusicView& v) { return v.split() ? v.wx : v.ox + S.text_x * v.f; }
inline double scene_words_w(const MusicScene& S, const MusicView& v) { return v.split() ? v.ww : S.text_w * v.f; }
// w, h: the window; s: the interface's pixel scale (sizes below are in its art pixels)
inline MusicFrame music_frame_for(int w, int h, double s) {
  double aw = w / std::max(0.5, s), ah = h / std::max(0.5, s);
  // a wide, short window (a strip on the side of the screen): the words keep their place beside
  // the cover, half and half (see music_view), instead of the cover alone between dark sides
  if (aw >= ah * kMusicSplitRatio && aw >= 560 && ah >= 150) return MusicFrame::Scene;
  if (aw * 4 >= ah * 5 && aw >= 620 && ah >= 330) return MusicFrame::Centre;
  // square windows too keep the words, under the cover, whenever there is height for them
  if (ah >= aw * 0.8 && aw >= 280 && ah >= 460) return MusicFrame::Tall;
  return MusicFrame::Cover;
}
// controls_top: where the control bar begins (the cover stays above it)
// below >= 0 (Tall and Cover only): the stacked composition. The cover, then what goes under it
// as one group (its words and the bar: `below` px), the whole of it a little above the window's
// middle, under the window's top strip (`above` px). Nothing is pinned to the window's floor, so
// a tall window has no hole between the words and the bar.
// words_px (Scene): how wide the words beside the cover are drawn, when known; the composition is
// then centred on what is really there (the wings to the end of the title), not on the picture
// Centre: `below` is the words' height under the cover (with the air above them), `above` the
// window's top strip, `list_open` the album's list in the right half.
inline MusicView music_view(const MusicScene& S, MusicFrame m, int w, int h, int controls_top, double below = -1, double above = 0, double words_px = -1,
                            bool list_open = false) {
  MusicView v;
  v.frame = m;
  if (w <= 0 || h <= 0) return v;
  if (m == MusicFrame::Centre) {
    // The cover with its wings, centred over its words, the group in the middle of the room left
    // between the window's top strip and the bar (a touch more air under it than over it). The
    // cover keeps to under half the height (the room's light around it is part of the picture).
    // The list open: the group moves into the left half, a little smaller if the half needs it.
    const double span = (S.side + 2.0 * (S.wing_gap + S.wing_w)) / S.side;
    const double tall = std::max(S.side, S.wing_h) / (double)S.side;
    const double words = std::max(0.0, below);
    const double mg = std::min(w, h) * 0.035;
    const double top = above + h * 0.03, bottom = std::max(top + 40.0, std::min((double)controls_top, h - mg) - h * 0.035);
    const double room_w = list_open ? w * 0.5 - 2 * mg : w - 2 * mg;
    double side = std::min({room_w * 0.94 / span, ((bottom - top) - words) / tall, h * 0.45});
    side = std::max(side, 24.0);
    const double group = side * tall + words;
    const double y0 = top + std::max(0.0, (bottom - top - group) * 0.46) + side * (tall - 1) / 2;
    const double cx = list_open ? w * 0.25 + mg * 0.5 : w / 2.0;
    v.f = side / S.side;
    v.ox = cx - (S.x + S.side / 2.0) * v.f;
    v.oy = y0 - S.y * v.f;
    return v;
  }
  if (below >= 0 && m != MusicFrame::Scene) {
    const double span = (S.side + 2.0 * (S.wing_gap + S.wing_w)) / S.side;  // the cover with its wings, wide
    const double tall = std::max(S.side, S.wing_h) / (double)S.side;           // the wings reach a hair past it
    const double air = h * 0.05;
    double side = std::min({w * 0.92 / span, (h - above - below - 2 * air) / tall, m == MusicFrame::Tall ? w * 0.62 : w * 0.56});
    side = std::max(side, 24.0);
    const double group = side * tall + below;
    // the free height: a little more under the group than over it (the eye's middle sits high)
    const double top = above + std::max(0.0, h - above - group) * 0.44 + side * (tall - 1) / 2;
    v.f = side / S.side;
    v.ox = w / 2.0 - (S.x + S.side / 2.0) * v.f;
    v.oy = top - S.y * v.f;
    // a wide, short window: what the picture leaves dark on the left, the cover gives back by
    // sitting a little right of the middle (the bar under it follows)
    if (v.ox > 0 && S.W * v.f >= w) v.ox = std::max({0.0, v.ox - w * 0.10, w - S.W * v.f});
    return v;
  }
  if (m == MusicFrame::Scene) {
    // All of the picture fits; then it grows toward filling the window, so no black bar is left
    // where the light cannot reach, as far as the cover, its wings and the start of the words
    // (a long title ends in "..." at the window's edge) stay inside, above the control bar.
    // (never more than a third larger: past that the cover would crowd a very wide window)
    const double fit = std::min(w / (double)S.W, h / (double)S.H);
    const double fill = std::min(std::max(w / (double)S.W, h / (double)S.H), fit * 1.33);
    const double cx0 = S.left_x();
    const double cy0 = std::min(S.y, S.wing_y()), cy1 = std::max(S.y + S.side, S.wing_y() + S.wing_h);
    const double mg = std::min(w, h) * 0.035, mgy = std::max(mg, h * 0.07);
    const double floor_y = std::min(h - mgy, (double)controls_top - mg * 0.5);
    // the place for scale f: centred, still covering the window, the content inside the margins
    auto place = [&](double f, double& ox, double& oy, double slack = 0) {
      // the words' end: as drawn (their width does not scale with the picture), or a guess
      const double cx1 = words_px > 0 ? S.text_x + words_px / f : S.text_x + S.text_w * 0.55;
      auto axis = [&](double win, double size, double c0, double c1, double lo, double hi, double pref, double& o, double give = 0) {
        double a = (size >= win ? win - size : (win - size) / 2) - give, b = size >= win ? 0.0 : a + give;  // covering
        double ca = lo - c0 * f, cb = hi - c1 * f;                                          // content inside
        double l = std::max(a, std::min(ca, cb)), r = std::min(b, cb);
        if (ca > cb) return false;
        if (l > r) { if (size >= win && f > fit + 1e-9) return false; l = ca; r = cb; }  // at fit: content first
        o = std::clamp(pref, std::max(l, ca), std::min(r, cb));
        return true;
      };
      // across: the content centred (as far as the picture still covers the window); down: the
      // picture centred, as before
      const double pref_x = words_px > 0 ? (w - (cx1 - cx0) * f) / 2 - cx0 * f : (w - S.W * f) / 2;
      return axis(w, S.W * f, cx0, cx1, mg, w - mg, pref_x, ox, slack) && axis(h, S.H * f, cy0, cy1, mgy, floor_y, (h - S.H * f) / 2, oy);
    };
    // Split: a window so wide that no picture of the scene's shape covers it while the cover and
    // its wings stay above the bar. The cover with its wings as large as the height allows (above
    // the bar, inside the margins, at most half the width), the words beside it, level with the
    // cover, and the two centred together in the window as one group (the words as wide as they
    // are drawn: a short title does not leave the window's right half empty). The window's colour
    // (music_backdrop) and the edge pass make the rest one room with the picture's light.
    if (w >= h * kMusicSplitRatio) {
      const double gw = S.right_x() + S.wing_w - S.left_x(), gh = cy1 - cy0;
      const double top = std::max(mg, h * 0.06), bottom = std::min(h - mg, (double)controls_top - mg * 0.6);
      double f = std::min((w / 2.0 - 2 * mg) / gw, (bottom - top) / gh);
      if (f > 0 && S.W * std::max(fit, f) < w - 1) {
        f = std::max(f, 0.02);
        const double gap = std::max(2 * mg, gw * f * 0.08);
        const double room = w - 2 * mg - gw * f - gap;  // what the words may take
        const double words = std::clamp(words_px > 0 ? words_px : w * 0.3, 0.0, std::max(0.0, room));
        const double gx = mg + std::max(0.0, (w - 2 * mg - (gw * f + gap + words)) / 2);  // the group's left edge
        v.f = f;
        v.ox = gx - cx0 * f;
        v.oy = top + (bottom - top - gh * f) / 2 - cy0 * f;
        v.wx = gx + gw * f + gap;
        v.ww = std::max(0.0, std::min(room, w - mg - v.wx));
        return v;
      }
    }
    double ox, oy, lo = fit, hi = fill;
    if (!place(fit, ox, oy)) {  // not even the whole picture leaves room: as before, centred
      v.f = fit; v.ox = (w - S.W * fit) / 2; v.oy = (h - S.H * fit) / 2;
      return v;
    }
    if (place(fill, ox, oy)) lo = fill;
    else for (int i = 0; i < 24; ++i) { double mid = (lo + hi) / 2; if (place(mid, ox, oy)) lo = mid; else hi = mid; }
    // (the picture always covers the window: a picture stopping short left a black fade beside
    // the words, where the light should be)
    place(lo, ox, oy);
    v.f = lo; v.ox = ox; v.oy = oy;
    return v;
  }
  double room = std::clamp((double)controls_top, h * 0.45, (double)h);
  double side, centre;
  if (m == MusicFrame::Tall) { side = std::min(w * 0.70, room * 0.52); centre = room * 0.40; }
  else { side = std::min(w * 0.62, room * 0.74); centre = room * 0.5; }
  // the cover with both wings stays inside the window (3% from each side): the spectrum is
  // never cut at the window's edge, a narrow window gets a smaller cover instead
  const double span = (S.side + 2.0 * (S.wing_gap + S.wing_w)) / S.side;
  side = std::min(side, w * 0.94 / span);
  // the picture covers the whole window: the cover sits off the scene's middle (little picture
  // to its left), so in a wide window it grows just enough for the picture to reach every edge,
  // never so far that it would cut the wings or reach the control bar. Otherwise a black band
  // faded in at the window's side.
  {
    const double c = S.x + S.side / 2.0, cy = S.y + S.side / 2.0;
    double f_cover = std::max({(w / 2.0) / c, (w / 2.0) / (S.W - c), centre / cy, (h - centre) / (S.H - cy)});
    const double cap = std::min(w * 0.94 / span, std::min(room * 0.92, (double)controls_top - centre - room * 0.04) * 2.0);
    side = std::max(side, std::min(f_cover * S.side, cap));
  }
  v.f = std::max(0.05, side / S.side);
  v.ox = w / 2.0 - (S.x + S.side / 2.0) * v.f;
  v.oy = centre - (S.y + S.side / 2.0) * v.f;
  // a very wide, short window: what the picture still leaves dark on the left, the cover gives
  // back by sitting a little right of the middle (10% of the width at most; the rest fades softly)
  // (only while the picture still reaches the right edge; a picture narrower than the window
  // keeps the cover in the middle)
  if (v.ox > 0 && S.W * v.f >= w) v.ox = std::max({0.0, v.ox - w * 0.10, w - S.W * v.f});
  return v;
}
// The window's own colour around the music's picture (mpv's background-color, and what the edge
// pass fades toward): the room's light as it is far from the cover, in the cover's colours, so a
// window much wider or taller than the picture is one lit room instead of a picture between two
// black bars. room: the cover's average border colour; k: how far the room's light is up (0..1).
template <class Colour>
inline Colour music_backdrop(Colour room, double k) {
  k = std::clamp(k, 0.0, 1.0);
  // (the room pass gives 0.5 at the cover's edge, its wash about 0.13 at a window's far corner)
  auto c = [&](double v) { return (decltype(room.r))std::lround(std::min(255.0, v * 0.13 * k)); };
  return Colour{c(room.r), c(room.g), c(room.b)};
}
inline MusicView mix_views(const MusicView& a, const MusicView& b, double k) {
  MusicView v = b;
  v.f = a.f + (b.f - a.f) * k; v.ox = a.ox + (b.ox - a.ox) * k; v.oy = a.oy + (b.oy - a.oy) * k;
  // the words glide too (into or out of the split composition: from where the scene put them)
  if (a.split() || b.split()) {
    const MusicScene S;
    const double ax = a.split() ? a.wx : a.ox + S.text_x * a.f, bx = b.split() ? b.wx : b.ox + S.text_x * b.f;
    const double aw = a.split() ? a.ww : S.text_w * a.f, bw = b.split() ? b.ww : S.text_w * b.f;
    v.wx = ax + (bx - ax) * k; v.ww = aw + (bw - aw) * k;
  }
  return v;
}
// The engine's own words for that framing: it fits the video into the window (keeping its shape,
// centred), then zooms by 2^zoom and moves it by pan, in shares of the zoomed video's size.
struct MpvFraming { double zoom = 0, pan_x = 0, pan_y = 0; };
inline MpvFraming mpv_framing(const MusicScene& S, const MusicView& v, int w, int h) {
  MpvFraming m;
  if (v.f <= 0 || w <= 0 || h <= 0) return m;
  double f0 = std::min(w / (double)S.W, h / (double)S.H);
  m.zoom = std::log2(v.f / f0);
  double bx = (w - S.W * v.f) / 2, by = (h - S.H * v.f) / 2;
  m.pan_x = (v.ox - bx) / (S.W * v.f);
  m.pan_y = (v.oy - by) / (S.H * v.f);
  return m;
}
// The part of the picture the window shows (screen pixels): where the shader's coordinates live.
struct MusicVisible { double x = 0, y = 0, w = 0, h = 0; };
inline MusicVisible music_visible(const MusicScene& S, const MusicView& v, int w, int h) {
  double x0 = std::max(0.0, v.ox), y0 = std::max(0.0, v.oy);
  double x1 = std::min((double)w, v.ox + S.W * v.f), y1 = std::min((double)h, v.oy + S.H * v.f);
  return {x0, y0, std::max(0.0, x1 - x0), std::max(0.0, y1 - y0)};
}

// The cover the engine found for this file (embedded, or a cover.jpg beside it), if any.
inline int64_t audio_cover_track(const std::vector<Track>& tracks) {
  for (const auto& t : tracks)
    if (t.type == "video" && t.albumart) return t.id;
  return 0;
}

// Fed by [cue_wave] and the cover [vidN], at 60 pictures a second (showfreqs analyses a new
// window every 384 samples: overlap 0.92 of 4096 at 24 kHz; the decay is per picture). Costly
// steps work on tiny pictures: the wings are coloured and cut out cell by cell (20x48) before
// they are scaled; the light is 32x18. The cover is scaled once and repeated by the engine.
// Every two-input step is led by the music's stream and repeats its last picture when a side
// ends, so the end of a song leaves the scene as it was.
// A song without a cover of its own wears Cue's music tile: near-black, with the note of Cue's
// music icon in big pixels (26x26 cells drawn once, scaled up whole).
inline std::string audio_placeholder_cover() {
  static const char* note[] = {"...###..", "...#.##.", "...#..##", "...#...#", "...#....",
                               "...#....", ".###....", "####....", "####....", ".##....."};
  std::string g = "color=c=0x0e0e0e:s=26x26:r=25:d=0.04";
  for (int y = 0; y < 10; ++y)
    for (int x = 0; x < 8;) {
      if (note[y][x] != '#') { ++x; continue; }
      int run = 0;
      while (x + run < 8 && note[y][x + run] == '#') ++run;
      g += ",drawbox=x=" + std::to_string(9 + x) + ":y=" + std::to_string(8 + y) + ":w=" + std::to_string(run) + ":h=1:color=0xd2d2d2:t=fill";
      x += run;
    }
  return g + ",";
}

inline std::string audio_cover_tail(int64_t cover, const MusicScene& S = MusicScene{}) {
  auto n = [](int v) { return std::to_string(v); };
  // the cover: the file's own (the engine's album-art track), or Cue's tile
  std::string v = cover > 0 ? "[vid" + n((int)cover) + "]" : audio_placeholder_cover();
  if (cover > 0 && S.cropped()) {  // the bars stay out of the cover, its wings and its light
    char c[160];
    std::snprintf(c, sizeof c, "crop=w=iw*%.4f:h=ih*%.4f:x=iw*%.4f:y=ih*%.4f,", S.crop_w, S.crop_h, S.crop_x, S.crop_y);
    v += c;
  }
  return "[cue_wave]aresample=24000,volume=3,"
         "showfreqs=s=24x20:r=60:mode=bar:ascale=sqrt:fscale=log:win_size=4096:overlap=0.92:averaging=1:colors=white,"
         "trim=start_frame=2,format=gray,crop=24:19:0:0,pad=24:20:0:0:color=0x262626,"
         "lagfun=decay=0.965,split=2[cue_bars][cue_beat];"
         // the wings' cells: bars turned sideways, mirrored so the bass sits level with the cover's middle
         "[cue_bars]transpose=clock,split=2[cue_lo][cue_hi0];[cue_hi0]vflip[cue_hi];[cue_hi][cue_lo]vstack[cue_cells];" +
         v + "format=gbrp,split=2[cue_c1][cue_c2];"
         // the cover, sharp
         "[cue_c1]scale=" + n(S.side) + ":" + n(S.side) + ":force_original_aspect_ratio=increase:flags=" + (cover > 0 ? "lanczos" : "neighbor") + ","
         "crop=" + n(S.side) + ":" + n(S.side) + ",setsar=1[cue_art];"
         // each row of cells takes the colour of the cover's row beside it, lifted to stay bright,
         // and each cell is as opaque as it is lit: a fading cell lets the light behind it through
         // (it never turns into a dark hole over a bright cover's light); then the cells become dots
         // with transparent gaps
         "[cue_c2]scale=1:48:flags=area,lutrgb=r=val*0.6+100:g=val*0.6+100:b=val*0.6+100,scale=20:48:flags=neighbor[cue_tint];"
         // (the music leads: both of the cells' copies end with it, so the last picture keeps its
         // holes; an alphamerge led by the cover would paint solid wings once the song ends)
         "[cue_cells]split=2[cue_ca][cue_cb];[cue_ca][cue_cb]alphamerge[cue_cell];[cue_tint]format=gbrap[cue_tinta];"
         "[cue_cell][cue_tinta]blend=c0_mode=normal:c0_opacity=0:c1_mode=normal:c1_opacity=0:c2_mode=normal:c2_opacity=0:"
         "c3_mode=normal:c3_opacity=1,format=yuva444p,"
         "scale=" + n(S.wing_w) + ":" + n(S.wing_h) + ":flags=neighbor,format=yuva420p[cue_wing];"
         // the gaps between the dots: a grid drawn once and multiplied into the wings' alpha
         // (drawing the grid on every picture cost more than everything else on the wings)
         "color=c=white:s=" + n(S.wing_w) + "x" + n(S.wing_h) + ":r=25:d=0.04,format=yuva420p,"
         "drawgrid=w=11:h=11:t=2:c=black@0:replace=1[cue_grid];"
         "[cue_wing][cue_grid]blend=c0_mode=normal:c1_mode=normal:c2_mode=normal:c3_mode=multiply,"
         "split=2[cue_wr][cue_wl0];[cue_wl0]hflip[cue_wl];"
         // the room: black, led by the music (so it ends with the song) and made by padding a few
         // pixels, not by scaling a picture to the full size 60 times a second. The soft light
         // around the cover is drawn by the video shader, centred on the cover (blur_shader.h)
         "[cue_beat]crop=2:2:0:0,format=yuv420p,lutyuv=y=16:u=128:v=128,"
         "pad=" + n(S.W) + ":" + n(S.H) + ":0:0:black[cue_bg];"
         "[cue_bg][cue_wl]overlay=" + n(S.left_x()) + ":" + n(S.wing_y()) + ":format=yuv420[cue_o1];"
         "[cue_o1][cue_wr]overlay=" + n(S.right_x()) + ":" + n(S.wing_y()) + ":format=yuv420[cue_o2];"
         "[cue_o2][cue_art]overlay=" + n(S.x) + ":" + n(S.y) + ":format=yuv420,setsar=1[vo]";
}

// The spectrum alone, in the same scene: a pixel equalizer mirrored around its centre (the bass
// in the middle, higher frequencies spreading to both sides) over a faint reflection, standing
// where the cover stands, so the words, the list and the window's framing work the same way.
// Bars climb at once and fall slowly (lagfun keeps a decaying peak), on the same dot grid as the
// interface. Everything after showfreqs works on a 48x20 picture, then on 384x200 cells.
inline std::string audio_spectrum_tail(const MusicScene& S = MusicScene{}) {
  auto n = [](int v) { return std::to_string(v); };
  const int w = 768, h = 400;  // the 384x200 cells doubled: whole pixels
  int x = S.x + S.side / 2 - w / 2, y = S.y + S.side / 2 - h / 2;
  return "[cue_wave]aresample=24000,volume=3,"
         "showfreqs=s=24x20:r=60:mode=bar:ascale=sqrt:fscale=log:win_size=4096:overlap=0.92:averaging=1:colors=white,"
         // the analyser needs two frames after a flush; keep the previous image until then
         "trim=start_frame=2,format=gray,"
         // drop showfreqs' always-lit bottom row; a dim row of dots marks silence instead
         "crop=24:19:0:0,pad=24:20:0:0:color=0x2a2a2a,"
         "split=2[cue_r][cue_l];[cue_l]hflip[cue_lf];[cue_lf][cue_r]hstack,"
         "lagfun=decay=0.965,lutyuv=y=val*0.82,"
         "scale=384:160:flags=neighbor,drawgrid=w=8:h=8:t=2:c=black,"
         "split=2[cue_top][cue_ref];[cue_ref]vflip,crop=384:40:0:8,lutyuv=y=val*0.18[cue_mirror];"
         "[cue_top][cue_mirror]vstack,scale=" + n(w) + ":" + n(h) + ":flags=neighbor,"
         "pad=" + n(S.W) + ":" + n(S.H) + ":" + n(x) + ":" + n(y) + ":color=black,setsar=1,format=yuv420p[vo]";
}

// The visual for this file. look 0: the cover with its wings (the file's cover, or Cue's tile
// when cover is 0); look 1: the spectrum alone.
// The cover alone, for the lightest music: the same scene (the cover sharp where it always stands,
// the words beside it) over the soft, still light the shader draws around it; no spectrum and no
// light show. The picture changes only twice a second (it is led by the music, so it ends with
// the song): the cover is drawn once and repeated.
inline std::string audio_still_tail(int64_t cover, const MusicScene& S = MusicScene{}) {
  auto n = [](int v) { return std::to_string(v); };
  std::string v = cover > 0 ? "[vid" + n((int)cover) + "]" : audio_placeholder_cover();
  if (cover > 0 && S.cropped()) {
    char c[160];
    std::snprintf(c, sizeof c, "crop=w=iw*%.4f:h=ih*%.4f:x=iw*%.4f:y=ih*%.4f,", S.crop_w, S.crop_h, S.crop_x, S.crop_y);
    v += c;
  }
  // the room: black, led by the music (it ends with the song), padded from a few pixels; the soft
  // still light around the cover is the video shader's. Sixty pictures a second, like the other
  // looks: while a song plays the engine draws only when a picture comes (a redraw asked for in
  // between waits for the next one), so the cover's tilt, the card's turning cover and the
  // interface over the picture moved at the picture's rate (one or two a second: a jump). Each
  // picture is a padded black frame and the cover laid on it: far lighter than the spectrum
  return "[cue_wave]aresample=8000,showwaves=s=2x2:r=60:mode=point,format=yuv420p,lutyuv=y=16:u=128:v=128,"
         "pad=" + n(S.W) + ":" + n(S.H) + ":0:0:black[cue_base];" +
         v + "format=gbrp,"
         "scale=" + n(S.side) + ":" + n(S.side) + ":force_original_aspect_ratio=increase:flags=" + (cover > 0 ? "lanczos" : "neighbor") + ","
         "crop=" + n(S.side) + ":" + n(S.side) + ",setsar=1[cue_art];"
         "[cue_base][cue_art]overlay=" + n(S.x) + ":" + n(S.y) + ":format=yuv420,setsar=1[vo]";
}

// look: 0 cover and spectrum, 1 spectrum only, 2 cover only (the lightest)
inline std::string audio_visual_tail(int64_t cover, const MusicScene& scene = MusicScene{}, int look = 0) {
  return look == 1 ? audio_spectrum_tail(scene) : look == 2 ? audio_still_tail(cover, scene) : audio_cover_tail(cover, scene);
}

inline std::string audio_waveform_graph(int64_t track, int64_t cover = 0, const MusicScene& scene = MusicScene{}, int look = 0) {
  if (track <= 0) return {};
  // Split decoded samples, leaving the audible branch untouched. Only the tiny
  // visual branch is resampled; nearest-neighbour scaling preserves real pixels.
  return "[aid" + std::to_string(track) + "]asplit=2[ao][cue_wave];" + audio_visual_tail(cover, scene, look);
}

// Every audio track at once, as recorded (game, microphone, voice chat...): amix scales each
// input by 1/n, the volume puts them back at their own level (works with any ffmpeg).
inline std::string audio_mix_graph(const std::vector<int64_t>& ids, bool waveform, int64_t cover = 0,
                                   const MusicScene& scene = MusicScene{}, int look = 0) {
  if (ids.size() < 2) return {};
  std::string g;
  for (int64_t id : ids) g += "[aid" + std::to_string(id) + "]";
  std::string n = std::to_string(ids.size());
  g += "amix=inputs=" + n + ":duration=longest:dropout_transition=0,volume=" + n;
  return waveform ? g + ",asplit=2[ao][cue_wave];" + audio_visual_tail(cover, scene, look) : g + "[ao]";
}

inline std::string lower_ascii(std::string s) {
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  return s;
}

// A recording made in parts (OBS: game, microphone, voice chat on their own tracks) rather than
// a film with versions of the same sound (dubs, commentary): several audio tracks, none of them
// tagged with a language, none named like an alternative. Only then do they play together
// on their own; anything else waits for the person to choose it.
inline bool looks_like_multitrack_recording(const std::vector<Track>& audios) {
  int n = 0;
  for (const auto& t : audios) {
    if (t.external) continue;
    ++n;
    std::string lang = lower_ascii(t.lang);
    if (!lang.empty() && lang != "und" && lang != "unk" && lang != "mis" && lang != "zxx") return false;
    std::string title = lower_ascii(t.title);
    for (const char* w : {"comment", "dub", "director", "descri", "karaoke", "instrumental", "original", "stereo", "5.1", "7.1"})
      if (title.find(w) != std::string::npos) return false;
  }
  return n >= 2;
}

// Only rebuild/reselect here. The exact seek must be a separate operation AFTER
// playback-restart, otherwise the rebuild can discard it and land at a keyframe.
inline std::vector<std::vector<std::string>> audio_switch_commands(
    const std::string& graph, int64_t single_track) {
  std::vector<std::vector<std::string>> commands{{"set", "lavfi-complex", graph}};
  if (graph.empty() && single_track > 0) {
    // Release a formerly mixed input even when aid still names that same track.
    commands.push_back({"set", "aid", "no"});
    commands.push_back({"set", "aid", std::to_string(single_track)});
  }
  return commands;
}
