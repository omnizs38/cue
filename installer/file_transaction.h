#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <utility>

// Staged files and backups live on the destination volume. Existing files are never opened
// for writing: a failed commit restores them by rename. Keep backups if recovery itself fails.
class FileTransaction {
 public:
  FileTransaction(std::filesystem::path root, std::filesystem::path work)
      : root_(std::move(root)), work_(std::move(work)) {}
  ~FileTransaction() {
    try {
      std::error_code ec;
      if (!committed_ && !rollback(ec)) return;
      std::filesystem::remove_all(work_, ec);
    } catch (...) { /* Preserve recovery files if cleanup cannot finish. */ }
  }
  FileTransaction(const FileTransaction&) = delete;
  FileTransaction& operator=(const FileTransaction&) = delete;
  std::filesystem::path staged(const std::filesystem::path& rel) const { return work_ / "new" / rel; }
  void add(const std::filesystem::path& rel, bool remove = false) { entries_.push_back({rel, remove}); }
  const std::filesystem::path& recovery_path() const { return work_; }
  bool recovered() const {
    for (const auto& e : entries_) if (e.backed_up || e.installed) return false;
    return true;
  }
  bool commit(std::error_code& error) {
    for (auto& e : entries_) {
      auto dst = root_ / e.rel, backup = work_ / "old" / e.rel;
      bool exists = std::filesystem::exists(dst, error);
      if (error) return fail(error);
      if (exists) {
        if (!std::filesystem::is_regular_file(dst, error)) {
          if (!error) error = std::make_error_code(std::errc::invalid_argument);
          return fail(error);
        }
        std::filesystem::create_directories(backup.parent_path(), error);
        if (error) return fail(error);
        std::filesystem::rename(dst, backup, error);
        if (error) return fail(error);
        e.backed_up = true;
      }
      if (!e.remove) {
        std::filesystem::create_directories(dst.parent_path(), error);
        if (error) return fail(error);
        std::filesystem::rename(staged(e.rel), dst, error);
        if (error) return fail(error);
        e.installed = true;
      }
    }
    committed_ = true;
    return true;
  }
 private:
  struct Entry { std::filesystem::path rel; bool remove = false, backed_up = false, installed = false; };
  bool fail(std::error_code& error) {
    std::error_code recovery;
    if (!rollback(recovery)) error = recovery;
    return false;
  }
  bool rollback(std::error_code& error) {
    bool ok = true;
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
      std::error_code ec;
      auto dst = root_ / it->rel;
      if (it->installed) {
        std::filesystem::remove(dst, ec);
        if (ec) { error = ec; ok = false; continue; }
        it->installed = false;
      }
      if (it->backed_up) {
        std::filesystem::rename(work_ / "old" / it->rel, dst, ec);
        if (ec) { error = ec; ok = false; continue; }
        it->backed_up = false;
      }
    }
    return ok;
  }
  std::filesystem::path root_, work_;
  std::vector<Entry> entries_;
  bool committed_ = false;
};
