#include "sound_parts.h"
#include "../core/detached_thread.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "../core/file_io.h"
#include "../core/util.h"
#include "mpv.h"
#include "platform.h"

namespace {

std::string read_file(const std::wstring& path) {
  std::ifstream f(std::filesystem::path(path), std::ios::binary);
  if (!f) return {};
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// `seconds` of the film's sound from `start`, as the engine plays it into a file of plain samples
// (8 kHz, one channel, 16 bits) as fast as it can decode, nothing on screen or in the speakers.
// `dur`: the film's length, when asked for.
bool read_sound(const std::string& path, double start, double seconds, const std::wstring& pcm_path,
                std::vector<int16_t>& out, double* dur) {
  out.clear();
  DeleteFileW(pcm_path.c_str());
  Mpv m;
  std::string err;
  if (!m.load(exe_dir(), &err) || !m.create(nullptr, 0, 0, &err)) return false;
  auto o = [&](const char* k, const std::string& v) { m.option(k, v); };
  o("config", "no"); o("terminal", "no"); o("msg-level", "all=no"); o("idle", "yes");
  o("vid", "no"); o("sid", "no"); o("sub-auto", "no"); o("audio-file-auto", "no"); o("cover-art-auto", "no");
  o("load-scripts", "no"); o("ytdl", "no"); o("osd-level", "0"); o("input-default-bindings", "no");
  o("cache", "no"); o("demuxer-readahead-secs", "1"); o("hr-seek", "yes"); o("replaygain", "no");
  o("ao", "pcm"); o("ao-pcm-file", narrow(pcm_path)); o("ao-pcm-waveheader", "no");
  o("audio-format", "s16"); o("audio-samplerate", std::to_string(intro::kRate)); o("audio-channels", "mono");
  o("untimed", "yes"); o("volume", "100"); o("volume-max", "100");
  char num[64];
  std::snprintf(num, sizeof num, "%.3f", std::max(0.0, start));
  o("start", num);
  std::snprintf(num, sizeof num, "%.3f", std::max(1.0, seconds));
  o("length", num);
  if (m.api().initialize(m.handle()) < 0) { m.destroy(); return false; }
  const char* cmd[] = {"loadfile", path.c_str(), nullptr};
  bool ended = false, failed = m.api().command(m.handle(), cmd) < 0;
  const ULONGLONG until = GetTickCount64() + 120000;  // never more than two minutes for a few minutes of sound
  while (!failed && !ended && GetTickCount64() < until) {
    mpv_event* e = m.api().wait_event(m.handle(), 1.0);
    if (!e) continue;
    if (e->event_id == MPV_EVENT_FILE_LOADED && dur) *dur = m.get_double("duration", 0);
    else if (e->event_id == MPV_EVENT_END_FILE) {
      auto* ef = static_cast<mpv_event_end_file*>(e->data);
      ended = true;
      failed = ef && ef->reason == MPV_END_FILE_REASON_ERROR;
    } else if (e->event_id == MPV_EVENT_SHUTDOWN) failed = true;
  }
  m.destroy();  // (the samples' file is complete once the engine is gone)
  const std::string bytes = read_file(pcm_path);
  DeleteFileW(pcm_path.c_str());
  if (failed || !ended || bytes.size() < 2) return false;
  out.resize(bytes.size() / 2);
  std::memcpy(out.data(), bytes.data(), out.size() * 2);
  return true;
}

}  // namespace

int sound_print_main(const std::string& path, const std::wstring& out, bool tail, double dur) {
  // after everything else on the PC: the processor and the disk
  SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
  SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
  intro::EpisodePrints e;
  std::vector<int16_t> pcm;
  const std::wstring pcm_path = out + L".pcm";
  if (!tail) {
    // the first minutes (where an opening is), and the film's length
    if (!read_sound(path, 0, intro::kHeadSeconds + 1, pcm_path, pcm, &dur)) return 2;
    if (!(dur >= intro::kMinFilm)) return 3;
    const size_t head = std::min(pcm.size(), (size_t)(intro::head_seconds(dur) * intro::kRate));
    e.head = intro::make_print(pcm.data(), head, 0);
  } else {
    // the last ones (where an ending is), the length known from the first read
    if (!(dur >= intro::kMinFilm)) return 3;
    const double from = intro::tail_start(dur);
    if (!read_sound(path, from, dur - from + 1, pcm_path, pcm, nullptr)) return 4;
    e.tail = intro::make_print(pcm.data(), pcm.size(), from);
  }
  e.dur = dur;
  const std::string bytes = intro::encode(e);
  const std::wstring tmp = out + L".part";
  {
    std::ofstream f(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc);
    if (!f.write(bytes.data(), (std::streamsize)bytes.size())) return 5;
  }
  return MoveFileExW(tmp.c_str(), out.c_str(), MOVEFILE_REPLACE_EXISTING) ? 0 : 6;
}

SoundFinder::~SoundFinder() { stop(); }

void SoundFinder::init(const std::wstring& folder, HWND hwnd, UINT msg) {
  std::lock_guard<std::mutex> lock(mu_);
  folder_ = folder;
  hwnd_ = hwnd;
  msg_ = msg;
}

std::string SoundFinder::identity(const std::string& path) {
  WIN32_FILE_ATTRIBUTE_DATA a{};
  if (!GetFileAttributesExW(widen(path).c_str(), GetFileExInfoStandard, &a)) return {};
  return path + "|" + std::to_string(((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow) + "|" +
         std::to_string(((uint64_t)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime);
}

void SoundFinder::load_locked() {
  if (loaded_ || folder_.empty()) return;
  loaded_ = true;
  const std::string text = read_file(folder_ + L"\\sound-parts.txt");
  for (size_t at = 0; at < text.size();) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(at, end - at);
    at = end + 1;
    const size_t tab = line.find('\t');
    if (tab == std::string::npos) continue;
    std::vector<SkipPart> parts;
    if (intro::decode_parts(line.substr(tab + 1), parts)) found_[line.substr(0, tab)] = line.substr(tab + 1);
  }
}

void SoundFinder::save_locked() {
  if (folder_.empty()) return;
  // at most a few thousand files remembered (about 150 kB): the oldest names go first
  while (found_.size() > 3000) found_.erase(found_.begin());
  std::string text;
  for (auto& [id, parts] : found_) text += id + "\t" + parts + "\n";
  const std::wstring path = folder_ + L"\\sound-parts.txt", tmp = path + L".tmp";
  {
    std::ofstream f(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc);
    if (!f.write(text.data(), (std::streamsize)text.size())) return;
  }
  MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

void SoundFinder::keep_identity_locked(const std::string& path, const std::string& id) {
  if (ids_.size() > 64) ids_.clear();  // (one a film opened: a long evening's worth)
  ids_[path] = id;
}

void SoundFinder::look_up(const std::string& path) {
  try {
    start_detached([this, path] {
      auto* r = new LookUp{path, false};
      const std::wstring w = widen(path);
      const DWORD a = GetFileAttributesW(w.c_str());
      r->reachable = a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY) && !cloud_only_attributes(a);
      const std::string id = r->reachable ? identity(path) : std::string();
      HWND hwnd;
      UINT msg;
      {
        std::lock_guard<std::mutex> lock(mu_);
        if (!id.empty()) keep_identity_locked(path, id);
        load_locked();  // (sound-parts.txt read here, not on the window's thread)
        hwnd = quit_ ? nullptr : hwnd_;
        msg = msg_;
      }
      if (!hwnd || !PostMessageW(hwnd, msg, 1, (LPARAM)r)) delete r;
    });
  } catch (...) {}
}

bool SoundFinder::known(const std::string& path, std::vector<SkipPart>& parts) {
  parts.clear();
  std::lock_guard<std::mutex> lock(mu_);
  auto kept = ids_.find(path);
  if (kept == ids_.end() || kept->second.empty()) return false;
  const std::string& id = kept->second;
  load_locked();
  auto it = found_.find(id);
  if (it != found_.end()) return intro::decode_parts(it->second, parts);
  auto early = early_.find(id);  // the opening, found before the ending is looked for
  return early != early_.end() && intro::decode_parts(early->second, parts);
}

void SoundFinder::request(const std::string& path, const std::string& partner) {
  std::lock_guard<std::mutex> lock(mu_);
  if (quit_ || folder_.empty()) return;
  next_ = std::make_unique<Job>(Job{path, partner});
  if (!worker_.joinable()) {
    try { worker_ = std::thread([this] { run(); }); } catch (...) { next_.reset(); return; }
  }
  cv_.notify_one();
}

void SoundFinder::stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    quit_ = true;
    next_.reset();
    if (helper_) TerminateProcess(helper_, 1);
  }
  cv_.notify_one();
  if (worker_.joinable()) worker_.join();
}

bool SoundFinder::prints_of(const std::string& path, bool tail, double dur, intro::EpisodePrints& out) {
  const std::string file = identity(path);
  if (file.empty()) return false;
  const std::string id = file + (tail ? "#tail" : "#head");
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (failed_.count(id)) return false;
    for (auto& [k, p] : prints_) if (k == id) { out = *p; return true; }
  }
  // the helper: this same Cue.exe, without a window, in a job that ends with Cue
  wchar_t exe[32768]{};
  const DWORD len = GetModuleFileNameW(nullptr, exe, 32768);
  if (!len || len >= 32768) return false;
  const std::wstring result = folder_ + L"\\sound-print.tmp";
  DeleteFileW(result.c_str());
  std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --sound-print \"" + result + L"\" \"" + widen(path) + L"\" " +
                     (tail ? L"tail " + std::to_wstring((long long)std::llround(dur * 1000)) : std::wstring(L"head 0"));
  STARTUPINFOW si{sizeof si};
  si.dwFlags = STARTF_FORCEOFFFEEDBACK;  // no spinning "working in background" pointer
  PROCESS_INFORMATION pi{};
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
  li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (job) SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li);
  if (!CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED | IDLE_PRIORITY_CLASS,
                      nullptr, nullptr, &si, &pi)) {
    if (job) CloseHandle(job);
    return false;
  }
  if (job) AssignProcessToJobObject(job, pi.hProcess);
  {
    std::lock_guard<std::mutex> lock(mu_);
    helper_ = pi.hProcess;
    if (quit_) TerminateProcess(pi.hProcess, 1);
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  WaitForSingleObject(pi.hProcess, 5 * 60 * 1000);
  DWORD code = 1;
  if (!GetExitCodeProcess(pi.hProcess, &code) || code == STILL_ACTIVE) { TerminateProcess(pi.hProcess, 1); code = 1; }
  {
    std::lock_guard<std::mutex> lock(mu_);
    helper_ = nullptr;
  }
  CloseHandle(pi.hProcess);
  if (job) CloseHandle(job);
  const std::string bytes = code == 0 ? read_file(result) : std::string();
  DeleteFileW(result.c_str());
  auto p = std::make_shared<intro::EpisodePrints>();
  std::lock_guard<std::mutex> lock(mu_);
  if (quit_) return false;
  if (!intro::decode(bytes, *p)) {
    if (code != 3) failed_.insert(id);  // (3: too short to look at; the caller remembers that)
    return false;
  }
  prints_.emplace_front(id, p);
  while (prints_.size() > 4) prints_.pop_back();
  out = *p;
  return true;
}

void SoundFinder::run() {
  // the comparison (a tenth of a second or two of arithmetic) waits for everything else too
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
  for (;;) {
    std::unique_ptr<Job> job;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&] { return quit_ || next_; });
      if (quit_) return;
      job = std::move(next_);
      load_locked();
    }
    // both there, and not only in the cloud (never brought down to be listened to): asked here,
    // not on the window's thread (18d)
    auto here = [](const std::string& p) {
      const DWORD a = GetFileAttributesW(widen(p).c_str());
      return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY) && !cloud_only_attributes(a);
    };
    if (!here(job->path) || !here(job->partner)) continue;
    const std::string ia = identity(job->path), ib = identity(job->partner);
    if (ia.empty() || ib.empty()) continue;
    {
      std::lock_guard<std::mutex> lock(mu_);
      keep_identity_locked(job->path, ia);
      keep_identity_locked(job->partner, ib);
      if (found_.count(ia)) { if (hwnd_) PostMessageW(hwnd_, msg_, 0, 0); continue; }
    }
    // first the openings (the film playing may be in its own right now), then the endings
    intro::EpisodePrints a, b, ta, tb;
    if (!prints_of(job->path, false, 0, a) || !prints_of(job->partner, false, 0, b)) continue;
    std::vector<SkipPart> pa, pb;
    intro::shared_parts(a, b, pa, pb);
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (quit_) return;
      early_[ia] = intro::encode_parts(pa);
      if (!pb.empty()) early_[ib] = intro::encode_parts(pb);
      if (hwnd_) PostMessageW(hwnd_, msg_, 0, 0);
    }
    if (prints_of(job->path, true, a.dur, ta) && prints_of(job->partner, true, b.dur, tb)) {
      a.tail = std::move(ta.tail);
      b.tail = std::move(tb.tail);
      intro::shared_parts(a, b, pa, pb);
    }
    std::lock_guard<std::mutex> lock(mu_);
    if (quit_) return;
    early_.erase(ia);
    early_.erase(ib);
    found_[ia] = intro::encode_parts(pa);
    // the partner's own: kept only when found (one episode without a match says little of it)
    if (!pb.empty() && !found_.count(ib)) found_[ib] = intro::encode_parts(pb);
    save_locked();
    if (hwnd_) PostMessageW(hwnd_, msg_, 0, 0);
  }
}
