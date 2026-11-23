// The halo while a film plays (1.4.1, 23/11), with the real Ambient on real layered windows: its
// colours flowing every tick as LightFlow moves them ("busy": every zone a level or two a tick;
// "calm": a level here and there), then going out. Every picture Windows is given
// (UpdateLayeredWindow / UpdateLayeredWindowIndirect, renamed to bench_ulw / bench_ulwi) is copied
// into a model of what each strip window holds (the whole surface, or only its dirty part), and
// the model of all strips, with their opacity and place, is hashed after every tick: two builds of
// ambient.cpp that show the same light give the same hash. It reports Cue's own time per tick, the
// system's (Wine's here, DWM's on Windows) and the pixels sent. DUMP=file.ppm writes what the
// strips show at the end of the flow.
//   build (from the repository):
//   F="-std=c++20 -O2 -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -I."
//   x86_64-w64-mingw32-g++-posix $F -DUpdateLayeredWindow=bench_ulw -DUpdateLayeredWindowIndirect=bench_ulwi -c src/win/ambient.cpp -o ambient_bench.o
//   x86_64-w64-mingw32-g++-posix $F tools/benchmark_halo.cpp ambient_bench.o src/ui/atmosphere.cpp -static -ldwmapi -lgdi32 -luser32 -o benchmark_halo.exe
//   benchmark_halo.exe <width> <height> [x] [calm]
#include <windows.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <tuple>
#include <vector>
#include "src/win/ambient.h"
static double now_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Model { int w = 0, h = 0, x = 0, y = 0, alpha = -1; std::vector<uint32_t> px; };
static std::map<HWND, Model> g_win;
static int g_calls; static long long g_px; static double g_sys;
static void take(HWND h, HDC src, const POINT* dst, const SIZE* s, const BLENDFUNCTION* bf, const RECT* dirty) {
  Model& m = g_win[h];
  if (bf) m.alpha = bf->SourceConstantAlpha;
  if (dst) { m.x = dst->x; m.y = dst->y; }
  if (!s || !src) return;
  DIBSECTION ds{};
  if (GetObjectW(GetCurrentObject(src, OBJ_BITMAP), sizeof ds, &ds) != sizeof ds || !ds.dsBm.bmBits) return;
  const uint32_t* b = (const uint32_t*)ds.dsBm.bmBits;
  if (m.w != s->cx || m.h != s->cy || !dirty) { m.w = s->cx; m.h = s->cy; m.px.assign(b, b + (size_t)m.w * m.h); g_px += (long long)m.w * m.h; return; }
  for (int y = dirty->top; y < dirty->bottom; ++y)
    std::memcpy(&m.px[(size_t)y * m.w + dirty->left], b + (size_t)y * m.w + dirty->left, (dirty->right - dirty->left) * 4);
  g_px += (long long)(dirty->right - dirty->left) * (dirty->bottom - dirty->top);
}
static BOOL WINAPI bench_ulw(HWND h, HDC d, POINT* p, SIZE* s, HDC src, POINT* sp, COLORREF k, BLENDFUNCTION* bf, DWORD f) {
  take(h, src, p, s, bf, nullptr);
  ++g_calls; const double t = now_s();
  BOOL r = UpdateLayeredWindow(h, d, p, s, src, sp, k, bf, f);
  g_sys += now_s() - t; return r;
}
static BOOL WINAPI bench_ulwi(HWND h, const UPDATELAYEREDWINDOWINFO* i) {
  take(h, i->hdcSrc, i->pptDst, i->psize, i->pblend, i->prcDirty);
  ++g_calls; const double t = now_s();
  BOOL r = UpdateLayeredWindowIndirect(h, i);
  g_sys += now_s() - t; return r;
}
extern "C" {
BOOL(WINAPI* __imp_bench_ulw)(HWND, HDC, POINT*, SIZE*, HDC, POINT*, COLORREF, BLENDFUNCTION*, DWORD) = bench_ulw;
BOOL(WINAPI* __imp_bench_ulwi)(HWND, const UPDATELAYEREDWINDOWINFO*) = bench_ulwi;
}
bool system_window_animation() { return true; }
static unsigned long long screen_hash() {
  unsigned long long hsh = 1469598103934665603ull;
  auto mix = [&](const void* p, size_t n) { const unsigned char* c = (const unsigned char*)p; for (size_t i = 0; i < n; ++i) { hsh ^= c[i]; hsh *= 1099511628211ull; } };
  std::vector<std::pair<HWND, Model*>> order;
  for (auto& [h, m] : g_win) order.push_back({h, &m});
  // (by place, not by handle: two runs get different handles for the same strips)
  std::sort(order.begin(), order.end(), [](auto& a, auto& b) { return std::make_tuple(a.second->x, a.second->y, a.second->w) < std::make_tuple(b.second->x, b.second->y, b.second->w); });
  for (auto& [h, mp] : order) {
    Model& m = *mp;
    const int vis = IsWindowVisible(h) ? 1 : 0;
    mix(&vis, sizeof vis);
    if (!vis) continue;
    mix(&m.alpha, sizeof m.alpha); mix(&m.x, sizeof m.x); mix(&m.y, sizeof m.y); mix(&m.w, sizeof m.w); mix(&m.h, sizeof m.h);
    mix(m.px.data(), m.px.size() * 4);
  }
  return hsh;
}
int main(int argc, char** argv) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const int W = argc > 2 ? atoi(argv[1]) : 2573, H = argc > 2 ? atoi(argv[2]) : 1447;
  const int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
  const int X = argc > 3 ? atoi(argv[3]) : (sw - W) / 2;
  const int slow = argc > 4 ? atoi(argv[4]) : 0;  // 1: a calmer light (a level here and there)
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = inst; wc.lpszClassName = L"bench.owner"; RegisterClassW(&wc);
  HWND owner = CreateWindowExW(0, wc.lpszClassName, L"o", WS_POPUP | WS_VISIBLE, X, (sh - H) / 2, W, H, nullptr, nullptr, inst, nullptr);
  Rgb z[26]; double l[26];
  for (int k = 0; k < 26; ++k) { z[k] = {(uint8_t)(40 + k * 7), (uint8_t)(200 - k * 5), 120}; l[k] = 0.8; }
  Ambient a; a.create(inst);
  unsigned long long all = 1469598103934665603ull;
  int tickn = 0; const bool trace = getenv("TRACE") != nullptr;
  auto dump = [&]() { if (!trace) return; std::printf("tick %d:", tickn); std::vector<std::pair<HWND, Model*>> o2; for (auto& [h, m] : g_win) o2.push_back({h, &m}); std::sort(o2.begin(), o2.end(), [](auto& a, auto& b) { return std::make_tuple(a.second->x, a.second->y, a.second->w) < std::make_tuple(b.second->x, b.second->y, b.second->w); }); for (auto& [h, mp] : o2) { Model& m = *mp; unsigned long long q = 1469598103934665603ull; for (auto v : m.px) { q ^= v; q *= 1099511628211ull; } std::printf(" [vis%d a%d %d,%d %dx%d %08llx]", IsWindowVisible(h) ? 1 : 0, m.alpha, m.x, m.y, m.w, m.h, q & 0xffffffff); } std::printf("\n"); };
  auto tick = [&](bool on) { a.update(owner, z, l, on, 0.6, 1 / 30.0); all ^= screen_hash(); all *= 1099511628211ull; dump(); ++tickn; };
  for (int i = 0; i < 90; ++i) tick(true);  // in, settled
  g_calls = 0; g_px = 0; g_sys = 0;
  double own = 0;
  for (int i = 0; i < 180; ++i) {  // 6 s at the poll's 30 Hz
    if (slow) { const int k = (i * 7) % 26; if (i % 2 == 0) z[k].r = (uint8_t)(z[k].r + 1); if (i % 5 == 0) l[(k + 3) % 26] = 0.8 + 0.02 * (i % 7); }
    else for (int k = 0; k < 26; ++k) { z[k].r = (uint8_t)(z[k].r + ((i + k) % 3 == 0)); z[k].b = (uint8_t)(z[k].b + ((i * 7 + k) % 5 == 0 ? 2 : 0)); l[k] = 0.8 + 0.1 * ((i + k) % 20) / 20.0; }
    const double s0 = now_s(), y0 = g_sys;
    a.update(owner, z, l, true, 0.6, 1 / 30.0);
    own += (now_s() - s0) - (g_sys - y0);
    all ^= screen_hash(); all *= 1099511628211ull; dump(); ++tickn;
  }
  const int calls = g_calls; const long long px = g_px; const double sys = g_sys;
  if (const char* out = getenv("DUMP")) {  // what the strips show now, laid on a dark screen (premultiplied alpha, times their opacity)
    std::vector<uint8_t> img((size_t)sw * sh * 3, 20);
    for (auto& [h, m] : g_win) {
      if (!IsWindowVisible(h)) continue;
      for (int y = 0; y < m.h; ++y) for (int x = 0; x < m.w; ++x) {
        const int X = m.x + x, Y = m.y + y; if (X < 0 || Y < 0 || X >= sw || Y >= sh) continue;
        const uint32_t p = m.px[(size_t)y * m.w + x]; const double k = m.alpha / 255.0, a = (p >> 24) / 255.0 * k;
        uint8_t* d = &img[((size_t)Y * sw + X) * 3];
        const int c[3] = {(int)(p >> 16 & 255), (int)(p >> 8 & 255), (int)(p & 255)};
        for (int i = 0; i < 3; ++i) d[i] = (uint8_t)std::min(255.0, c[i] * k + d[i] * (1 - a) + 0.5);
      }
    }
    RECT o; GetWindowRect(owner, &o);
    for (int y = std::max(0L, o.top); y < std::min((long)sh, o.bottom); ++y) for (int x = std::max(0L, o.left); x < std::min((long)sw, o.right); ++x) { uint8_t* d = &img[((size_t)y * sw + x) * 3]; d[0] = d[1] = d[2] = 0; }
    FILE* f = fopen(out, "wb"); fprintf(f, "P6\n%d %d\n255\n", sw, sh); fwrite(img.data(), 1, img.size(), f); fclose(f);
  }
  for (int i = 0; i < 30; ++i) tick(false);  // out
  std::printf("%dx%d %s: per tick, Cue %.2f ms + Windows %.2f ms, %d calls, %.0fk px sent | shown hash=%016llx\n", W, H, slow ? "calm" : "busy",
              own * 1000 / 180, sys * 1000 / 180, calls, px / 1000.0 / 180, all);
  a.destroy(); DestroyWindow(owner);
}
