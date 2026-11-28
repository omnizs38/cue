// The home list's pictures: an entry's own picture (a film's frame, a song's cover) made to fit
// its tile, and the stand-in drawn when it has none yet (a pixel landscape for a film, a note,
// a microphone or a book for audio), always the same one for the same file.
#pragma once
#include <cstdint>
#include <string>

#include "gfx.h"
#include "home.h"

namespace thumb_art {

// The picture into `out`, opaque: a frame fills the w x h tile (its middle, cropped); a cover is
// an h x h square, whole. The corner cells (`cut` px) are left transparent, the pixel
// language's rounded corner.
void make_tile(const HomeThumb& t, int w, int h, int cut, Canvas& out);

// Audio's pictures are square (a cover, or its stand-in), on the left of the tile: the tile's
// height across. A song's record peeks out from behind it (see make_disc).
bool square(HomeKind kind, bool audio);

// A record, `side` px across on the art grid of `s`, transparent around it: pixel grooves, the
// rim, two soft reflections, a label of `label_rgb` with a small mark. `turn` (radians) turns the
// reflections and the mark; `light` 0..1 dims it with the picture it belongs to.
void make_disc(uint32_t label_rgb, int side, double s, double turn, double light, Canvas& out);
// The label's colour for a cover (its most vivid colour, kept readable), and for a stand-in.
uint32_t accent(const HomeThumb& t);
uint32_t stand_in_accent(const std::string& seed);

// The stand-in for an entry of `kind` (an audio row may say Video: it is drawn as music), on the
// art grid of `s` (px per art px), seeded by `seed` (the path).
void make_stand_in(HomeKind kind, bool audio, const std::string& seed, int w, int h, double s, int cut, Canvas& out);

// Resamples src (sw x sh, 0xFFRRGGBB) into a w x h region of dst (row stride `stride` pixels):
// a box average where it shrinks, bilinear where it grows. (sx, sy, cw, ch): the part of the
// source used (a crop), in source px.
void resample(const uint32_t* src, int sw, int sh, double sx, double sy, double cw, double ch,
              uint32_t* dst, int stride, int w, int h);

// A small seed from a string (the same for the same path on every run).
uint32_t seed_of(const std::string& s);

}  // namespace thumb_art
