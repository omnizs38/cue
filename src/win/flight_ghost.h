// The player's stand-in while it flies into the mini player or back: a live picture of the window
// (a DWM thumbnail: Windows composes it from the window's own pixels, on the graphics card, as the
// video plays) in a click-through popup that only moves and scales. The window itself is already
// at its destination, out of sight (cloaked), so it changes size once and the engine draws at the
// new size while the picture flies; the window shows again where the picture lands.
#pragma once
#include <windows.h>
#include <dwmapi.h>

class FlightGhost {
 public:
  // shown at `screen` (screen px, the picture's place); false when Windows gives no thumbnail
  // (then the flight is the plain one)
  bool begin(HINSTANCE inst, HWND source, const RECT& screen);
  // the picture's place now; the source is cropped to that shape, centred (never stretched)
  void place(const RECT& screen);
  void end();
  bool on() const { return thumb_ != nullptr; }

 private:
  HWND wnd_ = nullptr, src_ = nullptr;
  HTHUMBNAIL thumb_ = nullptr;
  RECT last_{};
  bool update(const RECT& screen);
};

// A black stand-in held over the screen (or the window) while the window changes size for full
// screen, out of sight: see flight_ghost.cpp
class StageCover {
 public:
  // shown (or moved) at `screen`, painted at once; `wait`: until Windows has put it on screen
  bool show(HINSTANCE inst, const RECT& screen, bool rounded, bool wait = false);
  void end();
  bool on() const { return wnd_ != nullptr; }
  ~StageCover() { end(); }

 private:
  HWND wnd_ = nullptr;
};
