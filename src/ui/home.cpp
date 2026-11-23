#include "home.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "../core/i18n.h"
#include "thumb_art.h"

namespace {
// art px: a title, its quiet line and a progress rule, with air between entries (the hover's
// rounded box keeps 4 px around the words)
constexpr int kRowPitch = 38;
constexpr int kRowH = 33;
constexpr int kTitleGap = 24;  // "Recentes" to its first entry
constexpr int kContGap = 14;   // the entry to continue to "recentes" under it
// the ··· menu: the player's menus (rows of 13 px plus their breath, a hairline between groups)
// (16/11, as the player's menus: 10 art px of air above the first line's capitals and under the
// last's, and as much above the hairline as under it; the rule was 9 and the foot 4)
constexpr int kMenuRow = 18, kMenuRule = 10, kMenuInset = 4, kMenuFoot = 5;
const std::vector<int> kMenuProfile = {2, 1};
constexpr int kMaxRows = 6;    // the plain list's most (with pictures: the one to continue and three)
constexpr int kMaxRecent = 3;
constexpr double kEggDur[5] = {0, 5.4, 4.6, 5.4, 3.6};
// which surprises were seen: Christmas uses its own bits (5..7), so the old ones never count
constexpr int seen_bit(int v) { return v >= 1 && v <= 3 ? 1 << (v + 4) : 1 << v; }
}  // namespace

void HomeView::set_size(int w_, int h_, double s_) {
  w = w_;
  h = h_;
  s = std::max(1.0, s_);
  chrome.s = s;
  layout();
}

void HomeView::set_rows(std::vector<HomeRow> r) {
  if ((int)r.size() > kMaxRows) r.resize(kMaxRows);
  all_rows_ = std::move(r);
  layout();
}

namespace {
// clearing the list: each entry leaves 35 ms after the one above it, in 0.24 s; then the title
// takes 0.3 s to its new place, and the empty line comes in over its second half
constexpr double kClearStagger = 0.035, kClearRow = 0.24, kClearTitle = 0.3;
}  // namespace

double HomeView::clear_end() const {
  if (ghosts_.empty()) return clear_t0_;
  return clear_t0_ + kClearStagger * (ghosts_.size() - 1) + kClearRow - 0.08 + kClearTitle;
}

// how far the title has glided (0 at its old place) and the empty line come in, now
void HomeView::clear_fx(double now, double& title_k, double& empty_k) const {
  title_k = empty_k = 1;
  if (ghosts_.empty()) return;
  const double start = clear_t0_ + kClearStagger * (ghosts_.size() - 1) + kClearRow - 0.08;
  title_k = ease_in_out(std::clamp((now - start) / kClearTitle, 0.0, 1.0));
  empty_k = ease_out(std::clamp((now - start - kClearTitle * 0.45) / (kClearTitle * 0.75), 0.0, 1.0));
}

void HomeView::clear_animated(double now, Canvas* scratch) {
  ghosts_.clear();
  if (rows.empty() || reduce_motion() || w <= 0 || h <= 0) return;
  // the list as it is on screen, without the menu (it goes at the click) or a hover
  menu_ = false;
  menu_fade_.v = menu_fade_.target = 0;
  const int keep_focus = focus_;
  focus_ = -1;
  std::vector<double> keep_hov = hov_;
  for (size_t k = 0; k < rows.size() && k < hov_.size(); ++k) hov_[k] = 0;
  Canvas own;
  Canvas& snap = scratch ? *scratch : own;
  render(snap, now);
  hov_ = keep_hov;
  focus_ = keep_focus == kMenuClear ? -1 : keep_focus;
  std::vector<Rect> leaving;
  if (cont_) leaving.push_back(r_cont_label_.grow(2 * s));  // ("continuar" leaves first, with its entry)
  leaving.insert(leaving.end(), r_rows_.begin(), r_rows_.end());
  for (const Rect& r : leaving) {
    Ghost g;
    g.r = {std::max(0, r.x), std::max(0, r.y), std::min(r.w, w - std::max(0, r.x)), std::min(r.h, h - std::max(0, r.y))};
    if (g.r.w <= 0 || g.r.h <= 0) continue;
    g.img.resize(g.r.w, g.r.h);
    for (int y = 0; y < g.r.h; ++y)
      std::copy_n(&snap.px[(size_t)(g.r.y + y) * w + g.r.x], g.r.w, &g.img.px[(size_t)y * g.r.w]);
    ghosts_.push_back(std::move(g));
  }
  title_from_y_ = r_title_.y;
  clear_t0_ = now;
  blinked_ = false;
  update_face_target(now);
}

// Everything of the list on screen: its titles, its entries and the light around the one to
// continue (its reach), within the window.
Rect HomeView::list_band() const {
  Rect band = r_list_;
  for (const Rect& r : {r_title_, r_dots_, r_cont_label_}) {
    if (r.w <= 0 || r.h <= 0) continue;
    const int x0 = std::min(band.x, r.x), y0 = std::min(band.y, r.y);
    band = {x0, y0, std::max(band.r(), r.r()) - x0, std::max(band.b(), r.b()) - y0};
  }
  band = band.grow((int)std::lround(32 * s));
  const int x0 = std::max(0, band.x), y0 = std::max(0, band.y), x1 = std::min(w, band.r()), y1 = std::min(h, band.b());
  return x1 > x0 && y1 > y0 ? Rect{x0, y0, x1 - x0, y1 - y0} : Rect{};
}

Rect HomeView::list_reach() const {
  Rect band = r_title_;
  for (const Rect& r : {r_list_, r_dots_, r_cont_label_}) {
    if (r.w <= 0 || r.h <= 0) continue;
    const int x0 = std::min(band.x, r.x), y0 = std::min(band.y, r.y);
    band = {x0, y0, std::max(band.r(), r.r()) - x0, std::max(band.b(), r.b()) - y0};
  }
  return band.grow(2 * s);
}

void HomeView::cover_list(bool on) { list_cover_.target = on ? 1.0 : 0.0; }

void HomeView::swap_animated(double now, Canvas* scratch) {
  swap_t0_ = -10;
  swap_old_ = Canvas{};
  if (reduce_motion() || w <= 0 || h <= 0 || leave_t0_ > 0) return;
  // the list as it is on screen, without the menu (it goes at the click) or a hover
  menu_ = false;
  menu_fade_.v = menu_fade_.target = 0;
  std::vector<double> keep_hov = hov_;
  for (size_t k = 0; k < rows.size() && k < hov_.size(); ++k) hov_[k] = 0;
  // only the list's band is drawn for it (a clip: the same drawing code, nothing else touched):
  // a whole home at 4K was 33 MB drawn at the click (16/11: that is why it did not move under Settings)
  swap_r_ = list_band();
  if (swap_r_.w <= 0) { hov_ = keep_hov; return; }
  Canvas own;
  Canvas& snap = scratch ? *scratch : own;
  if (snap.w != w || snap.h != h) snap.resize(w, h);
  snap.set_clip(swap_r_);
  render(snap, now);
  snap.reset_clip();
  hov_ = keep_hov;
  swap_old_.resize(swap_r_.w, swap_r_.h);
  for (int y = 0; y < swap_r_.h; ++y)
    std::copy_n(&snap.px[(size_t)(swap_r_.y + y) * w + swap_r_.x], swap_r_.w, &swap_old_.px[(size_t)y * swap_r_.w]);
  swap_t0_ = now;
}

// Over the new list as it was drawn: it is hidden at first and comes in rising a few pixels
// (covered by the room's colour, lifting off), while the old one fades out on top.
void HomeView::draw_swap(Canvas& c, double now) const {
  if (!swapping(now) || swap_old_.w <= 0) return;
  const Palette& p = P();
  const double t = now - swap_t0_;
  const double in = ease_out(std::clamp((t - kSwapDelay) / kSwapIn, 0.0, 1.0));
  const Rect band = list_band();
  if (band.w > 0 && in < 1) {
    // the new list, risen from 4 px of art below: the band shifted down, then covered as it fades in
    const int rise = (int)std::lround((1 - in) * 4) * (int)std::lround(s);
    if (rise > 0) {
      for (int y = band.b() - 1; y >= band.y + rise; --y)
        std::copy_n(&c.px[(size_t)(y - rise) * c.w + band.x], band.w, &c.px[(size_t)y * c.w + band.x]);
      c.fill(band.x, band.y, band.w, rise, p.bg, 1.0);
    }
    c.fill(band, p.bg, 1 - in);
  }
  const double out = 1 - ease_out(std::clamp(t / kSwapOut, 0.0, 1.0));
  if (out > 0.003) c.blit(swap_old_, swap_r_.x, swap_r_.y, out);
}

void HomeView::set_thumb(int i, std::shared_ptr<const HomeThumb> t, double now) {
  if (i < 0 || i >= (int)all_rows_.size() || all_rows_[i].thumb == t) return;
  const bool had = all_rows_[i].thumb != nullptr;
  all_rows_[i].thumb = std::move(t);
  // a first picture fades in over the stand-in; a newer one (the frame moved on) simply replaces it,
  // and one that arrives while the list still waits for it comes in with the list
  all_rows_[i].thumb_at = had || reduce_motion() || list_held(now) ? -100 : now;
  if (i < (int)rows.size()) rows[i] = all_rows_[i];
}

void HomeView::hold_list(double now, double secs) {
  list_hold_until_ = reduce_motion() ? -1 : now + secs;
  list_in_t0_ = -10;
}

void HomeView::show_list(double now) {
  if (!list_held(now)) return;
  list_hold_until_ = -1;
  list_in_t0_ = now;
}

double HomeView::list_in(double now) const {
  if (list_held(now)) return 0;
  // (a wait that simply ran out: the list comes in from where the wait ended)
  const double t0 = list_in_t0_ > 0 ? list_in_t0_ : list_hold_until_;
  return t0 > 0 ? ease_out(std::clamp((now - t0) / 0.2, 0.0, 1.0)) : 1.0;
}

void HomeView::layout() {
  const int W = w / s, H = h / s;
  // the composition keeps its proportions on wide or huge windows: it never spreads past
  // 700 px of art (756 = 700 + 8%: the home holds its own beside the player's full-window picture)
  const int CW = std::min(W, 756), ox = (W - CW) / 2;
  const int split = (int)std::lround(CW * 0.42);
  double scale = std::clamp(std::min(CW, H * 16 / 9) / 180.0 * 1.08, 1.34, 3.6);
  const double fscale = std::round(scale * 3) / 3.0;
  // two blocks, the eyes with "open a file" under them and the recent files, laid out on the
  // composition's own proportions, then centred in the window as one group: the same air left
  // and right of the group (from the eyes or the line under them, whichever is wider, to the end
  // of the list), each block's middle on the window's middle
  const int iw = 11 + 6 + text_w(tr(str::home_open_file), 1);
  const double cx0 = ox + split * 0.56;
  const double half = std::max(11 * fscale, iw / 2.0);   // the eyes reach 8 + 3 cells of `scale`
  // the list's pictures: 48 x 27 (16:9) where the column has room, 32 x 18 where it is
  // narrower, none in a narrow window (the list keeps its small icons). With pictures, the list
  // starts a little further left: what they take from the names, the column gives back.
  // (the thresholds keep at least 70 px of title beside the picture and "24 min left")
  // (the column's left edge is the same with pictures or without: switching "mostrar imagens"
  // changes only what is in it. It is the pictures' edge wherever the column has room for them;
  // a narrow window, where the list never has pictures, keeps the list's own)
  int x0 = ox + (int)std::lround(CW * (CW >= 400 ? 0.45 : 0.49));
  int x1 = ox + CW - (int)std::lround(CW * 0.09);
  const double left = cx0 - half, right = x1;
  const int dx = (int)std::lround(((W - right) - left) / 2.0);
  const double cx = cx0 + dx;
  x0 += dx; x1 += dx;
  // the eyes and their line: from the eyes' top (6 cells above the face's centre) to the line's
  // baseline (its text box top + 10): the face's centre 12 px above the window's middle
  const double cy = H / 2.0 - 12;
  face.layout(cx * s, cy * s, s, fscale);
  face.max_look = 3.0 * face.scale / 1.5;

  // micro-instruction button under the eyes
  int ix = (int)std::lround(cx - iw / 2.0);
  int iy = (int)std::lround(face.eye_y() / s + 9 * face.scale + 14);
  r_open_ = {(ix - 5) * s, (iy - 3) * s, (iw + 10) * s, 18 * s};
  // "configurações" right under it, the same kind of line: its icon in the same column, its own
  // width (the eyes and "open a file" stay where they were)
  r_settings_ = {(ix - 5) * s, (iy - 3 + 18 + 2) * s, (11 + 6 + text_w(tr(str::settings_title), 1) + 10) * s, 18 * s};

  // Recentes column. When the newest entry is one to go back to, it stands first, apart, under
  // "continuar" (its picture twice the list's); the list goes on under "recentes".
  // Planned for both forms (with pictures and the list of words): the column's top is where the
  // taller of the two puts it, so switching "mostrar imagens" changes only what is in it.
  struct Plan { int tw = 0, th = 0, ctw = 0, cth = 0, card_h = 0, cont_extra = 0, lead = 0, count = 0, rest = 0, seen = 0; bool cont = false; };
  auto plan = [&](bool pics) {
    Plan p;
    if (!pics) p.tw = p.th = 0;  // the list of words (the ··· menu's choice)
    else if (CW >= 456) { p.tw = 48; p.th = 27; }
    else if (CW >= 400) { p.tw = 32; p.th = 18; }
    p.cont = pics && !all_rows_.empty() && all_rows_[0].progress >= 0 && all_rows_[0].progress < 1 &&
             all_rows_[0].kind != HomeKind::Music && !(all_rows_[0].audio && all_rows_[0].kind == HomeKind::Video);
    // its picture: the list's own nearly three times over where the column has room (it is the
    // one thing on the home meant to be opened), twice where it is narrower; 16:9 always
    const int lw = x1 - x0;
    if (!p.tw) p.ctw = p.cth = 0;
    else if (lw >= 300) { p.ctw = 128; p.cth = 72; }
    else if (lw >= 230) { p.ctw = 96; p.cth = 54; }
    else { p.ctw = p.tw * 2; p.cth = p.th * 2; }
    p.card_h = p.cont && p.cth ? p.cth + 6 : kRowH;
    p.cont_extra = p.cont ? kTitleGap - 3 + p.card_h + kContGap : 0;  // its word, the card, the air before "recentes"
    // as many entries as the window holds (two lines each); the rest wait for a larger window
    p.lead = p.cont ? 1 : 0;
    const int fit = std::clamp((H - 72 - p.cont_extra) / kRowPitch, p.lead ? 0 : 1, pics ? kMaxRecent : kMaxRows);
    p.count = std::min((int)all_rows_.size(), p.lead + fit);
    p.rest = p.count - p.lead;
    const int n = std::max(1, p.rest);
    // (seen: from the capitals of a title (2 px into its line) to the last entry's rule, or
    // its second line where it has none, 2 px higher: the middle of the two)
    // (with pictures, the last one's bottom edge: 3 px lower than the rule)
    const int last = p.th >= 27 ? 30 : 27;
    const int list_seen = p.count == 0 || p.rest == 0 ? 21 + 10 : kTitleGap - 3 + (n - 1) * kRowPitch + last - 2;
    p.seen = p.cont ? (kTitleGap - 3 + p.card_h - 2) + (p.rest > 0 ? kContGap + list_seen : 0) : list_seen;
    return p;
  };
  const Plan P = plan(covers), other = plan(!covers);
  thumb_w_ = P.tw; thumb_h_ = P.th;
  cont_ = P.cont;
  cont_tw_ = P.ctw; cont_th_ = P.cth;
  const int card_h = P.card_h, cont_extra = P.cont_extra, lead = P.lead;
  rows.assign(all_rows_.begin(), all_rows_.begin() + P.count);
  if (focus_ >= (int)rows.size() && focus_ < kOpen) focus_ = -1;
  const int rest = P.rest;
  recent_title_ = !cont_ || rest > 0;
  int n = std::max(1, rest);
  int y0 = (int)std::lround(H / 2.0 - std::max(P.seen, other.seen) / 2.0 - 1);
  r_rows_.clear();
  r_cont_label_ = {};
  int yr = y0;  // "recentes"
  if (cont_) {
    r_cont_label_ = {x0 * s, y0 * s, (x1 - x0) * s, 12 * s};
    r_rows_.push_back({(x0 - 6) * s, (y0 + kTitleGap - 3) * s, (x1 - x0 + 12) * s, card_h * s});
    yr = y0 + kTitleGap - 3 + card_h + kContGap;
  }
  r_title_ = recent_title_ ? Rect{x0 * s, yr * s, (x1 - x0) * s, 12 * s} : r_cont_label_;
  // (its dots end where the entries end, level with the middle of its title)
  r_dots_ = {(x1 - 8) * s, r_title_.y, 11 * s, 10 * s};
  for (int i = lead; i < (int)rows.size(); ++i)
    r_rows_.push_back({(x0 - 6) * s, (yr + kTitleGap - 3 + (i - lead) * kRowPitch) * s, (x1 - x0 + 12) * s, kRowH * s});
  const int block = (cont_ ? cont_extra : 0) + kTitleGap + n * kRowPitch;
  r_list_ = {(x0 - 8) * s, y0 * s - 6 * s, (x1 - x0 + 16) * s, (block + 12) * s};
  layout_menu();
  layout_update();
  layout_survey();
}

std::string HomeView::menu_label(int id) const {
  if(id==kMenuFolder)return tr(str::queue_folder);
  if (id == kMenuLang) return tr(str::more_language);
  if (id == kMenuClear) return tr(str::home_clear_recents);
  if (id == kMenuRemote) return tr(str::remote_title);
  if (id == kMenuCovers) return tr(str::home_show_pictures);
  if (id == kLangOpt) return tr(str::language_auto);
  if (id > kLangOpt && id <= kLangOpt + i18n::kLangCount) return i18n::native_name((i18n::Lang)(id - kLangOpt - 1));
  return "";
}

// The menu hangs under the ··· at the end of the Recentes title, as wide as its longest word.
void HomeView::layout_menu() {
  menu_ids_.clear();
  r_menu_items_.clear();
  if (menu_lang_) {
    for (int i = 0; i <= i18n::kLangCount; ++i) menu_ids_.push_back(kLangOpt + i);
  } else {
    menu_ids_.push_back(kMenuFolder);
    menu_ids_.push_back(kMenuLang);
    // ("show pictures" moved to Settings on 15/11: the home's own choice, beside its interface)
    if (remote_available) menu_ids_.push_back(kMenuRemote);
    if (!rows.empty()) menu_ids_.push_back(kMenuClear);
  }
  int words = 0;
  for (int id : menu_ids_) words = std::max(words, text_w(menu_label(id), 1));
  // the player's menus: an icon column, the words on one column, rows of 18 px, the destructive
  // entry set apart by a hairline, the same sheet and corners
  const int lead = 23, trail = menu_lang_ ? 8 : 16;  // check / icon column, chevron
  const int rule = !menu_lang_ && !menu_ids_.empty() && menu_ids_.back() == kMenuClear ? kMenuRule : 0;
  const int x1 = r_title_.r() / s, y0 = r_title_.y / s;
  // a short window: the rows close up (to 14 px), then the menu rises until it fits
  const int n = std::max(1, (int)menu_ids_.size()), room = (int)(h / s) - 6 - (y0 + 15);
  const int row = std::clamp((room - kMenuInset - kMenuFoot - rule) / n, 14, kMenuRow);
  const int W = std::max(150, 2 + lead + words + trail + 2), H = kMenuInset + kMenuFoot + row * n + rule;
  const int top = std::max(4, std::min(y0 + 15, (int)(h / s) - 6 - H));
  r_menu_ = {(x1 + 3 - W) * s, top * s, W * s, H * s};  // (its right edge under the ···'s)
  for (size_t i = 0; i < menu_ids_.size(); ++i) {
    int extra = rule && menu_ids_[i] == kMenuClear ? rule : 0;
    r_menu_items_.push_back({r_menu_.x + 2 * s, r_menu_.y + (kMenuInset + row * (int)i + extra) * s, (W - 4) * s, row * s});
  }
}

void HomeView::open_menu(bool lang) {
  menu_ = true;
  menu_lang_ = lang;
  layout_menu();
  if (kb_) focus_ = menu_ids_.empty() ? -1 : (lang ? kLangOpt + language + 1 : menu_ids_[0]);
}

// ── the survey's invitation ─────────────────────────────────────────
// A small card in the bottom-right corner, the menus' own sheet: on its left a little scene (the
// eyes over a notepad, a pencil writing three lines), the question and its two answers on the
// right. It shows only on the home (a file opened straight away never sees it), only where it
// covers nothing, and the scene writes twice when the card comes, then rests (and writes again
// under the pointer): nothing moves on its own for long.
namespace {
constexpr int kSvPad = 10, kSvArt = 34, kSvGap = 10, kSvH = 64;  // art px
constexpr double kSvCycle = 4.0, kSvWrite = 2.8, kSvHold = 3.4;   // a page: written, held, turned
constexpr int kSvLines[3] = {14, 14, 9};
bool overlaps(const Rect& a, const Rect& b) { return a.w > 0 && b.w > 0 && a.x < b.r() && b.x < a.r() && a.y < b.b() && b.y < a.b(); }
}  // namespace

void HomeView::layout_survey() {
  r_survey_ = r_survey_go_ = r_survey_no_ = {};
  survey_fits_ = false;
  const int W = (int)(w / s), H = (int)(h / s);
  const int gw = text_w(tr(str::survey_answer), 1) + 12, nw = text_w(tr(str::survey_no), 1) + 12;
  const int tw = std::max({text_w(tr(str::survey_title), 1), text_w(tr(str::survey_body), 1), gw + nw});
  const int cw = kSvPad + kSvArt + kSvGap + tw + kSvPad;
  // the bottom-right corner; else the bottom-left (over the version's line, when there is one) or
  // the middle of the foot: the first place where it covers nothing of the home's own
  const bool line = r_update_.w > 0 || r_news_.w > 0;
  const int ylow = H - kSvH - 14, yline = ylow - (line ? 22 : 0);
  const std::pair<int, int> spots[] = {{W - cw - 14, ylow}, {14, yline}, {(W - cw) / 2, yline}};
  Rect r{};
  for (auto [x, y] : spots) {
    if (x < 4 || y < 34) continue;
    const Rect c{x * s, y * s, cw * s, kSvH * s};
    // never over anything of the home's own (a short or narrow window simply goes without it)
    const Rect room = c.grow((int)(6 * s));
    bool clear = true;
    for (const Rect& o : {r_list_, r_open_, r_settings_, face.bounds(), r_title_, r_update_, r_update_skip_, r_news_, r_news_close_})
      if (overlaps(room, o)) { clear = false; break; }
    if (clear) { r = c; break; }
  }
  if (r.w <= 0) return;
  const int x = (int)std::lround(r.x / s), y = (int)std::lround(r.y / s);
  r_survey_ = r;
  const int tx = x + kSvPad + kSvArt + kSvGap;
  r_survey_go_ = {(tx - 6) * s, (y + 41) * s, gw * s, 16 * s};
  r_survey_no_ = {(tx - 6 + gw) * s, (y + 41) * s, nw * s, 16 * s};
  survey_fits_ = true;
}

int HomeView::survey_step(double now) const {
  if (survey_t0_ < 0) return -1;
  const double e = now - survey_t0_;
  if (e < 0 || e >= 2 * kSvCycle) return -1;
  return (int)(e * 8);
}

void HomeView::draw_survey(Canvas& c, double now, double a) const {
  const Palette& p = P();
  const int S = (int)s;
  const int rise = (int)std::lround((1 - a) * 4) * S;
  const Rect r{r_survey_.x, r_survey_.y + rise, r_survey_.w, r_survey_.h};
  draw_panel(c, r.x, r.y, r.w / s, r.h / s, s, {2, 1}, p.bg, a, p.edge, 0.95 * a);
  // the scene, on the art grid: the eyes, the notepad, the pencil
  const int bx = r.x + kSvPad * S, by = r.y + ((kSvH - kSvArt) / 2) * S;
  auto cell = [&](int ax, int ay, int aw, int ah, Rgb col, double al = 1.0) { c.fill(bx + ax * S, by + ay * S, aw * S, ah * S, col, al * a); };
  const int step = survey_step(now);
  const bool writing = step >= 0;
  const double u = writing ? std::fmod(step / 8.0, kSvCycle) : kSvWrite;  // resting: the page written
  // the page: written up to here (the turn of a page clears it)
  int done = 0, total = 0;
  for (int n : kSvLines) total += n;
  if (u < kSvWrite) done = (int)(u / kSvWrite * total); else if (u < kSvHold) done = total; else done = 0;
  // (Cue's own eyes, square, ten apart, over a notepad half their width again)
  cell(7, 15, 20, 17, p.pixel, 0.95);
  cell(7, 15, 20, 1, p.dim, 0.9);  // its binding
  int left = done, tipx = 24, tipy = 19;
  for (int i = 0; i < 3; ++i) {
    const int ly = 19 + 4 * i, n = std::min(left, kSvLines[i]);
    if (n > 0) { cell(10, ly, n, 1, p.bg); tipx = 10 + n; tipy = ly; }
    left -= n;
  }
  if (u >= kSvHold) { tipx = 10; tipy = 19; }
  // the pencil: its point on the line being written, leaning up and right (a wobble as it writes)
  const int wob = writing && u < kSvWrite && (step & 1) ? -1 : 0;
  for (int k = 1; k <= 6; ++k) cell(tipx + k, tipy - k + wob, 1, 1, k >= 5 ? p.faint : p.dim);
  // the eyes: on the pencil while it writes, up at the person under the pointer; a blink now and then
  double dx = writing ? std::clamp((tipx - 17) / 5.0, -1.0, 1.0) : 0.0, dy = writing ? 1 : 0;
  if (survey_hot_) dy = -1;
  const bool blink = std::fmod(now, 4.6) < 0.12 || (writing && u >= kSvHold && u < kSvHold + 0.12);
  const int ex = (int)std::lround(dx), ey = (int)std::lround(dy);
  for (int side : {6, 22}) {
    if (blink) cell(side + ex, 6 + ey, 6, 1, p.pixel);
    else cell(side + ex, 3 + ey, 6, 6, p.pixel);
  }
  // the words and the two answers
  const int tx = r.x + (kSvPad + kSvArt + kSvGap) * S;
  text(c, tr(str::survey_title), tx, r.y + 9 * S, s, p.fg, a);
  text(c, tr(str::survey_body), tx, r.y + 23 * S, s, p.dim, a);
  const double hg = hov(kSurveyGo), hn = hov(kSurveyNo);
  Rect go = r_survey_go_, no = r_survey_no_;
  go.y += rise; no.y += rise;
  if (hg > 0) c.cornered(go, s, p.hover, hg * a);
  if (hn > 0) c.cornered(no, s, p.hover, hn * a);
  text(c, tr(str::survey_answer), go.x + 6 * S, go.y + 3 * S, s, mix(p.fg, p.pixel, hg), a);
  text(c, tr(str::survey_no), no.x + 6 * S, no.y + 3 * S, s, mix(p.dim, p.fg, hn), a);
}

void HomeView::layout_update() {
  r_update_ = r_update_skip_ = r_news_ = r_news_close_ = {};
  const int H = h / s;
  int x = 10;
  if (!update_version.empty()) {
    std::string label = i18n::format(str::home_update_offer, {{"version", update_version}});
    int tw = 11 + 6 + text_w(label, 1);
    r_update_ = {x * s, (H - 24) * s, (tw + 10) * s, 16 * s};
    r_update_skip_ = {r_update_.r() + 2 * s, r_update_.y, 14 * s, 16 * s};
    x += tw + 10 + 2 + 14 + 10;
  }
  if (news) {
    int tw = 11 + 6 + text_w(tr(str::home_news), 1);
    r_news_ = {x * s, (H - 24) * s, (tw + 10) * s, 16 * s};
    r_news_close_ = {r_news_.r() + 2 * s, r_news_.y, 14 * s, 16 * s};
  }
}

int HomeView::hit(int x, int y) const {
  if (menu_ && r_menu_.contains(x, y)) {
    for (size_t i = 0; i < r_menu_items_.size(); ++i)
      if (r_menu_items_[i].contains(x, y)) return menu_ids_[i];
    return kMenuPad;
  }
  if (survey_shown() && survey_fits_) {
    if (r_survey_go_.contains(x, y)) return kSurveyGo;
    if (r_survey_no_.contains(x, y)) return kSurveyNo;
    if (r_survey_.contains(x, y)) return kSurveyPad;
  }
  if (!update_version.empty() && !update_busy) {
    if (r_update_.contains(x, y)) return kUpdate;
    if (r_update_skip_.contains(x, y)) return kUpdateSkip;
  }
  if (news) {
    if (r_news_.contains(x, y)) return kNews;
    if (r_news_close_.contains(x, y)) return kNewsClose;
  }
  if (r_dots_.contains(x, y)) return kDots;
  for (int i = 0; i < (int)r_rows_.size(); ++i)
    if (r_rows_[i].contains(x, y)) return i;
  if (r_open_.contains(x, y) && engine_problem.empty()) return kOpen;
  if (r_settings_.contains(x, y) && engine_problem.empty() && !drag_) return kSettings;
  return -1;
}

void HomeView::set_hover_row(int i, double now) {
  hover_ = i;
  update_face_target(now);
}

void HomeView::update_face_target(double now) {
  if (egg_ != 0) return;
  if (leave_t0_ > 0) return;
  if (!ghosts_.empty() && !drag_) {
    // the eyes follow the entries out, one after another
    const int j = std::clamp((int)((now - clear_t0_) / kClearStagger), 0, (int)ghosts_.size() - 1);
    const Rect& r = ghosts_[j].r;
    face.set_mode(Face::Mode::Recent, now);
    face.look_at(r.x + r.w * 0.2, r.cy(), 0.9);
    return;
  }
  if (drag_) {
    face.set_mode(Face::Mode::Drag, now);
    face.look_at(mx_, my_);
    // the closer the file gets to the face, the more it wants it
    double d = std::hypot(mx_ - face.cx, my_ - face.cy) / std::max(1.0, w * 0.42);
    face.excite = std::clamp(1.0 - d, 0.0, 1.0);
    face.excite *= face.excite * (3 - 2 * face.excite);  // smoothstep
    return;
  }
  int a = active();
  if (a >= 0 && a < (int)r_rows_.size()) {
    face.set_mode(Face::Mode::Recent, now);
    // looks a few px to the right, following the row a little
    face.look_at(r_rows_[a].x + r_rows_[a].w * 0.2, r_rows_[a].cy(), 0.9);
  } else if (tx_ > -100000 && now - track_moved_ < 6.0 &&
             std::hypot(tx_ - face.cx, ty_ - face.eye_y()) < std::max(w, h) * 0.55) {
    // follows the cursor discreetly, and only when it comes near
    face.set_mode(Face::Mode::Follow, now);
    double d = std::hypot(tx_ - face.cx, ty_ - face.eye_y()) / (double)std::max(1, w);
    face.look_at(tx_, ty_, std::clamp(0.45 + d * 0.5, 0.45, 0.7));
  } else {
    face.set_mode(Face::Mode::Idle, now);
  }
}

bool HomeView::is_background(int x, int y) const {
  if (menu_) return false;  // a click outside the open menu closes it (it is not a window drag)
  if (hit(x, y) != -1 || chrome.area(w).contains(x, y)) return false;
  if (face.bounds().contains(x, y)) return false;  // the eyes take clicks (and keep a secret)
  return true;
}

Rect HomeView::face_reach() const {
  // in cells of the face's scale (physical px = cells * scale * s), from the eyes' line: the eyes
  // sit 8 cells either side of the centre and look up to max_look (spring overshoot included),
  // each at most 1.5 blocks of 3 cells wide; a thought rises up to 16.5 cells right and 12 up; the
  // mouth opens below, under 16 cells; the hop lifts all of it a little. A generous margin
  // around that, so a rounding or an overshoot never draws past it.
  const double k = face.scale * s;
  const double look = (face.max_look * 1.5 + 2) * s;
  const int left = (int)std::floor(face.cx - 18 * k - look - 4 * s);
  const int right = (int)std::ceil(face.cx + 24 * k + look + 4 * s);
  const int top = (int)std::floor(face.eye_y() - 18 * k - look - 4 * s);
  const int bottom = (int)std::ceil(face.eye_y() + 24 * k + look + 4 * s);
  const int x0 = std::max(0, left), y0 = std::max(0, top), x1 = std::min(w, right), y1 = std::min(h, bottom);
  return x1 > x0 && y1 > y0 ? Rect{x0, y0, x1 - x0, y1 - y0} : Rect{};
}

uint64_t HomeView::eyes_only_key(double now) const {
  // anything else moving, or drawn from the clock rather than from state: a whole frame
  if (egg_ || !ghosts_.empty() || leave_t0_ > 0 || now - enter_t0_ < 0.45 || thumbs_fading(now) || clear_t0_ > now - 5 ||
      list_moving(now) || list_in(now) < 1 || iris_opening(now) || list_cover_.v > 0 || list_cover_.target > 0)
    return 0;
  if (survey || survey_vis_.v > 0 || survey_vis_.target > 0) return 0;  // (its scene keeps its own time)
  if (menu_fade_.v != menu_fade_.target || chrome.vis.v != chrome.vis.target) return 0;
  uint64_t d = 1469598103934665603ull;
  auto mixin = [&](uint64_t v) { d ^= v; d *= 1099511628211ull; };
  auto num = [&](double v) { uint64_t b = 0; std::memcpy(&b, &v, sizeof b); mixin(b); };
  auto str = [&](const std::string& t) { mixin(t.size()); for (unsigned char ch : t) mixin(ch); };
  auto rect = [&](const Rect& r) { mixin((uint64_t)(uint32_t)r.x << 32 | (uint32_t)r.y); mixin((uint64_t)(uint32_t)r.w << 32 | (uint32_t)r.h); };
  mixin((uint64_t)w); mixin((uint64_t)h); num(s);
  num(face.cx); num(face.cy); num(face.scale);  // (the eyes' place: their corner of the window)
  mixin(rows.size()); mixin(all_rows_.size());
  for (const HomeRow& r : rows) {
    str(r.name); str(r.right); num(r.progress); str(r.sub); mixin(r.audio); mixin((uint64_t)r.kind); str(r.seed);
    mixin((uint64_t)(uintptr_t)r.thumb.get()); num(r.thumb_at);
  }
  str(engine_problem); str(notice); mixin(!notice.empty() && now < notice_until);
  mixin((uint64_t)(int64_t)language); mixin((uint64_t)eggs_seen);
  str(update_version); num(update_progress); mixin(update_busy); mixin(news); mixin(remote_available);
  mixin(news && !reduce_motion() && std::fmod(now, 3.2) < 0.18);  // the spark's twinkle
  mixin(chrome.maximized); mixin(chrome.pinned); num(chrome.vis.v); num(chrome.s); mixin((uint64_t)(int64_t)chrome.hot);
  for (double v : chrome.hov) num(v);
  mixin((uint64_t)(int64_t)hover_); mixin((uint64_t)(int64_t)focus_); mixin(kb_); mixin(menu_); mixin(menu_lang_); mixin(drag_);
  mixin((uint64_t)(int64_t)mx_); mixin((uint64_t)(int64_t)my_);
  for (int id : menu_ids_) mixin((uint64_t)(int64_t)id);
  for (double v : hov_) num(v);
  num(menu_fade_.v); num(menu_fade_.target);
  mixin((uint64_t)(int64_t)title_from_y_);
  mixin(settings_lit);
  for (const Rect& r : {r_open_, r_settings_, r_title_, r_dots_, r_menu_, r_list_, r_update_, r_update_skip_, r_news_, r_news_close_, r_cont_label_}) rect(r);
  mixin(cont_); mixin(recent_title_); mixin(covers);
  for (const Rect& r : r_rows_) rect(r);
  mixin((uint64_t)(int64_t)thumb_w_); mixin((uint64_t)(int64_t)thumb_h_);
  // what the drawing reads from outside the home: the words' language, the palette, reduced motion
  str(i18n::tag(i18n::language()));
  const Palette& p = P();
  for (Rgb c : {p.bg, p.fg, p.dim, p.faint, p.hover, p.pixel, p.edge}) mixin((uint64_t)c.r << 16 | (uint64_t)c.g << 8 | c.b);
  mixin(reduce_motion());
  return d ? d : 1;
}

double HomeView::hov(int id) const {
  int k = id >= 100 ? (int)rows.size() + (id - 100) : id;
  return (k >= 0 && k < (int)hov_.size()) ? ease_out(hov_[k]) : 0.0;
}

HomeAction HomeView::mouse_move(int x, int y, double now) {
  mx_ = x;
  my_ = y;
  kb_ = false;
  chrome.vis.target = 1;
  chrome.hot = chrome.hit(x, y, w);
  int nh = hit(x, y);
  if (nh != hover_) { hover_ = nh; update_face_target(now); }
  if (egg_ == 4) face.look_at(x, y, 1.0);
  return {};
}

void HomeView::mouse_leave(double now) {
  hover_ = -1;
  chrome.vis.target = 0;
  chrome.hot = -1;
  mx_ = my_ = -1;
  update_face_target(now);
}

HomeAction HomeView::mouse_down(int x, int y, double now) {
  switch (chrome.hit(x, y, w)) {
    case WindowChrome::Min: return {HomeAct::WinMin, -1};
    case WindowChrome::Max: return {HomeAct::WinMax, -1};
    case WindowChrome::Close: return {HomeAct::WinClose, -1};
    case WindowChrome::Pin: return {HomeAct::WinPin, -1};
    default: break;
  }
  int hh = hit(x, y);
  bool in_menu = hh == kMenuFolder || hh == kMenuCovers || hh == kMenuPad || hh == kMenuClear || hh == kMenuLang || hh == kMenuRemote || (hh >= kLangOpt && hh <= kLangOpt + i18n::kLangCount);
  if (menu_ && !in_menu && hh != kDots) { menu_ = false; return {}; }
  if (hh == kMenuPad) return {};
  if(hh==kMenuFolder) {menu_=false;return {HomeAct::OpenFolder,-1};}
  if (hh == kMenuClear) { menu_ = false; return {HomeAct::ClearRecents, -1}; }
  if (hh == kMenuCovers) { menu_ = false; return {HomeAct::Covers, covers ? 0 : 1}; }
  if (hh == kMenuRemote) { menu_ = false; return {HomeAct::Remote, -1}; }
  if (hh == kMenuLang) { open_menu(true); return {}; }
  if (hh >= kLangOpt && hh <= kLangOpt + i18n::kLangCount) return {HomeAct::Language, hh - kLangOpt - 1};
  if (hh == kDots) { if (menu_) menu_ = false; else open_menu(false); return {}; }
  if (hh == kOpen) return {HomeAct::OpenDialog, -1};
  if (hh == kSettings) return {HomeAct::Settings, -1};
  if (hh == kUpdate) return {HomeAct::Update, -1};
  if (hh == kUpdateSkip) return {HomeAct::SkipUpdate, -1};
  if (hh == kSurveyGo) return {HomeAct::Survey, -1};
  if (hh == kSurveyNo) return {HomeAct::SurveyClose, -1};
  if (hh == kSurveyPad) return {};
  if (hh == kNews) return {HomeAct::News, -1};
  if (hh == kNewsClose) return {HomeAct::NewsClose, -1};
  if (hh >= 0 && hh < (int)rows.size()) return {HomeAct::OpenRecent, hh};
  if (face.bounds().contains(x, y)) {
    clicks_.push_back(now);
    clicks_.erase(std::remove_if(clicks_.begin(), clicks_.end(), [&](double t) { return now - t > 2.2; }), clicks_.end());
    if (clicks_.size() >= 7 && egg_ == 0) {
      clicks_.clear();
      start_egg(now);
      return {HomeAct::EggSeen, egg_variant_};
    }
  }
  return {};
}

HomeAction HomeView::key(const std::string& k, bool shift, double now) {
  std::vector<int> order;
  if (engine_problem.empty()) { order.push_back(kOpen); order.push_back(kSettings); }
  for (int i = 0; i < (int)rows.size(); ++i) order.push_back(i);
  order.push_back(kDots);
  if (menu_) for (int id : menu_ids_) order.push_back(id);
  if (!update_version.empty() && !update_busy) order.push_back(kUpdate);
  if (news) { order.push_back(kNews); order.push_back(kNewsClose); }
  if (survey_shown() && survey_fits_) { order.push_back(kSurveyGo); order.push_back(kSurveyNo); }
  auto idx = [&] { auto it = std::find(order.begin(), order.end(), focus_); return it == order.end() ? -1 : (int)(it - order.begin()); };
  if (k == "tab") {
    kb_ = true;
    int i = idx();
    int n = (int)order.size();
    if (n == 0) return {};
    i = i < 0 ? (shift ? n - 1 : 0) : (i + (shift ? n - 1 : 1)) % n;
    focus_ = order[i];
  } else if (k == "down" || k == "up") {
    kb_ = true;
    if (rows.empty()) return {};
    if (focus_ < 0 || focus_ >= (int)rows.size()) focus_ = k == "down" ? 0 : (int)rows.size() - 1;
    else focus_ = std::clamp(focus_ + (k == "down" ? 1 : -1), 0, (int)rows.size() - 1);
  } else if (k == "enter" || (k == "space" && kb_)) {
    if (focus_ == kOpen) return {HomeAct::OpenDialog, -1};
    if (focus_ == kSettings) return {HomeAct::Settings, -1};
    if (focus_ == kUpdate) return {HomeAct::Update, -1};
    if (focus_ == kNews) return {HomeAct::News, -1};
    if (focus_ == kNewsClose) { focus_ = -1; return {HomeAct::NewsClose, -1}; }
    if (focus_ == kSurveyGo) { focus_ = -1; return {HomeAct::Survey, -1}; }
    if (focus_ == kSurveyNo) { focus_ = -1; return {HomeAct::SurveyClose, -1}; }
    if (focus_ == kDots) { if (menu_) menu_ = false; else open_menu(false); }
    else if(focus_==kMenuFolder) {menu_=false;focus_=-1;return {HomeAct::OpenFolder,-1};}
    else if (focus_ == kMenuClear) { menu_ = false; focus_ = -1; return {HomeAct::ClearRecents, -1}; }
    else if (focus_ == kMenuCovers) { menu_ = false; focus_ = -1; return {HomeAct::Covers, covers ? 0 : 1}; }
    else if (focus_ == kMenuRemote) { menu_ = false; focus_ = -1; return {HomeAct::Remote, -1}; }
    else if (focus_ == kMenuLang) open_menu(true);
    else if (focus_ >= kLangOpt && focus_ <= kLangOpt + i18n::kLangCount) return {HomeAct::Language, focus_ - kLangOpt - 1};
    else if (focus_ >= 0 && focus_ < (int)rows.size()) return {HomeAct::OpenRecent, focus_};
  } else if (k == "esc") {
    if (menu_ && menu_lang_) { open_menu(false); if (kb_) focus_ = kMenuLang; }  // one level back
    else { menu_ = false; kb_ = false; focus_ = -1; }
  }
  update_face_target(now);
  return {};
}

bool HomeView::track(int x, int y, double now) {
  if (x == tx_ && y == ty_) {
    // still for a while: back to contemplating on its own
    if (face.mode == Face::Mode::Follow && now - track_moved_ > 6.0) { face.set_mode(Face::Mode::Idle, now); return true; }
    return false;
  }
  tx_ = x; ty_ = y;
  track_moved_ = now;
  update_face_target(now);
  return true;
}

void HomeView::drag_over(int x, int y, double now) {
  mx_ = x;
  my_ = y;
  if (!drag_) face.notice(now);  // Omni: it notices, crouches and hops
  drag_ = true;
  update_face_target(now);
}
void HomeView::drag_leave(double now) {
  drag_ = false;
  update_face_target(now);
}

void HomeView::leave(double now, bool swallow) {
  if (leave_t0_ > 0) return;
  swallow_ = swallow;
  leave_t0_ = now;
  menu_ = false;
  if (swallow) { face.set_mode(Face::Mode::Idle, now); face.look(0, 0); face.gulp(now); }
  else { face.closed = true; }  // lights off, the film starts
}

void HomeView::cancel_leave(double now) {
  if (leave_t0_ < 0) return;
  leave_t0_ = -1;
  gone_ = false;
  enter(now);
}

void HomeView::enter(double now, bool keep_face) {
  leave_t0_ = -1;
  gone_ = false;
  enter_t0_ = now;
  keep_face_ = keep_face;
  face.closed = false;
  face.happy = false;
  face.mouth.snap(0);
  face.notice(now);
  update_face_target(now);
}

void HomeView::enter_iris(double now) {
  enter(now, false);
  enter_t0_ = now - 1;  // no fade of its own: the iris is the entrance
  iris_t0_ = now;
}

void HomeView::fades(double now, double& content, double& fa) const {
  content = fa = 1;
  if (gone_) { content = fa = 0; return; }
  if (leave_t0_ > 0) {
    double t = now - leave_t0_;
    if (swallow_) {
      content = 1 - ease_out(std::clamp(t / 0.16, 0.0, 1.0));
      fa = 1 - ease_in_out(std::clamp((t - 0.2) / 0.16, 0.0, 1.0));  // eats it, then goes
    } else {
      content = 1 - ease_out(std::clamp(t / 0.16, 0.0, 1.0));
      fa = 1 - ease_out(std::clamp((t - 0.04) / 0.16, 0.0, 1.0));
    }
  } else if (now - enter_t0_ < 0.4) {
    double t = now - enter_t0_;
    fa = keep_face_ ? 1.0 : ease_out(std::clamp(t / 0.22, 0.0, 1.0));
    content = ease_out(std::clamp((t - 0.06) / 0.26, 0.0, 1.0));
  }
}

// ── easter egg ──────────────────────────────────────────────────────
void HomeView::start_egg(double now, int forced) {
  int v = forced;
  if (!v) {
    std::vector<int> unseen;
    for (int i = 1; i <= 3; ++i)
      if (!(eggs_seen & seen_bit(i))) unseen.push_back(i);
    if (!unseen.empty()) v = unseen[rng_.pick((int)unseen.size())];
    else v = rng_.uni() < 0.16 ? 4 : 1 + rng_.pick(3);
  }
  egg_ = v;
  egg_variant_ = v;
  eggs_seen |= seen_bit(v);
  egg_t0_ = now;
  flakes_.clear();
  shake_t_ = -1;
  if (v == 1) {
    // first snow: flakes drift down the whole window; six of them land on the eyes
    const double fall = std::max(160.0 * s, h / 3.2);  // px per second
    for (int i = 0; i < 54; ++i) {
      Flake f{};
      f.x = rng_.range(0.0, 1.0) * w;
      f.v = fall * rng_.range(0.7, 1.25);
      f.at = rng_.range(-0.6, 3.2);
      f.y0 = -8 * s;
      f.sway = rng_.range(2.0, 7.0) * s;
      f.ph = rng_.range(0.0, 6.28);
      f.size = rng_.uni() < 0.35 ? 2 : 1;
      f.eye = 0;
      flakes_.push_back(f);
    }
    const double a = std::lround(face.scale) * s, top = face.eye_y() - 3 * a;
    const double lands[6] = {1.35, 1.75, 2.3, 1.55, 2.05, 2.6};
    for (int i = 0; i < 6; ++i) {
      Flake f{};
      f.eye = i < 3 ? -1 : 1;
      f.land = lands[i];
      f.x = face.cx + f.eye * 8 * a + rng_.range(-1.5, 1.5) * a;
      f.v = fall * 0.8;
      f.y0 = -8 * s;
      f.at = f.land - (top - f.y0) / f.v;
      f.sway = 0;
      f.size = 2;
      flakes_.push_back(f);
    }
  }
  if (v == 4) { face.set_mode(Face::Mode::Stare, now); face.max_look = 3.0 * face.scale; }
}

bool HomeView::tick(double now, double dt) {
  bool moving = iris_opening(now) || (iris_t0_ > 0 && now - iris_t0_ < kIrisOpen + 0.05);
  if (egg_) {
    double t = now - egg_t0_;
    moving = true;
    switch (egg_) {
      case 1: {  // first snow: look up, a surprise as it lands, a shake, and a smile
        face.look(0, t > 0.5 && t < 3.6 ? -face.max_look : 0);
        face.wide = t > 1.35 && t < 1.75;
        if (t >= 3.6 && shake_t_ < 0) { shake_t_ = t; face.notice(now); }
        if (t >= 3.6 && t < 4.0) face.look(std::fmod(t, 0.14) < 0.07 ? face.max_look : -face.max_look, 0);
        face.happy = t >= 4.1 && t < 5.2;
        break;
      }
      case 2: {  // a Santa hat lands on the head: eyes shut at the bump, then a look up and a smile
        face.closed = t >= 0.62 && t < 0.82;
        if (t >= 0.62 && shake_t_ < 0) { shake_t_ = t; face.notice(now); }
        face.look(t > 3.5 ? face.max_look : 0, t > 0.9 && t < 1.5 ? -face.max_look : 0);
        face.happy = t >= 1.5 && t < 3.5;
        break;
      }
      case 3: {  // the lights: the eyes chase the running light, gasp at the blinking, then glow
        int n = lights_n();
        if (t >= 0.6 && t < 2.7) {
          double head = (t - 0.6) / 2.0 * (n + 2);
          double x = (std::clamp(head, 0.0, n - 1.0) + 0.5) * w / n;
          face.look_at(x, lights_y(x, t), 1.0);
        } else face.look(0, t < 4.6 ? -face.max_look * 0.6 : 0);
        face.wide = t >= 2.7 && t < 3.9;
        face.happy = t >= 3.9 && t < 4.7;
        break;
      }
      case 4:
        if (mx_ >= 0) face.look_at(mx_, my_, 1.0);
        break;
    }
    if (t >= kEggDur[egg_]) {
      egg_ = 0;
      face.wide = false;
      face.closed = false;
      face.happy = false;
      face.lx_off = face.ly_off = face.rx_off = face.ry_off = 0;
      face.max_look = 3.0 * face.scale / 1.5;
      face.mode = Face::Mode::Recent;  // force a refresh
      face.set_mode(Face::Mode::Idle, now);
      update_face_target(now);
    }
  }
  if (!notice.empty() && now > notice_until) { notice.clear(); moving = true; }
  menu_fade_.target = menu_ ? 1.0 : 0.0;
  moving |= menu_fade_.step(dt);
  {
    // the survey's card: in when wanted and there is room, gone as the home leaves (its place is
    // looked at again while it is wanted: the version's line may come or go meanwhile)
    if (survey) {
      const Rect before = r_survey_;
      layout_survey();
      if (before.x != r_survey_.x || before.y != r_survey_.y) moving = true;
    }
    survey_vis_.target = survey && survey_fits_ && leave_t0_ <= 0 && !drag_ ? 1.0 : 0.0;
    moving |= survey_vis_.step(dt);
    if (survey_vis_.target > 0 && survey_t0_ < 0) survey_t0_ = now + 0.5;  // it writes once it is in
    if (survey_vis_.target <= 0 && survey_vis_.v <= 0) survey_t0_ = -1;
    const bool hot = survey_shown() && r_survey_.contains(mx_, my_);
    if (hot != survey_hot_) {
      survey_hot_ = hot;
      moving = true;
      if (hot && survey_step(now) < 0) survey_t0_ = now;  // resting: it writes again under the pointer
    }
    // (resting, a blink now and then is a frame of its own)
    const int st = !survey_shown() ? -1 : survey_step(now) >= 0 ? survey_step(now) : std::fmod(now, 4.6) < 0.12 ? -3 : -1;
    if (st != survey_drawn_step_) { survey_drawn_step_ = st; moving = true; }
  }
  moving |= face.tick(now, dt);
  moving |= chrome.tick(dt);
  if (leave_t0_ > 0 && now - leave_t0_ < leave_dur() + 0.05) moving = true;
  if (now - enter_t0_ < 0.45) moving = true;
  if (thumbs_fading(now) || list_moving(now) || list_in(now) < 1) moving = true;
  if (list_cover_.step(dt)) moving = true;
  if (!ghosts_.empty()) {
    moving = true;
    const double last_out = clear_t0_ + kClearStagger * (ghosts_.size() - 1) + kClearRow;
    if (!blinked_ && now >= last_out) { blinked_ = true; face.next_blink = now; }  // all gone: a blink
    if (now >= clear_end()) { ghosts_.clear(); update_face_target(now); }
    else update_face_target(now);
  }
  size_t n = rows.size() + kHitIds;
  if (hov_.size() != n) hov_.assign(n, 0.0);
  for (size_t k = 0; k < n; ++k) {
    int id = k < rows.size() ? (int)k : 100 + (int)(k - rows.size());
    double v = hover_step(hov_[k], active() == id, dt);
    if (v != hov_[k]) { hov_[k] = v; moving = true; }
  }
  return moving;
}

double HomeView::next_event(double now) const {
  if (egg_ || iris_opening(now) || chrome.vis.v != chrome.vis.target || leave_t0_ > 0 || now - enter_t0_ < 0.45 || menu_fade_.v != menu_fade_.target || menu_fade_.v != (menu_ ? 1.0 : 0.0) || thumbs_fading(now) || !ghosts_.empty() ||
      list_moving(now) || list_in(now) < 1 || list_cover_.v != list_cover_.target) return 0;
  double t = face.next_event(now);
  if (!notice.empty()) t = std::min(t, notice_until - now);
  if (survey_shown()) {
    // its scene: the next of its eight frames a second while it writes; resting, the next blink
    if (survey_vis_.v != survey_vis_.target) return 0;
    if (survey_step(now) >= 0 || (survey_t0_ > now)) t = std::min(t, std::max(0.0, 0.125 - std::fmod(std::max(0.0, now - survey_t0_), 0.125)));
    else {
      const double ph = std::fmod(now, 4.6);
      t = std::min(t, ph < 0.12 ? 0.12 - ph : 4.6 - ph);
    }
  }
  return std::max(0.0, t);
}

// ── Christmas ───────────────────────────────────────────────────────
// Everything on the face's own grid: one cell is one art pixel of the eyes (scale x s).
void HomeView::draw_snow(Canvas& c, double t) const {
  const Palette& p = P();
  const double a = std::lround(face.scale) * s;
  int landed[2] = {0, 0};
  for (const Flake& f : flakes_) {
    double ft = t - f.at;
    if (ft < 0) continue;
    if (f.eye && t >= f.land) { if (!(shake_t_ >= 0 && t >= shake_t_)) ++landed[f.eye > 0]; continue; }
    double y = f.y0 + f.v * ft;
    if (y > h + 8 * s) continue;
    double x = f.x + f.sway * std::sin(ft * 1.7 + f.ph);
    int sz = f.size == 2 ? (int)a : (int)std::lround(s);  // big flakes are one cell of the eyes' grid
    double fade = t > kEggDur[1] - 0.6 ? (kEggDur[1] - t) / 0.6 : 1.0;
    c.fill((int)std::lround(x / s) * s, (int)std::lround(y / s) * s, sz, sz, f.size == 2 ? p.fg : p.dim, std::max(0.0, fade));
  }
  // the snow on top of each eye: a cap that grows with every flake, until the shake
  const double block = std::lround(3 * face.scale) * s;
  for (int e = 0; e < 2; ++e) {
    int n = landed[e];
    if (!n) continue;
    double ex = face.cx + (e ? 1 : -1) * 8 * face.scale * s;
    double top = face.eye_y() - block - face.hop.x * s * face.scale * 0.6;
    int cw = (int)std::lround(2 * block + 2 * a);
    for (int r = 0; r < std::min(n, 3); ++r) {
      int rw = cw - (int)std::lround(r * 2 * a);
      c.fill((int)std::lround(ex - rw / 2.0), (int)std::lround(top - (r + 1) * a), rw, (int)a, p.pixel);
    }
  }
  // the shake throws it off in little puffs
  if (shake_t_ >= 0 && t >= shake_t_ && t < shake_t_ + 0.8) {
    double k = (t - shake_t_) / 0.8;
    for (int e = 0; e < 2; ++e) {
      double ex = face.cx + (e ? 1 : -1) * 8 * face.scale * s, ey = face.eye_y() - block - 2 * a;
      for (int i = 0; i < 7; ++i) {
        double ang = -2.6 + i * 0.36 + (e ? 0.3 : -0.3);
        double d = k * (14 + i % 3 * 5) * a;
        double x = ex + std::cos(ang) * d, y = ey + std::sin(ang) * d + k * k * 18 * a;
        c.fill((int)std::lround(x / s) * s, (int)std::lround(y / s) * s, (int)a, (int)a, p.pixel, 1 - k);
      }
    }
  }
}

void HomeView::draw_hat(Canvas& c, double t) const {
  // a Santa hat in pixels: red cone leaning right, a white band, and a pompom that swings
  static const char* hat[] = {
      "............rr......",
      "...........RRr......",
      "..........RRRr......",
      ".........RRRRr......",
      "........RRRRRr......",
      ".......RRRRRRr......",
      "......RRRRRRRRr.....",
      ".....RRRRRRRRRRr....",
      "....RRRRRRRRRRRRr...",
      "..WWWWWWWWWWWWWWWW..",
      "..WwWWWwWWWWwWWWwW..",
      "..wwwwwwwwwwwwwwww..",
  };
  const Rgb red{214, 52, 58}, dark{150, 30, 44}, white{246, 244, 238}, shade{196, 196, 204};
  const double a = std::lround(face.scale) * s;
  const double block = std::lround(3 * face.scale) * s;
  const double rest = face.eye_y() - block - 4 * a - 12 * a - face.hop.x * s * face.scale * 0.6;
  double y = rest, x = face.cx - 10 * a, alpha = 1;
  if (t < 0.62) { double k = t / 0.62; y = lerp(-14 * a, rest, k * k); }                     // falls
  else if (t < 1.1) y = rest - std::sin((t - 0.62) / 0.48 * 3.14159) * 3 * a;                 // a little bounce
  if (t > 3.5) { double k = std::min(1.0, (t - 3.5) / 0.9); x += k * k * 34 * a; y -= std::sin(k * 3.14159) * 6 * a + k * 6 * a; alpha = 1 - k; }
  if (alpha <= 0) return;
  int X = (int)std::lround(x / s) * s, Y = (int)std::lround(y / s) * s, A = (int)a;
  for (int r = 0; r < 12; ++r)
    for (int col = 0; col < 20; ++col) {
      char ch = hat[r][col];
      if (ch == '.') continue;
      Rgb k = ch == 'R' ? red : ch == 'r' ? dark : ch == 'W' ? white : shade;
      c.fill(X + col * A, Y + r * A, A, A, k, alpha);
    }
  // the pompom swings at the tip and slowly settles
  double sw = t > 0.62 ? 2.4 * std::sin((t - 0.62) * 9) * std::exp(-(t - 0.62) * 1.2) : 0;
  int px = X + (int)std::lround((13 + sw) * a), py = Y - 3 * A;
  static const char* pom = ".WW.WWWwWWWw.ww.";
  for (int i = 0; i < 16; ++i)
    if (pom[i] != '.') c.fill(px + (i % 4) * A, py + (i / 4) * A, A, A, pom[i] == 'W' ? white : shade, alpha);
  // three small stars twinkle while it sits there
  if (t > 1.3 && t < 3.5) {
    const double sx[3] = {-14, 22, 26}, sy[3] = {2, -2, 9};
    for (int i = 0; i < 3; ++i) {
      double k = 0.5 + 0.5 * std::sin((t - 1.3) * 6 + i * 2.1);
      int cx = X + (int)std::lround((10 + sx[i]) * a), cy = Y + (int)std::lround(sy[i] * a);
      c.fill(cx, cy - A, A, 3 * A, P().pixel, k * alpha);
      c.fill(cx - A, cy, 3 * A, A, P().pixel, k * alpha);
    }
  }
}

int HomeView::lights_n() const { return std::clamp((int)std::lround(w / (26.0 * std::lround(face.scale) * s)), 8, 18); }

// the wire's height at x: it sags between the window's corners, drops in and goes back up
double HomeView::lights_y(double x, double t) const {
  double u = (x - w / 2.0) / (w / 2.0);
  double base = h * 0.12 + h * 0.07 * (1 - u * u);
  double in = std::min(1.0, t / 0.5), out = t > 4.7 ? std::min(1.0, (t - 4.7) / 0.6) : 0;
  return base - (1 - in * in * (3 - 2 * in)) * h * 0.3 - out * out * h * 0.3;
}

void HomeView::draw_lights(Canvas& c, double t) const {
  const double a = std::lround(face.scale) * s;
  const int A = (int)a, n = lights_n();
  const Rgb colors[4] = {{232, 70, 70}, {80, 200, 112}, {250, 198, 76}, {92, 150, 250}};
  // the wire, one cell at a time
  for (double x = 0; x < w; x += a) c.fill((int)x, (int)std::lround(lights_y(x, t) / a) * A, A, A, P().edge);
  for (int i = 0; i < n; ++i) {
    double x = (i + 0.5) * w / n, y = lights_y(x, t);
    // how lit: a light running along, then all of them blinking, then all on
    double lit = 0.12;
    if (t >= 0.6 && t < 2.7) { double d = (t - 0.6) / 2.0 * (n + 2) - i; if (d >= 0 && d < 3) lit = 1 - d / 3 * 0.75; }
    else if (t >= 2.7 && t < 3.9) lit = std::fmod(t - 2.7, 0.4) < 0.2 ? 1 : 0.12;
    else if (t >= 3.9) lit = 1;
    Rgb col = colors[i % 4];
    int bx = (int)std::lround(x / a) * A - A, by = (int)std::lround(y / a) * A + A;
    if (lit > 0.5) {  // a soft dithered glow around a lit bulb
      for (int gy = -2; gy <= 5; ++gy)
        for (int gx = -2; gx <= 4; ++gx)
          if (((gx + gy) & 1) == 0) c.fill(bx + gx * A, by + gy * A, A, A, col, 0.16 * lit);
    }
    c.fill(bx + A / 2, by - A, 2 * A, A, P().dim);  // the socket
    c.fill(bx, by, 3 * A, 3 * A, mix(Rgb{40, 40, 40}, col, lit));
    c.fill(bx + A, by + 3 * A, A, A, mix(Rgb{40, 40, 40}, col, lit));
    if (lit > 0.5) c.fill(bx, by, A, A, mix(col, Rgb{255, 255, 255}, 0.6), lit);  // the shine
  }
}

// ── the list's pictures ─────────────────────────────────────────────
namespace {
constexpr double kThumbFade = 0.24;  // a picture arriving over its stand-in
constexpr double kThumbRest = 0.26;  // how much darker a picture rests (full light under the pointer)
}  // namespace

Rect HomeView::thumb_rect(int i) const {
  if (!thumb_w_ || i < 0 || i >= (int)r_rows_.size()) return {};
  const Rect& r = r_rows_[i];
  if (cont_ && i == 0) return {r.x + 3 * s, r.y + 3 * s, cont_tw_ * s, cont_th_ * s};  // the one to continue: twice the size
  // 3 px inside the entry's rounded box; the small one level with the two lines of words
  return {r.x + 3 * s, r.y + (thumb_h_ >= 27 ? 3.0 : 5.5) * s, thumb_w_ * s, thumb_h_ * s};
}

const Canvas& HomeView::stand_in_for(int i) const {
  if ((int)stand_ins_.size() < (int)rows.size()) stand_ins_.resize(rows.size());
  Tile& t = stand_ins_[i];
  const HomeRow& row = rows[i];
  const Rect tr = thumb_rect(i);
  const std::string& seed = row.seed.empty() ? row.name : row.seed;
  const HomeKind kind = row.audio && row.kind == HomeKind::Video ? HomeKind::Music : row.kind;
  if (t.w != tr.w || t.h != tr.h || t.seed != seed || t.kind != kind) {
    thumb_art::make_stand_in(kind, row.audio, seed, tr.w, tr.h, s, std::max(1, (int)s), t.img);
    t.w = tr.w; t.h = tr.h; t.seed = seed; t.kind = kind; t.src = nullptr;
  }
  return t.img;
}

const Canvas& HomeView::tile_for(int i) const {
  const HomeRow& row = rows[i];
  if (!row.thumb) return stand_in_for(i);
  if ((int)tiles_.size() < (int)rows.size()) tiles_.resize(rows.size());
  Tile& t = tiles_[i];
  const Rect tr = thumb_rect(i);
  if (t.src != row.thumb.get() || t.w != tr.w || t.h != tr.h) {
    thumb_art::make_tile(*row.thumb, tr.w, tr.h, std::max(1, (int)s), t.img);
    t.src = row.thumb.get(); t.w = tr.w; t.h = tr.h;
  }
  return t.img;
}

// A song's record: made for its size, its turn and its light, again only when one changes
// (the turn in 1/64ths, the light in 1/32nds: a hover redraws it a few times, then never).
const Canvas& HomeView::disc_for(int i, double turn, double light) const {
  if ((int)discs_.size() < (int)rows.size()) discs_.resize(rows.size());
  Disc& d = discs_[i];
  const HomeRow& row = rows[i];
  const int side = (int)std::lround(thumb_rect(i).h * 0.92);
  const uint32_t label = row.thumb ? thumb_art::accent(*row.thumb) : thumb_art::stand_in_accent(row.seed.empty() ? row.name : row.seed);
  const int tq = (int)std::lround(turn / 6.283185307 * 64), lq = (int)std::lround(light * 32);
  if (d.side != side || d.label != label || d.turn != tq || d.light != lq || d.src != row.thumb.get()) {
    thumb_art::make_disc(label, side, s, tq / 64.0 * 6.283185307, lq / 32.0, d.img);
    d.side = side; d.label = label; d.turn = tq; d.light = lq; d.src = row.thumb.get();
  }
  return d.img;
}

bool HomeView::thumbs_fading(double now) const {
  for (const HomeRow& r : rows)
    if (r.thumb && now - r.thumb_at < kThumbFade + 0.05) return true;
  return false;
}

void HomeView::draw_thumb(Canvas& c, int i, double ha, double now) const {
  const Palette& p = P();
  const HomeRow& row = rows[i];
  const Rect tr = thumb_rect(i);
  if (tr.w <= 0) return;
  const int cut = std::max(1, (int)s), A = std::max(1, (int)std::lround(s));
  const double fade = row.thumb ? std::clamp((now - row.thumb_at) / kThumbFade, 0.0, 1.0) : 0.0;
  const double rest = kThumbRest * (1 - ha);  // at rest a touch darker, so the words lead
  // audio is square, on the left; a song's record peeks out from behind it, and under the
  // pointer slides out a few pixels more, turning as it goes (its reflections and its label's mark)
  const bool sq = thumb_art::square(row.kind, row.audio);
  const Rect pic = sq ? Rect{tr.x, tr.y, tr.h, tr.h} : tr;
  if (sq && (row.kind == HomeKind::Music || (row.audio && row.kind == HomeKind::Video))) {
    const Canvas& disc = disc_for(i, ha * 2.4, 1 - rest);
    const int slide = (int)std::lround(ha * 4) * A;
    const int dx = tr.x + (int)std::lround(tr.h * 1.14 / A) * A - disc.w / 2 + slide;
    const int dy = tr.y + (tr.h - disc.h) / 2;
    c.blit(disc, dx, dy);
    // the cover's shadow on it: one pixel of art along the cover's edge, as tall as the record there
    const double R = disc.w / 2.0, off = pic.r() - (dx + R);
    if (std::fabs(off) < R) {
      const int half = (int)std::sqrt(R * R - off * off);
      c.fill_pixels(pic.r(), dy + (int)R - half, A, 2 * half, {0, 0, 0}, 0.55);
    }
  }
  if (!row.thumb || fade < 1) c.blit(stand_in_for(i), tr.x, tr.y);
  if (row.thumb) c.blit(tile_for(i), tr.x, tr.y, ease_out(fade));
  if (rest > 0) c.cornered(pic, cut, p.bg, rest);
  // how far: a bar along the picture's foot, inside its cut corners
  const bool big = tr.h >= 27 * s;
  if (row.progress >= 0) {
    const int bh = std::max(1, (int)std::lround((big ? 2 : 1) * s));
    const int bx = pic.x + cut, bw = pic.w - 2 * cut, by = pic.b() - bh;
    const int pw = std::max(cut, (int)std::lround(bw * std::clamp(row.progress, 0.0, 1.0)));
    c.fill_pixels(bx, by, bw, bh, {0, 0, 0}, 0.62);
    c.fill_pixels(bx, by, pw, bh, mix(p.fg, p.pixel, ha), 0.8 + 0.2 * ha);
  }
  // under the pointer, a small play sign rises into the middle of the picture
  if (big && ha > 0.01) {
    const int rise = (int)std::lround((1 - ha) * 2) * A;
    const int bw = 13 * A, bh2 = 11 * A;
    const Rect b{pic.cx() - bw / 2, pic.cy() - bh2 / 2 + rise, bw, bh2};
    c.cornered(b, A, p.bg, 0.72 * ha);
    static const char* kPlay = "#..." "##.." "###." "####" "###." "##.." "#...";
    c.cells(kPlay, 4, 7, b.x + 5 * A, b.y + 2 * A, A, p.pixel, ha);
  }
}

// The entry to continue: its light (the picture's own colours, a soft glow around it), its
// picture twice the list's with the bar of how far, and beside it the title and its line.
const Canvas* HomeView::cont_light(Rect& at) const {
  if (!cont_ || rows.empty() || !rows[0].thumb) return nullptr;
  const HomeRow& row = rows[0];
  const Rect tr = thumb_rect(0);
  const Rect pic = thumb_art::square(row.kind, row.audio) ? Rect{tr.x, tr.y, tr.h, tr.h} : tr;
  const int reach = std::max(6, (int)std::lround(30 * s));
  if (cont_light_src_ != row.thumb.get() || cont_light_w_ != pic.w || cont_light_h_ != pic.h || cont_reach_ != reach) {
    Rgb corner[4];
    edge_colours(row.thumb->px.data(), row.thumb->w, row.thumb->h, corner);
    glow_light(cont_light_, pic.w + 2 * reach, pic.h + 2 * reach, {reach, reach, pic.w, pic.h}, reach, corner, 0.48);
    cont_light_src_ = row.thumb.get();
    cont_light_w_ = pic.w;
    cont_light_h_ = pic.h;
    cont_reach_ = reach;
  }
  at = {pic.x - reach, pic.y - reach, pic.w + 2 * reach, pic.h + 2 * reach};
  return &cont_light_;
}

// Under everything else of the list: the hover box and the light (the words of "continuar" and
// "recentes" are drawn over the light, never tinted by it).
void HomeView::draw_cont_back(Canvas& c, double now) const {
  if (!cont_ || rows.empty() || !thumb_w_) return;
  const Palette& p = P();
  const double ha = hov(0);
  if (ha > 0) c.cornered(r_rows_[0], s, p.hover, ha);
  Rect at;
  if (const Canvas* L = cont_light(at)) {
    // it comes in with its picture, and lights a little more under the pointer
    const double fade = std::clamp((now - rows[0].thumb_at) / kThumbFade, 0.0, 1.0);
    const double k = ease_out(fade) * (0.75 + 0.25 * ha);
    if (k > 0.003) c.blit(*L, at.x, at.y, k);
  }
}

void HomeView::draw_cont(Canvas& c, double ha, double now) const {
  const Palette& p = P();
  const Rect& r = r_rows_[0];
  const HomeRow& row = rows[0];
  (void)r;
  draw_thumb(c, 0, ha, now);  // (its box and its light are drawn first, under the titles: draw_cont_back)
  const Rect tr = thumb_rect(0);
  const Rect pic = thumb_art::square(row.kind, row.audio) ? Rect{tr.x, tr.y, tr.h, tr.h} : tr;
  const int nx = pic.r() + 10 * s, x1 = r.r() - 6 * s;
  // the title and its line, together in the middle of the picture's height
  const int mid = pic.y + pic.h / 2;
  const int ty = row.sub.empty() ? mid - 6 * s : mid - 13 * s, sy = mid + 1 * s;
  text(c, fit_end(row.name, x1 - nx, s), nx, ty, s, mix(mix(p.dim, p.fg, 0.6), p.fg, ha));
  if (!row.sub.empty()) text(c, fit_end(row.sub, x1 - nx, s), nx, sy, s, mix(p.dim, p.fg, ha * 0.5));
}

// ── render ──────────────────────────────────────────────────────────
void HomeView::render(Canvas& c, double now) {
  const Palette& p = P();
  if (c.w != w || c.h != h) c.resize(w, h);
  c.clear(p.bg);
  double et = egg_ ? now - egg_t0_ : 0;

  if (egg_ == 3) draw_lights(c, et);

  // micro-instruction / state line
  {
    std::string label = tr(str::home_open_file);
    Rgb col = p.dim;
    bool show_icon = true;
    if (!engine_problem.empty()) { label = engine_problem; show_icon = false; }
    else if (drag_) { label = tr(str::home_drop_here); col = p.fg; show_icon = false; }
    else if (!notice.empty()) { label = notice; show_icon = false; }
    double ha = show_icon ? hov(kOpen) : 0.0;
    int tw = text_w(label, s) + (show_icon ? 17 * s : 0);
    int x = (int)face.cx - tw / 2;
    int y = r_open_.y + 3 * s;
    // (the box: 5 px of art each side of the ink; the words end with a pixel of advance, not ink: 16/11)
    if (ha > 0) c.cornered({x - 5 * s, r_open_.y, tw + 9 * s, r_open_.h}, s, p.hover, ha);
    if (show_icon) {
      action_icon(c, "cue:open-file", {x, y, 11 * s, 12 * s}, s, mix(p.dim, p.pixel, ha));
      x += 17 * s;
    }
    text(c, label, x, y, s, mix(col, p.fg, ha));
    // "configurações": the same line, under it (not while a file is dragged in or the engine failed)
    if (show_icon) {
      const double hs = settings_lit ? 1.0 : hov(kSettings);
      const std::string sl = tr(str::settings_title);
      const int sx = (int)face.cx - tw / 2, sy = r_settings_.y + 3 * s;
      // (its left edge with the line above; the gear sits 2 px into the icons' column, so 7 px of art
      // each side of the ink: 16/11)
      if (hs > 0) c.cornered({(int)(sx - 5 * s), r_settings_.y, (int)(text_w(sl, s) + 28 * s), r_settings_.h}, s, p.hover, hs);
      // (the gear, 7 px, in the middle of the folder's 11 px column)
      c.bitmap(bmp("gear"), (int)(sx + 2 * s), (int)(sy + 2 * s), s, mix(p.dim, p.pixel, hs));
      text(c, sl, (int)(sx + 17 * s), sy, s, mix(p.dim, p.fg, hs));
    }
  }

  // Recentes (clearing: it waits for the entries to leave, then glides to its new place)
  double title_k = 1, empty_k = 1;
  clear_fx(now, title_k, empty_k);
  const int tdy = ghosts_.empty() ? 0 : (int)std::lround((title_from_y_ - r_title_.y) * (1 - title_k) / s) * (int)s;
  draw_cont_back(c, now);
  if (cont_) text(c, tr(str::home_continue), r_cont_label_.x, r_cont_label_.y, s, p.fg);
  if (recent_title_) text(c, tr(str::home_recents), r_title_.x, r_title_.y + tdy, s, p.fg);
  bool show_dots = menu_ || r_list_.contains(mx_, my_) || r_dots_.contains(mx_, my_) || (kb_ && focus_ == kDots);
  if (show_dots) {
    double on = menu_ ? 1.0 : hov(kDots);
    if (on > 0) c.cornered({r_dots_.x, r_dots_.y + tdy, r_dots_.w, r_dots_.h}, s, p.hover, on);
    c.bitmap(bmp("dots"), r_dots_.x + 3 * s, r_dots_.y + tdy + 5 * s, s, mix(p.dim, p.fg, on));
  }
  if (rows.empty() && empty_k > 0) {
    const int rise = (int)std::lround((1 - empty_k) * 3) * (int)s;
    text(c, tr(str::home_recents_empty), r_title_.x, r_title_.y + tdy + 21 * s + rise, s, p.faint, empty_k);
  }
  for (size_t i = 0; i < ghosts_.size(); ++i) {
    const double k = std::clamp((now - clear_t0_ - kClearStagger * i) / kClearRow, 0.0, 1.0);
    if (k >= 1) continue;
    const int drift = (int)std::lround(k * k * 8) * (int)s;  // it eases away, a few pixels right
    c.blit(ghosts_[i].img, ghosts_[i].r.x + drift, ghosts_[i].r.y, 1 - ease_out(k));
  }
  for (int i = 0; i < (int)rows.size(); ++i) {
    const Rect& r = r_rows_[i];
    double ha = hov(i);
    bool on = ha > 0.5;
    if (cont_ && i == 0 && thumb_w_) { draw_cont(c, ha, now); continue; }  // (its box: draw_cont_back)
    if (ha > 0) c.cornered(r, s, p.hover, ha);
    const HomeRow& row = rows[i];
    const bool resumable = row.progress >= 0;
    int x0 = r.x + 6 * s, x1 = r.r() - 6 * s;
    int rw = text_w(row.right, s);
    if (thumb_w_) {
      // the picture on the left, the title and what is left on its first line, the quiet line
      // under them; the progress lies along the picture's foot
      draw_thumb(c, i, ha, now);
      const Rect tr = thumb_rect(i);
      const bool big = thumb_h_ >= 27;
      const int nx = tr.r() + (big ? 8 : 7) * s;
      const int sy = big ? r.y + 17 * s : r.y + 15 * s;
      // (no quiet line: the title in the middle of the entry, with what is left beside it)
      const int ty = row.sub.empty() ? (big ? r.y + 11 * s : r.y + 9 * s) : big ? r.y + 5 * s : r.y + 3 * s;
      text(c, row.right, x1 - rw + s, ty, s, resumable ? mix(p.dim, p.fg, ha) : p.faint);
      std::string nm = fit_end(row.name, x1 - rw - 10 * s - nx, s);
      text(c, nm, nx, ty, s, mix(mix(p.dim, p.fg, 0.35), p.fg, ha));
      if (!row.sub.empty()) text(c, fit_end(row.sub, x1 - nx, s), nx, sy, s, mix(p.faint, p.dim, ha));
      continue;
    }
    // a narrow window: a living entry without its picture: the title and what is left on one
    // line, a quiet line under it (the episode, or the kind of file), and the progress as a rule
    // across the whole entry
    // (the two lines and the rule sit in the middle of the entry: 5 px above the capitals, 5 below the rule)
    int ty = row.sub.empty() ? r.y + 9 * s : r.y + 3 * s, sy = r.y + 15 * s;  // (no quiet line: lower, beside the rule's gap)
    c.bitmap(bmp(row.audio ? "k-audio" : "k-video"), x0, ty + 2 * s, s, mix(p.dim, p.fg, ha));
    // (a line's width counts the space after its last letter: the time ends where the rule ends)
    text(c, row.right, x1 - rw + s, ty, s, resumable ? mix(p.dim, p.fg, ha) : p.faint);
    int nx = x0 + 13 * s;
    std::string nm = fit_end(row.name, x1 - rw - 10 * s - nx, s);
    text(c, nm, nx, ty, s, mix(mix(p.dim, p.fg, 0.35), p.fg, ha));
    if (!row.sub.empty()) text(c, fit_end(row.sub, x1 - nx, s), nx, sy, s, mix(p.faint, p.dim, ha));
    if (resumable) {
      int lw = x1 - nx, ly = r.y + 27 * s;
      int pw = std::max(s, (int)std::lround(lw * std::clamp(row.progress, 0.0, 1.0) / s) * s);
      c.fill(nx, ly, lw, s, p.edge);
      c.fill(nx, ly, pw, s, on ? p.fg : p.dim);
    }
  }
  // at the start, the list (its title and entries) waits for its pictures, then fades in with them;
  // under the Settings panel it steps out the same way (see cover_list)
  if (const double li = std::min(list_in(now), 1 - list_cover_.v); li < 1) c.fill(list_reach(), p.bg, 1 - li);
  draw_swap(c, now);  // (the list changing look: the old one leaving, the new one rising in)
  if (menu_fade_.v > 0) {
    // it unfolds from the ···: a short fade and a 3 px drop, and leaves the same way
    double k = ease_out(menu_fade_.v);
    int dy = (int)std::lround((1 - k) * 3) * (int)s;
    draw_panel(c, r_menu_.x, r_menu_.y - dy, r_menu_.w / s, r_menu_.h / s, s, kMenuProfile, p.bg, k, p.edge, 0.95 * k);
    for (size_t i = 0; i < menu_ids_.size(); ++i) {
      int id = menu_ids_[i];
      Rect ir = r_menu_items_[i];
      ir.y -= dy;
      double ha = menu_ ? hov(id) : 0.0;
      if (ha > 0) c.cornered(ir, s, p.hover, ha * k);
      if (id == kMenuClear && i > 0) c.fill(ir.x + 4 * s, ir.y - (kMenuRule - kMenuRule / 2) * s, ir.w - 8 * s, s, p.edge, k);  // (the player's hairline)
      bool chosen = (id >= kLangOpt && id - kLangOpt - 1 == language) || (id == kMenuCovers && covers);
      if (chosen) c.bitmap(bmp("check"), ir.x + 5 * s, ir.y + (ir.h - 5 * s) / 2 / s * s, s, p.fg, k);
      const char* ic = id == kMenuFolder ? "cue:open-file" : id == kMenuLang ? "globe" : id == kMenuRemote ? "phone" : id == kMenuClear ? "cross" : nullptr;
      if (ic) action_icon(c, ic, {ir.x + 3 * s, ir.y, 12 * s, ir.h}, s, mix(p.dim, p.fg, ha), k);
      int ty = ir.y + (ir.h - 12 * s) / 2 + s;
      text(c, menu_label(id), ir.x + 23 * s, ty, s, chosen ? p.fg : mix(p.dim, p.fg, ha), k);
      if (id == kMenuLang) c.bitmap(bmp("chev-right5"), ir.r() - (int)std::lround(5 * s) - (int)std::lround(3 * s), ir.y + (ir.h - 5 * s) / 2 / s * s, s, mix(p.dim, p.fg, ha), k);
    }
  }

  // a newer version: one quiet line, bottom left; a progress whisper while it downloads
  if (!update_version.empty()) {
    layout_update();
    double ha = update_busy ? 0.0 : hov(kUpdate);
    std::string label;
    if (update_progress >= 0)
      label = i18n::format(str::home_update_downloading, {{"version", update_version}, {"percent", std::to_string((int)std::lround(update_progress * 100))}});
    else if (update_busy) label = tr(str::home_update_preparing);
    else label = i18n::format(str::home_update_offer, {{"version", update_version}});
    Rect r = r_update_;
    if (ha > 0) c.cornered({r.x, r.y, 11 * s + 6 * s + text_w(label, s) + 10 * s, r.h}, s, p.hover, ha);
    int x = r.x + 5 * s, y = r.y + 3 * s;
    // a small pixel arrow pointing down: "comes in"
    static const char* kDown = "..#.." "..#.." "#.#.#" ".###." "..#..";
    c.cells(kDown, 5, 5, x + 3 * s, y + 3 * s, s, mix(update_busy ? p.dim : p.fg, p.pixel, ha));
    text(c, label, x + 17 * s, y, s, mix(update_busy ? p.dim : p.fg, p.pixel, ha));
    if (update_progress >= 0) {
      int lw = text_w(label, s), lx = x + 17 * s, ly = y + 12 * s;
      c.fill(lx, ly, lw, s, p.edge);
      c.fill(lx, ly, (int)std::lround(lw * std::clamp(update_progress, 0.0, 1.0) / s) * s, s, p.fg);
    }
    if (!update_busy) {
      double hs = hov(kUpdateSkip);
      bool show = hs > 0 || r_update_.grow(4 * s).contains(mx_, my_) || r_update_skip_.contains(mx_, my_);
      if (show) {
        if (hs > 0) c.cornered(r_update_skip_, s, p.hover, hs);
        c.bitmap(bmp("x-small"), r_update_skip_.x + 5 * s, r_update_skip_.y + 6 * s, s, mix(p.faint, p.fg, hs));
      }
    }
  }

  // what's new: the same quiet line, a little spark that twinkles now and then
  if (news) {
    layout_update();
    double ha = hov(kNews);
    std::string label = tr(str::home_news);
    Rect r = r_news_;
    if (ha > 0) c.cornered(r, s, p.hover, ha);
    int x = r.x + 5 * s, y = r.y + 3 * s;
    bool twinkle = !reduce_motion() && std::fmod(now, 3.2) < 0.18;
    static const char* kSpark = "..#.." "..#.." "##.##" "..#.." "..#..";
    static const char* kSpark2 = "....." "..#.." ".###." "..#.." ".....";
    c.cells(twinkle ? kSpark2 : kSpark, 5, 5, x + 3 * s, y + 3 * s, s, mix(p.fg, p.pixel, std::max(ha, twinkle ? 1.0 : 0.0)));
    text(c, label, x + 17 * s, y, s, mix(p.fg, p.pixel, ha));
    double hs = hov(kNewsClose);
    bool show = hs > 0 || r_news_.grow(4 * s).contains(mx_, my_) || r_news_close_.contains(mx_, my_) || (kb_ && (focus_ == kNews || focus_ == kNewsClose));
    if (show) {
      if (hs > 0) c.cornered(r_news_close_, s, p.hover, hs);
      c.bitmap(bmp("x-small"), r_news_close_.x + 5 * s, r_news_close_.y + 6 * s, s, mix(p.faint, p.fg, hs));
    }
  }

  // the survey's invitation, last of the home's own (under the window's buttons and the fades)
  if (survey_vis_.v > 0 && survey_fits_) draw_survey(c, now, ease_out(survey_vis_.v));

  chrome.draw(c, w);
  double ca, fa;
  fades(now, ca, fa);
  if (ca < 1) c.fill(0, 0, w, h, p.bg, 1 - ca);
  face.now_hint = now;
  if (fa > 0) face.render(c, fa);
  if (egg_ == 1) draw_snow(c, et);
  if (egg_ == 2) draw_hat(c, et);
  if (iris_opening(now)) iris_cover(c, iris_ease((now - iris_t0_) / kIrisOpen), std::max(1, (int)std::lround(3 * s)));
}
