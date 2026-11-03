#pragma once
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <memory>

// One immutable request, with release/acquire publication of its result. No
// worker dereferences App or mpv; switching files/shutdown cannot call freed UI.
struct WordSubtitleJob {
  std::atomic<bool> done{false}, cancelled{false};
  std::filesystem::path file;
  bool submitted = false;  // UI thread only; keep file alive after sub-add
  ~WordSubtitleJob();
};
std::shared_ptr<WordSubtitleJob> request_word_subtitles(
    const std::filesystem::path& media, const std::filesystem::path& cache,
    bool reduced_motion, HWND notify, UINT message);
