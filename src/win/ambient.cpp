#include "ambient.h"
#include "platform.h"
#include "../ui/ambient_raster.h"
#include "../ui/ambient_ring.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <cstring>

static LRESULT CALLBACK glow_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_NCHITTEST) return HTTRANSPARENT;
  if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
  return DefWindowProcW(h, m, w, l);
}

bool Ambient::create(HINSTANCE inst) {
  if (failed_) return false;
  if (st_[0].wnd) return true;
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = glow_proc;
  wc.hInstance = inst;
  wc.lpszClassName = L"tinyoffice.Cue.ambient";
  RegisterClassExW(&wc);
  for (auto& s : st_) {
    s.wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"",
                            WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, inst, nullptr);
    if (!s.wnd) { destroy(); failed_ = true; return false; }
    // (15/11: shown and hidden at once, its own fade does the rest. Windows faded a hidden strip
    // out where it was: a film's halo hidden as the window glided home stayed behind it, a dark
    // block the old window's size fading over the desktop)
    BOOL off = TRUE;
    DwmSetWindowAttribute(s.wnd, DWMWA_TRANSITIONS_FORCEDISABLED, &off, sizeof off);
  }
  return true;
}

void Ambient::free_bitmaps() {
  for (auto& s : st_) {
    if (s.mem) DeleteDC(s.mem);
    if (s.bmp) DeleteObject(s.bmp);
    s.mem = nullptr;
    s.bmp = nullptr;
    s.bits = nullptr;
    s.held.clear();
    s.w = s.h = 0;
  }
}

void Ambient::free_surfaces() {
  free_bitmaps();
  for (auto& s : st_) { s.runs.clear(); s.runs.shrink_to_fit(); }
  built_ = false;
}

void Ambient::destroy() {
  free_surfaces();
  for (auto& s : st_) { if (s.wnd) DestroyWindow(s.wnd); s.wnd = nullptr; }
}

void Ambient::hide() {
  for (auto& s : st_) if (s.wnd && IsWindowVisible(s.wnd)) ShowWindow(s.wnd, SW_HIDE);
}

void Ambient::reset() {
  // A transient layered-window error must not disable the next video forever.
  if (failed_) { destroy(); failed_ = false; }
  want_ = false;
  reveal_.cut();
  next_out_ = -1;
  remap_on_ = painted_remap_on_ = false;
  last_a_ = -1;
  colors_dirty_ = true;
  since_paint_ = 1;
  hide();
}

// The visible edge of the player (without the invisible resize borders).
static RECT visible_rect(HWND owner) {
  RECT r{};
  if (FAILED(DwmGetWindowAttribute(owner, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r))) GetWindowRect(owner, &r);
  return r;
}

// Once per size: where the halo is, how strong at each pixel, and which point of the window's
// edge lights it. Everything later is a lookup.
void Ambient::build_shape(const RECT& r, int dpi) {
  free_surfaces();
  rect_ = r;
  int ww = r.right - r.left, wh = r.bottom - r.top;
  if (ww <= 0 || wh <= 0) return;
  dpi_ = dpi;
  int reach = (int)std::lround(std::min(ww, wh) * 0.14);
  M_ = std::clamp(reach, MulDiv(48, dpi, 96), MulDiv(120, dpi, 96));
  C_ = corner_tuck(dpi);
  {
    // the whole strips (top and bottom carry the corners and reach C_ px under the window, where
    // Windows 11 rounds its corners; left and right sit between them, so no pixel is lit twice)
    const int W = ww + 2 * M_, C = std::min({C_, ww / 2, wh / 2});
    const RECT full[4] = {{0, 0, W, M_ + C}, {0, M_ + wh - C, W, M_ + wh + M_}, {0, M_ + C, M_, M_ + wh - C}, {M_ + ww, M_ + C, M_ + ww + M_, M_ + wh - C}};
    for (int k = 0; k < 4; ++k) {
      st_[k].fx = full[k].left; st_[k].fy = full[k].top;
      st_[k].fw = std::max(0L, full[k].right - full[k].left); st_[k].fh = std::max(0L, full[k].bottom - full[k].top);
      st_[k].x = st_[k].y = st_[k].w = st_[k].h = 0;
    }
  }
  since_build_ = 0;
  rebuild_pending_ = false;
  const double P = 2.0 * (ww + wh);
  // a soft light: strong at the edge, fading like a lamp on a wall, ending at zero. Its alpha by
  // the squared distance, looked up: a 4K window's halo is about two million pixels, and this
  // used to cost a root and an exponential each (tens of ms, felt as a hitch when it first lit)
  constexpr int kFall = 4096;
  static uint8_t fall[kFall + 1];
  static bool fall_made = false;
  if (!fall_made) {
    for (int i = 0; i <= kFall; ++i) {
      const double qq = (double)i / kFall, u = std::sqrt(qq);
      fall[i] = (uint8_t)std::lround(std::exp(-2.8 * qq) * (1 - u) * 235);
    }
    fall_made = true;
  }
  const double inv_m2 = 1.0 / ((double)M_ * M_);
  std::vector<uint32_t> code;  // one strip at a time: (lut << 8 | a) per pixel, 0 where nothing glows
  for (int k = 0; k < 4; ++k) {
    Strip& s = st_[k];
    if (s.fw <= 0 || s.fh <= 0 || s.fw > 65535 || s.fh > 65535) continue;
    code.assign((size_t)s.fw * s.fh, 0);
    for (int ly = 0; ly < s.fh; ++ly)
      for (int lx = 0; lx < s.fw; ++lx) {
        int x = s.fx + lx, y = s.fy + ly;
        int qx = std::clamp(x, M_, M_ + ww - 1), qy = std::clamp(y, M_, M_ + wh - 1);
        const double dx = x - qx, dy = y - qy;
        double q = (dx * dx + dy * dy) * inv_m2;  // (the distance squared: no root per pixel)
        if (q >= 1) continue;
        // Under the window: only the wedges its rounded corners leave open are lit, at full
        // edge strength and with their corner's colour; everything else is covered.
        double corner_p = -1;
        if (x == qx && y == qy) {
          bool left = x < M_ + C_, right = x >= M_ + ww - C_, top = y < M_ + C_, bottom = y >= M_ + wh - C_;
          if (!((left || right) && (top || bottom))) continue;
          double cx = left ? M_ + C_ : M_ + ww - C_, cy = top ? M_ + C_ : M_ + wh - C_;
          double ox = x + 0.5 - cx, oy = y + 0.5 - cy;
          if (ox * ox + oy * oy <= (double)C_ * C_) continue;  // inside the arc: the window's own pixels
          corner_p = top ? (left ? 0.0 : ww - 1.0) : (right ? ww + wh - 1.0 : 2.0 * ww + wh - 1.0);
          q = 0;
        }
        const int a = fall[(int)(q * kFall)];
        if (a <= 1) continue;
        // which point of the window's edge this pixel faces, clockwise from the top-left corner
        double p;
        int ex = qx - M_, ey = qy - M_;
        if (corner_p >= 0) p = corner_p;
        else if (y < M_) p = ex;
        else if (x >= M_ + ww) p = ww + ey;
        else if (y >= M_ + wh) p = ww + wh + (ww - 1 - ex);
        else p = 2.0 * ww + wh + (wh - 1 - ey);
        int li = std::clamp((int)(p / P * kLut), 0, kLut - 1);
        code[(size_t)ly * s.fw + lx] = (uint32_t)li << 8 | (uint32_t)a;
      }
    // (23/11) the same pixels as runs of one colour: along a top or bottom strip the falloff is
    // the same across a row and the edge's colour changes every ~16 px; down a side strip, the
    // same down a column. A paint looks a colour up once per run and fills the run, instead of
    // once per pixel (the 4K opening window: 1.6 million lookups a paint, now 160 thousand), and the
    // shape takes about a tenth of the memory.
    const bool down = k >= 2;  // (the side strips: down their columns)
    s.runs.clear();
    const int outer = down ? s.fw : s.fh, inner = down ? s.fh : s.fw;
    for (int o = 0; o < outer; ++o) {
      uint32_t prev = 0;
      for (int i = 0; i < inner; ++i) {
        const size_t at = down ? (size_t)i * s.fw + o : (size_t)o * s.fw + i;
        const uint32_t c = code[at];
        if (c && c == prev && s.runs.back().n < 65535) { ++s.runs.back().n; continue; }
        if (c) s.runs.push_back({(uint32_t)at, 1, (uint16_t)(c >> 8), (uint8_t)(c & 0xFF), (uint8_t)down});
        prev = c;
      }
    }
    s.runs.shrink_to_fit();
    s.held.clear();
  }
  built_ = true;
  colors_dirty_ = true;
  RECT cut[4];
  layout_cut(r, cut);
  cut_surfaces(cut);
}

// The parts of the whole strips that fall on a screen (any of them), in halo coordinates
// (origin = the player's top-left minus the reach).
void Ambient::layout_cut(const RECT& r, RECT out[4]) const {
  RECT screens{GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0};
  screens.right = screens.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  screens.bottom = screens.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (screens.right <= screens.left || screens.bottom <= screens.top) {  // (never: the monitor, then)
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &mi);
    screens = mi.rcMonitor;
  }
  OffsetRect(&screens, -(r.left - M_), -(r.top - M_));  // into halo coordinates
  for (int k = 0; k < 4; ++k) {
    const Strip& s = st_[k];
    RECT full{s.fx, s.fy, s.fx + s.fw, s.fy + s.fh};
    if (s.runs.empty() || !IntersectRect(&out[k], &full, &screens)) SetRectEmpty(&out[k]);
  }
}

bool Ambient::covers(const RECT cut[4]) const {
  for (int k = 0; k < 4; ++k) {
    const Strip& s = st_[k];
    if (IsRectEmpty(&cut[k])) continue;
    if (!s.bits || cut[k].left < s.x || cut[k].top < s.y || cut[k].right > s.x + s.w || cut[k].bottom > s.y + s.h) return false;
  }
  return true;
}

// New surfaces for those parts of the strips; the shape (the costly part) stays as it is.
bool Ambient::cut_surfaces(const RECT cut[4]) {
  free_bitmaps();
  rebuild_pending_ = false;
  HDC screen = GetDC(nullptr);
  for (int k = 0; k < 4; ++k) {
    Strip& s = st_[k];
    s.x = cut[k].left; s.y = cut[k].top;
    s.w = std::max(0L, cut[k].right - cut[k].left); s.h = std::max(0L, cut[k].bottom - cut[k].top);
    if (s.w <= 0 || s.h <= 0) { s.w = s.h = 0; continue; }  // entirely off screen: nothing to make
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = s.w;
    bi.bmiHeader.biHeight = -s.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    s.mem = CreateCompatibleDC(screen);
    s.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!s.mem || !s.bmp || !bits) { ReleaseDC(nullptr, screen); free_surfaces(); failed_ = true; return false; }
    SelectObject(s.mem, s.bmp);
    s.bits = static_cast<uint32_t*>(bits);
    std::memset(s.bits, 0, (size_t)s.w * s.h * 4);
    s.held.clear();  // (a new surface holds nothing yet)
  }
  ReleaseDC(nullptr, screen);
  colors_dirty_ = true;
  return true;
}

// The zone colours spread around the edge (see ambient_ring.h); only ramps whose colour changed are
// rebuilt.
void Ambient::build_lut() {
  if (tint_.empty()) tint_.resize(kLut * 256, 0);
  static Rgb ring[kLut];
  static uint8_t ring_a[kLut];
  if (!ambient_ring(zone_, light_, rect_.right - rect_.left, rect_.bottom - rect_.top, ring, ring_a)) return;
  for (int k = 0; k < kLut; ++k) {
    const Rgb c = ring[k];
    const uint8_t na = ring_a[k];
    if (c.r != lr_[k] || c.g != lg_[k] || c.b != lb_[k] || na != la_[k]) {
      lr_[k] = c.r; lg_[k] = c.g; lb_[k] = c.b; la_[k] = na;
      ambient_ramp(c.r, c.g, c.b, na, tint_.data() + k * 256);
    }
  }
}

void Ambient::set_topmost(bool on) {
  for (auto& s : st_)
    if (s.wnd) SetWindowPos(s.wnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void Ambient::place(HWND owner, bool show) {
  HDWP dw = BeginDeferWindowPos(4);
  for (auto& s : st_) {
    if (!s.wnd || !s.bits) continue;
    UINT fl = SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | (show ? SWP_SHOWWINDOW : 0);
    if (dw) dw = DeferWindowPos(dw, s.wnd, owner, rect_.left - M_ + s.x, rect_.top - M_ + s.y, 0, 0, fl);
  }
  if (dw) EndDeferWindowPos(dw);
}

// the falloff's levels as they are: the table a paint reads when the light neither comes nor goes
static constexpr struct Levels {
  uint8_t v[256];
  constexpr Levels() : v{} { for (int i = 0; i < 256; ++i) v[i] = (uint8_t)i; }
} kLevels{};
// (no colour a paint can make: the tint table's alpha is never above 235, so 0xFFFFFFFF never appears)
static constexpr uint32_t kNothing = 0xFFFFFFFFu;

void Ambient::paint(HWND owner) {
  if (!built_) return;
  LARGE_INTEGER q0, qf;
  QueryPerformanceCounter(&q0);
  build_lut();
  bool needs_show = false;
  HDC screen = GetDC(nullptr);
  BLENDFUNCTION bf{AC_SRC_OVER, 0, blend(), AC_SRC_ALPHA};
  // (the cost the reveal goes by, paint_ms_, is a whole halo's: a paint that sent only a changed
  // part says nothing about it, see below)
  bool whole_paint = true;
  for (auto& s : st_) {
    if (!s.bits) { if (s.wnd && IsWindowVisible(s.wnd)) ShowWindow(s.wnd, SW_HIDE); continue; }
    needs_show |= !IsWindowVisible(s.wnd);
    // the surface is the strip's part on a screen: the shape's pixels off it are skipped
    const int ox = s.fx - s.x, oy = s.fy - s.y;
    const bool whole = ox == 0 && oy == 0 && s.w == s.fw && s.h == s.fh;
    // coming or going: each pixel's falloff through the reveal's table (one lookup more, only now)
    const uint8_t* level = remap_on_ ? remap_ : kLevels.v;
    const uint32_t* tint = tint_.data();
    // what each run holds already: a run whose colour is the same as last time is not written
    // again (a light flowing slowly changes a few levels at a time, and its faint outer pixels,
    // premultiplied, often not at all). Forgotten whenever the surfaces are made again.
    const bool fresh = s.held.size() != s.runs.size();  // (a new surface: everything is sent)
    if (fresh) s.held.assign(s.runs.size(), kNothing);
    uint32_t* held = s.held.data();
    size_t rewritten = 0;
    // the part that changed, in the surface's coordinates (only that goes to Windows)
    int dx0 = s.w, dy0 = s.h, dx1 = 0, dy1 = 0;
    auto changed = [&](int x0, int y0, int x1, int y1) {
      dx0 = std::min(dx0, x0); dy0 = std::min(dy0, y0); dx1 = std::max(dx1, x1); dy1 = std::max(dy1, y1);
    };
    for (const Run& q : s.runs) {
      const uint32_t v = tint[(size_t)q.lut * 256 + level[q.a]];
      uint32_t& had = *held++;
      if (v == had) continue;
      had = v;
      ++rewritten;
      const int x = (int)(q.off % (uint32_t)s.fw) + ox, y = (int)(q.off / (uint32_t)s.fw) + oy;
      if (whole) {
        uint32_t* d = s.bits + q.off;
        if (!q.down) { std::fill_n(d, q.n, v); changed(x, y, x + q.n, y + 1); }
        else { for (int i = 0; i < q.n; ++i, d += s.w) *d = v; changed(x, y, x + 1, y + q.n); }
        continue;
      }
      // a surface cut to the screens: the run clipped to it (in its own coordinates)
      if (!q.down) {
        if (y < 0 || y >= s.h) continue;
        const int x0 = std::max(x, 0), x1 = std::min(x + (int)q.n, s.w);
        if (x1 > x0) { std::fill_n(s.bits + (size_t)y * s.w + x0, x1 - x0, v); changed(x0, y, x1, y + 1); }
      } else {
        if (x < 0 || x >= s.w) continue;
        const int y0 = std::max(y, 0), y1 = std::min(y + (int)q.n, s.h);
        for (int yy = y0; yy < y1; ++yy) s.bits[(size_t)yy * s.w + x] = v;
        if (y1 > y0) changed(x, y0, x + 1, y1);
      }
    }
    POINT dst{rect_.left - M_ + s.x, rect_.top - M_ + s.y}, src{0, 0};
    SIZE sz{s.w, s.h};
    // (23/11) only what changed goes to Windows: a strip none of whose pixels changed, already shown
    // where it is with this opacity, is left as it is; otherwise the changed part is sent (the rest
    // of the window keeps the very same pixels from before). A new surface goes whole.
    RECT win{};
    const bool in_place = !fresh && IsWindowVisible(s.wnd) && GetWindowRect(s.wnd, &win) && win.left == dst.x &&
                          win.top == dst.y && win.right - win.left == s.w && win.bottom - win.top == s.h;
    if (rewritten < s.runs.size()) whole_paint = false;
    if (in_place && dx1 <= dx0 && bf.SourceConstantAlpha == last_a_) continue;
    RECT dirty{dx0, dy0, dx1, dy1};
    UPDATELAYEREDWINDOWINFO info{sizeof info, screen, &dst, &sz, s.mem, &src, 0, &bf, ULW_ALPHA, in_place && dx1 > dx0 ? &dirty : nullptr};
    if (!UpdateLayeredWindowIndirect(s.wnd, &info)) failed_ = true;
  }
  ReleaseDC(nullptr, screen);
  if (needs_show) place(owner, true); // colour changes do not need another move/z-order batch
  {
    LARGE_INTEGER q1;
    QueryPerformanceCounter(&q1); QueryPerformanceFrequency(&qf);
    const double ms = (q1.QuadPart - q0.QuadPart) * 1000.0 / qf.QuadPart;
    // only a paint of the whole halo (the reveal's own steps are those): one that sent a few
    // changed pixels costs far less, and would let the reveal believe it is cheap here
    if (whole_paint || paint_ms_ <= 0) paint_ms_ = paint_ms_ > 0 ? paint_ms_ * 0.7 + ms * 0.3 : ms;
  }
  last_a_ = blend();
  painted_remap_on_ = remap_on_;
  if (remap_on_) std::memcpy(painted_remap_, remap_, sizeof remap_);
  colors_dirty_ = false;
  since_paint_ = 0;
  for (int k = 0; k < kZones; ++k) { painted_zone_[k] = zone_[k]; painted_light_[k] = light_[k]; }
}

int Ambient::ticks_for_change() const {
  int d = 0;
  double dl = 0;
  for (int k = 0; k < kZones; ++k) {
    const Rgb a = zone_[k], b = painted_zone_[k];
    d = std::max({d, std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)});
    dl = std::max(dl, std::fabs(light_[k] - painted_light_[k]) * 255);
  }
  if (d <= 1 && dl <= 1.5) return 3;
  if (d <= 3 && dl <= 3.5) return 2;
  return 1;
}

void Ambient::follow(HWND owner, bool defer_geometry) {
  if (reveal_.phase == AmbientReveal::Off || !built_) return;
  // minimized: the light goes in the same instant the window starts for the taskbar. (17/11, 12:01: a
  // 0.25 s gathering-in stayed behind as a trail where the window had been, since Windows' animation
  // takes the window away at once; and it painted all through that animation.) The shape is kept: the
  // way back costs no rebuild (it used to be rebuilt for the taskbar button's little rectangle, and again)
  if (IsIconic(owner)) { vanish(); return; }
  RECT r = visible_rect(owner);
  if (r.right - r.left != rect_.right - rect_.left || r.bottom - r.top != rect_.bottom - rect_.top) {
    // a new size: the halo of the old one would stay behind as a frame around nothing, so it
    // goes at once; it is rebuilt for the new size and comes back in when the edge is let go
    if (defer_geometry) { reveal_.cut(); hide(); last_a_ = -1; rebuild_pending_ = true; return; }
    reveal_.cut();
    build_shape(r, GetDpiForWindow(owner));
    paint(owner);
    return;
  }
  // moved: same halo, just somewhere else. When the screens now show a part its surfaces do not
  // hold (a window coming back from past an edge), new surfaces are cut at once from the shape
  // (no recomputing it): while it is dragged, the whole strips, so the rest of the drag needs
  // nothing more; settled, only what is on a screen. Surfaces bigger than needed are trimmed a
  // moment after it settles (never on every step of a drag).
  RECT cut[4];
  rect_ = r;
  layout_cut(r, cut);
  if (!covers(cut)) {
    if (defer_geometry) {
      for (int k = 0; k < 4; ++k) {
        const Strip& s = st_[k];
        if (s.runs.empty()) SetRectEmpty(&cut[k]);
        else cut[k] = {s.fx, s.fy, s.fx + s.fw, s.fy + s.fh};
      }
    }
    if (!cut_surfaces(cut)) return;
    rebuild_pending_ = defer_geometry;
    paint(owner);
    place(owner, false);
    return;
  }
  for (int k = 0; k < 4; ++k)
    if (cut[k].right - cut[k].left != st_[k].w || cut[k].bottom - cut[k].top != st_[k].h) rebuild_pending_ = true;
  place(owner, false);
}

BYTE Ambient::blend() const { return (BYTE)std::lround(std::clamp(reveal_.opacity() * strength_, 0.0, 1.0) * 255); }

void Ambient::vanish() {
  reveal_.cut();
  next_out_ = -1;
  remap_on_ = painted_remap_on_ = false;
  last_a_ = -1;
  hide();
}

// The usual exit, or the one asked for; minimized, Windows' own animation sets it (none: at once,
// nearly: 0.1 s; never a glow where a window that is already gone was)
void Ambient::begin_out(HWND owner) {
  double len = AmbientReveal::kOut;
  if (next_out_ >= 0) len = next_out_;
  else if (owner && IsIconic(owner) && !system_window_animation()) len = 0.1;
  next_out_ = -1;
  reveal_.begin_out(len, motion_ && paint_ms_ < kOpacityOnlyMs);
}

// Made ahead, hidden: the windows and the shape for this size, so the first time the halo
// lights costs only its colours (the welcome's second screen used to build it mid-slide).
void Ambient::prepare(HWND owner, HINSTANCE inst) {
  if (failed_ || reveal_.phase != AmbientReveal::Off || want_) return;
  if (!create(inst)) return;
  const RECT r = visible_rect(owner);
  if (r.right - r.left <= 0 || r.bottom - r.top <= 0) return;
  if (built_ && r.right - r.left == rect_.right - rect_.left && r.bottom - r.top == rect_.bottom - rect_.top) return;
  build_shape(r, GetDpiForWindow(owner));
}

bool Ambient::prepared(HWND owner) const {
  if (!built_ || failed_) return false;
  const RECT r = visible_rect(owner);
  return r.right - r.left == rect_.right - rect_.left && r.bottom - r.top == rect_.bottom - rect_.top;
}

bool Ambient::update(HWND owner, const Rgb* zones, const double* light, bool visible, double strength, double dt, bool defer_geometry) {
  if (!st_[0].wnd || failed_) return false;
  want_ = visible;
  strength_ = std::clamp(strength, 0.0, 1.0);
  // (17/11) in from the edge outward, out back into the window: see ambient_reveal.h
  if (visible && !defer_geometry) reveal_.begin_in(motion_ && paint_ms_ < kOpacityOnlyMs);
  else if (reveal_.phase == AmbientReveal::In || reveal_.phase == AmbientReveal::On) begin_out(owner);
  const bool moving = reveal_.step(dt);
  since_paint_ += dt;
  for (int k = 0; k < kZones; ++k) {
    if (zones[k].r != zone_[k].r || zones[k].g != zone_[k].g || zones[k].b != zone_[k].b ||
        std::fabs(light[k] - light_[k]) > 1.0 / 1024) {
      zone_[k] = zones[k];
      light_[k] = light[k];
      colors_dirty_ = true;
    }
  }
  if (reveal_.phase == AmbientReveal::Off) {
    hide();
    last_a_ = -1;
    remap_on_ = painted_remap_on_ = false;
    return moving;
  }
  remap_on_ = reveal_.remapping();
  if (remap_on_) reveal_.remap(remap_);
  // going out, it stays where it lit (the window may already be on its way to the taskbar): no new
  // shape, no new place; it only gathers in
  if (reveal_.phase != AmbientReveal::Out && !IsIconic(owner)) {
    RECT r = visible_rect(owner);
    if (!built_ || r.right - r.left != rect_.right - rect_.left || r.bottom - r.top != rect_.bottom - rect_.top) {
      if (defer_geometry) {
        rebuild_pending_ = true;
        int a=blend();BLENDFUNCTION bf{AC_SRC_OVER,0,(BYTE)a,AC_SRC_ALPHA};
        if(a!=last_a_)for(auto& strip:st_)if(strip.bits)UpdateLayeredWindow(strip.wnd,nullptr,nullptr,nullptr,nullptr,nullptr,0,&bf,ULW_ALPHA);
        last_a_=a;return moving;
      }
      build_shape(r, GetDpiForWindow(owner));
      paint(owner);
      return moving;
    }
    // lit again after a while hidden (or made ahead, see prepare): the window may have moved
    // meanwhile, so its surfaces may not hold all of the halo where it is now
    if (last_a_ < 0 && !defer_geometry) {
      RECT cut[4];
      layout_cut(r, cut);
      if (!covers(cut)) cut_surfaces(cut);
    }
    rect_ = r;
    since_build_ += dt;
    if (rebuild_pending_ && since_build_ > 0.15 && !defer_geometry && !held_) {  // (never mid-drag: when it is let go)
      // the same shape, cut again to what the screens show now (the costly shape is not redone)
      RECT cut[4];
      layout_cut(r, cut);
      since_build_ = 0;
      if (cut_surfaces(cut)) { paint(owner); place(owner, false); }
      return moving;
    }
  }
  int a = blend();
  // T_POLL is 33 ms: allow timer jitter rather than accidentally painting every
  // second tick when 33 ms falls just short of 33.333 ms.
  // (hidden, its colours the same: painted anyway, since painting is what shows the strips. A
  // paused film turned the light off and on again used to keep it hidden until the next frame)
  // (a small change: painted with the next one, or after a tick or two; see painted_zone_)
  // (coming or going: the levels shown change on most ticks; like a colour, a step of one 8-bit level
  // at most waits for the next, a step of up to three for one more tick; larger, on every tick)
  int ticks = colors_dirty_ ? ticks_for_change() : 4;
  if (remap_on_ != painted_remap_on_) ticks = 1;
  else if (remap_on_) {
    int d = 0;
    for (int m = 0; m < 256; ++m) d = std::max(d, std::abs((int)remap_[m] - (int)painted_remap_[m]));
    if (d > 0) ticks = std::min(ticks, d <= 1 ? 3 : d <= 3 || paint_ms_ >= kHalfRateMs ? 2 : 1);
  }
  if ((ticks < 4 && since_paint_ >= ticks / 30.0 - 0.002) || last_a_ < 0) paint(owner);
  else if (a != last_a_) {
    // only the fade changed: no new pixels, just the strips' overall opacity
    BLENDFUNCTION bf{AC_SRC_OVER, 0, (BYTE)a, AC_SRC_ALPHA};
    for (auto& s : st_)
      if (s.bits) UpdateLayeredWindow(s.wnd, nullptr, nullptr, nullptr, nullptr, nullptr, 0, &bf, ULW_ALPHA);
    last_a_ = a;
  }
  return moving;
}
