// Persisted IDs: -1 small, 2 medium (the default), 0 large, 3 larger (1 was the old 1.5x "larger": read as 3).
constexpr int kDefaultUiSize = 2;
#pragma once
#include <algorithm>
#include <cmath>
constexpr int kUiSizes = 4;
inline int ui_size_index(int choice) { return choice == -1 ? 0 : choice == 2 ? 1 : (choice == 3 || choice == 1) ? 3 : 2; }
inline int ui_size_choice(int index) { constexpr int choices[] = {-1, 2, 0, 3}; return choices[std::clamp(index, 0, kUiSizes - 1)]; }
inline int next_ui_size(int choice, int direction) { return ui_size_choice(ui_size_index(choice) + direction); }

// The interface scale (physical px per art px). A pure function of DPI, window and choice: the
// same window always gets the same size, whatever happened before (no hysteresis), so a video
// never opens at one size and "corrects" itself later.
//   - it grows with the window: in step with it up to a roomy window (1.4x the reference of
//     1280 DIPs wide), then at half the rate, so full screen is a little bigger than the
//     window without taking over the picture (for every choice);
//   - width leads (the dock is a row): a 2.39:1 window is as wide as its 16:9 neighbour and
//     gets the same controls; the height only holds it back when it is under 520 DIPs per 1280 of width (wider than 2.46:1);
//   - a squarer window is measured by its area (as a 16:9 window of the same area), within
//     what its width holds (1000 DIPs per reference step), so 4:3 and 1:1 video keep their size;
//   - the four choices are proportions of that (0.5, 0.75, 1, 1.25: steps of a quarter, so at
//     the reference window they land exactly on 1, 1.5, 2 and 2.5 px per art px), on whole or
//     half art px so the pixel font stays as even as it can be;
//   - every step is at least half an art px bigger than the one before, at any window: a size
//     that looks the same as its neighbour would feel broken;
//   - a window too small for the design canvas (320 x 180 art px) holds it down.
// The base is Windows' own scale (it already weighs pixel density and viewing distance per
// display); the window factor reaches 6x the reference, so a 5K or 8K screen at 100% still
// grows its controls instead of stopping at 4K's.
// choice: persisted id: -1 small, 2 medium, 0 large, 3 larger.
inline double ui_scale_at(unsigned dpi, double k, int choice) {
  if (dpi < 48) dpi = 96;
  static const double mult[kUiSizes] = {0.5, 0.75, 1.0, 1.25};  // pequena, média, grande, maior
  return std::max(1.0, std::round(dpi / 48.0 * k * mult[ui_size_index(choice)] * 2) / 2);
}
// The scale of the smallest windows (the reference size): what minimum window sizes use.
inline double ui_base_scale(unsigned dpi, int choice) { return ui_scale_at(dpi, 1.0, choice); }
inline double ui_scale(unsigned dpi, int cw, int ch, int choice) {
  if (dpi < 48) dpi = 96;
  double dw = cw * 96.0 / dpi, dh = ch * 96.0 / dpi;  // the window in DIPs
  double k = std::min(dw / 1280.0, dh / 520.0);
  // a squarer window (4:3, 1:1 video) is not a smaller one: at the same area as a 16:9 window it
  // gets the same controls, as long as its width still holds the bar comfortably
  k = std::max(k, std::min(std::sqrt(dw * dh / (1280.0 * 720.0)), dw / 1000.0));
  k = std::clamp(k, 1.0, 6.0);
  // past the size of a roomy window (1.4x the reference) the controls grow at half the rate:
  // in full screen they get bigger than in the window, but less than the picture does
  if (k > 1.4) k = 1.4 + 0.5 * (k - 1.4);
  double s = 0;
  for (int i = 0; i <= ui_size_index(choice); ++i) {
    double si = ui_scale_at(dpi, k, ui_size_choice(i));
    s = i ? std::max(si, s + 0.5) : si;
  }
  double fit = std::floor(std::min(cw / 320.0, ch / 180.0) * 2) / 2;
  return std::max(1.0, std::min(s, std::max(1.0, fit)));
}

// Portrait windows recompose the controls into a compact dock about this many px of art wide.
constexpr int kCompactDockW = 204;
// Instead of stretching that dock across the whole width at the landscape scale (tiny icons),
// a portrait window raises the scale until the dock fills a share of the width that follows
// the chosen size: narrower than the window, and bigger.
// The landscape dock's width in px of art (with the subtitles and audio buttons).
constexpr int kLandscapeDockW = 372;
// Clear portrait proportions (through 3:4) keep the compact composition even when
// the screen/size choice leaves a wide art canvas. Near-square desktop windows
// (Windows Snap) only compact when the controls actually lack horizontal room.
constexpr int kRegularDockRoom = kLandscapeDockW + 28;
inline bool compact_video_dock(double s, int cw, int ch) {
  return ch >= cw * (4.0 / 3.0) || (ch >= cw * 1.15 && cw / s < kRegularDockRoom);
}
// A vertical video's bar: about 15% smaller than the rule that sizes it (on a phone video the bar
// and its menus took too much of the picture), in half art px; from 2 px up always at least one
// half step down; never under 1.
constexpr double kPortraitShrink = 0.85;
inline double portrait_shrink(double ps) {
  // (to the half step below unless the one above is within a quarter of it: rounding to the
  // nearest went up too often at large scales, 4.5 -> 4 is only 11% off)
  double shrunk = std::floor(ps * kPortraitShrink * 2 + 0.25) / 2;
  if (shrunk >= ps && ps >= 2) shrunk = ps - 0.5;
  return std::max(1.0, shrunk);
}
inline double portrait_scale(double s, int cw, int ch, int choice) {
  if (cw <= 0) return s;
  if (ch > cw && !compact_video_dock(s, cw, ch)) return s;
  if (ch < cw * 1.15) {
    // a squarish window (1:1 video, 4:3 easing in from 0.75): the landscape dock, but raised
    // until it fills a share of the width, so the controls are not lost under a tall picture
    double t = std::clamp((ch / (double)cw - 0.75) / 0.25, 0.0, 1.0);
    if (t <= 0) return s;
    static const double sq[kUiSizes] = {0.56, 0.67, 0.76, 0.84};
    double want = cw * sq[ui_size_index(choice)] * t / kLandscapeDockW;
    double ps = std::max(s, std::min(std::floor(want * 2) / 2, s + 1.5));
    // a little taller than wide (5:6 ... 1:1): a vertical video too, the same 15% off
    return ch > cw ? portrait_shrink(ps) : ps;
  }
  static const double share[kUiSizes] = {0.72, 0.80, 0.88, 0.94};
  double want = cw * share[ui_size_index(choice)] / kCompactDockW;
  double ps = std::max(s, std::min(std::floor(want * 2) / 2, s + 2));  // whole or half art px, like the medium size
  // then 15% off, whatever set it: the share above, or the window's own scale (a wider portrait
  // window, 4:5 or 3:4, is big enough for its own scale to be the larger one)
  return portrait_shrink(ps);
}

// Music framed close (a small or portrait window, the stacked composition): the window's own scale
// shrinks with a small window (the 320 x 180 canvas a film's bar needs), which left a song's few
// controls tiny. They are given back the screen's standard size (base: what a reference window
// gets), never more (a bigger window must not get bigger words than the wide composition shows),
// as long as the compact bar (204 px of art) fits 80% of the width and a quarter of the height.
constexpr int kMusicCompactW = 204;
inline double music_compact_scale(double s, int cw, int ch, int /*choice*/, double base) {
  if (cw <= 0 || ch <= 0) return s;
  double fit = std::floor(std::min(cw * 0.8 / kMusicCompactW, ch / 4.0 / 38.0) * 2) / 2;
  return std::max(s, std::min(base, fit));
}

// The mini players are laid out in art px and keep a small window at every interface size: a
// film's needs about 288 x 162 of them (its row of controls, the timeline, the menus over the
// picture), a song's card about 250 x 130 (its title above the playback, the controls under the
// window's buttons). Their pixels are the chosen size's (the screen's base for it, not grown with
// the window), stepped down by half art px until that room is there: at the large sizes the
// controls, the window's buttons and the menus were far too big for the little window, and the
// menus came out at another size than the bar (they had to step down on their own to fit).
constexpr int kMiniFilmW = 288, kMiniFilmH = 162, kCardMinW = 250, kCardMinH = 130;
inline double mini_fit(int cw, int ch, bool song) {
  if (cw <= 0 || ch <= 0) return 99;
  const int lo = std::min(cw, ch), hi = std::max(cw, ch);  // (a portrait film: its long side is the height)
  const double fit = song ? std::min(cw / (double)kCardMinW, ch / (double)kCardMinH)
                          : std::min(hi / (double)kMiniFilmW, lo / (double)kMiniFilmH);
  return std::floor(fit * 2) / 2;
}
inline double mini_scale(unsigned dpi, int cw, int ch, int choice, bool song) {
  return std::max(1.0, std::min(ui_base_scale(dpi, choice), mini_fit(cw, ch, song)));
}
inline double card_scale(double s, int cw, int ch) { return std::max(1.0, std::min(s, mini_fit(cw, ch, true))); }
