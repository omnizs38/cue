#include "../src/core/recents_view.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace remote;
  assert(series_title("Twin.Peaks.S01E03.1080p.mkv") == "Twin Peaks");
  assert(series_title("[Group] RuPaul's Drag Race - S18E15.mkv") == "RuPaul's Drag Race");
  assert(series_title("Mother Mary.mkv").empty());
  assert(episode_label("Twin Peaks - 1x02 - Title.mkv") == "S01E02");
  assert(episode_label("Show ep 7.mp4") == "E07");
  std::vector<RecentFile> files = {
      {"C:\\v\\RuPaul's Drag Race - S18E15.mkv", 1200, 2078},
      {"C:\\m\\Mother Mary.mkv", 4052, 6746},
      {"C:\\v\\RuPaul's Drag Race - S18E16.mkv", 0, 3562},
      {"C:\\t\\Twin Peaks - S01E03.mkv", 1289, 2900},
      {"C:\\t\\Twin Peaks - S01E01.mkv", 0, 5642},
      {"C:\\a\\song.flac", 30, 200},
      {"C:\\v\\RuPaul's Drag Race - S18E14.mkv", 0, 3000},
  };
  auto live = live_recents(files, 6);
  assert(live.size() == 4);
  assert(live[0].title == "RuPaul's Drag Race" && live[0].episode == "S18E15" && live[0].episodes == 3);
  assert(live[0].path == files[0].path && live[0].pos == 1200);  // the latest episode opens
  assert(live[1].title == "Mother Mary" && live[1].episode.empty() && live[1].episodes == 1);
  assert(live[2].title == "Twin Peaks" && live[2].episodes == 2);
  assert(live[3].title == "song" && live[3].audio);
  // the cap counts entries, and episodes of shown entries keep counting past it
  auto two = live_recents(files, 2);
  assert(two.size() == 2 && two[0].episodes == 3);
  int h = 0, m = 0;
  split_minutes(24 * 60 + 10, h, m); assert(h == 0 && m == 25);
  split_minutes(3900, h, m); assert(h == 1 && m == 5);
  split_minutes(5, h, m); assert(h == 0 && m == 1);
  std::cout << "Recents view: shows grouped under their latest episode, titles without extensions, audio, caps and spans passed\n";
}
