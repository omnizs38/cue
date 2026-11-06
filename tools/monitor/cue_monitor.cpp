// cue-monitor: watches Cue while it is used and writes a short report for whoever reads it later.
//
// Read-only: it never touches Cue, its settings or its files. Every 2 seconds it samples each
// Cue.exe (the player and its preview helper): memory, handles, GDI/USER objects, threads, CPU,
// GPU (load, video decode, memory), disk/pipe I/O and TCP connections. A folder watcher counts
// every write in Cue's data folder (%APPDATA%\tinypeep\Cue, or tinyoffice\Cue before 1.3), file by file.
//
// It writes almost nothing itself: one line per process per minute (appended once a minute) and a
// summary rewritten every 5 minutes and when it closes. The summary does the reading: start and
// end of every counter, growth per hour with how steady it was (R², a leak is steady), CPU by
// window state (in front, behind, minimized), writes per file per hour, connections, crashes,
// and a list of what looks excessive.
//
// Build (MinGW): g++ -std=c++20 -O2 -static -municode cue_monitor.cpp -o cue-monitor.exe
//                -lpsapi -lpdh -liphlpapi -lws2_32 -lversion -lole32 -lshell32 -luuid
// Use: run it, use Cue as usual, close the window (or Ctrl+C). Optional: --interval 1 (seconds),
// --minutes 90 (stops by itself), --out <folder>.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

// ── small helpers ───────────────────────────────────────────────────
static std::string utf8(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}
static std::string fmt(const char* f, ...) {
  char b[1024];
  va_list a;
  va_start(a, f);
  vsnprintf(b, sizeof b, f, a);
  va_end(a);
  return b;
}
static std::string clock_str(time_t t, bool secs = true) {
  tm v{};
  localtime_s(&v, &t);
  char b[32];
  strftime(b, sizeof b, secs ? "%H:%M:%S" : "%H:%M", &v);
  return b;
}
static std::string dur_str(double s) {
  int t = (int)s;
  return fmt("%dh%02dm%02ds", t / 3600, t / 60 % 60, t % 60);
}
static uint64_t ft64(const FILETIME& f) { return (uint64_t)f.dwHighDateTime << 32 | f.dwLowDateTime; }
static double mono() {
  static LARGE_INTEGER q{};
  if (!q.QuadPart) QueryPerformanceFrequency(&q);
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return (double)c.QuadPart / q.QuadPart;
}

// least squares over (x, y): growth per unit of x, and how steady it is (R², 1 = a straight line)
struct Trend { double slope = 0, r2 = 0; };
static Trend trend(const std::vector<double>& x, const std::vector<double>& y) {
  Trend t;
  size_t n = std::min(x.size(), y.size());
  if (n < 4) return t;
  double mx = 0, my = 0;
  for (size_t i = 0; i < n; ++i) { mx += x[i]; my += y[i]; }
  mx /= n; my /= n;
  double sxy = 0, sxx = 0, syy = 0;
  for (size_t i = 0; i < n; ++i) { sxy += (x[i] - mx) * (y[i] - my); sxx += (x[i] - mx) * (x[i] - mx); syy += (y[i] - my) * (y[i] - my); }
  if (sxx <= 0) return t;
  t.slope = sxy / sxx;
  t.r2 = syy > 0 ? sxy * sxy / (sxx * syy) : 1;
  return t;
}

// ── output: the timeline (appended once a minute) and the summary (rewritten) ──
static std::wstring g_dir, g_timeline, g_summary;
static std::mutex g_out_mu;
static std::string g_pending;  // timeline lines waiting for the minute's write
static void out_line(const std::string& s) { std::lock_guard<std::mutex> l(g_out_mu); g_pending += s + "\n"; }
static void out_flush() {
  std::lock_guard<std::mutex> l(g_out_mu);
  if (g_pending.empty()) return;
  if (FILE* f = _wfopen(g_timeline.c_str(), L"ab")) { fwrite(g_pending.data(), 1, g_pending.size(), f); fclose(f); }
  g_pending.clear();
}
static void write_file(const std::wstring& path, const std::string& s) {
  std::wstring tmp = path + L".tmp";
  if (FILE* f = _wfopen(tmp.c_str(), L"wb")) {
    fwrite(s.data(), 1, s.size(), f);
    fclose(f);
    MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
  }
}

// ── the folder watcher: every write in Cue's data folder, file by file ──
struct FileStat { int bursts = 0, created = 0, removed = 0; int64_t last_sec = -1; int minute_bursts = 0; };
static std::mutex g_fs_mu;
static std::map<std::wstring, FileStat> g_files;
static std::atomic<bool> g_quit{false};
static double g_t0 = 0;
static std::wstring g_data_dir;

static void watch_folder() {
  HANDLE dir = CreateFileW(g_data_dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (dir == INVALID_HANDLE_VALUE) { out_line("# folder watcher: could not open " + utf8(g_data_dir)); return; }
  std::vector<BYTE> buf(64 * 1024);
  while (!g_quit) {
    DWORD got = 0;
    if (!ReadDirectoryChangesW(dir, buf.data(), (DWORD)buf.size(), TRUE,
                               FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
                               &got, nullptr, nullptr)) break;
    if (got == 0) { out_line("# folder watcher: too many changes at once (buffer overflow)"); continue; }
    const int64_t sec = (int64_t)(mono() - g_t0);
    std::lock_guard<std::mutex> l(g_fs_mu);
    for (BYTE* p = buf.data();;) {
      auto* n = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(p);
      std::wstring name(n->FileName, n->FileNameLength / sizeof(wchar_t));
      {
        FileStat& f = g_files[name];
        if (n->Action == FILE_ACTION_ADDED || n->Action == FILE_ACTION_RENAMED_NEW_NAME) ++f.created;
        else if (n->Action == FILE_ACTION_REMOVED || n->Action == FILE_ACTION_RENAMED_OLD_NAME) ++f.removed;
        // one write makes several notifications (size, time): a second with activity counts once
        if (n->Action == FILE_ACTION_MODIFIED || n->Action == FILE_ACTION_ADDED || n->Action == FILE_ACTION_RENAMED_NEW_NAME) {
          if (f.last_sec != sec) { f.last_sec = sec; ++f.bursts; ++f.minute_bursts; }
        }
      }
      if (!n->NextEntryOffset) break;
      p += n->NextEntryOffset;
    }
  }
  CloseHandle(dir);
}

// ── GPU, through Windows' own counters (what Task Manager shows) ──
struct Gpu {
  PDH_HQUERY q = nullptr;
  PDH_HCOUNTER eng = nullptr, mem = nullptr;
  bool ok = false;
  std::map<DWORD, double> load, decode, memmb;
  void open() {
    if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS) return;
    ok = PdhAddEnglishCounterW(q, L"\\GPU Engine(*)\\Utilization Percentage", 0, &eng) == ERROR_SUCCESS;
    PdhAddEnglishCounterW(q, L"\\GPU Process Memory(*)\\Dedicated Usage", 0, &mem);
    if (ok) PdhCollectQueryData(q);
  }
  static DWORD pid_of(const wchar_t* inst) {
    const wchar_t* p = wcsstr(inst, L"pid_");
    return p ? (DWORD)wcstoul(p + 4, nullptr, 10) : 0;
  }
  void sample(const std::set<DWORD>& pids) {
    load.clear(); decode.clear(); memmb.clear();
    if (!ok || PdhCollectQueryData(q) != ERROR_SUCCESS) return;
    std::vector<BYTE> buf;
    auto read = [&](PDH_HCOUNTER c, auto&& each) {
      if (!c) return;
      DWORD size = 0, count = 0;
      if (PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &size, &count, nullptr) != (PDH_STATUS)PDH_MORE_DATA) return;
      buf.resize(size);
      auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
      if (PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &size, &count, items) != ERROR_SUCCESS) return;
      for (DWORD i = 0; i < count; ++i)
        if (items[i].FmtValue.CStatus == ERROR_SUCCESS || items[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA) each(items[i].szName, items[i].FmtValue.doubleValue);
    };
    // per process: each engine type summed, the busiest type is the process's load (Task Manager)
    std::map<std::pair<DWORD, std::wstring>, double> by_type;
    read(eng, [&](const wchar_t* name, double v) {
      DWORD pid = pid_of(name);
      if (!pids.count(pid)) return;
      const wchar_t* e = wcsstr(name, L"engtype_");
      by_type[{pid, e ? e + 8 : L"?"}] += v;
    });
    for (auto& [k, v] : by_type) {
      load[k.first] = std::max(load[k.first], v);
      if (k.second.find(L"VideoDecode") != std::wstring::npos) decode[k.first] = std::max(decode[k.first], v);
    }
    read(mem, [&](const wchar_t* name, double v) { DWORD pid = pid_of(name); if (pids.count(pid)) memmb[pid] += v / 1048576.0; });
  }
};

// ── one Cue process ─────────────────────────────────────────────────
struct Minute {  // what a minute adds up to
  int n = 0;
  double cpu = 0, cpu_max = 0, gpu = 0, gpu_max = 0, dec = 0, gmem = 0;
  uint64_t rops = 0, wops = 0, oops = 0, rb = 0, wb = 0, faults = 0;
};
struct Proc {
  DWORD pid = 0, parent = 0;
  HANDLE h = nullptr;
  std::string role;  // "player" or "helper"
  double first = 0;  // seconds since the monitor started
  bool alive = true;
  uint64_t cpu_prev = 0, faults_prev = 0;
  double t_prev = 0;
  IO_COUNTERS io_prev{};
  bool have_prev = false;
  Minute min;
  // the last values seen
  double priv = 0, ws = 0, peak_ws = 0;
  DWORD handles = 0, gdi = 0, user = 0, threads = 0;
  // per minute, for the trends
  std::vector<double> tm, s_priv, s_handles, s_gdi, s_user, s_threads;
  double priv0 = -1, priv_min = 1e18, priv_max = 0;
  DWORD h0 = 0, gdi0 = 0, user0 = 0, thr0 = 0, h_max = 0, gdi_max = 0, user_max = 0, thr_max = 0;
  double cpu_sum = 0, cpu_max = 0, gpu_sum = 0, gpu_max = 0, dec_sum = 0, gmem_max = 0;
  int samples = 0;
  uint64_t rops = 0, wops = 0, oops = 0, rb = 0, wb = 0;
  std::wstring path;
};

static std::map<DWORD, Proc> g_procs;
static std::vector<Proc> g_gone;  // ended processes, kept for the summary
static int g_cores = 1;

// window of the player: size, state, in front or not
struct WinState { int w = 0, h = 0; bool min = false, max = false, top = false, front = false, found = false; };
static WinState find_window(DWORD pid) {
  struct Ctx { DWORD pid; HWND best; int area; } c{pid, nullptr, -1};
  EnumWindows([](HWND w, LPARAM l) -> BOOL {
    auto* c = reinterpret_cast<Ctx*>(l);
    DWORD p = 0;
    GetWindowThreadProcessId(w, &p);
    if (p != c->pid || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
    if (GetWindowLongPtrW(w, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;  // the ambient light, overlays
    RECT r;
    GetWindowRect(w, &r);
    int a = (r.right - r.left) * (r.bottom - r.top);
    if (IsIconic(w)) a = 1;
    if (a > c->area) { c->area = a; c->best = w; }
    return TRUE;
  }, (LPARAM)&c);
  WinState s;
  if (!c.best) return s;
  s.found = true;
  RECT r;
  GetClientRect(c.best, &r);
  s.w = r.right; s.h = r.bottom;
  s.min = IsIconic(c.best); s.max = IsZoomed(c.best);
  s.top = (GetWindowLongPtrW(c.best, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
  HWND fg = GetForegroundWindow();
  s.front = fg == c.best || GetAncestor(fg, GA_ROOTOWNER) == c.best;
  return s;
}
static std::string win_key(const WinState& s) { return !s.found ? "no window" : s.min ? "minimized" : s.front ? "in front" : "behind"; }

// CPU of the player by window state
static std::map<std::string, std::pair<double, int>> g_cpu_by_state;

// ── connections ─────────────────────────────────────────────────────
static std::map<std::string, int> g_endpoints;  // "out 1.2.3.4:443" -> times seen (new connections)
static std::set<std::string> g_live;            // connections open at the last sample
static void sample_tcp(const std::set<DWORD>& pids) {
  std::set<std::string> now;
  for (int fam : {AF_INET, AF_INET6}) {
    DWORD size = 0;
    if (GetExtendedTcpTable(nullptr, &size, FALSE, fam, TCP_TABLE_OWNER_PID_ALL, 0) != ERROR_INSUFFICIENT_BUFFER) continue;
    std::vector<BYTE> buf(size);
    if (GetExtendedTcpTable(buf.data(), &size, FALSE, fam, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) continue;
    char ip[64];
    if (fam == AF_INET) {
      auto* t = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buf.data());
      for (DWORD i = 0; i < t->dwNumEntries; ++i) {
        auto& r = t->table[i];
        if (!pids.count(r.dwOwningPid)) continue;
        if (r.dwState == MIB_TCP_STATE_LISTEN) { now.insert(fmt("listen :%u", ntohs((u_short)r.dwLocalPort))); continue; }
        in_addr a; a.S_un.S_addr = r.dwRemoteAddr;
        inet_ntop(AF_INET, &a, ip, sizeof ip);
        bool inbound = ntohs((u_short)r.dwLocalPort) == 7878;  // the phone remote
        now.insert(fmt("%s %s:%u|%u", inbound ? "in" : "out", ip, inbound ? 0 : ntohs((u_short)r.dwRemotePort), ntohs((u_short)r.dwLocalPort)));
      }
    } else {
      auto* t = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID*>(buf.data());
      for (DWORD i = 0; i < t->dwNumEntries; ++i) {
        auto& r = t->table[i];
        if (!pids.count(r.dwOwningPid)) continue;
        if (r.dwState == MIB_TCP_STATE_LISTEN) { now.insert(fmt("listen6 :%u", ntohs((u_short)r.dwLocalPort))); continue; }
        inet_ntop(AF_INET6, r.ucRemoteAddr, ip, sizeof ip);
        bool inbound = ntohs((u_short)r.dwLocalPort) == 7878;
        now.insert(fmt("%s [%s]:%u|%u", inbound ? "in" : "out", ip, inbound ? 0 : ntohs((u_short)r.dwRemotePort), ntohs((u_short)r.dwLocalPort)));
      }
    }
  }
  for (auto& c : now)
    if (!g_live.count(c)) {
      std::string key = c.substr(0, c.find('|'));
      if (g_endpoints[key]++ < 3) out_line(clock_str(time(nullptr)) + " ! tcp " + key);  // the first few of each
    }
  g_live.swap(now);
}

// ── machine and Cue facts for the summary's head ────────────────────
static std::string machine_facts() {
  std::string s;
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  OSVERSIONINFOW v{sizeof v};
  if (auto f = (RtlGetVersionFn)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")) f(&v);
  s += fmt("windows %lu.%lu build %lu; ", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
  wchar_t cpu[128] = L"?";
  DWORD sz = sizeof cpu;
  RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, cpu, &sz);
  MEMORYSTATUSEX m{sizeof m};
  GlobalMemoryStatusEx(&m);
  s += fmt("cpu %s (%d threads); ram %.1f GB; ", utf8(cpu).c_str(), g_cores, m.ullTotalPhys / 1073741824.0);
  DISPLAY_DEVICEW d{sizeof d};
  for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &d, 0); ++i)
    if (d.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) { s += "gpu " + utf8(d.DeviceString) + "; "; break; }
  DEVMODEW dm{}; dm.dmSize = sizeof dm;
  if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) s += fmt("screen %lux%lu @ %lu Hz", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency);
  return s;
}
static std::string file_version(const std::wstring& path) {
  DWORD dummy = 0, n = GetFileVersionInfoSizeW(path.c_str(), &dummy);
  if (!n) return "?";
  std::vector<BYTE> b(n);
  if (!GetFileVersionInfoW(path.c_str(), 0, n, b.data())) return "?";
  VS_FIXEDFILEINFO* fi = nullptr;
  UINT len = 0;
  if (!VerQueryValueW(b.data(), L"\\", (void**)&fi, &len) || !fi) return "?";
  return fmt("%u.%u.%u", HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS), HIWORD(fi->dwFileVersionLS));
}

// ── sampling ────────────────────────────────────────────────────────
static Gpu g_gpu;
static WinState g_win, g_win_logged, g_win_prev;
static int g_samples = 0;
static double g_last_spike_cpu = -100, g_last_spike_io = -100;
static std::vector<std::string> g_crashes;
static time_t g_start_wall;

static void sample(double t) {
  // which Cue processes exist now (a helper's parent is a Cue)
  std::map<DWORD, std::pair<DWORD, DWORD>> found;  // pid -> (parent, threads)
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W e{sizeof e};
    for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e))
      if (!_wcsicmp(e.szExeFile, L"Cue.exe")) found[e.th32ProcessID] = {e.th32ParentProcessID, e.cntThreads};
    CloseHandle(snap);
  }
  // ended
  for (auto it = g_procs.begin(); it != g_procs.end();) {
    Proc& p = it->second;
    DWORD code = STILL_ACTIVE;
    bool gone = !found.count(it->first) || (p.h && GetExitCodeProcess(p.h, &code) && code != STILL_ACTIVE);
    if (gone) {
      if (p.h) GetExitCodeProcess(p.h, &code);
      std::string line = clock_str(time(nullptr)) + fmt(" ! %s %lu ended, exit 0x%08lX after %s", p.role.c_str(), p.pid, code, dur_str(t - p.first).c_str());
      if (code >= 0xC0000000 && code != STILL_ACTIVE) { line += " (CRASH)"; g_crashes.push_back(line); }
      out_line(line);
      if (p.h) CloseHandle(p.h);
      p.h = nullptr; p.alive = false;
      g_gone.push_back(p);
      it = g_procs.erase(it);
    } else ++it;
  }
  // new
  for (auto& [pid, info] : found) {
    if (g_procs.count(pid)) continue;
    Proc p;
    p.pid = pid; p.parent = info.first; p.first = t;
    p.role = found.count(info.first) ? "helper" : "player";
    p.h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!p.h) p.h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    wchar_t path[MAX_PATH * 2];
    DWORD n = MAX_PATH * 2;
    if (p.h && QueryFullProcessImageNameW(p.h, 0, path, &n)) p.path = path;
    out_line(clock_str(time(nullptr)) + fmt(" ! %s %lu started", p.role.c_str(), pid) + (p.role == "player" ? " (Cue " + file_version(p.path) + ")" : ""));
    g_procs[pid] = p;
  }
  std::set<DWORD> pids;
  for (auto& [pid, p] : g_procs) pids.insert(pid);
  g_gpu.sample(pids);
  if (!pids.empty()) sample_tcp(pids);

  // the player's window
  DWORD player = 0;
  for (auto& [pid, p] : g_procs) if (p.role == "player") player = pid;
  WinState w = player ? find_window(player) : WinState{};
  // logged when it changed and held for two samples (a resize being dragged is one line, not thirty)
  auto same = [](const WinState& a, const WinState& b) { return a.found == b.found && a.w == b.w && a.h == b.h && a.min == b.min && a.max == b.max && a.top == b.top && a.front == b.front; };
  if (same(w, g_win_prev) && !same(w, g_win_logged) && w.found) {
    out_line(clock_str(time(nullptr)) + fmt(" ~ window %dx%d %s%s%s", w.w, w.h, w.min ? "minimized" : w.max ? "maximized" : "normal", w.top ? ", pinned" : "", w.front ? ", in front" : ", behind"));
    g_win_logged = w;
  }
  g_win_prev = w;
  g_win = w;

  for (auto& [pid, p] : g_procs) {
    if (!p.h) continue;
    PROCESS_MEMORY_COUNTERS_EX m{};
    m.cb = sizeof m;
    if (GetProcessMemoryInfo(p.h, (PROCESS_MEMORY_COUNTERS*)&m, sizeof m)) {
      p.priv = m.PrivateUsage / 1048576.0; p.ws = m.WorkingSetSize / 1048576.0; p.peak_ws = m.PeakWorkingSetSize / 1048576.0;
    }
    GetProcessHandleCount(p.h, &p.handles);
    p.gdi = GetGuiResources(p.h, GR_GDIOBJECTS);
    p.user = GetGuiResources(p.h, GR_USEROBJECTS);
    p.threads = found[pid].second;
    FILETIME c, e, k, u;
    uint64_t cpu = GetProcessTimes(p.h, &c, &e, &k, &u) ? ft64(k) + ft64(u) : 0;
    IO_COUNTERS io{};
    GetProcessIoCounters(p.h, &io);
    uint64_t faults = m.PageFaultCount;
    if (p.have_prev) {
      double dt = std::max(1e-3, t - p.t_prev);
      double core = (cpu - p.cpu_prev) / 1e7 / dt * 100.0;  // 100 = one core busy
      double gl = g_gpu.load.count(pid) ? g_gpu.load[pid] : 0, dec = g_gpu.decode.count(pid) ? g_gpu.decode[pid] : 0;
      double gm = g_gpu.memmb.count(pid) ? g_gpu.memmb[pid] : 0;
      Minute& M = p.min;
      ++M.n; M.cpu += core; M.cpu_max = std::max(M.cpu_max, core); M.gpu += gl; M.gpu_max = std::max(M.gpu_max, gl); M.dec += dec; M.gmem = gm;
      M.rops += io.ReadOperationCount - p.io_prev.ReadOperationCount; M.wops += io.WriteOperationCount - p.io_prev.WriteOperationCount;
      M.oops += io.OtherOperationCount - p.io_prev.OtherOperationCount;
      M.rb += io.ReadTransferCount - p.io_prev.ReadTransferCount; M.wb += io.WriteTransferCount - p.io_prev.WriteTransferCount;
      M.faults += faults - p.faults_prev;
      ++p.samples; p.cpu_sum += core; p.cpu_max = std::max(p.cpu_max, core); p.gpu_sum += gl; p.gpu_max = std::max(p.gpu_max, gl);
      p.dec_sum += dec; p.gmem_max = std::max(p.gmem_max, gm);
      if (p.role == "player") { auto& s = g_cpu_by_state[win_key(w)]; s.first += core; ++s.second; }
      // a spike worth a line (one a minute at most per kind)
      if (core > 150 && t - g_last_spike_cpu > 60) { g_last_spike_cpu = t; out_line(clock_str(time(nullptr)) + fmt(" ! cpu %s %.0f%% of a core (%s)", p.role.c_str(), core, win_key(w).c_str())); }
      double wmb = (io.WriteTransferCount - p.io_prev.WriteTransferCount) / 1048576.0;
      if (wmb > 20 && t - g_last_spike_io > 60) { g_last_spike_io = t; out_line(clock_str(time(nullptr)) + fmt(" ! io %s wrote %.0f MB in %.0f s", p.role.c_str(), wmb, dt)); }
    }
    p.cpu_prev = cpu; p.io_prev = io; p.faults_prev = faults; p.t_prev = t; p.have_prev = true;
    if (p.priv0 < 0) { p.priv0 = p.priv; p.h0 = p.handles; p.gdi0 = p.gdi; p.user0 = p.user; p.thr0 = p.threads; }
    p.priv_min = std::min(p.priv_min, p.priv); p.priv_max = std::max(p.priv_max, p.priv);
    p.h_max = std::max(p.h_max, p.handles); p.gdi_max = std::max(p.gdi_max, p.gdi); p.user_max = std::max(p.user_max, p.user); p.thr_max = std::max(p.thr_max, p.threads);
  }
  ++g_samples;
}

// once a minute: one line per process, then the files written that minute
static void minute_lines(double t) {
  for (auto& [pid, p] : g_procs) {
    Minute& M = p.min;
    if (M.n == 0) continue;
    out_line(fmt("%s %-6s %6.0f %6.0f %5lu %4lu %4lu %3lu %4.0f/%-4.0f %3.0f/%-3.0f %3.0f %5.0f %6llu/%-7llu %6llu/%-7llu %6llu %6llu",
                 clock_str(time(nullptr), false).c_str(), p.role.c_str(), p.priv, p.ws, p.handles, p.gdi, p.user, p.threads,
                 M.cpu / M.n, M.cpu_max, M.gpu / M.n, M.gpu_max, M.dec / M.n, M.gmem,
                 (unsigned long long)M.rops, (unsigned long long)(M.rb / 1024), (unsigned long long)M.wops, (unsigned long long)(M.wb / 1024),
                 (unsigned long long)M.oops, (unsigned long long)M.faults));
    p.rops += M.rops; p.wops += M.wops; p.oops += M.oops; p.rb += M.rb; p.wb += M.wb;
    p.tm.push_back((t - p.first) / 3600.0);
    p.s_priv.push_back(p.priv); p.s_handles.push_back(p.handles); p.s_gdi.push_back(p.gdi); p.s_user.push_back(p.user); p.s_threads.push_back(p.threads);
    M = Minute{};
  }
  std::lock_guard<std::mutex> l(g_fs_mu);
  std::string files;
  for (auto& [name, f] : g_files)
    if (f.minute_bursts) { files += (files.empty() ? "" : ", ") + utf8(name) + fmt(" x%d", f.minute_bursts); f.minute_bursts = 0; }
  if (!files.empty()) out_line("      writes: " + files);
}

// ── the summary ─────────────────────────────────────────────────────
static std::string proc_summary(const Proc& p, double now_t, std::vector<std::string>& flags) {
  double life = ((p.alive ? now_t : p.t_prev) - p.first);
  std::string s = fmt("%s pid %lu, %s%s, %d samples\n", p.role.c_str(), p.pid, dur_str(life).c_str(), p.alive ? "" : " (ended)", p.samples);
  // trends only after the first 3 minutes (opening a file is allowed to take memory)
  std::vector<double> x, pv, hv, gv, uv, tv;
  for (size_t i = 0; i < p.tm.size(); ++i)
    if (p.tm[i] * 60 >= 3) { x.push_back(p.tm[i]); pv.push_back(p.s_priv[i]); hv.push_back(p.s_handles[i]); gv.push_back(p.s_gdi[i]); uv.push_back(p.s_user[i]); tv.push_back(p.s_threads[i]); }
  double span_min = x.size() >= 2 ? (x.back() - x.front()) * 60 : 0;
  auto line = [&](const char* name, double v0, double v1, double mn, double mx, const std::vector<double>& y, const char* unit, double flag_per_h) {
    Trend tr = trend(x, y);
    std::string l = fmt("  %-8s start %8.1f  end %8.1f  min %8.1f  max %8.1f", name, v0, v1, mn, mx);
    if (x.size() >= 4) l += fmt("  trend %+8.1f %s/h (R2 %.2f, %.0f min)", tr.slope, unit, tr.r2, span_min);
    if (x.size() >= 10 && span_min >= 15 && tr.r2 >= 0.8 && tr.slope >= flag_per_h)
      flags.push_back(fmt("%s %s grows steadily: %+.1f %s/h over %.0f min (R2 %.2f): possible leak", p.role.c_str(), name, tr.slope, unit, span_min, tr.r2));
    return l + "\n";
  };
  auto mn = [](const std::vector<double>& v, double d) { return v.empty() ? d : *std::min_element(v.begin(), v.end()); };
  s += line("private", p.priv0, p.priv, p.priv_min, p.priv_max, pv, "MB", 20);
  s += line("handles", p.h0, p.handles, mn(p.s_handles, p.handles), p.h_max, hv, "", 100);
  s += line("gdi", p.gdi0, p.gdi, mn(p.s_gdi, p.gdi), p.gdi_max, gv, "", 30);
  s += line("user", p.user0, p.user, mn(p.s_user, p.user), p.user_max, uv, "", 30);
  s += line("threads", p.thr0, p.threads, mn(p.s_threads, p.threads), p.thr_max, tv, "", 10);
  if (p.samples) {
    s += fmt("  cpu      avg %.1f%%  max %.0f%% of a core;  gpu avg %.1f%%  max %.0f%%  decode avg %.1f%%  gpu mem max %.0f MB;  peak working set %.0f MB\n",
             p.cpu_sum / p.samples, p.cpu_max, p.gpu_sum / p.samples, p.gpu_max, p.dec_sum / p.samples, p.gmem_max, p.peak_ws);
    double h = std::max(1.0 / 60, life / 3600.0);
    s += fmt("  io       read %.1f MB (%llu ops), write %.1f MB (%llu ops, %.0f/h), other %llu ops (%.0f/h)\n", p.rb / 1048576.0,
             (unsigned long long)p.rops, p.wb / 1048576.0, (unsigned long long)p.wops, p.wops / h, (unsigned long long)p.oops, p.oops / h);
    if (p.role == "player" && p.cpu_sum / p.samples > 60) flags.push_back(fmt("player cpu averages %.0f%% of a core", p.cpu_sum / p.samples));
  }
  return s;
}

static void write_summary(double t, bool final_) {
  std::vector<std::string> flags;
  std::string s;
  s += "CUE MONITOR SUMMARY" + std::string(final_ ? "" : " (running; rewritten every 5 min)") + "\n";
  s += "started " + clock_str(g_start_wall) + ", " + dur_str(t) + " watched, " + std::to_string(g_samples) + " samples\n";
  s += machine_facts() + "\n";
  s += "folder watched: " + utf8(g_data_dir) + "\n";
  s += "units: memory MB; cpu % of one core (100 = one core busy); trends per hour after each process's first 3 min; R2 near 1 = steady growth\n";
  s += "io: everything the process reads and writes (the film itself, pipes to the helper, devices); disk writes in Cue's own folder are under WRITES\n\n";
  s += "PROCESSES\n";
  for (auto& p : g_gone) s += proc_summary(p, t, flags);
  for (auto& [pid, p] : g_procs) s += proc_summary(p, t, flags);
  if (g_gone.empty() && g_procs.empty()) s += "  (Cue was not seen)\n";
  s += "\nPLAYER CPU BY WINDOW STATE\n";
  for (auto& [k, v] : g_cpu_by_state) {
    double avg = v.second ? v.first / v.second : 0;
    s += fmt("  %-10s avg %5.1f%%  (%s)\n", k.c_str(), avg, dur_str(v.second * (t / std::max(1, g_samples))).c_str());
    if (k == "minimized" && v.second > 30 && avg > 15) flags.push_back(fmt("player uses %.0f%% of a core while minimized", avg));
  }
  s += "\nWRITES IN CUE'S FOLDER (a burst = a second with writes to that file)\n";
  {
    std::lock_guard<std::mutex> l(g_fs_mu);
    std::vector<std::pair<int, std::wstring>> order;
    for (auto& [n, f] : g_files) order.push_back({f.bursts + f.created + f.removed, n});
    std::sort(order.rbegin(), order.rend());
    double h = std::max(1.0 / 60, t / 3600.0);
    int total = 0;
    for (auto& [k, n] : order) {
      const FileStat& f = g_files[n];
      total += f.bursts;
      WIN32_FILE_ATTRIBUTE_DATA a{};
      std::string size = GetFileAttributesExW((g_data_dir + L"\\" + n).c_str(), GetFileExInfoStandard, &a)
                             ? fmt("%.1f KB", ((uint64_t)a.nFileSizeHigh << 32 | a.nFileSizeLow) / 1024.0) : "gone";
      s += fmt("  %-40s %5d bursts (%5.1f/h), created %d, removed %d, now %s\n", utf8(n).c_str(), f.bursts, f.bursts / h, f.created, f.removed, size.c_str());
      if (t > 600 && f.bursts / h > 60) flags.push_back(fmt("%s is written %.0f times an hour", utf8(n).c_str(), f.bursts / h));
    }
    if (order.empty()) s += "  (nothing written)\n";
    else s += fmt("  total %d bursts, %.1f/h\n", total, total / h);
  }
  s += "\nCONNECTIONS (new ones seen; the phone remote listens on 7878)\n";
  if (g_endpoints.empty()) s += "  (none)\n";
  for (auto& [k, n] : g_endpoints) s += fmt("  %-40s x%d\n", k.c_str(), n);
  s += "\nCRASHES\n";
  if (g_crashes.empty()) s += "  (none)\n";
  for (auto& c : g_crashes) { s += "  " + c + "\n"; flags.push_back("a crash: " + c); }
  s += "\nWORTH A LOOK\n";
  if (flags.empty()) s += "  nothing stood out\n";
  for (auto& f : flags) s += "  - " + f + "\n";
  write_file(g_summary, s);
}

// ── the console ─────────────────────────────────────────────────────
static BOOL WINAPI on_ctrl(DWORD) { g_quit = true; Sleep(4000); return TRUE; }  // the main loop writes the summary meanwhile

int wmain(int argc, wchar_t** argv) {
  SetConsoleOutputCP(CP_UTF8);
  double interval = 2.0, stop_after = 0;
  std::wstring out_dir;
  for (int i = 1; i + 1 < argc; ++i) {
    if (!wcscmp(argv[i], L"--interval")) interval = std::clamp(_wtof(argv[++i]), 0.5, 60.0);
    else if (!wcscmp(argv[i], L"--minutes")) stop_after = _wtof(argv[++i]) * 60;
    else if (!wcscmp(argv[i], L"--out")) out_dir = argv[++i];
  }
  // the monitor is light itself: below normal priority, one write a minute
  SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  g_cores = (int)si.dwNumberOfProcessors;
  PWSTR app = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &app))) {
    // %APPDATA%\tinypeep\Cue since 1.3 (tinyoffice\Cue before)
    g_data_dir = std::wstring(app) + L"\\tinypeep\\Cue";
    if (GetFileAttributesW(g_data_dir.c_str()) == INVALID_FILE_ATTRIBUTES) g_data_dir = std::wstring(app) + L"\\tinyoffice\\Cue";
    CoTaskMemFree(app);
  }
  if (out_dir.empty()) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    out_dir = exe;
    out_dir = out_dir.substr(0, out_dir.find_last_of(L"\\/")) + L"\\cue-monitor";
  }
  CreateDirectoryW(out_dir.c_str(), nullptr);
  if (GetFileAttributesW(out_dir.c_str()) == INVALID_FILE_ATTRIBUTES) {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    out_dir = std::wstring(tmp) + L"cue-monitor";
    CreateDirectoryW(out_dir.c_str(), nullptr);
  }
  g_start_wall = time(nullptr);
  tm lt{};
  localtime_s(&lt, &g_start_wall);
  wchar_t stamp[32];
  wcsftime(stamp, 32, L"%Y%m%d-%H%M%S", &lt);
  g_dir = out_dir;
  g_timeline = out_dir + L"\\cue-monitor-" + stamp + L".txt";
  g_summary = out_dir + L"\\cue-monitor-" + stamp + L"-summary.txt";
  g_t0 = mono();

  out_line("# cue-monitor timeline. " + machine_facts());
  out_line("# Cue's folder: " + utf8(g_data_dir));
  out_line(fmt("# every %.1f s; one line per process per minute. '!' events, '~' window changes, 'writes:' files written in Cue's folder that minute", interval));
  out_line("# time  proc     priv     ws  hndl  gdi  usr thr  cpu avg/max gpu a/m dec  gmem  read ops/KB    write ops/KB   other faults");
  out_flush();

  SetConsoleCtrlHandler(on_ctrl, TRUE);
  g_gpu.open();
  std::thread watcher;
  if (!g_data_dir.empty()) watcher = std::thread(watch_folder);

  std::printf("cue-monitor: observando o Cue (só leitura).\n");
  std::printf("Relatórios em: %s\n", utf8(out_dir).c_str());
  std::printf("Use o Cue normalmente. Para terminar, feche esta janela ou aperte Ctrl+C.\n\n");
  if (!g_gpu.ok) std::printf("(contadores de GPU indisponíveis neste Windows: o resto funciona)\n");

  double next_minute = 60, next_summary = 300;
  while (!g_quit) {
    double t = mono() - g_t0;
    sample(t);
    if (t >= next_minute) { minute_lines(t); out_flush(); next_minute += 60; }
    if (t >= next_summary) { write_summary(t, false); next_summary += 300; }
    // the live line
    std::string live = clock_str(time(nullptr)) + "  " + dur_str(t);
    for (auto& [pid, p] : g_procs)
      live += fmt("  | %s %.0f MB, %lu hnd, cpu %.0f%%", p.role.c_str(), p.priv, p.handles, p.min.n ? p.min.cpu / p.min.n : 0.0);
    if (g_procs.empty()) live += "  | esperando o Cue abrir...";
    CONSOLE_SCREEN_BUFFER_INFO ci{};
    int cols = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci) ? ci.dwSize.X - 1 : 79;
    if ((int)live.size() > cols) live.resize(std::max(0, cols));
    std::printf("\r%-*s", cols, live.c_str());
    std::fflush(stdout);
    if (stop_after > 0 && t >= stop_after) break;
    for (int i = 0; i < (int)(interval * 10) && !g_quit; ++i) Sleep(100);
  }
  g_quit = true;
  double t = mono() - g_t0;
  minute_lines(t);
  out_line(clock_str(time(nullptr)) + " # monitor stopped after " + dur_str(t));
  out_flush();
  write_summary(t, true);
  std::printf("\n\nPronto. Envie os dois arquivos de %s:\n  %s\n  %s\n", utf8(out_dir).c_str(), utf8(g_timeline.substr(out_dir.size() + 1)).c_str(),
              utf8(g_summary.substr(out_dir.size() + 1)).c_str());
  // the watcher is blocked in ReadDirectoryChangesW: the process ends without waiting for it
  if (watcher.joinable()) watcher.detach();
  if (stop_after > 0) { std::printf("Aperte Enter para fechar.\n"); std::getchar(); }
  return 0;
}
