#include "word_subtitles.h"
#include "../core/word_highlight.h"
#include <windows.h>
#include <fstream>
#include <string>

WordSubtitleJob::~WordSubtitleJob() {
  if (!file.empty()) { std::error_code ec; std::filesystem::remove(file, ec); }
}
namespace {
struct Request {
  std::shared_ptr<WordSubtitleJob> job;
  std::filesystem::path media, cache;
  bool reduced;
  HWND notify;
  UINT message;
};
DWORD WINAPI build_subtitles(void* context) {
  std::unique_ptr<Request> request(static_cast<Request*>(context));
  const auto job = request->job;
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
  try {
    auto sidecar = request->media;
    sidecar.replace_extension(L".words.json");
    std::ifstream input(sidecar, std::ios::binary | std::ios::ate);
    if (input) {
      auto size = input.tellg();
      if (size > 0 && size <= std::streamoff(kWordHighlightMaxInput)) {
        std::string text(static_cast<size_t>(size), '\0');
        input.seekg(0);
        if (input.read(text.data(), static_cast<std::streamsize>(size))) {
          auto ass = word_highlight_ass(text, request->reduced);
          if (!ass.empty()) {
            wchar_t temp[MAX_PATH]{};
            if (GetTempFileNameW(request->cache.c_str(), L"wrd", 0, temp)) {
              job->file = temp;
              std::ofstream output(job->file, std::ios::binary | std::ios::trunc);
              output.write(ass.data(), static_cast<std::streamsize>(ass.size()));
              output.close();
              if (!output) { std::error_code ec; std::filesystem::remove(job->file, ec); job->file.clear(); }
            }
          }
        }
      }
    }
  } catch (...) {
    if (!job->file.empty()) { std::error_code ec; std::filesystem::remove(job->file, ec); job->file.clear(); }
  }
  job->done.store(true, std::memory_order_release);
  // Wake the same event drain as libmpv, including when playback is paused.
  // No pointer is posted; shutdown cancels notifications before releasing UI.
  if (!job->cancelled.load()) PostMessageW(request->notify, request->message, 0, 0);
  return 0;
}
}
std::shared_ptr<WordSubtitleJob> request_word_subtitles(
    const std::filesystem::path& media, const std::filesystem::path& cache, bool reduced_motion, HWND notify, UINT message) {
  auto job = std::make_shared<WordSubtitleJob>();
  auto request = std::make_unique<Request>(Request{job, media, cache, reduced_motion, notify, message});
  HANDLE thread = CreateThread(nullptr, 0, build_subtitles, request.get(), 0, nullptr);
  if (!thread) return {};
  request.release();
  CloseHandle(thread);
  return job;
}
