// Another interface language, while a song plays: every cached picture of words is drawn again
// at once (the song's "N tracks", up next at the end of a song, an open menu), not when the
// pointer next touches it. And the home's recent list is written again in the new language.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

static void settle(PlayView& p, double& t, int n = 30) { for (int i = 0; i < n; ++i) { t += 1.0 / 60; p.tick(t, 1.0 / 60); } }

int main(int argc, char** argv) {
  std::filesystem::path exe = argc > 0 ? argv[0] : "";
  i18n::set_language(i18n::Lang::En);
  PlayView p;
  p.in.audio_only = true; p.in.cover_scene = true; p.in.cover_art = true; p.in.has_cover = true;
  p.in.queue_active = true; p.in.queue = {{1, "a.flac", true}, {2, "b.flac", false}, {3, "c.flac", false}};
  p.in.music_title = "Song"; p.in.music_artist = "Artist";
  p.in.dur = 200; p.in.time = 195; p.in.next_title = "Next song"; p.in.has_next = true;
  p.resize(1280, 720, 2);
  double t = 1; p.reset(t);
  p.in.dur = 200; p.in.time = 195;
  settle(p, t, 60);
  const uint64_t music0 = p.music_gen_, next0 = p.next_gen_;
  assert(p.layers[PlayView::L_MUSIC].visible && music0 > 0 && next0 > 0);
  // nothing changes: nothing is drawn again
  settle(p, t);
  assert(p.music_gen_ == music0 && p.next_gen_ == next0);
  // the language changes: the captions ("3 tracks" -> "3 faixas") and up next are drawn again
  i18n::set_language(i18n::Lang::PtBR);
  settle(p, t);
  assert(p.music_gen_ > music0 && p.next_gen_ > next0);
  // an open menu is rebuilt in the new words too
  p.right_click(400, 300, t); settle(p, t);
  std::string before;
  for (auto& r : p.rows_) before += r.label + "|";
  i18n::set_language(i18n::Lang::De);
  p.changed(t);  // as the app does after a language change
  settle(p, t);
  std::string after;
  for (auto& r : p.rows_) after += r.label + "|";
  assert(before != after);
  // the home's recent list is written again (its lines are built by the app)
  // Resolve the source tree from the executable's place (build/tests/…), so the
  // test also passes when launched from another directory.
  std::filesystem::path root = std::filesystem::absolute(exe).parent_path().parent_path().parent_path();
  std::ifstream f(root / "src/win/app.cpp");
  std::stringstream ss; ss << f.rdbuf();
  const std::string s = ss.str();
  auto a = s.find("void App::apply_language(bool relayout)"), b = s.find("void App::set_language_pref", a);
  assert(a != std::string::npos && s.substr(a, b - a).find("refresh_recents(false)") != std::string::npos);
  i18n::set_language(i18n::Lang::En);
  std::puts("Language refresh: the song's captions, up next, open menus and the home's recent list follow a new language at once passed");
}
