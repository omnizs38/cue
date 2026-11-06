#include "flight_ghost.h"

#include <algorithm>
#include <cmath>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

static LRESULT CALLBACK ghost_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      // the thumbnail covers it all; under it, black (never the desktop's last pixels)
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
      EndPaint(h, &ps);
      return 0;
    }
  }
  return DefWindowProcW(h, m, w, l);
}

bool FlightGhost::begin(HINSTANCE inst, HWND source, const RECT& screen) {
  end();
  if (!source || screen.right <= screen.left || screen.bottom <= screen.top) return false;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = ghost_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"tinyoffice.Cue.flight";
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  wnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST, L"tinyoffice.Cue.flight", L"",
                         WS_POPUP, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top, nullptr, nullptr,
                         inst, nullptr);
  if (!wnd_) return false;
  BOOL off = TRUE;  // shown and gone at once (Windows' own fade would double the flight)
  DwmSetWindowAttribute(wnd_, DWMWA_TRANSITIONS_FORCEDISABLED, &off, sizeof off);
  int round = 2;  // DWMWCP_ROUND: the player's own corners
  DwmSetWindowAttribute(wnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &round, sizeof round);
  // no outline: Windows 11 draws a grey 1 px line round every rounded popup, and the picture
  // flew with a frame the player itself never has (the window's own border is off too)
  COLORREF none = 0xFFFFFFFE;  // DWMWA_COLOR_NONE
  DwmSetWindowAttribute(wnd_, 34 /*DWMWA_BORDER_COLOR*/, &none, sizeof none);
  src_ = source;
  if (FAILED(DwmRegisterThumbnail(wnd_, src_, &thumb_)) || !thumb_) { thumb_ = nullptr; end(); return false; }
  last_ = {};
  if (!update(screen)) { end(); return false; }
  SetWindowPos(wnd_, HWND_TOPMOST, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  last_ = screen;
  return true;
}

// The picture fills the ghost; the source's client area is cropped to the ghost's shape (centred),
// in the source window's own coordinates (its invisible resize borders left out).
bool FlightGhost::update(const RECT& screen) {
  if (!thumb_) return false;
  RECT wr, cr;
  POINT o{0, 0};
  if (!GetWindowRect(src_, &wr) || !GetClientRect(src_, &cr) || !ClientToScreen(src_, &o)) return false;
  const int cw = cr.right, ch = cr.bottom, gw = screen.right - screen.left, gh = screen.bottom - screen.top;
  if (cw <= 0 || ch <= 0 || gw <= 0 || gh <= 0) return false;
  int sw = cw, sh = ch;
  if ((double)cw / ch > (double)gw / gh) sw = std::max(1, (int)std::lround((double)ch * gw / gh));
  else sh = std::max(1, (int)std::lround((double)cw * gh / gw));
  const int sx = (o.x - wr.left) + (cw - sw) / 2, sy = (o.y - wr.top) + (ch - sh) / 2;
  DWM_THUMBNAIL_PROPERTIES p{};
  p.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
  p.rcDestination = {0, 0, gw, gh};
  p.rcSource = {sx, sy, sx + sw, sy + sh};
  p.opacity = 255;
  p.fVisible = TRUE;
  p.fSourceClientAreaOnly = FALSE;
  return SUCCEEDED(DwmUpdateThumbnailProperties(thumb_, &p));
}

void FlightGhost::place(const RECT& screen) {
  if (!thumb_ || !wnd_) return;
  if (EqualRect(&screen, &last_)) { update(screen); return; }  // (the source may have changed size)
  const bool grows = (screen.right - screen.left) * (screen.bottom - screen.top) > (last_.right - last_.left) * (last_.bottom - last_.top);
  // a frame where the two disagree crops the picture, never shows the black under it: growing,
  // the picture first; shrinking, the window first
  if (grows) update(screen);
  SetWindowPos(wnd_, HWND_TOPMOST, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
               SWP_NOACTIVATE | SWP_NOCOPYBITS);
  if (!grows) update(screen);
  last_ = screen;
}

void FlightGhost::end() {
  if (thumb_) { DwmUnregisterThumbnail(thumb_); thumb_ = nullptr; }
  if (wnd_) { DestroyWindow(wnd_); wnd_ = nullptr; }
  src_ = nullptr;
  last_ = {};
}

// ── the stage cover ─────────────────────────────────────────────────
// Black, click-through, above everything, never activated: it holds the screen still while the
// window changes size for full screen. Windows otherwise composes a frame or two of the window
// at its new place with its old contents (an L of black over the desktop, the rest of the new
// size not painted yet) before the engine draws at the new size.
static LRESULT CALLBACK cover_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: {
      RECT r;
      GetClientRect(h, &r);
      FillRect((HDC)w, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
      return 1;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
      EndPaint(h, &ps);
      return 0;
    }
  }
  return DefWindowProcW(h, m, w, l);
}

bool StageCover::show(HINSTANCE inst, const RECT& screen, bool rounded, bool wait) {
  if (screen.right <= screen.left || screen.bottom <= screen.top) return false;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = cover_proc;
    wc.hInstance = inst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"tinyoffice.Cue.stage";
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  if (!wnd_) {
    wnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST, L"tinyoffice.Cue.stage", L"", WS_POPUP,
                           screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top, nullptr, nullptr, inst, nullptr);
    if (!wnd_) return false;
    BOOL off = TRUE;  // there and gone at once: Windows' own fade would show the desktop through it
    DwmSetWindowAttribute(wnd_, DWMWA_TRANSITIONS_FORCEDISABLED, &off, sizeof off);
    COLORREF none = 0xFFFFFFFE;
    DwmSetWindowAttribute(wnd_, 34 /*DWMWA_BORDER_COLOR*/, &none, sizeof none);
  }
  int corners = rounded ? 2 : 1;  // over a window: its rounded corners; over the screen: square
  DwmSetWindowAttribute(wnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);
  SetWindowPos(wnd_, HWND_TOPMOST, screen.left, screen.top, screen.right - screen.left, screen.bottom - screen.top,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  // painted now, not when the message loop next comes round: a window shown and not yet painted
  // is composed empty (the screen showed through it while the player changed size)
  RedrawWindow(wnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
  // and on screen before the caller moves anything under it: one composition pass
  if (wait) DwmFlush();
  return true;
}

void StageCover::end() {
  if (wnd_) { DestroyWindow(wnd_); wnd_ = nullptr; }
}
