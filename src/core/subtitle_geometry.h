#pragma once
#include <algorithm>
#include <cmath>

// (1.4.1) How the words follow the window. Studied in "Cue · tamanho da legenda quando a janela
// muda" and "Cue · legenda maior sem virar três linhas" (22/11), then drawn by the author over his
// own screenshots ("dentro da imagem", 22/11 18:04): the subtitle is part of the picture.
// - The size follows the picture's reading side B: a landscape picture's 16:9 frame (the window's
//   height in a wider window, its width x 9/16 in a taller one), a squarer picture's own height
//   (4:3), a portrait picture's own width (larger words, as the BBC sizes vertical video). At the
//   window Cue opens in, B is the picture's height and the words are the size chosen. Larger, in
//   proportion; smaller, at a third of the rate (the cube root: a picture three times smaller has
//   words 2.1 times smaller, as in the author's drawing), never so large that a line of 64 letters
//   would not fit the width the engine breaks at: two lines stay two.
// - The width the engine breaks lines at is a share of the picture's width (subtitle_margin_x).
// - The position is the picture's too (subtitle_window_pos): at 93 the words stand inside the
//   picture where they stand at the opening window, whatever bands the window adds. Dragged under
//   the picture, they hang under it in every window that has a band (and stand at the window's
//   foot in one that has none).
// - Every choice of the subtitle style keeps its meaning at every size: the size chosen, the face,
//   the letter spacing, the outline and the box's padding are in the engine's scaled units; the
//   line spacing is a share of the letters' size (subtitle_line_gap_share).
// `ref_h` 0: the mini player, which keeps its own rule (the rule before 1.4.1, and its floor).
// `aspect` 0: the shape not known yet (as before). All in proportion: no resolution or Windows
// scale enters.
constexpr double kSubtitleMarginX = 12;         // the landscape side margin at the window Cue opens in (scaled px)
constexpr double kSubtitleWidthShare = 1 - 2 * kSubtitleMarginX / 1280.0;  // of a landscape picture's width
constexpr double kSubtitleWidthSharePortrait = 0.92;                     // of a portrait picture's width
// the picture as shown in the window (fitted inside it), and the side the words are read against
inline double subtitle_picture_w(double w, double h, double aspect) { return std::min(w, h * aspect); }
inline double subtitle_picture_base(double w, double h, double aspect) {
  const double pw = subtitle_picture_w(w, h, aspect);
  if (aspect < 1) return pw;                      // portrait: its own width
  if (aspect >= 16.0 / 9 - 1e-9) return pw * 9.0 / 16;  // 16:9 and wider: its 16:9 frame
  return pw / aspect;                             // squarer (4:3): its own height
}
inline double subtitle_wrap_width(int width, int height, double aspect);
inline double subtitle_typography_scale(int width, int height, double aspect = 0, double ref_h = 0, double size = 0, double track = 0) {
  if(width<=0||height<=0)return 1.0;
  const double w=width,h=height,w169=w*9.0/16.0;
  const bool known=std::isfinite(aspect)&&aspect>0;
  if(!(std::isfinite(ref_h)&&ref_h>0) || !known) {  // the mini player, or the shape not known yet: as before
    if(h>w169&&known) return std::clamp(std::max(std::min(h,w/aspect),w169)/h,0.25,1.0);
    return std::clamp(w169/h,1.0,2.0);
  }
  double b = subtitle_picture_base(w,h,aspect);
  if (b < ref_h && size > 0) {  // smaller than the opening window: at a third of the rate
    double slow = ref_h * std::cbrt(b / ref_h);
    // a line of 64 letters always fits the width the engine breaks at (22.65 of the letters' size,
    // measured in the engine with Cue's faces, plus the letter spacing)
    const double em64 = 22.65 + 64 * (std::isfinite(track) ? track : 0.0) / 44.0;
    if (em64 > 1) slow = std::min(slow, subtitle_wrap_width(width, height, aspect) * 720.0 / (size * em64));
    b = std::max(b, slow);
  }
  return std::clamp(b/h,0.05,3.0);
}
// the width the engine breaks a line at, in px of the window: a share of the picture's width
inline double subtitle_wrap_width(int width, int height, double aspect) {
  if (width <= 0 || height <= 0) return 0;
  if (!(std::isfinite(aspect) && aspect > 0)) return std::max(0.0, width - 2 * kSubtitleMarginX * height / 720.0);
  return subtitle_picture_w(width, height, aspect) * (aspect < 1 ? kSubtitleWidthSharePortrait : kSubtitleWidthShare);
}
// the engine's side margin (scaled px: the engine multiplies it by height/720) that gives that width;
// the mini player (`ref_h` 0) keeps its margins
inline double subtitle_margin_x(int width, int height, double aspect, double ref_h) {
  const bool portrait = std::isfinite(aspect) && aspect > 0 && aspect < 1;
  if (!(std::isfinite(ref_h) && ref_h > 0) || width <= 0 || height <= 0) return portrait ? 8 : 25;
  return std::max(0.0, (width - subtitle_wrap_width(width, height, aspect)) / 2) * 720.0 / height;
}
// (1.4.1) the line spacing setting (kLineGapMin..kLineGapMax) as a share of the letters' size, so it
// looks the same in every window and on every screen (it was a fixed number of the screen's px: at
// 4K the lines barely moved, in a small window a pulled-in setting laid the lines over each other).
// 0 is the face's own spacing; the lowest setting brings the lines as close as they go with this
// face's descenders (g, p, ç) still clear of the next line's accents (á, ã, é); above 0, 44 steps
// are a whole line (24: about half a line, as designed).
inline double subtitle_line_gap_share(int line_gap, int font) {
  static const double tightest[3] = {-0.230, -0.296, -0.255};  // Omni, Clean, Readable (glyph bounds)
  const double t = tightest[font < 0 || font > 2 ? 1 : font];
  if (line_gap < 0) return t * std::min(16, -line_gap) / 16.0;
  return std::min(line_gap, 44) / 44.0;
}

// (1.4.1, 22/11) Where the engine puts a bottom subtitle, measured in the shipped engine (libass in
// mpv) with Cue's settings: the last line's baseline at five positions (30 to 100), letter sizes 64
// to 209 px and window heights 1447 and 2088, within half a pixel; with two or three lines the last
// line stays put and the block grows upward one letter size per line. The engine moves a subtitle by
// a share of the window's height less a part of the letters (libass's line position), so a first
// model fitted at 93 alone put the words 35 to 100 px too high elsewhere (the controls did not see
// a dragged subtitle under the picture and covered it: the author's look, 22/11 19:10). The block's
// foot is the last line's box: its baseline and a fifth of the letter size under it.
inline double subtitle_baseline(double h, double pos, double fs) {
  const double p = pos / 100.0;
  return h * (1.0018 * p - 0.0003) - fs * (0.9764 - 0.7435 * (1 - p));
}
inline double subtitle_block_bottom(double h, double pos, double fs) { return subtitle_baseline(h, pos, fs) + 0.2 * fs; }
inline double subtitle_pos_for_bottom(double h, double bottom, double fs) {
  const double d = 1.0018 * h - 0.7435 * fs;  // bottom = p x d + (h x -0.0003 - 0.2329 fs + 0.2 fs)
  return d > 0 ? (bottom - (-0.0003 * h - 0.2329 * fs + 0.2 * fs)) / d * 100.0 : 0;
}
// (1.4.1, 22/11) The position is the picture's: a subtitle at 93 stands where it stands at the
// window Cue opens in, inside the picture, whatever bands the window adds (and above the controls
// when they cover the picture, as before). Above 100 it was dragged under the picture: it hangs a
// quarter of a line under the picture's foot (100) and lower in the band's room (200 at the
// window's foot); with no band it stands at the window's foot. Crossing the picture's foot while
// dragging, the words go from one side to the other: never cut by the picture's edge.
constexpr double kSubPosPictureMax = 200, kSubPosWindowFoot = 97;
inline double subtitle_bandless_pos(double pos) { return pos <= 100 ? pos : kSubPosWindowFoot; }
inline double subtitle_window_pos(double pos, int width, int height, double aspect, double fs, double gap_px, int rows) {
  if (!std::isfinite(pos)) return pos;
  if (width <= 0 || height <= 0 || !(std::isfinite(aspect) && aspect > 0) || !(fs > 0)) return subtitle_bandless_pos(pos);
  const double h = height, pic_h = std::min(h, width / aspect);
  if (pic_h >= h - 1) return subtitle_bandless_pos(pos);
  const double top = (h - pic_h) / 2;
  if (pos <= 100) return subtitle_pos_for_bottom(h, top + subtitle_block_bottom(pic_h, pos, fs), fs);
  const int n = std::clamp(rows, 1, 8);
  const double bh = n * fs + (n - 1) * (std::isfinite(gap_px) ? gap_px : 0.0), top0 = top + pic_h + 0.25 * fs;
  const double room = std::max(0.0, subtitle_block_bottom(h, kSubPosWindowFoot, fs) - (top0 + bh));
  const double f = std::min(1.0, (pos - 100) / 100.0);
  return std::min(kSubPosWindowFoot, subtitle_pos_for_bottom(h, top0 + bh + f * room, fs));
}
inline double subtitle_picture_pos(double wpos, int width, int height, double aspect, double fs, double gap_px, int rows) {
  if (width <= 0 || height <= 0 || !(std::isfinite(aspect) && aspect > 0) || !(fs > 0) || !std::isfinite(wpos)) return wpos;
  const double h = height, pic_h = std::min(h, width / aspect);
  if (pic_h >= h - 1) return wpos;
  const double top = (h - pic_h) / 2, bottom = subtitle_block_bottom(h, wpos, fs);
  const int n = std::clamp(rows, 1, 8);
  const double bh = n * fs + (n - 1) * (std::isfinite(gap_px) ? gap_px : 0.0), top0 = top + pic_h + 0.25 * fs;
  if (bottom - bh >= top0 - 1e-6) {
    const double room = std::max(0.0, subtitle_block_bottom(h, kSubPosWindowFoot, fs) - (top0 + bh));
    return 100 + 100 * (room > 0 ? std::clamp((bottom - top0 - bh) / room, 0.0, 1.0) : 0.0);
  }
  return std::min(100.0, subtitle_pos_for_bottom(pic_h, bottom - top, fs));
}
