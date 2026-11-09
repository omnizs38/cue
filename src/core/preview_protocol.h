#pragma once
#include <cstdint>

// Private helper protocol. Timing fields allow an external diagnostic to measure the
// same pipeline as the player without altering normal playback or writing thumbnail files.
struct PreviewHeader {
  uint32_t magic = 0x33565054;  // TPV3
  uint32_t type = 0, id = 0, session = 0;
  int32_t w = 0, h = 0;
  double t = 0, decode_ms = 0, prepare_ms = 0;
  uint32_t flags = 0, reserved = 0;  // bit 0: hardware decoding actually in use
};
static_assert(sizeof(PreviewHeader) == 56);

inline const char* preview_seek_mode(bool precise) {
  return precise ? "absolute+exact" : "absolute+keyframes";
}
