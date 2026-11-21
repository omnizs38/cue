// The home's Recent list, read for people: episodes of one show become one entry (the latest
// one watched), names lose their extensions, and each entry knows how much is left.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "remote_proto.h"
#include "util.h"

struct RecentFile {
  std::string path;
  double pos = 0, dur = 0;  // resume point (0: start) and length, seconds
  int64_t last = 0;         // when it was last opened (unix seconds; 0: unknown)
  std::string artist;       // a song's artist (its tags), "" when the file said nothing
  int chapters = 0, chapter = -1;  // a book's chapters, and the one it was left in (-1: unknown)
  bool seen = false;        // watched to its end (Cue starts it over next time)
  bool video = false;       // a film whatever its name says (an .ogg holding Theora)
};

// What an entry is, for the home: how its picture, its quiet line and its time read.
//   Video    a film or a clip: its frame, "faltam 41 min", when it was watched
//   Episode  an episode of a show (grouped): its frame, "S01E04 · ontem"
//   Music    a song: its cover, its length (a song starts over: no progress)
//   Listen   long audio, a podcast or a talk: its cover, what is left, where it stopped
//   Book     an audiobook (.m4b, or long audio in chapters): its cover, "capítulo 4 de 12"
enum class RecentKind { Video, Episode, Music, Listen, Book };

// Audio this long is listened to like a film is watched: it is come back to where it was left
// (a podcast, a lecture, a book). Shorter audio is music, and starts over.
constexpr double kLongListen = 20 * 60;
inline bool resumes_audio(double dur) { return dur >= kLongListen; }

// Where the home's picture of a film is taken: where it was left, else a tenth in (past the logos,
// before anything is given away; 5 s to 5 min), never past its middle: a clip of a few seconds (a
// screen recording) was asked for after its end, and had no picture. Unknown length: 30 s (the
// helper tries its start when there is no picture there).
inline double home_shot_time(double resume, double dur) {
  if (resume > 0 && (dur <= 0 || resume < dur)) return resume;
  if (dur <= 0) return 30.0;
  return std::min(std::clamp(dur * 0.1, 5.0, 300.0), dur * 0.5);
}

inline bool is_book_name(std::string_view name) {
  const size_t dot = name.rfind('.');
  if (dot == std::string_view::npos) return false;
  std::string e(name.substr(dot + 1));
  for (auto& c : e) c = (char)std::tolower((unsigned char)c);
  return e == "m4b" || e == "aax" || e == "aa";
}

inline RecentKind recent_kind(std::string_view name, bool audio, bool episode, double dur, int chapters) {
  if (!audio) return episode ? RecentKind::Episode : RecentKind::Video;
  if (is_book_name(name) || (chapters >= 3 && dur >= 40 * 60)) return RecentKind::Book;
  return resumes_audio(dur) ? RecentKind::Listen : RecentKind::Music;
}

// How long ago, the way a person says it: today, yesterday, 3 days, 2 weeks, 5 months, a year.
enum class Ago { Unknown, Today, Yesterday, Days, Weeks, Months, Years };
struct AgoText { Ago unit = Ago::Unknown; int n = 0; };
// `days`: calendar days between the opening and today (local time), see local_days_between
inline AgoText ago_from_days(int64_t days) {
  if (days < 0) return {Ago::Unknown, 0};  // a clock set back: say nothing rather than nonsense
  if (days == 0) return {Ago::Today, 0};
  if (days == 1) return {Ago::Yesterday, 1};
  if (days < 7) return {Ago::Days, (int)days};
  if (days < 30) return {Ago::Weeks, (int)(days / 7)};
  if (days < 365) return {Ago::Months, (int)std::clamp<int64_t>(days / 30, 1, 11)};
  return {Ago::Years, (int)std::max<int64_t>(1, days / 365)};
}
// Calendar days between two moments in local time (a film watched at 23:50 was "yesterday" at
// 00:10). 0 or less when unknown.
inline int64_t local_day_number(int64_t unix_seconds) {
  std::time_t t = (std::time_t)unix_seconds;
  std::tm tm{};
#ifdef _WIN32
  if (localtime_s(&tm, &t) != 0) return 0;
#else
  if (!localtime_r(&t, &tm)) return 0;
#endif
  // days since 1970 of that local date (civil-from-days, Howard Hinnant)
  int y = tm.tm_year + 1900;
  const unsigned m = (unsigned)tm.tm_mon + 1, d = (unsigned)tm.tm_mday;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}
inline AgoText ago(int64_t then, int64_t now) {
  if (then <= 0 || now <= 0) return {};
  return ago_from_days(local_day_number(now) - local_day_number(then));
}

struct LiveRecent {
  std::string path;     // what opens: the latest episode, or the file
  std::string title;    // "Twin Peaks", or the file name without its extension
  std::string episode;  // "S01E03" for a show, else ""
  int episodes = 1;     // episodes of this show among the recents
  bool audio = false;
  double pos = 0, dur = 0;
  int64_t last = 0;
  std::string artist;
  int chapters = 0, chapter = -1;
  bool seen = false;
  RecentKind kind = RecentKind::Video;
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
    r.audio = remote::is_audio_name(name) && !f.video;
    r.pos = f.pos;
    r.dur = f.dur;
    r.last = f.last;
    r.artist = f.artist;
    r.chapters = f.chapters;
    r.chapter = f.chapter;
    r.seen = f.seen;
    r.episode = id.empty() ? std::string() : remote::episode_label(name);
    r.title = id.empty() ? std::string() : remote::series_title(name);
    if (r.title.empty()) {
      r.episode.clear();
      size_t dot = name.rfind('.');
      r.title = dot != std::string::npos && dot > 0 && remote::is_playable_name(name) ? name.substr(0, dot) : name;
    }
    r.kind = recent_kind(name, r.audio, !r.episode.empty(), r.dur, r.chapters);
    out.push_back(r);
    ids.push_back(r.episode.empty() ? std::string() : id);
  }
  return out;
}

// Something to go back to: started (past its first seconds) and not finished, and not a song (a
// song starts over).
inline bool home_resumable(const LiveRecent& e) { return e.kind != RecentKind::Music && e.pos > 0 && e.dur > 0; }

// The home's order: the newest entry to go back to comes first (it stands apart, under
// "continuar"), even when a song or a file only peeked at was opened after it; the rest stay
// newest first. At most `max` entries.
inline void home_order(std::vector<LiveRecent>& v, size_t max) {
  for (size_t i = 0; i < v.size() && i < 32; ++i)
    if (home_resumable(v[i])) {
      if (i > 0) std::rotate(v.begin(), v.begin() + (std::ptrdiff_t)i, v.begin() + (std::ptrdiff_t)i + 1);
      break;
    }
  if (v.size() > max) v.resize(max);
}

// A title as the home shows it: without what is only technical in brackets ("(audio)",
// "(320 Kbps)", "[1080p x265]") and, for a song whose artist is known, without that artist in
// front ("Jane Remover - Music Baby" by Jane Remover: "Music Baby"; the artist is on the line
// below). Careful on purpose: a bracket goes only when every word in it is technical, the artist
// only when it is the whole first part, and a title that would come out empty stays as it was.
namespace title_clean {
inline std::string lower(std::string_view v) {
  std::string o(v);
  for (auto& c : o) c = (char)std::tolower((unsigned char)c);
  return o;
}
inline bool technical_word(const std::string& w, bool& number) {
  static const char* words[] = {"audio", "áudio", "official", "oficial", "hq", "hd", "uhd", "4k", "8k", "kbps", "kbit/s", "khz",
                                "x264", "x265", "h264", "h.264", "h265", "h.265", "hevc", "avc", "av1", "vp9", "aac", "mp3", "flac",
                                "opus", "web-dl", "webdl", "webrip", "bluray", "blu-ray", "brrip", "bdrip", "hdtv", "hdr", "hdr10",
                                "remux", "10bit", "8bit", "10-bit", "8-bit", "dts", "ac3", "eac3", "ddp5.1", "dd5.1", "320", "256", "192", "128"};
  for (auto* t : words) if (w == t) { number = w == "320" || w == "256" || w == "192" || w == "128"; return true; }
  auto digits = [](std::string_view v) { return !v.empty() && std::all_of(v.begin(), v.end(), [](char c) { return c >= '0' && c <= '9'; }); };
  if (w.size() >= 2 && (w.back() == 'p' || w.back() == 'k') && digits(std::string_view(w).substr(0, w.size() - 1))) {
    const int n = std::atoi(w.c_str());
    return w.back() == 'p' ? n >= 240 && n <= 4320 : n >= 64 && n <= 1411;  // 1080p; 320k
  }
  if (w.size() > 4 && w.compare(w.size() - 4, 4, "kbps") == 0 && digits(std::string_view(w).substr(0, w.size() - 4))) return true;
  if (w.size() > 3 && w.compare(w.size() - 3, 3, "bit") == 0 && digits(std::string_view(w).substr(0, w.size() - 3))) return true;
  if (digits(w)) { number = true; return true; }  // only with a technical word beside it (see below)
  return false;
}
// every word inside the brackets is technical, and a bare number has a technical word with it
inline bool technical(std::string_view inside) {
  std::string l = lower(inside), w;
  bool any = false, word = false, number = false;
  auto take = [&] {
    if (w.empty()) return true;
    bool n = false;
    if (!technical_word(w, n)) return false;
    any = true; (n ? number : word) = true;
    w.clear(); return true;
  };
  for (char c : l) {
    if (c == ' ' || c == ',' || c == '_' || c == '+' || c == '/') { if (!take()) return false; }
    else w += c;
  }
  if (!take()) return false;
  return any && word;
}
inline std::string trim(std::string t) {
  auto sep = [](unsigned char c) { return c == ' ' || c == '-' || c == '_' || c == '.' || c == '\t'; };
  // (the en and em dashes are three bytes: E2 80 93 / E2 80 94)
  for (bool again = true; again;) {
    again = false;
    while (!t.empty() && sep((unsigned char)t.back())) { t.pop_back(); again = true; }
    while (!t.empty() && sep((unsigned char)t.front())) { t.erase(0, 1); again = true; }
    for (const char* d : {"\xe2\x80\x93", "\xe2\x80\x94"}) {
      if (t.size() >= 3 && t.compare(t.size() - 3, 3, d) == 0) { t.resize(t.size() - 3); again = true; }
      if (t.size() >= 3 && t.compare(0, 3, d) == 0) { t.erase(0, 3); again = true; }
    }
  }
  return t;
}
}  // namespace title_clean

inline std::string clean_title(const std::string& title, const std::string& artist) {
  using namespace title_clean;
  std::string t = title;
  bool cut = false;
  // brackets whose content is only technical
  for (int guard = 0; guard < 8; ++guard) {
    bool removed = false;
    for (size_t open = 0; open < t.size(); ++open) {
      const char o = t[open];
      if (o != '(' && o != '[') continue;
      const size_t close = t.find(o == '(' ? ')' : ']', open + 1);
      if (close == std::string::npos) break;
      if (technical(std::string_view(t).substr(open + 1, close - open - 1))) {
        t.erase(open, close - open + 1);
        removed = cut = true;
        break;
      }
      open = close;
    }
    if (!removed) break;
  }
  // the spaces and the dash a removed bracket leaves ("Crimson - (320 Kbps)"); a title nothing
  // was taken from keeps its own ending ("Mr.", "Help -")
  if (cut) {
    std::string c;
    for (char ch : t) if (!(ch == ' ' && !c.empty() && c.back() == ' ')) c += ch;
    t = trim(c);
  }
  // the artist in front, followed by a dash
  const std::string a = trim(artist);
  if (!a.empty() && t.size() > a.size() && lower(t.substr(0, a.size())) == lower(a)) {
    std::string rest = t.substr(a.size());
    size_t i = 0;
    while (i < rest.size() && rest[i] == ' ') ++i;
    size_t dash = 0;
    if (i < rest.size() && rest[i] == '-') dash = 1;
    else if (rest.compare(i, 3, "\xe2\x80\x93") == 0 || rest.compare(i, 3, "\xe2\x80\x94") == 0) dash = 3;
    if (dash && i + dash < rest.size() && rest[i + dash] == ' ') {
      std::string song = trim(rest.substr(i + dash));
      if (!song.empty()) t = song;
    }
  }
  return t.empty() ? title : t;
}

// The file a recent entry's picture is kept in (in Cue's own "thumbs" folder): a hash of its
// normalized path (see normalize_path), so the name says nothing of the file or its folder.
inline std::string thumb_file_name(const std::string& normalized_path) {
  uint64_t h = 14695981039346656037ull;
  for (unsigned char c : normalized_path) { h ^= c; h *= 1099511628211ull; }
  static const char* hex = "0123456789abcdef";
  std::string o(16, '0');
  for (int i = 15; i >= 0; --i) { o[i] = hex[h & 15]; h >>= 4; }
  return o + ".jpg";
}

// Watched to its end: its place went back to the start when it ended (or it was left in its last
// moments), and its length is known. The same rule as the phone remote's "seen".
inline bool recent_seen(double resume_point, double pos, double dur) {
  if (resume_point > 0 || dur <= 0) return false;
  return pos <= 0 || pos > dur - 30 || pos > dur * 0.97;
}

// "24 min", "1 h 05 min" pieces: whole minutes, at least one
inline void split_minutes(double seconds, int& h, int& m) {
  seconds = std::isfinite(seconds) ? std::clamp(seconds, 0.0, 1e9) : 0.0;  // (a broken length must not overflow the cast)
  int total = std::max(1, (int)((seconds + 59) / 60));
  h = total / 60;
  m = total % 60;
}
