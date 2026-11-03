#include "home.h"

#include <algorithm>
#include <cmath>

#include "../core/i18n.h"

namespace {
constexpr int kRowPitch = 30;  // art px: a title, its quiet line and a progress rule
constexpr int kRowH = 28;
constexpr int kMaxRows = 6;
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

void HomeView::layout() {
  const int W = w / s, H = h / s;
  // the composition keeps its proportions on wide or huge windows: it never spreads past
  // 700 px of art and stays centred (ultrawide, 1440p, 4K at 100% look like a poster, not a sparse grid)
  // (756 = 700 + 8%: the home holds its own beside the player's full-window picture)
  const int CW = std::min(W, 756), ox = (W - CW) / 2;
  const int split = (int)std::lround(CW * 0.42);
  double scale = std::clamp(std::min(CW, H * 16 / 9) / 180.0 * 1.08, 1.34, 3.6);
  face.layout((ox + split * 0.56) * s, (H * 0.47) * s, s, std::round(scale * 3) / 3.0);
  face.max_look = 3.0 * face.scale / 1.5;

  // micro-instruction button under the eyes
  int iw = 11 + 6 + text_w(tr(str::home_open_file), 1);
  int ix = (int)std::lround(ox + split * 0.56 - iw / 2.0);
  int iy = (int)std::lround(face.eye_y() / s + 9 * face.scale + 14);
  r_open_ = {(ix - 5) * s, (iy - 3) * s, (iw + 10) * s, 18 * s};

  // Recentes column
  int x0 = ox + (int)std::lround(CW * 0.49);
  int x1 = ox + CW - (int)std::lround(CW * 0.09);
  // as many entries as the window holds (two lines each); the rest wait for a larger window
  const int fit = std::clamp((H - 64) / kRowPitch, 1, kMaxRows);
  rows.assign(all_rows_.begin(), all_rows_.begin() + std::min((int)all_rows_.size(), fit));
  if (focus_ >= (int)rows.size() && focus_ < kOpen) focus_ = -1;
  int n = std::max(1, (int)rows.size());
  int block = 12 + 9 + n * kRowPitch;
  int y0 = (int)std::lround(H * 0.47 - block / 2.0 - 3);
  r_title_ = {x0 * s, y0 * s, (x1 - x0) * s, 12 * s};
  r_dots_ = {(x1 - 9) * s, (y0 + 1) * s, 11 * s, 10 * s};
  r_rows_.clear();
  for (int i = 0; i < (int)rows.size(); ++i)
    r_rows_.push_back({(x0 - 5) * s, (y0 + 19 + i * kRowPitch) * s, (x1 - x0 + 10) * s, kRowH * s});
  r_list_ = {(x0 - 8) * s, y0 * s - 6 * s, (x1 - x0 + 16) * s, (block + 12) * s};
  layout_menu();
  layout_update();
}

std::string HomeView::menu_label(int id) const {
  if(id==kMenuFolder)return tr(str::queue_folder);
  if (id == kMenuLang) return tr(str::more_language);
  if (id == kMenuClear) return tr(str::home_clear_recents);
  if (id == kMenuRemote) return tr(str::remote_title);
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
    if (remote_available) menu_ids_.push_back(kMenuRemote);
    if (!rows.empty()) menu_ids_.push_back(kMenuClear);
  }
  int words = 0;
  for (int id : menu_ids_) words = std::max(words, text_w(menu_label(id), 1));
  // like the player's menus: an icon column, 14 px rows, and the destructive entry set apart
  const int lead = menu_lang_ ? 14 : 19, trail = menu_lang_ ? 6 : 14;  // check / icon column, chevron
  const int rule = !menu_lang_ && !menu_ids_.empty() && menu_ids_.back() == kMenuClear ? 5 : 0;
  const int W = std::max(96, 2 + lead + words + trail + 2), H = 4 + 14 * (int)menu_ids_.size() + rule;
  const int x1 = r_title_.r() / s, y0 = r_title_.y / s;
  r_menu_ = {(x1 + 2 - W) * s, (y0 + 13) * s, W * s, H * s};
  for (size_t i = 0; i < menu_ids_.size(); ++i) {
    int extra = rule && menu_ids_[i] == kMenuClear ? rule : 0;
    r_menu_items_.push_back({r_menu_.x + 2 * s, r_menu_.y + (2 + 14 * (int)i + extra) * s, (W - 4) * s, 14 * s});
  }
}

void HomeView::open_menu(bool lang) {
  menu_ = true;
  menu_lang_ = lang;
  layout_menu();
  if (kb_) focus_ = menu_ids_.empty() ? -1 : (lang ? kLangOpt + language + 1 : menu_ids_[0]);
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
  return -1;
}

void HomeView::set_hover_row(int i, double now) {
  hover_ = i;
  update_face_target(now);
}

void HomeView::update_face_target(double now) {
  if (egg_ != 0) return;
  if (leave_t0_ > 0) return;
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
  bool in_menu = hh == kMenuFolder || hh == kMenuPad || hh == kMenuClear || hh == kMenuLang || hh == kMenuRemote || (hh >= kLangOpt && hh <= kLangOpt + i18n::kLangCount);
  if (menu_ && !in_menu && hh != kDots) { menu_ = false; return {}; }
  if (hh == kMenuPad) return {};
  if(hh==kMenuFolder) {menu_=false;return {HomeAct::OpenFolder,-1};}
  if (hh == kMenuClear) { menu_ = false; return {HomeAct::ClearRecents, -1}; }
  if (hh == kMenuRemote) { menu_ = false; return {HomeAct::Remote, -1}; }
  if (hh == kMenuLang) { open_menu(true); return {}; }
  if (hh >= kLangOpt && hh <= kLangOpt + i18n::kLangCount) return {HomeAct::Language, hh - kLangOpt - 1};
  if (hh == kDots) { if (menu_) menu_ = false; else open_menu(false); return {}; }
  if (hh == kOpen) return {HomeAct::OpenDialog, -1};
  if (hh == kUpdate) return {HomeAct::Update, -1};
  if (hh == kUpdateSkip) return {HomeAct::SkipUpdate, -1};
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
  if (engine_problem.empty()) order.push_back(kOpen);
  for (int i = 0; i < (int)rows.size(); ++i) order.push_back(i);
  order.push_back(kDots);
  if (menu_) for (int id : menu_ids_) order.push_back(id);
  if (!update_version.empty() && !update_busy) order.push_back(kUpdate);
  if (news) { order.push_back(kNews); order.push_back(kNewsClose); }
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
    if (focus_ == kUpdate) return {HomeAct::Update, -1};
    if (focus_ == kNews) return {HomeAct::News, -1};
    if (focus_ == kNewsClose) { focus_ = -1; return {HomeAct::NewsClose, -1}; }
    if (focus_ == kDots) { if (menu_) menu_ = false; else open_menu(false); }
    else if(focus_==kMenuFolder) {menu_=false;focus_=-1;return {HomeAct::OpenFolder,-1};}
    else if (focus_ == kMenuClear) { menu_ = false; focus_ = -1; return {HomeAct::ClearRecents, -1}; }
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
  enter(now);
}

void HomeView::enter(double now, bool keep_face) {
  leave_t0_ = -1;
  enter_t0_ = now;
  keep_face_ = keep_face;
  face.closed = false;
  face.happy = false;
  face.mouth.snap(0);
  face.notice(now);
  update_face_target(now);
}

void HomeView::fades(double now, double& content, double& fa) const {
  content = fa = 1;
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
  bool moving = false;
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
  moving |= face.tick(now, dt);
  moving |= chrome.tick(dt);
  if (leave_t0_ > 0 && now - leave_t0_ < leave_dur() + 0.05) moving = true;
  if (now - enter_t0_ < 0.45) moving = true;
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
  if (egg_ || chrome.vis.v != chrome.vis.target || leave_t0_ > 0 || now - enter_t0_ < 0.45 || menu_fade_.v != menu_fade_.target || menu_fade_.v != (menu_ ? 1.0 : 0.0)) return 0;
  double t = face.next_event(now);
  if (!notice.empty()) t = std::min(t, notice_until - now);
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
    if (ha > 0) c.cornered({x - 5 * s, r_open_.y, tw + 10 * s, r_open_.h}, s, p.hover, ha);
    if (show_icon) {
      action_icon(c, "cue:open-file", {x, y, 11 * s, 12 * s}, s, mix(p.dim, p.pixel, ha));
      x += 17 * s;
    }
    text(c, label, x, y, s, mix(col, p.fg, ha));
  }

  // Recentes
  text(c, tr(str::home_recents), r_title_.x, r_title_.y, s, p.fg);
  bool show_dots = menu_ || r_list_.contains(mx_, my_) || r_dots_.contains(mx_, my_) || (kb_ && focus_ == kDots);
  if (show_dots) {
    double on = menu_ ? 1.0 : hov(kDots);
    if (on > 0) c.cornered(r_dots_, s, p.hover, on);
    c.bitmap(bmp("dots"), r_dots_.x + 3 * s, r_dots_.y + 5 * s, s, mix(p.dim, p.fg, on));
  }
  if (rows.empty()) {
    text(c, tr(str::home_recents_empty), r_title_.x, r_title_.y + 21 * s, s, p.faint);
  }
  for (int i = 0; i < (int)rows.size(); ++i) {
    const Rect& r = r_rows_[i];
    double ha = hov(i);
    bool on = ha > 0.5;
    if (ha > 0) c.cornered(r, s, p.hover, ha);
    // a living entry: the title and what is left on one line, a quiet line under it (the
    // episode, or the kind of file), and the progress as a rule across the whole entry
    int x0 = r.x + 5 * s, x1 = r.r() - 5 * s;
    const HomeRow& row = rows[i];
    int ty = r.y + 3 * s, sy = r.y + 14 * s;
    c.bitmap(bmp(row.audio ? "k-audio" : "k-video"), x0, ty + 2 * s, s, mix(p.dim, p.fg, ha));
    int rw = text_w(row.right, s);
    bool resumable = row.progress >= 0;
    text(c, row.right, x1 - rw, ty, s, resumable ? mix(p.dim, p.fg, ha) : p.faint);
    int nx = x0 + 13 * s;
    std::string nm = fit_end(row.name, x1 - rw - 10 * s - nx, s);
    text(c, nm, nx, ty, s, mix(mix(p.dim, p.fg, 0.35), p.fg, ha));
    if (!row.sub.empty()) text(c, fit_end(row.sub, x1 - nx, s), nx, sy, s, mix(p.faint, p.dim, ha));
    if (resumable) {
      int lw = x1 - nx, ly = r.y + 25 * s;
      int pw = std::max(s, (int)std::lround(lw * std::clamp(row.progress, 0.0, 1.0) / s) * s);
      c.fill(nx, ly, lw, s, p.edge);
      c.fill(nx, ly, pw, s, on ? p.fg : p.dim);
    }
  }
  if (menu_fade_.v > 0) {
    // it unfolds from the ···: a short fade and a 3 px drop, and leaves the same way
    double k = ease_out(menu_fade_.v);
    int dy = (int)std::lround((1 - k) * 3) * (int)s;
    draw_panel(c, r_menu_.x, r_menu_.y - dy, r_menu_.w / s, r_menu_.h / s, s, {1}, p.sheet, k, p.edge, k);
    for (size_t i = 0; i < menu_ids_.size(); ++i) {
      int id = menu_ids_[i];
      Rect ir = r_menu_items_[i];
      ir.y -= dy;
      double ha = menu_ ? hov(id) : 0.0;
      if (ha > 0) c.cornered(ir, s, p.hover, ha * k);
      if (id == kMenuClear && i > 0) c.fill(ir.x + 2 * s, ir.y - 3 * s, ir.w - 4 * s, s, p.edge, k);
      bool chosen = id >= kLangOpt && id - kLangOpt - 1 == language;
      if (chosen) c.bitmap(bmp("check"), ir.x + 3 * s, ir.y + 4 * s, s, p.fg, k);
      const char* ic = id == kMenuFolder ? "folder" : id == kMenuLang ? "globe" : id == kMenuRemote ? "phone" : id == kMenuClear ? "cross" : nullptr;
      if (ic) action_icon(c, ic, {ir.x + 3 * s, ir.y, 12 * s, ir.h}, s, mix(p.dim, p.fg, ha), k);
      int ty = ir.y + (ir.h - 12 * s) / 2 + s;
      text(c, menu_label(id), ir.x + (menu_lang_ ? 14 : 19) * s, ty, s, chosen ? p.fg : mix(p.dim, p.fg, ha), k);
      if (id == kMenuLang) c.bitmap(bmp("chev-right5"), ir.r() - 8 * s, ir.y + (ir.h - 5 * s) / 2 / s * s, s, mix(p.dim, p.fg, ha), k);
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

  chrome.draw(c, w);
  double ca, fa;
  fades(now, ca, fa);
  if (ca < 1) c.fill(0, 0, w, h, p.bg, 1 - ca);
  face.now_hint = now;
  if (fa > 0) face.render(c, fa);
  if (egg_ == 1) draw_snow(c, et);
  if (egg_ == 2) draw_hat(c, et);
}
