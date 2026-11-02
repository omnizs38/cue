// The home's Recent list, read for people: episodes of one show become one entry (the latest
// one watched), names lose their extensions, and each entry knows how much is left.
#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "remote_proto.h"
#include "util.h"

struct RecentFile {
  std::string path;
  double pos = 0, dur = 0;  // resume point (0: start) and length, seconds
};

struct LiveRecent {
  std::string path;     // what opens: the latest episode, or the file
  std::string title;    // "Twin Peaks", or the file name without its extension
  std::string episode;  // "S01E03" for a show, else ""
  int episodes = 1;     // episodes of this show among the recents
  bool audio = false;
  double pos = 0, dur = 0;
};

// `files` newest first. Returns at most `max` entries, newest first.
inline std::vector<LiveRecent> live_recents(const std::vector<RecentFile>& files, size_t max) {
  std::vector<LiveRecent> out;
  std::vector<std::string> ids;  // the show of each entry ("" for single files)
  for (const auto& f : files) {
    std::string name = file_name(f.path);
    std::string id = remote::series_id(name);
    if (!id.empty()) {
      bool merged = false;
      for (size_t i = 0; i < out.size(); ++i)
        if (ids[i] == id) { ++out[i].episodes; merged = true; break; }
      if (merged) continue;
    }
    if (out.size() >= max) continue;  // keep counting episodes of the entries already shown
    LiveRecent r;
    r.path = f.path;
    r.audio = remote::is_audio_name(name);
    r.pos = f.pos;
    r.dur = f.dur;
    r.episode = id.empty() ? std::string() : remote::episode_label(name);
    r.title = id.empty() ? std::string() : remote::series_title(name);
    if (r.title.empty()) {
      r.episode.clear();
      size_t dot = name.rfind('.');
      r.title = dot != std::string::npos && dot > 0 && remote::is_playable_name(name) ? name.substr(0, dot) : name;
    }
    out.push_back(r);
    ids.push_back(r.episode.empty() ? std::string() : id);
  }
  return out;
}

// "24 min", "1 h 05 min" pieces: whole minutes, at least one
inline void split_minutes(double seconds, int& h, int& m) {
  int total = std::max(1, (int)((seconds + 59) / 60));
  h = total / 60;
  m = total % 60;
}
