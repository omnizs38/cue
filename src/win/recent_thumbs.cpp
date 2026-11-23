#include "recent_thumbs.h"
#include "../core/detached_thread.h"

#include <algorithm>
#include <fstream>
#include <set>

#include "../core/recents_view.h"
#include "../core/store.h"
#include "../ui/thumb_art.h"
#include "cover_image.h"
#include "platform.h"

namespace {

// Small on purpose: the home shows a picture 48 x 27 px of art (96 x 54 at 200%), and a picture
// that is grown a little stays a fine picture. ~2 KB on disk, ~57 KB in memory.
constexpr int kFrameW = 160, kFrameH = 90;  // the largest a film's picture is kept (16:9)
constexpr int kCoverSide = 96;              // a cover, read from the file once, then kept (~3 KB)
constexpr size_t kMemo = 48;                 // pictures kept in memory (the home shows six)

// A picture that shows nothing (a black frame, an empty answer from Windows) is no picture.
bool blank(const std::vector<uint32_t>& px) {
  if (px.empty()) return true;
  int lo = 255, hi = 0;
  double sum = 0;
  for (size_t i = 0; i < px.size(); i += 3) {
    const uint32_t v = px[i];
    const int l = (int)(((v >> 16) & 255) * 3 + ((v >> 8) & 255) * 6 + (v & 255)) / 10;
    lo = std::min(lo, l); hi = std::max(hi, l); sum += l;
  }
  const double avg = sum / ((px.size() + 2) / 3);
  return hi - lo < 12 || avg < 5;
}

// Reduced to fit kFrameW x kFrameH (its own shape kept), opaque.
std::shared_ptr<HomeThumb> fit_frame(const std::vector<uint32_t>& src, int w, int h) {
  if (src.empty() || w <= 0 || h <= 0 || src.size() < (size_t)w * h) return nullptr;
  auto t = std::make_shared<HomeThumb>();
  const double k = std::min({1.0, (double)kFrameW / w, (double)kFrameH / h});
  t->w = std::max(1, (int)std::lround(w * k));
  t->h = std::max(1, (int)std::lround(h * k));
  t->px.resize((size_t)t->w * t->h);
  if (t->w == w && t->h == h) {
    for (size_t i = 0; i < t->px.size(); ++i) t->px[i] = src[i] | 0xFF000000u;
  } else {
    std::vector<uint32_t> opaque(src.begin(), src.begin() + (size_t)w * h);
    for (auto& v : opaque) v |= 0xFF000000u;
    thumb_art::resample(opaque.data(), w, h, 0, 0, w, h, t->px.data(), t->w, t->w, t->h);
  }
  return t;
}

// A cover as kept: square or not, within kCoverSide (a cover read back keeps its size).
std::shared_ptr<HomeThumb> fit_cover(const std::vector<uint32_t>& src, int w, int h) {
  if (src.empty() || w <= 0 || h <= 0 || src.size() < (size_t)w * h) return nullptr;
  auto t = std::make_shared<HomeThumb>();
  const double k = std::min({1.0, (double)kCoverSide / w, (double)kCoverSide / h});
  t->w = std::max(1, (int)std::lround(w * k));
  t->h = std::max(1, (int)std::lround(h * k));
  t->cover = true;
  t->px.resize((size_t)t->w * t->h);
  std::vector<uint32_t> opaque(src.begin(), src.begin() + (size_t)w * h);
  for (auto& v : opaque) v |= 0xFF000000u;
  if (t->w == w && t->h == h) t->px = std::move(opaque);
  else thumb_art::resample(opaque.data(), w, h, 0, 0, w, h, t->px.data(), t->w, t->w, t->h);
  return t;
}

// when a file was last written (0: unknown)
uint64_t written_at(const std::filesystem::path& p) {
  WIN32_FILE_ATTRIBUTE_DATA fa{};
  if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa)) return 0;
  return (uint64_t)fa.ftLastWriteTime.dwHighDateTime << 32 | fa.ftLastWriteTime.dwLowDateTime;
}

bool read_file(const std::filesystem::path& p, std::string& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  const std::streamoff n = f.tellg();
  if (n <= 0 || n > (4 << 20)) return false;
  out.assign((size_t)n, '\0');
  f.seekg(0);
  return (bool)f.read(out.data(), n);
}

}  // namespace

// The film's picture as a small JPEG, written beside the others and moved into place in one step.
bool RecentThumbs::write_picture(const std::filesystem::path& dir, const std::string& path, const HomeThumb& t) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const std::string jpg = encode_jpeg_bgra(t.px, t.w, t.h);
  if (jpg.empty()) return false;
  const auto dst = dir / thumb_file_name(normalize_path(path));
  auto tmp = dst;
  tmp += L".tmp";
  bool ok = false;
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    ok = f && f.write(jpg.data(), (std::streamsize)jpg.size()) && f.flush();
  }
  if (ok) ok = MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
  if (!ok) std::filesystem::remove(tmp, ec);
  return ok;
}

void RecentThumbs::start(HWND hwnd, UINT msg, std::filesystem::path dir) {
  if (sh_) return;
  auto sh = std::make_shared<Shared>();
  sh->hwnd = hwnd;
  sh->msg = msg;
  sh->dir = std::move(dir);
  try {
    start_detached([sh] { run(sh); });
    sh_ = std::move(sh);
  } catch (...) {}
}

void RecentThumbs::stop() {
  if (!sh_) return;
  {
    std::lock_guard<std::mutex> l(sh_->mu);
    sh_->quit = true;
    sh_->jobs.clear();
  }
  sh_->cv.notify_all();
  sh_.reset();
}

void RecentThumbs::push(Job j) {
  if (!sh_) return;
  {
    std::lock_guard<std::mutex> l(sh_->mu);
    // (a newer frame of the same film replaces one still waiting to be written)
    if (j.kind == Job::Save)
      sh_->jobs.erase(std::remove_if(sh_->jobs.begin(), sh_->jobs.end(), [&](const Job& o) { return o.kind == Job::Save && o.path == j.path; }), sh_->jobs.end());
    // frames first: they are what the person just left
    if (j.kind == Job::Save) sh_->jobs.push_front(std::move(j)); else sh_->jobs.push_back(std::move(j));
  }
  sh_->cv.notify_one();
}

std::shared_ptr<const HomeThumb> RecentThumbs::cached(const std::string& path, bool& found) const {
  auto it = memo_.find(normalize_path(path));
  found = it != memo_.end();
  return found ? it->second : nullptr;
}

void RecentThumbs::remember(const std::string& path, std::shared_ptr<const HomeThumb> t) {
  const std::string k = normalize_path(path);
  asked_.erase(k);
  if (!memo_.count(k)) memo_order_.push_back(k);
  memo_[k] = std::move(t);
  while (memo_order_.size() > kMemo) { memo_.erase(memo_order_.front()); memo_order_.erase(memo_order_.begin()); }
}

void RecentThumbs::want(const std::vector<Want>& entries) {
  for (const auto& e : entries) {
    const std::string k = normalize_path(e.path);
    if (memo_.count(k) || asked_.count(k)) continue;
    asked_[k] = true;
    Job j;
    j.kind = Job::Load;
    j.path = e.path;
    j.audio = e.audio;
    j.last = e.last;
    push(std::move(j));
  }
}

void RecentThumbs::forget(const std::string& path) {
  const std::string k = normalize_path(path);
  memo_.erase(k);
  memo_order_.erase(std::remove(memo_order_.begin(), memo_order_.end(), k), memo_order_.end());
  asked_.erase(k);
}

void RecentThumbs::save_frame(const std::string& path, std::vector<uint32_t> px, int w, int h, bool cover) {
  if (path.empty() || px.empty() || w <= 0 || h <= 0 || px.size() != (size_t)w * h) return;
  Job j;
  j.kind = Job::Save;
  j.path = path;
  j.px = std::move(px);
  j.w = w;
  j.h = h;
  j.cover = cover;
  push(std::move(j));
}

void RecentThumbs::pause_loads() {
  if (!sh_) return;
  std::vector<std::string> dropped;
  {
    std::lock_guard<std::mutex> l(sh_->mu);
    for (auto it = sh_->jobs.begin(); it != sh_->jobs.end();)
      if (it->kind == Job::Load) { dropped.push_back(it->path); it = sh_->jobs.erase(it); } else ++it;
  }
  for (const auto& p : dropped) asked_.erase(normalize_path(p));
}

void RecentThumbs::keep_only(const std::vector<std::string>& paths) {
  Job j;
  j.kind = Job::Prune;
  j.keep = paths;
  push(std::move(j));
  // (and the memory: a cleared list starts again from the files)
  if (paths.empty()) { memo_.clear(); memo_order_.clear(); asked_.clear(); }
}

void RecentThumbs::run(std::shared_ptr<Shared> sh) {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
  const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  auto post = [&](RecentThumbResult* r) {
    HWND hwnd;
    {
      std::lock_guard<std::mutex> l(sh->mu);
      if (sh->quit) { delete r; return; }
      hwnd = sh->hwnd;
    }
    if (!PostMessageW(hwnd, sh->msg, 0, (LPARAM)r)) delete r;
  };
  auto file_for = [&](const std::string& path) { return sh->dir / thumb_file_name(normalize_path(path)); };
  for (;;) {
    Job j;
    {
      std::unique_lock<std::mutex> l(sh->mu);
      sh->cv.wait(l, [&] { return sh->quit || !sh->jobs.empty(); });
      if (sh->quit) break;
      j = std::move(sh->jobs.front());
      sh->jobs.pop_front();
    }
    try {
      if (j.kind == Job::Load) {
        // The picture kept in Cue's own folder is answered first, without touching the file
        // itself: a film on an external disk asleep (or a network share) took seconds to answer
        // whether it was there, and every picture after it in the queue waited too (16/11: after
        // Cue was closed a while, the recent's picture "was not kept" and came late). Whether the
        // file is still there is the recents' own check (App::recents_worker).
        auto* r = new RecentThumbResult{j.path, nullptr, false, false};
        std::vector<uint32_t> px;
        int w = 0, h = 0;
        std::string data;
        const auto file = file_for(j.path);
        if (!j.audio) {
          // a film: the frame the helper took; from before its last opening, it is stale
          WIN32_FILE_ATTRIBUTE_DATA fa{};
          if (GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &fa)) {
            const uint64_t ft = (uint64_t)fa.ftLastWriteTime.dwHighDateTime << 32 | fa.ftLastWriteTime.dwLowDateTime;
            const int64_t written = (int64_t)(ft / 10000000ull) - 11644473600ll;  // unix seconds
            r->stale = j.last > 0 && written + 2 < j.last;
            if (read_file(file, data) && decode_image(data, px, w, h, kFrameW) && !blank(px)) r->thumb = fit_frame(px, w, h);
          }
          post(r);
          continue;
        }
        // a song, a podcast, a book: its cover, square or not, whole. Kept once read: the next start
        // reads the small picture instead of the cover inside the file (the slow part). The kept
        // one is shown at once; if the file changed since (new tags, a new cover), the cover is
        // read again and replaces it.
        const uint64_t kept = written_at(file);
        bool shown = false;
        if (kept && read_file(file, data)) {
          if (decode_image(data, px, w, h, kCoverSide) && !blank(px)) {
            r->thumb = fit_cover(px, w, h);
          }
          if (r->thumb) { post(r); r = nullptr; shown = true; }
        }
        const DWORD attrs = GetFileAttributesW(widen(j.path).c_str());
        // a file kept only in the cloud (OneDrive and the like) is not read for its cover: reading
        // its tags would bring the whole file down. The picture saved before, if any, is enough
        if (attrs == INVALID_FILE_ATTRIBUTES || cloud_only_attributes(attrs) || (shown && kept >= written_at(widen(j.path)))) {
          if (r) post(r);
          continue;
        }
        if (!r) r = new RecentThumbResult{j.path, nullptr, shown, false};  // (a newer cover replaces the one shown)
        if (cover_image(j.path, px, w, h, kCoverSide) && !blank(px)) {
          if (auto t = fit_cover(px, w, h)) {
            write_picture(sh->dir, j.path, *t);
            r->thumb = std::move(t);
          }
        } else if (!shown) {
          // a cover the tags reader does not know (the engine does): the one Cue's helper took
          if (read_file(file, data) && decode_image(data, px, w, h, kFrameW) && !blank(px))
            if (auto t = fit_frame(px, w, h)) { t->cover = true; r->thumb = std::move(t); }
        }
        if (shown && !r->thumb) { delete r; continue; }  // (the one shown stays)
        post(r);
      } else if (j.kind == Job::Save) {
        if (blank(j.px)) continue;  // a black frame: the picture it had stays
        std::shared_ptr<HomeThumb> made = fit_frame(j.px, j.w, j.h);
        if (!made) continue;
        made->cover = j.cover;
        write_picture(sh->dir, j.path, *made);
        post(new RecentThumbResult{j.path, std::move(made), true, false});
      } else if (j.kind == Job::Prune) {
        std::set<std::string> keep;
        for (const auto& p : j.keep) keep.insert(thumb_file_name(normalize_path(p)));
        std::error_code ec;
        for (std::filesystem::directory_iterator it(sh->dir, ec), end; !ec && it != end; it.increment(ec)) {
          const auto name = narrow(it->path().filename().wstring());
          const bool ours = name.size() == 20 && name.compare(16, 4, ".jpg") == 0;
          const bool tmp = name.size() == 24 && name.compare(16, 8, ".jpg.tmp") == 0;
          if ((ours && !keep.count(name)) || tmp) { std::error_code e2; std::filesystem::remove(it->path(), e2); }
        }
      }
    } catch (...) { /* one picture lost; the next goes on */ }
  }
  if (SUCCEEDED(co)) CoUninitialize();
}
