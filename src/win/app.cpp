#include "app.h"
#include "../core/subtitle_geometry.h"
#include "../core/file_io.h"
#include "single_instance.h"
#include "video_enhance.h"
#include "../core/video_enhance.h"

#include <dwmapi.h>
#include <wincodec.h>
#include <shlobj.h>
#include <shellapi.h>
#include <windowsx.h>
#include <timeapi.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstring>
#include <set>
#include <climits>
#include <cmath>
#include <ctime>
#include <fstream>
#include <mutex>

#include "../core/sub_style.h"
#include "../core/util.h"
#include "../core/recents_view.h"
#include "../core/window_policy.h"
#include "../core/resize_hit.h"
#include "../core/blur_geometry.h"
#include "../core/video_window.h"
#include "../core/viewport_gate.h"
#include "../core/fullscreen_geometry.h"
#include "blur_shader.h"
#include "../core/audio_mode.h"
#include "../core/preview_size.h"
#include "licenses.h"
#include "ui_language.h"
#include "../core/i18n.h"
#include "../core/version.h"

namespace {
// music's room light: part of the look (the films' ambient switch and strength are the films')
constexpr double kMusicLight = 0.30;
std::string subtitle_position_string(double pos) {
  char value[32]; std::snprintf(value, sizeof value, "%.3f", pos); return value;
}
void frame_saved_sound() {
  static const std::vector<char> wav = [] {
    constexpr int rate = 22050, count = 1764;
    std::vector<char> data(44 + count * 2, 0);
    auto put16=[&](int offset, unsigned value){data[offset]=(char)value;data[offset+1]=(char)(value>>8);};
    auto put32=[&](int offset, unsigned value){for(int i=0;i<4;++i)data[offset+i]=(char)(value>>(8*i));};
    std::memcpy(data.data(), "RIFF", 4); put32(4, data.size()-8); std::memcpy(data.data()+8,"WAVEfmt ",8);
    put32(16,16);put16(20,1);put16(22,1);put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);
    std::memcpy(data.data()+36,"data",4);put32(40,count*2);
    for(int i=0;i<count;++i){double t=(double)i/rate;double envelope=std::sin(3.14159265*i/count)*std::exp(-45*t);
      short sample=(short)(900*envelope*std::sin(2*3.14159265*880*t));put16(44+i*2,(unsigned short)sample);}
    return data;
  }();
  PlaySoundA(wav.data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT | SND_NOSTOP);
}
enum Obs : uint64_t {
  O_PAUSE = 1, O_TIME, O_DUR, O_VOL, O_MUTE, O_TRACKS, O_SID, O_AID, O_SUBDELAY, O_SUBVIS, O_IDLE, O_PATH, O_SUBTEXT, O_SPEED,
  O_VIDEO_PARAMS, O_HWDEC, O_EOF, O_OSDW, O_OSDH
};
constexpr uint64_t kReplySeek = 100, kReplyShot = 101, kReplyStill = 102, kReplyOverlay = 1000, kReplyOverlayRemove = 2000;  // +layer
constexpr uint64_t kHookLoad = 200, kHookPreloaded = 201, kHookUnload = 202;

// Every key the player reacts to is routed back to us; nothing of mpv's own bindings remains.
const char* kBindings[][2] = {
    {"HOME","home"},{"END","end"},{"PGUP","pageup"},{"PGDWN","pagedown"},
    {"q","q"},{"Q","q"},{"DEL","delete"},{"Ctrl+UP","c_up"},{"Ctrl+DOWN","c_down"},
    {"Ctrl+Shift+o","cs_o"},{"Ctrl+Shift+O","cs_o"},
    {"SPACE", "space"},          {"LEFT", "left"},       {"RIGHT", "right"},           {"UP", "up"},
    {"DOWN", "down"},            {"Shift+LEFT", "s_left"}, {"Shift+RIGHT", "s_right"}, {"ENTER", "enter"},
    {"KP_ENTER", "enter"},       {"ESC", "esc"},         {"TAB", "tab"},               {"Shift+TAB", "s_tab"},
    {"m", "m"},                  {"M", "m"},             {"f", "f"},                   {"F", "f"},
    {"s", "s"},                  {"S", "s"},             {"z", "z"},                   {"Z", "z"},
    {"x", "x"},                  {"X", "x"},             {"Ctrl+o", "c_o"},            {"Ctrl+O", "c_o"},
    {"Ctrl+v", "c_v"},           {"Ctrl+V", "c_v"},      {"MBTN_LEFT", "mb_left"},     {"MBTN_RIGHT", "mb_right"},
    {"WHEEL_UP", "wheel_up"},    {"WHEEL_DOWN", "wheel_down"}, {"MBTN_LEFT_DBL", "ignore"}, {"PLAYPAUSE", "space"},
    {"PLAY", "space"},           {"PAUSE", "space"},     {"MUTE", "ignore"},                {"VOLUME_UP", "ignore"},
    {"Ctrl+=", "c_plus"},        {"Ctrl++", "c_plus"},   {"Ctrl+-", "c_minus"},        {"Ctrl+0", "c_zero"},
    {"Ctrl+KP_ADD", "c_plus"},   {"Ctrl+KP_SUBTRACT", "c_minus"}, {"Ctrl+KP0", "c_zero"},
    {"VOLUME_DOWN", "ignore"},     {"[", "lbracket"},      {"]", "rbracket"},            {"\\", "backslash"},
    // the rest of the sheet's keys, so they work after a click on the picture too (the engine's
    // window has the keyboard then): repeat, next / previous, subtitles menu, always on top, ?
    {"l", "l"}, {"L", "l"}, {"n", "n"}, {"N", "n"}, {"p", "p"}, {"P", "p"}, {"c", "c"}, {"C", "c"},
    {"Ctrl+t", "c_t"}, {"Ctrl+T", "c_t"}, {"Ctrl+m", "c_m"}, {"Ctrl+M", "c_m"}, {"?", "qmark"},
};

COLORREF cref(Rgb c) { return RGB(c.r, c.g, c.b); }

// The engine reports its failure as a catalog key (it runs before the person could change the
// language); the words are looked up when shown, so they follow a language change.
std::string engine_text(const std::string& err) {
  for (str::Id id : {str::engine_missing_dll, str::engine_too_old, str::engine_failed})
    if (err == i18n::key_name(id)) return tr(id);
  return err.empty() ? tr(str::engine_failed) : err;
}
}  // namespace

static HWND g_app_hwnd = nullptr;  // where the engine's window forwards its input
// a song's card turns its new cover in as the song starts: its pictures come at 60 a second from
// the first one, so the turn needs no head start (see audio_still_tail)
static constexpr double kFlipSettle = 0.0;

double App::now() const {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return double(t.QuadPart - qp0_.QuadPart) / double(qpf_.QuadPart);
}

// Called from the UI thread and the engine thread: one file, opened once, a line at a time.
// cue.log: what went wrong, and the few facts that explain it (version, engine, the picture's
// helpers, the mini player). Routine steps are written only with --log. Lines wait in memory and
// reach the disk together (at most every minute, or at once for a failure and at the goodbye),
// the file stays under 256 KB (the older half kept as cue.log.1) and a log older than 5 days is
// cleared when Cue starts: the person's disk is not a diary.
namespace {
std::mutex g_log_mu;
std::string g_log_buf;
std::filesystem::path g_log_path;
constexpr std::uintmax_t kLogCap = 256 * 1024;
void log_write_locked() {
  if (g_log_buf.empty() || g_log_path.empty()) return;
  std::error_code ec;
  const auto size = std::filesystem::file_size(g_log_path, ec);
  if (!ec && size + g_log_buf.size() > kLogCap) {
    auto old = g_log_path; old += ".1";
    std::filesystem::remove(old, ec);
    std::filesystem::rename(g_log_path, old, ec);
  }
  std::ofstream f(g_log_path, std::ios::app | std::ios::binary);
  f << g_log_buf;
  g_log_buf.clear();
}
// the date on a log's first line (its oldest), as days since the epoch; -1 if unreadable
long log_first_day(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  char d[11] = {};
  if (!f.read(d, 10)) return -1;
  std::tm t{};
  if (std::sscanf(d, "%4d-%2d-%2d", &t.tm_year, &t.tm_mon, &t.tm_mday) != 3) return -1;
  t.tm_year -= 1900; t.tm_mon -= 1; t.tm_hour = 12;
  std::time_t tt = std::mktime(&t);
  return tt < 0 ? -1 : (long)(tt / 86400);
}
}  // namespace

void cue_log_open(const std::filesystem::path& dir) {
  std::lock_guard<std::mutex> lock(g_log_mu);
  g_log_path = dir / "cue.log";
  const long today = (long)(std::time(nullptr) / 86400);
  for (auto p : {g_log_path, std::filesystem::path(g_log_path.wstring() + L".1")}) {
    long day = log_first_day(p);
    std::error_code ec;
    if (day >= 0 && today - day > 5) std::filesystem::remove(p, ec);
  }
}

void cue_log_flush(bool crashing) {
  if (crashing) {  // the crash handler: never wait on a lock the crashed thread may hold
    if (g_log_mu.try_lock()) { log_write_locked(); g_log_mu.unlock(); }
    return;
  }
  std::lock_guard<std::mutex> lock(g_log_mu);
  log_write_locked();
}

void App::log(const std::string& msg) {
  std::time_t t = std::time(nullptr);
  char b[32];
  std::tm tmv{};
  localtime_s(&tmv, &t);
  std::strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &tmv);
  std::lock_guard<std::mutex> lock(g_log_mu);
  g_log_buf += b;
  g_log_buf += "  +" + std::to_string((long long)(now() * 1000)) + "ms  " + msg + "\n";
  // what explains a problem is on disk at once; the rest waits for the minute's write
  static const char* urgent[] = {"fail", "error", "could not", "refused", "unavailable", "crash", "engine:", "bye", "missing"};
  bool now_ = g_log_buf.size() > 16 * 1024;
  for (const char* u : urgent) if (!now_ && msg.find(u) != std::string::npos) now_ = true;
  if (now_) log_write_locked();
}

// a routine step: only with --log (when someone is looking into something)
void App::trace(const std::string& msg) { if (engine_log_) log(msg); }

// ── startup ─────────────────────────────────────────────────────────
bool App::init(HINSTANCE inst, const std::vector<std::string>& args, int show) {
  inst_ = inst;
  QueryPerformanceFrequency(&qpf_);
  QueryPerformanceCounter(&qp0_);
  const bool moved = migrate_data_dir();  // %APPDATA%\tinyoffice\Cue becomes tinypeep\Cue, once
  data_dir_ = std::filesystem::path(data_dir());
  cue_log_open(data_dir_);
  if (!moved) log("data folder: still the old one (it was in use); moved at the next start");
  for (auto& a : args) if (a == "--log") engine_log_ = true;
  // Prepare the worker now, but map the DLL only after the first paint. DLL
  // initialization holds Windows' loader lock and can stall window creation
  // on the UI thread even when LoadLibrary itself runs on a worker.
  engine_go_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (engine_go_) engine_thread_ = CreateThread(nullptr, 0, engine_main, this, 0, nullptr);
  // Loading the engine is part of opening the file, so it keeps normal priority.
  state_path_ = data_dir_ / "state.json";
  store_.load(state_path_);  // early: the window opens at its size, with the person's settings
  apply_language(false);     // the saved choice, or Windows' display languages (read once)
  play_.in.controls_free = store_.controls_free;
  play_.in.settings_examples = store_.settings_examples;
  play_.in.mini_sub_size = store_.mini_sub_size;
  play_.in.loop = store_.loop;
  play_.in.loop_all = store_.loop_all;
  play_.in.shuffle = store_.shuffle;
  play_.in.ui_scale = store_.ui_scale;
  play_.in.ambient = store_.ambient;
  play_.in.ambient_strength = store_.ambient_strength;
  play_.set_double_click_time(GetDoubleClickTime() / 1000.0);
  log(std::string("start ") + CUE_VERSION);
  if (store_.load_status != "ok") log("settings " + store_.load_status); else trace("settings ok");
  reduce_motion() = system_reduce_motion();
  set_high_contrast(system_high_contrast());
  for (auto& a : args) if (!a.empty() && a[0] != '-') pending_open_ = a;
  // the welcome comes on a plain first start; opened straight into a file, it waits for next time
  onboarding_ = !store_.onboarded && pending_open_.empty();
  for (auto& a : args) if (a == "--welcome") onboarding_ = pending_open_.empty();
  // what's new, once per version, for people who already knew Cue; a first start gets the
  // welcome instead (it tells the same story), so that version counts as seen
  if (!store_.onboarded) store_.news_seen = store_.news_closed = CUE_NEWS_VERSION;
  home_.news = compare_versions(CUE_NEWS_VERSION, store_.news_closed) > 0;  // the line at the bottom, until closed
  news_due_ = store_.onboarded && !onboarding_ && pending_open_.empty() && compare_versions(CUE_NEWS_VERSION, store_.news_seen) > 0;
  for (auto& a : args) if (a == "--news") news_due_ = !onboarding_ && pending_open_.empty();
  // the survey: offered on the home to people who updated (kept until answered or declined, so a
  // first file opened straight away does not lose it) or a week after Cue first ran here
  {
    bool dirty = false;
    if (store_.onboarded && store_.survey.empty() && compare_versions(CUE_NEWS_VERSION, store_.news_seen) > 0) { store_.survey = "due"; dirty = true; }
    if (store_.installed <= 0) { store_.installed = (int64_t)std::time(nullptr); dirty = true; }
    for (auto& a : args) if (a == "--survey") store_.survey = "due";
    if (dirty) save_state();
  }
  ob_.ambient = store_.ambient;
  ob_.ui_scale = store_.ui_scale;
  // a version found on an earlier run is offered right away; the details come with the next check
  if (!store_.update_available.empty() && compare_versions(store_.update_available, CUE_VERSION) > 0 &&
      store_.update_available != store_.update_skipped) {
    upd_ = Upd::Available;
    upd_rel_.version = store_.update_available;
  }
  trace(std::string("version ") + CUE_VERSION);

  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = wndproc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
  wc.hIconSm = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                          GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
  wc.hbrBackground = nullptr;
  wc.lpszClassName = L"tinyoffice.Cue";
  RegisterClassExW(&wc);
  WNDCLASSEXW vc{sizeof(vc)};
  vc.lpfnWndProc = videoproc;
  vc.hInstance = inst;
  vc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  vc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  vc.lpszClassName = L"tinyoffice.Cue.video";
  RegisterClassExW(&vc);

  // size from the monitor the cursor is on: 480×270 px of art, clamped to the work area
  POINT cp;
  GetCursorPos(&cp);
  HMONITOR mon = MonitorFromPoint(cp, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(mon, &mi);
  using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
  GetDpiForMonitorFn dpi_fn = nullptr;
  if (HMODULE sh = LoadLibraryW(L"shcore.dll"))
    dpi_fn = reinterpret_cast<GetDpiForMonitorFn>(reinterpret_cast<void*>(GetProcAddress(sh, "GetDpiForMonitor")));
  auto monitor_dpi = [&](HMONITOR m) {
    UINT dx = 96, dy = 96;
    if (dpi_fn) dpi_fn(m, 0, &dx, &dy);
    return dx;
  };
  UINT mdpi = monitor_dpi(mon);
  // first run: a comfortable 16:9 about two thirds of the screen (1280x720 on a 1080p
  // screen at 100%), never smaller than 960x540 DIPs nor taller than the work area allows
  int aw = mi.rcWork.right - mi.rcWork.left, ah = mi.rcWork.bottom - mi.rcWork.top;
  double k = mdpi / 96.0;
  int ww = (int)std::lround(std::clamp(aw * 0.67, 960 * k, 1600 * k));
  int wh = ww * 9 / 16;
  if (ww > aw * 0.9) { ww = (int)(aw * 0.9); wh = ww * 9 / 16; }
  if (wh > ah * 0.84) { wh = (int)(ah * 0.84); ww = wh * 16 / 9; }
  int fw = ww, fh = wh;  // frameless: the client is the window
  int x = mi.rcWork.left + (aw - fw) / 2, y = mi.rcWork.top + (ah - fh) / 2;
  bool start_max = false;
  // afterwards: where it was left, if that place is still on a screen
  if (store_.win.has && restore_saved_window(!pending_open_.empty(), store_.win.w, store_.win.h)) {
    RECT wr{store_.win.x, store_.win.y, store_.win.x + store_.win.w, store_.win.y + store_.win.h};
    if (HMONITOR wm = MonitorFromRect(&wr, MONITOR_DEFAULTTONULL)) {
      MONITORINFO wmi{sizeof(wmi)};
      GetMonitorInfoW(wm, &wmi);
      RECT in;
      if (IntersectRect(&in, &wr, &wmi.rcWork) && in.right - in.left > 160 && in.bottom - in.top > 90) {
        // saved at another scaling? keep the same size on screen
        double rk = monitor_dpi(wm) / (double)store_.win.dpi;
        x = store_.win.x; y = store_.win.y;
        fw = std::min<int>((int)std::lround(store_.win.w * rk), wmi.rcWork.right - wmi.rcWork.left);
        fh = std::min<int>((int)std::lround(store_.win.h * rk), wmi.rcWork.bottom - wmi.rcWork.top);
        start_max = false; // home always opens as a normal horizontal window
      }
    }
  }

  hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"Cue", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, fw, fh,
                          nullptr, nullptr, inst, this);
  if (hwnd_) SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
  if (!hwnd_) { log("could not create window: " + std::to_string(GetLastError())); return false; }
  trace("window ok");
  video_ = CreateWindowExW(0, vc.lpszClassName, L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd_, nullptr, inst, this);
  SetWindowLongPtrW(video_, GWLP_USERDATA, (LONG_PTR)this);
  g_app_hwnd = hwnd_;
  dpi_ = GetDpiForWindow(hwnd_);
  if (pacer_.start(hwnd_, WM_APP_VSYNC)) trace("vsync pacer ok");
  refresh_display_rate();
  make_cursors();
  {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    s_ = scale_for(rc.right - rc.left, rc.bottom - rc.top, store_.ui_scale);
  }
  apply_theme_frame();

  drop_ = new DropTarget(hwnd_);
  drop_->on_enter = [this](bool ok, POINT p) {
    if (ok && mode_ == Mode::Play) { play_.drag_files(true, now()); ensure_anim(); return; }  // music: the list invites the drop
    if (!ok || mode_ != Mode::Home || onboarding_) return;
    home_.drag_over(p.x, p.y, now());
    set_edge_highlight(true);
    ensure_anim();
  };
  drop_->on_over = [this](POINT p) { if (mode_ == Mode::Home && !onboarding_) { home_.drag_over(p.x, p.y, now()); ensure_anim(); } };
  drop_->on_leave = [this]() {
    if (mode_ == Mode::Play && !drop_->dropping) play_.drag_files(false, now());  // a drop keeps the list open
    home_.drag_leave(now()); set_edge_highlight(false); ensure_anim();
  };
  drop_->on_drop = [this](const std::vector<std::string>& f) {
    // a video dropped on the home gets eaten on its way in
    if (mode_ == Mode::Home) for (auto& x : f) if (!is_subtitle_path(x)) { swallow_next_ = true; break; }
    if (mode_ == Mode::Play) play_.drag_dropped();  // the list stays open to show what came in
    handle_files(f);
    swallow_next_ = false;
  };
  RegisterDragDrop(hwnd_, drop_);
  scene_.create(inst);
  scene_.on_action = [this](RemoteAction a) { remote_scene_action(a); };
  scene_.on_closed = [this]() {
    remote_pair_deadline_ = 0;
    remote_.set_pairing(false, "");
    gray_ = gray_target_ = 0;
    if (gray_sent_ != 0 && mpv_ok_) { mpv_.set_double_async("saturation", 0); gray_sent_ = 0; }
    ensure_anim();
  };
  scene_.on_tick = [this](double t) {
    scene_gray(t);
    double pair_now = now();  // App deadlines use time since launch; the overlay uses absolute QPC time.
    if (remote_.running() && !scene_.view.closing() && scene_.view.waiting() &&
        remote_pair_deadline_ > 0 && pair_now >= remote_pair_deadline_) {
      remote_code_ = new_pairing_code();
      remote_pair_deadline_ = pair_now + remote::kPairLifetime;
      remote_.set_pairing(!remote_code_.empty(), remote_code_);
      scene_.view.code = remote_code_;
      scene_.view.set_link(remote_code_.empty() ? "" : "http://" + scene_.view.address + "/#c=" + remote_code_);
    }
  };
  scene_.on_leaving = [this]() {
    gray_target_ = 0;  // colour comes back with the exit
    if (scene_paused_ && mode_ == Mode::Play && loaded_) request_pause(false);
    scene_paused_ = false;
  };
  news_host_.create(inst);
  news_host_.on_action = [this](SceneAct) {
    news_host_.close(news_host_.now());
  };
  news_host_.on_closed = [this]() { ensure_anim(); };

  // everything the engine needs, copied while nobody else is using it; then it may go on
  engine_cfg_.wid = (int64_t)(intptr_t)video_;
  engine_cfg_.slang = store_.slang;
  engine_cfg_.alang = store_.alang;
  engine_cfg_.volume = store_.volume;
  engine_cfg_.muted = store_.muted;
  engine_cfg_.log = engine_log_;
  refresh_recents(false); // draw saved rows before starting filesystem checks
  show_update_state();
  if (onboarding_) { ob_.set_size(cw_, ch_, s_); ob_.start(now()); trace("welcome"); }
  SetTimer(hwnd_, T_POLL, poll_slow_ ? 250 : 33, nullptr);
  SetTimer(hwnd_, T_MAINTENANCE, 5000, nullptr);
  ShowWindow(hwnd_, start_max && show != SW_SHOWMINIMIZED && show != SW_MINIMIZE ? SW_SHOWMAXIMIZED : show);
  UpdateWindow(hwnd_);
  trace("first paint");
  // Overlap the unchanged home departure with engine/file preparation, rather
  // than adding it after the engine has already become ready.
  if (engine_thread_) SetEvent(engine_go_);
  else on_engine_ready(start_engine());
  if (!pending_open_.empty()) { home_.leave(now(), false); ensure_anim(); }
  refresh_recents();
  // the phone remote, when it was left on: shortly after the start, never in its way
  if (store_.remote_on) SetTimer(hwnd_, T_REMOTE_START, 1500, nullptr);
  if (news_due_) SetTimer(hwnd_, T_NEWS, 450, nullptr);  // after the home has settled in
  return true;
}

int App::run() {
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    if (licenses_message(&m)) continue;
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  return (int)m.wParam;
}

void App::apply_theme_frame() {
  apply_window_corners(true);
  BOOL dark = TRUE;
  DwmSetWindowAttribute(hwnd_, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof dark);
  COLORREF cap = cref(P().bg), txt = cref(P().dim), edge = 0xFFFFFFFE;  // DWMWA_COLOR_NONE: no outline
  DwmSetWindowAttribute(hwnd_, 35 /*DWMWA_CAPTION_COLOR*/, &cap, sizeof cap);
  DwmSetWindowAttribute(hwnd_, 36 /*DWMWA_TEXT_COLOR*/, &txt, sizeof txt);
  DwmSetWindowAttribute(hwnd_, 34 /*DWMWA_BORDER_COLOR*/, &edge, sizeof edge);
}

void App::apply_window_corners(bool force) {
  // The custom non-client frame can prevent DWM's automatic rounding.
  // Opt in explicitly; DWM clips the whole window, including the video child.
  // No window region or per-frame mask: retain native shadow and resize/Snap.
  DWORD preference = fullscreen_ || IsZoomed(hwnd_) ? 1 /*DWMWCP_DONOTROUND*/ : 2 /*DWMWCP_ROUND*/;
  if (!force && corner_preference_ == preference) return;
  corner_preference_ = preference; // guard reentrant frame/size messages
  // Symmetric native frame gives DWM one consistent surface to round.
  MARGINS frame = preference == 2 ? MARGINS{1,1,1,1} : MARGINS{0,0,0,0};
  DwmExtendFrameIntoClientArea(hwnd_, &frame);
  DwmSetWindowAttribute(hwnd_, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &preference, sizeof preference);
}

void App::set_edge_highlight(bool on) {
  // Omni: dragging something over the window lights the edge up to `dim`
  COLORREF edge = on ? cref(P().dim) : 0xFFFFFFFE;  // lit while dragging, otherwise no outline
  DwmSetWindowAttribute(hwnd_, 34, &edge, sizeof edge);
}

void App::write_input_conf(const std::wstring& path) {
  std::string desired = "# Generated by Cue. Every binding is handled by the app itself.\n";
  for (auto& b : kBindings) desired += std::string(b[0]) + " script-binding main/" + b[1] + "\n";
  if (file_contents_equal(std::filesystem::path(path),desired)) return;
  std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
  f << desired;
}

// Registers Cue's subtitle fonts for the current user only (no admin, no system change).
// Windows 10 1809+ lets DirectWrite, and therefore libass, see per-user fonts.
void App::install_user_fonts() {
  wchar_t* local = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return;
  std::wstring dir = std::wstring(local) + L"\\Microsoft\\Windows\\Fonts";
  CoTaskMemFree(local);
  SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
  const wchar_t* files[3] = {L"CueOmni.ttf", L"CueClean.ttf", L"CueReadable.ttf"};
  const wchar_t* names[3] = {L"Cue Omni (TrueType)", L"Cue Clean (TrueType)", L"Cue Readable (TrueType)"};
  // the same fonts from when the app was called Player: taken out, so Windows lists them once
  const wchar_t* old_files[3] = {L"PlayerOmni.ttf", L"PlayerClean.ttf", L"PlayerReadable.ttf"};
  const wchar_t* old_names[3] = {L"Player Omni (TrueType)", L"Player Clean (TrueType)", L"Player Readable (TrueType)"};
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, nullptr, 0,
                      KEY_READ | KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
  bool changed = false;
  for (int i = 0; i < 3; ++i) {
    if (RegQueryValueExW(key, old_names[i], nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
      std::wstring old = dir + L"\\" + old_files[i];
      RemoveFontResourceExW(old.c_str(), 0, nullptr);
      RegDeleteValueW(key, old_names[i]);
      DeleteFileW(old.c_str());
      changed = true;
    }
  }
  for (int i = 0; i < 3; ++i) {
    std::wstring src = exe_dir() + L"\\fonts\\" + files[i], dst = dir + L"\\" + files[i];
    WIN32_FILE_ATTRIBUTE_DATA a{}, b{};
    if (!GetFileAttributesExW(src.c_str(), GetFileExInfoStandard, &a)) continue;
    bool same = GetFileAttributesExW(dst.c_str(), GetFileExInfoStandard, &b) && a.nFileSizeLow == b.nFileSizeLow;
    if (!same) {
      if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) { log("font copy failed"); continue; }
      AddFontResourceExW(dst.c_str(), 0, nullptr);
      changed = true;
    }
    // written only when missing or different: most starts touch nothing
    wchar_t cur[MAX_PATH * 2] = {};
    DWORD sz = sizeof cur - sizeof(wchar_t), type = 0;
    bool reg_ok = RegQueryValueExW(key, names[i], nullptr, &type, (BYTE*)cur, &sz) == ERROR_SUCCESS && type == REG_SZ &&
                  dst == cur;
    if (!reg_ok)
      RegSetValueExW(key, names[i], 0, REG_SZ, (const BYTE*)dst.c_str(), (DWORD)((dst.size() + 1) * sizeof(wchar_t)));
  }
  RegCloseKey(key);
  if (changed) {
    // tells the other apps without waiting for any of them (this used to wait up to 200 ms per window)
    SendNotifyMessageW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
    log("fonts installed for this user");
  }
}

DWORD WINAPI App::engine_main(void* p) {
  auto* self = static_cast<App*>(p);
  WaitForSingleObject(self->engine_go_, INFINITE);
  // The shell is already visible. Load while its departure animation runs.
  bool loaded = self->mpv_.load(exe_dir(), &self->engine_err_);
  if (loaded) self->trace("engine DLL mapped"); else self->log("engine DLL failed");
  bool ok = loaded && self->start_engine();
  PostMessageW(self->hwnd_, WM_APP_ENGINE, ok ? 1 : 0, 0);
  return 0;
}

// Runs on the engine thread (or inline if that thread could not start). Touches only mpv_,
// engine_cfg_, engine_err_, prop_name_ and has_background_box_, which the UI thread leaves
// alone until WM_APP_ENGINE.
bool App::start_engine() {
  trace("loading engine");
  if (!mpv_.loaded() && !mpv_.load(exe_dir(), &engine_err_)) { log(engine_err_); return false; }
  if (!mpv_.create(hwnd_, WM_APP_MPV, engine_cfg_.wid, &engine_err_)) { log(engine_err_); return false; }
  std::wstring conf = data_dir_.wstring() + L"\\input.conf";
  write_input_conf(conf);
  auto o = [&](const char* k, const std::string& v) { mpv_.option(k, v); };
  // config dir = the app folder: it holds no mpv.conf, only fonts\ (older mpv reads ~~/fonts)
  o("config", "yes");
  o("config-dir", narrow(exe_dir()));
  o("terminal", "no");
  auto cloud_path = data_dir_ / "blur-cloud-1.0.hook";
  if (!file_contents_equal(cloud_path,kBlurCloudShader)) { std::ofstream shader(cloud_path, std::ios::binary); shader << kBlurCloudShader; }
  o("vo", "gpu-next");
  o("glsl-shaders", narrow(cloud_path.wstring()));
  o("hwdec", "auto-safe");  // hardware decoding when it is safe, software otherwise
  o("idle", "yes");
  o("force-window", "no");
  o("keep-open", "yes");  // the end of a film pauses on its last frame; the home is Esc or the arrow away
  o("osc", "no");
  o("load-scripts", "no");
  o("load-stats-overlay", "no");
  o("load-console", "no");
  o("load-auto-profiles", "no");
  o("save-position-on-quit", "no");
  o("ytdl", "no");
  o("input-default-bindings", "no");
  o("input-builtin-bindings", "no");
  o("input-conf", narrow(conf));
  o("input-vo-keyboard", "yes");
  o("input-cursor", "yes");
  o("input-media-keys", "yes");
  o("window-dragging", "no");
  o("cursor-autohide", "no");          // the app hides the cursor itself (works on any mpv)
  o("video-sync", "display-resample");  // draws at the display rate: the UI animates at 60 Hz+, not 24
  // the sound keeps 0.25 s ready (the engine's default is 0.2): a hiccup upstream (the picture's
  // side as Windows hides or shows the window, a busy disk) is covered by what is queued. Not
  // more: after every jump the engine fills this much (and draws the music's picture for it)
  // before it plays again (at 0.75 s, and still at 0.4, a click on the timeline lagged). Volume
  // is applied as the sound leaves (not in the queue), so it stays instant.
  o("audio-buffer", "0.25");
  // smoothing, when on: the lightest blend there is (frames mix only at their changes: no judder
  // on pans, no "soap opera" frames invented). Off until a film that needs it asks for it.
  o("tscale", "oversample");
  o("interpolation", "no");
  o("osd-level", "1");
  o("osd-bar", "no");
  o("osd-on-seek", "no");
  o("osd-playing-msg", "");
  o("hr-seek", "yes");
  // HDR on an SDR screen: tone mapping without the per-frame peak analysis (a compute pass over
  // every 4K frame); the picture stays right, the GPU does far less
  o("hdr-compute-peak", "no");
  o("audio-client-name", "Cue");
  o("sub-auto", "fuzzy");
  o("sub-file-paths", "subs;Subs;legendas;Legendas;legenda");
  o("embeddedfonts", "yes");
  o("sub-fonts-dir", narrow(exe_dir() + L"\\fonts"));          // mpv 0.37+
  // The bundled fonts are already available through ~~/fonts. Updating the
  // per-user Windows registration is maintenance, not a prerequisite for video.
  auto shader_cache = data_dir_ / "shader-cache";
  CreateDirectoryW(shader_cache.c_str(), nullptr);
  o("gpu-shader-cache-dir", narrow(shader_cache.wstring()));
  wchar_t loc[LOCALE_NAME_MAX_LENGTH] = {};
  GetUserDefaultLocaleName(loc, LOCALE_NAME_MAX_LENGTH);
  std::string lang = narrow(loc);  // "pt-BR"
  std::string base = lang.substr(0, lang.find('-'));
  // the language picked by hand last time comes first, then the system's, then the usual ones
  o("slang", (engine_cfg_.slang.empty() ? "" : engine_cfg_.slang + ",") + lang + "," + base + ",por,pob,en,eng");
  if (!engine_cfg_.alang.empty()) o("alang", engine_cfg_.alang);
  o("audio-pitch-correction", "yes");  // other speeds keep natural voices
  o("screenshot-format", "png");
  o("screenshot-png-compression", "5");
  o("screenshot-tag-colorspace", "yes");
  o("volume-max", std::to_string((int)kVolMax));
  o("volume", std::to_string((int)engine_cfg_.volume));
  o("mute", engine_cfg_.muted ? "yes" : "no");
  o("msg-level", "all=error");
  // mpv writes its log file at verbose level whatever msg-level says: every start and every file
  // opened paid for hundreds of lines on disk. Only with `Cue.exe --log` now.
  if (engine_cfg_.log) o("log-file", narrow((data_dir_ / "mpv.log").wstring()));
  trace("engine initialize");
  int r = mpv_.api().initialize(mpv_.handle());
  if (r < 0) {
    engine_err_ = i18n::key_name(str::engine_failed);
    log(std::string("mpv_initialize: ") + mpv_.api().error_string(r));
    mpv_.destroy();
    return false;
  }
  mpv_.observe("pause", MPV_FORMAT_FLAG, O_PAUSE);
  mpv_.observe("time-pos", MPV_FORMAT_DOUBLE, O_TIME);
  mpv_.observe("duration", MPV_FORMAT_DOUBLE, O_DUR);
  mpv_.observe("volume", MPV_FORMAT_DOUBLE, O_VOL);
  mpv_.observe("mute", MPV_FORMAT_FLAG, O_MUTE);
  mpv_.observe("track-list", MPV_FORMAT_NODE, O_TRACKS);
  mpv_.observe("sid", MPV_FORMAT_STRING, O_SID);
  mpv_.observe("aid", MPV_FORMAT_STRING, O_AID);
  mpv_.observe("sub-delay", MPV_FORMAT_DOUBLE, O_SUBDELAY);
  mpv_.observe("sub-visibility", MPV_FORMAT_FLAG, O_SUBVIS);
  mpv_.observe("idle-active", MPV_FORMAT_FLAG, O_IDLE);
  mpv_.observe("path", MPV_FORMAT_STRING, O_PATH);
  mpv_.observe("sub-text", MPV_FORMAT_STRING, O_SUBTEXT);
  mpv_.observe("speed", MPV_FORMAT_DOUBLE, O_SPEED);
  mpv_.observe("hwdec-current", MPV_FORMAT_STRING, O_HWDEC);
  mpv_.observe("video-out-params",MPV_FORMAT_NODE,O_VIDEO_PARAMS);
  mpv_.observe("eof-reached", MPV_FORMAT_FLAG, O_EOF);
  mpv_.observe("osd-width", MPV_FORMAT_INT64, O_OSDW);
  // the engine's warnings, only the ones about the sound or the picture falling behind, go to
  // cue.log (a dozen per file at most): what "it stuttered" looked like from the inside
  mpv_.request_log("warn");
  mpv_.observe("osd-height", MPV_FORMAT_INT64, O_OSDH);
  mpv_.hook_add("on_load", kHookLoad);
  mpv_.hook_add("on_preloaded", kHookPreloaded);
  mpv_.hook_add("on_unload", kHookUnload);
  resolve_props();
  trace("engine ok");
  return true;
}

// Back on the UI thread: from here on the engine is ours.
void App::on_engine_ready(bool ok) {
  if (engine_thread_) { CloseHandle(engine_thread_); engine_thread_ = nullptr; }
  if (closing_app_) return;
  if (!ok) {
    if (engine_err_.empty()) engine_err_ = i18n::key_name(str::engine_failed);
    engine_failed_ = true;
    home_.cancel_leave(now());
    home_.engine_problem = engine_text(engine_err_);
    if (!pending_open_.empty()) { pending_open_.clear(); home_.show_notice(home_.engine_problem, now()); }
    InvalidateRect(hwnd_, nullptr, FALSE);
    ensure_anim();
    return;
  }
  mpv_.api().set_wakeup_callback(mpv_.handle(), [](void* ctx) {
    static_cast<App*>(ctx)->wake_mpv();
  }, this);
  mpv_ok_ = true;
  play_.in.iris_engine = true;  // the opening iris is the video shader's from now on
  drain_mpv();  // whatever the engine said while it was starting
  if (!pending_open_.empty()) { auto p = pending_open_; pending_open_.clear(); open(p); }
}

void App::start_maintenance() {
  if (maintenance_started_) return;
  HANDLE th = CreateThread(nullptr, 0, [](void* p) -> DWORD {
    auto* self = static_cast<App*>(p);
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    refresh_open_with();
    self->install_user_fonts();
    if (SUCCEEDED(com)) CoUninitialize();
    return 0;
  }, this, 0, nullptr);
  if (th) { maintenance_started_ = true; CloseHandle(th); update_cleanup_async(); }
  // after the start has settled: at most one quiet look at GitHub every 3 days
  if (update_due((int64_t)std::time(nullptr), store_.update_checked)) check_updates(false);
}

// ── updates ─────────────────────────────────────────────────────────
// Always on top: for a film in a corner while working. For this session only (a window that
// stays above everything should never come back by surprise on the next start).
// on top or not, quietly (the mini player pins its window and gives the pin back after)
void App::set_pin(bool on) {
  if (pinned_ == on) return;
  pinned_ = on;
  SetWindowPos(hwnd_, pinned_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  ambient_.set_topmost(pinned_);
  ambient_.follow(hwnd_);
  home_.chrome.pinned = play_.chrome.pinned = ob_.chrome.pinned = pinned_;
}

double App::mini_aspect() const {
  if (audio_only_) return 1.9;  // a song: the card, its cover on the left, the words and the bar beside it
  if (vid_w_ > 0 && vid_h_ > 0) {
    double ar = (double)vid_w_ / vid_h_;
    if (vid_rot_ == 90 || vid_rot_ == 270) ar = 1 / ar;
    return std::clamp(ar, 0.4, 2.6);
  }
  return 16.0 / 9.0;
}

// The mini window: 400 px (at 100%) on its long side, the picture's shape, 16 px from the
// work area's edges in the given corner
// The mini player's smallest (or largest) client size, in its own shape: the long side between
// 480 and 800 px at 100% for a film (smaller, a film's picture and subtitles stop being worth
// watching), between 440 and 640 for a song's card.
SIZE App::mini_client(bool smallest) const {
  const double k = dpi_ / 96.0, ar = mini_aspect();
  const double lng = (audio_only_ ? (smallest ? 440 : 640) : (smallest ? 480 : 800)) * k;
  return ar >= 1 ? SIZE{(LONG)std::lround(lng), (LONG)std::lround(lng / ar)} : SIZE{(LONG)std::lround(lng * ar), (LONG)std::lround(lng)};
}

RECT App::mini_rect(double ar, int corner) const {
  const double k = dpi_ / 96.0;
  const int lng = (int)std::lround((audio_only_ ? 480 : 540) * k);  // big enough to watch, small enough to work beside
  int cw = ar >= 1 ? lng : (int)std::lround(lng * ar), ch = ar >= 1 ? (int)std::lround(lng / ar) : lng;
  cw = std::max(cw, (int)std::lround(200 * k)); ch = std::max(ch, (int)std::lround(150 * k));
  return mini_place(cw, ch, corner);
}

// a client of cw x ch in the given corner of the work area, 16 px from its edges
RECT App::mini_place(int cw, int ch, int corner) const {
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
  const RECT wa = mi.rcWork;
  const int m = (int)std::lround(16 * dpi_ / 96.0);
  // the window's invisible resize borders: the picture itself sits m px from the edges
  RECT wr = window_rect(), cr; GetClientRect(hwnd_, &cr);
  const int fx = (wr.right - wr.left) - cr.right, fy = (wr.bottom - wr.top) - cr.bottom;
  const int w = cw + fx, h = ch + fy;
  const int left = (corner & 1) ? wa.right - m - cw - fx / 2 : wa.left + m - fx / 2;
  const int top = (corner & 2) ? wa.bottom - m - ch - fy / 2 : wa.top + m - fy / 2;
  return {left, top, left + w, top + h};
}

int App::mini_corner() const {
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
  RECT r = window_rect();
  const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
  return (cx > (mi.rcWork.left + mi.rcWork.right) / 2 ? 1 : 0) | (cy > (mi.rcWork.top + mi.rcWork.bottom) / 2 ? 2 : 0);
}

// The next file in the mini player: its new shape, from the same corner of where the window is
// (the person put it there), kept whole on the screen.
RECT App::mini_keep_place(double ar) const {
  RECT t = mini_rect(ar, 0), wr = window_rect();
  const int w = t.right - t.left, h = t.bottom - t.top, c = mini_corner();
  int left = (c & 1) ? wr.right - w : wr.left, top = (c & 2) ? wr.bottom - h : wr.top;
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
  RECT cr; GetClientRect(hwnd_, &cr);
  const int bx = ((wr.right - wr.left) - cr.right) / 2, by = ((wr.bottom - wr.top) - cr.bottom) / 2;  // invisible borders
  left = std::clamp(left, (int)mi.rcWork.left - bx, std::max((int)mi.rcWork.left - bx, (int)mi.rcWork.right + bx - w));
  top = std::clamp(top, (int)mi.rcWork.top - by, std::max((int)mi.rcWork.top - by, (int)mi.rcWork.bottom + by - h));
  return {left, top, left + w, top + h};
}

// Let go of the mini window: it stays where the person put it, kept whole on the screen. Only
// released right by a corner (its edges a few px from both of the corner's sides, or past them)
// does it glide into that corner.
void App::mini_settle() {
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
  const RECT wa = mi.rcWork;
  RECT wr = window_rect(), cr; GetClientRect(hwnd_, &cr);
  const int bx = ((wr.right - wr.left) - cr.right) / 2, by = ((wr.bottom - wr.top) - cr.bottom) / 2;
  const int x0 = wr.left + bx, y0 = wr.top + by, x1 = x0 + cr.right, y1 = y0 + cr.bottom;  // the picture
  const int c = mini_corner();
  const int dx = (c & 1) ? wa.right - x1 : x0 - wa.left, dy = (c & 2) ? wa.bottom - y1 : y0 - wa.top;
  const int snap = (int)std::lround(40 * dpi_ / 96.0);
  if (dx < snap && dy < snap) { animate_window_to(mini_place(cr.right, cr.bottom, c)); return; }
  int ox = 0, oy = 0;
  if (x0 < wa.left) ox = wa.left - x0; else if (x1 > wa.right) ox = wa.right - x1;
  if (y0 < wa.top) oy = wa.top - y0; else if (y1 > wa.bottom) oy = wa.bottom - y1;
  if (ox || oy) animate_window_to({wr.left + ox, wr.top + oy, wr.right + ox, wr.bottom + oy});
}

namespace { double snap_px(double amount, int height); }

// The flight into the mini player (and back): the window lands on a spring with a little life,
// and on the way the picture breaks into Cue's pixels and comes back sharp just before it lands;
// the controls step aside for the flight and return with the landing. Only the geometry moves
// with Windows' animations off, for music without its picture, or over another transition.
// Hidden from view (DWM cloak: the window stays, it is just not composed) while it changes size
// for the mini player: Windows otherwise shows the last big frame squeezed into the small window
// (with its big bar and any open menu) until the engine has drawn one at the new size.
void App::set_cloak(bool on) {
  if (on == cloaked_) return;
  cloaked_ = on;
  BOOL v = on ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd_, DWMWA_CLOAK, &v, sizeof v);
}

RECT App::client_screen() const {
  RECT cr{};
  GetClientRect(hwnd_, &cr);
  POINT o{0, 0};
  ClientToScreen(hwnd_, &o);
  return {o.x, o.y, o.x + cr.right, o.y + cr.bottom};
}

// The largest rect of shape ar inside r, centred.
static RECT contain_rect(const RECT& r, double ar) {
  const int w = r.right - r.left, h = r.bottom - r.top;
  if (w <= 0 || h <= 0 || ar <= 0) return r;
  int cw = w, ch = h;
  if ((double)w / h > ar) cw = std::max(1, (int)std::lround(h * ar)); else ch = std::max(1, (int)std::lround(w / ar));
  const int x = r.left + (w - cw) / 2, y = r.top + (h - ch) / 2;
  return {x, y, x + cw, y + ch};
}

static RECT mix_rect(const RECT& a, const RECT& b, double k) {
  auto m = [&](LONG p, LONG q) { return (LONG)std::lround(p + (q - p) * k); };
  return {m(a.left, b.left), m(a.top, b.top), m(a.right, b.right), m(a.bottom, b.bottom)};
}

void App::end_mini_fx(bool apply) {
  if (!mini_fx_.on) return;
  const bool pending = apply && mini_fx_.stage == 0;  // still on the old size: the switch happens now
  mini_fx_.on = false;
  end_snap();
  set_cloak(false);
  ghost_.end();
  play_.end_viewport_transition(now());
  if (pending) { geom_.on = false; mini_switch(mini_fx_.entering); }
  play_.changed(now());
}

bool App::start_mini_fx(bool entering) {
  end_mini_fx();  // a flight already under way (in and straight back out): it ends here
  if (reduce_motion() || !mpv_ok_ || !loaded_ || (audio_only_ && !waveform_active_) || fullscreen_transition_ || audio_switch_.stage || snap_.on) return false;
  mini_fx_ = MiniFx{};
  mini_fx_.on = true;
  mini_fx_.entering = entering;
  mini_fx_.t0 = now();
  // (the picture breaks into pixels where it is, then flies: shrinking into the corner on a spring
  // with a little life, or opening out to the old place and size)
  mini_fx_.rise = 0.12;
  mini_fx_.dur = entering ? 0.46 : 0.40;
  snap_subtitles(true);  // drawn after the mosaic: they step out with the controls
  play_.begin_viewport_transition(now(), true);
  ensure_anim();
  return true;
}

// The mini player on or off and the window at its new size (out of sight, during a flight).
void App::mini_switch(bool entering) {
  if (entering) {
    mini_player_ = play_.in.mini_player = true;
    if (IsZoomed(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    // on top only if the person pinned it (the pin in its corner; remembered for next time)
    set_pin(store_.mini_pinned);
    // no maximize while small: Windows' drag-to-the-top would blow the mini window up
    SetWindowLongPtrW(hwnd_, GWL_STYLE, GetWindowLongPtrW(hwnd_, GWL_STYLE) & ~(LONG_PTR)WS_MAXIMIZEBOX);
    mini_drops_ = mpv_ok_ ? mpv_.get_double("frame-drop-count", 0) + mpv_.get_double("vo-delayed-frame-count", 0) : 0;
    log("mini player on");
    play_.changed(now());
    play_.resize(cw_, ch_, s_);  // the bar becomes the mini player's at once, before the window shrinks
    // in flight: the mini player's size where the window was (centred on it), then it flies into
    // its corner; otherwise straight into the corner
    const RECT to = mini_rect(mini_aspect(), 3), from = window_rect();
    const int w = to.right - to.left, h = to.bottom - to.top;
    const int cx = (from.left + from.right) / 2, cy = (from.top + from.bottom) / 2;
    if (mini_fx_.on && !mini_fx_.ghost) {
      SetWindowPos(hwnd_, nullptr, cx - w / 2, cy - h / 2, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
      mini_fx_.to = to;
    } else {
      SetWindowPos(hwnd_, nullptr, to.left, to.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
  } else {
    end_card_flip(true);  // a song on its way in the card still opens; the cover lands flat
    mini_player_ = play_.in.mini_player = false;
    // the card's covers (this song's and its neighbours', read ahead at the card's size) go with it
    card_art_.reset(); ahead_next_ = {}; ahead_prev_ = {}; card_art_path_.clear();
    play_.in.card_art.reset(); play_.in.card_art_show = false;
    mini_drag_ = false;
    SetWindowLongPtrW(hwnd_, GWL_STYLE, GetWindowLongPtrW(hwnd_, GWL_STYLE) | WS_MAXIMIZEBOX);
    set_pin(mini_was_pinned_);
    geom_.on = false;
    if (mpv_ok_ && loaded_ && !audio_only_) {  // how the picture kept up while small (cue.log)
      double d = mpv_.get_double("frame-drop-count", 0) + mpv_.get_double("vo-delayed-frame-count", 0) - mini_drops_;
      log("mini player off (" + std::to_string((long long)std::max(0.0, d)) + " late or dropped frames)");
    } else log("mini player off");
    SetWindowPlacement(hwnd_, &mini_restore_);  // its old place and size (maximized again, if it was)
    play_.changed(now());
    play_.resize(cw_, ch_, s_);
  }
}

bool App::step_mini_fx(double t) {
  if (!mini_fx_.on) return false;
  if (in_sizemove_) { end_mini_fx(); return true; }  // the person took the window: it ends now
  if (mini_fx_.stage == 0) {  // the picture breaks up where it is
    const double k = (t - mini_fx_.t0) / std::max(0.01, mini_fx_.rise);
    const double a = smoothstep(0.0, 1.0, std::min(1.0, k));
    set_snap(0.12 * a, snap_px(0.6 * a, ch_), 0.0);
    if (k < 1) return true;
    // The ghost: the window's live picture where the window is (for the way in, the part the
    // mini player keeps: the picture in the mini player's shape), shown over it; the window goes
    // out of sight and takes its new size and place at once; the picture flies there.
    {
      const RECT here = client_screen();
      const RECT from = mini_fx_.entering ? contain_rect(here, mini_aspect()) : here;
      if (ghost_.begin(inst_, hwnd_, from)) {
        set_cloak(true);
        mini_fx_.cloaked = true;
        mini_fx_.ghost = true;
        geom_.on = false;
        mini_switch(mini_fx_.entering);
        mini_fx_.from = from;
        mini_fx_.gto = client_screen();
        ghost_.place(from);  // (the window changed shape: the picture is cropped to the ghost's again)
        mini_fx_.stage = 2;
        mini_fx_.t0 = t;
        return true;
      }
    }
    // out of sight for 80 ms while the window changes size: the engine draws at the new size
    // (two or three frames, even in 4K) before anyone sees it
    set_cloak(true);
    mini_fx_.cloaked = true;
    geom_.on = false;
    mini_switch(mini_fx_.entering);
    set_snap(0.12, snap_px(0.6, ch_), 0.0);
    mini_fx_.stage = 1;
    mini_fx_.t0 = t;
    return true;
  }
  if (mini_fx_.stage == 1) {
    if (t - mini_fx_.t0 < mini_fx_.hold) return true;
    set_cloak(false);
    mini_fx_.cloaked = false;
    mini_fx_.stage = 2;
    mini_fx_.t0 = t;
    if (mini_fx_.entering) {  // into the corner, on a spring, only moving (its size is already right)
      animate_window_to(mini_fx_.to);
      if (geom_.on) geom_.kind = 2;
      geom_.dur = mini_fx_.dur;
    }
  }
  if (mini_fx_.ghost) {
    if (mini_fx_.stage == 3) {  // landed: the window was shown last frame, the picture goes now
      ghost_.end();
      mini_fx_.on = false;
      play_.end_viewport_transition(now());
      play_.changed(now());
      return true;
    }
    const double k = clamp01((t - mini_fx_.t0) / std::max(0.01, mini_fx_.dur));
    double e;
    if (mini_fx_.entering) {  // into the corner: quick, a hair past its place, settling (ease out, back)
      const double c1 = 0.9, c3 = c1 + 1, x = k - 1;
      e = 1 + c3 * x * x * x + c1 * x * x;
    } else {  // out to the old place (often the whole screen): opening, no overshoot past it
      e = 1 - std::pow(1 - k, 3);
    }
    ghost_.place(mix_rect(mini_fx_.from, mini_fx_.gto, e));
    const double px = 1 - smoothstep(0.0, 0.9, k);  // sharp just before it lands
    set_snap(0.12 * px, snap_px(0.6 * px, ch_), 0.0);
    if (k >= 1) {
      ghost_.place(mini_fx_.gto);
      end_snap();
      set_cloak(false);  // the window shows where the picture is; the picture goes next frame
      mini_fx_.cloaked = false;
      mini_fx_.stage = 3;
    }
    return true;
  }
  const double k = (t - mini_fx_.t0) / std::max(0.01, mini_fx_.dur);
  if (k >= 1) {
    mini_fx_.on = false;
    end_snap();
    set_cloak(false);
    play_.end_viewport_transition(now());
    play_.changed(now());
    return true;
  }
  const double e = 1 - smoothstep(0.0, 0.9, k);  // sharp just before it lands
  set_snap(0.12 * e, snap_px(0.6 * e, ch_), 0.0);
  return true;
}

void App::enter_mini() {
  if (mini_player_ || mode_ != Mode::Play || fullscreen_ || fullscreen_transition_) return;
  if (mini_fx_.on && mini_fx_.stage < 2) return;  // already on its way
  mini_restore_ = WINDOWPLACEMENT{sizeof(WINDOWPLACEMENT)};
  if (!GetWindowPlacement(hwnd_, &mini_restore_)) return;
  mini_was_pinned_ = pinned_;
  // The flight: the picture breaks into pixels where it is, the window takes the mini player's
  // size once, out of sight, centred on where it was, and then only moves into its corner on a
  // spring as the picture comes sharp. Changing its size on every frame made the engine draw a
  // frame behind the window each time (black edges, a picture cut off), worse with a 4K film.
  if (!start_mini_fx(true)) mini_switch(true);
  ensure_anim();
}

void App::exit_mini(bool animate) {
  if (mini_fx_.on && mini_fx_.stage < 2) {  // on its way (in or out), the window not settled yet
    if (animate) return;
    // at once (home): a way in still at the old size is dropped, a way out finishes
    end_mini_fx(!(mini_fx_.entering && mini_fx_.stage == 0));
  }
  if (!mini_player_) return;
  // back: the pixels rise as the small window drifts toward its old place, the window opens up
  // out of sight, and the picture comes back sharp out of the pixels (maximized too)
  if (!animate || !start_mini_fx(false)) { end_mini_fx(); mini_switch(false); }
  ensure_anim();
}

void App::toggle_pin() {
  pinned_ = !pinned_;
  if (mini_player_ && store_.mini_pinned != pinned_) { store_.mini_pinned = pinned_; save_state(); }  // the mini player's own pin
  SetWindowPos(hwnd_, pinned_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  ambient_.set_topmost(pinned_);
  ambient_.follow(hwnd_);
  home_.chrome.pinned = play_.chrome.pinned = ob_.chrome.pinned = pinned_;
  say(tr(pinned_ ? str::win_pinned : str::win_unpinned));
  play_.changed(now());
  ensure_anim();
}

// The size, named, where the person is looking (the shortcuts change it without a menu).
void App::say_ui_size() {
  static const str::Id names[kUiSizes] = {str::size_small, str::size_medium, str::size_large, str::size_larger};
  say(i18n::format(str::size_now, {{"size", tr(names[ui_size_index(store_.ui_scale)])}}));
}

void App::say(const std::string& text) {
  double t = now();
  if (mode_ == Mode::Play) play_.toast(text, nullptr, t);
  else home_.show_notice(text, t);
  ensure_anim();
}

void App::show_update_state() {
  bool known = upd_ == Upd::Available || upd_ == Upd::Downloading || upd_ == Upd::Launching ||
               (upd_ == Upd::Checking && upd_install_after_check_);
  std::string v = short_version(upd_rel_.version);
  home_.update_version = known ? v : "";
  home_.update_busy = upd_ == Upd::Downloading || upd_ == Upd::Launching || upd_ == Upd::Checking;
  home_.update_progress = upd_ == Upd::Downloading && upd_pct_ >= 0 ? upd_pct_ / 100.0 : -1;
  play_.in.update_ready = known;
  switch (upd_) {
    case Upd::Idle: play_.in.update_label = tr(str::update_check); break;
    case Upd::Checking: play_.in.update_label = tr(str::update_checking); break;
    case Upd::Available: play_.in.update_label = i18n::format(str::update_to, {{"version", v}}); break;
    case Upd::Downloading:
      play_.in.update_label = i18n::format(str::update_downloading, {{"version", v}, {"percent", std::to_string(std::max(0, upd_pct_))}});
      break;
    case Upd::Launching: play_.in.update_label = tr(str::update_launching); break;
  }
  play_.changed(now());
  if (mode_ == Mode::Home && !onboarding_) InvalidateRect(hwnd_, nullptr, FALSE);
  ensure_anim();
}

void App::check_updates(bool manual) {
  if(!CUE_OFFICIAL_BUILD) {if(manual)say(tr(str::update_unofficial));return;}
  if(!manual&&update_auto_attempted_)return;
  if (update_check_pending_ || upd_ == Upd::Checking || upd_ == Upd::Downloading || upd_ == Upd::Launching) return;
  update_check_pending_=true;
  if(!manual)update_auto_attempted_=true;
  upd_manual_ = manual;
  Upd before = upd_;
  upd_ = Upd::Checking;
  if (before == Upd::Available && !manual) upd_ = Upd::Available;  // a quiet refresh keeps the offer on screen
  log(manual ? "update check (asked)" : "update check");
  update_check_async(hwnd_, WM_APP_UPDATE, manual);
  show_update_state();
}

void App::start_update() {
  if(!CUE_OFFICIAL_BUILD) {say(tr(str::update_unofficial));return;}
  if(update_check_pending_) {upd_install_after_check_=true;return;}
  if (upd_ == Upd::Downloading || upd_ == Upd::Launching) return;
  if (upd_rel_.asset_url.empty()) {  // only the version is known: ask GitHub for the file first
    upd_install_after_check_ = true;
    upd_ = Upd::Idle;
    check_updates(true);
    return;
  }
  upd_ = Upd::Downloading;
  upd_pct_ = 0;
  log("update download " + upd_rel_.asset_name);
  update_download_async(hwnd_, WM_APP_UPDATE, upd_rel_);
  show_update_state();
}

void App::on_update(UpdateMsg* m) {
  if(m->kind==UpdateMsg::Checked||m->kind==UpdateMsg::Failed)update_check_pending_=false;
  switch (m->kind) {
    case UpdateMsg::Checked: {
      store_.update_checked = (int64_t)std::time(nullptr);
      bool newer = m->found && compare_versions(m->rel.version, CUE_VERSION) > 0;
      log("update check: " + (m->found ? "latest " + m->rel.version : m->error));
      if (newer) {
        upd_rel_ = m->rel;
        store_.update_available = m->rel.version;
        bool skipped = m->rel.version == store_.update_skipped && !m->manual;
        upd_ = skipped ? Upd::Idle : Upd::Available;
        save_state();
        if (upd_install_after_check_) { upd_install_after_check_ = false; start_update(); break; }
        if (m->manual) say(i18n::format(str::update_available, {{"version", short_version(m->rel.version)}}));
      } else {
        upd_ = Upd::Idle;
        upd_install_after_check_ = false;
        store_.update_available.clear();
        save_state();
        if (m->manual) say(tr(m->found ? str::update_latest : str::update_no_installer));
      }
      break;
    }
    case UpdateMsg::Progress:
      if (upd_ == Upd::Downloading) upd_pct_ = m->percent;
      break;
    case UpdateMsg::Downloaded:
      log("update downloaded, " + m->error);
      upd_ = Upd::Launching;
      show_update_state();
      save_position();
      if (update_launch_installer(m->file, exe_dir())) {
        log("installer started");
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);  // it waits for us to close, then installs and reopens Cue
      } else {
        upd_ = Upd::Available;
        DeleteFileW(m->file.c_str());
        RemoveDirectoryW(std::filesystem::path(m->file).parent_path().c_str());
        say(tr(str::update_launch_failed));
      }
      break;
    case UpdateMsg::Failed:
      log("update failed: " + m->error);
      if (upd_ == Upd::Downloading) { upd_ = Upd::Available; say(tr(str::update_download_failed)); }
      else if (upd_ == Upd::Checking || upd_install_after_check_) {
        upd_ = upd_rel_.version.empty() ? Upd::Idle : Upd::Available;
        if (m->manual || upd_install_after_check_) {
          // say what actually happened: no release yet is not the same as no internet
          const std::string& e = m->error;
          if (e.find("http 404") != std::string::npos) say(tr(str::update_none_published));
          else if (e.find("http 403") != std::string::npos || e.find("http 429") != std::string::npos)
            say(tr(str::update_rate_limited));
          else if (e.find("http ") != std::string::npos) say(tr(str::update_bad_response));
          else say(tr(str::update_offline));
        }
        upd_install_after_check_ = false;
      }
      break;
  }
  show_update_state();
}



// ── mpv events ──────────────────────────────────────────────────────
void App::wake_mpv() {
  if (!mpv_wake_pending_.exchange(true))
    if (!PostMessageW(hwnd_, WM_APP_MPV, 0, 0)) mpv_wake_pending_ = false;
}

void App::drain_mpv() {
  // Clear before draining so a concurrent wake cannot be lost.
  mpv_wake_pending_ = false;
  if (!mpv_ok_ || !mpv_.ready()) return;  // not before the engine thread hands it over
  bool touched = false, overlay_ack = false;
  int processed = 0;
  while (processed++ < 128) {
    mpv_event* e = mpv_.wait();
    if (!e || e->event_id == MPV_EVENT_NONE) break;
    switch (e->event_id) {
      case MPV_EVENT_GET_PROPERTY_REPLY:
        if(e->reply_userdata==media_geometry_.current_request()) {
          bool changed=geometry_query_dirty_;
          geometry_query_dirty_=false;
          video_geometry_reply(e);
          if(changed) request_video_geometry(); // do not lose a reconfiguration received during this query
        }
        break;
      case MPV_EVENT_VIDEO_RECONFIG:
        request_video_geometry();
        break;
      case MPV_EVENT_SET_PROPERTY_REPLY:
        if (e->reply_userdata == pause_request_ && pause_pending_) {
          pause_pending_=false;
          if(e->error>=0) play_.in.paused=pause_target_;
          play_.changed(now()); touched=true;
        }
        break;
      case MPV_EVENT_PROPERTY_CHANGE:
        on_property(e->reply_userdata, (mpv_event_property*)e->data);
        // Keep the clock current, but a hidden toolbar needs no text/layout work.
        touched |= e->reply_userdata != O_TIME || (!hidden_now() && play_.needs_frame());
        break;
      case MPV_EVENT_CLIENT_MESSAGE: on_client_message((mpv_event_client_message*)e->data); break;
      case MPV_EVENT_FILE_LOADED: on_file_loaded(); break;
      case MPV_EVENT_END_FILE: on_end_file((mpv_event_end_file*)e->data); break;
      case MPV_EVENT_HOOK: {
        auto* h = (mpv_event_hook*)e->data;
        if (e->reply_userdata == kHookPreloaded) {
          configure_audio_mode();
          mpv_.hook_continue(h->id);
          break;
        }
        if (e->reply_userdata == kHookUnload) {
          // Unhook before the engine destroys/recreates its output, including
          // video -> audio replacements and failures before FILE_LOADED.
          release_video_window();
          mpv_.hook_continue(h->id);
          break;
        }
        // resume naturally: the start position is set before the file even opens
        play_.in.sub_selection.reset();
        sid_reselect_ = 0;
        std::string p = mpv_.get_string("path");
        // music starts from its beginning: a song is not a film to come back to halfway
        double rp = store_.resume && !remote::is_audio_name(p) ? store_.resume_point(p) : -1;
        resume_toast_ = rp;
        if (rp > 0) mpv_.set("file-local-options/start", std::to_string(rp));
        // what the person chose for this file last time: subtitle delay and visibility
        if (const RecentItem* r = store_.find(p)) {
          if (std::fabs(r->sub_delay) > 0.001) mpv_.set("file-local-options/sub-delay", std::to_string(r->sub_delay));
          mpv_.set("sub-visibility", r->sub_visible ? "yes" : "no");
        } else {
          mpv_.set("sub-visibility", "yes");
        }
        tracks_applied_ = false;
        if (music_reopening_) {
          // the same song again, for a new look: it continues where it was, and stays still
          // until the transition lets it go
          mpv_.set("file-local-options/start", std::to_string(std::max(0.0, audio_switch_.position)));
          mpv_.set("pause", "yes");
        } else mpv_.set("pause", "no");  // a new file always plays, even if the last one ended paused
        mpv_.set("loop-file", store_.loop ? "inf" : "no");
        eof_ = false;
        play_.in.ended = false;
        mpv_.hook_continue(h->id);
        break;
      }
      case MPV_EVENT_COMMAND_REPLY:
        if (e->reply_userdata == 103) {
          cloud_inflight_ = false;
          if (e->error < 0) cloud_options_.clear();
          if (cloud_pending_) { update_blur_cloud(); ensure_anim(); }
          break;
        }
        if (SubtitleSelection::is_reply(e->reply_userdata)) {
          subtitle_selection_reply(e->reply_userdata, e->error);
          touched = true;
          break;
        }
        if (e->reply_userdata >= kReplyOverlay && e->reply_userdata < kReplyOverlay + PlayView::L_COUNT) {
          Ov& o = ov_[e->reply_userdata - kReplyOverlay];
          o.added(e->error >= 0);
          overlay_ack = true;
        }
        if (e->reply_userdata >= kReplyOverlayRemove && e->reply_userdata < kReplyOverlayRemove + PlayView::L_COUNT) {
          int i = (int)(e->reply_userdata - kReplyOverlayRemove);
          ov_[i].removed(e->error >= 0);
          // It may have become visible again while removal was queued.
          play_.layers[i].dirty = e->error >= 0 || play_.layers[i].visible;
          overlay_ack = true;
        }
        if (e->reply_userdata == kReplyShot) {
          if (e->error >= 0) { play_.toast_folder(tr(str::toast_frame_saved), now()); frame_saved_sound(); }  // with a button to its folder
          else { last_frame_.clear(); play_.toast(tr(str::toast_frame_failed), nullptr, now()); }
          ensure_anim();
        }
        if (e->reply_userdata == kReplyStill) {
          // the film's still for Windows' media card: a small copy now (the engine frees its
          // frame when this event returns), the check and the JPEG on a worker
          bool asked = false;
          auto* r = static_cast<mpv_event_command*>(e->data);
          if (e->error >= 0 && r && r->result.format == MPV_FORMAT_NODE_MAP && r->result.u.list && !audio_only_ && loaded_) {
            int64_t w = 0, h = 0, stride = 0;
            const mpv_byte_array* data = nullptr;
            const mpv_node_list* m = r->result.u.list;
            for (int i = 0; i < m->num; ++i) {
              const std::string k = m->keys[i] ? m->keys[i] : "";
              const mpv_node& v = m->values[i];
              if (k == "w" && v.format == MPV_FORMAT_INT64) w = v.u.int64;
              else if (k == "h" && v.format == MPV_FORMAT_INT64) h = v.u.int64;
              else if (k == "stride" && v.format == MPV_FORMAT_INT64) stride = v.u.int64;
              else if (k == "data" && v.format == MPV_FORMAT_BYTE_ARRAY) data = static_cast<const mpv_byte_array*>(v.u.ba);
              else if (k == "format" && v.format == MPV_FORMAT_STRING && v.u.string && std::string(v.u.string) != "bgr0") data = nullptr, w = 0;
            }
            if (data && data->data && w > 1 && h > 1 && w <= 16384 && h <= 16384 && stride >= w * 4 && data->size >= (size_t)(stride * h)) {
              int sw = 0, sh = 0;
              auto px = shrink_bgr0(static_cast<const uint8_t*>(data->data), (int)w, (int)h, (int)stride, 480, 480, sw, sh);
              media_still_job_ = request_card_still(std::move(px), sw, sh);
              asked = !!media_still_job_;
            }
          }
          if (!asked && !audio_only_ && ++media_still_tries_ < 3) media_still_due_ = now() + 20;
        }
        if (e->reply_userdata == kReplySeek) {
          seek_in_flight_ = false;
          if (seek_pending_ >= 0) {
            double t = seek_pending_;
            seek_pending_ = -1;
            seek_in_flight_ = true;
            mpv_.cmd({"seek", std::to_string(t), "absolute+keyframes"}, kReplySeek);
          }
        }
        break;
      case MPV_EVENT_PLAYBACK_RESTART:
        // how long a file took to play, once (a song too: a film says it at its first frame)
        if (loaded_ && audio_only_ && !open_timed_) { open_timed_ = true; log("song plays (open " + std::to_string((int)((now() - open_started_) * 1000)) + "ms)"); }
        if (seek_fade_.stage == 2) { seek_fade_.stage = 3; seek_fade_.t0 = now(); ensure_anim(); }  // the new place: breathe back in
        audio_switch_restart();
        // the card: the new cover turns in, a beat after the song starts (the engine holds its
        // redraws for 0.1 s after a start: the turn's quick first part was lost there, a jump)
        if (turn_on_ && flip_.stage >= 2 && loaded_) { if (turn_restart_t_ < 0) turn_restart_t_ = now(); ensure_anim(); }
        else if (flip_.stage == 2 && loaded_) { flip_.stage = 3; flip_.t0 = now() + kFlipSettle; ensure_anim(); }
        if (wing_hold_ && loaded_ && wing_hold_t_ <= 0) wing_hold_t_ = now() + 0.2;  // its own picture is about to be up
        if (music_intro_ && hidden_now()) { music_intro_ = false; end_snap(); }  // nobody to show it to
        if (music_intro_ && !audio_switch_.stage) {
          // the new song resolves from big pixels to its picture, the way a track change reads
          music_intro_ = false;
          audio_switch_ = {};
          audio_switch_.stage = 5; audio_switch_.at = now();
          audio_switch_.paused = pause_pending_ ? pause_target_ : play_.in.paused;
          ensure_anim();
        }
        if(!media_geometry_.shape().valid()) request_video_geometry();
        if (loaded_ && !first_frame_) {
          log("first video frame (open " + std::to_string((int)((now() - open_started_) * 1000)) + "ms)");
          first_frame_ = true;
          background_warmup_.first_frame(now());
          try_start_iris();
        }
        break;
      case MPV_EVENT_SHUTDOWN: mpv_ok_ = false; break;
      case MPV_EVENT_LOG_MESSAGE: {
        auto* m = static_cast<mpv_event_log_message*>(e->data);
        if (!m || !m->text || engine_warns_ >= 12) break;
        std::string t = m->text;
        if (t.find("underrun") == std::string::npos && t.find("esynchron") == std::string::npos && t.find("too slow") == std::string::npos) break;
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
        ++engine_warns_;
        log(std::string("engine: ") + (m->prefix ? m->prefix : "") + ": " + t + (mini_player_ ? " (mini player)" : ""));
        break;
      }
      default: break;
    }
  }
  if (mode_ == Mode::Play && next_advance_.return_home(media_idle_, loaded_)) go_home();
  if (processed > 128) wake_mpv(); // return to input between large event batches
  // a layer that changed while its previous picture was in flight goes out now
  if (overlay_ack) {
    upload_overlays();
    if (mode_ == Mode::Play)
      for (auto& L : play_.layers) if (L.dirty) { ensure_anim(); break; }
  }
  // replies to our own async writes change nothing on screen: skip them
  if (mode_ == Mode::Play && touched && !hidden_now()) {
    if (!next_ep_.empty() && !next_cancelled_ && !store_.loop && remote_timer_ != -1 && !scene_.visible() && play_.in.dur > 10) {
      double left = play_.in.dur - play_.in.time;
      double wall_left = left / std::max(.1, play_.in.speed);
      int seconds = wall_left <= 10 ? std::max(0, (int)std::ceil(wall_left)) : -1;
      if (seconds != next_count_ && !audio_only_) {  // music says it at the top instead
        next_count_ = seconds;
        play_.next_notice(seconds < 0 ? "" : (next_auto_ ? std::string(tr(str::player_up_next)) + " " + std::to_string(seconds) + "s" : std::string(tr(str::player_up_next_manual))), now());
      }
    }
    play_.changed(now());
    run_actions();
    // time-pos changes on every video frame: with nothing on screen there is nothing to draw
    if (play_.needs_frame()) ensure_anim();
  }
}

void App::on_property(uint64_t id, const mpv_event_property* p) {
  auto dbl = [&](double def) { return (p->format == MPV_FORMAT_DOUBLE && p->data) ? *(double*)p->data : def; };
  auto flag = [&](bool def) { return (p->format == MPV_FORMAT_FLAG && p->data) ? *(int*)p->data != 0 : def; };
  auto str = [&]() { return (p->format == MPV_FORMAT_STRING && p->data && *(char**)p->data) ? std::string(*(char**)p->data) : std::string(); };
  PlayInput& in = play_.in;
  switch (id) {
    case O_PAUSE: if (!pause_pending_ && !audio_switch_.stage) in.paused = flag(false); if (in.paused) save_position(); break;
    case O_OSDW: case O_OSDH:
      if (p->format == MPV_FORMAT_INT64 && p->data) (id==O_OSDW?osd_w_:osd_h_)=(int)*(int64_t*)p->data;
      break;
    case O_TIME:
      if (audio_switch_.stage && audio_switch_.stage < 5) break;
      if (seek_hold_ >= 0) {
        double v = dbl(in.time);
        // the old place, or a zero while the picture restarts: not where the bar was sent
        if (now() < seek_hold_until_ && std::fabs(v - seek_hold_) > 0.75) break;
        seek_hold_ = -1;
      }
      in.time = dbl(in.time);
      last_time_ = in.time;
      if (what_until_ >= 0 && in.time >= what_until_ + 0.4) end_what(true);
      // A replacement can coalesce playback-restart notifications. A valid
      // advancing media clock after loading also releases the warmup gate.
      if (loaded_ && !first_frame_ && !in.paused && now() - loaded_at_ > 1.2 &&
          p->format == MPV_FORMAT_DOUBLE && p->data) {
        first_frame_ = true;
        background_warmup_.first_frame(now());
        // a file that does not say its frame rate: the decoder's own measure, once playing
        if (!audio_only_ && video_fps_ <= 0) { video_fps_ = mpv_.get_double("container-fps", 0); if (video_fps_ <= 0) video_fps_ = mpv_.get_double("estimated-vf-fps", 0); }
        if (!audio_only_) apply_smooth();
        try_start_iris();
      }
      break;
    case O_DUR:
      in.dur = dbl(0);
      if (loaded_ && in.dur > 0) store_.set_position(path_, last_time_, in.dur);
      break;
    case O_VOL: in.volume = dbl(in.volume); store_.volume = in.volume; break;
    case O_MUTE: in.muted = flag(false); store_.muted = in.muted; break;
    case O_SUBDELAY:
      in.sub_delay = dbl(0);
      if (loaded_) if (RecentItem* r = store_.find(path_)) { r->sub_delay = in.sub_delay; save_state(); }
      break;
    case O_SUBVIS:
      if (snap_.subs_hidden) break;  // the snap hid them for a moment; the person's choice stands
      in.sub_visible = flag(true);
      if (what_until_ >= 0 || now() - what_restored_at_ < 1.5) break;  // shown for a moment by the remote: not a choice
      if (loaded_ && now() - loaded_at_ > 1.0) if (RecentItem* r = store_.find(path_)) { r->sub_visible = in.sub_visible; save_state(); }
      break;
    case O_SID:
    case O_AID: {
      std::string v = str();
      int64_t idv = (v.empty() || v == "no") ? 0 : std::atoll(v.c_str());
      // while tracks play together the engine reports none selected: aid keeps the last single
      // track, the one to return to when the mix ends; the "no" of a forced reselection is not a
      // choice either (it must never be remembered as "no audio" for the file)
      if (id == O_AID && (amix_ || (aid_reselect_ && idv != aid_reselect_))) break;
      if (id == O_AID) aid_reselect_ = 0;
      if (id == O_SID && in.sub_selection.pending()) break;  // only the latest command reply confirms a click
      if (id == O_SID && sid_reselect_) {
        // The synchronous refresh has already finished. Its transient "no" may still be in
        // the event queue; read the settled track once, rather than leaving an open-ended filter.
        const std::string actual = mpv_.get_string("sid");
        idv = (actual.empty() || actual == "no") ? 0 : std::atoll(actual.c_str());
        sid_reselect_ = 0;
      }
      (id == O_SID ? in.sid : in.aid) = idv;
      // a choice made by hand (not the engine's pick at load) is remembered for the file,
      // and its language becomes the preferred one for the next files
      // (audio is never chosen "off" by hand: a "no" is the engine losing its device, not a choice)
      if (loaded_ && tracks_applied_ && !amix_ && !(id == O_AID && idv == 0) && now() - loaded_at_ > 1.0 && what_until_ < 0 && now() - what_restored_at_ > 1.5) {
        if (RecentItem* r = store_.find(path_)) {
          std::string lang;
          for (auto& t : id == O_SID ? in.subs : in.audios) if (t.id == idv) lang = t.lang;
          if (id == O_SID) { r->sid = idv; r->sid_lang = lang; if (!lang.empty()) store_.slang = lang; }
          else { r->aid = idv; r->aid_lang = lang; if (!lang.empty()) store_.alang = lang; }
          save_state();
        }
      }
      break;
    }
    case O_SPEED:
      // while the speed slider is held, the engine's echoes lag behind the hand: the hand wins
      if (!play_.dragging()) in.speed = dbl(1.0);
      break;
    case O_HWDEC: hwdec_ = str(); break;
    case O_VIDEO_PARAMS:
      request_video_geometry(); // read the complete CURRENT tuple, never merge scalar events
      break;
    case O_SUBTEXT: {
      std::string t = str();
      int lines = 1, longest = 0, cur = 0;
      for (size_t i = 0; i < t.size();) {
        if (t[i] == '\n') { ++lines; longest = std::max(longest, cur); cur = 0; ++i; continue; }
        next_cp(t, i);
        ++cur;
      }
      longest = std::max(longest, cur);
      in.sub_shown = !t.empty();
      if (!t.empty()) { in.sub_lines = lines; in.sub_chars = std::max(8, longest); }
      in.sub_text.clear();
      for (size_t st = 0; !t.empty() && st <= t.size();) {
        size_t en = t.find('\n', st);
        if (en == std::string::npos) en = t.size();
        in.sub_text.push_back(t.substr(st, en - st));
        st = en + 1;
      }
      return;  // no dock refresh needed
    }
    case O_TRACKS: {
      if (p->format != MPV_FORMAT_NODE || !p->data) break;
      auto all = Mpv::parse_tracks((mpv_node*)p->data);
      ++in.tracks_revision;
      in.subs.clear();
      in.audios.clear();
      int ext = 0;
      SubKind kind = SubKind::None;
      for (auto& t : all) {
        if (t.type == "sub") { in.subs.push_back(t); if (t.external) ++ext; if (t.selected) kind = sub_kind(t.codec); }
        if (t.type == "audio") in.audios.push_back(t);
      }
      bool kind_changed = kind != in.kind;
      in.kind = kind;
      if (loaded_ && ext > ext_subs_ && now() - loaded_at_ > 1.5) play_.toast("CC", "check", now(), true);  // CC ✓
      ext_subs_ = ext;
      if (kind_changed && loaded_) apply_style();
      if (loaded_ && !tracks_applied_ && !all.empty()) apply_media_tracks();
      break;
    }
    case O_IDLE:
      media_idle_ = flag(false); // decide after the event batch, including END_FILE/EOF

      break;
    case O_PATH: observed_path_ = str(); break;
    case O_EOF: {
      bool e = flag(false);
      if (e && !eof_ && loaded_) {
        // the film ended: it stays on its last frame, paused, controls in view; next time it starts over
        store_.set_position(path_, 0, in.dur);
        save_state();
        play_.activity(now());
        // the remote's sleep timer said "at the end of this episode"; else, with a phone in
        // hand, the next episode follows
        if (remote_timer_ == -1) { remote_timer_ = 0; next_cancelled_ = true; say(tr(str::remote_goodnight)); }
        else queue_next_episode();
      }
      eof_ = e;
      if (play_.in.ended != (e && loaded_)) { play_.in.ended = e && loaded_; play_.changed(now()); ensure_anim(); }
      break;
    }
  }
}

// Once the tracks of a file are known: give back the ones chosen for it last time, but only
// if a track with that number still has the same language (a different file stays safe).
void App::apply_media_tracks() {
  tracks_applied_ = true;
  const RecentItem* r = store_.find(path_);
  // all the tracks together: when chosen for this file before, or when the file is plainly a
  // recording in parts and nothing was chosen for it yet (said once, changed in the audio menu)
  if (play_.in.audios.size() >= 2) {
    if (r && r->aid == -2) {
      // remembered as "-2" with the track ids in aid_lang ("" = all of them)
      std::vector<int64_t> ids;
      for (size_t i = 0; i < r->aid_lang.size();) {
        size_t e = r->aid_lang.find(',', i);
        if (e == std::string::npos) e = r->aid_lang.size();
        int64_t id = std::atoll(r->aid_lang.substr(i, e - i).c_str());
        for (auto& t : play_.in.audios) if (t.id == id) ids.push_back(id);
        i = e + 1;
      }
      if (r->aid_lang.empty() || ids.size() >= 2) {
        set_audio_mix(r->aid_lang.empty() ? all_audio_ids() : ids, false);
        pick_subs_only(r);
        return;
      }
    }
    if ((!r || r->aid < 0) && looks_like_multitrack_recording(play_.in.audios)) {
      set_audio_mix(all_audio_ids(), false);
      say(i18n::format(str::audio_mixed, {{"n", std::to_string(play_.in.audios.size())}}));
      if (r) { pick_subs_only(r); }
      return;
    }
  }
  if (!r) return;
  auto pick = [&](const std::vector<Track>& v, int64_t want, const std::string& lang, const char* prop) {
    if (want < 0) return;
    if (want == 0) { mpv_.set_async(prop, "no"); return; }
    for (auto& t : v)
      if (t.id == want && t.lang == lang) { mpv_.set_async(prop, std::to_string(want)); return; }
  };
  pick(play_.in.subs, r->sid, r->sid_lang, "sid");
  if (!audio_only_ && r->aid > 0) pick(play_.in.audios, r->aid, r->aid_lang, "aid");  // never "no audio" from memory
}

void App::pick_subs_only(const RecentItem* r) {
  if (r->sid == 0) mpv_.set_async("sid", "no");
  else if (r->sid > 0)
    for (auto& t : play_.in.subs)
      if (t.id == r->sid && t.lang == r->sid_lang) { mpv_.set_async("sid", std::to_string(r->sid)); break; }
}

// One track, heard on its own. Coming out of a mix the engine may still think this track is the
// selected one (its aid never changed while the tracks were mixed), and choosing it again would
// do nothing: no decoder, no sound. So it is let go first, then chosen.
void App::select_audio(int64_t id) {
  if (id <= 0) return;
  if (cover_graph_live() && loaded_) {
    // a song with its own cover: the choice is kept for the file and the song opens again on
    // it at the same moment (the graph around the cover is never swapped mid-song)
    if (RecentItem* r = store_.find(path_)) {
      r->aid = id;
      r->aid_lang.clear();
      for (auto& t : play_.in.audios) if (t.id == id) r->aid_lang = t.lang;
      save_state();
    }
    amix_ = play_.in.amix = false;
    play_.in.amix_ids.clear();
    play_.in.aid = id;
    reopen_song();
    play_.changed(now());
    remote_publish();
    return;
  }
  bool was_mix = amix_;
  if (was_mix || audio_switch_.stage || (audio_only_ && waveform_active_)) {
    std::string graph = audio_only_ && waveform_active_ ? audio_waveform_graph(id, visual_cover(), music_scene_, music_look_on_) : "";
    if (!change_audio_graph(graph, id)) return;
    aid_reselect_ = id;
  } else {
    if (play_.in.aid == id) return;
    mpv_.set_async("aid", std::to_string(id));
  }
  amix_ = play_.in.amix = false;
  play_.in.amix_ids.clear();
  play_.in.aid = id;
  // the engine may report no change at all (same track before and after the mix): the file
  // remembers this choice here, not from that event
  if (was_mix)
    if (RecentItem* r = store_.find(path_)) {
      r->aid = id;
      r->aid_lang.clear();
      for (auto& t : play_.in.audios) if (t.id == id) r->aid_lang = t.lang;
      save_state();
    }
  play_.changed(now());
  remote_publish();
}

// Rebuilding the graph flushes demux/decoder state. A seek in the same command
// list can be consumed by that flush and land at the NEXT keyframe. Wait for the
// rebuild to restart, THEN seek, keeping the picture covered until that seek ends.
bool App::change_audio_graph(const std::string& graph, int64_t single_track) {
  if (!loaded_ || eof_)
    return mpv_.cmd_batch(audio_switch_commands(graph, single_track));
  if (fullscreen_transition_) settle_fullscreen_motion();
  auto& a = audio_switch_;
  if (!a.stage || a.stage == 5) {
    a.position = mpv_.get_double("time-pos", play_.in.time);
    a.paused = pause_pending_ ? pause_target_ : play_.in.paused;
    a.stage = 1; a.at = now(); a.retries = 0;
    mpv_.set("pause", "yes");
    snap_subtitles(true);
  }
  a.opening_mix = false;
  a.graph = graph; a.track = single_track; a.dirty = true;
  if (hidden_now()) {
    a.dirty = false; a.stage = 3; a.at = now();
    mpv_.cmd_batch(audio_switch_commands(a.graph, a.track));
  }
  ensure_anim();
  return true;
}

void App::cancel_audio_switch() {
  if (!audio_switch_.stage) { set_switch_gain(1); return; }  // never left quiet
  // a song being opened again is already on its way: a seek now would land on the old one
  if (audio_switch_.reopen && (audio_switch_.stage == 3 || audio_switch_.stage == 4) && now() - audio_switch_.at < 8) return;
  music_reopening_ = false;
  bool paused = audio_switch_.paused;
  audio_switch_ = {};
  set_switch_gain(1);
  end_snap();
  if (mpv_ok_) mpv_.set("pause", paused ? "yes" : "no");
}

// The picture while the sound changes language: it drains to black and white and breaks into
// big pixels, which then shuffle (like letters turning over) while the engine rebuilds and finds
// its place again; then the pixels settle, smaller and smaller, and the colour comes back last.
// The shuffle also hides the moment the engine shows a frame from further on.
namespace {
constexpr double kSwitchOut = 0.16, kSwitchIn = 0.32, kSwitchDim = 0.32;
constexpr double kSnapOut = 0.11, kSnapIn = 0.20, kSnapHold = 0.045;
double snap_px(double amount, int height);  // the mosaic's block size (defined with the fullscreen motion)
}

bool App::step_audio_switch(double t) {
  auto& a = audio_switch_;
  if (!a.stage) return false;
  bool rm = reduce_motion();
  auto shuffle = [&] {
    if (a.opening_mix) { set_snap(1.0, snap_px(1.0, ch_), 1.0); return; }
    // two coarse block sizes taking turns, a dozen times a second
    int beat = (int)std::floor((t - a.at) / 0.085);
    set_snap(kSwitchDim, snap_px(beat % 2 ? 0.84 : 1.0, ch_), 1.0);
  };
  // a song opened again for a new look moves quicker than a language change: the picture is all
  // that changes, and the sound waits only for the reopen
  const double out = a.opening_mix ? kSnapOut : a.reopen ? 0.12 : kSwitchOut;
  const double in = a.opening_mix ? kSnapIn : a.reopen ? 0.26 : kSwitchIn;
  const double hold = a.opening_mix ? kSnapHold : .05;
  if (a.stage == 1) {
    double k = rm ? 1.0 : clamp01((t - a.at) / out);
    if (a.opening_mix) set_snap(k * k, snap_px(k, ch_), std::min(1.0, k * 1.6));
    else set_snap(kSwitchDim * k, snap_px(0.3 + 0.7 * k * k, ch_), smoothstep(0.0, 0.7, k));
    if (a.reopen) set_switch_gain(1 - k);  // the sound breathes out with the picture
    if (k >= 1) { a.stage = 2; a.at = t; }
  } else if (a.stage == 2 && t - a.at < hold) {
    shuffle();
  } else if (a.stage == 2 && t - a.at >= hold) {
    // Give the shader's black picture a couple of presentation frames first.
    a.dirty = false; a.stage = 3; a.at = t;
    if (a.reopen) {
      if (!eof_) a.position = mpv_.get_double("time-pos", a.position);  // where it has played to
      music_reopening_ = true;
      release_video_window();
      mpv_.cmd({"loadfile", path_, "replace"});
    } else if (!mpv_.cmd_batch(audio_switch_commands(a.graph, a.track))) {
      cancel_audio_switch();
      return false;
    }
  } else if ((a.stage == 3 || a.stage == 4) && t - a.at > (a.reopen ? 8 : 4)) {
    // A failed decoder must not leave the picture scrambled indefinitely.
    cancel_audio_switch();
    return false;
  } else if (a.stage == 3 || a.stage == 4) {
    if (!rm) shuffle();
  } else if (a.stage == 5) {
    double k = rm ? 1.0 : clamp01((t - a.at) / in);
    if (a.reopen) set_switch_gain(rm ? 1.0 : ease_out(clamp01((t - a.at) / 0.2)));  // and back in
    double blocks = 1 - ease_out(k);
    if (a.opening_mix) {
      set_snap(1 - ease_out(clamp01(k * 1.35)),
               snap_px(1 - ease_out(clamp01((k - 0.08) / 0.85)), ch_),
               1 - smoothstep(0.45, 1.0, k));
      if (k >= 0.55) snap_subtitles(false);
    } else set_snap(kSwitchDim * (1 - ease_out(clamp01(k * 1.4))), snap_px(blocks, ch_), 1 - smoothstep(0.5, 1.0, k));
    if (k >= 1) {
      a.stage = 0;
      set_switch_gain(1);
      end_snap();
      // the person's choice, said once more: whatever the engine went through, a subtitle that
      // is on in the menu is on on screen
      if (play_.in.sid > 0 && play_.in.sub_visible) mpv_.set_flag_async("sub-visibility", true);
      return false;
    }
  }
  return true;
}

// After the audio graph is rebuilt and the engine has jumped back, the subtitle that was on
// screen can be lost (its line began before the place the engine came back to, and is not read
// again): the track still says "on", nothing shows. Letting go of the track and taking it again
// makes the engine read the current line anew. Its passing "no" is not the person's choice.
void App::refresh_subtitles() {
  int64_t sid = play_.in.sub_selection.shown(play_.in.sid);
  if (sid <= 0 || !mpv_ok_) return;
  sid_reselect_ = sid;
  if (!mpv_.cmd_batch({{"set", "sid", "no"}, {"set", "sid", std::to_string(sid)}})) sid_reselect_ = 0;
}

void App::select_subtitle(int64_t id) {
  if (id < 0) return;
  sid_reselect_ = 0;  // a new choice supersedes the audio refresh, including its suppressed "no"
  const uint64_t request = play_.in.sub_selection.begin(id);
  // Always send the choice. in.sid may still describe A while B is queued and the user picks A.
  const int error = mpv_.cmd({"set", "sid", id == 0 ? "no" : std::to_string(id)}, request);
  if (error < 0) subtitle_selection_reply(request, error);
  if (id > 0) set_subtitle_visibility(true);
}

void App::subtitle_selection_reply(uint64_t request, int error) {
  if (!play_.in.sub_selection.finish(request)) return;  // an older click or another file
  sid_reselect_ = 0;
  // A -> B -> A need not emit an observer event. Read the actual result after the last command,
  // also on failure, so the menu never keeps claiming a choice the engine did not accept.
  std::string value = mpv_.get_string("sid");
  char* text = value.data();
  mpv_event_property property{"sid", MPV_FORMAT_STRING, &text};
  on_property(O_SID, &property);
  if (error < 0) log("subtitle selection failed: " + std::to_string(error));
  play_.changed(now());
}

void App::set_subtitle_visibility(bool visible) {
  play_.in.sub_visible = visible;
  if (snap_.subs_hidden) {
    // A click during a mini/fullscreen/audio transition changes what is restored on landing.
    snap_.subs_were = visible;
    if (loaded_) if (RecentItem* r = store_.find(path_)) { r->sub_visible = visible; save_state(); }
  }
  mpv_.set_flag_async("sub-visibility", visible && !snap_.subs_hidden);
}

void App::audio_switch_restart() {
  auto& a = audio_switch_;
  if (a.stage != 3 && a.stage != 4) return;
  if (a.dirty) { a.stage = 2; a.at = now(); return; }
  double pos = mpv_.get_double("time-pos", -1);
  // a song opened again starts exactly where it was asked to (an absolute start is a precise
  // seek): a second seek would only make the sound wait for another decode
  bool landed = a.reopen && a.stage == 3 && pos >= 0 && std::fabs(pos - a.position) <= .2;
  if (!landed && (a.stage == 3 || (std::fabs(pos - a.position) > .075 && a.retries < 3))) {
    a.stage = 4; a.at = now(); ++a.retries;
    mpv_.cmd({"seek", std::to_string(a.position), "absolute+exact"});
    return;
  }
  a.stage = 5; a.at = now();
  play_.in.time = last_time_ = pos;
  refresh_subtitles();
  mpv_.set("pause", a.paused ? "yes" : "no");
  if (hidden_now()) { a.stage = 0; set_switch_gain(1); end_snap(); }
  ensure_anim();
}

// A jump in the sound (a click on the timeline, the arrow keys, the 15-second steps) used to cut
// the wave wherever it was and start the new place at full level: a pop. Now the sound breathes
// out in 16 ms, jumps, and breathes in over 50 ms once the new place plays (the engine's gain,
// under the person's volume). Paused, nothing to hear: the jump is immediate.
void App::seek_soft(std::vector<std::string> cmd) {
  auto& f = seek_fade_;
  if (!mpv_ok_ || play_.in.paused || audio_switch_.stage) { f.stage = 0; set_switch_gain(1); mpv_.cmd_vec(cmd); return; }
  if (f.stage == 2) { mpv_.cmd_vec(cmd); return; }  // already silent, waiting: the newer place wins
  if (f.stage == 3) f.t0 = now() - (1 - switch_gain_) * 0.016;  // breathing in: back out from where it is
  else if (f.stage == 0) f.t0 = now();
  f.stage = 1;
  f.cmd = std::move(cmd);
  step_seek_fade(now());
  ensure_anim();
}

bool App::step_seek_fade(double t) {
  auto& f = seek_fade_;
  if (!f.stage) return false;
  if (f.stage == 1) {
    double k = clamp01((t - f.t0) / 0.016);
    set_switch_gain(1 - k);
    if (k >= 1) { mpv_.cmd_vec(f.cmd); f.cmd.clear(); f.stage = 2; f.t0 = t; }
  } else if (f.stage == 2) {
    if (t - f.t0 > 1.5) { f.stage = 3; f.t0 = t; }  // no restart came: never left quiet
  } else {
    double k = clamp01((t - f.t0) / 0.05);
    set_switch_gain(k * k * (3 - 2 * k));
    if (k >= 1) { set_switch_gain(1); f.stage = 0; }
  }
  return true;
}

// The engine's own gain, under the person's volume (volume-gain, in dB): 1 is untouched, 0 silent.
// Sent only when it changes, so a fade is a dozen messages, and nothing at rest.
void App::set_switch_gain(double g) {
  g = std::clamp(g, 0.0, 1.0);
  if (std::fabs(g - switch_gain_) < 0.004 && (g == 0 || g == 1) == (switch_gain_ == 0 || switch_gain_ == 1)) return;
  switch_gain_ = g;
  char b[24];
  std::snprintf(b, sizeof b, "%.2f", g >= 0.999 ? 0.0 : g <= 0.001 ? -96.0 : 20 * std::log10(g));
  if (mpv_ok_) mpv_.set_async("volume-gain", b);
}

std::vector<int64_t> App::all_audio_ids() const {
  std::vector<int64_t> ids;
  for (auto& t : play_.in.audios) ids.push_back(t.id);
  return ids;
}

// Several tracks at once. The engine rebuilds its audio for each change, so an unchanged set
// is never sent again; the panel, the phone and the saved choice follow the same list.
void App::set_audio_mix(std::vector<int64_t> ids, bool remember) {
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  if (ids.size() < 2) return;
  // restoring a remembered mix as the file opens: the graph already has it (configure_audio_mode)
  if (cover_graph_live() && !remember) return;
  if (cover_graph_live() && loaded_ && (!amix_ || ids != play_.in.amix_ids)) {
    // a song with its own cover: the mix is built as the song opens again (see select_audio)
    if (RecentItem* r = store_.find(path_)) {
      r->aid = -2;
      r->aid_lang.clear();
      if (ids.size() < play_.in.audios.size())
        for (size_t i = 0; i < ids.size(); ++i) r->aid_lang += (i ? "," : "") + std::to_string(ids[i]);
      save_state();
    }
    amix_ = play_.in.amix = true;
    play_.in.amix_ids = ids;
    reopen_song();
    play_.changed(now());
    remote_publish();
    return;
  }
  if (!amix_ || ids != play_.in.amix_ids) {
    std::string g = audio_mix_graph(ids, audio_only_ && waveform_active_, visual_cover(), music_scene_, music_look_on_);
    if (g.empty()) return;
    if (!change_audio_graph(g, 0)) return;
    // Automatic restoration uses the fullscreen pixel fade, without changing window mode.
    audio_switch_.opening_mix = !remember && !audio_only_;
  }
  amix_ = play_.in.amix = true;
  play_.in.amix_ids = ids;
  if (remember)
    if (RecentItem* r = store_.find(path_)) {
      r->aid = -2;
      r->aid_lang.clear();
      if (ids.size() < play_.in.audios.size())
        for (size_t i = 0; i < ids.size(); ++i) r->aid_lang += (i ? "," : "") + std::to_string(ids[i]);
      save_state();
    }
  play_.changed(now());
  remote_publish();
}

// The mix is this file's: the next one starts plain (and finds its own tracks).
void App::clear_audio_mix() {
  cancel_audio_switch();
  aid_reselect_ = 0;
  sid_reselect_ = 0;
  play_.in.sub_selection.reset();
  if (!amix_) return;
  amix_ = play_.in.amix = false;
  play_.in.amix_ids.clear();
  if (mpv_ok_) mpv_.set("lavfi-complex", "");
}

// A song opened on its own brings its album: the folder's music becomes the queue, in name
// order, from this song. So previous and next are the album's tracks, the queue panel lists them
// and the album plays on. A queue the person built is never replaced.
void App::music_album_queue() {
  if (!audio_only_ || queue_.active() || path_.empty() || path_.find("://") != std::string::npos) return;
  std::error_code ec;
  std::filesystem::path p = std::filesystem::absolute(std::filesystem::path(widen(path_)), ec);
  if (ec) p = std::filesystem::path(widen(path_));
  const std::filesystem::path dir = p.parent_path();
  const std::string mine = narrow(p.filename().wstring());
  std::vector<std::string> names;
  if (!folder_names(dir.wstring(), names)) return;  // still being read: comes back here when it is
  names.erase(std::remove_if(names.begin(), names.end(), [](const std::string& n) { return !remote::is_audio_name(n); }), names.end());
  if (names.size() < 2) { log("album: " + std::to_string(names.size()) + " song(s) in the folder"); return; }
  std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return remote::natural_less(a, b); });
  std::vector<std::string> paths;
  size_t here = SIZE_MAX;
  for (auto& n : names) {
    // this song is matched by its name in its folder (the path it was opened with may be
    // spelled differently: case, slashes, a relative path), and keeps that exact path
    bool is_mine = here == SIZE_MAX && MediaQueue::same_path(n, mine);
    if (is_mine) here = paths.size();
    paths.push_back(is_mine ? path_ : narrow((dir / widen(n)).wstring()));
  }
  if (here == SIZE_MAX) { log("album: the playing song was not found in its folder"); return; }
  // A huge folder keeps the songs around this one.
  size_t lim = MediaQueue::kLimit, first = paths.size() > lim ? std::min(here > lim / 2 ? here - lim / 2 : 0, paths.size() - lim) : 0;
  std::vector<std::string> window(paths.begin() + first, paths.begin() + std::min(paths.size(), first + lim));
  queue_.append(window);
  if (!queue_.select_path(path_)) { queue_.clear(); return; }
  queue_refresh();
}

void App::list_folder(const std::wstring& dir) {
  folder_wait_ = false;
  if (folder_list_ && folder_list_->dir == dir && !folder_list_->done.load()) return;  // already on it
  folder_list_.reset();
  if (dir.empty()) return;
  try {
    auto job = std::make_shared<FolderList>();
    job->dir = dir;
    std::thread([job] {
      try { job->names = folder_files(job->dir, 4000); } catch (...) { job->names.clear(); }
      job->done.store(true, std::memory_order_release);
    }).detach();
    folder_list_ = std::move(job);
  } catch (...) {}
}

// The folder's file names once they have been read. False while the worker is still reading:
// the caller is run again as soon as it is done (the poll, see folder_wait_).
bool App::folder_names(const std::wstring& dir, std::vector<std::string>& out) {
  out.clear();
  if (dir.empty()) return true;
  if (!folder_list_ || folder_list_->dir != dir) list_folder(dir);
  if (!folder_list_) return true;  // could not start a reader: as an empty folder
  if (!folder_list_->done.load(std::memory_order_acquire)) { folder_wait_ = true; return false; }
  out.reserve(folder_list_->names.size());
  for (auto& n : folder_list_->names) out.push_back(narrow(n));
  return true;
}

// Colour (the default) or black and white: saved for all music. Only the picture's last pass
// changes (the mono shader pass), so the song never reopens: the colour drains away in 0.3 s.
void App::set_music_mono(bool on) {
  store_.music_mono = on;
  play_.in.music_mono = on;
  save_state();
  play_.changed(now());
  ensure_anim();
}

bool App::step_mono(double dt) {
  const double want = store_.music_mono && audio_only_ && cover_scene_ && music_look_on_ != 2 && mode_ == Mode::Play ? 1.0 : 0.0;
  if (mono_k_ == want) return false;
  // a song opened in black and white is grey from its first picture; a change is eased
  if (dt <= 0 || reduce_motion() || !first_frame_) mono_k_ = want;
  else mono_k_ = want > mono_k_ ? std::min(want, mono_k_ + dt / 0.3) : std::max(want, mono_k_ - dt / 0.3);
  update_blur_cloud();
  return mono_k_ != want;
}

// Cover and spectrum, the spectrum alone, or the cover alone (the lightest: no spectrum, no light
// show): saved for all music, and applied to the file playing now by rebuilding its picture (the
// sound's own branch is rebuilt with it, like a track change).
void App::set_music_look(int look) {
  look = std::clamp(look, 0, 2);
  store_.music_look = look;
  play_.in.music_look = look;
  save_state();
  // every song has both looks now (one without a cover wears Cue's tile)
  if (audio_only_ && waveform_active_ && !cover_fallback_ && look != music_look_on_) reopen_music();
  play_.changed(now());
  ensure_anim();
}

// The cover look has two compositions: wide (cover left, words right) and tall (words under the
// cover). When a resize ends on the other side of that line, the song is recomposed once; while
// an edge is still held nothing changes, so a drag never touches the sound.
// Another app's full-screen window over ours (a game, a full-screen video elsewhere) on the same
// screen: nothing of Cue can be seen, so nothing of Cue is drawn. Minimised windows are handled
// the same way where IsIconic is checked. Cue pinned on top stays awake.
bool App::covered() const {
  if (!hwnd_ || IsIconic(hwnd_)) return false;
  if (cloaked_) return false;  // our own cloak for a moment (the mini player's flight): still in view
  BOOL cloaked = FALSE;  // another virtual desktop
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd_, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked) return true;
  if (GetWindowLongPtrW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) return false;
  HWND fg = GetForegroundWindow();
  if (!fg || fg == hwnd_ || GetAncestor(fg, GA_ROOTOWNER) == hwnd_) return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(fg, &pid);
  if (pid == GetCurrentProcessId()) return false;
  wchar_t cls[64]{};
  GetClassNameW(fg, cls, 63);
  if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Shell_TrayWnd")) return false;
  HMONITOR m = MonitorFromWindow(fg, MONITOR_DEFAULTTONULL);
  if (!m || m != MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST)) return false;
  MONITORINFO mi{sizeof mi};
  RECT r{};
  if (!GetMonitorInfoW(m, &mi) || !GetWindowRect(fg, &r)) return false;
  return r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top && r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
}

// While hidden, the interface stops drawing, the cover's shader pass rests, and a film is
// presented once per frame instead of at the display's rate. Playback itself never changes.
// A film under a full-screen window: its frames are timed by the sound (no display resampling for
// nobody). Only when covered: switching the timing makes the engine settle its sound again, which
// a window coming back from the taskbar would hear as a hiccup. Music keeps its picture as it is:
// rebuilding the picture's chain to slow it down made the sound stall for a moment both ways.
void App::update_music_idle() {
  const bool film_out = mpv_ok_ && loaded_ && !audio_only_ && mode_ == Mode::Play && hidden_;
  if (film_out != film_idle_) {
    film_idle_ = film_out;
    send_sync();
  }
}

void App::set_hidden(bool h) {
  if (h == hidden_) return;
  hidden_ = h;
  trace(h ? "covered by a full-screen window: drawing paused" : "in view again");
  update_music_idle();  // a film: frames timed by the sound while covered
  update_blur_cloud();
  if (!h) { play_.changed(now()); ensure_anim(); }
}

// The taskbar preview's buttons: previous, play/pause, next. They do what the dock's do.
void App::taskbar_sync() {
  // between two files the buttons keep what they showed: no flicker while the next one opens
  if (mode_ == Mode::Play && !loaded_ && taskbar_seen_play_) return;
  taskbar_seen_play_ = mode_ == Mode::Play && loaded_;
  // called with every poll: the words are looked up only when the language changes
  static TaskbarButtons::State s;
  static std::string lang;
  // shown for the whole time something plays, also between two files: the preview's buttons
  // never vanish and come back while the next song opens
  s.shown = mode_ == Mode::Play;
  s.playing = s.shown && !play_.in.paused && !eof_;
  s.has_prev = s.shown && play_.in.queue_active && play_.in.has_prev;
  s.has_next = s.shown && play_.in.has_next;
  if (std::string l = i18n::tag(i18n::language()); l != lang) {
    lang = l;
    s.previous = widen(tr(str::queue_previous));
    s.next = widen(tr(str::queue_next));
    s.play = widen(tr(str::tip_play));
    s.pause = widen(tr(str::tip_pause));
  }
  taskbar_.sync(s);
  media_sync();
}

// What plays, on the lock screen and in the flyout over the volume keys: a song with its title,
// artist and cover; a film with its name. Sent when it changes; the timeline every few seconds.
void App::media_sync() {
  const bool active = mode_ == Mode::Play;
  if (!media_attached_ && active) {
    // joined on the first playback, not at start. A marker stays on disk while it is being
    // joined: if Windows' side ever took Cue down there, the next run leaves it alone.
    media_attached_ = true;
    std::error_code ec;
    const auto mark = data_dir_ / "media-controls.try";
    if (std::filesystem::exists(mark, ec)) log("media controls: skipped (the last attempt did not finish)");
    else {
      { std::ofstream(mark) << "1"; }
      bool ok = media_controls_.attach(hwnd_, WM_APP_MEDIA_BUTTON);
      std::filesystem::remove(mark, ec);
      if (ok) trace("media controls on"); else log("media controls unavailable");
    }
  }
  media_controls_.set_active(active);
  if (!active) { media_key_.clear(); media_cover_job_.reset(); return; }
  media_controls_.set_playing(!play_.in.paused && !eof_);
  media_controls_.set_buttons(play_.in.queue_active && play_.in.has_prev, play_.in.has_next);
  if (!loaded_) return;
  // Parse a film's filename once per file, rather than on every 33 ms poll. Music tags
  // can arrive later, so their actual words still invalidate the cached card immediately.
  const double t = now();
  if (path_ != media_key_ || audio_only_ != media_words_audio_ ||
      (audio_only_ && (play_.in.music_title != media_title_ || play_.in.music_artist != media_sub_))) {
    media_key_ = path_;
    media_words_audio_ = audio_only_;
    if (audio_only_) { media_title_ = play_.in.music_title; media_sub_ = play_.in.music_artist; }
    else remote::card_words(file_name(path_), media_title_, media_sub_);
    const auto& title = media_title_;
    const auto& artist = media_sub_;
    media_controls_.set_media(audio_only_, title, artist, "");  // the words at once; the picture follows
    media_cover_job_.reset();
    media_still_job_.reset();
    media_still_tries_ = 0;
    if (audio_only_) { media_cover_job_ = request_cover(path_); media_still_due_ = -1; }  // the song's one read
    else media_still_due_ = t + 6;  // a film shows a still of itself, taken a few seconds in
  }
  if (media_cover_job_ && media_cover_job_->done.load()) {
    if (!media_cover_job_->bytes.empty()) media_controls_.set_media(true, media_title_, media_sub_, media_cover_job_->bytes);
    media_cover_job_.reset();
  }
  // the film's still: the engine saves the frame on screen (no subtitles), it is read on a
  // worker; a black frame is tried again later, three times at most
  // (the frame is asked for in memory: nothing is encoded by the engine or written to disk; the
  // reply is shrunk on arrival, see kReplyStill, and turned into the card's picture on a worker)
  if (!audio_only_ && media_still_due_ > 0 && t >= media_still_due_ && first_frame_ && !play_.in.paused && !eof_) {
    media_still_due_ = -1;
    if (mpv_.cmd({"screenshot-raw", "video"}, kReplyStill) < 0 && ++media_still_tries_ < 3) media_still_due_ = t + 20;
  }
  if (media_still_job_ && media_still_job_->done.load()) {
    if (!media_still_job_->bytes.empty()) media_controls_.set_media(false, media_title_, media_sub_, media_still_job_->bytes);
    else if (++media_still_tries_ < 3) media_still_due_ = t + 20;
    media_still_job_.reset();
  }
  media_controls_.set_timeline(play_.in.time, play_.in.dur);
}

void App::taskbar_command(UINT id) {
  if (mode_ != Mode::Play || !loaded_) return;
  if (id == TaskbarButtons::kPlayPause) play_.request(PA::TogglePause, play_.in.paused || eof_ ? 0 : 1);
  else if (id == TaskbarButtons::kNext && play_.in.has_next) play_.request(PA::NextEpisode);
  else if (id == TaskbarButtons::kPrevious && play_.in.queue_active && play_.in.has_prev) play_.request(PA::PreviousInQueue);
  run_actions();
  taskbar_sync();
  ensure_anim();
}

// The window frames the scene (see MusicFrame): all of it, the cover with the words under it, or
// the cover alone. Called on every size change, also while an edge is being dragged: the engine's
// zoom and pan follow at once, nothing is rebuilt and the song never stops. Crossing from one
// framing to another glides for a third of a second.
void App::hold_seek(double target) {
  double d = play_.in.dur > 0 ? play_.in.dur : 1e9;
  seek_hold_ = std::clamp(target, 0.0, d);
  seek_hold_until_ = now() + 1.5;
  play_.in.time = last_time_ = seek_hold_;  // the bar answers at once
  play_.changed(now());
  ensure_anim();
}

void App::music_fit_window() {
  if (!audio_only_ || !cover_scene_ || cw_ <= 0 || ch_ <= 0) return;
  MusicFrame f = mini_player_ && audio_only_ && !fullscreen_ ? MusicFrame::Card : music_frame_for(cw_, ch_, s_);
  if (f != music_frame_) {
    view_from_ = play_.in.music_view.f > 0 ? play_.in.music_view : music_view_target();
    music_frame_ = f;
    view_t0_ = reduce_motion() || mini_fx_.on ? -1 : now();  // into or out of the mini player: under the pixels, at once
  }
  step_music_view(now());
  play_.changed(now());
  ensure_anim();
}

MusicView App::music_view_target() const {
  int top = ch_;
  Rect d = play_.blur_cloud().controls;
  if (d.h > 0 && d.cy() > ch_ / 2) top = d.y;  // the bar at the bottom: the cover stays above it
  // the list open with its header (a close composition, a small window): the cover is the
  // header's thumbnail, framed there like the card's cover
  if (music_frame_ != MusicFrame::Card) {
    const Rect c = play_.list_cover();
    if (c.w > 0) {
      MusicView v;
      v.frame = music_frame_;
      v.f = c.w / (double)music_scene_.side;
      v.ox = c.x - music_scene_.x * v.f;
      v.oy = c.y - music_scene_.y * v.f;
      return v;
    }
  }
  // a song's mini player: the cover where the card puts it (its words and bar beside it)
  if (music_frame_ == MusicFrame::Card) {
    MusicView v;
    v.frame = MusicFrame::Card;
    Rect c = play_.card_cover();
    if (c.w <= 0) return music_view(music_scene_, MusicFrame::Cover, cw_, ch_, top);
    v.f = c.w / (double)music_scene_.side;
    v.ox = c.x - music_scene_.x * v.f;
    v.oy = c.y - music_scene_.y * v.f;
    return v;
  }
  // a roomy window: the cover in the middle over its words, the bar along the foot (the list open:
  // the cover and its words in the left half)
  if (music_frame_ == MusicFrame::Centre)
    return music_view(music_scene_, MusicFrame::Centre, cw_, ch_, top, play_.music_centre_below(), play_.music_stack_above(), -1,
                      play_.music_list_beside());
  // framed close: the cover, its words and the bar as one group
  if (music_frame_ != MusicFrame::Scene && play_.music_stacked())
    return music_view(music_scene_, music_frame_, cw_, ch_, top, play_.music_stack_below(), play_.music_stack_above());
  return music_view(music_scene_, music_frame_, cw_, ch_, top, -1, 0, play_.music_words_w());
}

bool App::step_music_view(double t) {
  if (!audio_only_ || !cover_scene_ || cw_ <= 0 || ch_ <= 0) return false;
  MusicView v = music_view_target();
  // (what this framing was made for: a change from it is seen on the next frame, whoever framed it)
  if (music_frame_ == MusicFrame::Centre) { list_side_seen_ = play_.music_list_beside(); centre_below_seen_ = play_.music_centre_below(); }
  bool moving = false;
  if (view_t0_ >= 0) {
    double k = clamp01((t - view_t0_) / 0.34);
    v = mix_views(view_from_, v, k * k * (3 - 2 * k));
    moving = k < 1;
    if (!moving) view_t0_ = -1;
  }
  play_.in.music_view = v;
  MpvFraming m = mpv_framing(music_scene_, v, cw_, ch_);
  if (std::fabs(m.zoom - framing_sent_.zoom) > 1e-4 || std::fabs(m.pan_x - framing_sent_.pan_x) > 1e-5 ||
      std::fabs(m.pan_y - framing_sent_.pan_y) > 1e-5) {
    framing_sent_ = m;
    char b[3][32];
    std::snprintf(b[0], 32, "%.5f", m.zoom); std::snprintf(b[1], 32, "%.6f", m.pan_x); std::snprintf(b[2], 32, "%.6f", m.pan_y);
    // (the engine's own options, not file-local ones: those go back when a song unloads, and its
    // last picture, still on screen while the next one opens, was drawn again unframed. Every file
    // sets them when the engine reads it: a film back to none, a song to its framing)
    mpv_.set_async("video-zoom", b[0]);
    mpv_.set_async("video-pan-x", b[1]);
    mpv_.set_async("video-pan-y", b[2]);
    update_blur_cloud();
  }
  return moving;
}

// The engine's picture as it comes (a film, or a song with no picture of its own).
void App::frame_plain() {
  framing_sent_ = {0, 0, 0};
  mpv_.set("video-zoom", "0");
  mpv_.set("video-pan-x", "0");
  mpv_.set("video-pan-y", "0");
}

void App::begin_music_gap() {
  music_gap_ = true;
  music_gap_t_ = now();
}

void App::end_music_gap() {
  if (!music_gap_) return;
  music_gap_ = false;
  update_blur_cloud();
}

// ── the card's song change ──────────────────────────────────────────
// Next, previous, a song chosen in the list or the one that follows at the end, in the mini
// player's card: the cover turns away about its upright axis while the song breathes out (0.15 s),
// the next one opens while the cover is edge-on, and as it starts playing its cover turns in from
// the other side, settling with a little overshoot (0.42 s). The words step out with the old cover
// and the new ones slide in. The sound is never held: the old song fades for those 0.15 s, the new
// one starts at its full level.
// The spectrum-alone look has no cover in the engine's picture: in the card Cue draws the song's
// cover itself (read ahead at the card's size, the same picture the turn uses) over the square
// where the engine shows the spectrum. Nothing is reopened, the sound never stops, and back in
// the full player the spectrum is there as it was.
// (also while one song closes and the next opens: the interface keeps the song's scene then, and
// the look in use is the last one set up; a gap in this condition showed the spectrum behind the
// card for a moment at every song change)
bool App::card_drawn() const {
  return mode_ == Mode::Play && mini_player_ && !fullscreen_ && play_.in.audio_only && play_.in.cover_scene && !cover_art_ &&
         music_look_on_ == 1;
}

bool App::card_flip_ok() const {
  return mode_ == Mode::Play && mini_player_ && !fullscreen_ && audio_only_ && cover_scene_ && (cover_art_ || card_drawn()) && waveform_active_ &&
         loaded_ && first_frame_ && mpv_ok_ && !reduce_motion() && !hidden_now() && !audio_switch_.stage && !mini_fx_.on;
}

// The turn is one movement through the edge: away with an ease-in (fastest at the edge), in with an
// ease-out that leaves the edge at that same speed (885 degrees a second, so the hand-over reads
// as one turn, not a stop and a start), a whisper past flat, settled. 88.5 degrees: edge-on is a
// hair, never a lit sliver that seems to hang while the next song opens.
namespace { constexpr double kFlipEdge = 1.545, kFlipOut = 0.20, kFlipIn = 0.42, kFlipWait = 2.0, kFlipBack = 1.2; }

// The card's cover on screen, and the window's client area (screen px).
bool App::card_cover_screen(RECT& cover, RECT& clip) const {
  const Rect c = play_.card_cover();
  if (c.w <= 0 || !hwnd_) return false;
  POINT a{c.x, c.y}, b{c.x + c.w, c.y + c.h}, o{0, 0};
  if (!ClientToScreen(hwnd_, &a) || !ClientToScreen(hwnd_, &b) || !ClientToScreen(hwnd_, &o)) return false;
  cover = {a.x, a.y, b.x, b.y};
  clip = {o.x, o.y, o.x + cw_, o.y + ch_};
  return true;
}

// The cover the card shows, read ahead at the card's size, so a song change can start turning it
// the moment it is asked for.
void App::card_art_prefetch() {
  if (!mini_player_ || fullscreen_ || !audio_only_ || !(cover_art_ || card_drawn()) || mode_ != Mode::Play || turn_on_) return;
  const int side = play_.card_cover().w;
  if (side <= 0) return;
  if (!card_art_ || card_art_->side != side || card_art_path_ != path_) {
    card_art_ = request_cover_art(cover_vid_ > 0 ? cover_job_ : nullptr, side);
    card_art_path_ = path_;
  }
  // and the songs either side of it, a moment after this one started (never while it opens): a
  // skip has its next cover in hand, and turns through the edge in one movement
  if (!queue_.active() || !loaded_ || now() - loaded_at_ < 1.0) return;
  auto ahead = [&](Ahead& a, const MediaQueue::Item* item) {
    if (!item) return;
    if (a.job && a.id == item->id && a.job->side == side) return;
    a.id = item->id;
    a.job = request_cover_art(request_cover(item->path), side);
  };
  ahead(ahead_next_, queue_following());
  ahead(ahead_prev_, shuffle_previous());
}

// The drawn card cover (card_drawn): handed to the interface once read, shown while no turn draws
// it (a turn hands it back as the engine's cover would come back: once its shadow is home)
bool App::sync_card_art() {
  const bool drawn = card_drawn() && !mini_fx_.on;
  std::shared_ptr<const CoverArt> art;
  const bool ready = card_art_ && card_art_->done.load() && card_art_path_ == path_ && card_art_->art;
  if (drawn) art = ready ? card_art_->art : play_.in.card_art;  // (the last cover stays until the next is read)
  const bool show = drawn && art && art->ok() && !(turn_on_ && cov_hide_ > 0.0005);
  if (art != play_.in.card_art || show != play_.in.card_art_show) {
    play_.in.card_art = art;
    play_.in.card_art_show = show;
    play_.changed(now());
    return true;
  }
  return drawn && !ready;  // (still being read: look again next frame)
}

void App::queue_play(uint32_t id) {
  if (flip_.stage == 1) {  // still turning away: the newer choice wins
    if (turn_on_ && id != flip_.id) {
      const auto* item = queue_.get(id);
      next_art_ = item ? request_cover_art(request_cover(item->path), turn_cover_.right - turn_cover_.left) : nullptr;
      next_art_id_ = id;
    }
    flip_.id = id;
    return;
  }
  // a song chosen while the last turn is still under way: that one lands at once, this one turns
  if (turn_on_) end_card_flip(false);
  card_art_prefetch();
  // Cue's own turn, over the engine's picture: the old cover where the engine shows it, then the
  // engine leaves its cover out (the room and the shadow stay its own)
  RECT cover{}, clip{};
  if (flip_.stage == 0 && card_flip_ok() && card_art_ && card_art_->done.load() && card_art_->art && card_art_path_ == path_ &&
      card_cover_screen(cover, clip) && card_art_->side == cover.right - cover.left && view_t0_ < 0 &&
      turn_.begin(inst_, hwnd_, cover, clip, card_art_->art)) {
    turn_on_ = true;
    turn_cover_ = cover;
    turn_restart_t_ = -1;
    cov_hide_ = 0.002;  // (the cover out; its shadow fades as the window's comes in)
    flip_ = {};
    flip_.stage = 1; flip_.t0 = now(); flip_.id = id;
    // (read ahead for the songs either side: in hand already, the turn never waits at the edge)
    const int side = cover.right - cover.left;
    if (ahead_next_.job && ahead_next_.id == id && ahead_next_.job->side == side) next_art_ = ahead_next_.job;
    else if (ahead_prev_.job && ahead_prev_.id == id && ahead_prev_.job->side == side) next_art_ = ahead_prev_.job;
    else { const auto* item = queue_.get(id); next_art_ = item ? request_cover_art(request_cover(item->path), side) : nullptr; }
    next_art_id_ = id;
    play_.in.music_leaving = true;
    play_.changed(now());
    update_blur_cloud();
    ensure_anim();
    return;
  }
  if (flip_.stage == 3 || (flip_.stage == 0 && card_flip_ok())) {
    flip_.from = flip_.stage == 3 ? flip_ry_ : 0.0;  // (turning in: it turns right on round)
    flip_.stage = 1; flip_.t0 = now(); flip_.id = id;
    play_.in.music_leaving = true;
    play_.changed(now());
    ensure_anim();
    return;
  }
  queue_play_now(id);
}

void App::end_card_flip(bool play_pending) {
  if (!flip_.stage) return;
  const int stage = flip_.stage;
  const uint32_t id = flip_.id;
  if (turn_on_) {
    turn_.abort();
    turn_on_ = false;
    cov_hide_ = 0;
    turn_restart_t_ = -1;
    next_art_.reset();
  }
  flip_ = {};
  flip_ry_ = 0;
  play_.in.music_leaving = false;
  set_switch_gain(1);
  play_.changed(now());
  update_blur_cloud();
  if (stage == 1 && play_pending) queue_play_now(id);
}

// Cue's own turn (see cover_turn_window.h): the window's thread draws the cover; this side opens
// the next song at the edge, hands it the next cover, keeps the engine's shadow with the turn and
// gives the cover back to the engine once its picture shows the new one.
bool App::step_card_turn(double t) {
  auto& f = flip_;
  // the window stopped drawing before it was through (Windows refused its pixels): the engine's
  // cover comes back at once and a song waiting for the edge opens
  if (f.stage < 6 && turn_.done()) { end_card_flip(true); return false; }
  // the window moved under the cover (dragged, snapped): the turn lands at once
  RECT cover{}, clip{};
  if (in_sizemove_ || !card_cover_screen(cover, clip) || cover.left != turn_cover_.left || cover.top != turn_cover_.top ||
      cover.right != turn_cover_.right) {
    end_card_flip(true);
    return false;
  }
  {
    const auto ct = play_.cover_tilt();
    turn_.tilt(ct.rx * 3.14159265358979 / 180, ct.ry * 3.14159265358979 / 180, ct.k);
  }
  // The engine leaves the cover out and its shadow fades as the window's comes in (and back at the
  // end): the window says how much of the shadow it drew, the engine draws the rest
  if (f.stage < 5) {
    const double hide = std::max(0.002, turn_.shadow());
    if (std::fabs(hide - cov_hide_) > 0.004 || (hide >= 0.999 && cov_hide_ < 0.999)) { cov_hide_ = hide; update_blur_cloud(); }
  }
  if (f.stage == 1) {
    const double k = clamp01((t - f.t0) / CoverTurnTiming::kOut);
    if (!play_.in.paused && !eof_) set_switch_gain(1 - k);  // the old song breathes out with it
    if (!turn_.edge()) return true;
    f.stage = 2; f.t0 = t;
    const uint32_t id = f.id;
    queue_play_now(id);
    // (the engine has the next song's load before this: the old one stops there, never louder)
    set_switch_gain(1);
    if (loaded_) {  // nothing opened (the file is gone, nothing follows): the old cover turns back
      if (card_art_ && card_art_->art) turn_.set_to(card_art_->art);
      turn_restart_t_ = t;
    } else if (next_art_ && next_art_id_ == id) {
      // the card's cover from now on is this one (read already, at this size)
      card_art_ = next_art_;
      card_art_path_ = path_;
    }
    return true;
  }
  if (f.stage == 2) {  // at the edge, then turning in (the window's own clock)
    if (next_art_ && next_art_->done.load() && next_art_->art) turn_.set_to(next_art_->art);
    if (turn_.landed()) { f.stage = 3; f.t0 = t; }
    return true;
  }
  if (f.stage == 3) {
    // flat: the engine takes the cover back once the song plays (its first pictures come a moment
    // after it starts) or, if it never does, after a while: first the shadows hand over
    const bool shown = turn_restart_t_ >= 0 && t - turn_restart_t_ >= 0.15 && !music_gap_;
    if (shown || t - f.t0 > 2.5) { turn_.release(); f.stage = 4; f.t0 = t; }
    return true;
  }
  if (f.stage == 4) {  // the shadows handed over: the engine's cover comes back under the window's
    if (turn_.shadow_out() || t - f.t0 > 0.6) {
      cov_hide_ = 0;
      flip_ry_ = 0;
      update_blur_cloud();
      f.stage = 5; f.t0 = t;
    }
    return true;
  }
  if (f.stage == 5) {  // a few of the engine's pictures with the cover, then the window fades
    if (t - f.t0 >= 0.10) { turn_.finish(); f.stage = 6; f.t0 = t; }
    return true;
  }
  if (turn_.done() || t - f.t0 > 1.0) {
    turn_.abort();
    turn_on_ = false;
    next_art_.reset();
    flip_ = {};
    return false;
  }
  return true;
}

bool App::step_card_flip(double t) {
  auto& f = flip_;
  if (!f.stage) return false;
  if (hidden_now() || !mini_player_ || mode_ != Mode::Play) { end_card_flip(true); return false; }
  if (turn_on_) return step_card_turn(t);
  if (f.stage == 1) {
    const double k = clamp01((t - f.t0) / kFlipOut);
    flip_ry_ = f.from + (kFlipEdge - f.from) * k * k;  // eases away, faster and faster (ease in)
    if (!play_.in.paused && !eof_) set_switch_gain(1 - k);  // the old song breathes out with it
    update_blur_cloud();
    if (k < 1) return true;
    flip_ry_ = kFlipEdge;
    update_blur_cloud();
    f.stage = 2; f.t0 = t;
    const uint32_t id = f.id;
    queue_play_now(id);
    // (the engine has the next song's load before this: the old one stops there, never louder)
    set_switch_gain(1);
    if (loaded_) { f.stage = 3; f.t0 = t; }  // nothing opened (the file is gone, nothing follows): the cover turns back
    return true;
  }
  if (f.stage == 2) {  // edge-on while the next song opens (its first picture turns it in)
    if (t - f.t0 > kFlipWait) { f.stage = 3; f.t0 = t; }
    return true;
  }
  // in from the other side, past flat by a hair, then flat
  const double k = clamp01((t - f.t0) / kFlipIn);
  // ease out, back: leaves the edge at the turn-away's speed (slope 3*c3 - 2*c1 = 4.2: 2 x 0.42/0.20),
  // passes flat by about 6 degrees, lands
  const double c1 = kFlipBack, c3 = c1 + 1, x = k - 1;
  const double e = 1 + c3 * x * x * x + c1 * x * x;
  flip_ry_ = -kFlipEdge * (1 - e);
  update_blur_cloud();
  if (k >= 1) { flip_ = {}; flip_ry_ = 0; update_blur_cloud(); return false; }
  return true;
}

// The cover look and the spectrum (and the wide and tall compositions) are graphs built around
// the cover's track. Swapping that graph while the song plays leaves the engine either waiting
// for a cover picture it does not send again (silence) or ending the file (the album skips a
// song). So the song is opened again at the same moment, behind the same pixel transition the
// audio-track change uses: a clean start, like any song, with the new look.
void App::reopen_music() {
  if (!audio_only_ || !waveform_active_ || path_.empty()) return;
  if (amix_ && cover_vid_ <= 0) {  // tracks mixed, no cover track in the graph: rebuilt in place
    int look = cover_fallback_ ? 1 : store_.music_look;
    cover_art_ = look != 1;
    music_look_on_ = look;
    play_.in.cover_art = cover_art_;
    mpv_.set("file-local-options/scale", cover_art_ ? "spline36" : "nearest");
    mpv_.set("file-local-options/dscale", cover_art_ ? "mitchell" : "nearest");
    change_audio_graph(audio_mix_graph(play_.in.amix_ids, true, visual_cover(), music_scene_, music_look_on_), 0);
    update_blur_cloud();
    return;
  }
  reopen_song();
}

void App::reopen_song() {
  if (!audio_only_ || !waveform_active_ || path_.empty()) return;
  if (!loaded_) return;  // a load is already on its way: configure_audio_mode reads the choice
  if (fullscreen_transition_) settle_fullscreen_motion();
  auto& a = audio_switch_;
  if (!a.stage || a.stage == 5) {
    // a song that has ended opens again from its start, paused, instead of ending once more
    a.position = eof_ ? 0 : mpv_.get_double("time-pos", play_.in.time);
    a.paused = eof_ || (pause_pending_ ? pause_target_ : play_.in.paused);
    a.stage = 1; a.at = now(); a.retries = 0;
    // the song keeps playing while the picture goes out: the sound stops only for the reopen
    // itself (the place is read again just before it)
  }
  a.reopen = true; a.graph.clear(); a.track = 0; a.dirty = true;
  if (hidden_now() || reduce_motion()) {
    if (!eof_) a.position = mpv_.get_double("time-pos", a.position);
    a.dirty = false; a.stage = 3; a.at = now(); music_reopening_ = true; release_video_window(); mpv_.cmd({"loadfile", path_, "replace"});
  }
  ensure_anim();
}

// Smoothing: off by default (a choice in "..."), offered where it helps (a film whose frames do
// not land evenly on the screen's refreshes), never on battery. The blend only exists while the
// film is timed by the display (display-resample): the engine silently ignores it otherwise, so
// a film with smoothing on is timed that way whatever its size (big films are otherwise timed by
// the sound, which draws fewer pictures).
void App::apply_smooth() {
  const double hz = pacer_.interval() > 0 ? 1.0 / pacer_.interval() : 60.0;
  const bool helps = !audio_only_ && cadence_uneven(video_fps_, hz);
  play_.in.smooth_available = helps;
  play_.in.smooth_motion = store_.smooth_motion;
  const bool want = helps && store_.smooth_motion && !on_battery();
  smooth_on_ = want;
  if (mpv_ok_) mpv_.set_async("interpolation", want ? "yes" : "no");
  send_sync();
  if (!audio_only_ && loaded_) {  // so it can be checked: cue.log says when it works, and why not
    char b[160];
    std::snprintf(b, sizeof b, "smooth motion %s (%.3f fps on %.2f Hz%s%s)", want ? "on" : "off", video_fps_, hz,
                  helps ? "" : ", even cadence", on_battery() ? ", on battery" : "");
    if (b != smooth_logged_) { smooth_logged_ = b; log(b); }
  }
  play_.changed(now());
}

// The film's timing, one place for all three reasons: covered by a full-screen window (the
// sound times it), smoothing on (the display times it, or the blend does nothing), otherwise
// what decide_window chose for its size. File-local: the next file starts from the default.
void App::send_sync() {
  if (!mpv_ok_ || audio_only_) return;
  if (!sync_decided_ && !smooth_on_ && !film_idle_) return;  // nothing to say before the film's size is known
  std::string s = film_idle_ ? "audio" : smooth_on_ ? "display-resample" : video_sync_.empty() ? "display-resample" : video_sync_;
  if (s == sync_sent_) return;
  sync_sent_ = s;
  mpv_.set_async("file-local-options/video-sync", s);
}

void App::configure_audio_mode() {
  mpv_node node{};
  std::vector<Track> tracks;
  if (mpv_.api().get_property(mpv_.handle(), "track-list", MPV_FORMAT_NODE, &node) >= 0) {
    tracks = Mpv::parse_tracks(&node);
    mpv_.api().free_node_contents(&node);
  }
  const RecentItem* recent = store_.find(path_);
  int64_t id = audio_visual_track(tracks, recent ? recent->aid : -1,
                                  recent && !recent->aid_lang.empty() ? recent->aid_lang : store_.alang);
  audio_only_ = id > 0;
  play_.in.audio_only = audio_only_;
  waveform_active_ = false;
  cover_vid_ = audio_only_ ? audio_cover_track(tracks) : 0;
  cover_scene_ = cover_art_ = false;
  play_.in.cover_art = false;
  play_.in.has_cover = cover_vid_ > 0;
  play_.in.music_look = store_.music_look;
  play_.in.music_mono = store_.music_mono;
  play_.in.cover_tilt = store_.cover_tilt;
  play_.in.settings_examples = store_.settings_examples;
  play_.in.mini_sub_size = store_.mini_sub_size;
  // (a song keeps the interface's scene until its own is decided below: apply_smooth redraws
  // the interface on the way, and a scene off for that moment put the full bar in the film's
  // place, and back, at every song change)
  if (!audio_only_) play_.in.cover_scene = false;
  // File-local options are restored by mpv on unload: a following film must
  // never inherit the waveform, low-resolution scaler or audio clock.
  if (!audio_only_) {
    // the film's own size and rate (its first real video track)
    int vw = 0, vh = 0; video_fps_ = 0;
    early_shape_ = {};
    for (auto& t : tracks) if (t.type == "video" && !t.albumart) {
      vw = t.demux_w; vh = t.demux_h; video_fps_ = t.demux_fps;
      // the window's shape, known before the first picture when the engine says how the film is
      // turned (see on_file_loaded)
      if (vw > 0 && vh > 0 && t.demux_rotation >= 0)
        early_shape_ = {(int64_t)std::lround(vw * (t.demux_par > 0 ? t.demux_par : 1.0)), vh, t.demux_rotation};
      break;
    }
    // super resolution, NVIDIA's switch decides (Cue has none): on there, a film smaller than
    // the screen, on power. Set before the film starts, so it never rebuilds while it plays.
    vsr_on_ = false;
    if (!on_battery() && primary_gpu_nvidia() && nvidia_vsr_switch() == 1) {
      MONITORINFO mi{sizeof(mi)};
      GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
      double k = vsr_scale(vw, vh, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
      if (k > 0) {
        char vf[96];
        std::snprintf(vf, sizeof vf, "@cuevsr:d3d11vpp=scale=%.2f:scaling-mode=nvidia", k);
        vsr_on_ = mpv_.set("file-local-options/vf", vf);
        log(std::string("super resolution ") + (vsr_on_ ? "on" : "refused") + " (" + std::to_string(vw) + "x" + std::to_string(vh) + ")");
      }
    }
    // Build a restored video mix in on_preloaded, before decoders start. Rebuilding after
    // FILE_LOADED used to compete with the opening iris and flush the first 4K frames.
    std::vector<Track> audios;
    for (const auto& t : tracks) if (t.type == "audio") audios.push_back(t);
    std::vector<int64_t> mix;
    if (recent && recent->aid == -2) {
      if (recent->aid_lang.empty()) for (const auto& t : audios) mix.push_back(t.id);
      else for (size_t i = 0; i < recent->aid_lang.size();) {
        size_t e = recent->aid_lang.find(',', i);
        if (e == std::string::npos) e = recent->aid_lang.size();
        const int64_t wanted = std::atoll(recent->aid_lang.substr(i, e - i).c_str());
        for (const auto& t : audios) if (t.id == wanted) mix.push_back(t.id);
        i = e + 1;
      }
    } else if ((!recent || recent->aid < 0) && looks_like_multitrack_recording(audios)) {
      for (const auto& t : audios) mix.push_back(t.id);
    }
    std::sort(mix.begin(), mix.end());
    mix.erase(std::unique(mix.begin(), mix.end()), mix.end());
    if (mix.size() >= 2 && mpv_.set("file-local-options/lavfi-complex", audio_mix_graph(mix, false))) {
      amix_ = play_.in.amix = true;
      play_.in.amix_ids = mix;
    }
    apply_smooth();
    frame_plain();  // a film: never a song's framing
    end_music_gap();
    return;
  }
  video_fps_ = 0;
  apply_smooth();
  mpv_.set("file-local-options/force-window", "yes"); // controls even without a video stream
  mpv_.set("file-local-options/audio-display", "no");
  mpv_.set("file-local-options/video-sync", "audio"); // visualization never retimes the sound
  // a song starts from its first sample: the engine must not cut the sound's start to meet the
  // picture's first frame (the cover alone makes only two pictures a second)
  mpv_.set("file-local-options/initial-audio-sync", "no");
  mpv_.set("file-local-options/keep-open", "always");
  mpv_.set("file-local-options/aid", std::to_string(id));
  play_.in.aid = id;
  // the light show's meter: the loudness and the highs of every audio frame, read as the
  // filter's metadata. It changes nothing in the sound; if it fails, the engine leaves it out.
  // (the cover alone has no light show: no meter either, nothing read 30 times a second)
  // (a song opened in the mini player's card has the person's look too: the card masks the
  // spectrum's wings, and back in the full player they are there at once, nothing reopened)
  light_meter_ = (cover_fallback_ || store_.music_look != 2) &&
                 mpv_.set("file-local-options/af",
                          "@cuelvl:lavfi=[astats=metadata=1:reset=1:measure_perchannel=Zero_crossings_rate:measure_overall=RMS_level]");
  light_latency_ = std::clamp(mpv_.get_double("audio-buffer", 0.2), 0.0, 1.0) + 0.04;
  light_cover_job_ = cover_job_ ? cover_job_ : request_cover(path_);  // read since the song was opened
  if (!room_kept_) bg_ready_t_ = -1;  // (song to song: the room keeps its light; its colours cross over)
  if (!waveform_fallback_) {
    // the cover look is a picture (smooth scaling keeps the cover sharp); the spectrum alone is
    // pixels (nearest keeps every dot square)
    const int look = cover_fallback_ ? 1 : store_.music_look;
    const bool cover = look != 1;
    music_scene_ = MusicScene::wide();  // one picture; the window frames it (music_fit_window)
    if (cover && cover_vid_ > 0 && cover_job_) {
      // the bars were looked for since the song was opened (usually done by now); the engine is
      // waiting for this graph, so not for long. Only when it is the same picture the engine has.
      const ULONGLONG until = GetTickCount64() + 60;
      while (!cover_job_->done.load() && GetTickCount64() < until) Sleep(1);
      const auto& j = *cover_job_;
      int tw = 0, th = 0;
      for (auto& t : tracks) if (t.id == cover_vid_) { tw = t.demux_w; th = t.demux_h; }
      if (j.done.load() && j.found && tw > 0 && th > 0 && j.full_w > 0 && j.full_h > 0 &&
          std::fabs((double)tw / th - (double)j.full_w / j.full_h) < 0.01) {
        music_scene_.crop_x = j.x; music_scene_.crop_y = j.y; music_scene_.crop_w = j.w; music_scene_.crop_h = j.h;
      }
    }
    play_.in.music_scene = music_scene_;
    framing_sent_ = {1e9, 0, 0};          // file-local: the engine starts every file unframed
    // (the mini player's card stays the card from song to song: it used to fall back to the full
    // composition here, its spectrum and words over the cover, until the next resize)
    music_frame_ = mini_player_ && !fullscreen_ ? MusicFrame::Card : music_frame_for(cw_, ch_, s_);
    view_t0_ = -1;
    // (no framing yet, but its frame already: the list open beside the cover is judged on it. A
    // song picked in the open list used to be framed centred, the list over it, and stay so)
    play_.in.music_view = {};
    play_.in.music_view.frame = music_frame_;
    mpv_.set("file-local-options/scale", cover ? "spline36" : "nearest");
    mpv_.set("file-local-options/dscale", cover ? "mitchell" : "nearest");
    // several tracks heard at once (remembered for this file, or a recording in parts): the mix is
    // part of the graph from the start, so the song never has to rebuild it while it plays
    std::vector<int64_t> mix;
    {
      std::vector<Track> audios;
      for (auto& t : tracks) if (t.type == "audio") audios.push_back(t);
      if (recent && recent->aid == -2) {
        for (size_t i = 0; i < recent->aid_lang.size();) {
          size_t e = recent->aid_lang.find(',', i);
          if (e == std::string::npos) e = recent->aid_lang.size();
          int64_t m = std::atoll(recent->aid_lang.substr(i, e - i).c_str());
          if (m > 0) mix.push_back(m);
          i = e + 1;
        }
        if (recent->aid_lang.empty()) for (auto& t : audios) mix.push_back(t.id);
      } else if ((!recent || recent->aid < 0) && looks_like_multitrack_recording(audios)) {
        for (auto& t : audios) mix.push_back(t.id);
      }
      std::sort(mix.begin(), mix.end());
      mix.erase(std::unique(mix.begin(), mix.end()), mix.end());
      if (mix.size() < 2) mix.clear();
    }
    std::string graph = mix.empty() ? audio_waveform_graph(id, cover ? cover_vid_ : 0, music_scene_, look)
                                    : audio_mix_graph(mix, true, cover ? cover_vid_ : 0, music_scene_, look);
    waveform_active_ = mpv_.set("file-local-options/lavfi-complex", graph);
    if (waveform_active_ && !mix.empty()) { amix_ = play_.in.amix = true; play_.in.amix_ids = mix; }
    if (!waveform_active_) log("audio waveform filter unavailable; keeping audio controls");
    cover_scene_ = waveform_active_;  // both looks are a scene: the words, the list, the framing
    mono_k_ = store_.music_mono && cover_scene_ && look != 2 ? 1.0 : 0.0;  // grey from the song's first picture
    cover_art_ = waveform_active_ && cover;
    music_look_on_ = look;
    play_.in.cover_scene = cover_scene_;
    play_.in.cover_art = cover_art_;
    if (cover_scene_) {
      step_music_view(now());  // framed for this window from its first picture
      // and said right now, before the engine draws it (sent on its way, the first picture could
      // come out unframed: the whole scene, small, with its spectrum)
      char b[3][32];
      std::snprintf(b[0], 32, "%.5f", framing_sent_.zoom); std::snprintf(b[1], 32, "%.6f", framing_sent_.pan_x); std::snprintf(b[2], 32, "%.6f", framing_sent_.pan_y);
      if (framing_sent_.zoom < 1e8) {
        mpv_.set("video-zoom", b[0]);
        mpv_.set("video-pan-x", b[1]);
        mpv_.set("video-pan-y", b[2]);
      }
    } else frame_plain();
    play_.in.music_leaving = false;  // its words come in
    wing_hold_ = wing_sent_ && cover_scene_;  // (the last picture's wings stay masked until this one's is up)
    wing_hold_t_ = 0;
    // the captions under the cover: the tags when there are some, else the file's own name
    std::string title = mpv_.get_string("metadata/by-key/title"), artist = mpv_.get_string("metadata/by-key/artist"),
                album = mpv_.get_string("metadata/by-key/album");
    // under the title: who, and from which album, when the file says
    if (!album.empty() && album != title) artist = artist.empty() ? album : artist + " \xc2\xb7 " + album;
    if (title.empty()) {
      title = file_name(path_);
      size_t dot = title.rfind('.');
      if (dot != std::string::npos && dot > 0) title.resize(dot);
    }
    play_.in.music_title = title;
    play_.in.music_artist = artist;
  }
  if (waveform_fallback_) { frame_plain(); play_.in.cover_scene = false; }  // (no picture of its own: nothing to frame)
  end_music_gap();
  if (!audio_only_) return;
  if (queue_.active()) find_next_episode();  // the queue was read before this file was known as music
  else music_album_queue();
}

void App::on_client_message(const mpv_event_client_message* m) {
  if (m->num_args < 3 || std::string(m->args[0]) != "key-binding") return;
  std::string name = m->args[1];
  char st = m->args[2][0];
  bool press = st == 'd' || st == 'r' || st == 'p';
  // Buttons come directly from Win32, including WM_LBUTTONDBLCLK as a second
  // down. mpv's synthetic double-click bindings must not inject extra clicks.
  if (name == "mb_left" || name == "mb_right") return;
  if (!press) return;
  if (name == "wheel_up" || name == "wheel_down") { play_.wheel(name == "wheel_up" ? 1 : -1, now()); run_actions(); ensure_anim(); return; }
  if (name == "ignore") return;
  if (name == "lbracket") name = "[";
  else if (name == "rbracket") name = "]";
  else if (name == "backslash") name = "\\";
  else if (name == "qmark") name = "?";
  bool both=name.rfind("cs_",0)==0;
  bool shift = both||name.rfind("s_", 0) == 0, ctrl = both||name.rfind("c_", 0) == 0;
  std::string k = both?name.substr(3):(shift || ctrl) ? name.substr(2) : name;
  key(k, shift, ctrl);
}

// Read the engine's already-probed metadata only when requested; no new decoder,
// file scan, stat on a network path, background timer or ffprobe process.
void App::refresh_file_info() {
  MediaDetails info;
  info.name = file_name(path_);
  info.audio_only = audio_only_;
  if (mpv_ok_ && loaded_) {
    info.bytes = mpv_.get_double("file-size", -1);
    info.duration = mpv_.get_double("duration", play_.in.dur);
    info.container = mpv_.get_string("file-format");
    if (!audio_only_) {
      info.width = (int)mpv_.get_double("current-tracks/video/demux-w", 0);
      info.height = (int)mpv_.get_double("current-tracks/video/demux-h", 0);
      if (info.width <= 0) info.width = (int)mpv_.get_double("video-params/w", 0);
      if (info.height <= 0) info.height = (int)mpv_.get_double("video-params/h", 0);
      info.fps = mpv_.get_double("current-tracks/video/demux-fps", 0);
      info.video_codec = mpv_.get_string("current-tracks/video/codec");
    }
    info.audio_codec = mpv_.get_string("current-tracks/audio/codec");
    info.audio_layout = mpv_.get_string("audio-params/hr-channels");
    info.channels = (int)mpv_.get_double("audio-params/channel-count", 0);
    info.sample_rate = mpv_.get_double("audio-params/samplerate", 0);
  }
  play_.in.file_info = media_info_fields(info);
  ++play_.in.file_info_revision;
  play_.changed(now());
}

void App::on_file_loaded() {
  playback_priority(true);  // never slowed by Windows while something plays (back to normal at home)
  // The path property may still describe the previous file when FILE_LOADED
  // arrives. open() already owns the requested path; never replace it with a stale observation.
  closing_video_ = false;
  hide_video_pending_ = false;
  loaded_ = true;
  open_failures_ = 0;
  loaded_at_ = now();
  trace("file loaded");
  double dur = play_.in.dur;
  store_.touch(path_, dur, (int64_t)std::time(nullptr));
  play_.in.style = store_.style_for(path_);
  play_.in.default_style = store_.default_style;
  play_.place = store_.dock;
  ext_subs_ = 0;
  for (auto& t : play_.in.subs) if (t.external) ++ext_subs_;
  set_title(file_name(path_));
  const bool reopened = music_reopening_;  // the same song again: the interface stays as it is
  music_reopening_ = false;
  // a new song while music plays: it comes in through the pixels (see PLAYBACK_RESTART)
  music_intro_ = !reopened && audio_only_ && waveform_active_ && mode_ == Mode::Play && !reduce_motion() && !audio_switch_.stage && !flip_.stage;
  if (music_intro_) set_snap(kSwitchDim, snap_px(1.0, ch_), 1.0);  // held in big pixels until it plays
  play_.set_base_scale(s_); play_.resize(cw_, ch_, play_scale(store_.ui_scale));
  // a song that follows a song keeps the controls (and an open list) where they are: only the
  // picture changes, through the pixels; anything else starts the interface afresh
  const bool song_follows = !reopened && audio_only_ && last_was_music_ && mode_ == Mode::Play;
  last_was_music_ = audio_only_;
  if (!reopened && !song_follows) play_.reset(now());
  // black over the picture until its first frame is ready, then the pixel iris opens it
  first_frame_ = false;
  first_frame_t_ = -1;
  if (audio_only_) {
    // Audio does not have to wait for a native video frame to reveal its controls.
    first_frame_ = true;
    background_warmup_.first_frame(now());
  }
  logged_hw_ = false;
  accent_k_target_ = 0;  // neutral until this film's first colours are read
  accent_req_t_ = -10;
  if (!reopened && !song_follows) {
    // a new file may want a different window shape; the same song again, or the next song of
    // an album, keeps the window the person has (a small window stays small)
    geom_done_ = false;
    user_sized_ = false;
    request_video_geometry();
    // A film's window takes its shape now, from what the file says, while the engine starts its
    // decoder: the picture opens with its first frame. It waited for the engine's first picture,
    // then 60 ms, then the 0.26 s glide (about 0.4 s of black, and the glide drawn while the film
    // had started). The engine's own measure, a moment later, corrects it if it differs.
    if (!audio_only_ && early_shape_.width > 0 && early_shape_.height > 0) {
      vid_w_ = early_shape_.width; vid_h_ = early_shape_.height; vid_rot_ = early_shape_.rotation;
      trace("early geometry " + std::to_string(vid_w_) + "x" + std::to_string(vid_h_) + " rotate " + std::to_string(vid_rot_));
      decide_window();
    }
  } else {
    // the window is the person's: nothing reshapes it for this song either (the engine's
    // picture reports its shape again as the song starts, and that used to resize the window)
    user_sized_ = true;
  }
  if (!reopened && !song_follows) play_.begin_cover();
  play_.prepare(now());
  upload_overlays();
  if (mode_ != Mode::Play) {
    // the home finishes its goodbye first (a dropped file gets swallowed), then the video shows
    if (home_.leaving() && !home_.leave_done(now())) reveal_pending_ = true;
    else reveal_play();
  }
  if (resume_toast_ > 0) play_.toast(fmt_time(resume_toast_), "resume", now());
  resume_toast_ = -1;
  if (!tracks_applied_ && (!play_.in.subs.empty() || !play_.in.audios.empty())) apply_media_tracks();
  for (auto& s : pending_subs_) mpv_.cmd({"sub-add", s, "select"});
  pending_subs_.clear();
  apply_style();
  save_state();
  end_what(false);
  queue_refresh();
  remote_publish();
  if (!audio_only_) SetTimer(hwnd_, T_THUMB, (UINT)(PlaybackWarmup::kAtmosphereDelay * 1000), nullptr);
  else KillTimer(hwnd_, T_THUMB);
  if (hidden_ && !audio_only_ && !film_idle_) { film_idle_ = true; send_sync(); }  // still covered
  update_blur_cloud();  // the cover's shader pass follows this file's look at once
  ensure_anim();
}

void App::reveal_play() {
  reveal_pending_ = false;
  if (mode_ == Mode::Play) return;
  mode_ = Mode::Play;
  home_.drag_leave(now());
  set_edge_highlight(false);
  upload_overlays();
  ShowWindow(video_, SW_SHOW);
  home_canvas_ = Canvas{};  // the hidden home no longer needs a full-window bitmap
  home_back_ = Canvas{};
  SetTimer(hwnd_, T_POLL, poll_slow_ ? 250 : 33, nullptr);
  // the place in a film is written at most once a minute while it plays (pause, close, the end
  // and Windows shutting down write it at once): a crash loses a minute, the disk is left alone
  SetTimer(hwnd_, T_SAVE, 300000, nullptr);  // where a film is, every 5 minutes (and at pause, stop, close)
  try_start_iris();
  ensure_anim();
}

// The picture opens once the window has its shape: a new film reshapes the window (decide_window,
// a 0.26 s glide) a moment after its first frame. Opening during the glide drew the iris in the
// old shape, moved it every frame and made the engine draw a new size each frame (the stutter).
// A film whose shape never comes (or a slow one) opens anyway, at most 0.9 s after its first frame.
bool App::window_settled() const {
  if (audio_only_ || user_sized_ || fullscreen_ || mini_player_ || IsZoomed(hwnd_) || IsIconic(hwnd_)) return true;
  if (geom_.on || !geom_done_) return false;
  const double frame = pacer_.ok() ? pacer_.interval() : 1.0 / 60;
  return now() - geom_end_t_ >= 2 * frame;  // a couple of frames drawn at the new size
}

void App::try_start_iris() {
  const double t = now();
  if (first_frame_ && first_frame_t_ < 0) first_frame_t_ = t;
  const bool shaped = window_settled() || (first_frame_t_ >= 0 && t - first_frame_t_ > 0.9) || t - loaded_at_ > 2.0;
  if (!audio_switch_.stage && mode_ == Mode::Play && play_.iris_waiting() && first_frame_ && shaped) {
    play_.start_iris(now());
    log("picture opens (open " + std::to_string((int)((t - open_started_) * 1000)) + "ms)");
    ensure_anim();
  }
}

void App::on_end_file(const mpv_event_end_file* e) {
  if (e && e->reason == MPV_END_FILE_REASON_ERROR) end_music_gap();
  if (e && e->reason == MPV_END_FILE_REASON_ERROR && audio_only_ && waveform_active_ && cover_art_ && !cover_fallback_) {
    // the cover look could not start: the same file again, with the spectrum alone
    release_video_window();
    cover_fallback_ = true;
    cover_scene_ = cover_art_ = waveform_active_ = false;
    loaded_ = false;
    log("cover scene initialization failed; retrying with the spectrum");
    mpv_.cmd({"loadfile", path_, "replace"});
    return;
  }
  if (e && e->reason == MPV_END_FILE_REASON_ERROR && audio_only_ && waveform_active_ && !waveform_fallback_) {
    release_video_window();
    waveform_fallback_ = true;
    waveform_active_ = false;
    loaded_ = false;
    log("waveform initialization failed; retrying audio without visualization");
    mpv_.cmd({"loadfile", path_, "replace"});
    return;
  }
  if (!loaded_ && e && e->reason == MPV_END_FILE_REASON_ERROR && mode_ == Mode::Play && queue_.active() && open_failures_ < 3) {
    // a queued file that would not open: say so and play on, never sit on an empty screen
    ++open_failures_;
    play_.toast(i18n::format(str::queue_skipped, {{"name", file_stem(path_)}}), nullptr, now());
    log("queue: a file could not be opened; skipping");
    if (const auto* next = queue_following()) { queue_play(next->id); return; }
  }
  if (!loaded_) {
    reveal_pending_ = false;
    if (mode_ == Mode::Home) home_.cancel_leave(now());
    if (e && e->reason == MPV_END_FILE_REASON_ERROR) {
      home_.show_notice(tr(str::notice_open_failed), now());
      log("could not open: " + path_);
      ensure_anim();
    }
    return;
  }
  if (e && e->reason == MPV_END_FILE_REASON_EOF) queue_next_episode();
  save_position(e && e->reason == MPV_END_FILE_REASON_EOF);
  loaded_ = false;
  // the engine may destroy its video window next: let go of it first
  if (e && (e->reason == MPV_END_FILE_REASON_EOF || e->reason == MPV_END_FILE_REASON_ERROR)) release_video_window();
}

void App::save_position(bool finished) {
  if (path_.empty() || !loaded_) return;
  if (store_.set_position(path_, finished || audio_only_ ? 0 : last_time_, play_.in.dur)) save_state();
}

void App::save_state() { SetTimer(hwnd_, T_PERSIST, 400, nullptr); }

namespace {
struct SaveJob { Store snapshot; std::filesystem::path path; };
DWORD WINAPI save_worker(void* context) {
  auto* job = static_cast<SaveJob*>(context);
  bool ok = false;
  try { ok = job->snapshot.save(job->path); } catch (...) { /* report through the thread result */ }
  delete job;
  return ok ? 0 : 1;
}
}

void App::save_now(bool synchronous) {
  KillTimer(hwnd_, T_PERSIST);
  if (save_thread_) {
    // Never overlap writers to state.json.tmp. Slow disks do not block controls.
    if (WaitForSingleObject(save_thread_, synchronous ? INFINITE : 0) == WAIT_TIMEOUT) {
      SetTimer(hwnd_, T_PERSIST, 100, nullptr);
      return;
    }
    DWORD result = 1;
    GetExitCodeThread(save_thread_, &result);
    CloseHandle(save_thread_);
    save_thread_ = nullptr;
    if (result) log("could not save previous state");
  }
  if (!synchronous) {
    auto* job = new SaveJob{store_, state_path_}; // worker owns an immutable snapshot
    save_thread_ = CreateThread(nullptr, 0, save_worker, job, 0, nullptr);
    if (save_thread_) return;
    delete job;
  }
  // Closing/logoff: the window is already hidden; persist the latest state last.
  if (!store_.save(state_path_)) log("could not save state");
}

// ── what's new ──────────────────────────────────────────────────────
void App::show_news(bool asked) {
  if (asked) news_due_ = true;
  // only over a calm home: a file on its way, the welcome or the pairing scene come first
  if (!news_due_ || news_host_.visible() || mode_ != Mode::Home || onboarding_ || !pending_open_.empty() || scene_.visible() || IsIconic(hwnd_)) return;
  news_due_ = false;
  store_.news_seen = CUE_NEWS_VERSION;
  save_state();
  news_.version = CUE_NEWS_VERSION;
  news_host_.show(&news_, hwnd_, s_, news_host_.now());
  log("news");
}

// Closed by hand: it comes back only with the next version.
// The survey's invitation (see HomeView): only on the home, never over the welcome, the news or
// a scene; answered or declined, it never comes back.
bool App::survey_wanted() const {
  if (store_.survey == "answered" || store_.survey == "declined") return false;
  const bool week = store_.installed > 0 && (int64_t)std::time(nullptr) - store_.installed >= 7 * 86400;
  return (store_.survey == "due" || week) && mode_ == Mode::Home && !onboarding_ && !news_due_ && !news_host_.visible() &&
         !scene_.visible() && pending_open_.empty();
}

void App::answer_survey(bool yes) {
  store_.survey = yes ? "answered" : "declined";
  save_state();
  home_.survey = false;
  if (yes && (INT_PTR)ShellExecuteW(hwnd_, L"open", L"https://tally.so/r/8NzQZA", nullptr, nullptr, SW_SHOWNORMAL) <= 32)
    home_.show_notice(tr(str::toast_link_failed), now());
}

void App::close_news_line() {
  home_.news = false;
  store_.news_closed = CUE_NEWS_VERSION;
  save_state();
}

// ── opening ─────────────────────────────────────────────────────────
void App::open(std::string path, std::vector<std::string> subs) {
  // Own the argument before clearing next_ep_ (manual, phone and EOF callers
  // can all pass that member). Empty input must not tear down current playback.
  if (path.empty()) { log("ignored empty open request"); return; }
  if(folder_scan_&&folder_scan_->replace)folder_scan_->cancelled=true;
  if (!queue_.select_path(path)) queue_.clear();
  next_advance_.clear();
  next_thumb_job_.reset(); next_thumb_requested_ = false; play_.set_next_details("");
  media_idle_ = false;
  news_due_ = false;
  if (news_host_.visible()) news_host_.close(news_host_.now());
  settle_fullscreen_motion();  // a fullscreen movement in progress lands at once
  if (music_intro_) { music_intro_ = false; end_snap(); }
  seek_hold_ = -1;
  thumb_stop();
  // (song to song, the music's light carries on whatever the halo outside does: the card has none)
  const bool music_to_music = play_.in.audio_only && remote::is_audio_name(path) && mode_ == Mode::Play;
  reset_atmosphere(music_to_music);
  room_kept_ = music_to_music;
  // the last song's picture stays on screen while the next opens: it keeps its framing and passes
  if (music_to_music && audio_only_ && cover_scene_ && waveform_active_ && mpv_ok_) begin_music_gap();
  else { end_music_gap(); wing_hold_ = false; }
  if (onboarding_) {
    onboarding_ = false;
    store_.onboarded = true;
    save_state();
    home_.enter(now(), true);
  }
  if (mode_ == Mode::Home && mpv_ok_) { home_.leave(now(), swallow_next_); ensure_anim(); }
  swallow_next_ = false;
  if (!mpv_ok_) {
    // engine_err_ belongs to the engine thread until it reports back: only our own flag is read here
    if (engine_failed_) { home_.show_notice(home_.engine_problem, now()); ensure_anim(); }
    else pending_open_ = path;
    return;
  }
  if (loaded_) save_position();
  next_cancelled_ = false; next_count_ = -1; next_auto_ = false; next_ep_.clear(); play_.in.has_next = false; KillTimer(hwnd_, T_NEXT);
  loaded_ = false; pause_pending_=false; ++pause_request_;
  audio_only_ = waveform_active_ = waveform_fallback_ = cover_fallback_ = cover_scene_ = cover_art_ = false;
  cover_vid_ = 0;
  // until the engine says, the name tells: music keeps its controls between two songs, and
  // the picture's menu never offers to save a frame of a song
  // a song after a song keeps the scene (and a list open beside it) until the engine has
  // looked at the new one, so nothing jumps between them
  const bool song_to_song = play_.in.audio_only && remote::is_audio_name(path) && mode_ == Mode::Play;
  play_.in.audio_only = remote::is_audio_name(path);
  // a song's cover may be a video's frame boxed in black bars: found while the engine opens it
  // its folder is read on a worker while the engine opens it (up next, the album)
  if (path.find("://") == std::string::npos) list_folder(std::filesystem::path(widen(path)).parent_path().wstring());
  else { folder_list_.reset(); folder_wait_ = false; }
  // one read of it serves the picture's crop, the light's colours and the media card
  cover_job_ = play_.in.audio_only ? request_cover(path) : nullptr;
  if (!song_to_song) play_.in.has_cover = play_.in.cover_scene = play_.in.cover_art = false;
  play_.in.file_info.clear();
  ++play_.in.file_info_revision;
  vid_w_ = vid_h_ = vid_rot_ = 0;
  media_geometry_.reset();
  geometry_query_pending_=false;
  geometry_query_dirty_=false;
  geom_.on=false; geom_done_=false; user_sized_=false; pending_ar_=0;
  film_idle_ = false;  // file-local: the next file starts without it
  engine_warns_ = 0;
  sync_decided_ = false; sync_sent_.clear(); smooth_on_ = false;
  KillTimer(hwnd_,T_GEOM);
  path_ = path;
  open_started_ = now();
  open_timed_ = false;
  trace("open requested");
  observed_path_.clear();
  pending_subs_ = std::move(subs);
  clear_audio_mix();
  mpv_.cmd({"loadfile", path, "replace"});
}

void App::handle_files(const std::vector<std::string>& files) {
  std::vector<std::string> media, subs, folders;
  for (auto& f : files) {
    DWORD attr = GetFileAttributesW(widen(f).c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) folders.push_back(f);
    else (is_subtitle_path(f) ? subs : media).push_back(f);
  }
  // A folder brings its media (read on a worker, in name order, never recursively): into the
  // open list, or as a new list that starts playing. The files dropped with it come first.
  if (!folders.empty()) {
    const bool into_list = mode_ == Mode::Play && play_.queue_open();
    if (!media.empty()) { if (into_list) queue_add(media); else { queue_add(media, true); } }
    queue_folder(folders.front(), !into_list && media.empty());
    return;
  }
  if (!media.empty()) {
    if(mode_==Mode::Play&&play_.queue_open()) {queue_add(media);return;}
    queue_.clear();
    if (media.size() > 1) queue_.append(media);
    open(media[0], subs); queue_refresh(); return;
  }
  if (subs.empty()) return;
  if (mode_ == Mode::Play && loaded_) {
    for (auto& s : subs) mpv_.cmd({"sub-add", s, "select"});  // "CC ✓" comes from the track list
  } else {
    home_.show_notice(tr(str::notice_open_video_first), now());
    ensure_anim();
  }
}

void App::go_home() {
  exit_mini(false);  // back home: the full window, at once
  playback_priority(false);
  if (folder_scan_) folder_scan_->cancelled=true;
  // Retain the worker slot until cancellation completes; no new scan can pile up.
  queue_.clear(); queue_refresh();
  next_thumb_job_.reset(); next_thumb_requested_ = false; play_.set_next_details("");
  media_idle_ = false;
  next_advance_.clear();
  scene_paused_ = false;
  if (scene_.visible()) scene_.close(now());
  KillTimer(hwnd_, T_NEXT);
  next_count_ = -1; next_cancelled_ = true;
  play_.next_notice("", now());
  thumb_stop();
  clear_audio_mix();
  end_what(false);
  next_ep_.clear();
  background_warmup_.reset();
  reveal_pending_ = false;
  home_.enter(now());
  mode_ = Mode::Home;  // `loaded_` stays: the end-file event may still arrive and must save the position
  end_card_flip(false);
  end_music_gap();
  taskbar_sync();
  set_hidden(false);  // the home is only drawn while in view, and only checked while playing
  last_was_music_ = false;
  remove_overlays();
  KillTimer(hwnd_, T_SAVE);
  SetTimer(hwnd_, T_POLL, poll_slow_ ? 250 : 33, nullptr);  // the eyes follow the cursor on the home
  // the video window is hidden only once the engine has finished destroying its own window
  hide_video_pending_ = true;
  hide_video_since_ = now();
  try_hide_video();
  settle_fullscreen_motion();  // a fullscreen movement in progress lands at once
  if (fullscreen_) perform_fullscreen_switch();
  // Both portrait and ultrawide films must give the home its normal 16:9 canvas.
  pending_ar_=0;
  adapt_window(16.0 / 9.0);
  set_title("Cue");
  refresh_recents();
  home_.set_size(cw_, ch_, s_);
  InvalidateRect(hwnd_, nullptr, FALSE);
  ensure_anim();
}

// Checking whether files still exist can take seconds (a sleeping external drive, a network
// share), so it runs on a worker thread; the list is drawn at once and corrected when it answers.
struct RecentsCheck { HWND hwnd; UINT msg; unsigned gen; std::vector<std::wstring> paths; };
static DWORD WINAPI recents_worker(void* p) {
  auto* job = static_cast<RecentsCheck*>(p);
  auto* missing = new std::vector<std::wstring>;
  for (auto& w : job->paths)
    if (GetFileAttributesW(w.c_str()) == INVALID_FILE_ATTRIBUTES) missing->push_back(w);
  if (!PostMessageW(job->hwnd, job->msg, job->gen, (LPARAM)missing)) delete missing;
  delete job;
  return 0;
}

void App::refresh_recents(bool check) {
  if (check && !store_.recents.empty()) {
    auto* job = new RecentsCheck{hwnd_, WM_APP_RECENTS, ++recents_gen_, {}};
    for (auto& r : store_.recents) job->paths.push_back(widen(r.path));
    if (HANDLE th = CreateThread(nullptr, 0, recents_worker, job, 0, nullptr)) CloseHandle(th);
    else delete job;
  }
  // the list reads like a person would say it: one entry per show, what is left, the episode
  std::vector<RecentFile> files;
  for (auto& r : store_.recents) {
    if (missing_.count(widen(r.path))) continue;  // gone: not shown, not deleted
    files.push_back({r.path, store_.resume_point(r.path), r.dur});
  }
  auto span = [](double seconds) {
    int h = 0, m = 0;
    split_minutes(seconds, h, m);
    return h > 0 ? i18n::format(str::home_hours_minutes, {{"h", std::to_string(h)}, {"m", (m < 10 ? "0" : "") + std::to_string(m)}})
                 : i18n::format(str::home_minutes, {{"m", std::to_string(m)}});
  };
  std::vector<HomeRow> rows;
  shown_paths_.clear();
  for (auto& e : live_recents(files, 6)) {
    shown_paths_.push_back(e.path);
    HomeRow hr;
    hr.name = e.title;
    hr.audio = e.audio;
    if (e.pos > 0 && e.dur > 0) {
      hr.right = i18n::format(str::home_left, {{"time", span(std::max(0.0, e.dur - e.pos))}});
      hr.progress = std::clamp(e.pos / e.dur, 0.0, 1.0);
    } else if (e.dur > 0) hr.right = span(e.dur);
    if (!e.episode.empty())
      hr.sub = e.episodes > 1 ? e.episode + " \xc2\xb7 " + i18n::plural(str::home_episodes_n, e.episodes) : e.episode;
    else hr.sub = tr(e.audio ? str::home_kind_audio : str::home_kind_video);
    rows.push_back(hr);
  }
  home_.eggs_seen = store_.eggs_seen;
  home_.set_rows(rows);
}

// ── subtitle style ──────────────────────────────────────────────────
// Asks the engine, once and while it is idle, which property names it understands
// (they changed across mpv versions). After this, style writes never wait.
void App::resolve_props() {
  SubStyle a, b;
  b.bg_op = 0;
  for (const SubStyle& st : {a, b})
    for (auto& ps : sub_style_props(st, SubKind::Text)) {
      if (prop_name_.count(ps.names[0])) continue;
      std::string used;
      for (auto& n : ps.names) {
        char* v = mpv_.api().get_property_string(mpv_.handle(), n.c_str());
        if (v) { mpv_.api().free(v); used = n; break; }
      }
      prop_name_[ps.names[0]] = used;
    }
  // background-box arrived in mpv 0.38; older engines get opaque-box
  std::string ver = mpv_.get_string("mpv-version");  // "mpv 0.32.0-515-g..." / "mpv v0.39.0"
  int maj = 0, min = 0;
  size_t k = ver.find_first_of("0123456789");
  if (k != std::string::npos) std::sscanf(ver.c_str() + k, "%d.%d", &maj, &min);
  has_background_box_ = maj > 0 || min >= 38;
  log("engine " + ver);
}

void App::apply_style(bool live) {
  if (!mpv_ok_) return;
  for (auto& ps : sub_style_props(play_.in.style, play_.in.kind,subtitle_typography_scale(cw_,ch_))) {
    if (ps.names[0] == "sub-pos") continue;
    std::string key = ps.names[0];
    auto known = prop_name_.find(key);
    if (live && known != prop_name_.end()) {
      // while a slider moves: only what changed, never waiting for the engine
      if (applied_[key] == ps.value) continue;
      if (!known->second.empty()) mpv_.set_async(known->second.c_str(), ps.value);
      applied_[key] = ps.value;
      continue;
    }
    // names were resolved once at startup (resolve_props): every write is asynchronous
    if (known == prop_name_.end() || known->second.empty()) continue;
    std::string value = ps.value;
    if (key == "sub-border-style" && value == "background-box" && !has_background_box_) value = "opaque-box";
    // never resend a value the engine already has: a subtitle font write makes it reload
    // its fonts, and that froze the picture on every resize and fullscreen change
    auto it = applied_.find(key);
    if (it == applied_.end() || it->second != value) mpv_.set_async(known->second.c_str(), value);
    applied_[key] = value;
  }
  (void)live;
  std::string pos = subtitle_position_string(play_.lifted_pos());
  if (applied_["sub-pos"] != pos) mpv_.set_async("sub-pos", pos);
  applied_["sub-pos"] = pos;
}

// ── actions from the playback UI ────────────────────────────────────
void App::request_pause(bool paused) {
  if (paused && !play_.in.paused) save_position();  // the place is saved when it stops moving, not on a clock
  if (audio_switch_.stage && audio_switch_.stage < 5) {
    audio_switch_.paused = paused; play_.in.paused = paused;
    return;
  }
  pause_target_=paused; pause_pending_=true;
  if(pause_request_ < (1ull<<32)) pause_request_=1ull<<32;
  ++pause_request_; int flag=paused;
  if(mpv_.api().set_property_async(mpv_.handle(),pause_request_,"pause",MPV_FORMAT_FLAG,&flag)<0)
    pause_pending_=false;
}

void App::run_actions() {
  auto acts = play_.take_actions();
  double t = now();
  bool live_style = false;
  for (auto& a : acts) {
    switch (a.a) {
      case PA::NextEpisode: if(queue_.active()) {if(auto* i=queue_following())queue_play(i->id);} else if (!next_ep_.empty()) open(next_ep_); break;
      case PA::PreviousInQueue: if(queue_.active()) {if(auto* i=shuffle_previous())queue_play(i->id);} break;
      case PA::CancelNext: next_advance_.clear(); next_cancelled_ = true; next_count_ = -1; play_.next_notice("", t); break;
      case PA::TogglePause:
        if (eof_) {  // play at the end: from the beginning
          eof_ = false;
          play_.in.ended = false;
          mpv_.cmd({"seek", "0", "absolute"});
          request_pause(false);
        } else {
          request_pause(a.v>0);
        }
        break;
      case PA::SeekRel:
        cancel_audio_switch();
        hold_seek((seek_hold_ >= 0 ? seek_hold_ : play_.in.time) + a.v);
        seek_soft({"seek", std::to_string(a.v), "relative"});
        break;
      case PA::SeekAbs:
        cancel_audio_switch();
        if (seek_in_flight_) seek_pending_ = a.v;  // one seek in flight: scrubbing never queues up
        else { seek_in_flight_ = true; mpv_.cmd({"seek", std::to_string(a.v), "absolute+keyframes"}, kReplySeek); }
        break;
      case PA::SeekAbsExact: cancel_audio_switch(); seek_pending_ = -1; hold_seek(a.v); seek_soft({"seek", std::to_string(a.v), "absolute+exact"}); break;
      case PA::Volume: mpv_.set_double_async("volume", a.v); play_.in.volume = a.v; break;
      case PA::ToggleMute: mpv_.cmd({"cycle", "mute"}); break;
      case PA::Fullscreen: toggle_fullscreen(); break;
      case PA::SelectSub:
        select_subtitle(a.id);
        break;
      case PA::SelectAudio: select_audio(a.id); play_.announce_audio(t); break;
      case PA::AudioMix: {
        // a.id: the chosen tracks as a mask of their places in the list (0: every track)
        std::vector<int64_t> ids;
        uint64_t mask = (uint64_t)a.id;
        for (size_t i = 0; i < play_.in.audios.size() && i < 64; ++i)
          if (!mask || (mask >> i & 1)) ids.push_back(play_.in.audios[i].id);
        set_audio_mix(ids, true);
        play_.announce_audio(t);
        play_.changed(t);
        break;
      }
      case PA::AddSubFile: {
        auto f = open_file_dialog(hwnd_, DialogKind::Subtitle);
        for (auto& s : f) mpv_.cmd({"sub-add", s, "select"});
        break;
      }
      case PA::QueueAdd: { auto f=open_file_dialog(hwnd_,DialogKind::Video); if(!f.empty())queue_add(f); break; }
      case PA::QueueFolder: { auto p=pick_folder_dialog(hwnd_); if(!p.empty())queue_folder(p,false); break; }
      case PA::QueuePlay: queue_play((uint32_t)a.id); break;
      case PA::QueueRemove: if(queue_.remove((uint32_t)a.id))queue_refresh(); break;
      case PA::QueueMove: if(queue_.move((uint32_t)a.id,a.v<0?-1:1))queue_refresh(); break;
      case PA::QueueReorder: if(queue_.move_to((uint32_t)a.id,(int)a.v))queue_refresh(); break;
      case PA::QueueClear: queue_clear(); break;
      case PA::OpenFile: { auto f = open_file_dialog(hwnd_, DialogKind::Video); if (!f.empty()) handle_files(f); break; }
      case PA::CloseVideo: release_video_window(); save_position(); mpv_.cmd({"stop"}); break;
      case PA::StyleLive: live_style = true; break;  // coalesced: applied once per frame
      case PA::Style:
        apply_style();
        store_.set_style_for(path_, play_.in.style);
        save_state();
        break;
      case PA::StyleDefault:
        store_.default_style = play_.in.style;
        play_.in.default_style = play_.in.style;
        save_state();
        play_.changed(t);
        break;
      case PA::SubDelay: {
        double v = play_.in.sub_delay + a.v;
        mpv_.set_double_async("sub-delay", v);
        play_.in.sub_delay = v;
        play_.toast(i18n::format(str::toast_sub_delay, {{"delay", fmt_delay(v)}}), nullptr, t);
        break;
      }
      case PA::SubVisible:
        if (a.v > 0 && play_.in.sub_selection.shown(play_.in.sid) == 0 && !play_.in.subs.empty()) select_subtitle(play_.in.subs[0].id);
        set_subtitle_visibility(a.v > 0);
        if (!play_.in.subs.empty() && !play_.panel_open()) play_.toast(tr(a.v > 0 ? str::toast_sub_on : str::toast_sub_off), nullptr, t);
        break;
      case PA::Placement: store_.dock = play_.place; save_state(); break;
      case PA::WinMin: ShowWindow(hwnd_, SW_MINIMIZE); break;
      case PA::WinPin: toggle_pin(); break;
      case PA::SmoothMotion: store_.smooth_motion = a.v > 0.5; save_state(); apply_smooth(); break;
      case PA::MiniPlayer: if (a.v > 0.5) enter_mini(); else exit_mini(true); break;
      case PA::WinMax: toggle_maximize(); break;
      case PA::WinClose: PostMessageW(hwnd_, WM_CLOSE, 0, 0); break;
      case PA::WinDragBegin: begin_window_drag(); break;
      case PA::WinDragEnd: break;
      case PA::Preview: if (!preview_.failed) thumb_request(a.v); break;
      case PA::Speed:
        mpv_.set_double_async("speed", a.v);  // pitch stays natural (audio-pitch-correction)
        play_.in.speed = a.v;
        break;
      case PA::SaveFrame: save_frame(a.v > 0); break;
      case PA::OpenFrameFolder: if (!last_frame_.empty()) show_in_folder(last_frame_); break;
      case PA::FileInfo: refresh_file_info(); break;
      case PA::OpenLicenses: open_licenses(hwnd_, exe_dir()); break;
      case PA::OpenEmail: {
        // No mail app registered (common with webmail): the address goes to the clipboard instead.
        const std::wstring address = widen(CUE_CONTACT_EMAIL);
        const std::wstring link = L"mailto:" + address + L"?subject=Cue%20" + widen(CUE_VERSION);
        if ((INT_PTR)ShellExecuteW(hwnd_, L"open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32) break;
        bool copied = false;
        if (OpenClipboard(hwnd_)) {
          EmptyClipboard();
          size_t bytes = (address.size() + 1) * sizeof(wchar_t);
          if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* dst = GlobalLock(mem)) { memcpy(dst, address.c_str(), bytes); GlobalUnlock(mem); }
            if (SetClipboardData(CF_UNICODETEXT, mem)) copied = true; else GlobalFree(mem);
          }
          CloseClipboard();
        }
        play_.toast(tr(copied ? str::toast_email_copied : str::toast_link_failed), nullptr, t);
        break;
      }
      case PA::OpenLinkedIn:
      case PA::OpenGitHub: {
        const wchar_t* url = a.a == PA::OpenLinkedIn ? L"https://www.linkedin.com/in/gabrielbenzoni" : L"https://github.com/tinypeep";
        if ((INT_PTR)ShellExecuteW(hwnd_, L"open", url, nullptr, nullptr, SW_SHOWNORMAL) <= 32)
          play_.toast(tr(str::toast_link_failed), nullptr, t);
        break;
      }
      case PA::Shuffle:
        store_.shuffle = play_.in.shuffle = a.v > 0.5;
        shuffle_reset();
        save_state();
        find_next_episode();  // what follows is another song now
        play_.changed(t);
        break;
      case PA::Loop:
        // 2: repeat the file forever (a music video, a loop); 1: repeat the queue (an album
        // plays on from its first song); 0: off. Stays as chosen for the next files too
        store_.loop = a.v > 1.5;
        store_.loop_all = a.v > 0.5 && a.v < 1.5;
        play_.in.loop = store_.loop;
        play_.in.loop_all = store_.loop_all;
        find_next_episode();  // the list's end now leads back to its start, or not
        mpv_.set_async("loop-file", store_.loop ? "inf" : "no");
        if (store_.loop && eof_) {  // switched on at the end: it starts over right away
          eof_ = false;
          play_.in.ended = false;
          mpv_.cmd({"seek", "0", "absolute"});
          request_pause(false);
        }
        save_state(); play_.changed(now()); ensure_anim();
        break;
      case PA::SubEdit:
        if (a.v > 0) {
          // placing the subtitle: the film holds still on this line, and loses its colour
          sub_edit_resume_ = !play_.in.paused && !eof_;
          if (sub_edit_resume_) request_pause(true);
          auto [x, y] = sub_center_out();
          subfx_ = {true, t, -1, x, y, x, y};
          sub_lift_.target = 1.045;  // picked up
        } else if (subfx_.on) {
          if (sub_edit_resume_) request_pause(false);
          sub_edit_resume_ = false;
          auto [x, y] = sub_center_out();
          subfx_.t_out = t; subfx_.bx = x; subfx_.by = y;
          sub_lift_.target = 1.0;  // put down
        }
        if (reduce_motion()) {
          if (subfx_.t_in >= 0) subfx_.t_in = t - 10;
          if (subfx_.t_out >= 0) subfx_.t_out = t - 10;
          sub_lift_.snap(1.0);
        }
        step_sub_fx(t, 0);
        ensure_anim();
        break;
      case PA::MusicLook: set_music_look((int)a.v); break;
      case PA::MusicMono: set_music_mono(a.v > 0.5); break;
      case PA::SettingsExamples: store_.settings_examples = play_.in.settings_examples = a.v > 0.5; save_state(); break;
      case PA::MiniSubSize: store_.mini_sub_size = play_.in.mini_sub_size = std::clamp((int)std::lround(a.v), 0, 2); save_state(); ensure_anim(); break;
      case PA::CoverTilt: store_.cover_tilt = play_.in.cover_tilt = a.v > 0.5; if (!play_.in.cover_tilt) play_.pointer_left(); save_state(); play_.changed(now()); ensure_anim(); break;
      case PA::ControlsFree:
        store_.controls_free = a.v > 0;
        play_.in.controls_free = store_.controls_free;
        save_state(); play_.changed(now()); play_.activity(now()); ensure_anim();
        break;
      case PA::UiScale: set_ui_scale((int)a.v); break;
      case PA::Language: set_language_pref((int)a.v); break;
      case PA::Remote: open_remote_scene(); break;
      case PA::Update:
        if (upd_ == Upd::Available) start_update();
        else check_updates(true);
        break;
      case PA::AmbientStrength:
        store_.ambient_strength = std::clamp(a.v, 0.05, 1.0);
        play_.in.ambient_strength = store_.ambient_strength;
        save_state();  // debounced: dragging the slider writes once at the end
        if (ambient_.active()) {
          // applied right away (only the opacity changes, no repaint)
          double tn = now(), adt = std::clamp(tn - atmo_t_, 0.0, 0.1);
          atmo_t_ = tn;
          step_atmosphere(adt);
        }
        break;
      case PA::Ambient:
        store_.ambient = a.v > 0;
        play_.in.ambient = store_.ambient;
        save_state();
        play_.changed(t);
        break;
      case PA::SubLift: {
        std::string pos = subtitle_position_string(a.v);
        if (applied_["sub-pos"] != pos) { mpv_.set_async("sub-pos", pos); applied_["sub-pos"] = pos; }
        break;
      }
    }
  }
  if (live_style) apply_style(true);
}

// ── overlays: the playback UI is composited by mpv itself ──────────
void App::update_blur_cloud() {
  if (!mpv_ok_) return;
  auto cloud = play_.blur_cloud();
  if (mode_ != Mode::Play || audio_only_ || !first_frame_ || vid_w_ <= 0 || vid_h_ <= 0)
    cloud.alpha = 0;
  double tn = now();

  auto u = cloud_uniforms(cloud, cw_, ch_, vid_w_, vid_h_, vid_rot_, dpi_);
  char b[512];
  std::snprintf(b, sizeof b,
    "cloud_alpha=%.4f,cloud_x=%.6f,cloud_y=%.6f,cloud_rx=%.6f,cloud_ry=%.6f,cloud_sx=%.6f,cloud_sy=%.6f",
    cloud.alpha, u.x,u.y,u.rx,u.ry,u.sx,u.sy);
  // Ignore geometry while inactive: no redraws or option churn in clean playback.
  std::string options = cloud.alpha > 0 ? b : "cloud_alpha=0";
  // the opening iris, drawn by the engine's shader on the picture (it was a full-window bitmap
  // made and sent 60 times a second: 33 MB a frame in 4K, while the film was starting)
  // (also while the home still says goodbye: the picture is black before its window shows)
  const double iris = play_.in.iris_engine && loaded_ ? play_.iris_open(tn) : -1.0;
  bool fx = snap_.on || subfx_.on || iris >= 0;
  if (snap_.on) {
    char f[200];
    std::snprintf(f, sizeof f, ",cue_on=1,cue_dark=%.4f,cue_px=%.1f,cue_gray=%.4f,cue_ar=10,cue_br=0,cue_ir=-1", snap_.dark, snap_.px, snap_.gray);
    options += f;
  } else if (iris >= 0 && !subfx_.on) {
    char f[200];
    std::snprintf(f, sizeof f, ",cue_on=1,cue_dark=0,cue_px=1.0,cue_gray=0,cue_ar=10,cue_br=0,cue_cell=%.0f,cue_ir=%.4f",
                  std::max(2.0, std::round(3 * play_.in.s)), iris);
    options += f;
  } else if (fx) {
    // subtitle edit: grey and a little darker (so the line being placed stands out), spreading
    // from the subtitle in blocks of 8 interface pixels, and the colour coming back the same way
    auto ring = [](double t0, double dur, double tn) {
      if (t0 < 0) return 0.0;
      double x = std::clamp((tn - t0) / dur, 0.0, 1.0);
      return 1 - (1 - x) * (1 - x);
    };
    double ar = ring(subfx_.t_in, .42, tn), br = ring(subfx_.t_out, .45, tn);
    char f[360];
    std::snprintf(f, sizeof f,
      ",cue_on=1,cue_dark=0.32,cue_px=1.0,cue_gray=1,cue_cell=%.0f,cue_ax=%.5f,cue_ay=%.5f,cue_ar=%.4f,cue_bx=%.5f,cue_by=%.5f,cue_br=%.4f,cue_ir=-1",
      std::max(4.0, std::round(8 * play_.in.s)), subfx_.ax, subfx_.ay, ar, subfx_.bx, subfx_.by, br);
    options += f;
  } else if (snap_.was_on) {
    options += ",cue_on=0,cue_ir=-1";
  }
  // the music passes: while a song opens after a song, they stay as last sent (music_gap_)
  std::string mus;
  const bool was_wing = wing_sent_;
  // music with its cover: the soft shadow always, the tilt and its light under the pointer
  // (the card's drawn cover: the engine has none, but its cover pass still paints the room under
  // the square, hidden for good, so a turn never shows the black the spectrum left there)
  const bool drawn = card_drawn();
  bool cov = (cover_art_ || drawn) && mode_ == Mode::Play && first_frame_ && !hidden_;
  if (cov) {
    auto ct = play_.cover_tilt();
    // the shader works in the shown part of the picture: the cover's place there
    const MusicScene& S = music_scene_;
    MusicView v = play_.in.music_view.f > 0 ? play_.in.music_view : music_view(S, MusicFrame::Scene, cw_, ch_, ch_);
    MusicVisible vis = music_visible(S, v, cw_, ch_);
    if (vis.w < 1 || vis.h < 1) vis = {0, 0, (double)std::max(1, cw_), (double)std::max(1, ch_)};
    char f[220];
    std::snprintf(f, sizeof f, ",cov_on=1,cov_x=%.5f,cov_y=%.5f,cov_hw=%.5f,cov_rx=%.4f,cov_ry=%.4f,cov_k=%.3f,cov_hide=%.3f",
                  (v.ox + (S.x + S.side / 2.0) * v.f - vis.x) / vis.w, (v.oy + (S.y + S.side / 2.0) * v.f - vis.y) / vis.h,
                  S.side / 2.0 * v.f / vis.w,
                  ct.rx * 3.14159265358979 / 180, ct.ry * 3.14159265358979 / 180 + flip_ry_, ct.k, drawn ? 1.0 : cov_hide_);
    mus += f;
  } else if (cov_sent_) {
    mus += ",cov_on=0";
  }
  // music's light show, around the cover (or the spectrum standing in its place)
  bool lit = audio_only_ && cover_scene_ && music_look_on_ != 2 && mode_ == Mode::Play && first_frame_ && !hidden_ && light_k_ > 0.002;
  if (lit) {
    const MusicScene& S = music_scene_;
    MusicView v = play_.in.music_view.f > 0 ? play_.in.music_view : music_view(S, MusicFrame::Scene, cw_, ch_, ch_);
    MusicVisible vis = music_visible(S, v, cw_, ch_);
    if (vis.w < 1 || vis.h < 1) vis = {0, 0, (double)std::max(1, cw_), (double)std::max(1, ch_)};
    const MusicLightFrame& f = music_light_.frame();
    double wr[3] = {0, 0, 0}, wa[3] = {0, 0, 0};
    for (size_t i = 0; i < f.waves.size() && i < 3; ++i) { wr[i] = f.waves[f.waves.size() - 1 - i].r; wa[i] = f.waves[f.waves.size() - 1 - i].a; }
    char f2[1400];
    std::snprintf(f2, sizeof f2,
                  ",lit_on=1,lit_x=%.5f,lit_y=%.5f,lit_hw=%.5f,lit_k=%.3f,lit_e=%.3f,lit_s=%.3f,lit_br=%.3f,lit_ry=%.2f,lit_t=%.4f,lit_cov=%d,"
                  "lit_w0r=%.3f,lit_w0a=%.3f,lit_w1r=%.3f,lit_w1a=%.3f,lit_w2r=%.3f,lit_w2a=%.3f,lit_c0=%.3f,lit_c1=%.3f,lit_c2=%.3f,lit_c3=%.3f",
                  (v.ox + (S.x + S.side / 2.0) * v.f - vis.x) / vis.w, (v.oy + (S.y + S.side / 2.0) * v.f - vis.y) / vis.h,
                  S.side / 2.0 * v.f / vis.w, light_k_, f.energy, f.section, f.breath, reduce_motion() ? 0.35 : 1.0, f.turn - std::floor(f.turn), cover_art_ ? 1 : 0,
                  wr[0], wa[0], wr[1], wa[1], wr[2], wa[2], light_kick_[0], light_kick_[1], light_kick_[2], light_kick_[3]);
    mus += f2;
  } else if (lit_sent_) {
    mus += ",lit_on=0";
  }
  // the room's soft light around the cover (cover looks): drawn by the shader, centred on the
  // cover, always (ambient light or not); it fades in once the cover's colours are known
  // (the card in the spectrum-alone look has the cover look's room: its cover is Cue's own)
  bool bg = audio_only_ && cover_scene_ && (music_look_on_ != 1 || drawn) && mode_ == Mode::Play && first_frame_;
  if (bg) {
    const MusicScene& S = music_scene_;
    MusicView v = play_.in.music_view.f > 0 ? play_.in.music_view : music_view(S, MusicFrame::Scene, cw_, ch_, ch_);
    MusicVisible vis = music_visible(S, v, cw_, ch_);
    if (vis.w < 1 || vis.h < 1) vis = {0, 0, (double)std::max(1, cw_), (double)std::max(1, ch_)};
    double k = bg_ready_t_ < 0 ? 0.0 : reduce_motion() ? 1.0 : std::clamp((tn - bg_ready_t_) / 0.3, 0.0, 1.0);
    // the list's header: the cover a thumbnail, its light a quiet halo (the picture's frame is small
    // round it, and a room light cut by it would show as a lit box behind the header)
    k *= 1 - 0.7 * play_.list_header_mix();
    char f5[200];
    std::snprintf(f5, sizeof f5, ",bg_on=1,bg_x=%.5f,bg_y=%.5f,bg_hw=%.5f,bg_k=%.3f",
                  (v.ox + (S.x + S.side / 2.0) * v.f - vis.x) / vis.w, (v.oy + (S.y + S.side / 2.0) * v.f - vis.y) / vis.h,
                  S.side / 2.0 * v.f / vis.w, k * k * (3 - 2 * k));
    mus += f5;
  } else if (bg_sent_) {
    mus += ",bg_on=0";
  }
  // a song's mini player (the card): the wings step out, the cover keeps its light
  // (and the list's header: the cover a thumbnail, the wings would run into its words)
  // (the card in the spectrum-alone look: all of the spectrum steps out, not only beside the cover)
  bool wing = (audio_only_ && cover_scene_ && mode_ == Mode::Play && ((mini_player_ && !fullscreen_) || play_.list_cover().w > 0) && music_look_on_ != 1) ||
              (wing_hold_ && audio_only_ && cover_scene_ && mode_ == Mode::Play) || drawn;
  if (wing) {
    const MusicScene& S = music_scene_;
    MusicView v = play_.in.music_view.f > 0 ? play_.in.music_view : music_view(S, MusicFrame::Scene, cw_, ch_, ch_);
    MusicVisible vis = music_visible(S, v, cw_, ch_);
    if (vis.w < 1 || vis.h < 1) vis = {0, 0, (double)std::max(1, cw_), (double)std::max(1, ch_)};
    char f7[180];
    std::snprintf(f7, sizeof f7, ",wing_on=1,wing_all=%d,wing_x=%.5f,wing_y=%.5f,wing_hw=%.5f", drawn ? 1 : 0,
                  (v.ox + (S.x + S.side / 2.0) * v.f - vis.x) / vis.w, (v.oy + (S.y + S.side / 2.0) * v.f - vis.y) / vis.h,
                  S.side / 2.0 * v.f / vis.w);
    mus += f7;
    wing_sent_ = true;
  } else if (wing_sent_) {
    mus += ",wing_on=0,wing_all=0";
    wing_sent_ = false;
  }
  // the cover's eight border colours, for the light show and the room's light
  if (lit || bg) {
    char f6[600];
    int n6 = 0;
    for (int i = 0; i < 8 && n6 >= 0 && n6 < (int)sizeof f6 - 64; ++i)
      n6 += std::snprintf(f6 + n6, sizeof f6 - n6, ",lit_p%dr=%.3f,lit_p%dg=%.3f,lit_p%db=%.3f", i, light_pal_[i].r / 255.0, i,
                          light_pal_[i].g / 255.0, i, light_pal_[i].b / 255.0);
    mus += f6;
  }
  // the window's own colour around the music's picture: the room's far light, in the cover's
  // colours (grey with the music in black and white), black for anything else
  Rgb backdrop{0, 0, 0};
  if (bg && !music_gap_) {
    int r = 0, g = 0, b = 0;
    for (const Rgb& c : light_pal_) { r += c.r; g += c.g; b += c.b; }
    const double k = bg_ready_t_ < 0 ? 0.0 : reduce_motion() ? 1.0 : std::clamp((tn - bg_ready_t_) / 0.3, 0.0, 1.0);
    backdrop = music_backdrop(Rgb{(uint8_t)(r / 8), (uint8_t)(g / 8), (uint8_t)(b / 8)}, k * k * (3 - 2 * k) * (1 - 0.7 * play_.list_header_mix()));
    if (mono_k_ > 0.001) {
      const double m = std::min(1.0, mono_k_), l = 0.2126 * backdrop.r + 0.7152 * backdrop.g + 0.0722 * backdrop.b;
      backdrop = {(uint8_t)std::lround(backdrop.r + (l - backdrop.r) * m), (uint8_t)std::lround(backdrop.g + (l - backdrop.g) * m),
                  (uint8_t)std::lround(backdrop.b + (l - backdrop.b) * m)};
    }
  } else if (music_gap_) {
    backdrop = backdrop_sent_;  // a song opening after a song: the room keeps its colour meanwhile
  }
  // the list over the cover darkens the picture (the veil pass): the window around it darkens as
  // much, or the picture's frame would show as a darker band against it
  if (const double vl = audio_only_ && mode_ == Mode::Play ? play_.list_veil() : 0.0; vl > 0.001) {
    const double k = 1 - 0.86 * std::min(1.0, vl);
    backdrop = {(uint8_t)std::lround(backdrop.r * k), (uint8_t)std::lround(backdrop.g * k), (uint8_t)std::lround(backdrop.b * k)};
  }
  if (backdrop.r != backdrop_sent_.r || backdrop.g != backdrop_sent_.g || backdrop.b != backdrop_sent_.b || !backdrop_known_) {
    backdrop_sent_ = backdrop;
    backdrop_known_ = true;
    char bc[16];
    std::snprintf(bc, sizeof bc, "#%02X%02X%02X", backdrop.r, backdrop.g, backdrop.b);
    mpv_.set_async("background-color", bc);
  }
  // music: where the picture stops short of the window (much wider or taller than it), it fades
  // into the window's colour instead of ending on a straight line; never over the cover or wings
  bool edge = audio_only_ && cover_scene_ && mode_ == Mode::Play && first_frame_ && !hidden_;
  if (edge) {
    const MusicScene& S = music_scene_;
    MusicView v = play_.in.music_view.f > 0 ? play_.in.music_view : music_view(S, MusicFrame::Scene, cw_, ch_, ch_);
    MusicVisible vis = music_visible(S, v, cw_, ch_);
    double e[4] = {0, 0, 0, 0};
    if (vis.w >= 1 && vis.h >= 1) {
      const double x0 = v.ox + S.left_x() * v.f, x1 = v.ox + (S.right_x() + S.wing_w) * v.f;
      const double y0 = v.oy + std::min(S.y, S.wing_y()) * v.f, y1 = v.oy + std::max(S.y + S.side, S.wing_y() + S.wing_h) * v.f;
      const double base = 0.24 * std::min(vis.w, vis.h);
      auto width = [&](bool bar, double gap, double size) { return bar ? std::clamp(std::min(base, gap * 0.85), 0.0, size) / size : 0.0; };
      e[0] = width(vis.x > 0.5, x0 - vis.x, vis.w);
      e[1] = width(vis.x + vis.w < cw_ - 0.5, vis.x + vis.w - x1, vis.w);
      e[2] = width(vis.y > 0.5, y0 - vis.y, vis.h);
      e[3] = width(vis.y + vis.h < ch_ - 0.5, vis.y + vis.h - y1, vis.h);
    }
    edge = e[0] > 0.001 || e[1] > 0.001 || e[2] > 0.001 || e[3] > 0.001;
    if (edge) {
      char f3[200];
      std::snprintf(f3, sizeof f3, ",edge_on=1,edge_l=%.4f,edge_r=%.4f,edge_t=%.4f,edge_b=%.4f,edge_cr=%.4f,edge_cg=%.4f,edge_cb=%.4f", e[0], e[1],
                    e[2], e[3], backdrop.r / 255.0, backdrop.g / 255.0, backdrop.b / 255.0);
      mus += f3;
    }
  }
  if (!edge && edge_sent_) mus += ",edge_on=0";
  // music's list open over the cover: the picture steps back (eased by the interface's list_k)
  const double veil = audio_only_ && mode_ == Mode::Play ? play_.list_veil() : 0.0;
  if (veil > 0.001) { char fv[32]; std::snprintf(fv, sizeof fv, ",veil=%.3f", veil); mus += fv; }
  else if (veil_sent_ > 0.001) mus += ",veil=0";
  veil_sent_ = veil;
  // music in black and white (eased by step_mono)
  bool mono = mono_k_ > 0.001 && audio_only_ && mode_ == Mode::Play;
  if (mono) {
    char f4[32];
    std::snprintf(f4, sizeof f4, ",mono=%.3f", std::min(1.0, mono_k_));
    mus += f4;
  } else if (mono_sent_) {
    mus += ",mono=0";
  }
  if (music_gap_) { mus = music_opts_; wing_sent_ = was_wing; }
  else music_opts_ = mus;
  options += mus;
  cloud_pending_ = false;
  if (options == cloud_options_) return;
  if (cloud_inflight_) { cloud_pending_ = true; return; }
  if ((cloud.alpha > 0 || fx || cov || lit || bg || edge || mono) && !cloud_cadence_.ready(tn, .0015)) { cloud_pending_ = true; return; }
  if (!music_gap_) {
    cov_sent_ = cov;
    lit_sent_ = lit;
    bg_sent_ = bg;
    edge_sent_ = edge;
    mono_sent_ = mono;
  }
  snap_.was_on = fx;
  cloud_cadence_.mark(tn);
  cloud_options_ = options;
  cloud_inflight_ = mpv_.cmd({"set", "glsl-shader-opts", options}, 103) >= 0;
  if (!cloud_inflight_) cloud_options_.clear();
}

void App::upload_overlays() {
  if (!mpv_ok_) return;
  update_blur_cloud();
  for (int i = 0; i < PlayView::L_COUNT; ++i) {
    Layer& L = play_.layers[i];
    if (!L.dirty) continue;
    Ov& o = ov_[i];
    if (o.inflight || o.removing) continue;  // preserve the renderer's lease until its acknowledgement
    // at most 60 pictures a second per layer: the engine redraws its OSD for each one, and a
    // tooltip gliding at 120 Hz over a 4K film was enough to make the video hitch
    double tn = now();
    if (L.visible && o.shown && !o.cadence.ready(tn, .0015)) continue;
    o.cadence.mark(tn);
    L.dirty = false;
    if (L.visible && L.c.w > 0 && L.c.h > 0) {
      int b = o.on_screen == 0 ? 1 : 0;  // never the buffer the engine is showing
      o.buf[b] = L.c.px;
      int x = std::max(0, L.x), y = std::max(0, L.y);
      o.inflight = true;
      o.sending = b;
      int r = mpv_.cmd({"overlay-add", std::to_string(i), std::to_string(x), std::to_string(y),
                        "&" + std::to_string((uintptr_t)o.buf[b].data()), "0", "bgra", std::to_string(L.c.w),
                        std::to_string(L.c.h), std::to_string(L.c.w * 4)},
                       kReplyOverlay + i);
      if (r < 0) { o.inflight = false; o.sending = -1; }
      o.shown = true;
    } else if (o.shown) {
      o.removing = mpv_.cmd({"overlay-remove", std::to_string(i)}, kReplyOverlayRemove + i) >= 0;
      if (!o.removing) L.dirty = true;
    }
  }
}

bool App::overlays_pending() const {
  if (cloud_pending_) return true;
  for (int i = 0; i < PlayView::L_COUNT; ++i)
    if (play_.layers[i].dirty && !ov_[i].inflight && !ov_[i].removing) return true;
  return false;
}

void App::remove_overlays() {
  for (int i = 0; i < PlayView::L_COUNT; ++i) {
    play_.layers[i].visible = false;
    play_.layers[i].dirty = true;
  }
  upload_overlays();
}

// ── frames and timers ───────────────────────────────────────────────
void App::refresh_display_rate() {
  pacer_.refresh_interval(hwnd_);
  double iv = pacer_.interval();
  // The engine redraws its OSD for every overlay picture: uploads stay near 60 Hz, on a whole
  // divisor of the display's rate so they still land on its frames (144 Hz -> 72, 120 -> 60).
  int k = std::max(1, (int)std::ceil(1.0 / iv / 90.0 - 0.05));
  play_interval_ = iv * k;
  for (auto& o : ov_) o.cadence.period = play_interval_;
  cloud_cadence_.period = play_interval_;
}

void App::ensure_anim() {
  if (anim_on_) return;
  anim_on_ = true;
  // fine timer resolution only while something animates (it costs power system-wide);
  // display-locked ticks do not need it at all
  KillTimer(hwnd_, T_FINE);
  if (!pacer_.ok() && !fine_timer_) { timeBeginPeriod(1); fine_timer_ = true; }
  last_tick_ = now();
  home_paint_at_ = -1; // a new input gets an immediate paint
  KillTimer(hwnd_, T_WAKE);
  animation_cadence_.reset();
  frame();
}

void App::arm_animation_timer() {
  if (pacer_.ok()) pacer_.arm();
  else SetTimer(hwnd_, T_ANIM, animation_cadence_.wait_ms(now()), nullptr);
}

void App::frame() {
  if(!closing_app_&&mode_==Mode::Play&&play_.queue_open()&&!queue_thumb_timer_) {queue_thumb_timer_=true;SetTimer(hwnd_,T_QUEUE,60,nullptr);}
  if (hidden_now()) {
    // Playback continues in mpv; a minimized interface has no pixels to animate.
    anim_on_ = false;
    KillTimer(hwnd_, T_ANIM);
    KillTimer(hwnd_, T_WAKE);
    if (fine_timer_) { timeEndPeriod(1); fine_timer_ = false; }
    return;
  }
  double t = now();
  // home: every display frame; playback: the overlay rhythm (uploads cannot go faster anyway)
  animation_cadence_.period = (mode_ == Mode::Home || fullscreen_transition_) && pacer_.ok() ? pacer_.interval() : play_interval_;
  // display ticks arrive a hair early or late: half a frame of slack keeps every one of them
  double slack = pacer_.ok() ? pacer_.interval() * 0.45 : 0.0;
  if (!animation_cadence_.ready(t, slack)) { if (pacer_.ok()) pacer_.arm(); return; }
  animation_cadence_.mark(t);
  double dt = std::min(0.05, std::max(0.0, t - last_tick_));
  last_tick_ = t;
  bool moving = geom_.on;
  moving |= step_fullscreen_transition(t);
  moving |= step_audio_switch(t);
  // the words beside the cover changed width (another song's title): the composition, centred on
  // them, glides to its new place
  if (audio_only_ && cover_scene_ && mode_ == Mode::Play && music_frame_ == MusicFrame::Scene) {
    const double ww = play_.music_words_w();
    if (std::fabs(ww - words_w_seen_) > 0.5) {
      words_w_seen_ = ww;
      if (play_.in.music_view.f > 0 && !reduce_motion()) { view_from_ = play_.in.music_view; view_t0_ = t; }
      else step_music_view(t);
    }
  }
  // Centre: the list opened beside the cover (or closed): the cover and its words glide into the
  // left half (or back to the middle); the words' height changed (another song): to their place
  if (audio_only_ && cover_scene_ && mode_ == Mode::Play && music_frame_ == MusicFrame::Centre) {
    const bool side = play_.music_list_beside();
    const double below = play_.music_centre_below();
    if (side != list_side_seen_ || std::fabs(below - centre_below_seen_) > 0.5) {
      const bool toggled = side != list_side_seen_;
      list_side_seen_ = side;
      centre_below_seen_ = below;
      if (toggled && play_.in.music_view.f > 0 && !reduce_motion()) { view_from_ = play_.in.music_view; view_t0_ = t; }
      else step_music_view(t);
    }
  }
  // the list opened with its header (or closed from it): the cover glides into the thumbnail (or
  // back out of it)
  if (audio_only_ && cover_scene_ && mode_ == Mode::Play) {
    const Rect lc = play_.list_cover();
    if (lc.x != list_cover_seen_.x || lc.y != list_cover_seen_.y || lc.w != list_cover_seen_.w) {
      const bool toggled = (lc.w > 0) != (list_cover_seen_.w > 0);
      list_cover_seen_ = lc;
      if (toggled && play_.in.music_view.f > 0 && !reduce_motion()) { view_from_ = play_.in.music_view; view_t0_ = t; }
      else step_music_view(t);
    }
  }
  if (view_t0_ >= 0) moving |= step_music_view(t);
  moving |= step_sub_fx(t, dt);
  moving |= step_mono(dt);
  step_geom(t);
  moving |= step_mini_fx(t);
  card_art_prefetch();
  moving |= step_card_flip(t);
  moving |= sync_card_art();
  moving |= step_seek_fade(t);
  if (music_gap_ && t - music_gap_t_ > 4) end_music_gap();  // (a load that never reports: the passes follow again)
  if (mode_ == Mode::Home && onboarding_) {
    moving = ob_.tick(t, dt);
    if (ob_.finished(t)) {
      onboarding_ = false;
      home_.enter(t, ob_.face_handoff());  // the eyes already flew to their place (from the first screen)
      moving = true;
    }
  }
  if (mode_ == Mode::Home && onboarding_) {
    double raster = pacer_.ok() ? pacer_.interval() : 1.0 / 60;
    if ((double)cw_ * ch_ > 4.2e6) raster = std::max(raster, 1.0 / 60);
    if (!moving || home_paint_at_ < 0 || t - home_paint_at_ >= raster * 0.55) {
      present_home(t);
      home_paint_at_ = t;
    }
  } else if (mode_ == Mode::Home) {
    home_.survey = survey_wanted();
    moving |= home_.tick(t, dt);
    // Rasterize once per display frame; very large canvases (4K and up) stay at 60 Hz,
    // where a full software frame plus its copy to the screen would cost more than it shows.
    double raster = pacer_.ok() ? pacer_.interval() : 1.0 / 60;
    if ((double)cw_ * ch_ > 4.2e6) raster = std::max(raster, 1.0 / 60);
    if (!moving || home_paint_at_ < 0 || t - home_paint_at_ >= raster * 0.55) {
      present_home(t);
      home_paint_at_ = t;
    }
    if (reveal_pending_) { moving = true; if (home_.leave_done(t)) reveal_play(); }
  } else {
    if (play_.iris_waiting()) { try_start_iris(); moving = true; }
    // upload_overlays updates the shader once below, after the UI animation has advanced.
    if (play_.dragging() && !play_.win_dragging()) {
      POINT p = cursor_client();
      if (p.x != last_cursor_.x || p.y != last_cursor_.y) { last_cursor_ = p; play_.mouse_move(p.x, p.y, t); }
    }
    moving |= play_.tick(t, dt);
    run_actions();
    upload_overlays();
    moving |= overlays_pending();  // a held-back picture still has to go out
  }
  if (!moving) {
    anim_on_ = false;
    KillTimer(hwnd_, T_ANIM);
    if (fine_timer_) SetTimer(hwnd_, T_FINE, 1500, nullptr);  // released after a quiet moment, not every burst
    schedule_wake();
  } else arm_animation_timer();
}

// Draws the next home (or welcome) frame off screen, then hands Windows only the rectangle
// that actually changed: a blink repaints the eyes, not 1280 x 720 pixels. The copy to the
// screen and the compositor's work shrink with it. Shown right after the display's frame.
void App::present_home(double t) {
  if (onboarding_) ob_.render(home_back_, t);
  else home_.render(home_back_, t);
  const Canvas& a = home_back_;
  const Canvas& b = home_canvas_;
  RECT dirty{0, 0, a.w, a.h};
  if (a.w == b.w && a.h == b.h && !b.px.empty()) {
    int y0 = -1, y1 = -1;
    const size_t row = (size_t)a.w;
    for (int y = 0; y < a.h; ++y)
      if (std::memcmp(&a.px[y * row], &b.px[y * row], row * 4) != 0) { if (y0 < 0) y0 = y; y1 = y; }
    if (y0 < 0) { std::swap(home_canvas_, home_back_); return; }  // nothing on screen changed
    int x0 = a.w, x1 = -1;
    for (int y = y0; y <= y1; ++y) {
      const uint32_t* ra = &a.px[y * row];
      const uint32_t* rb = &b.px[y * row];
      int l = 0;
      while (l < x0 && ra[l] == rb[l]) ++l;
      if (l < x0) x0 = l;
      int r = a.w - 1;
      while (r > x1 && ra[r] == rb[r]) --r;
      if (r > x1) x1 = r;
    }
    if (x1 < x0) { std::swap(home_canvas_, home_back_); return; }
    dirty = {x0, y0, x1 + 1, y1 + 1};
  }
  std::swap(home_canvas_, home_back_);
  InvalidateRect(hwnd_, &dirty, FALSE);
  UpdateWindow(hwnd_);
}

void App::schedule_wake() {
  if (mode_ != Mode::Home) return;
  double d = onboarding_ ? ob_.next_event(now()) : home_.next_event(now());
  UINT ms = (UINT)std::clamp(d * 1000.0, 10.0, 60000.0);
  SetTimer(hwnd_, T_WAKE, ms, nullptr);
}

POINT App::cursor_client() const {
  POINT p;
  GetCursorPos(&p);
  ScreenToClient(hwnd_, &p);
  return p;
}

void App::mouse_button(int which, bool down, int x, int y) {
  if (mode_ != Mode::Play || closing_video_) return;
  double t = now();
  if (which == 0 && down) {  // the press that activated the window (WA_CLICKACTIVE) only wakes it
    play_.activation_click = click_activated_t_ > 0 && t - click_activated_t_ < 0.6;
    click_activated_t_ = 0;
  }
  if (which == 1) play_.right_click(x, y, t);
  else if (down) play_.mouse_down(x, y, t);
  else play_.mouse_up(x, y, t);
  run_actions();
  ensure_anim();
}

void App::key(const std::string& k, bool shift, bool ctrl) {
  double t = now();
  if (scene_.visible()) { if (k == "esc") scene_.close(t); return; }
  if (k == "esc" && next_count_ >= 0 && !next_cancelled_) {
    next_advance_.clear(); next_cancelled_ = true; next_count_ = -1; play_.next_notice("", t); ensure_anim(); return;
  }
  if(mode_==Mode::Play&&play_.queue_open()&&ctrl&&k=="o") {play_.key(k,shift,ctrl,t);run_actions();ensure_anim();return;}
  if (ctrl && shift && k == "o") { auto p=pick_folder_dialog(hwnd_); if(!p.empty())queue_folder(p,true); return; }
  if (ctrl && k == "o") {
    auto f = open_file_dialog(hwnd_, DialogKind::Video);
    if (!f.empty()) handle_files(f);
    return;
  }
  if (ctrl && k == "v") { handle_files(clipboard_files(hwnd_)); return; }
  if (ctrl && k == "t") { toggle_pin(); return; }  // always on top, like Windows' own Win+Ctrl+T
  if (ctrl && k == "m" && mode_ == Mode::Play) { if (mini_player_) exit_mini(true); else enter_mini(); return; }  // the mini player (m alone mutes)
  // interface size, anywhere: ctrl + / ctrl - / ctrl 0
  if (ctrl && (k == "plus" || k == "minus" || k == "zero")) {
    set_ui_scale(k == "zero" ? kDefaultUiSize : next_ui_size(store_.ui_scale, k == "plus" ? 1 : -1));
    say_ui_size();
    return;
  }
  if (mode_ == Mode::Home && onboarding_) {
    on_onboarding(ob_.key(k, shift, t));
    ensure_anim();
    return;
  }
  if (mode_ == Mode::Home) {
    HomeAction a = home_.key(k, shift, t);
    if(a.a==HomeAct::OpenFolder) {auto p=pick_folder_dialog(hwnd_);if(!p.empty())queue_folder(p,true);}
    if (a.a == HomeAct::OpenDialog) { auto f = open_file_dialog(hwnd_, DialogKind::Video); if (!f.empty()) handle_files(f); }
    if (a.a == HomeAct::OpenRecent) {
      if (a.index >= 0 && a.index < (int)shown_paths_.size()) open(shown_paths_[a.index]);
    }
    if (a.a == HomeAct::ClearRecents) { store_.clear_recents(); save_state(); refresh_recents(); }
    if (a.a == HomeAct::Update) start_update();
    if (a.a == HomeAct::Language) set_language_pref(a.index);
    if (a.a == HomeAct::Remote) open_remote_scene();
    if (a.a == HomeAct::News) show_news(true);
    if (a.a == HomeAct::NewsClose) close_news_line();
    if (a.a == HomeAct::Survey || a.a == HomeAct::SurveyClose) answer_survey(a.a == HomeAct::Survey);
    ensure_anim();
    return;
  }
  play_.key(k, shift, ctrl, t);
  run_actions();
  ensure_anim();
}

void App::toggle_fullscreen() {
  if (mini_player_ && !fullscreen_) return;  // full screen is the full player's (F, a double click): not from the mini player
  if (fullscreen_transition_ || audio_switch_.stage || mode_ != Mode::Play) return;
  begin_fullscreen_motion();
}

// ── fullscreen: the snap ────────────────────────────────────────────
// Everything happens inside the video's own shader, in the same picture the engine
// presents, so two images can never be on screen together (the old compositor copy and the
// real window disagreed for a frame at every hand-over: the flash).
//   out  (110 ms): the picture breaks into ever bigger pixels, loses its colour, goes dark
//   cut           the window changes size while the picture is black (nothing to stretch)
//   in   (200 ms): from black, the pixels halve 32 → 16 → 8 → 4 → 2 → 1 and snap into
//                  focus like a magnet, the colour comes back a beat later
// Pixel steps are the Cue's own language (the opening iris, the eyes, the type), and they
// read the same at 24, 60 or 144 frames per second.
namespace {

// physical pixel sizes for the mosaic at 1080 lines; scaled to the window's height
constexpr double kSnapSteps[] = {1, 2, 4, 8, 16, 32};
double snap_px(double amount, int height) {  // 0 = sharp, 1 = biggest blocks
  int i = std::clamp((int)std::floor(amount * 6.0), 0, 5);
  double k = std::max(1.0, height / 1080.0);
  return i == 0 ? 1.0 : std::round(kSnapSteps[i] * k);
}
}  // namespace

double App::video_aspect() const {
  double w = (double)vid_w_, h = (double)vid_h_;
  if (vid_rot_ % 180 == 90) std::swap(w, h);
  return w > 0 && h > 0 ? w / h : 0;
}

std::pair<double, double> App::sub_center_out() const {
  Rect r = play_.subtitle_rect();
  if (r.w <= 0 || vid_w_ <= 0 || vid_h_ <= 0) return {.5, .8};
  return output_point(r.x + r.w / 2.0, r.y + r.h / 2.0, cw_, ch_, vid_w_, vid_h_, vid_rot_);
}

// Subtitle edit: advances the rings and the lift; true while something still moves.
bool App::step_sub_fx(double t, double dt) {
  bool moving = false;
  if (subfx_.on) {
    moving = true;
    if (subfx_.t_out >= 0 && t - subfx_.t_out >= .45) subfx_.on = false;  // the colour is all back
    update_blur_cloud();
  } else if (snap_.was_on && !snap_.on) {
    update_blur_cloud();  // the last word: the effect off
  }
  if (dt > 0) moving |= sub_lift_.step(dt);
  // the lift moves in steps of 1.5%: every new size makes libass rasterise the line again,
  // so a handful of sizes (cached after the first time) instead of one per frame
  double q = 1.0 + std::round((sub_lift_.x - 1.0) / 0.015) * 0.015;
  // the mini player: the subtitles keep a readable size (about 19 px at 100%) however small the
  // window, in a few steps (each new size makes libass draw the lines again)
  if (mini_player_ && ch_ > 0 && cw_ > 0) {
    const double px = play_.in.style.size * ch_ / 720.0 * subtitle_typography_scale(cw_, ch_);
    const double want = 19.0 * dpi_ / 96.0;
    if (px > 0 && px < want) q *= std::round(std::min(2.5, want / px) * 10) / 10;
    // and the size chosen there (the mini player's subtitles panel): as it is, a size up, two up;
    // the full player's subtitle style stays as it was
    static const double kMiniSub[3] = {1.0, 1.25, 1.5};
    q *= kMiniSub[std::clamp(store_.mini_sub_size, 0, 2)];
  }
  char b[16];
  std::snprintf(b, sizeof b, "%.3f", q);
  if (mpv_ok_ && sub_scale_sent_ != b) { sub_scale_sent_ = b; mpv_.set_async("sub-scale", b); }
  return moving;
}

void App::set_snap(double dark, double px, double gray) {
  // a song's mini player: a song change is a quick, light dissolve, never the big grey blocks the
  // full window uses (in a card they swallowed the whole cover)
  if (mini_player_ && !mini_fx_.on) { px = std::max(1.0, px * 0.35); gray = 0; dark *= 0.4; }
  snap_.on = true;
  snap_.dark = std::clamp(dark, 0.0, 1.0);
  snap_.px = px;
  snap_.gray = std::clamp(gray, 0.0, 1.0);
  update_blur_cloud();
}

void App::snap_subtitles(bool hide) {
  if (hide == snap_.subs_hidden || !mpv_ok_) return;
  if (hide) {
    snap_.subs_were = play_.in.sub_visible;
    snap_.subs_hidden = true;
    if (snap_.subs_were) mpv_.set_flag_async("sub-visibility", false);
  } else {
    snap_.subs_hidden = false;
    if (snap_.subs_were) mpv_.set_flag_async("sub-visibility", true);
  }
}

void App::begin_fullscreen_motion() {
  // Entering fullscreen must not discard a new film's pending/following shape.
  if(!user_sized_ && (!geom_done_ || geom_.on) && video_aspect()>0) {
    pending_ar_=video_aspect();
    geom_done_=true;
  }
  geom_.on=false;
  KillTimer(hwnd_,T_GEOM);
  fullscreen_transition_=true;
  play_.begin_viewport_transition(now(),true);  // the controls step aside on this very frame
  fz_=FullscreenZoom{};
  fz_.entering=!fullscreen_;
  fz_.t0=now();
  if(reduce_motion() || !mpv_ok_ || (audio_only_ && !waveform_active_)) {  // music has a picture: it snaps too
    perform_fullscreen_switch();
    fullscreen_changed_at_=now();
    fullscreen_stage_=9;
  } else {
    snap_subtitles(true);  // subtitles are drawn after the shader: they step out with the controls
    fullscreen_stage_=1;
  }
  ensure_anim();
}

void App::settle_fullscreen_motion() {
  stage_cover_.end();
  if(!fullscreen_transition_) return;
  if(fz_.entering!=fullscreen_) perform_fullscreen_switch();
  end_snap();
  fullscreen_transition_=false; fullscreen_stage_=0;
  play_.end_viewport_transition(now());
}

void App::end_snap() {
  snap_subtitles(false);
  if (snap_.on) { snap_.on = false; update_blur_cloud(); }
}

bool App::step_fullscreen_transition(double t) {
  if(!fullscreen_transition_) return false;
  if(fullscreen_stage_!=logged_fs_stage_) {
    logged_fs_stage_=fullscreen_stage_;
    trace("fullscreen snap stage "+std::to_string(fullscreen_stage_));
  }
  const double frame=pacer_.ok()?pacer_.interval():1./60;
  auto go=[&](int stage) { fullscreen_stage_=stage; fz_.t0=t; };
  auto renderer_ready=[&] {
    RECT child{};
    bool native_ready=!mpv_win_ || (GetClientRect(mpv_win_,&child) && child.right==cw_ && child.bottom==ch_);
    if(geom_.on || !viewport_ready_for_controls(cw_,ch_,osd_w_,osd_h_,native_ready,t-fullscreen_changed_at_,0)) {
      fz_.ready_at=-1; return false;
    }
    if(fz_.ready_at<0) fz_.ready_at=t;
    return t-fz_.ready_at>=2*frame;  // a couple of frames drawn at the new size, still black
  };
  auto done=[&] {
    stage_cover_.end();
    end_snap();
    fullscreen_transition_=false; fullscreen_stage_=0; logged_fs_stage_=-1;
    play_.end_viewport_transition(t);  // the controls rise in on their spring
    trace("fullscreen snap done");
    music_fit_window();  // a full screen of another shape may want the other composition
  };
  switch(fullscreen_stage_) {
    case 1: {  // out: pixels grow, colour goes, light goes (accelerating: pulled in)
      double k=clamp01((t-fz_.t0)/kSnapOut);
      double e=k*k;
      set_snap(e, snap_px(k, ch_), std::min(1.0, k*1.6));
      if(k>=1) { set_snap(1.0, snap_px(1, ch_), 1.0); go(2); }
      break;
    }
    case 2: {  // the black picture is presented, our old overlays are gone: cut
      bool overlays=false;
      for(const auto& o:ov_) overlays|=o.shown||o.inflight||o.removing;
      if(overlays || t-fz_.t0<kSnapHold) break;
      {
        // the screen held black while the window changes size (the picture is black already):
        // Windows composes the new place with the old contents for a frame or two otherwise
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
        const RECT here=fullscreen_?mi.rcMonitor:client_screen();
        RECT all=mi.rcMonitor;
        UnionRect(&all,&all,&here);
        stage_cover_.show(inst_,all,false,true);
      }
      perform_fullscreen_switch();
      // into full screen: the cover stays over everything (a pinned window rises into the
      // topmost band with the switch, so the cover is put back on top of it); out of it, the
      // cover shrinks to the window at once (the desktop around it is final). It leaves when the
      // engine has drawn the window at its size
      if(stage_cover_.on()) {
        if(fullscreen_) {
          MONITORINFO mi{sizeof(mi)};
          GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
          stage_cover_.show(inst_,mi.rcMonitor,false);
        } else stage_cover_.show(inst_,client_screen(),!IsZoomed(hwnd_));
      }
      fullscreen_changed_at_=t; fz_.ready_at=-1;
      set_snap(1.0, snap_px(1, ch_), 1.0);  // the new size gets the same black, biggest blocks
      go(3);
      break;
    }
    case 3:  // the engine draws at the new size (black): then it may come back
      if(renderer_ready()) { stage_cover_.end(); go(4); }
      else if(t-fullscreen_changed_at_>0.6) stage_cover_.end();  // never held longer than that
      break;
    case 4: {  // in: light first, then the pixels halve and snap into focus, colour last
      double k=clamp01((t-fz_.t0)/kSnapIn);
      double light=1-ease_out(clamp01(k*1.35));
      double blocks=1-ease_out(clamp01((k-0.08)/0.85));
      double gray=1-smoothstep(0.45,1.0,k);
      set_snap(light, snap_px(blocks, ch_), gray);
      if(k>=0.55) snap_subtitles(false);
      if(k>=1) done();
      break;
    }
    case 9:
      if(renderer_ready()) done();
      break;
  }
  return fullscreen_transition_;
}

void App::perform_fullscreen_switch() {
  geom_.on = false;
  KillTimer(hwnd_, T_GEOM);
  if (!fullscreen_) {
    GetWindowPlacement(hwnd_, &wp_);
    saved_style_ = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
    // Set state before SetWindowPos sends reentrant sizing/non-client messages.
    fullscreen_ = play_.in.fullscreen = true;
    SetWindowLongPtrW(hwnd_, GWL_STYLE, (saved_style_ & ~(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE | WS_MINIMIZE)) | WS_POPUP);
    SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
  } else {
    fullscreen_ = play_.in.fullscreen = false;
    SetWindowLongPtrW(hwnd_, GWL_STYLE, saved_style_);
    SetWindowPlacement(hwnd_, &wp_);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    if (pending_ar_ > 0) { double ar = pending_ar_; pending_ar_ = 0; adapt_window(ar); }  // a file of another shape opened meanwhile
  }
  RECT client{}; GetClientRect(hwnd_, &client);
  SendMessageW(hwnd_, WM_SIZE, IsZoomed(hwnd_) ? SIZE_MAXIMIZED : SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
  RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
  play_.in.fullscreen = fullscreen_;
  apply_window_corners();
  play_.changed(now());

}

void App::set_title(const std::string& t) { SetWindowTextW(hwnd_, widen(t == "Cue" ? CUE_PRODUCT_NAME : t + " · " CUE_PRODUCT_NAME).c_str()); }

// ── timeline previews ───────────────────────────────────────────────
// Decoded by the helper process (thumbhost.cpp): its own engine at background priority, CPU only.
// Each frame is a keyframe seek and a small picture handed back through a pipe. Frames are
// cached per slice only when requested. No speculative full-timeline scan.
double App::thumb_step() const { return std::max(1.0, play_.in.dur / 300.0); }
int App::thumb_key(double t) const { return (int)std::floor(std::max(0.0, t) / thumb_step()); }

void App::thumb_request(double t) {
  if (audio_only_ || !mpv_ok_ || path_.empty() || preview_.failed) return;
  if (t >= 0) {
    thumb_want_ = t;
    // instant answer: this slice, or the nearest one already decoded while the exact one comes
    if (background_ready() && !thumb_cache_.empty()) {
      int k = thumb_key(t);
      auto it = thumb_cache_.lower_bound(k);
      if (it == thumb_cache_.end() || (it != thumb_cache_.begin() && it->first != k &&
                                       k - std::prev(it)->first < it->first - k)) --it;
      thumb_cache_.touch(it->first);
      if (it->first != thumb_shown_) {
        thumb_shown_ = it->first;
        play_.set_thumb(it->second.px, it->second.w, it->second.h, it->first * thumb_step());
        ensure_anim();
      }
      if (it->first == k) thumb_want_ = -1;
    }
  }
  // Timeline hover waits 3 s; the shared helper may start earlier for colours.
  // Keep the requested time: thumb_next serves it once timeline work is allowed.
  if (t >= 0 ? !background_ready() : !atmosphere_ready()) return;
  if (!thumbs_.running() && !thumbs_.start(hwnd_, WM_APP_THUMB)) { preview_.failed = true; return; }
  if (thumb_path_ != path_) {
    // the helper opens this file once; it answers Ready (or Failed) when it can decode it
    thumbs_.open(path_, play_.thumb_decode_width(), preview_.generation);
    thumb_path_ = path_;
    preview_.loaded = false;
    preview_.busy = true;
    return;
  }
  thumb_next();
}

// When free, serve an explicit hover/phone request or the current scene colour.
// Speculative distant seeks can saturate a HDD or network share; there is no background fill.
void App::thumb_next() {
  if (in_sizemove_ || !atmosphere_ready() || !preview_.loaded || preview_.busy || play_.in.dur <= 0) return;
  double t = -1;
  thumb_for_accent_ = false;
  if (background_ready() && thumb_want_ >= 0 && !thumb_cache_.count(thumb_key(thumb_want_))) t = thumb_want_;
  if (t < 0 && accent_want_ >= 0) {
    // Timeline buckets can span tens of seconds in a long film. Its cached
    // thumbnail is not the colour of the current scene: sample the requested time.
    t = accent_want_;
    accent_want_ = -1;
    thumb_for_accent_ = true;
  }
  if (t < 0) return;
  preview_.busy = true;
  thumb_seek_t_ = t;
  thumbs_.shot(++preview_.request, std::max(0.0, t), thumb_for_accent_ ? 0 : play_.thumb_hover_width());
}

void App::thumb_store(std::vector<uint32_t>&& px, int w, int h) {
  if (w <= 0 || h <= 0 || px.size() < (size_t)w * h) return;
  ThumbImg img;
  img.w = w; img.h = h;
  img.px = std::move(px);
  int k = thumb_key(thumb_seek_t_);
  if (thumb_for_accent_) {
    sample_atmosphere(img.px, img.w, img.h);
    return; // colour probes are transient; do not populate the hover cache every 0.9 seconds
  }
  // Colour probes already returned above at full sampling resolution. Resize only UI previews.
  const auto size = preview_size(img.w, img.h, play_.thumb_hover_width());
  if (size.w != img.w) {
    std::vector<uint32_t> small((size_t)size.w * size.h);
    for (int y = 0; y < size.h; ++y)
      for (int x = 0; x < size.w; ++x)
        small[(size_t)y * size.w + x] = img.px[(size_t)(y * img.h / size.h) * img.w + x * img.w / size.w];
    img.w = size.w; img.h = size.h; img.px = std::move(small);
  }
  if (!thumb_cache_.put(k, std::move(img))) {
    // A rejected oversized picture must not retry forever and churn the disk.
    if (thumb_want_ >= 0 && thumb_key(thumb_want_) == k) thumb_want_ = -1;
    if (remote_peek_want_ >= 0 && thumb_key(remote_peek_want_) == k) remote_peek_want_ = -1;
    return;
  }
  if (remote_peek_want_ >= 0) remote_peek_send();
  // the frame the cursor is waiting for: show it now
  if (background_ready() && thumb_want_ >= 0 && thumb_key(thumb_want_) == k) {
    thumb_want_ = -1;
    thumb_shown_ = k;
    thumb_cache_.touch(k);
    auto& c = thumb_cache_.find(k)->second;
    play_.set_thumb(c.px, c.w, c.h, k * thumb_step());
    ensure_anim();
  }
}

// One answer from the preview helper (it owns the decoding; we only keep the pictures).
void App::on_thumb(ThumbMsg* m) {
  if (m->process != thumbs_.epoch()) return;
  if (!preview_.accepts(m->session) && !(m->type == ThumbMsg::Gone && m->session == 0 && !thumb_path_.empty())) return;
  switch (m->type) {
    case ThumbMsg::Ready: preview_.loaded = true; preview_.busy = false; break;
    case ThumbMsg::Failed: preview_.loaded = false; preview_.busy = false; break;  // this file: no previews
    case ThumbMsg::Frame:
      if (preview_.accepts_frame(m->session, m->id)) {
        preview_.busy = false;
        preview_.misses = 0;
        thumb_store(std::move(m->px), m->w, m->h);
      }
      break;
    case ThumbMsg::NoFrame:
      if (preview_.accepts_frame(m->session, m->id)) { preview_.busy = false; if (++preview_.misses > 8) preview_.failed = true; }
      break;
    case ThumbMsg::Gone:
      preview_.loaded = false;
      preview_.failed = true;
      log("preview helper stopped");
      preview_.busy = false;
      break;
  }
  if (!preview_.failed && preview_.loaded && thumb_path_ == path_) thumb_next();
}

void App::thumb_stop() {
  KillTimer(hwnd_, T_THUMB);
  KillTimer(hwnd_, T_PREFETCH);
  thumb_cache_.clear();
  thumb_shown_ = INT_MIN;
  remote_peek_want_ = -1;
  remote_peek_sent_ = INT_MIN;
  preview_.reset(); // errors and replies from the old file must not disable its replacement
  play_.set_thumb({}, 0, 0, -1);
  thumb_want_ = thumb_seek_t_ = -1;
  accent_want_ = -1;
  thumb_for_accent_ = false;
  // the helper stays alive for the next file: it only lets go of this one
  if (thumbs_.running() && !thumb_path_.empty()) thumbs_.close_file();
  thumb_path_.clear();
  preview_.loaded = preview_.busy = false;
}

// ── own window chrome ───────────────────────────────────────────────
// Interface size: changes the art-pixel multiple; video, zoom and subtitle size are untouched.
// The welcome's choices apply at once, exactly like the same controls in the player.
void App::on_onboarding(ObAction a) {
  double t = now();
  switch (a.a) {
    case ObAct::None: break;
    case ObAct::Ambient:
      store_.ambient = a.v != 0;
      play_.in.ambient = store_.ambient;
      save_state();
      break;
    case ObAct::UiScale: set_ui_scale(a.v); break;
    case ObAct::Finish:
      store_.onboarded = true;
      save_state();
      home_.set_size(cw_, ch_, s_);
      ob_.finish(t, home_.face.cx, home_.face.cy, home_.face.scale);
      trace("welcome done");
      break;
    case ObAct::WinMin: ShowWindow(hwnd_, SW_MINIMIZE); break;
    case ObAct::WinPin: toggle_pin(); break;
    case ObAct::WinMax: toggle_maximize(); break;
    case ObAct::WinClose: PostMessageW(hwnd_, WM_CLOSE, 0, 0); break;
  }
}

// The scale for a client area of cw x ch at the window's DPI. The mini player keeps the
// interface's own size (a small window would otherwise shrink it): every path that sets the
// scale goes through here, so a move or a resize that ends (refresh_ui_scale) or a new size chosen
// in the menu no longer drops the card's menu and list to the small window's scale.
double App::scale_for(int cw, int ch, int choice) const {
  // (the mini players: the chosen size's own pixels, as far as the little window holds them)
  if (mini_player_) return mini_scale(dpi_, cw, ch, choice, audio_only_);
  return ui_scale(dpi_, cw, ch, choice);
}

// The player's own scale: portrait windows raise it (a compact dock), and so does music framed
// close (Cover or Tall: the compact music bar), so its few controls are not lost in the window.
double App::play_scale(int choice) const {
  if (mini_player_) return s_;  // (one scale for the whole mini player: its bar and its menus; see mini_scale)
  // (music keeps the screen's own size in a tall window: the film's portrait bar is raised to fill
  // the width, but music's bar is short and its words grew with it, far too big)
  double ps = audio_only_ ? s_ : portrait_scale(s_, cw_, ch_, choice);
  if (audio_only_ && music_frame_stacked(music_frame_for(cw_, ch_, s_)))
    ps = std::max(ps, music_compact_scale(s_, cw_, ch_, choice, ui_base_scale(dpi_, choice)));
  return ps;
}

// After a move to another monitor or a display change: the same rule, applied right away.
void App::refresh_ui_scale() {
  if (cw_ <= 0 || ch_ <= 0) return;
  if (scale_for(cw_, ch_, store_.ui_scale) == s_) return;
  rescale();
}

// A new interface scale (an edge let go, another screen, a size picked in Settings or with
// Ctrl +/−): everything that is measured in it is laid out again, the same way as for a new
// window size. Music's framing too: the composition (Centre, Tall, Cover...) is chosen in art px,
// so a new scale can change it; it used to stay the old one (the words gone under a compact bar).
void App::rescale() {
  if (cw_ <= 0 || ch_ <= 0) return;
  const double ns = scale_for(cw_, ch_, store_.ui_scale);
  // music: the cover glides to its new place (the bar it stays above has a new size, and the
  // composition may change) instead of jumping there
  if (ns != s_ && mode_ == Mode::Play && audio_only_ && cover_scene_ && play_.in.music_view.f > 0 && !reduce_motion() && !mini_fx_.on) {
    view_from_ = play_.in.music_view;
    view_t0_ = now();
  }
  if (ns != s_) trace("ui scale " + std::to_string(ns));
  s_ = ns;
  relayout();
  // the phone remote's scene and the news over the window: laid out at the new scale too (they
  // followed only the window's own moves and sizes)
  scene_.follow(hwnd_, s_);
  news_host_.follow(hwnd_, s_);
}

// The window's size or the interface's scale changed: the home, the welcome and the player laid
// out for them, music framed for them, the overlays sent in this same message (no frame with a
// stale interface)
void App::relayout() {
  home_.set_size(cw_, ch_, s_);
  ob_.set_size(cw_, ch_, s_);
  play_.set_base_scale(s_); play_.resize(cw_, ch_, play_scale(store_.ui_scale));
  if (mode_ == Mode::Play) {
    music_fit_window();  // the cover follows the window, also mid-drag
    play_.changed(now());
    apply_style(true);  // only what changed (normally just the lifted subtitle position)
    play_.prepare(now());
    upload_overlays();
  }
  ensure_anim();
}

// ── interface language ──────────────────────────────────────────────
// Everything Cue draws is measured from the words at layout time, so a new language is applied
// on the spot: no restart. Windows' own surfaces (title bar menu, file dialog buttons, message
// box buttons) stay in Windows' language.
void App::apply_language(bool relayout) {
  i18n::Lang l = apply_ui_language(store_.language, windows_ui_languages());
  int pref = -1;
  i18n::Lang chosen;
  if (i18n::parse_tag(store_.language, chosen)) pref = (int)chosen;
  play_.in.language = home_.language = pref;
  trace(std::string("language ") + i18n::tag(l) + (pref < 0 ? " (Windows)" : " (chosen)"));
  if (!relayout) return;
  if (engine_failed_) home_.engine_problem = engine_text(engine_err_);
  show_update_state();  // its labels are words too
  refresh_recents(false);  // the recent list's lines ("25 min left", "3 episodes") are written here
  if (cw_ > 0 && ch_ > 0) {
    home_.relayout();
    ob_.set_size(cw_, ch_, s_);
    play_.set_base_scale(s_); play_.resize(cw_, ch_, play_scale(store_.ui_scale));
    if (mode_ == Mode::Play) music_fit_window();  // (the words' new width: the composition follows)
  }
  play_.changed(now());
  if (mode_ == Mode::Play && loaded_) refresh_file_info();
  relocalize_licenses();
  if (remote_.running()) remote_boot();
  if (scene_.visible()) scene_.view.set_size(scene_.view.w, scene_.view.h, scene_.view.s);  // new words, next frame
  if (news_host_.visible()) news_.set_size(news_.w, news_.h, news_.ui_s);
  // Explorer's name for Cue's file type follows, in the background like at start
  HANDLE th = CreateThread(nullptr, 0, [](void*) -> DWORD {
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    refresh_open_with();
    if (SUCCEEDED(com)) CoUninitialize();
    return 0;
  }, nullptr, 0, nullptr);
  if (th) CloseHandle(th);
  InvalidateRect(hwnd_, nullptr, FALSE);
  ensure_anim();
}

void App::set_language_pref(int lang) {
  std::string pref = lang >= 0 && lang < i18n::kLangCount ? i18n::tag((i18n::Lang)lang) : i18n::kAuto;
  if (pref == store_.language) return;
  store_.language = pref;
  save_state();  // a choice made by hand survives closing, reopening and updating
  apply_language(true);
}

void App::set_ui_scale(int v) {
  v = std::clamp(v, -1, 3);
  if (v == 1) v = 3;
  if (onboarding_ && v != ob_.ui_scale) ob_.set_scale_choice(v, now());  // ctrl +/- on the welcome
  store_.ui_scale = v;
  play_.in.ui_scale = v;
  save_state();
  rescale();  // (the same base scale too: a portrait window, music framed close and the card follow the chosen size)
}

// Right click > salvar frame: exactly the frame on screen, without any of the UI.
void App::save_frame(bool with_subs) {
  if (!loaded_) return;
  wchar_t* pics = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &pics))) return;
  std::wstring root = std::wstring(pics) + L"\\Cue", old = std::wstring(pics) + L"\\.tinyoffice Cue";
  CoTaskMemFree(pics);
  // frames saved back when the app was called Player stay together with the new ones
  if (GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES && GetFileAttributesW(old.c_str()) != INVALID_FILE_ATTRIBUTES)
    MoveFileExW(old.c_str(), root.c_str(), 0);
  // Frames land directly in Pictures\Cue. An older Pictures\Cue\Frames folder is left as it is.
  std::wstring dir = root;
  SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
  std::wstring name = widen(file_name(path_));
  size_t dot = name.find_last_of(L'.');
  if (dot != std::wstring::npos && dot > 0) name.resize(dot);
  for (auto& ch : name) if (wcschr(L"\\/:*?\"<>|", ch)) ch = L'_';
  int tsec = (int)std::max(0.0, play_.in.time);
  wchar_t stamp[32];
  swprintf(stamp, 32, L"_%02d-%02d-%02d", tsec / 3600, tsec / 60 % 60, tsec % 60);
  std::wstring base = dir + L"\\" + name + stamp, file = base + L".png";
  for (int n = 2; GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES && n < 1000; ++n)
    file = base + L" (" + std::to_wstring(n) + L").png";
  // "subtitles" = the video with its subtitles; "video" = the bare picture. Never the OSD/overlays.
  last_frame_ = file;
  mpv_.cmd({"screenshot-to-file", narrow(file), with_subs ? "subtitles" : "video"}, kReplyShot);
}

// ── the window follows the content ──────────────────────────────────
RECT App::window_rect() const {
  RECT r;
  GetWindowRect(hwnd_, &r);
  return r;
}

void App::request_video_geometry() {
  if(!loaded_ || user_sized_ || !mpv_ok_) return;
  if(geometry_query_pending_) { geometry_query_dirty_=true; return; }
  auto request=media_geometry_.request();
  geometry_query_pending_=mpv_.api().get_property_async(mpv_.handle(),request,"video-out-params",MPV_FORMAT_NODE)>=0;
}

void App::video_geometry_reply(const mpv_event* event) {
  geometry_query_pending_=false;
  if(!loaded_ || event->error<0 || !event->data) return;
  auto* p=static_cast<const mpv_event_property*>(event->data);
  if(p->format!=MPV_FORMAT_NODE || !p->data) return;
  const auto& node=*static_cast<const mpv_node*>(p->data);
  if(node.format!=MPV_FORMAT_NODE_MAP || !node.u.list) return;
  int64_t w=0,h=0,dw=0,dh=0,rotation=0;
  for(int i=0;i<node.u.list->num;++i) {
    const char* key=node.u.list->keys[i]; const auto& value=node.u.list->values[i];
    if(value.format!=MPV_FORMAT_INT64) continue;
    if(!std::strcmp(key,"w")) w=value.u.int64;
    else if(!std::strcmp(key,"h")) h=value.u.int64;
    else if(!std::strcmp(key,"dw")) dw=value.u.int64;
    else if(!std::strcmp(key,"dh")) dh=value.u.int64;
    else if(!std::strcmp(key,"rotate")) rotation=value.u.int64;
  }
  VideoShape shape{dw>0?dw:w,dh>0?dh:h,rotation};
  double previous=video_aspect();
  if(!media_geometry_.accept(event->reply_userdata,shape)) return;
  vid_w_=shape.width; vid_h_=shape.height; vid_rot_=shape.rotation;
  if(user_sized_) return; // only respected until the next file is opened
  // (the same shape, within rounding: a film whose shape was read from its file at load keeps it)
  if(geom_done_ && previous>0 && std::fabs(previous-shape.aspect())<=previous*0.004) return;
  geom_done_=false;
  trace("media geometry "+std::to_string(vid_w_)+"x"+std::to_string(vid_h_)+" rotate "+std::to_string(vid_rot_));
  maybe_adapt_window();
}

void App::maybe_adapt_window() {
  if (in_sizemove_ || geom_done_ || !loaded_ || vid_w_ <= 0 || vid_h_ <= 0) return;
  // Coalesce output reconfigurations after receiving a complete current-file tuple.
  SetTimer(hwnd_, T_GEOM, 60, nullptr);
}

void App::decide_window() {
  KillTimer(hwnd_, T_GEOM);
  if (in_sizemove_ || geom_done_ || !loaded_ || vid_w_ <= 0 || vid_h_ <= 0) return;
  geom_done_ = true;
  // Heavy video (more than 1440p): the engine draws only real frames. Redrawing every vsync
  // (display-resample, which makes the UI glide at the screen's rate) costs too much there.
  bool big = std::max(vid_w_, vid_h_) > 2600 || std::min(vid_w_, vid_h_) > 1600;
  std::string sync = big ? "audio" : "display-resample";
  if (sync != video_sync_) { mpv_.set_async("video-sync", sync); video_sync_ = sync; }
  sync_decided_ = true;
  send_sync();  // smoothing on, or covered: the film's own timing wins over the size's
  if (!logged_hw_) { logged_hw_ = true; hw_log_t_ = now() + 1.5; }
  double w = (double)vid_w_, h = (double)vid_h_;
  if (vid_rot_ % 180 == 90) std::swap(w, h);  // phone videos: stored sideways, shown upright
  adapt_window(w / h);
}

// A comfortable window for this aspect ratio: never the video's own pixel size, never
// bigger than the work area. Every newly opened file gets its own aspect ratio.
void App::adapt_window(double ar) {
  if (ar <= 0) return;
  if (fullscreen_) { pending_ar_ = ar; return; }  // fullscreen just letterboxes; adapt on the way out
  if (IsIconic(hwnd_)) return;
  if (mini_player_) { animate_window_to(mini_keep_place(mini_aspect())); return; }  // the next file keeps the mini player where it is
  pending_ar_ = 0;
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
  RECT wa = mi.rcWork;
  int aw = wa.right - wa.left, ah = wa.bottom - wa.top;
  auto_portrait_ = ar < 0.95;
  if (IsZoomed(hwnd_)) {
    // Maximized stays maximized (a new file, the next song, back home): the new shape becomes the
    // place the window restores to. The placement is in workspace coordinates: the screen's,
    // moved by the taskbar when it sits on the left or the top. A restored window's borders are
    // inside its rectangle (the content goes edge to edge), so none are added.
    WINDOWPLACEMENT wp{sizeof(wp)};
    if (!GetWindowPlacement(hwnd_, &wp)) return;
    int dx = wa.left - mi.rcMonitor.left, dy = wa.top - mi.rcMonitor.top;
    RECT n = wp.rcNormalPosition;
    WinRect from{n.left + dx, n.top + dy, n.right + dx, n.bottom + dy};
    // the remembered place may be on another monitor: it is kept there only if it touches this one
    if (from.right <= wa.left || from.left >= wa.right || from.bottom <= wa.top || from.top >= wa.bottom)
      from = {wa.left, wa.top, wa.right, wa.bottom};
    auto r = video_window_rect(from, {wa.left, wa.top, wa.right, wa.bottom}, video_window_size(ar, aw, ah, dpi_, s_), 0, 0);
    wp.rcNormalPosition = {r.left - dx, r.top - dy, r.right - dx, r.bottom - dy};
    wp.showCmd = SW_SHOWMAXIMIZED;
    wp.flags &= ~WPF_SETMINPOSITION;
    SetWindowPlacement(hwnd_, &wp);
    return;
  }
  RECT wr = window_rect(), cr;
  GetClientRect(hwnd_, &cr);
  int cw = cr.right, ch = cr.bottom;
  int fx = (wr.right - wr.left) - cw, fy = (wr.bottom - wr.top) - ch;  // invisible resize borders
  auto r = video_window_rect({wr.left, wr.top, wr.right, wr.bottom}, {wa.left, wa.top, wa.right, wa.bottom},
                             video_window_size(ar, aw - fx, ah - fy, dpi_, s_), fx, fy);
  animate_window_to(RECT{r.left, r.top, r.right, r.bottom});
}

void App::animate_window_to(const RECT& r) {
  // already there (the same shape again, the next episode): nothing moves, nothing waits for it
  if (const RECT now_at = window_rect(); EqualRect(&now_at, &r)) { geom_.on = false; geom_.kind = 0; geom_end_t_ = -10; return; }
  geom_.kind = 0;  // (a caller that wants another curve sets it right after)
  geom_.from = window_rect();
  geom_.to = r;
  geom_.t0 = now();
  geom_.on = true;
  if(fullscreen_transition_) {
    // A file opened in fullscreen gets its final window once, under the live
    // compositor stage; no second window-resize animation competes with it.
    geom_.on=false;
    SetWindowPos(hwnd_,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
    return;
  }
  if (reduce_motion()) geom_.dur = 0;
  else geom_.dur = 0.26;
  step_geom(geom_.t0);
  ensure_anim();
}

// A short geometry transition. Playback never waits for it: the engine keeps rendering into
// the window while it changes shape.
void App::step_geom(double t) {
  if (!geom_.on) return;
  double k = geom_.dur <= 0 ? 1.0 : std::clamp((t - geom_.t0) / geom_.dur, 0.0, 1.0);
  // fullscreen: a spring's gentle start and long landing; shape changes keep their ease-out
  double e = geom_.kind == 2 ? bounce_ease(k) : geom_.kind ? spring_ease(k) : 1 - std::pow(1 - k, 3);
  auto L = [&](LONG a, LONG b) { return (int)std::lround(a + (b - a) * e); };
  int x = L(geom_.from.left, geom_.to.left), y = L(geom_.from.top, geom_.to.top);
  int r = L(geom_.from.right, geom_.to.right), b = L(geom_.from.bottom, geom_.to.bottom);
  SetWindowPos(hwnd_, nullptr, x, y, r - x, b - y, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  if (k >= 1) {
    geom_.on = false;
    geom_.kind = 0;
    geom_end_t_ = t;
    refresh_ui_scale();  // normally already right (taken from the target at the start)
  }
}

// ── atmosphere: dynamic accent + ambient light ──────────────────────
bool App::background_ready(double delay) const {
  return !audio_only_ && mode_ == Mode::Play && loaded_ && first_frame_ &&
         background_warmup_.ready(now(), play_.iris_active() || geom_.on || resizing_, delay);
}

bool App::atmosphere_ready() const {
  return background_ready(PlaybackWarmup::kAtmosphereDelay);
}

void App::reset_atmosphere(bool keep_music_light) {
  background_warmup_.reset();
  atmosphere_sampled_ = false;
  if (keep_music_light) {
    // one song to the next: the light show carries on and its colours cross to the new cover
    accent_media_t_ = -1;
    atmo_t_ = now();
    return;
  }
  ambient_.reset();  // no previous halo or expensive geometry rebuild during the new reveal
  music_light_.reset();
  light_cover_job_.reset();
  light_found_ = false;
  light_k_ = 0;
  for (auto& p : light_pal_) p = {150, 152, 168};
  for (double& kick : light_kick_) kick = 0;
  light_rings_ = 0;
  accent_sm_ = ColorSmoother{};
  accent_target_ = {255, 255, 255};
  accent_k_target_ = 0;
  play_.in.accent = accent_target_;
  play_.in.accent_k = 0;
  play_.in.scene = {}; ++play_.in.scene_revision;
  accent_want_ = -1;
  accent_req_t_ = -10;
  accent_media_t_ = -1;
  atmo_t_ = now();
  for (int k = 0; k < FrameColors::kZones; ++k) {
    zone_sm_[k] = ColorSmoother{};
    zone_target_[k] = {};
    zone_light_target_[k] = 0;
  }
}

void App::sample_atmosphere(const std::vector<uint32_t>& px, int w, int h) {
  if (!atmosphere_ready()) return;
  FrameColors fc = analyze_frame(px.data(), w, h);
  if (!fc.valid) return;
  atmosphere_sampled_ = true;
  accent_target_ = fc.accent;
  accent_k_target_ = fc.strength;
  for (int k = 0; k < FrameColors::kZones; ++k) { zone_target_[k] = fc.zone[k]; zone_light_target_[k] = fc.zone_light[k]; }
  ensure_anim();
}

// Slow, continuous blending (no jump at a cut). Playback is never waited on.
bool App::step_atmosphere(double dt) {
  if (audio_only_) return step_music_light(dt);
  if (!atmosphere_ready() && !ambient_.active()) return false;
  bool moving = accent_sm_.step(accent_target_, accent_k_target_, dt, 1.4);
  play_.in.accent = accent_sm_.rgb();
  play_.in.accent_k = accent_sm_.k;
  Rgb e[FrameColors::kZones];
  double li[FrameColors::kZones];
  if (store_.ambient || ambient_.active()) {
    for (int k = 0; k < FrameColors::kZones; ++k) {
      zone_sm_[k].step(zone_target_[k], zone_light_target_[k], dt, 0.9);  // soft, never a flash at a cut
      e[k] = zone_sm_[k].rgb();
      li[k] = zone_sm_[k].k;
    }
  }
  bool redraw = false;
  if (moving) {
    // the dock redraws only when the colour moved enough to show (its signature quantizes it)
    auto before = play_.dock_signature();
    play_.changed(now());
    redraw = play_.dock_signature() != before;
  }
  bool show = store_.ambient && !mini_player_ && !mini_fx_.on && atmosphere_sampled_ && atmosphere_ready() && !resizing_ && !fullscreen_ && !IsZoomed(hwnd_) &&  // the mini player: no light around it (back when it ends)
              !fullscreen_transition_ &&
              !IsIconic(hwnd_) && !play_.iris_active() && vid_w_ > 0;
  if (show) ambient_.create(inst_);  // allocate halo windows only after playback has settled
  ambient_.hold(in_sizemove_);
  if (store_.ambient || ambient_.active()) ambient_.update(hwnd_, e, li, show, store_.ambient_strength, dt, resizing_);
  return redraw;
}

// Music's light show (see MusicLight and the lit_* pass in blur_shader.h): inside the window,
// around the cover, rings and twinkles of pixel light move with the song in the cover's colours.
// Read at the poll's 30 Hz, only while it can be seen; the halo outside stays the films'.
bool App::step_music_light(double dt) {
  const double t = now();
  if (wing_hold_ && wing_hold_t_ > 0 && t >= wing_hold_t_) { wing_hold_ = false; wing_hold_t_ = 0; update_blur_cloud(); }
  // (music's light is part of its look: always on, at 30%, but for the cover alone; the films'
  // switch and strength are the films')
  const bool on = (!mini_player_ || audio_only_) && mode_ == Mode::Play && loaded_ && cover_scene_ && first_frame_ && music_look_on_ != 2;
  if (light_cover_job_ && light_cover_job_->done.load()) {
    light_found_ = light_cover_job_->lit;
    const FrameColors& fc = light_cover_job_->colors;
    if (light_found_) light_accent_ = fc.accent;
    // eight colours round the cover, from the left, clockwise: the zones of its border
    static const int zone_a[8] = {23, 0, 3, 7, 10, 13, 16, 20}, zone_b[8] = {23, 0, 4, 7, 10, 13, 17, 20};
    Rgb pal[8];
    for (int i = 0; i < 8; ++i) {
      if (!light_found_) { pal[i] = {150, 152, 168}; continue; }
      Rgb a = fc.zone[zone_a[i]], b = fc.zone[zone_b[i]];
      double la = 0.35 + 0.65 * fc.zone_light[zone_a[i]], lb = 0.35 + 0.65 * fc.zone_light[zone_b[i]];
      pal[i] = {(uint8_t)std::lround((a.r * la + b.r * lb) / 2), (uint8_t)std::lround((a.g * la + b.g * lb) / 2),
                (uint8_t)std::lround((a.b * la + b.b * lb) / 2)};
    }
    light_cover_job_.reset();
    if (room_kept_ && bg_ready_t_ >= 0 && !reduce_motion()) {
      // song to song: the room was lit all along; its colours cross to the new cover's
      for (int i = 0; i < 8; ++i) { light_pal_from_[i] = light_pal_[i]; light_pal_to_[i] = pal[i]; }
      pal_t0_ = now();
    } else {
      for (int i = 0; i < 8; ++i) light_pal_[i] = pal[i];
      bg_ready_t_ = now();  // the room's light fades in with the cover's own colours
    }
    room_kept_ = false;
    update_blur_cloud();
  }
  if (pal_t0_ >= 0) {
    const double k = std::clamp((t - pal_t0_) / 0.45, 0.0, 1.0), e = k * k * (3 - 2 * k);
    for (int i = 0; i < 8; ++i) light_pal_[i] = mix(light_pal_from_[i], light_pal_to_[i], e);
    if (k >= 1) pal_t0_ = -1;
    update_blur_cloud();
  }
  const bool show = on && !hidden_now() && !IsIconic(hwnd_);
  // the room's light fading in (with or without the light show)
  if (bg_ready_t_ >= 0 && t - bg_ready_t_ < 0.4 && !hidden_now()) update_blur_cloud();
  if (!show && light_k_ <= 0) return false;
  if (show && light_meter_) {
    std::string rms = mpv_.get_string("af-metadata/cuelvl/by-key/lavfi.astats.Overall.RMS_level");
    if (!rms.empty()) {
      std::string zcr = mpv_.get_string("af-metadata/cuelvl/by-key/lavfi.astats.1.Zero_crossings_rate");
      music_light_.feed(t, std::strtod(rms.c_str(), nullptr), zcr.empty() ? 0.0 : std::strtod(zcr.c_str(), nullptr), light_latency_);
    }
  }
  music_light_.step(t, dt, show && !play_.in.paused && !eof_, reduce_motion());
  // each beat lights a corner of the window in turn, clockwise from the top left
  if (int r = music_light_.rings_started(); r != light_rings_) { light_rings_ = r; light_kick_[r % 4] = 1; }
  bool kicking = false;
  for (double& kick : light_kick_) { kick *= std::exp(-dt / 0.7); if (kick < 0.01) kick = 0; else kicking = true; }
  // in slowly, out a bit faster; the strength is the ambient light's own
  double target = show ? std::clamp(kMusicLight * 1.6, 0.25, 1.0) : 0.0;
  double nk = light_k_ + std::clamp(target - light_k_, -dt / 0.45, dt / 0.8);
  bool moving = music_light_.moving() || kicking || std::fabs(nk - light_k_) > 1e-4;
  light_k_ = nk;
  if (moving) update_blur_cloud();
  return false;
}

void App::toggle_maximize() {
  if (fullscreen_) return;
  if (mini_player_) { exit_mini(true); return; }  // maximize from the mini player: back to the full player
  ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
}

void App::begin_window_drag() {
  settle_fullscreen_motion();
  geom_.on = false;
  KillTimer(hwnd_, T_GEOM);
  play_.handoff_window_drag(now());
  if (fullscreen_) return;
  if (mini_player_) {  // the mini window keeps its size under the pointer (no Snap resizing it)
    RECT wr = window_rect(); POINT c; GetCursorPos(&c);
    mini_drag_ = true;
    mini_drag_size_ = {wr.right - wr.left, wr.bottom - wr.top};
    mini_grab_ = {c.x - wr.left, c.y - wr.top};
  }
  // Let Windows move/restore/Snap the parent directly. Polling the cursor from
  // the animation timer caused synchronous parent/child moves and visible jumps.
  // Post the handoff so it starts after the current mpv event has been drained.
  if (mpv_win_ && IsWindow(mpv_win_)) PostMessageW(mpv_win_, WM_CANCELMODE, 0, 0);
  ReleaseCapture();
  POINT p; GetCursorPos(&p);
  PostMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(p.x, p.y));
}

// The video window belongs to the engine's thread. Its WM_SETCURSOR is intercepted
// so the app decides the cursor (hidden, open hand over the subtitle, closed hand while
// dragging it), whatever mpv version is in use. Its mouse moves are forwarded right away:
// hover must not wait for a timer.
static std::atomic<bool> g_hide_cursor{false};
static std::atomic<int> g_cursor_kind{0};  // 0 arrow, 2 open hand, 3 closed hand
static HCURSOR g_cur_grab = nullptr, g_cur_grabbing = nullptr;
static std::atomic<bool> g_mouse_pending{false};
static std::atomic<long> g_mouse_xy{0};
static constexpr const wchar_t* kMpvOriginalProc = L"Cue.OriginalMpvProc";
static bool apply_cursor() {
  if (g_hide_cursor.load()) { SetCursor(nullptr); return true; }
  int k = g_cursor_kind.load();
  if (k == 1) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return true; }
  if (k == 2 && g_cur_grab) { SetCursor(g_cur_grab); return true; }
  if (k == 3 && g_cur_grabbing) { SetCursor(g_cur_grabbing); return true; }
  return false;
}
static void forward_mouse(LPARAM l) {
  g_mouse_xy = (long)l;
  if (!g_mouse_pending.exchange(true) && g_app_hwnd) PostMessageW(g_app_hwnd, WM_APP + 5, 0, 0);
}
static std::atomic<double> g_hit_ui_scale{1};
static int video_resize_hit(HWND owner) {
  if(!owner||IsZoomed(owner)||(GetWindowLongPtrW(owner,GWL_STYLE)&WS_POPUP))return HTCLIENT;
  POINT p;GetCursorPos(&p);RECT r;GetWindowRect(owner,&r);
  int x=p.x-r.left, y=p.y-r.top, width=r.right-r.left;
  return player_resize_hit(x,y,width,r.bottom-r.top,GetDpiForWindow(owner),g_hit_ui_scale.load());
}
static LRESULT CALLBACK mpv_subclass(HWND h, UINT m, WPARAM w, LPARAM l) {
  // Keep the original procedure on this HWND. A queued call from the old output
  // must never read a null/global pointer belonging to its replacement window.
  auto original = reinterpret_cast<WNDPROC>(GetPropW(h, kMpvOriginalProc));
  if(m==WM_SETCURSOR||m==WM_LBUTTONDOWN||m==WM_LBUTTONDBLCLK){
   int edge=video_resize_hit(g_app_hwnd);
   if(edge!=HTCLIENT){
    if(m==WM_SETCURSOR){SetCursor(LoadCursorW(nullptr,edge==HTLEFT||edge==HTRIGHT?IDC_SIZEWE:edge==HTTOP||edge==HTBOTTOM?IDC_SIZENS:edge==HTTOPLEFT||edge==HTBOTTOMRIGHT?IDC_SIZENWSE:IDC_SIZENESW));return TRUE;}
    if(m==WM_LBUTTONDOWN||m==WM_LBUTTONDBLCLK){POINT p;GetCursorPos(&p);PostMessageW(g_app_hwnd,WM_NCLBUTTONDOWN,edge,MAKELPARAM(p.x,p.y));return 0;}
   }
  }

  if (m == WM_SETCURSOR && LOWORD(l) == HTCLIENT && apply_cursor()) return TRUE;
  if(m==WM_CANCELMODE||(m==WM_CAPTURECHANGED&&(GetKeyState(VK_LBUTTON)&0x8000)))
    if(g_app_hwnd)PostMessageW(g_app_hwnd,WM_APP+13,0,0);
  if(m==WM_MOUSEWHEEL) {
    if(g_app_hwnd)PostMessageW(g_app_hwnd,WM_APP+14,w,l);
    return 0; // one Win32 source preserves high-resolution wheel/touchpad deltas
  }
  if (m == WM_MOUSEMOVE) forward_mouse(l);
  // One source of button input. In a CS_DBLCLKS window Windows replaces the
  // second DOWN with DBLCLK; route both as DOWN and suppress mpv's synthesis.
  if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_LBUTTONUP || m == WM_RBUTTONUP) {
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) { SetFocus(h); SetCapture(h); }
    else if (m == WM_LBUTTONUP && GetCapture() == h) ReleaseCapture();
    UINT msg = m == WM_LBUTTONUP ? WM_APP + 8 : m == WM_RBUTTONUP ? WM_APP + 9 : WM_APP + 7;
    if (g_app_hwnd) PostMessageW(g_app_hwnd, msg, (WPARAM)h, l);
    return 0;
  }
  if (m == WM_RBUTTONDOWN || m == WM_RBUTTONDBLCLK) return 0;
  return original ? CallWindowProcW(original, h, m, w, l) : DefWindowProcW(h, m, w, l);
}

// Pixel hands for the subtitle, drawn on a 16-cell grid like the rest of the UI.
static HCURSOR pixel_cursor(const char* const* rows, int k) {
  int n = 16 * k;
  BITMAPV5HEADER bi{};
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = n;
  bi.bV5Height = -n;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask = 0x00FF0000; bi.bV5GreenMask = 0x0000FF00; bi.bV5BlueMask = 0x000000FF; bi.bV5AlphaMask = 0xFF000000;
  void* bits = nullptr;
  HDC dc = GetDC(nullptr);
  HBITMAP color = CreateDIBSection(dc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, dc);
  if (!color || !bits) return nullptr;
  auto* px = static_cast<uint32_t*>(bits);
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      char ch = rows[y / k][x / k];
      px[y * n + x] = ch == '#' ? 0xFF000000u : ch == 'o' ? 0xFFFFFFFFu : 0u;
    }
  HBITMAP mask = CreateBitmap(n, n, 1, 1, nullptr);
  ICONINFO ii{FALSE, (DWORD)(8 * k), (DWORD)(8 * k), mask, color};
  HCURSOR c = (HCURSOR)CreateIconIndirect(&ii);
  DeleteObject(mask);
  DeleteObject(color);
  return c;
}

void App::make_cursors() {
  static const char* open[16] = {
      "......##........", "...##.#o#.......", "..#oo##o#.##....", "..#oo#oo##oo#...",
      "...#oo#oo#oo#.#.", "...#oo#oo#oo##o#", ".##.#ooooooo#oo#", "#oo##oooooooooo#",
      "#ooo#ooooooooo#.", ".#oooooooooooo#.", "..#ooooooooooo#.", "..#oooooooooo#..",
      "...#ooooooooo#..", "....#oooooooo#..", ".....#oooooo#...", ".....#oooooo#..."};
  static const char* closed[16] = {
      "................", "................", "................", "....##.##.##....",
      "...#oo#oo#oo##..", "...#oo#oo#oo#o#.", "..##oooooooooo#.", ".#o#oooooooooo#.",
      ".#oooooooooooo#.", "..#ooooooooooo#.", "..#oooooooooo#..", "...#ooooooooo#..",
      "....#oooooooo#..", ".....#oooooo#...", ".....#oooooo#...", "................"};
  // The hand fills its 16-cell grid, where Windows' arrow fills about 60% of the cursor's box:
  // drawn at the box's full size it looked twice the arrow. Whole pixels at the scale that makes
  // it about as tall as the arrow (16 px at 100-125%, 32 at 150-200%, then up with the cursor).
  const int box = GetSystemMetricsForDpi(SM_CXCURSOR, dpi_);
  int k = std::max(1, (int)(box * 0.62 / 16 + 0.4));
  if (g_cur_grab) DestroyCursor(g_cur_grab);
  if (g_cur_grabbing) DestroyCursor(g_cur_grabbing);
  g_cur_grab = pixel_cursor(open, k);
  g_cur_grabbing = pixel_cursor(closed, k);
}

void App::kick() {
  if (!anim_on_) { ensure_anim(); return; }
  frame();  // the cadence admits only the next due frame, including input-driven updates
}

void App::on_play_mouse(int x, int y) {
  if (mode_ != Mode::Play || closing_video_) return;
  if (x == last_cursor_.x && y == last_cursor_.y) return;
  last_cursor_ = {x, y};
  if (!play_.dock_visible()) play_.changed(now()); // catch up the clock before showing the cached dock
  play_.set_shift(GetKeyState(VK_SHIFT) < 0);  // (a dragged bar: centred while Shift is held)
  play_.mouse_move(x, y, now());
  run_actions();
  kick();
}

// The engine's video window lives on the engine's thread, and Windows ties its input to ours.
// Before the engine destroys it, it must not be holding the mouse, the focus or our hook,
// otherwise both threads can end up waiting on each other (frozen app, spinning cursor).
void App::release_video_window() {
  closing_video_ = true;
  g_hide_cursor = false;
  if (mpv_win_ && IsWindow(mpv_win_)) {
    if (auto original = GetPropW(mpv_win_, kMpvOriginalProc))
      SetWindowLongPtrW(mpv_win_, GWLP_WNDPROC, (LONG_PTR)original);  // unhook
    PostMessageW(mpv_win_, WM_CANCELMODE, 0, 0);  // drops any mouse capture, on its own thread
  }
  mpv_win_ = nullptr;
  SetFocus(hwnd_);  // focus leaves the video window while its thread is still responsive
}

void App::try_hide_video() {
  if (!hide_video_pending_) return;
  bool engine_window = FindWindowExW(video_, nullptr, nullptr, nullptr) != nullptr;
  if (engine_window && now() - hide_video_since_ < 2.0) return;  // still being torn down
  hide_video_pending_ = false;
  ShowWindow(video_, SW_HIDE);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

// Shuts the engine down on a worker thread while this thread keeps answering the
// messages its window may still send us. Never longer than `max_ms`: past that we stop waiting
// (the process ends right after, and Windows takes back whatever the engine still held).
void App::shutdown_engine(DWORD max_ms) {
  if (!mpv_ok_ || !mpv_.ready()) return;  // still starting on its thread: nothing of ours to wait for
  release_video_window();
  mpv_handle* h = mpv_.handle();
  auto fn = mpv_.api().terminate_destroy;
  mpv_.forget();
  mpv_ok_ = false;
  struct Job { void (*fn)(mpv_handle*); mpv_handle* h; };
  auto* job = new Job{fn, h};
  HANDLE th = CreateThread(nullptr, 0, [](void* p) -> DWORD {
    auto* j = static_cast<Job*>(p);
    j->fn(j->h);
    delete j;
    return 0;
  }, job, 0, nullptr);
  if (!th) { delete job; return; }  // no thread: do not risk waiting on our own thread
  DWORD start = GetTickCount();
  while (MsgWaitForMultipleObjects(1, &th, FALSE, 20, QS_ALLINPUT) != WAIT_OBJECT_0 && GetTickCount() - start < max_ms) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) continue;  // we leave on our own terms, below
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  CloseHandle(th);
}

// Leaving: what the person sees goes first (window, glow, sound), then what must be kept
// (settings, position), then the rest is let go without waiting for anyone.
void App::quit_now() {
  next_cancelled_=true;next_advance_.clear();
  if(folder_scan_)folder_scan_->cancelled=true;
  folder_scan_.reset();
  for (UINT_PTR t : {T_ANIM, T_POLL, T_WAKE, T_SAVE, T_THUMB, T_PERSIST, T_GEOM, T_PREFETCH, T_FINE, T_MAINTENANCE, T_REMOTE, T_REMOTE_START, T_NEXT, T_NEWS, T_FOLDER, T_QUEUE}) KillTimer(hwnd_, t);
  if (mpv_ok_) release_video_window();  // focus and mouse leave the engine's window first, as before
  // gone from the screen and the taskbar now, with Windows' own closing animation; the work
  // below happens behind it
  ShowWindow(hwnd_, SW_HIDE);
  ambient_.destroy();
  scene_.destroy();
  news_host_.destroy();
  pacer_.stop();
  remote_.stop(false);
  if (mpv_ok_) mpv_.set_flag_async("pause", true);  // silence at once (the engine may take a moment to end)
  save_position();
  save_now(true);
  // everything is on disk: a Cue opened from now on starts at once, as the first one, instead of
  // handing its file to this one, which is leaving, and waiting for it to be gone
  if (instance_) instance_->release();
  log("bye");
  if (drop_) { RevokeDragDrop(hwnd_); drop_->Release(); drop_ = nullptr; }
  thumbs_.kill();  // the preview helper keeps nothing: ended, not asked
  // a short grace so the engine can close the audio device and finish a frame being saved;
  // the window is already gone, so nobody waits for it
  shutdown_engine(1200);
  if (fine_timer_) { timeEndPeriod(1); fine_timer_ = false; }
  // everything that matters is on disk: skip the long teardown of a 70 MB engine DLL
  TerminateProcess(GetCurrentProcess(), 0);
  DestroyWindow(hwnd_);  // never reached; the ordinary way out, just in case
}

void App::update_cursor(double t) {
  g_hit_ui_scale = s_;
  if (closing_video_) return;
  if (mpv_win_ && !IsWindow(mpv_win_)) mpv_win_ = nullptr;  // the engine recreates its window per file
  if (!mpv_win_) {
    HWND child = FindWindowExW(video_, nullptr, nullptr, nullptr);
    if (child) {
      auto original = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(child, GWLP_WNDPROC));
      if (original && original != mpv_subclass && SetPropW(child, kMpvOriginalProc, reinterpret_cast<HANDLE>(original))) {
        mpv_win_ = child;
        SetWindowLongPtrW(child, GWLP_WNDPROC, (LONG_PTR)mpv_subclass);
      }
    }
  }
  POINT p = cursor_client();
  RECT rc;
  GetClientRect(hwnd_, &rc);
  bool inside = PtInRect(&rc, p) && GetForegroundWindow() == hwnd_;
  bool hide = mode_ == Mode::Play && inside && !in_sizemove_ && !play_.pinned() && t - play_.last_activity() > tok::dock_idle;
  int kind = mode_ == Mode::Play && inside ? play_.cursor_kind() : 0;
  if (hide != g_hide_cursor.load() || kind != g_cursor_kind.load()) {
    g_hide_cursor = hide;
    g_cursor_kind = kind;
    // re-evaluate the cursor now, without waiting for the mouse to move
    HWND target = mpv_win_ ? mpv_win_ : video_;
    PostMessageW(target, WM_SETCURSOR, (WPARAM)target, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
  }
}

// ── window procedures ───────────────────────────────────────────────
LRESULT CALLBACK App::wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
  App* self = nullptr;
  if (m == WM_NCCREATE) {
    self = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
    self->hwnd_ = h;
  } else {
    self = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  }
  if (!self) return DefWindowProcW(h, m, w, l);
  return self->handle(m, w, l);
}

// The video host only matters before mpv creates its own window (or for audio-only files).
LRESULT CALLBACK App::videoproc(HWND h, UINT m, WPARAM w, LPARAM l) {
  App* self = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (self && (m==WM_SETCURSOR||m==WM_LBUTTONDOWN||m==WM_LBUTTONDBLCLK)) {
    int edge=video_resize_hit(self->hwnd_);
    if(edge!=HTCLIENT) {
      if(m==WM_SETCURSOR) {
        SetCursor(LoadCursorW(nullptr,edge==HTLEFT||edge==HTRIGHT?IDC_SIZEWE:edge==HTTOP||edge==HTBOTTOM?IDC_SIZENS:edge==HTTOPLEFT||edge==HTBOTTOMRIGHT?IDC_SIZENWSE:IDC_SIZENESW));
      } else {
        POINT p; GetCursorPos(&p); PostMessageW(self->hwnd_,WM_NCLBUTTONDOWN,edge,MAKELPARAM(p.x,p.y));
      }
      return m==WM_SETCURSOR ? TRUE : 0;
    }
  }
  if (m == WM_SETCURSOR && LOWORD(l) == HTCLIENT && apply_cursor()) return TRUE;
  if (self && self->mode_ == Mode::Play) {
    switch (m) {
      case WM_MOUSEMOVE: self->on_play_mouse(GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
      case WM_LBUTTONDOWN:
      case WM_LBUTTONDBLCLK: SetCapture(h); self->mouse_button(0, true, GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
      case WM_LBUTTONUP: ReleaseCapture(); self->mouse_button(0, false, GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
      case WM_RBUTTONUP: self->mouse_button(1, true, GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
      case WM_MOUSEWHEEL:self->mouse_wheel(w,l);return 0;
    }
  }
  return DefWindowProcW(h, m, w, l);
}

static std::string vk_name(WPARAM vk) {
  switch (vk) {
    case VK_SPACE: return "space";
    case VK_LEFT: return "left";
    case VK_RIGHT: return "right";
    case VK_UP: return "up";
    case VK_DOWN: return "down";
    case VK_RETURN: return "enter";
    case VK_ESCAPE: return "esc";
    case VK_TAB: return "tab";
    case VK_VOLUME_UP: case VK_VOLUME_DOWN: case VK_VOLUME_MUTE: return "";
    case VK_MEDIA_PLAY_PAUSE: return "space";
    case VK_MEDIA_NEXT_TRACK: return "n";
    case VK_MEDIA_PREV_TRACK: return "mediaprev";
    case VK_HOME:return "home";
    case VK_END:return "end";
    case VK_PRIOR:return "pageup";
    case VK_NEXT:return "pagedown";
    case VK_DELETE:return "delete";
    case VK_OEM_PLUS: case VK_ADD: return "plus";
    case VK_OEM_MINUS: case VK_SUBTRACT: return "minus";
    case '0': case VK_NUMPAD0: return "zero";
  }
  if ((vk >= 'A' && vk <= 'Z')) return std::string(1, (char)(vk - 'A' + 'a'));
  return "";
}

LRESULT App::handle(UINT m, WPARAM w, LPARAM l) {
  if (m == taskbar_created_ && taskbar_created_) { taskbar_.created(hwnd_); taskbar_sync(); return 0; }
  if (m == WM_COMMAND && TaskbarButtons::is_click(w)) { taskbar_command(LOWORD(w)); return 0; }
  switch (m) {
    case WM_ACTIVATE:
      click_activated_t_ = LOWORD(w) == WA_CLICKACTIVE ? now() : 0;
      break;
    case WM_ACTIVATEAPP:
      if(!w&&!closing_app_) {play_.cancel_queue_gesture();ensure_anim();}
      if(w) set_hidden(false);  // brought forward: in view at once, not at the next check
      break;
    case WM_APP_QUEUE_CANCEL:
    case WM_CANCELMODE:
      if(!closing_app_) {play_.cancel_queue_gesture();ensure_anim();}break;
    case WM_CAPTURECHANGED:
      if(!closing_app_&&(GetKeyState(VK_LBUTTON)&0x8000)) {play_.cancel_queue_gesture();ensure_anim();}
      break;
    case WM_COPYDATA: {
      if (closing_app_) return FALSE;
      std::vector<std::string> files;
      if (!SingleInstance::decode(reinterpret_cast<const COPYDATASTRUCT*>(l), files)) return FALSE;
      // Copy before the sender exits, then open on our normal event loop.
      // Rapid launches coalesce to the latest request.
      if (!files.empty()) launch_files_ = std::move(files);
      return PostMessageW(hwnd_, WM_APP_OPEN_FILES, 0, 0) ? TRUE : FALSE;
    }
    case WM_APP_OPEN_FILES: {
      if (closing_app_) return 0;
      auto files = std::move(launch_files_);
      launch_files_.clear();
      if (!files.empty()) handle_files(files);
      if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
      SetForegroundWindow(hwnd_);
      return 0;
    }
    case WM_APP_ENGINE: on_engine_ready(w != 0); return 0;
    case WM_APP_LEFT_DOWN:
    case WM_APP_LEFT_UP:
    case WM_APP_RIGHT_UP: {
      HWND source = (HWND)w;
      if (source != mpv_win_ || !IsWindow(source)) return 0; // late input from an unloaded file
      POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
      MapWindowPoints(source, hwnd_, &p, 1);
      mouse_button(m == WM_APP_RIGHT_UP ? 1 : 0, m != WM_APP_LEFT_UP, p.x, p.y);
      return 0;
    }
    case WM_APP_WHEEL: mouse_wheel(w,l);return 0;
    case WM_APP_MOUSE: {
      g_mouse_pending = false;
      long xy = g_mouse_xy.load();
      on_play_mouse(GET_X_LPARAM(xy), GET_Y_LPARAM(xy));
      return 0;
    }
    case WM_APP_MEDIA_BUTTON: {
      // the lock screen's or the flyout's buttons (and media keys routed through them)
      if (mode_ == Mode::Play && loaded_) {
        bool paused = play_.in.paused || eof_;
        if ((w == MediaControls::kPlay && paused) || (w == MediaControls::kPause && !paused)) taskbar_command(TaskbarButtons::kPlayPause);
        else if (w == MediaControls::kNext) taskbar_command(TaskbarButtons::kNext);
        else if (w == MediaControls::kPrevious) taskbar_command(TaskbarButtons::kPrevious);
      }
      return 0;
    }
    case WM_APP_RECENTS: {
      auto* missing = reinterpret_cast<std::vector<std::wstring>*>(l);
      if ((unsigned)w == recents_gen_) {
        missing_ = std::set<std::wstring>(missing->begin(), missing->end());
        refresh_recents(false);
        ensure_anim();
      }
      delete missing;
      return 0;
    }
    case WM_NCCALCSIZE:
      if (w) {
        // no title bar: the side and bottom frames stay (resize, shadow, Windows 11 corners)
        auto* pr = reinterpret_cast<NCCALCSIZE_PARAMS*>(l);
        RECT orig = pr->rgrc[0];
        DefWindowProcW(hwnd_, m, w, l);
        if (!fullscreen_ && !IsZoomed(hwnd_)) {
          // The content covers the whole window, edge to edge. The native border pixel this
          // used to keep was drawn dark and showed as a thin line along the rounded corners.
          // DWM still rounds the window (and clips the video with it) and keeps its shadow.
          pr->rgrc[0] = orig;
          return 0;
        }
        pr->rgrc[0].top = orig.top;
        if (IsZoomed(hwnd_) && !fullscreen_)
          pr->rgrc[0].top += GetSystemMetricsForDpi(SM_CYFRAME, dpi_) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi_);
        return 0;
      }
      break;
    case WM_NCHITTEST: {
      LRESULT r = DefWindowProcW(hwnd_, m, w, l);
      if (r != HTCLIENT || fullscreen_) return r;
      POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
      ScreenToClient(hwnd_, &p);
      if (!IsZoomed(hwnd_)) {
        int edge = player_resize_hit(p.x, p.y, cw_, ch_, dpi_, s_);
        if (edge != HTCLIENT) return edge;
      }
      if (mode_ == Mode::Home && (onboarding_ ? ob_.is_background(p.x, p.y) : home_.is_background(p.x, p.y)))
        return HTCAPTION;  // drag + Snap, native
      return HTCLIENT;
    }
    case WM_APP_MPV: drain_mpv(); return 0;
    case WM_APP_REMOTE: remote_events(); return 0;
    case WM_APP_UPDATE: {
      auto* um = reinterpret_cast<UpdateMsg*>(l);
      if (um && !closing_app_) on_update(um);
      delete um;
      return 0;
    }
    case WM_APP_VSYNC:
      pacer_.consumed();
      if (anim_on_) frame();
      return 0;
    case WM_DISPLAYCHANGE: refresh_display_rate(); refresh_ui_scale(); if (mode_ == Mode::Play && loaded_) apply_smooth(); break;  // a new refresh rate: smoothing re-decides
    case WM_POWERBROADCAST:
      if (w == PBT_APMPOWERSTATUSCHANGE && mode_ == Mode::Play && loaded_) apply_smooth();  // plugged in or out
      break;
    case WM_APP_THUMB: {
      auto* tm = reinterpret_cast<ThumbMsg*>(l);
      if (w == 1 && tm) { on_thumb(tm); delete tm; }
      return 0;
    }
    case WM_WINDOWPOSCHANGING:
      // moving the mini window: it stays its size, under the pointer, wherever Snap would put it
      if (mini_drag_ && in_sizemove_) {
        auto* wp = reinterpret_cast<WINDOWPOS*>(l);
        if (!(wp->flags & SWP_NOSIZE) || !(wp->flags & SWP_NOMOVE)) {
          POINT c; GetCursorPos(&c);
          wp->x = c.x - mini_grab_.x; wp->y = c.y - mini_grab_.y;
          wp->cx = mini_drag_size_.cx; wp->cy = mini_drag_size_.cy;
          wp->flags &= ~(SWP_NOSIZE | SWP_NOMOVE);
        }
      }
      break;
    case WM_SIZE: {
      // maximized from the mini player another way (a shortcut): it is the full player again
      if (w == SIZE_MAXIMIZED && mini_player_) {
        mini_player_ = play_.in.mini_player = false; mini_drag_ = false;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, GetWindowLongPtrW(hwnd_, GWL_STYLE) | WS_MAXIMIZEBOX);
        set_pin(mini_was_pinned_);
        log("mini player off (maximized)");
      }
      // minimized, nothing follows the pointer or animates: the poll wakes 4 times a second instead
      // of 30 (the media card, the taskbar buttons and the phone still hear in time)
      if (bool iconic = w == SIZE_MINIMIZED; iconic != poll_slow_) { poll_slow_ = iconic; SetTimer(hwnd_, T_POLL, iconic ? 250 : 33, nullptr); }
      if (w == SIZE_MINIMIZED) { was_iconic_ = true; return 0; }
      // back from the taskbar at the very same size: nothing to rebuild. The engine's window is
      // not resized again and the overlays it already holds stay (re-sending them made the engine
      // redo its frame at the moment it comes back, and music could stumble there)
      if (was_iconic_) {
        was_iconic_ = false;
        if (LOWORD(l) == cw_ && HIWORD(l) == ch_ && cw_ > 0) {
          home_.chrome.maximized = play_.chrome.maximized = IsZoomed(hwnd_) != 0;
          if (mode_ == Mode::Play) play_.changed(now());
          ensure_anim();
          return 0;
        }
      }
      apply_window_corners();
      cw_ = LOWORD(l);
      ch_ = HIWORD(l);
      // an edge held (not only a move): the light around the window steps out until it is let go
      if (in_sizemove_ && !resizing_ && (cw_ != sizemove_start_.cx || ch_ != sizemove_start_.cy)) resizing_ = true;
      home_.chrome.maximized = play_.chrome.maximized = IsZoomed(hwnd_) != 0;
      if (video_) SetWindowPos(video_, nullptr, 0, 0, cw_, ch_, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOCOPYBITS);
      // the engine's own window gets the new size right away, asynchronously (its thread does the
      // resize; ours never waits on it), instead of whenever it notices the parent changed
      if (mpv_win_ && IsWindow(mpv_win_))
        SetWindowPos(mpv_win_, nullptr, 0, 0, cw_, ch_, SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS | SWP_NOCOPYBITS);
      // the scale follows the window, but never mid-gesture: while the person drags an edge it
      // holds (applied when they let go), and while the window animates to a video's shape it
      // takes the size it is going to, at once
      if (cw_ > 0 && ch_ > 0 && !in_sizemove_) {
        int tw = cw_, th = ch_;
        if (geom_.on) { tw = geom_.to.right - geom_.to.left; th = geom_.to.bottom - geom_.to.top; }
        double ns = scale_for(tw, th, store_.ui_scale);  // (the mini player keeps the interface's own size)
        if (ns != s_) {
          s_ = ns;
          trace("ui scale " + std::to_string(s_));
          // previews already decoded stay (a size off for a while costs nothing); restarting the
          // preview engine here is what used to make fullscreen changes stutter
        }
      }
      relayout();
      return 0;
    }
    case WM_DPICHANGED: {
      dpi_ = HIWORD(w);
      refresh_display_rate();
      make_cursors();
      if (fullscreen_) {
        MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowPos(hwnd_, nullptr, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right-mi.rcMonitor.left, mi.rcMonitor.bottom-mi.rcMonitor.top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
      }
      RECT* r = reinterpret_cast<RECT*>(l);
      {
        // the client area of the suggested window rect, at the new DPI
        RECT wr, cr; GetWindowRect(hwnd_, &wr); GetClientRect(hwnd_, &cr);
        int fw = (wr.right - wr.left) - cr.right, fh = (wr.bottom - wr.top) - cr.bottom;  // frame
        s_ = scale_for((r->right - r->left) - fw, (r->bottom - r->top) - fh, store_.ui_scale);  // (the mini player's rule too)
      }
      SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
    }
    case WM_WINDOWPOSCHANGED:
      ambient_.follow(hwnd_, in_sizemove_);  // move existing pixels; defer expensive shape rebuilds
      scene_.follow(hwnd_, s_);
      news_host_.follow(hwnd_, s_);
      break;
    case WM_GETMINMAXINFO: {
      auto* mm = reinterpret_cast<MINMAXINFO*>(l);
      // the smallest window where the layout still fits at this monitor's base scale (not the
      // current one: that grows with the window and would stop it from getting smaller), the
      // medium size's: the larger sizes do not make the smallest window larger (at "maior" a
      // film's window could not get under 1280 x 720 px at 150%); in a window that small, their
      // pixels step down to what it holds (ui_scale fits the 320 x 180 canvas)
      double bs = ui_base_scale(dpi_, kDefaultUiSize);
      mm->ptMinTrackSize.x = 240 * bs;  // narrow enough for a portrait video, the dock recomposes
      mm->ptMinTrackSize.y = 180 * bs;
      // playing, the window stops where the player still looks right: a film keeps a bar that
      // holds its playback and leaves the picture room (320 x 180: below it the bar covered most
      // of the picture); a song keeps its cover, its words or bar and some air (260 x 220). The
      // small window for watching beside other work is the mini player (Ctrl+M)
      // (not under our own animations: the way back from the mini player starts smaller)
      if (mode_ == Mode::Play && !fullscreen_ && !mini_player_ && !geom_.on) {
        RECT wr, cr; GetWindowRect(hwnd_, &wr); GetClientRect(hwnd_, &cr);
        const int fx = (wr.right - wr.left) - cr.right, fy = (wr.bottom - wr.top) - cr.bottom;  // invisible borders
        const bool song = audio_only_ && loaded_;
        mm->ptMinTrackSize.x = (LONG)std::lround((song ? 260 : 320) * bs) + fx;
        mm->ptMinTrackSize.y = (LONG)std::lround((song ? 220 : 180) * bs) + fy;
      }
      if (mini_player_) {
        // the mini player stays a mini player: its long side between 300 and 720 px at 100% (a
        // song's card between 400 and 640), in its own shape (WM_SIZING holds it)
        RECT wr, cr; GetWindowRect(hwnd_, &wr); GetClientRect(hwnd_, &cr);
        const int fx = (wr.right - wr.left) - cr.right, fy = (wr.bottom - wr.top) - cr.bottom;
        SIZE lo = mini_client(true), hi = mini_client(false);
        mm->ptMinTrackSize = {lo.cx + fx, lo.cy + fy};
        mm->ptMaxTrackSize = {hi.cx + fx, hi.cy + fy};
      }
      return 0;
    }
    case WM_SIZING:
      // the mini player keeps its shape while an edge or a corner is dragged
      if (mini_player_) {
        auto* r = reinterpret_cast<RECT*>(l);
        RECT wr, cr; GetWindowRect(hwnd_, &wr); GetClientRect(hwnd_, &cr);
        const int fx = (wr.right - wr.left) - cr.right, fy = (wr.bottom - wr.top) - cr.bottom;
        const double ar = mini_aspect();
        int cw = (r->right - r->left) - fx, ch = (r->bottom - r->top) - fy;
        const bool by_height = w == WMSZ_TOP || w == WMSZ_BOTTOM;
        if (by_height) cw = (int)std::lround(ch * ar); else ch = (int)std::lround(cw / ar);
        SIZE lo = mini_client(true), hi = mini_client(false);
        cw = std::clamp(cw, (int)lo.cx, (int)hi.cx); ch = (int)std::lround(cw / ar);
        // the edge that is not held stays where it is
        if (w == WMSZ_LEFT || w == WMSZ_TOPLEFT || w == WMSZ_BOTTOMLEFT) r->left = r->right - cw - fx; else r->right = r->left + cw + fx;
        if (w == WMSZ_TOP || w == WMSZ_TOPLEFT || w == WMSZ_TOPRIGHT) r->top = r->bottom - ch - fy; else r->bottom = r->top + ch + fy;
        return TRUE;
      }
      break;
    case WM_ENTERSIZEMOVE:
      in_sizemove_ = true;
      settle_fullscreen_motion();
      geom_.on = false;  // the person takes over: our animation stops right there
      { RECT r; GetClientRect(hwnd_, &r); sizemove_start_ = {r.right, r.bottom}; }
      break;
    case WM_EXITSIZEMOVE: {
      in_sizemove_ = false;
      resizing_ = false;
      if (mini_player_) mini_settle();  // where it was let go, or into a corner right beside it
      mini_drag_ = false;
      refresh_display_rate();  // it may have moved to a monitor with another refresh rate
      refresh_ui_scale();      // the size it was given while the edge was held
      ambient_.follow(hwnd_);
      if (mode_ == Mode::Play) { play_.activity(now()); ensure_anim(); }
      RECT r;
      GetClientRect(hwnd_, &r);
      if (mode_ == Mode::Play) music_fit_window();
      if (r.right != sizemove_start_.cx || r.bottom != sizemove_start_.cy) {
        user_sized_ = true;              // respected for this media
        auto_portrait_ = false;          // their shape now, not ours
        if (r.bottom <= r.right * 1.05) landscape_size_ = {r.right, r.bottom};
      }
      break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd_, &ps);
      if (mode_ == Mode::Home) {
        if (home_canvas_.w != cw_ || home_canvas_.h != ch_) {
          if (onboarding_) ob_.render(home_canvas_, now());
          else home_.render(home_canvas_, now());
        }
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = home_canvas_.w;
        bi.bmiHeader.biHeight = -home_canvas_.h;  // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(dc, 0, 0, home_canvas_.w, home_canvas_.h, 0, 0, 0, home_canvas_.h, home_canvas_.px.data(), &bi,
                          DIB_RGB_COLORS);
      } else {
        FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
      }
      EndPaint(hwnd_, &ps);
      return 0;
    }
    case WM_TIMER:
      if (w == T_ANIM) frame();
      else if (w == T_WAKE) { KillTimer(hwnd_, T_WAKE); ensure_anim(); }
      else if (w == T_SAVE) { save_position(); cue_log_flush(); }
      else if (w == T_PERSIST) save_now();
      else if (w == T_MAINTENANCE) {
        bool idle_home = mode_ == Mode::Home && pending_open_.empty() && mpv_ok_;
        bool settled_media = mode_ == Mode::Play && loaded_ && first_frame_ && !play_.iris_active() && now() - loaded_at_ >= 3;
        if (idle_home || settled_media || engine_failed_) {
          start_maintenance();
          if (maintenance_started_) KillTimer(hwnd_, T_MAINTENANCE);
        } else SetTimer(hwnd_, T_MAINTENANCE, 500, nullptr);
      }
      else if (w == T_GEOM) decide_window();
      else if (w == T_REMOTE) remote_tick();
      else if (w == T_REMOTE_START) { KillTimer(hwnd_, T_REMOTE_START); remote_start(); }
      else if (w == T_NEWS) { KillTimer(hwnd_, T_NEWS); show_news(); }
      else if (w == T_NEXT) {
        // the episode ended with a phone in hand: the next one, after a breath
        KillTimer(hwnd_, T_NEXT);
        auto target = next_advance_.take();
        if (!target.empty() && mode_ == Mode::Play && !next_cancelled_ && !store_.loop && remote_timer_ != -1 && next_auto_)
          {if(queue_.active()) {if(auto* i=queue_following())queue_play(i->id);} else open(std::move(target));}
      }
      else if (w == T_QUEUE) { queue_thumbnail_tick(); }
      else if (w == T_FOLDER) { finish_folder(); }
      else if (w == T_PREFETCH) { KillTimer(hwnd_, T_PREFETCH); thumb_next(); }
      else if (w == T_FINE) {
        KillTimer(hwnd_, T_FINE);
        if (!anim_on_ && fine_timer_) { timeEndPeriod(1); fine_timer_ = false; }
      }
      else if (w == T_THUMB) {
        // Start colour sampling after 1.5 s; timeline decoding still waits 3 s.
        KillTimer(hwnd_, T_THUMB);
        if (mode_ == Mode::Play && loaded_) {
          if (atmosphere_ready() && play_.in.dur > 0 && !hidden_now()) thumb_request(-1);
          else SetTimer(hwnd_, T_THUMB, 250, nullptr);
        }
      }
      else if (w == T_POLL && mode_ == Mode::Home && !IsIconic(hwnd_)) {
        try_hide_video();
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hwnd_, &p);
        last_cursor_ = p;
        if (onboarding_ ? ob_.track(p.x, p.y, now()) : home_.track(p.x, p.y, now())) ensure_anim();
        if (ambient_.active()) {  // leaving a film: its light fades out
          double tn = now(), adt = std::clamp(tn - atmo_t_, 0.0, 0.1);
          atmo_t_ = tn;
          step_atmosphere(adt);
        }
      }
      else if (w == T_POLL && mode_ == Mode::Play) {
        if (folder_wait_ && folder_list_ && folder_list_->done.load(std::memory_order_acquire)) {
          folder_wait_ = false;  // the folder is read: what asked for it runs again
          if (loaded_ && audio_only_ && !queue_.active()) music_album_queue();
          if (loaded_ && !queue_.active()) find_next_episode();
        }
        taskbar_sync();
        if (++hidden_poll_ >= 15) { hidden_poll_ = 0; set_hidden(covered()); }  // twice a second
        if (!audio_only_ && !next_ep_.empty() && play_.in.dur > 0 &&  // music's "up next" has no picture
            (play_.in.dur - play_.in.time) / std::max(.1, play_.in.speed) <= 90 && !next_thumb_requested_) {
          next_thumb_job_ = request_file_thumbnail(next_ep_);
          next_thumb_requested_ = !!next_thumb_job_;
        }
        if (next_thumb_job_ && next_thumb_job_->done.load()) {
          play_.set_next_picture(next_thumb_job_->pixels,84,48);
          next_thumb_job_.reset(); ensure_anim();
        }
        if (hidden_now()) {
          if (ambient_.active()) {
            double tn = now(), dt = std::clamp(tn - atmo_t_, 0.0, 0.1);
            atmo_t_ = tn;
            step_atmosphere(dt);
          }
          return 0;
        }
        POINT p = cursor_client();
        RECT rc;
        GetClientRect(hwnd_, &rc);
        bool inside = PtInRect(&rc, p) && GetForegroundWindow() == hwnd_;
        if (inside && (p.x != last_cursor_.x || p.y != last_cursor_.y)) {
          on_play_mouse(p.x, p.y);  // fallback: moves normally arrive from the video window right away
        } else if (play_.dock_visible() && !play_.pinned() && now() - play_.last_activity() > tok::dock_idle) {
          ensure_anim();  // lets the dock notice 2 s of stillness and leave
        }
        update_cursor(now());
        // Shift pressed or let go with the pointer still: a dragged bar follows at once
        if (play_.dock_dragging() && play_.set_shift(GetKeyState(VK_SHIFT) < 0)) ensure_anim();
        if (hw_log_t_ > 0 && now() > hw_log_t_) {
          hw_log_t_ = 0;
          // which codec and pixel format: what decides whether the graphics card can decode it
          // (software decoding of a film costs a processor core or more, even minimized)
          std::string codec = mpv_.get_string("current-tracks/video/codec"), profile = mpv_.get_string("current-tracks/video/codec-profile"),
                      pix = mpv_.get_string("video-params/hw-pixelformat");
          if (pix.empty()) pix = mpv_.get_string("video-params/pixelformat");
          log("video " + std::to_string(vid_w_) + "x" + std::to_string(vid_h_) + " " + (codec.empty() ? "?" : codec) +
              (profile.empty() ? "" : " (" + profile + ")") + (pix.empty() ? "" : " " + pix) + ", decode " +
              (hwdec_.empty() || hwdec_ == "no" ? std::string("software") : hwdec_) + ", sync " + (sync_sent_.empty() ? video_sync_ : sync_sent_));
        }
        {
          // atmosphere at ~30 Hz from this timer, never on the 120 Hz animation path
          double tn = now(), adt = std::clamp(tn - atmo_t_, 0.0, 0.1);
          atmo_t_ = tn;
          if (step_atmosphere(adt)) ensure_anim();
        }
        // Sample colours independently of drawing the light. No repeated decode
        // while paused; spatial interpolation continues at the UI poll's ~30 Hz.
        if (atmosphere_ready() && preview_.loaded && !preview_.failed && now() - accent_req_t_ > (store_.ambient && ambient_.active() && !mini_player_ ? 1.5 : 4.0) &&
            (accent_media_t_ < 0 || std::fabs(play_.in.time - accent_media_t_) >= 0.25) &&
            !hidden_now()) {
          accent_req_t_ = now();
          accent_media_t_ = play_.in.time;
          accent_want_ = play_.in.time;
          thumb_next();
        }
      }
      return 0;
    case WM_MOUSEMOVE:
      if (mode_ == Mode::Home) {
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&tme);
        if (onboarding_) ob_.mouse_move(GET_X_LPARAM(l), GET_Y_LPARAM(l), now());
        else home_.mouse_move(GET_X_LPARAM(l), GET_Y_LPARAM(l), now());
        ensure_anim();
      }
      return 0;
    case WM_NCMOUSEMOVE:
      // over the draggable background Windows reports non-client moves: the home still needs them
      if (mode_ == Mode::Home) {
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE | TME_NONCLIENT, hwnd_, 0};
        TrackMouseEvent(&tme);
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(hwnd_, &p);
        if (onboarding_) ob_.mouse_move(p.x, p.y, now());
        else home_.mouse_move(p.x, p.y, now());
        ensure_anim();
      }
      break;
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      if (mode_ == Mode::Home) {
        POINT p = cursor_client();
        RECT rc;
        GetClientRect(hwnd_, &rc);
        if (!PtInRect(&rc, p)) {  // really left, not client<->caption
          if (onboarding_) ob_.mouse_leave(now()); else home_.mouse_leave(now());
          ensure_anim();
        }
      } else if (mode_ == Mode::Play) {
        POINT p = cursor_client();
        RECT rc;
        GetClientRect(hwnd_, &rc);
        if (!PtInRect(&rc, p)) { play_.pointer_left(); ensure_anim(); }  // a tilted cover settles back
      }
      return 0;
    case WM_LBUTTONDOWN:
      if (mode_ == Mode::Home && onboarding_) {
        on_onboarding(ob_.mouse_down(GET_X_LPARAM(l), GET_Y_LPARAM(l), now()));
        ensure_anim();
        return 0;
      }
      if (mode_ == Mode::Home) {
        HomeAction a = home_.mouse_down(GET_X_LPARAM(l), GET_Y_LPARAM(l), now());
        if(a.a==HomeAct::OpenFolder) {auto p=pick_folder_dialog(hwnd_);if(!p.empty())queue_folder(p,true);}
    if (a.a == HomeAct::OpenDialog) { auto f = open_file_dialog(hwnd_, DialogKind::Video); if (!f.empty()) handle_files(f); }
        else if (a.a == HomeAct::ClearRecents) { store_.clear_recents(); save_state(); refresh_recents(); }
        else if (a.a == HomeAct::EggSeen) { store_.eggs_seen = home_.eggs_seen; save_state(); }
        else if (a.a == HomeAct::WinMin) ShowWindow(hwnd_, SW_MINIMIZE);
        else if (a.a == HomeAct::WinPin) toggle_pin();
        else if (a.a == HomeAct::WinMax) toggle_maximize();
        else if (a.a == HomeAct::WinClose) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        else if (a.a == HomeAct::Update) start_update();
        else if (a.a == HomeAct::Language) set_language_pref(a.index);
        else if (a.a == HomeAct::Remote) open_remote_scene();
        else if (a.a == HomeAct::News) show_news(true);
        else if (a.a == HomeAct::NewsClose) close_news_line();
        else if (a.a == HomeAct::Survey || a.a == HomeAct::SurveyClose) answer_survey(a.a == HomeAct::Survey);
        else if (a.a == HomeAct::SkipUpdate) {
          store_.update_skipped = upd_rel_.version;
          upd_ = Upd::Idle;
          save_state();
          show_update_state();
        }
        if (a.a == HomeAct::OpenRecent) {
      if (a.index >= 0 && a.index < (int)shown_paths_.size()) open(shown_paths_[a.index]);
        }
        ensure_anim();
      }
      return 0;
    case WM_CHAR:
      // ? is wherever the keyboard layout puts it: read as the character it types
      if (w == L'?') { key("?", false, false); return 0; }
      break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      if (m == WM_SYSKEYDOWN && w != VK_RETURN) break;
      bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
      if (m == WM_SYSKEYDOWN && w == VK_RETURN) { if (mode_ == Mode::Play) toggle_fullscreen(); return 0; }
      std::string k = vk_name(w);
      if (k.empty()) break;
      key(k, shift, ctrl);
      return 0;
    }
    case WM_THEMECHANGED:
    case WM_DWMCOMPOSITIONCHANGED:
    case WM_SETTINGCHANGE:
      play_.set_double_click_time(GetDoubleClickTime() / 1000.0);
      reduce_motion() = system_reduce_motion();
      set_high_contrast(system_high_contrast());
      apply_theme_frame();
      ensure_anim();
      break;
    case WM_CLOSE:
      if (closing_app_) return 0;
      closing_app_ = true;
      {
        // remember the window for next time (the normal rect, even if maximized or fullscreen)
        WINDOWPLACEMENT pl{sizeof(pl)};
        if (fullscreen_) pl = wp_; else if (mini_player_) pl = mini_restore_; else GetWindowPlacement(hwnd_, &pl);  // the mini player's window is never the one remembered
        RECT r = pl.rcNormalPosition;  // workspace coordinates: shift to screen coordinates
        MONITORINFO pmi{sizeof(pmi)};
        GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &pmi);
        OffsetRect(&r, pmi.rcWork.left - pmi.rcMonitor.left, pmi.rcWork.top - pmi.rcMonitor.top);
        if (r.right - r.left > 200 && r.bottom - r.top > 120) {
          bool mx = (fullscreen_ || mini_player_) ? pl.showCmd == SW_SHOWMAXIMIZED : IsZoomed(hwnd_) != 0;
          store_.win = {true, (int)r.left, (int)r.top, (int)(r.right - r.left), (int)(r.bottom - r.top), (int)dpi_, mx};
        }
      }
      quit_now();  // does not return
      return 0;
    case WM_ENDSESSION:
      // Windows is logging off or shutting down (no WM_CLOSE comes): keep the place in the film
      if (w) { save_position(); save_now(true); }
      return 0;
    case WM_DESTROY: media_controls_.detach(); PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(hwnd_, m, w, l);
}

void App::mouse_wheel(WPARAM keys,LPARAM screen_point) {
  if(closing_app_||mode_!=Mode::Play)return;
  if(GET_KEYSTATE_WPARAM(keys)&MK_CONTROL) {
    size_wheel_+=GET_WHEEL_DELTA_WPARAM(keys);
    if(std::abs(size_wheel_)>=WHEEL_DELTA) {
      int dir=size_wheel_>0?1:-1;size_wheel_=0;
      int next=next_ui_size(store_.ui_scale,dir);
      if(next!=store_.ui_scale)set_ui_scale(next);
      say_ui_size();
    }
    return;
  }
  POINT p{GET_X_LPARAM(screen_point),GET_Y_LPARAM(screen_point)};ScreenToClient(hwnd_,&p);
  play_.mouse_move(p.x,p.y,now());
  play_.wheel(GET_WHEEL_DELTA_WPARAM(keys)/(double)WHEEL_DELTA,now());run_actions();ensure_anim();
}
