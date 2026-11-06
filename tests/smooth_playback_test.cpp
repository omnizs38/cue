// Playback is never shaken by the window: minimizing and restoring change nothing in the engine's
// sound or picture chains (rebuilding the picture's filter or switching the frame timing made the
// sound stall for a moment both ways), only Cue's own poll slows down. A song opened again for a
// new look fades its sound out and back in through the engine's own gain, never the person's
// volume. (A source check: the engine itself runs only on Windows.)
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string body(const std::string& s, const std::string& from, const std::string& to) {
  auto a = s.find(from), b = s.find(to, a);
  assert(a != std::string::npos && b != std::string::npos);
  return s.substr(a, b - a);
}

int main() {
  std::ifstream f("src/win/app.cpp");
  std::stringstream ss; ss << f.rdbuf();
  const std::string s = ss.str();
  // minimized or restored: only the poll changes pace
  std::string size = body(s, "case WM_SIZE: {", "apply_window_corners();");
  assert(size.find("T_POLL") != std::string::npos);
  assert(size.find("vf") == std::string::npos && size.find("video-sync") == std::string::npos && size.find("update_music_idle") == std::string::npos);
  assert(s.find("@cueidle") == std::string::npos);
  // music never stops: the sound keeps a generous queue, and a window back from the taskbar at
  // its own size rebuilds nothing (no engine window resize, no overlay re-upload)
  assert(s.find("o(\"audio-buffer\", \"0.25\");") != std::string::npos);
  {
    std::string restore = body(s, "if (was_iconic_) {", "apply_window_corners();");
    assert(restore.find("return 0;") != std::string::npos && restore.find("SetWindowPos") == std::string::npos &&
           restore.find("upload_overlays") == std::string::npos && restore.find("play_.resize") == std::string::npos);
  }
  // the only picture filter is super resolution, set once while the engine prepares the file
  // (its preload hook), never while a film plays
  {
    size_t first = s.find("\"file-local-options/vf\"");
    assert(first == std::string::npos || s.find("\"file-local-options/vf\"", first + 1) == std::string::npos);
    std::string prep = body(s, "void App::configure_audio_mode() {", "if (!audio_only_) return;");
    if (first != std::string::npos) assert(prep.find("\"file-local-options/vf\"") != std::string::npos && prep.find("@cuevsr:d3d11vpp") != std::string::npos);
  }
  // a film switches its timing only under a full-screen window (as before 1.2.3), never minimized
  std::string idle = body(s, "void App::update_music_idle()", "void App::set_hidden(bool h)");
  assert(idle.find("hidden_now()") == std::string::npos && idle.find("&& hidden_;") != std::string::npos);
  // the look's reopen: out with the picture, back in after, the person's volume untouched
  std::string sw = body(s, "bool App::step_audio_switch(double t)", "void App::refresh_subtitles()");
  assert(sw.find("if (a.reopen) set_switch_gain(1 - k);") != std::string::npos);
  assert(sw.find("set_switch_gain(rm ? 1.0 : ease_out(") != std::string::npos);
  std::string gain = body(s, "void App::set_switch_gain(double g)", "std::vector<int64_t> App::all_audio_ids()");
  assert(gain.find("\"volume-gain\"") != std::string::npos && gain.find("\"volume\"") == std::string::npos);
  assert(body(s, "void App::cancel_audio_switch()", "// The picture while the sound changes").find("set_switch_gain(1)") != std::string::npos);
  std::puts("Smooth playback: minimizing touches no engine chain, a new look fades the sound out and in (never the volume) passed");
}
