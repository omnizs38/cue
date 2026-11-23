// SubtitleRenderer integration: our five controls → libass properties via mpv.
#pragma once
#include <string>
#include <vector>

#include "model.h"

struct PropSet {
  std::vector<std::string> names;  // first is preferred; later ones are older mpv names
  std::string value;
};

extern const char* const kSubFontFamily[3];  // "Cue Omni", "Cue Clean", "Cue Readable"
extern const char* const kSubFontLabel[3];   // "Omni", "Clean", "Readable"

// dim (0..1): how much of the line shows; a panel over it steps it back (colours only, no layout)
std::vector<PropSet> sub_style_props(const SubStyle& st, SubKind kind, double typography_scale = 1.0, double dim = 1.0, double line_px = 0);
// (1.4.1) line_px: the letters' size on screen (px); the line spacing is a share of it (subtitle_line_gap_share).
// 0: not known (the spacing in px as before)
// What the style editor may show for the current track kind.
struct StyleCaps { bool font = true, size = true, opacity = true, position = true, ass_toggle = false; };
StyleCaps style_caps(const SubStyle& st, SubKind kind);
