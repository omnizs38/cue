#pragma once
#include <filesystem>
#include <stdexcept>

// Both build/tests and build/asan/bin must find the same integration source.
inline std::filesystem::path source_root(const char* executable) {
  auto directory = std::filesystem::absolute(executable).parent_path();
  while (!directory.empty()) {
    if (std::filesystem::is_regular_file(directory / "src/win/app.cpp")) return directory;
    const auto parent = directory.parent_path();
    if (parent == directory) break;
    directory = parent;
  }
  throw std::runtime_error("Cannot locate Cue source tree from test executable");
}
