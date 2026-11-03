// g++ -std=c++20 tests/audio_mix_persistence_test.cpp src/core/store.cpp src/core/json.cpp src/core/remote_proto.cpp -o audio_mix_persistence_test
#include "../src/core/store.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
int main() {
  namespace fs = std::filesystem;
  auto dir = fs::temp_directory_path() / ("cue-audio-mix-test-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directory(dir);
  for (int aid : {-2, -1, 0, 2}) {
    SettingsStore before;
    auto& item = before.touch("C:/media/movie.mkv", 600, 123);
    item.aid = aid;
    item.aid_lang = aid == -2 ? "1,3" : aid >= 0 ? "en" : "";
    assert(before.save(dir / "state.json"));
    SettingsStore after;
    assert(after.load(dir / "state.json"));
    assert(after.recents.size() == 1);
    assert(after.recents[0].aid == aid);
    assert(after.recents[0].aid_lang == item.aid_lang);
  }
  fs::remove_all(dir);
}
