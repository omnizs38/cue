// A song's cover (embedded in its tags or a picture beside it, see core/cover_art) as BGRA
// pixels (0xAARRGGBB), decoded by Windows' imaging codecs.
#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

bool cover_image(const std::string& path, std::vector<uint32_t>& px, int& w, int& h, int max_side);
bool decode_image(const std::string& data, std::vector<uint32_t>& px, int& w, int& h, int max_side,
                  int* full_w = nullptr, int* full_h = nullptr);

#include <atomic>
#include <memory>

#include "../ui/atmosphere.h"

// Everything a song's cover gives the player, read once on a background thread as the song
// opens (one read of the file, one decode, small): where the cover sits inside flat black bars
// (a video's frame boxed into the picture; x, y, w, h are shares of the picture, full_w and
// full_h its own size, to check it is the picture the engine shows), the colours around its
// border for music's light, and the cover as found (encoded) for Windows' media card.
// The last few songs are kept in memory (never on disk), so going back to one is instant.
struct CoverJob {
  std::atomic<bool> done{false};
  bool found = false;  // a cover, bars or not
  double x = 0, y = 0, w = 1, h = 1;
  int full_w = 0, full_h = 0;
  bool lit = false;    // the colours were read
  FrameColors colors;
  std::string bytes;
};
std::shared_ptr<CoverJob> request_cover(const std::string& path);

// The cover as the engine shows it in the card, `side` px a side (see core/cover_turn.h), read on a
// worker once `cover` is read: the bars cut away, the middle square, or Cue's music tile when
// there is no cover (cover null, or none found). For the card's turning cover.
#include "../core/cover_turn.h"
struct CoverArtJob {
  std::atomic<bool> done{false};
  std::shared_ptr<const CoverArt> art;
  int side = 0;
};
std::shared_ptr<CoverArtJob> request_cover_art(std::shared_ptr<CoverJob> cover, int side);

// A film's still for Windows' media card, from a small picture of the frame on screen (taken in
// memory, never written to disk): checked that it is not a black frame, then encoded as a JPEG on
// a worker. bytes stay empty for a dark frame.
struct CardStillJob {
  std::atomic<bool> done{false};
  std::string bytes;
};
std::shared_ptr<CardStillJob> request_card_still(std::vector<uint32_t> px, int w, int h);
// The engine's raw screenshot (bgr0, `stride` bytes a row) reduced to at most max_w x max_h,
// each pixel the average of a 2x2 sample. Quick enough for the UI thread at any film size.
std::vector<uint32_t> shrink_bgr0(const uint8_t* data, int w, int h, int stride, int max_w, int max_h, int& out_w, int& out_h);
