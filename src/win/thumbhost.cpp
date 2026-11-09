#include "thumbhost.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <cmath>
#include "../core/preview_commands.h"

#include "mpv.h"
#include "platform.h"
#include "../core/preview_session.h"
#include "../core/preview_size.h"
#include "../core/preview_protocol.h"

namespace {

using Header = PreviewHeader;

double clock_ms() {
  static const double frequency = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return (double)f.QuadPart; }();
  LARGE_INTEGER t; QueryPerformanceCounter(&t);
  return t.QuadPart * 1000.0 / frequency;
}

bool write_all(HANDLE h, const void* p, size_t n) {
  const char* c = static_cast<const char*>(p);
  while (n) {
    DWORD done = 0;
    if (!WriteFile(h, c, (DWORD)std::min<size_t>(n, 1 << 20), &done, nullptr) || !done) return false;
    c += done;
    n -= done;
  }
  return true;
}

bool read_all(HANDLE h, void* p, size_t n) {
  char* c = static_cast<char*>(p);
  while (n) {
    DWORD done = 0;
    if (!ReadFile(h, c, (DWORD)std::min<size_t>(n, 1 << 20), &done, nullptr) || !done) return false;
    c += done;
    n -= done;
  }
  return true;
}

void reply(HANDLE out, uint32_t type, uint32_t id, double t, int w = 0, int h = 0, const uint32_t* px = nullptr,
           uint32_t session = 0, double decode_ms = 0, double prepare_ms = 0, bool hardware = false) {
  Header hd;
  hd.type = type; hd.id = id; hd.t = t; hd.w = w; hd.h = h;
  hd.session = session;
  hd.decode_ms = decode_ms; hd.prepare_ms = prepare_ms; hd.flags = hardware ? 1u : 0u;
  write_all(out, &hd, sizeof hd);
  if (px && w > 0 && h > 0) write_all(out, px, (size_t)w * h * 4);
}

// waits for one of the wanted events (others are dropped); false on timeout or end
enum class WaitResult { Ready, Failed, Superseded };
struct ShotPriority {
  explicit ShotPriority(bool precise) {
    SetPriorityClass(GetCurrentProcess(), precise ? BELOW_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
  }
  ~ShotPriority() { SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS); }
};
WaitResult wait_for(Mpv& m, bool loading, double timeout, const PreviewCommands& commands,
                    PreviewCommands::Ticket ticket) {
  PreviewWait wait(loading, !loading);
  DWORD start = GetTickCount();
  while (GetTickCount() - start < (DWORD)(timeout * 1000)) {
    if (commands.superseded(ticket, loading)) return WaitResult::Superseded;
    mpv_event* e = m.api().wait_event(m.handle(), 0.010);
    if (!e) continue;
    auto event = PreviewWait::Event::Other;
    if (e->event_id == MPV_EVENT_FILE_LOADED) event = PreviewWait::Event::Loaded;
    else if (e->event_id == MPV_EVENT_SEEK) event = PreviewWait::Event::Seek;
    else if (e->event_id == MPV_EVENT_PLAYBACK_RESTART) event = PreviewWait::Event::Restart;
    else if (e->event_id == MPV_EVENT_SHUTDOWN) event = PreviewWait::Event::Shutdown;
    else if (e->event_id == MPV_EVENT_END_FILE) {
      auto* end = static_cast<mpv_event_end_file*>(e->data);
      event = end && end->reason == MPV_END_FILE_REASON_STOP ? PreviewWait::Event::Stop : PreviewWait::Event::End;
    }
    auto result = wait.consume(event);
    if (commands.superseded(ticket, loading)) return WaitResult::Superseded;
    if (result != PreviewWait::Result::Pending)
      return result == PreviewWait::Result::Ready ? WaitResult::Ready : WaitResult::Failed;
  }
  return WaitResult::Failed;
}

// Read independently of decoding, so the next seek can replace one already in progress.
// Chunked reads avoid a kernel call for every byte of a filename or pointer request.
void read_commands(HANDLE in, PreviewCommands& commands) {
  try {
    std::string line;
    char buffer[4096];
    DWORD n = 0;
    while (ReadFile(in, buffer, sizeof buffer, &n, nullptr) && n) {
      for (DWORD i = 0; i < n; ++i) {
        if (buffer[i] == '\n') {
          bool quit = line == "q";
          bool replace = line.rfind("s ", 0) != 0;
          if (!commands.push(std::move(line), replace)) { commands.stop(); return; }
          if (quit) return;
          line.clear();
        } else {
          if (line.size() >= PreviewCommands::kMaxLine) { commands.stop(); return; }
          line += buffer[i];
        }
      }
    }
  } catch (...) { /* a broken pipe or allocation only ends this optional helper */ }
  commands.stop();
}

}  // namespace

// ── helper side ─────────────────────────────────────────────────────
int thumb_host_main(bool hardware_test) {
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
  // Below-normal CPU priority; playback keeps the higher priority and owns the GPU.
  SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
  Mpv m;
  std::string err;
  if (!m.load(exe_dir(), &err) || !m.create(nullptr, 0, 0, &err)) { reply(out, ThumbMsg::Gone, 0, 0); return 1; }
  auto o = [&](const char* k, const char* v) { m.option(k, v); };
  o("config", "no"); o("terminal", "no"); o("idle", "yes"); o("pause", "yes"); o("keep-open", "always");
  o("vo", "null");
  o("audio", "no"); o("aid", "no"); o("sid", "no"); o("sub-auto", "no");
  o("hwdec", hardware_test ? "auto-copy" : "no");  // opt-in diagnostic only; normal playback keeps its GPU
  // Retain the bounded existing decoder-thread budget; changing thread/hardware policy
  // needs native measurements under simultaneous playback, not a scheduler simulation.
  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  const std::string threads = std::to_string(std::clamp((int)si.dwNumberOfProcessors / 2, 1, 4));
  o("hr-seek", "no"); o("vd-lavc-threads", threads.c_str());
  o("vd-lavc-skiploopfilter", "all"); o("vd-lavc-fast", "yes");
  o("sws-scaler", "fast-bilinear"); o("cache", "no"); o("demuxer-readahead-secs", "0");
  o("load-scripts", "no"); o("ytdl", "no"); o("osd-level", "0"); o("input-default-bindings", "no");
  o("msg-level", "all=no");
  if (m.api().initialize(m.handle()) < 0 || !m.api().command_ret) { reply(out, ThumbMsg::Gone, 0, 0); return 1; }
  PreviewCommands commands;
  std::thread reader(read_commands, in, std::ref(commands));
  PreviewCommands::Ticket ticket;
  std::string line;
  std::vector<uint32_t> px;
  bool loaded = false;
  bool hardware_known = false, hardware_in_use = false;
  uint32_t session = 0;
  while (commands.take(line, &ticket)) {
    if (line == "q") break;
    if (line.size() > 2 && line[0] == 'o') {
      SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
      hardware_known = hardware_in_use = false;
      // o <session> <width> <path> (the remainder is the full UTF-8 filename)
      size_t sp = line.find(' ', 2), next = sp == std::string::npos ? sp : line.find(' ', sp + 1);
      if (next == std::string::npos) continue;
      session = (uint32_t)std::strtoul(line.c_str() + 2, nullptr, 10);
      std::string vf = "scale=" + line.substr(sp + 1, next - sp - 1) + ":-2";
      std::string path = line.substr(next + 1);
      m.set("vf", vf);
      const char* args[] = {"loadfile", path.c_str(), "replace", nullptr};
      auto result = m.api().command(m.handle(), args) >= 0
          ? wait_for(m, true, 20, commands, ticket) : WaitResult::Failed;
      loaded = result == WaitResult::Ready;
      if (result == WaitResult::Superseded) continue;
      reply(out, loaded ? ThumbMsg::Ready : ThumbMsg::Failed, 0,
            loaded ? m.get_double("duration", 0) : 0, 0, 0, nullptr, session);
    } else if (line.size() > 2 && line[0] == 's') {
      // s <id> <time> <output width> <precise>; colours use precise seeks, navigation uses keyframes
      uint32_t id = 0;
      double t = 0;
      int output_width = 0;
      int precise = 1;
      if (sscanf(line.c_str() + 2, "%u %lf %d %d", &id, &t, &output_width, &precise) < 2 || !loaded) {
        reply(out, ThumbMsg::NoFrame, id, t, 0, 0, nullptr, session); continue;
      }
      const ShotPriority priority(precise != 0);
      while (true) {  // forget anything left from earlier requests
        mpv_event* e = m.api().wait_event(m.handle(), 0);
        if (!e || e->event_id == MPV_EVENT_NONE) break;
      }
      char ts[32];
      snprintf(ts, sizeof ts, "%.3f", std::max(0.0, t));
      const double decode_start = clock_ms();
      const char* sk[] = {"seek", ts, preview_seek_mode(precise != 0), nullptr};
      auto result = m.api().command(m.handle(), sk) >= 0
          ? wait_for(m, false, 10, commands, ticket) : WaitResult::Failed;
      if (result == WaitResult::Superseded) continue;
      bool ok = result == WaitResult::Ready;
      const double decoded_at = clock_ms();
      mpv_node res{};
      const char* sh[] = {"screenshot-raw", "video", nullptr};
      if (ok && m.api().command_ret(m.handle(), sh, &res) >= 0 && res.format == MPV_FORMAT_NODE_MAP) {
        int64_t w = 0, h = 0, stride = 0;
        const mpv_byte_array* ba = nullptr;
        for (int i = 0; i < res.u.list->num; ++i) {
          const char* k = res.u.list->keys[i];
          const mpv_node& v = res.u.list->values[i];
          if (!strcmp(k, "w") && v.format == MPV_FORMAT_INT64) w = v.u.int64;
          else if (!strcmp(k, "h") && v.format == MPV_FORMAT_INT64) h = v.u.int64;
          else if (!strcmp(k, "stride") && v.format == MPV_FORMAT_INT64) stride = v.u.int64;
          else if (!strcmp(k, "data") && v.format == MPV_FORMAT_BYTE_ARRAY) ba = (const mpv_byte_array*)v.u.ba;
        }
        if (w > 0 && h > 0 && w <= 2048 && h <= 2048 && ba && ba->data && stride >= w * 4 && (uint64_t)stride <= ba->size / (uint64_t)h) {
          const auto size = preview_size((int)w, (int)h, output_width);
          px.resize((size_t)size.w * size.h);
          for (int y = 0; y < size.h; ++y) {
            const int64_t sy = (int64_t)y * h / size.h;
            const uint32_t* src = (const uint32_t*)((const uint8_t*)ba->data + sy * stride);
            uint32_t* dst = px.data() + (size_t)y * size.w;
            for (int x = 0; x < size.w; ++x) dst[x] = src[(int64_t)x * w / size.w] | 0xFF000000u;
          }
          if (!commands.superseded(ticket)) {
            const double actual = m.get_double("time-pos", t);
            if (!hardware_known) {
              const std::string hw = m.get_string("hwdec-current");
              hardware_in_use = !hw.empty() && hw != "no";
              hardware_known = true;
            }
            reply(out, ThumbMsg::Frame, id, actual, size.w, size.h, px.data(), session,
                  decoded_at - decode_start, clock_ms() - decoded_at, hardware_in_use);
          }
        } else {
          reply(out, ThumbMsg::NoFrame, id, t, 0, 0, nullptr, session);
        }
      } else {
        reply(out, ThumbMsg::NoFrame, id, t, 0, 0, nullptr, session);
      }
      m.api().free_node_contents(&res);  // also release unexpected result types
    } else if (line == "x") {
      const char* st[] = {"stop", nullptr};
      m.api().command(m.handle(), st);
      loaded = false;
    }
  }
  reader.join();  // q/EOF already ended the reader; there is no blocked read to wait for
  m.destroy();
  return 0;
}

// ── player side ─────────────────────────────────────────────────────
// Detached I/O workers own this state, never the UI's ThumbClient. Handles remain
// valid until both workers exit, including when shutdown races a pending read/write.
struct ThumbClient::State {
  HANDLE proc = nullptr, job = nullptr, to = nullptr, from = nullptr;
  HWND notify = nullptr;
  UINT message = 0;
  uint32_t epoch = 0;
  std::atomic<uint32_t> session{0};
  std::mutex mu;
  bool stopped = false;
  PreviewCommands commands;
  ~State() {
    if (to) CloseHandle(to);
    if (from) CloseHandle(from);
    if (job) CloseHandle(job);
    if (proc) CloseHandle(proc);
  }
  void post(std::unique_ptr<ThumbMsg> m) {
    std::lock_guard<std::mutex> lock(mu);
    if (!stopped && PostMessageW(notify, message, 1, (LPARAM)m.get())) m.release();
  }
  void stop(bool report) {
    std::lock_guard<std::mutex> lock(mu);
    if (stopped) return;
    if (report) {
      std::unique_ptr<ThumbMsg> m(new(std::nothrow) ThumbMsg{});
      if (m) {
      m->type = ThumbMsg::Gone; m->process = epoch; m->session = session.load();
      if (PostMessageW(notify, message, 1, (LPARAM)m.get())) m.release();
      }
    }
    stopped = true;
    commands.stop();
    // Ending the child closes its pipe ends and releases blocked I/O workers.
    if (proc) TerminateProcess(proc, 0);
  }
};

ThumbClient::~ThumbClient() { shutdown(); }
bool ThumbClient::running() const {
  if (!state_) return false;
  std::lock_guard<std::mutex> lock(state_->mu);
  return !state_->stopped && state_->proc && WaitForSingleObject(state_->proc, 0) == WAIT_TIMEOUT;
}

bool ThumbClient::start(HWND notify, UINT msg) {
  if (running()) return true;
  shutdown();
  auto s = std::make_shared<State>();
  s->notify = notify; s->message = msg; s->epoch = ++epoch_;
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE child_in = nullptr, child_out = nullptr;
  if (!CreatePipe(&child_in, &s->to, &sa, 1 << 12)) return false;
  if (!CreatePipe(&s->from, &child_out, &sa, 1 << 20)) { CloseHandle(child_in); return false; }
  auto close_child = [&] { CloseHandle(child_in); CloseHandle(child_out); };
  if (!SetHandleInformation(s->to, HANDLE_FLAG_INHERIT, 0) ||
      !SetHandleInformation(s->from, HANDLE_FLAG_INHERIT, 0)) { close_child(); return false; }
  wchar_t exe[32768]{};
  DWORD len = GetModuleFileNameW(nullptr, exe, 32768);
  if (!len || len >= 32768) { close_child(); return false; }
  std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --thumb-host";
  STARTUPINFOEXW si{};
  si.StartupInfo.cb = sizeof(si);
  // no "working in background" cursor: the helper never shows a window, so Windows would spin
  // the pointer for up to five seconds each time it starts
  si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
  si.StartupInfo.hStdInput = child_in;
  si.StartupInfo.hStdOutput = si.StartupInfo.hStdError = child_out;
  SIZE_T bytes = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
  std::vector<unsigned char> attrs(bytes);
  si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
  if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes)) { close_child(); return false; }
  HANDLE inherit[] = {child_in, child_out};
  BOOL attributes_ok = UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                 inherit, sizeof inherit, nullptr, nullptr);
  PROCESS_INFORMATION pi{};
  BOOL ok = attributes_ok && CreateProcessW(exe, cmd.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si.StartupInfo, &pi);
  DeleteProcThreadAttributeList(si.lpAttributeList);
  close_child();
  if (!ok) return false;
  s->proc = pi.hProcess;
  s->job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
  li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!s->job || !SetInformationJobObject(s->job, JobObjectExtendedLimitInformation, &li, sizeof li) ||
      !AssignProcessToJobObject(s->job, s->proc)) {
    s->stop(false); CloseHandle(pi.hThread); return false;
  }
  state_ = s;
  try {
    std::thread([s] { read_loop(s); }).detach();
    std::thread([s] {
      try {
        std::string line;
        while (s->commands.take(line)) {
          line += '\n';
          if (!write_all(s->to, line.data(), line.size())) break;
        }
      } catch (...) { /* fail this optional helper, not the player */ }
      s->stop(true);
    }).detach();
  } catch (...) {
    s->stop(false); CloseHandle(pi.hThread); shutdown(); return false;
  }
  ok = ResumeThread(pi.hThread) != (DWORD)-1;
  CloseHandle(pi.hThread);
  if (!ok) { shutdown(); return false; }
  return true;
}

void ThumbClient::read_loop(std::shared_ptr<State> s) {
  try {
    while (true) {
      Header hd;
      if (!read_all(s->from, &hd, sizeof hd) || hd.magic != 0x33565054 ||
          hd.type < ThumbMsg::Ready || hd.type >= ThumbMsg::Gone || !std::isfinite(hd.t) ||
          !std::isfinite(hd.decode_ms) || hd.decode_ms < 0 || !std::isfinite(hd.prepare_ms) || hd.prepare_ms < 0 ||
          hd.flags > 1 || hd.reserved) break;
      auto m = std::make_unique<ThumbMsg>();
      m->type = (ThumbMsg::Type)hd.type;
      m->id = hd.id; m->session = hd.session; m->process = s->epoch; m->t = hd.t;
      m->decode_ms = hd.decode_ms; m->prepare_ms = hd.prepare_ms; m->hardware = (hd.flags & 1) != 0;
      if (hd.type == ThumbMsg::Frame) {
        if (hd.w <= 0 || hd.h <= 0 || hd.w > 2048 || hd.h > 2048) break;
        m->w = hd.w; m->h = hd.h;
        m->px.resize((size_t)hd.w * hd.h);
        if (!read_all(s->from, m->px.data(), m->px.size() * 4)) break;
      }
      // Stale file responses never occupy the UI message queue with pixel buffers.
      if (m->session == s->session.load()) s->post(std::move(m));
    }
  } catch (...) { /* allocation/protocol failure only disables previews */ }
  s->stop(true);
}

void ThumbClient::send(const std::string& line, bool replace) {
  if (state_ && !state_->commands.push(line, replace)) state_->stop(true);
}
void ThumbClient::open(const std::string& path, int width, uint32_t session) {
  if (!state_) return;
  state_->session = session;
  send("o " + std::to_string(session) + " " + std::to_string(width) + " " + path, true);
}
void ThumbClient::shot(uint32_t id, double t, int output_width, bool precise) {
  char b[80];
  snprintf(b, sizeof b, "s %u %.3f %d %d", id, t, output_width, precise ? 1 : 0);
  send(b);
}
void ThumbClient::close_file() { send("x", true); }
void ThumbClient::shutdown() {
  auto s = std::move(state_);
  if (!s) return;
  s->stop(false);  // no worker can post after this returns
  // PostMessage transfers ownership, but destroying a window doesn't delete lParam.
  // Called by the owning UI thread before restart/destruction.
  MSG msg;
  while (PeekMessageW(&msg, s->notify, s->message, s->message, PM_REMOVE))
    if (msg.wParam == 1) delete reinterpret_cast<ThumbMsg*>(msg.lParam);
}
void ThumbClient::kill() { shutdown(); }
