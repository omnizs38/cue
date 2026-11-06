#include "ambient.h"
#include "../ui/ambient_raster.h"

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
    s.w = s.h = 0;
  }
}

void Ambient::free_surfaces() {
  free_bitmaps();
  for (auto& s : st_) s.shape.clear();
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
  alpha_ = 0;
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
  for (int k = 0; k < 4; ++k) {
    Strip& s = st_[k];
    if (s.fw <= 0 || s.fh <= 0 || s.fw > 65535 || s.fh > 65535) continue;
    s.shape.reserve((size_t)s.fw * s.fh / 2);
    for (int ly = 0; ly < s.fh; ++ly)
      for (int lx = 0; lx < s.fw; ++lx) {
        int x = s.fx + lx, y = s.fy + ly;
        int qx = std::clamp(x, M_, M_ + ww - 1), qy = std::clamp(y, M_, M_ + wh - 1);
        double dx = x - qx, dy = y - qy;
        double u = std::sqrt(dx * dx + dy * dy) / M_;
        if (u >= 1) continue;
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
          u = 0;
        }
        // a soft light: strong at the edge, fading like a lamp on a wall, ending at zero
        double f = std::exp(-2.8 * u * u) * (1 - u);
        int a = (int)std::lround(f * 235);
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
        s.shape.push_back({(uint16_t)lx, (uint16_t)ly, (uint16_t)li, (uint8_t)a});
      }
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
    if (s.shape.empty() || !IntersectRect(&out[k], &full, &screens)) SetRectEmpty(&out[k]);
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
  }
  ReleaseDC(nullptr, screen);
  colors_dirty_ = true;
  return true;
}

// The zone colours spread around the edge, blended softly between zones (like the TV's LEDs).
void Ambient::build_lut() {
  if (tint_.empty()) tint_.resize(kLut * 256, 0);
  int ww = rect_.right - rect_.left, wh = rect_.bottom - rect_.top;
  double P = 2.0 * (ww + wh);
  if (P <= 0) return;
  double pos[kZones];
  for (int i = 0; i < 8; ++i) pos[i] = (i + 0.5) / 8 * ww;
  for (int i = 0; i < 5; ++i) pos[8 + i] = ww + (i + 0.5) / 5 * wh;
  for (int i = 0; i < 8; ++i) pos[13 + i] = ww + wh + (i + 0.5) / 8 * ww;
  for (int i = 0; i < 5; ++i) pos[21 + i] = 2.0 * ww + wh + (i + 0.5) / 5 * wh;
  static double r[kLut], g[kLut], b[kLut], l[kLut];
  for (int k = 0; k < kLut; ++k) {
    double p = (k + 0.5) / kLut * P;
    int j = kZones - 1;
    for (int z = 0; z < kZones; ++z) if (pos[z] <= p) j = z;
    int n = (j + 1) % kZones;
    double a = pos[j], bpos = n == 0 ? pos[0] + P : pos[n];
    if (p < pos[0]) { a = pos[kZones - 1] - P; bpos = pos[0]; }  // wrapping past the top-left corner
    double t = std::clamp((p - a) / std::max(1.0, bpos - a), 0.0, 1.0);
    t = t * t * (3 - 2 * t);
    r[k] = zone_[j].r + (zone_[n].r - zone_[j].r) * t;
    g[k] = zone_[j].g + (zone_[n].g - zone_[j].g) * t;
    b[k] = zone_[j].b + (zone_[n].b - zone_[j].b) * t;
    l[k] = light_[j] + (light_[n] - light_[j]) * t;
  }
  // a little extra blur around the ring, so no zone has a hard border
  static double tr[kLut], tg[kLut], tb[kLut], tl[kLut];
  const int R = 10;
  for (int k = 0; k < kLut; ++k) {
    double sr = 0, sg = 0, sb = 0, sl = 0;
    for (int d = -R; d <= R; ++d) { int q = (k + d + kLut) % kLut; sr += r[q]; sg += g[q]; sb += b[q]; sl += l[q]; }
    tr[k] = sr / (2 * R + 1); tg[k] = sg / (2 * R + 1); tb[k] = sb / (2 * R + 1); tl[k] = sl / (2 * R + 1);
  }
  for (int k = 0; k < kLut; ++k) {
    auto quantize = [](double v) { return (uint8_t)std::lround(std::clamp(v, 0.0, 255.0)); };
    uint8_t nr = quantize(tr[k]), ng = quantize(tg[k]), nb = quantize(tb[k]), na = quantize(tl[k] * 255);
    if (nr != lr_[k] || ng != lg_[k] || nb != lb_[k] || na != la_[k]) {
      lr_[k] = nr; lg_[k] = ng; lb_[k] = nb; la_[k] = na;
      ambient_ramp(nr, ng, nb, na, tint_.data() + k * 256);
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

void Ambient::paint(HWND owner) {
  if (!built_) return;
  build_lut();
  bool needs_show = false;
  HDC screen = GetDC(nullptr);
  BLENDFUNCTION bf{AC_SRC_OVER, 0, blend(), AC_SRC_ALPHA};
  for (auto& s : st_) {
    if (!s.bits) { if (s.wnd && IsWindowVisible(s.wnd)) ShowWindow(s.wnd, SW_HIDE); continue; }
    needs_show |= !IsWindowVisible(s.wnd);
    // the surface is the strip's part on a screen: the shape's pixels off it are skipped
    const int ox = s.fx - s.x, oy = s.fy - s.y;
    const bool whole = ox == 0 && oy == 0 && s.w == s.fw && s.h == s.fh;
    for (const Px& q : s.shape) {
      const unsigned px = (unsigned)(q.x + ox), py = (unsigned)(q.y + oy);
      if (!whole && (px >= (unsigned)s.w || py >= (unsigned)s.h)) continue;
      s.bits[(size_t)py * s.w + px] = tint_[(size_t)q.lut * 256 + q.a];
    }
    POINT dst{rect_.left - M_ + s.x, rect_.top - M_ + s.y}, src{0, 0};
    SIZE sz{s.w, s.h};
    if (!UpdateLayeredWindow(s.wnd, screen, &dst, &sz, s.mem, &src, 0, &bf, ULW_ALPHA)) failed_ = true;
  }
  ReleaseDC(nullptr, screen);
  if (needs_show) place(owner, true); // colour changes do not need another move/z-order batch
  last_a_ = blend();
  colors_dirty_ = false;
  since_paint_ = 0;
}

void Ambient::follow(HWND owner, bool defer_geometry) {
  if (alpha_ <= 0.001 || !built_) return;
  RECT r = visible_rect(owner);
  if (r.right - r.left != rect_.right - rect_.left || r.bottom - r.top != rect_.bottom - rect_.top) {
    // a new size: the halo of the old one would stay behind as a frame around nothing, so it
    // goes at once; it is rebuilt for the new size and fades back in when the edge is let go
    if (defer_geometry) { alpha_ = 0; hide(); last_a_ = -1; rebuild_pending_ = true; return; }
    alpha_ = 0;
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
        if (s.shape.empty()) SetRectEmpty(&cut[k]);
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

BYTE Ambient::blend() const { return (BYTE)std::lround(std::clamp(alpha_ * strength_, 0.0, 1.0) * 255); }

bool Ambient::update(HWND owner, const Rgb* zones, const double* light, bool visible, double strength, double dt, bool defer_geometry) {
  if (!st_[0].wnd || failed_) return false;
  want_ = visible;
  strength_ = std::clamp(strength, 0.0, 1.0);
  double target = visible && !defer_geometry ? 1.0 : 0.0;
  double na = alpha_ + std::clamp(target - alpha_, -dt / 0.45, dt / 0.8);  // in slowly, out a bit faster
  bool moving = std::fabs(na - alpha_) > 1e-4;
  alpha_ = na;
  since_paint_ += dt;
  for (int k = 0; k < kZones; ++k) {
    if (zones[k].r != zone_[k].r || zones[k].g != zone_[k].g || zones[k].b != zone_[k].b ||
        std::fabs(light[k] - light_[k]) > 1.0 / 1024) {
      zone_[k] = zones[k];
      light_[k] = light[k];
      colors_dirty_ = true;
    }
  }
  if (alpha_ <= 0.001) {
    hide();
    last_a_ = -1;
    return moving;
  }
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
  int a = blend();
  // T_POLL is 33 ms: allow timer jitter rather than accidentally painting every
  // second tick when 33 ms falls just short of 33.333 ms.
  // (hidden, its colours the same: painted anyway, since painting is what shows the strips. A
  // paused film turned the light off and on again used to keep it hidden until the next frame)
  if ((colors_dirty_ && since_paint_ >= 1.0 / 30 - 0.002) || last_a_ < 0) paint(owner);
  else if (a != last_a_) {
    // only the fade changed: no new pixels, just the strips' overall opacity
    BLENDFUNCTION bf{AC_SRC_OVER, 0, (BYTE)a, AC_SRC_ALPHA};
    for (auto& s : st_)
      if (s.bits) UpdateLayeredWindow(s.wnd, nullptr, nullptr, nullptr, nullptr, nullptr, 0, &bf, ULW_ALPHA);
    last_a_ = a;
  }
  return moving;
}
