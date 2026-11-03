#include "word_highlight.h"
#include "json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
constexpr size_t kMaxWords = 8192, kMaxWordBytes = 256, kCueWords = 10;
constexpr int kFadeMs = 80;
struct Word { std::string text; double start, end; };
using Cue = std::vector<Word>;

// Subtitle text is data, never ASS override syntax. Fullwidth equivalents for
// literal backslashes/braces avoid interpreting \N, drawing or override tags.
std::string escape(const std::string& text) {
  std::string out;
  for (unsigned char c : text) {
    if (c == '\\') out += "\xef\xbc\xbc";
    else if (c == '{') out += "\xef\xbd\x9b";
    else if (c == '}') out += "\xef\xbd\x9d";
    else if (c >= 32 && c != 127) out += char(c);
    else out += ' ';
  }
  return out;
}
bool space(unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
bool ascii_word(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
std::string stamp(double t, bool round_up = false) {
  // ASS event timestamps have centisecond precision. Enclose, don't truncate,
  // word intervals; transforms retain millisecond precision inside each cue.
  int cs = int(round_up ? std::ceil(t * 100 - 1e-7) : std::floor(t * 100 + 1e-7));
  char b[32];
  std::snprintf(b, sizeof b, "%d:%02d:%02d.%02d", cs / 360000, cs / 6000 % 60, cs / 100 % 60, cs % 100);
  return b;
}
int ms(double seconds) { return int(std::lround(seconds * 1000)); }

bool read_words(const JVal& array, std::vector<Cue>& cues, size_t& count, double& last_end) {
  if (array.t != JVal::Arr || array.a.empty()) return false;
  Cue cue;
  size_t cue_bytes = 0;
  for (const auto& v : array.a) {
    const auto& a = v["start"]; const auto& b = v["end"]; const auto& text = v["word"];
    if (a.t != JVal::Num || b.t != JVal::Num || text.t != JVal::Str ||
        !std::isfinite(a.n) || !std::isfinite(b.n) || a.n < last_end || b.n < a.n ||
        b.n > 86400 || text.s.empty() || text.s.size() > kMaxWordBytes || ++count > kMaxWords) return false;
    bool visible = false;
    for (unsigned char c : text.s) if (c >= 32 && !space(c) && c != 127) visible = true;
    if (!visible) return false;
    if (!cue.empty() && (cue.size() == kCueWords || cue_bytes + text.s.size() > 160 || a.n - last_end > .8)) {
      cues.push_back(std::move(cue)); cue = Cue{}; cue_bytes = 0;
    }
    cue.push_back({text.s, a.n, b.n}); cue_bytes += text.s.size(); last_end = b.n;
  }
  if (!cue.empty()) cues.push_back(std::move(cue));
  return true;
}

// Two events per cue, not per frame or per word. Both contain the same full
// text and metrics, so shaping, wrapping and baseline stay fixed. Resetting
// Glow before each word also discards the previous word's ASS transforms.
std::string glow(const Cue& cue, double origin, bool reduced) {
  std::string out;
  for (const auto& w : cue) {
    // Zero-duration punctuation stays in the caption but is never highlighted.
    if (w.end == w.start) { out += "{\\rGlow\\alpha&HFF&}" + escape(w.text); continue; }
    const int a = std::max(0, ms(w.start - origin));
    const int b = std::max(a + 1, ms(w.end - origin));
    const int fade = reduced ? 0 : std::min(kFadeMs, std::max(1, (b - a) / 2));
    out += "{\\rGlow\\bord6\\blur6\\3c&HFFFFFF&\\1a&HFF&\\3a&HFF&\\4a&HFF&";
    out += "\\t(" + std::to_string(a) + "," + std::to_string(a + fade) + ",\\3a&H70&)";
    out += "\\t(" + std::to_string(b) + "," + std::to_string(b + fade) + ",\\3a&HFF&)}";
    out += escape(w.text);
  }
  return out;
}
}  // namespace

std::string word_highlight_ass(const std::string& json, bool reduced_motion) {
  if (json.empty() || json.size() > kWordHighlightMaxInput) return {};
  JVal root;
  if (!json_parse(json, root) || root.t != JVal::Obj) return {};
  std::vector<Cue> cues;
  size_t count = 0;
  double last_end = 0;
  if (root["segments"].t == JVal::Arr) {
    if (root["segments"].a.empty()) return {};
    for (const auto& seg : root["segments"].a)
      if (!read_words(seg["words"], cues, count, last_end)) return {};
  } else if (!read_words(root["words"], cues, count, last_end)) return {};
  // Whisper preserves leading spaces. Also accept simple ASCII word arrays
  // without spaces, while leaving CJK/punctuation boundaries unmodified.
  for (auto& cue : cues) {
    for (size_t i = 0; i < cue.size(); ++i) {
      auto& text = cue[i].text;
      if (i == 0) { while (!text.empty() && space(text.front())) text.erase(0, 1); }
      else if (ascii_word(cue[i-1].text.back()) && ascii_word(text.front())) text.insert(text.begin(), ' ');
    }
  }
  std::string ass =
      "[Script Info]\nScriptType: v4.00+\nPlayResX: 1280\nPlayResY: 720\n"
      "WrapStyle: 0\nScaledBorderAndShadow: yes\n\n[V4+ Styles]\n"
      "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
      "Style: Base,Cue Clean,44,&H00FFFFFF,&H00FFFFFF,&H00101010,&HFF000000,0,0,0,0,100,100,0,0,1,2,0,2,40,40,50,1\n"
      "Style: Glow,Cue Clean,44,&HFFFFFFFF,&HFFFFFFFF,&HFFFFFFFF,&HFFFFFFFF,0,0,0,0,100,100,0,0,1,6,0,2,40,40,50,1\n\n"
      "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
  for (size_t i = 0; i < cues.size(); ++i) {
    const auto& cue = cues[i];
    double origin = std::floor(cue.front().start * 100 + 1e-7) / 100;
    double end = cue.back().end + (reduced_motion ? 0 : kFadeMs / 1000.0);
    // Never let neighboring cues collide, or show two different sentences.
    if (i + 1 < cues.size()) end = std::min(end, std::floor(cues[i+1].front().start * 100 + 1e-7) / 100);
    const std::string times = stamp(origin) + "," + stamp(end, true);
    std::string plain;
    for (const auto& w : cue) plain += escape(w.text);
    ass += "Dialogue: 0," + times + ",Glow,,0,0,0,," + glow(cue, origin, reduced_motion) + "\n";
    ass += "Dialogue: 1," + times + ",Base,,0,0,0,," + plain + "\n";
  }
  return ass;
}
