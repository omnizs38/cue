// Without a list of the person's own, a film is followed only by the next episode of its series.
#include "../src/core/remote_proto.h"
#include <cassert>
#include <cstdio>

int main() {
  using remote::next_in_series;
  // the bug: a film in Downloads offered the song beside it
  std::vector<std::string> downloads = {"Aftersun (2022).mkv", "Billie Eilish - Lunch.mp3", "setup.exe", "notes.txt",
                                        "Zodiac.2007.1080p.mkv"};
  assert(next_in_series("Aftersun (2022).mkv", downloads).empty());
  assert(next_in_series("Zodiac.2007.1080p.mkv", downloads).empty());
  // the same series, spelled a little differently from one file to the next
  std::vector<std::string> peaks = {"Twin Peaks - S01E01.mkv", "Twin Peaks S01E02.mkv", "Twin Peaks S01E03.mkv",
                                    "Twin Peaks - Soundtrack.mp3", "Lost S01E02.mkv"};
  assert(next_in_series("Twin Peaks - S01E01.mkv", peaks) == "Twin Peaks S01E02.mkv");
  assert(next_in_series("Twin Peaks S01E03.mkv", peaks).empty());  // the last one: nothing, not Lost
  // a new season follows the last episode of the one before
  assert(next_in_series("show.s01e10.mkv", {"show.s01e10.mkv", "show.s02e01.mkv", "other.s01e11.mkv"}) == "show.s02e01.mkv");
  // an episode that is a song is never offered after a film
  assert(next_in_series("Pod S01E01.mp4", {"Pod S01E01.mp4", "Pod S01E02.mp3"}).empty());
  // numbered without an episode marker: the same name with the next number
  std::vector<std::string> anime = {"[Sub] Show - 01 [1080p].mkv", "[Sub] Show - 02 [1080p].mkv", "[Sub] Show - 10 [1080p].mkv",
                                    "[Sub] Other - 03 [1080p].mkv", "ending.mp3"};
  assert(next_in_series("[Sub] Show - 01 [1080p].mkv", anime) == "[Sub] Show - 02 [1080p].mkv");
  assert(next_in_series("[Sub] Show - 02 [1080p].mkv", anime) == "[Sub] Show - 10 [1080p].mkv");  // natural order
  assert(next_in_series("[Sub] Show - 10 [1080p].mkv", anime).empty());
  // two different films with years are not a series
  assert(next_in_series("Heat (1995).mkv", {"Heat (1995).mkv", "Ronin (1998).mkv"}).empty());
  // a song never asks for a film's next
  assert(next_in_series("Song 01.mp3", {"Song 01.mp3", "Song 02.mp3"}).empty());
  std::puts("up next without a list: only the same series (episode numbers or the same name with the next number), never another film or a song passed");
}
