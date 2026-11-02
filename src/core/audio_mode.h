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
  int side = 520, x = 406, y = 237;  // the cover
  int wing_w = 220, wing_h = 528, wing_gap = 36;
  int text_x = 1262, text_w = 540;    // the words' column, centred on the cover's middle
  int wing_y() const { return y + side / 2 - wing_h / 2; }
  int left_x() const { return x - wing_gap - wing_w; }
  int right_x() const { return x + side + wing_gap; }
  static MusicScene wide() { return {}; }
};

// How the scene sits in the window. The engine always draws the wide scene; the window frames
// it with the engine's zoom and pan (no rebuild, no pause, live while an edge is dragged):
//   Scene: all of it, the words beside the cover (wide, roomy windows)
//   Tall:  the cover with its wings, the words under it (portrait windows)
//   Cover: the cover and its wings alone (small windows)
enum class MusicFrame { Scene, Tall, Cover };
// screen = (ox, oy) + scene * f
struct MusicView {
  double f = 0, ox = 0, oy = 0;
  MusicFrame frame = MusicFrame::Scene;
  bool words_beside() const { return frame == MusicFrame::Scene; }
  bool words_under() const { return frame == MusicFrame::Tall; }
};
// w, h: the window; s: the interface's pixel scale (sizes below are in its art pixels)
inline MusicFrame music_frame_for(int w, int h, double s) {
  double aw = w / std::max(0.5, s), ah = h / std::max(0.5, s);
  if (aw * 4 >= ah * 5 && aw >= 620 && ah >= 330) return MusicFrame::Scene;
  if (ah >= aw * 1.15 && aw >= 280 && ah >= 460) return MusicFrame::Tall;
  return MusicFrame::Cover;
}
// controls_top: where the control bar begins (the cover stays above it)
inline MusicView music_view(const MusicScene& S, MusicFrame m, int w, int h, int controls_top) {
  MusicView v;
  v.frame = m;
  if (w <= 0 || h <= 0) return v;
  if (m == MusicFrame::Scene) {
    v.f = std::min(w / (double)S.W, h / (double)S.H);
    v.ox = (w - S.W * v.f) / 2; v.oy = (h - S.H * v.f) / 2;
    return v;
  }
  double room = std::clamp((double)controls_top, h * 0.45, (double)h);
  double side, centre;
  if (m == MusicFrame::Tall) { side = std::min(w * 0.70, room * 0.52); centre = room * 0.40; }
  else { side = std::min(w * 0.62, room * 0.74); centre = room * 0.5; }
  v.f = std::max(0.05, side / S.side);
  v.ox = w / 2.0 - (S.x + S.side / 2.0) * v.f;
  v.oy = centre - (S.y + S.side / 2.0) * v.f;
  return v;
}
inline MusicView mix_views(const MusicView& a, const MusicView& b, double k) {
  MusicView v = b;
  v.f = a.f + (b.f - a.f) * k; v.ox = a.ox + (b.ox - a.ox) * k; v.oy = a.oy + (b.oy - a.oy) * k;
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
  // the light gathers around the cover: its centre, as a share of the picture
  char cx[16], cy[16];
  std::snprintf(cx, sizeof cx, "%.3f", (S.x + S.side / 2.0) / S.W);
  std::snprintf(cy, sizeof cy, "%.3f", (S.y + S.side / 2.0) / S.H);
  return "[cue_wave]aresample=24000,volume=3,"
         "showfreqs=s=24x20:r=60:mode=bar:ascale=sqrt:fscale=log:win_size=4096:overlap=0.92:averaging=1:colors=white,"
         "trim=start_frame=2,format=gray,crop=24:19:0:0,pad=24:20:0:0:color=0x262626,"
         "lagfun=decay=0.965,split=2[cue_bars][cue_pulse];"
         // the wings' cells: bars turned sideways, mirrored so the bass sits level with the cover's middle
         "[cue_bars]transpose=clock,split=2[cue_lo][cue_hi0];[cue_hi0]vflip[cue_hi];[cue_hi][cue_lo]vstack,format=gbrp[cue_cells];" +
         v + "format=gbrp,split=3[cue_c1][cue_c2][cue_c3];"
         // the cover, sharp
         "[cue_c1]scale=" + n(S.side) + ":" + n(S.side) + ":force_original_aspect_ratio=increase:flags=" + (cover > 0 ? "lanczos" : "neighbor") + ","
         "crop=" + n(S.side) + ":" + n(S.side) + ",setsar=1[cue_art];"
         // each row of cells takes the colour of the cover's row beside it, lifted to stay bright;
         // dark cells are cut out, then the cells become dots with transparent gaps
         "[cue_c2]scale=1:48:flags=area,lutrgb=r=val*0.6+100:g=val*0.6+100:b=val*0.6+100,scale=20:48:flags=neighbor[cue_tint];"
         "[cue_cells][cue_tint]blend=all_mode=multiply,colorkey=0x000000:0.02:0,format=yuva444p,"
         "scale=" + n(S.wing_w) + ":" + n(S.wing_h) + ":flags=neighbor,format=yuva420p[cue_wing];"
         // the gaps between the dots: a grid drawn once and multiplied into the wings' alpha
         // (drawing the grid on every picture cost more than everything else on the wings)
         "color=c=white:s=" + n(S.wing_w) + "x" + n(S.wing_h) + ":r=25:d=0.04,format=yuva420p,"
         "drawgrid=w=11:h=11:t=2:c=black@0:replace=1[cue_grid];"
         "[cue_wing][cue_grid]blend=c0_mode=normal:c1_mode=normal:c2_mode=normal:c3_mode=multiply,"
         "split=2[cue_wr][cue_wl0];[cue_wl0]hflip[cue_wl];"
         // the light: the cover's colours, blurred and dimmed, plus the music's energy in them
         "[cue_c3]scale=32:18:flags=area,gblur=sigma=3,lutrgb=r=val*0.55:g=val*0.55:b=val*0.55,split=2[cue_g1][cue_g2];"
         "[cue_g2]lutrgb=r=val*2:g=val*2:b=val*2[cue_gb];"
         "[cue_pulse]scale=32:18:flags=area,gblur=sigma=2,format=gbrp[cue_p];"
         "[cue_p][cue_gb]blend=all_mode=multiply[cue_pt];"
         "[cue_pt][cue_g1]blend=all_mode=addition:all_opacity=0.6,vignette=angle=0.9:x0=w*" + std::string(cx) + ":y0=h*" + cy + ","
         "format=yuv420p,scale=" + n(S.W) + ":" + n(S.H) + ":flags=fast_bilinear[cue_bg];"
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
inline std::string audio_visual_tail(int64_t cover, const MusicScene& scene = MusicScene{}, int look = 0) {
  return look == 1 ? audio_spectrum_tail(scene) : audio_cover_tail(cover, scene);
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
