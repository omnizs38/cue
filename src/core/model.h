#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Subtitle look: font, size, text opacity, background opacity, position, outline, line gap, kerning.
// line spacing range: below 0 the lines come closer than the font's own spacing
// (lines may be pulled closer than the font's own leading: down to -16 px at 44, about a third of a
// line, where descenders still clear the next line's capitals)
constexpr int kLineGapMin = -16, kLineGapMax = 24;
// volume goes past 100% up to this, as a booster (100 stays the natural level)
constexpr double kVolMax = 125;

struct SubStyle {
  int font = 1;        // 0 Omni, 1 Clean, 2 Readable
  int size = 36;       // libass scaled px (relative to a 720 px tall window); 44 until 1.4.1 (the author's choice, 22/11: a line 5% of the picture's height)
  int text_op = 100;   // 0..100
  int bg_op = 45;      // 0 transparent, 1..99 translucent black, 100 solid
  double pos = 93;     // sub-pos, % of window height (bottom of the block)
  int outline = 2;     // 0 = no outline; thickness when the background is transparent
  int line_gap = 0;    // space added between lines (px); negative pulls them together
  int tracking = 1;    // letter spacing, 'kerning' (scaled px, may be negative); 0 until 1.4.1 (a little air with the outline)
  int ass_mode = 0;    // ASS/SSA tracks: 0 original styling, 1 my style
  bool operator==(const SubStyle& o) const {
    return font == o.font && size == o.size && text_op == o.text_op && bg_op == o.bg_op &&
           (int)(pos * 10) == (int)(o.pos * 10) && ass_mode == o.ass_mode && outline == o.outline &&
           line_gap == o.line_gap && tracking == o.tracking;
  }
  bool operator!=(const SubStyle& o) const { return !(*this == o); }
};

constexpr double kSubPosTop = 16, kSubPosCenter = 56, kSubPosBottom = 93;
constexpr double kSubPosMin = 12, kSubPosMax = 97;

// A chapter of a film (MKV and MP4 carry them): where it starts and its name ("" when unnamed).
struct Chapter {
  double t = 0;
  std::string title;
  bool operator==(const Chapter& o) const { return t == o.t && title == o.title; }
};
// The chapter playing at `t`: the last one starting at or before it, -1 before the first or
// without chapters. A file with a single chapter has none worth showing (see chapters_shown).
inline int chapter_at(const std::vector<Chapter>& c, double t) {
  int k = -1;
  for (int i = 0; i < (int)c.size(); ++i) if (c[i].t <= t + 1e-6) k = i; else break;
  return k;
}
inline bool chapters_shown(const std::vector<Chapter>& c) { return c.size() >= 2; }

struct Track {
  int64_t id = 0;
  std::string type;  // video | audio | sub
  std::string lang, title, codec;
  // (1.4.1) the track's name where the file keeps it elsewhere: an MP4's own track name (OBS writes
  // the names given to its audio tracks there), or a handler that names it. Only for the menus'
  // words: nothing that decides what plays reads it (the recording's tracks are mixed as before)
  std::string name;
  bool selected = false, external = false, forced = false, is_default = false;
  std::string external_file;  // an external track's file (subtitles or sound loaded beside the film)
  bool albumart = false;
  int demux_w = 0, demux_h = 0;  // a picture's own size, when the file says
  double demux_fps = 0;          // a film's frame rate, when the file says
  double demux_par = 0;          // its pixels' shape (anamorphic films), when the file says (0: square)
  int demux_rotation = -1;       // how it is turned (phone videos), -1: the engine does not say
  bool hearing_impaired = false;
};

enum class SubKind { None, Text, Ass, Bitmap };

// Media-specific state: one entry per file, newest first.
struct RecentItem {
  std::string path;
  double pos = 0, dur = 0;
  int64_t last = 0;
  bool has_style = false;
  SubStyle style;
  // tracks the person chose for this file (-1 = never chosen), kept with their language so a
  // changed file never gets the wrong track
  int64_t sid = -1, aid = -1;
  std::string sid_lang, aid_lang;
  bool sub_visible = true;
  double sub_delay = 0;
  // what the home says of it beyond its name: a song's artist (from its tags), a book's chapters
  // and the one it was left in (-1: unknown)
  std::string artist;
  int chapters = 0, chapter = -1;
};

enum class DockAnchor { BottomCenter = 0, TopCenter, BottomLeft, BottomRight, Free };
struct DockPlace {
  DockAnchor anchor = DockAnchor::BottomCenter;
  double fx = 0.5, fy = 1.0;  // for Free: fraction of the free space
};
