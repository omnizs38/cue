#include "onboarding.h"

#include <algorithm>
#include <cmath>

#include "../core/i18n.h"
#include "../core/ui_scale.h"
#include "light.h"
#include "blur_cloud.h"

namespace {
const str::Id kSizeNames[kUiSizes] = {str::size_small, str::size_medium, str::size_large, str::size_larger};
const str::Id kTitles[4] = {str::ob_intro, str::ob_ambient_title, str::ob_bg_title, str::ob_size_title};
const str::Id kLines[4] = {str::ob_intro_line, str::ob_ambient_line, str::ob_bg_line, str::ob_size_line};

constexpr double kIntroType = 0.7, kCharDur = 0.045;
constexpr double kFootIn = 0.55;   // the footer comes in with the eyes, not after the greeting
constexpr double kScaleFade = 0.18;  // the controls crossfade from one size to the next
constexpr double kMoodDur = 5.0;   // the scene drifts through three film moods, slowly
constexpr double kSceneFps = 12;   // the little scene is a film at 12 pictures a second
constexpr int kDockW = 200, kDockH = 38, kDockAir = 8;  // the controls' sample, art px (at its own pixel)

// the light behind the eyes: cool on one side, warm on the other
constexpr Rgb kCool = kEyeCool, kMid = kEyeMid, kWarm = kEyeWarm;

int cps(const std::string& s) {
  int n = 0;
  for (size_t i = 0; i < s.size();) { next_cp(s, i); ++n; }
  return n;
}
std::string first_cps(const std::string& s, int n) {
  size_t i = 0;
  for (int k = 0; k < n && i < s.size(); ++k) next_cp(s, i);
  return s.substr(0, i);
}
double ease_k(double t) { return ease_out(clamp01(t)); }
// the toggle's label crossfades between "on" and "off": room for the longer of the two
int toggle_label_w(double s) { return std::max(text_w(tr(str::ob_on), s), text_w(tr(str::ob_off), s)); }
}  // namespace

std::vector<std::string> wrap_text(const std::string& t, int max_w, double s) { return wrap_lines(t, max_w, s); }

// The same number of lines, but as even as possible: no lonely last word.
static std::vector<std::string> wrap_balanced(const std::string& t, int max_w, double s) {
  auto lines = wrap_text(t, max_w, s);
  if (lines.size() < 2) return lines;
  int lo = max_w / (int)lines.size(), hi = max_w;
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (wrap_text(t, mid, s).size() <= lines.size()) hi = mid;
    else lo = mid + 1;
  }
  return wrap_text(t, hi, s);
}

// ── layout ──────────────────────────────────────────────────────────
int OnboardingView::scale_index() const { return ui_size_index(ui_scale); }

// The interface scale of each size in this window: the App's, or (previews, tests) the chosen
// one's in proportion (0.5, 0.75, 1, 1.25), on half px, each a step above the one before.
void OnboardingView::sizes() {
  static const double mult[kUiSizes] = {0.5, 0.75, 1.0, 1.25};
  const int cur = std::clamp(scale_index(), 0, kUiSizes - 1);
  for (int i = 0; i < kUiSizes; ++i) {
    double v = size_s[i] > 0 ? size_s[i] : std::max(1.0, std::round(ui_s * mult[i] / mult[cur] * 2) / 2);
    if (i > 0) v = std::max(v, eff_s_[i - 1] + 0.5);
    eff_s_[i] = v;
  }
}

void OnboardingView::set_size(int w_, int h_, double s_) {
  if (w_ <= 0 || h_ <= 0) return;  // minimized or not created yet: keep the last layout
  bool first = w == 0;
  w = w_;
  h = h_;
  ui_s = std::max(1.0, s_);
  chrome.s = ui_s;  // the window buttons stay exactly where the home has them
  sizes();
  // The welcome's pixel: a step above the home's (a quarter more, on the half-pixel grid: it is a
  // first impression, read from further away than a list), never more than the window holds
  // comfortably, never less than the home's. With the picture at a fixed size of art, every
  // window shows the same composition, only at its own pixel: nothing is ever just "smaller".
  // Made for the window, not for the size chosen on the third screen: choosing one changes the
  // controls, not the screen around them.
  // (1.4.1: also when the interface's own scale changed under the same window. The window's first
  // size reached the welcome before Cue knew the screen's DPI, at 96; on a laptop at 125% the page
  // kept that first, smaller scale for good, the controls on it a size bigger)
  if (first || w != page_s_w_ || h != page_s_h_ || eff_s_[1] != page_s_base_) {
    page_s_w_ = w; page_s_h_ = h; page_s_base_ = eff_s_[1];
    const double base = std::max(1.0, eff_s_[1]);
    const double want = std::round(base * 1.25 * 2) / 2;
    const double room = std::floor(std::min(w / 640.0, h / 400.0) * 2) / 2;  // the composition is ~600 x 380 art
    s = std::max(base, std::min(want, room));
  }
  layout();
  // a small window at a large size (or longer words): rather a half step smaller than words
  // under the footer
  while (overflow_ && s > 1) { s = std::max(1.0, s - 0.5); layout(); }
  if (first) { pos_.snap(page_); dot_.snap(page_); }
  place_face(0, !face_placed_);
}

void OnboardingView::layout() {
  if (w <= 0 || h <= 0) return;
  const double W = w / s, H = h / s;
  const double CW = std::max(40.0, std::min(W - 32, 560.0)), ox = (W - CW) / 2;
  // the footer stands with the content (under it, on the picture's edges), not at the window's
  // far corners: in a big window the buttons were small marks a long way from what they move on
  const double top = 26, bottom = H - 14;
  constexpr double kBtnH = 18, kFootGap = 30;  // (the plate: the installer's and the menus' rows, 18 px)
  auto R = [&](double x, double y, double ww, double hh) { return Rect(x * s, y * s, ww * s, hh * s); };

  // the words: a heading, its line (as many rows as the longest of the three needs) and a control
  line_rows_ = 1;
  for (auto id : kLines) line_rows_ = std::max<int>(line_rows_, (int)wrap_text(tr(id), (int)(CW * 0.86 * s), s).size());
  line_rows_ = std::min(line_rows_, 3);
  const double words_h = 24 + 4 + 13.0 * line_rows_ + 14 + 16 + kFootGap + kBtnH;
  const double gap = 22, avail = bottom - top;

  // the picture: wide enough for the controls at the largest size, at their own pixel, when the
  // window allows (otherwise all four sizes shrink by the same ratio)
  const double big = eff_s_[kUiSizes - 1];
  // (one size of art: the same composition in every window; the controls step down inside it
  // when the largest size would not fit, see ctrl_s_)
  double bw = std::min(320.0, std::max(120.0, CW * 0.86));
  double bh = std::round(bw * 9 / 16);
  overflow_ = false;
  if (bh + gap + words_h > avail) {
    bh = std::floor(avail - gap - words_h);
    if (bh < 54) { overflow_ = true; bh = std::max(40.0, bh); }
    bw = std::round(bh * 16 / 9);
  }
  bw = std::round(bw);
  const double by = top + std::max(0.0, (avail - (bh + gap + words_h)) / 2);
  box_ = R(std::round(W / 2 - bw / 2), std::round(by), bw, bh);

  // the controls' pixel at each size: their real one; when the largest does not fit the picture,
  // all four step down together (the same half-px steps apart as in the player), and only the
  // smallest windows let the smallest sizes meet at 1
  const double room = std::max(1.0, std::floor((box_.w + 1) / (double)(kDockW + 2 * kDockAir) * 2) / 2);
  const double down = std::max(0.0, big - room);
  for (int i = 0; i < kUiSizes; ++i) ctrl_s_[i] = std::max(1.0, eff_s_[i] - down);

  const double ty = by + bh + gap;
  // the footer: the way back (a word) on the picture's left edge, the page markers in the middle,
  // the way on (a plate, the one strong shape here) ending on its right edge
  {
    const double fy = ty + 28 + 13.0 * line_rows_ + 14 + 16 + kFootGap;
    double nw_max = 0, back_max = 0;
    for (int p = 0; p < kPages; ++p) {
      nw_max = std::max(nw_max, text_w(tr(p == kPages - 1 ? str::ob_start : str::ob_continue), 1) + 9.0 + 28);
      back_max = std::max(back_max, text_w(tr(p == 0 ? str::ob_skip : str::ob_back), 1) + 16.0);
    }
    const double dots_w = kPages * 14 + (kPages - 1) * 2;
    // (one width for "continuar" and "começar": a target that never shrinks; less where the
    // window is narrow)
    // (the markers stay in the middle: each side holds the wider of the two buttons)
    auto need_for = [&](double nw) { return dots_w + 2 * 12 + 2 * std::max(back_max - 8, nw); };
    if (need_for(std::max(nw_max, 96.0)) <= CW) nw_max = std::max(nw_max, 96.0);
    // on the picture's edges, or wider when the three do not fit between them (a narrow window, long
    // words): then centred, as wide as they need
    const double need = need_for(nw_max);
    const double span = std::min(CW, std::max(bw, need));
    const double fx0 = std::round(W / 2 - span / 2), fx1 = fx0 + span;
    for (int p = 0; p < kPages; ++p) {
      Lay& L = L_[p];
      L.next = R(fx1 - nw_max, fy, nw_max, kBtnH);
      const double bw2 = text_w(tr(p == 0 ? str::ob_skip : str::ob_back), 1) + 16;
      L.back = R(fx0 - 8, fy, bw2, kBtnH);  // (its word starts on the picture's edge; its hover plate reaches out)
      L.dots.clear();
      const double dx = W / 2 - (kPages * 14 + (kPages - 1) * 2) / 2.0;
      for (int i = 0; i < kPages; ++i) L.dots.push_back(R(dx + i * 16, fy + (kBtnH - 10) / 2, 14, 10));
    }
  }
  title_y_ = (int)std::lround(ty * s);
  line_y_ = (int)std::lround((ty + 28) * s);
  const double cy = ty + 28 + 13.0 * line_rows_ + 14;
  ctrl_y_ = (int)std::lround(cy * s);
  for (int p = 0; p < kPages; ++p) {
    L_[p].heading = R(ox, ty, CW, 24);
    L_[p].text_y = line_y_;
  }
  {
    double tw = 28 + 8 + toggle_label_w(1);
    L_[kPageLight].toggle = R(std::round(W / 2 - tw / 2), cy, 28, 16);
  }
  {
    // the background: two choices, as the sizes are chosen (1.4.1)
    Lay& L = L_[kPageBg];
    const str::Id names[2] = {str::more_with_bg, str::more_without_bg};
    double segw = 8;
    for (auto n : names) segw += text_w(tr(n), 1) + 16;
    L.segs.clear();
    double sx = std::round(W / 2 - segw / 2) + 4;
    for (auto n : names) {
      const double sw = text_w(tr(n), 1) + 16;
      L.segs.push_back(R(sx, cy, sw, 16));
      sx += sw;
    }
  }
  {
    Lay& L = L_[kPageSize];
    double segw = 8;
    for (auto n : kSizeNames) segw += text_w(tr(n), 1) + 16;
    L.segs.clear();
    double sx = std::round(W / 2 - segw / 2) + 4;
    for (auto n : kSizeNames) {
      double sw = text_w(tr(n), 1) + 16;
      L.segs.push_back(R(sx, cy, sw, 16));
      sx += sw;
    }
    const Rect& sel = L.segs[std::clamp(scale_index(), 0, kUiSizes - 1)];
    seg_x_.snap(sel.x);
    seg_w_.snap(sel.w);
  }
  if (L_[0].next.b() > (int)std::lround((H - 6) * s)) overflow_ = true;  // (the footer past the window's foot)

  // the eyes: as big as the picture holds, standing on a line two thirds down it
  double fs = std::min(bw * 0.7 / 22.0, bh * 0.33 / 6.0);  // (the eyes lead, the words still read beside them)
  fs = std::max(1.0, std::floor(fs * 3) / 3.0);
  face_scale_ = fs;
  face_x_ = box_.x + box_.w / 2.0;
  line_at_ = box_.y + (int)std::lround(std::round(bh * 0.70) * s);
  face_y_ = line_at_ - fs * s;  // Face::cy is the eyes' bottom: a little air above the line

  // their light: an ellipse wider than the picture, cool on the left and warm on the right, and
  // the line itself, lit where the light falls on it
  {
    const double rx = std::min(CW * 0.5, bw * 0.95), ry = bh * 0.62;
    const double ecy = face_y_ - 3 * fs * s;
    eye_light_r_ = Rect(face_x_ - rx * s, ecy - ry * s, 2 * rx * s, 2 * ry * s);
    const int ly = line_at_ - eye_light_r_.y, lh = std::max(1, (int)std::lround(s));
    // (an edge dragged mostly moves it: made again only when its own size or its line changes)
    const uint64_t key = (uint64_t)eye_light_r_.w << 40 ^ (uint64_t)eye_light_r_.h << 20 ^ (uint64_t)(ly & 0xFFFFF) ^ (uint64_t)lh << 60;
    if (key != eye_light_key_ || eye_light_.w != eye_light_r_.w) {
    eye_light_key_ = key;
    radial_light(eye_light_, eye_light_r_.w, eye_light_r_.h, kCool, kMid, kWarm, 0.5);
    for (int y = ly; y < ly + lh && y >= 0 && y < eye_light_.h; ++y) {
      uint32_t* row = eye_light_.px.data() + (size_t)y * eye_light_.w;
      for (int x = 0; x < eye_light_.w; ++x) {
        const double u = (x + 0.5) / eye_light_.w, d = std::fabs(u - 0.5) * 2;
        const double a = std::clamp((1 - d * d) * 0.85, 0.0, 1.0);
        const Rgb col = mix(u < 0.5 ? mix(kCool, kMid, u * 2) : mix(kMid, kWarm, (u - 0.5) * 2), Rgb{255, 255, 255}, 0.25);
        const uint32_t dst = row[x];
        const double da = (dst >> 24) / 255.0;
        auto ch = [&](uint8_t v, int sh) { return (uint32_t)std::min(255.0, v * a + ((dst >> sh) & 255) * (1 - a)); };
        const uint32_t A = (uint32_t)std::lround((a + da * (1 - a)) * 255);
        row[x] = (A << 24) | (ch(col.r, 16) << 16) | (ch(col.g, 8) << 8) | ch(col.b, 0);
      }
    }
    compose_on(eye_light_base_, eye_light_, P().bg, 1.0);
    }
  }
  scene_light_key_ = 0;  // made again for the new size when it is next drawn
}

void OnboardingView::place_face(double now, bool snap) {
  (void)now;
  double tx, ty, ts;
  if (finish_t0_ > 0) { tx = finish_fx_; ty = finish_fy_; ts = finish_fs_; }
  else { tx = face_x_; ty = face_y_; ts = face_scale_; }
  if (snap) { fx_.snap(tx); fy_.snap(ty); fs_.snap(ts); face_placed_ = true; }
  else { fx_.target = tx; fy_.target = ty; fs_.target = ts; }
  double fs = finish_t0_ > 0 ? ui_s : s;
  face.layout(fx_.x, fy_.x, fs, std::max(0.34, std::round(fs_.x * 3) / 3.0));
  // big eyes look around a little, not across the picture
  face.max_look = finish_t0_ > 0 ? 3.0 * face.scale / 1.5 : std::min(3.0 * face.scale / 1.5, 0.45 * face.scale + 1.5);
}

void OnboardingView::start(double now) {
  t0_ = now;
  page_enter_ = now;
  face.set_mode(Face::Mode::Follow, now);
  face.look(0, 0);
  if (reduce_motion()) t0_ = now - 10;
  knob_.snap(ambient ? 1 : 0);
  glow_.snap(ambient ? 1 : 0);
  bg_.snap(controls_free ? 1 : 0);
}

void OnboardingView::go(int p, double now) {
  p = std::clamp(p, 0, kPages - 1);
  if (p == page_) return;
  page_ = p;
  page_enter_ = now;
  pos_.target = p;
  dot_.target = p;
  if (kb_) focus_ = kNext;
  // the eyes close on the way to the scene (their light becomes the window's), open coming back
  face.closed = p != 0;
  if (p == 0) face.notice(now);
}

void OnboardingView::set_ambient(bool on, double now) {
  if (on == ambient) return;
  ambient = on;
  knob_.target = on ? 1 : 0;
  knob_.v += on ? 5 : -5;  // the flick that starts it
  glow_.target = on ? 1 : 0;
  toggle_t_ = now;
}

void OnboardingView::set_scale_choice(int id, double now) {
  if (id == ui_scale) return;
  // mid-crossfade: it starts again from what is shown most
  if (now - scale_t_ >= kScaleFade * 0.5 || prev_scale_idx_ < 0) prev_scale_idx_ = scale_index();
  ui_scale = id;
  if (L_[kPageSize].segs.size() == 4) {
    const Rect& sel = L_[kPageSize].segs[std::clamp(scale_index(), 0, kUiSizes - 1)];
    seg_x_.target = sel.x;
    seg_w_.target = sel.w;
  }
  scale_t_ = now;
}

void OnboardingView::set_controls_free(bool free, double now) {
  (void)now;
  if (free == controls_free) return;
  controls_free = free;
  bg_.target = free ? 1 : 0;
}

void OnboardingView::finish(double now, double face_x, double face_y, double face_scale) {
  if (finish_t0_ > 0) return;
  finish_t0_ = now;
  finish_fx_ = face_x;
  finish_fy_ = face_y;
  finish_fs_ = face_scale;
  face_handoff_ = pos_.x < 0.5;  // only when the eyes are on screen do they fly to the home
  face.closed = false;
  fx_ = Spring::rd(0.36, 0.9, 0.3); fx_.snap(face.cx);
  fy_ = Spring::rd(0.36, 0.9, 0.3); fy_.snap(face.cy);
  fs_ = Spring::rd(0.36, 1.0, 0.003); fs_.snap(face.scale * s / ui_s);  // same size on screen, in home units
  place_face(now, false);
  happy_until_ = 0;
  face.mouth.target = 0;
  face.look(0, 0);
}

// ── the scene ───────────────────────────────────────────────────────
// Time in the scene: from the welcome's start, at 12 pictures a second; it opens on the teal mood
// (the one the light is shown with). Reduced motion: one still picture.
double OnboardingView::scene_time(double now) const {
  if (reduce_motion()) return kMoodDur;
  double t = std::max(0.0, now - t0_) + kMoodDur;
  return std::floor(t * kSceneFps) / kSceneFps;
}

OnboardingView::Mood OnboardingView::mood(double now) const {
  // three film moods the scene drifts through, each with the light it would cast
  static const Mood moods[3] = {
      {{38, 44, 104}, {232, 128, 72}, {255, 218, 150}, {74, 52, 92}, {26, 18, 38}, {96, 92, 190}, {244, 140, 76}},
      {{10, 54, 84}, {58, 186, 176}, {224, 255, 240}, {20, 88, 98}, {8, 36, 46}, {40, 120, 200}, {64, 206, 186}},
      {{30, 12, 64}, {186, 62, 168}, {255, 186, 238}, {58, 22, 82}, {18, 8, 30}, {120, 60, 210}, {214, 76, 190}},
  };
  double cyc = std::fmod(scene_time(now) / kMoodDur, 3.0);
  int i0 = (int)cyc % 3, i1 = (i0 + 1) % 3;
  double f = smoothstep(0.6, 1.0, cyc - std::floor(cyc));
  auto M = [&](Rgb Mood::*m) { return mix(moods[i0].*m, moods[i1].*m, f); };
  return {M(&Mood::top), M(&Mood::bot), M(&Mood::sun), M(&Mood::hill_far), M(&Mood::hill_near), M(&Mood::glow_top), M(&Mood::glow_bot)};
}

// What the scene shows (its colours, its progress line), coarse enough that the same signature
// means the same pixels: the welcome draws again only when it changes.
uint64_t OnboardingView::scene_signature(double now) const {
  double cyc = std::fmod(scene_time(now) / kMoodDur, 3.0);
  uint64_t i0 = (uint64_t)cyc % 3;
  uint64_t f = (uint64_t)std::lround(smoothstep(0.6, 1.0, cyc - std::floor(cyc)) * 255);
  uint64_t prog = 0;
  if (box_.w > 0) {
    const int cell = std::max(1, (int)std::lround(box_.w / 96.0));
    const int lw = box_.w - 4 * cell;
    prog = (uint64_t)((lw * (0.18 + 0.7 * std::fmod(scene_time(now) / 24.0, 1.0))) / cell);
  }
  return i0 | f << 4 | prog << 16;
}

void OnboardingView::draw_scene(Canvas& c, const Rect& r, double a, double now, bool progress) const {
  if (a <= 0.003) return;
  const Palette& p = P();
  const Mood m = mood(now);
  const int x0 = r.x, y0 = r.y, ww = r.w, hh = r.h;
  const int cell = std::max(1, (int)std::lround(ww / 96.0));
  c.fill(x0, y0, ww, hh, p.screen, a);
  const int bands = 9, sky_h = (int)(hh * 0.72);
  for (int b = 0; b < bands; ++b) {
    int by0 = y0 + sky_h * b / bands, by1 = y0 + sky_h * (b + 1) / bands;
    by0 = y0 + (by0 - y0) / cell * cell;
    by1 = y0 + (by1 - y0) / cell * cell;
    c.fill(x0, by0, ww, by1 - by0, mix(m.top, m.bot, b / (bands - 1.0)), a);
  }
  // the sun, a pixel disc low behind the hills
  const double sr = ww * 0.094;
  const double scx = x0 + ww * 0.62, scy = y0 + sky_h * 0.78;
  for (int yy = (int)(scy - sr); yy < scy + sr; yy += cell)
    for (int xx = (int)(scx - sr); xx < scx + sr; xx += cell) {
      int qx = x0 + (xx - x0) / cell * cell, qy = y0 + (yy - y0) / cell * cell;
      double dx = qx + cell / 2.0 - scx, dy = qy + cell / 2.0 - scy;
      if (dx * dx + dy * dy <= sr * sr && qy < y0 + hh && qx >= x0 && qx + cell <= x0 + ww) c.fill(qx, qy, cell, cell, m.sun, a);
    }
  // two ridges of hills, drawn column by column on the cell grid
  for (int xx = 0; xx < ww; xx += cell) {
    double u = (double)xx / ww;
    double far_h = hh * (0.40 + 0.10 * std::sin(u * 7.0 + 1.3) + 0.05 * std::sin(u * 17.0));
    double near_h = hh * (0.26 + 0.08 * std::sin(u * 5.0 + 4.0) + 0.03 * std::sin(u * 23.0 + 2.0));
    int fy = y0 + hh - (int)far_h / cell * cell, ny = y0 + hh - (int)near_h / cell * cell;
    int cw = std::min(cell, ww - xx);
    c.fill(x0 + xx, fy, cw, y0 + hh - fy, m.hill_far, a);
    c.fill(x0 + xx, ny, cw, y0 + hh - ny, m.hill_near, a);
  }
  // a whisper of the player: a progress line along the bottom (the third screen has the real one)
  if (progress) {
    int ly = y0 + hh - 2 * cell, lx0 = x0 + 2 * cell, lw = ww - 4 * cell;
    int played = (int)((lw * (0.18 + 0.7 * std::fmod(scene_time(now) / 24.0, 1.0))) / cell) * cell;
    c.fill(lx0, ly, lw, std::max(1, cell / 2), p.pixel, a * 0.25);
    c.fill(lx0, ly, played, std::max(1, cell / 2), p.pixel, a * 0.85);
  }
  // the frame: one art px of edge, stepped corners
  int e = std::max(1, (int)std::lround(s));
  c.fill(x0 + e, y0 - e, ww - 2 * e, e, p.edge, a);
  c.fill(x0 + e, y0 + hh, ww - 2 * e, e, p.edge, a);
  c.fill(x0 - e, y0 + e, e, hh - 2 * e, p.edge, a);
  c.fill(x0 + ww, y0 + e, e, hh - 2 * e, p.edge, a);
}

double OnboardingView::window_light() const {
  if (finish_t0_ > 0 || light_inside) return 0;
  double page = clamp01(1 - std::fabs(pos_.x - 1) * 1.7);
  return clamp01(knob_.x) * page;
}

// The halo's 26 stretches around the window, in the scene's colours: the sky's at the top, the
// ground's at the bottom, mixed down the sides.
void OnboardingView::scene_zones(double now, Rgb zone[kZones], double light[kZones]) const {
  const Mood m = mood(now);
  for (int k = 0; k < kZones; ++k) {
    double v;  // 0 at the top .. 1 at the bottom
    if (k < 8) v = 0;
    else if (k < 13) v = (k - 8 + 0.5) / 5.0;
    else if (k < 21) v = 1;
    else v = 1 - (k - 21 + 0.5) / 5.0;
    zone[k] = mix(m.glow_top, m.glow_bot, v);
    light[k] = 0.9;
  }
}

// ── tick ────────────────────────────────────────────────────────────
bool OnboardingView::tick(double now, double dt) {
  bool moving = false;
  double t = now - t0_;
  moving |= pos_.step(dt);
  moving |= dot_.step(dt);
  moving |= knob_.step(dt);
  moving |= glow_.step(dt);
  moving |= seg_x_.step(dt) | seg_w_.step(dt);
  moving |= bg_.step(dt);
  place_face(now, false);
  moving |= fx_.step(dt) | fy_.step(dt) | fs_.step(dt);
  place_face(now, false);

  // the eyes follow the cursor on the first screen
  if (finish_t0_ < 0) {
    bool cursor = tx_ > -100000 && now - track_moved_ < 2.5;
    if (cursor) face.look_at(tx_, ty_, 0.7);
    else face.look(0, 0);
  }
  face.happy = now < happy_until_;
  const bool face_seen = pos_.x < 0.999 || finish_t0_ > 0;
  if (face_seen) moving |= face.tick(now, dt);
  moving |= chrome.tick(dt);

  hov_.resize(kDot2 + 1, 0.0);
  int act = kb_ ? focus_ : hover_;
  for (int id = 0; id <= kDot2; ++id) {
    double v = hover_step(hov_[id], act == id, dt);
    if (v != hov_[id]) { hov_[id] = v; moving = true; }
  }
  double typed_end = kIntroType + cps(tr(str::ob_intro)) * kCharDur + 0.9 + 0.4;
  if (t < typed_end && pos_.x < 0.999) moving = true;
  if (finish_t0_ > 0 && now - finish_t0_ < 0.6) moving = true;
  // the scene plays only while it is seen, and draws again only when its picture changed
  if (pos_.x > 0.001 && finish_t0_ < 0) {
    uint64_t k = scene_signature(now);
    if (k != scene_key_) { scene_key_ = k; moving = true; }
  }
  if (now - scale_t_ < kScaleFade + 0.02) moving = true;
  return moving;
}

double OnboardingView::next_event(double now) const {
  if (finish_t0_ > 0 || !pos_.settled() || !knob_.settled()) return 0;
  if (now - scale_t_ < kScaleFade + 0.02) return 0;
  if (chrome.vis.v != chrome.vis.target) return 0;
  double t = now - t0_;
  if (page_ == 0) {
    double typed_end = kIntroType + cps(tr(str::ob_intro)) * kCharDur + 0.9 + 0.4;
    if (t < typed_end) return 0;
    return face.next_event(now);
  }
  if (reduce_motion()) return 60;
  // the next picture of the scene that differs from this one (a mood turning, the line moving)
  const uint64_t k = scene_signature(now);
  for (double d = 1 / kSceneFps; d < 4.0; d += 1 / kSceneFps)
    if (scene_signature(now + d) != k) return std::max(0.0, d - 0.004);
  return 4.0;
}

// ── input ───────────────────────────────────────────────────────────
int OnboardingView::hit(int x, int y) const {
  if (finish_t0_ > 0) return kNone;
  const Lay& L = L_[page_];
  if (L.next.grow(2 * s).contains(x, y)) return kNext;
  if (L.back.grow(2 * s).contains(x, y)) return page_ == 0 ? kSkip : kBack;
  for (int i = 0; i < (int)L.dots.size(); ++i)
    if (L.dots[i].contains(x, y)) return kDot0 + i;
  if (page_ == kPageLight && L.toggle.grow(3 * s).contains(x, y)) return kToggle;
  if (page_ == kPageLight) {  // the label next to the toggle flips it too
    Rect lab{L.toggle.r(), L.toggle.y, (int)(8 * s) + toggle_label_w(s), L.toggle.h};
    if (lab.contains(x, y)) return kToggle;
  }
  if (page_ == kPageSize)
    for (int i = 0; i < (int)L.segs.size(); ++i)
      if (L.segs[i].contains(x, y)) return kSeg0 + i;
  if (page_ == kPageBg)
    for (int k = 0; k < (int)L.segs.size(); ++k)
      if (L.segs[k].contains(x, y)) return kBg0 + k;
  return kNone;
}

bool OnboardingView::is_background(int x, int y) const {
  if (hit(x, y) != kNone || chrome.area(w).contains(x, y)) return false;
  return page_ != 0 || !face.bounds().contains(x, y);
}

std::vector<int> OnboardingView::focus_order() const {
  std::vector<int> o;
  if (page_ == kPageLight) o.push_back(kToggle);
  if (page_ == kPageBg) o.push_back(controls_free ? kBg1 : kBg0);
  if (page_ == kPageSize) o.push_back(kSeg0 + std::clamp(scale_index(), 0, kUiSizes - 1));
  o.push_back(page_ == 0 ? kSkip : kBack);
  o.push_back(kNext);
  return o;
}

ObAction OnboardingView::pick_scale(int index, double now) {
  int id = ui_size_choice(std::clamp(index, 0, kUiSizes - 1));
  if (id == ui_scale) return {};
  set_scale_choice(id, now);
  return {ObAct::UiScale, id};
}

ObAction OnboardingView::activate(int id, double now) {
  switch (id) {
    case kNext:
      if (page_ == kPages - 1) return {ObAct::Finish, 0};
      go(page_ + 1, now);
      return {};
    case kBack: go(page_ - 1, now); return {};
    case kSkip: return {ObAct::Finish, 0};
    case kToggle: {
      bool on = !ambient;
      set_ambient(on, now);
      return {ObAct::Ambient, on ? 1 : 0};
    }
    case kSeg0: case kSeg1: case kSeg2: case kSeg3: case kSeg4: return pick_scale(id - kSeg0, now);
    case kBg0: case kBg1: {
      const bool free = id == kBg1;
      if (free == controls_free) return {};
      set_controls_free(free, now);
      return {ObAct::ControlsBg, free ? 1 : 0};
    }
    case kDot0: case kDot1: case kDot2: case kDot3: go(id - kDot0, now); return {};
    default: return {};
  }
}

ObAction OnboardingView::mouse_move(int x, int y, double now) {
  (void)now;
  mx_ = x; my_ = y;
  kb_ = false;
  chrome.vis.target = 1;
  chrome.hot = chrome.hit(x, y, w);
  hover_ = hit(x, y);
  return {};
}

void OnboardingView::mouse_leave(double now) {
  (void)now;
  hover_ = kNone;
  mx_ = my_ = -1;
  chrome.vis.target = 0;
  chrome.hot = -1;
}

ObAction OnboardingView::mouse_down(int x, int y, double now) {
  switch (chrome.hit(x, y, w)) {
    case WindowChrome::Min: return {ObAct::WinMin, 0};
    case WindowChrome::Max: return {ObAct::WinMax, 0};
    case WindowChrome::Close: return {ObAct::WinClose, 0};
    case WindowChrome::Pin: return {ObAct::WinPin, 0};
    default: break;
  }
  if (page_ == 0 && face.bounds().contains(x, y) && finish_t0_ < 0) {  // a poke: it hops and smiles
    face.hop.v = -40;
    happy_until_ = now + 0.7;
    return {};
  }
  return activate(hit(x, y), now);
}

ObAction OnboardingView::key(const std::string& k, bool shift, double now) {
  if (finish_t0_ > 0) return {};
  if (k == "esc") return {ObAct::Finish, 0};
  if (k == "tab") {
    kb_ = true;
    auto o = focus_order();
    auto it = std::find(o.begin(), o.end(), focus_);
    int i = it == o.end() ? (shift ? (int)o.size() - 1 : 0)
                          : ((int)(it - o.begin()) + (shift ? (int)o.size() - 1 : 1)) % (int)o.size();
    focus_ = o[i];
    return {};
  }
  if (page_ == kPageBg && (k == "left" || k == "right")) {
    kb_ = true;
    focus_ = k == "right" ? kBg1 : kBg0;
    return activate(focus_, now);
  }
  if (page_ == kPageSize && (k == "left" || k == "right")) {
    kb_ = true;
    focus_ = kSeg0 + std::clamp(scale_index() + (k == "right" ? 1 : -1), 0, kUiSizes - 1);
    return pick_scale(focus_ - kSeg0, now);
  }
  if (k == "space" && page_ == kPageLight && (!kb_ || focus_ == kToggle || focus_ == kNone)) return activate(kToggle, now);
  if (k == "enter" || k == "space") {
    // enter on a choice confirms it and moves on; on a button it presses that button
    bool choice = focus_ == kToggle || (focus_ >= kSeg0 && focus_ <= kSeg4) || focus_ == kBg0 || focus_ == kBg1;
    if (kb_ && focus_ != kNone && !(k == "enter" && choice)) return activate(focus_, now);
    return activate(kNext, now);
  }
  if (k == "right") return activate(kNext, now);
  if (k == "left" || k == "backspace") { go(page_ - 1, now); return {}; }
  return {};
}

bool OnboardingView::track(int x, int y, double now) {
  if (x == tx_ && y == ty_) return false;
  tx_ = x; ty_ = y;
  track_moved_ = now;
  return page_ == 0 || pos_.x < 0.999;
}

// ── drawing ─────────────────────────────────────────────────────────
void OnboardingView::draw_button(Canvas& c, const Rect& r, const std::string& label, bool primary, double hv,
                                 double a, bool chevron, bool label_only) const {
  if (a <= 0.003) return;
  const Palette& p = P();
  if (!label_only) {
    if (primary) c.cornered(r, (int)s, mix(p.hover, p.press, hv), a);
    else if (hv > 0) c.cornered(r, (int)s, p.hover, a * hv);
  }
  int tw = text_w(label, s) + (chevron ? (int)std::lround(9 * s) : 0);
  // (the line box is 12 rows, its capitals a row lower: one row more above than below)
  int x = r.x + (r.w - tw) / 2, y = r.y + (int)std::lround((std::floor((r.h / s - 12) / 2) + 1) * s);
  Rgb col = primary ? mix(p.fg, p.pixel, hv) : mix(p.dim, p.fg, hv);
  text(c, label, x, y, s, col, a);
  if (chevron) {
    int nudge = (int)std::lround(hv * s);  // leans toward where it goes
    c.bitmap(bmp("chev-right5"), x + tw - (int)std::lround(3 * s) + nudge, y + (int)std::lround(2 * s), s, col, a);
  }
}

void OnboardingView::draw_toggle(Canvas& c, const Rect& r, double a, Rgb on) const {
  // the player's own switch (the same drawing), here at the welcome's pixel
  draw_switch(c, r.x, r.y, r.w / (double)kSwitchW, knob_.x, on, std::max(hov(kToggle), 0.0), true, a);
}

// The switch's colour on the second screen: the film's, as in the player (there it is the film
// playing; here the little scene), lifted so a dark mood never leaves it unreadable on black.
Rgb OnboardingView::scene_ink(double now) const {
  Rgb col = mood(now).glow_bot;
  const double luma = 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
  if (luma < 150) col = mix(col, {255, 255, 255}, (150 - luma) / std::max(1.0, 255 - luma));
  return mix(P().fg, col, 0.85);
}

// The player's cloud behind a bar without background, on the scene: the picture softened (two
// box passes, about the shader's reach) and mixed in by the same falloff as the shader's
// composition (0.35 at the middle, gone at the edge of an ellipse round the bar).
void OnboardingView::soften_under(Canvas& c, const Rect& scene, const Rect& bar, double a) const {
  const int rx = (int)std::lround(bar.w * 0.62), ry = (int)std::lround(bar.h * 1.25);
  const int x0 = std::max(bar.cx() - rx, std::max(scene.x, 0)), y0 = std::max(bar.cy() - ry, std::max(scene.y, 0));
  const int x1 = std::min(bar.cx() + rx, std::min(scene.r(), c.w)), y1 = std::min(bar.cy() + ry, std::min(scene.b(), c.h));
  const Rect r(x0, y0, x1 - x0, y1 - y0);
  if (r.w <= 2 || r.h <= 2) return;
  const int R = std::max(1, (int)std::lround(3 * s));
  std::vector<uint32_t> src((size_t)r.w * r.h), tmp(src.size());
  for (int y = 0; y < r.h; ++y) std::copy_n(&c.px[(size_t)(r.y + y) * c.w + r.x], r.w, &src[(size_t)y * r.w]);
  auto pass = [&](const std::vector<uint32_t>& in, std::vector<uint32_t>& out, bool horizontal) {
    for (int y = 0; y < r.h; ++y)
      for (int x = 0; x < r.w; ++x) {
        unsigned sum[3] = {0, 0, 0}, n = 0;
        for (int d = -R; d <= R; ++d) {
          const int xx = horizontal ? std::clamp(x + d, 0, r.w - 1) : x, yy = horizontal ? y : std::clamp(y + d, 0, r.h - 1);
          const uint32_t v = in[(size_t)yy * r.w + xx];
          sum[0] += v >> 16 & 255; sum[1] += v >> 8 & 255; sum[2] += v & 255; ++n;
        }
        out[(size_t)y * r.w + x] = 0xFF000000u | (sum[0] / n) << 16 | (sum[1] / n) << 8 | sum[2] / n;
      }
  };
  pass(src, tmp, true);
  std::vector<uint32_t> soft(src.size());
  pass(tmp, soft, false);
  for (int y = 0; y < r.h; ++y)
    for (int x = 0; x < r.w; ++x) {
      const double qx = (r.x + x + 0.5 - bar.cx()) / rx, qy = (r.y + y + 0.5 - bar.cy()) / ry;
      const double m = 0.35 * a * (1 - smoothstep(0.25, 1.0, std::sqrt(qx * qx + qy * qy)));
      if (m < 0.0001) continue;
      uint32_t& d = c.px[(size_t)(r.y + y) * c.w + r.x + x];
      const uint32_t sv = soft[(size_t)y * r.w + x];
      auto ch = [&](int sh) { const double o = d >> sh & 255, v = (sv >> sh & 255) * 0.95; return (uint32_t)std::lround(o + (v - o) * m); };
      d = (d & 0xFF000000u) | ch(16) << 16 | ch(8) << 8 | ch(0);
    }
}

// The player's controls over the scene, as the player draws them (the same icons, rows and
// measures, at the size chosen), on the bar's own background.
void OnboardingView::draw_controls(Canvas& c0, const Rect& scene, double a, int size_index, bool free, double menu_a) const {
  if (a <= 0.003) return;
  const Palette& p = P();
  const double cs = ctrl_s_[std::clamp(size_index, 0, kUiSizes - 1)];
  auto px = [&](double v) { return (int)std::lround(v * cs); };
  const int dw = px(kDockW), dh = px(kDockH);
  const int x = scene.x + (scene.w - dw) / 2, y = scene.b() - px(kDockAir) - dh;
  // (1.4.1) with its background, the bar's sheet; without it, the player's bar without background:
  // the picture softened behind the buttons (its cloud, as the player's shader composes it) and
  // white ink with the controls' soft shadow
  Canvas ink;
  if (free) {
    soften_under(c0, scene, Rect(x, y, dw, dh), a);
    ink.resize(c0.w, c0.h);
    ink.clear_transparent();
  }
  Canvas& c = free ? ink : c0;
  c.set_clip(scene);
  if (!free) draw_panel(c, x, y, kDockW, kDockH, cs, {2, 1}, p.bg, 0.92 * a, p.edge, 0.95 * a);
  // the bar without background: the play row and the times white, the others a quiet grey
  const Rgb quiet = free ? Rgb{196, 196, 196} : p.dim, bright = free ? p.fg : p.dim;
  auto icon = [&](const char* name, double cx, Rgb ink) {
    const BitmapDef* b = bmp(name);
    if (!b) return;
    int bx = x + px(cx) - (int)std::lround(b->w * cs / 2), by = y + px(12) - (int)std::lround(b->h * cs / 2);
    c.bitmap(b, bx, by, cs, ink, a);
  };
  // the buttons' row: the volume and its line on the left, play in the middle, the rest on the right
  icon("sound-on", 12, quiet);
  {
    int bx = x + px(24), bw = px(40), by = y + px(11);
    c.fill(bx, by, bw, px(2), free ? Rgb{70, 70, 70} : p.faint, a);
    c.fill(bx, by, px(28), px(2), quiet, a);
    c.fill(bx + px(28) - px(1), y + px(8), px(2), px(7), quiet, a);
  }
  icon("rewind", kDockW / 2.0 - 22, bright);
  icon("pause", kDockW / 2.0, p.fg);
  icon("forward", kDockW / 2.0 + 22, bright);
  icon("cc", kDockW - 54, quiet);
  icon("fs-enter", kDockW - 32, quiet);
  icon("dots", kDockW - 12, quiet);
  // the timeline's row: the time, the line, the length
  const std::string t0 = "0:42:10", t1 = "1:23:20";
  text(c, t0, x + px(8), y + px(22), cs, bright, a);
  text(c, t1, x + dw - px(8) - text_w(t1, cs), y + px(22), cs, bright, a);
  int lx = x + px(8) + text_w(t0, cs) + px(7), lw = x + dw - px(8) - text_w(t1, cs) - px(7) - lx;
  if (lw > 0) {
    int ly = y + px(28);
    c.fill(lx, ly, lw, px(1), free ? Rgb{60, 60, 60} : p.faint, a);
    int played = (int)std::lround(lw * 0.36 / cs) * (int)std::max(1.0, cs);
    c.fill(lx, ly, played, px(1), p.fg, a);
  }
  if (free) {
    c.reset_clip();
    white_control_shadow(ink, 1);
    c0.set_clip(scene);
    c0.blit(ink, 0, 0, 1.0);
  }
  // the "…" menu open above its button, as the player hangs it: words, rows and panel at the size
  // chosen, so the choice reads at a glance (the bar alone changed little between two sizes); only
  // on the size's screen
  if (menu_a * a > 0.003) {
    Canvas& c = c0;
    const double ka = a;
    a = a * menu_a;
    static const str::Id items[] = {str::speed_title, str::more_chapters, str::more_mini_player};
    constexpr int kRow = 15, kPad = 4, kSide = 9;
    int tw = 0;
    for (str::Id id : items) tw = std::max(tw, text_w(tr(id), 1));
    const int mw = std::max(72, tw + 2 * kSide), mh = 3 * kRow + 2 * kPad;
    const int mx = x + dw - px(mw), my = y - px(4) - px(mh);
    draw_panel(c, mx, my, mw, mh, cs, {2, 1}, p.bg, 0.92 * a, p.edge, 0.95 * a);
    for (int i = 0; i < 3; ++i) {
      const int ry = my + px(kPad + i * kRow);
      if (i == 0) c.cornered(Rect(mx + px(2), ry, px(mw - 4), px(kRow)), (int)std::max(1.0, std::floor(cs)), p.hover, a);
      text(c, tr(items[i]), mx + px(kSide), ry + px(2), cs, i == 0 ? p.fg : p.dim, a);
    }
    a = ka;
  }
  c.reset_clip();
  c0.reset_clip();
}

// The eyes on their line, with their reflection under it.
void OnboardingView::draw_eyes(Canvas& c, double a, double now) {
  if (a <= 0.003) return;
  face.now_hint = now;
  // the reflection: the eyes drawn apart, then copied upside down under the line, fading
  const Rect fb = face.bounds();
  const int top = std::max(fb.y, 0), bottom = line_at_;
  if (finish_t0_ < 0 && bottom > top && fb.w > 0) {
    Face f2 = face;
    f2.cx -= fb.x;
    f2.cy -= top;
    reflect_.resize(fb.w, bottom - top);
    reflect_.clear_transparent();
    f2.render(reflect_, 1.0);
    const int eye_h = (int)std::lround(6 * face.scale * s);
    const int gap = std::max(1, (int)std::lround(s));
    for (int y = 0; y < reflect_.h; ++y) {
      const int dy = reflect_.h - 1 - y;          // distance above the line
      const int ty = bottom + gap + dy;           // its image under it
      if (ty < 0 || ty >= c.h) continue;
      double k = 1 - dy / std::max(1.0, eye_h * 1.1);
      if (k <= 0) continue;
      k = 0.22 * k * k * a;
      const uint32_t* src = reflect_.px.data() + (size_t)y * reflect_.w;
      uint32_t* dst = c.px.data() + (size_t)ty * c.w;
      for (int x = 0; x < reflect_.w; ++x) {
        const int X = fb.x + x;
        if (X < 0 || X >= c.w || !src[x]) continue;
        const uint32_t sp = src[x], dp = dst[X];
        const double sa = (sp >> 24) / 255.0 * k;
        auto ch = [&](int sh) { return (uint32_t)std::min(255.0, ((sp >> sh) & 255) * k + ((dp >> sh) & 255) * (1 - sa)); };
        const uint32_t A = (uint32_t)std::min(255.0, (sp >> 24) * k + (dp >> 24) * (1 - sa));
        dst[X] = (A << 24) | (ch(16) << 16) | (ch(8) << 8) | ch(0);
      }
    }
  }
  face.render(c, a);
}

void OnboardingView::draw_words(Canvas& c, int pg, double ox, double a, double now) {
  if (a <= 0.003) return;
  const Palette& p = P();
  const Lay& L = L_[pg];
  const double W = w / s;
  const double CW = std::max(40.0, std::min(W - 32, 560.0));
  auto center_text = [&](const std::string& t, int y, double ts, Rgb col, double al) {
    int tw = text_w(t, ts);
    text(c, t, (int)std::lround(w / 2.0 - tw / 2.0 + ox), y, ts, col, al);
  };
  auto paragraph = [&](str::Id body, int y, Rgb col, double al) {
    for (auto& line : wrap_balanced(tr(body), (int)(CW * 0.86 * s), s)) { center_text(line, y, s, col, al); y += (int)std::lround(13 * s); }
  };
  const double t = now - t0_;
  const double hs = 2 * s;
  if (pg == 0) {
    // the greeting is typed, then its line comes in under it
    std::string head = tr(str::ob_intro);
    int n = cps(head);
    int shown = reduce_motion() ? n : std::clamp((int)std::floor((t - kIntroType) / kCharDur) + 1, 0, n);
    std::string part = first_cps(head, shown);
    int tw = text_w(head, hs);
    int hx = (int)std::lround(w / 2.0 - tw / 2.0 + ox);
    int pw = text(c, part, hx, title_y_, hs, p.pixel, a);
    double done_at = kIntroType + n * kCharDur;
    // a caret while typing, blinking a moment after
    bool caret = t >= kIntroType - 0.3 && (t < done_at || (t < done_at + 0.9 && std::fmod(t - done_at, 0.45) < 0.24));
    if (caret && !reduce_motion())
      c.fill(hx + pw + (int)std::lround(2 * s), title_y_ + (int)std::lround(4 * hs / 2), (int)std::lround(hs * 1.5),
             (int)std::lround(hs * 7), p.dim, a);
    double la = reduce_motion() ? 1.0 : ease_k((t - done_at - 0.15) / 0.35);
    if (la > 0) paragraph(kLines[0], line_y_ + (int)std::lround((1 - la) * 3) * (int)s, p.dim, a * la);
  } else {
    center_text(tr(kTitles[pg]), title_y_, hs, p.pixel, a);
    paragraph(kLines[pg], line_y_, p.dim, a);
  }
  if (pg == kPageLight) {
    Rect tgl = L.toggle;
    tgl.x += (int)std::lround(ox);
    draw_toggle(c, tgl, a, scene_ink(now));
    double kc = clamp01(knob_.x);
    int lx = tgl.r() + (int)std::lround(8 * s), ly = tgl.y + (int)std::lround(3 * s);
    // the label crossfades with the knob
    text(c, tr(str::ob_on), lx, ly, s, p.fg, a * kc);
    text(c, tr(str::ob_off), lx, ly, s, mix(p.dim, p.fg, hov(kToggle)), a * (1 - kc));
  } else if (pg == kPageBg && L.segs.size() == 2) {
    // the background's two choices, the highlight sliding between them as the sizes' does
    const double k = clamp01(bg_.x);
    const Rect& r0 = L.segs[0];
    const Rect& r1 = L.segs[1];
    Rect hl{(int)std::lround(r0.x + (r1.x - r0.x) * k + ox), r0.y, (int)std::lround(r0.w + (r1.w - r0.w) * k), r0.h};
    c.cornered(hl, (int)s, p.press, a);
    for (int i = 0; i < 2; ++i) {
      Rect r = L.segs[i];
      r.x += (int)std::lround(ox);
      const bool sel = (i == 1) == controls_free;
      const double hv = hov(kBg0 + i);
      if (!sel && hv > 0) c.cornered(r, (int)s, p.hover, a * hv);
      const std::string name = tr(i == 0 ? str::more_with_bg : str::more_without_bg);
      const int tw = text_w(name, s);
      text(c, name, r.x + (r.w - tw) / 2, r.y + (int)std::lround(3 * s), s, sel ? p.pixel : mix(p.dim, p.fg, hv), a);
    }
  } else if (pg == kPageSize) {
    // the selector: a sliding highlight behind the chosen size
    Rect hl{(int)std::lround(seg_x_.x + ox), L.segs[0].y, (int)std::lround(seg_w_.x), L.segs[0].h};
    c.cornered(hl, (int)s, p.press, a);
    int sel = std::clamp(scale_index(), 0, kUiSizes - 1);
    for (int i = 0; i < kUiSizes; ++i) {
      Rect r = L.segs[i];
      r.x += (int)std::lround(ox);
      double hv = hov(kSeg0 + i);
      if (i != sel && hv > 0) c.cornered(r, (int)s, p.hover, a * hv);
      int tw = text_w(tr(kSizeNames[i]), s);
      text(c, tr(kSizeNames[i]), r.x + (r.w - tw) / 2, r.y + (int)std::lround(3 * s), s,
           i == sel ? p.pixel : mix(p.dim, p.fg, hv), a);
    }
  }
}

void OnboardingView::draw_footer(Canvas& c, double a, double now) {
  (void)now;
  const Palette& p = P();
  const Lay& L = L_[page_];
  for (int i = 0; i < kPages; ++i) {
    // the current page's marker stretches into a dash (and slides there on a spring)
    double on = clamp01(1 - std::fabs(i - dot_.x));
    double dw = std::round(4 + 10 * on) * s, dh = 2 * s;
    const Rect& r = L.dots[i];
    double cx = r.x + r.w / 2.0;
    double hv = hov(kDot0 + i);
    c.fill(std::round(cx - dw / 2), r.y + (r.h - dh) / 2, dw, dh, mix(mix(p.faint, p.dim, hv), p.fg, on), a);
  }
}

void OnboardingView::render(Canvas& c, double now) {
  const Palette& p = P();
  if (c.w != w || c.h != h) c.resize(w, h);
  c.clear(p.bg);
  const double t = now - t0_;
  const double u = clamp01(pos_.x);  // 0 the eyes .. 1 the scene
  // the end: from the first screen the words and the light fade while the eyes fly home; from the
  // others everything goes dark before the home opens through the iris
  const double fade = finish_t0_ > 0 ? 1 - ease_k((now - finish_t0_) / (face_handoff_ ? 0.22 : kDarkDur)) : 1.0;
  auto alpha_of = [&](int pg) {
    // the page leaving is gone before the one arriving shows: their words never overlap
    double d = std::fabs(pg - pos_.x);
    return d >= 1 ? 0.0 : ease_out(clamp01(1 - d * 2.0));
  };
  const double intro = reduce_motion() ? 1.0 : ease_k((t - 0.1) / 0.45);

  // 1 · the light behind the eyes; it gives way to the scene's as the eyes close
  {
    const double la = (1 - smoothstep(0.05, 0.75, u)) * intro * fade;
    if (la >= 0.999 && eye_light_base_.w == eye_light_r_.w) paste(c, eye_light_base_, eye_light_r_.x, eye_light_r_.y);
    else if (la > 0.003) c.blit(eye_light_, eye_light_r_.x, eye_light_r_.y, la);
  }
  // 2 · the scene's own light, inside the window when it is maximized (otherwise the real halo)
  if (light_inside && finish_t0_ < 0) {
    const double ga = clamp01(glow_.x) * clamp01(1 - std::fabs(pos_.x - 1) * 1.7) * fade;
    if (ga > 0.003) {
      const Mood m = mood(now);
      // (made again only when its colours moved a visible step: a mood turning is a dozen
      // makings, not one per picture of the scene)
      auto q = [](uint8_t v) { return (uint64_t)(v >> 3); };
      const uint64_t key = (q(m.glow_top.r) << 40) ^ (q(m.glow_top.g) << 32) ^ (q(m.glow_top.b) << 24) ^ (q(m.glow_bot.r) << 16) ^
                           (q(m.glow_bot.g) << 8) ^ q(m.glow_bot.b) ^ ((uint64_t)box_.w << 48);
      if (key != scene_light_key_) {
        scene_light_key_ = key;
        const int reach = std::max(4, (int)std::lround(box_.w * 0.16));
        scene_light_r_ = {box_.x - reach, box_.y - reach, box_.w + 2 * reach, box_.h + 2 * reach};
        const Rgb corner[4] = {m.glow_top, m.glow_top, m.glow_bot, m.glow_bot};
        rect_light(scene_light_, scene_light_r_.w, scene_light_r_.h, {reach, reach, box_.w, box_.h}, reach, corner,
                   1.1 * kDemoStrength);  // (as strong as the halo outside: 0.55 was drawn for 50%)
      }
      c.blit(scene_light_, scene_light_r_.x, scene_light_r_.y, ga);
    }
  }
  // 3 · the scene (it stays put between the second and third screens), and the controls over it
  {
    const double sa = (pos_.x < 1 ? smoothstep(0.3, 0.9, u) : 1.0) * fade;
    draw_scene(c, box_, sa, now, false);
    const double pa = clamp01(1 - std::fabs(pos_.x - 1) * 1.7) * sa;
    if (pa > 0.003) {
      // its progress line belongs to the second screen (the third shows the controls' own)
      const int cell = std::max(1, (int)std::lround(box_.w / 96.0));
      const int ly = box_.b() - 2 * cell, lx0 = box_.x + 2 * cell, lw = box_.w - 4 * cell;
      const int played = (int)((lw * (0.18 + 0.7 * std::fmod(scene_time(now) / 24.0, 1.0))) / cell) * cell;
      c.fill(lx0, ly, lw, std::max(1, cell / 2), p.pixel, pa * 0.25);
      c.fill(lx0, ly, played, std::max(1, cell / 2), p.pixel, pa * 0.85);
    }
    // the controls come with the third screen (the background) and stay on the fourth (the size),
    // where the "…" menu opens over them; a new size crossfades from the one before (both at their
    // own hard pixel), a new background from the other
    const double ca = (pos_.x <= kPageBg ? alpha_of(kPageBg) : 1.0) * fade, ma = alpha_of(kPageSize) * fade;
    const int cur = std::clamp(scale_index(), 0, kUiSizes - 1);
    const double sk = reduce_motion() ? 1.0 : ease_k((now - scale_t_) / kScaleFade);
    const double fk = clamp01(bg_.x);
    auto both = [&](double a, int size) {
      if (fk < 0.999) draw_controls(c, box_, a * (1 - fk), size, false, ma);
      if (fk > 0.001) draw_controls(c, box_, a * fk, size, true, ma);
    };
    if (sk < 1 && prev_scale_idx_ >= 0 && prev_scale_idx_ != cur) {
      both(ca * (1 - sk), prev_scale_idx_);
      both(ca * sk, cur);
    } else {
      both(ca, cur);
    }
  }
  // 4 · the words, sliding a little with their page
  const double foot_in = reduce_motion() ? 1.0 : ease_k((t - kFootIn) / 0.4);
  for (int pg = 0; pg < kPages; ++pg) {
    double d = pg - pos_.x;
    if (std::fabs(d) >= 1) continue;
    draw_words(c, pg, d * 28 * s, alpha_of(pg) * fade, now);
  }
  // the footer stands still: its buttons change shape and words between pages, never fly
  {
    const double fx = std::clamp(pos_.x, 0.0, kPages - 1.0);
    const int p0 = std::min(kPages - 2, (int)std::floor(fx)), p1 = p0 + 1;
    const double f = clamp01(fx - p0), fl = smoothstep(0.2, 0.8, f);
    auto lerp_r = [&](const Rect& a, const Rect& b) {
      return Rect((int)std::lround(a.x + (b.x - a.x) * f), a.y, (int)std::lround(a.w + (b.w - a.w) * f), a.h);
    };
    const double ba = fade * foot_in;
    const Rect nb = lerp_r(L_[p0].next, L_[p1].next), bb = lerp_r(L_[p0].back, L_[p1].back);
    auto next_l = [](int pg) { return tr(pg == kPages - 1 ? str::ob_start : str::ob_continue); };
    auto back_l = [](int pg) { return tr(pg == 0 ? str::ob_skip : str::ob_back); };
    const double hn = hov(kNext), hb = std::max(hov(kSkip), hov(kBack));
    // one plate each; the labels crossfade over it
    const Palette& pp = P();
    if (ba > 0.003) {
      c.cornered(nb, (int)s, mix(pp.hover, pp.press, hn), ba);
      if (hb > 0) c.cornered(bb, (int)s, pp.hover, ba * hb);
    }
    // (the same word on both pages is drawn once: two half-faded copies would dim it midway)
    auto labels = [&](const Rect& r, const std::string& l0, const std::string& l1, bool primary, double hv) {
      if (l0 == l1) { draw_button(c, r, l0, primary, hv, ba, primary, true); return; }
      draw_button(c, r, l0, primary, hv, ba * (1 - fl), primary, true);
      draw_button(c, r, l1, primary, hv, ba * fl, primary, true);
    };
    labels(nb, next_l(p0), next_l(p1), true, hn);
    labels(bb, back_l(p0), back_l(p1), false, hb);
  }
  draw_footer(c, fade * foot_in, now);
  chrome.draw(c, w);
  // 5 · the character belongs to the first screen: it closes its eyes on the way to the scene,
  // and only flies to the home when the welcome ends while it is on screen
  double fa = finish_t0_ > 0 ? (face_handoff_ ? 1.0 : 0.0) : intro * (1 - smoothstep(0.2, 0.6, u));
  if (fa > 0.003) draw_eyes(c, fa, now);
}
