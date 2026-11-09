// Previews and film colours use a separate helper: Cue.exe --thumb-host.
// Warmup/colour work runs below normal CPU priority; interactive navigation uses normal priority.
// Production previews decode on the CPU, leaving GPU decoding to playback. The helper is optional:
// failure removes previews without stopping the player. One active decode, one replaceable destination.
#pragma once
#include <windows.h>

#include <cstdint>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

int thumb_host_main(bool hardware_test = false);  // hardware_test is only used by the diagnostic

// One message from the helper, posted to the UI thread (lParam owns it).
struct ThumbMsg {
  enum Type : uint32_t { Ready = 1, Failed = 2, Frame = 3, NoFrame = 4, Gone = 5 } type;
  uint32_t id = 0;
  uint32_t session = 0, process = 0;
  double t = 0;
  double decode_ms = 0, prepare_ms = 0;
  bool hardware = false;
  int w = 0, h = 0;
  std::vector<uint32_t> px;
};

class ThumbClient {
 public:
  ThumbClient() = default;
  ~ThumbClient();
  ThumbClient(const ThumbClient&) = delete;
  ThumbClient& operator=(const ThumbClient&) = delete;
  bool start(HWND notify, UINT msg);  // spawns the helper (once)
  bool running() const;
  uint32_t epoch() const { return epoch_; }
  void open(const std::string& utf8_path, int width, uint32_t session);
  void shot(uint32_t id, double t, int output_width = 0, bool precise = false);
  void close_file();
  void shutdown();
  void kill();  // ends the helper at once, waits for nothing (the app is leaving)

 private:
  struct State;
  std::shared_ptr<State> state_;
  uint32_t epoch_ = 0;
  void send(const std::string& line, bool replace = false);
  static void read_loop(std::shared_ptr<State> state);
};
