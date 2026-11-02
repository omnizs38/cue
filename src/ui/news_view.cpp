#include "news_view.h"

#include <algorithm>
#include <cmath>

#include "../core/i18n.h"

namespace {
// What arrived, in the order it is told: an icon, a title, one short line. The first is the
// headline of the version and spans the whole width on a card.
struct Item { str::Id title, line; };
constexpr int kItemCount = 3;
const Item kItems[kItemCount] = {
    {str::news_remote_title, str::news_remote_line},
    {str::news_audio_title, str::news_audio_line},
    {str::news_fixes_title, str::news_fixes_line},
};
constexpr int kCols = 18, kRows = 14;  // icon cells, 1.5 art px each (like the welcome's)
inline int icon_cell(double s) { return std::max(1, (int)std::lround(1.5 * s)); }
// '#' drawing; '*' a sparkle that blinks. Icon 0 (the queue) is drawn entirely in code.
const char* kIcons[kItemCount][kRows] = {
    {"..................", "..................", "..................", "..................", "..................",
     "..................", "..................", "..................", "..................", "..................",
     "..................", "..................", "..................", ".................."},
    {  // a speech bubble; the letter inside changes language
        "..##############..",
        ".#..............#.",
        "#................#",
        "#................#",
        "#................#",
        "#................#",
        "#................#",
        "#................#",
        "#................#",
        ".#..............#.",
        "..###..#########..",
        "....#.#...........",
        "....##............",
        "..................",
    },
    {  // a gear, and a sparkle: things that work better
        "........##......*.",
        ".....#.####.#..***",
        "....##########..*.",
        ".....###..###.....",
        "....###....###....",
        "...####....####...",
        "...####....####...",
        "....###....###....",
        ".....###..###.....",
        "....##########....",
        ".....#.####.#.....",
        "........##........",
        "..................",
        "..................",
    },
};
const char* kKernel[2] = {".#.###.#.", "##.###.##"};
constexpr double kGravity = 140;  // art px per second squared
}  // namespace

void NewsView::set_size(int w_, int h_, double s_) {
  if (w_ <= 0 || h_ <= 0) return;
  w = w_;
  h = h_;
  ui_s = std::max(1.0, s_);
  // a comfortable read, like the welcome: about 720 x 405 art px fill the window
  double fit = std::floor(std::min(w / 720.0, h / 405.0) * 2) / 2;
  s = std::max(ui_s, std::min(std::max(1.0, fit), ui_s * 1.5));
  layout();
  while (overflow_ && s > 1) { s = std::max(1.0, s - 0.5); layout(); }
}

void NewsView::layout() {
  const double W = w / s, H = h / s;
  const double CW = std::min(W - 32, 540.0), ox = (W - CW) / 2;
  int ic = icon_cell(s);
  double icon_w = kCols * ic / s, icon_h = kRows * ic / s;
  auto R = [&](double x, double y, double ww, double hh) { return Rect(x * s, y * s, ww * s, hh * s); };
  // two columns when each still has room for the words; one in a narrow window
  cols_ = CW >= 2 * (icon_w + 12 + 160) + 28 ? 2 : 1;
  double colw = cols_ == 2 ? (CW - 28) / 2 : std::min(CW, 340.0);
  // the headline item: a card as wide as both columns, its words beside a padded icon
  double hero_w = cols_ == 2 ? CW : colw, pad = 8;
  lines_.clear();
  for (int i = 0; i < kItemCount; ++i) {
    double words_w = (i == 0 ? hero_w - 2 * pad : colw) - icon_w - 12;
    auto l = wrap_lines(tr(kItems[i].line), (int)std::lround(words_w * s), s);
    if (l.size() > 4) l.resize(4);
    lines_.push_back(l);
  }
  auto text_h = [&](int i) { return 12.0 + 4 + 12.0 * lines_[i].size(); };
  double hero_h = std::max(icon_h, text_h(0)) + 2 * pad;
  std::vector<double> rh;
  if (cols_ == 2) rh.push_back(std::max({icon_h, text_h(1), text_h(2)}));
  else { rh.push_back(std::max(icon_h, text_h(1))); rh.push_back(std::max(icon_h, text_h(2))); }
  double list_h = hero_h + 16;
  for (double v : rh) list_h += v;
  list_h += 16 * (rh.size() - 1);
  // eyes, heading (two lines tall), its line, the list, the buttons
  double total = 22 + 6 + 24 + 6 + 12 + 22 + list_h + 24 + 16;
  double top = std::max(16.0, (H - total) / 2);
  overflow_ = top + total > H - 10;
  double y = top;
  eye_y_ = (int)std::lround((y + 10) * s);
  y += 22 + 6;
  head_y_ = (int)std::lround(y * s);
  y += 24 + 6;
  line_y_ = (int)std::lround(y * s);
  y += 12 + 22;
  items_.clear();
  double lx = cols_ == 2 ? ox : std::round(W / 2 - colw / 2);
  items_.push_back(R(lx, y, hero_w, hero_h));
  double ry = y + hero_h + 16;
  if (cols_ == 2) { for (int c = 0; c < 2; ++c) items_.push_back(R(lx + c * (colw + 28), ry, colw, rh[0])); ry += rh[0] + 16; }
  else for (double v : rh) { items_.push_back(R(lx, ry, colw, v)); ry += v + 16; }
  y = ry - 16 + 24;
  // one button, in the middle
  double cw = text_w(tr(str::ob_continue), 1) + 9 + 22;
  buttons_[kContinue] = R(std::round(W / 2 - cw / 2), y, cw, 16);
  // the eyes: block of 2 art px at this scale, a little above the heading
  face_.layout(w / 2.0, eye_y_ + 3 * 1.6 * s, s, 1.6);
  face_.max_look = 2.5;
}

void NewsView::open(double now) {
  open_t0_ = now;
  close_t0_ = -1;
  hover_ = focus_ = -1;
  kb_ = false;
  kernels_.clear();
  face_ = Face{};
  layout();
  face_.set_mode(Face::Mode::Follow, now);
  face_.happy = true;
  next_pop_ = now + 0.4;
}

void NewsView::pop(double now) {
  Kernel k{};
  k.t0 = now;
  k.spin = rng_.pick(2);
  // from either end of the heading, outwards: the eyes stay clear
  int side = rng_.pick(2) ? 1 : -1;
  double half = text_w(tr(str::news_title), 2) / 2.0;
  k.x = w / 2.0 / s + side * (half + rng_.range(6.0, 34.0));
  k.y = head_y_ / s + 10;
  k.vx = side * rng_.range(4.0, 30.0);
  k.vy = -rng_.range(70.0, 96.0);
  kernels_.push_back(k);
}

bool NewsView::tick(double now, double dt) {
  double t = now - open_t0_;
  // a handful of kernels as it opens, then one now and then: still popping
  if (!reduce_motion() && close_t0_ < 0 && now >= next_pop_) {
    pop(now);
    if (t < 1.4) next_pop_ = now + rng_.range(0.08, 0.2);
    else next_pop_ = now + rng_.range(2.2, 4.8);
    if (rng_.uni() < 0.35) face_.notice(now);
  }
  kernels_.erase(std::remove_if(kernels_.begin(), kernels_.end(), [&](const Kernel& k) { return now - k.t0 > 1.6; }), kernels_.end());
  // happy while the popcorn flies, then just pleased to see you
  face_.happy = t < 1.6 || (!kernels_.empty() && now - kernels_.back().t0 < 0.5);
  face_.tick(now, dt);
  int active = kb_ ? focus_ : hover_;
  hov_[kContinue] = hover_step(hov_[kContinue], active == kContinue, dt);
  return true;
}

void NewsView::draw_button(Canvas& c, const Rect& r, const std::string& label, bool primary, double hv, double a,
                           bool chevron) const {
  const Palette& p = P();
  if (primary) c.cornered(r, (int)s, mix(p.hover, p.press, hv), a);
  else if (hv > 0) c.cornered(r, (int)s, p.hover, a * hv);
  int tw = text_w(label, s) + (chevron ? (int)std::lround(9 * s) : 0);
  int x = r.x + (r.w - tw) / 2, y = r.y + (int)std::lround(3 * s);
  Rgb col = primary ? mix(p.fg, p.pixel, hv) : mix(p.dim, p.fg, hv);
  text(c, label, x, y, s, col, a);
  if (chevron) {
    int nudge = (int)std::lround(hv * s);
    c.bitmap(bmp("chev-right5"), x + tw - (int)std::lround(3 * s) + nudge, y + (int)std::lround(2 * s), s, col, a);
  }
}

void NewsView::draw_icon(Canvas& c, int which, int x, int y, Rgb ink, double a, double now) const {
  const Palette& p = P();
  int ic = icon_cell(s);
  double cyc = std::fmod(std::max(0.0, now) * 1.3, 1.0);
  for (int yy = 0; yy < kRows; ++yy)
    for (int xx = 0; xx < kCols; ++xx) {
      char ch = kIcons[which][yy][xx];
      if (ch == '.') continue;
      double ca = 1.0;
      if (ch >= '1' && ch <= '3') {
        double ph = cyc * 4 - (ch - '1');
        ca = reduce_motion() ? 0.8 : 0.25 + 0.75 * (ph >= 0 && ph < 1.6 ? 1.0 : 0.0);
      } else if (ch == '*') {
        ca = reduce_motion() || std::fmod(now, 1.6) < 0.9 ? 1.0 : 0.25;
      }
      c.fill(x + xx * ic, y + yy * ic, ic, ic, ink, a * ca);
    }
  if (which == 0) {
    // up next: four rows (a thumbnail and a title bar) climbing one step at a time; the top one
    // is playing (bright, with a marker), a new row arrives at the bottom. Positions move in
    // whole cells, like the pixel interface: a pose, a short rest, the next pose.
    double cycle = reduce_motion() ? 0.0 : std::fmod(std::max(0.0, now), 1.8) / 1.8;
    double step = cycle < 0.72 ? 0.0 : ease_out((cycle - 0.72) / 0.28);  // rest, then move
    static const int bars[5] = {11, 8, 12, 7, 10};
    int turn = reduce_motion() ? 0 : (int)(std::max(0.0, now) / 1.8);
    for (int i = 0; i < 4; ++i) {
      int row = (int)std::lround((1 + 4 * i - 4 * step));  // in cells
      if (row < -1 || row > kRows - 2) continue;
      double ra = a;
      if (row < 1) ra *= clamp01(row + 1.0) * 0.5;               // leaving at the top
      if (i == 3) ra *= step;                                      // arriving at the bottom
      if (ra <= 0) continue;
      bool playing = (i == 0 && step < 0.5) || (i == 1 && step >= 0.5);
      Rgb col = playing ? p.pixel : ink;
      double ca = playing ? 1.0 : 0.55;
      int ry = y + row * ic;
      if (playing) c.fill(x, ry, ic, 3 * ic, col, ra);              // the now-playing bar
      c.fill(x + 2 * ic, ry, 4 * ic, 3 * ic, col, ra * ca);        // thumbnail
      int len = bars[(i + turn) % 5];
      c.fill(x + 7 * ic, ry + ic / 2, std::min(len, 11) * ic, ic, col, ra * ca);
      c.fill(x + 7 * ic, ry + 2 * ic, (len / 2) * ic, ic, col, ra * ca * 0.6);
    }
  } else if (which == 1) {
    // the letter inside the bubble changes language: Latin, Cyrillic, kana, German
    static const char* letters[4] = {"A", "\xd0\xaf", "\xe3\x81\x82", "\xc3\x9c"};
    double t = reduce_motion() ? 0.0 : std::max(0.0, now) / 1.1;
    int k = (int)t % 4;
    double in = reduce_motion() ? 1.0 : ease_out(clamp01((t - std::floor(t)) / 0.22));
    const char* g = letters[k];
    int gw = text_w(g, s), gx = x + 9 * ic - gw / 2;
    int gy = y + (int)std::lround(5.5 * ic - 6 * s) + (int)std::lround((1 - in) * 2 * s);
    text(c, g, gx, gy, s, p.pixel, a * in);
  }
}

void NewsView::render(Canvas& c, double now) {
  const Palette& p = P();
  if (c.w != w || c.h != h) c.resize(w, h);
  double t = now - open_t0_;
  double enter = reduce_motion() ? 1.0 : ease_out(t / 0.28);
  double leave = close_t0_ > 0 ? 1.0 - clamp01((now - close_t0_) / 0.24) : 1.0;
  c.clear(p.bg, 0.97);
  auto at = [&](double delay) { return reduce_motion() ? 1.0 : ease_out(clamp01((t - delay) / 0.34)); };

  // popcorn behind the heading, in its own little arcs
  for (auto& k : kernels_) {
    double kt = now - k.t0;
    double x = k.x + k.vx * kt, y = k.y + k.vy * kt + 0.5 * kGravity * kt * kt;
    double ka = kt > 1.2 ? 1 - (kt - 1.2) / 0.4 : 1.0;
    int cell = (int)std::max(1.0, std::round(1.5 * s));
    c.cells(kKernel[k.spin ^ ((int)(kt / 0.09) & 1)], 3, 3, (int)std::lround(x * s) / cell * cell, (int)std::lround(y * s) / cell * cell, cell,
            p.fg, std::max(0.0, ka) * 0.9);
  }
  face_.now_hint = now;
  face_.render(c, at(0.0));

  // heading, with the version on a pill beside it
  {
    double k = at(0.05);
    int lift = (int)std::lround((1 - k) * 6) * (int)s;
    std::string head = tr(str::news_title);
    int hw = text_w(head, 2 * s);
    int pill_w = text_w(version, s) + (int)std::lround(12 * s), gap = (int)std::lround(8 * s);
    int x = w / 2 - (hw + gap + pill_w) / 2, y = head_y_ + lift;
    text(c, head, x, y, 2 * s, p.pixel, k);
    Rect pill{x + hw + gap, y + (int)std::lround(5 * s), pill_w, (int)std::lround(16 * s)};
    c.cornered(pill, (int)s, p.fg, k);
    text(c, version, pill.x + (int)std::lround(6 * s), pill.y + (int)std::lround(3 * s), s, p.bg, k);
    std::string line = tr(str::news_line);
    text(c, line, w / 2 - text_w(line, s) / 2, line_y_ + lift, s, p.dim, k);
  }

  // what arrived: one after the other, rising a few px of art
  int ic = icon_cell(s);
  for (size_t i = 0; i < items_.size() && i < (size_t)kItemCount; ++i) {
    double k = at(0.18 + 0.08 * i);
    if (k <= 0.003) continue;
    Rect r = items_[i];
    r.y += (int)std::lround((1 - k) * 6 * s);
    if (i == 0) {
      // the headline: on its own card, content inset
      c.cornered(r, (int)s, p.band, k);
      r = r.inset((int)std::lround(8 * s));
    }
    int iy = r.y + (int)std::lround(1 * s);
    draw_icon(c, (int)i, r.x, iy, p.fg, k, now + i * 0.37);
    int tx = r.x + kCols * ic + (int)std::lround(12 * s);
    text(c, tr(kItems[i].title), tx, r.y, s, p.pixel, k);
    int ly = r.y + (int)std::lround(16 * s);
    for (auto& l : lines_[i]) { text(c, l, tx, ly, s, p.dim, k); ly += (int)std::lround(12 * s); }
  }

  double kb = at(0.55);
  draw_button(c, buttons_[kContinue], tr(str::ob_continue), true, hov_[kContinue], kb, true);
  if (kb_ && focus_ >= 0) {
    // keyboard focus: a 1 px ring on the art grid
    Rect r = buttons_[focus_].grow((int)s);
    int a = (int)s;
    c.fill(r.x + a, r.y, r.w - 2 * a, a, p.dim, kb);
    c.fill(r.x + a, r.b() - a, r.w - 2 * a, a, p.dim, kb);
    c.fill(r.x, r.y + a, a, r.h - 2 * a, p.dim, kb);
    c.fill(r.r() - a, r.y + a, a, r.h - 2 * a, p.dim, kb);
  }
  double alpha = enter * leave;
  if (alpha < 0.999) c.scale_alpha(alpha);
}

int NewsView::hit(int x, int y) const {
  if (buttons_[kContinue].grow((int)(2 * s)).contains(x, y)) return kContinue;
  return -1;
}

void NewsView::pointer_move(int x, int y, double now) {
  kb_ = false;
  hover_ = hit(x, y);
  face_.look_at(x, y, 0.8);
  (void)now;
}

SceneAct NewsView::pointer_down(int x, int y, double now) {
  int id = hit(x, y);
  if (id == kContinue) return {(int)NewsAct::Continue, -1};
  if (face_.bounds().contains(x, y)) { face_.notice(now); pop(now); pop(now); }  // poke: more popcorn
  return {};
}

SceneAct NewsView::key_down(const std::string& k, bool shift, double now) {
  (void)now; (void)shift;
  if (k == "esc") return {(int)NewsAct::Continue, -1};
  if (k == "tab" || k == "left" || k == "right") {
    kb_ = true;
    focus_ = kContinue;
    return {};
  }
  if (k == "enter" || k == "space") {
    return {(int)NewsAct::Continue, -1};
  }
  return {};
}
