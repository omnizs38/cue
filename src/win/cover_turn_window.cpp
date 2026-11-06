#include "cover_turn_window.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>

namespace {
double seconds() {
  static LARGE_INTEGER f = [] { LARGE_INTEGER v; QueryPerformanceFrequency(&v); return v; }();
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return double(t.QuadPart) / double(f.QuadPart);
}

LRESULT CALLBACK turn_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_NCHITTEST) return HTTRANSPARENT;
  if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
  return DefWindowProcW(h, m, w, l);
}

// a turn that never sees its next cover (a song that will not open) turns back in with the old one
constexpr double kEdgeWait = 2.0;
constexpr double kFade = 0.12;
// the shadows' hand-over (the window's in as the engine's goes, and back)
constexpr double kShadowCross = 0.12;
}  // namespace

struct CoverTurnWindow::State {
  HWND wnd = nullptr;
  HDC dc = nullptr;
  HBITMAP bmp = nullptr, old = nullptr;
  uint32_t* bits = nullptr;
  int w = 0, h = 0;
  double ccx = 0, ccy = 0, half = 0;
  std::shared_ptr<const CoverArt> from;
  std::mutex mu;
  std::shared_ptr<const CoverArt> to;  // (mu)
  std::atomic<bool> go{false}, finish{false}, quit{false}, release{false};
  std::atomic<bool> edge{false}, landed{false}, done{false}, shadow_out{false};
  std::atomic<double> angle{0}, rx{0}, ry{0}, k{0}, shadow{0};
  double t0 = 0;
  ~State() {
    if (dc) { if (old) SelectObject(dc, old); DeleteDC(dc); }
    if (bmp) DeleteObject(bmp);
  }
  // draws the cover turned by `a` (and the pointer's lean) and hands it to Windows
  bool present(const CoverArt& art, double a, BYTE alpha, double sh) {
    cover_turn_draw(art, bits, w, h, ccx, ccy, half, rx.load(), ry.load() + a, k.load(), sh);
    GdiFlush();
    BLENDFUNCTION bf{AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};
    POINT src{0, 0};
    SIZE sz{w, h};
    return UpdateLayeredWindow(wnd, nullptr, nullptr, &sz, dc, &src, 0, &bf, ULW_ALPHA) != FALSE;
  }
};

bool CoverTurnWindow::begin(HINSTANCE inst, HWND owner, const RECT& cover, const RECT& clip, std::shared_ptr<const CoverArt> from) {
  abort();
  if (!from || !from->ok()) return false;
  const int side = cover.right - cover.left;
  if (side < 8) return false;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = turn_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"tinyoffice.Cue.turn";
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    if (!registered) return false;
  }
  auto st = std::make_shared<State>();
  st->half = side / 2.0;
  // the cover's reach while it turns (its near side comes closer and grows), kept to the window
  const double cx = (cover.left + cover.right) / 2.0, cy = (cover.top + cover.bottom) / 2.0;
  const int reach = (int)std::ceil(st->half * 1.4) + 2;
  RECT box{(LONG)std::floor(cx) - reach, (LONG)std::floor(cy) - reach, (LONG)std::ceil(cx) + reach, (LONG)std::ceil(cy) + reach};
  box.left = std::max(box.left, clip.left); box.top = std::max(box.top, clip.top);
  box.right = std::min(box.right, clip.right); box.bottom = std::min(box.bottom, clip.bottom);
  st->w = box.right - box.left; st->h = box.bottom - box.top;
  if (st->w < 8 || st->h < 8) return false;
  st->ccx = cx - box.left; st->ccy = cy - box.top;
  st->from = std::move(from);
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof bi.bmiHeader;
  bi.bmiHeader.biWidth = st->w;
  bi.bmiHeader.biHeight = -st->h;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  st->dc = CreateCompatibleDC(nullptr);
  st->bmp = st->dc ? CreateDIBSection(st->dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
  if (!st->bmp || !bits) return false;
  st->bits = static_cast<uint32_t*>(bits);
  st->old = (HBITMAP)SelectObject(st->dc, st->bmp);
  // (made on this thread, owned by the player: it stays just above it, pinned or not; the thread
  // only changes its pixels, never its place, so Windows sends it nothing meanwhile)
  // (a pinned player is on top of everything: its turning cover with it)
  const bool topmost = owner && (GetWindowLongPtrW(owner, GWL_EXSTYLE) & WS_EX_TOPMOST);
  st->wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | (topmost ? WS_EX_TOPMOST : 0),
                            L"tinyoffice.Cue.turn", L"", WS_POPUP, box.left, box.top, st->w, st->h, owner, nullptr, inst, nullptr);
  if (!st->wnd) return false;
  BOOL off = TRUE;
  DwmSetWindowAttribute(st->wnd, DWMWA_TRANSITIONS_FORCEDISABLED, &off, sizeof off);
  // the first picture (the old cover, flat, where the engine shows it), then on screen
  cover_turn_draw(*st->from, st->bits, st->w, st->h, st->ccx, st->ccy, st->half, 0, 0, 0, 0.0);
  GdiFlush();
  BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  POINT pos{box.left, box.top}, src{0, 0};
  SIZE sz{st->w, st->h};
  if (!UpdateLayeredWindow(st->wnd, nullptr, &pos, &sz, st->dc, &src, 0, &bf, ULW_ALPHA)) {
    DestroyWindow(st->wnd);
    st->wnd = nullptr;
    return false;
  }
  SetWindowPos(st->wnd, topmost ? HWND_TOPMOST : HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  DwmFlush();  // composed before the engine is told to leave its own cover out
  st->t0 = seconds();
  auto* context = new std::shared_ptr<State>(st);
  thread_ = CreateThread(nullptr, 0, main, context, 0, nullptr);
  if (!thread_) {
    delete context;
    DestroyWindow(st->wnd);
    st->wnd = nullptr;
    return false;
  }
  // (a song opening on the other cores never takes its frame)
  SetThreadPriority(thread_, THREAD_PRIORITY_HIGHEST);
  st_ = std::move(st);
  return true;
}

void CoverTurnWindow::set_to(std::shared_ptr<const CoverArt> to) {
  if (!st_ || !to || !to->ok()) return;
  std::lock_guard<std::mutex> lock(st_->mu);
  st_->to = std::move(to);
}
void CoverTurnWindow::go() { if (st_) st_->go = true; }
void CoverTurnWindow::tilt(double rx, double ry, double k) {
  if (!st_) return;
  st_->rx = rx; st_->ry = ry; st_->k = k;
}
void CoverTurnWindow::release() { if (st_) st_->release = true; }
void CoverTurnWindow::finish() { if (st_) st_->finish = true; }
double CoverTurnWindow::shadow() const { return st_ ? st_->shadow.load() : 0.0; }
bool CoverTurnWindow::shadow_out() const { return st_ && st_->shadow_out.load(); }
bool CoverTurnWindow::edge() const { return st_ && st_->edge.load(); }
bool CoverTurnWindow::landed() const { return st_ && st_->landed.load(); }
bool CoverTurnWindow::done() const { return !st_ || st_->done.load(); }
double CoverTurnWindow::angle() const { return st_ ? st_->angle.load() : 0.0; }

void CoverTurnWindow::abort() {
  if (!st_) return;
  st_->quit = true;
  if (thread_) {
    WaitForSingleObject(thread_, 250);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
  if (st_->wnd) { DestroyWindow(st_->wnd); st_->wnd = nullptr; }
  st_.reset();  // (a thread that did not stop in time keeps its own state until it does)
}

DWORD WINAPI CoverTurnWindow::main(void* p) {
  std::unique_ptr<std::shared_ptr<State>> context(static_cast<std::shared_ptr<State>*>(p));
  auto st = std::move(*context);
  context.reset();
  enum { kAway, kAtEdge, kTurnIn, kFlat, kFading } stage = kAway;
  double t_edge = 0, t_in = 0, t_fade = 0, t_release = -1;
  double drawn_a = 0, drawn_rx = 0, drawn_ry = 0, drawn_k = 0, drawn_sh = 0;
  int drawn_alpha = 255;
  std::shared_ptr<const CoverArt> art = st->from, next;
  while (!st->quit) {
    // the compositor's next frame (a minimized or locked screen composes nothing: a plain wait)
    if (FAILED(DwmFlush())) Sleep(8);
    if (st->quit) break;
    const double t = seconds();
    double a = 0;
    int alpha = 255;
    if (stage == kAway) {
      const double k = (t - st->t0) / CoverTurnTiming::kOut;
      a = CoverTurnTiming::away(0, k);
      if (k >= 1) { stage = kAtEdge; t_edge = t; a = CoverTurnTiming::kEdge; st->edge = true; }
    }
    if (stage == kAtEdge) {
      a = CoverTurnTiming::kEdge;
      if (!next) { std::lock_guard<std::mutex> lock(st->mu); next = st->to; }
      // (the next cover was read ahead: it turns in at once, the same movement through the edge)
      const bool ready = next != nullptr;
      if (ready || t - t_edge >= kEdgeWait) {
        art = ready ? next : st->from;  // (nothing came: the old cover turns back in)
        stage = kTurnIn; t_in = t;
      }
    }
    if (stage == kTurnIn) {
      const double k = (t - t_in) / CoverTurnTiming::kIn;
      a = CoverTurnTiming::in(k);
      if (k >= 1) { stage = kFlat; a = 0; st->landed = true; }
    }
    if (stage == kFlat && st->finish) { stage = kFading; t_fade = t; }
    if (stage == kFading) {
      const double k = (t - t_fade) / kFade;
      alpha = (int)std::lround(255 * std::clamp(1 - k, 0.0, 1.0));
      if (k >= 1) { st->angle = 0; st->done = true; break; }
    }
    st->angle = a;
    // the shadow: in over the engine's as the turn starts, out again once released
    double sh = std::clamp((t - st->t0) / kShadowCross, 0.0, 1.0);
    if (st->release.load()) {
      if (t_release < 0) t_release = t;
      sh = std::min(sh, std::clamp(1 - (t - t_release) / kShadowCross, 0.0, 1.0));
      if (sh <= 0) st->shadow_out = true;
    }
    const double rx = st->rx.load(), ry = st->ry.load(), kk = st->k.load();
    if (std::fabs(a - drawn_a) < 1e-6 && alpha == drawn_alpha && rx == drawn_rx && ry == drawn_ry && kk == drawn_k && sh == drawn_sh) continue;
    if (st->quit) break;
    if (!st->present(*art, a, (BYTE)alpha, sh)) { st->done = true; break; }
    // (what is on screen now: the engine is told only what the window really drew)
    st->shadow = sh;
    drawn_a = a; drawn_alpha = alpha; drawn_rx = rx; drawn_ry = ry; drawn_k = kk; drawn_sh = sh;
  }
  st->done = true;
  return 0;
}
