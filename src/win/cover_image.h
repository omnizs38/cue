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

// The cover's colours around its border (as a film frame's), read on a background thread for
// music's light show. found is false for a song without a cover.
struct CoverColorsJob {
  std::atomic<bool> done{false};
  bool found = false;
  FrameColors colors;
  std::string bytes;  // the cover as found (encoded), for Windows' media card
};
std::shared_ptr<CoverColorsJob> request_cover_colors(std::string path);

// Where a song's cover sits inside flat black bars (a video's frame boxed into the picture), read
// on a background thread as the song opens, for the engine's picture of it. x, y, w, h are shares
// of the picture; full_w and full_h its own size, to check it is the picture the engine shows.
struct CoverTrimJob {
  std::atomic<bool> done{false};
  bool found = false;  // a cover, bars or not
  double x = 0, y = 0, w = 1, h = 1;
  int full_w = 0, full_h = 0;
};
std::shared_ptr<CoverTrimJob> request_cover_trim(std::string path);

// A still the engine saved for Windows' media card (a film has no cover): read, checked that it
// is not a black frame, and removed from disk. bytes stay empty for a dark or missing still.
struct CardStillJob {
  std::atomic<bool> done{false};
  std::string bytes;
};
std::shared_ptr<CardStillJob> request_card_still(std::wstring file);
