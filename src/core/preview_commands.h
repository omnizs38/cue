#pragma once
#include <algorithm>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

// The UI never writes a pipe. A slow decoder gets a bounded mailbox instead.
class PreviewCommands {
 public:
  struct Ticket { uint64_t revision = 0, control = 0; };
  static constexpr size_t kMaxLine = 16 * 1024, kMaxBytes = 64 * 1024, kMaxItems = 16;
  bool push(std::string line, bool replace = false) {
    std::lock_guard<std::mutex> lock(mu_);
    if (stopped_ || line.empty() || line.size() > kMaxLine ||
        line.find_first_of("\r\n") != std::string::npos || line.find('\0') != std::string::npos) return false;
    if (replace) { queue_.clear(); bytes_ = 0; }
    // Only the newest pending seek matters. Preserve load/stop ahead of it.
    const bool shot = line.rfind("s ", 0) == 0;
    if (shot) {
      for (auto it = queue_.begin(); it != queue_.end();) {
        if (it->line.rfind("s ", 0) == 0) { bytes_ -= it->line.size(); it = queue_.erase(it); }
        else ++it;
      }
    }
    if (queue_.size() >= kMaxItems || bytes_ + line.size() > kMaxBytes) return false;
    bytes_ += line.size();
    ++revision_;
    if (!shot) ++control_;
    queue_.push_back({std::move(line), {revision_, control_}});
    cv_.notify_one();
    return true;
  }
  bool take(std::string& line, Ticket* ticket = nullptr) {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [&] { return stopped_ || !queue_.empty(); });
    if (stopped_) return false;
    if (ticket) *ticket = queue_.front().ticket;
    line = std::move(queue_.front().line);
    queue_.pop_front();
    bytes_ -= line.size();
    return true;
  }
  // A hover must not abort the file load it needs. A new file or shutdown must.
  bool superseded(Ticket ticket, bool loading = false) const {
    std::lock_guard<std::mutex> lock(mu_);
    return stopped_ || ticket.control != control_ || (!loading && ticket.revision != revision_);
  }
  void stop() {
    std::lock_guard<std::mutex> lock(mu_);
    stopped_ = true;
    queue_.clear(); bytes_ = 0;
    cv_.notify_all();
  }
 private:
  mutable std::mutex mu_;
  std::condition_variable cv_;
  struct Entry { std::string line; Ticket ticket; };
  std::deque<Entry> queue_;
  uint64_t revision_ = 0, control_ = 0;
  size_t bytes_ = 0;
  bool stopped_ = false;
};
